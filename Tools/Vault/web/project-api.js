// Backoff between attempts on 423 LOCKED: the server serialises writes behind
// a shared Git lock, so a write arriving while another is running is transient
// contention, not a refusal. Five attempts total spans roughly three seconds.
export const LOCK_RETRY_DELAYS_MS = [150, 300, 600, 1200];

const defaultSleep = ms => new Promise(resolve => setTimeout(resolve, ms));

/**
 * Replay `send` while the server reports lock contention. The caller builds
 * the request (including its operationId) ONCE, so every attempt is the same
 * idempotent operation rather than a second write. Any other status — 409,
 * 422, 403, 404 — is a real answer about this request and is surfaced
 * immediately.
 */
export async function withLockRetry(send, { delays = LOCK_RETRY_DELAYS_MS, sleep = defaultSleep } = {}) {
  let attempt = 0;
  for (;;) {
    try {
      return await send();
    } catch (error) {
      if (error?.status !== 423 || attempt >= delays.length) throw error;
      await sleep(delays[attempt]);
      attempt += 1;
    }
  }
}

export function createProjectApi(csrfToken, fetcher = globalThis.fetch) {
  return async (path, options = {}) => {
    const method = options.method || 'GET';
    const body = options.body && { ...options.body };
    if (body && method !== 'GET' && !body.operationId) body.operationId = crypto.randomUUID();
    const response = await fetcher(path, {
      ...options, method,
      headers: { ...(body ? { 'Content-Type': 'application/json' } : {}),
        ...(method !== 'GET' ? { 'X-CSRF-Token': csrfToken } : {}), ...options.headers },
      ...(body ? { body: JSON.stringify(body) } : {}),
    });
    const payload = await response.json();
    if (!response.ok) {
      const error = new Error(payload.error?.message || payload.message || `Request failed (${response.status})`);
      error.code = payload.error?.code; error.status = response.status;
      throw error;
    }
    return payload;
  };
}
