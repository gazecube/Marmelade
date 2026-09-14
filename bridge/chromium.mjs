import { access } from 'node:fs/promises';
import { constants } from 'node:fs';
import { delimiter, join } from 'node:path';
import { spawn } from 'node:child_process';
import { homedir } from 'node:os';

const PLAYER_STATE = `(() => {
  const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
  const audio = document.querySelector('audio');
  const item = music && music.nowPlayingItem;
  const attributes = item && (item.attributes || item);
  const metadata = navigator.mediaSession && navigator.mediaSession.metadata;
  const kit = window.MusicKit || {};
  const text = selectors => {
    for (const selector of selectors) {
      const node = document.querySelector(selector);
      if (node && node.textContent.trim()) return node.textContent.trim();
    }
    return '';
  };
  const normalizeShuffle = value => {
    const textValue = String(value ?? '').toLowerCase();
    if (value === true || value === 1 || textValue === '1' || textValue === 'true' ||
        textValue === 'on' || textValue === 'songs' || textValue === 'shuffle') return true;
    return false;
  };
  const normalizeRepeat = value => {
    const textValue = String(value ?? '').toLowerCase();
    if (textValue === 'one' || textValue === 'single' || textValue === 'repeatone') return 'one';
    if (textValue === 'all' || textValue === 'repeatall') return 'all';
    if (value === (kit.PlayerRepeatMode && kit.PlayerRepeatMode.one) ||
        value === (kit.RepeatMode && kit.RepeatMode.one)) return 'one';
    if (value === (kit.PlayerRepeatMode && kit.PlayerRepeatMode.all) ||
        value === (kit.RepeatMode && kit.RepeatMode.all)) return 'all';
    if (value === 2) return 'all';
    if (value === 1) return 'one';
    return 'off';
  };
  const controlState = pattern => {
    const button = [...document.querySelectorAll('button')].find(node =>
      pattern.test(node.getAttribute('aria-label') || node.title || node.dataset.testid || ''));
    if (!button) return false;
    const label = (button.getAttribute('aria-label') || button.title || '').toLowerCase();
    const pressed = button.getAttribute('aria-pressed');
    const checked = button.getAttribute('aria-checked');
    const state = (button.dataset.state || '').toLowerCase();
    if (pressed === 'true' || checked === 'true' || state === 'on' || state === 'active' ||
        /turn off|disable/.test(label)) return true;
    if (pressed === 'false' || checked === 'false' || state === 'off' ||
        /turn on|enable/.test(label)) return false;
    return /selected|active|enabled|is-active/.test(button.className || '');
  };
  const rawPlaybackState = music && music.playbackState;
  const musicIsPlaying = music && (music.isPlaying === true || rawPlaybackState === 2 ||
    String(rawPlaybackState).toLowerCase() === 'playing');
  const rawShuffle = music ?
    (music.shuffleMode ?? music.queue?.shuffleMode ?? music.player?.shuffleMode ?? false) : false;
  const rawRepeat = music ?
    (music.repeatMode ?? music.queue?.repeatMode ?? music.player?.repeatMode ?? 'off') : 'off';
  const rawAutoplay = music ?
    (music.autoplayEnabled ?? music.isAutoplayEnabled ?? music.queue?.autoplayEnabled ?? null) : null;
  const rawAutomix = music ?
    (music.automixEnabled ?? music.isAutomixEnabled ?? music.queue?.automixEnabled ?? null) : null;
  return {
    url: location.href,
    pageTitle: document.title,
    loggedIn: music ? music.isAuthorized !== false :
      !document.querySelector('[data-testid="account-menu-sign-in"], button[aria-label*="Sign In" i]'),
    musicKitAvailable: Boolean(music),
    playback: music ? (musicIsPlaying ? 'playing' : 'paused') :
      (audio ? (audio.paused ? 'paused' : 'playing') : 'unavailable'),
    position: music ? (music.currentPlaybackTime || 0) : (audio ? audio.currentTime : 0),
    duration: music ? (music.currentPlaybackDuration || 0) :
      (audio && Number.isFinite(audio.duration) ? audio.duration : 0),
    volume: music ? music.volume : (audio ? audio.volume : 0.6),
    shuffleEnabled: normalizeShuffle(rawShuffle),
    repeatMode: normalizeRepeat(rawRepeat),
    autoplayEnabled: rawAutoplay == null ? controlState(/autoplay/i) : Boolean(rawAutoplay),
    automixEnabled: rawAutomix == null ? controlState(/auto\s*mix|automix/i) : Boolean(rawAutomix),
    currentTrack: attributes ? {
      id: item.id || null,
      title: attributes.name || attributes.title || '',
      artist: attributes.artistName || attributes.artist || '',
      album: attributes.albumName || attributes.album || '',
      artworkURL: attributes.artworkURL || (attributes.artwork && attributes.artwork.url) || null
    } : metadata ? {
      title: metadata.title || '', artist: metadata.artist || '',
      album: metadata.album || '',
      artworkURL: metadata.artwork && metadata.artwork.length ? metadata.artwork[metadata.artwork.length - 1].src : null
    } : {
      title: text(['.web-chrome-playback-lcd__song-name-scroll-inner-text', '[data-testid="lcd-meta-title"]']),
      artist: text(['.web-chrome-playback-lcd__sub-copy-scroll-inner-text', '[data-testid="lcd-meta-artist"]']),
      album: '', artworkURL: null
    }
  };
})()`;

function actionScript(action, value) {
  const labels = {
    play: ['play', 'play track'],
    pause: ['pause', 'pause track'],
    next: ['next', 'next track', 'skip forward'],
    previous: ['previous', 'previous track', 'skip back'],
  }[action] || [];
  if (action === 'seek') return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    if (music && typeof music.seekToTime === 'function') {
      await music.seekToTime(${Number(value)}); return true;
    }
    const audio = document.querySelector('audio');
    if (audio) { audio.currentTime = ${Number(value)}; return true; }
    return false;
  })()`;
  if (action === 'volume') return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    if (music) { music.volume = ${Number(value)}; return true; }
    const audio = document.querySelector('audio');
    if (audio) audio.volume = ${Number(value)};
    const slider = [...document.querySelectorAll('input[type="range"]')]
      .find(x => /volume/i.test(x.getAttribute('aria-label') || x.title || ''));
    if (slider) {
      const setter = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
      setter.call(slider, String(${Number(value)}));
      slider.dispatchEvent(new Event('input', {bubbles:true}));
      slider.dispatchEvent(new Event('change', {bubbles:true}));
    }
    return Boolean(audio || slider);
  })()`;
  if (action === 'shuffle') return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    const desired = ${JSON.stringify(Boolean(value))};
    const kit = window.MusicKit || {};
    const desiredValue = kit.PlayerShuffleMode?.songs ?? kit.ShuffleMode?.songs ?? 1;
    const offValue = kit.PlayerShuffleMode?.off ?? kit.ShuffleMode?.off ?? 0;
    if (music) {
      try {
        if (typeof music.setShuffleMode === 'function') {
          await music.setShuffleMode(desired ? desiredValue : offValue);
          return true;
        }
      } catch {}
      try {
        if (typeof music.changeShuffleMode === 'function') {
          await music.changeShuffleMode(desired ? desiredValue : offValue);
          return true;
        }
      } catch {}
      try {
        if ('shuffleMode' in music) {
          music.shuffleMode = desired ? desiredValue : offValue;
          return true;
        }
      } catch {}
      try {
        if (music.queue && 'shuffleMode' in music.queue) {
          music.queue.shuffleMode = desired ? desiredValue : offValue;
          return true;
        }
      } catch {}
    }
    const button = [...document.querySelectorAll('button')].find(node =>
      /shuffle/i.test(node.getAttribute('aria-label') || node.title || node.dataset.testid || ''));
    if (button) { button.click(); return true; }
    return false;
  })()`;
  if (action === 'repeat') return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    const desired = ${JSON.stringify(String(value || 'off'))};
    const kit = window.MusicKit || {};
    const map = {
      off: kit.PlayerRepeatMode?.none ?? kit.PlayerRepeatMode?.off ?? kit.RepeatMode?.none ?? kit.RepeatMode?.off ?? 0,
      all: kit.PlayerRepeatMode?.all ?? kit.RepeatMode?.all ?? 2,
      one: kit.PlayerRepeatMode?.one ?? kit.RepeatMode?.one ?? 1,
    };
    if (music) {
      try {
        if (typeof music.setRepeatMode === 'function') {
          await music.setRepeatMode(map[desired]);
          return true;
        }
      } catch {}
      try {
        if (typeof music.changeRepeatMode === 'function') {
          await music.changeRepeatMode(map[desired]);
          return true;
        }
      } catch {}
      try {
        if ('repeatMode' in music) {
          music.repeatMode = map[desired];
          return true;
        }
      } catch {}
      try {
        if (music.queue && 'repeatMode' in music.queue) {
          music.queue.repeatMode = map[desired];
          return true;
        }
      } catch {}
    }
    const button = [...document.querySelectorAll('button')].find(node =>
      /repeat/i.test(node.getAttribute('aria-label') || node.title || node.dataset.testid || ''));
    if (button) { button.click(); return true; }
    return false;
  })()`;
  if (action === 'autoplay' || action === 'automix') return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    const desired = ${JSON.stringify(Boolean(value))};
    const property = ${JSON.stringify(action === 'autoplay' ? 'autoplayEnabled' : 'automixEnabled')};
    const methodStem = ${JSON.stringify(action === 'autoplay' ? 'Autoplay' : 'Automix')};
    if (music) {
      try {
        const setter = music['set' + methodStem + 'Enabled'];
        if (typeof setter === 'function') { await setter.call(music, desired); return true; }
      } catch {}
      try {
        if (property in music) { music[property] = desired; return true; }
      } catch {}
      try {
        if (music.queue && property in music.queue) { music.queue[property] = desired; return true; }
      } catch {}
    }
    const pattern = ${action === 'autoplay' ? '/autoplay/i' : '/auto\\s*mix|automix/i'};
    const button = [...document.querySelectorAll('button')].find(node =>
      pattern.test(node.getAttribute('aria-label') || node.title || node.dataset.testid || ''));
    if (!button) return false;
    const pressed = button.getAttribute('aria-pressed');
    const checked = button.getAttribute('aria-checked');
    const label = (button.getAttribute('aria-label') || button.title || '').toLowerCase();
    const current = pressed === 'true' || checked === 'true' || /turn off|disable/.test(label) ||
      /selected|active|enabled|is-active/.test(button.className || '');
    if (current !== desired) button.click();
    return true;
  })()`;
  return `(async () => {
    const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
    if (music) {
      if (${JSON.stringify(action)} === 'play') await music.play();
      if (${JSON.stringify(action)} === 'pause') music.pause();
      if (${JSON.stringify(action)} === 'next') await music.skipToNextItem();
      if (${JSON.stringify(action)} === 'previous') await music.skipToPreviousItem();
      return true;
    }
    const labels = ${JSON.stringify(labels)};
    const button = [...document.querySelectorAll('button')].find(node => {
      const name = (node.getAttribute('aria-label') || node.title || node.dataset.testid || '')
        .trim().toLowerCase().replace(/\s+/g, ' ');
      return labels.includes(name);
    });
    if (button) { button.click(); return true; }
    const audio = document.querySelector('audio');
    if (audio && ${JSON.stringify(action)} === 'play') { audio.play(); return true; }
    if (audio && ${JSON.stringify(action)} === 'pause') { audio.pause(); return true; }
    return false;
  })()`;
}

async function executableFromPath(name) {
  for (const directory of (process.env.PATH || '').split(delimiter)) {
    const candidate = join(directory, name);
    try { await access(candidate, constants.X_OK); return candidate; } catch {}
  }
  return null;
}

async function findChromium() {
  if (process.env.MOTIF_APPLE_MUSIC_CHROMIUM) return process.env.MOTIF_APPLE_MUSIC_CHROMIUM;
  for (const name of ['chromium', 'chromium-browser', 'google-chrome-stable', 'google-chrome', 'chrome']) {
    const path = await executableFromPath(name);
    if (path) return path;
  }
  return null;
}

class CdpSocket {
  constructor(url) {
    this.nextId = 1;
    this.pending = new Map();
    this.socket = new WebSocket(url);
    this.ready = new Promise((resolve, reject) => {
      this.socket.addEventListener('open', resolve, { once: true });
      this.socket.addEventListener('error', reject, { once: true });
    });
    this.socket.addEventListener('message', event => {
      const message = JSON.parse(event.data);
      if (!message.id || !this.pending.has(message.id)) return;
      const { resolve, reject } = this.pending.get(message.id);
      this.pending.delete(message.id);
      if (message.error) reject(new Error(message.error.message));
      else resolve(message.result);
    });
  }
  async send(method, params = {}) {
    await this.ready;
    const id = this.nextId++;
    const result = new Promise((resolve, reject) => this.pending.set(id, {resolve, reject}));
    this.socket.send(JSON.stringify({ id, method, params }));
    return result;
  }
  close() { this.socket.close(); }
}

export class ChromiumAppleMusic {
  constructor({ port = 17877 } = {}) {
    this.port = port;
    this.profile = process.env.MOTIF_APPLE_MUSIC_PROFILE ||
      join(homedir(), '.local', 'share', 'motif-apple-music', 'chromium');
    this.child = null;
    this.cdp = null;
    this.error = null;
    this.targetId = null;
    this.windowId = null;
    this.stopping = false;
    this.restartTimer = null;
    this.everConnected = false;
    this.visible = process.env.MOTIF_APPLE_MUSIC_BROWSER_VISIBLE === '1';
    this.temporaryVisible = false;
    this.switchingToVisible = false;
  }

  async start() {
    if (process.env.MOTIF_APPLE_MUSIC_BROWSER_DISABLE === '1') {
      this.error = 'browser_disabled'; return;
    }
    const executable = await findChromium();
    if (!executable) { this.error = 'chromium_not_found'; return; }
    this.error = null;
    const arguments_ = [
      `--remote-debugging-port=${this.port}`,
      `--user-data-dir=${this.profile}`,
      '--autoplay-policy=no-user-gesture-required',
      '--disable-background-media-suspend',
      '--disable-background-timer-throttling',
      '--app=https://music.apple.com/',
    ];
    if (!this.visible) arguments_.push('--headless=new');
    if (process.env.MOTIF_APPLE_MUSIC_ENABLE_GPU !== '1')
      arguments_.push('--disable-gpu');
    this.child = spawn(executable, arguments_, { stdio: 'ignore' });
    this.child.once('error', error => {
      this.error = `chromium_launch_failed: ${error.message}`;
    });
    this.child.once('exit', () => {
      this.child = null;
      this.cdp?.close();
      this.cdp = null;
      if (!this.stopping && this.everConnected) {
        if (this.switchingToVisible) {
          this.switchingToVisible = false;
        } else if (this.temporaryVisible) {
          this.visible = false;
          this.temporaryVisible = false;
        }
        this.error = 'restarting_browser';
        this.restartTimer = setTimeout(() => {
          this.restartTimer = null;
          this.start().catch(error => { this.error = error.message; });
        }, 1000);
      }
    });
    for (let attempt = 0; attempt < 100; attempt += 1) {
      try {
        await this.connect();
        this.everConnected = true;
        return;
      } catch { await new Promise(r => setTimeout(r, 100)); }
    }
    this.error = 'chromium_debugging_unavailable';
  }

  async connect() {
    const pages = await fetch(`http://127.0.0.1:${this.port}/json/list`).then(r => r.json());
    const page = pages.find(item => item.type === 'page' && /music\.apple\.com/.test(item.url));
    if (!page) throw new Error('Apple Music page not found');
    this.cdp?.close();
    this.cdp = new CdpSocket(page.webSocketDebuggerUrl);
    await this.cdp.ready;
    this.targetId = page.id;
    try {
      const window = await this.cdp.send('Browser.getWindowForTarget', {targetId:this.targetId});
      this.windowId = window.windowId;
    } catch {}
  }

  async evaluate(expression) {
    if (!this.cdp) await this.connect();
    const result = await this.cdp.send('Runtime.evaluate', {
      expression, awaitPromise: true, returnByValue: true,
    });
    if (result.exceptionDetails) throw new Error(result.exceptionDetails.text);
    return result.result.value;
  }

  async state() { return this.evaluate(PLAYER_STATE); }
  async action(name, value) { return this.evaluate(actionScript(name, value)); }
  async libraryTracks(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/me/library/songs', {limit:${safeLimit}});
      const rows = result?.data?.data || result?.data || [];
      return rows.map(item => ({id:item.id,
        title:item.attributes?.name || '', artist:item.attributes?.artistName || '',
        album:item.attributes?.albumName || '',
        artworkURL:item.attributes?.artwork?.url || item.attributes?.artworkURL || '',
        duration:Math.round((item.attributes?.durationInMillis || 0) / 1000)}));
    })()`);
  }
  async libraryCollection(type, limit = 100) {
    const allowed = ['songs','albums','artists','playlists'];
    if (!allowed.includes(type)) throw new Error('Unsupported library collection');
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/me/library/${type}', {limit:${safeLimit}});
      const rows = result?.data?.data || result?.data || [];
      return rows.map(item => ({id:item.id,kind:${JSON.stringify(type.replace(/s$/, ''))},
        catalogId:item.attributes?.playParams?.catalogId || item.attributes?.playParams?.id || item.id,
        title:item.attributes?.name || '',
        artist:item.attributes?.artistName || '',
        album:item.attributes?.albumName || '',
        artworkURL:item.attributes?.artwork?.url || item.attributes?.artworkURL || '',
        trackNumber:item.attributes?.trackNumber || 0,
        discNumber:item.attributes?.discNumber || 0,
        duration:Math.round((item.attributes?.durationInMillis || 0) / 1000)}));
    })()`);
  }
  async recentlyPlayed(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/me/recent/played', {limit:${safeLimit}});
      const rows = result?.data?.data || result?.data || [];
      const seen = new Set();
      return rows.filter(item => {
        const kind = (item.type || '').replace(/^library-/,'').replace(/s$/,'');
        const id = item.attributes?.playParams?.catalogId ||
          item.attributes?.playParams?.id || item.id;
        if (kind !== 'album' || !id || seen.has(id)) return false;
        seen.add(id);
        return true;
      }).map(item => ({id:item.id,kind:'album',
        catalogId:item.attributes?.playParams?.catalogId || item.attributes?.playParams?.id || item.id,
        title:item.attributes?.name || '',artist:item.attributes?.artistName || '',
        album:item.attributes?.albumName || '',
        artworkURL:item.attributes?.artwork?.url || item.attributes?.artworkURL || '',
        duration:Math.round((item.attributes?.durationInMillis || 0) / 1000)}));
    })()`);
  }
  async libraryDetail(type, id, limit = 100) {
    if (!['albums','artists','playlists'].includes(type))
      throw new Error('Unsupported library detail');
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const relationship = ${JSON.stringify(type)} === 'artists' ? 'albums' : 'tracks';
      const base = '/v1/me/library/${type}/' + ${JSON.stringify(String(id))};
      const entityResult = await music.api.music(base, {limit:1,include:relationship});
      const entity = entityResult?.data?.data?.[0] || entityResult?.data?.[0];
      let rows = entity?.relationships?.[relationship]?.data || [];
      if (!rows.length) {
        const relationshipResult = await music.api.music(base + '/' + relationship, {limit:${safeLimit}});
        rows = relationshipResult?.data?.data || relationshipResult?.data || [];
      }
      const discCount = rows.reduce((maximum, item) =>
        Math.max(maximum, item.attributes?.discNumber || 0), 0);
      return {title:entity?.attributes?.name || '${type}',type:relationship,
        containerKind:${JSON.stringify(type.replace(/s$/, ''))},
        containerId:entity?.id || ${JSON.stringify(String(id))},
        containerCatalogId:entity?.attributes?.playParams?.catalogId ||
          entity?.attributes?.playParams?.id || entity?.id || ${JSON.stringify(String(id))},
        items:rows.map(item => ({id:item.id,kind:relationship.replace(/s$/,''),
          catalogId:item.attributes?.playParams?.catalogId || item.attributes?.playParams?.id || item.id,
          title:item.attributes?.name || '',artist:item.attributes?.artistName || '',
          album:item.attributes?.albumName || '',
          trackNumber:item.attributes?.trackNumber || 0,
          discNumber:item.attributes?.discNumber || 0,
          discCount,
          duration:Math.round((item.attributes?.durationInMillis || 0) / 1000)}))};
    })()`);
  }
  async currentView(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      const path = location.pathname;
      const parts = path.split('/').filter(Boolean);
      const kinds = ['playlist','album','song'];
      const kind = kinds.find(value => parts.includes(value)) ||
        (path.includes('/library/songs') ? 'library-songs' : 'page');
      const format = item => ({id:item.id,
        title:item.attributes?.name || item.title || '',
        artist:item.attributes?.artistName || item.artist || '',
        album:item.attributes?.albumName || item.album || '',
        duration:Math.round((item.attributes?.durationInMillis || item.durationInMillis || 0) / 1000)});
      if (music) {
        try {
          if (kind === 'library-songs') {
            const result = await music.api.music('/v1/me/library/songs', {limit:${safeLimit}});
            const rows = result?.data?.data || result?.data || [];
            return {key:path + location.search, type:kind, title:'Library Songs', items:rows.map(format)};
          }
          if (kind === 'playlist' || kind === 'album' || kind === 'song') {
            const kindIndex = parts.indexOf(kind);
            let id = kind === 'song' ? new URLSearchParams(location.search).get('i') : parts[parts.length - 1];
            if (!id && kindIndex >= 0) id = parts[kindIndex + 2] || parts[kindIndex + 1];
            const isLibrary = path.includes('/library/');
            const plural = kind + 's';
            const endpoint = isLibrary ? '/v1/me/library/' + plural + '/' + id :
              '/v1/catalog/' + music.storefrontId + '/' + plural + '/' + id;
            const result = await music.api.music(endpoint, {include:'tracks',limit:${safeLimit}});
            const entity = result?.data?.data?.[0] || result?.data?.[0];
            const rows = entity?.relationships?.tracks?.data || (entity ? [entity] : []);
            return {key:path + location.search, type:kind,
              title:entity?.attributes?.name || document.title.replace(/ - Apple Music.*$/, ''),
              items:rows.map(format)};
          }
        } catch (error) {
          // Fall through to the visible-row view when Apple changes a route.
        }
      }
      const seen = new Set();
      const items = [];
      for (const link of document.querySelectorAll('a[href*="/song/"]')) {
        if (seen.has(link.href)) continue;
        seen.add(link.href);
        const row = link.closest('[role="row"], [data-testid*="track"], li') || link.parentElement;
        const lines = (row?.innerText || link.innerText || '').split('\n').map(x => x.trim()).filter(Boolean);
        if (lines.length) items.push({id:link.href,title:lines[0],artist:lines[1] || '',album:'',duration:0});
        if (items.length >= ${safeLimit}) break;
      }
      return {key:path + location.search,type:kind,
        title:document.title.replace(/ - Apple Music.*$/, ''),items};
    })()`);
  }
  async searchSongs(query, limit = 50) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 50));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/catalog/' + music.storefrontId + '/search',
        {term:${JSON.stringify(String(query))},types:'songs',limit:${safeLimit}});
      const rows = result?.data?.results?.songs?.data || [];
      return rows.map(item => ({id:item.id,kind:'song',catalogId:item.id,
        title:item.attributes?.name || '', artist:item.attributes?.artistName || '',
        album:item.attributes?.albumName || '',
        duration:Math.round((item.attributes?.durationInMillis || 0) / 1000)}));
    })()`);
  }
  async listenNow(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/me/recommendations', {limit:25});
      const groups = result?.data?.data || result?.data || [];
      const rows = groups.flatMap(group => group.relationships?.contents?.data || []).slice(0,${safeLimit});
      return rows.map(item => ({id:item.id,
        kind:(item.type || '').replace(/^library-/,'').replace(/s$/,''),
        catalogId:item.attributes?.playParams?.catalogId || item.attributes?.playParams?.id || item.id,
        title:item.attributes?.name || '',artist:item.attributes?.artistName || '',
        album:item.attributes?.albumName || '',duration:Math.round((item.attributes?.durationInMillis || 0)/1000)}));
    })()`);
  }
  async radioStations(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const result = await music.api.music('/v1/catalog/' + music.storefrontId + '/charts',
        {types:'stations',limit:${safeLimit}});
      const rows = result?.data?.results?.stations?.[0]?.data || [];
      return rows.map(item => ({id:item.id,kind:'station',catalogId:item.id,
        title:item.attributes?.name || '',artist:item.attributes?.editorialNotes?.short || '',
        album:'',duration:0}));
    })()`);
  }
  async playItem(kind, id) {
    if (!['song','album','playlist','station'].includes(kind)) throw new Error('Unsupported queue item');
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      await music.setQueue({${JSON.stringify(kind)}:${JSON.stringify(String(id))}});
      await music.play();
      return true;
    })()`);
  }
  async queueView(limit = 100) {
    const safeLimit = Math.max(1, Math.min(100, Number(limit) || 100));
    return this.evaluate(`(() => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      if (!music) throw new Error('MusicKit instance unavailable');
      const queue = music.queue || {};
      const rows = Array.from(queue.items || queue._queueItems || []).slice(0,${safeLimit});
      const current = music.nowPlayingItem;
      let currentIndex = Number.isInteger(queue.position) && queue.position >= 0 ? queue.position :
        rows.findIndex(entry => {
          const item = entry?.item || entry;
          return item === current || (item?.id && item.id === current?.id);
        });
      const format = (item, index) => {
        item = item?.item || item;
        const attributes = item?.attributes || item || {};
        return {id:item?.id || String(index),kind:'song',
          catalogId:attributes.playParams?.catalogId || attributes.playParams?.id || item?.id || '',
          title:attributes.name || attributes.title || '',
          artist:attributes.artistName || attributes.artist || '',
          album:attributes.albumName || attributes.album || '',
          duration:Math.round((attributes.durationInMillis || 0) / 1000),
          trackNumber:attributes.trackNumber || 0,discNumber:attributes.discNumber || 0,
          current:index === currentIndex,autoplay:Boolean(item?.autoplay || item?.isAutoplay)};
      };
      if (!rows.length && current) rows.push(current);
      return {title:'Up Next',type:'queue',items:rows.map(format)};
    })()`);
  }
  async artworkRgb(size = 96, suppliedURL = null) {
    const safeSize = Math.max(32, Math.min(240, Number(size) || 96));
    return this.evaluate(`(async () => {
      const music = window.MusicKit && window.MusicKit.getInstance ? window.MusicKit.getInstance() : null;
      const item = music?.nowPlayingItem;
      const attributes = item && (item.attributes || item);
      let url = ${JSON.stringify(suppliedURL)} ||
        attributes?.artworkURL || attributes?.artwork?.url || null;
      if (!url) return null;
      url = url.replace('{w}', '${safeSize}').replace('{h}', '${safeSize}');
      const image = new Image(); image.crossOrigin = 'anonymous'; image.src = url;
      await image.decode();
      const canvas = document.createElement('canvas');
      canvas.width = ${safeSize}; canvas.height = ${safeSize};
      const context = canvas.getContext('2d', {willReadFrequently:true});
      context.drawImage(image, 0, 0, ${safeSize}, ${safeSize});
      const rgba = context.getImageData(0, 0, ${safeSize}, ${safeSize}).data;
      const rgb = new Uint8Array(${safeSize} * ${safeSize} * 3);
      for (let src = 0, dst = 0; src < rgba.length; src += 4) {
        rgb[dst++] = rgba[src];
        rgb[dst++] = rgba[src + 1];
        rgb[dst++] = rgba[src + 2];
      }
      let binary = '';
      for (let offset = 0; offset < rgb.length; offset += 32768)
        binary += String.fromCharCode(...rgb.subarray(offset, offset + 32768));
      return btoa(binary);
    })()`);
  }
  async show() {
    if (!this.visible && this.child) {
      this.visible = true;
      this.temporaryVisible = true;
      this.switchingToVisible = true;
      this.child.kill('SIGTERM');
      return;
    }
    if (!this.cdp) await this.connect();
    if (this.windowId != null)
      await this.cdp.send('Browser.setWindowBounds', {windowId:this.windowId,bounds:{windowState:'normal'}});
    await this.cdp.send('Page.bringToFront');
  }
  async diagnostics() {
    return this.evaluate(`(() => ({
      buttons:[...document.querySelectorAll('button')].map(node => ({
        ariaLabel:node.getAttribute('aria-label'), title:node.title,
        testId:node.dataset.testid || null
      })).filter(item => item.ariaLabel || item.title || item.testId),
      ranges:[...document.querySelectorAll('input[type="range"]')].map(node => ({
        ariaLabel:node.getAttribute('aria-label'), title:node.title,
        testId:node.dataset.testid || null, value:node.value
      })),
      audioElements:document.querySelectorAll('audio').length,
      mediaSessionMetadata:Boolean(navigator.mediaSession && navigator.mediaSession.metadata)
    }))()`);
  }
  stop() {
    this.stopping = true;
    if (this.restartTimer) clearTimeout(this.restartTimer);
    this.cdp?.close();
    if (this.child) this.child.kill('SIGTERM');
  }
}
