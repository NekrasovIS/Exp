// Placeholder entry point (issue #247) — the real web client is #220/#221.
// This file only proves that `@devicehub/core` resolves through the pnpm
// workspace link from an app package, nothing more.

import { CORE_PACKAGE_NAME } from "@devicehub/core";

console.log(`apps/web placeholder — linked against ${CORE_PACKAGE_NAME}`);
