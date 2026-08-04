#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

namespace devicehub {

/**
 * @brief Bottom-left sidebar section: channel management plus the live
 *        message log and send box.
 *
 * Pure presentation — exposes its controls so MainWindow can wire them to
 * ChatClient/ChatRestClient the same way it always has; this class only
 * owns layout.
 */
class ChatPanel : public QWidget {
    Q_OBJECT

public:
    explicit ChatPanel(QWidget* parent = nullptr);

    [[nodiscard]] QLineEdit* channelNameEdit() const { return channelNameEdit_; }
    [[nodiscard]] QPushButton* createChannelButton() const { return createChannelButton_; }
    [[nodiscard]] QComboBox* channelCombo() const { return channelCombo_; }
    [[nodiscard]] QPushButton* refreshChannelsButton() const { return refreshChannelsButton_; }
    [[nodiscard]] QPushButton* connectButton() const { return connectButton_; }
    [[nodiscard]] QPlainTextEdit* chatLog() const { return chatLog_; }
    [[nodiscard]] QLineEdit* messageEdit() const { return messageEdit_; }
    [[nodiscard]] QPushButton* sendButton() const { return sendButton_; }

private:
    QLineEdit* channelNameEdit_ = nullptr;
    QPushButton* createChannelButton_ = nullptr;
    QComboBox* channelCombo_ = nullptr;
    QPushButton* refreshChannelsButton_ = nullptr;
    QPushButton* connectButton_ = nullptr;
    QPlainTextEdit* chatLog_ = nullptr;
    QLineEdit* messageEdit_ = nullptr;
    QPushButton* sendButton_ = nullptr;
};

}  // namespace devicehub
