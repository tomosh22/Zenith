// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
const WORKLOG_START_PREFIX = "<!-- agent:worklog:start";
const WORKLOG_END = "<!-- agent:worklog:end -->";
function worklogStart(key) {
  return `${WORKLOG_START_PREFIX} ${key} -->`;
}
function specOf(description) {
  if (!description) return "";
  const at = description.indexOf(WORKLOG_START_PREFIX);
  return at === -1 ? description : description.slice(0, at);
}
function parseTicket(description) {
  const sections = splitSections(specOf(description));
  const goal = (sections.get("goal") ?? "").trim();
  const definitionOfDone = [];
  let open = null;
  for (const line of (sections.get("definition of done") ?? "").split("\n")) {
    const m = /^\s*[-*]\s*\[( |x|X)\]\s*(.+?)\s*$/.exec(line);
    if (m) {
      open = { text: m[2], done: m[1].toLowerCase() === "x" };
      definitionOfDone.push(open);
      continue;
    }
    if (line.trim().length === 0) continue;
    if (open && /^\s+\S/.test(line) && !/^\s*[-*]\s*\[/.test(line)) {
      open.text += ` ${line.trim()}`;
      continue;
    }
    open = null;
  }
  const gatesRaw = sections.get("gates");
  const gates = gatesRaw === void 0 ? null : gatesRaw.split("\n").map((l) => l.trim()).filter((l) => l.length > 0 && !l.startsWith("```") && !l.startsWith("#"));
  return { goal, definitionOfDone, gates };
}
function validateTicket(parsed, options) {
  if (options.categoryError) return options.categoryError;
  if (!parsed.goal) {
    return "The description has no `## Goal` section, so there is nothing to implement. Add one (see CONTRIBUTING.md, Agent loop).";
  }
  if (parsed.definitionOfDone.length === 0) {
    return "The description has no `## Definition of Done` checklist, so there is no observable outcome to verify against.";
  }
  const hasGates = parsed.gates !== null && parsed.gates.length > 0;
  if (!hasGates && !options.hasConfiguredGates) {
    return "No `## Gates` section on the ticket and no gates configured for this project or category, so nothing could verify the work before merging.";
  }
  return null;
}
function upsertWorklog(description, key, worklogBody) {
  const start = worklogStart(key);
  const block = `${start}
${worklogBody.trim()}
${WORKLOG_END}`;
  const existing = description ?? "";
  const startAt = existing.indexOf(start);
  if (startAt !== -1) {
    const endAt = existing.indexOf(WORKLOG_END, startAt);
    const cutTo = endAt === -1 ? existing.length : endAt + WORKLOG_END.length;
    return `${existing.slice(0, startAt)}${block}${existing.slice(cutTo)}`;
  }
  const spec = existing.replace(/\s+$/, "");
  return spec.length > 0 ? `${spec}

${block}
` : `${block}
`;
}
function splitSections(body) {
  const out = /* @__PURE__ */ new Map();
  let current = null;
  let buffer = [];
  const flush = () => {
    if (current !== null) out.set(current, buffer.join("\n"));
    buffer = [];
  };
  for (const line of body.split("\n")) {
    const heading = /^\s{0,3}#{2,3}\s+(.+?)\s*$/.exec(line);
    if (heading) {
      flush();
      current = heading[1].toLowerCase();
      continue;
    }
    if (current !== null) buffer.push(line);
  }
  flush();
  return out;
}
const REVIEW_TRIGGERS = [
  {
    test: /\b(?:unchanged|behaviou?r[-\s]preserving|byte[-\s]identical|bit[-\s]identical|identical|equivalent|no (?:behaviou?r|functional|semantic|observable) change)\b/i,
    reason: "asserts an EQUIVALENCE \u2014 a gate can show the new path passes, never that the old one is untouched"
  },
  {
    test: /\b(?:no regressions?|no new|must not|never|nothing else|without (?:changing|touching|breaking))\b/i,
    reason: "asserts an ABSENCE \u2014 a passing gate is not evidence that something did not happen"
  },
  {
    test: /\b(?:covered by (?:a )?tests?|has coverage|coverage for|is (?:covered|tested)|tests? (?:cover|assert)s?)\b/i,
    reason: "asserts something ABOUT the tests, which the tests cannot check for themselves"
  }
];
function dodAssertions(items) {
  const out = [];
  for (const item of items) {
    const trigger = REVIEW_TRIGGERS.find((t) => t.test.test(item.text));
    if (trigger) out.push({ item: item.text, reason: trigger.reason });
  }
  return out;
}
function reviewDecision(input) {
  const reasons = [];
  if (input.reviewOn.includes(input.risk)) {
    reasons.push(`risk ${input.risk} is in reviewOn`);
  }
  for (const hit of dodAssertions(input.definitionOfDone)) {
    reasons.push(`"${hit.item}" ${hit.reason}`);
  }
  // ★ THIRD OR TERM: the ticket's subject matter is governed by a recorded
  // decision. `Tools/zagent/protocol/TICK.md` used to carry this as prose in
  // front of the review-dispatch step ("if a ruling governs the subject
  // matter, review it whatever review.required says") rather than as
  // something computed — ZM-23 shipped a trainer row contradicting
  // ZM-D-196 Ruling 3 (verified against `Games/Zenithmon/Docs/DecisionLog.md`
  // 2026-09-21 — a prior copy of this comment said ZM-D-203, which is a
  // different, unrelated entry; ZM-D-196's own text is cross-cited as
  // "Ruling 3" by at least one later entry in the same log) and was only
  // caught because MEDIUM risk happened to sit in reviewOn; filed LOW,
  // honestly, it would have merged unread.
  // `decisionLogText`/`decisionIdPrefix` are optional: a caller that has not
  // resolved them (the existing compat.mjs call site does not) simply gets
  // no candidates here, same as before this term existed — never a crash,
  // never a forced review. Shares `matchGoverningRulings` with the claim-time
  // rulings.md resolution (client.mjs) so the two cannot drift into
  // different answers to "what governs this ticket".
  const rulingHits = input.decisionLogText && input.decisionIdPrefix
    ? matchGoverningRulings(
      input.decisionLogText,
      input.decisionIdPrefix,
      [input.goal, ...input.definitionOfDone.map((item) => item.text)].filter(Boolean).join("\n"),
      { minScore: REVIEW_RULING_MIN_SCORE }
    )
    : [];
  for (const hit of rulingHits) {
    reasons.push(
      `${hit.id} appears to govern this ticket's subject matter (${hit.identifierHits} identifier hit(s), ${hit.nounHits} noun hit(s)) — read it before merging`
    );
  }
  // `rulingHits` is additive and OMITTED entirely when empty, not present as
  // `[]` — every existing caller (including compat.mjs's `reviewOf`, which
  // does not pass decisionLogText/decisionIdPrefix yet, and every frozen
  // parity fixture) must see the exact `{required, reasons}` shape it saw
  // before this term existed. A caller that supplies the new optional
  // inputs and gets a real match is the only one that ever sees the key.
  const result = { required: reasons.length > 0, reasons };
  if (rulingHits.length) result.rulingHits = rulingHits.map((hit) => hit.id);
  return result;
}

// ─── shared decision-log matching (claim-time rulings.md + review's third OR term) ───
//
// Both readers ask the same underlying question — "which recorded rulings
// plausibly govern this ticket's subject matter" — from the ticket's Goal
// and Definition of Done against a category's `decisionLog`. One matcher,
// two call sites (client.mjs's rulings.md write, and `reviewDecision` above),
// each choosing its own `minScore`: rulings.md prefers RECALL,
// because a spurious candidate costs a reader a few seconds; the review
// trigger prefers recall too but a notch tighter, because a false
// positive there spends a whole reviewer dispatch while a false negative
// ships an unreviewed violation — still the worse of the two failure modes,
// which is why both thresholds stay on the loose side rather than meeting
// in the middle.
//
// ★ Measured on the real ZM board (2026-09-21): a raw `nounHits` count let
// ZM-76 match 25 rulings on 1 identifier hit and 22 NOUN hits — two generic
// domain nouns ("trainer", "scene", "test", "pin"...) recur in nearly every
// entry of a Zenithmon-shaped decision log, so counting them made
// `review.required` constant-true across the board. A noun's WEIGHT is now
// its rarity across the very entries being scanned — `nounRarity` below —
// so a term that recurs in most of the log's entries counts for almost
// nothing, and one confined to a couple of entries counts for a lot,
// without a hand-maintained stopword list: the log's own entry population
// is the corpus, so the weighting can never go stale the way a fixed list
// would. `identifierHits` keeps its flat weight (a real code identifier or
// a direct `<prefix>NNN` citation of the entry's own ID is strong evidence
// on its own, however common the surrounding prose is) and now also fires
// on a citation, not only a shared CONSTANT_CASE/PascalCase symbol — a
// ticket that names its governing ruling outright must not be outscored by
// one that merely shares a common word with it.
// ★ Recalibrated against the real ZM/ZEN boards (2026-09-21, see
// `Tools/Vault/cli/legacy-pure/ticket.mjs` git history for the measurement):
// with `nounRarity` normalized to (0, 1] per entry and capped at the three
// rarest shared terms, "two distinctive-noun hits" tops out at 3.0, so a
// bare noun overlap alone can no longer reach either threshold on its own —
// it takes real rarity AND volume together, or an identifier/citation hit.
// minScore 4 clears on identifierHits>=2 alone, or identifierHits==1 plus a
// modest rare-noun showing; minScore 8 needs identifierHits>=4, or
// identifierHits>=1 plus the full 3.0 noun cap, or a very strong
// identifier+noun combination — which is exactly ZM-23's and ZM-76's shape
// (both cite ZM-D-196/ZM-D-208 directly AND share real vocabulary with
// them), so the incident this term exists for still clears the tighter
// bar, not merely the looser one.
const RULINGS_FILE_MIN_SCORE = 4;
const REVIEW_RULING_MIN_SCORE = 8;
const IDENTIFIER_WEIGHT = 2;

// `nounRarity` below returns 1.0 for a term unique to exactly one entry
// and decays toward 0 as a term approaches ubiquity — normalized against
// THIS log's own entry count, so the two threshold constants above stay
// meaningful whether the corpus is a two-entry fixture or the real
// 400+-entry Zenithmon log. Without the normalization a "fully distinctive"
// term's raw weight is `log2(N+1)`, which scales with the corpus — flat
// thresholds tuned against a small fixture would then pass on almost
// everything once N grew into the hundreds (measured: a bare identifier
// weight of 2 was swamped by unnormalized noun scores of 15-30 on the real
// ZM log). Normalizing keeps "one maximally distinctive shared term" worth
// exactly 1.0 always, so `IDENTIFIER_WEIGHT` stays comparable to it at any
// corpus size instead of needing its own corpus-dependent rescale.

// CONSTANT_CASE / Prefixed_PascalCase identifiers, scanned out of running
// prose rather than matched whole (contrast SYMBOL_RE above, which anchors
// on an already-isolated backtick token). Shared with write.mjs's create()
// (the DoD blast-radius scan) so the two never disagree about what
// counts as "an identifier this codebase would recognise".
const IDENTIFIER_SCAN_RE = /\b(?:[A-Za-z][A-Za-z0-9]*_[A-Za-z0-9_]+|[A-Z][a-z0-9]+(?:[A-Z][a-z0-9]*)+)\b/g;

const MATCH_STOPWORDS = /* @__PURE__ */ new Set([
  "about", "after", "again", "against", "always", "another", "around", "because", "before",
  "being", "below", "between", "cannot", "could", "different", "does", "doing", "during",
  "either", "every", "explicitly", "finally", "first", "following", "further", "genuinely",
  "having", "here", "however", "instead", "itself", "just", "large", "later", "leave",
  "leaves", "makes", "means", "might", "more", "most", "much", "must", "never", "nothing",
  "only", "other", "over", "own", "particular", "please", "point", "rather", "really",
  "same", "should", "since", "small", "some", "still", "such", "take", "than", "that",
  "their", "them", "then", "there", "these", "they", "this", "those", "through", "under",
  "until", "very", "want", "wants", "were", "what", "when", "where", "which", "while",
  "will", "with", "within", "without", "would"
]);

function escapeForRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// Distinctive-noun / identifier extraction shared by both call sites.
function significantTerms(text) {
  const identifiers = /* @__PURE__ */ new Set();
  for (const match of String(text ?? "").matchAll(IDENTIFIER_SCAN_RE)) identifiers.add(match[0]);
  const nouns = /* @__PURE__ */ new Set();
  for (const word of String(text ?? "").split(/[^A-Za-z]+/)) {
    if (word.length < 6) continue;
    const lower = word.toLowerCase();
    if (MATCH_STOPWORDS.has(lower)) continue;
    nouns.add(lower);
  }
  return { identifiers, nouns };
}

// Splits a decision log into per-entry text blocks at heading boundaries
// (`##`/`###`/...), keyed by every `<prefix><digits>` reference the block
// contains. Deliberately format-tolerant rather than parsing a specific
// entry grammar: this repo's decision logs mix `## <date> -- <ID> --
// <title>` top-level entries with `### <ID> — <date>` sub-entries in the
// same file, and a heading-boundary split degrades gracefully on either —
// worst case an entry's block is smaller than its full prose, which costs
// recall a little rather than crashing or mis-attributing text to the wrong
// ID.
function decisionLogEntries(decisionLogText, idPrefix) {
  const text = String(decisionLogText ?? "");
  const prefix = String(idPrefix ?? "").trim();
  if (!text.trim() || !prefix) return [];
  const idPattern = new RegExp(`${escapeForRegExp(prefix)}(\\d{1,6})`, "g");
  const headingPattern = /^#{1,6}[ \t]+.*$/gm;
  const bounds = [...text.matchAll(headingPattern)].map((match) => match.index);
  if (bounds.length === 0 || bounds[0] !== 0) bounds.unshift(0);
  bounds.push(text.length);
  const entries = [];
  for (let i = 0; i < bounds.length - 1; i += 1) {
    const block = text.slice(bounds[i], bounds[i + 1]);
    const ids = /* @__PURE__ */ new Set();
    for (const match of block.matchAll(idPattern)) ids.add(`${prefix}${match[1]}`);
    if (ids.size === 0) continue;
    entries.push({ ids: [...ids], text: block });
  }
  return entries;
}

// How many of a decision log's OWN entries a noun appears in, computed
// fresh per call from `entries` (the same blocks being scored) — never
// pinned across logs or across time, so it tracks the log as it grows
// rather than drifting the way a hand-maintained list would.
function nounDocumentFrequencies(entries) {
  const df = /* @__PURE__ */ new Map();
  for (const entry of entries) {
    const { nouns } = significantTerms(entry.text);
    for (const noun of nouns) df.set(noun, (df.get(noun) ?? 0) + 1);
  }
  return df;
}

// log2((N+1)/df), normalized by its own ceiling log2(N+1) so the result is
// always in (0, 1]: a term unique to a single entry — however large the
// corpus — scores exactly 1 (matching the flat weight a raw hit count gave
// every noun hit before this existed, so a tiny single-entry fixture log
// still behaves exactly the way it always did). A term shared by most of a
// real, many-entry log decays toward 0 as `df` approaches `N`, regardless
// of how large N is. No stopword list: rarity is measured against the log
// doing the matching, not a fixed vocabulary.
function nounRarity(term, documentFrequencies, entryCount) {
  const df = documentFrequencies.get(term) ?? entryCount;
  const ceiling = Math.log2(entryCount + 1);
  if (ceiling <= 0) return 0; // entryCount === 0: no corpus to measure rarity against
  return Math.log2((entryCount + 1) / Math.max(1, df)) / ceiling;
}

// A ticket's Goal+DoD and a long-running decision entry both being
// long PROSE means they can share a couple of dozen words by sheer volume
// even when only a handful are individually distinctive — measured on
// ZM-76 pre-fix, 22 raw noun hits against a Zenithmon decision log. Rarity
// weighting alone does not bound that: summed across every shared word,
// enough individually-modest weights still reconstruct a large score. Only
// the `NOUN_HIT_CAP` rarest shared terms count, so a match is decided by
// its most distinctive overlap, not by how much vocabulary two long
// documents happen to share.
const NOUN_HIT_CAP = 3;

// `<idPrefix>NNN` citations inside the subject text itself (a ticket
// naming its governing ruling outright, e.g. "under ZM-D-208 §2.1") —
// distinct from `IDENTIFIER_SCAN_RE`, which only catches CONSTANT_CASE /
// Prefixed_PascalCase code symbols and never a hyphenated decision ID.
function citedDecisionIds(subjectText, idPrefix) {
  const prefix = String(idPrefix ?? '').trim();
  const ids = /* @__PURE__ */ new Set();
  if (!prefix) return ids;
  const idPattern = new RegExp(`${escapeForRegExp(prefix)}(\\d{1,6})`, 'g');
  for (const match of String(subjectText ?? '').matchAll(idPattern)) ids.add(`${prefix}${match[1]}`);
  return ids;
}

/**
 * Candidate decision-log entries whose text overlaps the given subject text
 * (a ticket's Goal + Definition of Done, typically). Returns entries scoring
 * at or above `minScore`, sorted strongest match first, deduplicated by ID
 * and capped at `limit`.
 *
 * `score = identifierHits * IDENTIFIER_WEIGHT + sum(nounRarity(shared noun))`.
 * `identifierHits` counts a shared CONSTANT_CASE/PascalCase code identifier
 * (e.g. `ZM_TRAINER_ROUTE1_RAMBLER` in both the ticket and the entry) AND a
 * direct citation of the entry's own decision ID in the subject text — both
 * are strong, largely self-sufficient evidence, so they carry a flat
 * weight. A noun hit's weight is that noun's rarity across the log's own
 * entries (`nounRarity`), not a flat 1 — see the block comment above the
 * threshold constants for why.
 */
function matchGoverningRulings(decisionLogText, idPrefix, subjectText, options = {}) {
  const minScore = options.minScore ?? RULINGS_FILE_MIN_SCORE;
  const limit = options.limit ?? 25;
  const subject = significantTerms(subjectText);
  const citedIds = citedDecisionIds(subjectText, idPrefix);
  if (subject.identifiers.size === 0 && subject.nouns.size === 0 && citedIds.size === 0) return [];
  const entries = decisionLogEntries(decisionLogText, idPrefix);
  const nounDf = nounDocumentFrequencies(entries);
  const entryCount = entries.length;
  const scored = [];
  for (const entry of entries) {
    const entryTerms = significantTerms(entry.text);
    let identifierHits = 0;
    for (const identifier of subject.identifiers) if (entryTerms.identifiers.has(identifier)) identifierHits += 1;
    if (entry.ids.some((id) => citedIds.has(id))) identifierHits += 1;
    const nounWeights = [];
    for (const noun of subject.nouns) {
      if (!entryTerms.nouns.has(noun)) continue;
      nounWeights.push(nounRarity(noun, nounDf, entryCount));
    }
    const nounHits = nounWeights.length;
    nounWeights.sort((a, b) => b - a);
    const nounScore = nounWeights.slice(0, NOUN_HIT_CAP).reduce((sum, w) => sum + w, 0);
    const score = identifierHits * IDENTIFIER_WEIGHT + nounScore;
    if (score < minScore) continue;
    for (const id of entry.ids) scored.push({ id, score, identifierHits, nounHits, snippet: entry.text.trim().slice(0, 600) });
  }
  scored.sort((a, b) => b.score - a.score || String(a.id).localeCompare(String(b.id)));
  const seen = /* @__PURE__ */ new Set();
  const deduped = [];
  for (const hit of scored) {
    if (seen.has(hit.id)) continue;
    seen.add(hit.id);
    deduped.push(hit);
    if (deduped.length >= limit) break;
  }
  return deduped;
}
const REFERENCE_QUALIFIERS = {
  blocking: "blocks",
  linked: "linked",
  related: "linked",
  parent: "parent"
};
const REFERENCE_NOUNS = {
  bug: ["BUG"],
  task: ["TASK"],
  story: ["STORY"],
  epic: ["EPIC"],
  subtask: ["SUBTASK"],
  issue: null,
  ticket: null,
  card: null
};
const QUALIFIED_REFERENCE_RE = new RegExp(
  `\\bthe\\s+(${Object.keys(REFERENCE_QUALIFIERS).join("|")})\\s+(${Object.keys(REFERENCE_NOUNS).join("|")})\\b`,
  "gi"
);
const BARE_BLOCKER_RE = /\bthe\s+blocker\b/gi;
function boardReferences(items) {
  const out = [];
  for (const item of items) {
    for (const m of item.text.matchAll(QUALIFIED_REFERENCE_RE)) {
      const relation = REFERENCE_QUALIFIERS[m[1].toLowerCase()];
      const types = REFERENCE_NOUNS[m[2].toLowerCase()] ?? null;
      out.push({
        item: item.text,
        phrase: m[0],
        relation,
        types,
        what: describeReference(relation, types)
      });
    }
    for (const m of item.text.matchAll(BARE_BLOCKER_RE)) {
      out.push({
        item: item.text,
        phrase: m[0],
        relation: "blocks",
        types: null,
        what: describeReference("blocks", null)
      });
    }
  }
  return out;
}
function describeReference(relation, types) {
  const noun = types === null ? "an issue" : `a ${types.join("/")}`;
  if (relation === "blocks") return `${noun} that BLOCKS this ticket`;
  if (relation === "parent") return `${noun} as this ticket's PARENT`;
  return `${noun} LINKED to this ticket`;
}
function danglingBoardReferences(items, linked) {
  return boardReferences(items).filter(
    (ref) => !linked.some(
      (l) => (l.relation === ref.relation || ref.relation === "linked" && l.relation === "blocks") && (ref.types === null || ref.types.includes(l.type))
    )
  );
}
const HUMAN_ONLY_CLAIMS = [
  { test: /\b(?:human|manual|visual)[-\s](?:sign[-\s]?off|approval)\b/gi, kind: "sign-off" },
  { test: /\bsigned[-\s]off by a (?:human|person)\b/gi, kind: "sign-off" },
  {
    test: /\bno agent (?:may|can|could|should|will) (?:sign|approve|accept|verify|confirm)\b/gi,
    kind: "sign-off"
  },
  {
    test: /\ba (?:human|person) must (?:sign|approve|accept|verify|confirm)\b/gi,
    kind: "sign-off"
  },
  { test: /\bhuman[-\s](?:executed|authored|only)\b/gi, kind: "execution" },
  { test: /\bneeds a (?:human|person)\b/gi, kind: "execution" },
  { test: /\bno agent (?:may|can|could|should|will)\b/gi, kind: "execution" },
  { test: /\bonly a (?:human|person) can\b/gi, kind: "execution" },
  { test: /\ba (?:human|person) must\b/gi, kind: "execution" },
  {
    test: /\bmust be (?:done|run|authored|performed) by a (?:human|person)\b/gi,
    kind: "execution"
  }
];
function humanOnlyClaims(description) {
  const spec = specOf(description);
  const out = [];
  const taken = [];
  for (const { test, kind } of HUMAN_ONLY_CLAIMS) {
    for (const m of spec.matchAll(test)) {
      const start = m.index;
      const end = start + m[0].length;
      if (taken.some(([s, e]) => start < e && end > s)) continue;
      taken.push([start, end]);
      out.push({ phrase: m[0].trim(), kind });
    }
  }
  return out;
}
const MAX_CITATIONS = 20;
const PATH_RE = /^[\w.@+-]+(?:\/[\w.@+-]+)+$/;
const SYMBOL_RE = /^(?:[A-Za-z][A-Za-z0-9]*_[A-Za-z0-9_]+|[A-Z][a-z0-9]+(?:[A-Z][a-z0-9]*){1,})$/;
function citationsIn(description) {
  const spec = specOf(description);
  const paths = [];
  const symbols = [];
  for (const match of spec.matchAll(/`([^`\n]+)`/g)) {
    const inner = match[1].trim();
    if (SYMBOL_RE.test(inner) && !symbols.includes(inner)) symbols.push(inner);
    const asPath = inner.replace(/\\/g, "/").replace(/^\.\//, "");
    if (isCitedPath(asPath) && !paths.includes(asPath)) paths.push(asPath);
  }
  for (const raw of spec.split(/[\s,;()[\]{}<>"'`|]+/)) {
    const token = raw.replace(/\\/g, "/").replace(/^\.\//, "").replace(/[.:!?]+$/, "");
    if (isCitedPath(token) && !paths.includes(token)) paths.push(token);
  }
  return {
    paths: paths.slice(0, MAX_CITATIONS),
    symbols: symbols.slice(0, MAX_CITATIONS),
    lines: lineCitations(spec).slice(0, MAX_CITATIONS)
  };
}
function lineCitations(spec) {
  const out = [];
  const push = (v) => {
    if (!out.includes(v)) out.push(v);
  };
  for (const m of spec.matchAll(
    /[\w./\\-]+\.(?:h|hpp|inl|cpp|c|cs|ts|tsx|js|ps1|psm1|json|md|yml|yaml):\d+/g
  )) {
    push(m[0]);
  }
  for (const m of spec.matchAll(/\blines?\s+\d+(?:\s*[-–]\s*\d+)?/gi)) {
    push(m[0]);
  }
  return out;
}
const BARE_FILE_RE = /^[\w@+-][\w.@+-]*\.(?:md|txt|cpp|cc|cxx|h|hpp|inl|ts|tsx|js|jsx|json|ps1|psm1|psd1|yml|yaml|cs|py|sh|sql|toml|ini)$/i;
function isCitedPath(token) {
  if (token.includes("*") || /^https?:/i.test(token)) return false;
  if (!token.includes("/")) return BARE_FILE_RE.test(token);
  if (!PATH_RE.test(token)) return false;
  const segments = token.split("/");
  return segments.length > 2 || segments[segments.length - 1].includes(".");
}
function logOf(description) {
  if (!description) return "";
  const at = description.indexOf(WORKLOG_START_PREFIX);
  return at === -1 ? "" : description.slice(at);
}
function replaceSpec(description, spec) {
  const log = logOf(description);
  const body = spec.replace(/\s+$/, "");
  return log ? `${body}

${log}` : `${body}
`;
}
function replaceSection(description, heading, body) {
  const spec = specOf(description);
  const lines = spec.split("\n");
  const wanted = heading.toLowerCase();
  let start = -1;
  let end = lines.length;
  for (let i = 0; i < lines.length; i++) {
    const match = /^\s{0,3}#{2,3}\s+(.+?)\s*$/.exec(lines[i]);
    if (!match) continue;
    if (start === -1 && match[1].toLowerCase() === wanted) {
      start = i;
      continue;
    }
    if (start !== -1) {
      end = i;
      break;
    }
  }
  if (start === -1) {
    throw new Error(`The description has no \`## ${heading}\` section to replace.`);
  }
  const replaced = [
    ...lines.slice(0, start + 1),
    ...body.replace(/\s+$/, "").split("\n"),
    "",
    ...lines.slice(end)
  ].join("\n");
  return replaceSpec(description, replaced);
}
// Distinct identifiers a piece of prose mentions, in the CONSTANT_CASE /
// Prefixed_PascalCase shapes this codebase's naming convention produces
// (root CLAUDE.md's Type Prefixes table). Exported for write.mjs's
// `create()` (the DoD blast-radius scan) so it extracts identifiers the
// same way `matchGoverningRulings` does internally, rather than a second,
// possibly-drifting regex.
function scanIdentifiers(text) {
  const found = [];
  const seen = /* @__PURE__ */ new Set();
  for (const match of String(text ?? "").matchAll(IDENTIFIER_SCAN_RE)) {
    if (seen.has(match[0])) continue;
    seen.add(match[0]);
    found.push(match[0]);
  }
  return found;
}

export {
  WORKLOG_END,
  WORKLOG_START_PREFIX,
  boardReferences,
  citationsIn,
  danglingBoardReferences,
  dodAssertions,
  humanOnlyClaims,
  matchGoverningRulings,
  parseTicket,
  replaceSection,
  replaceSpec,
  reviewDecision,
  REVIEW_RULING_MIN_SCORE,
  RULINGS_FILE_MIN_SCORE,
  scanIdentifiers,
  specOf,
  upsertWorklog,
  validateTicket,
  worklogStart
};
