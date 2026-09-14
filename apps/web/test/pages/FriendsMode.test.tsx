// Issue #439 — narrow-viewport ("one panel at a time", tab-switched
// between Friends/Messages) behavior of FriendsMode. Wide-viewport
// behavior (both list columns always visible) is unchanged from before
// and only lightly re-checked here; the focus is what changes once
// useIsNarrowViewport() reports true.

import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { FriendsMode } from "../../src/pages/FriendsMode.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, FakeWebSocket, jsonResponse, routedFetch, stubMatchMedia } from "../testUtils.js";

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

function stubOneThread(): void {
  vi.stubGlobal(
    "fetch",
    routedFetch([
      [
        /\/dm\/threads/,
        () => jsonResponse(200, [{ id: 5, other_login: "bob", created_at: "2026-01-01T00:00:00Z" }]),
      ],
    ]),
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

describe("FriendsMode on a wide viewport", () => {
  it("shows both list columns and the placeholder at once (unchanged from before issue #439)", () => {
    stubOneThread();

    render(
      <SessionProvider>
        <FriendsMode />
      </SessionProvider>,
    );

    expect(screen.getByRole("navigation", { name: "Friends" })).toBeInTheDocument();
    expect(screen.getByRole("navigation", { name: "Conversations" })).toBeInTheDocument();
    expect(screen.getByText("Select a conversation to start chatting.")).toBeInTheDocument();
  });
});

describe("FriendsMode on a narrow viewport", () => {
  beforeEach(() => {
    stubMatchMedia(true);
  });

  it("shows the Friends tab by default and hides Conversations/main", () => {
    stubOneThread();

    render(
      <SessionProvider>
        <FriendsMode />
      </SessionProvider>,
    );

    expect(screen.getByRole("navigation", { name: "Friends" })).toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Conversations" })).not.toBeInTheDocument();
    expect(screen.queryByText("Select a conversation to start chatting.")).not.toBeInTheDocument();
  });

  it("switching to the Messages tab shows Conversations and hides Friends", async () => {
    stubOneThread();

    render(
      <SessionProvider>
        <FriendsMode />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: "Messages" }));

    expect(screen.queryByRole("navigation", { name: "Friends" })).not.toBeInTheDocument();
    expect(await screen.findByRole("navigation", { name: "Conversations" })).toBeInTheDocument();
  });

  it("selecting a thread shows the conversation full-width with a back button, hiding both tabs", async () => {
    stubOneThread();

    render(
      <SessionProvider>
        <FriendsMode />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: "Messages" }));
    await userEvent.click(await screen.findByRole("button", { name: "bob" }));

    expect(await screen.findByRole("heading", { name: "bob" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "← Messages" })).toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Friends" })).not.toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Conversations" })).not.toBeInTheDocument();
  });

  it("the back button returns to the threads list", async () => {
    stubOneThread();

    render(
      <SessionProvider>
        <FriendsMode />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: "Messages" }));
    await userEvent.click(await screen.findByRole("button", { name: "bob" }));
    await userEvent.click(screen.getByRole("button", { name: "← Messages" }));

    expect(await screen.findByRole("navigation", { name: "Conversations" })).toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "← Messages" })).not.toBeInTheDocument();
  });
});
