// Tests for WP3 (`zagent integrate <KEY>` — land a lane ticket onto
// master). See `Tools/Vault/cli/integrate.mjs`'s own header and
// `Tools/Vault/spec/command-matrix.md`'s `### integrate` section for the
// full result/exit ladder this file exercises. Fixture shape mirrors
// `test/lanes-claim.test.mjs` (WP2) — duplicated rather than imported, the
// same reasoning that file gives for duplicating `test/lanes.test.mjs`'s own
// fixture: small enough that a shared import would cost more than it saves.
import { test, describe } from 'vitest';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { runClient } from '../cli/client.mjs';
import { git, executeIntegrate, recoverOperations } from '../server/operations.mjs';

const childPath = fileURLToPath(new URL('./helpers/integrate-child.mjs', import.meta.url));

// ═══ fixture helpers ═══════════════════════════════════════════════════

function ticketMd({ key, title, labels = [] }) {
  const labelsLine = labels.length ? `\nlabels: ${JSON.stringify(labels)}` : '';
  return `---\nkey: ${key}\ntitle: ${title}\nstatus: In Progress\nassignee: agent@zenith.invalid\nlane: ZM${labelsLine}\n---\n`
    + '## Goal\nFixture ticket for integrate tests.\n\n## Definition of Done\n- [ ] n/a\n\n## Gates\n- echo ok\n';
}

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-integrate-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'ZM/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'ZM/notes.md'), '# Notes\nline one\n');
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [{ project: 'ZM', path: 'ZM' }],
    writes: { enabled: true, testFaults: true },
    policy: {
      board: { guardrails: { protectedPaths: [] } },
      resolved: {
        agentEmail: 'agent@zenith.invalid',
        guardrails: { protectedPaths: [] },
        projects: {
          ZM: {
            repo: null, baseBranch: 'master', branching: 'direct', branchPrefix: 'agent/',
            gates: [], merge: { push: false }, conventions: '', cleanup: [], reviewOn: [],
            workerTools: 'Read,Edit,Write,Grep,Glob',
          },
        },
      },
    },
  }));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify({
    project: 'ZM',
    baseBranch: 'master',
    lanes: { enabled: true, root: path.join(root, '..', 'integrate-lanes-root'), integrateWaitMinutes: 240, buildSlotWaitMinutes: 90 },
  }));
  await git(root, ['init', '-q', '-b', 'master']);
  await git(root, ['config', 'user.email', 'vault-integrate-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault integrate tests']);
  // Deterministic regardless of the host's global config — a Windows box
  // with `core.autocrlf=true` would otherwise smudge every LF this fixture
  // writes into CRLF the moment `read-tree -u` (or any checkout) touches it.
  await git(root, ['config', 'core.autocrlf', 'false']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'fixture']);
  return root;
}

async function commonDirOf(root) {
  return path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
}

async function addWorktree(t, root) {
  const dir = path.join(os.tmpdir(), `vault-integrate-worktree-${Math.random().toString(36).slice(2)}`);
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

/** A shared authority + one registered ZM lane, reused across every classificatory test in a `describe`. */
async function sharedFixture(t) {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);
  await registerLane(root, 'ZM', lane);
  return { root, lane };
}

/** Add a ticket to master (a plain, non-board-shaped commit — never part of a later gap; see file header). */
async function addTicket(root, key, labels = []) {
  await fs.writeFile(path.join(root, `ZM/tickets/${key}.md`), ticketMd({ key, title: key, labels }));
  await git(root, ['add', `ZM/tickets/${key}.md`]);
  await git(root, ['commit', '-qm', `seed ${key}`]);
}

/** Cut `agent/<key>` fresh off the CURRENT master tip in the shared lane worktree, then commit the ticket's own work. */
async function cutBranch(root, lane, key, { extraFiles = { [`ZM/work-${key}.txt`]: 'work\n' }, subject = `${key}: the work` } = {}) {
  await git(lane, ['switch', '--detach', 'master']);
  await git(lane, ['switch', '-c', `agent/${key}`]);
  for (const [relative, content] of Object.entries(extraFiles)) {
    const full = path.join(lane, relative);
    await fs.mkdir(path.dirname(full), { recursive: true });
    await fs.writeFile(full, content);
    await git(lane, ['add', relative]);
  }
  if (Object.keys(extraFiles).length > 0) await git(lane, ['commit', '-qm', subject]);
  return git(lane, ['rev-parse', 'HEAD']);
}

/** A `board: `-shaped commit on master, the only kind `integrate`'s gap classifier treats as compatible. */
async function boardCommit(root, subject, opId, fileEdits = {}) {
  for (const [relative, content] of Object.entries(fileEdits)) {
    const full = path.join(root, relative);
    await fs.mkdir(path.dirname(full), { recursive: true });
    await fs.writeFile(full, content);
    await git(root, ['add', relative]);
  }
  return git(root, ['commit', '--allow-empty', '-qm', `board: ${subject}\n\nVault-Operation: ${opId}\n`]);
}

/** A commit on master that is NOT board-only (a real code change, or another lane's own `integrate` merge). */
async function codeCommit(root, subject) {
  return git(root, ['commit', '--allow-empty', '-qm', subject]);
}

async function writeVerdict(lane, key, tip, overrides = {}) {
  const tree = await git(lane, ['rev-parse', `${tip}^{tree}`]);
  const runDir = path.join(lane, '.zagent', 'run', key);
  await fs.mkdir(runDir, { recursive: true });
  const verdict = { key, phase: 'verify', state: 'complete', exitCode: 0, treeShaEnd: tree, ...overrides };
  await fs.writeFile(path.join(runDir, 'tick_gates.json'), JSON.stringify(verdict));
}

function child(args, extraEnv = {}) {
  return new Promise((resolve, reject) => {
    const processHandle = spawn(process.execPath, [childPath, ...args], { windowsHide: true, env: { ...process.env, ...extraEnv } });
    let stdout = ''; let stderr = '';
    processHandle.stdout.on('data', value => { stdout += value; });
    processHandle.stderr.on('data', value => { stderr += value; });
    processHandle.on('error', reject);
    processHandle.on('close', (code, signal) => resolve({ code, signal, stdout, stderr }));
  });
}

async function integrateVia(root, lane, key) {
  return runClient(['--vault', root, 'integrate', key, '--json'], { cwd: lane });
}

// ═══ not lane mode ═════════════════════════════════════════════════════

test('main checkout: integrate refuses exit 1 — not lane mode', async t => {
  const root = await fixture(t);
  const result = await runClient(['--vault', root, 'integrate', 'ZM-1', '--json'], { cwd: root });
  assert.equal(result.exitCode, 1, JSON.stringify(result.payload));
  assert.match(result.payload.error, /registered lane/);
});

// ═══ classificatory cases (one shared fixture) ══════════════════════════

describe('classificatory cases', () => {
  test('board-only gap (including a board commit landing before the lock) -> LANDED; parents (oldMaster, tip); main synced; first-parent shows it', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-1');
    const tip = await cutBranch(root, lane, 'ZM-1');
    const oldMasterBefore = await git(root, ['rev-parse', 'master']);
    // Two board-only commits land on master AFTER the lane branched — this
    // is also "a board commit arriving before the lock": `integrate` only
    // ever reads `oldMaster` fresh, inside the lock, so a commit that landed
    // any time before the CLI call is reclassified the same way regardless
    // of exactly when it arrived.
    await boardCommit(root, 'pin bump 1', 'op-1');
    await boardCommit(root, 'pin bump 2', 'op-2');
    const oldMaster = await git(root, ['rev-parse', 'master']);
    assert.notEqual(oldMaster, oldMasterBefore);
    await writeVerdict(lane, 'ZM-1', tip);

    const result = await integrateVia(root, lane, 'ZM-1');
    assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'LANDED');
    assert.equal(result.payload.regenNeeded, false);

    const merge = result.payload.merge;
    assert.equal(await git(root, ['rev-parse', 'master']), merge);
    const parents = (await git(root, ['log', '-1', '--format=%P', merge])).split(' ');
    assert.deepEqual(parents, [oldMaster, tip]);
    // The lane's own file reached the main checkout.
    assert.equal(await fs.readFile(path.join(root, 'ZM/work-ZM-1.txt'), 'utf8'), 'work\n');
    assert.equal((await git(root, ['status', '--porcelain'])).trim(), '');
    const firstParentLog = await git(root, ['log', '--first-parent', '--format=%s', 'master']);
    assert.ok(firstParentLog.split('\n').includes('integrate ZM-1: ZM-1'), firstParentLog);
  });

  test('a code commit in the gap -> REGATE, nothing moves', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-2');
    const tip = await cutBranch(root, lane, 'ZM-2');
    await codeCommit(root, 'a real code change, not board-shaped');
    const before = await git(root, ['rev-parse', 'master']);
    await writeVerdict(lane, 'ZM-2', tip);

    const result = await integrateVia(root, lane, 'ZM-2');
    assert.equal(result.exitCode, 20, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'REGATE');
    assert.equal(await git(root, ['rev-parse', 'master']), before, 'a REGATE result must move nothing');
  });

  test('another lane\'s own integrate merge in the gap -> REGATE (classified by commit, not by "integrate" special-casing)', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-3');
    const tip = await cutBranch(root, lane, 'ZM-3');
    // Shaped like a real `integrate` merge commit's own subject — deliberately
    // NOT `board: `-prefixed, which is what the classifier actually keys on.
    await codeCommit(root, 'integrate ZM-OTHER: another lane\'s own work');
    await writeVerdict(lane, 'ZM-3', tip);

    const result = await integrateVia(root, lane, 'ZM-3');
    assert.equal(result.exitCode, 20, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'REGATE');
  });

  test('ticket edited a board file master also changed -> CONFLICT, nothing moves', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-4');
    await git(lane, ['switch', '--detach', 'master']);
    await git(lane, ['switch', '-c', 'agent/ZM-4']);
    await fs.writeFile(path.join(lane, 'ZM/notes.md'), '# Notes\nlane version\n');
    await git(lane, ['add', 'ZM/notes.md']);
    await git(lane, ['commit', '-qm', 'ZM-4: edit notes']);
    const tip = await git(lane, ['rev-parse', 'HEAD']);
    await boardCommit(root, 'edit notes on master', 'op-conflict', { 'ZM/notes.md': '# Notes\nmaster version\n' });
    const before = await git(root, ['rev-parse', 'master']);
    await writeVerdict(lane, 'ZM-4', tip);

    const result = await integrateVia(root, lane, 'ZM-4');
    assert.equal(result.exitCode, 23, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'CONFLICT');
    assert.equal(await git(root, ['rev-parse', 'master']), before, 'a CONFLICT result must move nothing');
    assert.match(await fs.readFile(path.join(root, 'ZM/notes.md'), 'utf8'), /master version/);
  });

  test('an overlapping untracked file in the main checkout -> MAIN_DIRTY, nothing moves', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-5');
    const tip = await cutBranch(root, lane, 'ZM-5');
    const before = await git(root, ['rev-parse', 'master']);
    // An untracked file at the exact path the merge would introduce.
    await fs.writeFile(path.join(root, 'ZM/work-ZM-5.txt'), 'a human was here\n');
    await writeVerdict(lane, 'ZM-5', tip);

    const result = await integrateVia(root, lane, 'ZM-5');
    assert.equal(result.exitCode, 25, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'MAIN_DIRTY');
    assert.ok(result.payload.paths.some(p => p.includes('work-ZM-5.txt')), JSON.stringify(result.payload));
    assert.equal(await git(root, ['rev-parse', 'master']), before);
    assert.equal(await fs.readFile(path.join(root, 'ZM/work-ZM-5.txt'), 'utf8'), 'a human was here\n');
  });

  test('a LANDED merge preserves an unrelated dirty file in the main checkout', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-6');
    const tip = await cutBranch(root, lane, 'ZM-6');
    await fs.writeFile(path.join(root, 'ZM/notes.md'), '# Notes\nline one\nan uncommitted human edit\n');
    await writeVerdict(lane, 'ZM-6', tip);

    const result = await integrateVia(root, lane, 'ZM-6');
    assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
    assert.match(await fs.readFile(path.join(root, 'ZM/notes.md'), 'utf8'), /an uncommitted human edit/);
    const status = await git(root, ['status', '--porcelain']);
    assert.match(status, /ZM\/notes\.md/);
  });

  test('main checkout on another branch -> MAIN_NOT_ON_BASE', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-7');
    const tip = await cutBranch(root, lane, 'ZM-7');
    await writeVerdict(lane, 'ZM-7', tip);
    await git(root, ['switch', '-c', 'not-master']);
    t.onTestFinished(() => git(root, ['switch', 'master']).catch(() => {}));

    const result = await integrateVia(root, lane, 'ZM-7');
    assert.equal(result.exitCode, 26, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'MAIN_NOT_ON_BASE');
    await git(root, ['switch', 'master']);
  });

  test('human-gate without approved -> UNAPPROVED', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-8', ['human-gate']);
    const tip = await cutBranch(root, lane, 'ZM-8');
    await writeVerdict(lane, 'ZM-8', tip);

    const result = await integrateVia(root, lane, 'ZM-8');
    assert.equal(result.exitCode, 27, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'UNAPPROVED');
  });

  test('STALE_VERDICT: no tick_gates.json, measure-phase, tree mismatch, dirty lane, exit 10, exit 11', async t => {
    const { root, lane } = await sharedFixture(t);

    await addTicket(root, 'ZM-9');
    let tip = await cutBranch(root, lane, 'ZM-9');
    const noVerdict = await integrateVia(root, lane, 'ZM-9');
    assert.equal(noVerdict.exitCode, 21, JSON.stringify(noVerdict.payload));
    assert.equal(noVerdict.payload.result, 'STALE_VERDICT');

    await addTicket(root, 'ZM-10');
    tip = await cutBranch(root, lane, 'ZM-10');
    await writeVerdict(lane, 'ZM-10', tip, { phase: 'measure' });
    const measurePhase = await integrateVia(root, lane, 'ZM-10');
    assert.equal(measurePhase.exitCode, 21, JSON.stringify(measurePhase.payload));
    assert.equal(measurePhase.payload.result, 'STALE_VERDICT');

    await addTicket(root, 'ZM-11');
    tip = await cutBranch(root, lane, 'ZM-11');
    await writeVerdict(lane, 'ZM-11', tip, { treeShaEnd: '0'.repeat(40) });
    const treeMismatch = await integrateVia(root, lane, 'ZM-11');
    assert.equal(treeMismatch.exitCode, 21, JSON.stringify(treeMismatch.payload));
    assert.equal(treeMismatch.payload.result, 'STALE_VERDICT');

    await addTicket(root, 'ZM-12');
    tip = await cutBranch(root, lane, 'ZM-12');
    await writeVerdict(lane, 'ZM-12', tip);
    await fs.writeFile(path.join(lane, 'ZM/dirty.txt'), 'uncommitted\n');
    t.onTestFinished(() => fs.rm(path.join(lane, 'ZM/dirty.txt'), { force: true }));
    const dirtyLane = await integrateVia(root, lane, 'ZM-12');
    assert.equal(dirtyLane.exitCode, 21, JSON.stringify(dirtyLane.payload));
    assert.equal(dirtyLane.payload.result, 'STALE_VERDICT');
    await fs.rm(path.join(lane, 'ZM/dirty.txt'), { force: true });

    await addTicket(root, 'ZM-13');
    tip = await cutBranch(root, lane, 'ZM-13');
    await writeVerdict(lane, 'ZM-13', tip, { exitCode: 10 });
    const measured = await integrateVia(root, lane, 'ZM-13');
    assert.equal(measured.exitCode, 21, JSON.stringify(measured.payload));
    assert.equal(measured.payload.result, 'STALE_VERDICT');

    await addTicket(root, 'ZM-14');
    tip = await cutBranch(root, lane, 'ZM-14');
    await writeVerdict(lane, 'ZM-14', tip, { exitCode: 11 });
    const slotBusy = await integrateVia(root, lane, 'ZM-14');
    assert.equal(slotBusy.exitCode, 21, JSON.stringify(slotBusy.payload));
    assert.equal(slotBusy.payload.result, 'STALE_VERDICT');
  });

  test('exit 12 -> TREE_UNSTABLE directly, not STALE_VERDICT (carry_forward.md)', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-15');
    const tip = await cutBranch(root, lane, 'ZM-15');
    await writeVerdict(lane, 'ZM-15', tip, { exitCode: 12 });

    const result = await integrateVia(root, lane, 'ZM-15');
    assert.equal(result.exitCode, 24, JSON.stringify(result.payload));
    assert.equal(result.payload.result, 'TREE_UNSTABLE');
  });

  test('NOT_GREEN: WIP tip, no <KEY>: commit at all, and a real gate failure (exit 3)', async t => {
    const { root, lane } = await sharedFixture(t);

    await addTicket(root, 'ZM-16');
    await git(lane, ['switch', '--detach', 'master']);
    await git(lane, ['switch', '-c', 'agent/ZM-16']);
    await fs.writeFile(path.join(lane, 'ZM/work-ZM-16.txt'), 'work\n');
    await git(lane, ['add', 'ZM/work-ZM-16.txt']);
    await git(lane, ['commit', '-qm', 'WIP(ZM-16): parked red']);
    const wipTip = await git(lane, ['rev-parse', 'HEAD']);
    await writeVerdict(lane, 'ZM-16', wipTip);
    const wip = await integrateVia(root, lane, 'ZM-16');
    assert.equal(wip.exitCode, 22, JSON.stringify(wip.payload));
    assert.equal(wip.payload.result, 'NOT_GREEN');

    await addTicket(root, 'ZM-17');
    await git(lane, ['switch', '--detach', 'master']);
    await git(lane, ['switch', '-c', 'agent/ZM-17']);
    await fs.writeFile(path.join(lane, 'ZM/work-ZM-17.txt'), 'work\n');
    await git(lane, ['add', 'ZM/work-ZM-17.txt']);
    await git(lane, ['commit', '-qm', 'an unrelated commit message']);
    const noCommitTip = await git(lane, ['rev-parse', 'HEAD']);
    await writeVerdict(lane, 'ZM-17', noCommitTip);
    const noCommit = await integrateVia(root, lane, 'ZM-17');
    assert.equal(noCommit.exitCode, 22, JSON.stringify(noCommit.payload));
    assert.equal(noCommit.payload.result, 'NOT_GREEN');

    await addTicket(root, 'ZM-18');
    const tip = await cutBranch(root, lane, 'ZM-18');
    await writeVerdict(lane, 'ZM-18', tip, { exitCode: 3 });
    const gateFailed = await integrateVia(root, lane, 'ZM-18');
    assert.equal(gateFailed.exitCode, 22, JSON.stringify(gateFailed.payload));
    assert.equal(gateFailed.payload.result, 'NOT_GREEN');
  });

  test('regenNeeded is true for an added .cpp outside Middleware/', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-19');
    const tip = await cutBranch(root, lane, 'ZM-19', {
      extraFiles: { 'ZM/Source/New.cpp': '// new TU\n' },
      subject: 'ZM-19: add a translation unit',
    });
    await writeVerdict(lane, 'ZM-19', tip);

    const result = await integrateVia(root, lane, 'ZM-19');
    assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
    assert.equal(result.payload.regenNeeded, true);
  });
});

// ═══ CAS race ════════════════════════════════════════════════════════════

test('update-ref CAS race on master -> RACED, nothing of this run lands', async t => {
  const { root, lane } = await sharedFixture(t);
  await addTicket(root, 'ZM-20');
  const tip = await cutBranch(root, lane, 'ZM-20');
  await writeVerdict(lane, 'ZM-20', tip);
  const oldMaster = await git(root, ['rev-parse', 'master']);

  // A dangling commit object, never referenced by any branch, standing in
  // for "another writer landed something on master" between this run's own
  // fresh read of oldMaster (inside the lock) and its own CAS.
  const tree = await git(root, ['rev-parse', 'HEAD^{tree}']);
  const raceSha = await git(root, ['commit-tree', tree, '-p', oldMaster], 'board: a race winner\n\nVault-Operation: race-op\n');

  process.env.VAULT_TEST_INTEGRATE_MOVE_BASE = raceSha;
  t.onTestFinished(() => { delete process.env.VAULT_TEST_INTEGRATE_MOVE_BASE; });
  try {
    const outcome = await executeIntegrate(root, { key: 'ZM-20', project: 'ZM', baseBranch: 'master', tip, title: 'ZM-20' });
    assert.equal(outcome.result, 'RACED', JSON.stringify(outcome));
  } finally {
    delete process.env.VAULT_TEST_INTEGRATE_MOVE_BASE;
  }
  assert.equal(await git(root, ['rev-parse', 'master']), raceSha);
});

// ═══ crash recovery ══════════════════════════════════════════════════════

describe('crash recovery', () => {
  test('crash after PREPARED: nothing happened; a fresh retry lands normally', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-21');
    const tip = await cutBranch(root, lane, 'ZM-21');
    const oldMaster = await git(root, ['rev-parse', 'master']);

    const crash = await child(
      [root, 'ZM-21', 'ZM', 'master', tip, 'ZM-21'],
      { VAULT_TEST_CRASH_AFTER_INTEGRATE_PREPARED: '1' },
    );
    assert.notEqual(crash.code, 0);
    assert.equal(await git(root, ['rev-parse', 'master']), oldMaster);

    assert.deepEqual(await recoverOperations(root), { recovered: 1 });
    assert.equal(await git(root, ['rev-parse', 'master']), oldMaster, 'PREPARED-phase recovery must not move the ref');

    const retry = await executeIntegrate(root, { key: 'ZM-21', project: 'ZM', baseBranch: 'master', tip, title: 'ZM-21' });
    assert.equal(retry.result, 'LANDED', JSON.stringify(retry));
    assert.equal(await git(root, ['rev-parse', 'master']), retry.extra.merge);
  });

  test('crash after REF_UPDATED: recovery syncs the main checkout, preserving an unrelated staged edit', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-22');
    const tip = await cutBranch(root, lane, 'ZM-22');

    // An unrelated staged edit already sitting in the main checkout before
    // the crash — must survive recovery untouched, never RECOVERY_CONFLICT.
    await fs.writeFile(path.join(root, 'ZM/notes.md'), '# Notes\nline one\nstaged before the crash\n');
    await git(root, ['add', 'ZM/notes.md']);

    const crash = await child(
      [root, 'ZM-22', 'ZM', 'master', tip, 'ZM-22'],
      { VAULT_TEST_CRASH_AFTER_INTEGRATE_REF_UPDATED: '1' },
    );
    assert.notEqual(crash.code, 0);

    assert.deepEqual(await recoverOperations(root), { recovered: 1 });
    assert.equal(await fs.readFile(path.join(root, 'ZM/work-ZM-22.txt'), 'utf8'), 'work\n');
    assert.match(await fs.readFile(path.join(root, 'ZM/notes.md'), 'utf8'), /staged before the crash/);
    const cached = await git(root, ['diff', '--cached', '--name-only']);
    assert.match(cached, /ZM\/notes\.md/);
  });

  test('crash after SYNCED: recovery finds the touched paths already match and closes the journal out', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-23');
    const tip = await cutBranch(root, lane, 'ZM-23');

    const crash = await child(
      [root, 'ZM-23', 'ZM', 'master', tip, 'ZM-23'],
      { VAULT_TEST_CRASH_AFTER_INTEGRATE_SYNCED: '1' },
    );
    assert.notEqual(crash.code, 0);
    // The ref and the main checkout both already reflect the merge — only
    // the journal's own 'complete' write never happened.
    assert.equal(await fs.readFile(path.join(root, 'ZM/work-ZM-23.txt'), 'utf8'), 'work\n');

    assert.deepEqual(await recoverOperations(root), { recovered: 1 });
    assert.deepEqual(await recoverOperations(root), { recovered: 0 }, 'a second recovery pass must find nothing left to do');
  });

  test('a foreign state at a touched path recovers as RECOVERY_CONFLICT, never a crash', async t => {
    const { root, lane } = await sharedFixture(t);
    await addTicket(root, 'ZM-24');
    const tip = await cutBranch(root, lane, 'ZM-24');

    const crash = await child(
      [root, 'ZM-24', 'ZM', 'master', tip, 'ZM-24'],
      { VAULT_TEST_CRASH_AFTER_INTEGRATE_REF_UPDATED: '1' },
    );
    assert.notEqual(crash.code, 0);

    // Neither oldMaster's nor merge's version of the touched file: a foreign
    // state at that exact path, staged into the index.
    await fs.writeFile(path.join(root, 'ZM/work-ZM-24.txt'), 'neither side wrote this\n');
    await git(root, ['add', 'ZM/work-ZM-24.txt']);

    await assert.rejects(recoverOperations(root), e => e.code === 'RECOVERY_CONFLICT');
    assert.equal(await fs.readFile(path.join(root, 'ZM/work-ZM-24.txt'), 'utf8'), 'neither side wrote this\n');
  });
});
