# Copies the Qt plugin categories DeviceHub actually needs next to its
# executable on Windows. vcpkg's Qt6 there is a dynamic (DLL) build, not
# static — plugins live in a Qt6/plugins/ tree next to Qt6Core*.dll and
# are never found at runtime unless copied alongside the exe. There's no
# windeployqt.exe available (qttools isn't a project dependency), and no
# qt.conf pointing back into the vcpkg tree, so this script does the
# minimal equivalent by hand.
#
# Invoked as: cmake -DQT6_CORE_DLL=<path> -DDEST_DIR=<path> -P this-file
# QT6_CORE_DLL is Qt6::Core's own DLL ($<TARGET_FILE:Qt6::Core>) so the
# plugin root is derived from it rather than a hardcoded vcpkg path —
# stays correct for both Debug/Release configs and any vcpkg layout.

if(NOT DEFINED QT6_CORE_DLL OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DeployQtPluginsWindows.cmake requires -DQT6_CORE_DLL=... -DDEST_DIR=...")
endif()

cmake_path(GET QT6_CORE_DLL PARENT_PATH _qt6_bin_dir)
cmake_path(GET _qt6_bin_dir PARENT_PATH _qt6_prefix_dir)
set(_qt6_plugins_dir "${_qt6_prefix_dir}/Qt6/plugins")

# platforms: mandatory just to start (QGuiApplication can't init without
# a platform plugin, DeviceHub fails at launch with none deployed).
# multimedia: camera/screen-capture backend (ffmpeg/Windows Media
# Foundation) — without it QCamera/QScreenCapture silently produce no
# frames instead of erroring, which is what issue #154 turned out to be.
# tls: needed for HTTPS/WSS connections to the backend services.
# imageformats: decoding PNG/JPEG avatar and attachment images.
foreach(_category IN ITEMS platforms multimedia tls imageformats)
    set(_src "${_qt6_plugins_dir}/${_category}")
    if(EXISTS "${_src}")
        file(COPY "${_src}" DESTINATION "${DEST_DIR}")
    else()
        message(WARNING "Qt plugin category '${_category}' not found at ${_src}, skipping")
    endif()
endforeach()
