import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { CommunitiesSidebar } from "../../src/communities/CommunitiesSidebar.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderSidebar(selectedCommunityId: number | null = null, onSelectCommunity = vi.fn()) {
  render(
    <SessionProvider>
      <CommunitiesSidebar selectedCommunityId={selectedCommunityId} onSelectCommunity={onSelectCommunity} />
    </SessionProvider>,
  );
  return { onSelectCommunity };
}

function seedSession(token = "access-token"): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token,
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
}

beforeEach(() => {
  seedSession();
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

  it("renders an unread badge when unreadCounts has a positive entry for a community", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice" }])),
    );
    render(
      <SessionProvider>
        <CommunitiesSidebar
          selectedCommunityId={null}
          onSelectCommunity={vi.fn()}
          unreadCounts={new Map([[1, 12]])}
        />
      </SessionProvider>,
    );

    const button = await screen.findByRole("button", { name: /Robotics Club/ });
    expect(button).toHaveTextContent("12");
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

  describe("invite link (issue #301)", () => {
    it("shows the selected community's invite link and lets any member copy it", async () => {
      vi.stubGlobal(
        "fetch",
        vi
          .fn()
          .mockResolvedValue(
            jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "bob", invite_code: "ABCD1234EF" }]),
          ),
      );
      const writeText = vi.fn().mockResolvedValue(undefined);
      Object.defineProperty(navigator, "clipboard", { configurable: true, value: { writeText } });
      renderSidebar(1);

      const linkInput = await screen.findByLabelText("Invite link");
      expect(linkInput).toHaveValue(`${window.location.origin}/join/ABCD1234EF`);

      await userEvent.click(screen.getByRole("button", { name: "Copy" }));

      expect(writeText).toHaveBeenCalledWith(`${window.location.origin}/join/ABCD1234EF`);
      expect(await screen.findByRole("button", { name: "Copied!" })).toBeInTheDocument();
    });

    it("hides the invite block entirely when the selected community has no invite_code", async () => {
      vi.stubGlobal(
        "fetch",
        vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "bob" }])),
      );
      renderSidebar(1);

      await screen.findByRole("button", { name: "Robotics Club" });
      expect(screen.queryByLabelText("Invite link")).not.toBeInTheDocument();
    });

    it("shows Regenerate only to the community's owner", async () => {
      seedSession(fakeToken("alice"));
      vi.stubGlobal(
        "fetch",
        vi
          .fn()
          .mockResolvedValue(
            jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "ABCD1234EF" }]),
          ),
      );
      renderSidebar(1);

      expect(await screen.findByRole("button", { name: "Regenerate" })).toBeInTheDocument();
    });

    it("does not show Regenerate to a non-owner member", async () => {
      seedSession(fakeToken("carol"));
      vi.stubGlobal(
        "fetch",
        vi
          .fn()
          .mockResolvedValue(
            jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "ABCD1234EF" }]),
          ),
      );
      renderSidebar(1);

      await screen.findByLabelText("Invite link");
      expect(screen.queryByRole("button", { name: "Regenerate" })).not.toBeInTheDocument();
    });

    it("regenerating replaces the shown invite link", async () => {
      seedSession(fakeToken("alice"));
      const fetchSpy = vi
        .fn()
        .mockResolvedValueOnce(
          jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "OLDCODE001" }]),
        )
        .mockResolvedValueOnce(jsonResponse(200, { invite_code: "NEWCODE002" }))
        .mockResolvedValueOnce(
          jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "alice", invite_code: "NEWCODE002" }]),
        );
      vi.stubGlobal("fetch", fetchSpy);
      renderSidebar(1);

      await screen.findByDisplayValue(`${window.location.origin}/join/OLDCODE001`);
      await userEvent.click(screen.getByRole("button", { name: "Regenerate" }));

      expect(
        await screen.findByDisplayValue(`${window.location.origin}/join/NEWCODE002`),
      ).toBeInTheDocument();
    });
  });
});
