import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { git, executeOperation } from '../server/operations.mjs';
import { patchFields } from '../server/text-edit.mjs';
import { appendGitHistory } from '../cli/git-history.mjs';

test('new history comes from board commits while frozen events remain untouched', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-history-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({ boardRoots: [{ project: 'FD', path: 'FD' }], writes: { enabled: true } }));
  await fs.writeFile(path.join(root, 'FD/tickets/FD-1.md'), '---\nkey: FD-1\ntitle: Old\nstatus: To Do\n---\nBody');
  await git(root, ['init', '-q']); await git(root, ['config', 'user.name', 'Vault test actor']); await git(root, ['config', 'user.email', 'test@example.invalid']);
  await git(root, ['add', '.']); await git(root, ['commit', '-qm', 'Migration']);
  await executeOperation(root, { kind: 'title', prepare: async index => {
    const ticket = index.getTicket('FD-1'); return { changes: [{ path: ticket.relativePath, bytes: patchFields(ticket.rawBytes, { title: 'New' }) }], result: {} };
  } });
  const frozen = [{ field: 'status', fromValue: null, toValue: 'To Do', actor: 'legacy', createdAt: '2020-01-01T00:00:00.000Z' }];
  const result = await appendGitHistory(root, 'FD-1', frozen);
  assert.equal(result.length, 2);
  assert.deepEqual({ ...result[0], createdAt: null }, { field: 'title', fromValue: 'Old', toValue: 'New', actor: 'Vault test actor', createdAt: null });
  assert.deepEqual(result[1], frozen[0]);
  assert.equal((await appendGitHistory(root, 'FD-1', frozen, 1)).length, 1);
  const ticketPath = path.join(root, 'FD/tickets/FD-1.md');
  await fs.writeFile(ticketPath, patchFields(await fs.readFile(ticketPath), { title: 'External commit' }));
  await git(root, ['add', 'FD/tickets/FD-1.md']);
  await git(root, ['commit', '-qm', 'Human edit outside app']);
  const externalHistory = await appendGitHistory(root, 'FD-1', frozen, 1);
  assert.equal(externalHistory[0].toValue, 'External commit');
  await fs.appendFile(ticketPath, '\n');
  await git(root, ['add', 'FD/tickets/FD-1.md']);
  await git(root, ['commit', '-qm', 'Preserve final newline history']);
  const newlineHistory = await appendGitHistory(root, 'FD-1', frozen, 1);
  assert.equal(newlineHistory[0].field, 'description');
  assert.equal(newlineHistory[0].toValue, `${newlineHistory[0].fromValue}\n`);
});
