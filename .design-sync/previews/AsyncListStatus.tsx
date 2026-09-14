import { AsyncListStatus } from "@devicehub/ui";

// Real call sites (apps/web/src/channels/ChannelsSidebar.tsx,
// members/MembersSidebar.tsx, etc.) all pass the same shape: a sidebar
// list's own loading/error pair, rendered above the list itself.

export function Loading() {
  return <AsyncListStatus loading={true} error={null} loadingText="Loading channels…" />;
}

export function Error() {
  return <AsyncListStatus loading={false} error="Failed to load members." loadingText="Loading members…" />;
}

export function LoadingAndError() {
  // Both render independently of each other (issue #412) — not the
  // usual case, but a real one: a refetch can start while a previous
  // error message is still on screen.
  return (
    <AsyncListStatus
      loading={true}
      error="Connection lost — retrying…"
      loadingText="Loading conversations…"
    />
  );
}
