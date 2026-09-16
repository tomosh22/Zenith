// Generated from saas read-only policy helpers; see provenance.json. No DB or network imports.
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
  return { required: reasons.length > 0, reasons };
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
export {
  WORKLOG_END,
  WORKLOG_START_PREFIX,
  boardReferences,
  citationsIn,
  danglingBoardReferences,
  dodAssertions,
  humanOnlyClaims,
  parseTicket,
  replaceSection,
  replaceSpec,
  reviewDecision,
  specOf,
  upsertWorklog,
  validateTicket,
  worklogStart
};
