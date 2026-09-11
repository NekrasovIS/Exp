// issue #303 — friend requests (accept) and opening a DM thread, then
// a live message round trip through the same fake-WebSocket approach
// as communities-chat.spec.ts.

import { test, expect } from "@playwright/test";

import { mockChatSocket, seedSession } from "./helpers.js";

test("accepts an incoming friend request", async ({ page }) => {
  await seedSession(page, "alice");
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });

  let accepted = false;
  await page.route("**/friends/requests", async (route) => {
    await route.fulfill({
      status: 200,
      json: accepted ? [] : [{ id: 5, requester_login: "bob", created_at: "2026-01-01T00:00:00Z" }],
    });
  });
  await page.route("**/friends/requests/5/accept", async (route) => {
    accepted = true;
    await route.fulfill({ status: 200, json: {} });
  });
  await page.route("**/friends", async (route) => {
    await route.fulfill({ status: 200, json: accepted ? ["bob"] : [] });
  });

  await page.goto("/");
  await page.getByRole("button", { name: "Friends" }).click();
  await expect(page.getByText("bob")).toBeVisible();

  await page.getByRole("button", { name: "Accept" }).click();

  await expect(page.getByRole("button", { name: "Message" })).toBeVisible();
});

test("opens a DM thread and sends a live message", async ({ page }) => {
  await seedSession(page, "alice");
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await page.route("**/friends/requests", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await page.route("**/friends", async (route) => {
    await route.fulfill({ status: 200, json: ["bob"] });
  });
  await page.route("**/dm/threads", async (route) => {
    if (route.request().method() === "POST") {
      await route.fulfill({ status: 200, json: { id: 3 } });
      return;
    }
    await route.fulfill({ status: 200, json: [] });
  });
  await page.route("**/dm/threads/3/messages*", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await mockChatSocket(page, "bob");

  await page.goto("/");
  await page.getByRole("button", { name: "Friends" }).click();
  await page.getByRole("button", { name: "Message" }).click();

  // FriendsPanel's own "bob" row stays visible in the sidebar alongside
  // DirectMessageView's "bob" heading — getByText("bob") alone matches
  // both, so target the heading specifically.
  await expect(page.getByRole("heading", { name: "bob" })).toBeVisible();
  await page.getByLabel("Message", { exact: true }).fill("hi bob");
  await page.getByRole("button", { name: "Send", exact: true }).click();

  await expect(page.getByText("hi bob")).toBeVisible();
});
