#include "LinkPreviewCache.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace chat_service {
namespace {

TEST(LinkPreviewCacheTest, MissesForAnUrlThatWasNeverStored) {
    LinkPreviewCache cache(std::chrono::seconds{60});

    EXPECT_FALSE(cache.get("https://example.test/").has_value());
}

TEST(LinkPreviewCacheTest, ReturnsAStoredEntryBeforeItExpires) {
    LinkPreviewCache cache(std::chrono::seconds{60});
    LinkPreviewResult result;
    result.available = true;
    result.metadata.title = "Example";
    cache.put("https://example.test/", result);

    const auto cached = cache.get("https://example.test/");

    ASSERT_TRUE(cached.has_value());
    EXPECT_TRUE(cached->available);
    EXPECT_EQ(cached->metadata.title, "Example");
}

TEST(LinkPreviewCacheTest, StoresUnavailableResultsAsAHitNotAMiss) {
    LinkPreviewCache cache(std::chrono::seconds{60});
    LinkPreviewResult unavailable;
    unavailable.available = false;
    cache.put("https://broken.test/", unavailable);

    const auto cached = cache.get("https://broken.test/");

    ASSERT_TRUE(cached.has_value());
    EXPECT_FALSE(cached->available);
}

TEST(LinkPreviewCacheTest, ExpiredEntryIsTreatedAsAMiss) {
    LinkPreviewCache cache(std::chrono::seconds{0});
    LinkPreviewResult result;
    result.available = true;
    cache.put("https://example.test/", result);

    // ttl=0 — уже истекла к моменту первого get(); небольшая реальная
    // задержка вместо предположения, что std::chrono::steady_clock::now()
    // между put() и get() гарантированно продвинется хоть на сколько-то
    // без неё на некоторых платформах/таймерах низкого разрешения.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_FALSE(cache.get("https://example.test/").has_value());
}

TEST(LinkPreviewCacheTest, DifferentUrlsAreIndependent) {
    LinkPreviewCache cache(std::chrono::seconds{60});
    LinkPreviewResult resultA;
    resultA.available = true;
    resultA.metadata.title = "A";
    LinkPreviewResult resultB;
    resultB.available = true;
    resultB.metadata.title = "B";
    cache.put("https://a.test/", resultA);
    cache.put("https://b.test/", resultB);

    ASSERT_TRUE(cache.get("https://a.test/").has_value());
    ASSERT_TRUE(cache.get("https://b.test/").has_value());
    EXPECT_EQ(cache.get("https://a.test/")->metadata.title, "A");
    EXPECT_EQ(cache.get("https://b.test/")->metadata.title, "B");
}

TEST(LinkPreviewCacheTest, PuttingAgainReplacesThePreviousEntry) {
    LinkPreviewCache cache(std::chrono::seconds{60});
    LinkPreviewResult first;
    first.available = true;
    first.metadata.title = "First";
    cache.put("https://example.test/", first);

    LinkPreviewResult second;
    second.available = true;
    second.metadata.title = "Second";
    cache.put("https://example.test/", second);

    const auto cached = cache.get("https://example.test/");
    ASSERT_TRUE(cached.has_value());
    EXPECT_EQ(cached->metadata.title, "Second");
}

}  // namespace
}  // namespace chat_service
