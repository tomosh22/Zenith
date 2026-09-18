/*
 * Pure(ish) decision logic for rules `.claude/commands/tick.md` used to
 * enforce only by asking the orchestrator to remember them.
 *
 * `.claude/commands/tick.md:1274` states the principle this module exists to
 * serve: "a rule enforced at a check that already fails is a rule, and the
 * same rule in this paragraph is a hope." `Tools/zagent/TickChecks.psm1`
 * carries the equivalent checks for the legacy/remote backend — read as a
 * specification of intent, not ported line for line, because the two
 * backends disagree about almost everything except the shape of the answer.
 *
 * Every function here answers a question about `.zagent/run/<KEY>/` and the
 * git working tree; it never refuses, commits, or writes anything itself.
 * Wiring a refusal on top of an answer is a job for the call site in
 * write.mjs or client.mjs that already writes the mutation in question.
 *
 * Fail-closed convention, matched throughout: a missing or unparsable input
 * is its own distinguishable state, never silently folded into "nothing to
 * report" — that shape is indistinguishable from "checked and clean", which
 * is exactly the failure mode every check below exists to remove.
 */

import { execFile as execFileCallback } from 'node:child_process';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { promisify } from 'node:util';

const execFile = promisify(execFileCallback);

// ─── shared scratch helpers ────────────────────────────────────────────

/**
 * `filelist.txt` and `changed.txt` are the same format: one repo-relative,
 * forward-slashed path per line. Returns `null` (never an empty array) when
 * the file does not exist at all — "no file" and "an empty file" are
 * different facts a caller has to be able to tell apart.
 */
async function readPathListFile(filePath) {
  let raw;
  try {
    raw = await fs.readFile(filePath, 'utf8');
  } catch (error) {
    if (error?.code === 'ENOENT') return null;
    throw error;
  }
  const seen = new Set();
  const list = [];
  for (const line of raw.split(/\r?\n/)) {
    const value = line.trim();
    if (!value || seen.has(value)) continue;
    seen.add(value);
    list.push(value);
  }
  return list;
}

/**
 * `tick_gates.json`, read once and shared by `blockedScopeAllowed` and
 * `gateVerdictFresh` — both have to answer "missing" and "did not parse"
 * before they can answer their own question, and a check duplicated in two
 * functions is a check that drifts the day only one of them gets fixed.
 */
async function readTickGatesVerdict(repoRoot, key) {
  const filePath = path.join(repoRoot, '.zagent', 'run', String(key), 'tick_gates.json');
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

function optional(verdict, name, fallback = null) {
  return verdict && Object.prototype.hasOwnProperty.call(verdict, name) ? verdict[name] : fallback;
}

// Exit codes `Tools/tick_gates.ps1` can leave in `tick_gates.json`, mirrored
// from its own header rather than re-derived so the two cannot drift apart:
//   0 green - 2 regen failed - 3 a created file never reached the compiler
//   4 a gate line failed - 5 no gate list - 10 MEASURED, not a failure.
const GATE_EXIT_RED = new Set([2, 3, 4]);
const GATE_EXIT_MEASURED = 10;

// ─── 1. Definition-of-Done ─────────────────────────────────────────────

/**
 * The unticked Definition-of-Done boxes in a work log about to be filed as
 * Done. Mirrors `Get-UnmetDoneCriteria` in `Tools/zagent/ZagentClient.psm1`:
 * scanning only the `## Definition of Done` section matters, because a work
 * log's "Deviations" prose legitimately contains unticked checklists of its
 * own, and refusing on those would make this a check people route around.
 *
 * Blocked and In Review are the caller's business, not this function's: an
 * unmet DoD is the normal state for both, so the call site only invokes this
 * when the requested status is Done.
 */
export function unmetDoneCriteria(worklog) {
  const text = String(worklog ?? '');
  if (!text) return [];
  const unmet = [];
  let inSection = false;
  for (const line of text.split(/\r?\n/)) {
    if (/^\s*(?:#{1,6}\s*|\*\*)\s*Definition of Done/i.test(line)) {
      inSection = true;
      continue;
    }
    if (!inSection) continue;
    // The section ends at the next heading or bold run-in label.
    if (/^\s*#{1,6}\s/.test(line) || (/^\s*\*\*/.test(line) && !/^\s*[-*+]\s*\[/.test(line))) {
      inSection = false;
      continue;
    }
    const match = /^\s*[-*+]\s*\[\s\]\s*(.*)$/.exec(line);
    if (match) unmet.push(match[1].trim() || '(unlabelled item)');
  }
  return unmet;
}

// ─── 2. File-list excursions ───────────────────────────────────────────

/**
 * Which changed files were not on the worker's file list — and, distinctly,
 * whether there was a list to check against at all.
 *
 * Does NOT refuse anything: tick.md's rule is "every excursion must be
 * explained", not "no excursions allowed". `excursions` is `null` (never an
 * empty array) when `listFound` is false, so a caller cannot mistake
 * "unverified" for "verified clean".
 */
export async function fileListExcursion({ repoRoot, key, changed }) {
  const listPath = path.join(repoRoot, '.zagent', 'run', String(key), 'filelist.txt');
  const listed = await readPathListFile(listPath);
  const changedFiles = Array.isArray(changed) ? changed : [];
  if (listed === null) {
    return {
      key, listFound: false, listPath, listed: [], changed: changedFiles, excursions: null, missing: [],
      reason: `no filelist.txt at ${listPath} — there is nothing to compare the change set against, so excursions are UNVERIFIED, not clean`,
    };
  }
  const listedSet = new Set(listed);
  const changedSet = new Set(changedFiles);
  return {
    key, listFound: true, listPath, listed, changed: changedFiles,
    excursions: changedFiles.filter(value => !listedSet.has(value)),
    missing: listed.filter(value => !changedSet.has(value)),
    reason: null,
  };
}

/**
 * Which excursion paths are not named anywhere in the finish comment/work-log
 * text about to land on the ticket. Deliberately format-free: a plain
 * case-insensitive substring search over whatever text is about to be
 * written, not a template a worker has to reproduce exactly. Always returns
 * an array (never `null`) so a caller can `.length` it unconditionally.
 */
export function unexplainedExcursions(excursions, explanationText) {
  const list = Array.isArray(excursions) ? excursions : [];
  if (list.length === 0) return [];
  const text = String(explanationText ?? '').toLowerCase();
  return list.filter(value => !text.includes(String(value).toLowerCase()));
}

// ─── 3. Commit completeness ────────────────────────────────────────────

/**
 * Checks that every file recorded as changed (`.zagent/run/<KEY>/changed.txt`,
 * written by the `gates` step) actually landed in the commit — the
 * `commit -am` failure mode tick.md:1496 names by name: `-a` stages
 * modifications to tracked files and leaves a worker-CREATED file behind,
 * untracked, so the gates that certified the ticket green ran against a tree
 * where the file existed and the break surfaces on somebody else's next
 * clone.
 *
 * `missingFromCommit` is `null` (never an empty array) when there was
 * nothing recorded to compare against — that is "unverified", not "clean",
 * and a caller must not read the two the same way.
 */
export async function commitCompleteness({ repoRoot, key, sha = 'HEAD' }) {
  const changedPath = path.join(repoRoot, '.zagent', 'run', String(key), 'changed.txt');
  const recorded = await readPathListFile(changedPath);
  if (recorded === null || recorded.length === 0) {
    return {
      key, ok: false, recordedFound: recorded !== null, recorded: [], committed: [],
      missingFromCommit: null, extraInCommit: null,
      reason: recorded === null
        ? `no changed.txt at ${changedPath} — the gates step has not run (or ownership was lost before it wrote), so commit completeness is UNVERIFIED`
        : `changed.txt at ${changedPath} is empty — nothing was recorded as changed, so there is nothing to check the commit against`,
    };
  }
  let committed;
  try {
    const { stdout } = await execFile('git', ['-C', repoRoot, 'diff-tree', '--no-commit-id', '--name-only', '-r', sha], { encoding: 'utf8' });
    committed = stdout.split(/\r?\n/).map(value => value.trim()).filter(Boolean);
  } catch (error) {
    return {
      key, ok: false, recordedFound: true, recorded, committed: [], missingFromCommit: null, extraInCommit: null,
      reason: `'git diff-tree' against '${sha}' failed — ${error.message}`,
    };
  }
  const committedSet = new Set(committed);
  const recordedSet = new Set(recorded);
  const missing = recorded.filter(value => !committedSet.has(value));
  return {
    key, ok: missing.length === 0, recordedFound: true, recorded, committed,
    missingFromCommit: missing,
    extraInCommit: committed.filter(value => !recordedSet.has(value)),
    reason: missing.length
      ? `commit '${sha}' is missing ${missing.length} file(s) changed.txt recorded — the 'commit -am' failure mode: a CREATED file silently left behind`
      : null,
  };
}

// ─── 4. Durable findings ───────────────────────────────────────────────

const DECISION_REFERENCE = /\b[A-Za-z][A-Za-z0-9]*-D-\d+\b/g;
const DURABLE_FINDINGS_LABEL = /^\s*(?:#{1,6}\s*|\*{0,2})\s*durable findings\s*\*{0,2}\s*:?\s*(.*)$/i;

/**
 * Whether a work log carries an explicit durable-findings answer: a line
 * naming either "none" or one or more decision references (the `PROJ-D-N`
 * shape `zagent decide` entries carry — see any game's `Docs/DecisionLog.md`
 * ruling).
 *
 * tick.md's step 8 ("A FINDING THAT OUTLIVES THE TICKET IS A DECISION") asks
 * every tick to consider whether it learned something true of the codebase
 * rather than of the ticket, and until now nothing checked that it actually
 * did. Nobody can verify the judgement — "was there really nothing durable
 * here" is not computable — so, like the work log's own Coverage row, the
 * mechanism is to make SILENCE illegal rather than to grade the answer:
 * "none" is a legal answer, an absent line is not.
 *
 * Required shape (documented in the refusal, not just here): a line reading
 * `**Durable findings**: none` or `**Durable findings**: <PROJ-D-N[, ...]>`
 * anywhere in the work log — a heading (`## Durable findings`) works too,
 * with the answer on the heading line or the next non-blank line.
 */
export function durableFindingsAnswer(worklog) {
  const lines = String(worklog ?? '').split(/\r?\n/);
  let labelIndex = -1;
  let inline = '';
  for (let index = 0; index < lines.length; index += 1) {
    const match = DURABLE_FINDINGS_LABEL.exec(lines[index]);
    if (match) {
      labelIndex = index;
      inline = match[1] ?? '';
      break;
    }
  }
  if (labelIndex === -1) {
    return {
      present: false, answer: null, references: [],
      reason: 'the work log carries no "Durable findings" line — add `**Durable findings**: none` or name the decision reference(s) recorded via `zagent decide` (e.g. ZM-D-208)',
    };
  }
  let content = inline.trim();
  if (!content) {
    const collected = [];
    for (let index = labelIndex + 1; index < lines.length; index += 1) {
      const line = lines[index];
      if (/^\s*#{1,6}\s/.test(line)) break;
      if (/^\s*\*\*[^*]+\*\*\s*:?/.test(line) && collected.length) break;
      if (line.trim() === '') { if (collected.length) break; else continue; }
      collected.push(line.trim());
    }
    content = collected.join(' ').trim();
  }
  if (!content) {
    return {
      present: true, answer: null, references: [],
      reason: 'the "Durable findings" line has no content — say "none" or name the decision reference(s)',
    };
  }
  const references = [...content.matchAll(DECISION_REFERENCE)].map(match => match[0]);
  const isNone = /^none\.?$/i.test(content.trim());
  if (isNone || references.length) {
    return { present: true, answer: isNone ? 'none' : 'references', references, reason: null };
  }
  return {
    present: true, answer: null, references: [],
    reason: `the "Durable findings" line ("${content}") names neither "none" nor a decision reference like ZM-D-208 — say one or the other`,
  };
}

// ─── 5. blocked-scope defensibility ────────────────────────────────────

/**
 * Answers whether `blocked-scope` is defensible for this ticket right now,
 * per tick.md:1457's rule that the label means "not buildable as filed", not
 * "a run failed". A RED verdict (exit 2/3/4 — a gate actually ran and
 * actually failed) means this is a FAILURE block, not a scope one, whatever
 * else is true of the ticket. Exit 10 (a pin needs bumping) is explicitly
 * carved out as not a failure; green, absent, or still-running all permit
 * the label. An unparsable file fails the OTHER way — "cannot tell" must
 * never read as permission any more than as refusal.
 */
export async function blockedScopeAllowed({ repoRoot, key }) {
  const record = await readTickGatesVerdict(repoRoot, key);
  if (!record.found) {
    return {
      key, allowed: true, state: null, exitCode: null,
      reason: `no tick_gates.json at ${record.path} — no verdict on record to contradict a scope block`,
    };
  }
  if (record.error) {
    return {
      key, allowed: false, state: null, exitCode: null,
      reason: `tick_gates.json at ${record.path} did not parse (${record.error}) — cannot confirm the last run was not a gate failure`,
    };
  }
  const verdict = record.verdict ?? {};
  const state = optional(verdict, 'state');
  const exitCode = optional(verdict, 'exitCode');
  if (state === 'running') {
    return {
      key, allowed: true, state, exitCode,
      reason: 'the recorded run never finished (state: running) — a killed run leaves no verdict, so nothing contradicts a scope block',
    };
  }
  if (exitCode !== null && exitCode !== undefined && GATE_EXIT_RED.has(Number(exitCode))) {
    return {
      key, allowed: false, state, exitCode,
      reason: `the last gate run exited ${exitCode} — a gate actually failed, so this is a FAILURE block, not a scope block`,
    };
  }
  const reason = exitCode === 0
    ? 'the last gate run was green'
    : exitCode === GATE_EXIT_MEASURED
      ? 'the last verdict was exit 10 (a pin needs bumping) — explicitly not a failure'
      : `the last gate run's exit code (${exitCode}) does not indicate a failed gate line`;
  return { key, allowed: true, state, exitCode, reason };
}

// ─── 6. needs-gpu companion label ──────────────────────────────────────

// The fixed vocabulary a `needs-gpu` write must carry exactly one of. Since
// ZEN-6 removed the headless scene-authoring guard, "it re-authors a
// committed scene" stopped being a reason on its own —
// `gpu-reason:legacy-scene-reauthor` names that history rather than
// pretending it still applies.
export const GPU_REASON_LABELS = Object.freeze([
  'gpu-reason:render-capture',
  'gpu-reason:visual-measure',
  'gpu-reason:requires-graphics-test',
  'gpu-reason:legacy-scene-reauthor',
]);
const GPU_REASON_SET = new Set(GPU_REASON_LABELS);

/**
 * Whether a set of labels being APPLIED by one write (not a ticket's full,
 * already-saved label list) is defensible with respect to `needs-gpu`:
 * applying it demands exactly one companion `gpu-reason:*` label in the SAME
 * write. Deliberately not retroactive — a ticket that already carries
 * `needs-gpu` from before this rule existed is untouched until somebody
 * writes to its labels again.
 */
export function needsGpuCompanion(labelsApplied) {
  const labels = (Array.isArray(labelsApplied) ? labelsApplied : []).map(String);
  if (!labels.some(value => value.toLowerCase() === 'needs-gpu')) return { ok: true, reason: null };
  const companions = labels.filter(value => GPU_REASON_SET.has(value.toLowerCase()));
  if (companions.length === 1) return { ok: true, reason: null };
  const vocabulary = GPU_REASON_LABELS.join(', ');
  if (companions.length === 0) {
    return { ok: false, reason: `--label needs-gpu requires exactly one companion label naming why: ${vocabulary}` };
  }
  return {
    ok: false,
    reason: `--label needs-gpu requires exactly one companion label, not ${companions.length} (${companions.join(', ')}) — pick the one that actually applies: ${vocabulary}`,
  };
}

// ─── 7. Gate-verdict freshness ─────────────────────────────────────────

/**
 * Whether `tick_gates.json` describes a completed run against the CURRENT
 * HEAD. Not fresh when: the file is absent; it fails to parse; `state` is
 * not `complete` (a killed run leaves `running`, which is no verdict at
 * all); or `headSha` does not equal `git rev-parse HEAD` right now (the
 * verdict describes a different tree — treat it as absent). `skipped` is
 * returned as data, not judged here — that is a separate rule ("a red gate
 * voids every gate behind it") for the caller to apply once freshness is
 * settled.
 */
export async function gateVerdictFresh({ repoRoot, key }) {
  const record = await readTickGatesVerdict(repoRoot, key);
  if (!record.found) {
    return {
      key, fresh: false, state: null, exitCode: null, headSha: null, skipped: [],
      reason: `no tick_gates.json at ${record.path} — an absent verdict is treated the same as a stale one`,
    };
  }
  if (record.error) {
    return {
      key, fresh: false, state: null, exitCode: null, headSha: null, skipped: [],
      reason: `tick_gates.json at ${record.path} did not parse (${record.error}) — treat as absent`,
    };
  }
  const verdict = record.verdict ?? {};
  const state = optional(verdict, 'state');
  const exitCode = optional(verdict, 'exitCode');
  const headSha = optional(verdict, 'headSha');
  const skipped = Array.isArray(optional(verdict, 'skipped', [])) ? optional(verdict, 'skipped', []) : [];
  if (state !== 'complete') {
    return {
      key, fresh: false, state, exitCode, headSha, skipped,
      reason: `state is '${state}', not 'complete' — a killed run leaves the file mid-write, and that is not a verdict to trust`,
    };
  }
  let currentHead = null;
  try {
    const { stdout } = await execFile('git', ['-C', repoRoot, 'rev-parse', 'HEAD'], { encoding: 'utf8' });
    currentHead = stdout.trim();
  } catch {
    currentHead = null;
  }
  if (!currentHead) {
    return {
      key, fresh: false, state, exitCode, headSha, skipped,
      reason: `could not resolve current HEAD in ${repoRoot} — cannot confirm the verdict describes this tree`,
    };
  }
  if (!headSha || headSha !== currentHead) {
    return {
      key, fresh: false, state, exitCode, headSha, skipped,
      reason: `verdict headSha (${headSha ?? '(none)'}) does not match current HEAD (${currentHead}) — the verdict describes a different tree; treat as absent`,
    };
  }
  return { key, fresh: true, state, exitCode, headSha, skipped, reason: null };
}

/**
 * Should a `guard` verdict that otherwise passed be overridden into a
 * failure because the local gate verdict it never saw is not fresh? Only
 * ever turns a would-be PASS into a failure — never the reverse, and never
 * touches a result that was already non-zero: a guard refusal already
 * carries its own specific reason, and staleness has nothing to add to a
 * refusal that already fired.
 */
export function guardNeedsFreshnessOverride(guardExitCode, fresh) {
  return guardExitCode === 0 && !fresh;
}

// ─── 8. abandon's forbidden flags ──────────────────────────────────────

/**
 * Which of `--status`, `--assignee`, `--label` were passed to
 * `zagent abandon`. `abandon` is client-composed and must have NO way to
 * change status or assignee — tick.md step 9's ownership-lost path used to
 * be three negatives an agent had to hold under pressure ("use comment, but
 * do NOT change status, and do NOT clear the assignee"). This names every
 * forbidden flag actually present in one pass, matching both `--name value`
 * and `--name=value` spellings, so the refusal can list all of them rather
 * than stopping at the first.
 */
export function forbiddenAbandonFlags(argv) {
  const args = Array.isArray(argv) ? argv : [];
  const present = [];
  for (const name of ['status', 'assignee', 'label']) {
    const hit = args.some(token => token === `--${name}` || (typeof token === 'string' && token.startsWith(`--${name}=`)));
    if (hit) present.push(name);
  }
  return present;
}
