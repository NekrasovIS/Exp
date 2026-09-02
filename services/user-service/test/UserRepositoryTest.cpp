#include "UserRepository.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

// Требует работающий Postgres (см. docker-compose.yml), доступный по
// USER_SERVICE_DATABASE_URL (значение по умолчанию совпадает с docker-compose.yml).
// Пропускает себя вместо падения, если он не запущен.

namespace user_service {
namespace {

std::string envOrDefault(const char* name, const std::string& defaultValue) {
    const char* value = std::getenv(name);
    return value != nullptr ? std::string(value) : defaultValue;
}

std::string connectionString() {
    return envOrDefault("USER_SERVICE_DATABASE_URL",
                         "postgresql://user_service:dev-only-password@localhost:5433/user_service");
}

std::string uniqueLogin(const std::string& prefix) {
    return prefix + "-" +
           std::to_string(
               std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                   .count());
}

TEST(UserRepositoryTest, CreateUserSucceedsForFreshLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-fresh");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_TRUE(created);
}

TEST(UserRepositoryTest, CreateUserRejectsDuplicateLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-dup");

    bool firstCreated = false;
    try {
        firstCreated = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(firstCreated);

    EXPECT_FALSE(repository.createUser(login, "a-different-hash"));
}

TEST(UserRepositoryTest, FindPasswordHashReturnsStoredHashForExistingLogin) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-find");

    bool created = false;
    try {
        created = repository.createUser(login, "the-stored-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);

    const std::optional<std::string> hash = repository.findPasswordHash(login);

    ASSERT_TRUE(hash.has_value());
    EXPECT_EQ(*hash, "the-stored-hash");
}

TEST(UserRepositoryTest, FindPasswordHashReturnsNulloptForNonexistentLogin) {
    UserRepository repository(connectionString());

    std::optional<std::string> hash;
    try {
        hash = repository.findPasswordHash("no-such-login-ever-created");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_FALSE(hash.has_value());
}

TEST(UserRepositoryTest, FindProfileReturnsNulloptFieldsForFreshUser) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-profile-fresh");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);

    const std::optional<Profile> profile = repository.findProfile(login);

    ASSERT_TRUE(profile.has_value());
    EXPECT_EQ(profile->login, login);
    EXPECT_FALSE(profile->displayName.has_value());
    EXPECT_FALSE(profile->avatarUrl.has_value());
}

TEST(UserRepositoryTest, FindProfileReturnsNulloptForNonexistentLogin) {
    UserRepository repository(connectionString());

    std::optional<Profile> profile;
    try {
        profile = repository.findProfile("no-such-login-ever-created");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_FALSE(profile.has_value());
}

TEST(UserRepositoryTest, UpdateProfileRoundTripsThroughFindProfile) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-profile-update");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);

    ASSERT_EQ(repository.updateProfile(
                  login, ProfileUpdate{.displayName = "Alice",
                                        .avatarUrl = "https://example.test/alice.png",
                                        .publicKey = "base64-x25519-public-key"}),
              UpdateProfileResult::kUpdated);

    const std::optional<Profile> profile = repository.findProfile(login);
    ASSERT_TRUE(profile.has_value());
    ASSERT_TRUE(profile->displayName.has_value());
    EXPECT_EQ(*profile->displayName, "Alice");
    ASSERT_TRUE(profile->avatarUrl.has_value());
    EXPECT_EQ(*profile->avatarUrl, "https://example.test/alice.png");
    ASSERT_TRUE(profile->publicKey.has_value());
    EXPECT_EQ(*profile->publicKey, "base64-x25519-public-key");
}

TEST(UserRepositoryTest, UpdateProfileReturnsNoSuchUserForNonexistentLogin) {
    UserRepository repository(connectionString());

    UpdateProfileResult result = UpdateProfileResult::kUpdated;
    try {
        result = repository.updateProfile("no-such-login-ever-created", ProfileUpdate{.displayName = "Anyone"});
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_EQ(result, UpdateProfileResult::kNoSuchUser);
}

TEST(UserRepositoryTest, UpdateProfileReturnsEmailTakenForDuplicateEmail) {
    UserRepository repository(connectionString());
    const std::string loginA = uniqueLogin("user-repository-test-email-taken-a");
    const std::string loginB = uniqueLogin("user-repository-test-email-taken-b");
    const std::string email = uniqueLogin("user-repository-test-email-taken") + "@example.test";

    bool createdA = false;
    try {
        createdA = repository.createUser(loginA, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(createdA);
    ASSERT_TRUE(repository.createUser(loginB, "some-hash"));
    ASSERT_EQ(repository.updateProfile(loginA, ProfileUpdate{.email = email}), UpdateProfileResult::kUpdated);

    EXPECT_EQ(repository.updateProfile(loginB, ProfileUpdate{.email = email}), UpdateProfileResult::kEmailTaken);
}

TEST(UserRepositoryTest, ResolveOtpIdentifierFindsUserByLoginWhenEmailIsSet) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-otp-login");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);
    const std::string email = login + "@example.test";
    ASSERT_EQ(repository.updateProfile(login, ProfileUpdate{.email = email}), UpdateProfileResult::kUpdated);

    const auto resolved = repository.resolveOtpIdentifier(login);

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->login, login);
    ASSERT_TRUE(resolved->email.has_value());
    EXPECT_EQ(*resolved->email, email);
}

TEST(UserRepositoryTest, ResolveOtpIdentifierFindsUserByEmail) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-otp-email");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);
    const std::string email = login + "@example.test";
    ASSERT_EQ(repository.updateProfile(login, ProfileUpdate{.email = email}), UpdateProfileResult::kUpdated);

    const auto resolved = repository.resolveOtpIdentifier(email);

    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->login, login);
    ASSERT_TRUE(resolved->email.has_value());
    EXPECT_EQ(*resolved->email, email);
}

TEST(UserRepositoryTest, ResolveOtpIdentifierReturnsNulloptWhenNoEmailIsSet) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-otp-no-email");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);

    EXPECT_FALSE(repository.resolveOtpIdentifier(login).has_value());
}

TEST(UserRepositoryTest, ResolveOtpIdentifierReturnsNulloptForUnknownIdentifier) {
    UserRepository repository(connectionString());

    std::optional<OtpIdentity> resolved;
    try {
        resolved = repository.resolveOtpIdentifier("no-such-identifier-ever-created@example.test");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }

    EXPECT_FALSE(resolved.has_value());
}

TEST(UserRepositoryTest, ResolveOtpIdentifierFindsUserByTelegramChatId) {
    UserRepository repository(connectionString());
    const std::string login = uniqueLogin("user-repository-test-otp-telegram");

    bool created = false;
    try {
        created = repository.createUser(login, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(created);
    const std::string chatId = uniqueLogin("chat-id");
    ASSERT_EQ(repository.updateProfile(login, ProfileUpdate{.telegramChatId = chatId}),
              UpdateProfileResult::kUpdated);

    const auto resolvedByLogin = repository.resolveOtpIdentifier(login);
    ASSERT_TRUE(resolvedByLogin.has_value());
    ASSERT_TRUE(resolvedByLogin->telegramChatId.has_value());
    EXPECT_EQ(*resolvedByLogin->telegramChatId, chatId);

    const auto resolvedByChatId = repository.resolveOtpIdentifier(chatId);
    ASSERT_TRUE(resolvedByChatId.has_value());
    EXPECT_EQ(resolvedByChatId->login, login);
}

TEST(UserRepositoryTest, UpdateProfileReturnsTelegramChatIdTakenForDuplicateChatId) {
    UserRepository repository(connectionString());
    const std::string loginA = uniqueLogin("user-repository-test-telegram-taken-a");
    const std::string loginB = uniqueLogin("user-repository-test-telegram-taken-b");
    const std::string chatId = uniqueLogin("chat-id-taken");

    bool createdA = false;
    try {
        createdA = repository.createUser(loginA, "some-hash");
    } catch (const std::exception& error) {
        GTEST_SKIP() << "Postgres not reachable (" << error.what() << ") — run `docker compose up` to run this test.";
    }
    ASSERT_TRUE(createdA);
    ASSERT_TRUE(repository.createUser(loginB, "some-hash"));
    ASSERT_EQ(repository.updateProfile(loginA, ProfileUpdate{.telegramChatId = chatId}),
              UpdateProfileResult::kUpdated);

    EXPECT_EQ(repository.updateProfile(loginB, ProfileUpdate{.telegramChatId = chatId}),
              UpdateProfileResult::kTelegramChatIdTaken);
}

}  // namespace
}  // namespace user_service
