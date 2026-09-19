import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchProjectAction } from '../server/project-actions.mjs';
import { allTicketRecords, canonicalOf } from '../cli/compat.mjs';
import { expectedRevision } from '../cli/write.mjs';

let root;

const ZM_ACTIVE_SPRINT = 'zm-g59n0dtrphhiop35dtrj6kcg';
const ZM_S10_SPRINT = 'zm-f05lobp8sunh1l02szovyre6';
const ZM_CROSS_SPRINT = 'zm-esni34xjwv5an13cmzphpvwm';
const ZM_S11_SPRINT = 'zm-sqn8cg2lmewe64lvzsfejb6n';
const ZM_SPRINTS_PATH = 'ZM/sprints';
const ZEN_VERSIONS_PATH = 'ZEN/versions.md';

const CONFIG_FILES = {
  'ZEN/config/projects.md': `---
kind: jira.projects
rows:
  - id: cfg-project-1
    key: ZEN
    name: Engine
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
  - id: cfg-area
    project: ZEN
    key: cfg-area
    name: Cfg area
    type: TEXT
    description: null
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
---
`,
  'ZEN/config/board.md': `---
kind: board
name: Cfg board
type: KANBAN
columns:
  - id: cfg-col-1
    name: To Do
    position: 0
    statusIds: []
  - id: cfg-col-2
    name: Done
    position: 1
    statusIds: []
quickFilters: []
---
`,
};

const ZEN_VERSIONS = `---
kind: versions
rows:
  - id: zen-version-alpha
    name: Alpha
    status: UNRELEASED
    position: 0
    description: null
    release_date: null
---
`;

async function revisions(...paths) {
  return Object.fromEntries(await Promise.all(paths.map(async relative => [relative, await readRevision(root, relative)])));
}

function ticketRevisions(index, tickets) {
  return Object.fromEntries(tickets.map(ticket => [ticket.relativePath, expectedRevision(ticket)]));
}

function ticketRecord(index, key) {
  const record = index.getTicket(key);
  expect(record, `fixture ticket ${key}`).toBeTruthy();
  return record;
}

/** Rewrite the first frontmatter occurrence of a `field: value` line. */
async function patchFixtureField(relative, field, from, to) {
  const file = path.join(root, relative);
  const text = await readFile(file, 'utf8');
  await writeFile(file, text.replace(`${field}: ${from}`, `${field}: ${to}`));
}

async function setupFixture({ legacyBoards = false } = {}) {
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  if (legacyBoards) {
    config.boards = config.boardRoots;
    delete config.boardRoots;
  }
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Project config test']);
  await git(root, ['config', 'user.email', 'project-config@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
}

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-project-config-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  // Remove the relational metadata export so every table lives in a
  // per-project config record — the hand-authored vault shape.
  await rm(path.join(root, 'metadata.md'));
  for (const [relative, contents] of Object.entries(CONFIG_FILES)) {
    await mkdirAndWrite(relative, contents);
  }
  await writeFile(path.join(root, ZEN_VERSIONS_PATH), ZEN_VERSIONS);
  await setupFixture();
});

async function mkdirAndWrite(relative, contents) {
  await mkdir(path.dirname(path.join(root, relative)), { recursive: true });
  await writeFile(path.join(root, relative), contents);
}

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

const rejectsWith = (promise, code) => expect(promise).rejects.toMatchObject({ code });

test('issue.create honours config custom fields, requirements and versions', async () => {
  const index = await loadVault(root);
  const base = { action: 'issue.create', project: 'ZEN' };

  // The config fieldConfiguration makes cfg-tier REQUIRED for BUGs only.
  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { title: 'Bug without tier', type: 'BUG' },
    operationId: 'cfg-create-unrequired',
  }), 'INVALID');
  await expect(dispatchProjectAction(index, {
    ...base,
    data: { title: 'Bug without tier', type: 'BUG' },
    operationId: 'cfg-create-unrequired-2',
  })).rejects.toThrow(/is required for BUG/);

  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { title: 'Bad option', type: 'TASK', customFieldValues: { 'cfg-tier': ['Platinum'] } },
    operationId: 'cfg-create-bad-option',
  }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { title: 'Ghost field', type: 'TASK', customFieldValues: { 'ghost-field': ['x'] } },
    operationId: 'cfg-create-ghost-field',
  }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { title: 'Ghost version', type: 'TASK', fixVersions: ['Ghost'] },
    operationId: 'cfg-create-ghost-version',
  }), 'INVALID');

  // The field can also be addressed by its key, and cfg-owned resolves its
  // project through project_id rather than a project key.
  const created = await dispatchProjectAction(index, {
    ...base,
    data: {
      title: 'Created with config fields',
      type: 'TASK',
      customFieldValues: { 'cfg-tier': ['Gold'], 'cfg-area': ['Core'], 'cfg-owned': ['Owner'] },
      fixVersions: ['Alpha'],
    },
    operationId: 'cfg-create-happy',
  });
  expect(created.result).toMatchObject({ key: 'ZEN-23', created: true });
  const after = await loadVault(root);
  const ticket = after.getTicket('ZEN-23');
  expect(ticket.metadata.customFieldValues).toEqual({ 'cfg-field-1': ['Gold'], 'cfg-area': ['Core'], 'cfg-owned': ['Owner'] });
  expect(ticket.metadata.fixVersions).toEqual(['Alpha']);
});

test('issue.update walks config transitions and clears the claim leaving In Progress', async () => {
  let index = await loadVault(root);
  const issue = index.getTicket('ZEN-8');
  const start = await dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-8',
    data: { status: 'In Progress', assignee: 'human@example.invalid', customFieldValues: { 'cfg-area': ['Core'] } },
    revisions: { [issue.relativePath]: expectedRevision(issue) },
    operationId: 'cfg-update-start-progress',
  });
  expect(start.result).toMatchObject({ key: 'ZEN-8', updated: true });

  index = await loadVault(root);
  const inProgress = index.getTicket('ZEN-8');
  expect(inProgress.metadata.assignee).toBe('human@example.invalid');
  const stopped = await dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-8',
    data: { status: 'To Do' },
    revisions: { [inProgress.relativePath]: expectedRevision(inProgress) },
    operationId: 'cfg-update-stop-progress',
  });
  expect(stopped.result.updated).toBe(true);
  const after = await loadVault(root);
  const stoppedTicket = after.getTicket('ZEN-8');
  expect(stoppedTicket.metadata.assignee).toBeNull();
  expect(stoppedTicket.metadata.claimAttempt).toBeNull();
  expect(stoppedTicket.metadata.customFieldValues).toEqual({ 'cfg-area': ['Core'] }, 'untouched custom fields survive the patch');
});

test('sprint actions walk the ZM lifecycle in config mode', async () => {
  // S10 becomes the active-but-complete sprint: every member is Done.
  for (const key of ['ZM-37', 'ZM-38', 'ZM-39']) {
    await patchFixtureField(`ZM/tickets/${key}.md`, 'status', 'To Do', 'Done');
  }
  await patchFixtureField(`${ZM_SPRINTS_PATH}/${ZM_S10_SPRINT}.md`, 'status', 'PLANNED', 'ACTIVE');
  let index = await loadVault(root);
  const sprintPath = `${ZM_SPRINTS_PATH}/${ZM_S10_SPRINT}.md`;

  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
  }), 'INVALID');
  await expect(dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
  })).rejects.toThrow(/already active/);

  const completed = await dispatchProjectAction(index, {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
    operationId: 'cfg-sprint-complete-s10',
  });
  expect(completed.result).toMatchObject({ id: ZM_S10_SPRINT, status: 'COMPLETED', movedToBacklog: 0, movedToSprint: null });

  await expect(dispatchProjectAction(await loadVault(root), {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
  })).rejects.toThrow(/already completed/);
  await rejectsWith(dispatchProjectAction(await loadVault(root), {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
  }), 'INVALID');

  const deleted = await dispatchProjectAction(await loadVault(root), {
    action: 'sprint.delete',
    project: 'ZM',
    data: { id: ZM_S10_SPRINT },
    revisions: await revisions(sprintPath),
    operationId: 'cfg-sprint-delete-s10',
  });
  expect(deleted.result).toMatchObject({ id: ZM_S10_SPRINT, deleted: true });
  index = await loadVault(root);
  expect(index.getTicket('ZM-37').metadata.sprint).toBeNull();

  // The active sprint still has unfinished issues: a destination is required,
  // and null means the backlog.
  const activeSprintPath = `${ZM_SPRINTS_PATH}/${ZM_ACTIVE_SPRINT}.md`;
  const members = allTicketRecords(index).filter(ticket => String(ticket.project).toUpperCase() === 'ZM'
    && canonicalOf(ticket).sprint === ZM_ACTIVE_SPRINT && canonicalOf(ticket).status !== 'Done');
  expect(members.length).toBeGreaterThan(0);
  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_ACTIVE_SPRINT },
    revisions: { ...(await revisions(activeSprintPath)), ...ticketRevisions(index, members) },
  }), 'INVALID');

  const toBacklog = await dispatchProjectAction(index, {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_ACTIVE_SPRINT, destinationSprintId: null },
    revisions: { ...(await revisions(activeSprintPath)), ...ticketRevisions(index, members) },
    operationId: 'cfg-sprint-complete-backlog',
  });
  expect(toBacklog.result).toMatchObject({ id: ZM_ACTIVE_SPRINT, status: 'COMPLETED', movedToBacklog: members.length, movedToSprint: null });
  index = await loadVault(root);
  expect(index.getTicket('ZM-24').metadata.sprint).toBeNull();

  // A sprint with no unfinished issues validates a provided destination
  // against the planned catalogue before completing.
  for (const [key, from] of [
    ['ZM-47', 'To Do'], ['ZM-48', 'To Do'], ['ZM-51', 'To Do'], ['ZM-52', 'To Do'],
    ['ZM-56', 'Blocked'], ['ZM-59', 'To Do'], ['ZM-60', 'To Do'], ['ZM-63', 'To Do'], ['ZM-64', 'To Do'],
  ]) {
    await patchFixtureField(`ZM/tickets/${key}.md`, 'status', from, 'Done');
  }
  await patchFixtureField(`${ZM_SPRINTS_PATH}/${ZM_CROSS_SPRINT}.md`, 'status', 'PLANNED', 'ACTIVE');
  index = await loadVault(root);
  const crossPath = `${ZM_SPRINTS_PATH}/${ZM_CROSS_SPRINT}.md`;
  const crossBase = { action: 'sprint.complete', project: 'ZM', data: { id: ZM_CROSS_SPRINT }, revisions: await revisions(crossPath) };

  await expect(dispatchProjectAction(index, { ...crossBase, data: { id: ZM_CROSS_SPRINT, destinationSprintId: 'ghost' } }))
    .rejects.toThrow(/Destination sprint ghost was not found/);
  await expect(dispatchProjectAction(index, { ...crossBase, data: { id: ZM_CROSS_SPRINT, destinationSprintId: ZM_CROSS_SPRINT } }))
    .rejects.toThrow(/cannot carry unfinished issues into itself/);
  await expect(dispatchProjectAction(index, { ...crossBase, data: { id: ZM_CROSS_SPRINT, destinationSprintId: ZM_ACTIVE_SPRINT } }))
    .rejects.toThrow(/only move to a planned sprint/);

  const moved = await dispatchProjectAction(index, {
    ...crossBase,
    data: { id: ZM_CROSS_SPRINT, destinationSprintId: ZM_S11_SPRINT },
    operationId: 'cfg-sprint-complete-cross',
  });
  expect(moved.result).toMatchObject({ id: ZM_CROSS_SPRINT, status: 'COMPLETED', movedToSprint: ZM_S11_SPRINT, movedToBacklog: 0 });
});

test('version.delete strips the deleted version from ticket fixVersions', async () => {
  let index = await loadVault(root);
  const created = await dispatchProjectAction(index, {
    action: 'issue.create',
    project: 'ZEN',
    data: { title: 'Version carrier', type: 'TASK', fixVersions: ['Alpha'] },
    operationId: 'cfg-version-carrier',
  });
  expect(created.result.key).toBe('ZEN-23');

  index = await loadVault(root);
  const deleted = await dispatchProjectAction(index, {
    action: 'version.delete',
    project: 'ZEN',
    data: { id: 'Alpha' },
    revisions: await revisions(ZEN_VERSIONS_PATH),
    operationId: 'cfg-version-delete',
  });
  expect(deleted.result).toMatchObject({ id: 'Alpha', deleted: true });
  const after = await loadVault(root);
  expect(after.getTicket('ZEN-23').metadata.fixVersions).toEqual([]);
  expect(after.configs.get(ZEN_VERSIONS_PATH).fields.rows.some(row => row.name === 'Alpha')).toBe(false);
});

test('project.create scaffolds catalogs and allocates the first issue number', async () => {
  let index = await loadVault(root);
  const created = await dispatchProjectAction(index, {
    action: 'project.create',
    data: { key: 'NP', name: 'New Project', description: 'Fresh root' },
    revisions: { 'vault.config.json': await readRevision(root, 'vault.config.json') },
    operationId: 'cfg-project-create',
  });
  expect(created.result).toMatchObject({ key: 'NP', name: 'New Project', created: true });
  index = await loadVault(root);
  expect(index.configs.get('NP/workflow.md').fields.statuses.length).toBeGreaterThan(0);
  expect(index.configs.get('NP/workflow.md').fields.transitions.length).toBeGreaterThan(0);
  expect(index.configs.get('NP/labels.md').fields.rows).toEqual([]);
  expect(index.configs.get('NP/versions.md').fields.rows).toEqual([]);
  expect(index.configs.get('NP/config/board.md').fields.columns.length).toBeGreaterThan(0);
  const config = JSON.parse(await readFile(path.join(root, 'vault.config.json'), 'utf8'));
  expect(config.boardRoots).toContainEqual({ project: 'NP', path: 'NP' });
  expect(config.projects.NP).toMatchObject({ key: 'NP', name: 'New Project' });

  // A project without tickets allocates number 1.
  const first = await dispatchProjectAction(index, {
    action: 'issue.create',
    project: 'NP',
    data: { title: 'First issue' },
    operationId: 'cfg-project-first-issue',
  });
  expect(first.result).toMatchObject({ key: 'NP-1', created: true });
});

test('project.create accepts a legacy boards array and refuses an existing project', async () => {
  await setupFixture({ legacyBoards: true });
  let index = await loadVault(root);
  const created = await dispatchProjectAction(index, {
    action: 'project.create',
    data: { key: 'NP', name: 'New Project' },
    revisions: { 'vault.config.json': await readRevision(root, 'vault.config.json') },
    operationId: 'cfg-project-create-boards',
  });
  expect(created.result.created).toBe(true);
  const config = JSON.parse(await readFile(path.join(root, 'vault.config.json'), 'utf8'));
  expect(config.boards).toBeUndefined();
  expect(config.boardRoots.some(item => item.project === 'NP')).toBe(true);

  index = await loadVault(root);
  await expect(dispatchProjectAction(index, {
    action: 'project.create',
    data: { key: 'NP', name: 'Duplicate' },
    revisions: { 'vault.config.json': await readRevision(root, 'vault.config.json') },
  })).rejects.toThrow(/Project NP already exists/);
});
