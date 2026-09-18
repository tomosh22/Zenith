import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { issueDetailAsync, projectDetail } from '../server/project-api.mjs';
import { dispatchProjectAction } from '../server/project-actions.mjs';
import { expectedRevision } from '../cli/write.mjs';

let root;

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-project-actions-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Project action test']);
  await git(root, ['config', 'user.email', 'project-actions@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
});

afterEach(async () => { if (root) await rm(root, { recursive: true, force: true }); });

test('project.create atomically scaffolds a default workflow and board', async () => {
  const index = await loadVault(root);
  const configRevision = await readRevision(root, 'vault.config.json');
  const result = await dispatchProjectAction(index, {
    action: 'project.create',
    project: 'NEW',
    data: { key: 'NEW', name: 'New project', description: 'Created by test' },
    revisions: { 'vault.config.json': configRevision },
    operationId: 'create-new-project',
  });
  expect(result.result).toMatchObject({ key: 'NEW', created: true });
  const next = await loadVault(root);
  expect(projectDetail(next, 'NEW').statuses).toHaveLength(5);
  expect(projectDetail(next, 'NEW').board.columns.length).toBeGreaterThan(0);
  expect(await readFile(path.join(root, 'NEW', 'workflow.md'), 'utf8')).toContain('statuses');
  const retry = await dispatchProjectAction(next, {
    action: 'project.create',
    project: 'NEW',
    data: { key: 'NEW', name: 'New project', description: 'Created by test' },
    revisions: { 'vault.config.json': configRevision },
    operationId: 'create-new-project',
  });
  expect(retry).toEqual(result);
});

test('issue.update preserves body on a partial property edit', async () => {
  const index = await loadVault(root);
  const issue = index.getTicket('ZEN-7');
  const beforeBody = issue.bodyText;
  const result = await dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-7',
    data: { title: 'Updated title only' },
    revisions: { [issue.relativePath]: expectedRevision(issue) },
    operationId: 'partial-issue-update',
  });
  expect(result.result).toMatchObject({ key: 'ZEN-7', updated: true });
  expect((await loadVault(root)).getTicket('ZEN-7').bodyText).toBe(beforeBody);
});

test('issue actions accept a parent ticket that carries no type', async () => {
  await writeFile(path.join(root, 'ZEN', 'tickets', 'ZEN-900.md'), '---\nkey: ZEN-900\ntitle: Typeless parent\nstatus: To Do\n---\nBody\n');
  const index = await loadVault(root);
  const created = await dispatchProjectAction(index, {
    action: 'issue.create',
    project: 'ZEN',
    data: { title: 'Child of a typeless parent', type: 'TASK', parent: 'ZEN-900' },
    operationId: 'create-under-typeless',
  });
  expect(created.result).toMatchObject({ created: true });
  const issue = index.getTicket('ZEN-7');
  const updated = await dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-7',
    data: { parent: 'ZEN-900' },
    revisions: { [issue.relativePath]: expectedRevision(issue) },
    operationId: 'update-under-typeless',
  });
  expect(updated.result).toMatchObject({ key: 'ZEN-7', updated: true });
  const next = await loadVault(root);
  expect(next.getTicket('ZEN-7').metadata.parent).toBe('ZEN-900');
});

test('issue detail includes committed activity after the imported snapshot', async () => {
  const index = await loadVault(root);
  const ticket = index.getTicket('ZEN-7');
  await writeFile(path.join(root, ticket.relativePath), ticket.rawBytes.toString('utf8') + '\nExternal history entry.\n');
  await git(root, ['add', ticket.relativePath]);
  await git(root, ['commit', '-qm', 'External edit']);
  const next = await loadVault(root);
  const detail = await issueDetailAsync(next, 'ZEN-7', { root });
  expect(detail.history.some(row => row.field === 'description' && row.toValue?.includes('External history entry.'))).toBe(true);
});
