import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { promises as fs } from 'node:fs';
import { git, resolveAuthority } from '../server/operations.mjs';

// Schema-scoped fences leave authentication, Figma and Mermaid available.
// SELECT remains available for backup, capture and audit. No table is dropped.
export const freezeSql = `
BEGIN;
SET LOCAL lock_timeout = '10s';
DO $$ DECLARE r record; BEGIN
  FOR r IN SELECT schemaname, tablename FROM pg_tables WHERE schemaname IN ('jira','notion') ORDER BY schemaname,tablename LOOP
    EXECUTE format('LOCK TABLE %I.%I IN ACCESS EXCLUSIVE MODE',r.schemaname,r.tablename);
  END LOOP;
  IF EXISTS (SELECT 1 FROM jira.issues i JOIN jira.workflow_statuses s ON s.id=i.status_id WHERE s.name='In Progress') THEN
    RAISE EXCEPTION 'Cannot freeze: a ticket is In Progress';
  END IF;
END $$;
CREATE SCHEMA IF NOT EXISTS zenith_vault_retirement;
CREATE OR REPLACE FUNCTION zenith_vault_retirement.refuse_board_write() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN RAISE EXCEPTION 'Legacy board/docs writes are frozen; use Zenith Vault' USING ERRCODE='55000'; END $$;
CREATE OR REPLACE FUNCTION zenith_vault_retirement.refuse_shared_write() RETURNS trigger LANGUAGE plpgsql AS $$
DECLARE oldrow jsonb; newrow jsonb;
BEGIN
  IF TG_OP <> 'INSERT' THEN oldrow := to_jsonb(OLD); END IF;
  IF TG_OP <> 'DELETE' THEN newrow := to_jsonb(NEW); END IF;
  IF coalesce(oldrow->>'document_id','') LIKE 'notion:%' OR coalesce(newrow->>'document_id','') LIKE 'notion:%'
    OR coalesce(oldrow->>'document_type','') IN ('NOTION_PAGE','JIRA_ISSUE','JIRA_COMMENT')
    OR coalesce(newrow->>'document_type','') IN ('NOTION_PAGE','JIRA_ISSUE','JIRA_COMMENT') THEN
    RAISE EXCEPTION 'Legacy board/docs writes are frozen; use Zenith Vault' USING ERRCODE='55000';
  END IF;
  IF TG_OP='DELETE' THEN RETURN OLD; ELSE RETURN NEW; END IF;
END $$;
DO $$ DECLARE r record; shared text; BEGIN
  FOR r IN SELECT schemaname,tablename FROM pg_tables WHERE schemaname IN ('jira','notion') LOOP
    EXECUTE format('DROP TRIGGER IF EXISTS zenith_vault_write_fence ON %I.%I',r.schemaname,r.tablename);
    EXECUTE format('CREATE TRIGGER zenith_vault_write_fence BEFORE INSERT OR UPDATE OR DELETE OR TRUNCATE ON %I.%I FOR EACH STATEMENT EXECUTE FUNCTION zenith_vault_retirement.refuse_board_write()',r.schemaname,r.tablename);
  END LOOP;
  FOREACH shared IN ARRAY ARRAY['yjs_documents','document_versions'] LOOP
    EXECUTE format('DROP TRIGGER IF EXISTS zenith_vault_write_fence ON public.%I',shared);
    EXECUTE format('CREATE TRIGGER zenith_vault_write_fence BEFORE INSERT OR UPDATE OR DELETE ON public.%I FOR EACH ROW EXECUTE FUNCTION zenith_vault_retirement.refuse_shared_write()',shared);
  END LOOP;
END $$;
COMMIT;
SELECT 'legacy board/docs writers frozen' AS result;
`;

export const thawSql = `
BEGIN;
DO $$ DECLARE r record; BEGIN
  FOR r IN SELECT n.nspname AS schema,c.relname AS name FROM pg_trigger t JOIN pg_class c ON c.oid=t.tgrelid JOIN pg_namespace n ON n.oid=c.relnamespace
    WHERE t.tgname='zenith_vault_write_fence' AND n.nspname IN ('jira','notion','public') LOOP
    EXECUTE format('DROP TRIGGER zenith_vault_write_fence ON %I.%I',r.schema,r.name);
  END LOOP;
END $$;
DROP FUNCTION IF EXISTS zenith_vault_retirement.refuse_board_write();
DROP FUNCTION IF EXISTS zenith_vault_retirement.refuse_shared_write();
DROP SCHEMA IF EXISTS zenith_vault_retirement;
COMMIT;
`;

export function runSql(container, database, sql, role = process.env.VAULT_SNAPSHOT_ROLE) {
  if (!role) throw new Error('Set VAULT_SNAPSHOT_ROLE to the Postgres role used to connect (see README "Reproduce the capture").');
  if (!/^[A-Za-z0-9_-]+$/.test(container) || !/^[A-Za-z0-9_-]+$/.test(database)) throw new Error('Invalid container or database name');
  const result = spawnSync('docker', ['exec', '-i', container, 'psql', '-U', role, '-d', database, '-v', 'ON_ERROR_STOP=1', '-X', '-q'], { input: sql, encoding: 'utf8', windowsHide: true });
  if (result.error || result.status !== 0) throw new Error(result.error?.message ?? result.stderr);
  return result.stdout;
}

// A reverted claim still crossed the recovery boundary. Inspect durable journals
// as well as all reachable operation commits; absence of a current assignee is
// not evidence that no claim happened.
export async function assertBeforeFirstClaim(vaultRoot) {
  if (!vaultRoot) throw new Error('Thaw requires --vault ROOT to verify that no new claim occurred');
  const root = await resolveAuthority(path.resolve(vaultRoot));
  await fs.access(path.join(root, 'vault.config.json'));
  const common = path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
  const directory = path.join(common, 'vault/operations');
  let names = [];
  try { names = await fs.readdir(directory); } catch (e) { if (e.code !== 'ENOENT') throw e; }
  for (const name of names.filter(n => n.endsWith('.json'))) {
    const journal = JSON.parse(await fs.readFile(path.join(directory, name), 'utf8'));
    if (['claim', 'next'].includes(journal.kind)) throw new Error('A vault claim was journaled; forward recovery is required');
  }
  const log = await git(root, ['log', '--all', '--format=%B%x00']);
  if (log.split('\0').some(message => /Vault-Operation:/.test(message) && /^board:\s*(claim|next)\b/im.test(message))) {
    throw new Error('A vault claim was committed; forward recovery is required');
  }
}

if (path.resolve(process.argv[1] ?? '') === fileURLToPath(import.meta.url)) {
  const [container, database, mode] = process.argv.slice(2);
  if (!container || !database || !['--preview','--freeze','--thaw-before-first-claim'].includes(mode)) throw new Error('Usage: freeze-legacy.mjs CONTAINER DATABASE --preview|--freeze|--thaw-before-first-claim [--vault ROOT]');
  if (mode === '--thaw-before-first-claim') {
    const at = process.argv.indexOf('--vault');
    await assertBeforeFirstClaim(at >= 0 ? process.argv[at + 1] : null);
  }
  if (mode === '--preview') console.log(freezeSql);
  else console.log(runSql(container, database, mode === '--freeze' ? freezeSql : thawSql));
}
