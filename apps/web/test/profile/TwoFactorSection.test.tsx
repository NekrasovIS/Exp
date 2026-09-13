import { render, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";

import { TwoFactorSection } from "../../src/profile/TwoFactorSection.js";
import { SessionProvider } from "../../src/session/SessionContext.js";
import { fakeToken, jsonResponse, routedFetch } from "../testUtils.js";

const kStorageKey = "devicehub.web.session";

function seedSession(): void {
  localStorage.setItem(
    kStorageKey,
    JSON.stringify({ token: fakeToken("alice"), refreshToken: "r1", expiresAt: 9999999999 }),
  );
}

function renderSection() {
  return render(
    <SessionProvider>
      <TwoFactorSection />
    </SessionProvider>,
  );
}

beforeEach(() => {
  seedSession();
});

afterEach(() => {
  vi.unstubAllGlobals();
  localStorage.clear();
});

describe("TwoFactorSection", () => {
  it("shows a disabled status and an Enable button when 2FA is off", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: false })]]),
    );

    renderSection();

    expect(await screen.findByText(/currently disabled/i)).toBeInTheDocument();
    expect(screen.getByRole("button", { name: "Enable" })).toBeInTheDocument();
  });

  it("shows an enabled status with a Disable form when 2FA is on", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([[/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: true })]]),
    );

    renderSection();

    expect(await screen.findByText(/is enabled for your account/i)).toBeInTheDocument();
    expect(screen.getByLabelText("Current code, to disable")).toBeInTheDocument();
  });

  it("full setup flow: Enable shows a QR code and secret, Confirm shows one-time backup codes", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: false })],
        [
          /\/profile\/totp\/setup$/,
          () =>
            jsonResponse(200, {
              secret: "JBSWY3DPEHPK3PXP",
              otpauth_url: "otpauth://totp/DeviceHub:alice?secret=JBSWY3DPEHPK3PXP",
            }),
        ],
        [/\/profile\/totp\/confirm$/, () => jsonResponse(200, { backup_codes: ["ABCD-2345", "EFGH-6789"] })],
      ]),
    );

    renderSection();
    await userEvent.click(await screen.findByRole("button", { name: "Enable" }));

    expect(await screen.findByText("JBSWY3DPEHPK3PXP")).toBeInTheDocument();
    expect(await screen.findByRole("img", { name: /qr code/i })).toBeInTheDocument();

    await userEvent.type(screen.getByLabelText("Confirmation code"), "123456");
    await userEvent.click(screen.getByRole("button", { name: "Confirm" }));

    expect(await screen.findByText("ABCD-2345")).toBeInTheDocument();
    expect(screen.getByText("EFGH-6789")).toBeInTheDocument();
    expect(screen.getByText(/won't be shown again/i)).toBeInTheDocument();

    await userEvent.click(screen.getByRole("button", { name: "I've saved these" }));

    expect(screen.queryByText("ABCD-2345")).not.toBeInTheDocument();
    expect(await screen.findByText(/is enabled for your account/i)).toBeInTheDocument();
  });

  it("Cancel abandons a pending setup without any confirm request", async () => {
    const fetchSpy = vi.fn(
      routedFetch([
        [/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: false })],
        [
          /\/profile\/totp\/setup$/,
          () => jsonResponse(200, { secret: "SECRET", otpauth_url: "otpauth://totp/x?secret=SECRET" }),
        ],
      ]),
    );
    vi.stubGlobal("fetch", fetchSpy);

    renderSection();
    await userEvent.click(await screen.findByRole("button", { name: "Enable" }));
    await screen.findByText("SECRET");
    await userEvent.click(screen.getByRole("button", { name: "Cancel" }));

    expect(await screen.findByRole("button", { name: "Enable" })).toBeInTheDocument();
    expect(fetchSpy).not.toHaveBeenCalledWith(
      expect.stringMatching(/\/profile\/totp\/confirm$/),
      expect.anything(),
    );
  });

  it("shows the server's error message on an invalid confirmation code", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: false })],
        [
          /\/profile\/totp\/setup$/,
          () => jsonResponse(200, { secret: "SECRET", otpauth_url: "otpauth://totp/x?secret=SECRET" }),
        ],
        [/\/profile\/totp\/confirm$/, () => jsonResponse(400, { error: "invalid code" })],
      ]),
    );

    renderSection();
    await userEvent.click(await screen.findByRole("button", { name: "Enable" }));
    await userEvent.type(await screen.findByLabelText("Confirmation code"), "000000");
    await userEvent.click(screen.getByRole("button", { name: "Confirm" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("invalid code");
  });

  it("shows the server's error message on an invalid disable code, and stays enabled", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: true })],
        [/\/profile\/totp\/disable$/, () => jsonResponse(400, { error: "invalid code" })],
      ]),
    );

    renderSection();
    await userEvent.type(await screen.findByLabelText("Current code, to disable"), "000000");
    await userEvent.click(screen.getByRole("button", { name: "Disable" }));

    expect(await screen.findByRole("alert")).toHaveTextContent("invalid code");
    expect(screen.getByText(/is enabled for your account/i)).toBeInTheDocument();
  });

  it("disabling with a correct code returns to the disabled status", async () => {
    vi.stubGlobal(
      "fetch",
      routedFetch([
        [/\/profile\/totp\/status$/, () => jsonResponse(200, { enabled: true })],
        [/\/profile\/totp\/disable$/, () => jsonResponse(200, { status: "disabled" })],
      ]),
    );

    renderSection();
    await userEvent.type(await screen.findByLabelText("Current code, to disable"), "123456");
    await userEvent.click(screen.getByRole("button", { name: "Disable" }));

    expect(await screen.findByText(/currently disabled/i)).toBeInTheDocument();
  });
});
