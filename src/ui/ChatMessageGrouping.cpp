#include "ui/ChatMessageGrouping.h"

#include <QtGlobal>

namespace devicehub::chat_message_grouping {

namespace {
constexpr int kGroupingWindowSeconds = 5 * 60;
}  // namespace

QDateTime parseSentAt(const QString& raw) {
    if (const QDateTime isoParsed = QDateTime::fromString(raw, Qt::ISODateWithMs); isoParsed.isValid()) {
        return isoParsed;
    }
    // chat-service сериализует метки времени Postgres через пробел
    // ("YYYY-MM-DD HH:MM:SS.ffffff"), а не через ISO-разделитель 'T'.
    QString normalized = raw;
    normalized.replace(QLatin1Char(' '), QLatin1Char('T'));
    return QDateTime::fromString(normalized, Qt::ISODateWithMs);
}

bool shouldGroupWithPrevious(const ChatMessage& previous, const ChatMessage& current) {
    if (previous.author != current.author) {
        return false;
    }

    const QDateTime previousTime = parseSentAt(previous.sentAt);
    const QDateTime currentTime = parseSentAt(current.sentAt);
    if (!previousTime.isValid() || !currentTime.isValid()) {
        return false;
    }

    return qAbs(previousTime.secsTo(currentTime)) <= kGroupingWindowSeconds;
}

}  // namespace devicehub::chat_message_grouping
