#include "JsonGuard.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

/// libFuzzer harness for chat_service::json_guard::exceedsMaxNestingDepth()
/// — issue #133. The guard itself is small and non-recursive by
/// construction (see JsonGuard.cpp), so this is mainly a regression
/// harness confirming it never crashes/UB's on arbitrary bytes (embedded
/// NUL, unpaired surrogates, unterminated string literals) rather than an
/// expectation of finding something new the way the WebSocket-frame fuzz
/// target (issue #129's discovery) would.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    static_cast<void>(chat_service::json_guard::exceedsMaxNestingDepth(input, chat_service::json_guard::kMaxNestingDepth));
    return 0;
}
