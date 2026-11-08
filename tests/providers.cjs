'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert/strict');
const lrc = require('../app/src/lrc');
const fixture = '[00:01.50]First line\n[00:03.25]Second line';
function load(file, deps) {
  deps = { './matching': require('../app/src/matching'), ...deps };
  const module = { exports: {} };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '../app/src', file), 'utf8'), {
    module, exports: module.exports, require: name => { assert(name in deps, name); return deps[name]; }, URL,
    console: { error() {} }, Date,
  });
  return module.exports;
}
async function chainTest({ exact = null, search = [], qq = null, netease = null }) {
  const calls = [];
  const value = item => { if (item instanceof Error) throw item; return item; };
  const module = load('lyrics.js', {
    './lrc': lrc,
    './http': { fetchJSON: async url => { const stage = url.includes('/get?') ? 'lrclib-exact' : 'lrclib-search'; calls.push(stage); return value(stage === 'lrclib-exact' ? exact : search); } },
    './qqmusic': { fetchQQ: async () => { calls.push('qq'); return value(qq); } },
    './netease': { fetchNetEase: async () => { calls.push('netease'); return value(netease); } },
  });
  const result = await module.fetchLyrics('Track', 'Artist', 200);
  return { result, calls };
}
async function neteaseTest({ songs, lyric, rejectFirst = false }) {
  const calls = [];
  const module = load('netease.js', {
    './lrc': lrc,
    './http': { fetchJSON: async (url, options, timeout) => {
      assert(timeout > 0 && timeout <= 4000); assert(!options.headers.Cookie);
      calls.push(url);
      if (rejectFirst && calls.length === 1) throw Error('First node unavailable');
      return url.includes('/api/search/') ? { code: 200, result: { songs } } : { code: 200, ...lyric };
    } },
  });
  return { result: await module.fetchNetEase('Track', 'Artist', 200), calls };
}
(async () => {
  let test = await chainTest({ exact: { trackName: 'Track', artistName: 'Artist', duration: 200, syncedLyrics: fixture } });
  assert.equal(test.result.source, 'lrclib'); assert.deepEqual(test.calls, ['lrclib-exact']);
  test = await chainTest({ search: [{ trackName: 'Track', artistName: 'Artist', duration: 200, syncedLyrics: fixture }] });
  assert.equal(test.result.source, 'lrclib'); assert(!test.calls.includes('netease')); assert(!test.calls.includes('qq'));
  test = await chainTest({ qq: { lines: [{ time: 1, text: 'QQ line' }] } });
  assert.equal(test.result.source, 'qq'); assert(!test.calls.includes('netease'));
  test = await chainTest({ netease: { lines: [{ time: 1, text: 'NetEase line' }] } });
  assert.equal(test.result.source, 'netease'); assert.deepEqual(test.calls, ['lrclib-exact', 'lrclib-search', 'qq', 'netease']);
  test = await chainTest({ exact: Error('Timeout'), search: Error('Invalid JSON'), qq: Error('QQ unavailable'), netease: { lines: [{ time: 1, text: 'NetEase line' }] } });
  assert.equal(test.result.source, 'netease');
  test = await chainTest({ exact: { syncedLyrics: '[00:00.00]作曲：Someone' }, search: [{ syncedLyrics: 'plain lyrics' }], qq: { lines: [] }, netease: Error('Unavailable') });
  assert.equal(test.result.found, false); assert.equal(test.result.lines.length, 0);
  console.log('PASS LRCLIB -> QQ -> NetEase ordering; no NetEase request on prior success; errors and unusable lyrics fall through');

  const correct = { id: 42, name: 'Track', artists: [{ name: 'Artist' }], duration: 200000 };
  test = await neteaseTest({ songs: [{ ...correct, id: 1, artists: [{ name: 'Unrelated Singer' }] }, { ...correct, id: 2, name: 'Track (Live)' }, correct],
    lyric: { lrc: { lyric: '[offset:100]\n[00:00.00]作词：Someone\n[00:01.5][00:03.250]Original lyric\n[00:04.00]Next line' }, tlyric: { lyric: '[00:01.5]Translation' } } });
  assert.equal(test.result.songId, 42); assert.equal(test.result.lines[0].time, 1.6); assert.equal(test.result.lines[1].time, 3.35);
  assert.equal(test.result.lines.length, 3); assert(test.result.lines.every(line => line.text !== 'Translation'));
  assert(test.calls[1].includes('id=42&'));
  test = await neteaseTest({ songs: [{ ...correct, artists: [{ name: 'Someone Else' }] }], lyric: { lrc: { lyric: fixture } } });
  assert.equal(test.result, null); assert(test.calls.every(url => url.includes('/search/')));
  test = await neteaseTest({ songs: [correct], lyric: { nolyric: true, lrc: { lyric: '[00:00.00]纯音乐，请欣赏' } } }); assert.equal(test.result, null);
  test = await neteaseTest({ songs: [correct], lyric: { lrc: { lyric: fixture } }, rejectFirst: true });
  assert(test.result.lines.length); assert(test.calls[1].startsWith('https://interface3.music.163.com/'));
  console.log('PASS NetEase artist/version matching, original timed lyrics in seconds, offset/repeated tags, instrumentals and node recovery');
})().catch(error => { console.error(error); process.exitCode = 1; });
