// Wire types for user-service (issue #249) — mirror
// src/user/UserProfileClient.h/.cpp's documented contract. Optional
// fields collapse both "never set" (server sent `null`) and "not shown"
// (server omitted the key — issue #225/#243, viewing someone else's
// profile never includes email/telegram_chat_id) into `undefined`; a
// consumer that needs to tell those two apart can compare against the
// logged-in user's own login before calling fetchProfile().

export interface UserProfile {
  login: string;
  displayName?: string;
  avatarUrl?: string;
  publicKey?: string;
  /** Only present when the profile belongs to the caller themselves. */
  email?: string;
  /** Only present when the profile belongs to the caller themselves. */
  telegramChatId?: string;
}

export interface ProfileEdits {
  displayName: string;
  avatarUrl: string;
  email: string;
  telegramChatId: string;
}

export interface FriendRequestInfo {
  id: number;
  requesterLogin: string;
  createdAt: string;
}

export type SendFriendRequestStatus = "sent" | "accepted";
