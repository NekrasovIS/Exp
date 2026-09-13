#include "PrivateNetworkCheck.h"

#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#endif

namespace chat_service::private_network_check {

namespace {

bool isPrivateOrReservedIPv4(uint32_t hostOrderAddress) {
    return (hostOrderAddress & 0xFF000000U) == 0x7F000000U ||  // 127.0.0.0/8 (loopback)
           (hostOrderAddress & 0xFF000000U) == 0x0AU << 24 ||  // 10.0.0.0/8
           (hostOrderAddress & 0xFFF00000U) == 0xAC100000U ||  // 172.16.0.0/12
           (hostOrderAddress & 0xFFFF0000U) == 0xC0A80000U ||  // 192.168.0.0/16
           (hostOrderAddress & 0xFFFF0000U) == 0xA9FE0000U ||  // 169.254.0.0/16 (link-local, incl. cloud metadata)
           (hostOrderAddress & 0xFF000000U) == 0x00000000U;    // 0.0.0.0/8
}

bool isPrivateOrReservedIPv6(const unsigned char* bytes) {
    // IPv4-mapped (::ffff:a.b.c.d) and the deprecated IPv4-compatible
    // (::a.b.c.d) forms both embed a real IPv4 address in the last 4
    // bytes — checked as IPv4, not skipped, since either form lets an
    // attacker spell a private IPv4 target as an IPv6 literal.
    const bool first10Zero = std::memcmp(bytes, "\0\0\0\0\0\0\0\0\0\0", 10) == 0;
    if (first10Zero && (bytes[10] == 0 || (bytes[10] == 0xff && bytes[11] == 0xff))) {
        const uint32_t embeddedIPv4 = (static_cast<uint32_t>(bytes[12]) << 24) |
                                       (static_cast<uint32_t>(bytes[13]) << 16) |
                                       (static_cast<uint32_t>(bytes[14]) << 8) | static_cast<uint32_t>(bytes[15]);
        if (isPrivateOrReservedIPv4(embeddedIPv4)) {
            return true;
        }
    }

    // ::1 — loopback.
    if (std::memcmp(bytes, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\x01", 16) == 0) {
        return true;
    }
    // fc00::/7 — unique local.
    if ((bytes[0] & 0xfe) == 0xfc) {
        return true;
    }
    // fe80::/10 — link-local.
    if (bytes[0] == 0xfe && (bytes[1] & 0xc0) == 0x80) {
        return true;
    }
    // :: — unspecified.
    if (std::memcmp(bytes, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0) {
        return true;
    }
    return false;
}

}  // namespace

bool isPrivateOrReserved(const std::string& ipAddress) {
    in_addr ipv4Address{};
    if (inet_pton(AF_INET, ipAddress.c_str(), &ipv4Address) == 1) {
        return isPrivateOrReservedIPv4(ntohl(ipv4Address.s_addr));
    }

    in6_addr ipv6Address{};
    if (inet_pton(AF_INET6, ipAddress.c_str(), &ipv6Address) == 1) {
#ifdef _WIN32
        return isPrivateOrReservedIPv6(ipv6Address.u.Byte);
#else
        return isPrivateOrReservedIPv6(ipv6Address.s6_addr);
#endif
    }

    // Ни один из форматов не распознан — не может быть безопасным
    // адресом для подключения, так что fail closed (считаем приватным/
    // недопустимым), а не наоборот.
    return true;
}

std::optional<std::string> resolveFirstAddress(const std::string& hostname) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname.c_str(), nullptr, &hints, &result) != 0 || result == nullptr) {
        return std::nullopt;
    }

    char addressBuffer[INET6_ADDRSTRLEN] = {};
    std::optional<std::string> resolved;
    if (result->ai_family == AF_INET) {
        auto* ipv4 = reinterpret_cast<sockaddr_in*>(result->ai_addr);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, addressBuffer, sizeof(addressBuffer)) != nullptr) {
            resolved = std::string(addressBuffer);
        }
    } else if (result->ai_family == AF_INET6) {
        auto* ipv6 = reinterpret_cast<sockaddr_in6*>(result->ai_addr);
        if (inet_ntop(AF_INET6, &ipv6->sin6_addr, addressBuffer, sizeof(addressBuffer)) != nullptr) {
            resolved = std::string(addressBuffer);
        }
    }

    freeaddrinfo(result);
    return resolved;
}

}  // namespace chat_service::private_network_check
