import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { MessageRow } from "../src/MessageRow.js";

describe("MessageRow", () => {
  it("renders the author and children inside the bubble", () => {
    render(
      <ul>
        <MessageRow isOwn={false} author="alice">
          <span>hello there</span>
        </MessageRow>
      </ul>,
    );

    expect(screen.getByText("alice")).toBeInTheDocument();
    expect(screen.getByText("hello there")).toBeInTheDocument();
  });

  it("sets the given id on the row for deep-link anchors", () => {
    render(
      <ul>
        <MessageRow id="message-42" isOwn={false} author="alice">
          <span>hi</span>
        </MessageRow>
      </ul>,
    );

    expect(document.getElementById("message-42")).toBeInTheDocument();
  });

  it("omits the id when not given", () => {
    const { container } = render(
      <ul>
        <MessageRow isOwn={false} author="alice">
          <span>hi</span>
        </MessageRow>
      </ul>,
    );

    expect(container.querySelector("li")).not.toHaveAttribute("id");
  });
});
