#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;

namespace devicehub {

/**
 * @brief Main content area: shows a placeholder until a channel is
 *        selected in ChannelsPanel, then the message log and send box
 *        for that channel.
 *
 * Pure presentation — MainWindow owns ChatClient and feeds messages in
 * via appendLine()/clearLog(); this class only owns layout and the
 * placeholder/channel toggle.
 */
class ChatView : public QWidget {
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);

    /// Switches back to the "no channel selected" placeholder.
    void showPlaceholder();

    /// Switches to the chat page and sets its header to @p channelName.
    void showChannel(const QString& channelName);

    void appendLine(const QString& text);
    void clearLog();

    [[nodiscard]] QPlainTextEdit* chatLog() const { return chatLog_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }

private:
    QStackedWidget* stack_ = nullptr;
    QLabel* channelTitleLabel_ = nullptr;
    QPlainTextEdit* chatLog_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
};

}  // namespace devicehub
