// New in the TypeScript port (issue #248) — AuthClient itself is
// stateless per-call, same as its C++ counterpart; the token
// storage/refresh-scheduling logic that DeviceHub keeps in
// `MainWindow`/`refreshTimer_` needs an equivalent home here so apps/web
// and apps/mobile don't each reimplement it. Mirrors MainWindow.cpp's
// own constants/logic exactly: a 60-second buffer before expiry, and the
// scheduled delay capped at 24h so a very long-lived token doesn't hand
// a multi-day delay to `setTimeout` (browsers/Node clamp delays above
// ~24.8 days to effectively immediate, so re-arming daily is also
// correct there, not just a cosmetic match to the C++ side).

import type { AuthClient } from "./AuthClient.js";
import type { AuthTokens } from "./types.js";

const kRefreshBufferSeconds = 60;
const kMaxScheduleSeconds = 24 * 3600;

export interface SessionManagerOptions {
  /** Called every time the held tokens change — including to `null` on
   * {@link SessionManager.clear}. Use it to persist tokens (e.g.
   * `localStorage`/`AsyncStorage`) or update app state; SessionManager
   * itself never persists anything. */
  onTokensChanged?: (tokens: AuthTokens | null) => void;
  /** Called when a background scheduled refresh fails — there's no
   * caller awaiting that specific call, so this is the only way to learn
   * about it (e.g. to force a sign-out UI when the refresh token itself
   * has expired). */
  onRefreshError?: (error: unknown) => void;
  /** Seconds since the Unix epoch, UTC — overridable for tests. */
  now?: () => number;
  scheduleTimeout?: (callback: () => void, delayMs: number) => ReturnType<typeof setTimeout>;
  clearTimeout?: (handle: ReturnType<typeof setTimeout>) => void;
}

/** Holds the current access/refresh token pair in memory and schedules
 * `AuthClient.refreshAccessToken()` ahead of `expiresAt`, mirroring
 * `MainWindow`'s `refreshTimer_` — nothing here is specific to a login
 * method (password/OTP/register all funnel into {@link setTokens}). */
export class SessionManager {
  private tokens: AuthTokens | null = null;
  private refreshHandle: ReturnType<typeof setTimeout> | null = null;
  private readonly now: () => number;
  private readonly scheduleTimeoutFn: (
    callback: () => void,
    delayMs: number,
  ) => ReturnType<typeof setTimeout>;
  private readonly clearTimeoutFn: (handle: ReturnType<typeof setTimeout>) => void;

  constructor(
    private readonly authClient: AuthClient,
    private readonly options: SessionManagerOptions = {},
  ) {
    this.now = options.now ?? (() => Math.floor(Date.now() / 1000));
    // Wrapped in arrow functions, not the bare `setTimeout`/`clearTimeout`
    // references themselves: those are called below as `this.scheduleTimeoutFn(...)`/
    // `this.clearTimeoutFn(...)`, a *method* call that binds `this` to this
    // SessionManager instance — browsers' native setTimeout/clearTimeout
    // throw "TypeError: Illegal invocation" when invoked with a receiver
    // that isn't the window/global they expect. Wrapping keeps the actual
    // call to the native function a plain (unbound-`this`) call expression.
    this.scheduleTimeoutFn =
      options.scheduleTimeout ?? ((callback, delayMs) => setTimeout(callback, delayMs));
    this.clearTimeoutFn = options.clearTimeout ?? ((handle) => clearTimeout(handle));
  }

  getAccessToken(): string | null {
    return this.tokens?.token ?? null;
  }

  getTokens(): AuthTokens | null {
    return this.tokens;
  }

  /** Adopts @p tokens as the current session and (re)schedules the next
   * background refresh — call this after any of AuthClient's
   * token-issuing methods (requestToken/register/verifyOtp) resolve. */
  setTokens(tokens: AuthTokens): void {
    this.tokens = tokens;
    this.options.onTokensChanged?.(tokens);
    this.scheduleRefresh();
  }

  /** Drops the session and cancels any pending scheduled refresh — call
   * on sign-out. */
  clear(): void {
    this.tokens = null;
    this.clearScheduledRefresh();
    this.options.onTokensChanged?.(null);
  }

  /** Forces an immediate refresh regardless of the schedule — exposed
   * for callers that want to eagerly refresh (e.g. on app foreground)
   * rather than wait for the timer. Re-arms the schedule on success,
   * same as the timer-driven path. */
  async refreshNow(): Promise<void> {
    if (this.tokens === null) {
      return;
    }
    const refreshed = await this.authClient.refreshAccessToken(this.tokens.refreshToken);
    this.tokens = refreshed;
    this.options.onTokensChanged?.(refreshed);
    this.scheduleRefresh();
  }

  private scheduleRefresh(): void {
    this.clearScheduledRefresh();
    if (this.tokens === null) {
      return;
    }
    const delaySeconds = Math.min(
      kMaxScheduleSeconds,
      Math.max(1, this.tokens.expiresAt - this.now() - kRefreshBufferSeconds),
    );
    this.refreshHandle = this.scheduleTimeoutFn(() => {
      this.refreshNow().catch((error: unknown) => this.options.onRefreshError?.(error));
    }, delaySeconds * 1000);
  }

  private clearScheduledRefresh(): void {
    if (this.refreshHandle !== null) {
      this.clearTimeoutFn(this.refreshHandle);
      this.refreshHandle = null;
    }
  }
}
