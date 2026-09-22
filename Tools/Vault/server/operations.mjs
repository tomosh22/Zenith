import { spawn } from 'node:child_process';
import { createHash, randomUUID } from 'node:crypto';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { loadVault } from './main.js';

const LOCK = 'refs/vault/operation-lock';
export const hashBytes = bytes => bytes === null ? null : createHash('sha256').update(bytes).digest('hex');
export class OperationError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}
const fail = (code, message) => { throw new OperationError(code, message); };

export function git(cwd, args, input, extraEnv = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn('git', args, { cwd, windowsHide: true, env: { ...process.env, ...extraEnv } });
    const out = [], err = [];
    child.stdout.on('data', b => out.push(b)); child.stderr.on('data', b => err.push(b));
    child.on('error', reject);
    child.on('close', code => code === 0 ? resolve(Buffer.concat(out).toString().trimEnd())
      : reject(new OperationError('GIT', Buffer.concat(err).toString().trim() || `git ${args[0]} exited ${code}`)));
    child.stdin.on('error', () => {});
    child.stdin.end(input);
  });
}

async function context(root) {
  root = await fs.realpath(root);
  const repo = await git(root, ['rev-parse', '--show-toplevel']);
  const common = path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
  return { root, repo, common, state: path.join(common, 'vault') };
}

async function readJson(file) {
  try { return JSON.parse(await fs.readFile(file, 'utf8')); } catch (e) { if (e.code === 'ENOENT') return null; throw e; }
}

export async function resolveAuthority(root) {
  let ctx;
  try { ctx = await context(root); } catch { return path.resolve(root); }
  const registration = await readJson(path.join(ctx.state, 'authority.json'));
  if (!registration) return ctx.root;
  const target = await context(registration.root);
  if (target.common.toLowerCase() !== ctx.common.toLowerCase()) fail('AUTHORITY', 'Registered vault belongs to another repository');
  // Only the equivalent vault location in another worktree is redirected.
  // Explicit nested fixtures must remain isolated after production cutover.
  const requestedRelative = path.relative(ctx.repo, ctx.root).replaceAll('\\', '/');
  const authorityRelative = path.relative(target.repo, target.root).replaceAll('\\', '/');
  if (requestedRelative.toLowerCase() !== authorityRelative.toLowerCase()) return ctx.root;
  return target.root;
}

// ★ ON WINDOWS A CORRECT `rename` STILL FAILS, AND IT IS NOT A BUG IN THE
// CALLER. Replacing an existing file needs every handle on the DESTINATION
// closed, and Defender and the search indexer both open a file moments after
// it is created -- so the rename below throws `EPERM` (sometimes `EACCES` or
// `EBUSY`) purely because something else is reading the file we are about to
// overwrite. It is transient by construction: the scanner lets go.
//
// Measured: across seven full `npm test` runs on this tree, three failed, each
// on a DIFFERENT write-path test. Only one of the three surfaced its cause --
// `EPERM ... rename '<...>.json.vault-<uuid>.tmp' -> '<...>.json'` under
// `.git/vault/operations/` -- because the other two asserted on an exit code
// and threw the error payload away; they are consistent with the same cause but
// were not proven to be it. Four runs after this retry landed were green.
//
// The load of 72 parallel Vitest forks is what makes it frequent, but nothing
// about it is test-only -- a live `zagent` write hits the same call and would
// fail the same way, which is why the retry lives here rather than in a test
// helper.
//
// Retry rather than `rm` the destination first: an unlink-then-rename is not
// atomic, and a reader arriving in that window sees the file missing instead of
// seeing either version. `cli/zagent.mjs`'s `writeScratch` takes the unlink
// route, which is tolerable for scratch and would not be for a vault journal.
const RENAME_RETRY_CODES = new Set(['EPERM', 'EACCES', 'EBUSY']);

async function renameWithRetry(from, to, attempts = 6) {
  for (let attempt = 1; ; attempt += 1) {
    try { return await fs.rename(from, to); }
    catch (error) {
      if (!RENAME_RETRY_CODES.has(error?.code) || attempt >= attempts) throw error;
      // 5, 10, 20, 40, 80 ms -- ~155 ms total, well inside a scanner's grip.
      await new Promise(resolve => setTimeout(resolve, 5 * 2 ** (attempt - 1)));
    }
  }
}

async function atomic(file, bytes) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  const temporary = `${file}.vault-${randomUUID()}.tmp`;
  const handle = await fs.open(temporary, 'wx');
  try { await handle.writeFile(bytes); await handle.sync(); } finally { await handle.close(); }
  try { await renameWithRetry(temporary, file); } catch (e) { await fs.rm(temporary, { force: true }); throw e; }
}
const jsonWrite = (file, value) => atomic(file, Buffer.from(JSON.stringify(value, null, 2) + '\n'));
const alive = pid => { try { process.kill(pid, 0); return true; } catch (e) { return e.code !== 'ESRCH'; } };

async function acquire(ctx) {
  await fs.mkdir(ctx.state, { recursive: true });
  let old = null;
  try { old = await git(ctx.repo, ['rev-parse', '--verify', LOCK]); } catch {}
  if (old) {
    let owner;
    try { owner = JSON.parse(await git(ctx.repo, ['cat-file', 'blob', old])); } catch { fail('LOCKED', 'Unrecognized operation lock; refusing to evict it'); }
    if (owner.host !== os.hostname() || !Number.isInteger(owner.pid) || alive(owner.pid)) {
      fail('LOCKED', `Vault is busy (owner ${owner.pid} on ${owner.host})`);
    }
  }
  const owner = { pid: process.pid, host: os.hostname(), nonce: randomUUID(), heartbeat: new Date().toISOString() };
  let oid = await git(ctx.repo, ['hash-object', '-w', '--stdin'], JSON.stringify(owner));
  try { await git(ctx.repo, ['update-ref', LOCK, oid, old ?? '0'.repeat(oid.length)]); }
  catch { fail('LOCKED', 'Another vault operation acquired the lock'); }
  // PID liveness, not heartbeat age, controls reclamation. Heartbeat is diagnostic.
  let tail = Promise.resolve();
  const timer = setInterval(() => {
    tail = tail.then(async () => {
      owner.heartbeat = new Date().toISOString();
      const next = await git(ctx.repo, ['hash-object', '-w', '--stdin'], JSON.stringify(owner));
      await git(ctx.repo, ['update-ref', LOCK, next, oid]); oid = next;
    }).catch(() => {});
  }, 10000);
  timer.unref();
  return async () => { clearInterval(timer); await tail; await git(ctx.repo, ['update-ref', '-d', LOCK, oid]); };
}

async function configured(root) {
  const ctx = await context(await resolveAuthority(root));
  const config = await readJson(path.join(ctx.root, 'vault.config.json'));
  if (config?.writes?.enabled !== true) fail('READ_ONLY', 'Vault writes are disabled; configure writes.enabled explicitly');
  return { ...ctx, config };
}

async function register(ctx) {
  const file = path.join(ctx.state, 'authority.json');
  const existing = await readJson(file);
  if (existing && path.resolve(existing.root).toLowerCase() !== ctx.root.toLowerCase()) fail('AUTHORITY', 'A different authoritative vault is already registered');
  if (!existing) await jsonWrite(file, { version: 1, root: ctx.root });
}

async function safePath(root, relative) {
  if (typeof relative !== 'string' || !relative || path.isAbsolute(relative) || relative.includes('\0')) fail('PATH', 'Expected a relative vault path');
  const normalized = relative.replaceAll('\\', '/');
  if (normalized.split('/').some(p => p === '..' || p === '.git' || p === '.zagent') || /[:]/.test(normalized)) fail('PATH', 'Path escapes the editable vault');
  const full = path.resolve(root, normalized);
  if (full === root || !full.startsWith(root + path.sep)) fail('PATH', 'Path escapes the editable vault');
  let current = root;
  for (const part of path.relative(root, full).split(path.sep)) {
    current = path.join(current, part);
    try { if ((await fs.lstat(current)).isSymbolicLink()) fail('PATH', 'Symlink paths cannot be edited'); }
    catch (e) { if (e.code !== 'ENOENT') throw e; }
  }
  return full;
}

async function bytesAt(file) { try { return await fs.readFile(file); } catch (e) { if (e.code === 'ENOENT') return null; throw e; } }
export async function readDocument(root, relative) {
  root = await resolveAuthority(root);
  const full = await safePath(root, relative);
  for (let attempt = 0; attempt < 3; attempt++) {
    try {
      const a = await fs.stat(full), bytes = await fs.readFile(full), b = await fs.stat(full);
      if (a.mtimeMs === b.mtimeMs && a.size === b.size && bytes.length === b.size) {
        return { bytes, revision: { hash: hashBytes(bytes), mtimeMs: b.mtimeMs, size: b.size } };
      }
    } catch (e) { if (e.code === 'ENOENT') return { bytes: null, revision: { hash: null, size: 0, mtimeMs: null } }; throw e; }
  }
  fail('STALE', 'File changed while reading its revision');
}

export async function readRevision(root, relative) { return (await readDocument(root, relative)).revision; }

function matchesRevision(actual, expected) {
  if (expected === null || typeof expected === 'string') return actual.hash === expected;
  return expected && actual.hash === expected.hash
    && (expected.size === undefined || actual.size === expected.size)
    && (expected.mtimeMs === undefined || actual.mtimeMs === expected.mtimeMs);
}

const EPIC_TYPE = 'epic';
const IN_PROGRESS_STATUS = 'in progress';

function canonicalTicketField(record, names) {
  const metadata = record?.metadata ?? record?.fields ?? {};
  for (const name of names) {
    if (metadata[name] !== undefined && metadata[name] !== null) return metadata[name];
  }
  return undefined;
}

function isEpicInProgress(record) {
  return String(canonicalTicketField(record, ['type', 'issue_type', 'issueType']) ?? '').trim().toLowerCase() === EPIC_TYPE
    && String(canonicalTicketField(record, ['status', 'state']) ?? '').trim().toLowerCase() === IN_PROGRESS_STATUS;
}

function ticketKey(record) {
  return String(canonicalTicketField(record, ['key', 'ticket_key', 'issue_key', 'id']) ?? record?.relativePath ?? 'unknown ticket');
}

/**
 * Enforce ticket policies against a proposed snapshot.
 *
 * Imported vaults can contain epics that were already left In Progress by a
 * legacy writer. Keep those records editable so a migration can correct them,
 * while refusing any operation that introduces an epic in that status. The
 * source path is the primary identity here: a correction may also repair a
 * malformed or renamed key without being mistaken for a new ticket.
 */
function validateTicketPolicies(index, proposed) {
  for (const after of proposed.tickets.values()) {
    if (!isEpicInProgress(after)) continue;
    const before = index.getFile(after.relativePath);
    if (isEpicInProgress(before)) continue;
    fail('INVALID', `Epic ${ticketKey(after)} cannot be In Progress; move a child ticket instead`);
  }
}

async function validate(ctx, index, changes) {
  const stage = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-validate-'));
  try {
    for (const board of index.boardRoots) await fs.mkdir(path.join(stage, path.relative(ctx.root, board.absolutePath)), { recursive: true });
    for (const record of index.files.values()) {
      if (!record.rawBytes) continue;
      const target = path.join(stage, record.relativePath);
      await fs.mkdir(path.dirname(target), { recursive: true }); await fs.writeFile(target, record.rawBytes);
    }
    for (const change of changes) {
      const target = path.join(stage, change.path);
      if (change.after === null) await fs.rm(target, { force: true });
      else { await fs.mkdir(path.dirname(target), { recursive: true }); await fs.writeFile(target, Buffer.from(change.after, 'base64')); }
    }
    const proposed = await loadVault(stage);
    if (!proposed.valid) fail('INVALID', `Proposed vault is invalid: ${proposed.errors.map(e => `${e.relativePath ?? e.path}: ${e.message}`).join('; ')}`);
    validateTicketPolicies(index, proposed);
    for (const [key, before] of index.tickets) {
      const after = proposed.getTicket(key);
      if (!after || before.metadata.status === after.metadata.status) continue;
      const workflow = [...index.files.values()].find(record => record.project === before.project && record.fields?.kind === 'workflow');
      if (!workflow) continue;
      const statuses = workflow.fields.statuses ?? [];
      const from = statuses.find(s => s.name === before.metadata.status);
      const to = statuses.find(s => s.name === after.metadata.status);
      const permitted = from && to && (workflow.fields.transitions ?? []).some(edge =>
        (edge.from_status_id === null || edge.from_status_id === from.id) && edge.to_status_id === to.id);
      if (!permitted) fail('INVALID', `Workflow does not permit ${key}: ${before.metadata.status} → ${after.metadata.status}`);
    }
  } finally { await fs.rm(stage, { recursive: true, force: true }); }
}

async function branchState(ctx) {
  return { branch: await git(ctx.repo, ['symbolic-ref', 'HEAD']), head: await git(ctx.repo, ['rev-parse', 'HEAD']) };
}
async function ensureBranch(ctx, journal) {
  const current = await branchState(ctx);
  if (current.branch !== journal.branch || (current.head !== journal.parent && current.head !== journal.commit)) fail('BRANCH_MOVED', 'Checkout or branch moved during operation; refusing automatic recovery');
}

async function commitOperation(ctx, journal, journalFile) {
  await ensureBranch(ctx, journal);
  const currentHead = await git(ctx.repo, ['rev-parse', journal.branch]);
  if (journal.commit && currentHead === journal.commit) return;
  const indexFile = path.join(ctx.state, `index-${journal.id}`);
  const env = { GIT_INDEX_FILE: indexFile };
  try {
    await git(ctx.repo, ['read-tree', journal.parent], undefined, env);
    for (const change of journal.changes) {
      const relative = path.relative(ctx.repo, path.join(ctx.root, change.path)).replaceAll('\\', '/');
      if (change.after === null) await git(ctx.repo, ['update-index', '--force-remove', '--', relative], undefined, env);
      else {
        const blob = await git(ctx.repo, ['hash-object', '-w', `--path=${relative}`, '--stdin'], Buffer.from(change.after, 'base64'));
        const prior = await git(ctx.repo, ['ls-tree', journal.parent, '--', relative]);
        const mode = prior.startsWith('100755') ? '100755' : '100644';
        await git(ctx.repo, ['update-index', '--add', '--cacheinfo', `${mode},${blob},${relative}`], undefined, env);
      }
    }
    const tree = await git(ctx.repo, ['write-tree'], undefined, env);
    journal.commit = await git(ctx.repo, ['commit-tree', tree, '-p', journal.parent], `board: ${journal.kind}\n\nVault-Operation: ${journal.id}\n`);
    await jsonWrite(journalFile, journal);
    await git(ctx.repo, ['update-ref', journal.branch, journal.commit, journal.parent]);
  } finally { await fs.rm(indexFile, { force: true }); }
}

async function applyJournal(ctx, journal, file) {
  if (journal.phase === 'complete') return;
  await ensureBranch(ctx, journal);
  // Check every file before advancing any partial operation.
  for (const change of journal.changes) {
    const actual = hashBytes(await bytesAt(await safePath(ctx.root, change.path)));
    if (actual !== change.beforeHash && actual !== change.afterHash) fail('RECOVERY_CONFLICT', `External change in ${change.path}; journal ${journal.id} preserved`);
  }
  for (const [i, change] of journal.changes.entries()) {
    const target = await safePath(ctx.root, change.path);
    const actual = hashBytes(await bytesAt(target));
    if (actual === change.afterHash) continue;
    if (actual !== change.beforeHash) fail('STALE', `File changed during operation: ${change.path}`);
    if (change.after === null) await fs.rm(target);
    else await atomic(target, Buffer.from(change.after, 'base64'));
    journal.applied = i + 1; await jsonWrite(file, journal);
    if (process.env.VAULT_TEST_CRASH_AFTER_WRITE === String(i + 1) && ctx.config.writes.testFaults === true) process.kill(process.pid, 'SIGKILL');
  }
  if (journal.shouldCommit && journal.changes.length) {
    await commitOperation(ctx, journal, file);
    if (process.env.VAULT_TEST_CRASH_AFTER_COMMIT === '1' && ctx.config.writes.testFaults === true) process.kill(process.pid, 'SIGKILL');
    // Only operation paths are synchronized. Unrelated staged changes survive.
    const paths = journal.changes.map(c => path.relative(ctx.repo, path.join(ctx.root, c.path)).replaceAll('\\', '/'));
    await git(ctx.repo, ['reset', '-q', journal.commit, '--', ...paths]);
  }
  for (const change of journal.changes) {
    if (hashBytes(await bytesAt(await safePath(ctx.root, change.path))) !== change.afterHash) fail('RECOVERY_CONFLICT', `File changed after replacement: ${change.path}`);
  }
  journal.phase = 'complete'; await jsonWrite(file, journal);
}

async function recoverLocked(ctx) {
  const directory = path.join(ctx.state, 'operations'); await fs.mkdir(directory, { recursive: true });
  let count = 0;
  for (const name of (await fs.readdir(directory)).filter(n => n.endsWith('.json')).sort()) {
    const file = path.join(directory, name), journal = await readJson(file);
    if (journal.phase !== 'complete') { await applyJournal(ctx, journal, file); count++; }
  }
  return count;
}

export async function recoverOperations(root) {
  const ctx = await configured(root), release = await acquire(ctx);
  try { await register(ctx); return { recovered: await recoverLocked(ctx) }; } finally { await release(); }
}

/** Claim diagnostics use the same locked, recovered disk view as mutations. */
export async function withAuthoritativeIndex(root, reader) {
  root = await resolveAuthority(root);
  const config = await readJson(path.join(root, 'vault.config.json'));
  if (config?.writes?.enabled !== true) return reader(await loadVault(root));
  const ctx = await configured(root), release = await acquire(ctx);
  try { await register(ctx); await recoverLocked(ctx); return await reader(await loadVault(ctx.root)); }
  finally { await release(); }
}

export async function executeOperation(root, options) {
  const ctx = await configured(root), release = await acquire(ctx);
  try {
    await register(ctx); await recoverLocked(ctx);
    const id = options.id ?? randomUUID();
    if (!/^[A-Za-z0-9_-]{1,100}$/.test(id)) fail('ID', 'Invalid operation ID');
    const file = path.join(ctx.state, 'operations', `${id}.json`);
    const previous = await readJson(file);
    if (previous) {
      if (previous.kind !== options.kind) fail('ID', 'Operation ID was already used for a different operation');
      if ((previous.fingerprint ?? null) !== (options.fingerprint ?? null)) fail('ID', 'Operation ID was already used with different inputs');
      return { operationId: id, result: previous.result, commit: previous.commit ?? null };
    }
    const index = await loadVault(ctx.root);
    for (const [relative, expected] of Object.entries(options.expected ?? {})) {
      if (!matchesRevision(await readRevision(ctx.root, relative), expected)) fail('STALE', `File changed since it was opened: ${relative}`);
    }
    const prepared = await options.prepare(index);
    const changes = [], seen = new Set();
    for (const change of prepared.changes ?? []) {
      const target = await safePath(ctx.root, change.path);
      const relative = path.relative(ctx.root, target).replaceAll('\\', '/');
      if (seen.has(relative.toLowerCase())) fail('PATH', `Duplicate operation path: ${relative}`);
      seen.add(relative.toLowerCase());
      const boardRoots = index.boardRoots.filter(board => {
        const boardRelative = path.relative(board.absolutePath, target);
        return boardRelative === '' || (boardRelative !== '..' && !boardRelative.startsWith(`..${path.sep}`) && !path.isAbsolute(boardRelative));
      });
      if (boardRoots.some(board => board.readOnly === true)) fail('PATH', `Read-only vault path: ${relative}`);
      // Most mutations edit an indexed ticket/config or a configured board
      // root. A project-create operation is the one intentional exception:
      // it adds a new board root and its first catalogs in the same atomic
      // journal, so those paths cannot be found in the pre-operation index.
      const extraPaths = Array.isArray(options.allowPaths)
        ? options.allowPaths.map(value => String(value).replaceAll('\\', '/').replace(/\/+$/, ''))
        : [];
      const explicitlyAllowed = extraPaths.some(prefix => relative === prefix
        || relative.startsWith(prefix + '/'));
      const allowed = index.files.has(relative) || boardRoots.length > 0 || explicitlyAllowed;
      const configPath = String(index.configPath ?? 'vault.config.json').replaceAll('\\', '/').replace(/^\.\//, '');
      const editableConfig = options.kind === 'project.create' && relative === configPath;
      if (!allowed || (!editableConfig && !/\.(md|markdown)$/i.test(relative))) {
        fail('PATH', 'Not an editable vault path: ' + relative);
      }
      const before = await bytesAt(target), after = change.bytes === null ? null : Buffer.from(change.bytes);
      const scanned = index.getFile(relative);
      if (scanned?.hash && scanned.hash !== hashBytes(before)) fail('STALE', `File changed during preparation: ${relative}`);
      if (Object.hasOwn(options.expected ?? {}, change.path)
        && !matchesRevision(await readRevision(ctx.root, change.path), options.expected[change.path])) {
        fail('STALE', `File changed during preparation: ${relative}`);
      }
      if (hashBytes(before) === hashBytes(after)) continue;
      changes.push({ path: relative, before: before?.toString('base64') ?? null, after: after?.toString('base64') ?? null, beforeHash: hashBytes(before), afterHash: hashBytes(after) });
    }
    await validate(ctx, index, changes);
    const rechecked = await loadVault(ctx.root);
    const signature = snapshot => JSON.stringify([...snapshot.files.values()].map(r => [r.relativePath, r.hash ?? null]).sort((a, b) => a[0].localeCompare(b[0])));
    if (signature(index) !== signature(rechecked)) fail('STALE', 'Vault changed while preparing the operation');
    const { branch, head } = await branchState(ctx);
    const shouldCommit = options.commit ?? ctx.config.writes.commit ?? true;
    // A save commits the complete validated target files, including the bytes
    // the caller opened. Other paths, staged or unstaged, are never swept in.
    // Requiring a clean target would prevent saving a reloaded external edit.
    const journal = { version: 1, id, kind: options.kind ?? 'edit', fingerprint: options.fingerprint ?? null, root: ctx.root, phase: 'prepared', parent: head, branch, shouldCommit, changes, result: prepared.result, createdAt: new Date().toISOString() };
    await jsonWrite(file, journal);
    await applyJournal(ctx, journal, file);
    return { operationId: id, result: journal.result, commit: journal.commit ?? null };
  } finally { await release(); }
}

export async function saveDocument(root, relative, bytes, revision, options = {}) {
  if (revision === undefined) fail('STALE', 'A file revision is required to save');
  const savedBytes = Buffer.from(bytes);
  const operation = await executeOperation(root, { ...options, kind: 'save document',
    fingerprint: hashBytes(Buffer.from(JSON.stringify([relative, hashBytes(savedBytes)]))), expected: { [relative]: revision },
    prepare: async () => ({ changes: [{ path: relative, bytes: savedBytes }], result: { path: relative } }) });
  // Return the revision of the bytes this operation saved. Reading disk after
  // releasing the lock could attach a later writer's revision to our old text,
  // allowing the browser's next save to overwrite that later edit undetected.
  return { ...operation, revision: { hash: hashBytes(savedBytes), size: savedBytes.length } };
}
