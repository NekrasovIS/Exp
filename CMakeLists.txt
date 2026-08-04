cmake_minimum_required(VERSION 3.21)

# Dependencies (Qt6, GoogleTest) come from the vendored vcpkg submodule via
# its manifest (vcpkg.json), not a system package manager — see CLAUDE.md.
# Must be set before project().
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

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

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network WebSockets)

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
    src/auth/AuthClient.h
    src/auth/AuthClient.cpp
    src/chat/ChatClient.h
    src/chat/ChatClient.cpp
    src/chat/ChatRestClient.h
    src/chat/ChatRestClient.cpp
    src/ui/CommunitiesPanel.h
    src/ui/CommunitiesPanel.cpp
    src/ui/ChatPanel.h
    src/ui/ChatPanel.cpp
    src/ui/AccountMenu.h
    src/ui/AccountMenu.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/FooterBar.h
    src/ui/FooterBar.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_include_directories(devicehub_core PUBLIC src)
target_link_libraries(devicehub_core PUBLIC
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::WebSockets
)
add_executable(DeviceHub MACOSX_BUNDLE
    src/main.cpp
)
target_link_libraries(DeviceHub PRIVATE devicehub_core)

# vcpkg's Qt6 is built statically, so plugins aren't picked up at
# runtime unless explicitly linked in — the platform (cocoa) and
# multimedia backend plugins ride along automatically as part of the
# Widgets/Multimedia components, but the "permissions" category doesn't.
# Without this, QCamera::start()/QMicrophonePermission silently report
# "not granted" without ever asking macOS, because Qt can't even find a
# permission plugin to check with.
if(APPLE)
    qt_import_plugins(DeviceHub
        INCLUDE
            Qt6::QDarwinCameraPermissionPlugin
            Qt6::QDarwinMicrophonePermissionPlugin
    )
endif()

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
    test/AuthClientIntegrationTest.cpp
    test/ChatClientIntegrationTest.cpp
)
target_link_libraries(devicehub_tests PRIVATE
    devicehub_core
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(devicehub_tests)
