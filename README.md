# DeviceHub

[![CI](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml/badge.svg)](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml)

Десктопное Qt6-приложение для работы с устройствами ОС: аудио-выход,
микрофон, камера. Начиналось как демонстрация доступа к железу через
Qt Multimedia; развивается в сторону клиент-серверного инструмента с
подключением по web-ссылке (см. [CLAUDE.md](CLAUDE.md)). Раскладка
UI — сайдбар слева из двух колонок: узкая иконочная колонка сообществ
(круглые аватарки — первая буква названия; «+» снизу создаёт новое,
правый клик по аватарке — войти/переименовать/удалить) и список
каналов выбранного сообщества рядом с ней («+» создаёт канал, правый
клик — переименовать/удалить); клик по каналу открывает чат в основном
пространстве окна. Кнопка аккаунта в правом верхнем углу, футер с
профилем и настройками устройств (Audio Output, Microphone, Camera,
Screen Capture) слева. Тёмная тема с зелёным градиентным акцентом
(`src/ui/Theme.h`).

## Возможности

- Список доступных устройств вывода звука и проигрывание тестового
  синус-тона на выбранном устройстве.
- Список микрофонов и захват с индикатором уровня входного сигнала.
- Список камер и превью выбранной камеры.
- Список экранов и захват содержимого выбранного экрана с превью.
- Вход по логину/паролю: получение и проверка токена авторизации через
  отдельные сервисы `auth-service` + `user-service` (Postgres).
- Вход по одноразовому коду через email или Telegram (issue #156/#174) —
  окно `LoginWindow` показывается при каждом запуске (токен между
  перезапусками пока не сохраняется): ввод логина/email/Telegram
  chat_id → код доставляется по одному из привязанных каналов (Telegram
  в приоритете, если привязаны оба) → ввод кода завершает вход. Требует,
  чтобы у аккаунта уже был задан email и/или Telegram chat_id (`Account`
  → `Edit Profile`, поля `Email`/`Telegram chat ID`); своей
  passwordless-регистрации нет, только для уже существующих аккаунтов.
  Вход по паролю в `Account` (правый верхний угол) продолжает работать
  как раньше и тоже закрывает `LoginWindow` при успехе.
- Чаты и сообщества: подключение к каналу по ID и обмен сообщениями в
  реальном времени через `chat-service` (Postgres, WebSocket) — при
  открытии канала подгружается история сообщений, кнопка «Load older
  messages» подгружает более старые страницы.
- Desktop-уведомление (нативное OS, `QSystemTrayIcon`) о новом сообщении
  в открытом канале, пока окно DeviceHub не в фокусе — не показывается
  для собственных сообщений (`src/ui/DesktopNotifier`).
- Markdown-форматирование текста сообщений (**bold**, *italic*, `code`,
  списки, ссылки) — нативная конвертация Qt (`Qt::MarkdownText`), без
  своего парсера.
- Индикатор «печатает…» в открытом канале, пока собеседник набирает
  текст.
- Редактирование и удаление собственных сообщений (issue #107) —
  доступно только автору, с меткой «(edited)» на изменённых.
- Профиль пользователя (issue #110): отображаемое имя и ссылка на
  аватар, редактируются из «Account» → «Edit Profile»; футер и диалог
  показывают display_name вместо логина, как только он задан. Реальная
  загрузка/рендер картинки по avatar_url — за рамками этой версии,
  ссылка пока только хранится и показывается текстом.
- Роли и модерация (issue #114): владелец сообщества назначает
  модераторов («Manage Moderators…» в контекстном меню сообщества) —
  модератор может удалять чужие сообщения и переименовывать/удалять
  любые каналы сообщества, но не может редактировать чужие сообщения
  и не может управлять самим сообществом.
- Файловые вложения к сообщениям (issue #116, до 5 МБ) — кнопка
  «Attach» выбирает файл, загружает его на `chat-service` и сразу
  отправляет сообщение со ссылкой на вложение; у сообщения с вложением
  появляется кнопка «Download: <имя файла>» для сохранения на диск.
- Поиск по сообщениям открытого канала (issue #118): кнопка «Search» в
  шапке канала открывает окно с полем запроса и списком совпадений
  (регистронезависимая подстрока по тексту сообщения); клик по
  результату прокручивает список к сообщению, если оно уже подгружено.
- Сквозное шифрование (issue #122). **Phase 1** (issue #136) — при входе
  клиент один раз генерирует локальную X25519-пару ключей (libsodium),
  приватная половина никогда не покидает машину, публичная публикуется
  через `user-service`. **Phase 2** (issue #138) — чекбокс «Encrypted
  channel» в диалоге создания канала («+» в панели каналов); создатель
  генерирует симметричный ключ канала и заворачивает его для себя и
  каждого текущего участника сообщества, у кого уже есть опубликованный
  публичный ключ (у кого нет — тост с предупреждением, доступа не
  получит, пока владелец/модератор не завернёт ключ для него отдельно).
  Заголовок открытого зашифрованного канала — с иконкой замка 🔒; тело
  сообщений шифруется на клиенте перед отправкой и расшифровывается при
  получении/загрузке истории — chat-service видит только шифротекст.
  Кнопки «Attach»/«Search» отключены в зашифрованном канале (вложения
  и серверный поиск пока не поддерживаются для него — см. раздел
  chat-service ниже). Известные ограничения этой фазы: нет
  forward secrecy (компрометация ключа канала = компрометация всей его
  истории), нет мультиустройственности, нет автоматического
  довыпуска ключа новому участнику при вступлении.
- Личные сообщения и друзья (issue #187). **Фаза 1** — заявки в друзья:
  backend в `user-service` (`POST /friends/requests`,
  `GET /friends/requests`, accept/decline, `GET /friends`,
  `DELETE /friends/{login}` — см. раздел user-service ниже), без UI в
  DeviceHub пока. Взаимные заявки авто-принимаются в дружбу. **Фаза 2**
  — личные диалоги: backend в `chat-service` (`POST /dm/threads`,
  `GET /dm/threads`, `POST`/`GET /dm/threads/{id}/messages` — см. раздел
  chat-service ниже), тоже без UI. Открыть новый диалог можно только с
  другом — chat-service проверяет это через внутренний вызов на
  user-service (`GET /internal/friendship`, см.
  `services/chat-service/docs/diagrams/dm-open-thread-sequence.puml`);
  уже открытый диалог продолжает работать, даже если дружба потом
  разорвётся. Пока только REST — живой доставки через WebSocket и
  экрана переписки в клиенте (Фаза 3) нет.

## Сборка

Зависимости: CMake ≥ 3.21, компилятор с поддержкой C++20. Qt6 и
GoogleTest ставятся не через системный пакетный менеджер, а через
[vcpkg](https://vcpkg.io) (см. CLAUDE.md, «Кроссплатформенность») —
он подключён как git submodule, зависимости описаны в
[vcpkg.json](vcpkg.json).

Клонировать с submodule (или инициализировать в уже склонированном
репозитории):

```bash
git clone --recurse-submodules <url>
# или, если уже склонировано без --recurse-submodules:
git submodule update --init --recursive
```

Сборка (vcpkg сам соберёт Qt6 из исходников при первой конфигурации —
это медленно, от часа и дольше, и требует нескольких гигабайт диска;
повторные сборки используют кэш и быстрые):

```bash
cmake -S . -B build
cmake --build build --parallel
```

Путь к toolchain-файлу vcpkg прописан в `CMakeLists.txt` относительно
корня репозитория — передавать `-DCMAKE_TOOLCHAIN_FILE` вручную не
нужно, если submodule на месте.

Запуск:

```bash
open build/DeviceHub.app   # macOS
```

### macOS: разрешения камеры/микрофона переживают пересборку

По умолчанию `DeviceHub.app` подписывается ad-hoc (`codesign` без
identity) — подпись пересчитывается из содержимого бинарника при
каждой пересборке. macOS привязывает выданные разрешения Camera/
Microphone именно к подписи, так что после пересборки уже выданное
разрешение перестаёт действовать (хотя в System Settings может всё ещё
показываться как включённое).

Чтобы разрешения сохранялись между пересборками — один раз на машине
создать локальный сертификат для подписи кода:

1. **Keychain Access → Certificate Assistant → Create a Certificate…**
2. Имя, например, `DeviceHub Local Dev`; Identity Type — **Self Signed
   Root**; Certificate Type — **Code Signing**.
3. После создания — двойной клик по сертификату, раздел **Trust**,
   пункт **Code Signing** → **Always Trust** (самоподписанный
   сертификат по умолчанию не доверенный, без этого шага `codesign` не
   увидит его как рабочую identity — `security find-identity -v -p
   codesigning` покажет 0).

Затем один раз указать этот сертификат при конфигурации:

```bash
cmake -S . -B build -DDEVICEHUB_CODESIGN_IDENTITY="DeviceHub Local Dev"
cmake --build build --parallel
```

CMake запомнит это в кэше `build/`, повторять при каждой сборке не
нужно. Без `-DDEVICEHUB_CODESIGN_IDENTITY` сборка остаётся ad-hoc, как
раньше — специально не захардкожено машинно-специфичное имя
сертификата в `CMakeLists.txt` (см. CLAUDE.md, «Кроссплатформенность»).

## Тесты

```bash
ctest --test-dir build --output-on-failure
```

Тот же набор тестов гоняется в CI при каждом push и PR
([.github/workflows/ci.yml](.github/workflows/ci.yml)). Помимо
юнит-тестов на `devices/`, есть UI-тесты на `MainWindow` (построение
окна, заполнение списков устройств) — они не запускают реальный захват
(микрофон/камера/экран), т.к. голый тестовый бинарник не имеет
`Info.plist` с разрешениями macOS.

## Сервисы

Четыре независимо собираемых компонента: DeviceHub (этот репозиторий,
корень), `services/auth-service`, `services/user-service` и
`services/chat-service` — у каждого свой `vcpkg.json` и своя сборка.

### user-service

Владеет Postgres (пользователи: логин + Argon2id-хеш пароля, пароли в
открытом виде не хранятся и не логируются). `docker compose up -d`
поднимает Postgres для user-service (порт 5433) и chat-service (порт
5434) разом — у каждого сервиса своя база (database-per-service), и
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
поля. Ещё один эндпоинт — `POST /users/resolve-otp-identifier` (issue
#156/#174, без авторизации, вызывается самим auth-service) — принимает
`{"identifier"}` (login, email или Telegram chat_id) и отвечает
`{"found", "login", "email", "telegram_chat_id"}`, используется для
входа по одноразовому коду.

Заявки в друзья (issue #187, Фаза 1 — backend, без UI; сама фича
описана в разделе «Возможности» выше) — все требуют `Authorization:
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

`email` (issue #156) и `telegram_chat_id` (issue #174, вход по
одноразовому коду) — оба уникальны, если заданы (частичные уникальные
индексы, допускают несколько `NULL`); попытка поставить уже занятое
другим аккаунтом значение через `PATCH /users/me` отвечает `409`.

`public_key` (issue #136, Phase 1 сквозного шифрования — см. раздел
«Сквозное шифрование» ниже) — base64 публичной половины X25519-пары,
которую клиент публикует сюда; сам сервер её никак не использует,
только хранит и отдаёт для того, чтобы другие клиенты могли
шифровать этому пользователю.

### auth-service

Проверяет логин/пароль через user-service и только потом выдаёт
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

### chat-service

Сообщества (communities) содержат каналы (channels); пользователи
вступают в сообщества; в каналах — сообщения. Владеет своим Postgres
(поднимается тем же `docker compose up -d`, порт 5434). Каждый запрос
требует `Authorization: Bearer <token>`, проверяемый через auth-service
(`POST /auth/verify`) — тот же паттерн, что auth-service ⇄ user-service:

```bash
cmake -S services/chat-service -B services/chat-service/build
cmake --build services/chat-service/build --parallel
./services/chat-service/build/chat-service
```

Два порта: REST на `127.0.0.1:8082` (`CHAT_SERVICE_REST_PORT`) и
WebSocket на `127.0.0.1:8083` (`CHAT_SERVICE_WS_PORT`) —
`CHAT_SERVICE_HOST` меняет адрес для обоих. Ходит в auth-service по
`AUTH_SERVICE_HOST`/`AUTH_SERVICE_PORT` и (issue #187, Фаза 2, только
для `POST /dm/threads` — см. ниже) в user-service по
`USER_SERVICE_HOST`/`USER_SERVICE_PORT`.

REST: `POST /communities`, `GET /communities`,
`PATCH /communities/{id}` (переименование, только владелец),
`DELETE /communities/{id}` (только владелец),
`POST /communities/{id}/join`,
`POST /communities/{id}/moderators` (тело `{"login"}`, назначить
модератора, только владелец, issue #114),
`DELETE /communities/{id}/moderators/{login}` (снять, только владелец,
идемпотентно), `GET /communities/{id}/moderators`,
`POST /communities/{id}/channels`,
`GET /communities/{id}/channels`, `PATCH /channels/{id}`
(переименование), `DELETE /channels/{id}`,
`GET /channels/{id}/messages` (опционально `?limit=N&before_id=M` —
хронологическая страница из N сообщений, заканчивающаяся прямо перед
`M`; без `before_id` — самые последние). Переименовать/удалить канал
может его создатель, владелец родительского сообщества или модератор
этого сообщества (issue #114) — не только тот, кто конкретно создал
канал. Переименовать/удалить сам сообщество может только его владелец
— модераторам это не передаётся, остальным сервер отвечает 403.

Файловые вложения (issue #116, до 5 МБ — `kMaxAttachmentSizeBytes`):
`POST /channels/{id}/attachments` с телом `{"filename", "content_type",
"data_base64"}` — 400 при отсутствующих полях, невалидном base64 или
превышении лимита размера, иначе 201 с
`{"id", "filename", "content_type", "size_bytes"}`; `GET
/attachments/{id}` отдаёт сырые байты вложения (`Content-Disposition:
attachment` с санитизированным именем файла — CR/LF/`"` вырезаются, чтобы
имя файла не могло внедрить лишний HTTP-заголовок) или 404. Вложения
хранятся как base64 TEXT в той же Postgres, а не BYTEA — так libpqxx не
нужно связывать бинарные параметры (`pqxx::blob` — это Large Objects,
другой механизм); `id` вложения передаётся в `POST /channels/{id}/messages`
через WebSocket (см. ниже) как `attachment_id` — REST только загружает
байты, отправку самого сообщения делает WebSocket-клиент.

`GET /channels/{id}/messages/search?q=<текст>&limit=N` (issue #118) —
поиск по тексту сообщений канала, регистронезависимая проверка
подстроки (`position(lower(q) in lower(body)) > 0`, не полнотекстовый
поиск с ранжированием/стеммингом — осознанное упрощение первой
версии), самые новые совпадения первыми, `limit` по умолчанию 20;
пустой или отсутствующий `q` — 400; для зашифрованного канала (см. ниже)
— тоже 400, серверу нечего искать в шифротексте.

Сквозное шифрование, Phase 2 (issue #138) — `POST
/communities/{id}/channels` принимает необязательное `is_encrypted`
(по умолчанию `false`), задаётся только при создании, не меняется
потом. `GET /communities/{id}/members` — логины всех, кто состоит в
сообществе (нужно клиенту, чтобы знать, для кого заворачивать ключ
канала). `PUT /channels/{id}/keys/{login}` (тело `{"wrapped_key"}`) —
владелец канала/сообщества или модератор сохраняет чью-то обёрнутую
копию симметричного ключа канала (сервер никогда не видит сам ключ,
только уже завёрнутую клиентом копию — libsodium `crypto_box_seal`
публичным ключом получателя); `GET /channels/{id}/keys/me` отдаёт
собственную обёрнутую копию звонящего (404, если для него ещё никто
не завернул ключ — например, он вступил уже после создания канала).
Тело сообщений в зашифрованном канале — то же поле `body`, что и
всегда, просто шифротекст, который сервер хранит/ретранслирует как
есть, не пытаясь его понять. Загрузка вложений (`POST
/channels/{id}/attachments`) в зашифрованный канал пока отклоняется
(400) — вложения ещё не шифруются на клиенте в этой фазе.

WebSocket (`ws://host:8083`): первый фрейм от клиента —
`{"token", "channel_id"}`, дальше — `{"body"}` (опционально с
`"attachment_id"`, если сообщение ссылается на уже загруженное через
REST вложение); сервер рассылает новое сообщение всем, кто подписан на
тот же канал (включая отправителя), с `"attachment_id"`/
`"attachment_filename"` в каждой рассылке (`null`, если вложения нет).
REST остаётся источником истории (и теперь поиска), WebSocket — только
доставка в реальном времени, пока клиент подключён. `{"typing": true}` (issue #96) —
эфемерный индикатор набора текста, рассылается остальным подписчикам
канала как `{"user_typing": "<login>"}` (никогда не возвращается
отправителю); ничего не персистится, отдельного «перестал печатать» нет
— клиент сам скрывает индикатор через несколько секунд без нового
события. `{"edit_message": {"id", "body"}}` (issue #107) — редактировать
может только автор сообщения, даже после появления модерации (issue
#114) — модератор не должен подменять чужие слова, только удалять их.
`{"delete_message": {"id"}}` — шире, чем edit: удалить может автор,
владелец канала/сообщества или модератор сообщества. Успех рассылается
всем подписчикам как `{"message_edited": {"id", "body", "edited_at"}}`/
`{"message_deleted": {"id"}}`; 404/403 уходят только инициатору.

По тому же WebSocket — сигналинг для групповых голосовых звонков
(issue #46, mesh-топология, ещё без аудио/UI — только ретрансляция):
`{"call_join": true}` (в ответ `{"call_roster": [...]}` — кто уже в
звонке этого канала, плюс остальным участникам `{"call_peer_joined"}`),
`{"call_leave": true}` (остальным — `{"call_peer_left"}`; то же самое
происходит при обрыве соединения без явного leave),
`{"call_signal": {"to", "payload"}}` — непрозрачные данные (SDP/ICE)
ретранслируются адресату как `{"call_signal": {"from", "payload"}}`;
сервер их не разбирает. Участники звонка (`call_join`/`call_leave`) —
отдельный от подписки на текстовый канал список: можно быть в чате, не
будучи в звонке, и наоборот.

Личные диалоги (issue #187, Фаза 2) — переиспользуют message-модель, но
без community/channel: `POST /dm/threads` (тело `{"recipient_login"}`)
открывает диалог с другом или возвращает уже существующий (идемпотентно
— один и тот же id при повторном вызове в любом направлении), 400 при
попытке написать самому себе, 403, если получатель не друг (дружба
проверяется через `GET /internal/friendship` на user-service, см.
`UserServiceClient`, — уже открытый диалог продолжает работать, даже
если дружба потом разорвётся). `GET /dm/threads` — мои диалоги
(`{"id", "other_login", "created_at"}`). `POST
/dm/threads/{id}/messages` (тело `{"body"}`) и `GET
/dm/threads/{id}/messages` (та же пагинация `?limit=&before_id=`, что и
у `/channels/{id}/messages`) — оба отвечают 404 не-участнику диалога, а
не 403, чтобы не подтверждать само существование чужого диалога. Пока
без вложений/редактирования/удаления и без живой доставки через
WebSocket — только REST (Фаза 2b добавит доставку в реальном времени
отдельной задачей); UI в DeviceHub — Фаза 3.

### DeviceHub ↔ сервисы

Меню аккаунта (кнопка Account в правом верхнем углу) запрашивает
токен по введённым логину/паролю (адрес auth-service —
`AUTH_SERVICE_URL`, по умолчанию `http://127.0.0.1:8080`) и сразу
проверяет его тем же сервисом; кнопка «Register» тем же способом
регистрирует новый аккаунт через `POST /auth/register` и сразу
входит под ним — без токена сайдбар сообществ/каналов не работает.
Полученный при этом refresh-токен (issue #105) `MainWindow` использует
сама, без участия пользователя: за минуту до истечения текущего
access-токена автоматически шлётся `POST /auth/refresh`, так что
сессия не обрывается посреди работы — повторно вводить логин/пароль
не нужно, пока refresh-токен ещё не истёк (по умолчанию 30 дней).

Сайдбар использует токен, полученный в меню аккаунта, чтобы через REST
chat-service (`CHAT_SERVICE_URL`, по умолчанию `http://127.0.0.1:8082`):
создать/переименовать/удалить сообщество или канал (переименование и
удаление доступны только владельцу — остальным сервер отвечает 403),
вступить в сообщество, обновить списки. Клик по каналу подключается к
нему по WebSocket (`CHAT_SERVICE_WS_URL`, по умолчанию
`ws://127.0.0.1:8083`) и открывает чат в основном пространстве окна —
тем же REST-клиентом сразу подгружается и последняя страница истории
сообщений (issue #100), WebSocket отвечает только за живую доставку
новых. Кнопка «Load older messages» над списком (видна, если последняя
подгруженная страница пришла полной — вероятно, есть ещё) запрашивает
следующую страницу перед самым старым уже показанным сообщением и
вставляет её сверху, не сбивая прокрутку — если только пользователь
не был прижат к низу списка, автоскролл при новых сообщениях остаётся
как раньше.
Сообщения показываются не плоским логом, а bubble-стилем (как в
iMessage/Slack): свои сообщения — зелёным градиентом справа, чужие —
слева нейтральным цветом с аватаром (инициал автора на зелёном фоне)
и именем; подряд идущие сообщения одного автора в пределах нескольких
минут группируются — аватар/имя/время показываются один раз на
группу. Размеры (аватар, отступы, радиус) считаются от текущего
размера шрифта, а не заданы фиксированными пикселями; максимальная
ширина bubble — процент от ширины чата.

Кнопка «Attach» рядом с полем ввода (issue #116) открывает системный
диалог выбора файла, проверяет размер против того же лимита в 5 МБ, что
и сервер (чтобы не тратить round-trip на заведомо отклонённую загрузку),
и загружает файл через REST (`ChatRestClient::uploadAttachment`) —
получив id вложения, клиент сразу отправляет сообщение с этим id через
WebSocket (текст поля ввода на момент загрузки, может быть пустым) —
намеренное упрощение «загрузить и сразу отправить» вместо
двухшагового «прикрепить, затем нажать Send». У любого сообщения с
вложением (не только своего) появляется кнопка «Download: <имя файла>»
— клик скачивает байты через REST и предлагает сохранить их через
диалог сохранения файла.

Кнопка «Search» в шапке канала (issue #118) открывает отдельное окно
(`SearchDialog`) с полем запроса и списком совпадений (автор, текст,
время); Enter или кнопка «Search» внутри диалога шлют запрос через
`ChatRestClient::searchMessages()`, результат приходит в
`messagesFound()`. Двойной клик/Enter на результате пытается
прокрутить список сообщений открытого канала к найденному —
`ChatView::scrollToMessage()` ищет сообщение среди уже подгруженных
строк и возвращает false, если его там нет (например, оно дальше в
истории, чем зашла последняя подгрузка через «Load older messages»);
в этом случае показывается тост с подсказкой подгрузить историю
глубже — автоматической подгрузки недостающих страниц ради перехода
к результату в первой версии нет. При переключении канала список
результатов и поле запроса очищаются, чтобы не показывать находки из
другого канала.

Для групповых голосовых звонков (issue #46, Phase 2) в
`src/devices/CallAudioDeviceModule` реализован мост между аудио-пайплайном
libwebrtc и Qt Multimedia: `webrtc::AudioDeviceModule`, у которого захват —
push (внешний код зовёт `pushCapturedAudio()`, когда есть новый буфер
с микрофона, дальше — в `AudioTransport::RecordedDataIsAvailable`), а
воспроизведение — pull, но таймингом рулит сам модуль: отдельный поток
каждые ~10 мс зовёт `AudioTransport::NeedMorePlayData()` и отдаёт PCM
через callback (не в Qt GUI-потоке — маршалинг туда, если он нужен,
на совести вызывающего). `setPlayoutFormat()` (issue #70) переопределяет
частоту/число каналов, которые модуль запрашивает у WebRTC на
воспроизведении (по умолчанию 48kHz mono) — под реальное устройство
вывода, которое подключает `CallManager`. Кросс-поточная передача
аудио здесь всё ещё не тюнингована под живую нагрузку (без джиттер-
буфера сверх того, что уже даёт WebRTC), это осознанный риск первой
версии.

Оркестрация самого mesh-звонка (issue #46, Phase 3) — в
`src/chat/CallManager`: одна `webrtc::PeerConnectionFactoryInterface` (с
`CallAudioDeviceModule` в роли ADM) и по одному `webrtc::PeerConnection`
на каждого удалённого участника, без SFU. Подписан на call-сигналинг
`ChatClient` (`callRosterReceived`/`callPeerJoined`/`callPeerLeft`/
`callSignalReceived`) и правило инициации mesh соблюдает буквально:
только что присоединившийся участник сам предлагает (`offer`) каждому,
кто уже в ростере; существующие участники только отвечают (`answer`) —
так в одной паре не бывает двух одновременных offer. Callback'и
`PeerConnectionObserver`/`SetLocal-`/`SetRemoteDescriptionObserverInterface`
прилетают на собственном потоке WebRTC, а не на потоке Qt GUI — каждый
сразу перескакивает обратно через `QMetaObject::invokeMethod(...,
Qt::QueuedConnection)`, прежде чем трогать состояние `CallManager` или
звать `ChatClient`.

Реальный микрофон/динамики (issue #70) — `joinCall()` подключает
`AudioInputDevice::pcmDataAvailable()` к `pushCapturedAudio()` и
playout-sink `CallAudioDeviceModule` — к `AudioOutputDevice::writeAudio()`
(новый push-режим `QAudioSink`, рядом с уже существующим `playTestTone()`).
Оба Qt-устройства — один владелец одновременно: звонок и кнопки
теста микрофона/тестового тона в Settings делят один и тот же
`QAudioSource`/`QAudioSink`, включение одного молча забирает устройство
у другого — осознанный компромисс первой версии, как и раньше для
камеры/микрофона в Settings. Пустое/невыбранное устройство (`QAudioDevice`
без реального `preferredFormat()`) обрабатывается через `callError()`, а
не прокидывается дальше как есть — собственный `AudioTransportImpl`
WebRTC падает с fatal CHECK на нулевом числе каналов вместо мягкой
ошибки, это реально ловилось на живом тесте при разработке #70:
`nBytesPerSample` в `RecordedDataIsAvailable`/`NeedMorePlayData` — это
байты на весь интерлived-фрейм (`sizeof(int16_t) * nChannels`), а не на
один сэмпл, как было по ошибке в первой версии `CallAudioDeviceModule`.

В шапке открытого канала (issue #46, Phase 4) — кнопка «Call»/«Leave
call», кнопка «Mute»/«Unmute» (активна только во время звонка,
переключает реальный per-track mute через
`webrtc::AudioTrackInterface::set_enabled`, а не заглушку в UI) и метка
со списком участников звонка. `ChatView` сама ничего не знает про
`CallManager` — как и остальные панели, только шлёт UI-сигналы
(`callToggleRequested`/`muteToggleRequested`); `MainWindow` решает
join/leave по `CallManager::inCall()` и обновляет `ChatView` обратно
через `setCallState()`/`setCallParticipants()`. Переключение канала или
закрытие чата сначала выходит из активного звонка — он привязан к
подписанному каналу, оставлять его подключённым к уже недоступному
каналу не имеет смысла.

Видео с камеры в звонках (issue #72) начинается с моста захвата —
`src/devices/CallVideoTrackSource` — видео-аналога
`CallAudioDeviceModule` (#64): у Qt Multimedia нет готового способа
отдавать кадры в видео-пайплайн libwebrtc, только через свой
`webrtc::VideoTrackSourceInterface` (точнее, его удобную базу
`webrtc::AdaptedVideoTrackSource`, которая уже берёт на себя управление
подписчиками и адаптацию разрешения). `pushFrame()` конвертирует каждый
`QVideoFrame` в I420 через `QImage` как промежуточный шаг
(`QVideoFrame::toImage()`, затем `libyuv::ARGBToI420()`) — проще и
надёжнее, чем вручную разбирать все возможные форматы пикселей камеры,
ценой лишнего прохода конвертации. `CameraDevice` при этом пришлось
слегка перестроить: `QMediaCaptureSession` поддерживает только один
video sink одновременно, поэтому класс теперь сам владеет этим единственным
sink'ом и рассылает кадры через новый сигнал `frameAvailable()`, а не
отдаёт `captureSession()` под `setVideoOutput()` виджета напрямую —
превью в Settings и (в следующих задачах) исходящее видео звонка делят
один и тот же захват с камеры, тот же паттерн единственного владельца,
что уже был у `AudioInputDevice`/`AudioOutputDevice`.

Отдельно всплыл линковочный нюанс: сборка libwebrtc сама собирается без
RTTI, поэтому у экспортируемых классов вроде `AdaptedVideoTrackSource`
есть vtable, но нет typeinfo в `libwebrtc.a` — обычный, RTTI-включённый
код, определяющий виртуальные методы производного класса (или
инстанцирующий `webrtc::RefCountedObject<T>` через `make_ref_counted<T>()`),
должен сгенерировать typeinfo, ссылающийся на базовый — которого нет, и
линковка падает. В проекте нигде не используется `dynamic_cast`/`typeid`,
поэтому RTTI отключён (`-fno-rtti`/`/GR-`) для всего проекта разом в
корневом `CMakeLists.txt`, а не точечно для каждого нового файла,
который будет наследоваться от классов libwebrtc.

Подключение видео к mesh-звонку — в `CallManager`: `enableVideo()`
создаёт общий видео-трек один раз (при первом вызове) и добавляет его
в каждый уже существующий `PeerConnection`, вызывая `negotiateLocal()`
явно для каждого — тем же способом, каким `ensurePeerConnection()`/
`onCallRosterReceived()` уже договариваются об исходном аудио-треке, а
не реагируя на собственное уведомление WebRTC `OnRenegotiationNeeded()`
(он специально оставлен no-op'ом). `PeerConnection`, созданный уже
после включения видео, получает трек как часть своего собственного
исходного offer/answer (`attachVideoTrack()`, вызывается и из
`ensurePeerConnection()` тоже). После первого добавления трек больше
никогда не удаляется — `enableVideo()`/`disableVideo()` только
переключают `webrtc::VideoTrackInterface::set_enabled()`, тем же
способом, каким `setMuted()` уже работает для аудио.

За этим решением — два реальных бага, пойманных на живом тесте
(`CallManagerIntegrationTest`, теперь дополнен сценарием включения
видео посреди звонка):

- Опора одновременно на явный вызов `negotiateLocal()` и на
  `OnRenegotiationNeeded()` для ОДНОГО и того же изменения треков
  гонялась сама с собой: отложенный (через `QMetaObject::invokeMethod`)
  колбэк `OnRenegotiationNeeded` срабатывал уже ПОСЛЕ того, как явный
  вызов успел договориться заново, и накладывал сверху второй,
  ненужный offer — соединение получало сообщение не в том состоянии
  (`Called in wrong state: stable`). Решение — оставить только явный
  путь, `OnRenegotiationNeeded()` не переопределён вовсе.
- `RemoveTrackOrError()` (удаление видео-трека при `disableVideo()`)
  падало с фатальным assert внутри самого WebRTC, в обработке списка
  кодеков (`media/base/codec_list.cc`, `Check failed: present_codec ==
  codec`) — не баг в этом проекте, а вылезло только при повторной
  renegotiation после удаления трека. Обойдено тем, что трек больше не
  удаляется вообще, только `set_enabled(false)`.

UI (issue #91) добавляет кнопку «Enable Video»/«Disable Video» в шапку
канала (рядом с Call/Mute, тот же паттерн — обычная `QPushButton`,
активна только во время звонка) и полоску видео над списком сообщений:
локальное превью (`QVideoWidget`, тот же fan-out кадров камеры, что уже
используется для превью в Settings — `CameraDevice::frameAvailable`
рассчитан на нескольких подписчиков сразу) и по одному тайлу
(`QLabel` с `QPixmap`) на каждого участника, чьё видео пришло. Полоска
скрыта, когда показывать нечего, и полностью сбрасывается при выходе из
звонка (`ChatView::setCallState(false, ...)`), а не отдельным вызовом на
каждом месте выхода — так каждый из уже существующих путей завершения
звонка получает сброс бесплатно.

Приём удалённого видео потребовал нового бэкенд-пути, которого не было
даже после #72/#74: `CallManager::PeerObserver` не переопределял
`OnTrack()`, так что входящие видео-треки участников нигде не
подхватывались. `handleRemoteTrack()` (вызывается из `OnTrack()` тем же
способом, что и остальные колбэки `PeerConnectionObserver` — hop на GUI-
поток через `QMetaObject::invokeMethod` до касания состояния) заводит
для видео-трека приёмный `RemoteVideoSink` (реализация
`webrtc::VideoSinkInterface<webrtc::VideoFrame>`, хранится в
`PeerConnectionEntry` — по одному на пира, как и `videoSender` для
исходящей стороны) и подписывает его через `AddOrUpdateSink()`.
`RemoteVideoSink::OnFrame()` — зеркало `CallVideoTrackSource::pushFrame()`:
там `QImage` → I420 через `libyuv::ARGBToI420`, здесь I420 → `QImage`
через `libyuv::I420ToARGB`. Fires тоже не на GUI-потоке (на потоке
декодирования WebRTC), так что перед `emit remoteVideoFrameReceived()`
тот же hop через `invokeMethod`, что и везде в этом классе.

Демонстрация экрана в звонках (issue #112) переиспользует ровно тот же
общий видео-трек, что и камера, а не заводит второй: `enableVideo()`/
`enableScreenShare()` взаимоисключающие (включение одного сначала
выключает другое) и просто переключают, откуда `CallVideoTrackSource`
берёт кадры (`camera_` vs `screenCapture_`), заодно выставляя
`is_screencast()` через новый `CallVideoTrackSource::setIsScreencast()`
(поле — `std::atomic<bool>`, пишется из GUI-потока, читается WebRTC
откуда угодно). Проще второго трека (не нужна повторная
renegotiation/attach на каждого пира при переключении), ценой того,
что одновременно показать и камеру, и экран нельзя — осознанное
упрощение первой версии, не ограничение самого WebRTC. `ScreenCaptureDevice`
получил тот же рефакторинг, что раньше `CameraDevice` (issue #72):
теперь сам владеет единственным `QMediaCaptureSession`-слотом и
рассылает кадры через `frameAvailable()`, а не отдаёт `captureSession()`
виджету напрямую — иначе превью в Settings и исходящее видео звонка не
смогли бы делить один и тот же захват экрана. Кнопка «Share Screen» —
тот же UI-паттерн, что и «Enable Video», рядом в шапке канала.

Реальные end-to-end тесты требуют запущенных сервисов и Postgres и
пропускают себя (`GTEST_SKIP`), если те недоступны — CI пока не
поднимает весь стек одновременно, это ручная/локальная проверка:

- `AuthClientIntegrationTest` (DeviceHub) — регистрирует пользователя в
  user-service, получает и проверяет токен через auth-service.
- `ChatClientIntegrationTest` (DeviceHub) — то же плюс создаёт
  сообщество/канал и реальный round-trip через WebSocket chat-service.
- `CallManagerIntegrationTest` (DeviceHub) — два пользователя, один канал:
  оба присоединяются к звонку, второй по правилу инициации mesh шлёт offer
  первому через живой chat-service, проверяется реальный SDP offer/answer
  round-trip (без `callError`) и что leave/join долетают друг до друга;
  полную ICE/DTLS-связность намеренно не ждёт (не привязываться к
  доступности STUN в конкретном окружении).
- `UserServiceClientIntegrationTest` (auth-service) — то же самое, но
  напрямую от лица auth-service.
- `UserServiceIntegrationTest` (user-service) — регистрация и проверка
  учётных данных напрямую в Postgres.
- `ChatServiceIntegrationTest` (chat-service) — сообщества/каналы/
  сообщения напрямую в Postgres, включая личные диалоги (issue #187,
  Фаза 2) — не требует auth-service/user-service, вызывает ChatService
  напрямую.
- `UserServiceClientTest` (chat-service, issue #187, Фаза 2) —
  `UserServiceClient::areFriends()` против живого user-service (плюс
  auth-service для выпуска токенов, чтобы реально подружить два
  аккаунта через `POST /friends/requests`); недоступный-сервис случай
  выполняется всегда, без пропуска.

## Диаграммы

PlantUML-диаграммы классов и sequence-диаграммы для нетривиальных
потоков — исходники `.puml` в [docs/diagrams/](docs/diagrams/), рядом
с ними PNG-рендер того же имени; PNG перерендеривается в том же
коммите, что и любая правка `.puml` (`java -jar plantuml.jar -tpng
docs/diagrams/*.puml`).

- [devices.puml](docs/diagrams/devices.puml) — `src/devices`
  ([devices.png](docs/diagrams/devices.png))
- [auth.puml](docs/diagrams/auth.puml) — `src/auth`
  ([auth.png](docs/diagrams/auth.png))
- [chat.puml](docs/diagrams/chat.puml) — `src/chat`, включая
  `CallManager` и mesh-звонки
  ([chat.png](docs/diagrams/chat.png))
- [ui.puml](docs/diagrams/ui.puml) — `src/ui`
  ([ui.png](docs/diagrams/ui.png))
- [user.puml](docs/diagrams/user.puml) — `src/user` (issue #110,
  `IdentityKeyStore` добавлен issue #136)
  ([user.png](docs/diagrams/user.png))
- [mic-capture-sequence.puml](docs/diagrams/mic-capture-sequence.puml) —
  запуск захвата с микрофона
  ([mic-capture-sequence.png](docs/diagrams/mic-capture-sequence.png))
- [call-video-receive-sequence.puml](docs/diagrams/call-video-receive-sequence.puml) —
  приём видео от участника звонка (issue #91)
  ([call-video-receive-sequence.png](docs/diagrams/call-video-receive-sequence.png))

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
