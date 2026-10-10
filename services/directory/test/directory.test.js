import { test, before, after, beforeEach } from 'node:test';
import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { Firestore, Timestamp } from '@google-cloud/firestore';
import { configuration } from '../src/config.js';
import { Directory, LEASE_MS, PAGE_SIZE, validateListing } from '../src/directory.js';
import { createServer } from '../src/http.js';

// Fail closed instead of silently replacing the real emulator with a fake or
// accidentally using application-default credentials against a live project.
const config = configuration({ ...process.env, NODE_ENV: 'test' });
const db = new Firestore({ projectId: config.projectId });
const collection = `testDirectory${randomBytes(8).toString('hex')}`;
let clock = 100_000;
const directory = new Directory(db, { collection, now: () => clock });
const listing = (changes = {}) => ({
  sessionId: 'a'.repeat(32), address: '192.168.1.5', port: 25000,
  mode: 'deathmatch', players: 2, capacity: 15, passwordRequired: false,
  phase: 'lobby', ...changes
});
const status = expected => error => error.status === expected;
const dedicated = (changes = {}) => listing({ address: 'arena.example.test', players: 0,
  phase: 'idle', hosting: 'dedicated', protection: 'invite', admission: 'open', ...changes });

test('mixed contract validates frontend, idle and protection without relaxing private minimum', () => {
  assert.equal(validateListing(dedicated()).players, 0);
  for (const changes of [{ address: '127.0.0.1' }, { address: '0.0.0.0' }, { address: '224.1.2.3' },
    { address: 'localhost' }, { address: 'https://arena.example.test' }, { address: 'bad_name' },
    { address: '169.254.1.1' }, { phase: 'lobby' }, { sessionId: '0'.repeat(32) },
    { protection: 'password' }, { hosting: 'unknown' }, { passwordRequired: true },
    { admission: 'closed' }, { invitation: 'not-public' }])
    assert.throws(() => validateListing(dedicated(changes)), status(400));
  assert.throws(() => validateListing(listing({ players: 0 })), status(400));
  assert.equal(validateListing(dedicated({ address: '198.51.100.4' })).hosting, 'dedicated');
  assert.equal(validateListing(dedicated({ players: 2, phase: 'reserved', admission: 'closed' })).players, 2);
  assert.throws(() => validateListing(dedicated({ players: 2, phase: 'reserved', admission: 'open' })), status(400));
});

test('mixed publication isolates legacy reads, immutable classification and owner authority', async () => {
  await assert.rejects(directory.register(dedicated()), status(400));
  const created = await directory.register(dedicated(), true);
  const privateRow = await directory.register(listing({ passwordRequired: true }));
  assert.deepEqual((await directory.list()).listings.map(row => row.id), [privateRow.id]);
  const mixed = await directory.list(undefined, true);
  assert.equal(mixed.contract, 'mixed-v2');
  assert.equal(mixed.listings.length, 2);
  assert.equal(mixed.listings.find(row => row.id === privateRow.id).protection, 'password');
  assert.equal(mixed.listings.find(row => row.id === created.id).players, 0);
  assert.ok(!JSON.stringify(mixed).includes(created.ownerToken));
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 1, null), status(400));
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 1,
    listing({ sessionId: created.sessionId }), true), status(400));
  const updated = await directory.mutate(created.id, created.ownerToken, 1,
    dedicated({ players: 2, phase: 'lobby' }), true);
  assert.equal(updated.sessionId, created.sessionId);
  assert.equal(updated.revision, 2);
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 1, null, true), status(409));
  await directory.mutate(created.id, created.ownerToken, 2, null, true);
});

test('mixed and legacy cursors cannot cross capability contexts and reach every bounded row', async () => {
  for (let i = 1; i <= PAGE_SIZE + 2; ++i) {
    const value = i % 2 ? dedicated() : listing();
    const expiresAt = clock + LEASE_MS;
    await db.collection(collection).doc(i.toString(16).padStart(32, '0')).set({
      listing: value, expiresAt, revision: 1, leaseExpiry: Timestamp.fromMillis(expiresAt), ownerHash: 'c'.repeat(64) });
  }
  const mixed = await directory.list(undefined, true), old = await directory.list();
  assert.equal(mixed.listings.length, PAGE_SIZE);
  assert.match(mixed.nextCursor, /^m2_/);
  assert.ok(old.listings.every(row => !('hosting' in row) && row.players > 0));
  assert.ok(old.listings.length < PAGE_SIZE);
  await assert.rejects(directory.list(mixed.nextCursor), status(400));
  await assert.rejects(directory.list(old.nextCursor, true), status(400));
  assert.equal((await directory.list(mixed.nextCursor, true)).listings.length, 2);
  assert.ok((await directory.list(old.nextCursor)).listings.every(row => row.players > 0));
});
before(async () => { await db.listCollections(); });
beforeEach(async () => {
  clock = 100_000;
  await db.recursiveDelete(db.collection(collection));
  await db.recursiveDelete(db.collection(`${collection}Limits`));
});
after(async () => {
  await db.recursiveDelete(db.collection(collection));
  await db.recursiveDelete(db.collection(`${collection}Limits`));
  await db.terminate();
});

test('registration, ownership, renewal, immediate read expiry, no TTL dependency', async () => {
  const created = await directory.register(listing());
  assert.equal(created.expiresAt, clock + LEASE_MS);
  assert.match(created.ownerToken, /^[a-f0-9]{64}$/);
  await assert.rejects(directory.mutate(created.id, '0'.repeat(64), 1, listing()), status(401));
  clock += 20_000;
  const renewed = await directory.mutate(created.id, created.ownerToken, 1, listing({ phase: 'first-round' }));
  assert.equal(renewed.expiresAt, clock + LEASE_MS);
  assert.equal(renewed.revision, 2);
  assert.equal('ownerToken' in renewed, false);
  assert.equal('ownerHash' in renewed, false);
  clock = renewed.expiresAt - 1;
  assert.equal((await directory.list()).listings.length, 1);
  clock++;
  assert.deepEqual((await directory.list()).listings, []);
  assert.equal((await db.collection(collection).doc(created.id).get()).exists, true);
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 2, listing()), status(401));
});

test('concurrent owners cannot lose updates and shutdown cannot be resurrected', async () => {
  const created = await directory.register(listing());
  const otherInstance = new Directory(db, { collection, now: () => clock });
  const outcomes = await Promise.allSettled([
    directory.mutate(created.id, created.ownerToken, 1, listing({ phase: 'first-round' })),
    otherInstance.mutate(created.id, created.ownerToken, 1, listing({ players: 3 }))
  ]);
  assert.equal(outcomes.filter(o => o.status === 'fulfilled').length, 1);
  assert.equal(outcomes.find(o => o.status === 'rejected').reason.status, 409);
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 1, null), status(409));
  await directory.mutate(created.id, created.ownerToken, 2, null);
  await assert.rejects(directory.mutate(created.id, created.ownerToken, 2, listing()), status(401));
  assert.deepEqual((await directory.list()).listings, []);
});

test('restarted sessions receive independent authority and immutable password state', async () => {
  const first = await directory.register(listing());
  const second = await directory.register(listing({ sessionId: 'b'.repeat(32) }));
  assert.notEqual(first.ownerToken, second.ownerToken);
  await assert.rejects(directory.mutate(second.id, first.ownerToken, 1, null), status(401));
  await assert.rejects(directory.mutate(first.id, first.ownerToken, 1, listing({ sessionId: 'b'.repeat(32) })), status(400));
  await assert.rejects(directory.mutate(first.id, first.ownerToken, 1, listing({ passwordRequired: true })), status(400));
});

test('active index pagination ignores substantial expired history and retains every phase', async () => {
  const expired = 2000;
  for (let first = 1; first <= expired + PAGE_SIZE + 2; first += 400) {
    const batch = db.batch();
    for (let i = first; i < first + 400 && i <= expired + PAGE_SIZE + 2; i++) {
      const expiresAt = i <= expired ? clock : clock + LEASE_MS;
      batch.set(db.collection(collection).doc(i.toString(16).padStart(32, '0')), {
        listing: listing({ phase: ['lobby', 'first-round', 'closed'][i % 3],
          players: i % 2 ? 15 : 2, passwordRequired: i % 2 === 0 }),
        revision: 1, expiresAt, leaseExpiry: Timestamp.fromMillis(expiresAt),
        ownerHash: 'c'.repeat(64)
      });
    }
    await batch.commit();
  }
  const first = await directory.list();
  assert.equal(first.listings.length, PAGE_SIZE);
  assert.ok(first.nextCursor);
  const second = await directory.list(first.nextCursor);
  assert.equal(second.listings.length, 2);
  assert.equal(second.nextCursor, null);
  assert.equal(new Set([...first.listings, ...second.listings].map(row => row.id)).size, PAGE_SIZE + 2);
  assert.deepEqual(new Set(first.listings.map(row => row.phase)), new Set(['lobby', 'first-round', 'closed']));
  const serialized = JSON.stringify(first);
  assert.equal(serialized.includes('ownerHash'), false);
  assert.equal(serialized.includes('ownerToken'), false);
  await assert.rejects(directory.list('../cursor'), status(400));
  await assert.rejects(directory.list(`9999999999999999-${'a'.repeat(32)}`), status(400));
});

test('expiry cursors survive concurrent renewal, expiry and deletion without walking old history', async () => {
  const created = [];
  for (let i = 0; i < PAGE_SIZE + 3; i++) created.push(await directory.register(listing()));
  const first = await directory.list();
  const last = first.listings.at(-1);
  const renewed = created.find(row => row.id === first.listings[0].id);
  const deleted = created.find(row => row.id === last.id);
  clock += 20_000;
  await Promise.all([
    directory.mutate(renewed.id, renewed.ownerToken, 1, listing({ phase: 'closed' })),
    directory.mutate(deleted.id, deleted.ownerToken, 1, null)
  ]);
  const remaining = await directory.list(first.nextCursor);
  assert.equal(remaining.listings.length, 4); // three unread rows plus the renewed row
  assert.equal(remaining.nextCursor, null);
  assert.ok(remaining.listings.some(row => row.id === renewed.id && row.phase === 'closed'));
  assert.ok(!remaining.listings.some(row => row.id === deleted.id));
  clock = created[0].expiresAt;
  const afterExpiry = await directory.list(first.nextCursor);
  assert.deepEqual(afterExpiry.listings.map(row => row.id), [renewed.id]);
  const stored = (await db.collection(collection).doc(renewed.id).get()).data();
  assert.equal(stored.leaseExpiry.toMillis(), stored.expiresAt);
  await assert.rejects(directory.mutate(renewed.id, renewed.ownerToken, 1, null), status(409));
  await directory.mutate(renewed.id, renewed.ownerToken, 2, null);
  assert.equal((await directory.list()).listings.length, 0);
});

test('shared quota applies across service instances, recovers next window', async () => {
  const otherInstance = new Directory(db, { collection, now: () => clock });
  await db.collection(`${collection}Limits`).doc('register').set({ window: 1, count: 29 });
  const outcomes = await Promise.allSettled([directory.register(listing()), otherInstance.register(listing())]);
  assert.equal(outcomes.filter(o => o.status === 'fulfilled').length, 1);
  assert.equal(outcomes.find(o => o.status === 'rejected').reason.status, 429);
  clock = 120_000;
  assert.ok((await directory.register(listing())).id);
});

test('a lease expiring while the real query completes is excluded from the response', async () => {
  await directory.register(listing());
  const raced = new Directory(db, { collection, now: () => clock });
  const collectionRef = raced.listings;
  // Wrap only query completion to deterministically advance the service clock.
  // Reads, indexes and stored records still use the real Firestore emulator.
  raced.listings = { where: (...args) => {
    let query = collectionRef.where(...args);
    const chain = {
      orderBy: (...values) => { query = query.orderBy(...values); return chain; },
      limit: value => { query = query.limit(value); return chain; },
      get: async () => { const snapshot = await query.get(); clock += LEASE_MS; return snapshot; }
    };
    return chain;
  } };
  assert.deepEqual((await raced.list()).listings, []);
});

test('HTTP lifecycle, non-disclosing rejection, owner headers and no-store responses', async () => {
  const server = createServer(directory);
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const url = `http://127.0.0.1:${server.address().port}/v1/listings`;
  try {
    const post = body => fetch(url, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body });
    const response = await post(JSON.stringify(listing()));
    assert.equal(response.status, 201);
    assert.equal(response.headers.get('cache-control'), 'no-store');
    const created = await response.json();
    const rows = await (await fetch(url)).json();
    assert.equal(rows.listings[0].id, created.id);
    assert.equal(JSON.stringify(rows).includes(created.ownerToken), false);
    assert.equal((await fetch(`${url}/${created.id}`, { method: 'DELETE' })).status, 401);
    const removed = await fetch(`${url}/${created.id}`, { method: 'DELETE',
      headers: { Authorization: `Bearer ${created.ownerToken}`, 'If-Match': '1' } });
    assert.equal(removed.status, 204);
    const malformed = await post('{"secret":"do-not-echo"');
    assert.equal(malformed.status, 400);
    assert.equal((await malformed.text()).includes('do-not-echo'), false);
    assert.equal((await post(JSON.stringify({ padding: 'x'.repeat(3000) }))).status, 413);
    assert.equal((await fetch(`${url}?unexpected=1`)).status, 400);
  } finally {
    await new Promise(resolve => server.close(resolve));
  }
});
