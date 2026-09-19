import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';

import {
  evaluateContract,
  queueEntry,
} from '../cli/compat.mjs';
import { dispatchRead } from '../cli/read.js';
import { loadVault } from '../server/main.js';

const fixtureRoot = fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url));

function syntheticTicket(template, key, fields = {}, body = template.bodyText) {
  const record = structuredClone(template);
  const number = Number(key.split('-').at(-1));
  record.project = 'FD';
  record.relativePath = `FD/tickets/${key}.md`;
  record.metadata = {
    ...record.metadata,
    key,
    title: `Synthetic ${key}`,
    type: 'TASK',
    status: 'To Do',
    statusCategory: 'TODO',
    assignee: null,
    category: 'Foundry',
    parent: null,
    sprint: null,
    labels: [],
    blocks: [],
    links: [],
    position: number,
    ...fields,
    source: {
      ...record.metadata.source,
      id: `synthetic-${key}`,
      number,
    },
  };
  record.bodyText = body;
  return record;
}

test('queue claim filters cover the independent board predicates', async () => {
  const index = await loadVault(fixtureRoot);
  const template = index.getTicket('FD-11');
  assert.ok(template, 'fixture must provide a canonical Foundry ticket body');

  const cases = [
    { name: 'unassigned', key: 'FD-9001', fields: {}, claimable: true, assigned: false },
    {
      name: 'primary agent assigned',
      key: 'FD-9002',
      fields: { assignee: 'agent@example.invalid' },
      claimable: false,
      assigned: true,
    },
    {
      name: 'other agent assigned',
      key: 'FD-9003',
      fields: { assignee: 'other@example.test' },
      claimable: false,
      assigned: true,
    },
    {
      name: 'epic',
      key: 'FD-9004',
      fields: { type: 'EPIC' },
      claimable: false,
      assigned: false,
    },
  ];
  const records = cases.map(({ key, fields }) => syntheticTicket(template, key, fields));

  const inProgress = syntheticTicket(template, 'FD-9005', {
    status: 'In Progress',
    statusCategory: 'IN_PROGRESS',
  });
  const openBlocker = syntheticTicket(template, 'FD-9006', {
    title: 'Open blocker',
  });
  openBlocker.metadata.blocks = [{ key: 'FD-9007', reason: 'must land first' }];
  const openBlocked = syntheticTicket(template, 'FD-9007');
  const doneBlocker = syntheticTicket(template, 'FD-9008', {
    status: 'Done',
    statusCategory: 'DONE',
  });
  doneBlocker.metadata.blocks = [{ key: 'FD-9009', reason: 'already landed' }];
  const doneBlocked = syntheticTicket(template, 'FD-9009');
  const malformed = syntheticTicket(template, 'FD-9010', {}, 'A body without the required contract sections.');
  records.push(inProgress, openBlocker, openBlocked, doneBlocker, doneBlocked, malformed);
  for (const record of records) index.tickets.set(record.metadata.key, record);

  for (const row of cases) {
    const entry = queueEntry(index, index.getTicket(row.key));
    assert.equal(entry.assigned, row.assigned, `${row.name} assigned predicate`);
    assert.equal(entry.claimable, row.claimable, `${row.name} claimable predicate`);
  }

  const openEntry = queueEntry(index, openBlocked);
  assert.deepEqual(openEntry.blockedBy, ['FD-9006']);
  assert.equal(openEntry.claimable, false, 'an open blocker suppresses claiming');

  const doneEntry = queueEntry(index, doneBlocked);
  assert.deepEqual(doneEntry.blockedBy, [], 'a completed blocker is removed from the queue edge');
  assert.equal(doneEntry.claimable, true, 'a completed blocker does not suppress claiming');

  const malformedContract = evaluateContract(index, malformed, 'FD', 'Foundry');
  assert.equal(malformedContract.valid, false, 'malformed contract must fail targeted validation');
  assert.equal(
    queueEntry(index, malformed).claimable,
    true,
    'queue still exposes malformed work so an operator can repair it',
  );

  const queue = dispatchRead(index, ['queue', '--project', 'FD']);
  assert.equal(queue.exitCode, 0);
  assert.ok(queue.payload.some(row => row.key === 'FD-9001'));
  assert.ok(!queue.payload.some(row => row.key === 'FD-9005'), 'non To Do status is excluded from queue');
});
