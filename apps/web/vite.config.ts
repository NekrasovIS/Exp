import react from "@vitejs/plugin-react";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";
// Issue #303 — this real import (for configDefaults) already pulls in
// "vitest/config"'s own ambient module augmentation of Vite's
// UserConfig (the `test` field defineConfig() accepts below), making
// the triple-slash type-reference this file used to open with
// redundant — @typescript-eslint/triple-slash-reference now flags
// exactly that combination.
import { configDefaults } from "vitest/config";

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      // libsodium-wrappers@0.7.16's ESM build (dist/modules-esm/libsodium-wrappers.mjs)
      // imports a sibling "./libsodium.mjs" that the published package
      // doesn't actually ship (issue #269) — its CJS build (require("libsodium"))
      // is unaffected, so resolve straight to that file instead of the
      // broken "module" entry point. A bare-specifier alias would still
      // get rejected by the package's own "exports" map, so this points
      // at the real file on disk instead.
      "libsodium-wrappers": fileURLToPath(
        new URL("./node_modules/libsodium-wrappers/dist/modules/libsodium-wrappers.js", import.meta.url),
      ),
    },
  },
  test: {
    environment: "jsdom",
    setupFiles: ["./test/setup.ts"],
    // Issue #303 — e2e/*.spec.ts are Playwright tests (their own
    // test()/expect(), incompatible with Vitest's), not Vitest's;
    // Vitest's own default include pattern matches "*.spec.ts"
    // anywhere in the project, so without this it would try to run
    // them too. configDefaults.exclude, not a bare array, so this adds
    // to Vitest's own node_modules/dist/etc. exclusions instead of
    // replacing them.
    exclude: [...configDefaults.exclude, "e2e/**"],
  },
});
