import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { PasswordAuthForm } from "../../src/auth/PasswordAuthForm.js";
import { SessionProvider, useSession } from "../../src/session/SessionContext.js";
import { jsonResponse, routedFetch } from "../testUtils.js";

function Probe() {
  const { isAuthenticated } = useSession();
  return <p data-testid="probe">{isAuthenticated ? "signed-in" : "signed-out"}</p>;
}

function renderForm() {
  render(
    <SessionProvider>
      <PasswordAuthForm />
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

describe("PasswordAuthForm", () => {
  it("shows a validation error and makes no request when a field is empty", async () => {
    const fetchSpy = vi.fn();
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    expect(screen.getByRole("alert")).toHaveTextContent(/enter both login and password/i);
    expect(fetchSpy).not.toHaveBeenCalled();
  });

  it("signs in with valid credentials", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValue(jsonResponse(200, { token: "t1", refresh_token: "r1", expires_at: 9999999999 }));
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.type(screen.getByLabelText("Login"), "alice");
    await userEvent.type(screen.getByLabelText("Password"), "correct-password");
    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    expect(await screen.findByTestId("probe")).toHaveTextContent("signed-in");
  });

  it("shows the server's error message for wrong credentials", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(jsonResponse(401, { error: "invalid credentials" }));
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.type(screen.getByLabelText("Login"), "alice");
    await userEvent.type(screen.getByLabelText("Password"), "wrong-password");
    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("invalid credentials");
  });

  it("switches to register mode and reports an already-taken login without signing in", async () => {
    const fetchSpy = vi.fn().mockResolvedValue(jsonResponse(409, { registered: false }));
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.click(screen.getByRole("button", { name: "Create an account instead" }));
    await userEvent.type(screen.getByLabelText("Login"), "alice");
    await userEvent.type(screen.getByLabelText("Password"), "some-password");
    await userEvent.click(screen.getByRole("button", { name: "Create account" }));

    expect(await screen.findByRole("alert")).toHaveTextContent(/already taken/i);
    expect(screen.getByTestId("probe")).toHaveTextContent("signed-out");
  });

  it("challenges for a TOTP code when the account has 2FA enabled, then signs in on a correct code (issue #389)", async () => {
    const fetchSpy = vi.fn(
      routedFetch([
        [/\/auth\/token$/, () => jsonResponse(200, { totp_required: true, pending_token: "pending-1" })],
        [
          /\/auth\/totp\/verify$/,
          () => jsonResponse(200, { token: "t1", refresh_token: "r1", expires_at: 9999999999 }),
        ],
      ]),
    );
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.type(screen.getByLabelText("Login"), "alice");
    await userEvent.type(screen.getByLabelText("Password"), "correct-password");
    await userEvent.click(screen.getByRole("button", { name: "Sign in" }));

    const codeInput = await screen.findByLabelText("Authentication code");
    expect(screen.getByTestId("probe")).toHaveTextContent("signed-out");

    await userEvent.type(codeInput, "123456");
    await userEvent.click(screen.getByRole("button", { name: "Verify" }));

    expect(await screen.findByTestId("probe")).toHaveTextContent("signed-in");
    expect(fetchSpy).toHaveBeenCalledWith(
      expect.stringMatching(/\/auth\/totp\/verify$/),
      expect.objectContaining({ body: JSON.stringify({ pending_token: "pending-1", code: "123456" }) }),
    );
  });

  it("registering a fresh login signs in automatically", async () => {
    const fetchSpy = vi
      .fn()
      .mockResolvedValue(
        jsonResponse(201, { registered: true, token: "t1", refresh_token: "r1", expires_at: 9999999999 }),
      );
    vi.stubGlobal("fetch", fetchSpy);
    renderForm();

    await userEvent.click(screen.getByRole("button", { name: "Create an account instead" }));
    await userEvent.type(screen.getByLabelText("Login"), "brand-new-login");
    await userEvent.type(screen.getByLabelText("Password"), "some-password");
    await userEvent.click(screen.getByRole("button", { name: "Create account" }));

    expect(await screen.findByTestId("probe")).toHaveTextContent("signed-in");
  });
});
