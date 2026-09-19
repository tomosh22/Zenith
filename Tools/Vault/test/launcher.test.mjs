import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import { spawnSync } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';

// There is exactly one backend now -- Zenith Vault -- so the launcher no
// longer reads a `Tools/zagent/backend.json` to choose one. It always
// dispatches to `Tools/Vault/cli/client.mjs` with `--vault <repoRoot>`,
// where `<repoRoot>` is computed from the launcher's OWN location (two
// levels up from `Tools/zagent`), never from a config file.
async function setUpFixtureRepo() {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-launcher-'));
  const launcher = path.join(root, 'Tools/zagent/zagent.ps1');
  await fs.mkdir(path.dirname(launcher), { recursive: true });
  await fs.mkdir(path.join(root, 'Tools/Vault/cli'), { recursive: true });
  await fs.copyFile(fileURLToPath(new URL('../../zagent/zagent.ps1', import.meta.url)), launcher);
  return { root, launcher };
}

test('PowerShell launcher dispatches straight to the Vault client with --vault <repoRoot>, and forwards arguments', async t => {
  const { root, launcher } = await setUpFixtureRepo();
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.writeFile(
    path.join(root, 'Tools/Vault/cli/client.mjs'),
    'console.log(JSON.stringify(process.argv.slice(2))); process.exitCode=4;',
  );

  const result = spawnSync(
    'pwsh',
    ['-NoProfile', '-File', launcher, 'comment', 'FD-1', '--text', 'two words', '--json'],
    { encoding: 'utf8', windowsHide: true },
  );

  // The launcher's own exit code IS the Node process's exit code --
  // nothing here invents its own besides the "client is missing" case
  // covered below.
  assert.equal(result.status, 4, result.stderr);
  assert.deepEqual(
    JSON.parse(result.stdout),
    ['--vault', root, 'comment', 'FD-1', '--text', 'two words', '--json'],
  );
});

test('PowerShell launcher fails closed with exit 1 when the Vault client is missing', async t => {
  const { root, launcher } = await setUpFixtureRepo();
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  // Deliberately no Tools/Vault/cli/client.mjs written.

  const result = spawnSync('pwsh', ['-NoProfile', '-File', launcher, 'queue'], {
    encoding: 'utf8',
    windowsHide: true,
  });

  assert.equal(result.status, 1);
  assert.match(result.stderr, /Vault client is missing/i);
});
