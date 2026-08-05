#pragma once

#include <QString>
#include <QWidget>

namespace devicehub {

/// A chat message as received from ChatClient::messageReceived() —
/// sentAt is the raw server timestamp string (Postgres-serialized,
/// e.g. "2026-08-05 09:14:23.123456").
struct ChatMessage {
    QString author;
    QString body;
    QString sentAt;
};

/**
 * @brief One row in ChatView's message list.
 *
 * Full form (@p showHeader true) shows an avatar circle plus author
 * and time above the body text. Grouped form (@p showHeader false —
 * for a consecutive message from the same author, see
 * chat_message_grouping::shouldGroupWithPrevious()) shows only the
 * body, indented to align under where the avatar would be, so
 * consecutive messages from one author don't repeat their header.
 */
class ChatMessageRow : public QWidget {
    Q_OBJECT

public:
    ChatMessageRow(const ChatMessage& message, bool showHeader, QWidget* parent = nullptr);
};

}  // namespace devicehub
