# user-service

Один из трёх backend-сервисов DeviceHub (см. [README](../../README.md)
за общим описанием проекта и [docs/services/](.) за остальными двумя —
`auth-service` и `chat-service`). Владеет Postgres (пользователи: логин + Argon2id-хеш пароля, пароли в
открытом виде не хранятся и не логируются). `docker compose up -d`
поднимает Postgres для user-service (порт 5433) и chat-service (порт 5434) разом — у каждого сервиса своя база (database-per-service), и
оба порта выбраны так, чтобы не конфликтовать с Postgres на 5432,
если он уже используется другим проектом на этой машине.

```bash
docker compose up -d
cmake -S services/user-service -B services/user-service/build
cmake --build services/user-service/build --parallel
./services/user-service/build/user-service
```

По умолчанию слушает `127.0.0.1:8081` (`USER_SERVICE_HOST`/`USER_SERVICE_PORT`),
подключается к Postgres из `docker-compose.yml`
(`USER_SERVICE_DATABASE_URL` — переопределить). REST:
`POST /users/register`, `POST /users/verify-credentials` (оба без
авторизации — вызываются самим auth-service), `GET /users/{login}/profile`
и `PATCH /users/me` (issue #110, оба требуют `Authorization: Bearer
<token>` — проверяется через собственный `AuthServiceClient`, POST
`/auth/verify` к auth-service, `AUTH_SERVICE_HOST`/`AUTH_SERVICE_PORT`).
`PATCH /users/me` всегда пишет в логин из проверенного токена — логин
в теле запроса, если есть, игнорируется; частичное тело (только
`display_name`, только `avatar_url`, только `public_key`, только
`email` или только `telegram_chat_id`) не затирает несопровождённые
поля. `GET /users/{login}/profile` отдаёт `email`/`telegram_chat_id`
только когда `{login}` — сам вызывающий (issue #225/#243, pentest-
находка: раньше отдавал их для любого запрошенного логина) — просмотр
чужого профиля получает урезанную версию без этих двух полей;
`login`/`display_name`/`avatar_url`/`public_key` видны всегда, они и
задуманы публичными (`public_key`, например, нужен другим клиентам для
E2E-шифрования, issue #136/#138/#217). Ещё один эндпоинт —
`POST /users/resolve-otp-identifier` (issue
#156/#174, без авторизации, вызывается самим auth-service) — принимает
`{"identifier"}` (login, email или Telegram chat_id) и отвечает
`{"found", "login", "email", "telegram_chat_id"}`, используется для
входа по одноразовому коду.

Заявки в друзья (issue #187, Фаза 1 — backend, без UI; сама фича
описана в [docs/FEATURES.md](../FEATURES.md)) — все требуют `Authorization:
Bearer`: `POST /friends/requests {recipient_login}` (201 `"sent"`, либо
`"accepted"`, если у получателя уже была встречная pending-заявка —
взаимный интерес сразу становится дружбой без отдельного accept),
`GET /friends/requests` (входящие pending-заявки вызывающего),
`POST /friends/requests/{id}/accept` / `.../decline` (только адресат
конкретной заявки), `GET /friends` (список друзей вызывающего),
`DELETE /friends/{login}` (расфрендить, работает в любую сторону пары).
`GET /internal/friendship?user_a=&user_b=` (issue #187, Фаза 2, без
авторизации — вызывается только chat-service, не клиентами напрямую,
чтобы решить, можно ли открыть новый диалог личных сообщений; отвечает
`{"friends": bool}`).

Pentest issue #225 (без находок, кроме #243 выше): accept/decline чужой
заявки и повторное использование id уже обработанной заявки оба
отклоняются одним и тем же 404 (`WHERE recipient_login = ... AND
status = 'pending'` в `UserRepository::respondToFriendRequest()`) — не
палит существование чужой заявки другим кодом ответа. `PATCH /users/me`
никогда не читает `login` из тела запроса — самозванство через тело
невозможно. `/users/register`, `/users/verify-credentials`,
`/users/resolve-otp-identifier` без авторизации по конструкции
(вызываются только auth-service) — каждый принимает и возвращает
данные ровно по одному переданному логину/identifier, без пакетного
доступа или перечисления остальных пользователей.

`email` (issue #156) и `telegram_chat_id` (issue #174, вход по
одноразовому коду) — оба уникальны, если заданы (частичные уникальные
индексы, допускают несколько `NULL`); попытка поставить уже занятое
другим аккаунтом значение через `PATCH /users/me` отвечает `409`.

`public_key` (issue #136, Phase 1 сквозного шифрования — см.
[docs/FEATURES.md](../FEATURES.md), «Сквозное шифрование») — base64 публичной половины X25519-пары,
которую клиент публикует сюда; сам сервер её никак не использует,
только хранит и отдаёт для того, чтобы другие клиенты могли
шифровать этому пользователю.

