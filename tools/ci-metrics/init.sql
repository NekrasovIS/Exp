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

-- Построчная (per-file) детализация того же прогона llvm-cov report,
-- что и агрегат ci_coverage выше, — одна запись на файл, участвовавший
-- в конкретном coverage job'е конкретного сервиса. Позволяет находить
-- наименее покрытые файлы, а не только итоговый % по сервису — см.
-- doc-комментарий tools/ci-metrics/collect.py::parse_per_file_coverage().
CREATE TABLE IF NOT EXISTS ci_coverage_files (
    id BIGSERIAL PRIMARY KEY,
    run_id BIGINT NOT NULL REFERENCES ci_runs(run_id) ON DELETE CASCADE,
    service TEXT NOT NULL,
    file_path TEXT NOT NULL,
    region_coverage_percent NUMERIC(5,2) NOT NULL,
    function_coverage_percent NUMERIC(5,2) NOT NULL,
    line_coverage_percent NUMERIC(5,2) NOT NULL,
    UNIQUE (run_id, service, file_path)
);

-- Per-function детализация (llvm-cov report -show-functions) — какая
-- именно функция и в каком файле недопокрыта, а не только "файл X
-- покрыт на 60%". См. doc-комментарий
-- tools/ci-metrics/collect.py::parse_per_function_coverage().
CREATE TABLE IF NOT EXISTS ci_coverage_functions (
    id BIGSERIAL PRIMARY KEY,
    run_id BIGINT NOT NULL REFERENCES ci_runs(run_id) ON DELETE CASCADE,
    service TEXT NOT NULL,
    file_path TEXT NOT NULL,
    function_name TEXT NOT NULL,
    region_coverage_percent NUMERIC(5,2) NOT NULL,
    line_coverage_percent NUMERIC(5,2) NOT NULL,
    UNIQUE (run_id, service, file_path, function_name)
);

CREATE INDEX IF NOT EXISTS ci_runs_created_at_idx ON ci_runs (created_at);
CREATE INDEX IF NOT EXISTS ci_job_results_run_id_idx ON ci_job_results (run_id);
CREATE INDEX IF NOT EXISTS ci_coverage_run_id_idx ON ci_coverage (run_id);
CREATE INDEX IF NOT EXISTS ci_coverage_files_run_id_idx ON ci_coverage_files (run_id);
CREATE INDEX IF NOT EXISTS ci_coverage_functions_run_id_idx ON ci_coverage_functions (run_id);
