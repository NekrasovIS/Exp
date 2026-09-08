import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { AttachmentDownloadLink } from "../../src/chat/AttachmentDownloadLink.js";
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
  // jsdom doesn't implement the actual blob-URL machinery — only the
  // click-triggering path here is under test, not what the browser
  // does with the resulting URL.
  vi.stubGlobal("URL", { createObjectURL: vi.fn(() => "blob:fake"), revokeObjectURL: vi.fn() });
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("AttachmentDownloadLink", () => {
  it("fetches the attachment bytes and triggers a download on click", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(new Response(new Uint8Array([1, 2, 3]), { status: 200 }));
    vi.stubGlobal("fetch", fetchSpy);
    render(
      <SessionProvider>
        <AttachmentDownloadLink attachmentId={5} filename="report.pdf" />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: /Download: report\.pdf/ }));

    expect(fetchSpy).toHaveBeenCalledWith(expect.stringContaining("/attachments/5"), expect.anything());
    expect(URL.createObjectURL).toHaveBeenCalled();
  });

  it("shows an error when the download fails", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(404, { error: "no such attachment" })));
    render(
      <SessionProvider>
        <AttachmentDownloadLink attachmentId={5} filename="report.pdf" />
      </SessionProvider>,
    );

    await userEvent.click(screen.getByRole("button", { name: /Download: report\.pdf/ }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't download/i);
  });
});
