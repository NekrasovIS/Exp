// Password sign-in and registration (issue #265) — one form, an
// internal toggle switches which server call the same login/password
// fields go to, mirroring DeviceHub's LoginWindow second step.

import { AuthClient } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import { describeAuthError } from "./describeAuthError.js";
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
        const tokens = await authClient.requestToken(login.trim(), password);
        signIn(tokens);
      } else {
        const result = await authClient.register(login.trim(), password);
        if (!result.registered) {
          setError("That login is already taken.");
          return;
        }
        // Auto-login: a fresh registration also issues a token
        // immediately (AuthClient.register()'s own doc comment).
        if (result.tokens !== undefined) {
          signIn(result.tokens);
        }
      }
    } catch (err) {
      setError(describeAuthError(err));
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <form onSubmit={handleSubmit}>
      <label htmlFor="password-auth-login">Login</label>
      <input
        id="password-auth-login"
        value={login}
        onChange={(event) => setLogin(event.target.value)}
        autoFocus
      />
      <label htmlFor="password-auth-password">Password</label>
      <input
        id="password-auth-password"
        type="password"
        value={password}
        onChange={(event) => setPassword(event.target.value)}
      />
      {error !== null && <p role="alert">{error}</p>}
      <button type="submit" disabled={submitting}>
        {mode === "signIn" ? "Sign in" : "Create account"}
      </button>
      <button type="button" onClick={() => setMode(mode === "signIn" ? "register" : "signIn")}>
        {mode === "signIn" ? "Create an account instead" : "Sign in instead"}
      </button>
    </form>
  );
}
