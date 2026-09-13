import { render, screen, waitFor } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { MessageComposer } from "../../src/chat/MessageComposer.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function renderComposer(overrides: Partial<Parameters<typeof MessageComposer>[0]> = {}) {
  const onSend = overrides.onSend ?? vi.fn();
  const onCancelReply = overrides.onCancelReply ?? vi.fn();
  render(
    <SessionProvider>
      <MessageComposer
        channelId={7}
        communityId={1}
        onSend={onSend}
        {...overrides}
        onCancelReply={onCancelReply}
      />
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
  // Default stub for the /members request useMentionAutocomplete()
  // fires on mount (issue #326) — tests that care about a different
  // fetch response (upload success/failure, or the mention flow's own
  // member list) override this with their own vi.stubGlobal() call.
  vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, [])));
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
    // routedFetch, not a single shared mockResolvedValue Response — this
    // component now also fires a /members request on mount (issue #326's
    // mention autocomplete), and a Response body can only be read once;
    // reusing one Response instance for both requests would make whichever
    // one reads it second fail to parse its body.
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/attachments$/, () => jsonResponse(201, { id: 99, filename: "photo.png" })]]),
    );
    const { onSend } = renderComposer();

    const file = new File(["fake-bytes"], "photo.png", { type: "image/png" });
    await userEvent.upload(screen.getByLabelText("Attach a file"), file);

    expect(await screen.findByText(/Attached: photo\.png/)).toBeInTheDocument();

    await userEvent.type(screen.getByLabelText("Message"), "check this out");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    expect(onSend).toHaveBeenCalledWith("check this out", 99);
  });

  it("typing '@' shows member suggestions, and picking one inserts the mention", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));
    const { onSend } = renderComposer();

    const input = screen.getByLabelText("Message");
    await userEvent.type(input, "hey @al");

    await userEvent.click(await screen.findByRole("button", { name: "@alice" }));
    expect(input).toHaveValue("hey @alice ");

    await userEvent.type(input, "welcome");
    await userEvent.click(screen.getByRole("button", { name: "Send" }));

    expect(onSend).toHaveBeenCalledWith("hey @alice welcome", undefined);
  });

  it("ArrowDown/Enter selects a suggestion without submitting the form", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));
    const { onSend } = renderComposer();

    const input = screen.getByLabelText("Message");
    await userEvent.type(input, "hey @");
    await screen.findByRole("button", { name: "@bob" });

    await userEvent.keyboard("{ArrowDown}{Enter}");

    expect(input).toHaveValue("hey @bob ");
    expect(onSend).not.toHaveBeenCalled();
  });

  it("Escape dismisses the suggestions without changing the text", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice"])));
    renderComposer();

    const input = screen.getByLabelText("Message");
    await userEvent.type(input, "hey @al");
    await screen.findByRole("button", { name: "@alice" });

    await userEvent.keyboard("{Escape}");

    expect(screen.queryByRole("button", { name: "@alice" })).not.toBeInTheDocument();
    expect(input).toHaveValue("hey @al");
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
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/attachments$/, () => jsonResponse(400, { error: "not valid base64" })]]),
    );
    renderComposer();

    const file = new File(["fake-bytes"], "bad.bin", { type: "application/octet-stream" });
    await userEvent.upload(screen.getByLabelText("Attach a file"), file);

    expect(await screen.findByRole("alert")).toHaveTextContent(/couldn't upload/i);
  });

  it("persists the typed draft (debounced), and Send clears it (issue #376)", async () => {
    const { onSend } = renderComposer();

    await userEvent.type(screen.getByLabelText("Message"), "hello there");
    expect(localStorage.getItem("devicehub.web.draft.channel:7")).toBeNull();

    // Real timers throughout — the debounce is genuinely 500ms of real
    // wall-clock time here rather than faked, since faking it fights
    // this test's other async machinery (the mention-autocomplete
    // fetch on mount, userEvent's own internals).
    await waitFor(() => expect(localStorage.getItem("devicehub.web.draft.channel:7")).toBe("hello there"));

    await userEvent.click(screen.getByRole("button", { name: "Send" }));
    expect(onSend).toHaveBeenCalledWith("hello there", undefined);
    expect(localStorage.getItem("devicehub.web.draft.channel:7")).toBeNull();
  });

  it("restores a previously saved draft for this channel on mount (issue #376)", () => {
    localStorage.setItem("devicehub.web.draft.channel:7", "unsent draft");
    renderComposer();

    expect(screen.getByLabelText("Message")).toHaveValue("unsent draft");
  });
});
