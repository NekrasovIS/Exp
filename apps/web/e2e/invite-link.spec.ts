// issue #303/#124 — CommunitiesSidebar's invite-link block (copy for
// any member, regenerate for the owner only). Vitest already covers
// the component logic (test/communities/CommunitiesSidebar.test.tsx);
// this checks the one part that needs a real browser — the clipboard
// API — plus the same owner/non-owner UI branching end to end.

import { test, expect } from "@playwright/test";

import { seedSession } from "./helpers.js";

test("copies the invite link to the clipboard", async ({ page, context }) => {
  await context.grantPermissions(["clipboard-read", "clipboard-write"]);
  await seedSession(page, "alice");
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({
      status: 200,
      json: [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "ABCD1234EF" }],
    });
  });
  await page.route("**/communities/1/channels", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Robotics Club" }).click();

  const link = page.getByLabel("Invite link");
  await expect(link).toHaveValue(/\/join\/ABCD1234EF$/);

  await page.getByRole("button", { name: "Copy" }).click();
  await expect(page.getByRole("button", { name: "Copied!" })).toBeVisible();
  const clipboardText = await page.evaluate(() => navigator.clipboard.readText());
  expect(clipboardText).toContain("/join/ABCD1234EF");
});

test("only the owner sees Regenerate", async ({ page }) => {
  await seedSession(page, "carol");
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({
      status: 200,
      json: [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "ABCD1234EF" }],
    });
  });
  await page.route("**/communities/1/channels", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Robotics Club" }).click();

  await expect(page.getByLabel("Invite link")).toBeVisible();
  await expect(page.getByRole("button", { name: "Regenerate" })).not.toBeVisible();
});

test("the owner can regenerate the invite link", async ({ page }) => {
  await seedSession(page, "alice");
  let code = "OLDCODE001";
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({
      status: 200,
      json: [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: code }],
    });
  });
  await page.route("**/communities/1/channels", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await page.route("**/communities/1/invite/regenerate", async (route) => {
    code = "NEWCODE002";
    await route.fulfill({ status: 200, json: { invite_code: code } });
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Robotics Club" }).click();
  await expect(page.getByLabel("Invite link")).toHaveValue(/OLDCODE001$/);

  await page.getByRole("button", { name: "Regenerate" }).click();

  await expect(page.getByLabel("Invite link")).toHaveValue(/NEWCODE002$/);
});
