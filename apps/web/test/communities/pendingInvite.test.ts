import { beforeEach, describe, expect, it } from "vitest";

import { setPendingInviteCode, takePendingInviteCode } from "../../src/communities/pendingInvite.js";

beforeEach(() => {
  sessionStorage.clear();
});

describe("pendingInvite", () => {
  it("returns null when nothing was stashed", () => {
    expect(takePendingInviteCode()).toBeNull();
  });

  it("returns the stashed code once, then null", () => {
    setPendingInviteCode("ABCD1234EF");

    expect(takePendingInviteCode()).toBe("ABCD1234EF");
    expect(takePendingInviteCode()).toBeNull();
  });
});
