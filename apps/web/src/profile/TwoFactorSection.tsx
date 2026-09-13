// Issue #391 — profile-page section for TOTP-based two-factor
// authentication (issue #388 backend, #389 login-flow integration).
// Four mutually-exclusive views over the same section, gated by
// useTotp()'s state: loading -> disabled|enabled, disabled -> (Enable)
// -> pending setup -> (Confirm) -> one-time backup codes -> back to
// enabled.

import { useState, type FormEvent } from "react";

import styles from "./TwoFactorSection.module.css";
import { TotpQrCode } from "./TotpQrCode.js";
import { useTotp } from "./useTotp.js";

export function TwoFactorSection() {
  const {
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
  } = useTotp();
  const [confirmCode, setConfirmCode] = useState("");
  const [disableCode, setDisableCode] = useState("");

  function handleConfirm(event: FormEvent): void {
    event.preventDefault();
    void confirmSetup(confirmCode.trim());
    setConfirmCode("");
  }

  function handleDisable(event: FormEvent): void {
    event.preventDefault();
    void disable(disableCode.trim());
    setDisableCode("");
  }

  return (
    <section className={styles.section}>
      <h2 className={styles.heading}>Two-factor authentication</h2>
      {error !== null && <p role="alert">{error}</p>}

      {backupCodes !== null ? (
        <div className={styles.backupCodes}>
          <p className={styles.warning}>
            Save these backup codes somewhere safe — each works once if you lose access to your authenticator
            app, and they won&apos;t be shown again.
          </p>
          <ul className={styles.codeList}>
            {backupCodes.map((code) => (
              <li key={code}>{code}</li>
            ))}
          </ul>
          <button type="button" onClick={dismissBackupCodes}>
            I&apos;ve saved these
          </button>
        </div>
      ) : setupInfo !== null ? (
        <form onSubmit={handleConfirm} className={styles.setupForm}>
          <TotpQrCode otpauthUrl={setupInfo.otpauthUrl} />
          <p className={styles.mutedText}>
            Scan this with your authenticator app, or enter the secret manually:{" "}
            <code className={styles.secret}>{setupInfo.secret}</code>
          </p>
          <label htmlFor="totp-confirm-code">Confirmation code</label>
          <input
            id="totp-confirm-code"
            value={confirmCode}
            onChange={(event) => setConfirmCode(event.target.value)}
            autoFocus
          />
          <div className={styles.buttonRow}>
            <button type="submit" disabled={working}>
              Confirm
            </button>
            <button type="button" onClick={cancelSetup} disabled={working}>
              Cancel
            </button>
          </div>
        </form>
      ) : enabled === null ? (
        <p className={styles.mutedText}>Loading two-factor status…</p>
      ) : enabled ? (
        <form onSubmit={handleDisable} className={styles.disableForm}>
          <p className={styles.mutedText}>Two-factor authentication is enabled for your account.</p>
          <label htmlFor="totp-disable-code">Current code, to disable</label>
          <input
            id="totp-disable-code"
            value={disableCode}
            onChange={(event) => setDisableCode(event.target.value)}
          />
          <button type="submit" disabled={working}>
            Disable
          </button>
        </form>
      ) : (
        <>
          <p className={styles.mutedText}>Two-factor authentication is currently disabled.</p>
          <button type="button" onClick={() => void beginSetup()} disabled={working}>
            Enable
          </button>
        </>
      )}
    </section>
  );
}
