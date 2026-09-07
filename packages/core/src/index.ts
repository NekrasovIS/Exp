// Entry point for @devicehub/core (issue #219). Client modules for
// user-service/chat-service land here in #249/#250.

export const CORE_PACKAGE_NAME = "@devicehub/core";

export { ApiError } from "./http.js";
export type { FetchLike } from "./http.js";

export { AuthClient } from "./auth/AuthClient.js";
export { SessionManager } from "./auth/SessionManager.js";
export type { SessionManagerOptions } from "./auth/SessionManager.js";
export type { AuthTokens, RegisterResult, VerifyTokenResult } from "./auth/types.js";
