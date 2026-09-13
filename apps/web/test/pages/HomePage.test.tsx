import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { MemoryRouter } from "react-router-dom";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { HomePage } from "../../src/pages/HomePage.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

beforeEach(() => {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: "access-token",
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
  // Every mode's data hooks fetch something on mount — an empty array
  // satisfies all of them (communities/channels/friends/requests/
  // threads are all plain JSON arrays).
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("HomePage", () => {
  it("starts in communities mode", () => {
    render(
      <MemoryRouter>
        <SessionProvider>
          <HomePage />
        </SessionProvider>
      </MemoryRouter>,
    );

    expect(screen.getByText("Select a channel to start chatting.")).toBeInTheDocument();
  });

  it("toggles to friends mode and back", async () => {
    render(
      <MemoryRouter>
        <SessionProvider>
          <HomePage />
        </SessionProvider>
      </MemoryRouter>,
    );

    await userEvent.click(screen.getByRole("button", { name: "Friends" }));
    expect(screen.getByText("Select a conversation to start chatting.")).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Back to communities" }));
    expect(screen.getByText("Select a channel to start chatting.")).toBeInTheDocument();
  });

  it("links to the profile page (issue #385)", () => {
    render(
      <MemoryRouter>
        <SessionProvider>
          <HomePage />
        </SessionProvider>
      </MemoryRouter>,
    );

    expect(screen.getByRole("link", { name: "Profile" })).toHaveAttribute("href", "/profile");
  });
});
