// /join/:code (issue #301 — the "connect by link" gap this repo's own
// roadmap marker flagged as missing: the web client had no way to join
// a community except by manually pasting an invite code into
// CommunitiesSidebar's form after already being logged in).
//
// Two paths land here:
//  - Already signed in (bookmarked the link, or clicked it while
//    already logged in another tab) — this component joins directly
//    and leaves for "/" once done.
//  - Not signed in yet — stash the code and redirect to /login;
//    LoginPage's own "already authenticated -> /" redirect takes the
//    visitor straight to HomePage after sign-in/registration, never
//    back through this component, so the actual join for *this* path
//    happens in useCommunities()'s pending-invite effect once
//    CommunitiesMode mounts post-login (see its doc comment) — not
//    here.
//
// No anonymous/read-only access either way — matches the product
// decision to keep this a pre-fill for sign-in/registration, not a
// guest-viewing mode.

import { useEffect, useState } from "react";
import { useParams, Navigate } from "react-router-dom";

import { setPendingInviteCode } from "../communities/pendingInvite.js";
import { useCommunities } from "../communities/useCommunities.js";
import { useSession } from "../session/SessionContext.js";

export function JoinPage() {
  const { isAuthenticated } = useSession();
  const { code } = useParams<{ code: string }>();
  const { joinByCode } = useCommunities();
  const [status, setStatus] = useState<"joining" | "done" | "error">("joining");

  useEffect(() => {
    if (!isAuthenticated || code === undefined) {
      return;
    }
    let cancelled = false;
    void joinByCode(code).then(
      () => {
        if (!cancelled) setStatus("done");
      },
      () => {
        if (!cancelled) setStatus("error");
      },
    );
    return () => {
      cancelled = true;
    };
  }, [isAuthenticated, code, joinByCode]);

  if (code === undefined) {
    return <Navigate to="/" replace />;
  }
  if (!isAuthenticated) {
    setPendingInviteCode(code);
    return <Navigate to="/login" replace />;
  }
  if (status === "done") {
    return <Navigate to="/" replace />;
  }
  return (
    <main>
      <p>Joining…</p>
      {status === "error" && <p role="alert">That invite link doesn't work anymore.</p>}
    </main>
  );
}
