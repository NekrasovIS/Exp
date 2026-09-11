# Проверка forwarding через Janus videoroom (issue #230/#233)

Скриптовая (не GTest, т.к. проверяет реальную docker-инфраструктуру, а не
код C++-сервисов) проверка того, что SFU реально пересылает медиа между
тестовыми участниками через развёрнутый Janus — двумя (шаги 1-3) и тремя
(шаги 4-5, issue #233 — предварительное условие для удаления mesh-кода:
подтверждённый работающий SFU-звонок минимум с 3 участниками, а не
"теоретическая замена"; в частности, что новый участник, присоединяясь
третьим, видит через Janus обоих уже существующих publisher'ов, а не
только первого).

## Запуск

Нужен контейнер в той же docker-сети, что и `janus` (сеть по умолчанию у
`docker compose` — `<имя_каталога_репозитория>_default`, например
`exp_default`) — при запуске с хоста ICE зависает в `connecting` из-за NAT
hairpin (см. комментарий в начале `verify-forwarding.mjs`).

```sh
docker compose --profile sfu up -d janus
docker run --rm -it \
  --network exp_default \
  -e JANUS_HOST=janus \
  -v "$(pwd)/services/janus/verify:/work" \
  -w /work \
  node:24 sh -c "npm install && node verify-forwarding.mjs"
```

Успешный прогон заканчивается строкой
`=== ВЕРДИКТ: Janus videoroom SFU реально форвардит между тремя тестовыми участниками ===`
и кодом возврата 0. Комната создаётся заново на каждый прогон (ad-hoc
`request:"create"`, не статическая `devicehub-test` из
`janus.plugin.videoroom.jcfg`) — прогоны не накапливают состояние друг
после друга.
