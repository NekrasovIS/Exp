#pragma once

#include <QString>
#include <QWidget>

#include <optional>

class QLabel;
class QResizeEvent;

namespace devicehub {

class ChatBubble;

/// A chat message as received from ChatClient::messageReceived() —
/// sentAt is the raw server timestamp string (Postgres-serialized,
/// e.g. "2026-08-05 09:14:23.123456"). id and editedAt (issue #107)
/// are needed to target/label a specific message for editing/deleting;
/// editedAt is unset for a message that's never been edited. attachmentId
/// is -1 and attachmentFilename empty when the message has no attachment
/// (issue #116).
struct ChatMessage {
    qint64 id = 0;
    QString author;
    QString body;
    QString sentAt;
    std::optional<QString> editedAt;
    qint64 attachmentId = -1;
    QString attachmentFilename;
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

    [[nodiscard]] qint64 messageId() const { return messageId_; }

    /// Updates the displayed body text and appends an "(edited)" marker
    /// next to the timestamp, if this row has one shown (showHeader) —
    /// issue #107, called when ChatClient::messageEdited() fires for
    /// this row's message.
    void updateBody(const QString& newBody);

signals:
    /// "Edit" clicked (own messages only, issue #107) — @p currentBody
    /// lets the caller pre-fill an edit box without looking the message
    /// back up by id.
    void editRequested(qint64 id, const QString& currentBody);

    /// "Delete" clicked (own messages only).
    void deleteRequested(qint64 id);

    /// "Download" clicked on a message with an attachment (issue #116,
    /// any message, not just own).
    void downloadRequested(qint64 attachmentId, const QString& filename);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    ChatBubble* bubble_ = nullptr;
    QLabel* bodyLabel_ = nullptr;
    /// Null when constructed with showHeader false — grouped rows
    /// don't show a timestamp at all, see class doc comment.
    QLabel* timeLabel_ = nullptr;
    QString formattedSentAt_;
    qint64 messageId_ = 0;
};

}  // namespace devicehub
