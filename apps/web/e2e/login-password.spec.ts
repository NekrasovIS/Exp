// issue #303 — PasswordAuthForm's sign-in/register flows, the other
// half of login.spec.ts (which covers the OTP form). Vitest already
// unit-tests this component (test/auth/PasswordAuthForm.test.tsx);
// this drives the same flows through a real browser/react-router.

import { test, expect } from "@playwright/test";

import { fakeToken, mockEmptyCommunities } from "./helpers.js";

test("signs in with a password and lands on the home screen", async ({ page }) => {
  await page.route("**/auth/token", async (route) => {
    await route.fulfill({
      status: 200,
      json: { token: fakeToken("alice"), refresh_token: "refresh-token", expires_at: 9999999999 },
    });
  });
  await mockEmptyCommunities(page);

  await page.goto("/login");
  await page.getByRole("button", { name: "Sign in with password instead" }).click();
  await page.getByLabel("Login").fill("alice");
  await page.getByLabel("Password").fill("correct-password");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();

  await expect(page).toHaveURL(/\/$/);
  await expect(page.getByRole("button", { name: "Friends" })).toBeVisible();
});

test("shows an error for wrong credentials", async ({ page }) => {
  await page.route("**/auth/token", async (route) => {
    await route.fulfill({ status: 401, json: { error: "invalid credentials" } });
  });

  await page.goto("/login");
  await page.getByRole("button", { name: "Sign in with password instead" }).click();
  await page.getByLabel("Login").fill("alice");
  await page.getByLabel("Password").fill("wrong-password");
  await page.getByRole("button", { name: "Sign in", exact: true }).click();

  await expect(page.getByRole("alert")).toContainText("invalid credentials");
  await expect(page).toHaveURL(/\/login$/);
});

test("registers a new account and signs in automatically", async ({ page }) => {
  await page.route("**/auth/register", async (route) => {
    await route.fulfill({
      status: 201,
      json: {
        registered: true,
        token: fakeToken("newuser"),
        refresh_token: "refresh-token",
        expires_at: 9999999999,
      },
    });
  });
  await mockEmptyCommunities(page);

  await page.goto("/login");
  await page.getByRole("button", { name: "Sign in with password instead" }).click();
  await page.getByRole("button", { name: "Create an account instead" }).click();
  await page.getByLabel("Login").fill("newuser");
  await page.getByLabel("Password").fill("a-new-password");
  await page.getByRole("button", { name: "Create account" }).click();

  await expect(page).toHaveURL(/\/$/);
});

test("reports an already-taken login without signing in", async ({ page }) => {
  await page.route("**/auth/register", async (route) => {
    await route.fulfill({ status: 409, json: { registered: false } });
  });

  await page.goto("/login");
  await page.getByRole("button", { name: "Sign in with password instead" }).click();
  await page.getByRole("button", { name: "Create an account instead" }).click();
  await page.getByLabel("Login").fill("alice");
  await page.getByLabel("Password").fill("a-password");
  await page.getByRole("button", { name: "Create account" }).click();

  await expect(page.getByRole("alert")).toContainText("already taken");
  await expect(page).toHaveURL(/\/login$/);
});
