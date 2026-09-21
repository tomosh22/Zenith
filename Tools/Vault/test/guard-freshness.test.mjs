import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchLocal } from '../cli/local.mjs';
import { git } from '../server/operations.mjs';
import { blockedScopeAllowed, gateVerdictFresh } from '../cli/tick-checks.mjs';

// Covers three related freshness-fold defects, all against the CURRENT tree
// (measured on ZEN-19/ZEN-99/ZEN-101, see tick-checks.mjs's own comments):
//
//   (A) local.mjs `guard`'s `gateSelectionStale` collapsed "checked, clean"
//       and "never checked" onto the same `null` value.
//   (B) tick-checks.mjs `gateVerdictFresh` never looked at the verdict's own
//       exit code, so a crash that still wrote `state: 'complete'` read as a
//       trustworthy pass.
//   (C) tick-checks.mjs `blockedScopeAllowed` refused `blocked-scope` on any
//       RED verdict even when the red was a measured, same-tree flake.

async function tempRepo(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'guard-freshness-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Guard freshness tests']);
  await git(root, ['config', 'user.email', 'guard-freshness@example.test']);
  return root;
}

async function writeRun(root, key, name, content) {
  const dir = path.join(root, '.zagent', 'run', key);
  await fs.mkdir(dir, { recursive: true });
  await fs.writeFile(path.join(dir, name), content);
}

async function commitAndHead(root, file = 'f.txt') {
  await fs.writeFile(path.join(root, file), '1\n');
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'one']);
  return git(root, ['rev-parse', 'HEAD']);
}

// ─── (A) local.mjs `guard`'s gateSelectionStale is now three-valued ───────

function localIndex() {
  return { config: { policy: { resolved: { guardrails: { protectedPaths: [] } } } } };
}

test('(A) guard with no key: the fold never ran — gateSelectionStale stays null, not false', () => {
  const response = dispatchLocal(localIndex(), ['guard'], { file: 'Zenith/Core/Z_Core.cpp' });
  assert.equal(response.exitCode, 0);
  assert.equal(response.payload.gateSelectionStale, null, 'no ticket key was passed, so the fold never ran at all');
});

test('(A) guard with a key and a matching gate selection: gateSelectionStale is false, not null — a passing guard is now self-describing', () => {
  const context = { dispatchRead: () => ({ exitCode: 0, payload: { gates: ['zenith test Foundry'], changed: ['x.cpp'] } }) };
  const files = { file: 'x.cpp', gates: JSON.stringify({ gates: ['zenith test Foundry'], changed: ['x.cpp'] }) };
  const response = dispatchLocal(localIndex(), ['guard', 'FD-1'], files, context);
  assert.equal(response.exitCode, 0, JSON.stringify(response.payload));
  assert.equal(response.payload.gateSelectionStale, false, 'the fold ran and found the selection current — distinct from "never ran" (null)');
});

test('(A) guard with a key and a stale gate selection still refuses, unchanged', () => {
  const context = { dispatchRead: () => ({ exitCode: 0, payload: { gates: ['zenith test Foundry', 'zenith test Combat'], changed: ['x.cpp', 'y.cpp'] } }) };
  const files = { file: 'x.cpp\ny.cpp', gates: JSON.stringify({ gates: ['zenith test Foundry'], changed: ['x.cpp'] }) };
  const response = dispatchLocal(localIndex(), ['guard', 'FD-1'], files, context);
  assert.equal(response.exitCode, 1);
  assert.match(response.payload.gateSelectionStale, /no longer describes this diff/);
});

// ─── (B) gateVerdictFresh now checks the verdict's own exit code ──────────

test('(B) a clean, matching, exit-0 verdict is fresh', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 0, headSha: head }));
  const fresh = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(fresh.fresh, true, fresh.reason);
});

test('(B) state left as "error" (the producer-side fix) is caught by the EXISTING state branch, not the new exitCode branch', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'error', exitCode: 1, headSha: head }));
  const result = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.fresh, false);
  // The state check fires first and returns before the exitCode check is
  // ever reached — the two mechanisms are complementary, not double-firing
  // (and not producing a conflicting reason) on the same verdict shape.
  assert.match(result.reason, /state is 'error', not 'complete'/);
  assert.doesNotMatch(result.reason, /ladder documents/);
});

test('(B) a documented RED exit (2/3/4) on a complete, fresh-tree verdict is still FRESH — a red gate is a real answer, not staleness', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  for (const exitCode of [2, 3, 4]) {
    await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode, headSha: head }));
    const result = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
    assert.equal(result.fresh, true, `exit ${exitCode} is documented and must read as fresh: ${result.reason}`);
  }
});

test('(B) exit 10 (MEASURED) and exit 5 (no gate list) are documented and read as fresh', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  for (const exitCode of [5, 10]) {
    await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode, headSha: head }));
    const result = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
    assert.equal(result.fresh, true, `exit ${exitCode} is documented and must read as fresh: ${result.reason}`);
  }
});

test('(B) exit 8 ("no gate lines") is DOCUMENTED but VACUOUS — not fresh, and the reason names the empty run, not an undocumented code (ZEN-99 shape)', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  // A measure phase that reported 'complete' having run zero gate lines —
  // every line deferred, nothing executed, yet the verdict still says green
  // in shape (state: complete, a matching headSha).
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({
    state: 'complete', exitCode: 8, headSha: head, deferred: ['zenith test Zenithmon --headless'],
  }));
  const result = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.fresh, false, 'a phase that ran no gate lines must never read as fresh, even with a matching headSha');
  assert.match(result.reason, /no gate lines/, 'the reason must name the empty run');
  assert.doesNotMatch(result.reason, /is not one of the codes/, '8 is documented — it must not be reported as an undocumented/corrupt code');
});

test('(B) an UNDOCUMENTED exit code on an otherwise-complete, matching-tree verdict is NOT fresh (ZEN-101 shape)', async t => {
  const root = await tempRepo(t);
  const head = await commitAndHead(root);
  // Exactly the ZEN-101 shape: state left 'complete' by a wrapper that
  // caught nothing, top-level exitCode 1 — never a value the ladder
  // documents — headSha matches, every per-step result would have read 0.
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({
    state: 'complete', exitCode: 1, headSha: head,
    steps: [{ label: 'unit gate', line: 'zenith test Zenithmon', exitCode: 0 }],
  }));
  const result = await gateVerdictFresh({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.fresh, false);
  assert.match(result.reason, /exitCode 1 is not one of the codes/);
  assert.match(result.reason, /ladder documents/);
});

// ─── (C) blockedScopeAllowed consults 'flakes' before refusing a RED verdict ─

test('(C) a RED verdict with no flakes key at all: refused, preserving today\'s behaviour ("no flake evidence")', async t => {
  const root = await tempRepo(t);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 4 }));
  const result = await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.allowed, false);
  assert.match(result.reason, /no flake evidence/);
});

test('(C) a RED verdict with an empty flakes array: refused — nothing was ever shown to be a flake', async t => {
  const root = await tempRepo(t);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 4, flakes: [] }));
  const result = await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.allowed, false);
  assert.match(result.reason, /'flakes' is empty/);
});

test('(C) a RED verdict where every flake recovered on its re-run: blocked-scope is ALLOWED', async t => {
  const root = await tempRepo(t);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({
    state: 'complete', exitCode: 4,
    flakes: [{ line: 'npx vitest run test/tick-checks.test.mjs', firstExit: 1, rerunExit: 0 }],
  }));
  const result = await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.allowed, true, result.reason);
  assert.match(result.reason, /flake, not a failure/);
});

test('(C) a RED verdict where a red line SURVIVED its re-run: refused, and the message names which line', async t => {
  const root = await tempRepo(t);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({
    state: 'complete', exitCode: 4,
    flakes: [
      { line: 'npx vitest run test/tick-checks.test.mjs', firstExit: 1, rerunExit: 0 },
      { line: 'zenith test Zenithmon --headless', firstExit: 1, rerunExit: 1 },
    ],
  }));
  const result = await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' });
  assert.equal(result.allowed, false);
  assert.match(result.reason, /zenith test Zenithmon --headless/, 'the refusal must name the surviving line');
  assert.doesNotMatch(result.reason, /npx vitest run test\/tick-checks\.test\.mjs/, 'the recovered flake must not be blamed');
});

test('(C) exit 10 (MEASURED) and green (0) are unaffected by flakes — still allowed, same as before', async t => {
  const root = await tempRepo(t);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 0 }));
  assert.equal((await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' })).allowed, true);
  await writeRun(root, 'ZEN-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 10 }));
  assert.equal((await blockedScopeAllowed({ repoRoot: root, key: 'ZEN-1' })).allowed, true);
});
