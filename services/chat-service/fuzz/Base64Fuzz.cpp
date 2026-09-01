#include "base64.h"

#include <cstddef>
#include <cstdint>
#include <string>

/// libFuzzer harness for chat_service::base64::decode() — issue #133,
/// replaces/extends the interim GTest random-corpus stand-in
/// (Base64Test.DoesNotCrashOnRandomByteStrings) with real coverage-guided
/// fuzzing once a Clang toolchain is available (issue #132). decode()
/// takes attacker-controlled data_base64 straight from
/// POST /channels/{id}/attachments (issue #116).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);
    static_cast<void>(chat_service::base64::decode(input));
    return 0;
}
