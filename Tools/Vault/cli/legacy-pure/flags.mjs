// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
class BoardError extends Error {
}
const GLOBAL_FLAGS = ["json"];
const COMMAND_FLAGS = {
  help: [],
  contract: [],
  auth: ["name", "email"],
  provision: [],
  doctor: [],
  next: ["project", "category"],
  claim: ["project", "category"],
  show: ["project"],
  check: ["project"],
  queue: ["project"],
  owns: ["project"],
  comment: ["file", "text"],
  move: [],
  finish: ["status", "comment", "worklog"],
  gates: ["file", "text"],
  guard: ["file", "text"],
  create: [
    "project",
    "title",
    // `--file <path>` or `--text "…"` — `this.body()` takes either, and
    // the suite files a ticket with `--text`.
    "file",
    "text",
    "type",
    "priority",
    "complexity",
    "risk",
    "category",
    "status",
    "parent",
    "assignee",
    "label",
    "points",
    "severity",
    "repro",
    "environment",
    "due",
    "flag",
    "blocks",
    "blocked-by",
    "reason",
    "fix-version",
    "sprint"
  ],
  link: ["reason"],
  unlink: ["reason"],
  links: [],
  transitions: [],
  blocked: ["project"],
  epic: [],
  parent: [],
  // `--force` is how `sprint complete` carries unfinished work to the
  // backlog — a deliberate gesture, and one the closed vocabulary used
  // to REFUSE, which made the documented carry-over path unreachable
  // through the CLI while the function beneath it accepted it.
  sprint: ["project", "goal", "start", "end", "force"],
  version: ["project", "description"],
  update: [
    "label",
    "unlabel",
    "points",
    "clear",
    "resolution",
    "title",
    "type",
    "priority",
    "complexity",
    "risk",
    "severity",
    "repro",
    "environment",
    "due",
    "assignee",
    "body",
    "goal",
    "dod"
  ],
  estimate: ["points"],
  flag: ["clear"],
  resolve: ["resolution", "clear"],
  history: ["limit"],
  report: ["project"],
  board: ["project", "roadmap"],
  notion: ["project", "org"],
  docs: [
    "project",
    "category",
    "force",
    "dry-run",
    "no-verify",
    "depth",
    "archived",
    "recursive",
    "live",
    "out",
    "page",
    "append",
    "icon",
    "path",
    "titles",
    "deep",
    "limit",
    "file"
  ],
  rows: ["where", "limit", "org", "project", "ticket", "reason"],
  suggestions: ["all", "limit", "org", "project", "reason"],
  // The knowledge-base writers. All five take the same anchor flags and
  // read their row from `--file`/`--text`.
  ask: ["ticket", "org", "date", "file", "text", "category", "project"],
  decide: ["ticket", "org", "date", "file", "text", "category", "project"],
  shortfall: ["ticket", "org", "date", "file", "text", "category", "project"],
  changelog: ["ticket", "org", "date", "file", "text", "category", "project"],
  suggest: ["ticket", "org", "date", "file", "text", "category", "project"]
};
const ALL_FLAGS = /* @__PURE__ */ new Set([...GLOBAL_FLAGS, ...Object.values(COMMAND_FLAGS).flat()]);
function isHelpRequest(args) {
  const first = args.positional[0];
  if (first === void 0 || first === "help") return true;
  if (args.flags.has("help") || args.flags.has("h")) return true;
  return args.positional.some((token) => HELP_TOKENS.has(token));
}
const HELP_TOKENS = /* @__PURE__ */ new Set(["-h", "-help", "--help", "-?", "/?"]);
function assertKnownFlags(args) {
  const command = args.positional[0];
  if (command === void 0) return;
  const known = COMMAND_FLAGS[command];
  if (!known) return;
  const allowed = /* @__PURE__ */ new Set([...GLOBAL_FLAGS, ...known]);
  const unknown = [...args.flags.keys()].filter((name) => !allowed.has(name));
  if (unknown.length === 0) return;
  const details = unknown.map((name) => {
    const near = nearest(name, allowed);
    if (near) return `--${name} (did you mean --${near}?)`;
    if (ALL_FLAGS.has(name)) return `--${name} (not a flag \`${command}\` reads)`;
    return `--${name}`;
  });
  throw new BoardError(
    `Unknown flag${unknown.length > 1 ? "s" : ""} for \`zagent ${command}\`: ${details.join(", ")}.
${command} takes: ${[...allowed].sort().map((f) => `--${f}`).join(", ")}.
Nothing was claimed or changed. Flag names are a closed vocabulary now \u2014 a typo used to be swallowed, and \`--projectt ZM\` silently listed a different board.`
  );
}
function nearest(name, allowed) {
  let best = null;
  let bestDistance = Infinity;
  for (const candidate of allowed) {
    const distance = editDistance(name, candidate);
    if (distance < bestDistance) {
      bestDistance = distance;
      best = candidate;
    }
  }
  return bestDistance <= 1 ? best : null;
}
function editDistance(a, b) {
  if (Math.abs(a.length - b.length) > 1) return Infinity;
  let previous = Array.from({ length: b.length + 1 }, (_, i) => i);
  for (let i = 1; i <= a.length; i++) {
    const current = [i];
    for (let j = 1; j <= b.length; j++) {
      current[j] = Math.min(
        previous[j] + 1,
        current[j - 1] + 1,
        previous[j - 1] + (a[i - 1] === b[j - 1] ? 0 : 1)
      );
    }
    previous = current;
  }
  return previous[b.length];
}
export {
  assertKnownFlags,
  isHelpRequest
};
