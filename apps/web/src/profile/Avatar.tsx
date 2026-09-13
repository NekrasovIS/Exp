// Issue #384/#385 — renders a user's avatar image, falling back to the
// existing letter-in-circle placeholder (MembersSidebar's own look,
// issue #322) when none is set. Deliberately doesn't take an avatarUrl
// prop: GET /users/{login}/avatar (issue #384) is public and
// deterministic from the login alone, and 404s cleanly when nothing was
// ever uploaded — the plain <img onError> fallback below handles that
// without a separate "does this user have an avatar" lookup.
//
// Only the image-or-letter content is rendered, not the circular
// container itself — callers keep their own sizing/positioning
// (MembersSidebar's `.avatar` span already owns the online-status dot
// as a sibling, which this component knows nothing about).

import { useState } from "react";

import styles from "./avatar.module.css";
import { userServiceUrl } from "../config.js";

interface AvatarProps {
  login: string;
  /** Appended as a `?v=` query param (issue #385) — the URL is
   * otherwise identical before and after a new upload (still just
   * `/users/<login>/avatar`), so ProfilePage's own preview of its own
   * fresh upload would otherwise keep showing the browser's cached
   * response for that same URL. Callers that never re-upload an
   * already-rendered avatar (MembersSidebar) can omit this. */
  versionKey?: string | number;
}

export function Avatar({ login, versionKey }: AvatarProps) {
  const [loadFailed, setLoadFailed] = useState(false);

  if (loadFailed) {
    return <>{login.slice(0, 1).toUpperCase()}</>;
  }

  const src = `${userServiceUrl}/users/${encodeURIComponent(login)}/avatar`;
  return (
    <img
      className={styles.image}
      src={versionKey === undefined ? src : `${src}?v=${encodeURIComponent(String(versionKey))}`}
      alt=""
      onError={() => setLoadFailed(true)}
    />
  );
}
