#include "ui/ChatMessageRow.h"

#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QResizeEvent>
#include <QVBoxLayout>

#include "ui/ChatBubble.h"
#include "ui/ChatMessageGrouping.h"
#include "ui/IconFactory.h"

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
constexpr const char* kOwnTextColor = "#ffffff";

QString formatTime(const QString& rawSentAt) {
    const QDateTime parsed = chat_message_grouping::parseSentAt(rawSentAt);
    return parsed.isValid() ? parsed.toString(QStringLiteral("HH:mm")) : rawSentAt;
}

}  // namespace

ChatMessageRow::ChatMessageRow(const ChatMessage& message, bool showHeader, bool isOwnMessage, QWidget* parent)
    : QWidget(parent) {
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

    auto* bodyLabel = new QLabel(message.body, bubble_);
    bodyLabel->setObjectName(QStringLiteral("chatMessageBody"));
    bodyLabel->setWordWrap(true);
    // Issue #94: render **bold**/*italic*/`code`/links/lists via Qt's
    // own markdown-to-richtext conversion rather than a hand-rolled
    // parser. QLabel doesn't wire up network image loading for rich
    // text on its own, so a message body isn't a vector for fetching
    // attacker-controlled URLs (e.g. a tracking-pixel image) — links
    // only ever open on an explicit click (setOpenExternalLinks()),
    // never automatically.
    bodyLabel->setTextFormat(Qt::MarkdownText);
    bodyLabel->setOpenExternalLinks(true);
    bodyLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    if (isOwnMessage) {
        bodyLabel->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
    }

    if (showHeader) {
        auto* headerRow = new QHBoxLayout;
        headerRow->setSpacing(spacing);
        if (!isOwnMessage) {
            auto* authorLabel = new QLabel(message.author, bubble_);
            authorLabel->setProperty("chatAuthor", true);
            headerRow->addWidget(authorLabel);
        }
        auto* timeLabel = new QLabel(formatTime(message.sentAt), bubble_);
        timeLabel->setObjectName(QStringLiteral("mutedDescription"));
        if (isOwnMessage) {
            timeLabel->setStyleSheet(QStringLiteral("color: %1;").arg(QLatin1String(kOwnTextColor)));
        } else {
            headerRow->addStretch();
        }
        headerRow->addWidget(timeLabel);
        bubbleLayout->addLayout(headerRow);
    }
    bubbleLayout->addWidget(bodyLabel);

    if (isOwnMessage) {
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

void ChatMessageRow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    bubble_->setMaximumWidth(qMax(1, qRound(width() * kMaxBubbleWidthFraction)));
}

}  // namespace devicehub
