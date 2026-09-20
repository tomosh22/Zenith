/*
 * A REFUSED CLAIM MUST CHANGE NOTHING.
 *
 * The SQL board mutated before it validated: a targeted claim on a
 * `needs-human` ticket wrote status To Do -> In Progress and assignee
 * none -> the agent, and only THEN returned exit 4 refusing it.  The local
 * Vault CLI is a different implementation, and this file is the standing
 * proof that it does not share the defect: `claim`'s validation runs inside
 * the `runOperation` prepare callback, before any byte change is produced,
 * and `executeOperation` writes and commits only what a prepare RETURNS.
 *
 * Every refusal shape below asserts the whole observable surface, not just
 * the two fields the original report named:
 *   - the ticket file bytes,
 *   - `status`, `assignee` and `claimAttempt` as the loader sees them,
 *   - every other file in the vault work tree,
 *   - git HEAD, the commit subjects, and `git status --porcelain`.
 *
 * The git assertions are the ones that distinguish "validated first" from
 * "mutated, then rolled back", which is what the suggestion was really
 * about — `Restore-RefusedClaim` no longer exists, so there is no client
 * side net behind this.  `writes.commit` is therefore TRUE in this fixture:
 * a successful claim here really does write a `board: claim` commit, which
 * the last test asserts so the probe cannot pass vacuously.
 *
 * Out of scope by construction: the operation lock (`refs/vault/operation-lock`)
 * and `.git/vault/authority.json` are taken before `prepare` runs and released
 * after it throws.  Neither is a board field, a work-tree file or a commit, and
 * neither is in the snapshot below.  `executeOperation` also runs crash
 * recovery before `prepare`, which can finish a PREVIOUS operation's journal —
 * that is another operation's already-validated bytes, never this claim's, and
 * these fixtures have no interrupted journal to recover.
 */

import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchWrite } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: blocked, name: Blocked, category: TODO}
  - {id: review, name: In Review, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
  - {from_status_id: progress, to_status_id: blocked}
  - {from_status_id: progress, to_status_id: review}
  - {from_status_id: progress, to_status_id: done}
  - {from_status_id: progress, to_status_id: todo}
  - {from_status_id: blocked, to_status_id: progress}
  - {from_status_id: review, to_status_id: progress}
  - {from_status_id: review, to_status_id: done}
  - {from_status_id: done, to_status_id: progress}
---
`;

const BODY = `## Goal
Exercise the local writer.

## Definition of Done
- [ ] The isolated mutation path is observable

## Gates
- node --version
`;

function ticket({ id, key, title = key, type = 'TASK', status = 'To Do', assignee = null, labels = [], blocks = [] }) {
  const fields = {
    id, key, type, title, category: 'Local', parent: null, sprint: null, status, assignee,
    priority: 'MEDIUM', complexity: 'SIMPLE', risk: 'LOW', storyPoints: 1,
    position: Number(key.split('-').at(-1)), labels, fixVersions: [], blocks, links: [],
    source: { id: `source-${id}`, title: `source ${title}`, updated_at: null },
  };
  return `---\n${Object.entries(fields).map(([name, value]) => `${name}: ${JSON.stringify(value)}`).join('\n')}\n---\n${BODY}`;
}

// FD-1 is the one cleanly claimable ticket: the control test claims it, and
// its presence keeps the other cases from passing merely because the board
// is empty.  FD-4 blocks FD-3 and is itself To Do, so the block is open.
const DEFAULT_SPECS = [
  { id: 'one', key: 'FD-1', title: 'One' },
  { id: 'two', key: 'FD-2', title: 'Needs a person', labels: ['needs-human'] },
  { id: 'three', key: 'FD-3', title: 'Successor' },
  { id: 'four', key: 'FD-4', title: 'Predecessor', blocks: [{ key: 'FD-3', reason: 'the schema lands first' }] },
  { id: 'epic', key: 'FD-9', title: 'Epic', type: 'EPIC' },
];

async function makeVault(t, specs = DEFAULT_SPECS) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-claim-atomicity-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await fs.mkdir(path.join(root, 'FD', 'sprints'), { recursive: true });
  await fs.mkdir(path.join(root, 'FD', 'knowledge'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: [],
    // commit: TRUE — a refusal that silently committed would otherwise be
    // indistinguishable from a refusal that wrote nothing.
    writes: { enabled: true, commit: true, agentEmail: 'agent@example.test' },
    policy: {
      board: { agentEmail: 'agent@example.test', guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 } },
      client: { file: { project: 'FD', baseBranch: 'master', branching: 'direct', merge: { push: false } } },
      resolved: {
        routing: {}, guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 },
        projects: { FD: { repo: root, baseBranch: 'master', branching: 'direct', gates: [], merge: { push: false }, reviewOn: [] } },
      },
    },
  };
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify(config, null, 2));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await fs.writeFile(path.join(root, 'FD', 'versions.md'), `---\nkind: catalog\nrows:\n  - {id: v1, name: "1.0", status: UNRELEASED}\n---\n`);
  await fs.writeFile(path.join(root, 'FD', 'sprints', 'fd-s1.md'), `---\nkind: sprint\nslug: fd-s1\nname: S1\nstatus: PLANNED\n---\n`);
  for (const name of ['questions', 'decisions', 'shortfalls', 'changelog', 'suggestions']) {
    await fs.writeFile(path.join(root, 'FD', 'knowledge', `${name}.md`), `---\nkind: knowledge\ndatabase: ${name}\nrows: []\n---\n`);
  }
  for (const spec of specs) {
    await fs.writeFile(path.join(root, 'FD', 'tickets', `${spec.key}.md`), ticket(spec));
  }
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Claim atomicity tests']);
  await git(root, ['config', 'user.email', 'claim-atomicity@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

async function write(root, args, files = {}) {
  return dispatchWrite(await loadVault(root), args, files);
}

/** Every byte of the work tree plus the whole git-visible state. */
async function snapshot(root) {
  const files = {};
  const walk = async directory => {
    for (const entry of (await fs.readdir(directory, { withFileTypes: true })).sort((a, b) => a.name.localeCompare(b.name))) {
      if (entry.name === '.git') continue;
      const full = path.join(directory, entry.name);
      if (entry.isDirectory()) await walk(full);
      else files[path.relative(root, full).replaceAll('\\', '/')] = createHash('sha256').update(await fs.readFile(full)).digest('hex');
    }
  };
  await walk(root);
  return {
    files,
    head: await git(root, ['rev-parse', 'HEAD']),
    subjects: await git(root, ['log', '--format=%s']),
    status: await git(root, ['status', '--porcelain']),
  };
}

async function claimState(root, key) {
  const record = (await loadVault(root)).getTicket(key);
  return {
    status: record.metadata.status,
    assignee: record.metadata.assignee ?? null,
    claimAttempt: record.metadata.claimAttempt ?? null,
    hasAttemptToken: /claimAttempt/.test(record.rawText ?? String(record.rawBytes ?? '')),
  };
}

/**
 * The shared proof.  A refused claim must leave the named ticket's claim
 * fields exactly as they were, leave every file in the vault byte-identical,
 * and add no commit — `board: claim` in particular.
 */
async function assertRefusedClaimChangedNothing(root, key, args, { exitCode = 4, code = 'INVALID', error }) {
  const before = await snapshot(root);
  const stateBefore = await claimState(root, key);

  const result = await write(root, args);

  assert.equal(result.exitCode, exitCode, JSON.stringify(result.payload));
  assert.equal(result.payload.code, code, JSON.stringify(result.payload));
  assert.match(result.payload.error, error);

  const after = await snapshot(root);
  assert.deepEqual(after.files, before.files, 'a refused claim must not rewrite any vault file');
  assert.equal(after.head, before.head, 'a refused claim must not move HEAD');
  assert.equal(after.subjects, before.subjects, 'a refused claim must not add a commit');
  assert.ok(!/board: (claim|next)/.test(after.subjects), 'a refused claim must not leave a board: claim commit');
  assert.equal(after.status, before.status, 'a refused claim must not dirty the work tree');

  const stateAfter = await claimState(root, key);
  assert.deepEqual(stateAfter, stateBefore, 'a refused claim must not touch status, assignee or the attempt token');
  assert.equal(stateAfter.status, 'To Do');
  assert.equal(stateAfter.assignee, null);
  assert.equal(stateAfter.claimAttempt, null);
  assert.equal(stateAfter.hasAttemptToken, false, 'a refused claim must not write a claimAttempt token');
  return result;
}

test('a refused claim on a needs-human ticket writes no field and no commit', async t => {
  const root = await makeVault(t);
  await assertRefusedClaimChangedNothing(root, 'FD-2', ['claim', 'FD-2'], { error: /needs-human/ });
});

test('a refused claim on an EPIC writes no field and no commit', async t => {
  const root = await makeVault(t);
  // The epic has a second, independent guard downstream
  // (`validateTicketPolicies` refuses an epic reaching In Progress at all),
  // so this case asserts the CONTRACT refusal reached first: exit 4 with the
  // container wording, not the policy message.
  const result = await assertRefusedClaimChangedNothing(root, 'FD-9', ['claim', 'FD-9'], { error: /EPIC/ });
  assert.doesNotMatch(result.payload.error, /cannot be In Progress/, 'the contract check runs before any proposed-vault validation');
});

test('a refused claim on a ticket blocked by an open predecessor writes no field and no commit', async t => {
  const root = await makeVault(t);
  await assertRefusedClaimChangedNothing(root, 'FD-3', ['claim', 'FD-3'], { error: /blocked by FD-4 \(To Do\)/ });
  // The predecessor is untouched as well: nothing about the refusal reaches
  // the other side of the link.
  assert.deepEqual(await claimState(root, 'FD-4'), { status: 'To Do', assignee: null, claimAttempt: null, hasAttemptToken: false });
});

test('a queue claim with nothing claimable writes no field and no commit', async t => {
  const root = await makeVault(t, [
    { id: 'two', key: 'FD-2', title: 'Needs a person', labels: ['needs-human'] },
    { id: 'three', key: 'FD-3', title: 'Successor' },
    { id: 'four', key: 'FD-4', title: 'Predecessor', labels: ['needs-human'], blocks: [{ key: 'FD-3', reason: 'the schema lands first' }] },
    { id: 'epic', key: 'FD-9', title: 'Epic', type: 'EPIC' },
  ]);
  await assertRefusedClaimChangedNothing(root, 'FD-3', ['next', '--project', 'FD'], {
    exitCode: 3,
    code: 'EMPTY',
    error: /Nothing in "To Do"/,
  });
});

test('the atomicity probe is not vacuous: an ACCEPTED claim does write and commit', async t => {
  const root = await makeVault(t);
  const before = await snapshot(root);

  const claimed = await write(root, ['claim', 'FD-1']);
  assert.equal(claimed.exitCode, 0, JSON.stringify(claimed.payload));

  const after = await snapshot(root);
  assert.notDeepEqual(after.files, before.files, 'an accepted claim rewrites its ticket');
  assert.notEqual(after.head, before.head, 'an accepted claim moves HEAD');
  assert.match(after.subjects, /board: claim/, 'an accepted claim commits with the board: claim subject');
  assert.deepEqual(await claimState(root, 'FD-1'), {
    status: 'In Progress',
    assignee: 'agent@example.test',
    claimAttempt: claimed.payload.claimAttempt,
    hasAttemptToken: true,
  });
});
