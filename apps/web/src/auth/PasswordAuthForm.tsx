// Password sign-in and registration (issue #265) — one form, an
// internal toggle switches which server call the same login/password
// fields go to, mirroring DeviceHub's LoginWindow second step.

import { AuthClient, isTotpChallenge } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import styles from "./authForm.module.css";
import { describeApiError } from "../describeApiError.js";
import { TotpChallengeStep } from "./TotpChallengeStep.js";
import { authServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

type Mode = "signIn" | "register";

export function PasswordAuthForm() {
  const { signIn } = useSession();
  const authClient = useMemo(() => new AuthClient(authServiceUrl), []);

  const [mode, setMode] = useState<Mode>("signIn");
  const [login, setLogin] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);
  // Set only when the account has 2FA enabled (issue #389) — signIn()
  // is deferred until TotpChallengeStep resolves it with real tokens.
  const [pendingToken, setPendingToken] = useState<string | null>(null);

  async function handleSubmit(event: FormEvent): Promise<void> {
    event.preventDefault();
    if (login.trim() === "" || password === "") {
      setError("Enter both login and password.");
      return;
    }
    setError(null);
    setSubmitting(true);
    try {
      if (mode === "signIn") {
        const result = await authClient.requestToken(login.trim(), password);
        if (isTotpChallenge(result)) {
          setPendingToken(result.pendingToken);
        } else {
          signIn(result);
        }
      } else {
        const result = await authClient.register(login.trim(), password);
        if (!result.registered) {
          setError("That login is already taken.");
          return;
        }
        // Auto-login: a fresh registration also issues a token
        // immediately (AuthClient.register()'s own doc comment). A
        // fresh account can't have 2FA enabled yet, so this is always
        // a plain AuthTokens, never a TotpChallenge.
        if (result.tokens !== undefined) {
          signIn(result.tokens);
        }
      }
    } catch (err) {
      setError(describeApiError(err));
    } finally {
      setSubmitting(false);
    }
  }

  if (pendingToken !== null) {
    return (
      <TotpChallengeStep
        pendingToken={pendingToken}
        onVerified={signIn}
        onBack={() => setPendingToken(null)}
      />
    );
  }

  return (
    <form onSubmit={handleSubmit} className={styles.form}>
      <div className={styles.field}>
        <label htmlFor="password-auth-login">Login</label>
        <input
          id="password-auth-login"
          value={login}
          onChange={(event) => setLogin(event.target.value)}
          autoFocus
        />
      </div>
      <div className={styles.field}>
        <label htmlFor="password-auth-password">Password</label>
        <input
          id="password-auth-password"
          type="password"
          value={password}
          onChange={(event) => setPassword(event.target.value)}
        />
      </div>
      {error !== null && <p role="alert">{error}</p>}
      <button type="submit" className={styles.submitButton} disabled={submitting}>
        {mode === "signIn" ? "Sign in" : "Create account"}
      </button>
      <button
        type="button"
        className={styles.secondaryButton}
        onClick={() => setMode(mode === "signIn" ? "register" : "signIn")}
      >
        {mode === "signIn" ? "Create an account instead" : "Sign in instead"}
      </button>
    </form>
  );
}
