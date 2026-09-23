/*
 * `zagent integrate <KEY>` — land one lane ticket's `agent/<KEY>` branch onto
 * its project's base branch (plan "Integration mechanics"; WP3 brief).
 *
 * Everything here runs BEFORE the lock, and is read-only: refusals, the
 * UNAPPROVED check, and freshness/gate-verdict classification against the
 * ticket's own `tick_gates.json`. The part that actually moves refs and
 * syncs the main checkout — under `refs/vault/operation-lock`, the same lock
 * board writes use — is `executeIntegrate` in `server/operations.mjs`; this
 * module never touches git for anything but the read-only checks below.
 *
 * `integrate` deliberately does NOT go through `runOperation`/
 * `executeOperation` (write.mjs's board-mutation wrapper): it receives
 * `index.lane` and `index.root` directly from `dispatchWrite`'s own
 * argument, the same object `client.mjs`'s `dynamicDispatcher` set them on
 * after `loadVault` — there is no `prepare(fresh)` callback here, because
 * `integrate` never edits a board file.
 */

import { promises as fs } from 'node:fs';
import path from 'node:path';

import { git, executeIntegrate } from '../server/operations.mjs';
import { canonicalOf, projectPolicy, semanticFlags } from './compat.mjs';
import { agentEmail, actorOf, isAgentOwner, findTicket } from './write.mjs';
import { ticketCommitSha } from './tick-checks.mjs';

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;
const STATUS_IN_PROGRESS = 'In Progress';

// The complete result -> exit code ladder (plan "Integration mechanics" 2.5;
// WP3 brief's "Result ladder" table). `spec/command-matrix.md`'s `### integrate`
// section is the documented copy of this same table — keep both in step.
export const INTEGRATE_EXIT = Object.freeze({
  LANDED: 0,
  REGATE: 20,
  STALE_VERDICT: 21,
  NOT_GREEN: 22,
  CONFLICT: 23,
  TREE_UNSTABLE: 24,
  MAIN_DIRTY: 25,
  MAIN_NOT_ON_BASE: 26,
  UNAPPROVED: 27,
  LOCKED: 5,
  RACED: 5,
});

function refused(message) {
  return { exitCode: 1, payload: { error: message } };
}

function resultPayload(result, extra = {}) {
  const exitCode = INTEGRATE_EXIT[result];
  return { exitCode, payload: { result, ...extra } };
}

async function readTickGatesVerdict(lane, key) {
  const filePath = path.join(lane, '.zagent', 'run', String(key), 'tick_gates.json');
  let raw;
  try {
    raw = await fs.readFile(filePath, 'utf8');
  } catch (error) {
    if (error?.code === 'ENOENT') return { found: false, path: filePath, verdict: null, error: null };
    throw error;
  }
  try {
    return { found: true, path: filePath, verdict: JSON.parse(raw), error: null };
  } catch (error) {
    return { found: true, path: filePath, verdict: null, error: error.message };
  }
}

// Mirrors `Tools/tick_gates.ps1`'s own documented ladder — and
// `tick-checks.mjs`'s (private, unexported) `GATE_EXIT_DOCUMENTED` — by hand:
// this file does not own `tick-checks.mjs` and must not widen its edit scope
// into it just to import a constant. Keep in sync if that ladder ever grows.
const DOCUMENTED_GATE_EXITS = new Set([0, 2, 3, 4, 5, 8, 10, 11, 12]);
// Plan mechanics 2.5's `INTEGRATE_NOT_GREEN_EXITS` — a superset of
// `tick-checks.mjs`'s `GATE_EXIT_RED` ({2,3,4}) that also treats exit 5
// ("could not read the gate list") as a real failure here, not merely
// non-evidential: `integrate` requires a genuine green verify, and "no gate
// list was ever read" is not that.
const INTEGRATE_NOT_GREEN_EXITS = new Set([2, 3, 4, 5]);

/**
 * Freshness = verified tree (plan "Integration mechanics" 1). Classifies the
 * ticket's own `tick_gates.json` against the ladder the brief specifies.
 * Never throws for a missing or malformed verdict — that is `STALE_VERDICT`,
 * not an internal error; a real git failure resolving `tip^{tree}` is folded
 * into the same bucket for the same reason (nothing here is evidence the
 * ticket is unsafe to land, only that this run cannot confirm it is safe).
 */
async function classifyGateVerdict(lane, key, tip) {
  const record = await readTickGatesVerdict(lane, key);
  if (!record.found) {
    return { result: 'STALE_VERDICT', reason: `no tick_gates.json at ${record.path}` };
  }
  if (record.error || !record.verdict || typeof record.verdict !== 'object') {
    return { result: 'STALE_VERDICT', reason: `tick_gates.json at ${record.path} did not parse${record.error ? ` (${record.error})` : ''}` };
  }
  const verdict = record.verdict;
  if (verdict.phase !== 'verify' && verdict.phase !== 'all') {
    return { result: 'STALE_VERDICT', reason: `verdict phase '${verdict.phase}' is not verify/all — a measure-phase run is not a verification` };
  }
  if (verdict.state !== 'complete') {
    return { result: 'STALE_VERDICT', reason: `verdict state is '${verdict.state}', not 'complete'` };
  }
  const code = Number(verdict.exitCode);
  if (verdict.exitCode === null || verdict.exitCode === undefined || Number.isNaN(code) || !DOCUMENTED_GATE_EXITS.has(code)) {
    return { result: 'STALE_VERDICT', reason: `verdict exitCode ${verdict.exitCode} is not one of tick_gates.ps1's documented codes` };
  }
  // Exit 12 ("tree unstable") is checked BEFORE the NOT_GREEN set even though
  // it is a red gate result too: tick_gates.ps1 already retried verify once
  // internally by the time it writes 12, so this is a distinct terminal
  // (carry_forward.md) rather than another instance of "a gate line failed".
  if (code === 12) {
    return { result: 'TREE_UNSTABLE', reason: 'verdict complete with exit 12 (tree unstable) — tick_gates.ps1 already retried once internally' };
  }
  if (INTEGRATE_NOT_GREEN_EXITS.has(code)) {
    return { result: 'NOT_GREEN', reason: `verdict complete with exit ${code}` };
  }
  if (code === 8 || code === 10 || code === 11) {
    return { result: 'STALE_VERDICT', reason: `verdict complete with non-evidential exit ${code}` };
  }
  // code === 0: green, subject to the tree check below.
  let tipTree;
  try { tipTree = await git(lane, ['rev-parse', `${tip}^{tree}`]); } catch { tipTree = null; }
  if (!tipTree || verdict.treeShaEnd !== tipTree) {
    return { result: 'STALE_VERDICT', reason: `tip^{tree} (${tipTree ?? '(unresolvable)'}) does not match verdict.treeShaEnd (${verdict.treeShaEnd ?? '(none)'})` };
  }
  return { result: 'GREEN' };
}

/** `zagent integrate <KEY>` — the CLI entry point `write.mjs` dispatches to. */
export async function integrate(index, argv = [], files = {}) {
  const key = String(argv[1] ?? '');
  if (!ISSUE_KEY.test(key)) return refused('Usage: zagent integrate <TICKET_KEY>');

  // "not lane mode" also catches an UNREGISTERED linked worktree, on
  // purpose: `index.lane` is only ever set in `client.mjs`'s
  // `dynamicDispatcher` when `laneInfo.mode === 'lane'`, so an unregistered
  // worktree (whose mode is `'unregistered'`) lands here with exactly the
  // same refusal — no edit to `lanes.mjs`'s own `UNREGISTERED_REFUSED_COMMANDS`
  // is needed or wanted for a command that module does not own.
  if (!index?.lane?.project || !index?.lane?.repoRoot) {
    return refused('zagent integrate only runs in a registered lane (index.lane is unset) — the main checkout and an unregistered linked worktree both land here.');
  }
  const lane = index.lane.repoRoot;
  const project = String(index.lane.project).toUpperCase();
  const authority = index.root ?? index.vaultRoot;

  const record = findTicket(index, key);
  if (!record) return refused(`No ticket ${key}.`);
  const metadata = canonicalOf(record);
  if (String(metadata.status ?? '') !== STATUS_IN_PROGRESS) {
    return refused(`${key} is ${metadata.status ?? '(no status)'}, not In Progress; nothing to integrate.`);
  }
  const actor = actorOf(index, argv);
  if (!isAgentOwner(index, record, actor)) {
    return refused(`${key} is not agent-owned by ${agentEmail(index, argv)}; refusing to integrate.`);
  }
  const rowLane = String(metadata.lane ?? '').toUpperCase();
  if (rowLane !== project) {
    return refused(`${key}'s lane (${rowLane || '(none)'}) does not match this invocation's lane (${project}); refusing to integrate.`);
  }

  const flags = semanticFlags(record);
  if (flags.humanGate && !flags.approved) {
    return resultPayload('UNAPPROVED', { key, reason: `${key} carries human-gate without approved — park it at In Review for human sign-off.` });
  }

  const baseBranch = projectPolicy(index, project, metadata.category ?? null).project.baseBranch ?? 'master';

  // tip captured ONCE, here — threaded through as a literal SHA to every
  // later step (including inside the lock), never re-read from HEAD again.
  let tip;
  try { tip = await git(lane, ['rev-parse', 'HEAD']); }
  catch (error) { return refused(`Cannot resolve HEAD in the lane at ${lane}: ${error.message}`); }

  let laneBranch;
  try { laneBranch = await git(lane, ['symbolic-ref', 'HEAD']); }
  catch { laneBranch = null; }
  if (laneBranch !== `refs/heads/agent/${key}`) {
    return refused(`the lane at ${lane} is not on refs/heads/agent/${key} (found ${laneBranch ?? '(detached)'}); refusing to integrate.`);
  }

  // NOT_GREEN, first cut: the ticket's own first-parent commit, before ever
  // reading tick_gates.json.
  const sha = await ticketCommitSha(lane, key);
  if (!sha) {
    return resultPayload('NOT_GREEN', { key, reason: `no \`${key}:\` commit exists on refs/heads/agent/${key} — nothing to land` });
  }
  let subject;
  try { subject = await git(lane, ['log', '-1', '--format=%s', sha]); }
  catch (error) { return refused(`Cannot read commit ${sha}'s subject: ${error.message}`); }
  if (subject.startsWith(`WIP(${key}):`)) {
    return resultPayload('NOT_GREEN', { key, reason: `the ticket's first-parent commit is a WIP park ("${subject}") — the ticket is not green` });
  }

  let laneStatus;
  try { laneStatus = await git(lane, ['status', '--porcelain']); }
  catch (error) { return refused(`Cannot read lane status at ${lane}: ${error.message}`); }
  if (laneStatus.trim() !== '') {
    return resultPayload('STALE_VERDICT', { key, reason: `the lane tree at ${lane} is dirty; re-verify before integrating` });
  }

  const verdict = await classifyGateVerdict(lane, key, tip);
  if (verdict.result !== 'GREEN') {
    return resultPayload(verdict.result, { key, reason: verdict.reason });
  }

  const outcome = await executeIntegrate(authority, {
    key, project, baseBranch, tip, title: metadata.title ?? key,
  });
  return resultPayload(outcome.result, { key, ...outcome.extra });
}

export default integrate;
