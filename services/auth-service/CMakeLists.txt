cmake_minimum_required(VERSION 3.21)

# Independently buildable microservice — its own vcpkg.json (this
# directory), sharing the vcpkg tool/cache vendored at the repo root.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(AuthService LANGUAGES CXX)

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

# Source-based code coverage (issue #146) — see chat-service/CMakeLists.txt's
# copy of this same block for the full rationale.
option(DEVICEHUB_ENABLE_COVERAGE "Build with source-based code coverage instrumentation (needs Clang)" OFF)
if(DEVICEHUB_ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "DEVICEHUB_ENABLE_COVERAGE requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate -fcoverage-mapping)
endif()

find_package(httplib CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(auth_service_core
    src/base64_utils.h
    src/base64_utils.cpp
    src/JsonGuard.h
    src/JsonGuard.cpp
    src/TokenService.h
    src/TokenService.cpp
    src/RateLimiter.h
    src/RateLimiter.cpp
    src/HttpServer.h
    src/HttpServer.cpp
    src/UserServiceClient.h
    src/UserServiceClient.cpp
)
target_include_directories(auth_service_core PUBLIC src)
target_link_libraries(auth_service_core PUBLIC
    httplib::httplib
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

add_executable(auth-service src/main.cpp)
target_link_libraries(auth-service PRIVATE auth_service_core)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(auth_service_tests
    test/TokenServiceTest.cpp
    test/RateLimiterTest.cpp
    test/UserServiceClientIntegrationTest.cpp
    test/base64_utilsTest.cpp
    test/HttpServerTest.cpp
    test/JsonGuardTest.cpp
)
target_link_libraries(auth_service_tests PRIVATE
    auth_service_core
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(auth_service_tests)

# Real coverage-guided fuzzing (issue #121/#132/#133) — off by default,
# Clang-only; see chat-service/CMakeLists.txt's copy of this same block
# for the full rationale (MSVC's cl.exe doesn't support -fsanitize=fuzzer).
option(DEVICEHUB_ENABLE_FUZZING "Build libFuzzer targets (needs Clang)" OFF)
if(DEVICEHUB_ENABLE_FUZZING)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "DEVICEHUB_ENABLE_FUZZING requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
    endif()

    add_executable(token_verify_fuzz fuzz/TokenVerifyFuzz.cpp)
    target_link_libraries(token_verify_fuzz PRIVATE auth_service_core)
    target_compile_options(token_verify_fuzz PRIVATE -fsanitize=fuzzer,address)
    target_link_options(token_verify_fuzz PRIVATE -fsanitize=fuzzer,address)
endif()
