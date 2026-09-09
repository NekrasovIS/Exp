// Issue #303 — real-browser E2E coverage, alongside (not instead of)
// Vitest+jsdom's component/hook tests: routing, redirects, and
// sessionStorage behave differently enough in jsdom that a few flows
// this session hit repeatedly (the /join/:code redirect from #301,
// login) are worth a real Chromium pass, not just a simulated DOM.
//
// Runs against the production build (`vite preview`, not `vite`'s dev
// server) — deterministic static serving, no on-demand compilation to
// wait out. `pnpm build` (packages/core + this package) must have
// already run before this — same precondition the CI job
// (.github/workflows/web-clients.yml's `web-e2e`) already has via its
// own `pnpm build` step.
//
// No live backend (auth-service/user-service/chat-service/Postgres) —
// every test mocks its own network calls via page.route(), the same
// boundary Vitest's tests already mock at (fetch), just one level
// further from the code under test. Keeps this suite fast and free of
// docker-compose orchestration in CI.

import { defineConfig, devices } from "@playwright/test";

export default defineConfig({
  testDir: "./e2e",
  fullyParallel: true,
  forbidOnly: !!process.env.CI,
  retries: process.env.CI ? 1 : 0,
  reporter: process.env.CI ? "list" : "html",
  use: {
    baseURL: "http://127.0.0.1:4173",
    trace: "on-first-retry",
  },
  webServer: {
    // host/port/strictPort live in vite.config.ts's own `preview`
    // option, not CLI flags here — see that file's doc comment on why.
    command: "pnpm preview",
    url: "http://127.0.0.1:4173",
    reuseExistingServer: !process.env.CI,
    timeout: 30_000,
  },
  projects: [{ name: "chromium", use: { ...devices["Desktop Chrome"] } }],
});
