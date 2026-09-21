import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { runClient } from '../cli/client.mjs';

// Suggestions row 2026-08-26 (cli): a claim/next must archive any existing
// `.zagent/run/<KEY>/` to `<KEY>.prev/` BEFORE writing the fresh scratch, so a
// stale `body.md`/`ticket.json`/gate verdict from a previous attempt can
// never be read by mistake. `archivePreviousRun` in cli/client.mjs already
// does the rename (see its doc comment); these tests cover its three
// reachable outcomes plus the `archivedPriorRun` flag `writeCompatibilityScratch`
// now records on the claim/next payload for each of them.

async function exists(target) {
  try {
    await fs.access(target);
    return true;
  } catch (error) {
    if (error.code === 'ENOENT') return false;
    throw error;
  }
}

const claimDispatch = async () => ({
  exitCode: 0,
  payload: { key: 'FD-1', status: 'In Progress', body: 'Body\n' },
  writes: [],
});

// Parameterised over BOTH commands the archive branch names. `next` is in the
// guard (`command === 'claim' || command === 'next'`) and in both documents,
// and was covered nowhere through this code path: dropping `|| 'next'` left
// every test here green. The suite's other `next` coverage goes through
// cli/zagent.mjs, a different entry point that does not archive at all (see
// the note on persistMutationScratch in the no-prior-run test below).
// Written as a shared body called from two plain tests rather than with
// `test.each`: under `test.each(cases)(name, fn)` the case value is the only
// positional argument, so a `t` declared second is `undefined` and
// `t.onTestFinished` throws before a single assertion runs.
async function assertRenamesPriorRun(t, command) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), `vault-run-archive-rename-${command}-`));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));

  const runDir = path.join(root, '.zagent', 'run', 'FD-1');
  await fs.mkdir(runDir, { recursive: true });
  await fs.writeFile(path.join(runDir, 'sentinel.txt'), 'old-run\n');

  const result = await runClient(['--vault', root, command, 'FD-1', '--json'], { repoRoot: root, dispatch: claimDispatch });

  assert.equal(result.exitCode, 0);
  assert.equal(result.payload.archivedPriorRun, true);

  const prevDir = path.join(root, '.zagent', 'run', 'FD-1.prev');
  assert.equal(await fs.readFile(path.join(prevDir, 'sentinel.txt'), 'utf8'), 'old-run\n', 'the PRIOR run\'s own contents moved under .prev');
  assert.equal(await exists(path.join(runDir, 'sentinel.txt')), false, 'the old sentinel does not remain in the fresh run directory');

  // The fresh scratch was written into the (recreated) run directory, not
  // merged into what got archived.
  assert.equal(JSON.parse(await fs.readFile(path.join(runDir, 'ticket.json'), 'utf8')).key, 'FD-1');

  // The flag reaches last.json too (persistLast serialises result.payload
  // AFTER writeCompatibilityScratch mutates it in place — see client.mjs).
  assert.equal(JSON.parse(await fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8')).archivedPriorRun, true);
}

test('claim renames an existing run directory to <KEY>.prev and reports archivedPriorRun',
  async t => assertRenamesPriorRun(t, 'claim'));

test('next renames an existing run directory to <KEY>.prev and reports archivedPriorRun',
  async t => assertRenamesPriorRun(t, 'next'));

test('claim replaces an older <KEY>.prev rather than merging into it', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-run-archive-replace-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));

  const runDir = path.join(root, '.zagent', 'run', 'FD-1');
  const prevDir = path.join(root, '.zagent', 'run', 'FD-1.prev');
  await fs.mkdir(runDir, { recursive: true });
  await fs.mkdir(prevDir, { recursive: true });
  await fs.writeFile(path.join(runDir, 'sentinel.txt'), 'new-run\n');
  await fs.writeFile(path.join(prevDir, 'old-sentinel.txt'), 'stale-run\n');

  const result = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { repoRoot: root, dispatch: claimDispatch });

  assert.equal(result.exitCode, 0);
  assert.equal(result.payload.archivedPriorRun, true);

  // The OLDER archive is gone entirely, not merged with the newer one.
  assert.equal(await exists(path.join(prevDir, 'old-sentinel.txt')), false, 'the stale .prev contents were removed, not merged');
  assert.equal(await fs.readFile(path.join(prevDir, 'sentinel.txt'), 'utf8'), 'new-run\n', 'the just-archived run\'s contents replaced the old .prev');
});

test('claim with no prior run archives nothing and reports archivedPriorRun: false', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-run-archive-none-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));

  const result = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { repoRoot: root, dispatch: claimDispatch });

  assert.equal(result.exitCode, 0);
  assert.equal(result.payload.archivedPriorRun, false, 'present-and-false, not merely absent, so a reader can tell this client ran the check at all');
  assert.equal(await exists(path.join(root, '.zagent', 'run', 'FD-1.prev')), false, 'nothing was there to archive, so no .prev is created');
  assert.equal(JSON.parse(await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'ticket.json'), 'utf8')).key, 'FD-1');
});

// ★ THE GUARD WHOSE REMOVAL IS DESTRUCTIVE, AND THE ONLY ONE THAT IS.
// The archive is gated on `result.exitCode === 0` as well as on the command.
// Drop that clause and a REFUSED claim -- which still carries a `key` on its
// payload -- archives away the run directory of whatever ticket is currently
// in flight: precisely the loss this mechanism exists to prevent, inverted.
// The other three tests all stay green under that mutation, and nothing else
// in the suite reaches writeCompatibilityScratch on a non-zero exit at all
// (claim-refusal-atomicity.test.mjs imports dispatchWrite directly, never
// runClient), so without this test the clause is pinned by nothing.
test('a REFUSED claim archives nothing and leaves the in-flight run directory untouched', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-run-archive-refused-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));

  const runDir = path.join(root, '.zagent', 'run', 'FD-1');
  await fs.mkdir(runDir, { recursive: true });
  await fs.writeFile(path.join(runDir, 'sentinel.txt'), 'in-flight\n');

  // Exit 5 is the real refusal this protects: "repo busy / would steal a
  // ticket someone else holds". The payload still carries the key.
  const refusedDispatch = async () => ({ exitCode: 5, payload: { key: 'FD-1' }, writes: [] });
  const result = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { repoRoot: root, dispatch: refusedDispatch });

  assert.equal(result.exitCode, 5);
  assert.equal(await exists(path.join(root, '.zagent', 'run', 'FD-1.prev')), false, 'a refused claim must not create a .prev');
  assert.equal(
    await fs.readFile(path.join(runDir, 'sentinel.txt'), 'utf8'),
    'in-flight\n',
    'the in-flight run directory is byte-identical after a refusal -- it was never archived or overwritten');
  assert.equal(result.payload.archivedPriorRun, undefined, 'the flag is not set at all when the archive branch is not reached');
});
