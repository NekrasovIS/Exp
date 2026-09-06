#pragma once

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace chat_service {

/**
 * @brief Тонкий HTTP-клиент к plain HTTP JSON API Janus Gateway (issue
 *        #230/#231) — знает только протокол Janus (сессия/handle плагина
 *        videoroom, отправка сообщений с long-poll фолбэком на асинхронные
 *        события), ничего не знает про Postgres/каналы/членство. Решение,
 *        какому каналу какая комната нужна и можно ли вызывающей стороне
 *        к ней присоединиться, принимает ChatService::ensureCallRoom().
 *
 * Отказывает закрыто (fail closed), как и AuthServiceClient/
 * UserServiceClient: любая сетевая/протокольная ошибка возвращается как
 * std::nullopt/false, не бросает исключений.
 */
class JanusClient {
public:
    JanusClient(std::string host, int port);

    /// Гарантирует существование videoroom-комнаты @p roomId в Janus —
    /// идемпотентно: уже существующая комната (в т.ч. созданная только что
    /// другим экземпляром chat-service — гонка) считается успехом.
    /// @return False только при сетевой ошибке/некорректном ответе Janus.
    [[nodiscard]] bool ensureRoomExists(const std::string& roomId) const;

private:
    [[nodiscard]] std::optional<nlohmann::json> post(const std::string& path, const nlohmann::json& body) const;
    [[nodiscard]] std::optional<nlohmann::json> longPoll(std::int64_t sessionId) const;
    [[nodiscard]] std::optional<std::int64_t> createSession() const;
    [[nodiscard]] std::optional<std::int64_t> attachVideoroomHandle(std::int64_t sessionId) const;
    /// Отправляет запрос плагину videoroom; если Janus ответил "ack"
    /// (запрос обрабатывается асинхронно), забирает реальный результат
    /// через long-poll — см. комментарий об этом в
    /// services/janus/verify/verify-forwarding.mjs, где тот же протокол
    /// был впервые исследован для issue #230.
    [[nodiscard]] std::optional<nlohmann::json> sendPluginMessage(std::int64_t sessionId, std::int64_t handleId,
                                                                    const nlohmann::json& requestBody) const;

    std::string host_;
    int port_;
};

}  // namespace chat_service
