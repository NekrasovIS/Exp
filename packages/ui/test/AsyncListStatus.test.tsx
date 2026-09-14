import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { AsyncListStatus } from "../src/AsyncListStatus.js";

describe("AsyncListStatus", () => {
  it("renders nothing when not loading and there is no error", () => {
    const { container } = render(
      <AsyncListStatus loading={false} error={null} loadingText="Loading things…" />,
    );

    expect(container).toBeEmptyDOMElement();
  });

  it("shows the loading text while loading", () => {
    render(<AsyncListStatus loading={true} error={null} loadingText="Loading things…" />);

    expect(screen.getByText("Loading things…")).toBeInTheDocument();
  });

  it("shows the error message as an alert", () => {
    render(<AsyncListStatus loading={false} error="Something went wrong." loadingText="Loading things…" />);

    expect(screen.getByRole("alert")).toHaveTextContent("Something went wrong.");
  });

  it("renders both independently when loading and error are both set", () => {
    render(<AsyncListStatus loading={true} error="Stale data" loadingText="Loading things…" />);

    expect(screen.getByText("Loading things…")).toBeInTheDocument();
    expect(screen.getByRole("alert")).toHaveTextContent("Stale data");
  });
});
