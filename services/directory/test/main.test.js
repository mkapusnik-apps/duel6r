import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { createServer } from 'node:net';
import { randomBytes } from 'node:crypto';
import { setTimeout as delay } from 'node:timers/promises';
import { Firestore } from '@google-cloud/firestore';
import { configuration } from '../src/config.js';

// Enforce the existing demo-project/emulator guard before starting any client.
const config = configuration({ ...process.env, NODE_ENV: 'test' });

test('entry point routes listings to the selected database without default fallback', { timeout: 60_000 }, async () => {
  const named = `test-directory-${randomBytes(8).toString('hex')}`;
  const databases = ['(default)', named].map(databaseId => new Firestore({
    projectId: config.projectId, databaseId
  }));
  try {
    for (const [index, databaseId] of [undefined, named].entries()) {
      const socket = createServer();
      socket.listen(0, '127.0.0.1');
      await once(socket, 'listening');
      const port = socket.address().port;
      await new Promise(resolve => socket.close(resolve));
      const env = { ...process.env, NODE_ENV: 'test', PORT: String(port) };
      delete env.D6R_DIRECTORY_FIRESTORE_DATABASE;
      if (databaseId !== undefined) env.D6R_DIRECTORY_FIRESTORE_DATABASE = databaseId;
      const child = spawn(process.execPath, ['src/main.js'], { env, stdio: 'ignore' });
      const exited = once(child, 'exit');
      const url = `http://127.0.0.1:${port}/v1/listings`;
      let created;
      try {
        let ready = false;
        for (let attempt = 0; attempt < 100; attempt++) {
          assert.equal(child.exitCode, null, 'entry point exited before readiness');
          try {
            // Health checks do not query Firestore or consume directory quota.
            ready = (await fetch(`http://127.0.0.1:${port}/healthz`,
              { signal: AbortSignal.timeout(500) })).status === 200;
          } catch { /* Wait for the listening socket. */ }
          if (ready) break;
          await delay(50);
        }
        assert.ok(ready, 'entry point did not listen');
        const response = await fetch(url, { method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ sessionId: randomBytes(16).toString('hex'),
            address: '192.168.1.5', port: 25000, mode: 'deathmatch', players: 2,
            capacity: 15, passwordRequired: false, phase: 'lobby' }),
          signal: AbortSignal.timeout(10_000) });
        assert.equal(response.status, 201);
        created = await response.json();
        const path = `directoryListings/${created.id}`;
        assert.equal((await databases[index].doc(path).get()).exists, true);
        assert.equal((await databases[1 - index].doc(path).get()).exists, false);
        assert.equal((await fetch(`${url}/${created.id}`, { method: 'DELETE',
          signal: AbortSignal.timeout(10_000) })).status, 401);
        const removed = await fetch(`${url}/${created.id}`, { method: 'DELETE',
          headers: { Authorization: `Bearer ${created.ownerToken}`, 'If-Match': '1' },
          signal: AbortSignal.timeout(10_000) });
        assert.equal(removed.status, 204);
      } finally {
        child.kill('SIGTERM');
        const killDeadline = setTimeout(() => child.kill('SIGKILL'), 10_000);
        try { await exited; } finally { clearTimeout(killDeadline); }
        if (created) await databases[index].doc(`directoryListings/${created.id}`).delete();
      }
    }
  } finally {
    for (const db of databases) await db.terminate();
  }
});
