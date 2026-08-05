#pragma once

#include <QWidget>

#include "ui/ChatMessageRow.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace devicehub {

/**
 * @brief Main content area: shows a placeholder until a channel is
 *        selected in ChannelsPanel, then a grouped, avatar-annotated
 *        message list and send box for that channel.
 *
 * Pure presentation — MainWindow owns ChatClient and feeds messages in
 * via appendMessage()/appendSystemLine()/clearLog(); this class only
 * owns layout, the placeholder/channel toggle, and deciding (via
 * chat_message_grouping) whether a message needs its own header.
 */
class ChatView : public QWidget {
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);

    /// Switches back to the "no channel selected" placeholder.
    void showPlaceholder();

    /// Switches to the chat page and sets its header to @p channelName.
    void showChannel(const QString& channelName);

    /// Needed to decide whether an appended message is "own" (bubble
    /// right-aligned, accent-colored, no avatar) or someone else's.
    void setCurrentUserLogin(const QString& login);

    /// Appends a real chat message — grouped under the previous one
    /// (no repeated avatar/name/time) if they're from the same author
    /// within a few minutes of each other.
    void appendMessage(const ChatMessage& message);

    /// Appends a muted, centered system/status line (subscribed,
    /// errors, ...) — always breaks any pending message grouping, so
    /// the next real message gets its own header regardless of author.
    void appendSystemLine(const QString& text);

    /// Clears the message list and resets grouping state.
    void clearLog();

    [[nodiscard]] QWidget* messagesContainer() const { return messagesContainer_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }

signals:
    /// Emitted when the placeholder's "Create channel" button is
    /// clicked — MainWindow wires this to the same handling as
    /// ChannelsPanel's own "+" button.
    void createChannelRequested();

private:
    QStackedWidget* stack_ = nullptr;
    QLabel* channelTitleLabel_ = nullptr;
    QScrollArea* scrollArea_ = nullptr;
    QWidget* messagesContainer_ = nullptr;
    QVBoxLayout* messagesLayout_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
    bool hasLastMessage_ = false;
    ChatMessage lastMessage_;
    QString currentUserLogin_;
};

}  // namespace devicehub
