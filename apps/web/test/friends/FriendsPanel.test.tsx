import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { FriendsPanel } from "../../src/friends/FriendsPanel.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderPanel(onOpenThreadWith = vi.fn()) {
  render(
    <SessionProvider>
      <FriendsPanel onOpenThreadWith={onOpenThreadWith} />
    </SessionProvider>,
  );
  return { onOpenThreadWith };
}

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: "access-token",
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

function stubFriendsAndRequests(friends: string[], requests: unknown[]) {
  vi.stubGlobal(
    "fetch",
    vi.fn().mockImplementation((url: string) => {
      if (url.endsWith("/friends")) {
        return Promise.resolve(jsonResponse(200, friends));
      }
      return Promise.resolve(jsonResponse(200, requests));
    }),
  );
}

describe("FriendsPanel", () => {
  it("lists friends and incoming requests", async () => {
    stubFriendsAndRequests(
      ["bob"],
      [{ id: 1, requester_login: "carol", created_at: "2026-01-01T00:00:00Z" }],
    );
    renderPanel();

    expect(await screen.findByText("bob")).toBeInTheDocument();
    expect(screen.getByText("carol")).toBeInTheDocument();
  });

  it("calls onOpenThreadWith when Message is clicked", async () => {
    stubFriendsAndRequests(["bob"], []);
    const { onOpenThreadWith } = renderPanel();

    await screen.findByText("bob");
    await userEvent.click(screen.getByRole("button", { name: "Message" }));

    expect(onOpenThreadWith).toHaveBeenCalledWith("bob");
  });

  it("sends a friend request and reports an error on failure", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockImplementation((url: string, init?: RequestInit) => {
        if (init?.method === "POST") {
          return Promise.resolve(jsonResponse(400, { error: "cannot friend yourself" }));
        }
        if (url.endsWith("/friends")) {
          return Promise.resolve(jsonResponse(200, []));
        }
        return Promise.resolve(jsonResponse(200, []));
      }),
    );
    renderPanel();

    await screen.findByLabelText("Add a friend");
    await userEvent.type(screen.getByLabelText("Add a friend"), "alice");
    await userEvent.click(screen.getByRole("button", { name: "Send request" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't send/i);
  });
});
