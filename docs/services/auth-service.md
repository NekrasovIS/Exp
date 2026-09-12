# auth-service

Один из трёх backend-сервисов DeviceHub (см. [README](../../README.md)
за общим описанием проекта и [docs/services/](.) за остальными двумя —
`user-service` и `chat-service`). Проверяет логин/пароль через user-service и только потом выдаёт
подписанный HMAC-SHA256 токен с ограниченным сроком жизни:

```bash
cmake -S services/auth-service -B services/auth-service/build
cmake --build services/auth-service/build --parallel
AUTH_SERVICE_SECRET=dev-only-secret ./services/auth-service/build/auth-service
```

`AUTH_SERVICE_SECRET` обязателен — без него сервис не запустится (см.
CLAUDE.md, «Безопасность»). По умолчанию слушает `127.0.0.1:8080`
(`AUTH_SERVICE_HOST`/`AUTH_SERVICE_PORT`), ходит в user-service по
`USER_SERVICE_HOST`/`USER_SERVICE_PORT` (по умолчанию `127.0.0.1:8081`).
REST: `POST /auth/token` (`{"login", "password"}`), `POST /auth/verify`,
`POST /auth/register` (`{"login", "password"}` — forwards to
user-service's own registration and, on success, immediately issues a
token too, so a fresh account is auto-logged-in without a second
request), `POST /auth/refresh` (`{"refresh_token"}` — issue #105,
exchanges a still-valid refresh token for a fresh access token without
re-entering credentials). `/auth/token` and `/auth/register` both
return `refresh_token` alongside `token`/`expires_at` — long-lived
(30 days by default), doesn't rotate on refresh, and is rejected if
presented to `/auth/verify` or `/auth/token`-protected routes as if it
were an access token (`TokenService` marks it with `"typ": "refresh"`
in the signed payload). `/auth/token` and `/auth/register` are rate
limited per client address (issue #102) — `/auth/refresh` isn't,
matching `/auth/verify`'s reasoning (not meaningfully brute-forceable;
it needs a valid signed token, not a guessed password).

Вход по одноразовому коду (issue #156/#174): `POST /auth/otp/request`
(`{"identifier"}` — login, email или Telegram chat_id, всегда отвечает
200, чтобы ответ нельзя было использовать для проверки существования
аккаунта) и `POST /auth/otp/verify` (`{"identifier", "code"}` — при
совпадении ещё действующего кода выдаёт `token`/`refresh_token`, та же
форма ответа, что у `/auth/token`). Коды — 6-значные, живут 5 минут,
хранятся хешированными в памяти (у auth-service нет своей БД),
максимум 5 попыток ввода.

Доставка кода — через один из двух каналов, оба реализованы напрямую
поверх OpenSSL (`TlsConnection`), без libcurl:

- Email через SMTP — `SMTP_HOST`/`SMTP_PORT`/`SMTP_USERNAME`/
  `SMTP_PASSWORD`/`SMTP_FROM`.
- Telegram через Bot API (issue #174) — `TELEGRAM_BOT_TOKEN`; один
  HTTPS POST на `api.telegram.org/bot<token>/sendMessage`. Пользователь
  привязывает аккаунт, начав чат с ботом (бот присылает свой chat_id),
  и вставляет его в настройки профиля (`telegram_chat_id`).

Если у аккаунта заданы оба канала и на сервере настроен
`TELEGRAM_BOT_TOKEN`, код уходит в Telegram — мгновенная доставка в
чат вместо письма, которое может уйти в спам или прийти с задержкой.
Если ни `SMTP_HOST`, ни `TELEGRAM_BOT_TOKEN` не заданы, код просто
логируется в stdout вместо реальной отправки — так можно пройти весь
flow локально/в CI без настоящих учётных данных. У пользователя должен
быть задан хотя бы один из двух каналов (`PATCH /users/me` на
user-service, поля `email`/`telegram_chat_id`), иначе входить по коду
ему пока нельзя.

