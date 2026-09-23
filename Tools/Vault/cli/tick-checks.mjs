/*
 * Pure(ish) decision logic for rules `Tools/zagent/protocol/TICK.md` used to
 * enforce only by asking the orchestrator to remember them.
 *
 * `Tools/zagent/protocol/TICK.md#6-verify` states the principle this module exists to
 * serve: "a rule enforced at a check that already fails is a rule, and the
 * same rule in this paragraph is a hope." This is the sole implementation of
 * these checks — there is no second copy anywhere else in the repo.
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
// A measure phase that reports 'complete' having run ZERO gate lines (all
// deferred, none executed) exits 8 — documented, but the one code in the
// ladder that carries no evidence at all: nothing was verified, green or
// red. Chosen as 8, not 6, because 6 is already `zagent`'s own circuit-
// breaker-open code and a tick reads both ladders.
const GATE_EXIT_NO_GATE_LINES = 8;
// Two more non-evidential codes (WP2 item 7, parallel `/tick` lanes):
//   11 - the build slot (Tools/build_lock.ps1) could not be acquired within
//        lanes.buildSlotWaitMinutes — "slot busy". The phase never ran.
//   12 - a second start != end tree-SHA mismatch inside one firing —
//        "tree unstable". `tick_gates.ps1` retries once internally already;
//        this is what it leaves behind when the tree still would not hold
//        still the second time.
// Neither carries any evidence about whether the tree builds or passes its
// gates — the run never got far enough to answer that — so both sit beside 8
// in GATE_EXIT_NON_EVIDENTIAL, never in GATE_EXIT_RED or the blocked-scope
// RED set.
const GATE_EXIT_SLOT_BUSY = 11;
const GATE_EXIT_TREE_UNSTABLE = 12;
const GATE_EXIT_NON_EVIDENTIAL = new Set([GATE_EXIT_NO_GATE_LINES, GATE_EXIT_SLOT_BUSY, GATE_EXIT_TREE_UNSTABLE]);
// The full documented ladder `Tools/tick_gates.ps1`'s own header promises to
// leave in `exitCode`: 0, the three RED codes above, 5 ("could not read the
// gate list"), 8/11/12 (all non-evidential, above), and 10. "Documented" is
// not the same as "fresh evidence" — 2/3/4 and 5 are documented FAILURES and
// still fresh, current answers about the tree (a gate genuinely failed); 8,
// 11 and 12 are documented but VACUOUS (nothing ran, or the tree never held
// still long enough to be checked, so there is nothing to be fresh or stale
// about); an exit code that is not in this set at all is neither — it is
// what an internal crash outside the ladder's own contract looks like on the
// wire. Measured on ZEN-101: a phase crashed in a post-build assertion and
// the top-level process left exitCode 1 — not a value this ladder describes
// — while every per-step result read exitCode 0 and `state` was left
// 'complete', so nothing about the SHAPE of the file said anything had gone
// wrong.
const GATE_EXIT_DOCUMENTED = new Set([
  0, 2, 3, 4, 5, GATE_EXIT_NO_GATE_LINES, GATE_EXIT_MEASURED, GATE_EXIT_SLOT_BUSY, GATE_EXIT_TREE_UNSTABLE,
]);

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
 * `commit -am` failure mode Tools/zagent/protocol/TICK.md#6-verify names by name: `-a` stages
 * modifications to tracked files and leaves a worker-CREATED file behind,
 * untracked, so the gates that certified the ticket green ran against a tree
 * where the file existed and the break surfaces on somebody else's next
 * clone.
 *
 * `missingFromCommit` is `null` (never an empty array) when there was
 * nothing recorded to compare against — that is "unverified", not "clean",
 * and a caller must not read the two the same way.
 *
 * ★ THE COMMIT CHECKED IS THE TICKET'S OWN, NOT `HEAD`, AND THAT DISTINCTION
 * IS LOAD-BEARING. This check read `HEAD` unconditionally, which is only the
 * ticket's commit when nothing has committed since — and on the Vault backend
 * the board commits to THIS SAME repository, so an ordinary `zagent decide`
 * between the ticket's commit and its `finish` moves `HEAD` onto a
 * `board: decide` commit touching only `metadata.md`. Every one of the
 * ticket's files then reads as missing, and the refusal blames the
 * `commit -am` failure mode — naming files that are, in fact, committed.
 *
 * That is not a corner case: tick.md step 8 REQUIRES a `Durable findings`
 * answer naming either "none" or a `PROJ-D-N` reference, and the only way to
 * get such a reference is `zagent decide`, which must run BEFORE the work log
 * citing it is finished. So the two mandatory halves of step 8 refused each
 * other, and a fully green ticket could not be closed. Measured on ZEN-7.
 *
 * The ticket's commit is found by the subject convention tick.md step 7
 * mandates — `<KEY>: <title>`, or `WIP(<KEY>): <title> — GATES RED` for the
 * red-park path — taking the most recent match. When no such commit exists
 * (a hand-run tick, a different message) it falls back to `HEAD`, which is
 * the previous behaviour; `resolvedFrom` says which happened, so "checked the
 * ticket's commit" and "checked whatever HEAD was" stay distinguishable, per
 * this module's fail-closed convention.
 */
// Exported for `write.mjs`'s lane Done<=>landed check (WP2 item 3): a lane
// row going Done must resolve its OWN `<KEY>:` commit directly — never
// through `commitCompleteness`, which returns early with no `sha` at all
// when `changed.txt` is absent, and never via a HEAD fallback, because a
// branch that never carried a ticket commit is trivially an ancestor of
// `baseBranch`. `--first-parent` (WP2 item 6) is required here: after a
// lane-side `merge master` (REGATE), the plain log interleaves master's own
// commits — 254 of the last 300 on a real run were `board: ` — so a ticket
// that waited even a few hours can lose its `<KEY>:` commit past the `-n
// 200` window. `--first-parent` walks only this branch's own commits, never
// descending into a merged-in master.
export async function ticketCommitSha(repoRoot, key) {
  try {
    const { stdout } = await execFile(
      'git', ['-C', repoRoot, 'log', '--first-parent', '--format=%H%x1f%s', '-n', '200'], { encoding: 'utf8' },
    );
    const prefixes = [`${key}:`, `WIP(${key}):`];
    for (const line of stdout.split(/\r?\n/)) {
      const sep = line.indexOf('\x1f');
      if (sep < 0) continue;
      const subject = line.slice(sep + 1).trimStart();
      if (prefixes.some(prefix => subject.startsWith(prefix))) return line.slice(0, sep);
    }
  } catch {
    // An unreadable log is not this check's failure to report; fall back to
    // HEAD and let the diff-tree call below produce the real error.
  }
  return null;
}

export async function commitCompleteness({ repoRoot, key, sha = null }) {
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
  const resolved = sha ?? await ticketCommitSha(repoRoot, key);
  const resolvedFrom = sha ? 'caller' : (resolved ? 'ticket-commit' : 'head-fallback');
  const target = resolved ?? 'HEAD';

  let committed;
  try {
    const { stdout } = await execFile('git', ['-C', repoRoot, 'diff-tree', '--no-commit-id', '--name-only', '-r', target], { encoding: 'utf8' });
    committed = stdout.split(/\r?\n/).map(value => value.trim()).filter(Boolean);
  } catch (error) {
    return {
      key, ok: false, recordedFound: true, recorded, committed: [], missingFromCommit: null, extraInCommit: null,
      sha: target, resolvedFrom,
      reason: `'git diff-tree' against '${target}' failed — ${error.message}`,
    };
  }
  const committedSet = new Set(committed);
  const recordedSet = new Set(recorded);
  const missing = recorded.filter(value => !committedSet.has(value));
  return {
    key, ok: missing.length === 0, recordedFound: true, recorded, committed,
    missingFromCommit: missing,
    extraInCommit: committed.filter(value => !recordedSet.has(value)),
    sha: target, resolvedFrom,
    reason: missing.length
      ? `commit '${target}'${resolvedFrom === 'head-fallback' ? " (HEAD — no commit subject names this ticket, so this may not be the ticket's commit)" : ''}`
        + ` is missing ${missing.length} file(s) changed.txt recorded — the 'commit -am' failure mode: a CREATED file silently left behind`
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
  // A collected line commonly carries a leading list marker -- tick.md's own
  // step-8 template shows "- none, or: `<PROJ-D-N>`..." as a bullet under the
  // "**Durable findings**" label, which without this strip reads as "- none"
  // and refuses a work log that followed the documented template exactly.
  const withoutListMarker = content.replace(/^\s*(?:[-*+]|\d+[.)])\s+/, '').trim();
  const isNone = /^none\.?$/i.test(withoutListMarker);
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
 * per Tools/zagent/protocol/TICK.md#6-verify's rule that the label means "not buildable as filed", not
 * "a run failed". A RED verdict (exit 2/3/4 — a gate actually ran and
 * actually failed) means this is a FAILURE block, not a scope one, whatever
 * else is true of the ticket. Exit 10 (a pin needs bumping) is explicitly
 * carved out as not a failure; green, absent, or still-running all permit
 * the label. An unparsable file fails the OTHER way — "cannot tell" must
 * never read as permission any more than as refusal.
 *
 * A RED verdict is not automatically a refusal any more: `Tools/Vault`'s own
 * Vitest suite has at least two intermittent failures on Windows, so a red
 * exit can be a flake rather than something the ticket caused. The overall
 * verdict deliberately stays RED even when every failing line recovered on
 * an immediate re-run of the identical command on the identical tree
 * (measured on ZEN-99) — `tick_gates.json`'s `flakes` array (produced by
 * `Tools/tick_gates.ps1`, `[{line, firstExit, rerunExit}, ...]`) is the
 * evidence that happened. `blocked-scope` is allowed on a RED verdict only
 * when every entry in `flakes` recovered (`rerunExit === 0`) — never when
 * `flakes` is absent (an older verdict — "no flake evidence", the prior
 * behaviour), empty (nothing was ever shown to be a flake, so the red stands
 * unexplained), or any entry is still red after its own re-run.
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
    const flakesRaw = optional(verdict, 'flakes', undefined);
    if (!Array.isArray(flakesRaw)) {
      return {
        key, allowed: false, state, exitCode,
        reason: `the last gate run exited ${exitCode} and tick_gates.json carries no 'flakes' array (an older verdict — no flake evidence) — a gate actually failed, so this is a FAILURE block, not a scope block`,
      };
    }
    if (flakesRaw.length === 0) {
      return {
        key, allowed: false, state, exitCode,
        reason: `the last gate run exited ${exitCode} and 'flakes' is empty — no line is recorded as having recovered on a re-run, so this is a FAILURE block, not a scope block`,
      };
    }
    const survived = flakesRaw.filter(entry => Number(entry?.rerunExit) !== 0);
    if (survived.length > 0) {
      const lines = survived.map(entry => String(entry?.line ?? '(unnamed line)')).join(', ');
      return {
        key, allowed: false, state, exitCode,
        reason: `the last gate run exited ${exitCode} and still red after its own re-run: ${lines} — a gate actually failed, so this is a FAILURE block, not a scope block`,
      };
    }
    return {
      key, allowed: true, state, exitCode,
      reason: `the last gate run exited ${exitCode}, but every line in 'flakes' recovered on an immediate re-run of the same tree (rerunExit 0) — a flake, not a failure this ticket caused`,
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
  // `state === 'complete'` only says the file was written all the way
  // through; it says nothing about whether the exit code it carries is one
  // `Tools/tick_gates.ps1`'s own ladder actually describes, or — even if it
  // is documented — whether that code carries any evidence at all. Three-way,
  // not two:
  //   documented-and-evidential (0/2/3/4/5/10): a real, current answer about
  //     this tree, red or green — fresh (subject to the headSha check below).
  //   documented-but-vacuous (8): the phase reported 'complete' having run NO
  //     gate lines — nothing was verified, so there is nothing to be fresh
  //     about. Measured on ZEN-99: 12 gate lines deferred, verdict still
  //     green. Re-running the phase is the fix, not distrusting the file.
  //   undocumented (anything else): what an internal crash outside the
  //     ladder's own contract looks like on the wire (ZEN-101: top-level
  //     exitCode 1 — not a documented code — with `state` left 'complete' and
  //     every per-step result green). Distrust the file itself.
  //
  // The exitCode checks below are complementary to, not redundant with, the
  // producer-side fix that has `tick_gates.ps1` write `state: 'error'` for
  // that same ZEN-101 crash going forward: that write is caught by the
  // `state !== 'complete'` branch above and never reaches either check here.
  // These are what still catch an undocumented exit code on an OLDER
  // recorded verdict (from before that producer-side fix existed) or any
  // other path that manages to leave `state: 'complete'` with an exit code
  // outside the ladder — every branch covers a disjoint verdict shape, none
  // of them the same one twice.
  if (exitCode !== null && exitCode !== undefined && GATE_EXIT_NON_EVIDENTIAL.has(Number(exitCode))) {
    const code = Number(exitCode);
    const reason = code === GATE_EXIT_NO_GATE_LINES
      ? `exitCode ${GATE_EXIT_NO_GATE_LINES} — the phase reported 'complete' having run no gate lines at all ("no gate lines") — documented, but it is evidence of nothing: re-run the phase, this is not a corrupt or untrustworthy file`
      : code === GATE_EXIT_SLOT_BUSY
        ? `exitCode ${GATE_EXIT_SLOT_BUSY} — the build slot could not be acquired within lanes.buildSlotWaitMinutes ("slot busy") — the phase never ran, so there is nothing to be fresh about: wait for the slot and re-run`
        : `exitCode ${GATE_EXIT_TREE_UNSTABLE} — a second start != end tree-SHA mismatch inside one firing ("tree unstable") — the tree was never verified, only found to still be moving: re-run once it settles`;
    return { key, fresh: false, state, exitCode, headSha, skipped, reason };
  }
  if (exitCode !== null && exitCode !== undefined && !GATE_EXIT_DOCUMENTED.has(Number(exitCode))) {
    return {
      key, fresh: false, state, exitCode, headSha, skipped,
      reason: `exitCode ${exitCode} is not one of the codes Tools/tick_gates.ps1's own ladder documents (0, 2, 3, 4, 5, 8, 10, 11, 12) — state was left 'complete' but this is not a verdict the ladder describes; treat as absent`,
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

// ─── 9. Harness identity ────────────────────────────────────────────────

/**
 * `Tools/zagent/protocol/harnesses.json`, read once. Every caller below
 * treats a missing or unparsable file as "no harnesses known" rather than
 * throwing — a harness lookup is a courtesy check (refuse an unknown id,
 * name a known one's `workerToolDenial`), never something a corrupt config
 * file should be able to turn into a hard crash on every `claim`/`finish`.
 */
async function readHarnessesConfig(repoRoot) {
  const filePath = path.join(repoRoot, 'Tools', 'zagent', 'protocol', 'harnesses.json');
  let raw;
  try {
    raw = await fs.readFile(filePath, 'utf8');
  } catch (error) {
    return { found: false, path: filePath, harnesses: {}, error: error?.code === 'ENOENT' ? null : error.message };
  }
  try {
    const parsed = JSON.parse(raw);
    return { found: true, path: filePath, harnesses: (parsed && typeof parsed.harnesses === 'object' && parsed.harnesses) || {}, error: null };
  } catch (error) {
    return { found: true, path: filePath, harnesses: {}, error: error.message };
  }
}

/** Every harness id `harnesses.json` declares — what `claim --harness` validates against. */
export async function knownHarnessIds(repoRoot) {
  const config = await readHarnessesConfig(repoRoot);
  return Object.keys(config.harnesses);
}

/** One harness's profile object (`workerToolDenial`, `review`, …), or `null` if unknown/unset. */
export async function harnessProfile(repoRoot, harnessId) {
  if (!harnessId) return null;
  const config = await readHarnessesConfig(repoRoot);
  return config.harnesses[harnessId] ?? null;
}

// ─── 10. WORKER-MOVED-NOTHING — the I1 compensating control ────────────

/**
 * The fixed set of files `.zagent/run/<KEY>/` legitimately holds once the
 * ORCHESTRATOR itself has written to it — everything `claim` through
 * `finish` are documented to produce. Anything else found there is not one
 * of "the orchestrator's own writes" `scratchMutated` is named for: on a
 * `declared` harness (one whose worker has a real shell) that is exactly
 * where a worker with no business in `.zagent/` would leave a trace.
 */
const KNOWN_SCRATCH_FILES = new Set([
  'ticket.json', 'body.md', 'drift.txt', 'rulings.md', 'prompt.md',
  'filelist.txt', 'build_prompt.exit', 'predispatch.json', 'gates.json',
  'changed.txt', 'tick_gates.json', 'gates.log', 'diff.patch',
  'worklog.md', 'result.md',
  // ★ ADDED after the first REAL tick refused itself. tick_gates.ps1 writes
  // all of these into the same directory, BETWEEN predispatch and guard --
  // so every one of them read as a worker violation and guard refused a
  // clean ticket. 26 false positives on ZEN-106, none of them reachable by a
  // worker that has no shell and no board.
  'gates.prev.log', 'report.json', 'comment.md',
]);

/**
 * ★ AND A FIXED NAME SET CANNOT COVER THE REST: tick_gates.ps1 writes one
 * `gate_<random>.ps1` per gate line, plus `.out` and `.out.err` beside each.
 * The number is fresh every run, so no enumeration can ever match it. Match
 * the SHAPE instead. This is the orchestrator's own scratch by construction --
 * a worker cannot produce it, because producing it means running a gate.
 */
const ORCHESTRATOR_SCRATCH_PATTERNS = [
  /^gate_\d+\.ps1$/,
  /^gate_\d+\.ps1\.out$/,
  /^gate_\d+\.ps1\.out\.err$/,
];

const isOrchestratorScratch = name =>
  KNOWN_SCRATCH_FILES.has(name) || ORCHESTRATOR_SCRATCH_PATTERNS.some(re => re.test(name));

/**
 * `.zagent/run/<KEY>/predispatch.json` — the baseline `build_prompt.ps1`
 * writes LAST, after `claim` and after step 4's `git switch -c`, precisely
 * so neither of those two legitimate moves ever reads as a violation (see
 * that script's own header and TICK.md step 5).
 */
async function readPredispatch(repoRoot, key) {
  const filePath = path.join(repoRoot, '.zagent', 'run', String(key), 'predispatch.json');
  let raw;
  try {
    raw = await fs.readFile(filePath, 'utf8');
  } catch (error) {
    if (error?.code === 'ENOENT') return { found: false, path: filePath, data: null, error: null };
    throw error;
  }
  try {
    return { found: true, path: filePath, data: JSON.parse(raw), error: null };
  } catch (error) {
    return { found: true, path: filePath, data: null, error: error.message };
  }
}

/**
 * One `git status --porcelain` (v1, two-letter status) line, split into its
 * INDEX (position 0 — staged) and WORKTREE (position 1 — unstaged) status
 * characters. This function reads ONLY the index character; nothing in this
 * file's `indexStaged` logic ever branches on the worktree one — see the
 * doc comment on `workerMovedNothing` for why that separation is load-
 * bearing, not incidental.
 */
function porcelainIndexChar(line) {
  return typeof line === 'string' && line.length >= 2 ? line[0] : null;
}

/** The RAW porcelain lines (trimmed) whose INDEX column marks them staged. */
function stagedLines(lines) {
  const set = new Set();
  for (const line of Array.isArray(lines) ? lines : []) {
    const index = porcelainIndexChar(line);
    if (index && index !== ' ' && index !== '?') set.add(String(line).trim());
  }
  return set;
}

async function currentGitState(repoRoot) {
  const runGit = args => execFile('git', ['-C', repoRoot, ...args], { encoding: 'utf8' }).then(r => r.stdout).catch(() => null);
  const [headRaw, branchRaw, statusRaw] = await Promise.all([
    runGit(['rev-parse', 'HEAD']),
    runGit(['rev-parse', '--abbrev-ref', 'HEAD']),
    runGit(['status', '--porcelain']),
  ]);
  return {
    head: headRaw ? headRaw.trim() : null,
    branch: branchRaw ? branchRaw.trim() : null,
    lines: statusRaw ? statusRaw.split(/\r?\n/).filter(Boolean) : [],
  };
}

/** Every plain FILE currently under `.zagent/run/<KEY>/` — no subdirectory convention exists there, so one level deep is exhaustive. */
async function scratchEntries(repoRoot, key) {
  const dir = path.join(repoRoot, '.zagent', 'run', String(key));
  try {
    const entries = await fs.readdir(dir, { withFileTypes: true });
    return entries.filter(entry => entry.isFile()).map(entry => entry.name);
  } catch (error) {
    if (error?.code === 'ENOENT') return [];
    throw error;
  }
}

/**
 * The I1 compensating control TICK.md's Operating Invariants promise on a
 * `declared` harness (one whose worker has a real shell and no structural
 * denial): did the WORKER commit, branch, stage anything, touch the board,
 * or write into scratch it has no business touching, between the baseline
 * `build_prompt.ps1` recorded (step 5, deliberately the LAST write before
 * dispatch — never `claim`'s own; see that script's own header) and right
 * now?
 *
 * `state` is three-valued, matching this file's fail-closed convention: a
 * missing or unparsable baseline is `'unverified'`, never folded into
 * `'clean'` — the same distinction every other check here makes. `ok` is
 * `true` for both `clean` and `unverified` (this function ABSTAINS on a
 * missing baseline rather than refusing; what an `unverified` result means
 * is a call-site decision — `guard` tolerates it unconditionally, `finish`
 * only refuses it on a `declared` harness — never `false` unless `state`
 * is `'violated'`.
 *
 * `currentBoardRow` is caller-supplied (`{status, assignee, updatedAt}`)
 * because this file otherwise touches only git and the filesystem, never a
 * live Vault index — the two real call sites (`guard`'s dispatcher, which
 * already holds one, and `finish`'s `checkFinishContent`, which already
 * holds the ticket record) can supply it cheaply; omitted, `boardMoved` is
 * simply never checked rather than treated as evidence either way.
 *
 * ★ DELIBERATELY DISJOINT FROM `fileListExcursion`, NOT MERELY ORDERED
 * AROUND IT — that is how the two avoid double-reporting the same line.
 * The worker is SUPPOSED to change files: every edit moves a worktree-
 * column `M`, every new file adds a `??` line, so a naive set difference
 * over the whole of `git status --porcelain` reports `violated` on every
 * successful ticket. This function never reads the WORKTREE column or an
 * untracked (`??`) line for ANY purpose — `indexStaged` reads only the
 * INDEX column, because staging anything, even a file the worker was
 * authorised to edit, is the ORCHESTRATOR's own job at step 7
 * ("Integrate"), never the worker's; `guard` (where this runs) fires
 * BEFORE step 7, so a staged index column at that point is never
 * legitimate regardless of which file it names. Which file changed in the
 * WORKTREE, and whether that file was authorised, is `fileListExcursion`'s
 * question alone. The two checks read disjoint columns of the same `git
 * status` output — no single line can ever satisfy both, so neither has to
 * know the other ran, and there is no ordering rule to maintain.
 *
 * `indexStaged` is differential against `predispatch.json`'s own recorded
 * porcelain lines, not absolute against an assumed-empty baseline: step 3's
 * precondition requires a clean tree before `claim`, so the baseline
 * should always show zero staged lines in practice, but a baseline that
 * (for whatever reason) already had one staged must not have that SAME
 * line blamed on the worker. Only a staged line ABSENT from the baseline
 * counts.
 *
 * ★ NARROWER THAN I1, AND THAT GAP IS ACCEPTED, NOT CLOSED. This sees
 * commits, staged files, branches, board rows and stray scratch files. It
 * CANNOT see a worker that ran a BUILD: build output is gitignored, so it
 * leaves nothing in `git status` and nothing under `.zagent/run/<KEY>/`
 * for this function to find. On a `declared` harness that is a real
 * exposure — this repo has already measured a concurrent build producing
 * zero-byte assets — so a green result here is evidence the worker did
 * not commit, branch, stage, touch the board, or litter scratch; it is
 * NEVER proof the worker had no shell at all.
 */
export async function workerMovedNothing({ repoRoot, key, currentBoardRow = null }) {
  const record = await readPredispatch(repoRoot, key);
  if (!record.found) {
    return {
      ok: true, state: 'unverified', violations: [],
      reason: `no predispatch.json at ${record.path} — build_prompt.ps1 has not run (or never reached its write) for this ticket, so WORKER-MOVED-NOTHING cannot be checked. Tolerated at guard; whether that is tolerated at finish depends on the ticket's harness.`,
    };
  }
  const before = record.data;
  if (record.error || !before || typeof before !== 'object'
    || typeof before.head !== 'string' || typeof before.branch !== 'string' || !Array.isArray(before.porcelain)) {
    return {
      ok: true, state: 'unverified', violations: [],
      reason: `predispatch.json at ${record.path} ${record.error ? `did not parse (${record.error})` : 'is missing one of head/branch/porcelain'} — an incomplete baseline is treated the same as no baseline.`,
    };
  }

  const violations = [];
  const after = await currentGitState(repoRoot);

  if (after.head && after.head !== before.head) {
    violations.push({ kind: 'headMoved', detail: `predispatch HEAD ${before.head} -> current HEAD ${after.head}` });
  }
  if (after.branch && after.branch !== before.branch) {
    violations.push({ kind: 'branchMoved', detail: `predispatch branch "${before.branch}" -> current branch "${after.branch}"` });
  }
  const beforeStaged = stagedLines(before.porcelain);
  for (const line of stagedLines(after.lines)) {
    if (!beforeStaged.has(line)) violations.push({ kind: 'indexStaged', detail: line });
  }
  if (currentBoardRow && typeof currentBoardRow === 'object' && before.boardRow && typeof before.boardRow === 'object') {
    // ★ `updatedAt` WAS IN THIS LIST AND IS NOT A WORKER SIGNAL. The board moves
    // it for its own reasons -- on ZEN-106 predispatch captured `null` and the
    // row carried a timestamp by guard, with no board call in between that any
    // worker could have made. `status` and `assignee` are the fields a worker
    // calling `zagent move`/`finish` would actually move; a timestamp changing
    // on its own is noise that refuses clean tickets.
    for (const field of ['status', 'assignee']) {
      const was = before.boardRow[field] ?? null;
      const now = currentBoardRow[field] ?? null;
      if (String(was ?? '') !== String(now ?? '')) {
        violations.push({ kind: 'boardMoved', detail: `${field}: ${JSON.stringify(was)} -> ${JSON.stringify(now)}` });
      }
    }
  }
  for (const name of await scratchEntries(repoRoot, key)) {
    if (!isOrchestratorScratch(name)) violations.push({ kind: 'scratchMutated', detail: `.zagent/run/${key}/${name}` });
  }

  return {
    ok: violations.length === 0,
    state: violations.length === 0 ? 'clean' : 'violated',
    violations,
    reason: violations.length === 0 ? null
      : `${violations.length} WORKER-MOVED-NOTHING violation(s) for ${key}: ${violations.map(v => `${v.kind} (${v.detail})`).join('; ')}`,
  };
}

// ─── 11. Routing-line harness assertion ─────────────────────────────────

const ROUTING_LINE_HARNESS = /\bran on ([^:\s]+):/i;

/** The `<harness>` token out of a `ran on <harness>:<model>/<effort> …` routing line, or `null` if the line is absent. */
export function routingLineHarness(worklogText) {
  const match = ROUTING_LINE_HARNESS.exec(String(worklogText ?? ''));
  return match ? match[1] : null;
}

/**
 * Does the work log's own routing line ("ran on <harness>:…", TICK.md step
 * 8) name the same harness `predispatch.json` recorded at dispatch time
 * (step 5)? Three-valued for the identical reason `workerMovedNothing` is:
 * a missing `harness` field — on either side — is `'unverified'`, never a
 * throw and never a silent `'match'`. `finish` reads this the same way it
 * reads that check's `state`: on an `enforced` harness a missing
 * `predispatch.json` is tolerated, so `'unverified'` here must not itself
 * become a refusal; only a genuine `'mismatch'` does.
 */
export async function routingHarnessMatches({ repoRoot, key, worklog }) {
  const claimed = routingLineHarness(worklog);
  const record = await readPredispatch(repoRoot, key);
  const recorded = record.found && !record.error && record.data && typeof record.data === 'object' && typeof record.data.harness === 'string'
    ? record.data.harness
    : null;
  if (recorded === null) {
    return {
      state: 'unverified', match: null, claimed, recorded: null,
      reason: `no harness recorded in predispatch.json for ${key} — the work log's routing-line harness token cannot be checked against anything`,
    };
  }
  if (claimed === null) {
    return {
      state: 'unverified', match: null, claimed, recorded,
      reason: `the work log carries no "ran on <harness>:<model>/<effort> …" routing line for ${key} — cannot check its harness token`,
    };
  }
  const match = claimed === recorded;
  return {
    state: match ? 'match' : 'mismatch', match, claimed, recorded,
    reason: match ? null
      : `the work log's routing line names harness "${claimed}", but predispatch.json recorded "${recorded}" for ${key} — the two must agree`,
  };
}
