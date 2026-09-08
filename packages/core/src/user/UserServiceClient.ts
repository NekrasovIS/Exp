// TypeScript port of src/user/UserProfileClient.h/.cpp (issue #249) —
// same endpoints/fields. Every method here requires a bearer token
// (unlike AuthClient, which produces tokens rather than consuming them).

import {
  ApiError,
  extractErrorMessage,
  jsonRequestInit,
  requestJson,
  resolveUrl,
  type FetchLike,
} from "../http.js";
import type { FriendRequestInfo, ProfileEdits, SendFriendRequestStatus, UserProfile } from "./types.js";

interface ProfileResponseBody {
  login?: string;
  display_name?: string | null;
  avatar_url?: string | null;
  public_key?: string | null;
  email?: string | null;
  telegram_chat_id?: string | null;
}

interface FriendRequestResponseBody {
  id: number;
  requester_login: string;
  created_at: string;
}

const kGenericError = "Unexpected error from user-service";

function toUserProfile(body: ProfileResponseBody): UserProfile {
  const profile: UserProfile = { login: body.login ?? "" };
  if (body.display_name != null) profile.displayName = body.display_name;
  if (body.avatar_url != null) profile.avatarUrl = body.avatar_url;
  if (body.public_key != null) profile.publicKey = body.public_key;
  // issue #225/#243: these two keys are entirely absent from the
  // response when viewing someone else's profile — `in` distinguishes
  // "key absent" from "key present but null", though both collapse to
  // `undefined` on the UserProfile side either way.
  if ("email" in body && body.email != null) profile.email = body.email;
  if ("telegram_chat_id" in body && body.telegram_chat_id != null)
    profile.telegramChatId = body.telegram_chat_id;
  return profile;
}

function toFriendRequestInfo(body: FriendRequestResponseBody): FriendRequestInfo {
  return { id: body.id, requesterLogin: body.requester_login, createdAt: body.created_at };
}

export class UserServiceClient {
  constructor(
    private readonly baseUrl: string,
    private readonly fetchImpl: FetchLike = fetch,
  ) {}

  async fetchProfile(token: string, login: string): Promise<UserProfile> {
    const res = await requestJson<ProfileResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/users/${encodeURIComponent(login)}/profile`),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || res.body === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return toUserProfile(res.body);
  }

  /** Always sends all four fields (even unchanged/empty ones), matching
   * `UserProfileClient::updateOwnProfile()` — the server fills in the
   * rest of the profile from its own stored values on a partial-looking
   * edit, so this call is a full replace of these four fields, not a
   * merge on the client's side. */
  async updateOwnProfile(token: string, edits: ProfileEdits): Promise<UserProfile> {
    const res = await requestJson<ProfileResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/users/me"),
      jsonRequestInit("PATCH", token, {
        display_name: edits.displayName,
        avatar_url: edits.avatarUrl,
        email: edits.email,
        telegram_chat_id: edits.telegramChatId,
      }),
    );
    if (!res.ok || res.body === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return toUserProfile(res.body);
  }

  /** Separate partial-field call from {@link updateOwnProfile} — same
   * endpoint, but sends only `public_key` (issue #136), matching how the
   * C++ client keeps this independent from the rest of profile editing. */
  async publishPublicKey(token: string, publicKey: string): Promise<UserProfile> {
    const res = await requestJson<ProfileResponseBody>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/users/me"),
      jsonRequestInit("PATCH", token, { public_key: publicKey }),
    );
    if (!res.ok || res.body === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return toUserProfile(res.body);
  }

  async sendFriendRequest(token: string, recipientLogin: string): Promise<SendFriendRequestStatus> {
    const res = await requestJson<{ status?: SendFriendRequestStatus }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/friends/requests"),
      jsonRequestInit("POST", token, { recipient_login: recipientLogin }),
    );
    if (!res.ok || res.body?.status === undefined) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.status;
  }

  async listIncomingFriendRequests(token: string): Promise<FriendRequestInfo[]> {
    const res = await requestJson<FriendRequestResponseBody[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/friends/requests"),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body.map(toFriendRequestInfo);
  }

  async acceptFriendRequest(token: string, requestId: number): Promise<void> {
    const res = await requestJson<{ error?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/friends/requests/${requestId}/accept`),
      jsonRequestInit("POST", token),
    );
    if (!res.ok) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
  }

  async declineFriendRequest(token: string, requestId: number): Promise<void> {
    const res = await requestJson<{ error?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/friends/requests/${requestId}/decline`),
      jsonRequestInit("POST", token),
    );
    if (!res.ok) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
  }

  async listFriends(token: string): Promise<string[]> {
    const res = await requestJson<string[]>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, "/friends"),
      jsonRequestInit("GET", token),
    );
    if (!res.ok || !Array.isArray(res.body)) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
    return res.body;
  }

  async removeFriend(token: string, login: string): Promise<void> {
    const res = await requestJson<{ error?: string }>(
      this.fetchImpl,
      resolveUrl(this.baseUrl, `/friends/${encodeURIComponent(login)}`),
      jsonRequestInit("DELETE", token),
    );
    if (!res.ok) {
      throw new ApiError(res.status, extractErrorMessage(res.body) ?? kGenericError);
    }
  }
}
