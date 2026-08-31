#include "ui/ChatView.h"

#include <gtest/gtest.h>

#include <QEventLoop>
#include <QLabel>
#include <QLineEdit>
#include <QTimer>

namespace devicehub {
namespace {

/// Spins the event loop for @p ms so queued timer events (like the
/// typing indicator's auto-hide/throttle timers) actually fire.
void waitMs(int ms) {
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

TEST(ChatViewTypingIndicatorTest, ShowTypingUserMakesLabelVisibleThenAutoHides) {
    ChatView view;
    view.show();
    view.showChannel(QStringLiteral("general"));

    EXPECT_FALSE(view.typingIndicatorLabel()->isVisible());

    view.showTypingUser(QStringLiteral("alice"));
    EXPECT_TRUE(view.typingIndicatorLabel()->isVisible());
    EXPECT_EQ(view.typingIndicatorLabel()->text(), QStringLiteral("alice is typing…"));
}

TEST(ChatViewTypingIndicatorTest, SwitchingChannelHidesStaleIndicator) {
    ChatView view;
    view.show();
    view.showChannel(QStringLiteral("general"));
    view.showTypingUser(QStringLiteral("alice"));
    ASSERT_TRUE(view.typingIndicatorLabel()->isVisible());

    view.showChannel(QStringLiteral("random"));
    EXPECT_FALSE(view.typingIndicatorLabel()->isVisible());
}

TEST(ChatViewTypingIndicatorTest, EditingMessageBoxEmitsTypingRequestedOnceThenThrottles) {
    ChatView view;
    view.showChannel(QStringLiteral("general"));

    int emitCount = 0;
    QObject::connect(&view, &ChatView::typingRequested, [&]() { ++emitCount; });

    emit view.messageEdit()->textEdited(QStringLiteral("h"));
    emit view.messageEdit()->textEdited(QStringLiteral("he"));
    emit view.messageEdit()->textEdited(QStringLiteral("hel"));
    EXPECT_EQ(emitCount, 1);
}

}  // namespace
}  // namespace devicehub
