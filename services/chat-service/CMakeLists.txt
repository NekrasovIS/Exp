cmake_minimum_required(VERSION 3.21)

# Независимо собираемый микросервис — свой vcpkg.json (в этом
# каталоге), использует общий инструмент/кэш vcpkg, вендоренный в корне
# репозитория.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Тип zview у libpqxx публично наследуется от std::string_view; у MSVC
# dllexport распространяется на унаследованные члены базового класса
# даже когда это обычные inline-функции STL, поэтому сборка против
# стандартного динамического (DLL) триплета на Windows заставляет
# basic_string_view::size()/data() одновременно экспортироваться из
# pqxx.dll *и* локально инстанцироваться в любой единице трансляции,
# подключающей заголовки pqxx — "already defined" на этапе линковки для
# обоих. Этот класс проблем с границей DLL у MSVC не существует на
# динамических триплетах, которые vcpkg выбирает для macOS/Linux,
# поэтому ограничиваем только Windows, а не меняем линковку везде.
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg triplet")
    # Пакеты x64-windows-static собраны против статического CRT
    # (/MT, /MTd) — иначе CMake по умолчанию берёт динамический CRT
    # (/MD, /MDd), который линкуется без ошибок, но затем каждый
    # объектный файл расходится с каждой vcpkg .lib в том, какой CRT ему
    # нужен (LNK2038).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

project(ChatService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Максимальный набор предупреждений + pedantic-режим — то же правило,
# что и во всём проекте (DeviceHub).
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
endif()

# Сборки с санитайзерами (issue #145) — выключены по умолчанию, только
# GCC/Clang (MSVC не поддерживает -fsanitize=thread и сравнимый по
# полноте UBSan). ASan+UBSan совместимы в одном бинарнике; TSan нужна
# отдельная сборка (её раскладка теневой памяти конфликтует с ASan),
# отсюда два отдельных опциона, а не один. -fno-sanitize-recover=all
# заставляет находку UBSan прерывать процесс теста вместо того, чтобы
# просто напечатать диагностику и продолжить — так находка UB реально
# роняет job CI, как и любой другой провал теста.
option(DEVICEHUB_ENABLE_ASAN_UBSAN "Build with AddressSanitizer + UndefinedBehaviorSanitizer (GCC/Clang only)" OFF)
option(DEVICEHUB_ENABLE_TSAN "Build with ThreadSanitizer (GCC/Clang only; incompatible with DEVICEHUB_ENABLE_ASAN_UBSAN)" OFF)
if(DEVICEHUB_ENABLE_ASAN_UBSAN AND DEVICEHUB_ENABLE_TSAN)
    message(FATAL_ERROR "DEVICEHUB_ENABLE_ASAN_UBSAN and DEVICEHUB_ENABLE_TSAN cannot be combined in one build")
endif()
if(DEVICEHUB_ENABLE_ASAN_UBSAN OR DEVICEHUB_ENABLE_TSAN)
    if(MSVC)
        message(FATAL_ERROR "DEVICEHUB_ENABLE_ASAN_UBSAN/DEVICEHUB_ENABLE_TSAN require GCC or Clang, not MSVC")
    endif()
endif()
if(DEVICEHUB_ENABLE_ASAN_UBSAN)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()
if(DEVICEHUB_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
    # issue #170 — vcpkg-порт cpp-httplib по умолчанию включает
    # CPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO (см. её
    # INTERFACE_COMPILE_DEFINITIONS в сгенерированном httplibTargets.cmake),
    # которая на Linux/glibc заворачивает каждый запрос httplib::Client
    # через внутренний пул потоков асинхронного резолвера getaddrinfo_a().
    # Этот поток создаёт сам glibc, в обход обёртки pthread_create(),
    # которую инструментирует TSan для настройки теневой памяти на поток —
    # первый же malloc() в этом потоке (внутри собственных внутренностей
    # getaddrinfo у glibc) падает в аллокаторе TSan, поскольку TSan никогда
    # не видел создание этого потока. Именно поэтому под TSan падал любой
    # тест, делающий хотя бы один исходящий HTTP-запрос (HttpServerTest/
    # WebSocketServerTest/AuthServiceClientTest здесь — WebSocketServerTest
    # ловит это транзитивно через вызовы AuthServiceClient к
    # /auth/verify auth-service), тогда как тесты чистой логики без
    # сетевого I/O (JsonGuardTest и т.п.) всегда проходили нормально —
    # подтверждено локальным воспроизведением под
    # `clang++ -fsanitize=thread` на Linux с идентичным падением внутри
    # handle_requests()/generate_addrinfo() из glibc, которое исчезает,
    # как только эта макро отменена для TSan-сборки. Отмена макро здесь
    # откатывает на обычный синхронный вызов getaddrinfo() на том же
    # потоке-вызывающем — без дополнительного потока, без несовместимости
    # и без изменения поведения для сервиса, который всегда обращается
    # только к 127.0.0.1 (реальные таймауты резолвинга имени никогда и не
    # были нужны).
    add_compile_options(-UCPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
endif()

# Покрытие тестами на основе исходников (issue #146) — выключено по
# умолчанию, только Clang (-fprofile-instr-generate/-fcoverage-mapping
# из LLVM; --coverage у GCC потребовал бы другой, gcov-based конвейер
# отчётов). Применяется на весь проект через
# add_compile_options()/add_link_options() до определения любой цели,
# поэтому и chat_service_core, и chat_service_tests инструментируются
# согласованно — покрытию нужен каждый объектный файл, участвующий в
# прогоне теста, собранный так, а не только сам тестовый бинарник.
option(DEVICEHUB_ENABLE_COVERAGE "Build with source-based code coverage instrumentation (needs Clang)" OFF)
if(DEVICEHUB_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "DEVICEHUB_ENABLE_COVERAGE requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

find_package(httplib CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# Коды приглашений сообществ (issue #186) — RAND_bytes, не rand()/
# std::mt19937: угадываемый код приглашения даёт доступ к чужому
# сообществу в обход присоединения по явному списку, тот же класс
# риска, что уже обосновывает RAND_bytes для OTP-кодов в auth-service
# (см. OtpStore.cpp).
find_package(OpenSSL REQUIRED)

add_library(chat_service_core
    src/base64.h
    src/base64.cpp
    src/JsonGuard.h
    src/JsonGuard.cpp
    src/ChatRepository.h
    src/ChatRepository.cpp
    src/ChatService.h
    src/ChatService.cpp
    src/AuthServiceClient.h
    src/AuthServiceClient.cpp
    src/JanusClient.h
    src/JanusClient.cpp
    src/UserServiceClient.h
    src/UserServiceClient.cpp
    src/HttpServer.h
    src/HttpServer.cpp
    src/WebSocketServer.h
    src/WebSocketServer.cpp
)
target_include_directories(chat_service_core PUBLIC src)
target_link_libraries(chat_service_core PUBLIC
    httplib::httplib
    ixwebsocket::ixwebsocket
    libpqxx::pqxx
    nlohmann_json::nlohmann_json
    OpenSSL::Crypto
)

# vcpkg-порт ixwebsocket по умолчанию использует бэкенд mbedTLS на
# Windows (его собственный vcpkg.json отображает фичу "ssl" на
# "mbedtls" для platform:windows — macOS вместо этого получает
# "sectransp", поэтому это никогда не всплывало в CI). Источник энтропии
# mbedTLS на Windows вызывает BCryptGenRandom(), которая живёт в
# bcrypt.dll — без линковки против неё финальный бинарник падает с
# неразрешённым внешним символом.
if(WIN32)
    target_link_libraries(chat_service_core PUBLIC bcrypt)
endif()

add_executable(chat-service src/main.cpp)
target_link_libraries(chat-service PRIVATE chat_service_core)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(chat_service_tests
    test/Base64Test.cpp
    test/JsonGuardTest.cpp
    test/ChatServiceIntegrationTest.cpp
    test/WebSocketServerTest.cpp
    test/HttpServerTest.cpp
    test/AuthServiceClientTest.cpp
    test/JanusClientTest.cpp
    test/UserServiceClientTest.cpp
)
target_link_libraries(chat_service_tests PRIVATE
    chat_service_core
    GTest::gtest
    GTest::gtest_main
)

# Настоящий coverage-guided fuzzing (issue #132/#133) — нужен libFuzzer
# от Clang; cl.exe у MSVC не поддерживает -fsanitize=fuzzer (изначальный
# блокер issue #121), поэтому выключено по умолчанию и годится только
# там, где компилятор — Clang (macOS CI, либо clang-cl, если установлен
# локально). Выключено по умолчанию, чтобы обычный путь сборки/тестов
# (используемый везде, включая Windows-машины разработчиков) никогда не
# требовал Clang.
option(DEVICEHUB_ENABLE_FUZZING "Build libFuzzer targets (needs Clang)" OFF)
if(DEVICEHUB_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "DEVICEHUB_ENABLE_FUZZING requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()

    function(chat_service_add_fuzz_target name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE chat_service_core)
        target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address)
        target_link_options(${name} PRIVATE -fsanitize=fuzzer,address)
    endfunction()

    chat_service_add_fuzz_target(base64_fuzz fuzz/Base64Fuzz.cpp)
    chat_service_add_fuzz_target(json_guard_fuzz fuzz/JsonGuardFuzz.cpp)
    chat_service_add_fuzz_target(websocket_frame_fuzz fuzz/WebSocketFrameFuzz.cpp)
endif()
include(GoogleTest)
gtest_discover_tests(chat_service_tests)
