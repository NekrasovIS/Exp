import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { DmThreadsList } from "../../src/dm/DmThreadsList.js";
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
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("DmThreadsList", () => {
  it("lists existing threads and calls onSelectThread when one is clicked", async () => {
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValue(
          jsonResponse(200, [{ id: 5, other_login: "bob", created_at: "2026-01-01T00:00:00Z" }]),
        ),
    );
    const onSelectThread = vi.fn();
    render(
      <SessionProvider>
        <DmThreadsList selectedThreadId={null} onSelectThread={onSelectThread} />
      </SessionProvider>,
    );

    await userEvent.click(await screen.findByRole("button", { name: "bob" }));
    expect(onSelectThread).toHaveBeenCalledWith(5, "bob");
  });

  it("renders an unread badge for a thread with a positive count, and none for one without", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, [
          { id: 5, other_login: "bob", created_at: "2026-01-01T00:00:00Z" },
          { id: 6, other_login: "carol", created_at: "2026-01-01T00:00:00Z" },
        ]),
      ),
    );
    render(
      <SessionProvider>
        <DmThreadsList selectedThreadId={null} onSelectThread={vi.fn()} unreadCounts={new Map([[5, 2]])} />
      </SessionProvider>,
    );

    const bobButton = await screen.findByRole("button", { name: /bob/ });
    expect(bobButton).toHaveTextContent("2");
    const carolButton = screen.getByRole("button", { name: "carol" });
    expect(carolButton.textContent).toBe("carol");
  });
});
