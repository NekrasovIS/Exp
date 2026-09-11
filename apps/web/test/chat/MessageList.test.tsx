import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { MessageList } from "../../src/chat/MessageList.js";

const kMessages = [
  { id: 1, author: "alice", body: "hi from alice", sentAt: "2026-01-01T00:00:00Z" },
  { id: 2, author: "bob", body: "hi from bob", sentAt: "2026-01-01T00:01:00Z" },
];

describe("MessageList", () => {
  it("shows Edit and Delete only on the current user's own message", () => {
    render(
      <MessageList
        messages={kMessages}
        editedIds={new Set()}
        currentLogin="alice"
        isModerator={false}
        onEdit={vi.fn()}
        onDelete={vi.fn()}
      />,
    );

    const items = screen.getAllByRole("listitem");
    expect(items[0]).toHaveTextContent("hi from alice");
    expect(items[0]!.querySelector("button")).not.toBeNull();
    // bob's message: no Edit/Delete for alice (not the author, not a moderator).
    expect(items[1]!.querySelectorAll("button")).toHaveLength(0);
  });

  it("shows Delete (but not Edit) on someone else's message for a moderator", () => {
    render(
      <MessageList
        messages={kMessages}
        editedIds={new Set()}
        currentLogin="alice"
        isModerator={true}
        onEdit={vi.fn()}
        onDelete={vi.fn()}
      />,
    );

    const bobItem = screen.getAllByRole("listitem")[1]!;
    expect(bobItem).toHaveTextContent("hi from bob");
    const buttonNames = Array.from(bobItem.querySelectorAll("button")).map((b) => b.textContent);
    expect(buttonNames).toEqual(["Delete"]);
  });

  it("marks an edited message and lets the author save a new body", async () => {
    const onEdit = vi.fn();
    render(
      <MessageList
        messages={kMessages}
        editedIds={new Set([1])}
        currentLogin="alice"
        isModerator={false}
        onEdit={onEdit}
        onDelete={vi.fn()}
      />,
    );

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
    render(
      <MessageList
        messages={kMessages}
        editedIds={new Set()}
        currentLogin="alice"
        isModerator={false}
        onEdit={vi.fn()}
        onDelete={onDelete}
      />,
    );

    await userEvent.click(screen.getByRole("button", { name: "Delete" }));
    expect(onDelete).toHaveBeenCalledWith(1);
  });

  // Issue #307 — MessageBody's own split logic is unit-tested directly
  // in MessageBody.test.tsx; this only checks the wiring, and that
  // editing prefills the plain, unwrapped body (message.body is never
  // mutated to begin with — MessageBody only builds React nodes at
  // render time — but this pins that down explicitly).
  it("highlights a mention, and editing still prefills the plain unwrapped body", async () => {
    const messagesWithMention = [{ id: 1, author: "alice", body: "hi @bob", sentAt: "2026-01-01T00:00:00Z" }];
    render(
      <MessageList
        messages={messagesWithMention}
        editedIds={new Set()}
        currentLogin="alice"
        isModerator={false}
        onEdit={vi.fn()}
        onDelete={vi.fn()}
      />,
    );

    const item = screen.getAllByRole("listitem")[0]!;
    // The author's own <strong> is the first one — the mention is the second.
    expect(item.querySelectorAll("strong")[1]?.textContent).toBe("@bob");

    await userEvent.click(screen.getByRole("button", { name: "Edit" }));
    expect(screen.getByRole("textbox")).toHaveValue("hi @bob");
  });
});
