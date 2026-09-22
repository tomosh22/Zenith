import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readdir, readFile, rm, mkdtemp, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';

import { dispatchRead } from '../cli/read.js';
import { loadVault } from '../server/main.js';

const fixtureRoot = fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url));
const cliPath = fileURLToPath(new URL('../cli/zagent.mjs', import.meta.url));

// ★ `.zagent/` IS NOT VAULT CONTENT, AND HASHING IT MADE THIS TEST A DICE ROLL.
// It is gitignored scratch (.gitignore:241; `git ls-files` returns zero files for
// it), and EVERY zagent call writes `last.exit`/`last.json` into it by design --
// the tick protocol depends on reading them. So a read-only command legitimately
// writes there, and asserting it does not is simply wrong.
//
// Worse, `fixtureRoot` is the COMMITTED fixture shared by nine test files
// (acceptance, cli-entry, cli-process, cli-queue, history, http, mutations,
// parity, queries) which Vitest runs in PARALLEL -- so the before/after window
// spans writes from eight other files. Measured across three full runs: five
// different tests failed, never the same set twice, always on a `.zagent` byte.
// That reds any gate list containing `npm test`, which is every ticket touching
// Tools/**, and tick_gates' flake re-run does not rescue it because the re-run
// fails on a DIFFERENT test and so reads as red-to-red.
//
// Skip it. The assertion is about vault bytes; this directory holds none.
async function hashTree(root, current = root, rows = []) {
  for (const entry of await readdir(current, { withFileTypes: true })) {
    const absolute = path.join(current, entry.name);
    if (entry.name === '.zagent') continue;
    if (entry.isDirectory()) {
      await hashTree(root, absolute, rows);
    } else if (entry.isFile()) {
      rows.push([
        path.relative(root, absolute).split(path.sep).join('/'),
        createHash('sha256').update(await readFile(absolute)).digest('hex'),
      ]);
    }
  }
  return rows.sort(([left], [right]) => left.localeCompare(right));
}

function cliRead(root, args) {
  const child = spawnSync(
    process.execPath,
    [cliPath, '--vault', root, ...args, '--json'],
    {
      cwd: path.dirname(cliPath),
      encoding: 'utf8',
      env: { ...process.env, NODE_NO_WARNINGS: '1' },
      maxBuffer: 1024 * 1024,
      timeout: 30_000,
    },
  );
  const lines = `${child.stdout ?? ''}\n${child.stderr ?? ''}`
    .split(/\r?\n/)
    .map(line => line.trim())
    .filter(Boolean);
  const last = lines.at(-1);
  return {
    status: child.status,
    payload: last === undefined ? undefined : JSON.parse(last),
    output: lines,
  };
}

test('process CLI reads gates from --file, validates flags, and keeps help read-only', async t => {
  const changedRoot = await mkdtemp(path.join(tmpdir(), 'zenith-vault-cli-'));
  t.onTestFinished(() => rm(changedRoot, { recursive: true, force: true }));
  const changedPath = path.join(changedRoot, 'changed-files.txt');
  const changedText = 'Games/Foundry/Source/SimCore/FD_Core.cpp\n';
  await writeFile(changedPath, changedText);

  const before = await hashTree(fixtureRoot);
  const index = await loadVault(fixtureRoot);
  const expected = dispatchRead(index, ['gates', 'FD-11'], { file: changedText });
  const gates = cliRead(fixtureRoot, ['gates', 'FD-11', '--file', changedPath]);
  assert.equal(gates.status, expected.exitCode);
  assert.deepEqual(gates.payload, expected.payload);

  const typo = cliRead(fixtureRoot, ['queue', '--projectt', 'FD']);
  assert.equal(typo.status, 1);
  assert.match(String(typo.payload?.error), /Unknown flag.*projectt/i);

  const help = cliRead(fixtureRoot, ['claim', '--help']);
  assert.equal(help.status, 0);
  assert.equal(help.payload.readOnly, true);
  assert.ok(help.payload.commands.includes('gates'));
  assert.deepEqual(await hashTree(fixtureRoot), before);
});
