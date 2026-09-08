# Проверка forwarding через Janus videoroom (issue #230)

Скриптовая (не GTest, т.к. проверяет реальную docker-инфраструктуру, а не
код C++-сервисов) проверка того, что SFU реально пересылает медиа между
двумя тестовыми участниками через развёрнутый Janus.

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
`=== ВЕРДИКТ: Janus videoroom SFU реально форвардит между двумя тестовыми участниками ===`
и кодом возврата 0.
