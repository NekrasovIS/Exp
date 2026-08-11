cmake_minimum_required(VERSION 3.21)

# Independently buildable microservice — its own vcpkg.json (this
# directory), sharing the vcpkg tool/cache vendored at the repo root.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
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

add_executable(chat-service src/main.cpp)
target_link_libraries(chat-service PRIVATE chat_service_core)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(chat_service_tests
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
