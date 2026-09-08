import { render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it } from "vitest";

import { SessionProvider, useSession } from "../../src/session/SessionContext.js";

const kStorageKey = "devicehub.web.session";

function futureTokens(overrides: Partial<{ token: string; refreshToken: string }> = {}) {
  return {
    token: overrides.token ?? "access-token",
    refreshToken: overrides.refreshToken ?? "refresh-token",
    // Far enough out that SessionManager's background refresh timer
    // never fires during a test run.
    expiresAt: Math.floor(Date.now() / 1000) + 3600,
  };
}

function Probe() {
  const { isAuthenticated, getAccessToken, currentLogin } = useSession();
  return (
    <p data-testid="probe">
      {isAuthenticated ? `signed-in:${getAccessToken()}:${currentLogin ?? "?"}` : "signed-out"}
    </p>
  );
}

function tokenFor(login: string): string {
  const payload = btoa(JSON.stringify({ sub: login, exp: 9999999999 }))
    .replaceAll("+", "-")
    .replaceAll("/", "_")
    .replace(/=+$/, "");
  return `${payload}.sig`;
}

beforeEach(() => {
  localStorage.clear();
});

describe("SessionProvider", () => {
  it("starts signed out when localStorage has no stored session", () => {
    render(
      <SessionProvider>
        <Probe />
      </SessionProvider>,
    );
    expect(screen.getByTestId("probe")).toHaveTextContent("signed-out");
  });

  it("restores a stored session from localStorage on mount", () => {
    localStorage.setItem(kStorageKey, JSON.stringify(futureTokens({ token: "stored-token" })));

    render(
      <SessionProvider>
        <Probe />
      </SessionProvider>,
    );
    expect(screen.getByTestId("probe")).toHaveTextContent("signed-in:stored-token:?");
  });

  it("exposes currentLogin decoded from the token payload", () => {
    localStorage.setItem(kStorageKey, JSON.stringify(futureTokens({ token: tokenFor("alice") })));

    render(
      <SessionProvider>
        <Probe />
      </SessionProvider>,
    );
    expect(screen.getByTestId("probe")).toHaveTextContent(`signed-in:${tokenFor("alice")}:alice`);
  });

  it("treats a corrupted stored value as no session, without throwing", () => {
    localStorage.setItem(kStorageKey, "{not json");

    render(
      <SessionProvider>
        <Probe />
      </SessionProvider>,
    );
    expect(screen.getByTestId("probe")).toHaveTextContent("signed-out");
    expect(localStorage.getItem(kStorageKey)).toBeNull();
  });
});
