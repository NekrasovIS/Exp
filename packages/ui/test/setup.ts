// Mirrors apps/web/test/setup.ts (issue #264) — same jest-dom matchers +
// explicit RTL cleanup, since this package's tests use the same
// explicit-import describe/it/expect convention (no vitest globals).
import "@testing-library/jest-dom/vitest";

import { cleanup } from "@testing-library/react";
import { afterEach } from "vitest";

afterEach(() => {
  cleanup();
});
