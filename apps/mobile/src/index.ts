// Placeholder entry point (issue #247) — the real mobile client is
// #222/#223. This file only proves that `@devicehub/core` resolves
// through the pnpm workspace link from an app package, nothing more —
// no console/DOM/Node globals here on purpose, since this tsconfig
// doesn't assume any runtime yet (React Native's own ambient types
// arrive with the real client in #222).

import { CORE_PACKAGE_NAME } from "@devicehub/core";

export const linkedCorePackageName: string = CORE_PACKAGE_NAME;
