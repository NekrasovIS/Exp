import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { OtpLoginForm } from "../../src/auth/OtpLoginForm.js";
import { SessionProvider, useSession } from "../../src/session/SessionContext.js";
import { jsonResponse } from "../testUtils.js";

function Probe() {
  const { isAuthenticated } = useSession();
  return <p data-testid="probe">{isAuthenticated ? "signed-in" : "signed-out"}</p>;
}

function renderForm() {
  render(
    <SessionProvider>
      <OtpLoginForm />
      <Probe />
    </SessionProvider>,
  );
}

beforeEach(() => {
  localStorage.clear();
});

afterEach(() => {
  vi.unstubAllGlobals();
});

describe("OtpLoginForm", () => {
  it("shows a validation error and makes no request when the identifier is empty", async () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.click(screen.getByRole("button", { name: "Send code" }));

    expect(screen.getByRole("alert")).toHaveTextContent(/enter your login, email/i);
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it("requests a code, then verifies it and signs in", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, {}))
      .mockResolvedValueOnce(jsonResponse(200, { token: "t1", refresh_token: "r1", expires_at: 9999999999 }));
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.type(screen.getByLabelText(/login, email, or telegram/i), "alice");
    await userEvent.click(screen.getByRole("button", { name: "Send code" }));

    expect(await screen.findByLabelText(/6-digit code/i)).toBeInTheDocument();

    await userEvent.type(screen.getByLabelText(/6-digit code/i), "123456");
    await userEvent.click(screen.getByRole("button", { name: "Verify" }));

    expect(await screen.findByTestId("probe")).toHaveTextContent("signed-in");
  });

  it("shows the server's error message for an invalid code", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValueOnce(jsonResponse(200, {}))
      .mockResolvedValueOnce(jsonResponse(401, { error: "invalid or expired code" }));
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.type(screen.getByLabelText(/login, email, or telegram/i), "alice");
    await userEvent.click(screen.getByRole("button", { name: "Send code" }));
    await userEvent.type(await screen.findByLabelText(/6-digit code/i), "000000");
    await userEvent.click(screen.getByRole("button", { name: "Verify" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("invalid or expired code");
  });
});
