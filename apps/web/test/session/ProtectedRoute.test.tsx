import { render, screen } from "@testing-library/react";
import { beforeEach, describe, expect, it } from "vitest";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import { ProtectedRoute } from "../../src/session/ProtectedRoute.js";
import { SessionProvider } from "../../src/session/SessionContext.js";

const kStorageKey = "devicehub.web.session";

function renderAt(path: string) {
  render(
    <MemoryRouter initialEntries={[path]}>
      <SessionProvider>
        <Routes>
          <Route path="/login" element={<p>login screen</p>} />
          <Route element={<ProtectedRoute />}>
            <Route path="/" element={<p>home screen</p>} />
          </Route>
        </Routes>
      </SessionProvider>
    </MemoryRouter>,
  );
}

beforeEach(() => {
  localStorage.clear();
});

describe("ProtectedRoute", () => {
  it("redirects to /login when there is no session", () => {
    renderAt("/");
    expect(screen.getByText("login screen")).toBeInTheDocument();
    expect(screen.queryByText("home screen")).not.toBeInTheDocument();
  });

  it("renders the protected screen when a session is present", () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({
        token: "access-token",
        refreshToken: "refresh-token",
        expiresAt: Math.floor(Date.now() / 1000) + 3600,
      }),
    );

    renderAt("/");
    expect(screen.getByText("home screen")).toBeInTheDocument();
    expect(screen.queryByText("login screen")).not.toBeInTheDocument();
  });
});
