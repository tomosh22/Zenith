import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import {
  MAPPED_PROJECT_KEYS,
  assessReadiness,
  normalizeMutablePolicy,
  planMigration,
  prepareMigration,
  runMigrationCli,
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

/** Every table present but empty, plus one project row so a mapped key resolves. */
function emptySnapshot() {
  const value = { tables: Object.fromEntries(TABLE_NAMES.map(name => [name, []])) };
  value.tables['jira.projects'] = [{ id: 'project-zen', key: 'ZEN', name: 'Zenith' }];
  return value;
}

const EVIDENCE = 'a'.repeat(64);
const BACKUP = 'b'.repeat(40);
const DUMP = 'c'.repeat(64);
const FREEZE_HASH = 'f'.repeat(64);
const hashes = { snapshotEvidence: EVIDENCE, backupSha: BACKUP, dumpSha256: DUMP };
const policy = { resolved: { projects: { ZEN: { project: 'ZEN', categories: { engine: { project: 'ZEN' } } } } } };

async function tempRoot(t, label) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), `vault-migrate-${label}-`));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  return root;
}

const sha256OfFile = async file => createHash('sha256').update(await fs.readFile(file)).digest('hex');

test('readiness refuses a snapshot without a tables object before reading evidence', async () => {
  for (const value of [null, {}]) {
    const result = await assessReadiness(value, {});
    assert.equal(result.ready, false);
    assert.equal(result.productionReady, false);
    assert.deepEqual(result.reasons, ['Snapshot must contain a tables object.']);
    assert.deepEqual(result.productionReasons, ['Snapshot must contain a tables object.']);
    assert.equal(result.snapshotEvidence.supplied, false);
    assert.equal(result.backup.supplied, false);
    assert.equal(result.dump.supplied, false);
    assert.deepEqual(result.inProgress, []);
    assert.equal(result.issueCount, 0);
    assert.deepEqual(result.counts, {});
  }
});

test('in-progress detection names the drain condition and tolerates unknown projects', async () => {
  const value = {
    tables: {
      'jira.workflow_statuses': [{ id: 's1', name: 'In progress', category: 'IN_PROGRESS' }],
      'jira.projects': [{ id: 'p1', key: 'ZEN' }],
      'jira.issues': [
        { id: 'i1', project_id: 'p1', number: 1, status_id: 's1' },
        { id: 'i2', project_id: 'p1', number: 2, status_id: 'ghost-status' },
        { id: 'i3', project_id: 'project-unknown', number: 3, status_id: 's1' },
      ],
    },
  };
  const result = await assessReadiness(value, {});
  assert.deepEqual(result.inProgress.map(item => [item.key, item.project, item.status, item.category]), [
    ['ZEN-1', 'ZEN', 'In progress', 'IN_PROGRESS'],
    [null, null, 'In progress', 'IN_PROGRESS'],
  ]);
  assert.equal(result.issueCount, 3);
  assert.match(result.reasons.join(' '), /2 ticket\(s\) are still In Progress/);
});

test('snapshot evidence accepts readable files and hashes and rejects everything else', async t => {
  const root = await tempRoot(t, 'evidence');
  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify({ tables: {} }));
  const digest = await sha256OfFile(source);
  const ghost = path.join(root, 'ghost.json');
  const empty = path.join(root, 'empty.json');
  await fs.writeFile(empty, '');

  // No evidence at all.
  const none = await assessReadiness({ tables: {} }, {});
  assert.equal(none.snapshotEvidence.supplied, false);
  assert.equal(none.snapshotEvidence.valid, false);
  assert.match(none.reasons.join(' '), /A snapshot evidence path or hash is required/);

  // A path with a matching hash is valid; a mismatching hash is not.
  const byPath = await assessReadiness({ tables: {} }, { snapshotEvidence: { path: source, sha256: digest } });
  assert.equal(byPath.snapshotEvidence.valid, true);
  assert.equal(byPath.snapshotEvidence.sha256, digest);
  const mismatch = await assessReadiness({ tables: {} }, { snapshotEvidence: { path: source, sha256: 'd'.repeat(64) } });
  assert.match(mismatch.snapshotEvidence.reason, /hash does not match its file/);

  // A zero-byte file supplies evidence but cannot validate.
  const zero = await assessReadiness({ tables: {} }, { snapshotEvidence: empty });
  assert.equal(zero.snapshotEvidence.supplied, true);
  assert.equal(zero.snapshotEvidence.valid, false);
  assert.match(zero.reasons.join(' '), /Snapshot evidence is invalid/);

  // A directory fails to read with a non-ENOENT error and is reported.
  const directory = await assessReadiness({ tables: {} }, { snapshotEvidence: root });
  assert.match(directory.snapshotEvidence.reason, /Cannot read snapshot evidence/);

  // A bare hexadecimal string is accepted; opaque text is refused.
  const hash = await assessReadiness({ tables: {} }, { snapshotEvidence: EVIDENCE });
  assert.equal(hash.snapshotEvidence.valid, true);
  const junk = await assessReadiness({ tables: {} }, { snapshotEvidence: 'opaque-reference' });
  assert.match(junk.snapshotEvidence.reason, /readable file or a SHA-256 hash/);

  // Object evidence carrying no path/hash/value cannot be empty.
  const kindOnly = await assessReadiness({ tables: {} }, { snapshotEvidence: { kind: 'opaque' } });
  assert.equal(kindOnly.snapshotEvidence.supplied, true);
  assert.match(kindOnly.snapshotEvidence.reason, /cannot be empty/);

  // A path-shaped string equal to the source resolves through the source match.
  const matched = await assessReadiness({ tables: {} }, { snapshotEvidence: ghost, sourcePath: ghost });
  assert.equal(matched.snapshotEvidence.valid, true);
  assert.equal(matched.snapshotEvidence.sourcePath, path.resolve(ghost));

  // An unknown path-shaped string with no source match is refused.
  const stranger = await assessReadiness({ tables: {} }, { snapshotEvidence: ghost });
  assert.match(stranger.snapshotEvidence.reason, /readable file or a SHA-256 hash/);
});

test('backup and dump hashes are required in the documented shapes', async () => {
  const value = { tables: {} };
  const missing = await assessReadiness(value, { snapshotEvidence: EVIDENCE });
  assert.match(missing.backup.reason, /Backup SHA is required/);
  assert.match(missing.dump.reason, /Dump SHA-256 is required/);

  const malformed = await assessReadiness(value, { snapshotEvidence: EVIDENCE, backupSha: 'nope', dumpSha256: 'short' });
  assert.match(malformed.backup.reason, /SHA-1\/SHA-256/);
  assert.match(malformed.dump.reason, /hexadecimal SHA-256/);

  const ok = await assessReadiness(value, { snapshotEvidence: EVIDENCE, backupSHA: BACKUP, dumpHash: DUMP });
  assert.equal(ok.backup.valid, true);
  assert.equal(ok.dump.valid, true);
  assert.deepEqual(ok.reasons, []);
  assert.equal(ok.preparationReady, true);
  assert.equal(ok.ready, true);
  assert.equal(ok.productionReady, false, 'production still requires writer-freeze evidence');
});

test('writer-freeze evidence accepts marked JSON, digest files and hashes', async t => {
  const root = await tempRoot(t, 'freeze');
  const marked = path.join(root, 'freeze.json');
  await fs.writeFile(marked, JSON.stringify({ frozen: true }));

  const byPath = await assessReadiness({ tables: {} }, { writersFrozen: { path: marked } });
  assert.equal(byPath.writersFrozen.valid, true);
  assert.equal(byPath.writersFrozen.kind, 'file');

  for (const [name, body] of [
    ['wf.json', JSON.stringify({ writersFrozen: true })],
    ['state.json', JSON.stringify({ state: 'frozen' })],
    ['kind.json', JSON.stringify({ kind: 'writer-freeze' })],
  ]) {
    const file = path.join(root, name);
    await fs.writeFile(file, body);
    const result = await assessReadiness({ tables: {} }, { writersFrozen: { path: file } });
    assert.equal(result.writersFrozen.valid, true, `${name} should certify the freeze`);
  }

  // A file that is nothing but a SHA-256 digest certifies the freeze too.
  const digestFile = path.join(root, 'freeze.sha256');
  await fs.writeFile(digestFile, `${FREEZE_HASH}\n`);
  const byDigest = await assessReadiness({ tables: {} }, { writersFrozen: { path: digestFile } });
  assert.equal(byDigest.writersFrozen.valid, true);

  // An unmarked JSON file and a prose file are both refused.
  const plain = path.join(root, 'plain.json');
  await fs.writeFile(plain, JSON.stringify({ hello: 'world' }));
  const unmarked = await assessReadiness({ tables: {} }, { writersFrozen: { path: plain } });
  assert.match(unmarked.writersFrozen.reason, /marked frozen in JSON or carry a SHA-256 digest/);

  // A supplied sha256 certifies any readable file, and a wrong one is refused.
  const plainDigest = await sha256OfFile(plain);
  const withSha = await assessReadiness({ tables: {} }, { writersFrozen: { path: plain, sha256: plainDigest } });
  assert.equal(withSha.writersFrozen.valid, true);
  const wrongSha = await assessReadiness({ tables: {} }, { writersFrozen: { path: marked, sha256: '0'.repeat(64) } });
  assert.match(wrongSha.writersFrozen.reason, /hash does not match its file/);

  // A directory read error is reported; a missing file falls back to hash form.
  const directory = await assessReadiness({ tables: {} }, { writersFrozen: { path: root } });
  assert.match(directory.writersFrozen.reason, /Cannot read writer-freeze evidence/);
  const ghost = await assessReadiness({ tables: {} }, { writersFrozen: path.join(root, 'missing', 'freeze.json') });
  assert.equal(ghost.writersFrozen.valid, false);
  assert.equal(ghost.writersFrozen.kind, 'invalid');

  // A bare hash string and a sha256-only object are both valid.
  const byHash = await assessReadiness({ tables: {} }, { writersFrozen: FREEZE_HASH });
  assert.equal(byHash.writersFrozen.valid, true);
  assert.equal(byHash.writersFrozen.kind, 'hash');
  const shaOnly = await assessReadiness({ tables: {} }, { writersFrozen: { sha256: FREEZE_HASH } });
  assert.equal(shaOnly.writersFrozen.valid, true);

  // The bare flag is an attestation: required and never production-valid.
  const attestation = await assessReadiness({ tables: {} }, { ...hashes, writersFrozen: true, requireWritersFrozen: true });
  assert.equal(attestation.writersFrozen.kind, 'operator-attestation');
  assert.equal(attestation.writersFrozen.required, true);
  assert.equal(attestation.writersFrozen.valid, false);
  assert.match(attestation.productionReasons.join(' '), /operator attestation/);
  assert.deepEqual(attestation.reasons, [], 'a supplied attestation satisfies the readiness boundary');

  // Without the required flag the attestation only blocks production.
  const optional = await assessReadiness({ tables: {} }, { ...hashes, writersFrozen: true });
  assert.deepEqual(optional.reasons, []);
  assert.equal(optional.ready, true);
});

test('normalizeMutablePolicy strips branching in every accepted shape', () => {
  // Bare projects map.
  const bare = normalizeMutablePolicy({
    projects: {
      ZEN: { branching: 'branch', branchPrefix: 'agent/' },
      DP: { branching: 'branch', categories: { DevilsPlayground: { branching: 'branch', branchPrefix: 'p/' } } },
    },
  });
  assert.equal(bare.resolved.projects.ZEN.branching, 'direct');
  assert.equal(Object.hasOwn(bare.resolved.projects.ZEN, 'branchPrefix'), false);
  assert.equal(bare.resolved.projects.DP.categories.DevilsPlayground.branching, 'direct');
  assert.equal(Object.hasOwn(bare.resolved.projects.DP.categories.DevilsPlayground, 'branchPrefix'), false);

  // Client payload with categories: per-project rows are merged and cleaned.
  const client = normalizeMutablePolicy({
    client: {
      branching: 'branch',
      branchPrefix: 'agent/',
      gates: ['echo ok'],
      categories: {
        Zenithmon: { project: 'ZM', branching: 'branch', branchPrefix: 'agent/' },
        Engine: { branching: 'branch' },
      },
    },
  });
  assert.equal(client.resolved.projects.ZEN.branching, 'direct');
  assert.equal(client.resolved.projects.ZM.branching, 'direct');
  assert.equal(Object.hasOwn(client.resolved.projects.ZM, 'branchPrefix'), false);
  assert.equal(client.resolved.projects.Engine, undefined, 'a category without a project does not become one');
  assert.equal(client.client, undefined, 'the client payload is folded into resolved projects');

  // Nothing at all defaults every mapped project.
  const fallback = normalizeMutablePolicy();
  for (const key of MAPPED_PROJECT_KEYS) {
    assert.equal(fallback.resolved.projects[key].project, key);
    assert.equal(fallback.resolved.projects[key].branching, 'direct');
  }

  // A resolved policy with a client file: the file and its nested categories
  // are cleaned; a sibling category map is left to the file-only branch.
  const withFile = normalizeMutablePolicy({
    resolved: { projects: { ZEN: { branching: 'branch', branchPrefix: 'x/' } } },
    client: { file: { branching: 'branch', branchPrefix: 'y/', categories: { Zenithmon: { branching: 'branch', branchPrefix: 'z/' } } } },
  });
  assert.equal(withFile.resolved.projects.ZEN.branching, 'direct');
  assert.equal(Object.hasOwn(withFile.resolved.projects.ZEN, 'branchPrefix'), false);
  assert.equal(withFile.client.file.branching, 'direct');
  assert.equal(Object.hasOwn(withFile.client.file, 'branchPrefix'), false);
  assert.equal(withFile.client.file.categories.Zenithmon.branching, 'direct');

  // Client categories without a file are cleaned in place — but only for a
  // resolved-shaped policy, the one shape that carries the client through.
  const withCategories = normalizeMutablePolicy({
    resolved: { projects: { ZEN: { branching: 'direct' } } },
    client: { categories: { Engine: { branching: 'branch', branchPrefix: 'w/' } } },
  });
  assert.equal(withCategories.client.categories.Engine.branching, 'direct');
  assert.equal(Object.hasOwn(withCategories.client.categories.Engine, 'branchPrefix'), false);
});

test('planMigration collects supporting files and discovers living docs', async t => {
  const root = await tempRoot(t, 'living');
  const sourceRepo = path.join(root, 'repo');
  await fs.mkdir(path.join(sourceRepo, 'Docs', 'Nested'), { recursive: true });
  await fs.mkdir(path.join(sourceRepo, 'Games', 'Zenithmon', 'Docs'), { recursive: true });
  await fs.writeFile(path.join(sourceRepo, 'Docs', 'Roadmap.md'), '# Roadmap\n');
  await fs.writeFile(path.join(sourceRepo, 'Docs', 'notes.txt'), 'not markdown');
  await fs.writeFile(path.join(sourceRepo, 'Docs', 'Nested', 'Deep.md'), '# Deep\n');
  await fs.writeFile(path.join(sourceRepo, 'Games', 'Zenithmon', 'Docs', 'Status.md'), '# Status\n');
  const value = snapshot();
  const clientPolicy = {
    client: { categories: { Zenithmon: { project: 'ZM', docs: { dir: 'Games/Zenithmon/Docs' } } } },
  };

  const plan = await planMigration({
    snapshot: value,
    policy: clientPolicy,
    mappedProjects: ['ZEN'],
    ...hashes,
    supportingFiles: [
      'Docs/Roadmap.md',
      { path: 'Docs/Nested/Deep.md', title: 'Deep' },
      'DOCS/ROADMAP.MD',
      { file: 'Docs/from-file.md' },
      null,
      42,
    ],
    includeLivingDocs: true,
    livingDirs: ['Docs'],
    sourceRepo,
  });
  assert.equal(plan.source.path, '<in-memory snapshot>');
  assert.equal(plan.mode, 'dry-run');
  assert.equal(plan.destination, null);
  assert.deepEqual(plan.supportingFiles, [
    'Docs/Roadmap.md',
    { title: 'Deep', path: 'Docs/Nested/Deep.md' },
    { file: 'Docs/from-file.md', path: 'Docs/from-file.md' },
    'Games/Zenithmon/Docs/Status.md',
  ]);
  assert.deepEqual(plan.boardRoots[0], { project: 'ZEN', path: 'Zenith/Docs/Board' });
  assert.deepEqual(plan.boardRoots[1], { project: 'WEB', path: 'archive/legacy/WEB', readOnly: true, archive: true });

  // A bare livingDocs string is accepted as a single supporting file.
  const single = await planMigration({
    snapshot: value,
    mappedProjects: ['ZEN'],
    ...hashes,
    livingDocs: 'Docs/Roadmap.md',
  });
  assert.deepEqual(single.supportingFiles, ['Docs/Roadmap.md']);
});

test('planMigration validates supporting paths and living doc directories', async t => {
  const root = await tempRoot(t, 'living-errors');
  const sourceRepo = path.join(root, 'repo');
  await fs.mkdir(sourceRepo, { recursive: true });
  const value = snapshot();

  await assert.rejects(
    planMigration({ snapshot: value, mappedProjects: ['ZEN'], ...hashes, supportingFiles: [path.join(root, 'absolute.md')] }),
    /supporting file must be a relative path/,
  );
  await assert.rejects(
    planMigration({ snapshot: value, mappedProjects: ['ZEN'], ...hashes, supportingFiles: ['../escape.md'] }),
    /supporting file escapes the migration root/,
  );
  await assert.rejects(
    planMigration({ snapshot: value, mappedProjects: ['ZEN'], ...hashes, supportingFiles: ['ZEN/../.git/config'] }),
    /escapes the migration root/,
  );
  await assert.rejects(
    planMigration({ snapshot: value, mappedProjects: ['ZEN'], ...hashes, includeLivingDocs: true, livingDir: 'Ghost', sourceRepo }),
    /Cannot scan living docs directory Ghost/,
  );

  const tolerant = await planMigration({
    snapshot: value,
    mappedProjects: ['ZEN'],
    ...hashes,
    includeLivingDocs: true,
    livingDir: 'Ghost',
    allowMissingLivingDocs: true,
    sourceRepo,
  });
  assert.deepEqual(tolerant.supportingFiles, []);
});

test('planMigration requires a source and parses it strictly', async t => {
  const root = await tempRoot(t, 'source');
  await assert.rejects(planMigration(), /Migration requires --source SOURCE_JSON/);

  const broken = path.join(root, 'broken.json');
  await fs.writeFile(broken, '{ not json');
  await assert.rejects(planMigration({ sourcePath: broken }), /Invalid JSON in /);

  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify(snapshot()));
  const plan = await planMigration({ source, mappedProjects: ['ZEN'], ...hashes });
  assert.equal(plan.source.path, path.resolve(source));
  assert.equal(plan.source.sha256, await sha256OfFile(source));
  assert.equal(plan.snapshot.tables['jira.issues'].length, 2);
  assert.equal(plan.policy.resolved.projects.ZEN.project, 'ZEN', 'a default mutable policy is generated');
});

test('prepareMigration leaves a forensic destination behind when the audit fails', async t => {
  const root = await tempRoot(t, 'audit');
  const destination = path.join(root, 'prepared');
  await assert.rejects(
    prepareMigration({
      snapshot: emptySnapshot(),
      mappedProjects: [],
      destination,
      snapshotEvidence: EVIDENCE,
      backupSha: BACKUP,
      dumpSha256: DUMP,
      writersFrozen: FREEZE_HASH,
    }),
    error => {
      assert.equal(error.code, 'AUDIT_FAILED');
      assert.ok(Array.isArray(error.audit.entityDifferences));
      assert.ok(error.audit.entityDifferences.some(item => item.table === 'jira.issues'), 'empty excluded tables must differ');
      assert.equal(error.destination, destination);
      return true;
    },
  );
  assert.equal(await fs.stat(path.join(destination, 'vault.config.json')).then(() => true, () => false), true);
});

test('prepareMigration refuses a missing supporting file unless missing living docs are allowed', async t => {
  const root = await tempRoot(t, 'supporting');
  const sourceRepo = path.join(root, 'repo');
  await fs.mkdir(sourceRepo, { recursive: true });
  const destination = path.join(root, 'prepared');

  await assert.rejects(
    prepareMigration({
      snapshot: snapshot(),
      policy,
      destination,
      sourceRepo,
      supportingFiles: ['Docs/Missing.md'],
      ...hashes,
      writersFrozen: FREEZE_HASH,
    }),
    /Cannot copy supporting file Docs\/Missing\.md/,
  );
  assert.equal(await fs.stat(destination).then(() => true, () => false), true, 'the partial destination is kept');

  const tolerant = await prepareMigration({
    snapshot: snapshot(),
    policy,
    destination: path.join(root, 'tolerant'),
    sourceRepo,
    supportingFiles: ['Docs/Missing.md'],
    allowMissingLivingDocs: true,
    ...hashes,
    writersFrozen: FREEZE_HASH,
  });
  assert.equal(tolerant.audit.ok, true);
  assert.equal(await fs.stat(path.join(root, 'tolerant', 'vault.config.json')).then(() => true, () => false), true);
});

test('runMigrationCli prints usage and refuses malformed input', async t => {
  const root = await tempRoot(t, 'cli');
  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify(snapshot()));
  const brokenPolicy = path.join(root, 'broken-policy.json');
  await fs.writeFile(brokenPolicy, '{ oops');

  const help = await runMigrationCli(['--help']);
  assert.equal(help.exitCode, 0);
  assert.match(help.payload.usage, /--source SOURCE_JSON/);
  assert.ok(help.payload.notes.length >= 3);

  const bare = await runMigrationCli([]);
  assert.equal(bare.exitCode, 0);
  assert.equal(bare.payload.defaultMode, 'dry-run');

  const missingSource = await runMigrationCli(['--destination', 'nowhere']);
  assert.equal(missingSource.exitCode, 1);
  assert.match(missingSource.payload.error, /Missing --source SOURCE_JSON/);

  const badPolicy = await runMigrationCli(['--source', source, '--policy', brokenPolicy]);
  assert.equal(badPolicy.exitCode, 1);
  assert.match(badPolicy.payload.error, /Invalid JSON in /);

  const badKey = await runMigrationCli(['--source', source, '--project', 'bad key']);
  assert.match(badKey.payload.error, /Invalid mapped project key: BAD KEY/);
  const unmapped = await runMigrationCli(['--source', source, '--project', 'XYZ']);
  assert.match(unmapped.payload.error, /No repository board path is configured for project XYZ/);
  const unknown = await runMigrationCli(['--source', source, '--project', 'ZM']);
  assert.match(unknown.payload.error, /Snapshot does not contain configured project ZM/);
});

test('runMigrationCli dry-run maps every flag in both spellings and stays read-only', async t => {
  const root = await tempRoot(t, 'cli-dry');
  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify(snapshot()));
  const sourceRepo = path.join(root, 'repo');
  await fs.mkdir(path.join(sourceRepo, 'Docs'), { recursive: true });
  await fs.writeFile(path.join(sourceRepo, 'Docs', 'Found.md'), '# Found\n');
  const destination = path.join(root, 'planned');

  const plan = await runMigrationCli([
    '--snapshot=' + source,
    '--project', 'ZEN',
    '--evidence', source,
    '--backup-sha256', BACKUP,
    '--dump-hash', DUMP,
    '--supporting-file', 'Docs/Roadmap.md',
    '--supporting-file', 'Docs/Other.md',
    '--include-living-docs',
    '--repo', sourceRepo,
    '--living-dir', 'Docs',
    '--living-dir', 'Ghost',
    '--allow-missing-living-docs',
    '--out', destination,
  ]);
  assert.equal(plan.exitCode, 0);
  assert.equal(plan.payload.mode, 'dry-run');
  assert.equal(Object.hasOwn(plan.payload, 'snapshot'), false);
  assert.equal(Object.hasOwn(plan.payload, 'policy'), false);
  assert.equal(plan.payload.destination, path.resolve(destination));
  assert.equal(plan.payload.readiness.ready, true);
  assert.deepEqual(plan.payload.supportingFiles, ['Docs/Roadmap.md', 'Docs/Other.md', 'Docs/Found.md']);
  assert.equal(plan.payload.mappedIssueCount, 1);
  assert.equal(await fs.stat(destination).then(() => true, error => error.code !== 'ENOENT'), false, 'a dry run creates nothing');
});

test('runMigrationCli prepares a frozen destination end to end', async t => {
  const root = await tempRoot(t, 'cli-prepare');
  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify(snapshot()));
  const policyFile = path.join(root, 'policy.json');
  await fs.writeFile(policyFile, JSON.stringify(policy));
  const destination = path.join(root, 'prepared');

  const result = await runMigrationCli([
    '--source', source,
    '--config', policyFile,
    '--prepare',
    '--destination', destination,
    '--snapshot-evidence', source,
    '--backup-sha', BACKUP,
    '--dump-sha256', DUMP,
    '--writers-frozen',
  ]);
  assert.equal(result.exitCode, 0);
  assert.equal(result.payload.mode, 'prepare');
  assert.equal(Object.hasOwn(result.payload, 'snapshot'), false);
  assert.equal(result.payload.audit.ok, true);
  assert.equal(result.payload.config.writes.enabled, false);
  assert.equal(result.payload.config.readiness.ready, true);
  assert.equal(result.payload.config.readiness.productionReady, false, 'the flag is only an attestation');
  await fs.access(path.join(destination, 'Zenith', 'Docs', 'Board', 'tickets', 'ZEN-1.md'));
  await fs.access(path.join(destination, 'archive', 'legacy', 'WEB', 'tickets', 'WEB-1.md'));
  await fs.access(path.join(destination, 'metadata.md'));

  const again = await runMigrationCli([
    '--source', source,
    '--config', policyFile,
    '--prepare',
    '--destination', destination,
    '--snapshot-evidence', source,
    '--backup-sha', BACKUP,
    '--dump-sha256', DUMP,
    '--writers-frozen',
  ]);
  assert.equal(again.exitCode, 1);
  assert.match(again.payload.error, /EEXIST|already exists/);
});

test('runMigrationCli refuses an unready migration and honours --allow-unfrozen only for the freeze reason', async t => {
  const root = await tempRoot(t, 'cli-ready');
  const source = path.join(root, 'snapshot.json');
  await fs.writeFile(source, JSON.stringify(snapshot()));
  const missingDestination = path.join(root, 'never-created');

  const notReady = await runMigrationCli([
    '--source', source,
    '--project', 'ZEN',
    '--prepare',
    '--destination', missingDestination,
    '--snapshot-evidence', source,
    '--backup-sha', 'nope',
    '--dump-sha256', DUMP,
    '--writers-frozen',
  ]);
  assert.equal(notReady.exitCode, 1);
  assert.equal(notReady.payload.code, 'NOT_READY');
  assert.match(notReady.payload.readiness.reasons.join(' '), /Backup SHA/);
  assert.equal(await fs.stat(missingDestination).then(() => true, error => error.code !== 'ENOENT'), false);

  // A non-freeze reason is not forgiven by --allow-unfrozen.
  const stillBlocked = await runMigrationCli([
    '--source', source,
    '--project', 'ZEN',
    '--prepare',
    '--destination', missingDestination,
    '--snapshot-evidence', source,
    '--backup-sha', 'nope',
    '--dump-sha256', DUMP,
    '--allow-unfrozen',
  ]);
  assert.equal(stillBlocked.exitCode, 1);
  assert.equal(stillBlocked.payload.code, 'NOT_READY');

  // Only the missing freeze evidence is forgiven, and the migration proceeds.
  const unfrozen = await runMigrationCli([
    '--source', source,
    '--project', 'ZEN',
    '--prepare',
    '--output', path.join(root, 'unfrozen'),
    '--snapshot-evidence', source,
    '--backup-sha', BACKUP,
    '--dump-sha256', DUMP,
    '--require-writers-frozen',
    '--allow-unfrozen',
  ]);
  assert.equal(unfrozen.exitCode, 0);
  assert.equal(unfrozen.payload.audit.ok, true);
  await fs.access(path.join(root, 'unfrozen', 'vault.config.json'));
});

test('runMigrationCli reports an audit failure and leaves the destination behind', async t => {
  const root = await tempRoot(t, 'cli-audit');
  const source = path.join(root, 'empty.json');
  await fs.writeFile(source, JSON.stringify(emptySnapshot()));
  const destination = path.join(root, 'prepared');

  const result = await runMigrationCli([
    '--source', source,
    '--project', 'ZEN',
    '--prepare',
    '--destination', destination,
    '--evidence', source,
    '--backup-sha', BACKUP,
    '--dump-sha256', DUMP,
    '--writers-frozen',
  ]);
  assert.equal(result.exitCode, 1);
  assert.equal(result.payload.code, 'AUDIT_FAILED');
  assert.ok(result.payload.audit.entityDifferences.some(item => item.table === 'jira.issues'));
  assert.equal(await fs.stat(destination).then(() => true, () => false), true, 'the failed destination is kept for forensics');
});
