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

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets Network WebSockets Test)
find_package(unofficial-webrtc CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)

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
    src/devices/CallAudioDeviceModule.h
    src/devices/CallAudioDeviceModule.cpp
    src/devices/CallVideoTrackSource.h
    src/devices/CallVideoTrackSource.cpp
    src/auth/AuthClient.h
    src/auth/AuthClient.cpp
    src/chat/ChatClient.h
    src/chat/ChatClient.cpp
    src/chat/ChatRestClient.h
    src/chat/ChatRestClient.cpp
    src/chat/ChannelCrypto.h
    src/chat/ChannelCrypto.cpp
    src/chat/CallManager.h
    src/chat/CallManager.cpp
    src/user/UserProfileClient.h
    src/user/UserProfileClient.cpp
    src/user/IdentityKeyStore.h
    src/user/IdentityKeyStore.cpp
    src/ui/CommunitiesPanel.h
    src/ui/CommunitiesPanel.cpp
    src/ui/ChannelsPanel.h
    src/ui/ChannelsPanel.cpp
    src/ui/ChatView.h
    src/ui/ChatView.cpp
    src/ui/ChatMessageRow.h
    src/ui/ChatMessageRow.cpp
    src/ui/ChatMessageGrouping.h
    src/ui/ChatMessageGrouping.cpp
    src/ui/ChatBubble.h
    src/ui/ChatBubble.cpp
    src/ui/AccountMenu.h
    src/ui/AccountMenu.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ProfileDialog.h
    src/ui/ProfileDialog.cpp
    src/ui/ModeratorsDialog.h
    src/ui/ModeratorsDialog.cpp
    src/ui/SearchDialog.h
    src/ui/SearchDialog.cpp
    src/ui/LoginWindow.h
    src/ui/LoginWindow.cpp
    src/ui/FooterBar.h
    src/ui/FooterBar.cpp
    src/ui/Theme.h
    src/ui/Theme.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/IconFactory.h
    src/ui/IconFactory.cpp
    src/ui/ToastBanner.h
    src/ui/ToastBanner.cpp
    src/ui/NotificationPolicy.h
    src/ui/NotificationPolicy.cpp
    src/ui/DesktopNotifier.h
    src/ui/DesktopNotifier.cpp
)
target_include_directories(devicehub_core PUBLIC src)
target_link_libraries(devicehub_core PUBLIC
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::WebSockets
    unofficial::webrtc::webrtc
    unofficial-sodium::sodium
)

# WebRTC's own build disables RTTI, so classes it exports (e.g.
# AdaptedVideoTrackSource, used by CallVideoTrackSource, issue #72) have a
# vtable but no typeinfo in libwebrtc.a. Any RTTI-enabled code that defines
# a derived class's out-of-line virtuals — or instantiates
# webrtc::RefCountedObject<T> for one via make_ref_counted<T>() — needs to
# emit typeinfo referencing the base's, which doesn't exist, so the linker
# fails. Scoped to just the files that define such classes (rather than
# project-wide, as before): Qt itself is built with RTTI on, and disabling
# it project-wide made every Qt-facing translation unit (MainWindow.cpp
# included) disagree with Qt about RTTI, which crashes as soon as a
# polymorphic exception/type crosses that boundary — MSVC's "Access
# violation - no RTTI data!" (issue #104).
set(devicehub_webrtc_derived_sources
    src/devices/CallAudioDeviceModule.cpp
    src/devices/CallVideoTrackSource.cpp
    src/chat/CallManager.cpp
)
if(MSVC)
    set_source_files_properties(${devicehub_webrtc_derived_sources} PROPERTIES COMPILE_OPTIONS "/GR-")
else()
    set_source_files_properties(${devicehub_webrtc_derived_sources} PROPERTIES COMPILE_OPTIONS "-fno-rtti")
endif()
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

    # macOS ties Camera/Microphone permission grants to the app's code
    # signature. The default ad-hoc signature ("-") is derived from the
    # binary's own content and changes on every rebuild, silently
    # invalidating permissions granted to the previous build. Signing
    # with a stable local identity instead (see README for how to
    # create one in Keychain Access) keeps the signature — and granted
    # permissions — the same across rebuilds. Left as "-" by default so
    # this doesn't require every machine to have that identity set up;
    # set once per machine with
    # -DDEVICEHUB_CODESIGN_IDENTITY="<identity name>".
    set(DEVICEHUB_CODESIGN_IDENTITY "-" CACHE STRING
        "Code signing identity for DeviceHub.app. \"-\" (default) is ad-hoc.")
    add_custom_command(TARGET DeviceHub POST_BUILD
        COMMAND codesign --force --deep --sign "${DEVICEHUB_CODESIGN_IDENTITY}"
                "$<TARGET_BUNDLE_DIR:DeviceHub>"
        COMMENT "Code-signing DeviceHub.app (identity: ${DEVICEHUB_CODESIGN_IDENTITY})"
        VERBATIM
    )
endif()

# vcpkg's Qt6 on Windows is a dynamic (DLL) build, unlike the static
# macOS build the comment above describes — its plugins are never found
# at runtime unless copied next to the executable. Without this,
# DeviceHub.exe doesn't even start (no "windows" platform plugin) and,
# even patched around that, camera/screen capture silently produce no
# frames instead of erroring (no "multimedia" backend plugin — this was
# the real cause behind issue #154's "screen sharing doesn't work").
# See cmake/DeployQtPluginsWindows.cmake for why this is a hand-rolled
# copy rather than windeployqt (not a project dependency here).
if(WIN32)
    add_custom_command(TARGET DeviceHub POST_BUILD
        COMMAND ${CMAKE_COMMAND}
                -DQT6_CORE_DLL=$<TARGET_FILE:Qt6::Core>
                -DDEST_DIR=$<TARGET_FILE_DIR:DeviceHub>
                -P "${CMAKE_SOURCE_DIR}/cmake/DeployQtPluginsWindows.cmake"
        COMMENT "Deploying Qt plugins next to DeviceHub.exe"
        VERBATIM
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
    test/ToastBannerTest.cpp
    test/NotificationPolicyTest.cpp
    test/CommunitiesPanelTest.cpp
    test/ChannelsPanelTest.cpp
    test/ChatViewTest.cpp
    test/ChatMessageRowTest.cpp
    test/ChatBubbleTest.cpp
    test/FooterBarTest.cpp
    test/AccountMenuTest.cpp
    test/ProfileDialogTest.cpp
    test/UserProfileClientTest.cpp
    test/IdentityKeyStoreTest.cpp
    test/ChannelCryptoTest.cpp
    test/ModeratorsDialogTest.cpp
    test/IconFactoryTest.cpp
    test/ThemeTest.cpp
    test/ChatMessageGroupingTest.cpp
    test/ChatMessageRowMarkdownTest.cpp
    test/ChatViewTypingIndicatorTest.cpp
    test/ChatViewMessageHistoryTest.cpp
    test/SearchDialogTest.cpp
    test/LoginWindowTest.cpp
    test/AuthClientIntegrationTest.cpp
    test/ChatClientIntegrationTest.cpp
    test/ChatRestClientTest.cpp
    test/CallAudioDeviceModuleTest.cpp
    test/CallManagerIntegrationTest.cpp
    test/CallVideoTrackSourceTest.cpp
)
target_link_libraries(devicehub_tests PRIVATE
    devicehub_core
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(devicehub_tests DISCOVERY_TIMEOUT 60)
