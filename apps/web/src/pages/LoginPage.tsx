// Issue #265 — composes the OTP and password/register forms and
// redirects to the app once a session exists, mirroring DeviceHub's
// LoginWindow: one-time code is the default, password/register is a
// step away behind a link.

import { useState } from "react";
import { Navigate } from "react-router-dom";

import { OtpLoginForm } from "../auth/OtpLoginForm.js";
import { PasswordAuthForm } from "../auth/PasswordAuthForm.js";
import { useSession } from "../session/SessionContext.js";

type View = "otp" | "password";

export function LoginPage() {
  const { isAuthenticated } = useSession();
  const [view, setView] = useState<View>("otp");

  if (isAuthenticated) {
    return <Navigate to="/" replace />;
  }

  return (
    <main>
      <h1>DeviceHub</h1>
      {view === "otp" ? (
        <>
          <OtpLoginForm />
          <button type="button" onClick={() => setView("password")}>
            Sign in with password instead
          </button>
        </>
      ) : (
        <>
          <PasswordAuthForm />
          <button type="button" onClick={() => setView("otp")}>
            Sign in with a one-time code instead
          </button>
        </>
      )}
    </main>
  );
}
