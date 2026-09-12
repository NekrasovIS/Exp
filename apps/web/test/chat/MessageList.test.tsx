import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { MessageList } from "../../src/chat/MessageList.js";

const kMessages = [
  { id: 1, author: "alice", body: "hi from alice", sentAt: "2026-01-01T00:00:00Z", reactions: [] },
  { id: 2, author: "bob", body: "hi from bob", sentAt: "2026-01-01T00:01:00Z", reactions: [] },
];

function renderList(overrides: Partial<Parameters<typeof MessageList>[0]> = {}) {
  return render(
    <MessageList
      messages={kMessages}
      editedIds={new Set()}
      pinnedIds={new Set()}
      currentLogin="alice"
      isModerator={false}
      onEdit={vi.fn()}
      onDelete={vi.fn()}
      onReply={vi.fn()}
      onToggleReaction={vi.fn()}
      onPin={vi.fn()}
      onUnpin={vi.fn()}
      {...overrides}
    />,
  );
}

describe("MessageList", () => {
  it("shows Edit and Delete only on the current user's own message", () => {
    renderList();

    const items = screen.getAllByRole("listitem");
    expect(items[0]).toHaveTextContent("hi from alice");
    const aliceButtonNames = Array.from(items[0]!.querySelectorAll("button")).map((b) => b.textContent);
    expect(aliceButtonNames).toEqual(["React", "Reply", "Edit", "Delete"]);
    // bob's message: React/Reply are always available, but no Edit/Delete
    // for alice (not the author, not a moderator).
    const bobButtonNames = Array.from(items[1]!.querySelectorAll("button")).map((b) => b.textContent);
    expect(bobButtonNames).toEqual(["React", "Reply"]);
  });

  it("shows Delete and Pin (but not Edit) on someone else's message for a moderator", () => {
    renderList({ isModerator: true });

    const bobItem = screen.getAllByRole("listitem")[1]!;
    expect(bobItem).toHaveTextContent("hi from bob");
    const buttonNames = Array.from(bobItem.querySelectorAll("button")).map((b) => b.textContent);
    expect(buttonNames).toEqual(["React", "Reply", "Pin", "Delete"]);
  });

  it("marks an edited message and lets the author save a new body", async () => {
    const onEdit = vi.fn();
    renderList({ editedIds: new Set([1]), onEdit });

    expect(screen.getByText("(edited)")).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Edit" }));
    const input = screen.getByRole("textbox");
    await userEvent.clear(input);
    await userEvent.type(input, "updated body");
    await userEvent.click(screen.getByRole("button", { name: "Save" }));

    expect(onEdit).toHaveBeenCalledWith(1, "updated body");
  });

  it("calls onDelete with the message id", async () => {
    const onDelete = vi.fn();
    renderList({ onDelete });

    await userEvent.click(screen.getByRole("button", { name: "Delete" }));
    expect(onDelete).toHaveBeenCalledWith(1);
  });

  it("calls onReply with the message id for any message, including someone else's", async () => {
    const onReply = vi.fn();
    renderList({ onReply });

    const bobItem = screen.getAllByRole("listitem")[1]!;
    const bobReplyButton = Array.from(bobItem.querySelectorAll("button")).find(
      (b) => b.textContent === "Reply",
    )!;
    await userEvent.click(bobReplyButton);

    expect(onReply).toHaveBeenCalledWith(2);
  });

  it("renders no quote for a message with no replyToMessageId", () => {
    renderList();

    expect(screen.queryByText("Message unavailable")).not.toBeInTheDocument();
  });

  it("renders the resolved author/snippet quote for a reply whose original is loaded", () => {
    renderList({
      messages: [
        ...kMessages,
        { id: 3, author: "alice", body: "replying", sentAt: "t", replyToMessageId: 2, reactions: [] },
      ],
    });

    const replyItem = screen.getAllByRole("listitem")[2]!;
    expect(replyItem).toHaveTextContent("bob");
    expect(replyItem).toHaveTextContent("hi from bob");
  });

  it("renders 'Message unavailable' for a reply whose original isn't in the currently loaded history", () => {
    renderList({
      messages: [
        { id: 3, author: "alice", body: "replying", sentAt: "t", replyToMessageId: 999, reactions: [] },
      ],
    });

    expect(screen.getByText("Message unavailable")).toBeInTheDocument();
  });

  it("truncates a long original body in the quote", () => {
    const longBody = "x".repeat(80);
    renderList({
      messages: [
        { id: 1, author: "alice", body: longBody, sentAt: "t", reactions: [] },
        { id: 2, author: "bob", body: "replying", sentAt: "t", replyToMessageId: 1, reactions: [] },
      ],
    });

    const replyItem = screen.getAllByRole("listitem")[1]!;
    expect(replyItem).toHaveTextContent(`${"x".repeat(60)}…`);
    expect(replyItem).not.toHaveTextContent(longBody);
  });

  it("renders no reaction chips for a message with none", () => {
    renderList();

    expect(screen.queryByTitle("bob")).not.toBeInTheDocument();
  });

  it("renders a chip per emoji with its count, and hovering shows the voters", () => {
    renderList({
      messages: [{ ...kMessages[0]!, reactions: [{ emoji: "👍", logins: ["bob", "carol"] }] }],
    });

    const chip = screen.getByRole("button", { name: "👍 2" });
    expect(chip).toHaveAttribute("title", "bob, carol");
  });

  it("clicking an existing chip toggles that reaction", async () => {
    const onToggleReaction = vi.fn();
    renderList({
      messages: [{ ...kMessages[0]!, reactions: [{ emoji: "👍", logins: ["bob"] }] }],
      onToggleReaction,
    });

    await userEvent.click(screen.getByRole("button", { name: "👍 1" }));

    expect(onToggleReaction).toHaveBeenCalledWith(1, "👍");
  });

  it("clicking React opens a fixed emoji picker, and picking one toggles it and closes the picker", async () => {
    const onToggleReaction = vi.fn();
    renderList({ onToggleReaction });

    await userEvent.click(screen.getAllByRole("button", { name: "React" })[0]!);
    await userEvent.click(screen.getByRole("button", { name: "❤️" }));

    expect(onToggleReaction).toHaveBeenCalledWith(1, "❤️");
    expect(screen.queryByRole("button", { name: "❤️" })).not.toBeInTheDocument();
  });

  it("does not show a Pin/Unpin button for a non-moderator, even on their own message", () => {
    renderList();

    const aliceItem = screen.getAllByRole("listitem")[0]!;
    const buttonNames = Array.from(aliceItem.querySelectorAll("button")).map((b) => b.textContent);
    expect(buttonNames).not.toContain("Pin");
    expect(buttonNames).not.toContain("Unpin");
  });

  it("shows a 'Pinned' badge for a message in pinnedIds, regardless of role", () => {
    renderList({ pinnedIds: new Set([2]) });

    expect(screen.getByText("📌 Pinned")).toBeInTheDocument();
    const aliceItem = screen.getAllByRole("listitem")[0]!;
    expect(aliceItem).not.toHaveTextContent("📌 Pinned");
  });

  it("calls onPin for an unpinned message and onUnpin for an already-pinned one", async () => {
    const onPin = vi.fn();
    const onUnpin = vi.fn();
    renderList({ isModerator: true, pinnedIds: new Set([2]), onPin, onUnpin });

    await userEvent.click(screen.getByRole("button", { name: "Pin" }));
    expect(onPin).toHaveBeenCalledWith(1);

    await userEvent.click(screen.getByRole("button", { name: "Unpin" }));
    expect(onUnpin).toHaveBeenCalledWith(2);
  });

  // Issue #307 — MessageBody's own split logic is unit-tested directly
  // in MessageBody.test.tsx; this only checks the wiring, and that
  // editing prefills the plain, unwrapped body (message.body is never
  // mutated to begin with — MessageBody only builds React nodes at
  // render time — but this pins that down explicitly).
  it("highlights a mention, and editing still prefills the plain unwrapped body", async () => {
    const messagesWithMention = [
      { id: 1, author: "alice", body: "hi @bob", sentAt: "2026-01-01T00:00:00Z", reactions: [] },
    ];
    render(
      <MessageList
        messages={messagesWithMention}
        editedIds={new Set()}
        pinnedIds={new Set()}
        currentLogin="alice"
        isModerator={false}
        onEdit={vi.fn()}
        onDelete={vi.fn()}
        onReply={vi.fn()}
        onToggleReaction={vi.fn()}
        onPin={vi.fn()}
        onUnpin={vi.fn()}
      />,
    );

    const item = screen.getAllByRole("listitem")[0]!;
    // The author's own <strong> is the first one — the mention is the second.
    expect(item.querySelectorAll("strong")[1]?.textContent).toBe("@bob");

    await userEvent.click(screen.getByRole("button", { name: "Edit" }));
    expect(screen.getByRole("textbox")).toHaveValue("hi @bob");
  });
});
