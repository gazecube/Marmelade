import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
import { setTimeout as delay } from 'node:timers/promises';

const port = 18761;
const childEnv = { ...process.env, MOTIF_APPLE_MUSIC_BRIDGE_PORT: String(port) };
childEnv.MOTIF_APPLE_MUSIC_BROWSER_DISABLE = '1';
const child = spawn(process.execPath, ['bridge/server.mjs'], {
  env: childEnv,
  stdio: ['ignore', 'pipe', 'inherit'],
});

async function waitReady() {
  for (let attempt = 0; attempt < 30; attempt += 1) {
    try {
      const response = await fetch(`http://127.0.0.1:${port}/v1/status`);
      if (response.ok) return response.json();
    } catch {}
    await delay(50);
  }
  throw new Error('bridge did not become ready');
}

try {
  const status = await waitReady();
  assert.equal(status.ok, true);
  assert.equal(status.apiVersion, 8);
  assert.equal(status.mode, 'browser_disabled');

  const library = await fetch(`http://127.0.0.1:${port}/v1/library/tracks`).then(r => r.json());
  assert.equal(library.items[0].title, 'Cats on Mars');

  const exited = new Promise(resolve => child.once('exit', resolve));
  const shutdown = await fetch(`http://127.0.0.1:${port}/v1/shutdown`, {
    method: 'POST', headers: {'content-type':'application/json'}, body: '{}',
  });
  assert.equal(shutdown.ok, true);
  await Promise.race([
    exited,
    delay(2000).then(() => { throw new Error('bridge did not shut down'); }),
  ]);

  process.stdout.write('bridge smoke test passed\n');
} finally {
  child.kill('SIGTERM');
}
