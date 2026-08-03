#include "ChatRepository.h"
#include "ChatService.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Requires a live Postgres (see docker-compose.yml, chat-service-postgres)
// reachable at CHAT_SERVICE_DATABASE_URL (default matches
// docker-compose.yml). Skips itself rather than failing when it isn't
// running.

namespace chat_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

std::string uniqueSuffix() {
    return std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count());
}

TEST(ChatServiceIntegrationTest, CommunityChannelMembershipAndMessageRoundTrip) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    Community community{};
    try {
        community = service.createCommunity("integration-test-community-" + suffix);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_GT(community.id, 0);

    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general");
    ASSERT_TRUE(channelId.has_value());

    const std::vector<Channel> channels = service.listChannels(community.id);
    ASSERT_EQ(channels.size(), 1U);
    EXPECT_EQ(channels[0].name, "general");

    const std::string login = "integration-test-user-" + suffix;
    EXPECT_TRUE(service.joinCommunity(community.id, login));
    EXPECT_TRUE(service.joinCommunity(community.id, login));  // idempotent
    EXPECT_FALSE(service.joinCommunity(-1, login));           // no such community

    const std::optional<Message> posted = service.postMessage(*channelId, login, "hello, chat-service");
    ASSERT_TRUE(posted.has_value());
    EXPECT_EQ(posted->authorLogin, login);
    EXPECT_EQ(posted->body, "hello, chat-service");

    EXPECT_FALSE(service.postMessage(-1, login, "nowhere").has_value());  // no such channel

    const std::vector<Message> messages = service.recentMessages(*channelId, 10);
    ASSERT_EQ(messages.size(), 1U);
    EXPECT_EQ(messages[0].body, "hello, chat-service");
}

}  // namespace
}  // namespace chat_service
