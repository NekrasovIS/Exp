import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { MessageComposer } from "../../src/chat/MessageComposer.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderComposer(overrides: Partial<Parameters<typeof MessageComposer>[0]> = {}) {
  const onSend = overrides.onSend ?? vi.fn();
  const onCancelReply = overrides.onCancelReply ?? vi.fn();
  render(
    <SessionProvider>
      <MessageComposer channelId={7} onSend={onSend} {...overrides} onCancelReply={onCancelReply} />
    </SessionProvider>,
  );
  return { onSend, onCancelReply };
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

describe("MessageComposer", () => {
  it("does nothing when submitted empty", async () => {
    const { onSend } = renderComposer();
    await userEvent.click(screen.getByRole("button", { name: "Send" }));
    expect(onSend).not.toHaveBeenCalled();
  });

  it("sends the typed body with no attachment", async () => {
    const { onSend } = renderComposer();
    await userEvent.type(screen.getByLabelText("Message"), "hello there");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    expect(onSend).toHaveBeenCalledWith("hello there", undefined);
  });

  it("uploads a selected file, then sends with the resulting attachment id", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(201, { id: 99, filename: "photo.png" })));
    const { onSend } = renderComposer();

    const file = new File(["fake-bytes"], "photo.png", { type: "image/png" });
    await userEvent.upload(screen.getByLabelText("Attach a file"), file);

    expect(await screen.findByText(/Attached: photo\.png/)).toBeInTheDocument();

    await userEvent.type(screen.getByLabelText("Message"), "check this out");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    expect(onSend).toHaveBeenCalledWith("check this out", 99);
  });

  it("shows no reply bar when there is no reply target", () => {
    renderComposer();
    expect(screen.queryByText(/Replying to/)).not.toBeInTheDocument();
  });

  it("shows the reply bar with the target's author/snippet, and Cancel calls onCancelReply", async () => {
    const { onCancelReply } = renderComposer({
      replyTarget: { id: 5, author: "bob", snippet: "the original text" },
    });

    expect(screen.getByText("bob")).toBeInTheDocument();
    expect(screen.getByText(/the original text/)).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Cancel" }));
    expect(onCancelReply).toHaveBeenCalled();
  });

  it("clears the reply target after sending", async () => {
    const { onSend, onCancelReply } = renderComposer({
      replyTarget: { id: 5, author: "bob", snippet: "the original text" },
    });

    await userEvent.type(screen.getByLabelText("Message"), "here's my reply");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    expect(onSend).toHaveBeenCalledWith("here's my reply", undefined);
    expect(onCancelReply).toHaveBeenCalled();
  });

  it("shows an error when the upload fails", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(400, { error: "not valid base64" })));
    renderComposer();

    const file = new File(["fake-bytes"], "bad.bin", { type: "application/octet-stream" });
    await userEvent.upload(screen.getByLabelText("Attach a file"), file);

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't upload/i);
  });
});
