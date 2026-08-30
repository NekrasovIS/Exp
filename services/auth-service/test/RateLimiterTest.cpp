#include "RateLimiter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace auth_service {
namespace {

TEST(RateLimiterTest, AllowsUpToTheLimitThenRejects) {
    RateLimiter limiter(/*maxRequestsPerWindow=*/3, std::chrono::seconds{60});

    EXPECT_TRUE(limiter.allow("1.2.3.4"));
    EXPECT_TRUE(limiter.allow("1.2.3.4"));
    EXPECT_TRUE(limiter.allow("1.2.3.4"));
    EXPECT_FALSE(limiter.allow("1.2.3.4"));
}

TEST(RateLimiterTest, TracksEachKeyIndependently) {
    RateLimiter limiter(/*maxRequestsPerWindow=*/1, std::chrono::seconds{60});

    EXPECT_TRUE(limiter.allow("1.2.3.4"));
    EXPECT_FALSE(limiter.allow("1.2.3.4"));
    // A different key has its own, untouched budget.
    EXPECT_TRUE(limiter.allow("5.6.7.8"));
}

TEST(RateLimiterTest, AllowsAgainOnceTheWindowElapses) {
    RateLimiter limiter(/*maxRequestsPerWindow=*/1, std::chrono::milliseconds{50});

    EXPECT_TRUE(limiter.allow("1.2.3.4"));
    EXPECT_FALSE(limiter.allow("1.2.3.4"));

    std::this_thread::sleep_for(std::chrono::milliseconds{75});
    EXPECT_TRUE(limiter.allow("1.2.3.4"));
}

}  // namespace
}  // namespace auth_service
