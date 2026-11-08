// Shared LRC parsing for the lyric providers. Keep this module independent
// to avoid circular require dependencies between providers.

// Parse LRC into [{ time: 12.34, text: "This lyric line" }, ...].
// Time tags use [mm:ss.xx] or [mm:ss.xxx].
function parseLRC(lrcText) {
  const lines = [];
  if (!lrcText) return lines;
  const timeTagPattern = /\[(\d{2}):(\d{2})(?:\.(\d{2,3}))?\]/g;

  for (const rawLine of lrcText.split('\n')) {
    const tags = [...rawLine.matchAll(timeTagPattern)];
    if (tags.length === 0) continue;

    const text = rawLine.replace(timeTagPattern, '').trim();
    if (!text) continue; // Skip timestamps without lyric text.

    for (const tag of tags) {
      const minutes = Number(tag[1]);
      const seconds = Number(tag[2]);
      const fraction = tag[3] ? Number(`0.${tag[3]}`) : 0;
      const time = minutes * 60 + seconds + fraction;
      lines.push({ time, text });
    }
  }

  lines.sort((a, b) => a.time - b.time);
  return lines;
}

// Filter writing, production and publishing credits so they are not highlighted
// as lyrics. Use whitespace plus a colon instead of a word boundary, since
// Chinese credit labels do not have a JavaScript word boundary before a colon.
function isMetadataLine(text) {
  if (!text) return true;
  const t = text.trim().replace(/\s+/g, '');
  return (
    /^(作词|作曲|编曲|制作人|监制|混音|母带|出品|发行|钢琴|吉他|贝斯|鼓|键盘|弦乐|录音|配唱|和声|合声|视觉|封面|策划|统筹|宣发|原唱|翻唱)(编写)?\s*[:：]/.test(t) ||
    /^(词|曲)\s*[:：]/.test(t) ||
    /^(OP|SP|Studio|Label|Published)\s*[:：]/i.test(t)
  );
}

module.exports = { parseLRC, isMetadataLine };
