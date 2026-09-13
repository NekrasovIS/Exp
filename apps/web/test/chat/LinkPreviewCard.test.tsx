import { render, screen } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { LinkPreviewCard } from "../../src/chat/LinkPreviewCard.js";
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

function renderCard(text: string) {
  return render(
    <SessionProvider>
      <LinkPreviewCard text={text} />
    </SessionProvider>,
  );
}

describe("LinkPreviewCard", () => {
  it("renders nothing when the message has no URL", () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);

    const { container } = renderCard("just some text, no link here");

    expect(container).toBeEmptyDOMElement();
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it("renders a card with title, description, image and domain once the preview resolves", async () => {
    vi.stubGlobal(
      "fetch",
      vi.fn().mockResolvedValue(
        jsonResponse(200, {
          available: true,
          title: "Example Article",
          description: "A short summary.",
          image_url: "https://cdn.example.test/cover.png",
        }),
      ),
    );

    renderCard("check this out: https://example.test/article");

    const link = await screen.findByRole("link");
    expect(link).toHaveAttribute("href", "https://example.test/article");
    expect(screen.getByText("Example Article")).toBeInTheDocument();
    expect(screen.getByText("A short summary.")).toBeInTheDocument();
    expect(screen.getByText("example.test")).toBeInTheDocument();
    // alt="" is deliberate (decorative thumbnail, see LinkPreviewCard's
    // own comment) — that gives it an implicit "presentation" role, not
    // "img", so this queries the element directly instead of by role.
    expect(link.querySelector("img")).toHaveAttribute("src", "https://cdn.example.test/cover.png");
  });

  it("renders nothing when the server reports no preview is available", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, { available: false })));

    const { container } = renderCard("https://example.test/no-preview");

    // Wait for the async fetchLinkPreview() to resolve without ever
    // producing a link — findByRole would time out and throw, so this
    // polls until the DOM settles empty instead.
    await new Promise((resolve) => setTimeout(resolve, 0));
    expect(container).toBeEmptyDOMElement();
    expect(screen.queryByRole("link")).not.toBeInTheDocument();
  });

  it("renders nothing when the request itself fails", async () => {
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new Error("network down")));

    const { container } = renderCard("https://example.test/");

    await new Promise((resolve) => setTimeout(resolve, 0));
    expect(container).toBeEmptyDOMElement();
  });
});
