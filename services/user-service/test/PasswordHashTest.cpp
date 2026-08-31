#include "password_hash.h"

#include <gtest/gtest.h>
#include <sodium.h>

#include <string>

namespace {

TEST(PasswordHashTest, CorrectPasswordVerifies) {
    const std::string hashed = password_hash::hash("correct horse battery staple");
    EXPECT_TRUE(password_hash::verify(hashed, "correct horse battery staple"));
}

TEST(PasswordHashTest, WrongPasswordIsRejected) {
    const std::string hashed = password_hash::hash("correct horse battery staple");
    EXPECT_FALSE(password_hash::verify(hashed, "wrong password"));
}

TEST(PasswordHashTest, SamePasswordHashesDifferentlyEachTime) {
    const std::string first = password_hash::hash("same-password");
    const std::string second = password_hash::hash("same-password");
    EXPECT_NE(first, second);
}

TEST(PasswordHashTest, MalformedHashIsRejectedNotCrashed) {
    EXPECT_FALSE(password_hash::verify("not-a-real-hash", "anything"));
}

TEST(PasswordHashTest, EmptyPasswordHashesAndVerifies) {
    const std::string hashed = password_hash::hash("");
    EXPECT_TRUE(password_hash::verify(hashed, ""));
    EXPECT_FALSE(password_hash::verify(hashed, "not-empty"));
}

TEST(PasswordHashTest, HashAtOrOverStrbytesBoundaryIsRejectedNotCrashed) {
    // verify() explicitly guards against hash.size() >= crypto_pwhash_STRBYTES
    // before ever calling into libsodium — this exercises that boundary
    // directly rather than relying on a real hash happening to be shorter.
    const std::string tooLong(static_cast<size_t>(crypto_pwhash_STRBYTES), 'x');
    EXPECT_FALSE(password_hash::verify(tooLong, "anything"));
}

}  // namespace
