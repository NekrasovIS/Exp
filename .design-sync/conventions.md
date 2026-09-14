## DeviceHub UI conventions

**No provider needed.** Every component reads plain CSS custom properties
from `:root` — there is no theme/context provider to wrap anything in.
Just render the component; the tokens resolve automatically as long as
`@devicehub/ui/theme.css` is loaded on the page (already true for every
synced preview here).

**Styling idiom: CSS custom properties (`var(--*)`), not utility classes
or style props.** The real token names, defined once in `theme.css`:

| Purpose | Tokens |
|---|---|
| Accent (brand green) | `--color-accent`, `--color-accent-strong`, `--color-accent-hover`, `--color-accent-hover-strong` |
| Surfaces | `--color-bg`, `--color-surface`, `--color-surface-raised` |
| Borders | `--color-border`, `--color-border-strong` |
| Text | `--color-text`, `--color-text-secondary`, `--color-text-muted`, `--color-text-on-accent` |
| Status | `--color-danger`, `--color-danger-bg`, `--color-danger-border` |
| Spacing | `--space-1` (4px) through `--space-6` (32px) |
| Shape | `--radius-sm` (6px), `--radius-md` (10px) |
| Type | `--font-size-sm/base/lg/xl`, `--line-height-base`, `--font-family` |

Native `<button>` elements pick up an accent/danger look via a plain HTML
attribute, not a component prop: `data-variant="primary"` (gradient
accent fill) or `data-variant="danger"` (quiet red outline, warms up on
hover). A bare `<button type="submit">` gets the same look as
`data-variant="primary"` automatically.

**Where the truth lives.** `theme.css` (bound into this bundle) defines
every token above plus the base `button`/`input`/`[role="alert"]`
element styling — read it before inventing a new color or spacing value.
Each component's own `.module.css` (bundled alongside it) only adds
what's specific to that component; it never redefines a token.

**Build snippet** — a channel-like message thread, real composition from
`apps/web/src/chat/MessageList.tsx`:

```tsx
import { MessageRow } from "@devicehub/ui";

<ul>
  <MessageRow isOwn={false} author="alice">
    <span>Did you see the new build fail on main?</span>
  </MessageRow>
  <MessageRow isOwn={true} author="bob">
    <span>Yeah, looking at it now — think it's the linker.</span>
  </MessageRow>
</ul>
```
