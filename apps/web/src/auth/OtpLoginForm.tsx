// One-time-code sign-in (issue #265) — mirrors DeviceHub's LoginWindow
// default flow: identifier (login/email/Telegram chat id) -> request a
// code -> enter the code -> sign in. Requires the account to already
// have an email and/or Telegram chat id set (same precondition as the
// desktop client); there's no passwordless registration.

import { AuthClient } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import { describeAuthError } from "./describeAuthError.js";
import { authServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

type Step = "identifier" | "code";

export function OtpLoginForm() {
  const { signIn } = useSession();
  const authClient = useMemo(() => new AuthClient(authServiceUrl), []);

  const [step, setStep] = useState<Step>("identifier");
  const [identifier, setIdentifier] = useState("");
  const [code, setCode] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  async function handleRequestCode(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (identifier.trim() === "") {
      setError("Enter your login, email, or Telegram chat ID first.");
      return;
    }
    setError(null);
    setSubmitting(true);
    try {
      await authClient.requestOtp(identifier.trim());
      setStep("code");
    } catch (err) {
      setError(describeAuthError(err));
    } finally {
      setSubmitting(false);
    }
  }

  async function handleVerifyCode(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (code.trim() === "") {
      setError("Enter the code we sent you.");
      return;
    }
    setError(null);
    setSubmitting(true);
    try {
      const tokens = await authClient.verifyOtp(identifier.trim(), code.trim());
      signIn(tokens);
    } catch (err) {
      setError(describeAuthError(err));
    } finally {
      setSubmitting(false);
    }
  }

  if (step === "code") {
    return (
      <form onSubmit={handleVerifyCode}>
        <p>We sent a code to the channel linked to your account.</p>
        <label htmlFor="otp-code">6-digit code</label>
        <input id="otp-code" value={code} onChange={(event) => setCode(event.target.value)} autoFocus />
        {error !== null && <p role="alert">{error}</p>}
        <button type="submit" disabled={submitting}>
          Verify
        </button>
        <button type="button" onClick={() => setStep("identifier")}>
          Back
        </button>
      </form>
    );
  }

  return (
    <form onSubmit={handleRequestCode}>
      <label htmlFor="otp-identifier">Login, email, or Telegram chat ID</label>
      <input
        id="otp-identifier"
        value={identifier}
        onChange={(event) => setIdentifier(event.target.value)}
        autoFocus
      />
      {error !== null && <p role="alert">{error}</p>}
      <button type="submit" disabled={submitting}>
        Send code
      </button>
    </form>
  );
}
