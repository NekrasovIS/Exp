#pragma once

#include <QIcon>

class QColor;
class QString;

namespace devicehub::ui_icons {

/// Нарисованная вручную иконка "+" (два пересекающихся штриха),
/// рисуется напрямую, а не загружается из SVG-ресурса — статическая
/// сборка Qt6 через vcpkg не подключает плагин SVG icon-engine без
/// дополнительной настройки qt_import_plugins (тот же класс пробела,
/// что и у плагина разрешений камеры), поэтому глиф, нарисованный
/// QPainter, полностью обходит эту зависимость.
QIcon plusIcon(const QColor& strokeColor);

/// Иконка обновления через встроенный в Qt QStyle::SP_BrowserReload —
/// сама рисуется QCommonStyle, а не загружается из ресурса, поэтому не
/// подвержена тому же пробелу с SVG-плагином без ручной отрисовки дуг.
QIcon refreshIcon();

/// Значок-"аватар" со скруглённым квадратом и зелёным градиентом с
/// @p label (первая буква сообщества) по центру белым цветом —
/// используется иконочной полосой CommunitiesPanel вместо текстовой
/// строки.
QIcon communityAvatarIcon(const QString& label);

/// Нарисованная вручную иконка "друзья" (два пересекающихся круга) для
/// кнопки Friends в CommunitiesPanel (issue #187) — тот же приём, что и
/// у plusIcon(): QPainter напрямую, без SVG-плагина.
QIcon friendsIcon(const QColor& strokeColor);

}  // namespace devicehub::ui_icons
