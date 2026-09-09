import { render, screen } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { ChatView } from "../../src/chat/ChatView.js";
import { encryptMessage, generateChannelKey, wrapKeyForRecipient } from "../../src/crypto/channelCrypto.js";
import { getSodium } from "../../src/crypto/sodium.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, FakeWebSocket, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";
const kLogin = "alice";

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: fakeToken(kLogin),
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
}

beforeEach(() => {
  FakeWebSocket.instances = [];
  vi.stubGlobal("WebSocket", FakeWebSocket);
  seedSession();
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("ChatView", () => {
  it("mounts the real chat content for a non-encrypted channel", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={false} />
      </SessionProvider>,
    );

    expect(await screen.findByRole("button", { name: "Send" })).toBeInTheDocument();
    expect(FakeWebSocket.instances).toHaveLength(1);
  });

  it("shows a no-access message for an encrypted channel with no wrapped key on file", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/channels\/7\/keys\/me/, () => jsonResponse(404, { error: "not found" })],
        [/\/users\/me/, () => jsonResponse(200, { login: kLogin })],
      ]),
    );

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={true} />
      </SessionProvider>,
    );

    expect(await screen.findByText(/don't have access to this encrypted channel/i)).toBeInTheDocument();
    expect(FakeWebSocket.instances).toHaveLength(0);
  });

  it("decrypts and shows history once this login's channel key can be unwrapped", async () => {
    const sodium = await getSodium();
    const identityKeyPair = sodium.crypto_box_keypair();
    localStorage.setItem(
      `devicehub.web.identityKeys.${kLogin}`,
      JSON.stringify({
        publicKey: sodium.to_base64(identityKeyPair.publicKey, sodium.base64_variants.ORIGINAL),
        secretKey: sodium.to_base64(identityKeyPair.privateKey, sodium.base64_variants.ORIGINAL),
      }),
    );

    const channelKey = await generateChannelKey();
    const wrappedKey = await wrapKeyForRecipient(channelKey, identityKeyPair.publicKey);
    const ciphertext = await encryptMessage("hello from a", channelKey);

    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/channels\/7\/keys\/me/, () => jsonResponse(200, { wrapped_key: wrappedKey })],
        [/\/users\/me/, () => jsonResponse(200, { login: kLogin })],
        [
          /\/channels\/7\/messages/,
          () =>
            jsonResponse(200, [{ id: 1, author: "bob", body: ciphertext, sent_at: "2026-01-01T00:00:00Z" }]),
        ],
      ]),
    );

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={true} />
      </SessionProvider>,
    );

    expect(await screen.findByText("hello from a")).toBeInTheDocument();
    expect(FakeWebSocket.instances).toHaveLength(1);
  });

  // Issue #322 — ChatView forwards presence from whichever channel
  // socket is actually subscribed up to its caller (CommunitiesMode
  // owns the aggregated state; MembersSidebar is a sibling, not a
  // descendant, of ChatView).
  it("forwards onlineMembers/presenceChanged from the socket to the given callbacks", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    const onOnlineMembers = vi.fn();
    const onPresenceChanged = vi.fn();

    render(
      <SessionProvider>
        <ChatView
          channelId={7}
          communityId={1}
          isEncrypted={false}
          onOnlineMembers={onOnlineMembers}
          onPresenceChanged={onPresenceChanged}
        />
      </SessionProvider>,
    );
    await screen.findByRole("button", { name: "Send" });
    const socket = FakeWebSocket.instances[0]!;

    socket.onmessage?.({
      data: JSON.stringify({ subscribed: true, channel_id: 7, online_members: ["bob"] }),
    });
    expect(onOnlineMembers).toHaveBeenCalledWith(["bob"]);

    socket.onmessage?.({ data: JSON.stringify({ presence_changed: { login: "carol", online: true } }) });
    expect(onPresenceChanged).toHaveBeenCalledWith("carol", true);
  });
});
