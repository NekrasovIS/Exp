#include "OtpStore.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace auth_service {
namespace {

TEST(OtpStoreTest, IssuedCodeIsSixDigits) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);

    const std::string code = store.issue("alice");

    EXPECT_EQ(code.size(), 6);
    for (char digit : code) {
        EXPECT_TRUE(digit >= '0' && digit <= '9');
    }
}

TEST(OtpStoreTest, VerifyReturnsTrueForTheCorrectCode) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);
    const std::string code = store.issue("alice");

    EXPECT_TRUE(store.verify("alice", code));
}

TEST(OtpStoreTest, VerifyReturnsFalseForAnUnknownLogin) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);

    EXPECT_FALSE(store.verify("no-such-login", "123456"));
}

TEST(OtpStoreTest, VerifyReturnsFalseForTheWrongCode) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);
    store.issue("alice");

    EXPECT_FALSE(store.verify("alice", "000000"));
}

TEST(OtpStoreTest, CodeIsSingleUse) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);
    const std::string code = store.issue("alice");
    ASSERT_TRUE(store.verify("alice", code));

    // Тот же код повторно — запись уже удалена успешной проверкой.
    EXPECT_FALSE(store.verify("alice", code));
}

TEST(OtpStoreTest, IssuingAgainReplacesThePreviousCode) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);
    const std::string firstCode = store.issue("alice");
    const std::string secondCode = store.issue("alice");

    EXPECT_FALSE(store.verify("alice", firstCode));
    EXPECT_TRUE(store.verify("alice", secondCode));
}

TEST(OtpStoreTest, CodeExpiresAfterTtl) {
    OtpStore store(std::chrono::seconds{0}, /*maxAttempts=*/5);
    const std::string code = store.issue("alice");
    // TTL=0 — код уже просрочен к моменту первой проверки; небольшая
    // пауза не даёт тесту зависеть от того, успеет ли steady_clock
    // тикнуть между issue() и verify() при TTL ровно в 0.
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

    EXPECT_FALSE(store.verify("alice", code));
}

TEST(OtpStoreTest, CodeIsInvalidatedAfterMaxAttempts) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/3);
    const std::string code = store.issue("alice");

    EXPECT_FALSE(store.verify("alice", "wrong-1"));
    EXPECT_FALSE(store.verify("alice", "wrong-2"));
    EXPECT_FALSE(store.verify("alice", "wrong-3"));
    // Три неверные попытки исчерпали лимит — даже правильный код
    // больше не проходит, запись уже удалена.
    EXPECT_FALSE(store.verify("alice", code));
}

TEST(OtpStoreTest, DifferentLoginsHaveIndependentCodes) {
    OtpStore store(std::chrono::seconds{60}, /*maxAttempts=*/5);
    const std::string aliceCode = store.issue("alice");
    const std::string bobCode = store.issue("bob");

    EXPECT_FALSE(store.verify("alice", bobCode));
    EXPECT_TRUE(store.verify("bob", bobCode));
    EXPECT_TRUE(store.verify("alice", aliceCode));
}

}  // namespace
}  // namespace auth_service
