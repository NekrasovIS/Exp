#include "base64.h"

#include <cstddef>
#include <cstdint>
#include <string>

/// libFuzzer-обвязка для chat_service::base64::decode() — issue #133,
/// заменяет/расширяет временную заглушку на GTest со случайным корпусом
/// (Base64Test.DoesNotCrashOnRandomByteStrings) настоящим fuzzing'ом,
/// управляемым покрытием, как только станет доступен тулчейн Clang
/// (issue #132). decode() получает контролируемые атакующим
/// data_base64 напрямую из POST /channels/{id}/attachments (issue #116).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string input(reinterpret_cast<const char*>(data), size);
    static_cast<void>(chat_service::base64::decode(input));
    return 0;
}
