#include "TokenService.h"

#include <cstddef>
#include <cstdint>
#include <string>

/// libFuzzer harness for TokenService::verifyToken() — issue #133.
/// @p token comes straight from the client-controlled Authorization:
/// Bearer header on every authenticated request across all three
/// services (via AuthServiceClient -> POST /auth/verify), so it's
/// exactly the kind of untrusted input CLAUDE.md's fuzz-testing section
/// calls out. A fixed secret is enough here — the fuzzer's job is
/// finding inputs that crash/UB the base64url-decode + HMAC-compare +
/// JSON-parse pipeline verifyToken() runs over arbitrary bytes, not
/// finding the secret itself.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static const auth_service::TokenService service("fuzz-harness-only-secret");
    const std::string token(reinterpret_cast<const char*>(data), size);
    static_cast<void>(service.verifyToken(token));
    return 0;
}
