#pragma once

#include <QString>
#include <QWidget>

class QResizeEvent;

namespace devicehub {

class ChatBubble;

/// A chat message as received from ChatClient::messageReceived() —
/// sentAt is the raw server timestamp string (Postgres-serialized,
/// e.g. "2026-08-05 09:14:23.123456").
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
};

/**
 * @brief One row in ChatView's message list, bubble-styled (iMessage/
 *        Slack-like).
 *
 * Own messages (@p isOwnMessage true) are right-aligned in a green-
 * gradient ChatBubble with no avatar. Others' messages are left-
 * aligned in a neutral ChatBubble; full form (@p showHeader true) adds
 * an avatar circle plus author and time above the body, grouped form
 * (for a consecutive message from the same author, see
 * chat_message_grouping::shouldGroupWithPrevious()) omits them so
 * consecutive messages don't repeat their header.
 *
 * Every size here (avatar diameter, spacing, bubble padding/radius) is
 * derived from the current font's metrics rather than a fixed pixel
 * constant, and the bubble's maximum width is recomputed as a
 * percentage of the row's own width in resizeEvent() rather than
 * hardcoded — so the whole row scales with font size and available
 * space instead of being pinned to specific pixel numbers.
 */
class ChatMessageRow : public QWidget {
    Q_OBJECT

public:
    ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage, QWidget* parent = nullptr);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ChatBubble* bubble_ = nullptr;
};

}  // namespace devicehub
