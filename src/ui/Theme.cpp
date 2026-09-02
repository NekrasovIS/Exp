#include "ui/Theme.h"

#include <QString>

namespace devicehub::ui_theme {

QString discordDarkStyleSheet() {
    // Акцент — зелёный градиент (в духе изумрудного: #34d399 -> #059669),
    // с более светлым вариантом при наведении и более тёмным при
    // нажатии — QSS не умеет алгоритмически осветлять/затемнять
    // градиент, поэтому каждое состояние прописывает свои собственные
    // stop-точки.
    //
    // Отступы (padding) ниже — это плейсхолдеры %1/%2/%3 для
    // kSpacingSm/Md/Lg (подставляются через .arg() в конце), а не
    // зашитые числа, так что отступы в QSS не могут незаметно
    // разойтись с той же шкалой, которую использует код layout'а на C++.
    return QStringLiteral(R"(
        QMainWindow, QDialog {
            background-color: #1c1e21;
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

        devicehub--CommunitiesPanel {
            background-color: #17191c;
            border-right: 1px solid #2a2d31;
        }

        /* Тот же тон, что и у sidebar/ChannelsPanel (#202327) — оба
           боковых списка (каналы слева, участники справа) читаются как
           одна пара, отдельная от самой тёмной иконочной полосы
           сообществ (#17191c) и от основного содержимого (#1c1e21). */
        devicehub--MemberListPanel {
            background-color: #202327;
            border-left: 1px solid #2a2d31;
        }

        devicehub--ChannelsPanel, devicehub--ChatView {
            background-color: transparent;
        }

        /* QListWidget не имел вообще никакого стиля — список участников
           показывал стандартный светло-серый фон Qt/Windows, а выбранная
           строка/иконка подсвечивалась стандартным синим Windows —
           резко выбивалось из тёмной темы. Скруглённая полупрозрачная
           подсветка вместо этого работает одинаково и для строк
           текстовых списков (каналы, участники), и для круглых иконок
           сообществ (просвечивает как скруглённый квадрат позади круга —
           тот же приём, что и hover-подсветка сервера в Discord).
           padding — только у текстовых списков по имени объекта, чтобы
           не сдвигать точно подобранную сетку иконок сообществ. */
        QListWidget {
            background: transparent;
            border: none;
            outline: 0;
        }

        QListWidget::item {
            border-radius: 8px;
        }

        QListWidget::item:hover {
            background-color: rgba(255, 255, 255, 15);
        }

        QListWidget::item:selected {
            background-color: rgba(255, 255, 255, 25);
        }

        QListWidget::item:selected:hover {
            background-color: rgba(255, 255, 255, 30);
        }

        QListWidget#channelList::item, QListWidget#memberList::item {
            padding: 6px 8px;
        }

        /* QMenu (правый клик по сообществу/каналу, аккаунт в футере) не
           имел вообще никакого стиля — показывал стандартное светлое
           меню Windows. */
        QMenu {
            background-color: #202327;
            color: #e3e6e8;
            border: 1px solid #2a2d31;
            border-radius: 8px;
            padding: 4px;
        }

        QMenu::item {
            padding: 6px 24px 6px 12px;
            border-radius: 6px;
        }

        QMenu::item:selected {
            background-color: #383c41;
        }

        QMenu::item:disabled {
            color: #8b939c;
        }

        QMenu::separator {
            height: 1px;
            background-color: #2a2d31;
            margin: 4px 8px;
        }

        /* QToolTip (например, у иконочных кнопок композера/сайдбара)
           был таким же нестилизованным светлым системным всплытием. */
        QToolTip {
            background-color: #202327;
            color: #e3e6e8;
            border: 1px solid #2a2d31;
            border-radius: 4px;
            padding: 4px 8px;
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

        QLabel[chatAuthor="true"] {
            font-weight: 600;
        }

        QScrollArea#chatMessagesScrollArea, QScrollArea#chatMessagesScrollArea > QWidget > QWidget,
        QWidget#chatMessagesContainer {
            background: transparent;
            border: none;
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

        /* Тонкий скроллбар без стрелок (issue: визуальный проход по
           мотивам Discord) вместо толстого нативного Windows-скроллбара
           — применяется глобально через QApplication::setStyleSheet(),
           так что все прокручиваемые списки/области получают его сразу,
           а не только та, ради которой его добавили. */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0px;
        }

        QScrollBar::handle:vertical {
            background-color: #383c41;
            border-radius: 4px;
            min-height: 24px;
        }

        QScrollBar::handle:vertical:hover {
            background-color: #464b51;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
            background: none;
            border: none;
        }

        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: none;
        }

        QScrollBar:horizontal {
            background: transparent;
            height: 8px;
            margin: 0px;
        }

        QScrollBar::handle:horizontal {
            background-color: #383c41;
            border-radius: 4px;
            min-width: 24px;
        }

        QScrollBar::handle:horizontal:hover {
            background-color: #464b51;
        }

        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0px;
            background: none;
            border: none;
        }

        QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
            background: none;
        }

        QWidget#chatComposer {
            background-color: #17191c;
            border: 1px solid #2a2d31;
            border-radius: 22px;
        }

        QLineEdit[composerInput="true"] {
            background: transparent;
            border: none;
            border-radius: 0;
        }

        QLineEdit[composerInput="true"]:focus {
            border: none;
        }

        QPushButton[composerIcon="true"] {
            background: transparent;
            border: none;
            border-radius: 16px;
            padding: 0;
        }

        QPushButton[composerIcon="true"]:hover {
            background-color: rgba(255, 255, 255, 20);
        }

        QPushButton[composerIcon="true"]:pressed {
            background-color: rgba(255, 255, 255, 35);
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
