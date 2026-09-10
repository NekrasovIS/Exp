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
      pinnedIds={new Set()}
      currentLogin="alice"
      isModerator={false}
      onEdit={vi.fn()}
      onDelete={vi.fn()}
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
    expect(items[0]!.querySelector("button")).not.toBeNull();
    // bob's message: no Edit/Delete for alice (not the author, not a moderator).
    expect(items[1]!.querySelectorAll("button")).toHaveLength(0);
  });

  it("shows Delete and Pin (but not Edit) on someone else's message for a moderator", () => {
    renderList({ isModerator: true });

    const bobItem = screen.getAllByRole("listitem")[1]!;
    expect(bobItem).toHaveTextContent("hi from bob");
    const buttonNames = Array.from(bobItem.querySelectorAll("button")).map((b) => b.textContent);
    expect(buttonNames).toEqual(["Pin", "Delete"]);
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
});
