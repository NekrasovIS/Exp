#include "ui/CallWindow.h"

#include <gtest/gtest.h>

#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QPair>
#include <QPushButton>
#include <QSet>
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
    EXPECT_FALSE(window.localScreenShareVideoWidget()->isHidden());

    window.setScreenShareEnabled(false);
    EXPECT_EQ(window.screenShareToggleButton()->text(), QStringLiteral("Share Screen"));
    EXPECT_TRUE(window.localScreenShareVideoWidget()->isHidden());
}

TEST(CallWindowTest, VideoAndScreenShareLocalPreviewsAreIndependent) {
    // issue #185: камера и демонстрация экрана больше не взаимоисключают
    // друг друга — включение одной не трогает видимость другой, и обе
    // локальные плитки могут быть видны одновременно.
    CallWindow window;

    window.setVideoEnabled(true);
    window.setScreenShareEnabled(true);
    EXPECT_FALSE(window.localVideoWidget()->isHidden());
    EXPECT_FALSE(window.localScreenShareVideoWidget()->isHidden());

    window.setVideoEnabled(false);
    EXPECT_TRUE(window.localVideoWidget()->isHidden());
    EXPECT_FALSE(window.localScreenShareVideoWidget()->isHidden());
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

TEST(CallWindowTest, NineRemoteTilesDoNotOverlapInTheSameGridCell) {
    // issue #437, регрессия найденного в аудите бага: старая каскадная
    // модель (nextTileCascadePosition(), kCascadeMaxSteps == 8) отдавала
    // 9-й плитке координаты, идентичные 1-й — плитки садились друг на
    // друга ровно в одной точке без какого-либо сигнала об этом. Реальный
    // QGridLayout не может разместить два виджета в одной ячейке, так что
    // с любым числом плиток каждая должна получить свою уникальную
    // (row, column).
    CallWindow window;
    for (int i = 0; i < 9; ++i) {
        window.showRemoteVideoFrame(QStringLiteral("peer%1").arg(i), QImage(4, 4, QImage::Format_ARGB32),
                                     /*isScreenShare=*/false);
    }

    auto* grid = window.findChild<QGridLayout*>();
    ASSERT_NE(grid, nullptr);
    QSet<QPair<int, int>> occupiedCells;
    for (int i = 0; i < grid->count(); ++i) {
        int row = 0;
        int column = 0;
        int rowSpan = 0;
        int columnSpan = 0;
        grid->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
        EXPECT_FALSE(occupiedCells.contains({row, column}));
        occupiedCells.insert({row, column});
    }
    EXPECT_EQ(occupiedCells.size(), 9);
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
    // issue #215/#437: свёрнутое состояние — активные плитки переносятся
    // (addWidget() на новый QGridLayout реparent'ит их) и уменьшаются до
    // компактного размера.
    CallWindow window;
    QWidget overlayCanvas;
    window.setVideoEnabled(true);
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
    QWidget* localTile = window.localVideoWidget();
    QWidget* remoteTile = window.findChild<QLabel*>(QStringLiteral("remoteVideoTile"));
    const int fullSize = localTile->minimumWidth();

    window.detachTilesTo(&overlayCanvas);

    EXPECT_EQ(localTile->parentWidget(), &overlayCanvas);
    EXPECT_EQ(remoteTile->parentWidget(), &overlayCanvas);
    EXPECT_LT(localTile->minimumWidth(), fullSize);
    EXPECT_LT(remoteTile->minimumWidth(), fullSize);
}

TEST(CallWindowTest, DetachTilesToKeepsActiveLocalTileVisible) {
    // issue #287 (и снова актуально для issue #437's грида):
    // QWidget::setParent(), вызванный неявно внутри QGridLayout::addWidget(),
    // скрывает виджет как побочный эффект смены родителя — до фикса #287
    // это молча "выключало" уже включённую камеру/демонстрацию экрана
    // при каждом сворачивании звонка, хотя setVideoEnabled(true) явно
    // просил её показывать. Соседний тест
    // DetachTilesToMovesActiveTilesToNewHostAndShrinksThem проверяет
    // только parentWidget()/размер, не видимость — этот тест закрывает
    // именно её.
    CallWindow window;
    QWidget overlayCanvas;
    window.setVideoEnabled(true);
    window.setScreenShareEnabled(true);
    ASSERT_FALSE(window.localVideoWidget()->isHidden());
    ASSERT_FALSE(window.localScreenShareVideoWidget()->isHidden());

    window.detachTilesTo(&overlayCanvas);

    EXPECT_FALSE(window.localVideoWidget()->isHidden());
    EXPECT_FALSE(window.localScreenShareVideoWidget()->isHidden());
}

TEST(CallWindowTest, DetachTilesToPreservesDisabledLocalTileVisibility) {
    // issue #215: участник без активного видео не должен внезапно
    // "появиться" только оттого, что звонок свернули — detachTilesTo()
    // не добавляет неактивные локальные виджеты в новый грид и не
    // форсирует их видимыми.
    CallWindow window;
    QWidget overlayCanvas;
    ASSERT_TRUE(window.localVideoWidget()->isHidden());

    window.detachTilesTo(&overlayCanvas);

    EXPECT_TRUE(window.localVideoWidget()->isHidden());
}

TEST(CallWindowTest, ReattachTilesRestoresVideoStripAsHostAndFullSize) {
    CallWindow window;
    QWidget overlayCanvas;
    window.setVideoEnabled(true);
    window.showRemoteVideoFrame(QStringLiteral("alice"), QImage(4, 4, QImage::Format_ARGB32),
                                 /*isScreenShare=*/false);
    QWidget* localTile = window.localVideoWidget();
    QWidget* remoteTile = window.findChild<QLabel*>(QStringLiteral("remoteVideoTile"));
    const int fullSize = localTile->minimumWidth();
    window.detachTilesTo(&overlayCanvas);
    ASSERT_LT(localTile->minimumWidth(), fullSize);

    window.reattachTiles();

    EXPECT_NE(localTile->parentWidget(), &overlayCanvas);
    EXPECT_NE(remoteTile->parentWidget(), &overlayCanvas);
    EXPECT_EQ(localTile->minimumWidth(), fullSize);
    EXPECT_EQ(remoteTile->minimumWidth(), fullSize);
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

    // Плитка теперь живёт под overlayCanvas, а не под window
    // (QGridLayout::addWidget() внутри relayoutVideoGrid() реparent'ит
    // её из дерева CallWindow), поэтому искать нужно там же.
    QLabel* remoteTile = overlayCanvas.findChild<QLabel*>(QStringLiteral("remoteVideoTile"));
    ASSERT_NE(remoteTile, nullptr);
    EXPECT_EQ(remoteTile->parentWidget(), &overlayCanvas);
}

}  // namespace
}  // namespace devicehub
