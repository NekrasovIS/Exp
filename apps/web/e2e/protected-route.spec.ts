// issue #303 — ProtectedRoute.tsx's redirect is unit-tested against
// jsdom already (test/session/ProtectedRoute.test.tsx); this is the
// same behavior through a real browser's own navigation/history APIs.

import { test, expect } from "@playwright/test";

test("redirects an unauthenticated visitor from / to /login", async ({ page }) => {
  await page.goto("/");

  await expect(page).toHaveURL(/\/login$/);
  await expect(page.getByRole("button", { name: "Send code" })).toBeVisible();
});
