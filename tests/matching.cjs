'use strict';
const fs = require('fs');
const path = require('path');
const vm = require('vm');
const assert = require('assert/strict');
const root = path.join(__dirname, '../app/src');
const lrc = require('../app/src/lrc');
const lyric = '[00:01.00]Correct song\n[00:04.00]Next line';
const requested = { title: 'wait 4 u', artist: 'Westwood', duration: 163 };
const wrongQQ = { songmid: 'wrong', songname: 'Wait 4 U', singer: [{ name: 'Milk N Cooks' }, { name: 'Brewski' }], interval: 192 };
const rightQQ = { songmid: 'right', songname: 'wait 4 u', singer: [{ name: 'Westwood' }], interval: 163 };
function load(file, dependencies) {
  const module = { exports: {} };
  vm.runInNewContext(fs.readFileSync(path.join(root, file), 'utf8'), {
    module, exports: module.exports, URL, Buffer, Date, console: { error() {} },
    require: name => name === './matching' ? require('../app/src/matching') : dependencies[name],
  }, { filename: file });
  return module.exports;
}
async function qq(songs, lyrics = {}, title = requested.title, artist = requested.artist) {
  const ids = [];
  const provider = load('qqmusic.js', {
    crypto: require('crypto'), './lrc': lrc,
    './http': { fetchJSON: async (url, options) => {
      if (options.method !== 'POST') return { data: { song: { list: songs } } };
      const id = JSON.parse(options.body).req_1.param.songMID;
      ids.push(id);
      return { req_1: { data: { lyric: Buffer.from(lyrics[id] ?? lyric).toString('base64') } } };
    } },
  });
  return { result: await provider.fetchQQ(title, artist, undefined, requested.duration), ids };
}
async function chain({ exact = null, search = [], songs = [wrongQQ], netease = true, input = requested } = {}) {
  const calls = [];
  const qqProvider = load('qqmusic.js', {
    crypto: require('crypto'), './lrc': lrc,
    './http': { fetchJSON: async (url, options) => {
      calls.push(options.method === 'POST' ? 'qq-lyric' : 'qq-search');
      return options.method === 'POST' ? { req_1: { data: { lyric: Buffer.from(lyric).toString('base64') } } } : { data: { song: { list: songs } } };
    } },
  });
  const provider = load('lyrics.js', {
    './lrc': lrc,
    './http': { fetchJSON: async url => { const get = url.includes('/get?'); calls.push(get ? 'exact' : 'search'); return get ? exact : search; } },
    './qqmusic': qqProvider,
    './netease': { fetchNetEase: async () => { calls.push('netease'); return netease ? { lines: lrc.parseLRC(lyric) } : null; } },
  });
  return { result: await provider.fetchLyrics(input.title, input.artist, input.duration), calls };
}
(async () => {
  // The live QQ search really returns this other artist as its first result.
  let test = await qq([wrongQQ]);
  assert.equal(test.result, null, 'Do not use Milk N Cooks/Brewski for Westwood');
  assert.deepEqual(test.ids, [], 'Do not request lyrics for a rejected artist');
  test = await qq([wrongQQ, { ...rightQQ, songmid: 'other-title', songname: 'Overtime' }, rightQQ]);
  assert(test.result?.lines.length); assert.deepEqual(test.ids, ['right']);
  for (const name of ['Wood', 'Westwoods', 'Westwood Tribute', 'Eastwood']) {
    test = await qq([{ ...rightQQ, singer: [{ name }] }]); assert.equal(test.result, null, name);
  }
  test = await qq([{ ...rightQQ, singer: [] }]); assert.equal(test.result, null);
  test = await qq([{ ...rightQQ, songmid: 'live', songname: 'wait 4 u (Live)' }, rightQQ]); assert.deepEqual(test.ids, ['right']);
  test = await qq([{ ...rightQQ, songmid: 'far', interval: 300 }, rightQQ]); assert.deepEqual(test.ids, ['right']);
  test = await qq([{ ...rightQQ, songmid: 'empty' }, rightQQ], { empty: '' }); assert.deepEqual(test.ids, ['empty', 'right']);
  test = await qq([{ ...rightQQ, songmid: 'metadata' }, rightQQ], { metadata: '[00:00.00]作词：Someone' }); assert.deepEqual(test.ids, ['metadata', 'right']);
  test = await qq([{ ...rightQQ, singer: [{ name: 'WÉSTWOOD' }] }]); assert(test.result?.lines.length);
  test = await qq([rightQQ], {}, 'WAIT 4 U - 2024 Remaster', 'Westwood, Guest'); assert(test.result?.lines.length);
  test = await qq([{ ...rightQQ, singer: [{ name: 'AC/DC' }] }], {}, 'wait 4 u', 'AC/DC'); assert(test.result?.lines.length);
  console.log('PASS QQ rejects other/missing artists and titles, ranks duration, skips unusable lyrics and accepts normalized names');

  const rightLrc = { trackName: 'wait 4 u', artistName: 'Westwood', duration: 163, syncedLyrics: lyric };
  const wrongLrc = { ...rightLrc, artistName: 'Milk N Cooks/Brewski' };
  test = await chain({ exact: wrongLrc, search: [wrongLrc, rightLrc] });
  assert.equal(test.result.source, 'lrclib'); assert.deepEqual(test.calls, ['exact', 'search']);
  test = await chain({ search: [wrongLrc, { ...rightLrc, trackName: 'Overtime' }] });
  assert.equal(test.result.source, 'netease'); assert.deepEqual(test.calls, ['exact', 'search', 'qq-search', 'netease']);
  test = await chain({ exact: { syncedLyrics: lyric }, search: [{ ...rightLrc, artistName: '' }], netease: false });
  assert.equal(test.result.found, false);
  test = await chain({ search: [{ ...rightLrc, duration: 300, syncedLyrics: '[00:01.00]Long version' }, rightLrc] });
  assert.equal(test.result.lines[0].text, 'Correct song');
  console.log('PASS LRCLIB validates exact/search metadata; unmatched QQ falls through to NetEase; all mismatches yield no lyrics');

  const neteaseCalls = [];
  const netease = load('netease.js', {
    './lrc': lrc,
    './http': { fetchJSON: async url => {
      neteaseCalls.push(url);
      return url.includes('/search/') ? { code: 200, result: { songs: [
        { id: 1, name: 'wait 4 u', artists: [{ name: 'Eastwood' }], duration: 163000 },
        { id: 3446352573, name: 'wait 4 u', artists: [{ name: 'Westwood' }], duration: 162562 },
      ] } } : { code: 200, lrc: { lyric } };
    } },
  });
  const found = await netease.fetchNetEase(requested.title, requested.artist, requested.duration);
  assert.equal(found.songId, 3446352573);
  assert(neteaseCalls[1].includes('id=3446352573&'));
  console.log('PASS NetEase excludes a similar but different artist (Eastwood)');

  const { cleanTitle, rankCandidates } = require('../app/src/matching');
  const localTitle = 'wait 4 u [prod. pxiqzes]';
  assert.equal(cleanTitle(localTitle), 'wait 4 u');
  assert.equal(cleanTitle('wait 4 u (produced by pxiqzes)'), 'wait 4 u');
  assert.equal(cleanTitle('wait 4 u (Live)'), 'wait 4 u (Live)');
  const candidate = (artist, duration = 162.562, title = 'wait 4 u') => ({ title, artists: [artist], duration });
  const records = [candidate('Milk N Cooks', 192), candidate('Westwood'), candidate('DC The Don', 132)];
  assert.equal(rankCandidates(records, localTitle, '', 162)[0]?.artists[0], 'Westwood');
  assert.equal(rankCandidates([candidate('Westwood'), candidate('Someone Else', 163)], localTitle, '', 162).length, 0, 'Ambiguous artists need a stronger identity');
  assert.equal(rankCandidates([candidate('Westwood'), candidate('WESTWOOD', 163)], localTitle, '', 162).length, 2, 'Duplicate records for the same artist are fine');
  assert.equal(rankCandidates(records, localTitle, '', 0).length, 0, 'Missing artist and duration must not select the first result');
  assert.equal(rankCandidates([candidate('Milk N Cooks', 192)], localTitle, '', 162).length, 0, 'Same title alone is insufficient');
  assert.equal(rankCandidates([candidate('Westwood', 162, 'wait 4 u (Live)')], localTitle, '', 162).length, 0);
  assert.equal(rankCandidates([candidate('Westwood', 162, 'wait 4 you')], localTitle, '', 162).length, 0, 'No fuzzy title-only guesses');
  test = await chain({ input: { title: localTitle, artist: '', duration: 162 }, search: [{ ...wrongLrc, duration: 192 }, rightLrc], netease: false });
  assert.equal(test.result.source, 'lrclib'); assert.equal(test.result.artist, 'Westwood'); assert.deepEqual(test.calls, ['search']);
  test = await chain({ input: { title: localTitle, artist: '', duration: 162 }, search: [rightLrc, { ...rightLrc, artistName: 'Another artist', syncedLyrics: null }], songs: [], netease: false });
  assert.equal(test.result.found, false, 'Untimed results still count when excluding ambiguous artists');
  console.log('PASS local production tags, absent artist, exact title/duration constraints, duplicate and ambiguous candidates');
})().catch(error => { console.error(error); process.exitCode = 1; });
