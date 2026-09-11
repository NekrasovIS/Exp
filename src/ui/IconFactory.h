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

/// Значок-"аватар" — круг с зелёным градиентом с @p label (первая буква
/// сообщества) по центру белым
/// цветом — используется иконочной полосой CommunitiesPanel вместо
/// текстовой строки.
QIcon communityAvatarIcon(const QString& label);

/// Тот же круглый аватар, что и communityAvatarIcon(), плюс маленький
/// зелёный кружок с рамкой в цвет фона панели в правом нижнем углу,
/// когда @p online — presence-индикатор участника (issue #309) в
/// MemberListPanel. Отдельная функция, не необязательный параметр у
/// communityAvatarIcon(): та рисует аватары сообществ в CommunitiesPanel,
/// где presence не имеет смысла — общий параметр там был бы всегда
/// false и только шумел бы в каждом вызове.
QIcon memberAvatarIcon(const QString& label, bool online);

/// Стрелка отправки (бумажный самолётик) — рисуется вручную по той же
/// причине, что и plusIcon() (нет SVG icon-engine в статической сборке
/// Qt6). Используется как иконка кнопки "Send" в композере сообщения
/// вместо текстовой подписи.
QIcon sendIcon(const QColor& fillColor);

/// Нарисованная вручную иконка "друзья" (два пересекающихся круга) для
/// кнопки Friends в CommunitiesPanel (issue #187) — тот же приём, что и
/// у plusIcon(): QPainter напрямую, без SVG-плагина.
QIcon friendsIcon(const QColor& strokeColor);

/// Значок-шестерёнка (кольцо с зубцами вокруг, полая середина) —
/// рисуется вручную по той же причине, что и остальные значки в этом
/// файле. Используется как иконка кнопки настроек в FooterBar вместо
/// эмодзи "⚙" в тексте кнопки.
QIcon settingsIcon(const QColor& fillColor);

/// Значок "участники" (два перекрывающихся силуэта человека) — рисуется
/// вручную по той же причине, что и остальные значки в этом файле.
/// Используется кнопкой сворачивания/разворачивания MemberListPanel в
/// шапке ChatView (issue #184).
QIcon membersIcon(const QColor& fillColor);

}  // namespace devicehub::ui_icons
