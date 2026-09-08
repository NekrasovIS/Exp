// Shared test helpers (issue #265) — mirrors packages/core's own test
// convention for stubbing fetch, since every auth form here constructs
// its own AuthClient internally (using the global fetch) rather than
// taking one injected.

export function jsonResponse(status: number, body: unknown): Response {
  return new Response(JSON.stringify(body), { status });
}
