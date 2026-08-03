#include "base64_utils.h"

#include <array>
#include <string_view>

namespace base64_utils {

namespace {
constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::array<int8_t, 256> buildDecodeTable() {
    std::array<int8_t, 256> table{};
    table.fill(-1);
    for (size_t i = 0; i < kAlphabet.size(); ++i) {
        table[static_cast<uint8_t>(kAlphabet[i])] = static_cast<int8_t>(i);
    }
    return table;
}
}  // namespace

std::string encodeUrl(const std::vector<uint8_t>& data) {
    std::string result;
    result.reserve((data.size() + 2) / 3 * 4);

    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8) |
                                static_cast<uint32_t>(data[i + 2]);
        result += kAlphabet[(chunk >> 18) & 0x3F];
        result += kAlphabet[(chunk >> 12) & 0x3F];
        result += kAlphabet[(chunk >> 6) & 0x3F];
        result += kAlphabet[chunk & 0x3F];
    }

    const size_t remaining = data.size() - i;
    if (remaining == 1) {
        const uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        result += kAlphabet[(chunk >> 18) & 0x3F];
        result += kAlphabet[(chunk >> 12) & 0x3F];
    } else if (remaining == 2) {
        const uint32_t chunk = (static_cast<uint32_t>(data[i]) << 16) | (static_cast<uint32_t>(data[i + 1]) << 8);
        result += kAlphabet[(chunk >> 18) & 0x3F];
        result += kAlphabet[(chunk >> 12) & 0x3F];
        result += kAlphabet[(chunk >> 6) & 0x3F];
    }

    return result;
}

std::vector<uint8_t> decodeUrl(const std::string& text) {
    static const std::array<int8_t, 256> kDecodeTable = buildDecodeTable();

    std::vector<uint8_t> result;
    result.reserve(text.size() / 4 * 3);

    uint32_t buffer = 0;
    int bitsCollected = 0;
    for (const char c : text) {
        const int8_t value = kDecodeTable[static_cast<uint8_t>(c)];
        if (value < 0) {
            return {};
        }
        buffer = (buffer << 6) | static_cast<uint32_t>(value);
        bitsCollected += 6;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            result.push_back(static_cast<uint8_t>((buffer >> bitsCollected) & 0xFF));
        }
    }

    return result;
}

}  // namespace base64_utils
