# Сборка и тесты

Как собрать и протестировать десктопный клиент DeviceHub (этот
репозиторий, корень) — см. [README](../README.md) за общим описанием
проекта. Backend-сервисы собираются отдельно, каждый в своём
`services/<name>/` — см. [docs/services/](services/).

## Сборка

Зависимости: CMake ≥ 3.21, компилятор с поддержкой C++20. Qt6 и
GoogleTest ставятся не через системный пакетный менеджер, а через
[vcpkg](https://vcpkg.io) (см. [CLAUDE.md](../CLAUDE.md), «Кроссплатформенность») —
он подключён как git submodule, зависимости описаны в
[vcpkg.json](../vcpkg.json).

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
([.github/workflows/ci.yml](../.github/workflows/ci.yml)). Помимо
юнит-тестов на `devices/`, есть UI-тесты на `MainWindow` (построение
окна, заполнение списков устройств) — они не запускают реальный захват
(микрофон/камера/экран), т.к. голый тестовый бинарник не имеет
`Info.plist` с разрешениями macOS.

