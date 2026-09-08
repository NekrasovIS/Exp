import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { beforeEach, describe, expect, it } from "vitest";
import { MemoryRouter, Route, Routes } from "react-router-dom";

import { LoginPage } from "../../src/pages/LoginPage.js";
import { SessionProvider } from "../../src/session/SessionContext.js";

const kStorageKey = "devicehub.web.session";

function renderPage() {
  render(
    <MemoryRouter initialEntries={["/login"]}>
      <SessionProvider>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/" element={<p>home screen</p>} />
        </Routes>
      </SessionProvider>
    </MemoryRouter>,
  );
}

beforeEach(() => {
  localStorage.clear();
});

describe("LoginPage", () => {
  it("shows the one-time-code form by default", () => {
    renderPage();
    expect(screen.getByRole("button", { name: "Send code" })).toBeInTheDocument();
  });

  it("switches to the password form and back", async () => {
    renderPage();

    await userEvent.click(screen.getByRole("button", { name: "Sign in with password instead" }));
    expect(screen.getByRole("button", { name: "Sign in" })).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "Sign in with a one-time code instead" }));
    expect(screen.getByRole("button", { name: "Send code" })).toBeInTheDocument();
  });

  it("redirects to / when a session already exists", () => {
    localStorage.setItem(
      kStorageKey,
      JSON.stringify({
        token: "access-token",
        refreshToken: "refresh-token",
        expiresAt: Math.floor(Date.now() / 1000) + 3600,
      }),
    );

    renderPage();
    expect(screen.getByText("home screen")).toBeInTheDocument();
  });
});
