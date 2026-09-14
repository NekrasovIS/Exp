# design-sync notes for @devicehub/ui

## Repo-specific gotchas

- **No Storybook anywhere in this monorepo** (`apps/web`, `packages/*`) —
  confirmed via `Glob` for `.storybook/main.*` and `*.stories.*`, both
  empty. Always `shape: "package"`.
- **`packages/ui` is source-only** — `package.json`'s `main`/`exports`
  point straight at `src/index.ts`, no `dist/` build at all (apps/web's
  own vite/vitest compiles the TSX/CSS Modules directly from source; see
  issue #433). This means:
  - `--node-modules packages/ui/node_modules` alone fails with
    `ENOENT ... node_modules/@devicehub/ui/package.json` — a package
    can't resolve itself from its own `node_modules` (npm never
    self-installs a workspace package into its own tree). Always pass
    `--entry ./packages/ui/src/index.ts` explicitly alongside it.
  - The converter's "run the build script" step is a no-op here:
    `packages/ui`'s `"build": "tsc -p tsconfig.json"` has
    `noEmit: true` in its `tsconfig.json` (typecheck-only, by design) —
    running it produces zero output. This is the legitimate "user says
    there's no build" case, not a broken build — go straight to
    `--entry` synth mode, don't chase a `dist/` that will never exist.
- **`cfg.cssEntry`/`cfg.tokensGlob` cannot reach outside `packages/ui`**
  (a deliberate security bound — see `package-build.mjs`'s comment
  above `workspaceRoot`/`pkgRoot`). The package's design tokens
  originally lived in `apps/web/src/theme.css`, one level up — every
  preview rendered completely unstyled (`[TOKENS_MISSING]`, `Styled`
  rubric failure) until issue #435 moved the tokens themselves into
  `packages/ui/src/theme.css` (re-exported via `apps/web/src/theme.css`'s
  `@import "@devicehub/ui/theme.css"`). `cfg.cssEntry` now points at
  `src/theme.css` directly — inside the package, no bound violation.
  **If a future component needs a token that still only lives in
  `apps/web`, move it into `packages/ui/src/theme.css` too** rather than
  reaching for `tokensGlob`/`extraFonts` workarounds — the bound will
  reject anything outside the package regardless.
- **Playwright's browser binary was already cached** at
  `%LOCALAPPDATA%\ms-playwright\chromium-1243` (Windows; not
  `~/.cache/ms-playwright`, the Linux/macOS path) from an earlier,
  unrelated Playwright e2e setup in `apps/web` — no download needed.
  `package-validate.mjs` still needs the **`playwright` npm package
  importable from `.ds-sync/`'s own location**, though — it's not
  enough that `apps/web/node_modules/playwright` exists. Run
  `npm i playwright@<version matching apps/web's own pin>` inside
  `.ds-sync/` (this repo: `1.63.0`) even though the browser itself is
  already cached; it resolves instantly, no re-download.

## Known render warns

None outstanding — after the theme.css move, `package-validate.mjs`
reports zero warnings (`tokens: 33 defined, 21 referenced`, render check
2/2 clean, no `[RENDER_THIN]`).

## Re-sync risks

- **`packages/ui`'s token/component set is tiny today** (2 components,
  ~33 tokens) — a future re-sync after `packages/ui` grows should expect
  the `[TOKENS_MISSING]` check to matter a lot more; keep every new
  token defined in `packages/ui/src/theme.css` itself, never only in
  `apps/web/src/theme.css`.
- **`avatarPlaceholderStyles` (a CSS-only export from `index.ts`) is
  correctly excluded** from the component count — it's not a
  PascalCase-exported function/class, so the converter's own heuristic
  already skips it. If `packages/ui` ever exports another CSS-Modules
  object under a PascalCase name, double-check it doesn't get
  miscounted as a component (`componentSrcMap: {"Name": null}` to
  exclude it explicitly).
- **Authored previews (`.design-sync/previews/AsyncListStatus.tsx`,
  `MessageRow.tsx`) are hand-composed from this repo's real call sites**
  (`apps/web/src/channels/ChannelsSidebar.tsx` etc. for loading-text
  strings; `apps/web/src/chat/MessageList.tsx` for the message-thread
  shape) — if those call sites' copy changes, the previews will drift
  from "realistic" without anything flagging it (previews aren't
  content-linked to their source usage). Not urgent, just not
  self-healing.
- **Data-attribute variants aren't part of either component's own
  `.d.ts`** — `data-variant="primary"/"danger"` is a plain-`<button>`
  convention from `apps/web/src/theme.css`, documented in
  `conventions.md` but invisible to the `.d.ts`-based prop extraction
  since neither synced component is a `<button>` itself. If a future
  synced component wraps a `<button>` with this convention, its
  `.d.ts` won't show a `variant` prop unless one is added explicitly.
