#!/usr/bin/env python3
"""Опрашивает GitHub Actions API за прогонами workflow CI и пишет
per-job статус, число тестов и % покрытия кода в ci-metrics-postgres
(issue #193) — источник данных для дашборда Grafana в этом каталоге.

Pull, а не push: раннеры GitHub Actions не имеют сетевого доступа к
локальной машине разработчика, поэтому эта сторона (машина
разработчика) сама периодически опрашивает GitHub API через уже
аутентифицированный `gh` CLI, а не наоборот.

Использование: python tools/ci-metrics/collect.py [--limit N] [--repo OWNER/REPO]
Требует: gh (аутентифицированный), docker compose (поднятый ci-metrics-postgres).
"""

import argparse
import json
import re
import subprocess
import sys

GH = "gh"

JOB_NAME_RE = re.compile(
    r"^(?P<kind>[a-zA-Z][a-zA-Z-]*)"
    r"(?:\s*\((?P<arg1>[^,)]+)(?:,\s*(?P<arg2>[^)]+))?\))?$"
)
TEST_SUMMARY_RE = re.compile(
    r"(?P<passed_percent>\d+(?:\.\d+)?)% tests passed, "
    r"(?P<failed>\d+) tests failed out of (?P<total>\d+)"
)
# llvm-cov report's TOTAL line: Regions/Missed/Cover%, Functions/Missed/Cover%,
# Lines/Missed/Cover%, Branches/Missed/Cover% — see .github/workflows/ci.yml's
# "Generate coverage report" step. Not anchored to line start: every raw job
# log line from the GitHub API is itself prefixed with an ISO timestamp
# ("2026-09-04T14:09:14...Z TOTAL   384   152   60.42%   ..."), so `^TOTAL`
# never matches — search anywhere in the log instead.
COVERAGE_TOTAL_RE = re.compile(
    r"TOTAL\s+\d+\s+\d+\s+(?P<region_pct>[\d.]+)%\s+"
    r"\d+\s+\d+\s+(?P<function_pct>[\d.]+)%\s+"
    r"\d+\s+\d+\s+(?P<line_pct>[\d.]+)%"
)


def run(args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, text=True, **kwargs)


def gh_json(args):
    result = run([GH, *args])
    return json.loads(result.stdout)


def classify_job(job_name):
    """Раскладывает имя job'а из матрицы ci.yml на (kind, service, sanitizer).

    Примеры входа: "build-and-test", "sanitizers (auth-service, tsan)",
    "coverage (chat-service, chat_service_tests)", "services (user-service)",
    "fuzz (chat-service, base64_fuzz json_guard_fuzz)".
    """
    match = JOB_NAME_RE.match(job_name)
    if match is None:
        return job_name, None, None
    kind = match.group("kind")
    arg1 = match.group("arg1")
    arg2 = match.group("arg2")
    if kind == "sanitizers":
        return kind, arg1, arg2
    if kind in ("coverage", "services", "fuzz"):
        return kind, arg1, None
    return kind, None, None


def fetch_job_log(repo, job_id):
    result = subprocess.run(
        [GH, "api", f"repos/{repo}/actions/jobs/{job_id}/logs", "--allow-escape-sequences"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return ""
    return result.stdout


def parse_test_summary(log_text):
    match = TEST_SUMMARY_RE.search(log_text)
    if match is None:
        return None, None
    return int(match.group("total")), int(match.group("failed"))


def parse_coverage(log_text):
    match = COVERAGE_TOTAL_RE.search(log_text)
    if match is None:
        return None
    return {
        "region": match.group("region_pct"),
        "function": match.group("function_pct"),
        "line": match.group("line_pct"),
    }


def sql_str(value):
    if value is None:
        return "NULL"
    return "'" + str(value).replace("'", "''") + "'"


def sql_num(value):
    return "NULL" if value is None else str(value)


def existing_run_ids():
    result = run(
        ["docker", "compose", "exec", "-T", "ci-metrics-postgres", "psql", "-U", "ci_metrics",
         "-d", "ci_metrics", "-tAc", "SELECT run_id FROM ci_runs"]
    )
    return {int(line) for line in result.stdout.splitlines() if line.strip()}


def apply_sql(statements):
    if not statements:
        return
    script = "\n".join(statements)
    subprocess.run(
        ["docker", "compose", "exec", "-T", "ci-metrics-postgres", "psql", "-U", "ci_metrics",
         "-d", "ci_metrics", "-v", "ON_ERROR_STOP=1"],
        input=script,
        text=True,
        check=True,
    )


def collect(repo, workflow, limit):
    runs = gh_json([
        "run", "list", "--repo", repo, "--workflow", workflow, "--limit", str(limit),
        "--json", "databaseId,headBranch,headSha,event,createdAt,url,conclusion,status",
    ])
    runs = [r for r in runs if r["status"] == "completed"]

    known_ids = existing_run_ids()
    new_runs = [r for r in runs if r["databaseId"] not in known_ids]
    if not new_runs:
        print("No new completed runs to collect.")
        return

    statements = []
    for run_info in new_runs:
        run_id = run_info["databaseId"]
        print(f"Run {run_id} ({run_info['headBranch']}, {run_info['conclusion']})...")
        statements.append(
            "INSERT INTO ci_runs (run_id, workflow, branch, commit_sha, event, conclusion, created_at, html_url) "
            f"VALUES ({run_id}, {sql_str(workflow)}, {sql_str(run_info['headBranch'])}, "
            f"{sql_str(run_info['headSha'])}, {sql_str(run_info['event'])}, {sql_str(run_info['conclusion'])}, "
            f"{sql_str(run_info['createdAt'])}, {sql_str(run_info['url'])}) "
            "ON CONFLICT (run_id) DO NOTHING;"
        )

        jobs = gh_json(["run", "view", str(run_id), "--repo", repo, "--json", "jobs"])["jobs"]
        for job in jobs:
            if job["status"] != "completed":
                continue
            kind, service, sanitizer = classify_job(job["name"])
            started_at = job.get("startedAt")
            completed_at = job.get("completedAt")
            duration_seconds = None
            if started_at and completed_at:
                from datetime import datetime
                fmt = "%Y-%m-%dT%H:%M:%SZ"
                duration_seconds = int(
                    (datetime.strptime(completed_at, fmt) - datetime.strptime(started_at, fmt)).total_seconds()
                )

            tests_total = tests_failed = None
            job_id = job.get("databaseId")
            needs_log = kind in ("build-and-test", "services", "sanitizers", "coverage") and job_id
            log_text = fetch_job_log(repo, job_id) if needs_log else ""
            if needs_log:
                tests_total, tests_failed = parse_test_summary(log_text)

            statements.append(
                "INSERT INTO ci_job_results (run_id, job_name, kind, service, sanitizer, conclusion, "
                "started_at, completed_at, duration_seconds, tests_total, tests_failed) VALUES ("
                f"{run_id}, {sql_str(job['name'])}, {sql_str(kind)}, {sql_str(service)}, {sql_str(sanitizer)}, "
                f"{sql_str(job['conclusion'])}, {sql_str(started_at)}, {sql_str(completed_at)}, "
                f"{sql_num(duration_seconds)}, {sql_num(tests_total)}, {sql_num(tests_failed)}) "
                "ON CONFLICT (run_id, job_name) DO NOTHING;"
            )

            if kind == "coverage" and service and job["conclusion"] == "success":
                coverage = parse_coverage(log_text)
                if coverage:
                    statements.append(
                        "INSERT INTO ci_coverage (run_id, service, region_coverage_percent, "
                        "function_coverage_percent, line_coverage_percent) VALUES ("
                        f"{run_id}, {sql_str(service)}, {coverage['region']}, {coverage['function']}, "
                        f"{coverage['line']}) ON CONFLICT (run_id, service) DO NOTHING;"
                    )

    apply_sql(statements)
    print(f"Collected {len(new_runs)} run(s).")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", default="NekrasovIS/Exp")
    parser.add_argument("--workflow", default="CI")
    parser.add_argument("--limit", type=int, default=20)
    args = parser.parse_args()

    try:
        collect(args.repo, args.workflow, args.limit)
    except subprocess.CalledProcessError as error:
        print(f"Command failed: {' '.join(error.cmd)}\n{error.stderr}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
