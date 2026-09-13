// Issue #385 — the logged-in user's own profile: display name editing
// and avatar upload, mirroring DeviceHub's ProfileDialog (which
// pre-fills all four ProfileEdits fields from the current profile
// before Save, so an edit to just one field doesn't blank the rest —
// see its own setProfile()/saveRequested()). updateOwnProfile() always
// sends all four fields (see its own doc comment), so
// updateDisplayName() below echoes back the other three unchanged.

import { UserServiceClient } from "@devicehub/core";
import type { UserProfile } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { userServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useProfile() {
  const { getAccessToken, currentLogin } = useSession();
  const client = useMemo(() => new UserServiceClient(userServiceUrl), []);

  const [profile, setProfile] = useState<UserProfile | null>(null);
  const [loading, setLoading] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState<string | null>(null);
  // Bumped on every successful upload — see Avatar's own `versionKey`
  // doc comment for why a fresh cache-busting value is needed here.
  const [avatarVersion, setAvatarVersion] = useState(0);

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null || currentLogin === null) {
      return;
    }
    setLoading(true);
    setError(null);
    try {
      setProfile(await client.fetchProfile(token, currentLogin));
    } catch {
      setError("Couldn't load your profile.");
    } finally {
      setLoading(false);
    }
  }, [client, getAccessToken, currentLogin]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const updateDisplayName = useCallback(
    async (displayName: string) => {
      const token = getAccessToken();
      if (token === null || profile === null) {
        return;
      }
      setSaving(true);
      setError(null);
      try {
        setProfile(
          await client.updateOwnProfile(token, {
            displayName,
            avatarUrl: profile.avatarUrl ?? "",
            email: profile.email ?? "",
            telegramChatId: profile.telegramChatId ?? "",
          }),
        );
      } catch {
        setError("Couldn't save your display name.");
      } finally {
        setSaving(false);
      }
    },
    [client, getAccessToken, profile],
  );

  const uploadAvatar = useCallback(
    async (file: File) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      setSaving(true);
      setError(null);
      try {
        const bytes = new Uint8Array(await file.arrayBuffer());
        await client.uploadAvatar(token, file.type, bytes);
        // The server already updated avatar_url as a side effect of the
        // upload (see UserServiceClient.uploadAvatar()'s own doc
        // comment) — refetch rather than guess its exact new value.
        await refresh();
        setAvatarVersion((version) => version + 1);
      } catch {
        setError("Couldn't upload that image.");
      } finally {
        setSaving(false);
      }
    },
    [client, getAccessToken, refresh],
  );

  return { profile, loading, saving, error, avatarVersion, updateDisplayName, uploadAvatar };
}
