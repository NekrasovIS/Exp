-- Схема БД метрик CI (issue #193). Наполняется исключительно
-- tools/ci-metrics/collect.py; сам CI (.github/workflows/ci.yml) сюда
-- ничего не пишет напрямую — раннеры GitHub Actions не имеют сетевого
-- доступа к этой локальной машине.

CREATE TABLE IF NOT EXISTS ci_runs (
    run_id BIGINT PRIMARY KEY,
    workflow TEXT NOT NULL,
    branch TEXT NOT NULL,
    commit_sha TEXT NOT NULL,
    event TEXT NOT NULL,
    conclusion TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL,
    html_url TEXT NOT NULL
);

-- Один job из матрицы CI (build-and-test, services/<service>,
-- sanitizers/<service,sanitizer>, fuzz/<service>) на один прогон.
-- service/sanitizer NULL там, где job не относится к конкретному
-- сервису/санитайзеру (например, build-and-test).
CREATE TABLE IF NOT EXISTS ci_job_results (
    id BIGSERIAL PRIMARY KEY,
    run_id BIGINT NOT NULL REFERENCES ci_runs(run_id) ON DELETE CASCADE,
    job_name TEXT NOT NULL,
    kind TEXT NOT NULL,
    service TEXT,
    sanitizer TEXT,
    conclusion TEXT NOT NULL,
    started_at TIMESTAMPTZ,
    completed_at TIMESTAMPTZ,
    duration_seconds INTEGER,
    tests_total INTEGER,
    tests_failed INTEGER,
    UNIQUE (run_id, job_name)
);

-- % покрытия из вывода `llvm-cov report` (coverage job, per-service) —
-- см. doc-комментарий у tools/ci-metrics/collect.py::parse_coverage().
CREATE TABLE IF NOT EXISTS ci_coverage (
    id BIGSERIAL PRIMARY KEY,
    run_id BIGINT NOT NULL REFERENCES ci_runs(run_id) ON DELETE CASCADE,
    service TEXT NOT NULL,
    region_coverage_percent NUMERIC(5,2) NOT NULL,
    function_coverage_percent NUMERIC(5,2) NOT NULL,
    line_coverage_percent NUMERIC(5,2) NOT NULL,
    UNIQUE (run_id, service)
);

CREATE INDEX IF NOT EXISTS ci_runs_created_at_idx ON ci_runs (created_at);
CREATE INDEX IF NOT EXISTS ci_job_results_run_id_idx ON ci_job_results (run_id);
CREATE INDEX IF NOT EXISTS ci_coverage_run_id_idx ON ci_coverage (run_id);
