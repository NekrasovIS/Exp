#include "JsonGuard.h"

#include <gtest/gtest.h>

#include <string>

namespace chat_service::json_guard {
namespace {

TEST(JsonGuardTest, ShallowObjectIsWithinLimit) {
    EXPECT_FALSE(exceedsMaxNestingDepth(R"({"token":"x","channel_id":1})", 32));
}

TEST(JsonGuardTest, EmptyPayloadIsWithinLimit) {
    EXPECT_FALSE(exceedsMaxNestingDepth("", 32));
}

TEST(JsonGuardTest, NestingExactlyAtLimitIsWithinLimit) {
    // 32 levels of '[' — depth reaches exactly maxDepth, not beyond it.
    const std::string payload(32, '[');
    EXPECT_FALSE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, NestingOneBeyondLimitExceeds) {
    const std::string payload(33, '[');
    EXPECT_TRUE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, DeeplyNestedArrayExceedsLimit) {
    const std::string payload(500, '[');
    EXPECT_TRUE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, MixedBracketAndBraceNestingExceedsLimit) {
    std::string payload;
    for (int i = 0; i < 200; ++i) {
        payload += (i % 2 == 0) ? "[" : "{";
    }
    EXPECT_TRUE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, BracketsInsideStringLiteralsAreNotCounted) {
    // A string value containing '[' characters shouldn't count toward
    // nesting depth — only structural brackets/braces do.
    const std::string payload = R"({"body":"[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["})";
    EXPECT_FALSE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, EscapedQuoteInsideStringDoesNotEndStringEarly) {
    // If the escaped quote were mishandled, the string would appear to
    // end early and the following '[' characters (still logically inside
    // the string) would be miscounted as structural nesting.
    std::string payload = R"({"body":"a\")";
    for (int i = 0; i < 50; ++i) {
        payload += "[";
    }
    payload += R"("})";
    EXPECT_FALSE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, UnbalancedClosingBracketsDoNotUnderflowOrCrash) {
    const std::string payload(500, ']');
    EXPECT_FALSE(exceedsMaxNestingDepth(payload, 32));
}

}  // namespace
}  // namespace chat_service::json_guard
