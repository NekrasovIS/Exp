/// <reference types="vitest/config" />
import react from "@vitejs/plugin-react";
import { fileURLToPath } from "node:url";
import { defineConfig } from "vite";

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
  },
});
