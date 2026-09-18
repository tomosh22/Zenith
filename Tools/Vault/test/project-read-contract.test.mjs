import { beforeAll, expect, test } from 'vitest';
import { fileURLToPath } from 'node:url';
import { loadVault } from '../server/main.js';
import { projectDetail, projectList, issueDetail } from '../server/project-api.mjs';
let index;
beforeAll(async () => { index = await loadVault(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url))); });

test('projects expose only their own issues and stable ordered workflow identities', () => {
  for (const root of index.boardRoots) {
    const project = projectDetail(index, root.project);
    expect(project.tickets.every(ticket => ticket.project === root.project)).toBe(true);
    expect(new Set(project.statuses.map(status => status.id)).size).toBe(project.statuses.length);
    expect(project.statuses.map(status => status.position)).toEqual(project.statuses.map(status => status.position).sort((a, b) => a - b));
    expect(project.readOnly).toBe(true);
  }
  expect(projectList(index).projects.some(project => project.key === 'ZEN')).toBe(true);
});

test('config revision map includes the metadata supplying board settings', () => {
  const project = projectDetail(index, 'ZEN');
  expect(project.revisions['metadata.md']).toMatchObject({ hash: index.getFile('metadata.md').hash });
  expect(project.revisions['ZEN/workflow.md']).toMatchObject({ hash: index.getFile('ZEN/workflow.md').hash });
});

test('user credentials cannot escape through project or comment identities', () => {
  const original = index.tables.get('public.users');
  index.tables.set('public.users', original.map(user => ({ ...user, password_hash: 'credential-sentinel', session_token: 'credential-sentinel' })));
  try {
    const project = projectDetail(index, 'ZEN');
    expect(project.users.length).toBeGreaterThan(0);
    expect(JSON.stringify(project)).not.toContain('credential-sentinel');
    expect(JSON.stringify(issueDetail(index, 'ZEN-1'))).not.toContain('credential-sentinel');
  } finally { index.tables.set('public.users', original); }
});

test('workflow rules are joined through transitions rather than a nonexistent project column', () => {
  const transition = projectDetail(index, 'ZEN').workflow.transitions[0];
  const original = index.tables.get('jira.workflow_transition_rules');
  index.tables.set('jira.workflow_transition_rules', [{ id: 'rule-test', transition_id: transition.id, kind: 'VALIDATOR', type: 'COMMENT_REQUIRED', config: {} }]);
  try {
    expect(projectDetail(index, 'ZEN').workflow.transitions.find(row => row.id === transition.id).rules).toEqual(expect.arrayContaining([expect.objectContaining({ id: 'rule-test', type: 'COMMENT_REQUIRED' })]));
    expect(projectDetail(index, 'DP').workflow.transitions.flatMap(row => row.rules)).not.toEqual(expect.arrayContaining([expect.objectContaining({ id: 'rule-test' })]));
  } finally { index.tables.set('jira.workflow_transition_rules', original); }
});

test('custom options and ordered screen fields survive relational-to-document projection', () => {
  const projectId = index.tables.get('jira.projects').find(row => row.key === 'ZEN').id;
  const injected = {
    'jira.custom_fields': [{ id: 'field-test', project_id: projectId, key: 'flavour', name: 'Flavour', type: 'SELECT' }],
    'jira.custom_field_options': [{ id: 'option-b', field_id: 'field-test', value: 'Beta', position: 1 }, { id: 'option-a', field_id: 'field-test', value: 'Alpha', position: 0 }],
    'jira.screens': [{ id: 'screen-test', project_id: projectId, name: 'Review' }],
    'jira.screen_fields': [{ id: 'screen-field-b', screen_id: 'screen-test', custom_field_id: 'field-test', builtin_field: null, position: 1 }, { id: 'screen-field-a', screen_id: 'screen-test', builtin_field: 'description', custom_field_id: null, position: 0 }],
  };
  const original = new Map(Object.keys(injected).map(key => [key, index.tables.get(key)]));
  for (const [key, value] of Object.entries(injected)) index.tables.set(key, value);
  try {
    const project = projectDetail(index, 'ZEN');
    expect(project.customFields.find(field => field.id === 'field-test').options).toEqual(['Alpha', 'Beta']);
    expect(project.screens.find(screen => screen.id === 'screen-test').fields).toEqual([
      expect.objectContaining({ builtinField: 'description', customFieldId: null }),
      expect.objectContaining({ builtinField: null, customFieldId: 'field-test' }),
    ]);
    expect(projectDetail(index, 'DP').customFields.some(field => field.id === 'field-test')).toBe(false);
  } finally { for (const [key, value] of original) index.tables.set(key, value); }
});
