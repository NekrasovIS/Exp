#include "ui/ChatMessageRow.h"

#include <QAction>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPoint>
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
    bubble_->setObjectName(QStringLiteral("chatMessageBubble"));
    auto* bubbleLayout = new QVBoxLayout(bubble_);
    bubbleLayout->setContentsMargins(bubblePaddingH, bubblePaddingV, bubblePaddingH, bubblePaddingV);
    bubbleLayout->setSpacing(bubbleInnerSpacing);

    bodyLabel_ = new QLabel(message.body, bubble_);
    bodyLabel_->setObjectName(QStringLiteral("chatMessageBody"));
    bodyLabel_->setWordWrap(true);
    // Issue #94: рендерим **bold**/*italic*/`code`/ссылки/списки через
    // встроенное в Qt преобразование markdown в rich text, а не через
    // самописный парсер. QLabel сам по себе не подключает загрузку
    // изображений по сети для rich text, поэтому тело сообщения не
    // становится вектором для запроса URL, контролируемых атакующим
    // (например, изображения-трекера) — ссылки открываются только по
    // явному клику (setOpenExternalLinks()), никогда автоматически.
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
        // Контекстное меню по правому клику вместо всегда видимых кнопок
        // (issue #150) — доступно на каждой строке собственного сообщения
        // независимо от showHeader, поскольку сгруппированные
        // (последовательные) сообщения не повторяют заголовок, но
        // каждому отдельному сообщению всё равно нужен свой способ
        // адресации для редактирования/удаления (issue #107). Построено
        // через popup() (неблокирующий), а не exec(), чтобы тест мог
        // напрямую вызвать соответствующий QAction, не прокручивая
        // модальный event loop.
        bubble_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(bubble_, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            auto* menu = new QMenu(bubble_);
            menu->setAttribute(Qt::WA_DeleteOnClose);
            menu->setObjectName(QStringLiteral("chatMessageContextMenu"));
            QAction* editAction = menu->addAction(tr("Edit"));
            editAction->setObjectName(QStringLiteral("editMessageAction"));
            connect(editAction, &QAction::triggered, this,
                    [this]() { emit editRequested(messageId_, bodyLabel_->text()); });
            QAction* deleteAction = menu->addAction(tr("Delete"));
            deleteAction->setObjectName(QStringLiteral("deleteMessageAction"));
            connect(deleteAction, &QAction::triggered, this, [this]() { emit deleteRequested(messageId_); });
            menu->popup(bubble_->mapToGlobal(pos));
        });

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
