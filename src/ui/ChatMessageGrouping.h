#pragma once

#include <QDateTime>

#include "ui/ChatMessageRow.h"

namespace devicehub::chat_message_grouping {

/// Разбирает строку метки времени от chat-service (сериализация
/// Postgres, разделённая пробелом, например "2026-08-05 09:14:23.123456")
/// в QDateTime; возвращает невалидный QDateTime, если @p raw не
/// удалось разобрать.
QDateTime parseSentAt(const QString& raw);

/// True, если @p current нужно сгруппировать с @p previous — тот же
/// автор и в пределах небольшого временного окна — чтобы ChatView мог
/// не повторять для него заголовок с аватаром/именем/временем. False,
/// если хотя бы одну из меток времени не удалось разобрать, — так
/// группировка происходит только тогда, когда её можно подтвердить.
bool shouldGroupWithPrevious(const ChatMessage& previous, const ChatMessage& current);

}  // namespace devicehub::chat_message_grouping
