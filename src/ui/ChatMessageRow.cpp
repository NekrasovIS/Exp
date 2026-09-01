#include "ui/ChatMessageRow.h"

#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui/ChatBubble.h"
#include "ui/ChatMessageGrouping.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

namespace devicehub {

namespace {

constexpr qreal kAvatarEm = 2.0;
constexpr qreal kSpacingEm = 0.4;
constexpr qreal kBubblePaddingHEm = 0.7;
constexpr qreal kBubblePaddingVEm = 0.4;
constexpr qreal kBubbleInnerSpacingEm = 0.15;
constexpr qreal kNewGroupTopMarginEm = 0.5;
constexpr qreal kGroupedTopMarginEm = 0.08;
constexpr qreal kMaxBubbleWidthFraction = 0.7;
constexpr const char* kOwnTextColor = ui_theme::kAccentForeground;

QString formatTime(const QString& rawSentAt) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(rawSentAt);
    return parsed.isValid() ? parsed.toString(QStringLiteral("HH:mm")) : rawSentAt;
}

}  // namespace

ChatMessageRow::ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage, QWidget* parent)
    : QWidget(parent), messageId_(message.id) {
    const qreal em = QFontMetricsF(font()).height();
    const int avatarSize = qRound(em * kAvatarEm);
    const int spacing = qRound(em * kSpacingEm);
    const int bubblePaddingH = qRound(em * kBubblePaddingHEm);
    const int bubblePaddingV = qRound(em * kBubblePaddingVEm);
    const int bubbleInnerSpacing = qRound(em * kBubbleInnerSpacingEm);
    const int topMargin = qRound(em * (showHeader ? kNewGroupTopMarginEm : kGroupedTopMarginEm));

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, topMargin, 0, 0);
    rootLayout->setSpacing(spacing);

    bubble_ = new ChatBubble(isOwnMessage, this);
    auto* bubbleLayout = new QVBoxLayout(bubble_);
    bubbleLayout->setContentsMargins(bubblePaddingH, bubblePaddingV, bubblePaddingH, bubblePaddingV);
    bubbleLayout->setSpacing(bubbleInnerSpacing);

    bodyLabel_ = new QLabel(message.body, bubble_);
    bodyLabel_->setObjectName(QStringLiteral("chatMessageBody"));
    bodyLabel_->setWordWrap(true);
    // Issue #94: render **bold**/*italic*/`code`/links/lists via Qt's
    // own markdown-to-richtext conversion rather than a hand-rolled
    // parser. QLabel doesn't wire up network image loading for rich
    // text on its own, so a message body isn't a vector for fetching
    // attacker-controlled URLs (e.g. a tracking-pixel image) — links
    // only ever open on an explicit click (setOpenExternalLinks()),
    // never automatically.
    bodyLabel_->setTextFormat(Qt::MarkdownText);
    bodyLabel_->setOpenExternalLinks(true);
    bodyLabel_->setTextInteractionFlags(Qt::TextBrowserInteraction);
    if (isOwnMessage) {
        bodyLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
    }

    formattedSentAt_ = formatTime(message.sentAt);
    if (showHeader) {
        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(spacing);
        if (!isOwnMessage) {
            auto* authorLabel = new QLabel(message.author, bubble_);
            authorLabel->setProperty("chatAuthor", true);
            headerRow->addWidget(authorLabel);
        }
        const QString timeText =
            message.editedAt.has_value() ? formattedSentAt_ + QStringLiteral(" (edited)") : formattedSentAt_;
        timeLabel_ = new QLabel(timeText, bubble_);
        timeLabel_->setObjectName(QStringLiteral("mutedDescription"));
        if (isOwnMessage) {
            timeLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
        } else {
            headerRow->addStretch();
        }
        headerRow->addWidget(timeLabel_);
        bubbleLayout->addLayout(headerRow);
    }
    bubbleLayout->addWidget(bodyLabel_);

    if (message.attachmentId >= 0) {
        auto* downloadButton = new QPushButton(tr("Download: %1").arg(message.attachmentFilename), bubble_);
        downloadButton->setObjectName(QStringLiteral("downloadAttachmentButton"));
        if (isOwnMessage) {
            downloadButton->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
        }
        const qint64 attachmentId = message.attachmentId;
        const QString attachmentFilename = message.attachmentFilename;
        connect(downloadButton, &QPushButton::clicked, this, [this, attachmentId, attachmentFilename]() {
            emit downloadRequested(attachmentId, attachmentFilename);
        });
        bubbleLayout->addWidget(downloadButton);
    }

    if (isOwnMessage) {
        // Available on every own-message row regardless of showHeader —
        // grouped (consecutive) messages don't repeat their header, but
        // each individual message still needs its own way to target it
        // for editing/deleting (issue #107).
        auto* controlsRow = new QHBoxLayout;
        controlsRow->setSpacing(spacing);
        controlsRow->addStretch(1);
        auto* editButton = new QPushButton(tr("Edit"), bubble_);
        editButton->setObjectName(QStringLiteral("editMessageButton"));
        editButton->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
        connect(editButton, &QPushButton::clicked, this,
                [this]() { emit editRequested(messageId_, bodyLabel_->text()); });
        auto* deleteButton = new QPushButton(tr("Delete"), bubble_);
        deleteButton->setObjectName(QStringLiteral("deleteMessageButton"));
        deleteButton->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
        connect(deleteButton, &QPushButton::clicked, this, [this]() { emit deleteRequested(messageId_); });
        controlsRow->addWidget(editButton);
        controlsRow->addWidget(deleteButton);
        bubbleLayout->addLayout(controlsRow);

        rootLayout->addStretch(1);
        rootLayout->addWidget(bubble_);
    } else {
        if (showHeader) {
            auto* avatarLabel = new QLabel(this);
            avatarLabel->setFixedSize(avatarSize, avatarSize);
            avatarLabel->setPixmap(
                ui_icons::communityAvatarIcon(message.author.left(1).toUpper()).pixmap(avatarSize, avatarSize));
            rootLayout->addWidget(avatarLabel, /*stretch=*/0, Qt::AlignTop);
        } else {
            rootLayout->addSpacing(avatarSize);
        }
        rootLayout->addWidget(bubble_);
        rootLayout->addStretch(1);
    }
}

void ChatMessageRow::updateBody(const QString& newBody) {
    bodyLabel_->setText(newBody);
    if (timeLabel_ != nullptr) {
        timeLabel_->setText(formattedSentAt_ + QStringLiteral(" (edited)"));
    }
}

void ChatMessageRow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    bubble_->setMaximumWidth(qMax(1, qRound(width() * kMaxBubbleWidthFraction)));
}

}  // namespace devicehub
