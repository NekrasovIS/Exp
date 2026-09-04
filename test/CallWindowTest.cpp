#include "ui/CallWindow.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QVideoWidget>

namespace devicehub {
namespace {

TEST(CallWindowTest, StartsWithDefaultButtonLabels) {
    CallWindow window;

    EXPECT_EQ(window.muteToggleButton()->text(), QStringLiteral("Mute"));
    EXPECT_EQ(window.videoToggleButton()->text(), QStringLiteral("Enable Video"));
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Share Screen"));
    EXPECT_EQ(window.leaveCallButton()->text(), QStringLiteral("Leave call"));
}

TEST(CallWindowTest, SetMutedTogglesButtonLabel) {
    CallWindow window;

    window.setMuted(true);
    EXPECT_EQ(window.muteToggleButton()->text(), QStringLiteral("Unmute"));

    window.setMuted(false);
    EXPECT_EQ(window.muteToggleButton()->text(), QStringLiteral("Mute"));
}

TEST(CallWindowTest, SetVideoEnabledTogglesButtonLabelAndLocalPreviewVisibility) {
    CallWindow window;

    window.setVideoEnabled(true);
    EXPECT_EQ(window.videoToggleButton()->text(), QStringLiteral("Disable Video"));
    EXPECT_FALSE(window.localVideoWidget()->isHidden());

    window.setVideoEnabled(false);
    EXPECT_EQ(window.videoToggleButton()->text(), QStringLiteral("Enable Video"));
    EXPECT_TRUE(window.localVideoWidget()->isHidden());
}

TEST(CallWindowTest, SetScreenShareEnabledTogglesButtonLabelAndLocalPreviewVisibility) {
    CallWindow window;

    window.setScreenShareEnabled(true);
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Stop Sharing"));
    EXPECT_FALSE(window.localVideoWidget()->isHidden());

    window.setScreenShareEnabled(false);
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Share Screen"));
    EXPECT_TRUE(window.localVideoWidget()->isHidden());
}

TEST(CallWindowTest, SetCallParticipantsShowsJoinedNames) {
    CallWindow window;

    window.setCallParticipants({QStringLiteral("alice"), QStringLiteral("bob")});

    // isHidden() отражает явный флаг hide/show независимо от того,
    // отображается ли (никогда не показываемый, в headless-тесте) виджет
    // реально на экране — тот же паттерн см. в ToastBannerTest.cpp.
    EXPECT_FALSE(window.callParticipantsLabel()->isHidden());
    EXPECT_TRUE(window.callParticipantsLabel()->text().contains(QStringLiteral("alice")));
    EXPECT_TRUE(window.callParticipantsLabel()->text().contains(QStringLiteral("bob")));
}

TEST(CallWindowTest, SetCallParticipantsWithEmptyListHidesLabel) {
    CallWindow window;
    window.setCallParticipants({QStringLiteral("alice")});
    ASSERT_FALSE(window.callParticipantsLabel()->isHidden());

    window.setCallParticipants({});

    EXPECT_TRUE(window.callParticipantsLabel()->isHidden());
}

TEST(CallWindowTest, ShowRemoteVideoFrameCreatesATileFindableByObjectName) {
    CallWindow window;

    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32));

    EXPECT_NE(window.findChild<QLabel*>(QStringLiteral("remoteVideoTile")), nullptr);
}

TEST(CallWindowTest, RemoveRemoteVideoDropsThatParticipantsTile) {
    CallWindow window;
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32));

    window.removeRemoteVideo(QStringLiteral("alice"));

    EXPECT_EQ(window.findChild<QLabel*>(QStringLiteral("remoteVideoTile")), nullptr);
}

TEST(CallWindowTest, ResetForNewCallClearsRemoteTilesAndParticipants) {
    CallWindow window;
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32));
    window.setCallParticipants({QStringLiteral("alice")});

    window.resetForNewCall();

    EXPECT_EQ(window.findChild<QLabel*>(QStringLiteral("remoteVideoTile")), nullptr);
    EXPECT_TRUE(window.callParticipantsLabel()->isHidden());
}

TEST(CallWindowTest, ClickingMuteToggleButtonEmitsMuteToggleRequested) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::muteToggleRequested);

    emit window.muteToggleButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(CallWindowTest, ClickingVideoToggleButtonEmitsVideoToggleRequested) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::videoToggleRequested);

    emit window.videoToggleButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(CallWindowTest, ClickingScreenShareToggleButtonEmitsScreenShareToggleRequested) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::screenShareToggleRequested);

    emit window.screenShareToggleButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

TEST(CallWindowTest, ClickingLeaveCallButtonEmitsLeaveCallRequested) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::leaveCallRequested);

    emit window.leaveCallButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

}  // namespace
}  // namespace devicehub
