// issue #303 — the OTP/password sign-in forms already have Vitest
// coverage (test/auth/*.test.tsx); this exercises the same flow end to
// end in a real browser, through react-router's actual navigation
// rather than a MemoryRouter, and real localStorage rather than
// jsdom's.

import { test, expect } from "@playwright/test";

import { mockEmptyCommunities, mockOtpLogin } from "./helpers.js";

test("signs in with a one-time code and lands on the home screen", async ({ page }) => {
  await mockOtpLogin(page, "alice");
  await mockEmptyCommunities(page);

  await page.goto("/login");
  await page.getByLabel("Login, email, or Telegram chat ID").fill("alice");
  await page.getByRole("button", { name: "Send code" }).click();

  await page.getByLabel("6-digit code").fill("123456");
  await page.getByRole("button", { name: "Verify" }).click();

  await expect(page).toHaveURL(/\/$/);
  await expect(page.getByRole("button", { name: "Friends" })).toBeVisible();
});

test("shows the server's error message for an invalid code", async ({ page }) => {
  await page.route("**/auth/otp/request", async (route) => {
    await route.fulfill({ status: 200, json: {} });
  });
  await page.route("**/auth/otp/verify", async (route) => {
    await route.fulfill({ status: 401, json: { error: "invalid code" } });
  });

  await page.goto("/login");
  await page.getByLabel("Login, email, or Telegram chat ID").fill("alice");
  await page.getByRole("button", { name: "Send code" }).click();
  await page.getByLabel("6-digit code").fill("000000");
  await page.getByRole("button", { name: "Verify" }).click();

  await expect(page.getByRole("alert")).toContainText("invalid code");
  await expect(page).toHaveURL(/\/login$/);
});

test("switches to the password form and back", async ({ page }) => {
  await page.goto("/login");

  await page.getByRole("button", { name: "Sign in with password instead" }).click();
  await expect(page.getByRole("button", { name: "Sign in" })).toBeVisible();

  await page.getByRole("button", { name: "Sign in with a one-time code instead" }).click();
  await expect(page.getByRole("button", { name: "Send code" })).toBeVisible();
});
