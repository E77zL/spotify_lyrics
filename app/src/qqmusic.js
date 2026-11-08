'use strict';
// Upstream QQ endpoints and signing, with shared cancellation and JSON deadlines.
const crypto = require('crypto');
const { parseLRC, isMetadataLine } = require('./lrc');
const { fetchJSON } = require('./http');
const { cleanTitle, rankCandidates } = require('./matching');
const HEADERS = { Referer: 'https://y.qq.com/', 'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36' };

async function fetchQQ(title, artist, signal, duration) {
  const keyword = [cleanTitle(title), artist].filter(Boolean).join(' ');
  const data = await fetchJSON('https://c.y.qq.com/soso/fcgi-bin/client_search_cp?w=' + encodeURIComponent(keyword) + '&format=json&platform=yqq&p=1&n=8', { headers: HEADERS, signal });
  const songs = data?.data?.song?.list;
  if (!Array.isArray(songs) || !songs.length) return null;
  const candidates = rankCandidates(songs.filter(song => song?.songmid).map(song => ({
    title: song.songname, artists: (song.singer || []).map(singer => singer.name), duration: song.interval, value: song,
  })), title, artist, duration).slice(0, 3);
  for (const { value: song } of candidates) {
    const lines = await songLyrics(song.songmid, signal);
    if (lines.length) return { lines, artist: song.singer.map(singer => singer.name).filter(Boolean).join(', ') };
  }
  return null;
}

async function songLyrics(songMID, signal) {
  signal?.throwIfAborted();
  const request = {
    comm: { cv: 4747474, ct: 24, format: 'json', inCharset: 'utf-8', outCharset: 'utf-8', notice: 0, platform: 'yqq.json', needNewCode: 1, uin: 0, g_tk: 0 },
    req_1: { module: 'music.musichallSong.PlayLyricInfo', method: 'GetPlayLyricInfo', param: { songMID, songType: 0 } },
  };
  // Public upstream web signing constant; no account credentials are involved.
  request.comm.sign = crypto.createHash('md5').update('zza' + JSON.stringify(request) + 'eJ5l4tJN3NysFOW4n9Q0kXivNaHfY').digest('hex');
  const result = await fetchJSON('https://u.y.qq.com/cgi-bin/musicu.fcg', {
    method: 'POST', headers: { ...HEADERS, 'Content-Type': 'application/json' }, body: JSON.stringify(request), signal,
  });
  const raw = result?.req_1?.data?.lyric;
  if (!raw) return [];
  const lines = parseLRC(Buffer.from(raw, 'base64').toString('utf8')).filter(line => !isMetadataLine(line.text));
  return lines;
}

module.exports = { fetchQQ };
