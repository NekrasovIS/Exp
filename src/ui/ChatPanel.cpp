#include "ui/ChatPanel.h"

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace devicehub {

ChatPanel::ChatPanel(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto* title = new QLabel(tr("Chat"), this);
    title->setProperty("sectionTitle", true);

    channelNameEdit_ = new QLineEdit(this);
    channelNameEdit_->setObjectName(QStringLiteral("channelNameEdit"));
    channelNameEdit_->setPlaceholderText(tr("New channel name"));

    createChannelButton_ = new QPushButton(tr("Create channel in selected community"), this);
    createChannelButton_->setObjectName(QStringLiteral("createChannelButton"));
    createChannelButton_->setProperty("accent", true);

    channelCombo_ = new QComboBox(this);
    channelCombo_->setObjectName(QStringLiteral("channelCombo"));

    refreshChannelsButton_ = new QPushButton(tr("Refresh channels"), this);
    refreshChannelsButton_->setObjectName(QStringLiteral("refreshChannelsButton"));

    connectButton_ = new QPushButton(tr("Connect to selected channel"), this);
    connectButton_->setObjectName(QStringLiteral("connectToChannelButton"));
    connectButton_->setProperty("accent", true);

    chatLog_ = new QPlainTextEdit(this);
    chatLog_->setObjectName(QStringLiteral("chatLog"));
    chatLog_->setReadOnly(true);

    messageEdit_ = new QLineEdit(this);
    messageEdit_->setObjectName(QStringLiteral("chatMessageEdit"));
    messageEdit_->setPlaceholderText(tr("Message"));

    sendButton_ = new QPushButton(tr("Send"), this);
    sendButton_->setObjectName(QStringLiteral("sendChatMessageButton"));
    sendButton_->setProperty("accent", true);

    layout->addWidget(title);
    layout->addWidget(channelNameEdit_);
    layout->addWidget(createChannelButton_);
    layout->addWidget(channelCombo_);
    layout->addWidget(refreshChannelsButton_);
    layout->addWidget(connectButton_);
    layout->addWidget(chatLog_, /*stretch=*/1);
    layout->addWidget(messageEdit_);
    layout->addWidget(sendButton_);
}

}  // namespace devicehub
