#include "ui/Theme.h"

#include <QString>

namespace devicehub::ui_theme {

QString discordDarkStyleSheet() {
    // Accent is a green gradient (emerald-ish: #34d399 -> #059669), with a
    // brighter variant on hover and a darker one when pressed — QSS can't
    // algorithmically lighten/darken a gradient, so each state spells out
    // its own stops.
    //
    // Paddings below are %1/%2/%3 placeholders for kSpacingSm/Md/Lg
    // (filled in via .arg() at the end) rather than hardcoded numbers,
    // so QSS spacing can't silently drift from the same scale the C++
    // layout code uses.
    return QStringLiteral(R"(
        QMainWindow, QDialog {
            background-color: #1c1e21;
        }

        QWidget#topBar {
            background-color: #1c1e21;
            border-bottom: 1px solid #2a2d31;
        }

        QWidget#sidebar {
            background-color: #202327;
        }

        QLabel#mainContentPlaceholder {
            color: #e3e6e8;
            font-size: 15px;
            font-weight: 600;
        }

        devicehub--FooterBar {
            background-color: #17191c;
            border-top: 1px solid #2a2d31;
        }

        devicehub--CommunitiesPanel, devicehub--ChannelsPanel, devicehub--ChatView, devicehub--AccountMenu {
            background-color: transparent;
        }

        QLabel {
            color: #e3e6e8;
        }

        QLabel[sectionTitle="true"] {
            color: #8b939c;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#mutedDescription {
            color: #8b939c;
            font-size: 12px;
        }

        QLabel#footerProfileLabel {
            color: #e3e6e8;
            font-weight: 600;
        }

        QLabel#footerAvatar, QLabel#chatChannelTitle[sectionTitle="true"] {
            font-weight: 700;
        }

        QLabel#footerAvatar {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #34d399, stop:1 #059669);
            color: #ffffff;
            border-radius: 14px;
        }

        QPushButton {
            background-color: #2a2d31;
            color: #e3e6e8;
            border: 1px solid #383c41;
            border-radius: 8px;
            padding: %1px %3px;
        }

        QPushButton:hover {
            background-color: #383c41;
            border: 1px solid #464b51;
        }

        QPushButton:pressed {
            background-color: #202327;
        }

        QPushButton[accent="true"] {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #34d399, stop:1 #059669);
            color: #ffffff;
            font-weight: 600;
            border: none;
        }

        QPushButton[accent="true"]:hover {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #4ade80, stop:1 #10b981);
        }

        QPushButton[accent="true"]:pressed {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #059669, stop:1 #047857);
        }

        QPushButton[iconOnly="true"] {
            padding: 0;
        }

        QLineEdit, QComboBox, QPlainTextEdit {
            background-color: #17191c;
            color: #e3e6e8;
            border: 1px solid #2a2d31;
            border-radius: 8px;
            padding: %1px %2px;
            selection-background-color: #10b981;
        }

        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
            border: 1px solid #34d399;
        }

        QComboBox::drop-down {
            border: none;
        }

        QTabWidget::pane {
            border: none;
            background-color: #1c1e21;
        }

        QTabBar::tab {
            background-color: #202327;
            color: #8b939c;
            padding: %1px %3px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
        }

        QTabBar::tab:selected {
            background-color: #1c1e21;
            color: #ffffff;
            border-bottom: 2px solid #34d399;
        }

        QGroupBox {
            border: none;
            margin-top: 16px;
            color: #8b939c;
            font-weight: 700;
            font-size: 12px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            padding: 0 4px;
        }

        QProgressBar {
            background-color: #17191c;
            border: none;
            border-radius: 8px;
            text-align: center;
            color: #e3e6e8;
        }

        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #34d399, stop:1 #059669);
            border-radius: 8px;
        }

        QFrame#accountMenuPopup {
            background-color: #202327;
            border: 1px solid #2a2d31;
            border-radius: 12px;
        }

        QWidget#toastBanner {
            border-radius: 8px;
        }

        QWidget#toastBanner QLabel {
            font-weight: 600;
        }

        QWidget#toastBanner[variant="success"] {
            background-color: #0d3b28;
            border: 1px solid #059669;
        }

        QWidget#toastBanner[variant="success"] QLabel {
            color: #6ee7b7;
        }

        QWidget#toastBanner[variant="error"] {
            background-color: #3f1616;
            border: 1px solid #a32d2d;
        }

        QWidget#toastBanner[variant="error"] QLabel {
            color: #f09595;
        }

        QWidget#toastBanner[variant="info"] {
            background-color: #202327;
            border: 1px solid #383c41;
        }

        QWidget#toastBanner[variant="info"] QLabel {
            color: #e3e6e8;
        }
    )")
        .arg(kSpacingSm)
        .arg(kSpacingMd)
        .arg(kSpacingLg);
}

}  // namespace devicehub::ui_theme
