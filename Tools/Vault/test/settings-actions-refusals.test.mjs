import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchSettingsAction } from '../server/settings-actions.mjs';
import { expectedRevision, patchRecord } from '../cli/write.mjs';

let root;

const ZEN_PROJECT_ID = 'r1m7idj9n6rkv5b1iulqz2ho';
const ZEN_BOARD_ID = 'kk4qvydqx1c0ghgwvuqzdzlq';
const STATUS = {
  todo: 'q3spz9u36h2bf6qa942uqfz1',
  inProgress: 'rdei4coufefotrv2feispqhx',
  inReview: 'nif4xpysb3vxfbvcssw7s6q3',
  done: 'dxhlckk5kgxv4mgp4e8lcu7s',
  blocked: 'qowd8heaf6ej93d62zuugdxv',
};
const COLUMN = {
  todo: 'wygxkaiwynztxr3ypwsgv19g',
  inProgress: 'qnnmw9ih4rpnnjl7ziuo1m3v',
  inReview: 'j18qbt713owjdg3sq7gmxwgm',
  done: 'y6uuwnep037jsc1fhqprl5hi',
  blocked: 'zs2q8qm36jakw84a1xts3ufb',
};
const TRANSITION_BLOCK = 'ad35e1b7-266f-4d05-8bd6-c403ab9f645b';
const TRANSITION_STOP_PROGRESS = '7535964d-128f-4ebb-9b42-64af01c498d1';

async function revisions(...paths) {
  return Object.fromEntries(await Promise.all(paths.map(async relative => [relative, await readRevision(root, relative)])));
}

const metadataRevisions = () => revisions('metadata.md');
const workflowRevisions = () => revisions('metadata.md', 'ZEN/workflow.md');

async function setupMetadataFixture() {
  const index = await loadVault(root);
  const metadata = [...index.metadataFiles.values()][0];
  const tables = structuredClone(metadata.fields.tables);
  const project = tables['jira.projects'].find(row => row.key === 'ZEN');
  tables['jira.custom_fields'] = [
    { id: 'imported-field', project_id: project.id, key: 'imported-tier', name: 'Imported tier', type: 'SELECT', description: null, position: 0 },
  ];
  tables['jira.custom_field_options'] = [
    { id: 'imported-option', field_id: 'imported-field', value: 'Gold', position: 0 },
  ];
  tables['jira.screens'] = [
    { id: 'imported-screen', project_id: project.id, name: 'Imported screen', description: null, position: 0 },
  ];
  await writeFile(path.join(root, metadata.relativePath), patchRecord(metadata, { tables }));
}

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-settings-refusals-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await setupMetadataFixture();
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Settings refusal test']);
  await git(root, ['config', 'user.email', 'settings-refusals@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
});

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

const rejectsWith = (promise, code) => expect(promise).rejects.toMatchObject({ code });

test('request normalization refuses malformed actions, projects, data and operation ids', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchSettingsAction(index, null), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {}), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.summon' }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.create', project: 'TOOLONGKEY1' }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.create', project: '1BAD' }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.create' }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.create', project: 'ZEN', data: ['nope'] }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { action: 'field.create', project: 'ZEN', data: 'nope' }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'x', name: 'X', type: 'TEXT' },
    revisions: await metadataRevisions(),
    operationId: 'bad id!',
  }), 'ID');
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZZZ',
    data: { key: 'x', name: 'X', type: 'TEXT' },
  }), 'NOT_FOUND');
});

test('a read-only project root refuses settings edits', async () => {
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.boardRoots.find(item => item.project === 'ZEN').readOnly = true;
  await writeFile(configPath, JSON.stringify(config));
  const index = await loadVault(root);
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'blocked', name: 'Blocked', type: 'TEXT' },
  }), 'READ_ONLY');
});

test('workflow.status.create validates names, categories and duplicates', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.status.create', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 42 } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'x'.repeat(101) } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Waiting', category: 'SIDEWAYS' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'To Do' } }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { name: 'Waiting', category: 'IN_PROGRESS' },
    operationId: 'status-create-waiting',
  });
  expect(created.result).toMatchObject({ action: 'workflow.status.create', project: 'ZEN', updated: true });
  const after = await loadVault(root);
  const statuses = after.configs.get('ZEN/workflow.md').fields.statuses;
  expect(statuses).toHaveLength(6);
  expect(statuses.some(row => row.name === 'Waiting' && row.category === 'IN_PROGRESS')).toBe(true);
});

test('workflow.status.update validates targets, categories and name collisions', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.status.update', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'ghost' } }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: STATUS.done, category: 'SIDEWAYS' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: STATUS.blocked, name: 'To Do' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: STATUS.done, category: 'IN_PROGRESS' } }), 'INVALID');

  const renamed = await dispatchSettingsAction(index, {
    ...base,
    data: { id: STATUS.blocked, name: 'Frozen' },
    operationId: 'status-rename-blocked',
  });
  expect(renamed.result.updated).toBe(true);
  const after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.statuses.some(row => row.name === 'Frozen')).toBe(true);
});

test('workflow.status.delete refuses used statuses and removes unused ones with their transitions', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.status.delete', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'ghost' } }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: STATUS.todo } }), 'INVALID');

  const deleted = await dispatchSettingsAction(index, {
    ...base,
    data: { id: STATUS.blocked },
    operationId: 'status-delete-blocked',
  });
  expect(deleted.result.updated).toBe(true);
  const after = await loadVault(root);
  const workflow = after.configs.get('ZEN/workflow.md').fields;
  expect(workflow.statuses).toHaveLength(4);
  expect(workflow.statuses.some(row => row.name === 'Blocked')).toBe(false);
  expect(workflow.transitions.some(row => row.to_status_id === STATUS.blocked || row.from_status_id === STATUS.blocked)).toBe(false);
});

test('workflow.status.reorder validates the order list and applies a new order', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.status.reorder', project: 'ZEN', revisions: await workflowRevisions() };
  const full = [STATUS.todo, STATUS.inProgress, STATUS.inReview, STATUS.blocked, STATUS.done];

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: full.slice(0, 4) } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: [...full, 'ghost'] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: [STATUS.todo, STATUS.todo, ...full.slice(2)] } }), 'INVALID');

  const reordered = await dispatchSettingsAction(index, {
    ...base,
    data: { order: [...full].reverse() },
    operationId: 'status-reorder',
  });
  expect(reordered.result.updated).toBe(true);
  const after = await loadVault(root);
  const positions = Object.fromEntries(after.configs.get('ZEN/workflow.md').fields.statuses.map(row => [row.id, row.position]));
  expect(positions[STATUS.done]).toBe(0);
  expect(positions[STATUS.todo]).toBe(4);
});

test('workflow.transition.create validates endpoints and duplicates', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.create', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { toStatusId: 'ghost' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { fromStatusId: 'ghost', toStatusId: STATUS.todo } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { fromStatusId: STATUS.todo, toStatusId: STATUS.todo } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { fromStatusId: null, toStatusId: STATUS.blocked } }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { fromStatusId: STATUS.todo, toStatusId: STATUS.blocked, name: 'Park' },
    operationId: 'transition-create-park',
  });
  expect(created.result.updated).toBe(true);
  const after = await loadVault(root);
  const edge = after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.name === 'Park');
  expect(edge.from_status_id).toBe(STATUS.todo);
  expect(edge.to_status_id).toBe(STATUS.blocked);
});

test('workflow.transition.delete removes a transition and its stored rules', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.delete', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'ghost' } }), 'NOT_FOUND');

  const deleted = await dispatchSettingsAction(index, {
    ...base,
    data: { id: TRANSITION_STOP_PROGRESS },
    operationId: 'transition-delete-stop',
  });
  expect(deleted.result.updated).toBe(true);
  const after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.some(row => row.id === TRANSITION_STOP_PROGRESS)).toBe(false);
});

test('workflow.transition.rule.create validates kind, type, config and custom fields', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.rule.create', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: 'ghost', kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} },
  }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'CURSE', type: 'COMMENT_REQUIRED', config: {} },
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'SET_RESOLUTION', config: {} },
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { field: 'assignee', customFieldId: 'imported-field' } },
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { field: 'nonsense' } },
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'CONDITION', type: 'ORG_ROLE', config: {} },
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { customFieldId: 'ghost-field' } },
  }), 'NOT_FOUND');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { field: 'assignee' } },
    operationId: 'rule-create-field-required',
  });
  expect(created.result.updated).toBe(true);
  const after = await loadVault(root);
  const rules = after.tables.get('jira.workflow_transition_rules');
  expect(rules).toHaveLength(1);
  expect(rules[0]).toMatchObject({ transition_id: TRANSITION_BLOCK, kind: 'VALIDATOR', rule_type: 'FIELD_REQUIRED' });
});

test('workflow.transition.rule.create refuses a custom field owned by another project', async () => {
  const index = await loadVault(root);
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.create',
    project: 'DP',
    data: { transitionId: 'ghost', kind: 'VALIDATOR', type: 'FIELD_REQUIRED', config: { customFieldId: 'imported-field' } },
    revisions: await revisions('metadata.md', 'DP/workflow.md'),
  }), 'NOT_FOUND');
});

test('workflow.transition.rule.delete removes only a known rule', async () => {
  const index = await loadVault(root);
  const revisions = await workflowRevisions();
  const created = await dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.create',
    project: 'ZEN',
    data: { transitionId: TRANSITION_BLOCK, kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} },
    revisions,
    operationId: 'rule-delete-seed',
  });
  const ruleId = (await loadVault(root)).tables.get('jira.workflow_transition_rules')[0].id;
  const freshRevisions = await workflowRevisions();

  await rejectsWith(dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.delete',
    project: 'ZEN',
    data: {},
    revisions: freshRevisions,
  }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.delete',
    project: 'ZEN',
    data: { id: 'ghost' },
    revisions: freshRevisions,
  }), 'NOT_FOUND');

  const deleted = await dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.delete',
    project: 'ZEN',
    data: { id: ruleId },
    revisions: freshRevisions,
    operationId: 'rule-delete',
  });
  expect(deleted.result.updated).toBe(true);
  expect((await loadVault(root)).tables.get('jira.workflow_transition_rules')).toHaveLength(0);
  expect(created.result.action).toBe('workflow.transition.rule.create');
});

test('workflow.transition.screen validates the transition and screen, then attaches and detaches', async () => {
  const index = await loadVault(root);
  const base = { action: 'workflow.transition.screen', project: 'ZEN', revisions: await workflowRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { transitionId: 'ghost', screenId: 'imported-screen' } }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { transitionId: TRANSITION_BLOCK, screenId: 'ghost-screen' } }), 'NOT_FOUND');

  const attached = await dispatchSettingsAction(index, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, screenId: 'imported-screen' },
    operationId: 'screen-attach',
  });
  expect(attached.result.updated).toBe(true);
  let after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).screen_id).toBe('imported-screen');

  const detached = await dispatchSettingsAction(after, {
    ...base,
    data: { transitionId: TRANSITION_BLOCK, screenId: null },
    revisions: await workflowRevisions(),
    operationId: 'screen-detach',
  });  expect(detached.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).screen_id).toBe(null);
});

test('field.create validates key, name, type and options', async () => {
  const index = await loadVault(root);
  const base = { action: 'field.create', project: 'ZEN', revisions: await metadataRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'Bad Key', name: 'X', type: 'TEXT' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', type: 'TEXT' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', name: 'X', type: 'WIDGET' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', name: 'X', type: 'SELECT' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', name: 'X', type: 'TEXT', options: ['Gold'] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', name: 'X', type: 'SELECT', options: ['Gold', 'Gold'] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { key: 'ok-key', name: 'X', type: 'SELECT', options: ['Gold', '  '] } }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { key: 'customer-tier', name: 'Customer tier', type: 'SELECT', options: ['Gold', 'Silver'] },
    operationId: 'field-create-tier',
  });
  expect(created.result).toMatchObject({ action: 'field.create', project: 'ZEN', created: true, key: 'customer-tier' });
  const after = await loadVault(root);
  const field = after.tables.get('jira.custom_fields').find(row => row.key === 'customer-tier');
  expect(field.project_id).toBe(ZEN_PROJECT_ID);
  expect(after.tables.get('jira.custom_field_options').filter(row => row.field_id === field.id).map(row => row.value)).toEqual(['Gold', 'Silver']);

  await rejectsWith(dispatchSettingsAction(after, {
    ...base,
    data: { key: 'customer-tier', name: 'Customer tier again', type: 'TEXT' },
    revisions: await metadataRevisions(),
  }), 'INVALID');
});

test('field.delete refuses unknown fields and cleans dependent rows', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchSettingsAction(index, { action: 'field.delete', project: 'ZEN', data: { id: 'ghost' } }), 'NOT_FOUND');

  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'doomed', name: 'Doomed', type: 'SELECT', options: ['Gold'] },
    revisions: await metadataRevisions(),
    operationId: 'field-delete-seed',
  });
  const fieldId = created.result.id;
  let after = await loadVault(root);
  await dispatchSettingsAction(after, {
    action: 'field.requirement.set',
    project: 'ZEN',
    data: { issueType: 'TASK', customFieldId: fieldId, builtinField: null, requirement: 'REQUIRED' },
    revisions: await metadataRevisions(),
    operationId: 'field-delete-requirement',
  });
  after = await loadVault(root);
  expect(after.tables.get('jira.field_configurations')).toHaveLength(1);

  const deleted = await dispatchSettingsAction(after, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: fieldId },
    revisions: await revisions('metadata.md', 'ZEN/workflow.md'),
    operationId: 'field-delete',
  });
  expect(deleted.result).toMatchObject({ id: fieldId, deleted: true });
  after = await loadVault(root);
  expect(after.tables.get('jira.custom_fields').some(row => row.id === fieldId)).toBe(false);
  expect(after.tables.get('jira.custom_field_options').some(row => row.field_id === fieldId)).toBe(false);
  expect(after.tables.get('jira.field_configurations')).toHaveLength(0);
});

test('field.requirement.set validates issue type, requirement and field references', async () => {
  const index = await loadVault(root);
  const base = { action: 'field.requirement.set', project: 'ZEN', revisions: await metadataRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'WIDGET', builtinField: 'assignee', requirement: 'REQUIRED' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'TASK', builtinField: 'assignee', requirement: 'MUST' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'TASK', builtinField: 'assignee', customFieldId: 'imported-field', requirement: 'REQUIRED' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'TASK', builtinField: null, customFieldId: null, requirement: 'REQUIRED' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'TASK', builtinField: 'nonsense', requirement: 'REQUIRED' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { issueType: 'TASK', builtinField: null, customFieldId: 'ghost-field', requirement: 'REQUIRED' } }), 'NOT_FOUND');

  const set = await dispatchSettingsAction(index, {
    ...base,
    data: { issueType: 'TASK', builtinField: 'assignee', builtin_field: null, customFieldId: null, requirement: 'REQUIRED' },
    operationId: 'requirement-set-required',
  });
  expect(set.result).toMatchObject({ action: 'field.requirement.set', project: 'ZEN', updated: true });
  let after = await loadVault(root);
  expect(after.tables.get('jira.field_configurations')).toHaveLength(1);

  const cleared = await dispatchSettingsAction(after, {
    ...base,
    data: { issueType: 'TASK', builtinField: 'assignee', requirement: 'OPTIONAL' },
    revisions: await metadataRevisions(),
    operationId: 'requirement-set-default',
  });
  expect(cleared.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.tables.get('jira.field_configurations')).toHaveLength(0);
});

test('field.requirement.set refuses a custom field owned by another project', async () => {
  const index = await loadVault(root);
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'field.requirement.set',
    project: 'DP',
    data: { issueType: 'TASK', builtinField: null, customFieldId: 'imported-field', requirement: 'REQUIRED' },
    revisions: await metadataRevisions(),
  }), 'NOT_FOUND');
});

test('screen.create validates name, duplicates and field references', async () => {
  const index = await loadVault(root);
  const base = { action: 'screen.create', project: 'ZEN', revisions: await metadataRevisions() };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Imported screen' } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: { builtinField: 'assignee' } } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: [{ builtinField: 'assignee', customFieldId: 'imported-field' }] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: [{ builtinField: null, customFieldId: null }] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: [{ builtinField: 'nonsense' }] } }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: [{ builtinField: null, customFieldId: 'ghost-field' }] } }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Review', fields: [{ builtinField: 'assignee' }, { builtinField: 'assignee' }] } }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { name: 'Review screen', fields: [{ builtinField: 'assignee' }, { builtinField: null, customFieldId: 'imported-field' }] },
    operationId: 'screen-create-review',
  });
  expect(created.result).toMatchObject({ action: 'screen.create', project: 'ZEN', created: true, name: 'Review screen' });
  const after = await loadVault(root);
  const rows = after.tables.get('jira.screen_fields').filter(row => row.screen_id === created.result.id);
  expect(rows.map(row => row.builtin_field ?? row.custom_field_id).sort()).toEqual(['assignee', 'imported-field']);
});

test('screen.fields.set refuses unknown screens and replaces the field list', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchSettingsAction(index, {
    action: 'screen.fields.set',
    project: 'ZEN',
    data: { id: 'ghost-screen', fields: [{ builtinField: 'assignee' }] },
    revisions: await metadataRevisions(),
  }), 'NOT_FOUND');

  const set = await dispatchSettingsAction(index, {
    action: 'screen.fields.set',
    project: 'ZEN',
    data: { id: 'imported-screen', fields: [{ builtinField: 'description' }] },
    revisions: await metadataRevisions(),
    operationId: 'screen-fields-set',
  });
  expect(set.result).toMatchObject({ id: 'imported-screen', updated: true });
  const after = await loadVault(root);
  const rows = after.tables.get('jira.screen_fields').filter(row => row.screen_id === 'imported-screen');
  expect(rows).toHaveLength(1);
  expect(rows[0].builtin_field).toBe('description');
});

test('screen.fields.set refuses a screen owned by another project', async () => {
  const index = await loadVault(root);
  await rejectsWith(dispatchSettingsAction(index, {
    action: 'screen.fields.set',
    project: 'DP',
    data: { id: 'imported-screen', fields: [{ builtinField: 'assignee' }] },
    revisions: await metadataRevisions(),
  }), 'NOT_FOUND');
});

test('screen.delete refuses unknown screens and detaches the workflow reference', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchSettingsAction(index, {
    action: 'screen.delete',
    project: 'ZEN',
    data: { id: 'ghost-screen' },
    revisions: await revisions('metadata.md', 'ZEN/workflow.md'),
  }), 'NOT_FOUND');

  const created = await dispatchSettingsAction(index, {
    action: 'screen.create',
    project: 'ZEN',
    data: { name: 'Doomed screen' },
    revisions: await metadataRevisions(),
    operationId: 'screen-delete-seed',
  });
  const screenId = created.result.id;
  let after = await loadVault(root);
  await dispatchSettingsAction(after, {
    action: 'workflow.transition.screen',
    project: 'ZEN',
    data: { transitionId: TRANSITION_BLOCK, screenId },
    revisions: await workflowRevisions(),
    operationId: 'screen-delete-attach',
  });
  after = await loadVault(root);

  const deleted = await dispatchSettingsAction(after, {
    action: 'screen.delete',
    project: 'ZEN',
    data: { id: screenId },
    revisions: await revisions('metadata.md', 'ZEN/workflow.md'),
    operationId: 'screen-delete',
  });
  expect(deleted.result).toMatchObject({ id: screenId, deleted: true });
  after = await loadVault(root);
  expect(after.tables.get('jira.screens').some(row => row.id === screenId)).toBe(false);
  expect(after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === TRANSITION_BLOCK).screen_id).toBe(null);
});

test('board.settings.update validates type, swimlane, card fields and filters', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.settings.update', project: 'ZEN' };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { type: 'SCRUMPLUS' }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { swimlaneBy: 'SIDEWAYS' }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { cardFields: ['key', 'nope'] }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { cardFields: 'key' }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { filter: { nope: 1 } }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { filter: ['BUG'] }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { filter: { flagged: 'yes' } }, revisions: await metadataRevisions() }), 'INVALID');

  const updated = await dispatchSettingsAction(index, {
    ...base,
    data: { name: 'Engine board', type: 'SCRUM', swimlaneBy: 'EPIC', cardFields: ['key', 'priority'], filter: { type: 'BUG' } },
    revisions: await metadataRevisions(),
    operationId: 'board-settings-update',
  });
  expect(updated.result).toMatchObject({ id: ZEN_BOARD_ID, updated: true });
  const after = await loadVault(root);
  const board = after.tables.get('jira.boards').find(row => row.id === ZEN_BOARD_ID);
  expect(board).toMatchObject({ name: 'Engine board', type: 'SCRUM', swimlane_by: 'EPIC', card_fields: ['key', 'priority'], filter: { type: 'BUG' } });
});

test('board.column.create requires a name and appends a column', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchSettingsAction(index, {
    action: 'board.column.create',
    project: 'ZEN',
    data: {},
    revisions: await metadataRevisions(),
  }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    action: 'board.column.create',
    project: 'ZEN',
    data: { name: 'Custom lane' },
    revisions: await metadataRevisions(),
    operationId: 'board-column-create',
  });
  expect(created.result).toMatchObject({ name: 'Custom lane', created: true });
  const after = await loadVault(root);
  const columns = after.tables.get('jira.board_columns').filter(row => row.board_id === ZEN_BOARD_ID);
  expect(columns).toHaveLength(6);
  expect(columns.some(row => row.id === created.result.id && row.position === 6)).toBe(true);
});

test('board.column.update refuses unknown and foreign columns and validates the WIP limit', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.column.update', project: 'ZEN' };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'ghost' }, revisions: await metadataRevisions() }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: COLUMN.blocked, wipLimit: -1 }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: COLUMN.blocked, wipLimit: 1.5 }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'bkhrhb6xvepsxf7x6eph1npm' }, revisions: await metadataRevisions() }), 'NOT_FOUND');

  const updated = await dispatchSettingsAction(index, {
    ...base,
    data: { id: COLUMN.blocked, name: 'Frozen', wipLimit: 3 },
    revisions: await metadataRevisions(),
    operationId: 'board-column-update',
  });
  expect(updated.result).toMatchObject({ id: COLUMN.blocked, updated: true });
  const after = await loadVault(root);
  const column = after.tables.get('jira.board_columns').find(row => row.id === COLUMN.blocked);
  expect(column).toMatchObject({ name: 'Frozen', wip_limit: 3 });
});

test('board.column.delete refuses unknown columns and removes mappings with the column', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.column.delete', project: 'ZEN' };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { id: 'ghost' }, revisions: await metadataRevisions() }), 'NOT_FOUND');

  const deleted = await dispatchSettingsAction(index, {
    ...base,
    data: { id: COLUMN.blocked },
    revisions: await metadataRevisions(),
    operationId: 'board-column-delete',
  });
  expect(deleted.result).toMatchObject({ id: COLUMN.blocked, deleted: true });
  const after = await loadVault(root);
  expect(after.tables.get('jira.board_columns').filter(row => row.board_id === ZEN_BOARD_ID)).toHaveLength(4);
  expect(after.tables.get('jira.board_column_statuses').some(row => row.column_id === COLUMN.blocked)).toBe(false);
});

test('board.column.reorder validates the order and applies it', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.column.reorder', project: 'ZEN' };
  const full = [COLUMN.todo, COLUMN.inProgress, COLUMN.inReview, COLUMN.done, COLUMN.blocked];

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: full.slice(0, 4) }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: [...full, 'ghost'] }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { order: [COLUMN.todo, COLUMN.todo, ...full.slice(2)] }, revisions: await metadataRevisions() }), 'INVALID');

  const reordered = await dispatchSettingsAction(index, {
    ...base,
    data: { order: [...full].reverse() },
    revisions: await metadataRevisions(),
    operationId: 'board-column-reorder',
  });
  expect(reordered.result.updated).toBe(true);
  const after = await loadVault(root);
  const positions = Object.fromEntries(
    after.tables.get('jira.board_columns').filter(row => row.board_id === ZEN_BOARD_ID).map(row => [row.id, row.position]),
  );
  expect(positions[COLUMN.blocked]).toBe(0);
  expect(positions[COLUMN.todo]).toBe(4);
});

test('board.column.status.map and unmap validate column and status and rewrite mappings', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.column.status.map', project: 'ZEN' };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { columnId: 'ghost', statusId: STATUS.blocked }, revisions: await metadataRevisions() }), 'NOT_FOUND');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { columnId: COLUMN.todo, statusId: 'ghost' }, revisions: await metadataRevisions() }), 'NOT_FOUND');

  const mapped = await dispatchSettingsAction(index, {
    ...base,
    data: { columnId: COLUMN.todo, statusId: STATUS.blocked },
    revisions: await metadataRevisions(),
    operationId: 'board-column-map',
  });
  expect(mapped.result).toMatchObject({ columnId: COLUMN.todo, statusId: STATUS.blocked, updated: true });
  let after = await loadVault(root);
  expect(after.tables.get('jira.board_column_statuses').some(row => row.column_id === COLUMN.todo && row.status_id === STATUS.blocked)).toBe(true);

  const unmapped = await dispatchSettingsAction(after, {
    action: 'board.column.status.unmap',
    project: 'ZEN',
    data: { columnId: COLUMN.todo, statusId: STATUS.blocked },
    revisions: await metadataRevisions(),
    operationId: 'board-column-unmap',
  });
  expect(unmapped.result.updated).toBe(true);
  after = await loadVault(root);
  expect(after.tables.get('jira.board_column_statuses').some(row => row.column_id === COLUMN.todo && row.status_id === STATUS.blocked)).toBe(false);
});

test('board.quick-filter.create validates the filter and appends; delete refuses unknown ids', async () => {
  const index = await loadVault(root);
  const base = { action: 'board.quick-filter.create', project: 'ZEN' };

  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Bugs', filter: { nope: 1 } }, revisions: await metadataRevisions() }), 'INVALID');
  await rejectsWith(dispatchSettingsAction(index, { ...base, data: { name: 'Bugs', filter: 'BUG' }, revisions: await metadataRevisions() }), 'INVALID');

  const created = await dispatchSettingsAction(index, {
    ...base,
    data: { name: 'Bugs only', filter: { type: 'BUG' } },
    revisions: await metadataRevisions(),
    operationId: 'quick-filter-create',
  });
  expect(created.result).toMatchObject({ name: 'Bugs only', created: true });
  let after = await loadVault(root);
  const stored = after.tables.get('jira.board_quick_filters').find(row => row.id === created.result.id);
  expect(stored).toMatchObject({ board_id: ZEN_BOARD_ID, name: 'Bugs only', filter: { type: 'BUG' } });

  await rejectsWith(dispatchSettingsAction(after, {
    action: 'board.quick-filter.delete',
    project: 'ZEN',
    data: { id: 'ghost' },
    revisions: await metadataRevisions(),
  }), 'NOT_FOUND');

  const deleted = await dispatchSettingsAction(after, {
    action: 'board.quick-filter.delete',
    project: 'ZEN',
    data: { id: created.result.id },
    revisions: await metadataRevisions(),
    operationId: 'quick-filter-delete',
  });
  expect(deleted.result).toMatchObject({ id: created.result.id, deleted: true });
  expect((await loadVault(root)).tables.get('jira.board_quick_filters')).toHaveLength(0);
});
