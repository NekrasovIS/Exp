import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it, vi } from "vitest";

import { QuickSwitcher, type QuickSwitcherItem } from "../../src/pages/QuickSwitcher.js";

function items(): QuickSwitcherItem[] {
  return [
    { key: "community:1", label: "Acme", group: "Community", onSelect: vi.fn() },
    { key: "channel:7", label: "general", group: "Channel", onSelect: vi.fn() },
    { key: "channel:8", label: "random", group: "Channel", onSelect: vi.fn() },
  ];
}

describe("QuickSwitcher", () => {
  it("renders every item with its group label", () => {
    render(<QuickSwitcher items={items()} onClose={vi.fn()} />);

    expect(screen.getByText("Acme")).toBeInTheDocument();
    expect(screen.getByText("general")).toBeInTheDocument();
    expect(screen.getByText("random")).toBeInTheDocument();
  });

  it("filters the list as the query changes, case-insensitively", async () => {
    render(<QuickSwitcher items={items()} onClose={vi.fn()} />);

    await userEvent.type(screen.getByRole("textbox"), "GEN");

    expect(screen.getByText("general")).toBeInTheDocument();
    expect(screen.queryByText("random")).not.toBeInTheDocument();
    expect(screen.queryByText("Acme")).not.toBeInTheDocument();
  });

  it("shows a no-matches message when nothing matches the query", async () => {
    render(<QuickSwitcher items={items()} onClose={vi.fn()} />);

    await userEvent.type(screen.getByRole("textbox"), "nope");

    expect(screen.getByText("No matches.")).toBeInTheDocument();
  });

  it("clicking an item selects it and closes the switcher", async () => {
    const onClose = vi.fn();
    const list = items();
    render(<QuickSwitcher items={list} onClose={onClose} />);

    await userEvent.click(screen.getByText("general"));

    expect(list[1]?.onSelect).toHaveBeenCalledTimes(1);
    expect(onClose).toHaveBeenCalledTimes(1);
  });

  it("Escape closes without selecting anything", async () => {
    const onClose = vi.fn();
    const list = items();
    render(<QuickSwitcher items={list} onClose={onClose} />);

    await userEvent.keyboard("{Escape}");

    expect(onClose).toHaveBeenCalledTimes(1);
    for (const item of list) {
      expect(item.onSelect).not.toHaveBeenCalled();
    }
  });

  it("ArrowDown then Enter selects the second item", async () => {
    const onClose = vi.fn();
    const list = items();
    render(<QuickSwitcher items={list} onClose={onClose} />);

    await userEvent.keyboard("{ArrowDown}{Enter}");

    expect(list[1]?.onSelect).toHaveBeenCalledTimes(1);
    expect(list[0]?.onSelect).not.toHaveBeenCalled();
  });

  it("Enter with the default (first) item selected picks that one", async () => {
    const onClose = vi.fn();
    const list = items();
    render(<QuickSwitcher items={list} onClose={onClose} />);

    await userEvent.keyboard("{Enter}");

    expect(list[0]?.onSelect).toHaveBeenCalledTimes(1);
  });
});
