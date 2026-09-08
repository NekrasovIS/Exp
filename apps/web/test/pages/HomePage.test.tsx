import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
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
      <SessionProvider>
        <HomePage />
      </SessionProvider>,
    );

    expect(screen.getByText("Select a channel to start chatting.")).toBeInTheDocument();
  });

  it("toggles to friends mode and back", async () => {
    render(
      <SessionProvider>
        <HomePage />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: "Friends" }));
    expect(screen.getByText("Select a conversation to start chatting.")).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Back to communities" }));
    expect(screen.getByText("Select a channel to start chatting.")).toBeInTheDocument();
  });
});
