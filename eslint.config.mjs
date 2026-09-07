// @ts-check
import js from "@eslint/js";
import tseslint from "typescript-eslint";

export default tseslint.config(
  // A config object with ONLY `ignores` (no `files`) is a *global*
  // ignore in ESLint's flat config — unlike a `files` restriction on one
  // entry (which doesn't limit the *other* entries in this array, a
  // real gotcha here), this excludes matching paths from every config
  // below. Needed because the repo root also holds the unrelated C++/Qt
  // project (src/, services/, vcpkg/ — the latter vendors third-party
  // JS build scripts and even a stray syntax-invalid .js file ESLint
  // has no business parsing).
  {
    ignores: [
      "vcpkg/**",
      "src/**",
      "services/**",
      "test/**",
      "build/**",
      "build-*/**",
      "**/dist/**",
      "**/node_modules/**",
    ],
  },
  js.configs.recommended,
  ...tseslint.configs.recommended,
  {
    rules: {
      "@typescript-eslint/no-unused-vars": ["error", { argsIgnorePattern: "^_" }],
    },
  },
);
