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
`PATCH /communities/{id}` (переименование), `DELETE /communities/{id}`,
`POST /communities/{id}/join`, `POST /communities/{id}/channels`,
`GET /communities/{id}/channels`, `PATCH /channels/{id}`
(переименование), `DELETE /channels/{id}`,
`GET /channels/{id}/messages`. Переименовать/удалить сообщество или
канал может только его создатель (`owner_login`) — остальным сервер
отвечает 403.

WebSocket (`ws://host:8083`): первый фрейм от клиента —
`{"token", "channel_id"}`, дальше — `{"body"}`; сервер рассылает новое
сообщение всем, кто подписан на тот же канал (включая отправителя).
REST остаётся источником истории, WebSocket — только доставка в
реальном времени, пока клиент подключён.

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

### DeviceHub ↔ сервисы

Меню аккаунта (кнопка Account в правом верхнем углу) запрашивает
токен по введённым логину/паролю (адрес auth-service —
`AUTH_SERVICE_URL`, по умолчанию `http://127.0.0.1:8080`) и сразу
проверяет его тем же сервисом; кнопка «Register» тем же способом
регистрирует новый аккаунт через `POST /auth/register` и сразу
входит под ним — без токена сайдбар сообществ/каналов не работает.

Сайдбар использует токен, полученный в меню аккаунта, чтобы через REST
chat-service (`CHAT_SERVICE_URL`, по умолчанию `http://127.0.0.1:8082`):
создать/переименовать/удалить сообщество или канал (переименование и
удаление доступны только владельцу — остальным сервер отвечает 403),
вступить в сообщество, обновить списки. Клик по каналу подключается к
нему по WebSocket (`CHAT_SERVICE_WS_URL`, по умолчанию
`ws://127.0.0.1:8083`) и открывает чат в основном пространстве окна.
Сообщения показываются не плоским логом, а bubble-стилем (как в
iMessage/Slack): свои сообщения — зелёным градиентом справа, чужие —
слева нейтральным цветом с аватаром (инициал автора на зелёном фоне)
и именем; подряд идущие сообщения одного автора в пределах нескольких
минут группируются — аватар/имя/время показываются один раз на
группу. Размеры (аватар, отступы, радиус) считаются от текущего
размера шрифта, а не заданы фиксированными пикселями; максимальная
ширина bubble — процент от ширины чата.

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

Только мост захвата — подключение к mesh-звонку через `CallManager`
(добавление видео-трека, renegotiation SDP при включении/выключении) и
UI (кнопка камеры, отображение видео) — в следующих задачах, по той же
схеме поэтапной разработки, что и голосовые звонки.

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
  сообщения напрямую в Postgres.

## Правила разработки

Стиль кода, структура проекта, конвенции по задачам/веткам/коммитам,
диаграммы и требования к тестированию — в [CLAUDE.md](CLAUDE.md).
