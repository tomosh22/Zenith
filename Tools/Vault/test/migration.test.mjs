import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import {
  assessReadiness,
  normalizeMutablePolicy,
  planMigration,
  prepareMigration,
} from '../scripts/migrate.mjs';

const TABLE_NAMES = [
  'jira.board_column_statuses', 'jira.board_columns', 'jira.board_quick_filters',
  'jira.boards', 'jira.categories', 'jira.comments', 'jira.custom_field_options',
  'jira.custom_field_values', 'jira.custom_fields', 'jira.field_configurations',
  'jira.issue_activity', 'jira.issue_labels', 'jira.issue_links', 'jira.issue_versions',
  'jira.issues', 'jira.labels', 'jira.permission_grants', 'jira.permission_schemes',
  'jira.project_role_members', 'jira.project_roles', 'jira.projects', 'jira.screen_fields',
  'jira.screens', 'jira.sprints', 'jira.versions', 'jira.workflow_statuses',
  'jira.workflow_transition_rules', 'jira.workflow_transitions', 'jira.workflows',
  'notion.database_properties', 'notion.database_rows', 'notion.databases',
  'notion.projects', 'public.users', 'public.organizations', 'public.memberships',
];

function snapshot() {
  const tables = Object.fromEntries(TABLE_NAMES.map(name => [name, []]));
  tables['jira.projects'] = [
    { id: 'project-zen', key: 'ZEN', name: 'Zenith' },
    { id: 'project-web', key: 'WEB', name: 'Legacy Web' },
  ];
  tables['jira.categories'] = [{ id: 'category-engine', name: 'Engine' }];
  tables['jira.workflow_statuses'] = [{ id: 'status-done', workflow_id: 'workflow-zen', name: 'Done', category: 'DONE', position: 3 }];
  tables['jira.workflows'] = [{ id: 'workflow-zen', project_id: 'project-zen', name: 'Default' }];
  tables['jira.sprints'] = [{ id: 'sprint-zen', project_id: 'project-zen', name: 'Imported', status: 'CLOSED', continuous: false }];
  tables['jira.issues'] = [
    {
      id: 'issue-zen', project_id: 'project-zen', number: 1, title: 'Mapped ticket', type: 'TASK',
      category_id: 'category-engine', status_id: 'status-done', priority: 'MEDIUM', complexity: 'SIMPLE',
      risk: 'LOW', position: 1, description: null,
    },
    {
      id: 'issue-web', project_id: 'project-web', number: 1, title: 'Archived ticket', type: 'TASK',
      category_id: 'category-engine', status_id: 'status-done', priority: 'LOW', complexity: 'TRIVIAL',
      risk: 'LOW', position: 2, description: null,
    },
  ];
  return { tables };
}

const policy = { resolved: { projects: { ZEN: { project: 'ZEN', categories: { engine: { project: 'ZEN' } } } } } };
const hashes = { snapshotEvidence: 'a'.repeat(64), backupSha: 'b'.repeat(40), dumpSha256: 'c'.repeat(64) };

test('readiness requires real evidence, a frozen writer set, and no active tickets', async () => {
  const value = snapshot();
  const missing = await assessReadiness(value, { snapshotEvidence: 'opaque-reference', backupSha: 'backup', dumpSha256: 'dump', requireWritersFrozen: true });
  assert.equal(missing.ready, false);
  assert.equal(missing.snapshotEvidence.valid, false);
  assert.equal(missing.backup.valid, false);
  assert.equal(missing.dump.valid, false);
  assert.equal(missing.writersFrozen.supplied, false);

  value.tables['jira.workflow_statuses'][0].name = 'In Progress';
  value.tables['jira.workflow_statuses'][0].category = 'IN_PROGRESS';
  const frozen = await assessReadiness(value, { ...hashes, writersFrozen: true, requireWritersFrozen: true });
  assert.equal(frozen.ready, false);
  assert.deepEqual(frozen.inProgress.map(item => item.key), ['ZEN-1', 'WEB-1']);
  assert.match(frozen.reasons[0], /2 ticket/);
  value.tables['jira.workflow_statuses'][0].name = 'In Review';
  const parked = await assessReadiness(value, { ...hashes, writersFrozen: true, requireWritersFrozen: true });
  assert.deepEqual(parked.inProgress, [], 'In Review is not the named In Progress drain condition');
  assert.equal(parked.preparationReady, true);
  assert.equal(parked.productionReady, false, 'operator attestation alone is still not production freeze evidence');
});

test('generated mutable policies use the canonical checkout for every project', () => {
  const value = normalizeMutablePolicy({ resolved: { projects: {
    ZEN: { project: 'ZEN', branching: 'direct' },
    DP: { project: 'DP', branching: 'branch', branchPrefix: 'agent/' },
  } } });
  assert.equal(value.resolved.projects.ZEN.branching, 'direct');
  assert.equal(value.resolved.projects.DP.branching, 'direct');
  assert.equal(Object.hasOwn(value.resolved.projects.DP, 'branchPrefix'), false);
});

test('dry-run is read-only and prepare maps six-style roots while archiving legacy tickets', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-migration-test-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  const destination = path.join(root, 'prepared');
  const sourceRepo = path.join(root, 'source-repo');
  await fs.mkdir(path.join(sourceRepo, 'Docs'), { recursive: true });
  await fs.writeFile(path.join(sourceRepo, 'Docs', 'Roadmap.md'), '# Living roadmap\n');
  const value = snapshot();

  const plan = await planMigration({ snapshot: value, policy, ...hashes, includeLivingDocs: true, livingDirs: ['Docs'], sourceRepo });
  assert.equal(plan.mappedIssueCount, 1);
  assert.equal(plan.archivedIssueCount, 1);
  assert.deepEqual(plan.archivedProjects.map(item => item.project), ['WEB']);
  assert.ok(plan.supportingFiles.includes('Docs/Roadmap.md'));
  assert.equal(await fs.stat(destination).then(() => true, error => error.code !== 'ENOENT'), false);

  const prepared = await prepareMigration({
    snapshot: value,
    policy,
    destination,
    sourceRepo,
    includeLivingDocs: true,
    livingDirs: ['Docs'],
    ...hashes,
    writersFrozen: true,
  });
  assert.equal(prepared.audit.ok, true);
  assert.equal(prepared.audit.entityDifferences.length, 0);
  assert.equal(prepared.audit.entityCount, TABLE_NAMES.length);
  assert.equal(prepared.audit.ticketCount, 2);
  assert.equal(prepared.audit.mapped, 1);
  assert.equal(prepared.audit.archived, 1);
  assert.equal(prepared.config.boardRoots.length, 2);
  await fs.access(path.join(destination, 'Zenith', 'Docs', 'Board', 'tickets', 'ZEN-1.md'));
  await fs.access(path.join(destination, 'archive', 'legacy', 'WEB', 'tickets', 'WEB-1.md'));
  await fs.access(path.join(destination, 'Docs', 'Roadmap.md'));
  const config = JSON.parse(await fs.readFile(path.join(destination, 'vault.config.json'), 'utf8'));
  assert.equal(config.writes.enabled, false);
  assert.equal(config.readiness.ready, true);
  assert.equal(config.readiness.productionReady, false);
  assert.equal(config.readiness.writersFrozen.kind, 'operator-attestation');
  assert.equal(config.boardRoots.find(item => item.project === 'WEB').readOnly, true);
  assert.ok(config.supportingFiles.some(item => (typeof item === 'string' ? item : item.path) === 'Docs/Roadmap.md'));

  await assert.rejects(
    prepareMigration({ snapshot: value, policy, destination, ...hashes, writersFrozen: true }),
    error => error.code === 'EEXIST',
  );
});
