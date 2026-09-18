import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchSettingsAction } from '../server/settings-actions.mjs';

let root;

const STATUS = {
  todo: 'q3spz9u36h2bf6qa942uqfz1',
  inProgress: 'rdei4coufefotrv2feispqhx',
  inReview: 'nif4xpysb3vxfbvcssw7s6q3',
  done: 'dxhlckk5kgxv4mgp4e8lcu7s',
  blocked: 'qowd8heaf6ej93d62zuugdxv',
};
const TRANSITION_BLOCK = 'ad35e1b7-266f-4d05-8bd6-c403ab9f645b';

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
  'ZEN/config/fieldConfiguration.md': `---
kind: fieldConfiguration
rows: []
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

async function revisions(...paths) {
  return Object.fromEntries(await Promise.all(paths.map(async relative => [relative, await readRevision(root, relative)])));
}

const configRevisions = () => revisions(
  'ZEN/workflow.md',
  'ZEN/config/projects.md',
  'ZEN/config/jira.custom_fields.md',
  'ZEN/config/screens.md',
  'ZEN/config/fieldConfiguration.md',
  'ZEN/config/board.md',
);

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-settings-config-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  // Remove the relational metadata export so every table lives in a
  // per-project config record — the hand-authored vault shape.
  await rm(path.join(root, 'metadata.md'));
  for (const [relative, contents] of Object.entries(CONFIG_FILES)) {
    await mkdir(path.dirname(path.join(root, relative)), { recursive: true });
    await writeFile(path.join(root, relative), contents);
  }
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Settings config test']);
  await git(root, ['config', 'user.email', 'settings-config@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
});

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

const rejectsWith = (promise, code) => expect(promise).rejects.toMatchObject({ code });

test('workflow actions edit the per-project workflow config when no metadata export exists', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.status.create', project: 'ZEN', revisions: await revisions('ZEN/workflow.md') };

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { name: 'Waiting', category: 'IN_PROGRESS' },
    operationId: 'config-status-create',
  });
  expect(created.result).toMatchObject({ action: 'workflow.status.create', project: 'ZEN', updated: true });
  let after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.statuses).toHaveLength(6);
  // The reorder lists status IDs, not names — fetch the created status's id.
  const waitingId = after.configs.get('ZEN/workflow.md').fields.statuses.find(row => row.name === 'Waiting').id;

  const reordered = await dispatchSettingsAction(after, {
    action: 'workflow.status.reorder',
    project: 'ZEN',
    data: { order: [STATUS.done, STATUS.blocked, STATUS.inReview, STATUS.inProgress, STATUS.todo, waitingId] },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'config-status-reorder',
  });
  expect(reordered.result.updated).toBe(true);
  after = await loadVault(root);
  const positions = Object.fromEntries(after.configs.get('ZEN/workflow.md').fields.statuses.map(row => [row.name, row.position]));
  expect(positions.Done).toBe(0);
  expect(positions['To Do']).toBe(4);

  // A status no ticket uses can be deleted along with its transitions.
  const deleted = await dispatchSettingsAction(after, {
    action: 'workflow.status.delete',
    project: 'ZEN',
    data: { id: STATUS.blocked },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'config-status-delete',
  });
  expect(deleted.result.updated).toBe(true);
  after = await loadVault(root);
  const workflow = after.configs.get('ZEN/workflow.md').fields;
  expect(workflow.statuses.some(row => row.name === 'Blocked')).toBe(false);
  expect(workflow.transitions.some(row => row.to_status_id === STATUS.blocked || row.from_status_id === STATUS.blocked)).toBe(false);
});

test('transition rules embed in the workflow config when no rule table exists', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.rule.create', project: 'ZEN', revisions: await revisions('ZEN/workflow.md') };

  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: 'ghost', kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} },
  }), 'NOT_FOUND');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} },
    operationId: 'config-rule-create',
  });
  expect(created.result.updated).toBe(true);
  let after = await loadVault(root);
  const edge = after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK);
  expect(edge.rules).toHaveLength(1);
  expect(edge.rules[0]).toMatchObject({ kind: 'VALIDATOR', type: 'COMMENT_REQUIRED' });
  const ruleId = edge.rules[0].id;

  const deleted = await dispatchSettingsAction(after, {
    action: 'workflow.transition.rule.delete',
    project: 'ZEN',
    data: { id: ruleId },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'config-rule-delete',
  });
  expect(deleted.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).rules).toHaveLength(0);

  await rejectsWith(dispatchSettingsAction(after, {
    action: 'workflow.transition.rule.delete',
    project: 'ZEN',
    data: { id: ruleId },
    revisions: await revisions('ZEN/workflow.md'),
  }), 'NOT_FOUND');
});

test('transition screens resolve against config screen rows', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.screen', project: 'ZEN', revisions: await revisions('ZEN/workflow.md') };

  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, screenId: 'ghost-screen' },
  }), 'NOT_FOUND');

  const attached = await dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, screenId: 'cfg-screen-1' },
    operationId: 'config-screen-attach',
  });
  expect(attached.result.updated).toBe(true);
  let after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).screen_id).toBe('cfg-screen-1');

  const detached = await dispatchSettingsAction(after, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, screenId: null },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'config-screen-detach',
  });
  expect(detached.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).screen_id).toBe(null);
});

test('field.create and field.requirement.set write per-project config records', async () => {
  const index = await loadVault(root);

  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'cfg-area', name: 'Cfg area', type: 'TEXT' },
    revisions: await revisions('ZEN/config/jira.custom_fields.md'),
    operationId: 'config-field-create',
  });
  expect(created.result).toMatchObject({ action: 'field.create', project: 'ZEN', created: true, key: 'cfg-area' });
  let after = await loadVault(root);
  const rows = after.configs.get('ZEN/config/jira.custom_fields.md').fields.rows;
  expect(rows.some(row => row.key === 'cfg-area' && row.project_id === 'cfg-project-1')).toBe(true);

  const set = await dispatchSettingsAction(after, {
    action: 'field.requirement.set',
    project: 'ZEN',
    data: { issueType: 'TASK', builtinField: 'assignee', requirement: 'REQUIRED' },
    revisions: await revisions('ZEN/config/fieldConfiguration.md'),
    operationId: 'config-requirement-set',
  });
  expect(set.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/fieldConfiguration.md').fields.rows).toHaveLength(1);

  const cleared = await dispatchSettingsAction(after, {
    action: 'field.requirement.set',
    project: 'ZEN',
    data: { issueType: 'TASK', builtinField: 'assignee', requirement: 'OPTIONAL' },
    revisions: await revisions('ZEN/config/fieldConfiguration.md'),
    operationId: 'config-requirement-clear',
  });
  expect(cleared.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/fieldConfiguration.md').fields.rows).toHaveLength(0);
});

test('field.create bootstraps a missing config file through the fallback path', async () => {
  const index = await loadVault(root);
  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'WEB',
    data: { key: 'bootstrapped', name: 'Bootstrapped', type: 'TEXT' },
    operationId: 'config-field-bootstrap',
  });
  expect(created.result.created).toBe(true);
  const after = await loadVault(root);
  const record = after.configs.get('WEB/config/customFields.md');
  expect(record.fields.kind).toBe('customFields');
  expect(record.fields.rows.some(row => row.key === 'bootstrapped' && row.project_id === 'WEB')).toBe(true);
});

test('field.create and field.delete resolve a hand-authored customFields.md record by kind', async () => {
  // The originally-broken spelling: without the customFields kind hint the
  // prepare stage reads zero rows, so a create replaced the record's rows
  // (wiping cfg-field-1) and a delete failed NOT_FOUND while the outer
  // hint-using check had found the field.
  await rm(path.join(root, 'ZEN/config/jira.custom_fields.md'));
  await writeFile(path.join(root, 'ZEN/config/customFields.md'), `---
kind: customFields
rows:
  - id: cfg-field-1
    project: ZEN
    key: cfg-tier
    name: Cfg tier
    type: SELECT
    description: null
    position: 0
---
`);
  const index = await loadVault(root);

  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'cfg-area', name: 'Cfg area', type: 'TEXT' },
    revisions: await revisions('ZEN/config/customFields.md'),
    operationId: 'config-field-create-renamed',
  });
  expect(created.result).toMatchObject({ action: 'field.create', project: 'ZEN', created: true, key: 'cfg-area' });
  let after = await loadVault(root);
  const rows = after.configs.get('ZEN/config/customFields.md').fields.rows;
  expect(rows.map(row => row.key)).toEqual(['cfg-tier', 'cfg-area']);

  const deleted = await dispatchSettingsAction(after, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: 'cfg-field-1' },
    revisions: await revisions('ZEN/config/customFields.md', 'ZEN/config/screens.md', 'ZEN/config/fieldConfiguration.md', 'ZEN/workflow.md'),
    operationId: 'config-field-delete-renamed',
  });
  expect(deleted.result).toMatchObject({ id: 'cfg-field-1', deleted: true });
  after = await loadVault(root);
  const remaining = after.configs.get('ZEN/config/customFields.md').fields.rows;
  expect(remaining.map(row => row.key)).toEqual(['cfg-area']);
});

test('field.delete cleans embedded screen fields and workflow rules in config mode', async () => {
  const index = await loadVault(root);
  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'doomed-cfg', name: 'Doomed cfg', type: 'SELECT', options: ['Gold'] },
    revisions: await revisions('ZEN/config/jira.custom_fields.md'),
    operationId: 'config-field-delete-seed',
  });
  const fieldId = created.result.id;
  let after = await loadVault(root);
  await dispatchSettingsAction(after, {
    action: 'workflow.transition.rule.create',
    project: 'ZEN',
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { customFieldId: fieldId } },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'config-field-delete-rule',
  });
  after = await loadVault(root);
  await dispatchSettingsAction(after, {
    action: 'screen.fields.set',
    project: 'ZEN',
    data: { id: 'cfg-screen-1', fields: [{ builtinField: null, customFieldId: fieldId }] },
    revisions: await revisions('ZEN/config/screens.md'),
    operationId: 'config-field-delete-screen',
  });
  after = await loadVault(root);

  const deleted = await dispatchSettingsAction(after, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: fieldId },
    revisions: await revisions('ZEN/config/jira.custom_fields.md', 'ZEN/config/screens.md', 'ZEN/config/fieldConfiguration.md', 'ZEN/workflow.md'),
    operationId: 'config-field-delete',
  });
  expect(deleted.result).toMatchObject({ id: fieldId, deleted: true });
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/jira.custom_fields.md').fields.rows.some(row => row.id === fieldId)).toBe(false);
  // The config-mode screens record is cleaned too: the embedded reference to
  // the deleted field is removed, not just the workflow rules.
  const screen = after.configs.get('ZEN/config/screens.md').fields.rows.find(row => row.id === 'cfg-screen-1');
  expect(screen.fields).toEqual([]);
  const edge = after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK);
  expect(edge.rules).toEqual([]);
});

test('field.delete refuses stale revisions for its config targets', async () => {
  const index = await loadVault(root);
  // Capture the customFields revision BEFORE the field.create below: that
  // write appends a row, so the captured revision goes stale.
  const staleFieldRevision = (await revisions('ZEN/config/jira.custom_fields.md'))['ZEN/config/jira.custom_fields.md'];
  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'stale-doomed', name: 'Stale doomed', type: 'SELECT', options: ['Gold'] },
    revisions: await revisions('ZEN/config/jira.custom_fields.md'),
    operationId: 'config-field-delete-stale-seed',
  });
  const fieldId = created.result.id;
  // Capture the screens revision BEFORE the screen write below embeds the
  // field, so the captured revision goes stale too.
  const staleScreenRevision = (await revisions('ZEN/config/screens.md'))['ZEN/config/screens.md'];
  let after = await loadVault(root);
  await dispatchSettingsAction(after, {
    action: 'screen.fields.set',
    project: 'ZEN',
    data: { id: 'cfg-screen-1', fields: [{ builtinField: null, customFieldId: fieldId }] },
    revisions: await revisions('ZEN/config/screens.md'),
    operationId: 'config-field-delete-stale-screen',
  });
  after = await loadVault(root);
  const fresh = await revisions('ZEN/config/jira.custom_fields.md', 'ZEN/config/fieldConfiguration.md', 'ZEN/workflow.md');

  // A stale screens revision is refused before anything is written.
  await rejectsWith(dispatchSettingsAction(after, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: fieldId },
    revisions: { ...fresh, 'ZEN/config/screens.md': staleScreenRevision },
    operationId: 'config-field-delete-stale-screens',
  }), 'STALE');
  // The same for a stale customFields revision.
  await rejectsWith(dispatchSettingsAction(after, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: fieldId },
    revisions: { ...fresh, 'ZEN/config/jira.custom_fields.md': staleFieldRevision },
    operationId: 'config-field-delete-stale-fields',
  }), 'STALE');
  // Neither refusal reached the prepare stage, so the field survives.
  expect((await loadVault(root)).configs.get('ZEN/config/jira.custom_fields.md').fields.rows.some(row => row.id === fieldId)).toBe(true);
});

test('screen actions keep fields embedded on the config rows', async () => {
  const index = await loadVault(root);
  const base = { project: 'ZEN' };

  const created = await dispatchSettingsAction(index, {
    action: 'screen.create',
    ...base,
    data: { name: 'Cfg review', fields: [{ builtinField: 'assignee' }, { builtinField: null, customFieldId: 'cfg-field-1' }] },
    revisions: await revisions('ZEN/config/screens.md'),
    operationId: 'config-screen-create',
  });
  expect(created.result).toMatchObject({ created: true, name: 'Cfg review' });
  let after = await loadVault(root);
  let screen = after.configs.get('ZEN/config/screens.md').fields.rows.find(row => row.id === created.result.id);
  expect(screen.fields).toEqual([
    { builtinField: 'assignee', customFieldId: null },
    { builtinField: null, customFieldId: 'cfg-field-1' },
  ]);

  const set = await dispatchSettingsAction(after, {
    action: 'screen.fields.set',
    ...base,
    data: { id: created.result.id, fields: [{ builtinField: 'description' }] },
    revisions: await revisions('ZEN/config/screens.md'),
    operationId: 'config-screen-fields-set',
  });
  expect(set.result.updated).toBe(true);
  after = await loadVault(root);
  screen = after.configs.get('ZEN/config/screens.md').fields.rows.find(row => row.id === created.result.id);
  expect(screen.fields).toEqual([{ builtinField: 'description', customFieldId: null }]);

  const deleted = await dispatchSettingsAction(after, {
    action: 'screen.delete',
    ...base,
    data: { id: created.result.id },
    revisions: await revisions('ZEN/config/screens.md', 'ZEN/workflow.md'),
    operationId: 'config-screen-delete',
  });
  expect(deleted.result.deleted).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/screens.md').fields.rows.some(row => row.id === created.result.id)).toBe(false);

  await rejectsWith(dispatchSettingsAction(after, {
    action: 'screen.fields.set',
    ...base,
    data: { id: 'ghost', fields: [] },
    revisions: await revisions('ZEN/config/screens.md'),
  }), 'NOT_FOUND');
});

test('board actions fall back to the per-project board config', async () => {
  const index = await loadVault(root);
  const base = { project: 'ZEN', revisions: await revisions('ZEN/config/board.md') };

  const updated = await dispatchSettingsAction(index, {
    action: 'board.settings.update',
    ...base,
    data: { name: 'Cfg board renamed', type: 'SCRUM', swimlaneBy: 'EPIC', cardFields: ['key', 'priority'], filter: { type: 'BUG' } },
    operationId: 'config-board-update',
  });
  expect(updated.result.updated).toBe(true);
  let after = await loadVault(root);
  let board = after.configs.get('ZEN/config/board.md').fields;
  expect(board).toMatchObject({ name: 'Cfg board renamed', type: 'SCRUM', swimlaneBy: 'EPIC', cardFields: ['key', 'priority'], filter: { type: 'BUG' } });

  const column = await dispatchSettingsAction(after, {
    action: 'board.column.create',
    ...base,
    data: { name: 'Cfg lane' },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-column-create',
  });
  expect(column.result.created).toBe(true);
  after = await loadVault(root);
  board = after.configs.get('ZEN/config/board.md').fields;
  expect(board.columns).toHaveLength(3);
  const laneId = column.result.id;

  const renamed = await dispatchSettingsAction(after, {
    action: 'board.column.update',
    ...base,
    data: { id: laneId, name: 'Cfg lane 2', wipLimit: 3 },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-column-update',
  });
  expect(renamed.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.columns.find(row => row.id === laneId)).toMatchObject({ name: 'Cfg lane 2', wipLimit: 3 });

  const mapped = await dispatchSettingsAction(after, {
    action: 'board.column.status.map',
    ...base,
    data: { columnId: 'cfg-col-1', statusId: STATUS.blocked },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-map',
  });
  expect(mapped.result).toMatchObject({ columnId: 'cfg-col-1', statusId: STATUS.blocked, updated: true });
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.columns.find(row => row.id === 'cfg-col-1').statusIds).toEqual([STATUS.blocked]);

  const unmapped = await dispatchSettingsAction(after, {
    action: 'board.column.status.unmap',
    ...base,
    data: { columnId: 'cfg-col-1', statusId: STATUS.blocked },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-unmap',
  });
  expect(unmapped.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.columns.find(row => row.id === 'cfg-col-1').statusIds).toEqual([]);

  const filter = await dispatchSettingsAction(after, {
    action: 'board.quick-filter.create',
    ...base,
    data: { name: 'Cfg bugs', filter: { type: 'BUG' } },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-filter-create',
  });
  expect(filter.result.created).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.quickFilters).toHaveLength(1);

  const removedFilter = await dispatchSettingsAction(after, {
    action: 'board.quick-filter.delete',
    ...base,
    data: { id: filter.result.id },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-filter-delete',
  });
  expect(removedFilter.result.deleted).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.quickFilters).toHaveLength(0);

  const reordered = await dispatchSettingsAction(after, {
    action: 'board.column.reorder',
    ...base,
    data: { order: ['cfg-col-2', 'cfg-col-1', laneId] },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-reorder',
  });
  expect(reordered.result.updated).toBe(true);
  after = await loadVault(root);
  const positions = Object.fromEntries(after.configs.get('ZEN/config/board.md').fields.columns.map(row => [row.id, row.position]));
  expect(positions['cfg-col-2']).toBe(0);
  expect(positions[laneId]).toBe(2);

  const deletedColumn = await dispatchSettingsAction(after, {
    action: 'board.column.delete',
    ...base,
    data: { id: laneId },
    revisions: await revisions('ZEN/config/board.md'),
    operationId: 'config-board-column-delete',
  });
  expect(deletedColumn.result.deleted).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/config/board.md').fields.columns.some(row => row.id === laneId)).toBe(false);

  await rejectsWith(dispatchSettingsAction(after, {
    action: 'board.column.update',
    ...base,
    data: { id: 'ghost' },
    revisions: await revisions('ZEN/config/board.md'),
  }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(after, {
    action: 'board.quick-filter.delete',
    ...base,
    data: { id: 'ghost' },
    revisions: await revisions('ZEN/config/board.md'),
  }), 'NOT_FOUND');
});
