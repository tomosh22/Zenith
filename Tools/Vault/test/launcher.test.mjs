import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

test('PowerShell launcher selects Vault explicitly, forwards arguments, and fails closed on invalid config', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-launcher-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  const launcher = path.join(root, 'Tools/zagent/zagent.ps1');
  await fs.mkdir(path.dirname(launcher), { recursive: true });
  await fs.mkdir(path.join(root, 'Tools/Vault/cli'), { recursive: true });
  await fs.copyFile(fileURLToPath(new URL('../../zagent/zagent.ps1', import.meta.url)), launcher);
  await fs.writeFile(path.join(root, 'Tools/Vault/cli/client.mjs'), 'console.log(JSON.stringify(process.argv.slice(2))); process.exitCode=4;');
  const config = path.join(root, 'Tools/zagent/backend.json');
  await fs.writeFile(config, JSON.stringify({ backend: 'vault', vaultRoot: '../../vault with spaces' }));
  const run = () => spawnSync('pwsh', ['-NoProfile', '-File', launcher, 'comment', 'FD-1', '--text', 'two words', '--json'], { encoding: 'utf8', windowsHide: true });
  let result = run();
  assert.equal(result.status, 4, result.stderr);
  assert.deepEqual(JSON.parse(result.stdout), ['--vault', path.join(root, 'vault with spaces'), 'comment', 'FD-1', '--text', 'two words', '--json']);
  await fs.writeFile(config, '{broken');
  result = run(); assert.equal(result.status, 1); assert.match(result.stderr, /JSON/i);
});
