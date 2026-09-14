import { act, render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
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

  // Issue #318 — same typing protocol channels always supported
  // server-side (issue #96), now wired up on the web client too, for
  // both the plain and encrypted composer.

  it("typing in the (non-encrypted) composer sends a typing frame, and a received user_typing shows the indicator", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={false} />
      </SessionProvider>,
    );
    await screen.findByLabelText("Message");
    const socket = FakeWebSocket.instances[0]!;

    await userEvent.type(screen.getByLabelText("Message"), "h");
    expect(socket.sent.map((frame) => JSON.parse(frame))).toContainEqual({ typing: true });

    expect(screen.queryByText("bob is typing…")).not.toBeInTheDocument();
    act(() => socket.onmessage?.({ data: JSON.stringify({ user_typing: "bob" }) }));
    expect(screen.getByText("bob is typing…")).toBeInTheDocument();
  });

  it("typing in the encrypted composer sends a typing frame, and a received user_typing shows the indicator", async () => {
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
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/channels\/7\/keys\/me/, () => jsonResponse(200, { wrapped_key: wrappedKey })],
        [/\/users\/me/, () => jsonResponse(200, { login: kLogin })],
        [/\/channels\/7\/messages/, () => jsonResponse(200, [])],
      ]),
    );

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={true} />
      </SessionProvider>,
    );
    await screen.findByLabelText("Message");
    const socket = FakeWebSocket.instances[0]!;

    await userEvent.type(screen.getByLabelText("Message"), "h");
    expect(socket.sent.map((frame) => JSON.parse(frame))).toContainEqual({ typing: true });

    act(() => socket.onmessage?.({ data: JSON.stringify({ user_typing: "bob" }) }));
    expect(screen.getByText("bob is typing…")).toBeInTheDocument();
  });

  // Issue #409 — EncryptedChatViewContent used to reimplement the
  // mention-autocomplete <input> wiring independently of
  // MessageComposer.tsx, and never got issue #369's fix: picking a
  // suggestion used to reposition the caret via requestAnimationFrame,
  // scheduled independently of React's own commit ordering, which hit a
  // reproducible input-scrambling failure on CI. The fix moved this to a
  // useLayoutEffect keyed on the body text instead. Both composers now
  // share useMentionInput(), which only ever uses the useLayoutEffect
  // approach.
  //
  // This asserts the mechanism directly (requestAnimationFrame is never
  // called) rather than the end symptom (garbled text after continued
  // typing) — confirmed by testing against this file's own pre-#409 code
  // that the symptom-level assertion passes even with the buggy
  // requestAnimationFrame version in this jsdom/vitest environment (the
  // same non-reproduction #369's own fix already ran into: "not
  // reproduced under a stubbed/delayed requestAnimationFrame either").
  // Asserting on the mechanism is what actually fails before this fix
  // and passes after it.
  it("picking a mention suggestion in the encrypted composer never uses requestAnimationFrame (issue #369's fix)", async () => {
    const rafSpy = vi.spyOn(window, "requestAnimationFrame");
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
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/channels\/7\/keys\/me/, () => jsonResponse(200, { wrapped_key: wrappedKey })],
        [/\/users\/me/, () => jsonResponse(200, { login: kLogin })],
        [/\/channels\/7\/messages/, () => jsonResponse(200, [])],
        [/\/communities\/1\/members/, () => jsonResponse(200, ["alice", "bob"])],
      ]),
    );

    render(
      <SessionProvider>
        <ChatView channelId={7} communityId={1} isEncrypted={true} />
      </SessionProvider>,
    );
    const input = await screen.findByLabelText("Message");

    // Click path (selectMention()).
    await userEvent.type(input, "hey @al");
    await userEvent.click(await screen.findByRole("button", { name: "@alice" }));
    expect(input).toHaveValue("hey @alice ");

    // Keyboard path (handleKeyDown()'s Enter/Tab branch) — the old code
    // had a second, separate requestAnimationFrame call here.
    await userEvent.type(input, "@b");
    await screen.findByRole("button", { name: "@bob" });
    await userEvent.keyboard("{Enter}");
    expect(input).toHaveValue("hey @alice @bob ");

    expect(rafSpy).not.toHaveBeenCalled();
  });
});
