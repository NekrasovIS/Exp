// Carries an invite code across the /login redirect (issue #301 — the
// "connect by link" gap: /join/<code> pre-fills the invite so it's
// applied automatically once the visitor signs in or registers,
// instead of requiring them to already have an account and paste the
// code into CommunitiesSidebar's own join form). sessionStorage, not
// React Router state — the visitor may register (a page navigation
// away from OtpLoginForm/PasswordAuthForm and back), which router
// state wouldn't survive but a same-tab storage key does.

const kStorageKey = "devicehub.web.pendingInviteCode";

export function setPendingInviteCode(code: string): void {
  sessionStorage.setItem(kStorageKey, code);
}

/** Reads and clears the pending code in one step — a stored code is
 * meant to be applied at most once, right after the next successful
 * sign-in, not re-applied on every later page load. */
export function takePendingInviteCode(): string | null {
  const code = sessionStorage.getItem(kStorageKey);
  if (code !== null) {
    sessionStorage.removeItem(kStorageKey);
  }
  return code;
}
