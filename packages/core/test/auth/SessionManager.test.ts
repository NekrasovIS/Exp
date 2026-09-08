import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { SessionManager } from "../../src/auth/SessionManager.js";
import type { AuthClient } from "../../src/auth/AuthClient.js";
import type { AuthTokens } from "../../src/auth/types.js";

function fakeAuthClient(): AuthClient {
  return { refreshAccessToken: vi.fn() } as unknown as AuthClient;
}

const kBaseTokens: AuthTokens = { token: "t1", refreshToken: "r1", expiresAt: 1000 };

describe("SessionManager", () => {
  beforeEach(() => {
    vi.useFakeTimers();
    vi.setSystemTime(0);
  });

  afterEach(() => {
    vi.useRealTimers();
  });

  it("stores tokens and notifies onTokensChanged", () => {
    const onTokensChanged = vi.fn();
    const manager = new SessionManager(fakeAuthClient(), { onTokensChanged });

    manager.setTokens(kBaseTokens);

    expect(manager.getAccessToken()).toBe("t1");
    expect(manager.getTokens()).toEqual(kBaseTokens);
    expect(onTokensChanged).toHaveBeenCalledWith(kBaseTokens);
  });

  it("getAccessToken returns null before any tokens are set", () => {
    const manager = new SessionManager(fakeAuthClient());

    expect(manager.getAccessToken()).toBeNull();
  });

  it("schedules a refresh 60 seconds before expiresAt and applies it", async () => {
    const authClient = fakeAuthClient();
    const refreshed: AuthTokens = { token: "t2", refreshToken: "r1", expiresAt: 2000 };
    vi.mocked(authClient.refreshAccessToken).mockResolvedValue(refreshed);
    const onTokensChanged = vi.fn();
    const manager = new SessionManager(authClient, { onTokensChanged });

    // now=0, expiresAt=1000, buffer=60 -> scheduled at 940s.
    manager.setTokens(kBaseTokens);
    onTokensChanged.mockClear();

    await vi.advanceTimersByTimeAsync(939_000);
    expect(authClient.refreshAccessToken).not.toHaveBeenCalled();

    await vi.advanceTimersByTimeAsync(1_000);
    expect(authClient.refreshAccessToken).toHaveBeenCalledWith("r1");
    expect(manager.getTokens()).toEqual(refreshed);
    expect(onTokensChanged).toHaveBeenCalledWith(refreshed);
  });

  it("re-arms the schedule after a background refresh, using the new expiresAt", async () => {
    const authClient = fakeAuthClient();
    vi.mocked(authClient.refreshAccessToken).mockResolvedValueOnce({
      token: "t2",
      refreshToken: "r1",
      expiresAt: 2000,
    });
    const manager = new SessionManager(authClient);
    manager.setTokens(kBaseTokens);

    await vi.advanceTimersByTimeAsync(940_000); // first refresh fires (t=940s)
    expect(authClient.refreshAccessToken).toHaveBeenCalledTimes(1);

    vi.mocked(authClient.refreshAccessToken).mockResolvedValueOnce({
      token: "t3",
      refreshToken: "r1",
      expiresAt: 3000,
    });
    // Next refresh should be scheduled relative to the NEW expiresAt
    // (2000), not the original one — due at t=940+1000-60=1940s.
    await vi.advanceTimersByTimeAsync(999_000); // t=1939s
    expect(authClient.refreshAccessToken).toHaveBeenCalledTimes(1);
    await vi.advanceTimersByTimeAsync(1_000); // t=1940s
    expect(authClient.refreshAccessToken).toHaveBeenCalledTimes(2);
  });

  it("floors the delay at 1 second when already within the buffer or past expiry", async () => {
    const authClient = fakeAuthClient();
    vi.mocked(authClient.refreshAccessToken).mockResolvedValue({
      token: "t2",
      refreshToken: "r1",
      expiresAt: 2000,
    });
    const manager = new SessionManager(authClient);

    // expiresAt is already in the past relative to now=0.
    manager.setTokens({ token: "t1", refreshToken: "r1", expiresAt: -100 });

    await vi.advanceTimersByTimeAsync(1_000);
    expect(authClient.refreshAccessToken).toHaveBeenCalledTimes(1);
  });

  it("caps the scheduled delay at 24 hours for a very long-lived token", async () => {
    const authClient = fakeAuthClient();
    vi.mocked(authClient.refreshAccessToken).mockResolvedValue({
      token: "t2",
      refreshToken: "r1",
      expiresAt: 999_999_999,
    });
    const manager = new SessionManager(authClient);

    // expiresAt far in the future — the naive delay would be huge, but
    // must be capped at 24h (86_400s) so the timer re-arms daily instead.
    manager.setTokens({ token: "t1", refreshToken: "r1", expiresAt: 999_999_999 });

    await vi.advanceTimersByTimeAsync(86_400_000 - 1_000);
    expect(authClient.refreshAccessToken).not.toHaveBeenCalled();
    await vi.advanceTimersByTimeAsync(1_000);
    expect(authClient.refreshAccessToken).toHaveBeenCalledTimes(1);
  });

  it("cancels the scheduled refresh and notifies onTokensChanged(null) on clear()", async () => {
    const authClient = fakeAuthClient();
    const onTokensChanged = vi.fn();
    const manager = new SessionManager(authClient, { onTokensChanged });
    manager.setTokens(kBaseTokens);

    manager.clear();

    expect(manager.getAccessToken()).toBeNull();
    expect(onTokensChanged).toHaveBeenCalledWith(null);

    await vi.advanceTimersByTimeAsync(1_000_000);
    expect(authClient.refreshAccessToken).not.toHaveBeenCalled();
  });

  it("refreshNow() forces an immediate refresh and reschedules", async () => {
    const authClient = fakeAuthClient();
    vi.mocked(authClient.refreshAccessToken).mockResolvedValue({
      token: "t2",
      refreshToken: "r1",
      expiresAt: 2000,
    });
    const manager = new SessionManager(authClient);
    manager.setTokens(kBaseTokens);

    await manager.refreshNow();

    expect(authClient.refreshAccessToken).toHaveBeenCalledWith("r1");
    expect(manager.getTokens()).toEqual({ token: "t2", refreshToken: "r1", expiresAt: 2000 });
  });

  it("refreshNow() is a no-op when no session is active", async () => {
    const authClient = fakeAuthClient();
    const manager = new SessionManager(authClient);

    await manager.refreshNow();

    expect(authClient.refreshAccessToken).not.toHaveBeenCalled();
  });

  it("calls onRefreshError when a background refresh fails, without throwing", async () => {
    const authClient = fakeAuthClient();
    const failure = new Error("network down");
    vi.mocked(authClient.refreshAccessToken).mockRejectedValue(failure);
    const onRefreshError = vi.fn();
    const manager = new SessionManager(authClient, { onRefreshError });
    manager.setTokens(kBaseTokens);

    await vi.advanceTimersByTimeAsync(940_000);

    expect(onRefreshError).toHaveBeenCalledWith(failure);
  });
});
