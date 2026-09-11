// Issue #326 — the dropdown itself for @mention autocomplete. Pure
// display + click-to-select; keyboard navigation (Up/Down/Enter/Tab/
// Escape) is handled by the composer's own onKeyDown, not here — this
// component only needs to render `activeIndex`'s highlight.

import styles from "./MentionSuggestions.module.css";

interface MentionSuggestionsProps {
  suggestions: string[];
  activeIndex: number;
  onSelect: (login: string) => void;
}

export function MentionSuggestions({ suggestions, activeIndex, onSelect }: MentionSuggestionsProps) {
  if (suggestions.length === 0) {
    return null;
  }
  return (
    <ul className={styles.list}>
      {suggestions.map((login, index) => (
        <li key={login}>
          <button
            type="button"
            className={`${styles.item} ${index === activeIndex ? styles.itemActive : ""}`}
            // Selecting with the mouse must not steal focus from the
            // composer's <input> — losing focus mid-selection would
            // strand the cursor position applySuggestion() relies on.
            onMouseDown={(event) => event.preventDefault()}
            onClick={() => onSelect(login)}
          >
            @{login}
          </button>
        </li>
      ))}
    </ul>
  );
}
