# Копирует категории Qt-плагинов, реально нужные DeviceHub, рядом с его
# исполняемым файлом на Windows. Qt6 из vcpkg там собран динамически
# (DLL), не статически — плагины лежат в дереве Qt6/plugins/ рядом с
# Qt6Core*.dll и никогда не находятся в рантайме, если их не скопировать
# рядом с exe. windeployqt.exe недоступен (qttools не входит в
# зависимости проекта), и qt.conf, указывающего обратно в дерево vcpkg,
# тоже нет, поэтому этот скрипт вручную делает минимальный эквивалент.
#
# Вызывается как: cmake -DQT6_CORE_DLL=<путь> -DDEST_DIR=<путь> -P этот-файл
# QT6_CORE_DLL — это сама DLL Qt6::Core ($<TARGET_FILE:Qt6::Core>), так
# что корень плагинов выводится из неё, а не берётся из захардкоженного
# пути vcpkg — остаётся верным и для Debug/Release, и для любой
# раскладки vcpkg.

if(NOT DEFINED QT6_CORE_DLL OR NOT DEFINED DEST_DIR)
    message(FATAL_ERROR "DeployQtPluginsWindows.cmake requires -DQT6_CORE_DLL=... -DDEST_DIR=...")
endif()

cmake_path(GET QT6_CORE_DLL PARENT_PATH _qt6_bin_dir)
cmake_path(GET _qt6_bin_dir PARENT_PATH _qt6_prefix_dir)
set(_qt6_plugins_dir "${_qt6_prefix_dir}/Qt6/plugins")

# platforms: обязателен просто для старта (QGuiApplication не может
# инициализироваться без платформенного плагина, DeviceHub падает при
# запуске, если ни один не развёрнут).
# multimedia: backend камеры/захвата экрана (ffmpeg/Windows Media
# Foundation) — без него QCamera/QScreenCapture молча не дают кадров
# вместо ошибки, чем в итоге и оказался issue #154.
# tls: нужен для HTTPS/WSS-соединений с backend-сервисами.
# imageformats: декодирование PNG/JPEG для аватаров и вложений.
foreach(_category IN ITEMS platforms multimedia tls imageformats)
    set(_src "${_qt6_plugins_dir}/${_category}")
    if(EXISTS "${_src}")
        file(COPY "${_src}" DESTINATION "${DEST_DIR}")
    else()
        message(WARNING "Qt plugin category '${_category}' not found at ${_src}, skipping")
    endif()
endforeach()
