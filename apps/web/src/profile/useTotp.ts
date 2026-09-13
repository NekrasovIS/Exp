// Issue #388/#391 — the logged-in user's own two-factor (TOTP) status
// and setup/disable flow, mirroring useProfile.ts's shape (own hook per
// profile-page section, backed directly by @devicehub/core's client).

import { UserServiceClient } from "@devicehub/core";
import type { TotpSetupInfo } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { describeApiError } from "../describeApiError.js";
import { userServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export function useTotp() {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new UserServiceClient(userServiceUrl), []);

  // null while the initial GET /profile/totp/status is in flight.
  const [enabled, setEnabled] = useState<boolean | null>(null);
  // Non-null between beginSetup() and either confirmSetup() succeeding
  // or cancelSetup() — nothing is enabled server-side yet at this point.
  const [setupInfo, setSetupInfo] = useState<TotpSetupInfo | null>(null);
  // Shown exactly once, right after confirmSetup() succeeds — see
  // UserServiceClient.confirmTotp()'s own doc comment.
  const [backupCodes, setBackupCodes] = useState<string[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [working, setWorking] = useState(false);

  const refresh = useCallback(async () => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    try {
      setEnabled(await client.isTotpEnabled(token));
    } catch (err) {
      setError(describeApiError(err));
    }
  }, [client, getAccessToken]);

  useEffect(() => {
    void refresh();
  }, [refresh]);

  const beginSetup = useCallback(async () => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    setWorking(true);
    setError(null);
    try {
      setSetupInfo(await client.setupTotp(token));
    } catch (err) {
      setError(describeApiError(err));
    } finally {
      setWorking(false);
    }
  }, [client, getAccessToken]);

  // No server call — nothing was persisted by setupTotp() until a
  // confirmTotp() call actually succeeds, so abandoning a pending setup
  // is purely local state.
  const cancelSetup = useCallback(() => {
    setSetupInfo(null);
  }, []);

  const confirmSetup = useCallback(
    async (code: string) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      setWorking(true);
      setError(null);
      try {
        const result = await client.confirmTotp(token, code);
        setBackupCodes(result.backupCodes);
        setSetupInfo(null);
        setEnabled(true);
      } catch (err) {
        setError(describeApiError(err));
      } finally {
        setWorking(false);
      }
    },
    [client, getAccessToken],
  );

  const dismissBackupCodes = useCallback(() => {
    setBackupCodes(null);
  }, []);

  const disable = useCallback(
    async (code: string) => {
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      setWorking(true);
      setError(null);
      try {
        await client.disableTotp(token, code);
        setEnabled(false);
      } catch (err) {
        setError(describeApiError(err));
      } finally {
        setWorking(false);
      }
    },
    [client, getAccessToken],
  );

  return {
    enabled,
    setupInfo,
    backupCodes,
    error,
    working,
    beginSetup,
    cancelSetup,
    confirmSetup,
    dismissBackupCodes,
    disable,
  };
}
