import { test } from 'node:test';
import assert from 'node:assert/strict';
import { setTimeout as delay } from 'node:timers/promises';
import { createServer } from '../src/http.js';

test('overload and response timeout keep unfinished work inside the concurrency cap', { timeout: 15000 }, async () => {
  const releases = [];
  let blocked = true;
  const server = createServer({ list: () => blocked
    ? new Promise(resolve => releases.push(() => resolve({ listings: [], nextCursor: null })))
    : Promise.resolve({ listings: [], nextCursor: null }) });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const url = `http://127.0.0.1:${server.address().port}/v1/listings`;
  try {
    const pending = Array.from({ length: 32 }, () => fetch(url).then(async response => {
      await response.text(); return response.status;
    }));
    for (let i = 0; releases.length < 32 && i < 100; i++) await delay(10);
    assert.equal(releases.length, 32);
    assert.equal((await fetch(url)).status, 429);
    assert.deepEqual(await Promise.all(pending), Array(32).fill(503));
    assert.equal((await fetch(url)).status, 429, 'timed-out database work still occupies its slot');
    blocked = false;
    for (const release of releases) release();
    await delay(10);
    assert.equal((await fetch(url)).status, 200);
  } finally {
    for (const release of releases) release();
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
  }
});
