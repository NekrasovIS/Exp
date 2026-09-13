// Shared by every form that calls auth-service/user-service directly
// (issue #265, generalized in #391 for the profile page's TOTP forms).
// Their error strings ("invalid credentials", "invalid or expired
// code", "invalid code", "too many requests, try again later") are
// already short and user-presentable, so they're shown as-is — this
// only covers the one case that isn't: a network/parse failure that
// never reached the server at all.

import { ApiError } from "@devicehub/core";

export function describeApiError(error: unknown): string {
  if (error instanceof ApiError) {
    return error.message;
  }
  return "Couldn't reach the server. Check your connection and try again.";
}
