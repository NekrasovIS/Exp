#include "ChatRepository.h"
#include "ChatService.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <string>

// Требует работающий Postgres (см. docker-compose.yml,
// chat-service-postgres), доступный по CHAT_SERVICE_DATABASE_URL
// (значение по умолчанию соответствует docker-compose.yml). Пропускает
// себя, а не падает, если он не запущен.

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
    EXPECT_TRUE(service.joinCommunity(community.id, login));  // идемпотентно
    EXPECT_FALSE(service.joinCommunity(-1, login));           // нет такого сообщества

    const std::optional<Message> posted = service.postMessage(*channelId, login, "hello, chat-service");
    ASSERT_TRUE(posted.has_value());
    EXPECT_EQ(posted->authorLogin, login);
    EXPECT_EQ(posted->body, "hello, chat-service");

    EXPECT_FALSE(service.postMessage(-1, login, "nowhere").has_value());  // нет такого канала

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

    // Три сообщения, от старых к новым: "one", "two", "three".
    ASSERT_TRUE(service.postMessage(*channelId, owner, "one").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "two").has_value());
    const std::optional<Message> three = service.postMessage(*channelId, owner, "three");
    ASSERT_TRUE(three.has_value());

    // Самая новая страница, limit 1: только "three".
    const std::vector<Message> newestPage = service.recentMessages(*channelId, 1);
    ASSERT_EQ(newestPage.size(), 1U);
    EXPECT_EQ(newestPage[0].body, "three");

    // Страница до "three", limit 1: "two" — beforeId листает историю назад.
    const std::vector<Message> olderPage = service.recentMessages(*channelId, 1, three->id);
    ASSERT_EQ(olderPage.size(), 1U);
    EXPECT_EQ(olderPage[0].body, "two");

    // Страница до самого старого сообщения: ничего не осталось.
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

    // Не-владельцам запрещено, а не просто молча игнорируется.
    EXPECT_EQ(service.renameCommunity(community.id, "hijacked", intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.deleteCommunity(community.id, intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.renameChannel(*channelId, "hijacked", intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.deleteChannel(*channelId, intruder), MutationResult::kForbidden);

    // Несуществующие id сообщаются отдельно от "forbidden".
    EXPECT_EQ(service.renameCommunity(-1, "nowhere", owner), MutationResult::kNotFound);
    EXPECT_EQ(service.deleteCommunity(-1, owner), MutationResult::kNotFound);
    EXPECT_EQ(service.renameChannel(-1, "nowhere", owner), MutationResult::kNotFound);
    EXPECT_EQ(service.deleteChannel(-1, owner), MutationResult::kNotFound);

    // Переименование канала в имя, уже занятое в том же сообществе, даёт конфликт.
    const std::optional<std::int64_t> secondChannelId = service.createChannel(community.id, "random", owner);
    ASSERT_TRUE(secondChannelId.has_value());
    EXPECT_EQ(service.renameChannel(*secondChannelId, "general", owner), MutationResult::kConflict);

    // Владелец действительно может переименовывать и удалять.
    EXPECT_EQ(service.renameChannel(*channelId, "renamed-by-owner", owner), MutationResult::kSuccess);
    const std::vector<Channel> channelsAfterRename = service.listChannels(community.id);
    EXPECT_TRUE(std::any_of(channelsAfterRename.begin(), channelsAfterRename.end(),
                             [](const Channel& channel) { return channel.name == "renamed-by-owner"; }));

    EXPECT_EQ(service.deleteChannel(*channelId, owner), MutationResult::kSuccess);
    EXPECT_EQ(service.renameCommunity(community.id, "renamed-by-owner-community", owner), MutationResult::kSuccess);
    EXPECT_EQ(service.deleteCommunity(community.id, owner), MutationResult::kSuccess);

    // Удаление сообщества каскадно затрагивает и оставшийся канал.
    EXPECT_EQ(service.renameChannel(*secondChannelId, "anything", owner), MutationResult::kNotFound);
}

TEST(ChatServiceIntegrationTest, ListCommunitiesIncludesCreatedCommunity) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    Community created{};
    try {
        created = service.createCommunity("integration-test-listed-" + suffix, "integration-test-owner-" + suffix);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    const std::vector<Community> communities = service.listCommunities();

    EXPECT_TRUE(std::any_of(communities.begin(), communities.end(),
                             [&](const Community& community) { return community.id == created.id; }));
}

TEST(ChatServiceIntegrationTest, CreateChannelRejectsNonexistentCommunity) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    std::optional<std::int64_t> channelId;
    try {
        channelId = service.createChannel(-1, "nowhere", "integration-test-owner-" + uniqueSuffix());
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_FALSE(channelId.has_value());
}

TEST(ChatServiceIntegrationTest, ListChannelsOfNonexistentCommunityIsEmpty) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    std::vector<Channel> channels;
    try {
        channels = service.listChannels(-1);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_TRUE(channels.empty());
}

TEST(ChatServiceIntegrationTest, RecentMessagesRespectsLimitAndOrdersOldestFirst) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "integration-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("integration-test-limit-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    ASSERT_TRUE(service.postMessage(*channelId, owner, "first").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "second").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "third").has_value());

    const std::vector<Message> limited = service.recentMessages(*channelId, 2);

    ASSERT_EQ(limited.size(), 2U);
    EXPECT_EQ(limited[0].body, "second");
    EXPECT_EQ(limited[1].body, "third");
}

TEST(ChatServiceIntegrationTest, RecentMessagesOfNonexistentChannelIsEmpty) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    std::vector<Message> messages;
    try {
        messages = service.recentMessages(-1, 10);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_TRUE(messages.empty());
}

TEST(ChatServiceIntegrationTest, SearchMessagesFindsCaseInsensitiveSubstringNewestFirst) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "integration-test-search-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("integration-test-search-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    ASSERT_TRUE(service.postMessage(*channelId, owner, "hello world").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "unrelated").has_value());
    ASSERT_TRUE(service.postMessage(*channelId, owner, "WORLD peace").has_value());

    const std::vector<Message> found = service.searchMessages(*channelId, "world", 10);

    ASSERT_EQ(found.size(), 2U);
    EXPECT_EQ(found[0].body, "WORLD peace");
    EXPECT_EQ(found[1].body, "hello world");
}

TEST(ChatServiceIntegrationTest, EditAndDeleteMessageAreRestrictedToTheAuthor) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string author = "edit-test-author-" + suffix;
    const std::string intruder = "edit-test-intruder-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("edit-test-community-" + suffix, author);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", author);
    ASSERT_TRUE(channelId.has_value());

    const std::optional<Message> posted = service.postMessage(*channelId, author, "original body");
    ASSERT_TRUE(posted.has_value());
    EXPECT_FALSE(posted->editedAt.has_value());

    // Даже собственный владелец канала (здесь он же и автор, но суть в
    // том, что правило — авторство, а не владение) не может
    // редактировать/удалять от чужого имени — эмулируем это другим логином.
    EXPECT_EQ(service.editMessage(posted->id, *channelId, intruder, "hijacked").result, MutationResult::kForbidden);
    EXPECT_EQ(service.deleteMessage(posted->id, *channelId, intruder), MutationResult::kForbidden);

    // Несуществующий id и реальный id, но не тот канал, — оба "not found".
    EXPECT_EQ(service.editMessage(-1, *channelId, author, "nowhere").result, MutationResult::kNotFound);
    EXPECT_EQ(service.editMessage(posted->id, -1, author, "wrong channel").result, MutationResult::kNotFound);

    // Автор действительно может редактировать.
    const EditMessageResult edited = service.editMessage(posted->id, *channelId, author, "edited body");
    EXPECT_EQ(edited.result, MutationResult::kSuccess);
    EXPECT_FALSE(edited.editedAt.empty());

    const std::vector<Message> messagesAfterEdit = service.recentMessages(*channelId, 10);
    ASSERT_EQ(messagesAfterEdit.size(), 1U);
    EXPECT_EQ(messagesAfterEdit[0].body, "edited body");
    ASSERT_TRUE(messagesAfterEdit[0].editedAt.has_value());

    // Автор действительно может удалять.
    EXPECT_EQ(service.deleteMessage(posted->id, *channelId, author), MutationResult::kSuccess);
    EXPECT_TRUE(service.recentMessages(*channelId, 10).empty());
    EXPECT_EQ(service.deleteMessage(posted->id, *channelId, author), MutationResult::kNotFound);  // уже удалено
}

TEST(ChatServiceIntegrationTest, PromoteAndDemoteModeratorAreRestrictedToTheOwner) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "moderation-test-owner-" + suffix;
    const std::string intruder = "moderation-test-intruder-" + suffix;
    const std::string target = "moderation-test-target-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("moderation-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_TRUE(service.listModerators(community.id).empty());
    EXPECT_EQ(service.promoteModerator(community.id, target, intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.promoteModerator(-1, target, owner), MutationResult::kNotFound);

    // Владелец может назначить модератора — цель не обязана уже быть
    // участником (promoteModerator() неявно добавляет её в сообщество).
    EXPECT_EQ(service.promoteModerator(community.id, target, owner), MutationResult::kSuccess);
    EXPECT_EQ(service.listModerators(community.id), std::vector<std::string>{target});

    EXPECT_EQ(service.demoteModerator(community.id, target, intruder), MutationResult::kForbidden);
    EXPECT_EQ(service.demoteModerator(community.id, target, owner), MutationResult::kSuccess);
    EXPECT_TRUE(service.listModerators(community.id).empty());

    // Снятие с должности того, кто никогда не был модератором, — безобидная операция без эффекта.
    EXPECT_EQ(service.demoteModerator(community.id, "never-was-a-mod-" + suffix, owner), MutationResult::kSuccess);
}

TEST(ChatServiceIntegrationTest, ModeratorsCanDeleteMessagesAndManageChannelsButNotEditMessagesOrTheCommunity) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "moderation-test-owner2-" + suffix;
    const std::string moderator = "moderation-test-mod-" + suffix;
    const std::string author = "moderation-test-author-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("moderation-test-community2-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", author);
    ASSERT_TRUE(channelId.has_value());
    ASSERT_EQ(service.promoteModerator(community.id, moderator, owner), MutationResult::kSuccess);

    const std::optional<Message> posted = service.postMessage(*channelId, author, "original");
    ASSERT_TRUE(posted.has_value());

    // Модератор не может редактировать чужое сообщение — только удаление.
    EXPECT_EQ(service.editMessage(posted->id, *channelId, moderator, "hijacked").result, MutationResult::kForbidden);

    // Модератор не может переименовывать/удалять само сообщество.
    EXPECT_EQ(service.renameCommunity(community.id, "hijacked", moderator), MutationResult::kForbidden);
    EXPECT_EQ(service.deleteCommunity(community.id, moderator), MutationResult::kForbidden);

    // Модератор МОЖЕТ переименовать/удалить канал, который не создавал.
    EXPECT_EQ(service.renameChannel(*channelId, "renamed-by-moderator", moderator), MutationResult::kSuccess);

    // Модератор МОЖЕТ удалить сообщение, автором которого не является.
    EXPECT_EQ(service.deleteMessage(posted->id, *channelId, moderator), MutationResult::kSuccess);
    EXPECT_TRUE(service.recentMessages(*channelId, 10).empty());

    EXPECT_EQ(service.deleteChannel(*channelId, moderator), MutationResult::kSuccess);
}

TEST(ChatServiceIntegrationTest, AttachmentUploadAndMessageReferenceRoundTrip) {
    const std::string connectionString = envOrDefault(
        "CHAT_SERVICE_DATABASE_URL", "postgresql://chat_service:dev-only-password@localhost:5434/chat_service");

    ChatRepository repository(connectionString);
    ChatService service(repository);

    const std::string suffix = uniqueSuffix();
    const std::string owner = "attachment-test-owner-" + suffix;
    Community community{};
    try {
        community = service.createCommunity("attachment-test-community-" + suffix, owner);
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    const std::optional<std::int64_t> channelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(channelId.has_value());

    // "Hello" в base64 — здесь не нужен настоящий декодер, этот уровень
    // никогда не декодирует, только хранит/возвращает дословно.
    const std::optional<AttachmentMetadata> attachment = service.createAttachment(
        *channelId, AttachmentUpload{.uploaderLogin = owner,
                                      .filename = "greeting.txt",
                                      .contentType = "text/plain",
                                      .dataBase64 = "SGVsbG8=",
                                      .sizeBytes = 5});
    ASSERT_TRUE(attachment.has_value());
    EXPECT_GT(attachment->id, 0);
    EXPECT_EQ(attachment->filename, "greeting.txt");
    EXPECT_EQ(attachment->contentType, "text/plain");
    EXPECT_EQ(attachment->sizeBytes, 5);

    const std::optional<AttachmentData> fetched = service.findAttachmentData(attachment->id);
    ASSERT_TRUE(fetched.has_value());
    EXPECT_EQ(fetched->filename, "greeting.txt");
    EXPECT_EQ(fetched->contentType, "text/plain");
    EXPECT_EQ(fetched->data, "SGVsbG8=");

    EXPECT_FALSE(service.findAttachmentData(-1).has_value());
    EXPECT_FALSE(service
                     .createAttachment(-1, AttachmentUpload{.uploaderLogin = owner,
                                                              .filename = "nowhere.txt",
                                                              .contentType = "text/plain",
                                                              .dataBase64 = "SGVsbG8=",
                                                              .sizeBytes = 5})
                     .has_value());

    const std::optional<Message> posted =
        service.postMessage(*channelId, owner, "check out this file", attachment->id);
    ASSERT_TRUE(posted.has_value());
    ASSERT_TRUE(posted->attachmentId.has_value());
    EXPECT_EQ(*posted->attachmentId, attachment->id);
    ASSERT_TRUE(posted->attachmentFilename.has_value());
    EXPECT_EQ(*posted->attachmentFilename, "greeting.txt");

    // Несуществующий id вложения отклоняется так же, как уже отклоняется
    // несуществующий id канала (foreign_key_violation -> nullopt).
    EXPECT_FALSE(service.postMessage(*channelId, owner, "broken reference", 999999999).has_value());

    // recentMessages() (LEFT JOIN) показывает те же поля вложения.
    const std::vector<Message> messages = service.recentMessages(*channelId, 10);
    ASSERT_EQ(messages.size(), 1U);
    ASSERT_TRUE(messages[0].attachmentId.has_value());
    EXPECT_EQ(*messages[0].attachmentId, attachment->id);
    ASSERT_TRUE(messages[0].attachmentFilename.has_value());
    EXPECT_EQ(*messages[0].attachmentFilename, "greeting.txt");
}

TEST(ChatServiceIntegrationTest, CreateChannelDefaultsToNotEncryptedAndFlagRoundTripsThroughListAndFind) {
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

    const std::optional<std::int64_t> plainChannelId = service.createChannel(community.id, "general", owner);
    ASSERT_TRUE(plainChannelId.has_value());
    const std::optional<std::int64_t> encryptedChannelId =
        service.createChannel(community.id, "secret", owner, /*isEncrypted=*/true);
    ASSERT_TRUE(encryptedChannelId.has_value());

    const std::vector<Channel> channels = service.listChannels(community.id);
    ASSERT_EQ(channels.size(), 2U);
    const auto plain = std::find_if(channels.begin(), channels.end(),
                                     [&](const Channel& c) { return c.id == *plainChannelId; });
    const auto encrypted = std::find_if(channels.begin(), channels.end(),
                                         [&](const Channel& c) { return c.id == *encryptedChannelId; });
    ASSERT_NE(plain, channels.end());
    ASSERT_NE(encrypted, channels.end());
    EXPECT_FALSE(plain->isEncrypted);
    EXPECT_TRUE(encrypted->isEncrypted);

    const std::optional<Channel> foundPlain = service.findChannel(*plainChannelId);
    ASSERT_TRUE(foundPlain.has_value());
    EXPECT_FALSE(foundPlain->isEncrypted);
    const std::optional<Channel> foundEncrypted = service.findChannel(*encryptedChannelId);
    ASSERT_TRUE(foundEncrypted.has_value());
    EXPECT_TRUE(foundEncrypted->isEncrypted);

    EXPECT_FALSE(service.findChannel(-1).has_value());
}

TEST(ChatServiceIntegrationTest, ListMembersReturnsEveryoneWhoJoined) {
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

    const std::string memberA = "integration-test-member-a-" + suffix;
    const std::string memberB = "integration-test-member-b-" + suffix;
    ASSERT_TRUE(service.joinCommunity(community.id, memberA));
    ASSERT_TRUE(service.joinCommunity(community.id, memberB));

    const std::vector<std::string> members = service.listMembers(community.id);
    EXPECT_NE(std::find(members.begin(), members.end(), memberA), members.end());
    EXPECT_NE(std::find(members.begin(), members.end(), memberB), members.end());

    EXPECT_TRUE(service.listMembers(-1).empty());
}

TEST(ChatServiceIntegrationTest, SetChannelKeyRoundTripsThroughFindChannelKeyAndRestrictsToOwnerOrModerator) {
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
    const std::optional<std::int64_t> channelId =
        service.createChannel(community.id, "secret", owner, /*isEncrypted=*/true);
    ASSERT_TRUE(channelId.has_value());

    const std::string member = "integration-test-member-" + suffix;
    const std::string outsider = "integration-test-outsider-" + suffix;
    ASSERT_TRUE(service.joinCommunity(community.id, member));

    // Ключ ещё не установлен — искать нечего.
    EXPECT_FALSE(service.findChannelKey(*channelId, owner).has_value());

    // Владелец оборачивает ключ для себя и для member.
    EXPECT_EQ(service.setChannelKey(*channelId, owner, owner, "wrapped-for-owner"), MutationResult::kSuccess);
    EXPECT_EQ(service.setChannelKey(*channelId, member, owner, "wrapped-for-member"), MutationResult::kSuccess);

    const std::optional<std::string> ownerKey = service.findChannelKey(*channelId, owner);
    ASSERT_TRUE(ownerKey.has_value());
    EXPECT_EQ(*ownerKey, "wrapped-for-owner");
    const std::optional<std::string> memberKey = service.findChannelKey(*channelId, member);
    ASSERT_TRUE(memberKey.has_value());
    EXPECT_EQ(*memberKey, "wrapped-for-member");

    // Никто не обернул ключ для outsider.
    EXPECT_FALSE(service.findChannelKey(*channelId, outsider).has_value());

    // У outsider нет полномочий устанавливать чей-либо ключ в этом канале.
    EXPECT_EQ(service.setChannelKey(*channelId, member, outsider, "forged"), MutationResult::kForbidden);

    // Перезапись существующего ключа успешна (ON CONFLICT DO UPDATE).
    EXPECT_EQ(service.setChannelKey(*channelId, member, owner, "re-wrapped-for-member"), MutationResult::kSuccess);
    const std::optional<std::string> updatedMemberKey = service.findChannelKey(*channelId, member);
    ASSERT_TRUE(updatedMemberKey.has_value());
    EXPECT_EQ(*updatedMemberKey, "re-wrapped-for-member");

    EXPECT_EQ(service.setChannelKey(-1, member, owner, "irrelevant"), MutationResult::kNotFound);
}

}  // namespace
}  // namespace chat_service
