#include "LinkPreviewFetcher.h"

#include <gtest/gtest.h>
#include <httplib.h>

#include <chrono>
#include <string>
#include <thread>

namespace chat_service {
namespace {

constexpr const char* kFixtureHost = "127.0.0.1";
constexpr int kFixturePort = 18099;

/// Небольшой реальный HTTP-сервер, изображающий "внешний сайт", чтобы
/// LinkPreviewFetcher можно было проверить по-настоящему (скачивание,
/// редиректы, обрезание по размеру), не выходя в реальный интернет.
/// Маршруты регистрируются вызывающим кодом до старта — так же, как
/// ScopedServer в HttpServerTest.cpp.
class TestSiteFixture {
public:
    template <typename SetupFn>
    explicit TestSiteFixture(SetupFn&& setup) {
        setup(server_);
        thread_ = std::thread([this] { server_.listen(kFixtureHost, kFixturePort); });
        httplib::Client probe(kFixtureHost, kFixturePort);
        probe.set_connection_timeout(0, 50000);
        for (int attempt = 0; attempt < 100; ++attempt) {
            if (probe.Get("/")) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    ~TestSiteFixture() {
        server_.stop();
        thread_.join();
    }

    TestSiteFixture(const TestSiteFixture&) = delete;
    TestSiteFixture& operator=(const TestSiteFixture&) = delete;

    static std::string baseUrl() { return std::string("http://") + kFixtureHost + ":" + std::to_string(kFixturePort); }

private:
    httplib::Server server_;
    std::thread thread_;
};

/// LinkPreviewFetcher с отключённой SSRF-проверкой — исключительно для
/// того, чтобы тестам было чем дотянуться до TestSiteFixture на
/// 127.0.0.1 (сам по себе приватный адрес). Ни один из
/// production-вызывающих кодов (LinkPreviewService) никогда не передаёт
/// этот override — см. doc-комментарий параметра isAddressAllowed в
/// LinkPreviewFetcher.h.
LinkPreviewFetcher fetcherAllowingLocalFixture(int maxRedirects = 3) {
    return LinkPreviewFetcher(std::chrono::seconds{2}, 256 * 1024, maxRedirects,
                               [](const std::string&) { return true; });
}

TEST(LinkPreviewFetcherTest, FetchReturnsTheBodyForA200Response) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/page", [](const httplib::Request&, httplib::Response& response) {
            response.set_content("<html><head><title>Hi</title></head></html>", "text/html");
        });
    });
    const LinkPreviewFetcher fetcher = fetcherAllowingLocalFixture();

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/page");

    ASSERT_TRUE(body.has_value());
    EXPECT_NE(body->find("<title>Hi</title>"), std::string::npos);
}

TEST(LinkPreviewFetcherTest, FetchFollowsARedirectToItsTarget) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/start", [](const httplib::Request&, httplib::Response& response) {
            response.set_redirect("/final");
        });
        server.Get("/final", [](const httplib::Request&, httplib::Response& response) {
            response.set_content("final content", "text/html");
        });
    });
    const LinkPreviewFetcher fetcher = fetcherAllowingLocalFixture();

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/start");

    ASSERT_TRUE(body.has_value());
    EXPECT_EQ(*body, "final content");
}

TEST(LinkPreviewFetcherTest, FetchGivesUpAfterTooManyRedirects) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/loop", [](const httplib::Request&, httplib::Response& response) {
            response.set_redirect("/loop");
        });
    });
    const LinkPreviewFetcher fetcher = fetcherAllowingLocalFixture(/*maxRedirects=*/2);

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/loop");

    EXPECT_FALSE(body.has_value());
}

TEST(LinkPreviewFetcherTest, FetchTruncatesTheResponseAtTheConfiguredByteLimit) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/big", [](const httplib::Request&, httplib::Response& response) {
            response.set_content(std::string(10000, 'x'), "text/html");
        });
    });
    const LinkPreviewFetcher fetcher(std::chrono::seconds{2}, /*maxResponseBytes=*/100, 3,
                                       [](const std::string&) { return true; });

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/big");

    ASSERT_TRUE(body.has_value());
    EXPECT_LE(body->size(), 100u);
}

TEST(LinkPreviewFetcherTest, FetchReturnsNulloptForANon2xxResponse) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/missing", [](const httplib::Request&, httplib::Response& response) { response.status = 404; });
    });
    const LinkPreviewFetcher fetcher = fetcherAllowingLocalFixture();

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/missing");

    EXPECT_FALSE(body.has_value());
}

TEST(LinkPreviewFetcherTest, FetchRejectsAnUnsupportedScheme) {
    const LinkPreviewFetcher fetcher = fetcherAllowingLocalFixture();

    EXPECT_FALSE(fetcher.fetch("ftp://example.test/").has_value());
    EXPECT_FALSE(fetcher.fetch("file:///etc/passwd").has_value());
}

// Issue #396 — с настройками по умолчанию (никакого override не
// передано) реальный SSRF-guard должен отклонить локальную фикстуру
// ровно потому, что 127.0.0.1 приватный, несмотря на то, что сервер там
// действительно работает и ответил бы 200.
TEST(LinkPreviewFetcherTest, DefaultConstructedFetcherRejectsAPrivateAddressEvenWhenTheServerIsReachable) {
    TestSiteFixture fixture([](httplib::Server& server) {
        server.Get("/page", [](const httplib::Request&, httplib::Response& response) {
            response.set_content("<html></html>", "text/html");
        });
    });
    const LinkPreviewFetcher fetcher;  // default isAddressAllowed — the real SSRF guard.

    const auto body = fetcher.fetch(TestSiteFixture::baseUrl() + "/page");

    EXPECT_FALSE(body.has_value());
}

}  // namespace
}  // namespace chat_service
