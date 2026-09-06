#include "OtpStore.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <array>
#include <iomanip>
#include <sstream>

namespace auth_service {

namespace {
constexpr int kCodeDigits = 6;
constexpr unsigned int kCodeModulus = 1000000;
}  // namespace

OtpStore::OtpStore(std::chrono::seconds ttl, int maxAttempts) : ttl_(ttl), maxAttempts_(maxAttempts) {}

std::string OtpStore::generateNumericCode() {
    unsigned int value = 0;
    // RAND_bytes — криптографически стойкий генератор, не rand()/
    // std::rand(): предсказуемый код одноразового входа сводит на нет
    // весь смысл этой схемы.
    RAND_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(value));
    value %= kCodeModulus;
    std::ostringstream out;
    out << std::setfill('0') << std::setw(kCodeDigits) << value;
    return out.str();
}

std::string OtpStore::hashCode(const std::string& code) {
    // EVP_Digest, не устаревшая SHA256() — тот же паттерн, что уже
    // применяется для HMAC в TokenService.cpp.
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digestLen = 0;
    EVP_Digest(code.data(), code.size(), digest.data(), &digestLen, EVP_sha256(), nullptr);

    std::ostringstream out;
    for (unsigned int i = 0; i < digestLen; ++i) {
        out << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(digest[i]);
    }
    return out.str();
}

std::string OtpStore::issue(const std::string& login) {
    const std::string code = generateNumericCode();

    const std::lock_guard<std::mutex> lock(mutex_);
    entries_[login] = Entry{.codeHash = hashCode(code),
                             .expiresAt = std::chrono::steady_clock::now() + ttl_,
                             .attemptsRemaining = maxAttempts_};
    return code;
}

bool OtpStore::verify(const std::string& login, const std::string& code) {
    const std::lock_guard<std::mutex> lock(mutex_);

    const auto it = entries_.find(login);
    if (it == entries_.end()) {
        return false;
    }
    if (std::chrono::steady_clock::now() >= it->second.expiresAt) {
        entries_.erase(it);
        return false;
    }
    if (hashCode(code) != it->second.codeHash) {
        if (--it->second.attemptsRemaining <= 0) {
            entries_.erase(it);
        }
        return false;
    }

    entries_.erase(it);
    return true;
}

}  // namespace auth_service
