cmake_minimum_required(VERSION 3.21)

# Independently buildable microservice — its own vcpkg.json (this
# directory), sharing the vcpkg tool/cache vendored at the repo root.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# libpqxx's zview type publicly derives from std::string_view; MSVC's
# dllexport propagates to inherited base-class members even when
# they're plain STL inline functions, so building against the default
# dynamic (DLL) triplet on Windows makes basic_string_view::size()/
# data() get exported from pqxx.dll *and* locally instantiated by any
# translation unit that includes the pqxx headers — a link-time
# "already defined" for both. This class of MSVC DLL-boundary issue
# doesn't exist on the dynamic triplets vcpkg picks for macOS/Linux, so
# it's scoped to Windows only rather than switching linkage everywhere.
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg triplet")
    # x64-windows-static's packages are built against the static CRT
    # (/MT, /MTd) — CMake defaults to the dynamic CRT (/MD, /MDd)
    # otherwise, which links fine but then every object file disagrees
    # with every vcpkg .lib on which CRT it wants (LNK2038).
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
endif()

project(ChatService LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Maximum warnings + pedantic mode — same project-wide rule as DeviceHub.
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

find_package(httplib CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

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
)

# vcpkg's ixwebsocket defaults to an mbedTLS backend on Windows (its own
# vcpkg.json maps the "ssl" feature to "mbedtls" for platform:windows —
# macOS gets "sectransp" instead, which is why this never surfaced in
# CI). mbedTLS's Windows entropy source calls BCryptGenRandom(), which
# lives in bcrypt.dll — without linking against it, the final binary
# fails with an unresolved external symbol.
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
)
target_link_libraries(chat_service_tests PRIVATE
    chat_service_core
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(chat_service_tests)
