import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { ChannelsSidebar } from "../../src/channels/ChannelsSidebar.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderSidebar(communityId: number | null, onSelectChannel = vi.fn()) {
  render(
    <SessionProvider>
      <ChannelsSidebar communityId={communityId} selectedChannelId={null} onSelectChannel={onSelectChannel} />
    </SessionProvider>,
  );
  return { onSelectChannel };
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

describe("ChannelsSidebar", () => {
  it("renders nothing when no community is selected", () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);
    const { container } = render(
      <SessionProvider>
        <ChannelsSidebar communityId={null} selectedChannelId={null} onSelectChannel={vi.fn()} />
      </SessionProvider>,
    );

    expect(container).toBeEmptyDOMElement();
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it("loads and lists channels for the given community", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "general", owner: "alice" }])),
    );
    renderSidebar(42);

    expect(await screen.findByRole("button", { name: "#general" })).toBeInTheDocument();
  });

  it("calls onSelectChannel when a channel is clicked", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(jsonResponse(200, [{ id: 1, name: "general", owner: "alice" }])),
    );
    const { onSelectChannel } = renderSidebar(42);

    await userEvent.click(await screen.findByRole("button", { name: "#general" }));
    expect(onSelectChannel).toHaveBeenCalledWith(1);
  });

  it("creates a channel and refreshes the list", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(201, { id: 2, name: "random", is_encrypted: false }))
      .mockResolvedValueOnce(jsonResponse(200, [{ id: 2, name: "random", owner: "alice" }]));
    vi.stubGlobal("fetch", fetchSpy);
    renderSidebar(42);

    await screen.findByLabelText("New channel");
    await userEvent.type(screen.getByLabelText("New channel"), "random");
    await userEvent.click(screen.getByRole("button", { name: "Create" }));

    expect(await screen.findByRole("button", { name: "#random" })).toBeInTheDocument();
    expect(fetchSpy).toHaveBeenCalledTimes(3);
  });

  it("shows an error when channel creation fails", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, []))
      .mockResolvedValueOnce(jsonResponse(409, { error: "channel name already taken" }));
    vi.stubGlobal("fetch", fetchSpy);
    renderSidebar(42);

    await screen.findByLabelText("New channel");
    await userEvent.type(screen.getByLabelText("New channel"), "general");
    await userEvent.click(screen.getByRole("button", { name: "Create" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't create/i);
  });
});
