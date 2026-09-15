// Issue #439 — narrow-viewport ("one panel at a time") behavior of
// CommunitiesMode. Wide-viewport behavior (all columns at once) is
// already covered indirectly by HomePage.test.tsx; these tests focus on
// what changes once useIsNarrowViewport() reports true.

import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { CommunitiesMode } from "../../src/pages/CommunitiesMode.js";
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

function stubOneCommunityWithOneChannel(): void {
  vi.stubGlobal(
    "fetch",
    routedFetch([
      [
        /\/communities\/mine/,
        () => jsonResponse(200, [{ id: 1, name: "Acme", ownerLogin: kLogin, isEncrypted: false }]),
      ],
      [
        /\/communities\/1\/channels/,
        () => jsonResponse(200, [{ id: 7, name: "general", ownerLogin: kLogin, isEncrypted: false }]),
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

describe("CommunitiesMode on a wide viewport", () => {
  it("keeps every column visible after selecting a community (unchanged from before issue #439)", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    // Selecting a community narrows CommunitiesMode's own state, but on
    // a wide viewport that must never hide any of the other columns —
    // this is exactly the behavior the narrow-viewport tests below
    // deliberately change.
    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));

    expect(screen.getByRole("navigation", { name: "Communities" })).toBeInTheDocument();
    expect(await screen.findByRole("navigation", { name: "Channels" })).toBeInTheDocument();
    expect(screen.getByText("Select a channel to start chatting.")).toBeInTheDocument();
    expect(screen.getByRole("navigation", { name: "Members" })).toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "← Communities" })).not.toBeInTheDocument();
  });
});

// Issue #459 — Ctrl+K quick switcher.
describe("CommunitiesMode's quick switcher", () => {
  it("Ctrl+K opens it listing communities and the current community's channels", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );
    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));

    await userEvent.keyboard("{Control>}k{/Control}");

    expect(screen.getByRole("textbox", { name: "Jump to a community or channel" })).toBeInTheDocument();
    // "Acme"/"general" also appear as the sidebar's own buttons — the
    // switcher's own list items are buttons whose accessible name
    // includes the group label ("Community Acme"/"Channel general").
    expect(screen.getByRole("button", { name: "Community Acme" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Channel general" })).toBeInTheDocument();
  });

  it("selecting a channel from the switcher opens it, same as clicking it in the sidebar", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );
    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));
    await userEvent.keyboard("{Control>}k{/Control}");

    // Two "general" matches by now (the switcher's own entry and the
    // channel sidebar's button) — the switcher's list item is a button
    // whose accessible name also includes its group label.
    await userEvent.click(screen.getByRole("button", { name: "Channel general" }));

    expect(screen.queryByRole("textbox", { name: "Jump to a community or channel" })).not.toBeInTheDocument();
    await waitFor(() => expect(FakeWebSocket.instances).toHaveLength(1));
  });

  it("Escape closes the switcher without navigating anywhere", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );
    await userEvent.keyboard("{Control>}k{/Control}");
    expect(screen.getByRole("textbox", { name: "Jump to a community or channel" })).toBeInTheDocument();

    await userEvent.keyboard("{Escape}");

    expect(screen.queryByRole("textbox", { name: "Jump to a community or channel" })).not.toBeInTheDocument();
  });
});

describe("CommunitiesMode on a narrow viewport", () => {
  beforeEach(() => {
    stubMatchMedia(true);
  });

  it("shows only the communities sidebar initially", () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    expect(screen.getByRole("navigation", { name: "Communities" })).toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Channels" })).not.toBeInTheDocument();
    expect(screen.queryByText("Select a channel to start chatting.")).not.toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Members" })).not.toBeInTheDocument();
  });

  it("selecting a community switches to the channels sidebar with a back button", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));

    expect(screen.queryByRole("navigation", { name: "Communities" })).not.toBeInTheDocument();
    expect(await screen.findByRole("navigation", { name: "Channels" })).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "← Communities" })).toBeInTheDocument();
  });

  it("selecting a channel switches to the chat view with a back button", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));
    await userEvent.click(await screen.findByRole("button", { name: "#general" }));

    expect(screen.queryByRole("navigation", { name: "Channels" })).not.toBeInTheDocument();
    expect(screen.getByRole("button", { name: "← Channels" })).toBeInTheDocument();
    await waitFor(() => expect(FakeWebSocket.instances).toHaveLength(1));
  });

  it("the back button returns from the chat view to the channels sidebar", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));
    await userEvent.click(await screen.findByRole("button", { name: "#general" }));
    await userEvent.click(await screen.findByRole("button", { name: "← Channels" }));

    expect(await screen.findByRole("navigation", { name: "Channels" })).toBeInTheDocument();
    expect(screen.queryByRole("button", { name: "← Channels" })).not.toBeInTheDocument();
  });

  it("the back button returns from the channels sidebar to the communities sidebar", async () => {
    stubOneCommunityWithOneChannel();

    render(
      <SessionProvider>
        <CommunitiesMode />
      </SessionProvider>,
    );

    await userEvent.click(await screen.findByRole("button", { name: "Acme" }));
    await userEvent.click(await screen.findByRole("button", { name: "← Communities" }));

    expect(await screen.findByRole("navigation", { name: "Communities" })).toBeInTheDocument();
    expect(screen.queryByRole("navigation", { name: "Channels" })).not.toBeInTheDocument();
  });
});
