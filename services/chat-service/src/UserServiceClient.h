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

    [[nodiscard]] bool areFriends(const std::string& loginA, const std::string& loginB) const;

private:
    std::string host_;
    int port_;
};

}  // namespace chat_service
