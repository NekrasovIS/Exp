// Shared HTTP plumbing for every REST client in this package (auth/user/
// chat-service, issue #248/#249/#250) — kept in one place so all three
// clients throw the same error shape and none of them accidentally
// diverges on how a body/status is interpreted.

/** Minimal subset of the global `fetch` signature — lets tests inject a
 * stub instead of hitting the network, and lets callers on runtimes
 * without a global `fetch` (older Node, some RN setups) pass a polyfill. */
export type FetchLike = (input: string, init?: RequestInit) => Promise<Response>;

/** Thrown by every REST client here on a non-2xx response, or on a 2xx
 * response whose body doesn't match the expected shape (the server's own
 * `HttpServer`s in this repo never return 2xx with a malformed body in
 * practice, but a client must not assume that of a network boundary). */
export class ApiError extends Error {
  constructor(
    public readonly status: number,
    message: string,
  ) {
    super(message);
    this.name = "ApiError";
  }
}

interface RawResponse<T> {
  status: number;
  ok: boolean;
  body: T | undefined;
}

/** Exported for the rare caller that needs the raw `Response` itself
 * (e.g. chat-service's downloadAttachment, whose success body is raw
 * bytes, not JSON — only its *error* body needs parsing). */
export async function readJsonBody<T>(response: Response): Promise<T | undefined> {
  try {
    return (await response.json()) as T;
  } catch {
    return undefined;
  }
}

/** Runs a fetch and parses its body as JSON without throwing on a non-2xx
 * status — some callers (e.g. auth-service's register, which returns a
 * valid `{registered}` body on both 201 and 409) need to inspect the body
 * regardless of status, mirroring the C++ clients' own per-method
 * handling rather than a single blanket "throw on non-2xx" rule. */
export async function requestJson<T>(
  fetchImpl: FetchLike,
  url: string,
  init: RequestInit,
): Promise<RawResponse<T>> {
  const response = await fetchImpl(url, init);
  const body = await readJsonBody<T>(response);
  return { status: response.status, ok: response.ok, body };
}

/** @returns the `"error"` string field of @p body, if it has the shape
 * `{error: string}` — the convention every HttpServer in this repo uses
 * for its error responses. */
export function extractErrorMessage(body: unknown): string | undefined {
  if (typeof body === "object" && body !== null && "error" in body) {
    const error = (body as { error: unknown }).error;
    return typeof error === "string" ? error : undefined;
  }
  return undefined;
}

export function jsonRequestInit(method: string, token: string | undefined, body?: unknown): RequestInit {
  const headers: Record<string, string> = { "Content-Type": "application/json" };
  if (token !== undefined) {
    headers["Authorization"] = `Bearer ${token}`;
  }
  const init: RequestInit = { method, headers };
  if (body !== undefined) {
    init.body = JSON.stringify(body);
  }
  return init;
}

/** Joins @p baseUrl and @p path the same way every client here resolves
 * its endpoints — a plain string join, not `URL.resolve`, since every
 * path used in this package is already a fixed literal or has its only
 * dynamic segment percent-encoded by the caller (matching the C++
 * clients, which percent-encode logins embedded in a path themselves). */
export function resolveUrl(baseUrl: string, path: string): string {
  return baseUrl.replace(/\/+$/, "") + path;
}
