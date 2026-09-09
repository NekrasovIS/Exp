// Shared E2E helpers (issue #303) — every test here mocks its own
// backend calls via page.route() rather than relying on a live
// auth-service/user-service/chat-service, so this only has the small
// bits every test file otherwise repeats: a token shaped the way
// decodeTokenSubject.ts expects, one-line fetch mocks for the
// endpoints these flows touch, and a fake chat-service WebSocket
// (page.routeWebSocket() — never touches a real server either).

import type { Page } from "@playwright/test";

function encodeBase64Url(payload: unknown): string {
  const base64 = Buffer.from(JSON.stringify(payload)).toString("base64");
  return base64.replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");
}

/** Matches TokenService.cpp's format closely enough for the client to
 * read a subject back out of it (decodeTokenSubject.ts) — the
 * signature segment is never checked client-side. */
export function fakeToken(login: string): string {
  return `${encodeBase64Url({ sub: login, exp: 9999999999 })}.sig`;
}

/** Mocks /auth/otp/request and /auth/otp/verify (auth-service, port
 * 8080 by default — see src/config.ts) so OtpLoginForm's flow
 * completes without a live server. */
export async function mockOtpLogin(page: Page, login: string): Promise<void> {
  await page.route("**/auth/otp/request", async (route) => {
    await route.fulfill({ status: 200, json: {} });
  });
  await page.route("**/auth/otp/verify", async (route) => {
    await route.fulfill({
      status: 200,
      json: { token: fakeToken(login), refresh_token: "refresh-token", expires_at: 9999999999 },
    });
  });
}

/** Mocks chat-service's /communities/mine (chat-service REST, port
 * 8082 by default) — HomePage's default view loads this immediately
 * after landing, so every "reach the home screen" test needs it
 * mocked even when communities themselves aren't what's under test. */
export async function mockEmptyCommunities(page: Page): Promise<void> {
  await page.route("**/communities/mine", async (route) => {
    await route.fulfill({ status: 200, json: [] });
  });
}

/** Seeds an already-authenticated session directly into localStorage
 * before any page script runs (page.addInitScript, not
 * page.evaluate() after goto() — SessionContext reads this on mount,
 * so it has to already be there) — lets tests that aren't *about*
 * signing in skip driving the login form every time. Call before
 * page.goto(). */
export async function seedSession(page: Page, login: string): Promise<void> {
  const token = fakeToken(login);
  await page.addInitScript(
    ({ storageKey, token: t }) => {
      localStorage.setItem(
        storageKey,
        JSON.stringify({ token: t, refreshToken: "refresh-token", expiresAt: 9999999999 }),
      );
    },
    { storageKey: "devicehub.web.session", token },
  );
}

/** A fake chat-service WebSocket (page.routeWebSocket — the connection
 * never reaches a real server): answers the hello frame
 * ({token, channel_id} or {token, dm_thread_id} — ChatClient.ts reads
 * whichever is present back off the *response*, not the request, so
 * echoing whichever one the client itself sent is correct for either
 * a channel or a DM-thread subscription) with {subscribed, ...same
 * id field...}, and echoes any sent {body} back as a message from
 * @p login, the same "broadcast back to the sender too" behavior
 * chat-service's own WebSocketServer has. Returns nothing — tests
 * that need to assert on what the page sent can inspect DOM state
 * after instead, keeping this one-way. */
export async function mockChatSocket(page: Page, login: string): Promise<void> {
  let nextMessageId = 1;
  await page.routeWebSocket(/:8083\//, (ws) => {
    ws.onMessage((raw) => {
      const message = JSON.parse(String(raw)) as Record<string, unknown>;
      if (typeof message.token === "string") {
        const idField = "channel_id" in message ? "channel_id" : "dm_thread_id";
        ws.send(JSON.stringify({ subscribed: true, [idField]: message[idField] }));
        return;
      }
      if (typeof message.body === "string") {
        ws.send(
          JSON.stringify({
            id: nextMessageId++,
            author: login,
            body: message.body,
            sent_at: new Date().toISOString(),
          }),
        );
      }
    });
  });
}
