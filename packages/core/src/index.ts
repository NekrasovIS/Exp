// Entry point for @devicehub/core (issue #219).

export const CORE_PACKAGE_NAME = "@devicehub/core";

export { ApiError } from "./http.js";
export type { FetchLike } from "./http.js";

export { AuthClient } from "./auth/AuthClient.js";
export { SessionManager } from "./auth/SessionManager.js";
export type { SessionManagerOptions } from "./auth/SessionManager.js";
export type { AuthTokens, RegisterResult, VerifyTokenResult } from "./auth/types.js";

export { UserServiceClient } from "./user/UserServiceClient.js";
export type { FriendRequestInfo, ProfileEdits, SendFriendRequestStatus, UserProfile } from "./user/types.js";

export { ChatRestClient } from "./chat/ChatRestClient.js";
export { ChatClient } from "./chat/ChatClient.js";
export type { WebSocketFactory, WebSocketLike } from "./chat/ChatClient.js";
export type {
  ChannelUnreadCount,
  ChatItem,
  ChatMessageInfo,
  DirectMessageInfo,
  DirectMessageThreadInfo,
  IncomingChatMessage,
  ThreadUnreadCount,
} from "./chat/types.js";
