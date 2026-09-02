#include "ui/ChatBubble.h"

#include <gtest/gtest.h>

namespace devicehub {
namespace {

// Реальное поведение ChatBubble — это заливка в paintEvent() (градиент для
// собственных сообщений, нейтральная заливка для чужих) — вопрос рендеринга,
// который тесты этого проекта не проверяют (нет сэмплирования пикселей). Это
// просто smoke-тесты на конструирование, подтверждающие, что оба варианта
// собираются без падения.

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
