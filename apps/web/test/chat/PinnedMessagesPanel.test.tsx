import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { PinnedMessagesPanel } from "../../src/chat/PinnedMessagesPanel.js";

describe("PinnedMessagesPanel", () => {
  it("shows a placeholder message when nothing is pinned", () => {
    render(<PinnedMessagesPanel pinned={[]} />);

    expect(screen.getByText("No pinned messages.")).toBeInTheDocument();
  });

  it("renders author, body, and who pinned it for each entry", () => {
    render(
      <PinnedMessagesPanel
        pinned={[
          {
            id: 1,
            author: "alice",
            body: "pin me",
            sentAt: "2026-01-01T00:00:00Z",
            pinnedBy: "bob",
            pinnedAt: "2026-01-01T00:05:00Z",
          },
        ]}
      />,
    );

    expect(screen.getByText("alice")).toBeInTheDocument();
    expect(screen.getByText(/pin me/)).toBeInTheDocument();
    expect(screen.getByText(/by bob/)).toBeInTheDocument();
  });

  it("scrolls the corresponding message into view when clicked", async () => {
    document.body.innerHTML = '<div id="message-1"></div>';
    const scrollIntoView = vi.fn();
    document.getElementById("message-1")!.scrollIntoView = scrollIntoView;

    render(
      <PinnedMessagesPanel
        pinned={[
          {
            id: 1,
            author: "alice",
            body: "pin me",
            sentAt: "2026-01-01T00:00:00Z",
            pinnedBy: "bob",
            pinnedAt: "t",
          },
        ]}
      />,
    );

    await userEvent.click(screen.getByRole("button"));

    expect(scrollIntoView).toHaveBeenCalled();
  });
});
