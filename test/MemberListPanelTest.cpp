#include "ui/MemberListPanel.h"

#include <gtest/gtest.h>

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QListWidget>
#include <QSignalSpy>
#include <QUrl>

#include "user/AvatarCache.h"
#include "user/UserProfileClient.h"

namespace devicehub {
namespace {

QByteArray encodeTinyRedPng() {
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

TEST(MemberListPanelTest, StartsEmptyWithZeroCountTitle) {
    MemberListPanel panel;

    EXPECT_EQ(panel.listWidget()->count(), 0);
    EXPECT_EQ(panel.titleLabel()->text(), QStringLiteral("MEMBERS — 0"));
}

TEST(MemberListPanelTest, SetMembersPopulatesTheListAndTitleCount) {
    MemberListPanel panel;

    panel.setMembers({QStringLiteral("bob"), QStringLiteral("alice")});

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.titleLabel()->text(), QStringLiteral("MEMBERS — 2"));
}

TEST(MemberListPanelTest, SetMembersSortsAlphabeticallyCaseInsensitive) {
    MemberListPanel panel;

    panel.setMembers({QStringLiteral("carol"), QStringLiteral("Alice"), QStringLiteral("bob")});

    ASSERT_EQ(panel.listWidget()->count(), 3);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("Alice"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
    EXPECT_EQ(panel.listWidget()->item(2)->text(), QStringLiteral("carol"));
}

TEST(MemberListPanelTest, SetMembersReplacesPreviousContents) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice")});

    panel.setMembers({QStringLiteral("bob"), QStringLiteral("carol")});

    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("bob"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("carol"));
}

// issue #217: showContextMenu() (private, вызывается через
// customContextMenuRequested()) открывает реальный QMenu::exec()
// (блокирующий модальный вызов) только когда клик пришёлся на
// чужую строку в зашифрованном канале — эти тесты проверяют только
// более ранние guard-условия, ничего не кликая внутри самого меню, по
// тому же ограничению, что и у CommunitiesPanelTest (там его
// контекстное меню тоже не тестируется дальше самого открытия).

TEST(MemberListPanelTest, ContextMenuEmitsNothingWhenClickIsOutsideAnyItem) {
    MemberListPanel panel;
    panel.setChannelEncrypted(true);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    emit panel.listWidget()->customContextMenuRequested(QPoint(0, 10000));

    EXPECT_EQ(spy.count(), 0);
}

TEST(MemberListPanelTest, ContextMenuEmitsNothingWhenChannelIsNotEncrypted) {
    MemberListPanel panel;
    panel.setChannelEncrypted(false);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    const QPoint pos = panel.listWidget()->visualItemRect(panel.listWidget()->item(0)).center();
    emit panel.listWidget()->customContextMenuRequested(pos);

    EXPECT_EQ(spy.count(), 0);
}

// Issue #309 — setOnlineLogins()/setLoginOnline() only change which icon
// is drawn (memberAvatarIcon(..., online)), never the item's text() —
// showContextMenu() above and the sorting tests below both depend on
// text() staying exactly the login.

TEST(MemberListPanelTest, SetOnlineLoginsSurvivesAFollowingSetMembersCall) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});
    panel.setOnlineLogins({QStringLiteral("alice")});

    // A REST refresh of the member list (setMembers()) shouldn't blank
    // out presence state that arrived separately over the WebSocket.
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob"), QStringLiteral("carol")});

    ASSERT_EQ(panel.listWidget()->count(), 3);
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("alice"));
}

TEST(MemberListPanelTest, SetLoginOnlineTogglesWithoutChangingItemText) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});

    panel.setLoginOnline(QStringLiteral("bob"), true);
    ASSERT_EQ(panel.listWidget()->count(), 2);
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));

    panel.setLoginOnline(QStringLiteral("bob"), false);
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
}

TEST(MemberListPanelTest, SetOnlineLoginsReplacesThePreviousSetEntirely) {
    MemberListPanel panel;
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});
    panel.setOnlineLogins({QStringLiteral("alice")});

    panel.setOnlineLogins({QStringLiteral("bob")});

    // Nothing observable from outside except that it doesn't crash and
    // item text is untouched — the icon itself isn't asserted on
    // (QIcon has no equality worth comparing), same limitation as the
    // rest of this file's icon-drawing methods.
    EXPECT_EQ(panel.listWidget()->item(0)->text(), QStringLiteral("alice"));
    EXPECT_EQ(panel.listWidget()->item(1)->text(), QStringLiteral("bob"));
}

TEST(MemberListPanelTest, SetAvatarCacheAppliesARealPhotoOnceItLoadsAndRestoresTheLetterOtherwise) {
    // Issue #384/#442 — не требует сети: UserProfileClient::avatarFetched()
    // вызывается напрямую (Qt-сигналы всегда public-функции), тот же
    // приём, что и в AvatarCacheTest.cpp.
    UserProfileClient client(QUrl(QStringLiteral("http://127.0.0.1:1")));
    AvatarCache cache(client);
    MemberListPanel panel;
    panel.setAvatarCache(&cache);
    panel.setMembers({QStringLiteral("alice"), QStringLiteral("bob")});
    const QImage aliceLetterIcon = panel.listWidget()->item(0)->icon().pixmap(28, 28).toImage();
    const QImage bobLetterIcon = panel.listWidget()->item(1)->icon().pixmap(28, 28).toImage();

    client.avatarFetched(QStringLiteral("alice"), encodeTinyRedPng(), QStringLiteral("image/png"));

    // alice — только что загруженное реальное фото, bob — по-прежнему
    // буква-заглушка (для его login ничего не приходило).
    EXPECT_NE(panel.listWidget()->item(0)->icon().pixmap(28, 28).toImage(), aliceLetterIcon);
    EXPECT_EQ(panel.listWidget()->item(1)->icon().pixmap(28, 28).toImage(), bobLetterIcon);
}

TEST(MemberListPanelTest, ContextMenuEmitsNothingForTheCurrentUsersOwnRow) {
    MemberListPanel panel;
    panel.setCurrentUserLogin(QStringLiteral("alice"));
    panel.setChannelEncrypted(true);
    panel.setMembers({QStringLiteral("alice")});
    QSignalSpy spy(&panel, &MemberListPanel::grantChannelKeyAccessRequested);

    const QPoint pos = panel.listWidget()->visualItemRect(panel.listWidget()->item(0)).center();
    emit panel.listWidget()->customContextMenuRequested(pos);

    EXPECT_EQ(spy.count(), 0);
}

}  // namespace
}  // namespace devicehub
