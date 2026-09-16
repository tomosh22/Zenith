import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { test } from 'vitest';

import { dispatchHistoryRead } from '../cli/history.mjs';
import { loadVault } from '../server/main.js';

const fixtureRoot = new URL('../fixtures/snapshot/', import.meta.url);

test('history dispatcher covers every frozen reference case', async () => {
  const index = await loadVault(new URL('vault/', fixtureRoot));
  const reference = JSON.parse(await readFile(new URL('reference.json', fixtureRoot), 'utf8'));
  const cases = reference.cases.filter(caseValue => caseValue.argv?.[0] === 'history');
  assert.equal(cases.length, 251);

  for (const [number, caseValue] of cases.entries()) {
    const actual = dispatchHistoryRead(index, caseValue.argv);
    assert.ok(actual, `history case ${number} must dispatch`);
    assert.equal(actual.exitCode, caseValue.exitCode, `history case ${number} exit code`);
    assert.deepEqual(actual.payload, caseValue.payload, `history case ${number} payload`);
  }
});

test('history sorts complete microseconds before formatting and keeps actor names', () => {
  const index = {
    tickets: new Map([
      ['FD-1', { metadata: { key: 'FD-1', source: { id: 'issue-1' } } }],
    ]),
    history: new Map([
      ['FD/history/pre-migration.md', {
        fields: {
          kind: 'history',
          rows: [
            {
              id: 'older',
              issue_id: 'issue-1',
              field: 'status',
              from_value: 'In Progress',
              to_value: 'Done',
              actor_id: 'user-1',
              created_at: '2026-01-02T03:04:05.123456',
            },
            {
              id: 'newer',
              issue_id: 'issue-1',
              field: 'status',
              from_value: 'To Do',
              to_value: 'In Progress',
              actor_id: 'user-1',
              created_at: '2026-01-02T03:04:05.123457',
            },
            {
              id: 'z-tie',
              issue_id: 'issue-1',
              field: 'assignee',
              from_value: null,
              to_value: 'the agent',
              actor_id: 'user-1',
              created_at: '2026-01-02T03:04:05.223456',
            },
            {
              id: 'a-tie',
              issue_id: 'issue-1',
              field: 'status',
              from_value: 'In Progress',
              to_value: 'To Do',
              actor_id: 'user-1',
              created_at: '2026-01-02T03:04:05.223456',
            },
          ],
        },
      }],
    ]),
    tables: new Map([
      ['public.users', [{ id: 'user-1', name: 'Actor Name', email: 'actor@example.test' }]],
    ]),
    metadataFiles: new Map([
      ['metadata.md', {
        fields: {
          kind: 'metadata',
          ordering: { history: { 'issue-1': ['a-tie', 'z-tie'] } },
        },
      }],
    ]),
  };

  const result = dispatchHistoryRead(index, ['history', 'FD-1']);
  assert.equal(result.exitCode, 0);
  assert.deepEqual(result.payload.map(row => row.field), ['status', 'assignee', 'status', 'status']);
  assert.equal(result.payload[0].createdAt, '2026-01-02T03:04:05.223Z');
  assert.equal(result.payload[1].createdAt, '2026-01-02T03:04:05.223Z');
  assert.equal(result.payload[2].createdAt, '2026-01-02T03:04:05.123Z');
  assert.equal(result.payload[3].createdAt, '2026-01-02T03:04:05.123Z');
  assert.deepEqual(result.payload.slice(0, 2).map(row => row.toValue), ['To Do', 'the agent']);
  assert.ok(result.payload.every(row => row.actor === 'Actor Name'));
});

test('an empty frozen history file does not fall back to a stale activity table', () => {
  const index = {
    tickets: new Map([
      ['FD-1', { metadata: { key: 'FD-1', source: { id: 'issue-1' } } }],
    ]),
    history: new Map([
      ['FD/history/pre-migration.md', { fields: { kind: 'history', rows: [] } }],
    ]),
    tables: new Map([
      ['jira.issue_activity', [{
        id: 'stale',
        issue_id: 'issue-1',
        field: 'status',
        from_value: 'To Do',
        to_value: 'Done',
        created_at: '2026-01-02T03:04:05.123456',
      }]],
    ]),
  };

  assert.deepEqual(dispatchHistoryRead(index, ['history', 'FD-1']), {
    exitCode: 0,
    payload: [],
  });
});

test('non-history commands are left for the parent dispatcher', () => {
  assert.equal(dispatchHistoryRead({}, ['show', 'FD-1']), null);
});
