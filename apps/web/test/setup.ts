// Vitest setup (issue #264) — extends `expect` with jest-dom's DOM
// matchers (toBeInTheDocument(), etc.) for every test in this package,
// without each test file importing it itself.
import "@testing-library/jest-dom/vitest";

// React Testing Library's own auto-cleanup registers via a global
// `afterEach`, which only exists when vitest's `test.globals` option is
// on (it isn't here — tests import describe/it/expect explicitly,
// matching @devicehub/core's convention) — so it never fires without
// this explicit registration, leaving one test's rendered tree mounted
// underneath the next test's.
import { cleanup } from "@testing-library/react";
import { afterEach } from "vitest";

afterEach(() => {
  cleanup();
});
