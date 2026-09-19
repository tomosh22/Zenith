import { spawnSync } from 'node:child_process';
import { mkdirSync, openSync, closeSync } from 'node:fs';
import path from 'node:path';

// Deliberately explicit: the only operation against the source is pg_dump.
const [source, destination, output, roleArg] = process.argv.slice(2);
const role = roleArg ?? process.env.VAULT_SNAPSHOT_ROLE;
if (!source || !destination?.startsWith('zenith-vault-snapshot-') || !output || !role) {
  throw new Error('Usage: node snapshot.mjs SOURCE_CONTAINER zenith-vault-snapshot-NAME OUTPUT.dump SOURCE_ROLE (or set VAULT_SNAPSHOT_ROLE); SOURCE_ROLE is the source Postgres role, also used as the database name to dump.');
}
function docker(args, options = {}) {
  const r = spawnSync('docker', args, { encoding: 'utf8', ...options });
  if (r.error || r.status !== 0) throw new Error(r.error?.message || r.stderr || `docker exited ${r.status}`);
  return r.stdout;
}
mkdirSync(path.dirname(path.resolve(output)), { recursive: true });
const fd = openSync(output, 'wx');
try { docker(['exec', source, 'pg_dump', '-U', role, '-d', role, '-Fc'], { stdio: ['ignore', fd, 'pipe'] }); }
finally { closeSync(fd); }
docker(['run', '-d', '--name', destination, '-p', '127.0.0.1:55439:5432', '-e', `POSTGRES_USER=${role}`, '-e', 'POSTGRES_PASSWORD=vault-local-snapshot', '-e', 'POSTGRES_DB=vault_snapshot', 'postgres:16']);
let ready = false;
for (let i = 0; i < 40; i++) {
  const r = spawnSync('docker', ['exec', destination, 'pg_isready', '-U', role, '-d', 'vault_snapshot'], { encoding: 'utf8' });
  if (r.status === 0) { ready = true; break; }
  await new Promise(resolve => setTimeout(resolve, 500));
}
if (!ready) throw new Error('Snapshot container did not become ready; source was not modified.');
const input = openSync(output, 'r');
try { docker(['exec', '-i', destination, 'pg_restore', '-U', role, '-d', 'vault_snapshot', '--no-owner', '--no-privileges', '--exit-on-error'], { stdio: [input, 'pipe', 'pipe'] }); }
finally { closeSync(input); }
docker(['exec', destination, 'psql', '-U', role, '-d', 'vault_snapshot', '-v', 'ON_ERROR_STOP=1', '-c',
  "CREATE ROLE vault_reader LOGIN PASSWORD 'vault-local-readonly'; GRANT CONNECT ON DATABASE vault_snapshot TO vault_reader; GRANT USAGE ON SCHEMA public, jira, notion TO vault_reader; GRANT SELECT ON ALL TABLES IN SCHEMA public, jira, notion TO vault_reader; ALTER ROLE vault_reader SET default_transaction_read_only = on;"]);
console.log(`Restored snapshot into ${destination} on localhost:55439. Source remains unchanged.`);
