// Second step of a 2FA login (issue #389/#391) — shared by
// PasswordAuthForm and OtpLoginForm: both hand this component the
// pendingToken from a TotpChallenge and get an AuthTokens back on
// success, without duplicating this form twice.

import { AuthClient } from "@devicehub/core";
import type { AuthTokens } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import styles from "./authForm.module.css";
import { describeApiError } from "../describeApiError.js";
import { authServiceUrl } from "../config.js";

interface TotpChallengeStepProps {
  pendingToken: string;
  onVerified: (tokens: AuthTokens) => void;
  onBack: () => void;
}

export function TotpChallengeStep({ pendingToken, onVerified, onBack }: TotpChallengeStepProps) {
  const authClient = useMemo(() => new AuthClient(authServiceUrl), []);

  const [code, setCode] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

  async function handleSubmit(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (code.trim() === "") {
      setError("Enter your authenticator code.");
      return;
    }
    setError(null);
    setSubmitting(true);
    try {
      onVerified(await authClient.verifyTotp(pendingToken, code.trim()));
    } catch (err) {
      setError(describeApiError(err));
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <form onSubmit={handleSubmit} className={styles.form}>
      <p className={styles.hint}>
        Enter the 6-digit code from your authenticator app, or one of your backup codes.
      </p>
      <div className={styles.field}>
        <label htmlFor="totp-challenge-code">Authentication code</label>
        <input
          id="totp-challenge-code"
          value={code}
          onChange={(event) => setCode(event.target.value)}
          autoFocus
        />
      </div>
      {error !== null && <p role="alert">{error}</p>}
      <button type="submit" className={styles.submitButton} disabled={submitting}>
        Verify
      </button>
      <button type="button" className={styles.secondaryButton} onClick={onBack}>
        Back
      </button>
    </form>
  );
}
