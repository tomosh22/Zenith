import assert from 'node:assert/strict';
import { freezeSql, thawSql, runSql } from './freeze-legacy.mjs';

const container = process.argv[2];
if (!container?.startsWith('zenith-vault-snapshot-')) throw new Error('Freeze tests require an isolated snapshot container');
const database = `vault_freeze_test_${Date.now()}`;
let created = false;
try {
  runSql(container, 'postgres', `CREATE DATABASE ${database} TEMPLATE vault_snapshot;`); created = true;
  assert.throws(() => runSql(container, database, freezeSql), /ticket is In Progress/);
  // Only this newly-created test database is edited to exercise the drained case.
  runSql(container, database, `UPDATE jira.issues i SET status_id=(SELECT t.id FROM jira.workflow_statuses t WHERE t.workflow_id=s.workflow_id AND t.name='To Do' LIMIT 1) FROM jira.workflow_statuses s WHERE s.id=i.status_id AND s.name='In Progress';`);
  assert.match(runSql(container, database, freezeSql), /writers frozen/);
  assert.throws(() => runSql(container, database, 'UPDATE jira.issues SET title=title;'), /writes are frozen/);
  assert.throws(() => runSql(container, database, 'DELETE FROM notion.pages WHERE false;'), /writes are frozen/);
  assert.throws(() => runSql(container, database, "INSERT INTO public.yjs_documents(document_id,state) VALUES ('notion:page:vault-fence-test','\\x'::bytea);"), /writes are frozen/);
  runSql(container, database, "BEGIN; INSERT INTO public.yjs_documents(document_id,state) VALUES ('figma:page:vault-fence-test','\\x'::bytea); ROLLBACK;");
  assert.match(runSql(container, database, 'SELECT count(*) FROM jira.issues;'), /244/);
  runSql(container, database, thawSql);
  runSql(container, database, 'BEGIN; UPDATE jira.issues SET title=title; ROLLBACK;');
  console.log(JSON.stringify({ passed: 7, liveDatabaseModified: false, sourceSnapshotModified: false }));
} finally {
  if (created) runSql(container, 'postgres', `DROP DATABASE ${database} WITH (FORCE);`);
}
