import { execFile as execFileCallback, spawn } from 'node:child_process';
import { cp, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { expect, test } from '@playwright/test';
import { removeTempVault } from './helpers/vault-app.mjs';

const execFile = promisify(execFileCallback);
const here = path.resolve(new URL('.', import.meta.url).pathname.replace(/^\/(?:[A-Za-z]:)/, match => match.slice(1)));
const httpScript = path.resolve(here, '..', 'server', 'http.mjs');
const vaultFixture = path.resolve(here, '..', 'fixtures', 'snapshot', 'vault');

async function freePort() {
  const listener = net.createServer();
  await new Promise((resolve, reject) => { listener.once('error', reject); listener.listen(0, '127.0.0.1', resolve); });
  const port = listener.address().port;
  await new Promise(resolve => listener.close(resolve));
  return port;
}

async function git(root, ...args) {
  await execFile('git', ['-C', root, ...args], { windowsHide: true });
}

async function makeVault({ writes = false, links = false, twoStatuses = false, crlf = false, commit = false } = {}) {
  const root = await mkdtemp(path.join(os.tmpdir(), 'zenith-vault-app-'));
  await mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await writeFile(path.join(root, 'vault.config.json'), `${JSON.stringify({
    boardRoots: [{ project: 'FD', path: 'FD' }],
    writes: writes ? { enabled: true, commit } : { enabled: false },
  }, null, 2)}\n`);
  const eol = crlf ? '\r\n' : '\n';
  const documentText = (text, bom = false) => `${bom ? '\uFEFF' : ''}${text}`.replaceAll('\n', eol);
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), documentText('---\nkey: FD-1\ntitle: First ticket\nstatus: To Do\npriority: HIGH\n---\n\n# First ticket\n\nA safe local document.\n', crlf));
  if (links) await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), documentText('---\nkey: FD-2\ntitle: Linked ticket\nstatus: To Do\npriority: LOW\n---\n\nSee [[FD-1|the first ticket]].\n'));
  else if (twoStatuses) await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), documentText('---\nkey: FD-2\ntitle: Second ticket\nstatus: In Progress\npriority: LOW\n---\n\nA second lane.\n'));
  await git(root, 'init', '-b', 'main');
  await git(root, 'config', 'user.name', 'Vault Browser Test');
  await git(root, 'config', 'user.email', 'vault-browser-test@example.invalid');
  await git(root, 'add', '.');
  await git(root, 'commit', '-m', 'initial vault');
  return root;
}

async function withFullReadOnlyApp(callback) {
  const root = await mkdtemp(path.join(os.tmpdir(), 'zenith-vault-app-full-'));
  await cp(vaultFixture, root, { recursive: true });
  const server = await startServer(root);
  try { return await callback({ root, url: server.url }); }
  finally {
    server.child.kill();
    await new Promise(resolve => server.child.once('close', resolve));
    await removeTempVault(root);
  }
}

async function startServer(root) {
  const port = await freePort();
  const child = spawn(process.execPath, [httpScript, '--vault', root, '--port', String(port), '--web-root', path.resolve(here, '..', 'artifacts', 'project-web')], {
    cwd: path.dirname(httpScript),
    env: { ...process.env, NODE_NO_WARNINGS: '1' },
    windowsHide: true,
  });
  let stdout = '';
  let stderr = '';
  child.stderr.on('data', chunk => { stderr += chunk.toString(); });
  const address = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`HTTP server did not start: ${stderr}`)), 15_000);
    child.once('error', error => { clearTimeout(timer); reject(error); });
    child.stdout.on('data', chunk => {
      stdout += chunk.toString();
      for (const line of stdout.split(/\r?\n/)) {
        try {
          const payload = JSON.parse(line);
          if (payload.url) { clearTimeout(timer); resolve(payload); return; }
        } catch { /* wait for the complete JSON line */ }
      }
    });
  });
  return { child, url: address.url };
}

async function withApp(options, callback) {
  const root = await makeVault(options);
  const server = await startServer(root);
  try { return await callback({ root, url: server.url }); }
  finally {
    server.child.kill();
    await new Promise(resolve => server.child.once('close', resolve));
    await removeTempVault(root);
  }
}

test.describe('Vault browser app', () => {
  test('loads the read model, searches the tree, previews Markdown, and shows the board', async ({ page }) => {
    await withApp({ writes: false }, async ({ url }) => {
      await page.goto(`${url}?workspace=documents`);
      await expect(page.getByText('Zenith Vault', { exact: true })).toBeVisible();
      await expect(page.getByText('Read only')).toBeVisible();
      await expect(page.getByRole('button', { name: 'Board' })).toBeVisible();

      await page.getByRole('textbox', { name: 'Search vault' }).fill('First ticket');
      const ticketButton = page.locator('button[title="FD/tickets/FD-1.md"]');
      await expect(ticketButton).toBeVisible();
      await ticketButton.click();
      await expect(page.getByText('FD/tickets/FD-1.md')).toBeVisible();
      await expect(page.locator('.markdown')).toContainText('First ticket');
      await expect(page.getByRole('button', { name: 'Save' })).toBeDisabled();
      await expect(page.getByRole('combobox', { name: 'Ticket status' })).toBeDisabled();

      await page.getByRole('button', { name: 'Board' }).click();
      await expect(page.getByTestId('board')).toBeVisible();
      await expect(page.getByTestId('ticket-card')).toContainText('FD-1');
      await page.screenshot({ path: 'artifacts/app.png', fullPage: true });
      await page.getByRole('button', { name: 'Backlog' }).click();
      await expect(page.getByTestId('backlog')).toBeVisible();
      await page.getByRole('button', { name: 'Roadmap' }).click();
      await expect(page.getByTestId('roadmap')).toBeVisible();
    });
  });

  test('edits through CodeMirror and saves through the managed writer', async ({ page }) => {
    await withApp({ writes: true }, async ({ root, url }) => {
      await page.goto(`${url}?workspace=documents`);
      await expect(page.locator('.health-row')).toContainText('Live');
      await page.locator('button[title="FD/tickets/FD-1.md"]').click();
      const editor = page.getByTestId('editor').locator('.cm-content');
      await expect(editor).toContainText('First ticket');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nBrowser save sentinel');
      const save = page.getByRole('button', { name: 'Save' });
      await expect(save).toBeEnabled();
      await save.click();
      await expect(page.getByText('Saved.')).toBeVisible({ timeout: 15_000 });
      await expect(page.locator('[role="alert"]')).toHaveCount(0);
      await expect.poll(async () => (await readFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), 'utf8'))).toContain('Browser save sentinel');
    });
  });

  test('round-trips a CRLF/BOM document on a no-op save and preserves its bytes on edit', async ({ page }) => {
    await withApp({ writes: true, crlf: true }, async ({ root, url }) => {
      const file = path.join(root, 'FD', 'tickets', 'FD-1.md');
      const before = await readFile(file);
      await page.goto(`${url}?workspace=documents`);
      const session = await page.request.get(`${url}/api/session`).then(response => response.json());
      const document = await page.request.get(`${url}/api/document?path=${encodeURIComponent('FD/tickets/FD-1.md')}`).then(response => response.json());
      const noOp = await page.request.put(`${url}/api/document`, {
        headers: { 'content-type': 'application/json', 'x-csrf-token': session.csrfToken },
        data: { path: document.path, bytesBase64: document.bytesBase64, revision: document.revision },
      });
      expect(noOp.ok()).toBeTruthy();
      expect(await readFile(file)).toEqual(before);

      await page.locator('button[title="FD/tickets/FD-1.md"]').click();
      const editor = page.getByTestId('editor').locator('.cm-content');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nCRLF edit sentinel');
      await page.getByRole('button', { name: 'Save' }).click();
      await expect(page.getByText('Saved.')).toBeVisible({ timeout: 15_000 });
      const after = await readFile(file);
      expect(after.subarray(0, 3)).toEqual(Buffer.from([0xef, 0xbb, 0xbf]));
      expect(after.toString('utf8')).toContain('\r\nCRLF edit sentinel');
      expect(after.toString('utf8').replace(/\r\n/g, '')).not.toContain('\n');
    });
  });

  test('keeps local edits during an external change and offers a disk reload', async ({ page }) => {
    await withApp({ writes: true }, async ({ root, url }) => {
      await page.goto(`${url}?workspace=documents`);
      await page.locator('button[title="FD/tickets/FD-1.md"]').click();
      const editor = page.getByTestId('editor').locator('.cm-content');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nLocal unsaved text');
      await expect(page.getByText('Unsaved changes')).toBeVisible();
      const file = path.join(root, 'FD', 'tickets', 'FD-1.md');
      const external = (await readFile(file, 'utf8')).replace('A safe local document.', 'External disk edit.');
      await writeFile(file, external);
      await expect(page.getByRole('alert')).toContainText('File changed on disk', { timeout: 15_000 });
      await expect(editor).toContainText('Local unsaved text');
      await page.getByRole('button', { name: 'Reload disk' }).click();
      await expect(editor).toContainText('External disk edit.');
      await expect(editor).not.toContainText('Local unsaved text');
    });
  });

  test('does not apply a delayed save response to a document selected meanwhile', async ({ page }) => {
    await withApp({ writes: true, links: true }, async ({ url }) => {
      let release;
      const held = new Promise(resolve => { release = resolve; });
      await page.route('**/api/document', async route => {
        if (route.request().method() === 'PUT') await held;
        await route.continue();
      });
      await page.goto(`${url}?workspace=documents`);
      await page.locator('button[title="FD/tickets/FD-1.md"]').click();
      const editor = page.getByTestId('editor').locator('.cm-content');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nDelayed save text');
      const save = page.getByRole('button', { name: 'Save' });
      await save.click();
      await page.locator('button[title="FD/tickets/FD-2.md"]').click();
      await expect(page.getByText('FD/tickets/FD-2.md')).toBeVisible();
      await expect(page.getByTestId('editor').locator('.cm-content')).toContainText('Linked ticket');
      release();
      await expect(page.getByText('All changes saved')).toBeVisible({ timeout: 15_000 });
      await expect(page.getByText('FD/tickets/FD-2.md')).toBeVisible();
      await page.unroute('**/api/document');
    });
  });

  test('opens document deep links and ignores an older read after selecting another document', async ({ page }) => {
    await withApp({ writes: false, links: true }, async ({ url }) => {
      let release;
      const held = new Promise(resolve => { release = resolve; });
      await page.route('**/api/document?path=FD%2Ftickets%2FFD-1.md', async route => { await held; await route.continue(); });
      try {
        await page.goto(`${url}?workspace=documents&path=FD%2Ftickets%2FFD-1.md`);
        await page.locator('button[title="FD/tickets/FD-2.md"]').click();
        await expect(page.getByTestId('editor').locator('.cm-content')).toContainText('Linked ticket');
        const delayedResponse = page.waitForResponse(response => response.url().includes('path=FD%2Ftickets%2FFD-1.md'));
        release();
        await delayedResponse;
        await expect(page.getByTestId('editor').locator('.cm-content')).toContainText('Linked ticket');
        await expect(page.getByText('FD/tickets/FD-2.md')).toBeVisible();
      } finally { release(); }
    });
  });

  test('follows wiki links, shows backlinks, and offers CodeMirror completions', async ({ page }) => {
    await withApp({ writes: false, links: true }, async ({ url }) => {
      await page.goto(`${url}?workspace=documents`);
      await page.locator('button[title="FD/tickets/FD-1.md"]').click();
      await expect(page.getByTestId('backlinks')).toContainText('FD-2');
      const firstEditor = page.getByTestId('editor').locator('.cm-content');
      await firstEditor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nDraft preserved while following links');
      await page.getByTestId('backlinks').getByRole('button', { name: 'FD-2' }).click();
      await expect(page.getByText('FD/tickets/FD-2.md')).toBeVisible();
      await expect(page.locator('.markdown a[href="#wiki-FD-1"]')).toBeVisible();
      await page.locator('.markdown a[href="#wiki-FD-1"]').click();
      await expect(page.getByText('FD/tickets/FD-1.md')).toBeVisible();
      await expect(page.getByTestId('editor').locator('.cm-content')).toContainText('Draft preserved while following links');
      const editor = page.getByTestId('editor').locator('.cm-content');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\n[[FD');
      await page.keyboard.press('Control+Space');
      await expect(page.locator('.cm-tooltip-autocomplete')).toContainText('FD-1', { timeout: 5_000 });
    });
  });

  test('read-only board drag is refused without changing the ticket', async ({ page }) => {
    await withApp({ writes: false, twoStatuses: true }, async ({ root, url }) => {
      await page.goto(`${url}?workspace=documents`);
      await page.getByRole('button', { name: 'Board' }).click();
      await expect(page.locator('.board-column')).toHaveCount(2);
      const card = page.getByTestId('ticket-card').filter({ hasText: 'FD-1' });
      await card.dragTo(page.locator('.board-column').nth(0));
      await expect(card).toContainText('FD-1');
      await expect.poll(async () => (await readFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), 'utf8'))).toContain('status: To Do');
    });
  });

  test('writable board drag uses the managed transition and records a commit', async ({ page }) => {
    await withApp({ writes: true, twoStatuses: true, commit: true }, async ({ root, url }) => {
      const beforeHead = (await execFile('git', ['-C', root, 'rev-parse', 'HEAD'], { windowsHide: true })).stdout.trim();
      await page.goto(`${url}?workspace=documents`);
      await page.getByRole('button', { name: 'Board' }).click();
      await expect(page.locator('.board-column')).toHaveCount(2);
      const card = page.getByTestId('ticket-card').filter({ hasText: 'FD-1' });
      const progressColumn = page.locator('.board-column').filter({ hasText: 'In Progress' });
      await card.dragTo(progressColumn);
      await expect.poll(async () => (await readFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), 'utf8'))).toMatch(/status:\s+(?:"In Progress"|In Progress)/);
      await expect.poll(async () => (await execFile('git', ['-C', root, 'rev-parse', 'HEAD'], { windowsHide: true })).stdout.trim()).not.toBe(beforeHead);
      const message = (await execFile('git', ['-C', root, 'log', '-1', '--format=%B'], { windowsHide: true })).stdout;
      expect(message).toContain('Vault-Operation:');
    });
  });

  test('full read-only vault renders multiple board lanes for visual review', async ({ page }) => {
    await withFullReadOnlyApp(async ({ url }) => {
      await page.goto(`${url}?workspace=documents`);
      await page.getByRole('button', { name: 'Board' }).click();
      await expect(page.getByTestId('board')).toBeVisible();
      const lanes = await page.locator('.board-column').count();
      expect(lanes).toBeGreaterThan(1);
      await page.screenshot({ path: 'artifacts/app.png', fullPage: true });
    });
  });
});

