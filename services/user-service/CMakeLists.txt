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
# которая тоже использует std::string_view напрямую (диагностика из
# chat-service, issue #98 — собственная проверка префикса Bearer через
# std::string_view в HttpServer.cpp достаточна, чтобы вызвать это и
# здесь, issue #110) — "already defined" на этапе линковки для обоих.
# Ограничено только Windows; этот класс проблем с границей DLL у MSVC не
# существует на динамических триплетах, которые vcpkg выбирает для
# macOS/Linux.
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg triplet")
    # Пакеты x64-windows-static собраны против статического CRT
    # (/MT, /MTd) — иначе CMake по умолчанию берёт динамический CRT
    # (/MD, /MDd), который линкуется без ошибок, но затем каждый
    # объектный файл расходится с каждой vcpkg .lib в том, какой CRT ему
    # нужен (LNK2038).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

project(UserService LANGUAGES CXX)

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

# Сборки с санитайзерами (issue #145) — полное обоснование см. в копии
# этого же блока в chat-service/CMakeLists.txt.
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
    # issue #170 — полное обоснование см. в копии этого же блока в
    # chat-service/CMakeLists.txt (асинхронный getaddrinfo у cpp-httplib
    # против отслеживания потоков в TSan).
    add_compile_options(-UCPPHTTPLIB_USE_NON_BLOCKING_GETADDRINFO)
endif()

# Покрытие тестами на основе исходников (issue #146) — полное
# обоснование см. в копии этого же блока в chat-service/CMakeLists.txt.
option(DEVICEHUB_ENABLE_COVERAGE "Build with source-based code coverage instrumentation (needs Clang)" OFF)
if(DEVICEHUB_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "DEVICEHUB_ENABLE_COVERAGE requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

find_package(httplib CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(user_service_core
    src/password_hash.h
    src/password_hash.cpp
    src/JsonGuard.h
    src/JsonGuard.cpp
    src/UserRepository.h
    src/UserRepository.cpp
    src/UserService.h
    src/UserService.cpp
    src/AuthServiceClient.h
    src/AuthServiceClient.cpp
    src/HttpServer.h
    src/HttpServer.cpp
)
target_include_directories(user_service_core PUBLIC src)
target_link_libraries(user_service_core PUBLIC
    httplib::httplib
    libpqxx::pqxx
    unofficial-sodium::sodium
    nlohmann_json::nlohmann_json
)

add_executable(user-service src/main.cpp)
target_link_libraries(user-service PRIVATE user_service_core)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(user_service_tests
    test/PasswordHashTest.cpp
    test/JsonGuardTest.cpp
    test/UserServiceIntegrationTest.cpp
    test/UserRepositoryTest.cpp
    test/HttpServerTest.cpp
    test/AuthServiceClientTest.cpp
)
target_link_libraries(user_service_tests PRIVATE
    user_service_core
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(user_service_tests)
