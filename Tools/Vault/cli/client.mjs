#!/usr/bin/env node

/**
 * Local replacement for the wire client used by Tools/zagent.
 *
 * The client owns argument/file transport and durable `.zagent` scratch.  It
 * does not implement board mutations.  A caller may inject `dispatch(context)`
 * (the mutating CLI does this), while the default falls back to the read-only
 * Vault dispatcher.  This keeps the transport usable during the staged
 * cutover without importing the previous board application or a database driver.
 */

import { constants, promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { randomUUID } from 'node:crypto';

import { inspectArguments } from './arguments.mjs';
import { dispatchWrite } from './write.mjs';
import { matchGoverningRulings, RULINGS_FILE_MIN_SCORE } from './legacy-pure/ticket.mjs';
import { forbiddenAbandonFlags, gateVerdictFresh, guardNeedsFreshnessOverride } from './tick-checks.mjs';
import { workingTreeChanges } from './working-tree.mjs';

export const PROTOCOL_VERSION = 1;
export const SCRATCH_ROOT = '.zagent';

// ★ A flag's meaning depends on the COMMAND it appears on, not just its
// name — `--goal` is a FILE PATH for `update` (replace the Goal section
// from a file) and free PROSE for `sprint create`/`sprint gate` (the
// sprint's own goal text; write.mjs's `sprint()` reads it straight off
// argv, never through `files`). This used to be one global flag-name list
// (`FILE_FLAGS`) checked against every command, plus a second list
// (`WRITE_COMMANDS`) deciding whether a missing path falls back to literal
// text — two mechanisms answering the one question "is this flag a file
// HERE". A command missing from the second list inherited the first list's
// strictness with no fallback at all, which is exactly what `sprint` did:
// `zagent sprint create <NAME> --goal "some prose"` tried to open the prose
// as a filename and crashed with `Cannot read --goal …: ENOENT` — a
// regression of a trap this repo already found and fixed once, in the
// retired PowerShell client (see MEMORY's `--goal` reference note).
//
// One table now answers both questions per command: which flags are
// file-valued at all, and whether a missing path is a hard error
// ('strict' — gates/guard/docs, where a typo must never silently weaken a
// verdict or write the wrong page) or falls back to inline text ('lenient'
// — every other write command, where a missing path commonly means the
// caller passed a short comment or worklog directly). A command absent
// from this table has no file-valued flags at all — `sprint`, `version`,
// `link`, etc. — so `--goal` on `sprint` is never even looked at here, and
// `write.mjs`'s own command reads whatever argv gave it.
const PATH_VALUED_FLAGS_BY_COMMAND = Object.freeze({
  comment: Object.freeze({ flags: Object.freeze(['file', 'comment']), mode: 'lenient' }),
  finish: Object.freeze({ flags: Object.freeze(['comment', 'worklog']), mode: 'lenient' }),
  create: Object.freeze({ flags: Object.freeze(['file', 'body']), mode: 'lenient' }),
  update: Object.freeze({ flags: Object.freeze(['body', 'goal', 'dod']), mode: 'lenient' }),
  ask: Object.freeze({ flags: Object.freeze(['file']), mode: 'lenient' }),
  decide: Object.freeze({ flags: Object.freeze(['file']), mode: 'lenient' }),
  shortfall: Object.freeze({ flags: Object.freeze(['file']), mode: 'lenient' }),
  changelog: Object.freeze({ flags: Object.freeze(['file']), mode: 'lenient' }),
  suggest: Object.freeze({ flags: Object.freeze(['file']), mode: 'lenient' }),
  gates: Object.freeze({ flags: Object.freeze(['file']), mode: 'strict' }),
  guard: Object.freeze({ flags: Object.freeze(['file']), mode: 'strict' }),
  docs: Object.freeze({ flags: Object.freeze(['file']), mode: 'strict' }),
});
const NO_PATH_VALUED_FLAGS = Object.freeze({ flags: Object.freeze([]), mode: 'lenient' });

/**
 * The file-valued flags for one command, and whether a missing path is a
 * hard error ('strict') or falls back to literal inline text ('lenient').
 * Exported so a caller (or a test) can ask the same question `client.mjs`
 * itself asks, without duplicating the table.
 */
export function pathValuedFlagsFor(command) {
  return PATH_VALUED_FLAGS_BY_COMMAND[String(command ?? '').toLowerCase()] ?? NO_PATH_VALUED_FLAGS;
}

// Retained for callers that used to enumerate "every flag name ANY command
// might read from a file" (the old, command-blind list). Nothing in this
// module derives behaviour from it any more — `pathValuedFlagsFor` does —
// but it stays as a deduplicated union so an external reader that still
// imports it keeps a truthful (if less precise) answer rather than an
// import error.
export const FILE_FLAGS = Object.freeze([
  ...new Set(Object.values(PATH_VALUED_FLAGS_BY_COMMAND).flatMap(entry => entry.flags)),
]);

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

// `workingTreeChanges` moved to working-tree.mjs (with its parser) so
// tick-checks.mjs can derive the same changed-file set for the excursion and
// commit-completeness checks without importing this module and closing an
// import cycle through write.mjs. Re-exported here because it is already
// part of this module's public surface (tests import it from here).
export { workingTreeChanges };

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
  const { flags: pathFlags, mode } = pathValuedFlagsFor(command);
  for (const name of pathFlags) {
    const value = flagValue(args, name);
    if (value === undefined) continue;
    try {
      files[name] = await readInput(value, cwd, name);
    } catch (error) {
      // The PowerShell client treats a missing path-valued mutation input as
      // inline text (small comments and work logs are common).  Gates,
      // guard and docs remain strict because a typo there must never weaken
      // a verdict or silently write the wrong page — see
      // `PATH_VALUED_FLAGS_BY_COMMAND` above for which commands are which.
      if (mode === 'lenient' && /ENOENT|file does not exist/i.test(error.message)) files[name] = value;
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

// Returns whether a prior run directory was actually archived, so the caller
// can record `archivedPriorRun` on the claim/next payload — present and
// `false` on a first claim, not merely absent, so a reader can tell this
// client ran the check at all (see the call site in writeCompatibilityScratch).
//
// ★ ABSENT HAS A THIRD MEANING, so do not read it as "an older client". If the
// rm succeeds and the rename then throws (EPERM with a handle open under the
// old run directory is realistic on Windows), the older `.prev` is already
// gone, the STALE run directory survives, no fresh scratch is written, and the
// exit code stays 0 — with the flag absent. `scratchError` on the payload is
// the only thing that distinguishes that from an old client, so check it
// before concluding the scratch you are looking at is fresh.
async function archivePreviousRun(root, key) {
  if (!/^[A-Za-z0-9_-]{1,100}$/.test(String(key ?? ''))) return false;
  const current = path.join(root, 'run', key);
  try { await fs.stat(current); } catch (error) { if (error.code === 'ENOENT') return false; throw error; }
  const previous = path.join(root, 'run', `${key}.prev`);
  await fs.rm(previous, { recursive: true, force: true });
  await fs.rename(current, previous);
  return true;
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

// ★ I3 now requires "every recorded RULING that governs the ticket's subject
// matter" pasted into the worker prompt (tick.md:66-70) — but finding those
// was entirely manual, and a loop worker has no shell and no network, so a
// ruling it is never handed does not exist for it. ZM-23 shipped a trainer
// row contradicting ZM-D-196 Ruling 3 (not ZM-D-203, a different entry — see
// `legacy-pure/ticket.mjs`'s copy of this note for the verification) and
// pinned the violation with a new test asserting it was correct; nothing
// mechanical could have caught it.
//
// Pure rendering, separate from the `fs.readFile` below, so it can be
// exercised directly against a fabricated payload/decision-log string
// without a real claim or a real repo on disk.
export function renderRulingsMarkdown(key, payload, decisionLogText) {
  const decisionLog = typeof payload?.decisionLog === 'string' && payload.decisionLog.trim() ? payload.decisionLog.trim() : null;
  const decisionIdPrefix = typeof payload?.decisionIdPrefix === 'string' && payload.decisionIdPrefix.trim() ? payload.decisionIdPrefix.trim() : null;
  if (!decisionLog || !decisionIdPrefix) {
    return `# Rulings for ${key}\n\n`
      + 'This ticket\'s category declares no decision log (`decisionLog`/`decisionIdPrefix` unset in '
      + 'zagent.project.json) — nothing was scanned. Written empty on purpose: a missing file and an '
      + '"I checked and found nothing" file must not read the same to a worker that cannot tell them apart.\n';
  }
  if (decisionLogText === null || decisionLogText === undefined) {
    return `# Rulings for ${key}\n\n`
      + `Declared decision log \`${decisionLog}\` (prefix \`${decisionIdPrefix}\`) could not be read — nothing was scanned.\n`;
  }
  const goal = typeof payload?.goal === 'string' ? payload.goal : '';
  const definitionOfDone = Array.isArray(payload?.definitionOfDone)
    ? payload.definitionOfDone.map(item => (typeof item?.text === 'string' ? item.text : String(item ?? ''))).join('\n')
    : '';
  const subject = `${goal}\n${definitionOfDone}`;
  const matches = matchGoverningRulings(decisionLogText, decisionIdPrefix, subject, { minScore: RULINGS_FILE_MIN_SCORE, limit: 25 });
  const lines = [`# Rulings for ${key}`, '', `Decision log: \`${decisionLog}\` (prefix \`${decisionIdPrefix}\`).`, ''];
  if (matches.length === 0) {
    lines.push(
      'No entry matched the Goal/Definition-of-Done terms scanned for. This is a candidate scan against '
      + 'this ticket\'s Goal and Definition of Done, not proof nothing applies — grep the log yourself before '
      + 'assuming it is clean.',
    );
  } else {
    lines.push(
      `${matches.length} candidate ruling(s), strongest match first. This is RECALL-tuned (a spurious `
      + 'candidate costs a few seconds; a missed one ships a violation) — verify each before pasting it into '
      + 'the worker prompt (I3):',
      '',
    );
    for (const match of matches) {
      lines.push(
        `## ${match.id}`,
        '',
        `Matched ${match.identifierHits} identifier hit(s), ${match.nounHits} noun hit(s).`,
        '',
        `> ${match.snippet.replace(/\n/g, '\n> ')}`,
        '',
      );
    }
  }
  return `${lines.join('\n')}\n`;
}

async function writeRulingsFile(scratchRoot, repoRoot, key, payload) {
  const decisionLog = typeof payload?.decisionLog === 'string' ? payload.decisionLog.trim() : '';
  const decisionIdPrefix = typeof payload?.decisionIdPrefix === 'string' ? payload.decisionIdPrefix.trim() : '';
  let decisionLogText = null;
  if (decisionLog && decisionIdPrefix) {
    try {
      decisionLogText = await fs.readFile(path.resolve(repoRoot, decisionLog), 'utf8');
    } catch {
      decisionLogText = null;
    }
  }
  const markdown = renderRulingsMarkdown(key, payload, decisionLogText);
  await atomicWrite(path.join(scratchRoot, 'run', key, 'rulings.md'), Buffer.from(markdown));
}

async function writeCompatibilityScratch(root, command, result, options = {}) {
  const payload = result.payload;
  const key = payload?.key ?? payload?.ticket?.key;
  const safeKey = scratchKey(key);
  if ((command === 'claim' || command === 'next') && result.exitCode === 0 && safeKey) {
    // Mutate the SAME payload object `result.payload` points at — `finish()`
    // (persistLast + outputResult) reads `result.payload` after this function
    // returns, and this is the smallest change that reaches both `last.json`
    // and `--json` stdout without threading a second value through runClient.
    //
    // The shape check is BEFORE the archive, not after it. It cannot currently
    // fail — `safeKey` is non-null only when `payload.key` or
    // `payload.ticket.key` is a matching string, which no primitive, null or
    // array can satisfy — but guarding afterwards would mean that if it ever
    // did fail, the rename had already happened and we would report nothing:
    // "archived but unreported" is the one state the docs promise cannot
    // occur, so make it unreachable rather than merely unlikely.
    if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
      payload.archivedPriorRun = await archivePreviousRun(root, safeKey);
    }
  }
  await applyScratchWrites(root, result.writes);
  if (command === 'gates' && result.exitCode === 0 && safeKey) {
    // `guard` consumes this exact JSON and compares both the selected lines
    // and the changed set. Keep it in the ticket run directory so a new
    // claim archives the complete prior attempt together with its token.
    await atomicWrite(path.join(root, 'run', safeKey, 'gates.json'), Buffer.from(`${JSON.stringify(payload, null, 2)}\n`));
  }
  if (command === 'guard' && safeKey && typeof options.files?.file === 'string') {
    // tick.md step 6.4 tells the orchestrator to write this by hand "for
    // step 7's docs-mirror check and for the work log" — `guard` already
    // computed this exact changed set to do its own comparison, so nothing
    // should depend on a human remembering to pipe it to a file themselves.
    // Written regardless of guard's own exit code: a Blocked ticket's work
    // log benefits from an accurate changed set just as much as a Done one.
    await atomicWrite(path.join(root, 'run', safeKey, 'changed.txt'), Buffer.from(options.files.file));
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
  // `root` is always `<repoRoot>/.zagent` (SCRATCH_ROOT above) — derive the
  // repo root from it rather than threading a second path through every
  // caller of writeCompatibilityScratch.
  await writeRulingsFile(root, path.dirname(root), safeKey, payload);
}

async function persistLast(root, result, json) {
  await fs.mkdir(root, { recursive: true });
  if (json) await atomicWrite(path.join(root, 'last.json'), Buffer.from(`${JSON.stringify(result.payload ?? null, null, 2)}\n`));
  await atomicWrite(path.join(root, 'last.exit'), Buffer.from(`${result.exitCode}\n`));
}

// The sentinel `.zagent/last.exit` carries between "this invocation has not
// recorded yet" and "this invocation recorded 0" — a NON-NUMERIC token, so a
// reader that parses it as an integer gets a clear non-answer rather than a
// number that happens to mean success. Mirrors `tick_gates.json`'s own
// `state: "running"`, which exists for the identical reason on the gate
// side: a killed run must not leave behind a verdict indistinguishable from
// a completed one.
const LAST_EXIT_RUNNING = 'running';

/**
 * Invalidate this invocation's slot BEFORE any command-specific logic —
 * including a pre-dispatch usage refusal — gets a chance to return without
 * recording. `.zagent/last.exit` exists precisely so a caller can read an
 * exit code no shell or harness can corrupt; a stale value left over from an
 * earlier command is worse than none; it is indistinguishable from a fresh,
 * current answer, and nothing downstream ever double-checks it. `last.json`
 * gets the same treatment for the same reason — a stale payload sitting
 * next to a fresh exit code (or the reverse) is its own trap, and
 * `persistLast` itself only ever rewrites `last.json` when `--json` was
 * passed, so a stale one otherwise survives forever untouched.
 *
 * Best-effort: a filesystem that cannot be written to here will also fail
 * the real persist later (surfaced there as `scratchError`), so silently
 * proceeding to run the actual command is strictly better than refusing an
 * otherwise-working invocation over its own bookkeeping write.
 */
async function markRunning(root) {
  try {
    await fs.mkdir(root, { recursive: true });
    await atomicWrite(path.join(root, 'last.exit'), Buffer.from(`${LAST_EXIT_RUNNING}\n`));
    await fs.rm(path.join(root, 'last.json'), { force: true });
  } catch {
    /* best-effort; see doc comment above */
  }
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
  // ★★ STEP 6.4's FRESHNESS QUESTION FOLDS INTO `guard`, BECAUSE GUARD IS
  // ALREADY THE MANDATORY MERGE-BLOCKING STEP (tick.md:1107-1136,
  // tick.md:1274 — "a rule enforced at a check that already fails is a rule,
  // and the same rule in this paragraph is a hope").
  //
  // `guard`'s own logic (dispatchLocal, via dispatchRead) answers "does the
  // recorded gate selection still cover this diff" — it never reads
  // `tick_gates.json` and cannot answer "did the run that produced that
  // selection actually finish, against the tree HEAD is on right now".
  // Reading state/headSha/skipped correctly "by eye" is exactly the shape
  // this repo keeps finding does not survive an unattended loop.
  //
  // Only ever turns a WOULD-BE PASS into a failure — never the reverse. A
  // guard refusal that already fired carries its own specific reason, and a
  // stale local verdict has nothing to add to it, so this is gated on guard
  // itself having answered 0.
  if (command === 'guard') {
    const key = ticketKeyForGate(context.args);
    if (key) {
      const repoRoot = context.repoRoot ?? context.vaultRoot;
      const verdict = await gateVerdictFresh({ repoRoot, key });
      if (guardNeedsFreshnessOverride(read.exitCode, verdict.fresh)) {
        process.stderr.write(`zagent: guard ${key} — guard passed, but its gate verdict is not fresh: ${verdict.reason}. Treating this as a guard failure — a stale or missing tick_gates.json cannot certify the tree HEAD is on right now.\n`);
        return {
          ...read,
          exitCode: 1,
          payload: {
            ...(read.payload && typeof read.payload === 'object' && !Array.isArray(read.payload) ? read.payload : {}),
            error: true,
            gateVerdictFresh: false,
            gateVerdictReason: verdict.reason,
          },
        };
      }
      // A genuine fresh PASS used to leave the payload silent about the fold
      // ever having run: the branch above only ever ADDS `gateVerdictFresh`
      // on the override path, so a caller reading a successful guard's
      // payload had no positive evidence the freshness check happened at
      // all — only the absence of a refusal, indistinguishable from a guard
      // that never consulted `tick_gates.json` in the first place. Make the
      // success path self-describing too.
      if (read.exitCode === 0) {
        return {
          ...read,
          payload: {
            ...(read.payload && typeof read.payload === 'object' && !Array.isArray(read.payload) ? read.payload : {}),
            gateVerdictFresh: true,
          },
        };
      }
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

  // The scratch root is resolvable exactly when `--vault` (or an injected
  // `options.repoRoot`) is present — the same condition every persist call
  // below already needs. Resolve it ONCE, before ANY command-specific
  // branching runs (including a pre-dispatch usage refusal, like
  // `abandon`'s forbidden-flag check, that returns before ever reaching the
  // vault), and route every return through `finish()` — the single choke
  // point every exit from this function now funnels through, mirroring
  // zagent.ps1's `Exit-Zagent` ("every exit in this file goes through
  // here"). A return that bypasses it is exactly the defect this exists to
  // remove: a stale `.zagent/last.exit` left over from a PREVIOUS
  // invocation, indistinguishable from a fresh, current answer.
  const scratchRoot = parsed.vaultRoot ? path.join(path.resolve(options.repoRoot ?? parsed.vaultRoot), SCRATCH_ROOT) : null;
  if (scratchRoot) await markRunning(scratchRoot);

  async function finish(result) {
    if (!scratchRoot) return result;
    try {
      await persistLast(scratchRoot, result, parsed.json);
      return result;
    } catch (error) {
      // A scratch failure must be visible, but must not turn a successfully
      // completed board operation into an ambiguous result, and must not
      // throw out of the one function every exit path relies on to record
      // anything at all.
      const withError = {
        ...result,
        scratchError: error.message ?? String(error),
        payload: {
          ...(result.payload && typeof result.payload === 'object' && !Array.isArray(result.payload) ? result.payload : {}),
          scratchError: error.message ?? String(error),
        },
      };
      try { await persistLast(scratchRoot, withError, parsed.json); } catch { /* no further durable path is available */ }
      return withError;
    }
  }

  if (parsed.commandArgs.includes('--help') || parsed.commandArgs[0] === 'help' || !parsed.commandArgs[0]) {
    return finish({ exitCode: 0, payload: usage(), writes: [] });
  }
  // ★★ `abandon` IS CLIENT-COMPOSED, NOT A BOARD/WRITE COMMAND. tick.md step
  // 9's ownership-lost path used to be three negatives an agent had to hold
  // under pressure: "use comment, but do NOT change status, and do NOT
  // clear the assignee — this is the one release where the assignee must
  // stay put." A rule that only says what NOT to do does not survive an
  // unattended loop, so it is removed structurally: `abandon` has no
  // `--status`, `--assignee` or `--label` flag at all, and it dispatches
  // the existing `comment` mutation rather than inventing a new one.
  if (parsed.command === 'abandon') {
    const forbidden = forbiddenAbandonFlags(parsed.commandArgs);
    if (forbidden.length) {
      return finish({
        exitCode: 1,
        payload: {
          error: `zagent abandon does not accept ${forbidden.map(name => `--${name}`).join(', ')}. `
            + 'Ownership was lost, not resolved: the assignee stays put and the status stays untouched (tick.md step 9). '
            + 'Post the work log with `zagent abandon <KEY> --comment <file>`; use `zagent update` yourself, deliberately, if you actually mean to change any of them.',
        },
        writes: [],
      });
    }
    const key = parsed.commandArgs[1];
    if (!key) return finish({ exitCode: 1, payload: { error: 'Usage: zagent abandon <KEY> --comment <file> [--json]' }, writes: [] });
    // `comment` reads its text from `--file`/`--text` (see COMMAND_FLAGS in
    // legacy-pure/flags.mjs), not `--comment` — remap the one flag abandon's
    // own signature uses so the rest of this file never has to know
    // `abandon` was ever the verb typed.
    const rest = parsed.commandArgs.slice(2).map(token => {
      if (token === '--comment') return '--file';
      if (typeof token === 'string' && token.startsWith('--comment=')) return `--file=${token.slice('--comment='.length)}`;
      return token;
    });
    parsed.commandArgs = ['comment', key, ...rest];
    parsed.command = 'comment';
  }
  if (!parsed.vaultRoot) return finish({ exitCode: 1, payload: { error: 'Missing required --vault ROOT' }, writes: [] });
  const cwd = path.resolve(options.cwd ?? process.cwd());
  const scratch = scratchRoot;
  let result;
  let dispatchedFiles;
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
      dispatchedFiles = files;
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
        repoRoot: path.resolve(options.repoRoot ?? parsed.vaultRoot),
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
    await writeCompatibilityScratch(scratch, parsed.command, result, { ...options, ticketProvider, files: options.files ?? dispatchedFiles });
  } catch (error) {
    // A scratch failure must be visible, but must not turn a successfully
    // completed board operation into an ambiguous result. Preserve the
    // operation exit code and attach a diagnostic for callers; `finish()`
    // still records the real exit code below regardless.
    result = {
      ...result,
      scratchError: error.message ?? String(error),
      payload: { ...(result.payload && typeof result.payload === 'object' ? result.payload : {}), scratchError: error.message ?? String(error) },
    };
  }
  return finish(result);
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
