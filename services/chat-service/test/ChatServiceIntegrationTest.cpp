#include "ChatRepository.h"
#include "ChatService.h"

#include <gtest/gtest.h>

#include <algorithm>
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
    const std::string owner = "integration-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("integration-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_GT(community.id, 0);
    EXPECT_EQ(community.ownerLogin, owner);

    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    const std::vector<Channel> channels = service.listChannels(community.id);
    ASSERT_EQ(channels.size(), 1U);
    EXPECT_EQ(channels[0].name, "general");
    EXPECT_EQ(channels[0].ownerLogin, owner);

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

TEST(ChatServiceIntegrationTest, RecentMessagesWithBeforeIdPagesBackwardThroughHistory) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "pagination-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("pagination-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    // Three messages, oldest to newest: "one", "two", "three".
    ASSERT_TRUE(service.postMessage(*channelId, owner, "one").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "two").has_value());
    const std::optional<Message> three = service.postMessage(*channelId, owner, "three");
    ASSERT_TRUE(three.has_value());

    // Newest page, limit 1: just "three".
    const std::vector<Message> newestPage = service.recentMessages(*channelId, 1);
    ASSERT_EQ(newestPage.size(), 1U);
    EXPECT_EQ(newestPage[0].body, "three");

    // Page before "three", limit 1: "two" — beforeId pages backward.
    const std::vector<Message> olderPage = service.recentMessages(*channelId, 1, three->id);
    ASSERT_EQ(olderPage.size(), 1U);
    EXPECT_EQ(olderPage[0].body, "two");

    // Page before the oldest message: nothing left.
    const std::vector<Message> oneMessagePage = service.recentMessages(*channelId, 10, newestPage[0].id);
    ASSERT_EQ(oneMessagePage.size(), 2U);
    const std::vector<Message> beforeOldest = service.recentMessages(*channelId, 10, oneMessagePage[0].id);
    EXPECT_TRUE(beforeOldest.empty());
}

TEST(ChatServiceIntegrationTest, RenameAndDeleteAreRestrictedToTheOwner) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "integration-test-owner-" + suffix;
    const std::string intruder = "integration-test-intruder-" + suffix;

    Community community{};
    try {
        community = service.createCommunity("integration-test-mutable-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    // Non-owners are forbidden, not just silently ignored.
    EXPECT_EQ(service.renameCommunity(community.id, "hijacked", intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.deleteCommunity(community.id, intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.renameChannel(*channelId, "hijacked", intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.deleteChannel(*channelId, intruder), MutationResult::kForbidden);

    // Nonexistent ids are reported distinctly from "forbidden".
    EXPECT_EQ(service.renameCommunity(-1, "nowhere", owner), MutationResult::kNotFound);
    EXPECT_EQ(service.deleteCommunity(-1, owner), MutationResult::kNotFound);
    EXPECT_EQ(service.renameChannel(-1, "nowhere", owner), MutationResult::kNotFound);
    EXPECT_EQ(service.deleteChannel(-1, owner), MutationResult::kNotFound);

    // Renaming a channel to a name already taken in the same community conflicts.
    const std::optional<std::int64_t> secondChannelId = service.createChannel(community.id, "random", owner);
    ASSERT_TRUE(secondChannelId.has_value());
    EXPECT_EQ(service.renameChannel(*secondChannelId, "general", owner), MutationResult::kConflict);

    // The owner can actually rename and delete.
    EXPECT_EQ(service.renameChannel(*channelId, "renamed-by-owner", owner), MutationResult::kSuccess);
    const std::vector<Channel> channelsAfterRename = service.listChannels(community.id);
    EXPECT_TRUE(std::any_of(channelsAfterRename.begin(), channelsAfterRename.end(),
                             [](const Channel& channel) { return channel.name == "renamed-by-owner"; }));

    EXPECT_EQ(service.deleteChannel(*channelId, owner), MutationResult::kSuccess);
    EXPECT_EQ(service.renameCommunity(community.id, "renamed-by-owner-community", owner), MutationResult::kSuccess);
    EXPECT_EQ(service.deleteCommunity(community.id, owner), MutationResult::kSuccess);

    // Deleting the community cascades to its remaining channel too.
    EXPECT_EQ(service.renameChannel(*secondChannelId, "anything", owner), MutationResult::kNotFound);
}

}  // namespace
}  // namespace chat_service
