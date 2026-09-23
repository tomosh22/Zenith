/*
 * Lane mode: one board project bound to one persistent git worktree, so
 * `/tick` can run more than one project at a time (Docs/plans/parallel
 * lanes). This module owns:
 *
 *   - `resolveLaneMode(cwd)` — is this invocation the main checkout, a
 *     registered lane, or an unregistered linked worktree.
 *   - The lane registry, `<git-common-dir>/vault/lanes.json` (beside
 *     `authority.json`, never committed).
 *   - `zagent lane which|list|add|remove`.
 *   - `computeDoctorLanes`, the async lane block `doctor` stashes on
 *     `index.laneDoctor` before `local.mjs`'s synchronous `doctor` case
 *     reads it.
 *
 * `index.lane` (set by client.mjs after `loadVault`, never here) is the ONE
 * carrier a mutation-time consumer reads: `{ project, repoRoot }` in lane
 * mode, `undefined` in main mode. Never confuse it with `index.root` /
 * `index.vaultRoot`, which stay the AUTHORITY in every mode — every
 * authority-scoped caller (server/index.js, issue-interactions.mjs,
 * project-actions.mjs, write.mjs's own `repoRootOf`) depends on that.
 */

import { spawn } from 'node:child_process';
import { promises as fs, readFileSync } from 'node:fs';
import path from 'node:path';
import { randomUUID } from 'node:crypto';

import { git } from '../server/operations.mjs';
import { diagnostic } from '../server/diagnostics.js';
import { canonicalOf, policyOf } from './compat.mjs';

// Mutating lane subcommands refuse from here unless `lanes.enabled`; `which`
// and `list` always answer. Commands a truly UNREGISTERED linked worktree
// must never drive — this is what fixes the live hazard of a
// `.claude/worktrees/*` session silently operating on `C:\dev\Zenith`.
export const UNREGISTERED_REFUSED_COMMANDS = new Set([
  'claim', 'next', 'check', 'finish', 'abandon', 'gates', 'guard',
]);

const DEFAULT_LANES_CONFIG = Object.freeze({
  enabled: false,
  root: 'C:/dev/Zenith-lanes',
  integrateWaitMinutes: 240,
  buildSlotWaitMinutes: 90,
});

const LANE_ALLOWED_FLAGS = Object.freeze({
  which: Object.freeze(['json']),
  list: Object.freeze(['json']),
  add: Object.freeze(['path', 'json']),
  remove: Object.freeze(['json']),
});

// ── argv helpers (deliberately local: write.mjs's own valueOf/positional
// are private, and this file must not widen its edit scope into write.mjs
// to reach them) ────────────────────────────────────────────────────────

function valueOf(args, name) {
  const flag = `--${name}`;
  const index = args.indexOf(flag);
  if (index !== -1) return args[index + 1];
  const prefix = `${flag}=`;
  const hit = args.find(value => typeof value === 'string' && value.startsWith(prefix));
  return hit ? hit.slice(prefix.length) : undefined;
}

async function pathExists(target) {
  try { await fs.access(target); return true; } catch { return false; }
}

// ── `--lane` is not a vocabulary flag (D2) ────────────────────────────────
//
// Strip it out of the argv BEFORE `inspectArguments`/`assertKnownFlags` ever
// see it — those enforce the closed per-command flag table in
// legacy-pure/flags.mjs, a vendored/generated file this work package does
// not edit, so `--lane` can never be added there. The lane reaches
// operations only through `index.lane`; this only validates that a
// caller-supplied `--lane` (if any) agrees with where the invocation
// actually runs.

export function extractUserLaneFlag(commandArgs) {
  const args = Array.isArray(commandArgs) ? commandArgs.slice() : [];
  let present = false;
  let value;
  const result = [];
  for (let index = 0; index < args.length; index += 1) {
    const token = args[index];
    if (token === '--lane') {
      present = true;
      value = args[index + 1];
      index += 1;
      continue;
    }
    if (typeof token === 'string' && token.startsWith('--lane=')) {
      present = true;
      value = token.slice('--lane='.length);
      continue;
    }
    result.push(token);
  }
  return { present, value, args: result };
}

// ── git-common-dir mode resolution (D6) ───────────────────────────────────
//
// `git rev-parse --git-common-dir` prints a RELATIVE `.git` from the main
// checkout and an ABSOLUTE `C:/…/.git` from a linked worktree; resolve with
// `path.resolve(root, out)` exactly as the private helper at
// `server/operations.mjs`'s `context()` does — that helper is not exported,
// so this is a deliberate, small re-implementation rather than a circular
// import.
//
// The main checkout is recognised structurally, not via `authority.json`:
// for the main checkout, `--git-common-dir` resolves to `<repo>/.git`, so
// its OWN PARENT is the repo toplevel. For a linked worktree it resolves to
// the MAIN checkout's `.git`, so its parent is the main checkout, not the
// worktree's own toplevel. That single comparison needs no board
// registration to exist yet, which matters because `resolveLaneMode` must
// answer correctly even before any operation has ever run from this tree.

export async function laneGitContext(cwd) {
  let root;
  try { root = await fs.realpath(path.resolve(cwd)); } catch { return null; }
  let repo;
  try { repo = await git(root, ['rev-parse', '--show-toplevel']); } catch { return null; }
  let commonRaw;
  try { commonRaw = await git(root, ['rev-parse', '--git-common-dir']); } catch { return null; }
  const common = path.resolve(root, commonRaw);
  return { root, repo, common };
}

function normalizeForCompare(value) {
  return path.resolve(String(value ?? '')).replace(/\\/g, '/').toLowerCase();
}

function lanesRegistryPath(common) {
  return path.join(common, 'vault', 'lanes.json');
}

export async function readLanesRegistry(common) {
  if (!common) return { lanes: {} };
  const file = lanesRegistryPath(common);
  try {
    const raw = JSON.parse(await fs.readFile(file, 'utf8'));
    const lanes = raw && typeof raw === 'object' && !Array.isArray(raw) && raw.lanes && typeof raw.lanes === 'object' && !Array.isArray(raw.lanes)
      ? raw.lanes
      : {};
    return { lanes };
  } catch (error) {
    // A missing registry is the ordinary "no lanes yet" state. A malformed
    // one (hand-edited, truncated by a crash mid-write before the atomic
    // rename ever lands) must not crash mode resolution for EVERY `zagent`
    // invocation, including a bare `--help` — treat it the same as absent
    // rather than propagating a parse error out of `resolveLaneMode`.
    return { lanes: {} };
  }
}

// See client.mjs's `renameWithRetry` for why: a correct rename can still
// throw EPERM/EACCES/EBUSY on Windows for a moment after the destination
// file is created (Defender / the search indexer briefly hold it open).
// That helper is private to client.mjs — reimplemented here rather than
// imported, to avoid a circular import between the two modules.
const RENAME_RETRY_CODES = new Set(['EPERM', 'EACCES', 'EBUSY']);

async function renameWithRetry(from, to, attempts = 6) {
  for (let attempt = 1; ; attempt += 1) {
    try { return await fs.rename(from, to); }
    catch (error) {
      if (!RENAME_RETRY_CODES.has(error?.code) || attempt >= attempts) throw error;
      await new Promise(resolve => setTimeout(resolve, 5 * 2 ** (attempt - 1)));
    }
  }
}

async function writeLanesRegistry(common, registry) {
  const file = lanesRegistryPath(common);
  await fs.mkdir(path.dirname(file), { recursive: true });
  const bytes = Buffer.from(`${JSON.stringify({ lanes: registry.lanes ?? {} }, null, 2)}\n`);
  const temp = `${file}.tmp-${randomUUID()}`;
  try {
    await fs.writeFile(temp, bytes, { flag: 'wx' });
    await renameWithRetry(temp, file);
  } finally {
    await fs.rm(temp, { force: true });
  }
}

/**
 * `main` when `cwd` resolves to the authority checkout itself; `lane` when
 * it resolves to a worktree registered in `lanes.json`; `unregistered` for
 * any other linked worktree of the same repository; `main` again when `cwd`
 * is not inside a git repository at all (mirrors today's behaviour, and
 * keeps a non-repo embedded/unit fixture working unchanged).
 *
 * Never calls bare `process.cwd()` — every caller drives this from its own
 * resolved `cwd` (client.mjs's `runClient`), and tests drive it exclusively
 * via `{ cwd: <fixture path> }`.
 */
export async function resolveLaneMode(cwd) {
  const ctx = await laneGitContext(cwd);
  if (!ctx) {
    const fallback = path.resolve(cwd);
    return { mode: 'main', project: null, path: fallback, authorityRoot: fallback, common: null };
  }
  const authorityRoot = path.dirname(ctx.common);
  if (normalizeForCompare(ctx.repo) === normalizeForCompare(authorityRoot)) {
    return { mode: 'main', project: null, path: ctx.repo, authorityRoot, common: ctx.common };
  }
  const registry = await readLanesRegistry(ctx.common);
  for (const [project, entry] of Object.entries(registry.lanes ?? {})) {
    if (entry?.path && normalizeForCompare(entry.path) === normalizeForCompare(ctx.repo)) {
      return { mode: 'lane', project, path: ctx.repo, authorityRoot, common: ctx.common, entry };
    }
  }
  return { mode: 'unregistered', project: null, path: ctx.repo, authorityRoot, common: ctx.common };
}

// ── `zagent.project.json`'s `lanes` block (D5) ────────────────────────────
//
// Read directly off the authority's `zagent.project.json`, the same file
// `compat.mjs`'s (private, unexported) `projectFileOf` reads — NOT through
// the `PROJECT_FILE_OWNED` overlay `policyOf` applies, because that overlay
// is deliberately a fixed, named field list and `lanes` is not on it.
// `projectFileOf` cannot be imported without widening this work package's
// edit scope into the rest of compat.mjs, so this is a small, deliberate
// re-read of the same file.
function stripCommentKeys(value) {
  if (Array.isArray(value)) return value.map(stripCommentKeys);
  if (!value || typeof value !== 'object') return value;
  const out = {};
  for (const [key, inner] of Object.entries(value)) {
    if (key.startsWith('$')) continue;
    out[key] = stripCommentKeys(inner);
  }
  return out;
}

function readProjectFile(authorityRoot) {
  if (!authorityRoot || typeof authorityRoot !== 'string') return null;
  try {
    return stripCommentKeys(JSON.parse(readFileSync(path.join(authorityRoot, 'zagent.project.json'), 'utf8')));
  } catch {
    return null; // No repo file (a fixture vault, or a malformed one) — defaults stand.
  }
}

export function readLanesConfig(index) {
  const authorityRoot = index?.root ?? index?.vaultRoot;
  const projectFile = readProjectFile(authorityRoot);
  const configured = projectFile && typeof projectFile.lanes === 'object' && projectFile.lanes !== null && !Array.isArray(projectFile.lanes)
    ? projectFile.lanes
    : null;
  return { ...DEFAULT_LANES_CONFIG, ...(configured ?? {}) };
}

function baseBranchOf(authorityRoot) {
  return readProjectFile(authorityRoot)?.baseBranch ?? 'master';
}

/** boardRoots entries this checkout can write — the same shape a `lane add` target must be one of. */
export function writableProjectKeys(index) {
  const roots = Array.isArray(index?.boardRoots) ? index.boardRoots : [];
  return new Set(
    roots
      .filter(row => row && !row.readOnly && !row.archive)
      .map(row => String(row.project ?? '').toUpperCase())
      .filter(Boolean),
  );
}

function findInFlight(index, project) {
  const email = String(policyOf(index).agentEmail ?? '').toLowerCase();
  const tickets = index?.tickets instanceof Map ? index.tickets.values() : [];
  for (const record of tickets) {
    const metadata = canonicalOf(record);
    if (String(metadata.status ?? '').toLowerCase() !== 'in progress') continue;
    if (String(metadata.assignee ?? '').toLowerCase() !== email) continue;
    if (String(metadata.lane ?? '').toUpperCase() !== String(project ?? '').toUpperCase()) continue;
    return metadata.key ?? null;
  }
  return null;
}

function refused(message) {
  return { exitCode: 1, payload: { error: message } };
}

function laneWhich(index, laneInfo) {
  return {
    exitCode: 0,
    payload: {
      mode: laneInfo.mode,
      project: laneInfo.mode === 'lane' ? laneInfo.project : null,
      path: laneInfo.path ?? null,
      inFlight: laneInfo.mode === 'lane' ? findInFlight(index, laneInfo.project) : null,
    },
  };
}

async function laneList(laneInfo) {
  const registry = await readLanesRegistry(laneInfo.common);
  const lanes = await Promise.all(Object.entries(registry.lanes ?? {}).map(async ([project, entry]) => ({
    project,
    path: entry.path,
    exists: await pathExists(entry.path),
    createdAt: entry.createdAt ?? null,
    provisionedAt: entry.provisionedAt ?? null,
  })));
  return { exitCode: 0, payload: { lanes } };
}

// Real process runner, overridable per invocation (`context.laneRunProcess`)
// so tests can stub `git worktree add` / `Tools/lane_init.ps1` without
// actually running them. `{ stream: true }` (used only for the lane_init
// call, which can run for tens of minutes) ALSO mirrors the child's
// stdout/stderr onto the parent's stderr as it arrives, so `zagent lane add`
// shows live progress; the buffered result returned to the caller is
// unchanged either way. The git calls stay quiet (no `stream` option).
function defaultRunProcess(command, args, cwd, { stream = false } = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { cwd, windowsHide: true });
    const out = [];
    const err = [];
    child.stdout.on('data', chunk => {
      out.push(chunk);
      if (stream) process.stderr.write(chunk);
    });
    child.stderr.on('data', chunk => {
      err.push(chunk);
      if (stream) process.stderr.write(chunk);
    });
    child.on('error', reject);
    child.on('close', code => resolve({ code, stdout: Buffer.concat(out).toString(), stderr: Buffer.concat(err).toString() }));
  });
}

// ── the `lanes.json` lock (D-race) ────────────────────────────────────────
//
// Two concurrent `lane add`/`lane remove` each used to read the registry at
// the start of their (multi-minute) run and write it at the end, so one
// could silently drop the other's entry. The fix is narrow: only the
// re-read-then-write around each command's own edit is serialized, never
// the long-running `git worktree add` / `lane_init.ps1` work before it.

const LANE_LOCK_RETRY_MS = 50;
const LANE_LOCK_RETRY_LIMIT = 100; // ~5s of polling before giving up
const LANE_LOCK_STALE_MS = 60_000;

function lanesLockPath(common) {
  return path.join(common, 'vault', 'lanes.json.lock');
}

async function acquireLanesLock(common) {
  const lockFile = lanesLockPath(common);
  await fs.mkdir(path.dirname(lockFile), { recursive: true });
  const stamp = () => `${process.pid}\n${new Date().toISOString()}\n`;
  for (let attempt = 0; ; attempt += 1) {
    try {
      await fs.writeFile(lockFile, stamp(), { flag: 'wx' });
      return lockFile;
    } catch (error) {
      if (error?.code !== 'EEXIST') throw error;
      let stat;
      try { stat = await fs.stat(lockFile); } catch { stat = null; }
      if (stat && Date.now() - stat.mtimeMs > LANE_LOCK_STALE_MS) {
        // Stale-lock takeover: the previous holder almost certainly crashed
        // without reaching its `finally`. Clear it and try again right away.
        try { await fs.rm(lockFile, { force: true }); } catch { /* raced another taker */ }
        continue;
      }
      if (attempt >= LANE_LOCK_RETRY_LIMIT) {
        throw new Error(`timed out waiting for the lanes registry lock at ${lockFile}`);
      }
      await new Promise(resolve => setTimeout(resolve, LANE_LOCK_RETRY_MS));
    }
  }
}

async function releaseLanesLock(lockFile) {
  try { await fs.rm(lockFile, { force: true }); } catch { /* already gone */ }
}

/** Run `fn` with the lanes registry lock held; released in `finally` whether `fn` resolves or throws. */
export async function withLanesLock(common, fn) {
  const lockFile = await acquireLanesLock(common);
  try {
    return await fn();
  } finally {
    await releaseLanesLock(lockFile);
  }
}

function trimTail(text, maxLines) {
  const lines = String(text ?? '').split(/\r?\n/);
  const tail = lines.length <= maxLines ? lines : lines.slice(-maxLines);
  return tail.join('\n').trim();
}

async function laneAdd(index, context, laneInfo) {
  const args = Array.isArray(context.args) ? context.args : [];
  const project = String(args[2] ?? '').toUpperCase();
  const config = readLanesConfig(index);
  if (!config.enabled) return refused('lanes are disabled; set "lanes": { "enabled": true, … } in zagent.project.json first.');
  if (!project || project.startsWith('--')) return refused('Usage: zagent lane add <PROJECT> [--path <dir>]');
  if (!laneInfo.common || !laneInfo.authorityRoot) return refused('zagent lane add must be run inside a git checkout of this repository.');
  const writable = writableProjectKeys(index);
  if (!writable.has(project)) return refused(`${project} is not a writable board project (check boardRoots in vault.config.json).`);
  const registry = await readLanesRegistry(laneInfo.common);
  if (registry.lanes?.[project]) return refused(`${project} already has a registered lane at ${registry.lanes[project].path}.`);
  const requestedPath = valueOf(args, 'path');
  const targetPath = path.resolve(requestedPath ?? path.join(config.root, project));
  const baseBranch = baseBranchOf(laneInfo.authorityRoot);
  const run = context.laneRunProcess ?? defaultRunProcess;

  const alreadyRegisteredElsewhere = Object.values(registry.lanes ?? {}).some(
    entry => entry?.path && normalizeForCompare(entry.path) === normalizeForCompare(targetPath),
  );
  if (alreadyRegisteredElsewhere) return refused(`${targetPath} is already a registered lane; choose a different --path.`);

  // Retry-after-lane_init-failure (I5/I1): a prior `lane add` for this exact
  // path can have created the worktree — the expensive part, tens of
  // minutes and ~14 GB of copied assets — and then failed inside
  // `lane_init.ps1`, leaving the worktree unregistered on disk. Reuse it
  // rather than requiring the caller to delete it first: `git worktree add`
  // refuses outright once the path exists.
  if (await pathExists(targetPath)) {
    const existingCtx = await laneGitContext(targetPath);
    if (!existingCtx || normalizeForCompare(existingCtx.common) !== normalizeForCompare(laneInfo.common)) {
      return refused(
        `${targetPath} already exists and is not a linked worktree of this repository; `
        + `remove it by hand or choose a different --path.`,
      );
    }
    // `lane_init.ps1` asserts a clean tree itself; this only puts the
    // worktree back on `baseBranch`, and fails safely (reporting git's own
    // error) if local edits would be overwritten by doing so.
    const checkout = await run('git', ['checkout', '--detach', baseBranch], targetPath);
    if (checkout.code !== 0) {
      return refused(`git -C ${targetPath} checkout --detach ${baseBranch} failed: ${(checkout.stderr || checkout.stdout || '').trim()}`);
    }
  } else {
    const worktree = await run('git', ['worktree', 'add', '--detach', targetPath, baseBranch], laneInfo.authorityRoot);
    if (worktree.code !== 0) return refused(`git worktree add ${targetPath} ${baseBranch} failed: ${(worktree.stderr || worktree.stdout || '').trim()}`);
  }

  const initScript = path.join(laneInfo.authorityRoot, 'Tools', 'lane_init.ps1');
  if (!(await pathExists(initScript))) {
    return refused(
      `Tools/lane_init.ps1 is missing. The worktree at ${targetPath} was created but NOT registered; `
      + `remove it by hand (\`git worktree remove --force ${targetPath}\`) or add the script and retry \`zagent lane add ${project}\` `
      + `— a retry reuses this worktree rather than re-copying it.`,
    );
  }
  const init = await run(
    'pwsh',
    ['-NoProfile', '-File', initScript, '-Lane', targetPath, '-Main', laneInfo.authorityRoot],
    laneInfo.authorityRoot,
    { stream: true },
  );
  if (init.code !== 0) {
    const combined = [init.stdout, init.stderr].filter(Boolean).join('\n');
    return refused(
      `Tools/lane_init.ps1 failed (exit ${init.code}): ${trimTail(combined, 40) || '(no output)'}. `
      + `The worktree at ${targetPath} was created but NOT registered; fix the failure and retry \`zagent lane add ${project}\` `
      + `— a retry reuses this worktree rather than re-copying it, or remove it by hand.`,
    );
  }

  await withLanesLock(laneInfo.common, async () => {
    const fresh = await readLanesRegistry(laneInfo.common);
    const now = new Date().toISOString();
    fresh.lanes = { ...(fresh.lanes ?? {}), [project]: { project, path: targetPath, createdAt: now, provisionedAt: now } };
    await writeLanesRegistry(laneInfo.common, fresh);
  });
  return { exitCode: 0, payload: { project, path: targetPath, registered: true } };
}

async function laneRemove(index, context, laneInfo) {
  const args = Array.isArray(context.args) ? context.args : [];
  const project = String(args[2] ?? '').toUpperCase();
  const config = readLanesConfig(index);
  if (!config.enabled) return refused('lanes are disabled; set "lanes": { "enabled": true, … } in zagent.project.json first.');
  if (!project || project.startsWith('--')) return refused('Usage: zagent lane remove <PROJECT>');
  if (!laneInfo.common) return refused('zagent lane remove must be run inside a git checkout of this repository.');
  const registry = await readLanesRegistry(laneInfo.common);
  const entry = registry.lanes?.[project];
  if (!entry) return refused(`${project} has no registered lane.`);

  let status;
  try { status = await git(entry.path, ['status', '--porcelain']); }
  catch (error) { return refused(`Cannot inspect the lane at ${entry.path}: ${error.message}`); }
  if (status.trim() !== '') return refused(`${project}'s lane at ${entry.path} has uncommitted changes; commit or discard them first.`);

  const baseBranch = baseBranchOf(laneInfo.authorityRoot);
  let unmergedRaw = '';
  try { unmergedRaw = await git(entry.path, ['branch', '--list', `agent/${project}-*`, '--no-merged', baseBranch]); } catch { unmergedRaw = ''; }
  const unlanded = unmergedRaw.split('\n').map(line => line.replace(/^\*?\s+/, '').trim()).filter(Boolean);
  if (unlanded.length) return refused(`${project}'s lane has unlanded branch(es): ${unlanded.join(', ')}. Land or discard them first.`);

  for (const relative of ['Build/output', 'Build/obj', 'Build/artifacts']) {
    await fs.rm(path.join(entry.path, relative), { recursive: true, force: true });
  }
  let games = [];
  try { games = await fs.readdir(path.join(entry.path, 'Games')); } catch { games = []; }
  for (const game of games) {
    await fs.rm(path.join(entry.path, 'Games', game, 'Build', 'output'), { recursive: true, force: true });
    await fs.rm(path.join(entry.path, 'Games', game, 'Build', 'obj'), { recursive: true, force: true });
  }

  const run = context.laneRunProcess ?? defaultRunProcess;
  const removed = await run('git', ['worktree', 'remove', '--force', entry.path], laneInfo.authorityRoot);
  if (removed.code !== 0) return refused(`git worktree remove --force ${entry.path} failed: ${(removed.stderr || removed.stdout || '').trim()}`);

  await withLanesLock(laneInfo.common, async () => {
    const fresh = await readLanesRegistry(laneInfo.common);
    const next = { ...(fresh.lanes ?? {}) };
    delete next[project];
    await writeLanesRegistry(laneInfo.common, { lanes: next });
  });
  return { exitCode: 0, payload: { project, removed: true } };
}

function assertLaneFlags(sub, args) {
  const allowed = new Set(LANE_ALLOWED_FLAGS[sub] ?? []);
  for (let index = 2; index < args.length; index += 1) {
    const token = args[index];
    if (typeof token !== 'string' || !token.startsWith('--')) continue;
    const name = token.slice(2).split('=')[0];
    if (!allowed.has(name)) {
      return `Unknown flag --${name} for \`zagent lane ${sub}\`. ${sub} takes: ${[...allowed].sort().map(f => `--${f}`).join(', ') || '(none)'}.`;
    }
  }
  return null;
}

/**
 * `zagent lane which|list|add|remove`. Intercepted in client.mjs's
 * `dynamicDispatcher` before the write/read fallback — `lane` is neither a
 * board write nor a read command, and there is no per-command routing table
 * in client.mjs to add it to otherwise.
 */
export async function dispatchLane(index, context) {
  const args = Array.isArray(context.args) ? context.args : [];
  const sub = String(args[1] ?? '').toLowerCase();
  if (!LANE_ALLOWED_FLAGS[sub]) {
    return refused('Usage: zagent lane which|list|add <PROJECT> [--path <dir>]|remove <PROJECT>');
  }
  const flagError = assertLaneFlags(sub, args);
  if (flagError) return refused(flagError);
  const laneInfo = context.laneInfo ?? await resolveLaneMode(context.cwd);
  if (sub === 'which') return laneWhich(index, laneInfo);
  if (sub === 'list') return laneList(laneInfo);
  if (sub === 'add') return laneAdd(index, context, laneInfo);
  return laneRemove(index, context, laneInfo);
}

/**
 * The async lane block `doctor` stashes on `index.laneDoctor` (D4) before
 * `local.mjs`'s synchronous `doctor` case reads it. Per-lane:
 * `{ project, path, exists, clean, head, provisioned, mergeHead,
 * unlandedBranches }`, plus a count of linked worktrees that are neither
 * the authority nor a registered lane. `diagnostic` is non-null only when
 * THIS invocation's own lane is unprovisioned or missing — the one lane
 * condition that belongs in `diagnostics` rather than the informational
 * `lanes` block (an unlanded `agent/*` elsewhere is a normal lane state,
 * not a fault).
 */
export async function computeDoctorLanes(authorityRoot, laneInfo) {
  const common = laneInfo?.common;
  if (!common) return { lanes: [], unregisteredWorktreeCount: 0, diagnostic: null };
  const registry = await readLanesRegistry(common);
  const entries = Object.entries(registry.lanes ?? {});
  const baseBranch = baseBranchOf(authorityRoot);

  const lanes = [];
  for (const [project, entry] of entries) {
    const exists = await pathExists(entry.path);
    let clean = null;
    let head = null;
    let mergeHead = null;
    let unlandedBranches = [];
    if (exists) {
      try { clean = (await git(entry.path, ['status', '--porcelain'])).trim() === ''; } catch { clean = null; }
      try { head = await git(entry.path, ['rev-parse', 'HEAD']); } catch { head = null; }
      try { mergeHead = await git(entry.path, ['rev-parse', '--verify', '-q', 'MERGE_HEAD']); } catch { mergeHead = null; }
      try {
        const raw = await git(entry.path, ['branch', '--list', `agent/${project}-*`, '--no-merged', baseBranch]);
        unlandedBranches = raw.split('\n').map(line => line.replace(/^\*?\s+/, '').trim()).filter(Boolean);
      } catch { unlandedBranches = []; }
    }
    lanes.push({ project, path: entry.path, exists, clean, head, provisioned: Boolean(entry.provisionedAt), mergeHead, unlandedBranches });
  }

  let unregisteredWorktreeCount = 0;
  try {
    const list = await git(authorityRoot, ['worktree', 'list', '--porcelain']);
    const worktreePaths = list.split(/\r?\n\r?\n/).map(block => {
      const match = /^worktree (.+)$/m.exec(block);
      return match ? match[1].trim() : null;
    }).filter(Boolean);
    const known = new Set([normalizeForCompare(authorityRoot), ...entries.map(([, entry]) => normalizeForCompare(entry.path))]);
    unregisteredWorktreeCount = worktreePaths.filter(worktreePath => !known.has(normalizeForCompare(worktreePath))).length;
  } catch {
    unregisteredWorktreeCount = 0;
  }

  let laneDiagnostic = null;
  if (laneInfo?.mode === 'lane') {
    const entry = registry.lanes?.[laneInfo.project];
    const exists = entry ? await pathExists(entry.path) : false;
    if (!entry || !entry.provisionedAt || !exists) {
      laneDiagnostic = diagnostic(
        'LANE_NOT_PROVISIONED',
        `Lane ${laneInfo.project} at ${laneInfo.path} is not fully provisioned; run \`zagent lane add ${laneInfo.project}\` again, or check Tools/lane_init.ps1.`,
        { severity: 'error', project: laneInfo.project, path: laneInfo.path },
      );
    }
  }

  return { lanes, unregisteredWorktreeCount, diagnostic: laneDiagnostic };
}
