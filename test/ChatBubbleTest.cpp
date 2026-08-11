#include "ui/ChatBubble.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

// ChatBubble's real behavior is its paintEvent() fill (gradient for
// own messages, neutral for others) — a rendering concern this
// project's tests don't verify (no pixel sampling). These are
// construction-only smoke tests confirming both variants build
// without crashing.

TEST(ChatBubbleTest, ConstructsForOwnMessage) {
    ChatBubble bubble(/*isOwnMessage=*/true);
    SUCCEED();
}

TEST(ChatBubbleTest, ConstructsForOthersMessage) {
    ChatBubble bubble(/*isOwnMessage=*/false);
    SUCCEED();
}

}  // namespace
}  // namespace devicehub
