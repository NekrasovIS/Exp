import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { MessageSearch } from "../../src/chat/MessageSearch.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderSearch() {
  render(
    <SessionProvider>
      <MessageSearch channelId={7} />
    </SessionProvider>,
  );
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

describe("MessageSearch", () => {
  it("shows matching results", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [
            { id: 1, author: "alice", body: "found this", sent_at: "2026-01-01T00:00:00Z" },
          ]),
        ),
    );
    renderSearch();

    await userEvent.type(screen.getByLabelText("Search this channel"), "found");
    await userEvent.click(screen.getByRole("button", { name: "Search" }));

    expect(await screen.findByText("found this")).toBeInTheDocument();
  });

  it("shows a no-matches message for an empty result set", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
    renderSearch();

    await userEvent.type(screen.getByLabelText("Search this channel"), "nothing");
    await userEvent.click(screen.getByRole("button", { name: "Search" }));

    expect(await screen.findByText("No matches.")).toBeInTheDocument();
  });

  it("shows an error when the channel is encrypted (search rejected with 400)", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(jsonResponse(400, { error: "search isn't available for encrypted channels" })),
    );
    renderSearch();

    await userEvent.type(screen.getByLabelText("Search this channel"), "anything");
    await userEvent.click(screen.getByRole("button", { name: "Search" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/search isn't available/i);
  });
});
