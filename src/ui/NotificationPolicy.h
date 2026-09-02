#pragma once

class QString;

namespace devicehub::notification_policy {

/// True, если новое сообщение чата от @p messageAuthor должно вызывать
/// системное уведомление — не тогда, когда главное окно уже активно
/// (пользователь на него смотрит), и никогда для собственных сообщений
/// пользователя (chat-service возвращает эхом каждое сообщение и его
/// отправителю тоже, см. doc-комментарий ChatClient). Пустой
/// @p currentUserLogin (вход ещё не выполнен) никогда не подавляет
/// уведомление на проверке совпадения автора.
bool shouldNotify(bool windowActive, const QString& messageAuthor, const QString& currentUserLogin);

}  // namespace devicehub::notification_policy
