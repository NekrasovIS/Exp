// Issue #391 — renders the `otpauth://` URI from setupTotp() as a
// scannable QR code, client-side (no image round-trips through any
// server). `qrcode`'s toString({type: "svg"}) returns plain markup, not
// a full document, so it can be inlined directly.

import QRCode from "qrcode";
import { useEffect, useState } from "react";

import styles from "./TotpQrCode.module.css";

interface TotpQrCodeProps {
  otpauthUrl: string;
}

export function TotpQrCode({ otpauthUrl }: TotpQrCodeProps) {
  const [svgMarkup, setSvgMarkup] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    setSvgMarkup(null);
    void QRCode.toString(otpauthUrl, { type: "svg", margin: 1 }).then((markup) => {
      if (!cancelled) {
        setSvgMarkup(markup);
      }
    });
    return () => {
      cancelled = true;
    };
  }, [otpauthUrl]);

  if (svgMarkup === null) {
    return <p className={styles.placeholder}>Generating QR code…</p>;
  }

  // The markup comes from qrcode's own encoder, run locally against a
  // URL we just requested from user-service ourselves — not
  // externally-supplied HTML, so this doesn't carry injection risk.
  return (
    <div
      className={styles.qr}
      role="img"
      aria-label="QR code for your authenticator app"
      dangerouslySetInnerHTML={{ __html: svgMarkup }}
    />
  );
}
