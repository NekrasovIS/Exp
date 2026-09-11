#include "ui/DirectMessageView.h"

#include <gtest/gtest.h>

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

namespace devicehub {
namespace {

TEST(DirectMessageViewTest, StartsDisabledUntilAThreadIsShown) {
    DirectMessageView view;

    EXPECT_FALSE(view.messageEdit()->isEnabled());
    EXPECT_FALSE(view.sendButton()->isEnabled());
}

TEST(DirectMessageViewTest, ShowThreadEnablesComposingAndClearsMessages) {
    DirectMessageView view;
    view.setMessages({DirectMessageInfo{.id = 1, .author = "alice", .body = "hi", .sentAt = "now"}});

    view.showThread("bob");

    EXPECT_TRUE(view.messageEdit()->isEnabled());
    EXPECT_TRUE(view.sendButton()->isEnabled());
    EXPECT_EQ(view.messagesList()->count(), 0);
}

TEST(DirectMessageViewTest, ShowPlaceholderDisablesComposingAndClearsMessages) {
    DirectMessageView view;
    view.showThread("bob");
    view.setMessages({DirectMessageInfo{.id = 1, .author = "alice", .body = "hi", .sentAt = "now"}});

    view.showPlaceholder();

    EXPECT_FALSE(view.messageEdit()->isEnabled());
    EXPECT_FALSE(view.sendButton()->isEnabled());
    EXPECT_EQ(view.messagesList()->count(), 0);
}

TEST(DirectMessageViewTest, SetMessagesPopulatesTheListInOrder) {
    DirectMessageView view;
    view.showThread("bob");

    view.setMessages({DirectMessageInfo{.id = 1, .author = "alice", .body = "hi", .sentAt = "now"},
                       DirectMessageInfo{.id = 2, .author = "bob", .body = "hello", .sentAt = "now"}});

    ASSERT_EQ(view.messagesList()->count(), 2);
    EXPECT_EQ(view.messagesList()->item(0)->text(), QStringLiteral("alice: hi"));
    EXPECT_EQ(view.messagesList()->item(1)->text(), QStringLiteral("bob: hello"));
}

TEST(DirectMessageViewTest, AppendMessageAddsToTheEnd) {
    DirectMessageView view;
    view.showThread("bob");
    view.setMessages({DirectMessageInfo{.id = 1, .author = "alice", .body = "hi", .sentAt = "now"}});

    view.appendMessage(DirectMessageInfo{.id = 2, .author = "bob", .body = "hello", .sentAt = "now"});

    ASSERT_EQ(view.messagesList()->count(), 2);
    EXPECT_EQ(view.messagesList()->item(1)->text(), QStringLiteral("bob: hello"));
}

TEST(DirectMessageViewTest, ClickingSendWithNonEmptyTextEmitsSendMessageRequestedAndClearsTheField) {
    DirectMessageView view;
    view.showThread("bob");
    view.messageEdit()->setText("hello there");
    QSignalSpy spy(&view, &DirectMessageView::sendMessageRequested);

    view.sendButton()->click();

    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QStringLiteral("hello there"));
    EXPECT_TRUE(view.messageEdit()->text().isEmpty());
}

TEST(DirectMessageViewTest, ClickingSendWithEmptyTextEmitsNothing) {
    DirectMessageView view;
    view.showThread("bob");
    QSignalSpy spy(&view, &DirectMessageView::sendMessageRequested);

    view.sendButton()->click();

    EXPECT_EQ(spy.count(), 0);
}

TEST(DirectMessageViewTest, PressingEnterInMessageEditEmitsSendMessageRequested) {
    DirectMessageView view;
    view.showThread("bob");
    view.messageEdit()->setText("hello there");
    QSignalSpy spy(&view, &DirectMessageView::sendMessageRequested);

    emit view.messageEdit()->returnPressed();

    EXPECT_EQ(spy.count(), 1);
}

// Issue #313 — same three cases as ChatViewTypingIndicatorTest.cpp,
// mirrored here now that DM threads support the same typing protocol.

TEST(DirectMessageViewTest, ShowTypingUserMakesLabelVisible) {
    DirectMessageView view;
    view.show();
    view.showThread("bob");

    EXPECT_FALSE(view.typingIndicatorLabel()->isVisible());

    view.showTypingUser(QStringLiteral("bob"));
    EXPECT_TRUE(view.typingIndicatorLabel()->isVisible());
    EXPECT_EQ(view.typingIndicatorLabel()->text(), QStringLiteral("bob is typing…"));
}

TEST(DirectMessageViewTest, SwitchingThreadHidesStaleIndicator) {
    DirectMessageView view;
    view.show();
    view.showThread("bob");
    view.showTypingUser(QStringLiteral("bob"));
    ASSERT_TRUE(view.typingIndicatorLabel()->isVisible());

    view.showThread("carol");
    EXPECT_FALSE(view.typingIndicatorLabel()->isVisible());
}

TEST(DirectMessageViewTest, EditingMessageBoxEmitsTypingRequestedOnceThenThrottles) {
    DirectMessageView view;
    view.showThread("bob");

    int emitCount = 0;
    QObject::connect(&view, &DirectMessageView::typingRequested, [&]() { ++emitCount; });

    emit view.messageEdit()->textEdited(QStringLiteral("h"));
    emit view.messageEdit()->textEdited(QStringLiteral("hi"));
    emit view.messageEdit()->textEdited(QStringLiteral("hi "));
    EXPECT_EQ(emitCount, 1);
}

}  // namespace
}  // namespace devicehub
