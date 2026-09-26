import http from 'node:http';
import { DirectoryError, MAX_BODY_BYTES } from './directory.js';

function reply(response, status, body) {
  if (response.destroyed || response.writableEnded) return;
  response.writeHead(status, {
    'Content-Type': 'application/json', 'Cache-Control': 'no-store',
    'X-Content-Type-Options': 'nosniff'
  });
  response.end(body === undefined ? undefined : JSON.stringify(body));
}

async function readBody(request) {
  if (request.headers['content-type'] !== 'application/json') throw new DirectoryError(415);
  const chunks = [];
  let length = 0;
  for await (const chunk of request) {
    length += chunk.length;
    if (length > MAX_BODY_BYTES) throw new DirectoryError(413);
    chunks.push(chunk);
  }
  try { return JSON.parse(Buffer.concat(chunks).toString('utf8')); }
  catch { throw new DirectoryError(400); }
}

export function createServer(directory) {
  let inFlight = 0;
  // Per-instance ingress guard complements the shared database quotas. Do not
  // trust X-Forwarded-For, and do not store addresses or credential-bearing logs.
  let windowStart = Date.now(), requests = 0;
  const server = http.createServer({ maxHeaderSize: 4096, requestTimeout: 5000, headersTimeout: 5000 }, async (req, res) => {
    if (Date.now() - windowStart >= 60_000) { windowStart = Date.now(); requests = 0; }
    if (++requests > 1200 || inFlight >= 32) { reply(res, 429, { error: 'Directory unavailable.' }); return; }
    inFlight++;
    // Response deadline is bounded, but work remains counted until it settles.
    // No timed-out promise may escape the concurrent-work cap.
    const timer = setTimeout(() => reply(res, 503, { error: 'Directory unavailable.' }), 8000);
    try {
      if (!req.url || req.url.length > 256) throw new DirectoryError(400);
      const url = new URL(req.url, 'http://directory.invalid');
      if (req.method === 'GET' && url.pathname === '/healthz' && !url.search) {
        reply(res, 200, { status: 'ok' }); return;
      }
      if (req.method === 'GET' && url.pathname === '/v1/listings') {
        if ([...url.searchParams.keys()].some(key => key !== 'cursor') || url.searchParams.getAll('cursor').length > 1) {
          throw new DirectoryError(400);
        }
        reply(res, 200, await directory.list(url.searchParams.get('cursor') ?? undefined)); return;
      }
      if (req.method === 'POST' && url.pathname === '/v1/listings' && !url.search) {
        reply(res, 201, await directory.register(await readBody(req))); return;
      }
      const match = /^\/v1\/listings\/([a-f0-9]{32})$/.exec(url.pathname);
      if (match && !url.search && ['PUT', 'DELETE'].includes(req.method)) {
        const authorization = req.headers.authorization ?? '';
        if (!/^Bearer [a-f0-9]{64}$/.test(authorization)) throw new DirectoryError(401);
        const revision = req.headers['if-match'];
        if (!/^[1-9][0-9]{0,14}$/.test(revision ?? '')) throw new DirectoryError(400);
        const value = await directory.mutate(match[1], authorization.slice(7), Number(revision),
          req.method === 'DELETE' ? null : await readBody(req));
        reply(res, value === null ? 204 : 200, value === null ? undefined : value); return;
      }
      throw new DirectoryError(404);
    } catch (error) {
      const status = error instanceof DirectoryError ? error.status : 503;
      reply(res, status, { error: status === 503 || status === 429 ? 'Directory unavailable.' : 'Directory request rejected.' });
    } finally {
      clearTimeout(timer);
      inFlight--;
      if (!req.complete) req.destroy();
    }
  });
  server.maxConnections = 64;
  server.keepAliveTimeout = 2000;
  server.setTimeout(10_000, socket => socket.destroy());
  return server;
}
