/* Spotify Lyrics control. MIT. Integration v1.2. */
(function () {
  'use strict';
  if (window.__spotifyLyricsHudControl) return;
  window.__spotifyLyricsHudControl = true;
  const token = '__HUD_CONNECTION_TOKEN__';
  const icon = '<svg role="img" height="16" width="16" viewBox="0 0 16 16" aria-hidden="true" style="transform:translateY(1.5px);overflow:visible"><rect x="1" y="1" width="14" height="14" rx="1.5" fill="none" stroke="currentColor" stroke-width="1.3"/><path d="M7.15 4.5h1.7L12 12h-1.65l-.65-1.75H6.3L5.65 12H4L7.15 4.5zm-.35 4.45h2.4L8 5.75 6.8 8.95z" fill="currentColor" fill-rule="evenodd"/></svg>';
  let socket = null, button = null, enabled = false, reconnect = null, stopped = false, observer = null, bootTimer = null;
  let lastSample = null, pendingSeek = null, seekTimer = null;
  let customizationMenu = null;
  function clearPendingSeek() {
    clearTimeout(seekTimer); seekTimer = null; pendingSeek = null;
  }
  function seekPosition(seek, now, duration) {
    return Math.max(0, Math.min(duration, seek.target + (seek.playing ? now - seek.at : 0)));
  }
  function publishPlayback(checkOnly = false) {
    if (stopped || !socket || socket.readyState !== WebSocket.OPEN) return;
    let playing = false;
    try {
      const player = Spicetify.Player;
      const item = player.data?.item;
      const meta = item?.metadata;
      playing = !!player.isPlaying();
      const valid = !!meta?.title && /^spotify:(track|local):/.test(item.uri || '');
      const now = Date.now(), duration = valid ? player.getDuration() : 0;
      let position = valid ? Math.max(0, player.getProgress()) : 0, seek = false;
      if (pendingSeek) {
        const target = seekPosition(pendingSeek, now, duration);
        if (!valid || item.uri !== pendingSeek.uri || now >= pendingSeek.expires ||
            (!pendingSeek.announce && Math.abs(position - target) <= 250)) {
          seek = valid && item.uri === pendingSeek.uri && Math.abs(position - target) > 500;
          clearPendingSeek();
        } else {
          // Spicetify.seek returns before Spotify refreshes its playback cache.
          // Keep the requested clock until acknowledgement; never send the old position back.
          position = target; seek = pendingSeek.announce;
          pendingSeek.target = target; pendingSeek.at = now; pendingSeek.playing = playing;
          pendingSeek.announce = false;
          if (!seekTimer) seekTimer = setTimeout(() => {
            seekTimer = null; publishPlayback(true);
          }, 50);
          if (checkOnly === true) return;
        }
      } else if (checkOnly === true) return;
      const artists = valid ? Object.keys(meta).filter(k => /^artist_name(?::\d+)?$/.test(k))
        .sort((a, b) => Number(a.split(':')[1] || 0) - Number(b.split(':')[1] || 0))
        .map(k => meta[k]).filter(Boolean).join(', ') : '';
      const snapshot = valid ? { found: true, title: meta.title, artist: artists,
        durationSec: duration / 1000, positionSec: position / 1000,
        isPlaying: playing, sentAt: now, seek } : { found: false, sentAt: now };
      socket.send(JSON.stringify({ type: 'playback', snapshot }));
      lastSample = snapshot;
    } catch {
      if (checkOnly === true) clearPendingSeek();
      /* Player may be between tracks; retry without replacing the clock. */
    }
  }
  function checkSeek() {
    if (!lastSample?.found) return;
    try {
      const expected = lastSample.positionSec * 1000 + (lastSample.isPlaying ? Date.now() - lastSample.sentAt : 0);
      if (Math.abs(Spicetify.Player.getProgress() - expected) > 200) publishPlayback();
    } catch { }
  }
  async function playerAction(state) {
    let requestedSeek = null;
    try {
      const player = Spicetify.Player;
      if (state.action === 'seek' && [-5000, 5000].includes(state.deltaMs)) {
        const now = Date.now(), duration = player.getDuration(), uri = player.data?.item?.uri;
        const position = pendingSeek && pendingSeek.uri === uri ? seekPosition(pendingSeek, now, duration) : player.getProgress();
        if (!uri || !Number.isFinite(position) || !Number.isFinite(duration) || duration <= 0) return;
        requestedSeek = { uri, target: Math.max(0, Math.min(duration, position + state.deltaMs)),
          at: now, expires: now + 1500, playing: !!player.isPlaying(), announce: true };
        pendingSeek = requestedSeek;
        const result = player.seek(requestedSeek.target);
        publishPlayback();
        await result;
        return;
      } else if (state.action === 'volume' && [-0.05, 0.05].includes(state.delta)) {
        const volume = player.getVolume();
        if (!Number.isFinite(volume)) return;
        await player.setVolume(Math.max(0, Math.min(1, volume + state.delta)));
      } else if (state.action === 'toggle') await player.togglePlay();
      else return;
      publishPlayback();
    } catch {
      // An older failed command must not undo a newer seek or a track change.
      if (requestedSeek && pendingSeek === requestedSeek) { clearPendingSeek(); publishPlayback(); }
    }
  }
  function connect() {
    if (stopped) return;
    try {
      const ws = new WebSocket('ws://127.0.0.1:27946/control?token=' + token);
      socket = ws;
      ws.onopen = publishPlayback;
      ws.onmessage = event => {
        if (socket !== ws) return;
        try {
          const state = JSON.parse(event.data);
          if (state.type === 'state') { enabled = state.enabled === true; button.active = enabled; }
          else if (state.type === 'getPlayback') publishPlayback();
          else if (state.type === 'playerAction') void playerAction(state);
        } catch (_) { }
      };
      ws.onerror = () => ws.close();
      ws.onclose = () => { if (socket !== ws) return; clearPendingSeek(); socket = null; button.active = false; clearTimeout(reconnect); if (!stopped) reconnect = setTimeout(connect, 1500); };
    } catch (_) { if (!stopped) reconnect = setTimeout(connect, 1500); }
  }
  function boot() {
    if (stopped) return;
    if (!window.Spicetify?.Playbar?.Button) { bootTimer = setTimeout(boot, 400); return; }
    button = new Spicetify.Playbar.Button('Desktop lyrics', icon, () => {
      if (!socket || socket.readyState !== WebSocket.OPEN) {
        Spicetify.showNotification?.('Spotify Lyrics is starting. Try again in a moment.', true);
        return;
      }
      socket.send(JSON.stringify({ type: 'setEnabled', enabled: !enabled }));
    }, false, false, true);
    let microphone = null;
    function positionButton() {
      if (microphone?.isConnected && button.element?.isConnected && microphone.previousElementSibling === button.element) return;
      microphone = document.querySelector?.('[data-testid="lyrics-button"]');
      if (microphone?.parentElement && button.element && microphone.previousElementSibling !== button.element) microphone.parentElement.insertBefore(button.element, microphone);
    }
    positionButton();
    if (Spicetify.Menu?.Item && Spicetify.Config?.custom_apps?.includes('marketplace')) {
      customizationMenu = new Spicetify.Menu.Item('Customization', false, () => {
        Spicetify.Platform?.History?.push('/marketplace');
      });
      customizationMenu.register();
    }
    if (typeof MutationObserver !== 'undefined') { observer = new MutationObserver(positionButton); observer.observe(document.body, { childList: true, subtree: true }); }
    connect();
    const player = Spicetify.Player;
    player?.addEventListener?.('songchange', publishPlayback);
    player?.addEventListener?.('onplaypause', publishPlayback);
    player?.addEventListener?.('onprogress', checkSeek);
  }
  window.addEventListener('beforeunload', () => {
    stopped = true; clearPendingSeek(); clearTimeout(bootTimer); clearTimeout(reconnect);
    Spicetify.Player?.removeEventListener?.('songchange', publishPlayback);
    Spicetify.Player?.removeEventListener?.('onplaypause', publishPlayback);
    Spicetify.Player?.removeEventListener?.('onprogress', checkSeek);
    if (observer) observer.disconnect(); if (socket) socket.close();
    customizationMenu?.deregister();
  });
  boot();
})();
