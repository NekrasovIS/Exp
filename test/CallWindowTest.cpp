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

TEST(CallWindowTest, ClickingMinimizeButtonEmitsMinimizeRequested) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::minimizeRequested);

    emit window.minimizeButton()->clicked();

    EXPECT_EQ(spy.count(), 1);
}

// Issue #312 — a fixed set of reaction buttons, each emitting
// reactionRequested() with exactly its own emoji, plus a transient
// feed label for incoming reactions.

TEST(CallWindowTest, HasAFixedNonEmptySetOfReactionButtons) {
    CallWindow window;

    EXPECT_GT(window.reactionButtons().size(), 0);
    for (QPushButton* button : window.reactionButtons()) {
        EXPECT_FALSE(button->text().isEmpty());
    }
}

TEST(CallWindowTest, ClickingAReactionButtonEmitsReactionRequestedWithItsOwnEmoji) {
    CallWindow window;
    QSignalSpy spy(&window, &CallWindow::reactionRequested);
    QPushButton* firstButton = window.reactionButtons().first();
    const QString expectedEmoji = firstButton->text();

    emit firstButton->clicked();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), expectedEmoji);
}

TEST(CallWindowTest, ClickingASecondReactionButtonEmitsItsOwnDifferentEmoji) {
    CallWindow window;
    ASSERT_GT(window.reactionButtons().size(), 1);
    QSignalSpy spy(&window, &CallWindow::reactionRequested);
    QPushButton* secondButton = window.reactionButtons().at(1);
    const QString expectedEmoji = secondButton->text();

    emit secondButton->clicked();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), expectedEmoji);
    EXPECT_NE(expectedEmoji, window.reactionButtons().first()->text());
}

TEST(CallWindowTest, ShowReactionMakesTheFeedLabelVisibleWithLoginAndEmoji) {
    CallWindow window;
    window.show();

    EXPECT_FALSE(window.reactionFeedLabel()->isVisible());

    window.showReaction(QStringLiteral("bob"), QStringLiteral("\U0001F389"));

    EXPECT_TRUE(window.reactionFeedLabel()->isVisible());
    EXPECT_TRUE(window.reactionFeedLabel()->text().contains(QStringLiteral("bob")));
    EXPECT_TRUE(window.reactionFeedLabel()->text().contains(QStringLiteral("\U0001F389")));
}

TEST(CallWindowTest, ClosingTheWindowEmitsMinimizeRequestedInsteadOfClosing) {
    // issue #215: closeEvent() перенаправляет на то же самое, что и клик
    // "Minimize" — окно не закрывается само по себе (event->ignore()), а
    // значит close() не должен переводить его в скрытое состояние, если
    // оно перед этим было видимым.
    CallWindow window;
    window.show();
    QSignalSpy spy(&window, &CallWindow::minimizeRequested);

    window.close();

    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(window.isHidden());
}

TEST(CallWindowTest, DetachTilesToMovesActiveTilesToNewHostAndShrinksThem) {
    // issue #215: свёрнутое состояние — плитки переносятся (setParent), а
    // не пересоздаются, и уменьшаются до компактного размера.
    CallWindow window;
    QWidget overlayCanvas;
    window.setVideoEnabled(true);
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
    auto* localTile = qobject_cast<DraggableVideoTile*>(window.localVideoWidget()->parentWidget());
    auto* remoteTile = qobject_cast<DraggableVideoTile*>(
        window.findChild<QLabel*>(QStringLiteral("remoteVideoTile"))->parentWidget());
    const int fullSize = localTile->width();

    window.detachTilesTo(&overlayCanvas);

    EXPECT_EQ(localTile->parentWidget(), &overlayCanvas);
    EXPECT_EQ(remoteTile->parentWidget(), &overlayCanvas);
    EXPECT_LT(localTile->width(), fullSize);
    EXPECT_LT(remoteTile->width(), fullSize);
}

TEST(CallWindowTest, DetachTilesToPreservesDisabledLocalTileVisibility) {
    // issue #215: участник без активного видео не должен внезапно
    // "появиться" только оттого, что звонок свернули — detachTilesTo()
    // переносит плитки как есть, не форсируя их видимыми.
    CallWindow window;
    QWidget overlayCanvas;
    ASSERT_TRUE(window.localVideoWidget()->parentWidget()->isHidden());

    window.detachTilesTo(&overlayCanvas);

    EXPECT_TRUE(window.localVideoWidget()->parentWidget()->isHidden());
}

TEST(CallWindowTest, ReattachTilesRestoresVideoStripAsHostAndFullSize) {
    CallWindow window;
    QWidget overlayCanvas;
    window.setVideoEnabled(true);
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
    auto* localTile = qobject_cast<DraggableVideoTile*>(window.localVideoWidget()->parentWidget());
    auto* remoteTile = qobject_cast<DraggableVideoTile*>(
        window.findChild<QLabel*>(QStringLiteral("remoteVideoTile"))->parentWidget());
    const int fullSize = localTile->width();
    window.detachTilesTo(&overlayCanvas);
    ASSERT_LT(localTile->width(), fullSize);

    window.reattachTiles();

    EXPECT_NE(localTile->parentWidget(), &overlayCanvas);
    EXPECT_NE(remoteTile->parentWidget(), &overlayCanvas);
    EXPECT_EQ(localTile->width(), fullSize);
    EXPECT_EQ(remoteTile->width(), fullSize);
    // Инвариант удалённых плиток (см. removeRemoteVideo()) — всегда видны,
    // пока существуют, независимо от свёрнутого/развёрнутого состояния.
    EXPECT_FALSE(remoteTile->isHidden());
}

TEST(CallWindowTest, TilesCreatedWhileMinimizedGoStraightToTheOverlayHost) {
    // issue #215: новый удалённый видеопоток, впервые появившийся уже
    // после сворачивания, тоже должен попасть в оверлей, а не молча
    // создаться в скрытом videoStrip_.
    CallWindow window;
    QWidget overlayCanvas;
    window.detachTilesTo(&overlayCanvas);

    window.showRemoteVideoFrame(QStringLiteral("bob"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);

    // Плитка теперь живёт под overlayCanvas, а не под window (setParent()
    // в placeTile() переносит её из дерева CallWindow), поэтому искать
    // нужно там же.
    auto* remoteTile = qobject_cast<DraggableVideoTile*>(
        overlayCanvas.findChild<QLabel*>(QStringLiteral("remoteVideoTile"))->parentWidget());
    EXPECT_EQ(remoteTile->parentWidget(), &overlayCanvas);
}

}  // namespace
}  // namespace devicehub
