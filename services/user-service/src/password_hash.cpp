#include "password_hash.h"

#include <sodium.h>

#include <array>
#include <stdexcept>

namespace password_hash {

namespace {

void ensureSodiumInitialized() {
    // sodium_init() безопасно вызывать несколько раз; используем статическую
    // локальную переменную, чтобы конкурентные вызовы оплачивали инициализацию
    // только один раз (C++11 гарантирует потокобезопасную инициализацию
    // локальных статических переменных функции).
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
        // Отказывает только при исчерпании ресурсов (например, не удаётся
        // выделить memlimit); вызывающий код не может осмысленно это исправить.
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
