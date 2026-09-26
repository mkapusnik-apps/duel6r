import { Firestore } from '@google-cloud/firestore';
import { configuration } from './config.js';
import { Directory } from './directory.js';
import { createServer } from './http.js';

// Configuration failures must never echo environment values or SDK diagnostics.
try {
  const config = configuration();
  const db = new Firestore({ projectId: config.projectId });
  const server = createServer(new Directory(db));
  server.on('error', () => { console.error('Directory startup failed.'); process.exitCode = 1; });
  server.listen(config.port, '0.0.0.0');
  let stopping = false;
  const stop = () => {
    if (stopping) return;
    stopping = true;
    const deadline = setTimeout(() => process.exit(1), 9000);
    deadline.unref();
    server.close(async () => {
      try { await db.terminate(); } catch { /* No raw SDK diagnostics. */ }
      clearTimeout(deadline);
    });
  };
  process.on('SIGTERM', stop);
  process.on('SIGINT', stop);
} catch {
  console.error('Directory configuration failed.');
  process.exitCode = 1;
}
