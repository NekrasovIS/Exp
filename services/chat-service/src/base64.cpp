#include "base64.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace chat_service::base64 {

namespace {
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::array<int8_t, 256> buildDecodeTable() {
    std::array<int8_t, 256> table{};
    table.fill(-1);
    for (std::size_t i = 0; i < kAlphabet.size(); ++i) {
        table[static_cast<uint8_t>(kAlphabet[i])] = static_cast<int8_t>(i);
    }
    return table;
}
}  // namespace

std::optional<std::string> decode(const std::string& text) {
    static const std::array<int8_t, 256> kDecodeTable = buildDecodeTable();

    std::string result;
    result.reserve(text.size() / 4 * 3);

    uint32_t buffer = 0;
    int bitsCollected = 0;
    for (const char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
            continue;
        }
        const int8_t value = kDecodeTable[static_cast<uint8_t>(c)];
        if (value < 0) {
            return std::nullopt;
        }
        buffer = (buffer << 6) | static_cast<uint32_t>(value);
        bitsCollected += 6;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            result.push_back(static_cast<char>((buffer >> bitsCollected) & 0xFF));
        }
    }

    return result;
}

}  // namespace chat_service::base64
