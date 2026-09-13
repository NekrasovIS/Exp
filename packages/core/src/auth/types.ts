// Wire types for auth-service (issue #248) — mirror src/auth/AuthClient.h's
// documented contract 1:1. `expiresAt` is always Unix seconds, UTC (not
// milliseconds), matching the server's `expires_at` field.

export interface AuthTokens {
  token: string;
  refreshToken: string;
  expiresAt: number;
}

export interface VerifyTokenResult {
  valid: boolean;
  subject?: string;
}

export interface RegisterResult {
  registered: boolean;
  tokens?: AuthTokens;
}

/** Returned by `/auth/token`/`/auth/otp/verify` (issue #389) instead of
 * {@link AuthTokens} when the account has TOTP enabled (issue #388) —
 * `pendingToken` is a short-lived, single-purpose token that only
 * `AuthClient.verifyTotp()` accepts, not a usable session token. */
export interface TotpChallenge {
  totpRequired: true;
  pendingToken: string;
}

export type LoginResult = AuthTokens | TotpChallenge;

/** Narrows a {@link LoginResult} — a plain `"totpRequired" in result`
 * check would also work, but callers read better against a named
 * predicate at the `if` site. */
export function isTotpChallenge(result: LoginResult): result is TotpChallenge {
  return "totpRequired" in result;
}
