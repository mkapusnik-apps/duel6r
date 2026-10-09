export function configuration(env = process.env) {
  const mode = env.NODE_ENV ?? 'production';
  if (!['production', 'development', 'test'].includes(mode)) throw new Error('Invalid environment.');
  const projectId = env.GOOGLE_CLOUD_PROJECT;
  if (!projectId || !/^[a-z][a-z0-9-]{4,61}[a-z0-9]$/.test(projectId)) {
    throw new Error('An explicit GOOGLE_CLOUD_PROJECT is required.');
  }
  const databaseId = env.D6R_DIRECTORY_FIRESTORE_DATABASE ?? '(default)';
  if (databaseId !== '(default)' && (
    !/^[a-z][a-z0-9-]{2,61}[a-z0-9]$/.test(databaseId) || /[^a-z0-9-]/.test(databaseId) ||
    /^[a-f0-9]{8}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{4}-[a-f0-9]{12}$/.test(databaseId)
  )) {
    throw new Error('Invalid D6R_DIRECTORY_FIRESTORE_DATABASE.');
  }
  const emulator = env.FIRESTORE_EMULATOR_HOST;
  if (emulator && (mode === 'production' || !/^[a-zA-Z0-9.-]+:[0-9]{1,5}$/.test(emulator))) {
    throw new Error('Emulator requires explicit test or development configuration.');
  }
  if (mode === 'test' && (!emulator || !projectId.startsWith('demo-'))) {
    throw new Error('Tests require a demo project and Firestore emulator.');
  }
  const portText = env.PORT ?? '8080';
  if (!/^[0-9]{1,5}$/.test(portText) || Number(portText) < 1 || Number(portText) > 65535) {
    throw new Error('Invalid PORT.');
  }
  return { projectId, databaseId, port: Number(portText), mode };
}
