// Issue #267 — case-insensitive substring search over a channel's
// message history (issue #118) via ChatRestClient.searchMessages();
// chat-service itself rejects this for encrypted channels (400), but
// ChatView never mounts this for one in the first place (see
// ChatView.tsx).

import { ChatRestClient } from "@devicehub/core";
import type { ChatMessageInfo } from "@devicehub/core";
import { useMemo, useState, type FormEvent } from "react";

import { chatServiceRestUrl } from "../config.js";
import { useSession } from "../session/SessionContext.js";

interface MessageSearchProps {
  channelId: number;
}

export function MessageSearch({ channelId }: MessageSearchProps) {
  const { getAccessToken } = useSession();
  const client = useMemo(() => new ChatRestClient(chatServiceRestUrl), []);

  const [query, setQuery] = useState("");
  const [results, setResults] = useState<ChatMessageInfo[] | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [searching, setSearching] = useState(false);

  async function handleSearch(event: FormEvent): Promise<void> {
    event.preventDefault();
    const token = getAccessToken();
    if (token === null || query.trim() === "") {
      return;
    }
    setError(null);
    setSearching(true);
    try {
      setResults(await client.searchMessages(token, channelId, query.trim()));
    } catch {
      setError("Search isn't available for this channel.");
      setResults(null);
    } finally {
      setSearching(false);
    }
  }

  return (
    // A plain <div role="search"> rather than the standard HTML
    // <search> element — this React/DOM version's dev build doesn't
    // recognize <search> yet and warns on every render.
    <div role="search">
      <form onSubmit={handleSearch}>
        <label htmlFor="message-search-query">Search this channel</label>
        <input id="message-search-query" value={query} onChange={(event) => setQuery(event.target.value)} />
        <button type="submit" disabled={searching}>
          Search
        </button>
      </form>
      {error !== null && <p role="alert">{error}</p>}
      {results !== null && (
        <ul>
          {results.length === 0 && <li>No matches.</li>}
          {results.map((message) => (
            <li key={message.id}>
              <strong>{message.author}</strong> {message.body}
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
