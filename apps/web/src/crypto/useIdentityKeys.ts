// TypeScript port of src/user/IdentityKeyStore.h/.cpp (issue #269) — a
// per-login, persistent X25519 identity keypair. The secret half never
// leaves localStorage (there's no export/sync — losing it means losing
// access to everything wrapped for this identity, same known
// limitation as the C++ store, see its doc comment); the public half
// is republished on every load, deliberately unconditionally
// (idempotent — a PATCH that doesn't change the stored value is cheap
// and harmless, matching MainWindow.cpp's own comment on this).

import { UserServiceClient } from "@devicehub/core";
import { useEffect, useState } from "react";

import { getSodium } from "./sodium.js";
import { userServiceUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export interface IdentityKeys {
  ownPublicKey: Uint8Array;
  ownSecretKey: Uint8Array;
}

function storageKey(login: string): string {
  return `devicehub.web.identityKeys.${login}`;
}

interface StoredKeyPair {
  publicKey: string;
  secretKey: string;
}

async function loadOrGenerateKeyPair(login: string): Promise<IdentityKeys> {
  const sodium = await getSodium();
  const raw = localStorage.getItem(storageKey(login));
  if (raw !== null) {
    try {
      const stored = JSON.parse(raw) as StoredKeyPair;
      return {
        ownPublicKey: sodium.from_base64(stored.publicKey, sodium.base64_variants.ORIGINAL),
        ownSecretKey: sodium.from_base64(stored.secretKey, sodium.base64_variants.ORIGINAL),
      };
    } catch {
      // A corrupted entry is unrecoverable (the secret key inside it,
      // if any, isn't necessarily even valid) — fall through and
      // generate a fresh identity rather than throwing during app
      // startup.
    }
  }
  const keyPair = sodium.crypto_box_keypair();
  const stored: StoredKeyPair = {
    publicKey: sodium.to_base64(keyPair.publicKey, sodium.base64_variants.ORIGINAL),
    secretKey: sodium.to_base64(keyPair.privateKey, sodium.base64_variants.ORIGINAL),
  };
  localStorage.setItem(storageKey(login), JSON.stringify(stored));
  return { ownPublicKey: keyPair.publicKey, ownSecretKey: keyPair.privateKey };
}

/** Loads (or generates, on first use) this login's identity keypair
 * and republishes its public half — null while that's in flight, e.g.
 * during the first render right after sign-in. */
export function useIdentityKeys(): IdentityKeys | null {
  const { getAccessToken, currentLogin } = useSession();
  const [keys, setKeys] = useState<IdentityKeys | null>(null);

  useEffect(() => {
    setKeys(null);
    if (currentLogin === null) {
      return;
    }
    let cancelled = false;
    void (async () => {
      const loaded = await loadOrGenerateKeyPair(currentLogin);
      if (cancelled) {
        return;
      }
      setKeys(loaded);
      const token = getAccessToken();
      if (token === null) {
        return;
      }
      const sodium = await getSodium();
      const client = new UserServiceClient(userServiceUrl);
      try {
        await client.publishPublicKey(
          token,
          sodium.to_base64(loaded.ownPublicKey, sodium.base64_variants.ORIGINAL),
        );
      } catch {
        // Best-effort — a failed (re)publish here doesn't block using
        // the identity locally; it just means other members may not
        // find this login's key yet next time they set up a channel.
      }
    })();
    return () => {
      cancelled = true;
    };
  }, [currentLogin, getAccessToken]);

  return keys;
}
