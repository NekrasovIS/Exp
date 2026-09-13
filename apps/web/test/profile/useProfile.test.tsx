import { act, renderHook, waitFor } from "@testing-library/react";
import React from "react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { useProfile } from "../../src/profile/useProfile.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function Wrapper({ children }: { children: React.ReactNode }) {
  return <SessionProvider>{children}</SessionProvider>;
}

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
  );
}

beforeEach(() => {
  seedSession();
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("useProfile", () => {
  it("loads the current user's own profile on mount", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, {
          login: "alice",
          display_name: "Alice",
          avatar_url: "/users/alice/avatar",
          public_key: null,
          email: null,
          telegram_chat_id: null,
        }),
      ),
    );

    const { result } = renderHook(() => useProfile(), { wrapper: Wrapper });

    await waitFor(() => expect(result.current.loading).toBe(false));
    expect(result.current.profile).toEqual({
      login: "alice",
      displayName: "Alice",
      avatarUrl: "/users/alice/avatar",
    });
  });

  it("updateDisplayName sends the edit and stores the server's updated profile", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [
          /\/users\/alice\/profile$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: "Old Name",
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
        [
          /\/users\/me$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: "New Name",
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
      ]),
    );

    const { result } = renderHook(() => useProfile(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    await act(() => result.current.updateDisplayName("New Name"));

    expect(result.current.profile?.displayName).toBe("New Name");
    expect(result.current.saving).toBe(false);
  });

  it("uploadAvatar posts the file's bytes, refreshes the profile, and bumps avatarVersion", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/profile\/avatar$/, () => jsonResponse(200, { avatar_url: "/users/alice/avatar" })],
        [
          /\/users\/alice\/profile$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: null,
              avatar_url: "/users/alice/avatar",
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
      ]),
    );

    const { result } = renderHook(() => useProfile(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));
    const versionBefore = result.current.avatarVersion;

    const file = new File(["fake-bytes"], "avatar.png", { type: "image/png" });
    await act(() => result.current.uploadAvatar(file));

    expect(result.current.error).toBeNull();
    expect(result.current.avatarVersion).toBe(versionBefore + 1);
  });

  it("surfaces an error when the avatar upload is rejected", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [
          /\/profile\/avatar$/,
          () => jsonResponse(400, { error: "'content_type' must be an image/* MIME type" }),
        ],
        [
          /\/users\/alice\/profile$/,
          () =>
            jsonResponse(200, {
              login: "alice",
              display_name: null,
              avatar_url: null,
              public_key: null,
              email: null,
              telegram_chat_id: null,
            }),
        ],
      ]),
    );

    const { result } = renderHook(() => useProfile(), { wrapper: Wrapper });
    await waitFor(() => expect(result.current.loading).toBe(false));

    const file = new File(["fake-bytes"], "not-an-image.txt", { type: "text/plain" });
    await act(() => result.current.uploadAvatar(file));

    expect(result.current.error).toMatch(/couldn't upload/i);
  });
});
