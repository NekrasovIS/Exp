import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { CommunitiesSidebar } from "../../src/communities/CommunitiesSidebar.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderSidebar(onSelectCommunity = vi.fn()) {
  render(
    <SessionProvider>
      <CommunitiesSidebar selectedCommunityId={null} onSelectCommunity={onSelectCommunity} />
    </SessionProvider>,
  );
  return { onSelectCommunity };
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

describe("CommunitiesSidebar", () => {
  it("loads and lists the caller's communities", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice" }])),
    );
    renderSidebar();

    expect(await screen.findByRole("button", { name: "Robotics Club" })).toBeInTheDocument();
  });

  it("calls onSelectCommunity when a community is clicked", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice" }])),
    );
    const { onSelectCommunity } = renderSidebar();

    await userEvent.click(await screen.findByRole("button", { name: "Robotics Club" }));
    expect(onSelectCommunity).toHaveBeenCalledWith(1);
  });

  it("joins by invite code and refreshes the list", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(200, { id: 2, name: "Chess Club" }))
      .mockResolvedValueOnce(jsonResponse(200, [{ id: 2, name: "Chess Club", owner: "bob" }]));
    vi.stubGlobal("fetch", fetchSpy);
    renderSidebar();

    await screen.findByLabelText("Invite code");
    await userEvent.type(screen.getByLabelText("Invite code"), "ABCD1234EF");
    await userEvent.click(screen.getByRole("button", { name: "Join" }));

    expect(await screen.findByRole("button", { name: "Chess Club" })).toBeInTheDocument();
    expect(fetchSpy).toHaveBeenCalledTimes(3);
  });

  it("shows an error for an unknown invite code", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(404, { error: "no such community" }));
    vi.stubGlobal("fetch", fetchSpy);
    renderSidebar();

    await screen.findByLabelText("Invite code");
    await userEvent.type(screen.getByLabelText("Invite code"), "not-a-code");
    await userEvent.click(screen.getByRole("button", { name: "Join" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/doesn't match any community/i);
  });
});
