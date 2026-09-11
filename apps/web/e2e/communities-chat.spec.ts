// issue #303 — join-by-code -> channel list -> create a channel ->
// send/receive a live chat message, through a real browser end to
// end. The channel-list/create-channel/message-history pieces already
// have Vitest coverage; what's genuinely only provable in a real
// browser is the live part — a real WebSocket round trip (mocked at
// the socket boundary via page.routeWebSocket, not at fetch) actually
// updating the DOM.

import { test, expect } from "@playwright/test";

import { mockChatSocket, seedSession } from "./helpers.js";

test("joins a community by code, creates a channel, and sends a live message", async ({ page }) => {
  await seedSession(page, "alice");

  let joined = false;
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({
      status: 200,
      json: joined ? [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "ABCD1234EF" }] : [],
    });
  });
  await page.route("**/communities/join-by-code", async (route) => {
    joined = true;
    await route.fulfill({ status: 200, json: { id: 1, name: "Robotics Club" } });
  });

  let channelCreated = false;
  await page.route("**/communities/1/channels", async (route) => {
    if (route.request().method() === "POST") {
      channelCreated = true;
      await route.fulfill({ status: 201, json: { id: 7, is_encrypted: false } });
      return;
    }
    await route.fulfill({
      status: 200,
      json: channelCreated ? [{ id: 7, name: "general", owner: "alice", is_encrypted: false }] : [],
    });
  });
  await page.route("**/communities/1/moderators", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await page.route("**/channels/7/messages*", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
  await mockChatSocket(page, "bob");

  await page.goto("/");
  await page.getByLabel("Invite code").fill("ABCD1234EF");
  await page.getByRole("button", { name: "Join" }).click();
  await expect(page.getByRole("button", { name: "Robotics Club" })).toBeVisible();
  await page.getByRole("button", { name: "Robotics Club" }).click();

  await page.getByLabel("New channel").fill("general");
  await page.getByRole("button", { name: "Create" }).click();
  await expect(page.getByRole("button", { name: "#general" })).toBeVisible();
  await page.getByRole("button", { name: "#general" }).click();

  await page.getByLabel("Message", { exact: true }).fill("hello from alice");
  await page.getByRole("button", { name: "Send" }).click();

  // The fake socket in mockChatSocket() echoes any sent body back as a
  // message from "bob" — seeing that arrive is what actually proves
  // the WebSocket round trip works, not just that the composer cleared.
  await expect(page.getByText("hello from alice")).toBeVisible();
  await expect(page.getByText("bob")).toBeVisible();
});
