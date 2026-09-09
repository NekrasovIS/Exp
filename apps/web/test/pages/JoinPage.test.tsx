import { render, screen } from "@testing-library/react";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import { JoinPage } from "../../src/pages/JoinPage.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";
const kPendingInviteKey = "devicehub.web.pendingInviteCode";

function renderPage(path = "/join/ABCD1234EF") {
  render(
    <MemoryRouter initialEntries={[path]}>
      <SessionProvider>
        <Routes>
          <Route path="/join/:code" element={<JoinPage />} />
          <Route path="/login" element={<p>login screen</p>} />
          <Route path="/" element={<p>home screen</p>} />
        </Routes>
      </SessionProvider>
    </MemoryRouter>,
  );
}

function seedSession(login = "alice"): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({
      token: fakeToken(login),
      refreshToken: "refresh-token",
      expiresAt: Math.floor(Date.now() / 1000) + 3600,
    }),
  );
}

beforeEach(() => {
  localStorage.clear();
  sessionStorage.clear();
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("JoinPage", () => {
  it("stashes the code and redirects to /login when not signed in", () => {
    renderPage();

    expect(screen.getByText("login screen")).toBeInTheDocument();
    expect(sessionStorage.getItem(kPendingInviteKey)).toBe("ABCD1234EF");
  });

  it("joins the community and redirects home when already signed in", async () => {
    seedSession();
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, [])) // useCommunities()'s own initial refresh()
      .mockResolvedValueOnce(jsonResponse(200, { id: 1, name: "Robotics Club" })) // join-by-code
      .mockResolvedValueOnce(jsonResponse(200, [{ id: 1, name: "Robotics Club", owner: "bob" }])); // refresh() after joining
    vi.stubGlobal("fetch", fetchSpy);

    renderPage();

    expect(await screen.findByText("home screen")).toBeInTheDocument();
    expect(fetchSpy).toHaveBeenCalledTimes(3);
    expect(JSON.parse((fetchSpy.mock.calls[1] as [string, RequestInit])[1].body as string)).toEqual({
      code: "ABCD1234EF",
    });
  });

  it("shows an error and stays put when the invite code is invalid", async () => {
    seedSession();
    vi.stubGlobal(
      "fetch",
      vi
        .fn()
        .mockResolvedValueOnce(jsonResponse(200, []))
        .mockResolvedValueOnce(jsonResponse(404, { error: "no such community" })),
    );

    renderPage();

    expect(await screen.findByRole("alert")).toHaveTextContent(/doesn't work anymore/i);
    expect(screen.queryByText("home screen")).not.toBeInTheDocument();
  });
});
