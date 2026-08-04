#include "ui/Theme.h"

#include <QString>

namespace devicehub::ui_theme {

QString discordDarkStyleSheet() {
    return QStringLiteral(R"(
        QMainWindow, QDialog {
            background-color: #313338;
        }

        QWidget#topBar {
            background-color: #313338;
            border-bottom: 1px solid #1e1f22;
        }

        QWidget#sidebar {
            background-color: #2b2d31;
        }

        QLabel#mainContentPlaceholder {
            color: #949ba4;
            font-size: 14px;
        }

        devicehub--FooterBar {
            background-color: #232428;
            border-top: 1px solid #1e1f22;
        }

        devicehub--CommunitiesPanel, devicehub--ChatPanel, devicehub--AccountMenu {
            background-color: transparent;
        }

        QLabel {
            color: #dbdee1;
        }

        QLabel[sectionTitle="true"] {
            color: #949ba4;
            font-size: 12px;
            font-weight: 700;
        }

        QLabel#footerProfileLabel {
            color: #dbdee1;
            font-weight: 600;
        }

        QLabel#footerAvatar {
            background-color: #5865f2;
            color: #ffffff;
            font-weight: 700;
            border-radius: 14px;
        }

        QPushButton {
            background-color: #4e5058;
            color: #dbdee1;
            border: none;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton:hover {
            background-color: #6d6f78;
        }

        QPushButton:pressed {
            background-color: #3f4147;
        }

        QPushButton[accent="true"] {
            background-color: #5865f2;
            color: #ffffff;
            font-weight: 600;
        }

        QPushButton[accent="true"]:hover {
            background-color: #4752c4;
        }

        QPushButton[accent="true"]:pressed {
            background-color: #3c45a5;
        }

        QLineEdit, QComboBox, QPlainTextEdit {
            background-color: #1e1f22;
            color: #dbdee1;
            border: 1px solid #1e1f22;
            border-radius: 4px;
            padding: 6px 8px;
            selection-background-color: #5865f2;
        }

        QLineEdit:focus, QComboBox:focus, QPlainTextEdit:focus {
            border: 1px solid #5865f2;
        }

        QComboBox::drop-down {
            border: none;
        }

        QTabWidget::pane {
            border: none;
            background-color: #313338;
        }

        QTabBar::tab {
            background-color: #2b2d31;
            color: #949ba4;
            padding: 8px 16px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }

        QTabBar::tab:selected {
            background-color: #313338;
            color: #ffffff;
        }

        QGroupBox {
            border: none;
            margin-top: 16px;
            color: #949ba4;
            font-weight: 700;
            font-size: 12px;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            padding: 0 4px;
        }

        QProgressBar {
            background-color: #1e1f22;
            border: none;
            border-radius: 4px;
            text-align: center;
            color: #dbdee1;
        }

        QProgressBar::chunk {
            background-color: #23a55a;
            border-radius: 4px;
        }

        QFrame#accountMenuPopup {
            background-color: #2b2d31;
            border: 1px solid #1e1f22;
            border-radius: 8px;
        }
    )");
}

}  // namespace devicehub::ui_theme
