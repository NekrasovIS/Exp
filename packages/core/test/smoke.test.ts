import { describe, expect, it } from "vitest";

import { CORE_PACKAGE_NAME } from "../src/index.js";

// issue #247: confirms `pnpm test` actually discovers and runs Vitest
// through the workspace before any real client code exists (see
// #248/#249/#250) — not testing behavior, just that the toolchain wiring
// (pnpm workspace -> package script -> vitest -> ts source) works end to end.
describe("workspace smoke test", () => {
  it("resolves package exports through the TypeScript build path", () => {
    expect(CORE_PACKAGE_NAME).toBe("@devicehub/core");
  });
});
