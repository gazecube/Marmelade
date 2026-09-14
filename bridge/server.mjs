import http from 'node:http';
import { URL } from 'node:url';
import { ChromiumAppleMusic } from './chromium.mjs';

const host = process.env.MOTIF_APPLE_MUSIC_BRIDGE_HOST || '127.0.0.1';
const port = Number.parseInt(process.env.MOTIF_APPLE_MUSIC_BRIDGE_PORT || '17876', 10);
const browserPort = Number.parseInt(process.env.MOTIF_APPLE_MUSIC_CDP_PORT || '17877', 10);
const browser = new ChromiumAppleMusic({ port: browserPort });
const state = { engineConnected: false, loggedIn: false, playback: 'unavailable',
  currentTrack: null, position: 0, duration: 0, volume: 0.6, shuffleEnabled: false, repeatMode: 'off', automixEnabled: false, autoplayEnabled: false, error: null };
const demoTracks = [{ id: 'demo-cats-on-mars', title: 'Cats on Mars', artist: 'SEATBELTS', duration: 164 }];
let libraryTracks = [];
let currentView = {key:'',type:'page',title:'Apple Music',items:[]};

function send(response, status, body) {
  const json = JSON.stringify(body);
  response.writeHead(status, {'content-type':'application/json; charset=utf-8',
    'content-length':Buffer.byteLength(json), 'cache-control':'no-store'});
  response.end(json);
}
function sendText(response, status, body, contentType = 'text/plain; charset=utf-8') {
  response.writeHead(status, {'content-type':contentType,
    'content-length':Buffer.byteLength(body), 'cache-control':'no-store'});
  response.end(body);
}
function sendBuffer(response, status, body, contentType = 'application/octet-stream') {
  response.writeHead(status, {'content-type':contentType,
    'content-length':body.length, 'cache-control':'no-store'});
  response.end(body);
}
function readBody(request) {
  return new Promise((resolve, reject) => { let body = ''; request.setEncoding('utf8');
    request.on('data', chunk => { body += chunk; if (body.length > 1048576) request.destroy(); });
    request.on('end', () => resolve(body ? JSON.parse(body) : {})); request.on('error', reject); });
}
async function refreshState() {
  try { Object.assign(state, await browser.state(), {engineConnected:true, error:null}); }
  catch (error) { state.engineConnected = false; state.error = browser.error || error.message; }
}

const server = http.createServer(async (request, response) => {
  const url = new URL(request.url, `http://${host}:${port}`);
  try {
    if (request.method === 'GET' && url.pathname === '/v1/status')
      return send(response, 200, {ok:true, service:'motif-apple-music-bridge', apiVersion:8,
        mode: browser.error || (!state.engineConnected ? 'starting_browser' : (!state.loggedIn ? 'needs_login' : 'ready')),
        engineMode: browser.visible ? 'visible' : 'headless',
        browserProfile: browser.profile});
    if (request.method === 'POST' && url.pathname === '/v1/browser/show') {
      await browser.show(); return send(response, 200, {ok:true});
    }
    if (request.method === 'POST' && url.pathname === '/v1/shutdown') {
      send(response, 200, {ok:true});
      setImmediate(shutdown);
      return;
    }
    if (request.method === 'GET' && url.pathname === '/v1/diagnostics')
      return send(response, 200, {ok:true, diagnostics:await browser.diagnostics()});
    if (request.method === 'GET' && url.pathname === '/v1/library/tracks') {
      try {
        currentView = await browser.currentView(100);
        if (currentView.type === 'library-songs') libraryTracks = currentView.items;
      } catch {}
      return send(response, 200, currentView.items.length ?
        {source:'apple-music-web',view:currentView,items:currentView.items} :
        {source:'demo',view:currentView,items:demoTracks});
    }
    const collection = url.pathname.match(/^\/v1\/library\/(songs|albums|artists|playlists)$/);
    if (request.method === 'GET' && collection) {
      const items = await browser.libraryCollection(collection[1], 100);
      const viewTitle = collection[1][0].toUpperCase() + collection[1].slice(1);
      return send(response, 200, {source:'apple-music-web',viewTitle,
        viewType:collection[1],items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/library/recent') {
      const items = await browser.recentlyPlayed(100);
      return send(response, 200, {source:'apple-music-web',viewTitle:'Recently Played',
        viewType:'recent',items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/listen-now') {
      const items = await browser.listenNow(100);
      return send(response, 200, {source:'apple-music-web',viewTitle:'Listen Now',
        viewType:'recommendations',items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/radio') {
      const items = await browser.radioStations(100);
      return send(response, 200, {source:'apple-music-web',viewTitle:'Radio',
        viewType:'radio',items});
    }
    const detail = url.pathname.match(/^\/v1\/library\/(albums|artists|playlists)\/([^/]+)$/);
    if (request.method === 'GET' && detail) {
      const view = await browser.libraryDetail(detail[1], decodeURIComponent(detail[2]), 100);
      return send(response, 200, {source:'apple-music-web',viewTitle:view.title,
        viewType:view.type,containerKind:view.containerKind,containerId:view.containerId,
        containerCatalogId:view.containerCatalogId,items:view.items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/search') {
      const query = (url.searchParams.get('q') || '').toLowerCase();
      let items;
      try { items = await browser.searchSongs(query, 50); }
      catch { items = (libraryTracks.length ? libraryTracks : demoTracks).filter(track =>
        `${track.title} ${track.artist}`.toLowerCase().includes(query)); }
      return send(response, 200, {source:'apple-music-web',viewTitle:`Search: ${query}`,
        viewType:'search',query,items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/player/state') return send(response, 200, state);
    if (request.method === 'GET' && url.pathname === '/v1/player/queue') {
      const view = await browser.queueView(100);
      return send(response, 200, {source:'apple-music-web',viewTitle:view.title,
        viewType:view.type,items:view.items});
    }
    if (request.method === 'GET' && url.pathname === '/v1/player/artwork.rgb') {
      const requestedSize = Number.parseInt(url.searchParams.get('size') || '160', 10);
      const rgbBase64 = await browser.artworkRgb(requestedSize, url.searchParams.get('url'));
      if (!rgbBase64) return send(response, 404, {ok:false,error:'artwork_unavailable'});
      return sendBuffer(response, 200, Buffer.from(rgbBase64, 'base64'), 'application/octet-stream');
    }
    const action = url.pathname.match(/^\/v1\/player\/(play|pause|next|previous)$/);
    if (request.method === 'POST' && action) {
      await readBody(request); const applied = await browser.action(action[1]); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied, action:action[1], ...state});
    }
    if (request.method === 'POST' && url.pathname === '/v1/player/volume') {
      const body = await readBody(request);
      if (!Number.isFinite(body.value) || body.value < 0 || body.value > 1)
        return send(response, 400, {ok:false,error:'volume_must_be_between_0_and_1'});
      const applied = await browser.action('volume', body.value); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied,...state});
    }
    if (request.method === 'POST' && url.pathname === '/v1/player/seek') {
      const body = await readBody(request);
      if (!Number.isFinite(body.time) || body.time < 0)
        return send(response, 400, {ok:false,error:'nonnegative_time_required'});
      const applied = await browser.action('seek', body.time); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied,...state});
    }
    if (request.method === 'POST' && url.pathname === '/v1/player/shuffle') {
      const body = await readBody(request);
      if (typeof body.enabled !== 'boolean')
        return send(response, 400, {ok:false,error:'enabled_boolean_required'});
      const applied = await browser.action('shuffle', body.enabled); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied,...state});
    }
    if (request.method === 'POST' && url.pathname === '/v1/player/repeat') {
      const body = await readBody(request);
      if (!['off','all','one'].includes(body.mode))
        return send(response, 400, {ok:false,error:'repeat_mode_must_be_off_all_or_one'});
      const applied = await browser.action('repeat', body.mode); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied,...state});
    }
    if (request.method === 'POST' && (url.pathname === '/v1/player/automix' || url.pathname === '/v1/player/autoplay')) {
      const body = await readBody(request);
      if (typeof body.enabled !== 'boolean')
        return send(response, 400, {ok:false,error:'enabled_boolean_required'});
      const actionName = url.pathname.endsWith('/automix') ? 'automix' : 'autoplay';
      const applied = await browser.action(actionName, body.enabled); await refreshState();
      return send(response, applied ? 200 : 409, {ok:applied,...state});
    }
    if (request.method === 'POST' && url.pathname === '/v1/player/queue') {
      const body = await readBody(request);
      if (!body.kind || !body.id)
        return send(response, 400, {ok:false,error:'kind_and_id_required'});
      const applied = await browser.playItem(body.kind, body.id);
      await refreshState();
      return send(response, 200, {ok:applied,...state});
    }
    return send(response, 404, {ok:false,error:'not_found'});
  } catch (error) { return send(response, 503, {ok:false,error:'browser_control_failed',detail:error.message}); }
});

server.on('error', error => {
  if (error.code === 'EADDRINUSE') {
    process.stderr.write(`[applemusic-bridge] ${host}:${port} is already served by another process\n`);
    browser.stop();
    process.exit(0);
  }
  process.stderr.write(`[applemusic-bridge] server error: ${error.message}\n`);
  browser.stop();
  process.exit(1);
});

await browser.start();
await refreshState();
const stateTimer = setInterval(refreshState, 1000);
server.listen(port, host, () => process.stdout.write(`[applemusic-bridge] listening on http://${host}:${port}\n`));
function shutdown() { clearInterval(stateTimer); browser.stop(); server.close(() => process.exit(0)); setTimeout(() => process.exit(1), 2000).unref(); }
process.on('SIGINT', shutdown); process.on('SIGTERM', shutdown);
