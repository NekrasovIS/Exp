#pragma once

#include <string>

namespace chat_service {

/**
 * @brief Вызывает GET /internal/friendship на user-service, чтобы
 *        разрешить открытие нового диалога личных сообщений только
 *        между друзьями (issue #187, Фаза 2).
 *
 * Отказывает закрыто (fail closed), как и AuthServiceClient: любая
 * сетевая/протокольная ошибка трактуется как "не друзья", а не
 * приводит к исключению — недоступный user-service не должен открывать
 * доступ к новым диалогам по умолчанию.
 */
class UserServiceClient {
public:
    UserServiceClient(std::string host, int port);

    /// Дружба симметрична — @p loginA/@p loginB можно менять местами без
    /// изменения результата.
    [[nodiscard]] bool areFriends(const std::string& loginA, const std::string& loginB) const;

    /// Issue #471 — вызывает GET /internal/blocked, чтобы chat-service
    /// мог запретить открытие нового диалога, если @p blockerLogin
    /// заблокировал @p blockedLogin. Направленно, в отличие от
    /// areFriends() выше — вызывающая сторона (handleOpenThread) зовёт
    /// это дважды, если нужна проверка в обе стороны. Fail closed, как
    /// и areFriends(): любая сетевая/протокольная ошибка трактуется как
    /// "заблокирован" — недоступный user-service не должен ОТКРЫВАТЬ
    /// новые диалоги по умолчанию (тот же принцип, что у fail closed для
    /// дружбы, только в другую сторону, поскольку здесь true запрещает,
    /// а не разрешает).
    [[nodiscard]] bool isBlocked(const std::string& blockerLogin, const std::string& blockedLogin) const;

private:
    std::string host_;
    int port_;
};

}  // namespace chat_service
