# DeviceHub

[![CI](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml/badge.svg)](https://github.com/NekrasovIS/Exp/actions/workflows/ci.yml)

Десктопное Qt6-приложение для работы с устройствами ОС: аудио-выход,
микрофон, камера. Начиналось как демонстрация доступа к железу через
Qt Multimedia; развивается в сторону клиент-серверного инструмента с
подключением по web-ссылке (см. [CLAUDE.md](CLAUDE.md)).

## Возможности

- Список доступных устройств вывода звука и проигрывание тестового
  синус-тона на выбранном устройстве.
- Список микрофонов и захват с индикатором уровня входного сигнала.
- Список камер и превью выбранной камеры.
- Список экранов и захват содержимого выбранного экрана с превью.
- Вход по логину/паролю: получение и проверка токена авторизации через
  отдельные сервисы `auth-service` + `user-service` (Postgres).

## Сборка

Зависимости: CMake ≥ 3.21, компилятор с поддержкой C++20, Qt6
(Widgets, Multimedia, MultimediaWidgets), GoogleTest.

На macOS зависимости ставятся через Homebrew:

```bash
brew install qt@6 googletest
```

Сборка:

```bash
cmake -S . -B build
cmake --build build --parallel
```

На macOS путь к Qt6 определяется автоматически через `brew --prefix
qt@6`; на других платформах передайте его явно через
`-DCMAKE_PREFIX_PATH=<путь к Qt6>`.

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

Три независимо собираемых компонента: DeviceHub (этот репозиторий,
корень), `services/auth-service` и `services/user-service` — у каждого
свой `vcpkg.json` и своя сборка.

### user-service

Владеет Postgres (пользователи: логин + Argon2id-хеш пароля, пароли в
открытом виде не хранятся и не логируются). Локальный Postgres — через
Docker:

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
REST: `POST /auth/token` (`{"login", "password"}`), `POST /auth/verify`.

### DeviceHub ↔ сервисы

Секция «Authorization» в UI запрашивает токен по введённым
логину/паролю (адрес auth-service — `AUTH_SERVICE_URL`, по умолчанию
`http://127.0.0.1:8080`) и сразу проверяет его тем же сервисом.

Реальные end-to-end тесты требуют запущенных сервисов и Postgres и
пропускают себя (`GTEST_SKIP`), если те недоступны — CI пока не
поднимает весь стек одновременно, это ручная/локальная проверка:

- `AuthClientIntegrationTest` (DeviceHub) — регистрирует пользователя в
  user-service, получает и проверяет токен через auth-service.
- `UserServiceClientIntegrationTest` (auth-service) — то же самое, но
  напрямую от лица auth-service.
- `UserServiceIntegrationTest` (user-service) — регистрация и проверка
  учётных данных напрямую в Postgres.

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
