'use strict';

// Search relevance is not identity: every source must verify its returned metadata.
const normalize = text => String(text || '').normalize('NFKD').toLowerCase().replace(/[\s\p{M}\p{P}\p{S}]/gu, '');
const cleanTitle = text => String(text || '')
  .replace(/\s*[\[(]\s*(?:prod\.?(?:\s+by)?|produced\s+by|prodby)\s*[:：]?\s+[^)\]]+[\])]/gi, '')
  .replace(/\s*\((?:feat\.?|ft\.?|featuring)\s+[^)]*\)/gi, '')
  .replace(/\s*(?:[-–]\s*)?(?:\(?\d{4}\s*)?remaster(?:ed)?(?:\s*\d{4})?\)?\s*$/i, '').trim();

function similarity(a, b) {
  a = normalize(a); b = normalize(b);
  if (!a || !b) return 0;
  if (a === b) return 1;
  const row = Array.from({ length: b.length + 1 }, (_, i) => i);
  for (let i = 1; i <= a.length; i++) {
    let prior = row[0]; row[0] = i;
    for (let j = 1; j <= b.length; j++) {
      const old = row[j];
      row[j] = Math.min(row[j] + 1, row[j - 1] + 1, prior + Number(a[i - 1] !== b[j - 1]));
      prior = old;
    }
  }
  return 1 - row[b.length] / Math.max(a.length, b.length);
}

function artistNames(value) {
  const text = String(value || '').replace(/[()[\]]/g, ' ');
  // Preserve words within an artist's name; never match arbitrary substrings.
  return text.split(/[,;，；、]|\s+\/\s+|\s+(?:&|feat\.?|ft\.?|featuring)\s+/i).map(normalize).filter(Boolean);
}

function matchesArtist(artists, artist) {
  if (!String(artist || '').trim()) return true;
  const primary = artistNames(artist)[0];
  if (!primary || !Array.isArray(artists)) return false;
  return artists.some(name => normalize(name) === normalize(artist) || artistNames(name).includes(primary));
}

const versions = text => [...String(text || '').matchAll(/\b(live|remix|instrumental|karaoke|acoustic|sped\s*up|slowed)\b|伴奏|现场/gi)]
  .map(match => normalize(match[0])).sort().join(',');

// Providers adapt their data to { title, artists: string[], duration: seconds, value }.
function rankCandidates(candidates, title, artist, duration) {
  const wantedTitle = cleanTitle(title);
  const wantedVersion = versions(wantedTitle);
  const hasArtist = !!String(artist || '').trim();
  const ranked = candidates.filter(item => item && matchesArtist(item.artists, artist) && versions(cleanTitle(item.title)) === wantedVersion)
    .map(item => {
      const titleScore = similarity(cleanTitle(item.title), wantedTitle);
      const seconds = Number(item.duration);
      const timing = seconds > 0 && duration > 0 ? Math.max(0, 1 - Math.abs(seconds - duration) / 20) : 0.5;
      return { item, titleScore, seconds, score: titleScore * 0.9 + timing * 0.1 };
    })
    .filter(({ item, titleScore, seconds }) => hasArtist ? titleScore >= 0.8 :
      titleScore === 1 && seconds > 0 && duration > 0 && Math.abs(seconds - duration) <= 2 && item.artists?.some(normalize))
    .sort((a, b) => b.score - a.score)
    .map(item => item.item);
  if (!hasArtist) {
    // Local files can have no artist tag. Require a unique identity near their duration.
    // Include records without timed lyrics in this check so a cover is not hidden.
    const identities = new Set(ranked.map(item => item.artists.map(normalize).filter(Boolean).sort().join(',')));
    if (identities.size !== 1) return [];
  }
  return ranked;
}

module.exports = { cleanTitle, rankCandidates };
