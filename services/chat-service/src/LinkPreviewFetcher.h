#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace chat_service {

/**
 * @brief Скачивает HTML внешней страницы по URL, найденному в тексте
 *        сообщения (issue #396), с защитой от SSRF — единственная точка
 *        в chat-service, инициирующая сетевой запрос по адресу,
 *        выбранному недоверенным пользователем, а не самим сервисом.
 *
 * Для каждого хоста (включая цель каждого редиректа) резолвит DNS сам
 * (PrivateNetworkCheck::resolveFirstAddress()), проверяет полученный IP
 * (PrivateNetworkCheck::isPrivateOrReserved()) и лишь затем подключается
 * — причём подключается именно к этому проверенному IP
 * (httplib::Client::set_hostname_addr_map()), а не даёт httplib
 * резолвить хост заново самостоятельно: иначе между проверкой и
 * реальным подключением DNS для того же хоста мог бы отдать другой (уже
 * не проверенный) адрес — классический TOCTOU/DNS rebinding. Редиректы
 * не доверяются httplib's set_follow_location() ровно по той же
 * причине — каждый хоп разбирается и проверяется этим классом заново,
 * а не следуется автоматически.
 */
class LinkPreviewFetcher {
public:
    /// @p requestTimeout — на каждое отдельное HTTP-соединение (не
    /// суммарно на все редиректы). @p maxResponseBytes — читать не
    /// больше этого объёма тела ответа: og-теги всегда в `<head>`,
    /// который на практике укладывается в первые несколько десятков КБ
    /// даже самых тяжёлых страниц. @p maxRedirects ограничивает цепочку
    /// 3xx-переходов. @p isAddressAllowed — резолвленный IP разрешён для
    /// подключения, если возвращает true; по умолчанию — обратное
    /// PrivateNetworkCheck::isPrivateOrReserved() (issue #396, защита от
    /// SSRF). Существует как параметр исключительно для тестов, которым
    /// нужно фетчить локальную тестовую фикстуру на 127.0.0.1, —
    /// единственный вызывающий код в продакшене (LinkPreviewService)
    /// никогда не передаёт ничего, кроме значения по умолчанию.
    explicit LinkPreviewFetcher(std::chrono::seconds requestTimeout = std::chrono::seconds{5},
                                 std::size_t maxResponseBytes = 256 * 1024, int maxRedirects = 3,
                                 std::function<bool(const std::string&)> isAddressAllowed = defaultIsAddressAllowed);

    /// @return тело ответа (обрезанное до maxResponseBytes), либо
    /// nullopt, если @p url имеет недопустимую схему, любой хост в
    /// цепочке (сам URL или цель редиректа) резолвится в приватный/
    /// служебный адрес, превышен лимит редиректов, истёк таймаут, или
    /// сервер ответил не 2xx/3xx.
    [[nodiscard]] std::optional<std::string> fetch(const std::string& url) const;

private:
    [[nodiscard]] static bool defaultIsAddressAllowed(const std::string& resolvedIp);

    std::chrono::seconds requestTimeout_;
    std::size_t maxResponseBytes_;
    int maxRedirects_;
    std::function<bool(const std::string&)> isAddressAllowed_;
};

}  // namespace chat_service
