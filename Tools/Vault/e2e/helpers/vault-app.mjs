import { execFile as callbackExecFile } from 'node:child_process';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { createVaultServer } from '../../server/http.mjs';

const execFile = promisify(callbackExecFile);
const fixture = fileURLToPath(new URL('../../fixtures/snapshot/vault/', import.meta.url));

/** Windows can keep handles open briefly after a killed server or git child; retry with backoff, then give up — a leftover temp dir is harmless, failing a green test is not. */
export async function removeTempVault(root) {
  for (let attempt = 1; attempt <= 5; attempt += 1) {
    try {
      await rm(root, { recursive: true, force: true });
      return;
    } catch {
      if (attempt === 5) return;
      await new Promise(resolve => setTimeout(resolve, 200 * attempt));
    }
  }
}

/** Each browser test owns a disposable vault and Git repository. */
export async function withProjectApp(callback, { readOnly = false, commit = false, configure } = {}) {
  const root = await mkdtemp(path.join(os.tmpdir(), 'vault-project-browser-'));
  let server;
  try {
    await cp(fixture, root, { recursive: true });
    const configPath = path.join(root, 'vault.config.json');
    const config = JSON.parse(await readFile(configPath, 'utf8'));
    config.writes = { enabled: !readOnly, commit };
    await writeFile(configPath, JSON.stringify(config));
    await configure?.(root, config);
    for (const args of [['init', '-b', 'main'], ['config', 'user.name', 'Browser Test'], ['config', 'user.email', 'test@example.invalid'], ['add', '.'], ['commit', '-m', 'Initial fixture']]) {
      await execFile('git', ['-C', root, ...args], { windowsHide: true });
    }
    server = await createVaultServer({ vaultRoot: root, webRoot: fileURLToPath(new URL('../../artifacts/project-web/', import.meta.url)) });
    const url = await server.start();
    return await callback({ root, url, server });
  } finally {
    await server?.close();
    await removeTempVault(root);
  }
}
