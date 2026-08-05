#pragma once

#include <QDateTime>

#include "ui/ChatMessageRow.h"

namespace devicehub::chat_message_grouping {

/// Parses a chat-service timestamp string (Postgres-serialized, space-
/// separated, e.g. "2026-08-05 09:14:23.123456") into a QDateTime;
/// returns an invalid QDateTime if @p raw doesn't parse.
QDateTime parseSentAt(const QString& raw);

/// True if @p current should be grouped under @p previous — same
/// author and within a short time window — so ChatView can skip
/// repeating the avatar/name/time header for it. False whenever either
/// timestamp fails to parse, so grouping only ever happens when it can
/// be verified.
bool shouldGroupWithPrevious(const ChatMessage& previous, const ChatMessage& current);

}  // namespace devicehub::chat_message_grouping
