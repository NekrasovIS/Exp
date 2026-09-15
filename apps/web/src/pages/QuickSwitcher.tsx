// Issue #459 — Ctrl+K/Cmd+K quick switcher. Pure presentational + its
// own small amount of input state (query/activeIndex) — unlike
// MentionSuggestions.tsx (a dropdown attached to an existing <input>,
// so keyboard handling lives in the caller), this is a self-contained
// modal with its own dedicated <input>, so there's no caller to split
// the keyboard handling into.
//
// Scope: jumps between what the current page already has loaded (all
// of the caller's communities, plus the channels of whichever one is
// currently open) — not a global index server-side. That covers the
// common case (Discord's own Ctrl+K is also mostly "communities you're
// in / channels of the current one"); browsing a channel list of a
// community you haven't opened yet still means clicking it once first.

import { useMemo, useState, type KeyboardEvent } from "react";

import styles from "./QuickSwitcher.module.css";

export interface QuickSwitcherItem {
  key: string;
  label: string;
  group: string;
  onSelect: () => void;
}

interface QuickSwitcherProps {
  items: QuickSwitcherItem[];
  onClose: () => void;
}

export function QuickSwitcher({ items, onClose }: QuickSwitcherProps) {
  const [query, setQuery] = useState("");
  const [activeIndex, setActiveIndex] = useState(0);

  const filtered = useMemo(() => {
    const needle = query.trim().toLowerCase();
    return needle === "" ? items : items.filter((item) => item.label.toLowerCase().includes(needle));
  }, [items, query]);

  function pick(item: QuickSwitcherItem): void {
    item.onSelect();
    onClose();
  }

  function handleQueryChange(value: string): void {
    setQuery(value);
    setActiveIndex(0);
  }

  function handleKeyDown(event: KeyboardEvent<HTMLInputElement>): void {
    if (event.key === "ArrowDown") {
      event.preventDefault();
      setActiveIndex((index) => Math.min(index + 1, Math.max(filtered.length - 1, 0)));
    } else if (event.key === "ArrowUp") {
      event.preventDefault();
      setActiveIndex((index) => Math.max(index - 1, 0));
    } else if (event.key === "Enter") {
      event.preventDefault();
      const item = filtered[activeIndex];
      if (item !== undefined) {
        pick(item);
      }
    } else if (event.key === "Escape") {
      onClose();
    }
  }

  return (
    <div className={styles.backdrop} onClick={onClose}>
      <div className={styles.dialog} onClick={(event) => event.stopPropagation()}>
        <input
          autoFocus
          type="text"
          className={styles.input}
          placeholder="Jump to a community or channel…"
          aria-label="Jump to a community or channel"
          value={query}
          onChange={(event) => handleQueryChange(event.target.value)}
          onKeyDown={handleKeyDown}
        />
        <ul className={styles.list}>
          {filtered.map((item, index) => (
            <li key={item.key}>
              <button
                type="button"
                className={`${styles.item} ${index === activeIndex ? styles.itemActive : ""}`}
                // Same reason as MentionSuggestions.tsx: a mouse pick
                // must not steal focus from the filter <input>.
                onMouseDown={(event) => event.preventDefault()}
                onClick={() => pick(item)}
              >
                <span className={styles.group}>{item.group}</span>
                {item.label}
              </button>
            </li>
          ))}
          {filtered.length === 0 && <li className={styles.empty}>No matches.</li>}
        </ul>
      </div>
    </div>
  );
}
