import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { MentionSuggestions } from "../../src/chat/MentionSuggestions.js";

describe("MentionSuggestions", () => {
  it("renders nothing when there are no suggestions", () => {
    const { container } = render(<MentionSuggestions suggestions={[]} activeIndex={0} onSelect={vi.fn()} />);

    expect(container).toBeEmptyDOMElement();
  });

  it("renders each suggestion prefixed with '@', highlighting the active one", () => {
    render(<MentionSuggestions suggestions={["alice", "bob"]} activeIndex={1} onSelect={vi.fn()} />);

    expect(screen.getByRole("button", { name: "@alice" })).toBeInTheDocument();
    const activeButton = screen.getByRole("button", { name: "@bob" });
    expect(activeButton.className).toMatch(/itemActive/);
  });

  it("clicking a suggestion calls onSelect with its login", async () => {
    const onSelect = vi.fn();
    render(<MentionSuggestions suggestions={["alice", "bob"]} activeIndex={0} onSelect={onSelect} />);

    await userEvent.click(screen.getByRole("button", { name: "@bob" }));

    expect(onSelect).toHaveBeenCalledWith("bob");
  });
});
