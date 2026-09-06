#include "ui/CallWindow.h"

#include <gtest/gtest.h>

#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QVideoWidget>

#include "ui/DraggableVideoTile.h"

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

// Видимость переключается на обёртке DraggableVideoTile, а не на самом
// localVideoWidget()/localScreenShareVideoWidget() (issue #185, часть
// про перетаскиваемые плитки) — content_->isHidden() не отражает
// скрытие родителя (Qt не выставляет этот флаг на детях, когда прячет
// сам контейнер), поэтому эти тесты проверяют parentWidget()
// (саму плитку), а не сам видео-виджет.

TEST(CallWindowTest, SetVideoEnabledTogglesButtonLabelAndLocalPreviewVisibility) {
    CallWindow window;

    window.setVideoEnabled(true);
    EXPECT_EQ(window.videoToggleButton()->text(), QStringLiteral("Disable Video"));
    EXPECT_FALSE(window.localVideoWidget()->parentWidget()->isHidden());

    window.setVideoEnabled(false);
    EXPECT_EQ(window.videoToggleButton()->text(), QStringLiteral("Enable Video"));
    EXPECT_TRUE(window.localVideoWidget()->parentWidget()->isHidden());
}

TEST(CallWindowTest, SetScreenShareEnabledTogglesButtonLabelAndLocalPreviewVisibility) {
    CallWindow window;

    window.setScreenShareEnabled(true);
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Stop Sharing"));
    EXPECT_FALSE(window.localScreenShareVideoWidget()->parentWidget()->isHidden());

    window.setScreenShareEnabled(false);
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Share Screen"));
    EXPECT_TRUE(window.localScreenShareVideoWidget()->parentWidget()->isHidden());
}

TEST(CallWindowTest, VideoAndScreenShareLocalPreviewsAreIndependent) {
    // issue #185: камера и демонстрация экрана больше не взаимоисключают
    // друг друга — включение одной не трогает видимость другой, и обе
    // локальные плитки могут быть видны одновременно.
    CallWindow window;

    window.setVideoEnabled(true);
    window.setScreenShareEnabled(true);
    EXPECT_FALSE(window.localVideoWidget()->parentWidget()->isHidden());
    EXPECT_FALSE(window.localScreenShareVideoWidget()->parentWidget()->isHidden());

    window.setVideoEnabled(false);
    EXPECT_TRUE(window.localVideoWidget()->parentWidget()->isHidden());
    EXPECT_FALSE(window.localScreenShareVideoWidget()->parentWidget()->isHidden());
}

TEST(CallWindowTest, LocalPreviewTilesAreDraggable) {
    // issue #185: обе локальные плитки — DraggableVideoTile, а не голый
    // QVideoWidget напрямую в layout'е.
    CallWindow window;

    EXPECT_NE(qobject_cast<DraggableVideoTile*>(window.localVideoWidget()->parentWidget()), nullptr);
    EXPECT_NE(qobject_cast<DraggableVideoTile*>(window.localScreenShareVideoWidget()->parentWidget()), nullptr);
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

    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);

    EXPECT_NE(window.findChild<QLabel*>(QStringLiteral("remoteVideoTile")), nullptr);
}

TEST(CallWindowTest, CameraAndScreenShareFromTheSamePeerGetSeparateTiles) {
    // issue #185: удалённая камера и демонстрация экрана одного и того
    // же участника — два независимых видеотрека, каждый со своей
    // плиткой, а не общей.
    CallWindow window;

    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/true);

    EXPECT_EQ(window.findChildren<QLabel*>(QStringLiteral("remoteVideoTile")).size(), 2);

    window.removeRemoteVideo(QStringLiteral("alice"), /*isScreenShare=*/true);

    EXPECT_EQ(window.findChildren<QLabel*>(QStringLiteral("remoteVideoTile")).size(), 1);
}

TEST(CallWindowTest, RemoveRemoteVideoDropsThatParticipantsTile) {
    CallWindow window;
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);

    window.removeRemoteVideo(QStringLiteral("alice"), /*isScreenShare=*/false);

    EXPECT_EQ(window.findChild<QLabel*>(QStringLiteral("remoteVideoTile")), nullptr);
}

TEST(CallWindowTest, ResetForNewCallClearsRemoteTilesAndParticipants) {
    CallWindow window;
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
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
