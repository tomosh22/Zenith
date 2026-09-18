import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile, mkdir } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchProjectAction } from '../server/project-actions.mjs';

let root;
beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-action-safety-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']); await git(root, ['config', 'user.name', 'Action test']); await git(root, ['config', 'user.email', 'test@example.invalid']);
  await git(root, ['add', '.']); await git(root, ['commit', '-qm', 'Fixture']);
});
afterEach(async () => { if (root) await rm(root, { recursive: true, force: true }); });

test('issue allocation is project-scoped and never overwrites existing tickets', async () => {
  const index = await loadVault(root);
  const previous = new Map([...index.tickets.values()].map(ticket => [ticket.relativePath, ticket.hash]));
  const request = { action: 'issue.create', project: 'ZEN', data: { title: 'A new ticket', body: 'Description text', type: 'TASK' }, operationId: 'safe-create', revisions: {} };
  const result = await dispatchProjectAction(index, request);
  expect(result.result.key).toBe('ZEN-23');
  const after = await loadVault(root);
  for (const [file, hash] of previous) expect(after.getFile(file)?.hash, file).toBe(hash);
  expect(after.getTicket('ZEN-23').bodyText).toContain('Description text');
  await dispatchProjectAction(after, request);
  expect((await loadVault(root)).tickets.size).toBe(index.tickets.size + 1);
});

test('an update without the opened revision is rejected without modifying an issue', async () => {
  const index = await loadVault(root);
  const before = index.getTicket('ZEN-7').hash;
  await expect(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', data: { title: 'Must not save' }, revisions: {}, operationId: 'missing-revision' })).rejects.toMatchObject({ code: expect.stringMatching(/STALE|INVALID/) });
  expect((await loadVault(root)).getTicket('ZEN-7').hash).toBe(before);
});

test('project creation refuses an existing unindexed scaffold file', async () => {
  await mkdir(path.join(root, 'NEW'), { recursive: true });
  const owned = path.join(root, 'NEW', 'workflow.md');
  await writeFile(owned, 'Existing user-owned file.\n');
  const index = await loadVault(root);
  const originalConfig = await readFile(path.join(root, 'vault.config.json'), 'utf8');
  await expect(dispatchProjectAction(index, { action: 'project.create', project: 'NEW', data: { key: 'NEW', name: 'New project' }, revisions: { 'vault.config.json': await readRevision(root, 'vault.config.json') }, operationId: 'existing-scaffold' })).rejects.toBeTruthy();
  expect(await readFile(owned, 'utf8')).toBe('Existing user-owned file.\n');
  expect(await readFile(path.join(root, 'vault.config.json'), 'utf8')).toBe(originalConfig);
});

test('a partial property update preserves the description and unrelated properties', async () => {
  const index = await loadVault(root);
  const ticket = index.getTicket('ZEN-7');
  await dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', data: { title: 'Renamed safely' }, revisions: { [ticket.relativePath]: await readRevision(root, ticket.relativePath) }, operationId: 'partial-update' });
  const after = (await loadVault(root)).getTicket('ZEN-7');
  expect(after.metadata.title).toBe('Renamed safely');
  expect(after.bodyText).toBe(ticket.bodyText);
  for (const field of ['priority', 'labels', 'parent', 'sprint', 'assignee', 'type']) expect(after.metadata[field]).toEqual(ticket.metadata[field]);
});
