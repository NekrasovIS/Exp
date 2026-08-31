#pragma once

#include <string_view>

namespace user_service::json_guard {

/// Nesting depth generous enough for every message this protocol actually
/// sends (a handful of levels deep at most) while staying far short of
/// what it takes to stack-overflow nlohmann::json's recursive-descent
/// parser.
inline constexpr int kMaxNestingDepth = 32;

/// Returns true if @p payload's bracket/brace nesting (outside string
/// literals) exceeds @p maxDepth. Meant to be checked before handing
/// attacker-controlled bytes to nlohmann::json::parse(): its recursive-
/// descent parser stack-overflows the whole process on deeply nested
/// input (e.g. `[[[[...]]]]`) well before any size limit would catch it,
/// since a few hundred bytes are enough to reach thousands of levels.
[[nodiscard]] bool exceedsMaxNestingDepth(std::string_view payload, int maxDepth);

}  // namespace user_service::json_guard
