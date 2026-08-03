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
- Запрос и проверка токена авторизации у отдельного сервиса `auth-service`.

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

`services/auth-service` — отдельный микросервис авторизации (свой
`vcpkg.json`, независимая сборка). Выдаёт и проверяет
HMAC-подписанные токены с ограниченным сроком жизни через REST:

```bash
cmake -S services/auth-service -B services/auth-service/build
cmake --build services/auth-service/build --parallel
AUTH_SERVICE_SECRET=dev-only-secret ./services/auth-service/build/auth-service
```

`AUTH_SERVICE_SECRET` обязателен — без него сервис не запустится (см.
CLAUDE.md, «Безопасность»). По умолчанию слушает `127.0.0.1:8080`
(`AUTH_SERVICE_HOST`/`AUTH_SERVICE_PORT` — переопределить).

DeviceHub обращается к нему из секции «Authorization» в UI (адрес —
`AUTH_SERVICE_URL`, по умолчанию `http://127.0.0.1:8080`). Тест
`AuthClientIntegrationTest` делает реальный round-trip, если сервис
запущен, и пропускает себя (`GTEST_SKIP`), если нет — CI пока не
поднимает оба сервиса одновременно, это ручная/локальная проверка.

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
