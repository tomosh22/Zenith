import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';
import { ProjectApiError, issueDetail, issueDetailAsync, projectDetail, projectList } from '../server/project-api.mjs';

let root;

const ZEN_DONE = 'dxhlckk5kgxv4mgp4e8lcu7s';
const ZEN_IN_PROGRESS = 'rdei4coufefotrv2feispqhx';
const ZEN_TODO = 'q3spz9u36h2bf6qa942uqfz1';
const ZEN_1_ID = 'npl4x2loj6gm0dzcuzj9kgm1';
const ZEN_5_ID = 'ge8d8l43pb559wic2uthq4wv';
const REOPEN_TRANSITION = '879c2a51-352d-46aa-860e-06802f877625';
// ZEN-1's frozen history row is the assignee unassignment by the reporter.
const HISTORIAN_ID = 'n9xhr52s8n49fa832egoqf4z';

const CONFIG_FILES = {
  'ZEN/config/projects.md': `---
kind: jira.projects
rows:
  - id: cfg-project-1
    key: ZEN
    name: Engine
  - id: cfg-project-2
    key: AUX
    name: Auxiliary
---
`,
  'ZEN/config/jira.custom_fields.md': `---
kind: customFields
rows:
  - id: cfg-field-1
    project: ZEN
    key: cfg-tier
    name: Cfg tier
    type: SELECT
    description: null
    position: 0
    options:
      - Gold
      - Silver
  - id: cfg-struct
    project: ZEN
    key: cfg-struct
    name: Cfg struct
    type: SELECT
    description: null
    position: 1
    options:
      - id: opt-alpha
        value: Alpha
        position: 0
      - id: opt-beta
        value: Beta
        position: 1
  - id: cfg-owned
    project_id: cfg-project-1
    key: cfg-owned
    name: Cfg owned
    type: TEXT
    description: null
    position: 2
---
`,
  'ZEN/config/fieldConfiguration.md': `---
kind: fieldConfiguration
rows:
  - issueType: BUG
    customFieldId: cfg-field-1
    requirement: REQUIRED
---
`,
  'ZEN/config/screens.md': `---
kind: screens
rows:
  - id: cfg-screen-1
    project: ZEN
    name: Cfg screen
    description: null
    position: 0
    fields:
      - builtinField: assignee
        customFieldId: null
        position: 0
      - builtinField: null
        customFieldId: cfg-field-1
---
`,
  'ZEN/config/board.md': `---
kind: board
name: Cfg board
type: KANBAN
columns: []
quickFilters: []
---
`,
  'ZEN/config/versions.md': `---
kind: versions
rows:
  - id: cfg-ver-alpha
    project: ZEN
    name: Alpha
    status: UNRELEASED
    position: 0
    description: First config release
    release_date: null
---
`,
};

/** Rewrite the first frontmatter occurrence of a `field: value` line. */
async function patchFixtureField(relative, from, to) {
  const file = path.join(root, relative);
  const text = await readFile(file, 'utf8');
  await writeFile(file, text.replace(from, to));
}

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-project-read-config-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  // Remove the relational metadata export so every table lives in a
  // per-project config record — the hand-authored vault shape.
  await rm(path.join(root, 'metadata.md'));
  for (const [relative, contents] of Object.entries(CONFIG_FILES)) {
    await mkdir(path.dirname(path.join(root, relative)), { recursive: true });
    await writeFile(path.join(root, relative), contents);
  }
  // ZEN-5 joins the config version so the release aggregates have a member.
  await patchFixtureField('ZEN/tickets/ZEN-5.md', 'fixVersions: []', 'fixVersions:\n  - Alpha');
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  // A project the vault config names without any board root or catalog row.
  config.projects = { CFGONLY: { name: 'Config named', description: 'Described' } };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Project read config test']);
  await git(root, ['config', 'user.email', 'project-read-config@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
});

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

const assertThrowsCode = (call, code) => {
  try {
    call();
  } catch (error) {
    expect(error).toBeInstanceOf(ProjectApiError);
    expect(error.code).toBe(code);
    return;
  }
  throw new Error(`Expected the call to throw ${code}`);
};

test('projectList resolves board roots, config catalog rows and vault-config projects', async () => {
  const index = await loadVault(root);
  const list = projectList(index);

  const zen = list.projects.find(project => project.key === 'ZEN');
  expect(zen).toMatchObject({ key: 'ZEN', name: 'Engine', readOnly: false, path: 'ZEN' });
  // AUX exists only as a config catalog row: listed, but read-only because it
  // has no board root to write into.
  expect(list.projects.find(project => project.key === 'AUX'))
    .toMatchObject({ key: 'AUX', name: 'Auxiliary', readOnly: true, path: null, description: null });
  // A project named only by the vault config joins through its projects map.
  expect(list.projects.find(project => project.key === 'CFGONLY'))
    .toMatchObject({ key: 'CFGONLY', name: 'Config named', description: 'Described', readOnly: true, path: null });
  // A board root without any config row falls back to a derived name.
  expect(list.projects.find(project => project.key === 'WEB'))
    .toMatchObject({ key: 'WEB', name: 'Project WEB', readOnly: false });
  expect(list.projects).toEqual([...list.projects].sort((left, right) => left.key.localeCompare(right.key)));
  // The list-level revision map covers the vault configuration record.
  expect(Object.keys(list.revisions)).toEqual(['vault.config.json']);
  expect(list.revisions['vault.config.json']).toMatchObject({ hash: index.configRecord.hash });
});

test('projectDetail projects the per-project config catalogs into the read model', async () => {
  const index = await loadVault(root);
  const detail = projectDetail(index, 'ZEN');

  expect(detail).toMatchObject({ key: 'ZEN', name: 'Engine', readOnly: false });
  expect(detail.actions.create).toEqual({ action: 'issue.create' });
  // Statuses come from the per-project workflow config record, ordered by position.
  expect(detail.statuses.map(status => `${status.position}:${status.name}`)).toEqual([
    '0:To Do', '1:In Progress', '2:In Review', '3:Done', '4:Blocked',
  ]);
  const reopen = detail.workflow.transitions.find(edge => edge.id === REOPEN_TRANSITION);
  expect(reopen).toMatchObject({
    name: 'Reopen',
    fromStatusId: ZEN_DONE,
    toStatusId: ZEN_IN_PROGRESS,
    screenId: null,
    rules: [],
  });
  expect(detail.workflow).toMatchObject({ path: 'ZEN/workflow.md', hash: index.getFile('ZEN/workflow.md').hash });
  expect(detail.workflow.source.kind).toBe('workflow');

  // Config catalog rows surface with embedded options, fields and owners.
  expect(detail.customFields.find(field => field.id === 'cfg-field-1'))
    .toMatchObject({ key: 'cfg-tier', type: 'SELECT', options: ['Gold', 'Silver'] });
  expect(detail.customFields.find(field => field.key === 'cfg-struct').options).toEqual(['Alpha', 'Beta']);
  // A row owned through project_id resolves against the config project table.
  expect(detail.customFields.find(field => field.key === 'cfg-owned').id).toBe('cfg-owned');
  const screen = detail.screens.find(row => row.id === 'cfg-screen-1');
  expect(screen.fields).toEqual([
    expect.objectContaining({ builtinField: 'assignee', customFieldId: null, position: 0 }),
    // A field without a position falls back to its list index.
    expect.objectContaining({ builtinField: null, customFieldId: 'cfg-field-1', position: 1 }),
  ]);
  expect(detail.fieldConfiguration).toEqual([
    expect.objectContaining({ issueType: 'BUG', customFieldId: 'cfg-field-1', requirement: 'REQUIRED' }),
  ]);
  // The version aggregates join canonical fixVersions against the config row.
  // Version scoping is strict: a row that carries a project reference must
  // belong to the owner, so the fixture's other roots' releases (ZM/FD rows
  // with unresolvable project ids) do not surface here.
  const alpha = detail.versions.find(version => version.id === 'cfg-ver-alpha');
  expect(alpha).toMatchObject({
    name: 'Alpha',
    status: 'UNRELEASED',
    issues: 1,
    done: 1,
    path: 'ZEN/config/versions.md',
  });
  expect(detail.versions.map(version => version.id)).toEqual(['cfg-ver-alpha']);
  expect(detail.labels.map(label => label.name)).toEqual(expect.arrayContaining(['needs-human', 'needs-gpu', 'flaky']));
  expect(detail.categories).toEqual([]);

  // The revision map joins every record the projection consulted.
  expect(detail.revisions['vault.config.json']).toMatchObject({ hash: index.configRecord.hash });
  expect(detail.revisions['ZEN/tickets/ZEN-5.md']).toMatchObject({ hash: index.getTicket('ZEN-5').hash });
  expect(Object.keys(detail.revisions)).toEqual(expect.arrayContaining([
    'ZEN/config/projects.md',
    'ZEN/config/jira.custom_fields.md',
    'ZEN/config/screens.md',
    'ZEN/config/fieldConfiguration.md',
    'ZEN/config/versions.md',
    'ZEN/config/board.md',
    'ZEN/workflow.md',
    'ZEN/labels.md',
  ]));
});

test('global-scan version rows are scoped to the owning project', async () => {
  const index = await loadVault(root);
  // Rows reaching versionRows through the relational scan (no owning config
  // record) follow boardModel's scoping rule: an unscoped row is global, a
  // row whose project reference resolves to the owner stays, and a row that
  // resolves elsewhere — or nowhere — is excluded rather than tolerated.
  index.tables.set('jira.versions', [
    { id: 'ver-open', name: 'Unscoped', status: 'UNRELEASED', position: 10 },
    { id: 'ver-zen', name: 'Zen owned', project_id: 'cfg-project-1', status: 'RELEASED', position: 11 },
    { id: 'ver-ghost', name: 'Ghost ref', project_id: 'unresolvable-id', status: 'UNRELEASED', position: 12 },
    { id: 'ver-aux', name: 'Aux owned', project: 'AUX', status: 'UNRELEASED', position: 13 },
  ]);
  // ZEN keeps its own config row, the unscoped row and the row resolving to
  // it through the project table; the AUX-owned and unresolvable rows drop.
  expect(projectDetail(index, 'ZEN').versions.map(version => version.id))
    .toEqual(['cfg-ver-alpha', 'ver-open', 'ver-zen']);
  // AUX sees the mirror image: the unscoped row and its own row stay, while
  // the ZEN catalog row and the unresolvable reference are both excluded.
  expect(projectDetail(index, 'AUX').versions.map(version => version.id))
    .toEqual(['ver-open', 'ver-aux']);
});

test('board settings fall back to a synthetic board and honour relational overrides', async () => {
  const index = await loadVault(root);
  // No jira.boards rows exist in config mode — the read model synthesizes one
  // column per workflow status.
  const detail = projectDetail(index, 'ZEN');
  expect(detail.board.boards).toHaveLength(1);
  expect(detail.board.boards[0]).toMatchObject({ id: 'zen-board', name: 'Board', type: 'KANBAN' });
  expect(detail.board.boards[0].columns).toEqual(detail.statuses.map(status => ({
    id: status.id,
    name: status.name,
    position: status.position,
    statusIds: [status.id],
  })));
  expect(detail.board.columns).toEqual(detail.board.boards[0].columns);
  expect(detail.board.quickFilters).toEqual([]);

  // Relational board rows take over when a table supplies them. Ownership is
  // resolved through the project table id, the project key, or the absence of
  // any project scoping; everything else is filtered out.
  index.tables.set('jira.boards', [
    { id: 'board-zen', name: 'Zen board', type: 'SCRUM', project_id: 'cfg-project-1' },
    // Id-less on purpose: it survives the filter through its project key and
    // must surface the derived id its columns resolve against.
    { project: 'ZEN', project_id: 'unmapped-id' },
    { name: 'Anonymous board' },
    { id: 'board-foreign', project_id: 'other-project' },
  ]);
  index.tables.set('jira.board_columns', [
    { id: 'col-later', board_id: 'board-zen', name: 'Later', position: 2 },
    { id: 'col-now', board_id: 'board-zen', name: 'Now', position: 1, wip_limit: 3 },
  ]);
  index.tables.set('jira.board_column_statuses', [
    { column_id: 'col-now', status_id: ZEN_TODO, position: 1 },
    { column_id: 'col-now', status_id: ZEN_IN_PROGRESS, position: 0 },
  ]);
  index.tables.set('jira.board_quick_filters', [
    { id: 'qf-zen', project: 'ZEN', name: 'Mine' },
    { id: 'qf-open', name: 'Open' },
    { id: 'qf-foreign', project: 'WEB', name: 'Foreign' },
  ]);
  const board = projectDetail(index, 'ZEN').board;
  expect(board.boards.map(row => row.name)).toEqual(['Zen board', 'Board', 'Anonymous board']);
  const zenBoard = board.boards[0];
  expect(zenBoard.columns.map(column => column.id)).toEqual(['col-now', 'col-later']);
  expect(zenBoard.columns.find(column => column.id === 'col-now'))
    .toMatchObject({ name: 'Now', wipLimit: 3, statusIds: [ZEN_IN_PROGRESS, ZEN_TODO] });
  expect(zenBoard.columns.find(column => column.id === 'col-later')).toMatchObject({ wipLimit: null, statusIds: [] });
  // Boards without an id still resolve their columns through a derived id,
  // and the derived id is stable: owner key + 1-based position.
  expect(board.boards[1]).toMatchObject({ id: 'zen-board-2', name: 'Board', type: 'KANBAN' });
  // Quick filters survive when they belong to the project or to no project.
  expect(board.quickFilters.map(row => row.id)).toEqual(['qf-zen', 'qf-open']);
  // WEB inherits the unscoped board and unscoped quick filter, plus its own.
  const web = projectDetail(index, 'WEB');
  expect(web.board.boards.map(row => row.name)).toEqual(['Anonymous board']);
  expect(web.board.boards[0].columns).toEqual([]);
  expect(web.board.quickFilters.map(row => row.id)).toEqual(['qf-open', 'qf-foreign']);
});

test('workflow rules join through transitions even when the workflow lives in a config record', async () => {
  const index = await loadVault(root);
  index.tables.set('jira.workflow_transition_rules', [
    { id: 'rule-reopen', transition_id: REOPEN_TRANSITION, kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} },
    { id: 'rule-ghost', transition_id: 'ghost-transition', kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: {} },
  ]);
  const workflow = projectDetail(index, 'ZEN').workflow;
  // A rule whose transition does not exist is dropped.
  expect(workflow.rules.some(rule => rule.id === 'rule-ghost')).toBe(false);
  expect(workflow.rules).toEqual([expect.objectContaining({ id: 'rule-reopen', type: 'COMMENT_REQUIRED' })]);
  // Surviving rules merge into the embedded rule list of their transition.
  const reopen = workflow.transitions.find(edge => edge.id === REOPEN_TRANSITION);
  expect(reopen.rules).toEqual([expect.objectContaining({ id: 'rule-reopen' })]);
});

test('users join from the relational table with identity fallbacks', async () => {
  const index = await loadVault(root);
  index.tables.set('public.users', [
    { id: 'user-zoe', name: 'Zoe', email: 'zoe@saas.invalid' },
    { user_id: 'user-al', display_name: 'Al', mail: 'al@saas.invalid' },
    { id: 'user-zoe', name: 'Zoe duplicate' },
    { note: 'neither identity nor name' },
  ]);
  const users = projectDetail(index, 'ZEN').users;
  expect(users.map(user => user.name)).toEqual(['Al', 'Zoe']);
  expect(users.find(user => user.name === 'Al')).toMatchObject({ id: 'user-al', email: 'al@saas.invalid' });
  expect(users.find(user => user.name === 'Zoe')).toMatchObject({ id: 'user-zoe', email: 'zoe@saas.invalid' });
});

test('serializeTicket projects tickets with stable fallbacks', async () => {
  const index = await loadVault(root);
  // A ticket record without a key falls back to its file basename for both
  // identity and title, and carries no source id or number.
  const orphan = {
    relativePath: 'ZEN/tickets/orphan-ticket.md',
    project: 'ZEN',
    metadata: { status: 'Blocked' },
    fields: {},
    bodyText: 'Body text',
    hash: 'hash-orphan',
    size: 11,
    stat: { mtimeMs: 456 },
    source: {},
  };
  index.tickets.set('orphan', orphan);
  try {
    const tickets = projectDetail(index, 'ZEN').tickets;
    const exported = tickets.find(ticket => ticket.key === 'ZEN-1');
    expect(exported).toMatchObject({
      path: 'ZEN/tickets/ZEN-1.md',
      project: 'ZEN',
      title: 'Engine gates & pinned baselines',
      status: 'To Do',
      type: 'EPIC',
      priority: 'HIGH',
      complexity: 'MODERATE',
      risk: 'MEDIUM',
      category: 'Engine',
      labels: ['deferred'],
    });
    expect(exported.id).toBe(ZEN_1_ID);
    expect(exported.number).toBe(1);
    expect(String(exported.updatedAt)).toMatch(/^2026-08-22/);

    const fallback = tickets.find(ticket => ticket.key === 'orphan-ticket');
    expect(fallback).toMatchObject({
      path: 'ZEN/tickets/orphan-ticket.md',
      project: 'ZEN',
      key: 'orphan-ticket',
      title: 'orphan-ticket',
      status: 'Blocked',
      body: 'Body text',
      size: 11,
      mtimeMs: 456,
    });
    expect(fallback.id).toBeUndefined();
    expect(fallback.number).toBeUndefined();
    expect(fallback.updatedAt).toBeNull();
    expect(fallback.statusCategory).toBeNull();
  } finally {
    index.tickets.delete('orphan');
  }
});

test('issueDetail resolves config-mode issues with family, links, history and revisions', async () => {
  const index = await loadVault(root);
  index.tables.set('public.users', [{ id: HISTORIAN_ID, name: 'Historian', email: 'historian@saas.invalid' }]);
  index.tables.set('jira.issue_links', [
    { id: 'link-out', source_issue_id: ZEN_1_ID, target_issue_id: ZEN_5_ID, type: 'BLOCKS' },
    { id: 'link-in', source_issue_id: ZEN_5_ID, target_issue_id: ZEN_1_ID, type: 'RELATES_TO' },
    { id: 'link-ghost', source_issue_id: 'ghost-issue', target_issue_id: ZEN_5_ID, type: 'BLOCKS' },
  ]);
  // A synthetic child exercises the family projection and canonical links.
  const child = {
    relativePath: 'ZEN/tickets/ZEN-90.md',
    project: 'ZEN',
    metadata: {
      key: 'ZEN-90',
      title: 'Injected child',
      type: 'TASK',
      status: 'To Do',
      parent: 'ZEN-1',
      links: [{ key: 'ZEN-5', type: 'BLOCKS' }],
    },
    fields: { key: 'ZEN-90' },
    bodyText: '',
    hash: 'hash-z90',
    size: 24,
    source: {},
  };
  index.tickets.set('ZEN-90', child);
  try {
    // Issue keys resolve case-insensitively.
    const detail = issueDetail(index, 'zen-1');
    expect(detail.issue).toMatchObject({
      path: 'ZEN/tickets/ZEN-1.md',
      project: 'ZEN',
      key: 'ZEN-1',
      title: 'Engine gates & pinned baselines',
      type: 'EPIC',
      status: 'To Do',
    });
    expect(detail.issue.id).toBe(ZEN_1_ID);
    expect(detail.issue.number).toBe(1);
    expect(detail.revision).toMatchObject({ hash: index.getTicket('ZEN-1').hash });
    expect(detail.revisions['ZEN/tickets/ZEN-1.md']).toMatchObject({ hash: index.getTicket('ZEN-1').hash });
    expect(detail.project).toMatchObject({ key: 'ZEN', name: 'Engine' });
    // Frozen history resolves its actor through the users table.
    expect(detail.history).toEqual([expect.objectContaining({
      issueId: ZEN_1_ID,
      field: 'assigneeId',
      fromValue: HISTORIAN_ID,
      toValue: null,
      actorName: 'Historian',
      actorEmail: 'historian@saas.invalid',
    })]);
    expect(detail.links.outgoing).toEqual([expect.objectContaining({ key: 'ZEN-5', type: 'BLOCKS', label: 'blocks' })]);
    expect(detail.links.incoming).toEqual([expect.objectContaining({ key: 'ZEN-5', type: 'RELATES_TO', label: 'relates to' })]);
    expect(detail.comments).toEqual([]);
    expect(detail.family.children).toEqual([expect.objectContaining({ id: 'ZEN-90', key: 'ZEN-90', title: 'Injected child' })]);

    const childDetail = issueDetail(index, 'ZEN-90');
    expect(childDetail.family.parent).toMatchObject({ id: ZEN_1_ID, key: 'ZEN-1' });
    expect(childDetail.links.outgoing).toEqual([expect.objectContaining({ key: 'ZEN-5', label: 'blocks' })]);
    // Without a source row the number falls back to the key's numeric suffix.
    expect(childDetail.issue.number).toBe(90);
    expect(childDetail.issue.id).toBeUndefined();
  } finally {
    index.tickets.delete('ZEN-90');
  }
});

test('unknown projects and issues throw NOT_FOUND', async () => {
  const index = await loadVault(root);
  assertThrowsCode(() => projectDetail(index, 'GHOST'), 'NOT_FOUND');
  assertThrowsCode(() => issueDetail(index, 'GHOST-99'), 'NOT_FOUND');
});

test('a config-only project without a board root projects empty collections', async () => {
  const index = await loadVault(root);
  const detail = projectDetail(index, 'AUX');
  expect(detail).toMatchObject({ key: 'AUX', name: 'Auxiliary', readOnly: true, path: null });
  expect(detail.tickets).toEqual([]);
  expect(detail.statuses).toEqual([]);
  expect(detail.workflow).toMatchObject({ path: null, hash: null, statuses: [], transitions: [] });
  expect(detail.sprints).toEqual([]);
  // AUX does not own the ZEN config version row, and foreign rows with
  // unresolvable project references are excluded rather than tolerated as
  // unowned.
  expect(detail.versions).toEqual([]);
  expect(detail.customFields).toEqual([]);
  expect(detail.screens).toEqual([]);
  expect(detail.labels).toEqual([]);
  expect(detail.users).toEqual([]);
  expect(detail.board).toEqual({
    boards: [expect.objectContaining({ id: 'aux-board', name: 'Board', type: 'KANBAN', columns: [] })],
    columns: [],
    quickFilters: [],
  });
});

test('issueDetailAsync enriches writable roots and passes bare callers through', async () => {
  const index = await loadVault(root);
  const detail = issueDetail(index, 'ZEN-1');

  // Without a root there is nothing to enrich — the frozen history wins.
  const bare = await issueDetailAsync(index, 'ZEN-1', { root: null });
  expect(bare.history).toEqual(detail.history);

  // A root that is not a repository cannot resolve HEAD either.
  const plainDir = await mkdtemp(path.join(os.tmpdir(), 'vault-no-repo-'));
  try {
    const untouched = await issueDetailAsync(index, 'ZEN-1', { root: plainDir });
    expect(untouched.history).toEqual(detail.history);
  } finally {
    await rm(plainDir, { recursive: true, force: true });
  }

  // The writable committed root resolves HEAD and caches the history walk.
  const enriched = await issueDetailAsync(index, 'ZEN-1');
  expect(enriched.history.length).toBeGreaterThanOrEqual(detail.history.length);
  const cached = await issueDetailAsync(index, 'ZEN-1');
  expect(cached.history).toEqual(enriched.history);
});
