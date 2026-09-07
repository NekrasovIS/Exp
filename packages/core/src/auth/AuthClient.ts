// TypeScript port of src/auth/AuthClient.h/.cpp (issue #248) — same
// endpoints/fields, ported from Qt signals to promises: a method resolves
// on success and throws ApiError on failure, instead of emitting
// tokenReceived()/errorOccurred(). None of these calls need a bearer
// token — this client *produces* tokens, it doesn't consume them.

import {
  ApiError,
  extractErrorMessage,
  jsonRequestInit,
  requestJson,
  resolveUrl,
  type FetchLike,
} from "../http.js";
import type { AuthTokens, RegisterResult, VerifyTokenResult } from "./types.js";

interface TokenResponseBody {
  token?: string;
  refresh_token?: string;
  expires_at?: number;
}

interface VerifyResponseBody {
  valid?: boolean;
  subject?: string;
}

interface RegisterResponseBody {
  registered?: boolean;
  token?: string;
  refresh_token?: string;
  expires_at?: number;
}

interface RefreshResponseBody {
  token?: string;
  expires_at?: number;
}

const kMalformedResponse = "Malformed response from auth-service";

function toAuthTokens(body: TokenResponseBody): AuthTokens {
  return {
    token: body.token ?? "",
    refreshToken: body.refresh_token ?? "",
    expiresAt: body.expires_at ?? 0,
  };
}

export class AuthClient {
  constructor(
    private readonly baseUrl: string,
    private readonly fetchImpl: FetchLike = fetch,
  ) {}

  async requestToken(login: string, password: string): Promise<AuthTokens> {
    const res = await requestJson<TokenResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/token"),
      jsonRequestInit("POST", undefined, { login, password }),
    );
    if (res.body?.token === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kMalformedResponse);
    }
    return toAuthTokens(res.body);
  }

  async verifyToken(token: string): Promise<VerifyTokenResult> {
    const res = await requestJson<VerifyResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/verify"),
      jsonRequestInit("POST", undefined, { token }),
    );
    // Mirrors AuthClient.cpp's own asymmetry: a missing "valid" field is
    // always reported as this fixed generic message, never the body's
    // own "error" field (unlike every other method in this class).
    if (res.body?.valid === undefined) {
      throw new ApiError(res.status, kMalformedResponse);
    }
    const result: VerifyTokenResult = { valid: res.body.valid };
    if (res.body.subject !== undefined) {
      result.subject = res.body.subject;
    }
    return result;
  }

  async register(login: string, password: string): Promise<RegisterResult> {
    // The body is inspected regardless of HTTP status — auth-service
    // returns a valid `{registered}` body on both 201 (created) and 409
    // (login already taken), and only a genuine failure to parse a
    // `registered` field at all should be treated as an error.
    const res = await requestJson<RegisterResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/register"),
      jsonRequestInit("POST", undefined, { login, password }),
    );
    if (res.body?.registered === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kMalformedResponse);
    }
    const result: RegisterResult = { registered: res.body.registered };
    // Auto-login: a fresh registration also issues a token immediately,
    // exactly like AuthClient::registerUser() re-emitting tokenReceived().
    if (res.body.registered && res.body.token !== undefined) {
      result.tokens = toAuthTokens(res.body);
    }
    return result;
  }

  /** @returns fresh access token/expiry — @p refreshToken is echoed back
   * unchanged in the result (auth-service never rotates refresh tokens,
   * see TokenService's doc comment), letting the caller keep treating
   * the result as a complete {@link AuthTokens} without tracking the
   * refresh token separately. */
  async refreshAccessToken(refreshToken: string): Promise<AuthTokens> {
    const res = await requestJson<RefreshResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/refresh"),
      jsonRequestInit("POST", undefined, { refresh_token: refreshToken }),
    );
    if (res.body?.token === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kMalformedResponse);
    }
    return { token: res.body.token, refreshToken, expiresAt: res.body.expires_at ?? 0 };
  }

  /** The server always responds success regardless of whether @p identifier
   * resolves to a real account — this deliberately doesn't leak account
   * existence through the response, so the resolved promise carries no
   * information beyond "the request was accepted". */
  async requestOtp(identifier: string): Promise<void> {
    const res = await requestJson<{ error?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/otp/request"),
      jsonRequestInit("POST", undefined, { identifier }),
    );
    if (!res.ok) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kMalformedResponse);
    }
  }

  async verifyOtp(identifier: string, code: string): Promise<AuthTokens> {
    const res = await requestJson<TokenResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/auth/otp/verify"),
      jsonRequestInit("POST", undefined, { identifier, code }),
    );
    if (res.body?.token === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kMalformedResponse);
    }
    return toAuthTokens(res.body);
  }
}
