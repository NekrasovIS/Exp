#include "JsonGuard.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

/// libFuzzer-обвязка для разбора сырых байт WebSocket-фрейма
/// chat-service (issue #121/#133) — в отличие от
/// SubscribedDispatchSurvivesAdversarialPayloads (GTest на живом
/// сервере с заранее придуманным корпусом враждебных, но валидных JSON
/// payload'ов), здесь фаззер сам исследует пространство сырых байт по
/// обратной связи от покрытия — включая совсем не-JSON и синтаксически
/// битый ввод, который GTest-корпус не перечисляет вручную.
///
/// Не вызывает сам WebSocketServer::handleHello()/handleSubscribedMessage()
/// напрямую — им нужны живые ix::WebSocket/ChatService/AuthServiceClient
/// (сетевой сокет, Postgres), что не подходит для быстрого
/// in-process libFuzzer-цикла. Вместо этого повторяет здесь ту же
/// последовательность разбора и те же обращения к полям JSON
/// (json_guard::exceedsMaxNestingDepth() → nlohmann::json::parse() с
/// отключёнными исключениями → .get<T>() на предполагаемых полях
/// каждой из известных форм кадра), что и сам WebSocketServer.cpp —
/// падение/UB здесь означает падение/UB и там, при этом не требует
/// собственно поднятого сервера. Пробует сразу обе формы (Hello и
/// кадр уже подписанного соединения) на одних и тех же байтах, а не
/// только одну — реальный клиент мог прислать их в любом порядке, а
/// сервер отличает их только по тому, есть ли уже подписка на этот
/// сокет, что для fuzz-таргета не имеет значения.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string payload(reinterpret_cast<const char*>(data), size);

    if (chat_service::json_guard::exceedsMaxNestingDepth(payload, chat_service::json_guard::kMaxNestingDepth)) {
        return 0;
    }
    const nlohmann::json body = nlohmann::json::parse(payload, nullptr, /*allow_exceptions=*/false);
    if (body.is_discarded()) {
        return 0;
    }

    // Hello: {"token", "channel_id"} или {"token", "dm_thread_id"}.
    if (body.contains("token") && body["token"].is_string()) {
        if (body.contains("channel_id") && body["channel_id"].is_number_integer()) {
            static_cast<void>(body["channel_id"].get<std::int64_t>());
        }
        if (body.contains("dm_thread_id") && body["dm_thread_id"].is_number_integer()) {
            static_cast<void>(body["dm_thread_id"].get<std::int64_t>());
        }
    }

    // Кадры подписанного соединения — та же диспетчеризация по
    // наличию ключа, что и в WebSocketServer::handleSubscribedMessage().
    if (body.contains("call_signal")) {
        const nlohmann::json& signal = body["call_signal"];
        if (signal.is_object() && signal.contains("to") && signal["to"].is_string()) {
            static_cast<void>(signal["to"].get<std::string>());
        }
    } else if (body.contains("edit_message")) {
        const nlohmann::json& edit = body["edit_message"];
        if (edit.is_object() && edit.contains("id") && edit["id"].is_number_integer()) {
            static_cast<void>(edit["id"].get<std::int64_t>());
        }
        if (edit.is_object() && edit.contains("body") && edit["body"].is_string()) {
            static_cast<void>(edit["body"].get<std::string>());
        }
    } else if (body.contains("delete_message")) {
        const nlohmann::json& del = body["delete_message"];
        if (del.is_object() && del.contains("id") && del["id"].is_number_integer()) {
            static_cast<void>(del["id"].get<std::int64_t>());
        }
    } else if (body.contains("body") && body["body"].is_string()) {
        static_cast<void>(body["body"].get<std::string>());
        if (body.contains("attachment_id") && body["attachment_id"].is_number_integer()) {
            static_cast<void>(body["attachment_id"].get<std::int64_t>());
        }
    }

    return 0;
}
