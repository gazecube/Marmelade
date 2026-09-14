import { spawn } from 'node:child_process';
import http from 'node:http';
import { setTimeout as delay } from 'node:timers/promises';

const port = 18763;
const env = {
  ...process.env,
  MOTIF_APPLE_MUSIC_BRIDGE_PORT: String(port),
  MOTIF_APPLE_MUSIC_BRIDGE_URL: `http://127.0.0.1:${port}`,
  MOTIF_APPLE_MUSIC_BROWSER_DISABLE: '1',
};
const bridge = spawn(process.execPath, ['bridge/server.mjs'], { env, stdio: 'ignore' });

try {
  for (let attempt = 0; attempt < 30; attempt += 1) {
    try {
      if ((await fetch(`http://127.0.0.1:${port}/v1/status`)).ok) break;
    } catch {}
    if (attempt === 29) throw new Error('bridge did not become ready');
    await delay(50);
  }
  const test = spawn('./test/bridge-reuse', [], { env, stdio: 'inherit' });
  const code = await new Promise(resolve => test.once('exit', resolve));
  if (code !== 0) process.exitCode = code || 1;
} finally {
  bridge.kill('SIGTERM');
}

const oldPort = 18764;
const oldBridge = http.createServer((request, response) => {
  const body = JSON.stringify({ok:true,service:'motif-apple-music-bridge',apiVersion:2});
  response.writeHead(200, {'content-type':'application/json','content-length':Buffer.byteLength(body)});
  response.end(body);
});
await new Promise(resolve => oldBridge.listen(oldPort, '127.0.0.1', resolve));
try {
  const oldEnv = {...env, EXPECT_INCOMPATIBLE:'1',
    MOTIF_APPLE_MUSIC_BRIDGE_URL:`http://127.0.0.1:${oldPort}`};
  const test = spawn('./test/bridge-reuse', [], {env:oldEnv, stdio:'inherit'});
  const code = await new Promise(resolve => test.once('exit', resolve));
  if (code !== 0) process.exitCode = code || 1;
} finally {
  await new Promise(resolve => oldBridge.close(resolve));
}
