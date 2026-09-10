import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { MessageList } from "../../src/chat/MessageList.js";

const kMessages = [
  { id: 1, author: "alice", body: "hi from alice", sentAt: "2026-01-01T00:00:00Z" },
  { id: 2, author: "bob", body: "hi from bob", sentAt: "2026-01-01T00:01:00Z" },
];

function renderList(overrides: Partial<Parameters<typeof MessageList>[0]> = {}) {
  return render(
    <MessageList
      messages={kMessages}
      editedIds={new Set()}
      currentLogin="alice"
      isModerator={false}
      onEdit={vi.fn()}
      onDelete={vi.fn()}
      onReply={vi.fn()}
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
    expect(aliceButtonNames).toEqual(["Reply", "Edit", "Delete"]);
    // bob's message: Reply is always available, but no Edit/Delete for
    // alice (not the author, not a moderator).
    const bobButtonNames = Array.from(items[1]!.querySelectorAll("button")).map((b) => b.textContent);
    expect(bobButtonNames).toEqual(["Reply"]);
  });

  it("shows Delete (but not Edit) on someone else's message for a moderator", () => {
    renderList({ isModerator: true });

    const bobItem = screen.getAllByRole("listitem")[1]!;
    expect(bobItem).toHaveTextContent("hi from bob");
    const buttonNames = Array.from(bobItem.querySelectorAll("button")).map((b) => b.textContent);
    expect(buttonNames).toEqual(["Reply", "Delete"]);
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
    await userEvent.click(bobItem.querySelector("button")!);

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
        { id: 3, author: "alice", body: "replying", sentAt: "t", replyToMessageId: 2 },
      ],
    });

    const replyItem = screen.getAllByRole("listitem")[2]!;
    expect(replyItem).toHaveTextContent("bob");
    expect(replyItem).toHaveTextContent("hi from bob");
  });

  it("renders 'Message unavailable' for a reply whose original isn't in the currently loaded history", () => {
    renderList({
      messages: [{ id: 3, author: "alice", body: "replying", sentAt: "t", replyToMessageId: 999 }],
    });

    expect(screen.getByText("Message unavailable")).toBeInTheDocument();
  });

  it("truncates a long original body in the quote", () => {
    const longBody = "x".repeat(80);
    renderList({
      messages: [
        { id: 1, author: "alice", body: longBody, sentAt: "t" },
        { id: 2, author: "bob", body: "replying", sentAt: "t", replyToMessageId: 1 },
      ],
    });

    const replyItem = screen.getAllByRole("listitem")[1]!;
    expect(replyItem).toHaveTextContent(`${"x".repeat(60)}…`);
    expect(replyItem).not.toHaveTextContent(longBody);
  });
});
