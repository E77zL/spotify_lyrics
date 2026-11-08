'use strict';

// Keep the deadline active until JSON is consumed, and propagate track changes.
async function fetchJSON(url, options = {}, timeoutMs = 8000) {
  const controller = new AbortController();
  const cancel = () => controller.abort(options.signal.reason);
  options.signal?.throwIfAborted();
  options.signal?.addEventListener('abort', cancel, { once: true });
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    const response = await fetch(url, { ...options, signal: controller.signal });
    if (!response.ok) { await response.body?.cancel(); return null; }
    return await response.json();
  } finally {
    clearTimeout(timer);
    options.signal?.removeEventListener('abort', cancel);
  }
}

module.exports = { fetchJSON };
