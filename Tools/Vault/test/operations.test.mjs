import { test } from 'vitest';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { executeOperation, saveDocument, readRevision, recoverOperations, resolveAuthority, git } from '../server/operations.mjs';
import { patchFields } from '../server/text-edit.mjs';
const childPath = fileURLToPath(new URL('./helpers/operation-child.mjs', import.meta.url));

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-operations-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({ boardRoots: [{ project: 'FD', path: 'FD' }], writes: { enabled: true, testFaults: true } }));
  for (const key of ['FD-1', 'FD-2']) await fs.writeFile(path.join(root, `FD/tickets/${key}.md`), `---\nkey: ${key}\ntitle: Before\nstatus: To Do\n---\nBody\n`);
  await fs.writeFile(path.join(root, 'unrelated.txt'), 'before');
  await git(root, ['init', '-q']); await git(root, ['config', 'user.email', 'vault-test@example.invalid']); await git(root, ['config', 'user.name', 'Vault tests']);
  await git(root, ['add', '.']); await git(root, ['commit', '-qm', 'fixture']);
  return root;
}
const update = (root, id, title = 'After') => executeOperation(root, { id, kind: 'test update', prepare: async index => {
  const record = index.getTicket('FD-1');
  return { changes: [{ path: record.relativePath, bytes: patchFields(record.rawBytes, { title }) }], result: { title } };
} });
function child(root, id, extraEnv = {}) {
  return new Promise((resolve, reject) => {
    const processHandle = spawn(process.execPath, [childPath, root, id], { windowsHide: true, env: { ...process.env, ...extraEnv } });
    let stderr = ''; processHandle.stderr.on('data', value => { stderr += value; });
    processHandle.stdout.resume(); processHandle.on('error', reject); processHandle.on('close', (code, signal) => resolve({ code, signal, stderr }));
  });
}

test('commits only operation paths and retries an operation ID without repeating it', async t => {
  const root = await fixture(t);
  await fs.writeFile(path.join(root, 'unrelated.txt'), 'user staged'); await git(root, ['add', 'unrelated.txt']);
  const operation = await update(root, 'once');
  assert.equal(operation.result.title, 'After'); assert.match(operation.commit, /^[a-f0-9]+$/);
  assert.equal(await git(root, ['show', 'HEAD:unrelated.txt']), 'before');
  assert.equal(await git(root, ['diff', '--cached', '--name-only']), 'unrelated.txt');
  assert.deepEqual(await update(root, 'once'), operation);
  assert.equal((await git(root, ['log', '--oneline'])).split('\n').length, 2);
});

test('stale editor saves and invalid proposed graphs preserve authoritative bytes', async t => {
  const root = await fixture(t), relative = 'FD/tickets/FD-1.md';
  const before = await fs.readFile(path.join(root, relative)), revision = await readRevision(root, relative);
  await update(root, 'newer');
  await assert.rejects(saveDocument(root, relative, before, revision), e => e.code === 'STALE');
  const fresh = await readRevision(root, relative);
  await assert.rejects(saveDocument(root, relative, Buffer.from('---\nkey: [broken\n---\n'), fresh), e => e.code === 'INVALID');
  assert.match((await fs.readFile(path.join(root, relative))).toString(), /title: "After"/);
});

test('path escape and edits made during preparation fail closed', async t => {
  const root = await fixture(t);
  await assert.rejects(executeOperation(root, { kind: 'escape', prepare: async () => ({ changes: [{ path: '../escape.md', bytes: Buffer.from('bad') }] }) }), e => e.code === 'PATH');
  await assert.rejects(executeOperation(root, { kind: 'race', prepare: async index => {
    const ticket = index.getTicket('FD-1');
    await fs.writeFile(path.join(root, ticket.relativePath), patchFields(ticket.rawBytes, { title: 'External' }));
    return { changes: [{ path: ticket.relativePath, bytes: patchFields(ticket.rawBytes, { title: 'Overwrite' }) }] };
  } }), e => e.code === 'STALE');
  assert.match(await fs.readFile(path.join(root, 'FD/tickets/FD-1.md'), 'utf8'), /External/);
});

test('hard kill after a partial multi-file write recovers once under a dead-owner lock', async t => {
  const root = await fixture(t);
  const crash = await child(root, 'killed-write', { VAULT_TEST_CRASH_AFTER_WRITE: '1' });
  assert.notEqual(crash.code, 0);
  assert.deepEqual(await recoverOperations(root), { recovered: 1 });
  for (const key of ['FD-1', 'FD-2']) assert.match(await fs.readFile(path.join(root, `FD/tickets/${key}.md`), 'utf8'), /After crash/);
  assert.deepEqual(await recoverOperations(root), { recovered: 0 });
  assert.equal((await git(root, ['log', '--oneline'])).split('\n').length, 2);
});

test('hard kill after the commit does not duplicate a successful commit', async t => {
  const root = await fixture(t);
  assert.notEqual((await child(root, 'killed-commit', { VAULT_TEST_CRASH_AFTER_COMMIT: '1' })).code, 0);
  const committed = await git(root, ['rev-parse', 'HEAD']);
  assert.deepEqual(await recoverOperations(root), { recovered: 1 });
  assert.equal(await git(root, ['rev-parse', 'HEAD']), committed);
  assert.equal(await git(root, ['status', '--porcelain']), '');
});

test('recovery refuses a third-party edit instead of overwriting it', async t => {
  const root = await fixture(t);
  await child(root, 'conflict', { VAULT_TEST_CRASH_AFTER_WRITE: '1' });
  const target = path.join(root, 'FD/tickets/FD-1.md');
  await fs.writeFile(target, 'Human recovery data');
  await assert.rejects(recoverOperations(root), e => e.code === 'RECOVERY_CONFLICT');
  assert.equal(await fs.readFile(target, 'utf8'), 'Human recovery data');
});

test('a live process lock cannot be stolen and dead-owner recovery permits another writer', async t => {
  const root = await fixture(t);
  const holder = spawn(process.execPath, [childPath, root, 'holder'], { windowsHide: true, env: { ...process.env, VAULT_TEST_HOLD: '1' } });
  t.onTestFinished(() => holder.kill('SIGKILL'));
  holder.stderr.resume();
  const closed = new Promise(resolve => holder.on('close', resolve));
  await new Promise((resolve, reject) => { holder.stdout.on('data', b => { if (b.toString().includes('LOCK_HELD')) resolve(); }); holder.on('error', reject); holder.on('close', () => reject(new Error('Holder exited before acquiring lock'))); });
  await assert.rejects(update(root, 'competitor'), e => e.code === 'LOCKED');
  holder.kill('SIGKILL'); await closed;
  assert.equal((await update(root, 'after-holder')).result.title, 'After');
});

test('a second worktree resolves writes to the registered authority', async t => {
  const root = await fixture(t);
  await update(root, 'register');
  const other = path.join(root, 'other-checkout');
  await git(root, ['worktree', 'add', '-q', '-b', 'other', other]);
  assert.equal((await resolveAuthority(other)).toLowerCase(), root.toLowerCase());
  await update(other, 'from-other', 'Authoritative');
  assert.match(await fs.readFile(path.join(root, 'FD/tickets/FD-1.md'), 'utf8'), /Authoritative/);
  assert.doesNotMatch(await fs.readFile(path.join(other, 'FD/tickets/FD-1.md'), 'utf8'), /Authoritative/);
});

test('raw editor status changes obey the configured workflow graph', async t => {
  const root = await fixture(t);
  await fs.writeFile(path.join(root, 'FD/workflow.md'), '---\nkind: workflow\nstatuses:\n  - {id: todo, name: To Do, category: TODO}\n  - {id: done, name: Done, category: DONE}\ntransitions: []\n---\n');
  await git(root, ['add', 'FD/workflow.md']); await git(root, ['commit', '-qm', 'Workflow']);
  const relative = 'FD/tickets/FD-1.md', original = await fs.readFile(path.join(root, relative));
  await assert.rejects(saveDocument(root, relative, patchFields(original, { status: 'Done' }), await readRevision(root, relative)), e => e.code === 'INVALID' && /Workflow/.test(e.message));
  assert.deepEqual(await fs.readFile(path.join(root, relative)), original);
});

test('a directory junction cannot redirect an operation outside the vault', async t => {
  const root = await fixture(t), external = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-outside-'));
  t.onTestFinished(() => fs.rm(external, { recursive: true, force: true }));
  await fs.symlink(external, path.join(root, 'FD/tickets/escape'), process.platform === 'win32' ? 'junction' : 'dir');
  await assert.rejects(executeOperation(root, { kind: 'escape', prepare: async () => ({ changes: [{ path: 'FD/tickets/escape/FD-9.md', bytes: Buffer.from('Never written') }] }) }), e => e.code === 'PATH');
  assert.deepEqual(await fs.readdir(external), []);
});

test('a reloaded external edit can be saved without requiring a clean working tree', async t => {
  const root = await fixture(t), relative = 'FD/tickets/FD-1.md';
  const original = await fs.readFile(path.join(root, relative));
  const external = patchFields(original, { title: 'Human edit' });
  await fs.writeFile(path.join(root, relative), external);
  const revision = await readRevision(root, relative);
  const result = await saveDocument(root, relative, Buffer.concat([external, Buffer.from('More text\n')]), revision);
  assert.ok(result.commit);
  assert.match(await git(root, ['show', `HEAD:${relative}`]), /Human edit/);
  assert.match(await git(root, ['show', `HEAD:${relative}`]), /More text/);
  const savedBytes = await fs.readFile(path.join(root, relative));
  assert.equal(result.revision.size, savedBytes.length);
  await fs.appendFile(path.join(root, relative), 'Later external writer\n');
  await assert.rejects(saveDocument(root, relative, savedBytes, result.revision), e => e.code === 'STALE');
});

test('an explicit nested read-only fixture is not redirected into the production authority', async t => {
  const root = await fixture(t);
  await update(root, 'register-main');
  const nested = path.join(root, 'fixtures/frozen');
  await fs.mkdir(nested, { recursive: true });
  await fs.writeFile(path.join(nested, 'vault.config.json'), JSON.stringify({ boardRoots: [], writes: { enabled: false } }));
  assert.equal(await resolveAuthority(nested), await fs.realpath(nested));
  await assert.rejects(update(nested, 'must-not-write'), e => e.code === 'READ_ONLY');
});

test('commit hashing applies Git attributes while preserving raw working bytes', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-git-filters-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({ boardRoots: [{ project: 'FD', path: 'FD' }], writes: { enabled: true } }));
  await fs.writeFile(path.join(root, '.gitattributes'), '* text=auto\nFD/tickets/FD-2.md -text\n');
  const initialText = Buffer.from('---\r\nkey: FD-1\r\ntitle: Before\r\nstatus: To Do\r\n---\r\nBody');
  const initialRaw = Buffer.from('---\r\nkey: FD-2\r\ntitle: Before\r\nstatus: To Do\r\n---\r\nBody');
  await fs.writeFile(path.join(root, 'FD/tickets/FD-1.md'), initialText);
  await fs.writeFile(path.join(root, 'FD/tickets/FD-2.md'), initialRaw);
  await git(root, ['init', '-q']);
  await git(root, ['config', 'core.autocrlf', 'true']);
  await git(root, ['config', 'user.email', 'vault-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault tests']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'fixture']);

  const textPath = 'FD/tickets/FD-1.md';
  const rawPath = 'FD/tickets/FD-2.md';
  const nextText = Buffer.from('---\r\nkey: FD-1\r\ntitle: After text\r\nstatus: To Do\r\n---\r\nBody');
  const nextRaw = Buffer.from('---\r\nkey: FD-2\r\ntitle: After raw\r\nstatus: To Do\r\n---\r\nBody');
  await saveDocument(root, textPath, nextText, await readRevision(root, textPath));
  await saveDocument(root, rawPath, nextRaw, await readRevision(root, rawPath));

  assert.deepEqual(await fs.readFile(path.join(root, textPath)), nextText);
  assert.deepEqual(await fs.readFile(path.join(root, rawPath)), nextRaw);
  assert.equal(await git(root, ['show', `HEAD:${textPath}`]), nextText.toString().replaceAll('\r\n', '\n'));
  assert.equal(await git(root, ['show', `HEAD:${rawPath}`]), nextRaw.toString());
  assert.equal(await git(root, ['status', '--porcelain']), '');
});
