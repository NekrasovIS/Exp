// issue #303 — JoinPage.tsx's unauthenticated branch is unit-tested
// against jsdom (test/pages/JoinPage.test.tsx: sessionStorage +
// react-router's MemoryRouter). This is the same behavior in a real
// browser, whose sessionStorage/history implementation is what the
// feature actually has to work against in production, not a
// simulation of it.

import { test, expect } from "@playwright/test";

test("stashes the invite code and redirects an unauthenticated visitor to /login", async ({ page }) => {
  await page.goto("/join/ABCD1234EF");

  await expect(page).toHaveURL(/\/login$/);
  const stashed = await page.evaluate(() => sessionStorage.getItem("devicehub.web.pendingInviteCode"));
  expect(stashed).toBe("ABCD1234EF");
});
