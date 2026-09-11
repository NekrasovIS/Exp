import { render } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { MessageBody } from "../../src/chat/MessageBody.js";

describe("MessageBody", () => {
  it("wraps a single mention in a <strong>", () => {
    const { container } = render(<MessageBody text="hi @bob" />);

    expect(container.textContent).toBe("hi @bob");
    const strong = container.querySelector("strong");
    expect(strong?.textContent).toBe("@bob");
  });

  it("wraps multiple mentions, each its own element", () => {
    const { container } = render(<MessageBody text="@alice and @bob, take a look" />);

    const mentions = Array.from(container.querySelectorAll("strong")).map((el) => el.textContent);
    expect(mentions).toEqual(["@alice", "@bob"]);
  });

  it("renders plain text unchanged when there are no mentions", () => {
    const { container } = render(<MessageBody text="no mentions here" />);

    expect(container.textContent).toBe("no mentions here");
    expect(container.querySelector("strong")).toBeNull();
  });

  it("does not treat an email address as a mention", () => {
    const { container } = render(<MessageBody text="reach me at bob@example.com" />);

    expect(container.textContent).toBe("reach me at bob@example.com");
    expect(container.querySelector("strong")).toBeNull();
  });

  it("matches logins with digits, underscores and hyphens", () => {
    const { container } = render(<MessageBody text="@bob_2-test hi" />);

    expect(container.querySelector("strong")?.textContent).toBe("@bob_2-test");
  });
});
