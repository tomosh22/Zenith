#!/usr/bin/env node

/**
 * Local replacement for the wire client used by Tools/zagent.
 *
 * The client owns argument/file transport and durable `.zagent` scratch.  It
 * does not implement board mutations.  A caller may inject `dispatch(context)`
 * (the mutating CLI does this), while the default falls back to the read-only
 * Vault dispatcher.  This keeps the transport usable during the staged
 * cutover without importing the SaaS application or a database driver.
 */

import { constants, promises as fs } from 'node:fs';
import { execFile as execFileCallback } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomUUID } from 'node:crypto';
import { promisify } from 'node:util';

import { inspectArguments } from './arguments.mjs';
import { dispatchWrite } from './write.mjs';

const execFile = promisify(execFileCallback);

export const PROTOCOL_VERSION = 1;
export const FILE_FLAGS = Object.freeze(['file', 'comment', 'worklog', 'body', 'goal', 'dod']);
export const SCRATCH_ROOT = '.zagent';

function optionValue(args, name) {
  for (let index = 0; index < args.length; index += 1) {
    const value = args[index];
    if (value === name && index + 1 < args.length) return args[index + 1];
    if (typeof value === 'string' && value.startsWith(`${name}=`)) return value.slice(name.length + 1);
  }
  return undefined;
}

function hasOption(args, name) {
  return args.includes(name) || args.some(value => typeof value === 'string' && value.startsWith(`${name}=`));
}

function withoutGlobalOptions(args) {
  const result = [];
  for (let index = 0; index < args.length; index += 1) {
    const value = args[index];
    if (value === '--json' || value === '--protocol') {
      if (value === '--protocol') index += 1;
      continue;
    }
    if (typeof value === 'string' && (value.startsWith('--vault=') || value.startsWith('--client=') || value.startsWith('--protocol='))) continue;
    if (value === '--vault' || value === '--client') {
      index += 1;
      continue;
    }
    result.push(value);
  }
  return result;
}

export function parseClientArgs(rawArgs = [], options = {}) {
  const args = Array.isArray(rawArgs) ? rawArgs.slice() : [];
  const vaultValue = optionValue(args, '--vault') ?? options.vaultRoot;
  const vaultRoot = vaultValue === undefined ? undefined : path.resolve(String(vaultValue));
  const commandArgs = withoutGlobalOptions(args);
  return {
    rawArgs: args,
    commandArgs,
    command: String(commandArgs[0] ?? '').toLowerCase(),
    vaultRoot,
    json: hasOption(args, '--json'),
    client: options.client ?? optionValue(args, '--client') ?? null,
    protocol: Number(optionValue(args, '--protocol') ?? PROTOCOL_VERSION),
  };
}

function flagValue(args, name) {
  return optionValue(args, `--${name}`);
}

function resolveInputPath(value, cwd, flag) {
  if (typeof value !== 'string' || value.trim() === '') throw new Error(`--${flag} requires a file path`);
  if (value.includes('\0')) throw new Error(`--${flag} contains an invalid path`);
  return path.resolve(cwd, value);
}

async function readInput(pathValue, cwd, flag) {
  const file = resolveInputPath(pathValue, cwd, flag);
  try { return await fs.readFile(file, 'utf8'); }
  catch (error) { throw new Error(`Cannot read --${flag} ${pathValue}: ${error.message}`); }
}

function parseWorkingTreeChanges(stdout) {
  const paths = [];
  for (const line of String(stdout ?? '').split(/\r?\n/)) {
    if (line.length <= 3) continue;
    for (const part of line.slice(3).split(' -> ')) {
      let value = part.trim();
      if (value.length >= 2 && value.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1);
      if (value && !paths.includes(value)) paths.push(value);
    }
  }
  return paths;
}

/** Return modified, deleted, renamed, and untracked files in the client repo. */
export async function workingTreeChanges(repoRoot) {
  try {
    const { stdout } = await execFile('git', ['-C', path.resolve(repoRoot), 'status', '--porcelain', '-uall'], { encoding: 'utf8' });
    return parseWorkingTreeChanges(stdout);
  } catch {
    // A gates call may legitimately run before the first edit. A guard call
    // handles the empty result explicitly; neither path falls back to a
    // commit range which would hide newly-created files.
    return [];
  }
}

async function addWorkingTreeInput(commandArgs, files, repoRoot) {
  const command = String(commandArgs[0] ?? '').toLowerCase();
  if ((command !== 'gates' && command !== 'guard') || files.file !== undefined || flagValue(commandArgs, 'text') !== undefined) return;
  const changed = await workingTreeChanges(repoRoot);
  if (command === 'guard' && changed.length === 0) throw new Error(`guard: nothing has changed in ${repoRoot} — the worker wrote no files.`);
  files.file = `${changed.join('\n')}\n`;
}

function ticketKeyForGate(args) {
  const value = args[1];
  return typeof value === 'string' && /^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(value) ? value : null;
}

async function readRecordedGateSelection(scratchRoot, commandArgs, files) {
  if (String(commandArgs[0] ?? '').toLowerCase() !== 'guard' || files.gates !== undefined) return;
  const key = ticketKeyForGate(commandArgs);
  if (!key) return;
  try {
    files.gates = await fs.readFile(path.join(scratchRoot, 'run', key, 'gates.json'), 'utf8');
  } catch (error) {
    if (error.code !== 'ENOENT') throw new Error(`Cannot read recorded gates for ${key}: ${error.message}`);
  }
}

const WRITE_COMMANDS = new Set([
  'next', 'claim', 'comment', 'move', 'finish', 'create', 'link', 'unlink',
  'parent', 'update', 'estimate', 'flag', 'resolve', 'ask', 'decide',
  'shortfall', 'changelog', 'suggest',
]);

// Agent mutations which carry the claim CAS token.  `comment` is deliberately
// absent: it is the comment-only abandonment path after a human takeover.
const ATTEMPT_COMMANDS = new Set(['finish', 'move', 'update', 'estimate', 'flag', 'resolve']);

function withoutWriterProtocolOptions(args) {
  const valued = new Set(['--attempt', '--claim-attempt', '--operation-id', '--op-id', '--actor', '--actor-email']);
  const boolean = new Set(['--human']);
  const result = [];
  for (let index = 0; index < args.length; index += 1) {
    const value = String(args[index] ?? '');
    const prefix = [...valued].find(name => value.startsWith(`${name}=`));
    if (prefix) continue;
    if (valued.has(value)) {
      index += 1;
      continue;
    }
    if (boolean.has(value)) continue;
    result.push(args[index]);
  }
  return result;
}

function hasWriterAttempt(args) {
  return args.some(value => value === '--attempt' || value === '--claim-attempt'
    || (typeof value === 'string' && (value.startsWith('--attempt=') || value.startsWith('--claim-attempt='))));
}

function actorIsHuman(args) {
  if (args.includes('--human')) return true;
  const actor = optionValue(args, '--actor');
  return typeof actor === 'string' && actor.toLowerCase() === 'human';
}

function ticketKeyForAttempt(args) {
  const command = String(args[0] ?? '').toLowerCase();
  if (!ATTEMPT_COMMANDS.has(command)) return null;
  const value = args[1];
  return typeof value === 'string' && /^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(value) ? value : null;
}

/**
 * Read only the token persisted by a previous local claim.  The current board
 * is intentionally never consulted here: a missing or stale token must reach
 * the writer's ownership check and fail closed.
 */
export async function persistedClaimAttempt(scratchRoot, args = []) {
  const key = ticketKeyForAttempt(args);
  if (!key || actorIsHuman(args) || hasWriterAttempt(args)) return null;
  const file = path.join(scratchRoot, 'run', key, 'ticket.json');
  let value;
  try {
    value = JSON.parse(await fs.readFile(file, 'utf8'));
  } catch (error) {
    if (error.code === 'ENOENT') return null;
    throw new Error(`Cannot read persisted claim scratch for ${key}: ${error.message}`);
  }
  const attempt = value && typeof value === 'object' ? value.claimAttempt : undefined;
  return typeof attempt === 'string' && attempt.trim() !== '' ? attempt : null;
}

export async function injectPersistedClaimAttempt(scratchRoot, args = []) {
  const attempt = await persistedClaimAttempt(scratchRoot, args);
  return attempt ? [...args, '--attempt', attempt] : [...args];
}

async function maybeReadGates(value, cwd) {
  if (value === undefined) return undefined;
  const file = path.resolve(cwd, value);
  try {
    const stat = await fs.stat(file);
    if (stat.isFile()) return await fs.readFile(file, 'utf8');
  } catch (error) {
    if (error.code !== 'ENOENT') throw new Error(`Cannot read --gates ${value}: ${error.message}`);
  }
  // A direct JSON value is useful to callers embedding the local client.  A
  // missing path which is not JSON remains an actionable missing-file error.
  if (/^\s*[[{]/.test(value)) return value;
  throw new Error(`Cannot read --gates ${value}: file does not exist`);
}

/** Read all path-valued protocol inputs without writing anything. */
export async function readInputFiles(commandArgs, options = {}) {
  const args = Array.isArray(commandArgs) ? commandArgs : [];
  const cwd = path.resolve(options.cwd ?? process.cwd());
  const files = {};
  const command = String(args[0] ?? '').toLowerCase();
  for (const name of FILE_FLAGS) {
    const value = flagValue(args, name);
    if (value === undefined) continue;
    try {
      files[name] = await readInput(value, cwd, name);
    } catch (error) {
      // The PowerShell client treats a missing path-valued mutation input as
      // inline text (small comments and work logs are common).  Gates and
      // guard remain strict because a typo there must never weaken a verdict.
      if (WRITE_COMMANDS.has(command) && /ENOENT|file does not exist/i.test(error.message)) files[name] = value;
      else throw error;
    }
  }
  if ((command === 'gates' || command === 'guard') && files.file === undefined) {
    const text = flagValue(args, 'text');
    if (text !== undefined) files.file = text;
  }
  if (command === 'guard') {
    const gates = flagValue(args, 'gates');
    if (gates !== undefined) files.gates = await maybeReadGates(gates, cwd);
  }
  return files;
}

function normalizeResult(value) {
  if (value && typeof value === 'object' && Number.isInteger(value.exitCode)) {
    return {
      exitCode: value.exitCode,
      payload: value.payload ?? value.result ?? null,
      text: value.text,
      writes: value.writes ?? value.payload?.writes ?? [],
    };
  }
  if (value && typeof value === 'object' && Object.hasOwn(value, 'result')) {
    return { exitCode: 0, payload: value.result, text: value.text, writes: value.writes ?? value.result?.writes ?? [] };
  }
  if (value === undefined || value === null) return { exitCode: 1, payload: { error: 'The local Vault dispatcher returned no result.' }, writes: [] };
  return { exitCode: 0, payload: value, writes: value?.writes ?? [] };
}

function safeScratchRelative(value) {
  if (typeof value !== 'string' || value.trim() === '' || path.isAbsolute(value) || value.includes('\0')) throw new Error('Scratch write path must be relative');
  const normalized = value.replaceAll('\\', '/');
  if (normalized.split('/').some(part => part === '..' || part === '.git' || part === '.zagent')) throw new Error('Scratch write escapes .zagent/run');
  return normalized.replace(/^\.\//, '');
}

function bytesFromWrite(write) {
  if (write?.bytes instanceof Uint8Array) return Buffer.from(write.bytes);
  if (typeof write?.bytes === 'string') {
    if (write.encoding === 'utf8' || write.encoding === 'utf-8' || write.encoding === 'text') return Buffer.from(write.bytes);
    return Buffer.from(write.bytes, 'base64');
  }
  if (typeof write?.content === 'string') return Buffer.from(write.content);
  if (typeof write?.text === 'string') return Buffer.from(write.text);
  if (typeof write?.data === 'string') return Buffer.from(write.data, write.encoding === 'base64' ? 'base64' : 'utf8');
  if (write?.value !== undefined) return Buffer.from(`${JSON.stringify(write.value, null, 2)}\n`);
  throw new Error('Scratch write has no content');
}

async function atomicWrite(file, bytes, { exclusive = false } = {}) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  if (exclusive) {
    await fs.writeFile(file, bytes, { flag: 'wx' });
    return;
  }
  const temporary = `${file}.tmp-${randomUUID()}`;
  try {
    await fs.writeFile(temporary, bytes, { flag: 'wx' });
    await fs.rename(temporary, file);
  } finally {
    await fs.rm(temporary, { force: true });
  }
}

async function archivePreviousRun(root, key) {
  if (!/^[A-Za-z0-9_-]{1,100}$/.test(String(key ?? ''))) return;
  const current = path.join(root, 'run', key);
  try { await fs.stat(current); } catch (error) { if (error.code === 'ENOENT') return; throw error; }
  const previous = path.join(root, 'run', `${key}.prev`);
  await fs.rm(previous, { recursive: true, force: true });
  await fs.rename(current, previous);
}

function scratchKey(value) {
  return /^[A-Za-z0-9_-]{1,100}$/.test(String(value ?? '')) ? String(value) : null;
}

async function applyScratchWrites(root, writes) {
  for (const write of Array.isArray(writes) ? writes : []) {
    if (!write || typeof write !== 'object') continue;
    const base = String(write.base ?? 'scratch').toLowerCase();
    if (base !== 'scratch') continue; // authoritative files belong to the operation engine
    const relative = safeScratchRelative(write.path ?? write.relativePath);
    const target = path.resolve(root, 'run', relative);
    if (!target.startsWith(path.resolve(root, 'run') + path.sep)) throw new Error('Scratch write escapes .zagent/run');
    await atomicWrite(target, bytesFromWrite(write));
  }
}

async function writeCompatibilityScratch(root, command, result, options = {}) {
  const payload = result.payload;
  const key = payload?.key ?? payload?.ticket?.key;
  const safeKey = scratchKey(key);
  if ((command === 'claim' || command === 'next') && result.exitCode === 0 && safeKey) {
    await archivePreviousRun(root, safeKey);
  }
  await applyScratchWrites(root, result.writes);
  if (command === 'gates' && result.exitCode === 0 && safeKey) {
    // `guard` consumes this exact JSON and compares both the selected lines
    // and the changed set. Keep it in the ticket run directory so a new
    // claim archives the complete prior attempt together with its token.
    await atomicWrite(path.join(root, 'run', safeKey, 'gates.json'), Buffer.from(`${JSON.stringify(payload, null, 2)}\n`));
  }
  if (!((command === 'claim' || command === 'next') && result.exitCode === 0 && safeKey)) return;
  const run = path.join(root, 'run', safeKey);
  await fs.mkdir(run, { recursive: true });
  const ticket = payload?.ticket && typeof payload.ticket === 'object' ? payload.ticket : payload;
  await atomicWrite(path.join(run, 'ticket.json'), Buffer.from(`${JSON.stringify(ticket, null, 2)}\n`));
  let body = typeof payload?.body === 'string' ? payload.body
    : typeof payload?.bodyText === 'string' ? payload.bodyText
      : typeof payload?.bodyMarkdown === 'string' ? payload.bodyMarkdown : undefined;
  if (body === undefined && typeof options.ticketProvider === 'function') {
    const current = await options.ticketProvider(String(key));
    if (current) {
      if (current.ticket && typeof current.ticket === 'object') await atomicWrite(path.join(run, 'ticket.json'), Buffer.from(`${JSON.stringify(current.ticket, null, 2)}\n`));
      if (typeof current.body === 'string') body = current.body;
    }
  }
  if (body !== undefined) await atomicWrite(path.join(run, 'body.md'), Buffer.from(body));
  if (typeof payload?.drift === 'string') await atomicWrite(path.join(run, 'drift.txt'), Buffer.from(payload.drift));
}

async function persistLast(root, result, json) {
  await fs.mkdir(root, { recursive: true });
  if (json) await atomicWrite(path.join(root, 'last.json'), Buffer.from(`${JSON.stringify(result.payload ?? null, null, 2)}\n`));
  await atomicWrite(path.join(root, 'last.exit'), Buffer.from(`${result.exitCode}\n`));
}

async function dynamicDispatcher(context) {
  const [{ loadVault }, { resolveAuthority, withAuthoritativeIndex }, { dispatchRead }, { appendGitHistory }] = await Promise.all([
    import('../server/main.js'),
    import('../server/operations.mjs'),
    import('./read.js'),
    import('./git-history.mjs'),
  ]);
  const authority = await resolveAuthority(context.vaultRoot);
  const index = await loadVault(authority);
  // Mutations have one explicit local owner.  The dispatcher returns null for
  // retained read commands; only those then reach the read fallback below.
  const write = await dispatchWrite(index, context.args, context.files);
  if (write !== null && write !== undefined) return write;
  // Read-only fallback keeps this wrapper useful before the write dispatcher
  // is linked.  It deliberately loads files from the explicit vault root.
  const command = String(context.args[0] ?? '').toLowerCase();
  const read = command === 'check'
    ? await withAuthoritativeIndex(authority, current => dispatchRead(current, context.args, context.files))
    : dispatchRead(index, context.args, context.files);
  if (command === 'history' && read?.exitCode === 0) {
    const key = context.args.find(value => /^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(String(value ?? '')));
    if (key) {
      const limitValue = optionValue(context.args, '--limit');
      const limit = limitValue === undefined ? 50 : Number(limitValue);
      const frozen = Array.isArray(read.payload) ? read.payload : [];
      const payload = await appendGitHistory(authority, key, frozen, Number.isInteger(limit) && limit >= 0 ? limit : 50);
      return { ...read, payload };
    }
  }
  return read;
}

function usage() {
  return {
    usage: 'node cli/client.mjs --vault ROOT COMMAND [flags] [--json]',
    protocolVersion: PROTOCOL_VERSION,
    scratch: '.zagent/last.json, .zagent/last.exit, .zagent/run/<KEY>/ticket.json, .zagent/run/<KEY>/body.md',
    readOnlyFallback: true,
  };
}

/**
 * Execute a local client call.  `options.dispatch` receives one object:
 * `{vaultRoot, args, files, client, protocol, cwd, rawArgs}`.  It may return
 * `{exitCode, payload}` or the operation engine's `{result, commit}` shape.
 */
export async function runClient(rawArgs = process.argv.slice(2), options = {}) {
  const parsed = parseClientArgs(rawArgs, options);
  if (parsed.commandArgs.includes('--help') || parsed.commandArgs[0] === 'help' || !parsed.commandArgs[0]) {
    const result = { exitCode: 0, payload: usage(), writes: [] };
    if (parsed.vaultRoot) await persistLast(path.join(options.repoRoot ?? parsed.vaultRoot, SCRATCH_ROOT), result, parsed.json);
    return result;
  }
  if (!parsed.vaultRoot) return { exitCode: 1, payload: { error: 'Missing required --vault ROOT' }, writes: [] };
  const cwd = path.resolve(options.cwd ?? process.cwd());
  const scratch = path.join(path.resolve(options.repoRoot ?? parsed.vaultRoot), SCRATCH_ROOT);
  let result;
  try {
    const inspected = inspectArguments(withoutWriterProtocolOptions(parsed.commandArgs));
    if (inspected.help) {
      result = { exitCode: 0, payload: usage(), writes: [] };
    } else if (inspected.error) {
      result = { exitCode: 1, payload: { error: inspected.error }, writes: [] };
    } else {
      // The old launcher did not put the claim token on finish/move/update.
      // Recover it from the durable claim scratch before dispatching, without
      // reading the current ticket to manufacture a fresh token.
      const commandArgs = await injectPersistedClaimAttempt(scratch, parsed.commandArgs);
      const files = await readInputFiles(commandArgs, { cwd });
      await addWorkingTreeInput(commandArgs, files, path.resolve(options.repoRoot ?? parsed.vaultRoot));
      await readRecordedGateSelection(scratch, commandArgs, files);
      const context = {
        vaultRoot: parsed.vaultRoot,
        args: commandArgs,
        files,
        client: parsed.client,
        protocol: parsed.protocol,
        cwd,
        rawArgs: parsed.rawArgs,
      };
      const dispatch = options.dispatch ?? dynamicDispatcher;
      const value = await dispatch(context);
      result = normalizeResult(value);
    }
  } catch (error) {
    result = { exitCode: Number.isInteger(error?.exitCode) ? error.exitCode : 1, payload: { error: error.message ?? String(error) }, writes: [] };
  }
  try {
    const ticketProvider = options.ticketProvider ?? (async key => {
      if (parsed.command !== 'claim' && parsed.command !== 'next') return null;
      const [{ loadVault }, { resolveAuthority }] = await Promise.all([
        import('../server/main.js'),
        import('../server/operations.mjs'),
      ]);
      const authority = await resolveAuthority(parsed.vaultRoot);
      const index = await loadVault(authority);
      const record = [...index.tickets.values()].find(item => String(item.metadata?.key ?? '').toUpperCase() === String(key).toUpperCase());
      return record ? { ticket: record.metadata, body: record.bodyText } : null;
    });
    await writeCompatibilityScratch(scratch, parsed.command, result, { ...options, ticketProvider });
    await persistLast(scratch, result, parsed.json);
  } catch (error) {
    // A scratch failure must be visible, but must not turn a successfully
    // completed board operation into an ambiguous result.  Preserve the
    // operation exit code and attach a diagnostic for callers.
    result = {
      ...result,
      scratchError: error.message ?? String(error),
      payload: { ...(result.payload && typeof result.payload === 'object' ? result.payload : {}), scratchError: error.message ?? String(error) },
    };
    try { await persistLast(scratch, result, parsed.json); } catch { /* no further durable path is available */ }
  }
  return result;
}

function outputResult(result, json) {
  if (json) return `${JSON.stringify(result.payload ?? null)}\n`;
  if (typeof result.text === 'string') return `${result.text}\n`;
  return `${JSON.stringify(result.payload ?? null)}\n`;
}

const invoked = process.argv[1] ? path.resolve(process.argv[1]) : '';
if (invoked === path.resolve(fileURLToPath(import.meta.url))) {
  const parsed = parseClientArgs(process.argv.slice(2));
  const result = await runClient(process.argv.slice(2));
  process.stdout.write(outputResult(result, parsed.json));
  process.exitCode = result.exitCode;
}

export default runClient;
