// Issue #412 — the "loading .../error" pair rendered above a list was
// hand-copied identically into ChannelsSidebar, CommunitiesSidebar,
// FriendsPanel, DmThreadsList, and MembersSidebar (only the loading
// text itself differs between them). Pure extraction, no behavior
// change: `loading` and `error` are still rendered independently of
// each other, exactly as each of the five call sites already did,
// rather than treating them as mutually exclusive.

import styles from "./AsyncListStatus.module.css";

interface AsyncListStatusProps {
  loading: boolean;
  error: string | null;
  loadingText: string;
}

export function AsyncListStatus({ loading, error, loadingText }: AsyncListStatusProps) {
  return (
    <>
      {loading && <p className={styles.mutedText}>{loadingText}</p>}
      {error !== null && <p role="alert">{error}</p>}
    </>
  );
}
