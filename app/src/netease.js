'use strict';
// Public lyrics only: no login, cookies, account credentials or translated lyrics.
const { isMetadataLine } = require('./lrc');
const { fetchJSON } = require('./http');
const { cleanTitle, rankCandidates } = require('./matching');

const HOSTS = ['https://interface.music.163.com', 'https://interface3.music.163.com', 'https://music.163.com'];
const HEADERS = { Referer: 'https://music.163.com/', 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/131.0 Safari/537.36' };

function parseNetEaseLyrics(text) {
  const raw = String(text || '');
  const offset = Number(raw.match(/\[offset:([+-]?\d+)\]/i)?.[1] || 0) / 1000;
  const lines = [];
  const pattern = /\[(\d+):(\d{1,2})(?:[.:](\d{1,3}))?\]/g;
  for (const line of raw.split(/\r?\n/)) {
    const tags = [...line.matchAll(pattern)];
    const text = line.replace(/\[[^\]]*\]/g, '').trim();
    if (!text || isMetadataLine(text) || /^(纯音乐[，,]?请欣赏|此歌曲为没有填词的纯音乐|instrumental)$/i.test(text)) continue;
    for (const tag of tags) {
      if (Number(tag[2]) >= 60) continue;
      const time = Number(tag[1]) * 60 + Number(tag[2]) + Number('0.' + (tag[3] || '0')) + offset;
      lines.push({ time: Math.max(0, time), text });
    }
  }
  return lines.sort((a, b) => a.time - b.time);
}

async function fetchNetEase(title, artist, duration, signal) {
  if (!String(title || '').trim()) return null;
  const deadline = Date.now() + 14000;
  async function request(path) {
    for (const host of HOSTS) {
      signal?.throwIfAborted();
      const remaining = deadline - Date.now();
      if (remaining <= 0) return null;
      try {
        const data = await fetchJSON(host + path, { headers: HEADERS, signal }, Math.min(4000, remaining));
        if (data?.code === 200) return data;
      } catch (_) { signal?.throwIfAborted(); }
    }
    return null;
  }
  const queries = [...new Set([`${cleanTitle(title)} ${artist || ''}`.trim(), cleanTitle(title)])];
  const tried = new Set();
  for (const query of queries) {
    const data = await request('/api/search/get/web?s=' + encodeURIComponent(query) + '&type=1&offset=0&limit=20');
    const songs = data?.result?.songs;
    if (!Array.isArray(songs)) continue;
    const candidates = rankCandidates(songs.filter(song => song && /^\d+$/.test(String(song.id))).map(song => ({
      title: song.name, artists: (song.artists || song.ar || []).map(item => item.name),
      duration: Number(song.duration || song.dt || 0) / 1000, value: song,
    })), title, artist, duration).slice(0, 3);
    for (const { value: song } of candidates) {
      if (tried.has(song.id)) continue;
      tried.add(song.id);
      const data = await request('/api/song/lyric?id=' + encodeURIComponent(song.id) + '&lv=-1&kv=-1&tv=-1&rv=-1');
      if (!data || data.nolyric || data.uncollected) continue;
      const lines = parseNetEaseLyrics(data.lrc?.lyric);
      if (lines.length) return { lines, songId: song.id, artist: (song.artists || song.ar || []).map(item => item.name).filter(Boolean).join(', ') };
    }
  }
  return null;
}

module.exports = { fetchNetEase };
