#!/usr/bin/env node

import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { mkdir, readFile, rename, rm, writeFile } from 'node:fs/promises';

import { loadVault } from '../server/main.js';
import { inspectArguments } from './arguments.mjs';
import { dispatchRead } from './read.js';
import { dispatchWrite, isWriteCommand } from './write.mjs';

const DEFERRED_OR_REMOVED = new Set([
  'auth', 'provision', 'notion', 'contract',
]);

function valueFor(args, name) {
  const at = args.indexOf(name);
  if (at >= 0 && at + 1 < args.length) return args[at + 1];
  const match = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return match ? match.slice(name.length + 1) : undefined;
}

function withoutGlobalOptions(args) {
  const out = [];
  for (let i = 0; i < args.length; i += 1) {
    const value = args[i];
    if (value === '--json') continue;
    if (value === '--vault') {
      i += 1;
      continue;
    }
    if (typeof value === 'string' && value.startsWith('--vault=')) continue;
    out.push(value);
  }
  return out;
}

// Writer controls are intentionally local to the mutation protocol.  The
// historical closed vocabulary predates durable operation IDs, actor
// selection, and claim attempt CAS, so remove only these controls before the
// legacy inspector runs.  All ordinary command flags still go through its
// typo-catching validation.
function withoutWriterProtocolOptions(args) {
  const valued = new Set(['--attempt', '--claim-attempt', '--operation-id', '--op-id', '--actor', '--actor-email', '--kind', '--status', '--name', '--table']);
  const boolean = new Set(['--human', '--continuous']);
  const out = [];
  for (let i = 0; i < args.length; i += 1) {
    const value = String(args[i] ?? '');
    const prefix = [...valued].find(name => value.startsWith(`${name}=`));
    if (prefix) continue;
    if (valued.has(value)) {
      i += 1;
      continue;
    }
    if (boolean.has(value)) continue;
    out.push(args[i]);
  }
  return out;
}

function unsupportedReason(args) {
  const command = String(args[0] ?? '').toLowerCase();
  const sub = String(args[1] ?? '').toLowerCase();
  if (DEFERRED_OR_REMOVED.has(command)) {
    return `Command ${args[0]} is unavailable: the vault CLI is read-only and this operation has no local implementation`;
  }
  if (command === 'docs' && (sub === 'sync' || sub === 'write')) {
    return `Command docs ${sub} is unavailable: remote/document writes are removed from the read-only vault`;
  }
  return null;
}

function optionValue(args, name) {
  const at = args.indexOf(name);
  if (at >= 0 && at + 1 < args.length) return args[at + 1];
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return prefix ? prefix.slice(name.length + 1) : undefined;
}

async function readCliFiles(args, command) {
  const files = {};
  // `files.file` is the content contract used by the old dispatcher.  The
  // local executable accepts its path-valued equivalent and reads it without
  // ever writing a scratch or result file.
  if (command === 'gates' || command === 'guard') {
    const changedPath = optionValue(args, '--file');
    if (changedPath !== undefined) files.file = await readFile(path.resolve(changedPath), 'utf8');
  }
  if (command === 'guard') {
    const gatesPath = optionValue(args, '--gates');
    if (gatesPath !== undefined) files.gates = await readFile(path.resolve(gatesPath), 'utf8');
  }
  // Local mutation commands use the same path-valued file convention as the
  // legacy client.  The vault process owns the explicit vault, while these
  // files are caller inputs; slurp them before dispatch and never write a
  // result beside them.  A missing `--comment`/`--worklog` value is retained
  // as inline text for the local CLI convenience; `--file`/`--body` follows
  // the same fallback so small scripts need not create a temporary file.
  if (isWriteCommand([command])) {
    for (const name of ['file', 'comment', 'worklog', 'body', 'goal', 'dod']) {
      const value = optionValue(args, `--${name}`);
      if (value === undefined) continue;
      try {
        files[name] = await readFile(path.resolve(value), 'utf8');
      } catch (error) {
        if (error?.code !== 'ENOENT') throw error;
        files[name] = value;
      }
    }
  }
  return files;
}

function printPayload(payload) {
  process.stdout.write(`${JSON.stringify(payload ?? null)}\n`);
}

async function writeScratch(root, relative, value) {
  const target = path.join(root, '.zagent', relative);
  await mkdir(path.dirname(target), { recursive: true });
  const temporary = `${target}.tmp-${process.pid}`;
  await writeFile(temporary, value, 'utf8');
  try {
    await rename(temporary, target);
  } catch (error) {
    if (error?.code !== 'EEXIST' && error?.code !== 'EPERM') throw error;
    await rm(target, { force: true });
    await rename(temporary, target);
  }
}

/**
 * Persist this invocation's exit code, mirroring `client.mjs`'s `finish()` /
 * `persistLast` choke point (see its doc comment). `.zagent/last.exit` is
 * written from exactly one place in this file so a caller reading it after a
 * `node cli/zagent.mjs …` call never sees a code left over from a previous
 * invocation. `scratchRoot` is null exactly when `--vault` was never
 * supplied, in which case there is no `.zagent/` to write into at all — that
 * one path (the missing-`--vault` refusal) legitimately cannot record and is
 * the sole exception; every other return in `runCli` funnels through here.
 *
 * Best-effort: a filesystem that refuses this write also fails visibly
 * wherever the substantive result was produced, so swallowing a write error
 * here is strictly better than turning an otherwise-reported result into a
 * thrown exception over its own bookkeeping.
 */
async function persistExit(scratchRoot, response) {
  if (scratchRoot) {
    try {
      await writeScratch(scratchRoot, 'last.exit', `${response.exitCode}\n`);
    } catch {
      // best-effort; see doc comment above
    }
  }
  return response;
}

/** Persist the durable claim handoff consumed by the next tick step. */
async function persistMutationScratch(root, command, payload) {
  if (!payload || typeof payload !== 'object' || Array.isArray(payload)) return;
  await writeScratch(root, 'last.json', `${JSON.stringify(payload, null, 2)}\n`);
  if (command !== 'claim' && command !== 'next') return;
  const key = String(payload.key ?? '');
  if (!/^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(key)) return;
  const runRoot = path.join('run', key);
  await writeScratch(root, path.join(runRoot, 'ticket.json'), `${JSON.stringify(payload, null, 2)}\n`);
  await writeScratch(root, path.join(runRoot, 'body.md'), String(payload.body ?? ''));
  if (payload.claimAttempt || payload.attempt) {
    await writeScratch(root, path.join(runRoot, 'attempt'), `${String(payload.claimAttempt ?? payload.attempt)}\n`);
  }
}

export async function runCli(rawArgs = process.argv.slice(2)) {
  const args = Array.isArray(rawArgs) ? rawArgs.slice() : [];
  const vaultRoot = valueFor(args, '--vault');
  // Resolved once, before any command-specific branching (including the
  // pre-dispatch refusals below), and reused by every return through
  // `persistExit` — mirrors `client.mjs`'s `scratchRoot` (`runClient`,
  // "Resolve it ONCE, before ANY command-specific branching runs").
  const scratchRoot = vaultRoot ? path.resolve(vaultRoot) : null;
  const json = args.includes('--json');
  const commandArgs = withoutGlobalOptions(args);
  const command = String(commandArgs[0] ?? '').toLowerCase();

  // Help is handled before root loading and before the deferred-command gate.
  // This keeps `claim --help` useful even though claim itself is unavailable
  // in the local read-only implementation.
  const inspected = inspectArguments(isWriteCommand(commandArgs) ? withoutWriterProtocolOptions(commandArgs) : commandArgs);
  if (inspected.help) {
    let readOnly = true;
    if (vaultRoot) {
      try {
        const config = JSON.parse(await readFile(path.resolve(vaultRoot, 'vault.config.json'), 'utf8'));
        readOnly = config?.writes?.enabled !== true;
      } catch {
        // Help remains useful for a missing or malformed root. The normal
        // command path reports that root error after argument inspection.
      }
    }
    const payload = {
      usage: 'node cli/zagent.mjs --vault PATH COMMAND [--json]',
      readOnly,
      commands: ['show', 'check', 'queue', 'owns', 'next', 'claim', 'comment', 'move', 'finish', 'links', 'blocked', 'transitions', 'history', 'gates', 'guard', 'epic', 'sprint', 'version', 'update', 'estimate', 'flag', 'resolve', 'create', 'link', 'unlink', 'parent', 'ask', 'decide', 'shortfall', 'changelog', 'suggest', 'rows', 'suggestions', 'doctor', 'docs ls', 'docs read', 'docs search', 'docs status', 'board status'],
    };
    printPayload(payload);
    return persistExit(scratchRoot, { exitCode: 0, payload });
  }
  if (!vaultRoot) {
    // No `--vault` means no repo root to derive `.zagent/` from — `scratchRoot`
    // is null here by construction, so this is the one return in `runCli`
    // that legitimately cannot record an exit code (see `persistExit`'s doc
    // comment). Routed through `persistExit` anyway for structural
    // uniformity; it is a no-op given a null root.
    const response = { exitCode: 1, payload: { error: 'Missing required --vault ROOT' } };
    printPayload(response.payload);
    return persistExit(scratchRoot, response);
  }

  const unsupported = unsupportedReason(commandArgs);
  if (unsupported) {
    const response = { exitCode: 1, payload: { error: unsupported } };
    printPayload(response.payload);
    return persistExit(scratchRoot, response);
  }

  if (inspected.error) {
    const error = isWriteCommand(commandArgs)
      ? `${inspected.error} Vault writes require writes.enabled=true; this mutation is read-only until then.`
      : inspected.error;
    const response = { exitCode: 1, payload: { error } };
    printPayload(response.payload);
    return persistExit(scratchRoot, response);
  }

  try {
    // A registered checkout is the single mutable authority for this Git
    // repository. Reads from a secondary worktree must see that same board;
    // mutable check additionally takes the operation lock and recovers any
    // pending journal before evaluating the shared claim predicate.
    const { resolveAuthority, withAuthoritativeIndex } = await import('../server/operations.mjs');
    const authority = await resolveAuthority(path.resolve(vaultRoot));
    const index = await loadVault(authority);
    const files = await readCliFiles(commandArgs, command);
    let response;
    if (command === 'check') {
      response = await withAuthoritativeIndex(authority, fresh => dispatchRead(fresh, commandArgs, files));
    } else {
      response = await dispatchWrite(index, commandArgs, files) ?? dispatchRead(index, commandArgs, files);
    }
    if (command === 'history' && response?.exitCode === 0 && Array.isArray(response.payload)) {
      const { appendGitHistory } = await import('./git-history.mjs');
      const key = commandArgs.find(value => /^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(String(value)));
      const rawLimit = optionValue(commandArgs, '--limit');
      const limit = rawLimit === undefined ? 50 : Number(rawLimit);
      response = { ...response, payload: await appendGitHistory(authority, key, response.payload, Number.isFinite(limit) ? limit : 50) };
    }
    if (response?.exitCode === 0 && isWriteCommand(commandArgs)) {
      await persistMutationScratch(authority, command, response.payload);
    }
    printPayload(response.payload);
    return persistExit(scratchRoot, response);
  } catch (error) {
    const response = { exitCode: 1, payload: { error: error?.message ?? String(error) } };
    printPayload(response.payload);
    return persistExit(scratchRoot, response);
  }
}

const invokedPath = process.argv[1] ? path.resolve(process.argv[1]) : '';
const modulePath = path.resolve(fileURLToPath(import.meta.url));
if (invokedPath === modulePath) {
  const response = await runCli();
  process.exitCode = response.exitCode;
}

export { dispatchRead };
export default runCli;
