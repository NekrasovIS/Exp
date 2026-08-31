#include "JsonGuard.h"

namespace chat_service::json_guard {

bool exceedsMaxNestingDepth(std::string_view payload, int maxDepth) {
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (const char c : payload) {
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        switch (c) {
            case '"':
                inString = true;
                break;
            case '[':
            case '{':
                ++depth;
                if (depth > maxDepth) {
                    return true;
                }
                break;
            case ']':
            case '}':
                --depth;
                break;
            default:
                break;
        }
    }
    return false;
}

}  // namespace chat_service::json_guard
