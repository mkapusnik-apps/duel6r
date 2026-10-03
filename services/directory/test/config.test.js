import { test } from 'node:test';
import assert from 'node:assert/strict';
import { configuration } from '../src/config.js';
import { validateListing } from '../src/directory.js';

test('requires explicit project and disallows accidental production emulator use', () => {
  assert.throws(() => configuration({}));
  assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test', FIRESTORE_EMULATOR_HOST: 'localhost:8081' }));
  assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'real-test', NODE_ENV: 'test', FIRESTORE_EMULATOR_HOST: 'localhost:8081' }));
  assert.equal(configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test' }).port, 8080);
  assert.equal(configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test', NODE_ENV: 'development',
    FIRESTORE_EMULATOR_HOST: 'localhost:8081', PORT: '9090' }).port, 9090);
});

test('rejects invalid configuration without echoing environment values', () => {
  for (const PORT of ['0', '65536', '8080suffix', '-1']) {
    assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test', PORT }));
  }
  assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'secret\nproject' }), error => !error.message.includes('secret'));
  assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test', NODE_ENV: 'unknown' }));
});

test('database configuration preserves the default and accepts explicit named databases', () => {
  assert.equal(configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test' }).databaseId, '(default)');
  for (const databaseId of ['(default)', 'staging-directory', 'directory', 'abcd', 'a'.repeat(62) + '1']) {
    assert.equal(configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test',
      D6R_DIRECTORY_FIRESTORE_DATABASE: databaseId }).databaseId, databaseId);
  }
});

test('invalid explicit database IDs fail closed without echoing their values', () => {
  for (const databaseId of ['', 'abc', 'a'.repeat(64), 'Staging', '1directory', 'directory-',
    ' directory', 'directory\n', 'some/database', 'some_database', '(other)',
    'f47ac10b-58cc-0372-8567-0e02b2c3d479']) {
    assert.throws(() => configuration({ GOOGLE_CLOUD_PROJECT: 'demo-test',
      D6R_DIRECTORY_FIRESTORE_DATABASE: databaseId }),
    { message: 'Invalid D6R_DIRECTORY_FIRESTORE_DATABASE.' });
  }
});

test('bounded validated fields and no secret extension fields', () => {
  const listing = (changes = {}) => ({ sessionId: 'a'.repeat(32), address: '192.168.1.5',
    port: 25000, mode: 'deathmatch', players: 2, capacity: 15, passwordRequired: false,
    phase: 'lobby', ...changes });
  for (const changes of [ { password: 'secret' }, { sessionId: '../x' }, { players: 16 },
    { capacity: 0 }, { players: 0 }, { address: '0.0.0.0' }, { address: '224.1.1.1' },
    { address: '169.254.1.1' }, { address: '255.255.255.255' }, { address: '010.0.0.1' },
    { port: 65536 }, { phase: 'unknown' }, { mode: 'x' }, { passwordRequired: 'false' } ]) {
    assert.throws(() => validateListing(listing(changes)), error => error.status === 400);
  }
  assert.equal(validateListing(listing({ address: '8.8.8.8' })).address, '8.8.8.8');
  assert.equal(validateListing(listing({ address: '10.0.0.255' })).address, '10.0.0.255');
  for (const input of [null, [], {}, 'x']) assert.throws(() => validateListing(input));
});
