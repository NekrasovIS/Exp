#include "ui/ChatMessageRow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include "ui/ChatMessageGrouping.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {

constexpr int kAvatarSize = 32;

QString formatTime(const QString& rawSentAt) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(rawSentAt);
    return parsed.isValid() ? parsed.toString(QStringLiteral("HH:mm")) : rawSentAt;
}

}  // namespace

ChatMessageRow::ChatMessageRow(const ChatMessage& message, bool showHeader, QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(ui_theme::kSpacingSm);

    auto* bodyLabel = new QLabel(message.body, this);
    bodyLabel->setWordWrap(true);

    if (showHeader) {
        auto* avatarLabel = new QLabel(this);
        avatarLabel->setFixedSize(kAvatarSize, kAvatarSize);
        avatarLabel->setPixmap(
            ui_icons::communityAvatarIcon(message.author.left(1).toUpper()).pixmap(kAvatarSize, kAvatarSize));
        layout->addWidget(avatarLabel, /*stretch=*/0, Qt::AlignTop);

        auto* contentLayout = new QVBoxLayout;
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(2);

        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(ui_theme::kSpacingSm);
        auto* authorLabel = new QLabel(message.author, this);
        authorLabel->setProperty("chatAuthor", true);
        auto* timeLabel = new QLabel(formatTime(message.sentAt), this);
        timeLabel->setObjectName(QStringLiteral("mutedDescription"));
        headerRow->addWidget(authorLabel);
        headerRow->addWidget(timeLabel);
        headerRow->addStretch();

        contentLayout->addLayout(headerRow);
        contentLayout->addWidget(bodyLabel);
        layout->addLayout(contentLayout, /*stretch=*/1);
    } else {
        layout->addSpacing(kAvatarSize);
        layout->addWidget(bodyLabel, /*stretch=*/1);
    }
}

}  // namespace devicehub
