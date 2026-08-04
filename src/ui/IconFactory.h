#pragma once

#include <QIcon>

class QColor;

namespace devicehub::ui_icons {

/// Hand-drawn "+" icon (two crossing strokes), painted directly rather
/// than loaded from an SVG resource — vcpkg's static Qt6 build doesn't
/// wire up the SVG icon-engine plugin without extra qt_import_plugins
/// setup (same class of gap as the camera permission plugin), so a
/// QPainter-drawn glyph avoids that dependency entirely.
QIcon plusIcon(const QColor& strokeColor);

/// Refresh icon via Qt's built-in QStyle::SP_BrowserReload — itself
/// painted by QCommonStyle rather than loaded from a resource, so it's
/// exempt from the same SVG-plugin gap without us hand-drawing arcs.
QIcon refreshIcon();

}  // namespace devicehub::ui_icons
