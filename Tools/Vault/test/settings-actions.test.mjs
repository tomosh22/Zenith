import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchSettingsAction } from '../server/settings-actions.mjs';
import { bytesOf, patchRecord } from '../cli/write.mjs';

let root;

async function revisions(...paths) {
  return Object.fromEntries(await Promise.all(paths.map(async relative => [relative, await readRevision(root, relative)])));
}

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
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-settings-actions-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await setupMetadataFixture();
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Settings test']);
  await git(root, ['config', 'user.email', 'settings@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
});

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

test('settings actions preserve imported rows and clean field references atomically', async () => {
  let index = await loadVault(root);
  const created = await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'customer-tier', name: 'Customer tier', type: 'SELECT', options: ['Gold', 'Silver'] },
    revisions: await revisions('metadata.md'),
    operationId: 'settings-field-create',
  });
  expect(created.result).toMatchObject({ action: 'field.create', project: 'ZEN', created: true });
  const fieldId = created.result.id;

  index = await loadVault(root);
  const metadataRevision = await revisions('metadata.md');
  const required = await dispatchSettingsAction(index, {
    action: 'field.requirement.set',
    project: 'ZEN',
    data: { issueType: 'TASK', customFieldId: fieldId, builtinField: null, requirement: 'REQUIRED' },
    revisions: metadataRevision,
    operationId: 'settings-field-required',
  });
  expect(required.result.updated).toBe(true);

  index = await loadVault(root);
  const screen = await dispatchSettingsAction(index, {
    action: 'screen.create',
    project: 'ZEN',
    data: { name: 'Review screen', fields: [{ builtinField: 'assignee', customFieldId: null }] },
    revisions: await revisions('metadata.md'),
    operationId: 'settings-screen-create',
  });
  expect(screen.result.created).toBe(true);
  const screenId = screen.result.id;

  index = await loadVault(root);
  await dispatchSettingsAction(index, {
    action: 'screen.fields.set',
    project: 'ZEN',
    data: { id: screenId, fields: [{ builtinField: 'description', customFieldId: null }, { builtinField: null, customFieldId: fieldId }] },
    revisions: await revisions('metadata.md'),
    operationId: 'settings-screen-fields',
  });

  index = await loadVault(root);
  const beforeDelete = await revisions('metadata.md', 'ZEN/workflow.md');
  await dispatchSettingsAction(index, {
    action: 'field.delete',
    project: 'ZEN',
    data: { id: fieldId },
    revisions: beforeDelete,
    operationId: 'settings-field-delete',
  });

  index = await loadVault(root);
  const tables = index.tables;
  const fields = tables.get('jira.custom_fields');
  expect(fields.some(row => row.id === 'imported-field')).toBe(true);
  expect(fields.some(row => row.id === fieldId)).toBe(false);
  expect(tables.get('jira.custom_field_options').some(row => row.field_id === fieldId)).toBe(false);
  expect(tables.get('jira.field_configurations').some(row => row.custom_field_id === fieldId)).toBe(false);
  expect(tables.get('jira.screen_fields').some(row => row.custom_field_id === fieldId)).toBe(false);
  expect(tables.get('jira.screens').some(row => row.id === 'imported-screen')).toBe(true);
});

test('field and board actions require an opened revision and preserve foreign project rows', async () => {
  const index = await loadVault(root);
  const stale = await revisions('metadata.md');
  await dispatchSettingsAction(index, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'one-shot', name: 'One shot', type: 'TEXT' },
    revisions: stale,
    operationId: 'settings-stale-seed',
  });
  const changed = await loadVault(root);
  await expect(dispatchSettingsAction(changed, {
    action: 'field.create',
    project: 'ZEN',
    data: { key: 'must-reject', name: 'Must reject', type: 'TEXT' },
    revisions: stale,
    operationId: 'settings-stale-write',
  })).rejects.toMatchObject({ code: 'STALE' });

  const boardIndex = await loadVault(root);
  const board = boardIndex.tables.get('jira.boards').find(row => row.project_id === boardIndex.tables.get('jira.projects').find(row => row.key === 'ZEN').id);
  const boardRevision = await revisions('metadata.md');
  await dispatchSettingsAction(boardIndex, {
    action: 'board.settings.update',
    project: 'ZEN',
    data: { type: 'SCRUM', swimlaneBy: 'EPIC', cardFields: ['key', 'priority'], filter: { type: 'BUG' } },
    revisions: boardRevision,
    operationId: 'settings-board-update',
  });
  const afterBoard = await loadVault(root);
  const boardAfter = afterBoard.tables.get('jira.boards').find(row => row.id === board.id);
  expect(boardAfter.type).toBe('SCRUM');
  expect(boardAfter.swimlane_by).toBe('EPIC');
  expect(boardAfter.card_fields).toEqual(['key', 'priority']);
  expect(boardAfter.filter).toEqual({ type: 'BUG' });

  const columnsBefore = afterBoard.tables.get('jira.board_columns').filter(row => row.board_id === board.id);
  const created = await dispatchSettingsAction(afterBoard, {
    action: 'board.column.create',
    project: 'ZEN',
    data: { name: 'Custom lane' },
    revisions: await revisions('metadata.md'),
    operationId: 'settings-column-create',
  });
  expect(created.result.created).toBe(true);
  const final = await loadVault(root);
  expect(final.tables.get('jira.board_columns').filter(row => row.board_id === board.id)).toHaveLength(columnsBefore.length + 1);
  expect(final.tables.get('jira.boards').some(row => row.project_id !== board.project_id && row.type === 'SCRUM' && row.id !== board.id)).toBe(false);
});

test('workflow rules use the shared validator and screen attachment is project scoped', async () => {
  let index = await loadVault(root);
  const workflow = index.configs.get('ZEN/workflow.md');
  const transition = workflow.fields.transitions[0];
  await expect(dispatchSettingsAction(index, {
    action: 'workflow.transition.rule.create',
    project: 'ZEN',
    data: { transitionId: transition.id, kind: 'VALIDATOR', type: 'SET_RESOLUTION', config: {} },
    revisions: await revisions('metadata.md', 'ZEN/workflow.md'),
    operationId: 'settings-invalid-rule',
  })).rejects.toMatchObject({ code: 'INVALID' });

  const screen = index.tables.get('jira.screens').find(row => row.id === 'imported-screen');
  await dispatchSettingsAction(index, {
    action: 'workflow.transition.screen',
    project: 'ZEN',
    data: { transitionId: transition.id, screenId: screen.id },
    revisions: await revisions('ZEN/workflow.md'),
    operationId: 'settings-screen-attach',
  });
  const after = await loadVault(root);
  const edge = after.configs.get('ZEN/workflow.md').fields.transitions.find(row => row.id === transition.id);
  expect(edge.screen_id).toBe(screen.id);
});
