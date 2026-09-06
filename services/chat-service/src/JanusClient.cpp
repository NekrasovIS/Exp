#include "JanusClient.h"

#include <httplib.h>

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>

namespace chat_service {

namespace {

// Только для корреляции запрос/ответ в рамках одного HTTP-обмена с Janus —
// не секрет и не идентификатор пользователя, поэтому обычный счётчик
// достаточен (в отличие от OTP-кодов/приглашений, где нужен RAND_bytes).
std::string nextTransactionId() {
    static std::atomic<std::uint64_t> counter{0};
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::to_string(now) + "-" + std::to_string(counter.fetch_add(1, std::memory_order_relaxed));
}

// plugindata.data — там же, где сам videoroom-плагин кладёт "exists"/
// "error_code"/"videoroom" и т.п.; отсутствие узла означает, что ответ не
// от плагина (например, core-level "error") — пустой объект вместо
// исключения, дальше по коду это просто не совпадёт ни с одним ожидаемым полем.
nlohmann::json videoroomData(const nlohmann::json& response) {
    if (!response.contains("plugindata") || !response["plugindata"].contains("data")) {
        return nlohmann::json::object();
    }
    return response["plugindata"]["data"];
}

}  // namespace

JanusClient::JanusClient(std::string host, int port) : host_(std::move(host)), port_(port) {}

std::optional<nlohmann::json> JanusClient::post(const std::string& path, const nlohmann::json& body) const {
    httplib::Client client(host_, port_);

    nlohmann::json withTransaction = body;
    withTransaction["transaction"] = nextTransactionId();
    const httplib::Result result = client.Post(path, withTransaction.dump(), "application/json");
    if (!result || result->status != 200) {
        return std::nullopt;
    }

    nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded()) {
        return std::nullopt;
    }
    return response;
}

std::optional<nlohmann::json> JanusClient::longPoll(std::int64_t sessionId) const {
    httplib::Client client(host_, port_);

    const std::string path = "/janus/" + std::to_string(sessionId) + "?maxev=1&rid=" + nextTransactionId();
    const httplib::Result result = client.Get(path);
    if (!result || result->status != 200) {
        return std::nullopt;
    }

    nlohmann::json response = nlohmann::json::parse(result->body, nullptr, /*allow_exceptions=*/false);
    if (response.is_discarded()) {
        return std::nullopt;
    }
    return response;
}

std::optional<std::int64_t> JanusClient::createSession() const {
    const std::optional<nlohmann::json> response = post("/janus", nlohmann::json{{"janus", "create"}});
    if (!response || response->value("janus", "") != "success") {
        return std::nullopt;
    }
    return (*response)["data"]["id"].get<std::int64_t>();
}

std::optional<std::int64_t> JanusClient::attachVideoroomHandle(std::int64_t sessionId) const {
    const std::optional<nlohmann::json> response =
        post("/janus/" + std::to_string(sessionId),
             nlohmann::json{{"janus", "attach"}, {"plugin", "janus.plugin.videoroom"}});
    if (!response || response->value("janus", "") != "success") {
        return std::nullopt;
    }
    return (*response)["data"]["id"].get<std::int64_t>();
}

std::optional<nlohmann::json> JanusClient::sendPluginMessage(std::int64_t sessionId, std::int64_t handleId,
                                                               const nlohmann::json& requestBody) const {
    std::optional<nlohmann::json> response =
        post("/janus/" + std::to_string(sessionId) + "/" + std::to_string(handleId),
             nlohmann::json{{"janus", "message"}, {"body", requestBody}});
    if (!response) {
        return std::nullopt;
    }
    if (response->value("janus", "") == "ack") {
        response = longPoll(sessionId);
    }
    return response;
}

bool JanusClient::ensureRoomExists(const std::string& roomId) const {
    const std::optional<std::int64_t> sessionId = createSession();
    if (!sessionId) {
        return false;
    }
    const std::optional<std::int64_t> handleId = attachVideoroomHandle(*sessionId);
    if (!handleId) {
        return false;
    }

    const std::optional<nlohmann::json> existsResponse =
        sendPluginMessage(*sessionId, *handleId, nlohmann::json{{"request", "exists"}, {"room", roomId}});
    if (existsResponse && videoroomData(*existsResponse).value("exists", false)) {
        return true;
    }

    const std::optional<nlohmann::json> createResponse = sendPluginMessage(
        *sessionId, *handleId,
        nlohmann::json{
            {"request", "create"}, {"room", roomId}, {"publishers", 8}, {"bitrate", 512000}, {"videocodec", "vp8,h264"}});
    if (!createResponse) {
        return false;
    }
    const nlohmann::json data = videoroomData(*createResponse);
    if (data.value("videoroom", "") == "created") {
        return true;
    }
    // 427 = "room already exists" — гонка с другим экземпляром chat-service
    // или с параллельным call_join на тот же канал между проверкой exists
    // выше и этим create; сама комната от этого никуда не делась.
    return data.value("error_code", 0) == 427;
}

}  // namespace chat_service
