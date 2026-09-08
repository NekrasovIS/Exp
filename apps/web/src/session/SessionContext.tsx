// App-shell session wiring (issue #264) — SessionManager from
// @devicehub/core holds tokens in memory and schedules background
// refresh, but deliberately never persists anything itself (see its own
// doc comment); this is the localStorage-backed home for that, plus the
// React state every screen reads to know whether it's signed in.

import { AuthClient, SessionManager } from "@devicehub/core";
import type { AuthTokens } from "@devicehub/core";
import { createContext, useContext, useMemo, useState, type ReactNode } from "react";

import { decodeTokenSubject } from "../auth/decodeTokenSubject.js";
import { authServiceUrl } from "../config.js";

const kStorageKey = "devicehub.web.session";

function loadStoredTokens(): AuthTokens | null {
  const raw = localStorage.getItem(kStorageKey);
  if (raw === null) {
    return null;
  }
  try {
    return JSON.parse(raw) as AuthTokens;
  } catch {
    // A corrupted value is no better than no session — fail closed by
    // dropping it rather than throwing during app startup.
    localStorage.removeItem(kStorageKey);
    return null;
  }
}

function storeTokens(tokens: AuthTokens | null): void {
  if (tokens === null) {
    localStorage.removeItem(kStorageKey);
  } else {
    localStorage.setItem(kStorageKey, JSON.stringify(tokens));
  }
}

interface SessionContextValue {
  /** True once a token pair is held — doesn't guarantee it's still
   * valid server-side (an expired refresh token surfaces later, via
   * onRefreshError below), only that the app has something to try. */
  isAuthenticated: boolean;
  getAccessToken: () => string | null;
  /** The signed-in user's own login, decoded from the token payload
   * (see decodeTokenSubject.ts) — display/UI-logic use only (e.g. "is
   * this my message"), never an auth decision. null when signed out. */
  currentLogin: string | null;
  /** Adopts a fresh token pair after login/register/OTP verification. */
  signIn: (tokens: AuthTokens) => void;
  signOut: () => void;
}

const SessionContext = createContext<SessionContextValue | null>(null);

export function SessionProvider({ children }: { children: ReactNode }) {
  // Bumped on every token change so the memo below actually
  // recomputes — without it in the dependency array, useMemo would
  // keep returning its first snapshot forever, since sessionManager
  // itself (the only other dependency) never changes identity. The
  // token itself is always read fresh from sessionManager.getAccessToken(),
  // never stored redundantly in this state.
  const [tokenVersion, bumpTokenVersion] = useState(0);

  const sessionManager = useMemo(() => {
    const authClient = new AuthClient(authServiceUrl);
    const manager = new SessionManager(authClient, {
      onTokensChanged: (tokens) => {
        storeTokens(tokens);
        bumpTokenVersion((n) => n + 1);
      },
      onRefreshError: () => {
        // The refresh token itself is no longer valid (expired/revoked)
        // — there's nothing left to retry, so drop the session and let
        // ProtectedRoute redirect to /login on the next render.
        manager.clear();
      },
    });
    const stored = loadStoredTokens();
    if (stored !== null) {
      manager.setTokens(stored);
    }
    return manager;
    // Constructed once — sessionManager is stable for the provider's lifetime.
  }, []);

  const value = useMemo<SessionContextValue>(() => {
    const accessToken = sessionManager.getAccessToken();
    return {
      isAuthenticated: accessToken !== null,
      getAccessToken: () => sessionManager.getAccessToken(),
      currentLogin: accessToken === null ? null : decodeTokenSubject(accessToken),
      signIn: (tokens) => sessionManager.setTokens(tokens),
      signOut: () => sessionManager.clear(),
    };
  }, [sessionManager, tokenVersion]);

  return <SessionContext.Provider value={value}>{children}</SessionContext.Provider>;
}

export function useSession(): SessionContextValue {
  const value = useContext(SessionContext);
  if (value === null) {
    throw new Error("useSession() must be called within a <SessionProvider>");
  }
  return value;
}
