// Issue #385 — the web client's first profile-editing screen (it had
// none at all before this): display name + avatar, mirroring
// DeviceHub's ProfileDialog scope-for-scope (email/telegram_chat_id
// stay desktop-only for now, same as this issue's own description).

import { useEffect, useState } from "react";
import { Link } from "react-router-dom";

import styles from "./ProfilePage.module.css";
import { Avatar } from "./Avatar.js";
import { useProfile } from "./useProfile.js";
import { useSession } from "../session/SessionContext.js";

export function ProfilePage() {
  const { currentLogin } = useSession();
  const { profile, loading, saving, error, avatarVersion, updateDisplayName, uploadAvatar } = useProfile();
  const [displayNameDraft, setDisplayNameDraft] = useState("");

  // Only overwrite the draft when the profile itself changes (initial
  // load, or after a save round-trips a new value) — not on every
  // render, or the user could never type ahead of a still-in-flight
  // save.
  useEffect(() => {
    setDisplayNameDraft(profile?.displayName ?? "");
  }, [profile]);

  function handleAvatarSelected(event: React.ChangeEvent<HTMLInputElement>): void {
    const file = event.target.files?.[0];
    event.target.value = "";
    if (file !== undefined) {
      void uploadAvatar(file);
    }
  }

  function handleSubmit(event: React.FormEvent): void {
    event.preventDefault();
    void updateDisplayName(displayNameDraft.trim());
  }

  return (
    <main className={styles.page}>
      <div className={styles.card}>
        <Link to="/" className={styles.backLink}>
          ← Back
        </Link>
        <h1 className={styles.title}>Your profile</h1>
        {error !== null && <p role="alert">{error}</p>}
        {loading && <p className={styles.mutedText}>Loading your profile…</p>}
        {!loading && currentLogin !== null && (
          <>
            <div className={styles.avatarRow}>
              <span className={styles.avatar}>
                <Avatar login={currentLogin} versionKey={avatarVersion} />
              </span>
              <div>
                <label htmlFor="profile-avatar" className={styles.avatarLabel}>
                  Change avatar
                </label>
                <input
                  id="profile-avatar"
                  type="file"
                  accept="image/*"
                  onChange={handleAvatarSelected}
                  disabled={saving}
                />
              </div>
            </div>
            <form onSubmit={handleSubmit} className={styles.form}>
              <label htmlFor="profile-display-name">Display name</label>
              <input
                id="profile-display-name"
                value={displayNameDraft}
                onChange={(event) => setDisplayNameDraft(event.target.value)}
              />
              <button type="submit" disabled={saving}>
                Save
              </button>
            </form>
          </>
        )}
      </div>
    </main>
  );
}
