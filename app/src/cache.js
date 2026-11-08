'use strict';
// Bound retained provider data by entry count and estimated UTF-16 storage.
class LyricCache {
  constructor(maxEntries = 8, maxBytes = 2 * 1024 * 1024) {
    this.entries = new Map();
    this.bytes = 0;
    this.maxEntries = maxEntries;
    this.maxBytes = maxBytes;
  }
  get(key) {
    const entry = this.entries.get(key);
    if (!entry) return undefined;
    this.entries.delete(key);
    this.entries.set(key, entry);
    return entry.value;
  }
  set(key, value) {
    const previous = this.entries.get(key);
    if (previous) { this.bytes -= previous.bytes; this.entries.delete(key); }
    const bytes = 128 + key.length * 2 + (value.artist?.length || 0) * 2 +
      (value.lines || []).reduce((sum, line) => sum + 64 + String(line.text || '').length * 2, 0);
    if (bytes > this.maxBytes || this.maxEntries < 1) return;
    this.entries.set(key, { value, bytes });
    this.bytes += bytes;
    while (this.entries.size > this.maxEntries || this.bytes > this.maxBytes) {
      const oldest = this.entries.keys().next().value;
      this.bytes -= this.entries.get(oldest).bytes;
      this.entries.delete(oldest);
    }
  }
}
module.exports = { LyricCache };
