# Дашборд CI (Grafana)

Часть служебной инфраструктуры разработки DeviceHub (см.
[README](../README.md) за общим описанием проекта). История прогонов CI ([.github/workflows/ci.yml](../.github/workflows/ci.yml))
— pass/fail по job'ам, число упавших тестов, % покрытия кода
(coverage), прохождение под санитайзерами (ASan+UBSan/TSan) — в виде
дашборда Grafana (issue #193). Раннеры GitHub Actions не имеют сетевого
доступа к локальной машине разработчика, поэтому это pull: локальный
скрипт сам периодически опрашивает GitHub Actions API, а не CI что-то
пишет наружу.

```bash
docker compose --profile ci-metrics up -d
python tools/ci-metrics/collect.py
```

`collect.py` требует аутентифицированный `gh` (`gh auth login`) и
поднятый `ci-metrics-postgres`; опрашивает последние прогоны workflow
`CI`, парсит из логов job'ов ctest-сводку ("XX% tests passed, YY tests
failed out of ZZ") и `llvm-cov report`-строку `TOTAL`, пишет всё в
Postgres (`docker-compose.yml`, порт 5435). Идемпотентен — уже
собранные `run_id` пропускаются, можно перезапускать периодически
(например, из планировщика задач) без дублирования.

Дашборд на http://localhost:3000 (логин `admin`, пароль
`GF_SECURITY_ADMIN_PASSWORD`/`dev-only-password` по умолчанию — тот же
принцип, что и у паролей Postgres в `docker-compose.yml`, см.
CLAUDE.md, "Безопасность") — провижионится сам при первом старте
Grafana из `tools/ci-metrics/grafana/`, ничего вручную создавать не
нужно.

