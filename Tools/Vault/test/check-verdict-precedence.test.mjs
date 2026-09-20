// ZEN-76: for `check`, the DURABLE verdict outranks the TRANSIENT repo lock.
//
// `check` is read-only and answers "would a claim take this ticket?". The
// exit-code ladder used to test the I5 one-ticket-per-repo lock BEFORE the
// contract, so while any ticket was in flight it answered 5 (repo busy) for
// an EPIC, a needs-human ticket or a contract failure — reporting the one
// obstacle that clears by itself and hiding the one that does not.
//
// Measured during the /tick audit: `zagent check ZEN-1` on an EPIC returned 5
// with a ticket in flight, so the tick protocol's own claim that "an EPIC
// comes back exit 4 from `zagent check`" was false in exactly the situation a
// running loop is always in — it holds a ticket by definition.
//
// `claim` and `next` are deliberately NOT changed: they mutate, and refusing
// to steal an in-flight ticket outranks everything. The last test pins that,
// so a future "simplification" that unifies the two ladders fails here.
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchRead } from '../cli/read.js';
import { dispatchWrite } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
  - {from_status_id: progress, to_status_id: done}
---
`;

const AGENT = 'agent@example.test';

function ticket({ key, type = 'TASK', status = 'To Do', assignee = 'null', labels = '[]' }) {
  return `---
key: ${key}
type: ${type}
title: ${key}
status: "${status}"
assignee: ${assignee}
category: Engine
complexity: SIMPLE
risk: LOW
labels: ${labels}
---

## Goal

Something.

## Definition of Done

- [ ] Something observable.
`;
}

// `inFlight: true` puts FD-1 In Progress and assigned to the agent, which is
// what holds the one-ticket-per-repo lock.
async function makeVault(t, { inFlight }) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-check-precedence-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: [],
    writes: { enabled: true, commit: false, agentEmail: AGENT },
    policy: {
      board: { agentEmail: AGENT, guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 } },
      client: { file: { project: 'FD', baseBranch: 'master', branching: 'direct', merge: { push: false } } },
      resolved: {
        routing: {}, guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 },
        // A gate line is required for the contract to pass at all: a project
        // with none refuses every ticket with "nothing could verify the work".
        projects: { FD: { repo: root, baseBranch: 'master', branching: 'direct', gates: ['echo gate'], merge: { push: false }, reviewOn: [] } },
      },
    },
  };
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify(config, null, 2));
  await fs.writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await fs.writeFile(
    path.join(root, 'FD', 'tickets', 'FD-1.md'),
    inFlight
      ? ticket({ key: 'FD-1', status: 'In Progress', assignee: AGENT })
      : ticket({ key: 'FD-1' }),
  );
  await fs.writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), ticket({ key: 'FD-2', type: 'EPIC' }));
  await fs.writeFile(path.join(root, 'FD', 'tickets', 'FD-3.md'), ticket({ key: 'FD-3' }));
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Check precedence tests']);
  await git(root, ['config', 'user.email', 'check-precedence@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

const check = async (root, key) => dispatchRead(await loadVault(root), ['check', key]);
const claim = async (root, key) => dispatchWrite(await loadVault(root), ['claim', key], {});

test('an EPIC checks as 4 even while another ticket holds the repo lock', async t => {
  const root = await makeVault(t, { inFlight: true });
  const res = await check(root, 'FD-2');
  assert.equal(res.exitCode, 4, 'the durable verdict, not repo-busy');
  assert.equal(res.payload.check.durable.blocked, true);
  assert.match(res.payload.check.durable.reason, /EPIC/);
  // The lock is still REPORTED — it is hidden from the exit code, not erased.
  assert.equal(res.payload.check.inFlight, 'FD-1');
});

test('the same EPIC checks as 4 with nothing in flight, so the code is stable', async t => {
  const root = await makeVault(t, { inFlight: false });
  const res = await check(root, 'FD-2');
  assert.equal(res.exitCode, 4);
  assert.equal(res.payload.check.inFlight, null);
});

test('a perfectly claimable ticket still reports repo-busy as 5', async t => {
  // The carve-out is only for tickets with their OWN reason to be refused.
  // FD-3 has none, so the lock is the whole answer and must still surface.
  const root = await makeVault(t, { inFlight: true });
  const res = await check(root, 'FD-3');
  assert.equal(res.exitCode, 5, 'nothing durable is wrong with FD-3');
  assert.equal(res.payload.check.durable.blocked, false);
  assert.equal(res.payload.check.inFlight, 'FD-1');
});

test('a claimable ticket checks as 0 once the lock clears', async t => {
  const root = await makeVault(t, { inFlight: false });
  const res = await check(root, 'FD-3');
  assert.equal(res.exitCode, 0);
});

test('claim is UNCHANGED: the lock still outranks a durable refusal', async t => {
  // check is read-only; claim mutates, so refusing to steal comes first there.
  // If someone ever unifies the two ladders, this is what goes red.
  const root = await makeVault(t, { inFlight: true });
  const res = await claim(root, 'FD-2');
  assert.equal(res.exitCode, 5, 'claim reports repo-busy, not the contract');
});
