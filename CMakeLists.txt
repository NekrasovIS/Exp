cmake_minimum_required(VERSION 3.21)

# Зависимости (Qt6, GoogleTest) приходят из вендоренного submodule'а
# vcpkg через его манифест (vcpkg.json), а не через системный
# пакетный менеджер — см. CLAUDE.md. Должно быть установлено до
# project().
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

# Максимальный набор предупреждений + pedantic-режим на весь проект, а
# не точечно на отдельные цели.
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
    src/devices/IScreenCaptureBackend.h
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
    src/ui/CommunityConnectDialog.h
    src/ui/CommunityConnectDialog.cpp
    src/ui/ChannelsPanel.h
    src/ui/ChannelsPanel.cpp
    src/ui/MemberListPanel.h
    src/ui/MemberListPanel.cpp
    src/ui/FriendsPanel.h
    src/ui/FriendsPanel.cpp
    src/ui/DirectMessageView.h
    src/ui/DirectMessageView.cpp
    src/ui/ChatView.h
    src/ui/ChatView.cpp
    src/ui/CallWindow.h
    src/ui/CallWindow.cpp
    src/ui/FloatingCallTilesOverlay.h
    src/ui/FloatingCallTilesOverlay.cpp
    src/ui/DraggableVideoTile.h
    src/ui/DraggableVideoTile.cpp
    src/ui/ChatMessageRow.h
    src/ui/ChatMessageRow.cpp
    src/ui/MessageFormatting.h
    src/ui/MessageFormatting.cpp
    src/ui/ChatMessageGrouping.h
    src/ui/ChatMessageGrouping.cpp
    src/ui/ChatBubble.h
    src/ui/ChatBubble.cpp
    src/ui/SettingsDialog.h
    src/ui/SettingsDialog.cpp
    src/ui/ProfileDialog.h
    src/ui/ProfileDialog.cpp
    src/ui/ModeratorsDialog.h
    src/ui/ModeratorsDialog.cpp
    src/ui/SearchDialog.h
    src/ui/SearchDialog.cpp
    src/ui/PinnedMessagesDialog.h
    src/ui/PinnedMessagesDialog.cpp
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

# Захват экрана на Windows идёт напрямую через Windows.Graphics.Capture
# (WinRT), а не через Qt6 QScreenCapture — в этой сборке Qt6 Multimedia
# (vcpkg) backend захвата экрана для Windows не собран, QScreenCapture
# возвращает NotSupportedError на любом экране (issue #154). windowsapp
# — активационные фабрики WinRT для классического (не UWP-упакованного)
# desktop-приложения; d3d11 — устройство/текстуры для копирования
# захваченного кадра в CPU-память.
if(WIN32)
    target_sources(devicehub_core PRIVATE
        src/devices/WindowsScreenCaptureBackend.h
        src/devices/WindowsScreenCaptureBackend.cpp
    )
    target_link_libraries(devicehub_core PUBLIC d3d11 windowsapp)
endif()

# Собственная сборка WebRTC отключает RTTI, поэтому у классов, которые
# она экспортирует (например, AdaptedVideoTrackSource, используемый
# CallVideoTrackSource, issue #72), есть vtable, но нет typeinfo в
# libwebrtc.a. Любой код со включённым RTTI, который определяет
# внестрочные виртуальные функции производного класса — либо
# инстанцирует под него webrtc::RefCountedObject<T> через
# make_ref_counted<T>() — должен сгенерировать typeinfo, ссылающийся на
# typeinfo базового класса, которого не существует, поэтому линковка
# падает. Ограничено только файлами, определяющими такие классы (а не
# всем проектом, как было раньше): сам Qt собран с включённым RTTI, и
# отключение его на весь проект заставляло каждую единицу трансляции,
# работающую с Qt (включая MainWindow.cpp), расходиться с Qt в вопросе
# RTTI, что падало, как только полиморфное исключение/тип пересекал эту
# границу — "Access violation - no RTTI data!" у MSVC (issue #104).
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

# Qt6 из vcpkg собран статически, поэтому плагины не подхватываются в
# рантайме без явной линковки — платформенный (cocoa) и мультимедийный
# backend-плагины подключаются автоматически как часть компонентов
# Widgets/Multimedia, а вот категория "permissions" — нет. Без этого
# QCamera::start()/QMicrophonePermission молча отвечают "not granted",
# ни разу не спросив macOS, потому что Qt даже не может найти плагин
# разрешений, чтобы у него спросить.
if(APPLE)
    qt_import_plugins(DeviceHub
        INCLUDE
            Qt6::QDarwinCameraPermissionPlugin
            Qt6::QDarwinMicrophonePermissionPlugin
    )

    # macOS привязывает выдачу разрешений на камеру/микрофон к подписи
    # кода приложения. Подпись ad-hoc по умолчанию ("-") выводится из
    # содержимого самого бинарника и меняется при каждой пересборке,
    # незаметно инвалидируя разрешения, выданные предыдущей сборке.
    # Подпись стабильной локальной идентичностью вместо этого (см.
    # README о том, как создать её в Keychain Access) сохраняет подпись —
    # и выданные разрешения — одинаковыми между пересборками. По
    # умолчанию оставлено "-", чтобы это не требовало от каждой машины
    # иметь такую идентичность настроенной; задаётся один раз на машину
    # через -DDEVICEHUB_CODESIGN_IDENTITY="<имя идентичности>".
    set(DEVICEHUB_CODESIGN_IDENTITY "-" CACHE STRING
        "Code signing identity for DeviceHub.app. \"-\" (default) is ad-hoc.")
    add_custom_command(TARGET DeviceHub POST_BUILD
        COMMAND codesign --force --deep --sign "${DEVICEHUB_CODESIGN_IDENTITY}"
                "$<TARGET_BUNDLE_DIR:DeviceHub>"
        COMMENT "Code-signing DeviceHub.app (identity: ${DEVICEHUB_CODESIGN_IDENTITY})"
        VERBATIM
    )
endif()

# Qt6 из vcpkg на Windows собран динамически (DLL), в отличие от
# статической сборки для macOS, описанной в комментарии выше — его
# плагины никогда не находятся в рантайме, если их не скопировать рядом
# с исполняемым файлом. Без этого DeviceHub.exe даже не запускается (нет
# платформенного плагина "windows"), а захват камеры молча не даёт
# кадров вместо ошибки (нет мультимедийного backend-плагина). Сам захват
# экрана больше вообще не идёт через этот плагин — issue #154 оказался
# отсутствующим backend'ом QScreenCapture в этой сборке Qt, а не только
# недостающими плагинами, поэтому его заменили на прямую реализацию
# через Windows.Graphics.Capture (см. doc-комментарий
# ScreenCaptureDevice). См. cmake/DeployQtPluginsWindows.cmake — почему
# это самодельное копирование, а не windeployqt (не входит в зависимости
# проекта здесь).
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
    test/MemberListPanelTest.cpp
    test/CommunityConnectDialogTest.cpp
    test/ChannelsPanelTest.cpp
    test/FriendsPanelTest.cpp
    test/DirectMessageViewTest.cpp
    test/ChatViewTest.cpp
    test/CallWindowTest.cpp
    test/FloatingCallTilesOverlayTest.cpp
    test/DraggableVideoTileTest.cpp
    test/ChatMessageRowTest.cpp
    test/ChatBubbleTest.cpp
    test/FooterBarTest.cpp
    test/SettingsDialogTest.cpp
    test/DesktopNotifierTest.cpp
    test/ProfileDialogTest.cpp
    test/UserProfileClientTest.cpp
    test/IdentityKeyStoreTest.cpp
    test/ChannelCryptoTest.cpp
    test/ModeratorsDialogTest.cpp
    test/IconFactoryTest.cpp
    test/ThemeTest.cpp
    test/ChatMessageGroupingTest.cpp
    test/ChatMessageRowMarkdownTest.cpp
    test/MessageFormattingTest.cpp
    test/ChatViewTypingIndicatorTest.cpp
    test/ChatViewMessageHistoryTest.cpp
    test/SearchDialogTest.cpp
    test/PinnedMessagesDialogTest.cpp
    test/LoginWindowTest.cpp
    test/AuthClientIntegrationTest.cpp
    test/ChatClientIntegrationTest.cpp
    test/ChatRestClientTest.cpp
    test/CallAudioDeviceModuleTest.cpp
    test/CallManagerIntegrationTest.cpp
    test/CallVideoTrackSourceTest.cpp
    test/ScreenCaptureDeviceTest.cpp
)
target_link_libraries(devicehub_tests PRIVATE
    devicehub_core
    Qt6::Test
    GTest::gtest
    GTest::gtest_main
)
include(GoogleTest)
gtest_discover_tests(devicehub_tests DISCOVERY_TIMEOUT 60)
