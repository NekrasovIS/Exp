#include "base32.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace user_service::base32 {

namespace {
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

std::array<int8_t, 256> buildDecodeTable() {
    std::array<int8_t, 256> table{};
    table.fill(-1);
    for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
        table[static_cast<uint8_t>(kAlphabet[i])] = static_cast<int8_t>(i);
    }
    return table;
}
}  // namespace

std::string encode(const std::string& bytes) {
    std::string result;
    result.reserve((bytes.size() + 4) / 5 * 8);

    uint64_t buffer = 0;
    int bitsCollected = 0;
    for (const char c : bytes) {
        buffer = (buffer << 8) | static_cast<uint8_t>(c);
        bitsCollected += 8;
        while (bitsCollected >= 5) {
            bitsCollected -= 5;
            result.push_back(kAlphabet[(buffer >> bitsCollected) & 0x1f]);
        }
    }
    if (bitsCollected > 0) {
        result.push_back(kAlphabet[(buffer << (5 - bitsCollected)) & 0x1f]);
    }
    // Pad to a multiple of 8 characters (RFC 4648 §6) — the padding
    // length is fully determined by the input length modulo 5, but
    // rounding up to the next multiple of 8 is simpler than a lookup
    // table and produces the identical result.
    while (result.size() % 8 != 0) {
        result.push_back('=');
    }
    return result;
}

std::optional<std::string> decode(const std::string& text) {
    static const std::array<int8_t, 256> kDecodeTable = buildDecodeTable();

    std::string result;
    result.reserve(text.size() * 5 / 8);

    uint64_t buffer = 0;
    int bitsCollected = 0;
    for (const char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
            continue;
        }
        const int8_t value = kDecodeTable[static_cast<uint8_t>(c)];
        if (value < 0) {
            return std::nullopt;
        }
        buffer = (buffer << 5) | static_cast<uint64_t>(value);
        bitsCollected += 5;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            result.push_back(static_cast<char>((buffer >> bitsCollected) & 0xff));
        }
    }

    return result;
}

}  // namespace user_service::base32
