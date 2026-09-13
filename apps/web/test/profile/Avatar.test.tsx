import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { Avatar } from "../../src/profile/Avatar.js";
import { userServiceUrl } from "../../src/config.js";

describe("Avatar", () => {
  it("renders an <img> pointed at the deterministic per-login avatar URL", () => {
    // alt="" (decorative — the login is already shown alongside it by
    // every caller) maps to ARIA role "presentation", not "img", so
    // this queries the tag directly rather than via getByRole().
    const { container } = render(<Avatar login="alice" />);

    const img = container.querySelector("img");
    expect(img).toHaveAttribute("src", `${userServiceUrl}/users/alice/avatar`);
  });

  it("falls back to the first letter of the login when the image fails to load (no avatar set)", () => {
    const { container } = render(<Avatar login="bob" />);

    fireEvent.error(container.querySelector("img")!);

    expect(screen.getByText("B")).toBeInTheDocument();
    expect(container.querySelector("img")).toBeNull();
  });
});
