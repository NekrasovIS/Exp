import { render, screen } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";

import { MembersSidebar } from "../../src/members/MembersSidebar.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
  );
}

describe("MembersSidebar", () => {
  it("renders nothing when no community is selected", () => {
    seedSession();
    const { container } = render(
      <SessionProvider>
        <MembersSidebar communityId={null} onlineLogins={new Set()} />
      </SessionProvider>,
    );

    expect(container).toBeEmptyDOMElement();
  });

  it("lists members and marks online ones with a dot", async () => {
    seedSession();
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(jsonResponse(200, ["alice", "bob"])));

    render(
      <SessionProvider>
        <MembersSidebar communityId={1} onlineLogins={new Set(["bob"])} />
      </SessionProvider>,
    );

    expect(await screen.findByText("alice")).toBeInTheDocument();
    const bobRow = screen.getByText("bob").closest("li");
    const aliceRow = screen.getByText("alice").closest("li");
    expect(bobRow?.querySelector('[title="Online"]')).not.toBeNull();
    expect(aliceRow?.querySelector('[title="Online"]')).toBeNull();
    expect(screen.getByText("MEMBERS — 2")).toBeInTheDocument();
  });
});
