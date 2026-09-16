import { test } from 'vitest';
import assert from 'node:assert/strict';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { canonicalOf } from '../cli/compat.mjs';
import {
  dispatchLinks,
  dispatchRows,
  dispatchSprint,
  dispatchSuggestions,
  dispatchVersion,
} from '../cli/queries.mjs';

const fixtureRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../fixtures/snapshot/vault');

async function fixture() {
  return loadVault(fixtureRoot);
}

test('links uses source-owned direction and exact inverse labels', async () => {
  const index = await fixture();
  const rows = dispatchLinks(index, ['links', 'FD-11']);
  assert.equal(rows.exitCode, 0);
  assert.ok(rows.payload.length > 0);
  assert.ok(rows.payload.every(row => row.direction === 'outgoing'));
  assert.ok(rows.payload.every(row => row.label === 'blocks'));

  const incoming = dispatchLinks(index, ['links', 'FD-14']).payload.find(row => row.direction === 'incoming');
  assert.equal(incoming?.key, 'FD-11');
  assert.equal(incoming?.label, 'is blocked by');

  const ticket = index.tickets.get('FD-11');
  const originalBlocks = ticket.metadata.blocks;
  const originalLinks = ticket.metadata.links;
  ticket.metadata.blocks = originalBlocks.filter(row => row.key !== 'FD-12');
  ticket.metadata.links = [...originalLinks, { key: 'FD-3', type: 'RELATES_TO', reason: 'new canonical edge' }];
  const edited = dispatchLinks(index, ['links', 'FD-11']).payload;
  assert.equal(edited.some(row => row.key === 'FD-12'), false);
  assert.deepEqual(edited.find(row => row.key === 'FD-3'), {
    id: null,
    type: 'RELATES_TO',
    direction: 'outgoing',
    key: 'FD-3',
    title: index.tickets.get('FD-3').metadata.title,
    status: index.tickets.get('FD-3').metadata.status,
    statusCategory: index.tickets.get('FD-3').metadata.statusCategory,
    reason: 'new canonical edge',
    label: 'relates to',
  });
  ticket.metadata.blocks = originalBlocks;
  ticket.metadata.links = originalLinks;
});

test('sprint aggregates read canonical sprint references', async () => {
  const index = await fixture();
  const ticket = index.tickets.get('FD-100');
  const before = dispatchSprint(index, ['sprint', 'list', '--project', 'FD']);
  const sprintRecord = index.sprints.get(ticket.metadata.sprint);
  const sprintId = canonicalOf(sprintRecord).source.id;
  const sprint = before.payload.find(row => row.id === sprintId);
  assert.ok(sprint);

  const original = ticket.metadata.sprint;
  ticket.metadata.sprint = null;
  const after = dispatchSprint(index, ['sprint', 'list', '--project', 'FD']);
  assert.equal(after.payload.find(row => row.id === sprintId)?.issues, sprint.issues - 1);
  ticket.metadata.sprint = original;
  const id=sprintRecord.metadata.source.id;
  sprintRecord.metadata.name='Edited sprint';
  sprintRecord.metadata.status='COMPLETED';
  sprintRecord.metadata.startDate=null;
  sprintRecord.metadata.gate=null;
  const changed=dispatchSprint(index,['sprint','list','--project','FD']).payload.find(row=>row.id===id);
  assert.equal(changed.name,'Edited sprint');
  assert.equal(changed.status,'COMPLETED');
  assert.equal(changed.startDate,null);
  assert.equal(changed.gate,null);
});

test('version aggregates read canonical fixVersions references', async () => {
  const index = await fixture();
  const ticket = index.tickets.get('FD-100');
  const before = dispatchVersion(index, ['version', 'list', '--project', 'FD']);
  const version = before.payload.find(row => row.name === 'Feature Complete');
  assert.equal(version?.issues, 40);

  const original = ticket.metadata.fixVersions;
  ticket.metadata.fixVersions = [];
  const after = dispatchVersion(index, ['version', 'list', '--project', 'FD']);
  assert.equal(after.payload.find(row => row.name === 'Feature Complete')?.issues, 39);
  ticket.metadata.fixVersions = ['Feature Complete'];
  const added = dispatchVersion(index, ['version', 'list', '--project', 'FD']);
  assert.equal(added.payload.find(row => row.name === 'Feature Complete')?.issues, 40);
  ticket.metadata.fixVersions = original;
});

test('sprint and version list use configured default project when --project is omitted', async () => {
  const index = await fixture();
  const policy = index.vaultConfig.policy;
  const original = policy.client.file.project;
  policy.client.file.project = 'FD';
  assert.deepEqual(
    dispatchSprint(index, ['sprint', 'list']).payload,
    dispatchSprint(index, ['sprint', 'list', '--project', 'FD']).payload,
  );
  assert.deepEqual(
    dispatchVersion(index, ['version', 'list']).payload,
    dispatchVersion(index, ['version', 'list', '--project', 'FD']).payload,
  );
  policy.client.file.project = original;
});

test('rows resolves SELECT ids, filters by property, and applies limit', async () => {
  const index = await fixture();
  const rows = dispatchRows(index, ['rows', 'questions', '--where', 'Status=RESOLVED', '--limit', '1']);
  assert.equal(rows.exitCode, 0);
  assert.equal(rows.payload.length, 1);
  assert.equal(rows.payload[0].values.Status, 'resolved');
  assert.equal(rows.payload[0].values['Cost if wrong'], 'low');
  assert.match(rows.payload[0].url, /\/database\//);
  assert.throws(()=>dispatchRows(index,['rows','questions','--where','Status=missing','--limit','invalid']),/^Error: --limit must be a whole number/);
});

test('suggestions defaults to new and --all includes resolved rows', async () => {
  const index = await fixture();
  const open = dispatchSuggestions(index, ['suggestions']);
  const all = dispatchSuggestions(index, ['suggestions', '--all']);
  assert.equal(open.payload.length, 14);
  assert.equal(all.payload.length, 31);
  assert.ok(open.payload.every(row => row.values.Status === 'new'));
  assert.ok(all.payload.some(row => row.values.Status === 'done'));
});
