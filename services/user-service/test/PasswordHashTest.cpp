#include "password_hash.h"

#include <gtest/gtest.h>

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

}  // namespace
