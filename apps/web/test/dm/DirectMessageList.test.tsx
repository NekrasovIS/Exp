import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { DirectMessageList } from "../../src/dm/DirectMessageList.js";

const kMessages = [
  { id: 1, author: "alice", body: "hi from alice", sentAt: "2026-01-01T00:00:00Z" },
  { id: 2, author: "bob", body: "hi from bob", sentAt: "2026-01-01T00:01:00Z" },
];

describe("DirectMessageList", () => {
  it("renders every message's author and body", () => {
    render(
      <DirectMessageList
        messages={kMessages}
        currentLogin="alice"
        otherLogin="bob"
        readPointers={new Map()}
      />,
    );

    expect(screen.getByText("hi from alice")).toBeInTheDocument();
    expect(screen.getByText("hi from bob")).toBeInTheDocument();
  });

  it("shows 'Seen' on the current user's own message once the other participant's pointer reaches it", () => {
    render(
      <DirectMessageList
        messages={kMessages}
        currentLogin="alice"
        otherLogin="bob"
        readPointers={new Map([["bob", 1]])}
      />,
    );

    const items = screen.getAllByRole("listitem");
    expect(items[0]).toHaveTextContent("Seen");
  });

  it("does not show 'Seen' before the other participant's pointer reaches the message", () => {
    render(
      <DirectMessageList
        messages={kMessages}
        currentLogin="alice"
        otherLogin="bob"
        readPointers={new Map([["bob", 0]])}
      />,
    );

    expect(screen.queryByText("Seen")).not.toBeInTheDocument();
  });

  it("never shows 'Seen' on the other participant's own message", () => {
    render(
      <DirectMessageList
        messages={kMessages}
        currentLogin="alice"
        otherLogin="bob"
        readPointers={new Map([["bob", 99]])}
      />,
    );

    const bobItem = screen.getAllByRole("listitem")[1]!;
    expect(bobItem).not.toHaveTextContent("Seen");
  });
});
