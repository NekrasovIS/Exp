#include "password_hash.h"

#include <sodium.h>

#include <array>
#include <stdexcept>

namespace password_hash {

namespace {

void ensureSodiumInitialized() {
    // sodium_init() is safe to call multiple times; guard with a static
    // local so concurrent callers only pay for it once (C++11 guarantees
    // thread-safe initialization of function-local statics).
    static const bool initialized = [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("libsodium failed to initialize");
        }
        return true;
    }();
    static_cast<void>(initialized);
}

}  // namespace

std::string hash(const std::string& password) {
    ensureSodiumInitialized();

    std::array<char, crypto_pwhash_STRBYTES> out{};
    if (crypto_pwhash_str(out.data(), password.c_str(), password.size(), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                           crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        // Only fails on resource exhaustion (e.g., can't allocate the
        // memlimit); not something callers can meaningfully recover from.
        throw std::runtime_error("password hashing failed (out of memory)");
    }

    return std::string(out.data());
}

bool verify(const std::string& hash, const std::string& password) {
    ensureSodiumInitialized();

    if (hash.size() >= crypto_pwhash_STRBYTES) {
        return false;
    }

    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace password_hash
