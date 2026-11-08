'use strict';
const assert = require('assert/strict');
const { LyricCache } = require('../app/src/cache');
const song = { found: true, lines: [{ time: 1, text: 'A cached lyric' }] };
const cache = new LyricCache(2, 1024);
cache.set('a', song); cache.set('b', song);
assert.equal(cache.get('a'), song);
cache.set('c', song);
assert.equal(cache.get('b'), undefined); // Reads keep a frequently used song alive.
assert.equal(cache.get('a'), song);
const before = cache.bytes;
cache.set('a', song);
assert.equal(cache.bytes, before); // Replacement never double-counts storage.
cache.set('large', { lines: [{ time: 0, text: '夜'.repeat(1000) }] });
assert.equal(cache.get('large'), undefined);
assert.equal(cache.get('a'), song); // Oversized responses still play but are not retained.
cache.set('c', { lines: [{ time: 0, text: 'long'.repeat(85) }] });
assert(cache.bytes <= 1024);
assert.equal(cache.get('a'), undefined); // The byte budget also evicts below the entry limit.
assert.equal(cache.entries.size, 1);
console.log('PASS lyric cache recency, replacement, oversized Unicode payloads and byte-budget eviction');
