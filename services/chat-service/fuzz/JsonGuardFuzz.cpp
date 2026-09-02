#include "JsonGuard.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

/// libFuzzer-обвязка для chat_service::json_guard::exceedsMaxNestingDepth()
/// — issue #133. Сама проверка небольшая и нерекурсивная по построению
/// (см. JsonGuard.cpp), поэтому это в основном регрессионная обвязка,
/// подтверждающая, что она никогда не падает и не приводит к UB на
/// произвольных байтах (встроенный NUL, непарные суррогаты,
/// незавершённые строковые литералы), а не ожидание найти что-то новое,
/// как это сделала цель fuzzing'а WebSocket-кадров (находка issue #129).
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view input(reinterpret_cast<const char*>(data), size);
    static_cast<void>(chat_service::json_guard::exceedsMaxNestingDepth(input, chat_service::json_guard::kMaxNestingDepth));
    return 0;
}
