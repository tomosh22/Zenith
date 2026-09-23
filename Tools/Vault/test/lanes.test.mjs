// Tests for WP1 (parallel `/tick` lanes): mode resolution, `zagent lane
// which|list|add|remove`, the `index.lane` carrier reaching `showPayload`
// and `checkFinishContent`, the unregistered-worktree refusal, `--lane`
// flag handling, and the `doctor` `lanes` block.  See
// `Tools/zagent/protocol/TICK.md` and the plan's "Parallel `/tick` lanes"
// section for the design; this file only exercises what WP1 owns.
import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';

import { runClient } from '../cli/client.mjs';
import { dispatchWrite } from '../cli/write.mjs';
import { git } from '../server/operations.mjs';
import { loadVault } from '../server/main.js';

async function fixture(t, { lanesEnabled = false, lanesRoot } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-lanes-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.mkdir(path.join(root, 'WEB'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [
      { project: 'FD', path: 'FD' },
      { project: 'WEB', path: 'WEB', readOnly: true, archive: true },
    ],
    writes: { enabled: true },
    policy: {
      board: { guardrails: { protectedPaths: [] } },
      resolved: {
        agentEmail: 'agent@zenith.invalid',
        guardrails: { protectedPaths: [] },
        projects: {
          FD: {
            repo: null,
            baseBranch: 'master',
            branching: 'direct',
            branchPrefix: 'agent/',
            gates: [],
            merge: { push: false },
            conventions: '',
            cleanup: [],
            reviewOn: [],
            workerTools: 'Read,Edit,Write,Grep,Glob',
          },
        },
      },
    },
  }));
  await fs.writeFile(
    path.join(root, 'FD/tickets/FD-1.md'),
    '---\nkey: FD-1\ntitle: Before\nstatus: To Do\n---\n## Goal\nFixture ticket for lane tests.\n\n## Definition of Done\n- [ ] n/a\n\n## Gates\n- echo ok\n',
  );
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify({
    project: 'FD',
    baseBranch: 'master',
    lanes: { enabled: lanesEnabled, root: lanesRoot ?? path.join(root, '..', 'lanes-root'), integrateWaitMinutes: 240, buildSlotWaitMinutes: 90 },
  }));
  await git(root, ['init', '-q', '-b', 'master']);
  await git(root, ['config', 'user.email', 'vault-lanes-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault lanes tests']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'fixture']);
  return root;
}

async function commonDirOf(root) {
  return path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
}

async function addWorktree(t, root, { detach = true } = {}) {
  const dir = path.join(os.tmpdir(), `vault-lane-worktree-${randomUUID()}`);
  await git(root, ['worktree', 'add', ...(detach ? ['--detach'] : []), dir, 'master']);
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', dir]); } catch { /* already removed by the test itself */ } });
  return dir;
}

async function registerLane(root, project, lanePath, { provisioned = true } = {}) {
  const common = await commonDirOf(root);
  const file = path.join(common, 'vault', 'lanes.json');
  await fs.mkdir(path.dirname(file), { recursive: true });
  let registry = { lanes: {} };
  try { registry = JSON.parse(await fs.readFile(file, 'utf8')); } catch { /* first lane */ }
  registry.lanes = registry.lanes ?? {};
  const now = new Date().toISOString();
  registry.lanes[project] = { project, path: lanePath, createdAt: now, provisionedAt: provisioned ? now : null };
  await fs.writeFile(file, `${JSON.stringify(registry, null, 2)}\n`);
  return file;
}

function normalize(value) {
  return path.resolve(String(value ?? '')).replace(/\\/g, '/').toLowerCase();
}

// ═══ resolveLaneMode / `lane which` ═══════════════════════════════════════

test('lane which: main when disabled/no registry, lane for a registered worktree, unregistered for another', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  const other = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);

  const main = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: root });
  assert.equal(main.exitCode, 0, JSON.stringify(main.payload));
  assert.equal(main.payload.mode, 'main');
  assert.equal(main.payload.project, null);
  assert.equal(main.payload.inFlight, null);

  const laneWhich = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: lane });
  assert.equal(laneWhich.exitCode, 0, JSON.stringify(laneWhich.payload));
  assert.equal(laneWhich.payload.mode, 'lane');
  assert.equal(laneWhich.payload.project, 'FD');
  assert.equal(normalize(laneWhich.payload.path), normalize(lane));
  assert.equal(laneWhich.payload.inFlight, null);

  const unregistered = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: other });
  assert.equal(unregistered.exitCode, 0, JSON.stringify(unregistered.payload));
  assert.equal(unregistered.payload.mode, 'unregistered');
  assert.equal(unregistered.payload.project, null);
});

test('lane which reports inFlight for an In Progress agent row carrying lane: <PROJECT>', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);
  await fs.writeFile(
    path.join(root, 'FD/tickets/FD-2.md'),
    '---\nkey: FD-2\ntitle: In flight\nstatus: In Progress\nassignee: agent@zenith.invalid\nlane: FD\n---\nBody\n',
  );
  await git(root, ['add', 'FD/tickets/FD-2.md']);
  await git(root, ['commit', '-qm', 'FD-2 in flight']);

  const result = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: lane });
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  assert.equal(result.payload.inFlight, 'FD-2');
});

// ═══ unregistered worktree refusal ═════════════════════════════════════

test('an unregistered worktree refuses claim/next with exit 1 and writes no ticket scratch; show still works', async t => {
  const root = await fixture(t);
  const other = await addWorktree(t, root);

  const claim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: other });
  assert.equal(claim.exitCode, 1);
  assert.match(claim.payload.error, /unregistered linked worktree/);
  await assert.rejects(fs.access(path.join(other, '.zagent', 'run', 'FD-1')));

  const next = await runClient(['--vault', root, 'next', '--json'], { cwd: other });
  assert.equal(next.exitCode, 1);
  assert.match(next.payload.error, /unregistered linked worktree/);

  const show = await runClient(['--vault', root, 'show', 'FD-1', '--json'], { cwd: other });
  assert.equal(show.exitCode, 0, JSON.stringify(show.payload));
});

// ═══ scratch isolation + the index.lane carrier ═══════════════════════════

test('lane mode: scratch (last.json, run/<KEY>/) lands under the LANE, not the authority', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);

  const claim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: lane, repoRoot: lane });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));

  const ticketScratch = path.join(lane, '.zagent', 'run', 'FD-1', 'ticket.json');
  assert.equal(JSON.parse(await fs.readFile(ticketScratch, 'utf8')).key, 'FD-1');
  await assert.rejects(fs.access(path.join(root, '.zagent', 'run', 'FD-1')), 'the authority checkout must not receive the lane\'s scratch');
  assert.equal((await fs.readFile(path.join(lane, '.zagent', 'last.exit'), 'utf8')).trim(), '0');
});

test('index.lane reaches showPayload\'s `repo` for a read command (show), which runs on the SAME index the carrier was set on', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);

  const show = await runClient(['--vault', root, 'show', 'FD-1', '--json'], { cwd: lane });
  assert.equal(show.exitCode, 0, JSON.stringify(show.payload));
  assert.equal(normalize(show.payload.repo), normalize(lane), 'index.lane.repoRoot must reach showPayload\'s `repo` field');
});

// ★ THE CARRIER GAP IS CLOSED (WP2 item 0). WP1's report pinned a KNOWN
// LIMITATION here: `claim()`/`finish()` (cli/write.mjs) hand their work to
// `runOperation(index, …)`, which calls `executeOperation(index.root,
// { prepare })` (server/operations.mjs) — and `executeOperation` loads its
// OWN fresh `VaultIndex` and calls `prepare(fresh)`, so `fresh` never used to
// see `.lane` even though the OUTER `index` (the one `dynamicDispatcher` set
// `.lane` on) did. `runOperation` now wraps `prepare` to copy `index.lane`
// onto `fresh` before calling it — see the carrier-gap comment above
// `runOperation` in write.mjs. This test replaces the old KNOWN-LIMITATION
// pin with the fixed behaviour: `claim`'s payload `repo` is the lane path
// (via `showPayload`'s `index.lane?.repoRoot`), and `finish`'s
// `checkFinishContent` resolves the LANE's own working tree, not the
// authority's.
test('the carrier gap is closed: claim\'s payload `repo` is the lane path, and finish\'s checkFinishContent reads the lane\'s own working tree', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);

  const claimIndex = await loadVault(root);
  claimIndex.lane = { project: 'FD', repoRoot: lane };
  const claim = await dispatchWrite(claimIndex, ['claim', 'FD-1']);
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  assert.equal(
    normalize(claim.payload.repo),
    normalize(lane),
    'claim\'s payload `repo` must be the LANE path now that runOperation threads index.lane onto `fresh`',
  );
  const attempt = claim.payload.claimAttempt;

  const originalWrite = process.stderr.write.bind(process.stderr);
  const lines = [];
  process.stderr.write = chunk => { lines.push(String(chunk)); return true; };
  let result;
  try {
    const index = await loadVault(root);
    index.lane = { project: 'FD', repoRoot: lane };
    result = await dispatchWrite(index, ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], {
      comment: 'x',
      worklog: 'Durable findings: none.',
    });
  } finally {
    process.stderr.write = originalWrite;
  }
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  // checkFinishContent resolves `repoRoot` from `fresh.lane.repoRoot` — the
  // missing-filelist.txt warning must name the LANE's own path now, never
  // the authority's.
  assert.ok(
    lines.some(line => /no filelist\.txt/.test(line) && line.toLowerCase().includes(normalize(lane).replace(/\//g, path.sep))),
    `expected checkFinishContent to warn about the LANE's own filelist.txt, got: ${JSON.stringify(lines)}`,
  );
});

// ═══ `--lane` handling (D2) ════════════════════════════════════════════

test('a user-supplied --lane must match the invocation; --lane from main is refused; a matching --lane never trips assertKnownFlags', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);

  const fromMain = await runClient(['--vault', root, 'show', 'FD-1', '--lane', 'FD', '--json'], { cwd: root });
  assert.equal(fromMain.exitCode, 1);
  assert.match(fromMain.payload.error, /--lane FD does not match/);

  const mismatched = await runClient(['--vault', root, 'show', 'FD-1', '--lane', 'ZEN', '--json'], { cwd: lane });
  assert.equal(mismatched.exitCode, 1);
  assert.match(mismatched.payload.error, /--lane ZEN does not match/);

  const matching = await runClient(['--vault', root, 'show', 'FD-1', '--lane', 'FD', '--json'], { cwd: lane });
  assert.equal(matching.exitCode, 0, JSON.stringify(matching.payload));
  assert.ok(!/Unknown flag/i.test(JSON.stringify(matching.payload)), '--lane must never reach assertKnownFlags\'s closed vocabulary');
});

// ═══ `lane add` / `lane remove` ═══════════════════════════════════════

function stubLaneRunProcess({ initExitCode = 0 } = {}) {
  const calls = [];
  const run = async (command, args, cwd) => {
    calls.push({ command, args, cwd });
    if (command === 'git') {
      try { return { code: 0, stdout: await git(cwd, args), stderr: '' }; }
      catch (error) { return { code: 1, stdout: '', stderr: error.message }; }
    }
    // The real `Tools/lane_init.ps1` (PowerShell) is WP6's, not this
    // package's; stub its execution while still exercising a real
    // `git worktree add` immediately before it.
    return { code: initExitCode, stdout: '', stderr: initExitCode === 0 ? '' : 'stubbed lane_init.ps1 failure' };
  };
  return { calls, run };
}

test('lane add is refused while lanes are disabled', async t => {
  const root = await fixture(t, { lanesEnabled: false });
  const result = await runClient(['--vault', root, 'lane', 'add', 'FD', '--json'], { cwd: root });
  assert.equal(result.exitCode, 1);
  assert.match(result.payload.error, /lanes are disabled/);
});

test('lane add: a stubbed lane_init.ps1 success registers the lane; a stubbed failure does not', async t => {
  const lanesRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-lanes-root-'));
  t.onTestFinished(() => fs.rm(lanesRoot, { recursive: true, force: true }));
  const root = await fixture(t, { lanesEnabled: true, lanesRoot });
  await fs.mkdir(path.join(root, 'Tools'), { recursive: true });
  await fs.writeFile(path.join(root, 'Tools', 'lane_init.ps1'), '# placeholder for WP6\n');

  const failing = stubLaneRunProcess({ initExitCode: 1 });
  const failed = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', path.join(lanesRoot, 'FD-failed'), '--json'], {
    cwd: root,
    laneRunProcess: failing.run,
  });
  assert.equal(failed.exitCode, 1);
  assert.match(failed.payload.error, /lane_init\.ps1 failed/);
  const listAfterFailure = await runClient(['--vault', root, 'lane', 'list', '--json'], { cwd: root });
  assert.equal(listAfterFailure.payload.lanes.length, 0, 'a failed provisioning must not register the lane');

  const succeeding = stubLaneRunProcess({ initExitCode: 0 });
  const targetPath = path.join(lanesRoot, 'FD');
  const added = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', targetPath, '--json'], {
    cwd: root,
    laneRunProcess: succeeding.run,
  });
  assert.equal(added.exitCode, 0, JSON.stringify(added.payload));
  assert.equal(normalize(added.payload.path), normalize(targetPath));
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', targetPath]); } catch { /* already gone */ } });

  const list = await runClient(['--vault', root, 'lane', 'list', '--json'], { cwd: root });
  assert.equal(list.exitCode, 0, JSON.stringify(list.payload));
  assert.equal(list.payload.lanes.length, 1);
  assert.equal(list.payload.lanes[0].project, 'FD');
  assert.equal(list.payload.lanes[0].exists, true);

  const which = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: targetPath });
  assert.equal(which.payload.mode, 'lane');
  assert.equal(which.payload.project, 'FD');
});

test('lane remove refuses a dirty lane', async t => {
  const root = await fixture(t, { lanesEnabled: true });
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);
  await fs.writeFile(path.join(lane, 'dirty.txt'), 'uncommitted\n');

  const result = await runClient(['--vault', root, 'lane', 'remove', 'FD', '--json'], { cwd: root });
  assert.equal(result.exitCode, 1);
  assert.match(result.payload.error, /uncommitted changes/);

  const list = await runClient(['--vault', root, 'lane', 'list', '--json'], { cwd: root });
  assert.equal(list.payload.lanes.length, 1, 'a refused removal must leave the registry entry in place');
});

// ═══ `doctor`'s `lanes` block ═══════════════════════════════════════════

test('doctor: an unlanded agent/* branch appears under `lanes`, never under `diagnostics`', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);
  await git(lane, ['switch', '-c', 'agent/FD-9']);
  await fs.writeFile(path.join(lane, 'parked.txt'), 'unlanded work\n');
  await git(lane, ['add', 'parked.txt']);
  await git(lane, ['commit', '-qm', 'FD-9: parked work']);

  const result = await runClient(['--vault', root, 'doctor', '--json'], { cwd: root });
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  assert.deepEqual(result.payload.diagnostics, []);
  const fdLane = result.payload.lanes.find(entry => entry.project === 'FD');
  assert.ok(fdLane, 'the registered lane must appear under `lanes`');
  assert.deepEqual(fdLane.unlandedBranches, ['agent/FD-9']);
  assert.equal(fdLane.provisioned, true);
  assert.equal(fdLane.exists, true);
});

test('doctor: unregisteredWorktreeCount counts linked worktrees that are neither the authority nor a registered lane', async t => {
  const root = await fixture(t);
  const zero = await runClient(['--vault', root, 'doctor', '--json'], { cwd: root });
  assert.equal(zero.payload.unregisteredWorktreeCount, 0);

  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane);
  const strayOne = await addWorktree(t, root);
  const strayTwo = await addWorktree(t, root);

  const two = await runClient(['--vault', root, 'doctor', '--json'], { cwd: root });
  assert.equal(two.exitCode, 0, JSON.stringify(two.payload));
  assert.equal(two.payload.unregisteredWorktreeCount, 2);
  void strayOne; void strayTwo;
});

test('doctor: the invoking lane being unprovisioned (or missing) flips ok and the exit code; another lane\'s own state does not', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'FD', lane, { provisioned: false });

  const fromMain = await runClient(['--vault', root, 'doctor', '--json'], { cwd: root });
  assert.equal(fromMain.exitCode, 0, JSON.stringify(fromMain.payload));
  assert.deepEqual(fromMain.payload.diagnostics, [], 'an unprovisioned lane is only a diagnostic for ITS OWN invocation');

  const fromLane = await runClient(['--vault', root, 'doctor', '--json'], { cwd: lane });
  assert.equal(fromLane.exitCode, 1);
  assert.equal(fromLane.payload.ok, false);
  assert.ok(fromLane.payload.diagnostics.some(entry => entry.code === 'LANE_NOT_PROVISIONED'));
});

// ═══ main mode is unaffected ══════════════════════════════════════════

test('main mode: claim/finish behave exactly as before, with `lane` reachable but inert', async t => {
  const root = await fixture(t);
  const claim = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { cwd: root });
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  assert.equal(claim.payload.repo, null, 'no lanes.enabled, no lane project.repo configured — unchanged from before this work package');
  const which = await runClient(['--vault', root, 'lane', 'which', '--json'], { cwd: root });
  assert.equal(which.exitCode, 0);
  assert.equal(which.payload.mode, 'main');
});
