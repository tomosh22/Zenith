// Tests for WP2 (parallel `/tick` lanes, second half): lane-scoped
// claim/next/check (I5 rescoped), Done <=> landed enforcement on lane rows,
// the `approved` human-gate closeout path, `--first-parent` commit
// resolution, and the new non-evidential gate exit codes 11/12. See
// `Tools/zagent/protocol/TICK.md`'s "Lane mode" section and the plan's
// "Parallel `/tick` lanes" section for the design; this file only exercises
// what WP2 owns (`cli/read.js`, `cli/write.mjs`, `cli/tick-checks.mjs`,
// `cli/compat.mjs`'s `semanticFlags`, and `cli/client.mjs`'s carrier/restore
// seams). `test/lanes.test.mjs` (WP1) covers mode resolution, `zagent lane
// which|list|add|remove`, and the carrier itself; this file builds on that
// and reuses the same small fixture shape.
import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';

import { runClient } from '../cli/client.mjs';
import { dispatchRead } from '../cli/read.js';
import { dispatchWrite } from '../cli/write.mjs';
import { canonicalOf } from '../cli/compat.mjs';
import { ticketCommitSha, gateVerdictFresh } from '../cli/tick-checks.mjs';
import { git } from '../server/operations.mjs';
import { loadVault } from '../server/main.js';
import { patchFields } from '../server/text-edit.mjs';

// ═══ fixture helpers (mirrors test/lanes.test.mjs's shape; duplicated
// rather than imported — the brief asks for new tests in a new file, and
// these are small enough that a shared import would cost more than it
// saves) ═══════════════════════════════════════════════════════════════

function ticketMd({ key, title, labels = [] }) {
  const labelsLine = labels.length ? `\nlabels: ${JSON.stringify(labels)}` : '';
  return `---\nkey: ${key}\ntitle: ${title}\nstatus: To Do${labelsLine}\n---\n`
    + '## Goal\nFixture ticket for lane-claim tests.\n\n## Definition of Done\n- [ ] n/a\n\n## Gates\n- echo ok\n';
}

async function fixture(t, { lanesEnabled = false, lanesRoot } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-lanes-claim-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.mkdir(path.join(root, 'ZM/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [
      { project: 'FD', path: 'FD' },
      { project: 'ZM', path: 'ZM' },
    ],
    writes: { enabled: true },
    policy: {
      board: { guardrails: { protectedPaths: [] } },
      resolved: {
        agentEmail: 'agent@zenith.invalid',
        guardrails: { protectedPaths: [] },
        projects: {
          FD: {
            repo: null, baseBranch: 'master', branching: 'direct', branchPrefix: 'agent/',
            gates: [], merge: { push: false }, conventions: '', cleanup: [], reviewOn: [],
            workerTools: 'Read,Edit,Write,Grep,Glob',
          },
          ZM: {
            repo: null, baseBranch: 'master', branching: 'direct', branchPrefix: 'agent/',
            gates: [], merge: { push: false }, conventions: '', cleanup: [], reviewOn: [],
            workerTools: 'Read,Edit,Write,Grep,Glob',
          },
        },
      },
    },
  }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-1.md'), ticketMd({ key: 'FD-1', title: 'One' }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-2.md'), ticketMd({ key: 'FD-2', title: 'Two' }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-3.md'), ticketMd({ key: 'FD-3', title: 'Three' }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-4.md'), ticketMd({ key: 'FD-4', title: 'Four' }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-5.md'), ticketMd({ key: 'FD-5', title: 'Gated', labels: ['human-gate'] }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-6.md'), ticketMd({ key: 'FD-6', title: 'Gated+approved', labels: ['human-gate', 'approved'] }));
  await fs.writeFile(path.join(root, 'ZM/tickets/ZM-1.md'), ticketMd({ key: 'ZM-1', title: 'One' }));
  await fs.writeFile(path.join(root, 'ZM/tickets/ZM-2.md'), ticketMd({ key: 'ZM-2', title: 'Two' }));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify({
    project: 'FD',
    baseBranch: 'master',
    lanes: { enabled: lanesEnabled, root: lanesRoot ?? path.join(root, '..', 'lanes-root'), integrateWaitMinutes: 240, buildSlotWaitMinutes: 90 },
  }));
  await git(root, ['init', '-q', '-b', 'master']);
  await git(root, ['config', 'user.email', 'vault-lanes-claim-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault lanes-claim tests']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'fixture']);
  return root;
}

async function commonDirOf(root) {
  return path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
}

async function addWorktree(t, root) {
  const dir = path.join(os.tmpdir(), `vault-lane-claim-worktree-${randomUUID()}`);
  await git(root, ['worktree', 'add', '--detach', dir, 'master']);
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', dir]); } catch { /* already removed */ } });
  return dir;
}

async function registerLane(root, project, lanePath) {
  const common = await commonDirOf(root);
  const file = path.join(common, 'vault', 'lanes.json');
  await fs.mkdir(path.dirname(file), { recursive: true });
  let registry = { lanes: {} };
  try { registry = JSON.parse(await fs.readFile(file, 'utf8')); } catch { /* first lane */ }
  registry.lanes = registry.lanes ?? {};
  const now = new Date().toISOString();
  registry.lanes[project] = { project, path: lanePath, createdAt: now, provisionedAt: now };
  await fs.writeFile(file, `${JSON.stringify(registry, null, 2)}\n`);
}

async function laneFieldOf(root, key) {
  const index = await loadVault(root);
  const record = [...index.tickets.values()].find(item => String(canonicalOf(item).key ?? '').toUpperCase() === key.toUpperCase());
  const lane = canonicalOf(record).lane;
  return lane === undefined ? null : lane;
}

async function plainRepo(t, prefix) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), prefix));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await git(root, ['init', '-q', '-b', 'master']);
  await git(root, ['config', 'user.email', `${prefix}@example.invalid`]);
  await git(root, ['config', 'user.name', prefix]);
  await fs.writeFile(path.join(root, 'seed.txt'), 'seed\n');
  await git(root, ['add', 'seed.txt']);
  await git(root, ['commit', '-qm', 'seed']);
  return root;
}

// ═══ I5 rescoped: main mode is unaffected (self-exclusion kept) ═══════════

test('main mode: an agent re-claims its own already-In-Progress ticket and succeeds (self-exclusion kept)', async t => {
  const root = await fixture(t);
  const first = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: root });
  assert.equal(first.exitCode, 0, JSON.stringify(first.payload));
  const second = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: root });
  assert.equal(second.exitCode, 0, JSON.stringify(second.payload));
});

// ═══ I5 rescoped: lane scope ═══════════════════════════════════════════

test('two lanes for different projects each claim their own project, sequentially, and each row carries its lane', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  const laneZM = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);
  await registerLane(root, 'ZM', laneZM);

  const claimFD = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(claimFD.exitCode, 0, JSON.stringify(claimFD.payload));
  const claimZM = await runClient(['--vault', root, 'claim', 'ZM-1', '--json'], { cwd: laneZM });
  assert.equal(claimZM.exitCode, 0, JSON.stringify(claimZM.payload));

  assert.equal(await laneFieldOf(root, 'FD-1'), 'FD');
  assert.equal(await laneFieldOf(root, 'ZM-1'), 'ZM');
});

test('scope rules: a main-scope in-flight row blocks any lane claim; a lane in-flight row blocks main; two lanes never block each other', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  const laneZM = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);
  await registerLane(root, 'ZM', laneZM);

  // A main-scope in-flight row blocks a lane claim, even for another project.
  const mainClaim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: root });
  assert.equal(mainClaim.exitCode, 0, JSON.stringify(mainClaim.payload));
  const blockedLane = await runClient(['--vault', root, 'claim', 'ZM-1', '--json'], { cwd: laneZM });
  assert.equal(blockedLane.exitCode, 5, JSON.stringify(blockedLane.payload));
  // The scope label names the CALLING invocation's own scope — lane ZM here
  // — not the holder's; the holder (FD-1, held at main scope) is what makes
  // this claim conflict at all.
  assert.match(blockedLane.payload.error, /\(lane ZM\)/);

  const release = await runClient(
    ['--vault', root, 'update', 'FD-1', '--assignee', 'none', '--attempt', mainClaim.payload.claimAttempt, '--json'],
    { cwd: root },
  );
  assert.equal(release.exitCode, 0, JSON.stringify(release.payload));

  // A lane in-flight row blocks main, regardless of project.
  const laneClaim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(laneClaim.exitCode, 0, JSON.stringify(laneClaim.payload));
  const blockedMain = await runClient(['--vault', root, 'claim', 'ZM-1', '--json'], { cwd: root });
  assert.equal(blockedMain.exitCode, 5, JSON.stringify(blockedMain.payload));
  assert.match(blockedMain.payload.error, /\(main checkout\)/); // scope label is the CALLER's own scope (main)

  // Two lanes never block each other.
  const otherLane = await runClient(['--vault', root, 'claim', 'ZM-1', '--json'], { cwd: laneZM });
  assert.equal(otherLane.exitCode, 0, JSON.stringify(otherLane.payload));
});

test('lane next yields only its project; the lane\'s own in-flight ticket exits 3 (EMPTY), never 5; --project mismatch exits 1 without claiming; a targeted claim of another project\'s key exits 1', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);

  const mismatch = await runClient(['--vault', root, 'next', '--project', 'ZM', '--json'], { cwd: laneFD });
  assert.equal(mismatch.exitCode, 1, JSON.stringify(mismatch.payload));
  assert.match(mismatch.payload.error, /does not match lane FD/);

  const targetedMismatch = await runClient(['--vault', root, 'claim', 'ZM-1', '--json'], { cwd: laneFD });
  assert.equal(targetedMismatch.exitCode, 1, JSON.stringify(targetedMismatch.payload));
  assert.match(targetedMismatch.payload.error, /belongs to ZM, not lane FD/);

  const next = await runClient(['--vault', root, 'next', '--json'], { cwd: laneFD });
  assert.equal(next.exitCode, 0, JSON.stringify(next.payload));
  assert.equal(next.payload.key, 'FD-1');
  assert.equal(await laneFieldOf(root, 'FD-1'), 'FD');

  const idleNext = await runClient(['--vault', root, 'next', '--json'], { cwd: laneFD });
  assert.equal(idleNext.exitCode, 3, JSON.stringify(idleNext.payload));
});

test('claim writes lane:; finish, --assignee none and a human move to To Do all clear it; a main-mode claim nulls a stale lane:', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);

  // claim writes lane:, and finish clears it.
  const claim1 = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(claim1.exitCode, 0, JSON.stringify(claim1.payload));
  assert.equal(await laneFieldOf(root, 'FD-1'), 'FD');
  const finish1 = await runClient(
    ['--vault', root, 'finish', 'FD-1', '--status', 'Blocked', '--attempt', claim1.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(finish1.exitCode, 0, JSON.stringify(finish1.payload));
  assert.equal(await laneFieldOf(root, 'FD-1'), null);

  // --assignee none clears it.
  const claim2 = await runClient(['--vault', root, 'claim', 'FD-2', '--json'], { cwd: laneFD });
  assert.equal(claim2.exitCode, 0, JSON.stringify(claim2.payload));
  assert.equal(await laneFieldOf(root, 'FD-2'), 'FD');
  const unassign = await runClient(
    ['--vault', root, 'update', 'FD-2', '--assignee', 'none', '--attempt', claim2.payload.claimAttempt, '--json'],
    { cwd: laneFD },
  );
  assert.equal(unassign.exitCode, 0, JSON.stringify(unassign.payload));
  assert.equal(await laneFieldOf(root, 'FD-2'), null);

  // a human move out of In Progress clears it.
  const claim3 = await runClient(['--vault', root, 'claim', 'FD-3', '--json'], { cwd: laneFD });
  assert.equal(claim3.exitCode, 0, JSON.stringify(claim3.payload));
  assert.equal(await laneFieldOf(root, 'FD-3'), 'FD');
  const humanMove = await runClient(['--vault', root, 'move', 'FD-3', 'To Do', '--human', '--json'], { cwd: laneFD });
  assert.equal(humanMove.exitCode, 0, JSON.stringify(humanMove.payload));
  assert.equal(await laneFieldOf(root, 'FD-3'), null);

  // A stale `lane:` — left on the row by hand, as if a previous lane claim's
  // release had never run — is nulled by a MAIN-mode claim rather than left
  // in place.
  const index = await loadVault(root);
  const record = [...index.tickets.values()].find(item => String(canonicalOf(item).key ?? '') === 'FD-4');
  await fs.writeFile(path.join(root, record.relativePath), patchFields(record.rawBytes, { lane: 'FD' }));
  assert.equal(await laneFieldOf(root, 'FD-4'), 'FD');
  const mainClaim = await runClient(['--vault', root, 'claim', 'FD-4', '--json'], { cwd: root });
  assert.equal(mainClaim.exitCode, 0, JSON.stringify(mainClaim.payload));
  assert.equal(await laneFieldOf(root, 'FD-4'), null);
});

// ═══ Done <=> landed (lane rows only) ═════════════════════════════════

test('finish Done on a lane row: refused with no <KEY>: commit, refused when not landed, accepted once landed', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);

  const claim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const attempt = claim.payload.claimAttempt;
  const finishDone = extra => runClient(
    ['--vault', root, 'finish', 'FD-1', '--status', 'Done', '--attempt', attempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json', ...extra],
    { cwd: laneFD },
  );

  const noCommit = await finishDone([]);
  assert.equal(noCommit.exitCode, 4, JSON.stringify(noCommit.payload));
  assert.match(noCommit.payload.error, /no `FD-1:` commit exists/);

  await git(laneFD, ['switch', '-c', 'agent/FD-1']);
  await fs.writeFile(path.join(laneFD, 'work.txt'), 'work\n');
  await git(laneFD, ['add', 'work.txt']);
  await git(laneFD, ['commit', '-qm', 'FD-1: the work']);

  const notLanded = await finishDone([]);
  assert.equal(notLanded.exitCode, 4, JSON.stringify(notLanded.payload));
  assert.match(notLanded.payload.error, /not landed on master/);

  // "land" it: fast-forward master to the ticket commit, the way `integrate`
  // would after a board-only or clean merge (mechanics 2.3). This avoids
  // checking `master` out in the lane, which would collide with the
  // authority checkout already having it checked out.
  await git(laneFD, ['update-ref', 'refs/heads/master', 'refs/heads/agent/FD-1']);

  const landed = await finishDone([]);
  assert.equal(landed.exitCode, 0, JSON.stringify(landed.payload));
});

// ═══ `approved` / human-gate closeout ═══════════════════════════════════

test('human-gate lane row: approved + landed lets an agent finish Done; the same row via move/update --status Done is still refused', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);

  const claim = await runClient(['--vault', root, 'claim', 'FD-5', '--json'], { cwd: laneFD });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const parked = await runClient(
    ['--vault', root, 'finish', 'FD-5', '--status', 'In Review', '--attempt', claim.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(parked.exitCode, 0, JSON.stringify(parked.payload));

  const approved = await runClient(['--vault', root, 'update', 'FD-5', '--label', 'approved', '--human', '--json'], { cwd: root });
  assert.equal(approved.exitCode, 0, JSON.stringify(approved.payload));

  const reclaim = await runClient(['--vault', root, 'claim', 'FD-5', '--json'], { cwd: laneFD });
  assert.equal(reclaim.exitCode, 0, JSON.stringify(reclaim.payload));
  const attempt = reclaim.payload.claimAttempt;

  // `move` and `update --status Done` never get the closeout bypass, landed
  // or not — they skip `checkFinishContent`'s safeguards entirely.
  const move = await runClient(['--vault', root, 'move', 'FD-5', 'Done', '--attempt', attempt, '--json'], { cwd: laneFD });
  assert.equal(move.exitCode, 4, JSON.stringify(move.payload));
  assert.match(move.payload.error, /human-gate/i);
  // `--status` is not in `update`'s real CLI flag vocabulary (only
  // `estimate`/`flag`/`resolve`'s shorthands and internal callers reach it) —
  // exercise `write.mjs`'s `update()` directly, the way test/mutations.test.mjs
  // already does for this same field, with the lane carrier set by hand the
  // way `dynamicDispatcher` would.
  const updateIndex = await loadVault(root);
  updateIndex.lane = { project: 'FD', repoRoot: laneFD };
  const updateDone = await dispatchWrite(updateIndex, ['update', 'FD-5', '--status', 'Done', '--attempt', attempt]);
  assert.equal(updateDone.exitCode, 4, JSON.stringify(updateDone.payload));
  assert.match(updateDone.payload.error, /human-gate/i);

  await git(laneFD, ['switch', '-c', 'agent/FD-5']);
  await fs.writeFile(path.join(laneFD, 'work.txt'), 'work\n');
  await git(laneFD, ['add', 'work.txt']);
  await git(laneFD, ['commit', '-qm', 'FD-5: the work']);
  await git(laneFD, ['update-ref', 'refs/heads/master', 'refs/heads/agent/FD-5']);

  const done = await runClient(
    ['--vault', root, 'finish', 'FD-5', '--status', 'Done', '--attempt', attempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(done.exitCode, 0, JSON.stringify(done.payload));
});

test('human-gate main-mode row: approved does not help — an agent finish Done is still refused', async t => {
  const root = await fixture(t, { lanesEnabled: false });
  const claim = await runClient(['--vault', root, 'claim', 'FD-6', '--json'], { cwd: root });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const done = await runClient(
    ['--vault', root, 'finish', 'FD-6', '--status', 'Done', '--attempt', claim.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: root },
  );
  assert.equal(done.exitCode, 4, JSON.stringify(done.payload));
  assert.match(done.payload.error, /human-gate/i);
});

test('the agent identity cannot apply --label approved or --label integration-conflict on a non-lane row; a human can apply approved', async t => {
  const root = await fixture(t, { lanesEnabled: false });

  const agentApproved = await runClient(['--vault', root, 'update', 'FD-1', '--label', 'approved', '--json'], { cwd: root });
  assert.equal(agentApproved.exitCode, 4, JSON.stringify(agentApproved.payload));
  assert.match(agentApproved.payload.error, /only a human applies `approved`/);

  const createApproved = await runClient(
    ['--vault', root, 'create', '--project', 'FD', '--title', 'New', '--text', 'Body.', '--label', 'approved', '--json'],
    { cwd: root },
  );
  assert.equal(createApproved.exitCode, 4, JSON.stringify(createApproved.payload));
  assert.match(createApproved.payload.error, /only a human applies `approved`/);

  const agentConflict = await runClient(['--vault', root, 'update', 'FD-2', '--label', 'integration-conflict', '--json'], { cwd: root });
  assert.equal(agentConflict.exitCode, 4, JSON.stringify(agentConflict.payload));
  assert.match(agentConflict.payload.error, /only a lane row/);

  const humanApproved = await runClient(['--vault', root, 'update', 'FD-1', '--label', 'approved', '--human', '--json'], { cwd: root });
  assert.equal(humanApproved.exitCode, 0, JSON.stringify(humanApproved.payload));
});

// ═══ human-gate scratch carry-forward ═══════════════════════════════════

test('an approved human-gate lane re-claim restores the six scratch files; a Blocked re-claim restores nothing', async t => {
  const root = await fixture(t);
  const laneFD = await addWorktree(t, root);
  await registerLane(root, 'FD', laneFD);

  const claim = await runClient(['--vault', root, 'claim', 'FD-5', '--json'], { cwd: laneFD });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const parked = await runClient(
    ['--vault', root, 'finish', 'FD-5', '--status', 'In Review', '--attempt', claim.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(parked.exitCode, 0, JSON.stringify(parked.payload));

  const runDir = path.join(laneFD, '.zagent', 'run', 'FD-5');
  const scratchFiles = {
    'filelist.txt': 'FD/tickets/FD-5.md\n',
    'changed.txt': 'FD/tickets/FD-5.md\n',
    'predispatch.json': '{"headSha":"deadbeef"}\n',
    'gates.json': '{"gates":[]}\n',
    'tick_gates.json': '{"state":"complete","exitCode":0}\n',
    'worklog.md': '**Durable findings**: none.\n',
  };
  for (const [name, content] of Object.entries(scratchFiles)) {
    await fs.writeFile(path.join(runDir, name), content);
  }

  // `agent/FD-5` must exist for a restore to happen at all — create it and
  // leave the lane detached again, mirroring the branch a real dispatch
  // would have left behind.
  await git(laneFD, ['switch', '-c', 'agent/FD-5']);
  await git(laneFD, ['switch', '--detach', 'HEAD']);

  const approved = await runClient(['--vault', root, 'update', 'FD-5', '--label', 'approved', '--human', '--json'], { cwd: root });
  assert.equal(approved.exitCode, 0, JSON.stringify(approved.payload));

  const reclaim = await runClient(['--vault', root, 'claim', 'FD-5', '--json'], { cwd: laneFD });
  assert.equal(reclaim.exitCode, 0, JSON.stringify(reclaim.payload));

  for (const [name, content] of Object.entries(scratchFiles)) {
    const restored = await fs.readFile(path.join(runDir, name), 'utf8');
    assert.equal(restored, content, `${name} must be restored from the .prev archive`);
  }

  // Free the lane (FD-5 is still In Progress and I5 is one ticket per lane
  // at a time) before claiming a second ticket in it.
  const freeLane = await runClient(
    ['--vault', root, 'finish', 'FD-5', '--status', 'Blocked', '--attempt', reclaim.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(freeLane.exitCode, 0, JSON.stringify(freeLane.payload));

  // A Blocked ticket (no human-gate at all) re-claimed by name gets a FRESH
  // run directory — nothing is restored.
  const claimOther = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(claimOther.exitCode, 0, JSON.stringify(claimOther.payload));
  const otherRunDir = path.join(laneFD, '.zagent', 'run', 'FD-1');
  await fs.writeFile(path.join(otherRunDir, 'filelist.txt'), 'marker\n');
  const blocked = await runClient(
    ['--vault', root, 'finish', 'FD-1', '--status', 'Blocked', '--attempt', claimOther.payload.claimAttempt, '--comment', 'x', '--worklog', 'Durable findings: none.', '--json'],
    { cwd: laneFD },
  );
  assert.equal(blocked.exitCode, 0, JSON.stringify(blocked.payload));
  const reclaimBlocked = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: laneFD });
  assert.equal(reclaimBlocked.exitCode, 0, JSON.stringify(reclaimBlocked.payload));
  await assert.rejects(
    fs.access(path.join(otherRunDir, 'filelist.txt')),
    'a Blocked re-claim must get a FRESH run directory, never the .prev archive',
  );
});

// ═══ integration-conflict and the circuit breaker ═══════════════════════

test('integration-conflict labeled Blocked rows do not count toward the circuit breaker', async t => {
  const root = await fixture(t, { lanesEnabled: false });
  const now = new Date().toISOString();
  for (const key of ['FD-1', 'FD-2', 'FD-3']) {
    const index = await loadVault(root);
    const record = index.getTicket(key);
    const bytes = patchFields(record.rawBytes, { status: 'Blocked', assignee: 'agent@zenith.invalid', labels: ['integration-conflict'], updatedAt: now });
    await fs.writeFile(path.join(root, record.relativePath), bytes);
  }
  const current = await loadVault(root);
  const check = dispatchRead(current, ['check', 'FD-4'], {});
  assert.equal(check.exitCode, 0, JSON.stringify(check.payload));
  assert.equal(check.payload.check.breaker?.open, false, JSON.stringify(check.payload.check.breaker));
});

// ═══ ticketCommitSha with --first-parent ═════════════════════════════════

test('ticketCommitSha finds the ticket commit past 200+ interleaved master commits, with --first-parent', { timeout: 60000 }, async t => {
  const root = await plainRepo(t, 'vault-lanes-claim-fp-');

  await git(root, ['switch', '-c', 'agent/FD-9']);
  await fs.writeFile(path.join(root, 'work.txt'), 'work\n');
  await git(root, ['add', 'work.txt']);
  await git(root, ['commit', '-qm', 'FD-9: the ticket commit']);
  const ticketSha = await git(root, ['rev-parse', 'HEAD']);

  await git(root, ['switch', 'master']);
  for (let i = 0; i < 205; i += 1) {
    await git(root, ['commit', '--allow-empty', '-qm', `board: filler ${i}`]);
  }

  await git(root, ['switch', 'agent/FD-9']);
  await git(root, ['merge', '--no-ff', '--no-edit', 'master']);

  const sha = await ticketCommitSha(root, 'FD-9');
  assert.equal(
    sha, ticketSha,
    '--first-parent must walk only agent/FD-9\'s own history, finding the ticket commit past 200+ interleaved master commits',
  );
});

// ═══ non-evidential gate exit codes 11/12 ════════════════════════════════

test('gateVerdictFresh: complete verdicts with exit 11 and 12 are non-evidential, like 8', async t => {
  const root = await plainRepo(t, 'vault-lanes-claim-verdict-');
  const head = await git(root, ['rev-parse', 'HEAD']);

  for (const [exitCode, reasonPattern] of [[11, /slot busy/], [12, /tree unstable/]]) {
    const runDir = path.join(root, '.zagent', 'run', 'FD-9');
    await fs.mkdir(runDir, { recursive: true });
    await fs.writeFile(
      path.join(runDir, 'tick_gates.json'),
      JSON.stringify({ state: 'complete', exitCode, headSha: head, skipped: [] }),
    );
    const verdict = await gateVerdictFresh({ repoRoot: root, key: 'FD-9' });
    assert.equal(verdict.fresh, false, `exitCode ${exitCode} must not be fresh`);
    assert.match(verdict.reason, reasonPattern);
  }
});
