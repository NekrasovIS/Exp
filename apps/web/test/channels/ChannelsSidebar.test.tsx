import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { ChannelsSidebar } from "../../src/channels/ChannelsSidebar.js";
import { getSodium } from "../../src/crypto/sodium.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";
const kLogin = "alice";

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

  it("renders an unread badge for a channel with a positive count, and none for a zero/absent one", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, [
          { id: 1, name: "general", owner: "alice" },
          { id: 2, name: "random", owner: "alice" },
        ]),
      ),
    );
    render(
      <SessionProvider>
        <ChannelsSidebar
          communityId={42}
          selectedChannelId={null}
          onSelectChannel={vi.fn()}
          unreadCounts={new Map([[1, 5]])}
        />
      </SessionProvider>,
    );

    const generalButton = await screen.findByRole("button", { name: /#general/ });
    expect(generalButton).toHaveTextContent("5");
    const randomButton = screen.getByRole("button", { name: "#random" });
    expect(randomButton.textContent).toBe("#random");
  });

  it("sets up encryption for members with a published key and reports the rest as skipped", async () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({
        token: fakeToken(kLogin),
        refreshToken: "refresh-token",
        expiresAt: Math.floor(Date.now() / 1000) + 3600,
      }),
    );
    const sodium = await getSodium();
    const bob = sodium.crypto_box_keypair();

    const fetchSpy = vi.fn(async (url: string, init?: RequestInit) => {
      if (init?.method === "POST" && /\/communities\/42\/channels$/.test(url)) {
        return jsonResponse(201, { id: 9, name: "secret", is_encrypted: true });
      }
      if (/\/communities\/42\/channels$/.test(url)) {
        return jsonResponse(200, []);
      }
      if (/\/communities\/42\/members/.test(url)) {
        return jsonResponse(200, [kLogin, "bob", "carol"]);
      }
      if (/\/users\/bob\/profile/.test(url)) {
        return jsonResponse(200, {
          login: "bob",
          public_key: sodium.to_base64(bob.publicKey, sodium.base64_variants.ORIGINAL),
        });
      }
      if (/\/users\/carol\/profile/.test(url)) {
        return jsonResponse(200, { login: "carol" });
      }
      if (init?.method === "PUT" && /\/keys\//.test(url)) {
        return jsonResponse(200, {});
      }
      return jsonResponse(200, []);
    });
    vi.stubGlobal("fetch", fetchSpy);
    renderSidebar(42);

    await screen.findByLabelText("New channel");
    await userEvent.type(screen.getByLabelText("New channel"), "secret");
    await userEvent.click(screen.getByLabelText("Encrypted channel"));
    await userEvent.click(screen.getByRole("button", { name: "Create" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/carol.*haven't set up encryption/i);
  });
});
