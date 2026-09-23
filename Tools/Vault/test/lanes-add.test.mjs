// Tests for the `zagent lane add` hardening pass: reusing an unregistered
// worktree left behind by a failed `lane_init.ps1` run, refusing a
// non-worktree path that happens to already exist, and the `lanes.json`
// read-modify-write race fix (re-read-then-merge under a lockfile). See
// `Tools/Vault/cli/lanes.mjs`'s `laneAdd`/`withLanesLock` and
// `Tools/Vault/test/lanes.test.mjs` for the fixtures/injectable-runner
// pattern this file follows.
import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { randomUUID } from 'node:crypto';

import { runClient } from '../cli/client.mjs';
import { git } from '../server/operations.mjs';
import { withLanesLock } from '../cli/lanes.mjs';

async function fixture(t, { lanesEnabled = true, lanesRoot } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-lanes-add-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [
      { project: 'FD', path: 'FD' },
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
    '---\nkey: FD-1\ntitle: Before\nstatus: To Do\n---\n## Goal\nFixture ticket.\n\n## Definition of Done\n- [ ] n/a\n\n## Gates\n- echo ok\n',
  );
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify({
    project: 'FD',
    baseBranch: 'master',
    lanes: { enabled: lanesEnabled, root: lanesRoot ?? path.join(root, '..', 'lanes-root'), integrateWaitMinutes: 240, buildSlotWaitMinutes: 90 },
  }));
  await fs.mkdir(path.join(root, 'Tools'), { recursive: true });
  await fs.writeFile(path.join(root, 'Tools', 'lane_init.ps1'), '# placeholder\n');
  await git(root, ['init', '-q', '-b', 'master']);
  await git(root, ['config', 'user.email', 'vault-lanes-add-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault lanes-add tests']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'fixture']);
  return root;
}

async function commonDirOf(root) {
  return path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
}

async function addWorktree(t, root, { detach = true } = {}) {
  const dir = path.join(os.tmpdir(), `vault-lane-add-worktree-${randomUUID()}`);
  await git(root, ['worktree', 'add', ...(detach ? ['--detach'] : []), dir, 'master']);
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', dir]); } catch { /* already removed by the test itself */ } });
  return dir;
}

async function readRegistry(root) {
  const common = await commonDirOf(root);
  const file = path.join(common, 'vault', 'lanes.json');
  try { return JSON.parse(await fs.readFile(file, 'utf8')); } catch { return { lanes: {} }; }
}

function normalize(value) {
  return path.resolve(String(value ?? '')).replace(/\\/g, '/').toLowerCase();
}

// A runner that executes real git for `git` calls (so `checkout --detach` /
// `worktree add` actually run against the fixture repo) and stubs
// `pwsh Tools/lane_init.ps1` — recording every call it sees so tests can
// assert exactly which git subcommand ran.
function stubLaneRunProcess({ initExitCode = 0, onInit } = {}) {
  const calls = [];
  const run = async (command, args, cwd) => {
    calls.push({ command, args: args.slice(), cwd });
    if (command === 'git') {
      try { return { code: 0, stdout: await git(cwd, args), stderr: '' }; }
      catch (error) { return { code: 1, stdout: '', stderr: error.message }; }
    }
    if (onInit) await onInit();
    return { code: initExitCode, stdout: '', stderr: initExitCode === 0 ? '' : 'stubbed lane_init.ps1 failure' };
  };
  return { calls, run };
}

// ═══ reuse of an existing unregistered worktree ═══════════════════════════

test('lane add reuses an existing unregistered worktree: no `git worktree add`, a `checkout --detach` instead, then registers', async t => {
  const root = await fixture(t);
  const lane = await addWorktree(t, root);

  const stub = stubLaneRunProcess();
  const result = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', lane, '--json'], {
    cwd: root,
    laneRunProcess: stub.run,
  });
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  assert.equal(normalize(result.payload.path), normalize(lane));
  assert.equal(result.payload.registered, true);

  assert.ok(
    !stub.calls.some(call => call.command === 'git' && call.args[0] === 'worktree' && call.args[1] === 'add'),
    `expected no \`git worktree add\`, got calls: ${JSON.stringify(stub.calls)}`,
  );
  const checkoutCall = stub.calls.find(call => call.command === 'git' && call.args[0] === 'checkout' && call.args[1] === '--detach');
  assert.ok(checkoutCall, `expected a \`git checkout --detach\` call, got: ${JSON.stringify(stub.calls)}`);
  assert.equal(normalize(checkoutCall.cwd), normalize(lane));
  assert.equal(checkoutCall.args[2], 'master');
  assert.ok(stub.calls.some(call => call.command === 'pwsh'), 'expected lane_init.ps1 to still run');

  const registry = await readRegistry(root);
  assert.ok(registry.lanes?.FD, 'the lane must be registered after a successful reuse');
  assert.equal(normalize(registry.lanes.FD.path), normalize(lane));

  const list = await runClient(['--vault', root, 'lane', 'list', '--json'], { cwd: root });
  assert.equal(list.payload.lanes.length, 1);
  assert.equal(list.payload.lanes[0].project, 'FD');
});

test('the retry-after-failure message says a retry reuses the worktree', async t => {
  const root = await fixture(t);
  const targetPath = path.join(os.tmpdir(), `vault-lane-add-fresh-${randomUUID()}`);
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', targetPath]); } catch { /* best effort */ } });

  const stub = stubLaneRunProcess({ initExitCode: 1 });
  const failed = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', targetPath, '--json'], {
    cwd: root,
    laneRunProcess: stub.run,
  });
  assert.equal(failed.exitCode, 1);
  assert.match(failed.payload.error, /lane_init\.ps1 failed/);
  assert.match(failed.payload.error, /a retry reuses this worktree/);

  // Retrying against the SAME path must reuse the worktree left behind by
  // the failure above, not fail at `git worktree add` because the path
  // already exists.
  const retryStub = stubLaneRunProcess({ initExitCode: 0 });
  const retried = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', targetPath, '--json'], {
    cwd: root,
    laneRunProcess: retryStub.run,
  });
  assert.equal(retried.exitCode, 0, JSON.stringify(retried.payload));
  assert.ok(
    !retryStub.calls.some(call => call.command === 'git' && call.args[0] === 'worktree' && call.args[1] === 'add'),
    'a retry against an existing unregistered worktree must not re-run `git worktree add`',
  );
});

// ═══ a path that exists but is not a worktree of this repo ═══════════════

test('lane add refuses a --path that exists but is not a linked worktree of this repository', async t => {
  const root = await fixture(t);
  const plainDir = path.join(os.tmpdir(), `vault-lane-add-plain-${randomUUID()}`);
  await fs.mkdir(plainDir, { recursive: true });
  t.onTestFinished(() => fs.rm(plainDir, { recursive: true, force: true }));

  const stub = stubLaneRunProcess();
  const result = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', plainDir, '--json'], {
    cwd: root,
    laneRunProcess: stub.run,
  });
  assert.equal(result.exitCode, 1);
  assert.match(result.payload.error, /not a linked worktree/);
  assert.equal(stub.calls.length, 0, 'must refuse before running any git or lane_init call');

  const registry = await readRegistry(root);
  assert.ok(!registry.lanes?.FD, 'a refused add must not register a lane');
});

// ═══ registry merge under concurrent writers ══════════════════════════════

test('lane add re-reads the registry before writing, so a lane added concurrently by another process survives', async t => {
  const root = await fixture(t);
  const common = await commonDirOf(root);
  const registryFile = path.join(common, 'vault', 'lanes.json');

  const writeConcurrentEntry = async () => {
    // Simulate a SEPARATE `zagent lane add ZEN` completing while THIS one is
    // still inside its (real, tens-of-minutes) `lane_init.ps1` run.
    await fs.mkdir(path.dirname(registryFile), { recursive: true });
    const concurrent = { lanes: { ZEN: { project: 'ZEN', path: path.join(os.tmpdir(), 'zen-lane'), createdAt: new Date().toISOString(), provisionedAt: new Date().toISOString() } } };
    await fs.writeFile(registryFile, `${JSON.stringify(concurrent, null, 2)}\n`);
  };

  const stub = stubLaneRunProcess({ onInit: writeConcurrentEntry });
  const targetPath = path.join(os.tmpdir(), `vault-lane-add-race-${randomUUID()}`);
  t.onTestFinished(async () => { try { await git(root, ['worktree', 'remove', '--force', targetPath]); } catch { /* best effort */ } });

  const result = await runClient(['--vault', root, 'lane', 'add', 'FD', '--path', targetPath, '--json'], {
    cwd: root,
    laneRunProcess: stub.run,
  });
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));

  const registry = await readRegistry(root);
  assert.ok(registry.lanes?.ZEN, 'the concurrently-added ZEN lane must survive this write');
  assert.ok(registry.lanes?.FD, 'this call\'s own FD lane must also be registered');
  assert.equal(normalize(registry.lanes.FD.path), normalize(targetPath));
});

// ═══ the lock is always released ══════════════════════════════════════════

test('withLanesLock releases the lock file even when the wrapped function throws', async t => {
  const root = await fixture(t);
  const common = await commonDirOf(root);
  const lockFile = path.join(common, 'vault', 'lanes.json.lock');

  await assert.rejects(withLanesLock(common, async () => { throw new Error('boom'); }), /boom/);
  await assert.rejects(fs.access(lockFile), /ENOENT/, 'the lock file must not remain after the wrapped function throws');

  // And the lock is usable again immediately afterwards (not left stuck).
  let ran = false;
  await withLanesLock(common, async () => { ran = true; });
  assert.equal(ran, true);
  await assert.rejects(fs.access(lockFile), /ENOENT/, 'the lock file must be released after a successful run too');
});
