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
