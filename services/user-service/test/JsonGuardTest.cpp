#include "JsonGuard.h"

#include <gtest/gtest.h>

#include <string>

namespace user_service::json_guard {
namespace {

TEST(JsonGuardTest, ShallowObjectIsWithinLimit) {
    EXPECT_FALSE(exceedsMaxNestingDepth(R"({"display_name":"x","avatar_url":"y"})", 32));
}

TEST(JsonGuardTest, EmptyPayloadIsWithinLimit) {
    EXPECT_FALSE(exceedsMaxNestingDepth("", 32));
}

TEST(JsonGuardTest, NestingExactlyAtLimitIsWithinLimit) {
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

TEST(JsonGuardTest, BracketsInsideStringLiteralsAreNotCounted) {
    const std::string payload = R"({"display_name":"[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[["})";
    EXPECT_FALSE(exceedsMaxNestingDepth(payload, 32));
}

TEST(JsonGuardTest, EscapedQuoteInsideStringDoesNotEndStringEarly) {
    std::string payload = R"({"display_name":"a\")";
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
}  // namespace user_service::json_guard
