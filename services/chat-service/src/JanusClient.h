#pragma once

// Полный, а не json_fwd.hpp: sendMessage() ниже берёт std::optional<nlohmann::json>
// с default-аргументом std::nullopt — MSVC инстанцирует специальные
// члены std::optional<T> уже при разборе этого объявления, для чего T
// (nlohmann::json) должен быть полным типом в этой точке, а не просто
// объявлен.
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace chat_service {

/**
 * @brief Тонкий HTTP-клиент к plain HTTP JSON API Janus Gateway (issue
 *        #230/#231/#232) — знает только протокол Janus (сессия/handle
 *        плагина videoroom, отправка сообщений, long-poll за асинхронными
 *        событиями), ничего не знает про Postgres/каналы/членство. Решение,
 *        какому каналу какая комната нужна и можно ли вызывающей стороне
 *        к ней присоединиться, принимает ChatService/WebSocketServer.
 *
 * Два независимых потребителя протокола:
 *  - ensureRoomExists() (issue #231) — сам создаёт короткоживущую сессию
 *    Janus только на время одной проверки/создания комнаты.
 *  - createSession()/attachHandle()/sendMessage()/longPollOnce() (issue
 *    #232) — обобщённые примитивы протокола для прокси-сигналинга
 *    WebSocketServer: одна сессия на WS-подключение DeviceHub-клиента,
 *    живущая всё время звонка (publish/subscribe handle'ы), с отдельным
 *    потоком, гоняющим longPollOnce() в цикле — сам JanusClient этим
 *    циклом не управляет, только даёт из чего его собрать.
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

    /// Создаёт новую сессию Janus ("janus":"create").
    [[nodiscard]] std::optional<std::int64_t> createSession() const;

    /// Attach'ит новый handle плагина @p plugin (например,
    /// "janus.plugin.videoroom") к сессии @p sessionId.
    [[nodiscard]] std::optional<std::int64_t> attachHandle(std::int64_t sessionId, const std::string& plugin) const;

    /// Отправляет @p body (+опционально @p jsep) как "message" указанному
    /// handle'у — возвращает ровно то, что ответил Janus на сам POST, без
    /// какой-либо магии: это может быть "ack" (реальный результат придёт
    /// асинхронно, забрать его должен вызывающий через отдельный
    /// longPollOnce() на этой же сессии) или немедленный "success"/"event".
    /// В отличие от приватного фолбэка ensureRoomExists() ниже, здесь
    /// вызывающая сторона (WebSocketServer, issue #232) сама решает, что
    /// делать с "ack" — у неё уже есть свой постоянный цикл long-poll на
    /// эту сессию, второй, разовый, здесь был бы гонкой за одну и ту же
    /// асинхронную очередь Janus.
    [[nodiscard]] std::optional<nlohmann::json> sendMessage(std::int64_t sessionId, std::int64_t handleId,
                                                             const nlohmann::json& body,
                                                             const std::optional<nlohmann::json>& jsep = std::nullopt) const;

    /// Один блокирующий long-poll GET за асинхронными событиями сессии
    /// @p sessionId. JanusClient не хранит между вызовами никакого
    /// состояния цикла — организация цикла (и его остановка) на совести
    /// вызывающей стороны.
    [[nodiscard]] std::optional<nlohmann::json> longPollOnce(std::int64_t sessionId) const;

private:
    [[nodiscard]] std::optional<nlohmann::json> post(const std::string& path, const nlohmann::json& body) const;
    /// Отправляет запрос плагину videoroom; если Janus ответил "ack"
    /// (запрос обрабатывается асинхронно), забирает реальный результат
    /// через долгий опрос сессии — только для ensureRoomExists(), у
    /// которой нет своего постоянного цикла long-poll (см. doc-комментарий
    /// класса) — см. также комментарий об этом протоколе в
    /// services/janus/verify/verify-forwarding.mjs, где он был впервые
    /// исследован для issue #230.
    [[nodiscard]] std::optional<nlohmann::json> sendPluginMessageWithAckFallback(
        std::int64_t sessionId, std::int64_t handleId, const nlohmann::json& requestBody) const;

    std::string host_;
    int port_;
};

}  // namespace chat_service
