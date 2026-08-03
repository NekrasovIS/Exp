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
([.github/workflows/ci.yml](.github/workflows/ci.yml)).

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
