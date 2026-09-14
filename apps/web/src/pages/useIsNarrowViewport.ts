// Issue #439 — the one place the whole web client checks viewport width;
// every other page/component reads its result rather than touching
// matchMedia itself, so the breakpoint stays single-sourced.

import { useEffect, useState } from "react";

/** Below this width, CommunitiesMode/FriendsMode switch from showing all
 *  sidebar columns at once to showing exactly one panel at a time. */
export const kNarrowViewportBreakpointPx = 768;

function matchesNarrowQuery(): boolean {
  // jsdom (Vitest's default test environment) doesn't implement
  // matchMedia at all — falling back to "not narrow" here means every
  // existing test that doesn't explicitly stub it keeps exercising the
  // current (wide) layout unchanged, rather than failing to render.
  if (typeof window === "undefined" || typeof window.matchMedia !== "function") {
    return false;
  }
  return window.matchMedia(`(max-width: ${kNarrowViewportBreakpointPx}px)`).matches;
}

export function useIsNarrowViewport(): boolean {
  const [isNarrow, setIsNarrow] = useState(matchesNarrowQuery);

  useEffect(() => {
    if (typeof window.matchMedia !== "function") {
      return;
    }
    const mediaQueryList = window.matchMedia(`(max-width: ${kNarrowViewportBreakpointPx}px)`);
    const handleChange = (event: MediaQueryListEvent): void => setIsNarrow(event.matches);
    mediaQueryList.addEventListener("change", handleChange);
    // The query object itself may have changed state between the
    // initial useState() call and this effect running — re-sync rather
    // than trust the constructor-time snapshot.
    setIsNarrow(mediaQueryList.matches);
    return () => mediaQueryList.removeEventListener("change", handleChange);
  }, []);

  return isNarrow;
}
