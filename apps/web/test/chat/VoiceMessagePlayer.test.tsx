import { cleanup, render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { VoiceMessagePlayer } from "../../src/chat/VoiceMessagePlayer.js";
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
  // Same rationale as AttachmentDownloadLink.test.tsx — jsdom doesn't
  // implement the actual blob-URL machinery.
  vi.stubGlobal("URL", { createObjectURL: vi.fn(() => "blob:fake"), revokeObjectURL: vi.fn() });
});

afterEach(() => {
  // Explicit cleanup() before unstubbing — VoiceMessagePlayer revokes
  // its blob URL from a useEffect cleanup on unmount, so URL.revokeObjectURL
  // needs to still be the stub at unmount time, not the real jsdom URL
  // (which doesn't implement it at all — same gap the stub above works
  // around). Testing Library's own automatic per-test cleanup runs in an
  // afterEach registered before this file's, so without this explicit
  // call here, the stub would already be torn down by the time it fires.
  cleanup();
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("VoiceMessagePlayer", () => {
  it("shows a Play button before anything is fetched, deferring the download", () => {
    render(
      <SessionProvider>
        <VoiceMessagePlayer attachmentId={5} filename="voice-message-1.webm" />
      </SessionProvider>,
    );

    expect(screen.getByRole("button", { name: /Play voice message/ })).toBeInTheDocument();
  });

  it("fetches the attachment and renders a playable <audio> element on click", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(new Response(new Uint8Array([1, 2, 3]), { status: 200 }));
    vi.stubGlobal("fetch", fetchSpy);
    render(
      <SessionProvider>
        <VoiceMessagePlayer attachmentId={5} filename="voice-message-1.webm" />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: /Play voice message/ }));

    expect(fetchSpy).toHaveBeenCalledWith(expect.stringContaining("/attachments/5"), expect.anything());
    expect(await screen.findByLabelText("voice-message-1.webm")).toBeInTheDocument();
    expect(screen.queryByRole("button", { name: /Play voice message/ })).not.toBeInTheDocument();
  });

  it("shows an error when the fetch fails", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(404, { error: "no such attachment" })));
    render(
      <SessionProvider>
        <VoiceMessagePlayer attachmentId={5} filename="voice-message-1.webm" />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: /Play voice message/ }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't load/i);
  });
});
