cmake_minimum_required(VERSION 3.21)
project(DeviceHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Maximum warnings + pedantic mode for the whole project, not per-target.
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

# On macOS, Qt6 installed via Homebrew isn't on the default CMake search
# path; ask Homebrew where it lives instead of hardcoding a path.
# Other platforms/setups pass CMAKE_PREFIX_PATH in from outside.
if(APPLE)
    execute_process(
        COMMAND brew --prefix qt@6
        OUTPUT_VARIABLE HOMEBREW_QT6_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(HOMEBREW_QT6_PREFIX)
        list(APPEND CMAKE_PREFIX_PATH "${HOMEBREW_QT6_PREFIX}")
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

add_library(devicehub_core
    src/devices/DeviceEnumerator.h
    src/devices/DeviceEnumerator.cpp
    src/devices/AudioOutputDevice.h
    src/devices/AudioOutputDevice.cpp
    src/devices/AudioInputDevice.h
    src/devices/AudioInputDevice.cpp
    src/devices/CameraDevice.h
    src/devices/CameraDevice.cpp
    src/devices/ScreenCaptureDevice.h
    src/devices/ScreenCaptureDevice.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_include_directories(devicehub_core PUBLIC src)
target_link_libraries(devicehub_core PUBLIC
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)
add_executable(DeviceHub MACOSX_BUNDLE
    src/main.cpp
)
target_link_libraries(DeviceHub PRIVATE devicehub_core)

set_target_properties(DeviceHub PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.devicehub.app"
    MACOSX_BUNDLE_BUNDLE_NAME "DeviceHub"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
    MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
)

enable_testing()
find_package(GTest CONFIG REQUIRED)

add_executable(devicehub_tests
    test/QtTestEnvironment.h
    test/QtTestEnvironment.cpp
    test/DeviceEnumeratorTest.cpp
    test/MainWindowUiTest.cpp
)
target_link_libraries(devicehub_tests PRIVATE
    devicehub_core
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(devicehub_tests)
