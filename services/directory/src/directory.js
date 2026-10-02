import { createHash, randomBytes, timingSafeEqual } from 'node:crypto';
import { FieldPath, Timestamp } from '@google-cloud/firestore';

export const LEASE_MS = 60_000;
export const PAGE_SIZE = 25;
export const MAX_BODY_BYTES = 2048;
const idPattern = /^[a-f0-9]{32}$/;
const tokenPattern = /^[a-f0-9]{64}$/;

export class DirectoryError extends Error {
  constructor(status) { super('Directory request failed.'); this.status = status; }
}

function requireValue(condition, status = 400) {
  if (!condition) throw new DirectoryError(status);
}

function exactKeys(value, keys) {
  return value && typeof value === 'object' && !Array.isArray(value)
    && Object.keys(value).length === keys.length && keys.every(key => Object.hasOwn(value, key));
}

// The endpoint is data, never a URL fetched by the directory. Reject special-use
// destinations, but do not mistake public-unicast or a private .255 for broadcast.
function validAddress(address) {
  if (typeof address !== 'string') return false;
  const parts = address.split('.');
  if (parts.length !== 4 || !parts.every(p => /^(0|[1-9][0-9]{0,2})$/.test(p) && Number(p) <= 255)) return false;
  const [a, b] = parts.map(Number);
  return a !== 0 && a < 224 && !(a === 169 && b === 254);
}

export function validateListing(value) {
  requireValue(exactKeys(value, ['sessionId', 'address', 'port', 'mode', 'players', 'capacity', 'passwordRequired', 'phase']));
  requireValue(typeof value.sessionId === 'string' && idPattern.test(value.sessionId));
  requireValue(validAddress(value.address));
  requireValue(Number.isInteger(value.port) && value.port >= 1 && value.port <= 65535);
  requireValue(['deathmatch', 'predator', 'teams'].includes(value.mode));
  requireValue(Number.isInteger(value.capacity) && value.capacity >= 1 && value.capacity <= 15);
  requireValue(Number.isInteger(value.players) && value.players >= 1 && value.players <= value.capacity);
  requireValue(typeof value.passwordRequired === 'boolean');
  requireValue(['lobby', 'first-round', 'closed'].includes(value.phase));
  return { ...value };
}

function digest(value) { return createHash('sha256').update(value).digest('hex'); }

function publicListing(id, data) {
  return { id, ...validateListing(data.listing), expiresAt: data.expiresAt, revision: data.revision };
}

export class Directory {
  constructor(db, { now = Date.now, collection = 'directoryListings' } = {}) {
    this.db = db;
    this.now = now;
    this.listings = db.collection(collection);
    this.limits = db.collection(`${collection}Limits`);
  }

  // Shared fixed-window quotas bound admitted operations across stateless instances.
  // Rejected requests still perform quota transactions. Deployment ingress and
  // scaling limits must bound that work; these quotas are not a database DoS shield.
  // These are deliberately global limits for a small directory, not identities.
  // A busy or abused directory may become unavailable; gameplay remains separate.
  async quota(kind) {
    const maximum = { read: 240, register: 30, mutate: 600 }[kind];
    requireValue(maximum !== undefined);
    const ref = this.limits.doc(kind);
    await this.db.runTransaction(async tx => {
      const doc = await tx.get(ref);
      const window = Math.floor(this.now() / 60_000);
      const count = doc.exists && doc.data().window === window ? doc.data().count : 0;
      requireValue(count < maximum, 429);
      tx.set(ref, { window, count: count + 1 });
    }, { maxAttempts: 3 });
  }

  async register(input) {
    const listing = validateListing(input);
    await this.quota('register');
    const id = randomBytes(16).toString('hex');
    const ownerToken = randomBytes(32).toString('hex');
    const expiresAt = this.now() + LEASE_MS;
    const data = { listing, ownerHash: digest(ownerToken), revision: 1, expiresAt,
      leaseExpiry: Timestamp.fromMillis(expiresAt) };
    await this.listings.doc(id).create(data);
    return { ...publicListing(id, data), ownerToken };
  }

  async mutate(id, ownerToken, revision, input) {
    requireValue(typeof id === 'string' && idPattern.test(id));
    requireValue(typeof ownerToken === 'string' && tokenPattern.test(ownerToken), 401);
    requireValue(Number.isSafeInteger(revision) && revision >= 1);
    const listing = input === null ? null : validateListing(input);
    await this.quota('mutate');
    const ref = this.listings.doc(id);
    return this.db.runTransaction(async tx => {
      const doc = await tx.get(ref);
      const data = doc.data();
      // Same response for a nonexistent, expired, or incorrectly owned listing.
      requireValue(doc.exists && data.expiresAt > this.now()
        && timingSafeEqual(Buffer.from(data.ownerHash, 'hex'), Buffer.from(digest(ownerToken), 'hex')), 401);
      requireValue(data.revision === revision, 409);
      if (listing === null) { tx.delete(ref); return null; }
      requireValue(listing.sessionId === data.listing.sessionId);
      // A session password is fixed until session end, including lobby return.
      requireValue(listing.passwordRequired === data.listing.passwordRequired);
      const expiresAt = this.now() + LEASE_MS;
      const replacement = { ...data, listing, revision: revision + 1, expiresAt,
        leaseExpiry: Timestamp.fromMillis(expiresAt) };
      tx.set(ref, replacement);
      return publicListing(id, replacement);
    }, { maxAttempts: 3 });
  }

  async list(cursor) {
    let position;
    if (cursor !== undefined) {
      requireValue(typeof cursor === 'string' && cursor.length <= 64);
      const match = /^([0-9]{1,16})-([a-f0-9]{32})$/.exec(cursor);
      requireValue(match !== null && Number.isSafeInteger(Number(match[1])));
      position = { expiry: Number(match[1]), id: match[2] };
    }
    await this.quota('read');
    const now = this.now();
    // The index excludes expired history before the result limit is applied.
    // Renewals can move a row forward across pages. Identity is the stable ID,
    // not a row position; a traversal is a live view, not a frozen snapshot.
    let query = this.listings.where('leaseExpiry', '>', Timestamp.fromMillis(now))
      .orderBy('leaseExpiry').orderBy(FieldPath.documentId()).limit(PAGE_SIZE + 1);
    if (position) query = query.startAfter(Timestamp.fromMillis(position.expiry), position.id);
    const snapshot = await query.get();
    const page = snapshot.docs.slice(0, PAGE_SIZE);
    const readTime = this.now();
    return {
      listings: page.filter(doc => doc.data().expiresAt > readTime).map(doc => publicListing(doc.id, doc.data())),
      nextCursor: snapshot.size > PAGE_SIZE ? `${page.at(-1).data().expiresAt}-${page.at(-1).id}` : null
    };
  }
}
