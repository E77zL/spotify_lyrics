'use strict';
// Providers run strictly in order. A failed request must not skip later sources.
const { parseLRC, isMetadataLine } = require('./lrc');
const { fetchQQ } = require('./qqmusic');
const { fetchNetEase } = require('./netease');
const { fetchJSON } = require('./http');
const { cleanTitle, rankCandidates } = require('./matching');

const HEADERS = { 'User-Agent': 'spotify-lyrics-hud/5.1.0 (https://github.com/Fanbeixi/spotify-lyrics-HUD_CN)' };
function syncedLines(text) { return parseLRC(text).filter(line => !isMetadataLine(line.text)); }
function matchedRecords(records, title, artist, duration) {
  return rankCandidates(records.filter(Boolean).map(item => ({
    title: item.trackName, artists: [item.artistName], duration: item.duration, value: item,
  })), title, artist, duration);
}

async function exactMatch(title, artist, duration, signal) {
  if (!String(artist || '').trim()) return null;
  const url = new URL('https://lrclib.net/api/get');
  url.searchParams.set('track_name', title);
  url.searchParams.set('artist_name', artist);
  if (duration) url.searchParams.set('duration', String(Math.round(duration)));
  const data = await fetchJSON(url.toString(), { headers: HEADERS, signal });
  const [match] = matchedRecords([data], title, artist, duration);
  return match ? { lines: syncedLines(match.value.syncedLyrics), artist: match.value.artistName } : null;
}

async function search(title, artist, duration, signal) {
  const url = new URL('https://lrclib.net/api/search');
  url.searchParams.set('track_name', cleanTitle(title));
  if (artist) url.searchParams.set('artist_name', artist);
  const data = await fetchJSON(url.toString(), { headers: HEADERS, signal });
  if (!Array.isArray(data)) return null;
  for (const { value } of matchedRecords(data, title, artist, duration)) {
    const lines = syncedLines(value.syncedLyrics);
    if (lines.length) return { lines, artist: value.artistName };
  }
  return null;
}

async function fetchLyrics(title, artist, duration, signal) {
  const providers = [
    ['LRCLIB', () => exactMatch(title, artist, duration, signal), 'lrclib'],
    ['LRCLIB', () => search(title, artist, duration, signal), 'lrclib'],
    ['QQ', () => fetchQQ(title, artist, signal, duration), 'qq'],
    ['NetEase', () => fetchNetEase(title, artist, duration, signal), 'netease'],
  ];
  // Strict fallback order, including failures; cancellation stops the chain.
  for (const [name, lookup, source] of providers) {
    signal?.throwIfAborted();
    try {
      const result = await lookup();
      signal?.throwIfAborted();
      if (result?.lines?.length) return { found: true, lines: result.lines, source, artist: result.artist || artist };
    } catch (error) {
      signal?.throwIfAborted();
      console.error(name + ':', error.message);
    }
  }
  return { found: false, lines: [] };
}

module.exports = { fetchLyrics, parseLRC };
