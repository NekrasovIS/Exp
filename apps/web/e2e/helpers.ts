// Shared E2E helpers (issue #303) — every test here mocks its own
// backend calls via page.route() rather than relying on a live
// auth-service/user-service/chat-service, so this only has the small
// bits every test file otherwise repeats: a token shaped the way
// decodeTokenSubject.ts expects, and one-line fetch mocks for the
// endpoints these flows touch.

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
