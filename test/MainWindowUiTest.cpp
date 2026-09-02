#include "ui/MainWindow.h"

#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QTabWidget>

#include "devices/DeviceEnumerator.h"

// Эти тесты намеренно никогда не нажимают кнопку, которая запускала бы
// реальный захват (микрофон/камера/экран): у обычного CLI-бинарника
// devicehub_tests нет описаний использования (usage descriptions) в
// Info.plist, и macOS аварийно завершает процесс, который обращается к этим
// API без них — в отличие от DeviceHub.app. Безопасно проверить без
// открытия устройства можно то, что UI собирается корректно и его списки
// устройств совпадают с тем, что сообщает DeviceEnumerator.

namespace devicehub {
namespace {

TEST(MainWindowUiTest, ConstructsWithoutCrashing) {
    const MainWindow window;
    EXPECT_EQ(window.windowTitle(), QStringLiteral("DeviceHub"));
}

TEST(MainWindowUiTest, LoginWindowIsShownAtStartupSinceNoTokenIsEverPersisted) {
    // Issue #156: there's no stored-token persistence across restarts,
    // so "not authenticated yet" is unconditionally true right after
    // construction — LoginWindow (a separate top-level QDialog, not a
    // widget embedded in MainWindow's own layout, so isVisible() here
    // doesn't depend on MainWindow itself ever being shown) should
    // already be showing.
    const MainWindow window;

    const auto* identifierEdit = window.findChild<QLineEdit*>(QStringLiteral("loginIdentifierEdit"));
    ASSERT_NE(identifierEdit, nullptr);
    EXPECT_TRUE(identifierEdit->window()->isVisible());
}

TEST(MainWindowUiTest, DeviceSettingsAreSeparateTabsInDialog) {
    const MainWindow window;

    auto* tabs = window.findChild<QTabWidget*>("settingsTabs");
    ASSERT_NE(tabs, nullptr);
    EXPECT_EQ(tabs->count(), 4);
}

TEST(MainWindowUiTest, SidebarAndFooterExist) {
    const MainWindow window;

    auto* sidebar = window.findChild<QWidget*>("sidebar");
    auto* mainContentPlaceholder = window.findChild<QLabel*>("mainContentPlaceholder");
    auto* footerProfileLabel = window.findChild<QLabel*>("footerProfileLabel");
    auto* footerSettingsButton = window.findChild<QPushButton*>("footerSettingsButton");

    ASSERT_NE(sidebar, nullptr);
    ASSERT_NE(mainContentPlaceholder, nullptr);
    ASSERT_NE(footerProfileLabel, nullptr);
    ASSERT_NE(footerSettingsButton, nullptr);

    EXPECT_EQ(footerProfileLabel->text(), QStringLiteral("Not signed in"));
}

TEST(MainWindowUiTest, AvatarClickShowsAccountSettingsMenuWithDisabledActionsWhenSignedOut) {
    // Issue #151: клик по аватару в футере открывает меню Edit
    // Profile/Sign Out. Оба пункта имеют смысл только после входа,
    // поэтому оба стартуют отключёнными.
    const MainWindow window;

    auto* avatar = window.findChild<QLabel*>("footerAvatar");
    ASSERT_NE(avatar, nullptr);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(5, 5), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(avatar, &press);

    auto* menu = window.findChild<QMenu*>("accountSettingsMenu");
    ASSERT_NE(menu, nullptr);
    auto* editProfileAction = menu->findChild<QAction*>("editProfileAction");
    auto* signOutAction = menu->findChild<QAction*>("signOutAction");
    ASSERT_NE(editProfileAction, nullptr);
    ASSERT_NE(signOutAction, nullptr);
    EXPECT_FALSE(editProfileAction->isEnabled());
    EXPECT_FALSE(signOutAction->isEnabled());
}

TEST(MainWindowUiTest, DeviceCombosMatchEnumerator) {
    const MainWindow window;
    const DeviceEnumerator enumerator;

    auto* outputCombo = window.findChild<QComboBox*>("outputCombo");
    auto* inputCombo = window.findChild<QComboBox*>("inputCombo");
    auto* cameraCombo = window.findChild<QComboBox*>("cameraCombo");
    auto* screenCombo = window.findChild<QComboBox*>("screenCombo");

    ASSERT_NE(outputCombo, nullptr);
    ASSERT_NE(inputCombo, nullptr);
    ASSERT_NE(cameraCombo, nullptr);
    ASSERT_NE(screenCombo, nullptr);

    EXPECT_EQ(outputCombo->count(), enumerator.audioOutputs().size());
    EXPECT_EQ(inputCombo->count(), enumerator.audioInputs().size());
    EXPECT_EQ(cameraCombo->count(), enumerator.cameras().size());
    EXPECT_EQ(screenCombo->count(), enumerator.screens().size());
}

TEST(MainWindowUiTest, ActionControlsExistWithExpectedInitialLabels) {
    const MainWindow window;

    auto* playToneButton = window.findChild<QPushButton*>("playToneButton");
    auto* toggleMicButton = window.findChild<QPushButton*>("toggleMicButton");
    auto* toggleCameraButton = window.findChild<QPushButton*>("toggleCameraButton");
    auto* toggleScreenCaptureButton = window.findChild<QPushButton*>("toggleScreenCaptureButton");

    ASSERT_NE(playToneButton, nullptr);
    ASSERT_NE(toggleMicButton, nullptr);
    ASSERT_NE(toggleCameraButton, nullptr);
    ASSERT_NE(toggleScreenCaptureButton, nullptr);

    EXPECT_EQ(playToneButton->text(), QStringLiteral("Play test tone"));
    EXPECT_EQ(toggleMicButton->text(), QStringLiteral("Start capture"));
    EXPECT_EQ(toggleCameraButton->text(), QStringLiteral("Start camera"));
    EXPECT_EQ(toggleScreenCaptureButton->text(), QStringLiteral("Start screen capture"));
}

TEST(MainWindowUiTest, LoginWindowPasswordFieldsExistWithPasswordMasked) {
    // Issue #177: вход/регистрация по паролю теперь часть LoginWindow
    // (шаг "Sign in with password instead"), а не отдельного всплывающего
    // AccountMenu.
    const MainWindow window;

    auto* loginEdit = window.findChild<QLineEdit*>("loginPasswordLoginEdit");
    auto* passwordEdit = window.findChild<QLineEdit*>("loginPasswordEdit");
    auto* signInButton = window.findChild<QPushButton*>("passwordSignInButton");
    auto* registerButton = window.findChild<QPushButton*>("loginRegisterButton");

    ASSERT_NE(loginEdit, nullptr);
    ASSERT_NE(passwordEdit, nullptr);
    ASSERT_NE(signInButton, nullptr);
    ASSERT_NE(registerButton, nullptr);
    EXPECT_EQ(passwordEdit->echoMode(), QLineEdit::Password);
    EXPECT_EQ(signInButton->text(), QStringLiteral("Sign In"));
    EXPECT_EQ(registerButton->text(), QStringLiteral("Register"));
}

TEST(MainWindowUiTest, DeviceErrorStatusLabelsExistAndStartEmpty) {
    const MainWindow window;

    auto* micStatusLabel = window.findChild<QLabel*>("micStatusLabel");
    auto* cameraStatusLabel = window.findChild<QLabel*>("cameraStatusLabel");
    auto* screenStatusLabel = window.findChild<QLabel*>("screenStatusLabel");

    ASSERT_NE(micStatusLabel, nullptr);
    ASSERT_NE(cameraStatusLabel, nullptr);
    ASSERT_NE(screenStatusLabel, nullptr);
    EXPECT_TRUE(micStatusLabel->text().isEmpty());
    EXPECT_TRUE(cameraStatusLabel->text().isEmpty());
    EXPECT_TRUE(screenStatusLabel->text().isEmpty());
}

TEST(MainWindowUiTest, ChatMessagingControlsExist) {
    const MainWindow window;

    // #51: плоский лог на QPlainTextEdit заменён прокручиваемым, динамически
    // заполняемым списком сообщений (виджеты ChatMessageRow) — саму логику
    // группировки заголовков см. в ChatMessageGroupingTest.
    auto* chatMessagesContainer = window.findChild<QWidget*>("chatMessagesContainer");
    auto* chatMessageEdit = window.findChild<QLineEdit*>("chatMessageEdit");
    auto* sendChatMessageButton = window.findChild<QPushButton*>("sendChatMessageButton");
    auto* channelTitle = window.findChild<QLabel*>("chatChannelTitle");

    ASSERT_NE(chatMessagesContainer, nullptr);
    ASSERT_NE(chatMessageEdit, nullptr);
    ASSERT_NE(sendChatMessageButton, nullptr);
    ASSERT_NE(channelTitle, nullptr);

    // Иконка без подписи (issue: визуальный проход по мотивам Discord —
    // композер сообщения теперь "таблетка" с иконочными кнопками), но
    // подсказка остаётся текстовой для доступности/тестируемости.
    EXPECT_EQ(sendChatMessageButton->toolTip(), QStringLiteral("Send"));
}

TEST(MainWindowUiTest, AttachFileButtonExists) {
    // #116: кнопка "Attach" расположена в композере сообщения ChatView —
    // про сценарий «загрузить, затем автоматически отправить» см.
    // MainWindow::onAttachFileClicked().
    const MainWindow window;

    auto* attachFileButton = window.findChild<QPushButton*>("attachFileButton");

    ASSERT_NE(attachFileButton, nullptr);
    EXPECT_EQ(attachFileButton->toolTip(), QStringLiteral("Attach"));
}

TEST(MainWindowUiTest, CallControlsExistAndMuteStartsDisabled) {
    // #68: кнопки Call/Mute находятся в заголовке канала ChatView — про то,
    // как MainWindow синхронизирует их текст/состояние доступности с
    // CallManager, см. ChatView::setCallState().
    const MainWindow window;

    auto* callToggleButton = window.findChild<QPushButton*>("callToggleButton");
    auto* muteToggleButton = window.findChild<QPushButton*>("muteToggleButton");

    ASSERT_NE(callToggleButton, nullptr);
    ASSERT_NE(muteToggleButton, nullptr);

    EXPECT_EQ(callToggleButton->text(), QStringLiteral("Call"));
    EXPECT_EQ(muteToggleButton->text(), QStringLiteral("Mute"));
    EXPECT_FALSE(muteToggleButton->isEnabled());
}

TEST(MainWindowUiTest, VideoToggleButtonExistsAndStartsDisabled) {
    // #91: переключатель видео находится в заголовке канала ChatView рядом
    // с Call/Mute — про то, как MainWindow синхронизирует его текст/
    // состояние доступности с CallManager, см. ChatView::setVideoEnabled().
    const MainWindow window;

    auto* videoToggleButton = window.findChild<QPushButton*>("videoToggleButton");

    ASSERT_NE(videoToggleButton, nullptr);
    EXPECT_EQ(videoToggleButton->text(), QStringLiteral("Enable Video"));
    EXPECT_FALSE(videoToggleButton->isEnabled());
}

TEST(MainWindowUiTest, ScreenShareToggleButtonExistsAndStartsDisabled) {
    // #112: зеркально повторяет кнопку переключения видео — в CallManager
    // взаимоисключает её, тот же паттерн UI-сигналов.
    const MainWindow window;

    auto* screenShareToggleButton = window.findChild<QPushButton*>("screenShareToggleButton");

    ASSERT_NE(screenShareToggleButton, nullptr);
    EXPECT_EQ(screenShareToggleButton->text(), QStringLiteral("Share Screen"));
    EXPECT_FALSE(screenShareToggleButton->isEnabled());
}

TEST(MainWindowUiTest, SearchButtonExists) {
    // #118: кнопка "Search" находится в заголовке канала ChatView — про
    // сценарий запроса/результатов см. подключение SearchDialog в MainWindow.
    const MainWindow window;

    auto* searchButton = window.findChild<QPushButton*>("searchButton");

    ASSERT_NE(searchButton, nullptr);
    EXPECT_EQ(searchButton->text(), QStringLiteral("Search"));
}

TEST(MainWindowUiTest, CommunityAndChannelManagementControlsExist) {
    const MainWindow window;

    auto* communityList = window.findChild<QListWidget*>("communityList");
    auto* createCommunityButton = window.findChild<QPushButton*>("createCommunityButton");
    auto* refreshCommunitiesButton = window.findChild<QPushButton*>("refreshCommunitiesButton");
    auto* channelList = window.findChild<QListWidget*>("channelList");
    auto* createChannelButton = window.findChild<QPushButton*>("createChannelButton");
    auto* refreshChannelsButton = window.findChild<QPushButton*>("refreshChannelsButton");

    ASSERT_NE(communityList, nullptr);
    ASSERT_NE(createCommunityButton, nullptr);
    ASSERT_NE(refreshCommunitiesButton, nullptr);
    ASSERT_NE(channelList, nullptr);
    ASSERT_NE(createChannelButton, nullptr);
    ASSERT_NE(refreshChannelsButton, nullptr);

    EXPECT_EQ(communityList->count(), 0);
    EXPECT_EQ(channelList->count(), 0);
    EXPECT_FALSE(createCommunityButton->icon().isNull());
    EXPECT_FALSE(createChannelButton->icon().isNull());
}

TEST(MainWindowUiTest, EmptyStatesOfferAnActionButton) {
    const MainWindow window;

    // CommunitiesPanel теперь представляет собой узкую панель с иконками
    // (#50), на которой нет места для описательного текста пустого
    // состояния — её всегда присутствующая кнопка "+" сама служит призывом к
    // действию, поэтому отдельная кнопка пустого состояния всё ещё есть
    // только у ChannelsPanel и у плейсхолдера ChatView.
    auto* channelEmptyStateButton = window.findChild<QPushButton*>("emptyStateCreateChannelButton");
    auto* placeholderCreateButton = window.findChild<QPushButton*>("placeholderCreateChannelButton");

    ASSERT_NE(channelEmptyStateButton, nullptr);
    ASSERT_NE(placeholderCreateButton, nullptr);

    EXPECT_EQ(channelEmptyStateButton->text(), QStringLiteral("Create channel"));
    EXPECT_EQ(placeholderCreateButton->text(), QStringLiteral("Create channel"));
}

}  // namespace
}  // namespace devicehub
