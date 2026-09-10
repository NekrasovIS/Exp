// Issue #326 — @mention autocomplete for the message composer. Doesn't
// reuse #322's useMembers() (still an open, unmerged PR at the time
// this was written) — fetches the community's member logins itself,
// directly through ChatRestClient.listMembers() (already on master
// since #250), the same REST call MemberListPanel-equivalents already
// make. Once #322 lands, a follow-up could consolidate the two, but
// this hook has no dependency on it either way.
//
// Deliberately plain-text/<input>-oriented: trigger detection, prefix
// filtering, and keyboard navigation all work off a (text, cursorPos)
// pair the caller reads from its own <input>/<textarea> element,
// mirroring the client-side "@" detection DeviceHub's ChatView uses
// (word-boundary before "@", no whitespace in the typed prefix) so
// both clients trigger on the same input shapes.

import { ChatRestClient } from "@devicehub/core";
import { useCallback, useEffect, useMemo, useState } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

export interface MentionAutocompleteResult {
  text: string;
  cursorPos: number;
}

export function useMentionAutocomplete(communityId: number) {
  const { getAccessToken } = useSession();
  const restClient = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);
  const [memberLogins, setMemberLogins] = useState<string[]>([]);

  useEffect(() => {
    const token = getAccessToken();
    if (token === null) {
      return;
    }
    restClient
      .listMembers(token, communityId)
      .then(setMemberLogins)
      .catch(() => setMemberLogins([]));
  }, [restClient, getAccessToken, communityId]);

  const [suggestions, setSuggestions] = useState<string[]>([]);
  const [activeIndex, setActiveIndex] = useState(0);
  const [triggerPos, setTriggerPos] = useState<number | null>(null);

  const handleTextChange = useCallback(
    (text: string, cursorPos: number) => {
      const textBeforeCursor = text.slice(0, cursorPos);
      const atPos = textBeforeCursor.lastIndexOf("@");
      // "@" must start a word (start of string or after whitespace) —
      // otherwise "user@example.com" would trigger on every letter
      // after "@", same rule as the mention-highlighting regex (#307)
      // and DeviceHub's own updateMentionAutocomplete().
      const startsWord = atPos === 0 || /\s/.test(textBeforeCursor[atPos - 1] ?? "");
      const prefix = atPos >= 0 ? textBeforeCursor.slice(atPos + 1) : "";
      if (atPos < 0 || !startsWord || prefix.includes(" ")) {
        setSuggestions([]);
        setTriggerPos(null);
        return;
      }
      const matches = memberLogins.filter((login) => login.toLowerCase().startsWith(prefix.toLowerCase()));
      setSuggestions(matches);
      setActiveIndex(0);
      setTriggerPos(matches.length > 0 ? atPos : null);
    },
    [memberLogins],
  );

  const moveActive = useCallback(
    (delta: number) => {
      setActiveIndex((prev) => {
        if (suggestions.length === 0) {
          return prev;
        }
        return (prev + delta + suggestions.length) % suggestions.length;
      });
    },
    [suggestions.length],
  );

  const dismiss = useCallback(() => {
    setSuggestions([]);
    setTriggerPos(null);
  }, []);

  const applySuggestion = useCallback(
    (text: string, cursorPos: number, login: string): MentionAutocompleteResult => {
      if (triggerPos === null) {
        return { text, cursorPos };
      }
      const newText = `${text.slice(0, triggerPos)}@${login} ${text.slice(cursorPos)}`;
      const newCursorPos = triggerPos + login.length + 2;
      setSuggestions([]);
      setTriggerPos(null);
      return { text: newText, cursorPos: newCursorPos };
    },
    [triggerPos],
  );

  const applyActive = useCallback(
    (text: string, cursorPos: number): MentionAutocompleteResult => {
      const login = suggestions[activeIndex];
      if (login === undefined) {
        return { text, cursorPos };
      }
      return applySuggestion(text, cursorPos, login);
    },
    [suggestions, activeIndex, applySuggestion],
  );

  return { suggestions, activeIndex, handleTextChange, applySuggestion, applyActive, moveActive, dismiss };
}
