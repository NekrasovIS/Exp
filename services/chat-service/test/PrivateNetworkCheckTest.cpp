#include "PrivateNetworkCheck.h"

#include <gtest/gtest.h>

namespace chat_service {
namespace {

// Issue #396 (защита от SSRF) — security-тесты: каждый диапазон, в
// который превью ссылок не должно уметь ходить, независимо от того, что
// написал недоверенный пользователь в тексте сообщения.

TEST(PrivateNetworkCheckTest, LoopbackIPv4IsPrivate) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("127.0.0.1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("127.255.255.255"));
}

TEST(PrivateNetworkCheckTest, Rfc1918RangesArePrivate) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("10.0.0.1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("10.255.255.255"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("172.16.0.1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("172.31.255.255"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("192.168.0.1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("192.168.255.255"));
}

TEST(PrivateNetworkCheckTest, LinkLocalIncludingCloudMetadataEndpointIsPrivate) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("169.254.0.1"));
    // AWS/GCP/Azure instance metadata endpoint — the canonical SSRF target.
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("169.254.169.254"));
}

TEST(PrivateNetworkCheckTest, UnspecifiedAndZeroNetworkAreReserved) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("0.0.0.0"));
}

TEST(PrivateNetworkCheckTest, PublicIPv4AddressesAreNotPrivate) {
    EXPECT_FALSE(private_network_check::isPrivateOrReserved("8.8.8.8"));
    EXPECT_FALSE(private_network_check::isPrivateOrReserved("1.1.1.1"));
    // 172.32.0.0 is just outside the 172.16.0.0/12 private block.
    EXPECT_FALSE(private_network_check::isPrivateOrReserved("172.32.0.1"));
}

TEST(PrivateNetworkCheckTest, IPv6LoopbackAndUniqueLocalAndLinkLocalArePrivate) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("::1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("fc00::1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("fd12:3456:789a::1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("fe80::1"));
}

TEST(PrivateNetworkCheckTest, Ipv4MappedIPv6AddressesAreCheckedAsTheirEmbeddedIPv4) {
    // The classic bypass attempt: spell a private IPv4 target as an
    // IPv6 literal to slip past a naive string-based IPv4-only check.
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("::ffff:127.0.0.1"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("::ffff:169.254.169.254"));
    EXPECT_FALSE(private_network_check::isPrivateOrReserved("::ffff:8.8.8.8"));
}

TEST(PrivateNetworkCheckTest, PublicIPv6AddressIsNotPrivate) {
    // Google Public DNS's IPv6 address.
    EXPECT_FALSE(private_network_check::isPrivateOrReserved("2001:4860:4860::8888"));
}

TEST(PrivateNetworkCheckTest, MalformedInputFailsClosedAsPrivate) {
    EXPECT_TRUE(private_network_check::isPrivateOrReserved("not-an-ip-address"));
    EXPECT_TRUE(private_network_check::isPrivateOrReserved(""));
}

TEST(PrivateNetworkCheckTest, ResolveFirstAddressReturnsNulloptForANonexistentHostname) {
    const auto resolved =
        private_network_check::resolveFirstAddress("this-hostname-should-never-resolve.invalid");
    EXPECT_FALSE(resolved.has_value());
}

TEST(PrivateNetworkCheckTest, ResolveFirstAddressReturnsTheLiteralItselfForAnIpAddress) {
    const auto resolved = private_network_check::resolveFirstAddress("127.0.0.1");
    ASSERT_TRUE(resolved.has_value());
    EXPECT_TRUE(private_network_check::isPrivateOrReserved(*resolved));
}

}  // namespace
}  // namespace chat_service
