#pragma once

#include <optional>
#include <string>

namespace chat_service::private_network_check {

/// Резолвит @p hostname (DNS) и возвращает первый полученный IP-адрес в
/// текстовом виде (IPv4 или IPv6) — nullopt, если резолвинг не удался.
/// Если @p hostname уже сам является IP-литералом, возвращает его же
/// без обращения к DNS.
[[nodiscard]] std::optional<std::string> resolveFirstAddress(const std::string& hostname);

/// True, если @p ipAddress (текстовое представление IPv4 или IPv6,
/// включая IPv4-mapped IPv6 вида "::ffff:127.0.0.1") принадлежит
/// приватному/служебному диапазону: loopback (127.0.0.0/8, ::1),
/// RFC 1918 (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16), link-local
/// (169.254.0.0/16 — в том числе облачный metadata-эндпоинт
/// 169.254.169.254; fe80::/10), unique-local IPv6 (fc00::/7) и
/// 0.0.0.0/8. Issue #396 — превью ссылок ходит во внешний интернет по
/// адресу, найденному в тексте сообщения от недоверенного
/// пользователя; без этой проверки сервис превратился бы в открытый
/// SSRF-прокси во внутреннюю сеть/облачные метаданные.
[[nodiscard]] bool isPrivateOrReserved(const std::string& ipAddress);

}  // namespace chat_service::private_network_check
