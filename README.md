# DeviceHub

[![CI](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml/badge.svg)](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml)

Десктопное Qt6-приложение для работы с устройствами ОС: аудио-выход,
микрофон, камера. Начиналось как демонстрация доступа к железу через
Qt Multimedia; развивается в сторону клиент-серверного инструмента с
подключением по web-ссылке (см. [CLAUDE.md](CLAUDE.md)). Раскладка
UI — сайдбар слева (сообщества сверху, чат снизу), кнопка аккаунта в
правом верхнем углу, футер с профилем и настройками устройств
(Audio Output, Microphone, Camera, Screen Capture) слева. Тёмная тема
в стиле Discord (`src/ui/Theme.h`).

## Возможности

- Список доступных устройств вывода звука и проигрывание тестового
  синус-тона на выбранном устройстве.
- Список микрофонов и захват с индикатором уровня входного сигнала.
- Список камер и превью выбранной камеры.
- Список экранов и захват содержимого выбранного экрана с превью.
- Вход по логину/паролю: получение и проверка токена авторизации через
  отдельные сервисы `auth-service` + `user-service` (Postgres).
- Чаты и сообщества: подключение к каналу по ID и обмен сообщениями в
  реальном времени через `chat-service` (Postgres, WebSocket).

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
`POST /users/register`, `POST /users/verify-credentials`.

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
request).

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
`AUTH_SERVICE_HOST`/`AUTH_SERVICE_PORT`.

REST: `POST /communities`, `GET /communities`,
`POST /communities/{id}/join`, `POST /communities/{id}/channels`,
`GET /communities/{id}/channels`, `GET /channels/{id}/messages`.

WebSocket (`ws://host:8083`): первый фрейм от клиента —
`{"token", "channel_id"}`, дальше — `{"body"}`; сервер рассылает новое
сообщение всем, кто подписан на тот же канал (включая отправителя).
REST остаётся источником истории, WebSocket — только доставка в
реальном времени, пока клиент подключён.

### DeviceHub ↔ сервисы

Меню аккаунта (кнопка Account в правом верхнем углу) запрашивает
токен по введённым логину/паролю (адрес auth-service —
`AUTH_SERVICE_URL`, по умолчанию `http://127.0.0.1:8080`) и сразу
проверяет его тем же сервисом; кнопка «Register» тем же способом
регистрирует новый аккаунт через `POST /auth/register` и сразу
входит под ним — без токена «Chat» и «Communities» не работают.

Сайдбар «Chat» использует токен, полученный в меню аккаунта, чтобы
через REST chat-service (`CHAT_SERVICE_URL`, по умолчанию
`http://127.0.0.1:8082`): создать сообщество/вступить, создать
канал, обновить списки сообществ/каналов (выпадающие списки) — и
подключиться к выбранному каналу по WebSocket
(`CHAT_SERVICE_WS_URL`, по умолчанию `ws://127.0.0.1:8083`) для
реального времени.

Реальные end-to-end тесты требуют запущенных сервисов и Postgres и
пропускают себя (`GTEST_SKIP`), если те недоступны — CI пока не
поднимает весь стек одновременно, это ручная/локальная проверка:

- `AuthClientIntegrationTest` (DeviceHub) — регистрирует пользователя в
  user-service, получает и проверяет токен через auth-service.
- `ChatClientIntegrationTest` (DeviceHub) — то же плюс создаёт
  сообщество/канал и реальный round-trip через WebSocket chat-service.
- `UserServiceClientIntegrationTest` (auth-service) — то же самое, но
  напрямую от лица auth-service.
- `UserServiceIntegrationTest` (user-service) — регистрация и проверка
  учётных данных напрямую в Postgres.
- `ChatServiceIntegrationTest` (chat-service) — сообщества/каналы/
  сообщения напрямую в Postgres.

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
