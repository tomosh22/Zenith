// Ported from SaaS packages/lib/src/board.ts; see provenance.json.
const JIRA_BOARD_TYPES = ["KANBAN", "SCRUM"];
function boardShowsOnlyActiveSprint(type) {
  return type === "SCRUM";
}
const JIRA_SPRINT_STATUSES = ["PLANNED", "ACTIVE", "COMPLETED"];
const CURRENT_SPRINT_SUFFIX = " (current)";
function sprintDisplayName(name, status) {
  return status === "ACTIVE" ? `${name}${CURRENT_SPRINT_SUFFIX}` : name;
}
const JIRA_SWIMLANE_MODES = ["NONE", "EPIC", "ASSIGNEE"];
const JIRA_SWIMLANE_LABELS = {
  NONE: "No swimlanes",
  EPIC: "By epic",
  ASSIGNEE: "By assignee"
};
const JIRA_CARD_FIELDS = [
  "key",
  "type",
  "priority",
  "assignee",
  "storyPoints",
  "category",
  "epic",
  "severity",
  "flagged",
  "blocked"
];
const JIRA_CARD_FIELD_LABELS = {
  key: "Issue key",
  type: "Type",
  priority: "Priority",
  assignee: "Assignee",
  storyPoints: "Story points",
  category: "Category",
  epic: "Epic",
  severity: "Severity",
  flagged: "Flag",
  blocked: "Blocked-by badge"
};
const DEFAULT_CARD_FIELDS = [...JIRA_CARD_FIELDS];
function normalizeCardFields(value) {
  if (!Array.isArray(value)) return [...DEFAULT_CARD_FIELDS];
  const allowed = new Set(JIRA_CARD_FIELDS);
  const out = value.filter((v) => typeof v === "string" && allowed.has(v));
  return out;
}
function blockedByKeys(blockers) {
  return blockers.map((b) => b.key).join(", ");
}
function blockedByTooltip(blockers) {
  const head = `Blocked by ${blockedByKeys(blockers)} \u2014 open this issue to see or remove the link`;
  const explained = blockers.filter((b) => (b.reason ?? "").trim() !== "");
  if (explained.length === 0) return head;
  return [head, "", ...explained.map((b) => `${b.key}: ${b.reason.trim()}`)].join("\n");
}
const NEEDS_GPU_LABEL = "needs-gpu";
const NEEDS_HUMAN_LABEL = "needs-human";
const HUMAN_GATE_LABEL = "human-gate";
const DEFERRED_LABEL = "deferred";
const JIRA_AGENT_MARKER_LABELS = [
  NEEDS_GPU_LABEL,
  NEEDS_HUMAN_LABEL,
  HUMAN_GATE_LABEL,
  DEFERRED_LABEL
];
const MARKERS_SKIPPED_BY_QUEUE = [NEEDS_HUMAN_LABEL, DEFERRED_LABEL];
const MARKERS_REFUSING_TARGETED_CLAIM = [NEEDS_HUMAN_LABEL];
const JIRA_AGENT_MARKER_DESCRIPTIONS = {
  "needs-gpu": "Claimed like any other ticket. Flags that the deliverable needs a Vulkan build and a windowed run, not the headless Null config the gates default to.",
  "needs-human": "Never claimed \u2014 not by the queue, not by a targeted claim. No machine can produce the deliverable; it needs a person.",
  "human-gate": "The loop does the whole job, then stops at In Review instead of Done.",
  deferred: 'Skipped by the queue, still taken by a targeted claim \u2014 "not yet", rather than "not ever".'
};
function isAgentMarkerLabel(name) {
  return JIRA_AGENT_MARKER_LABELS.includes(name);
}
function matchesFilter(issue, filter) {
  if (filter.type !== void 0 && filter.type !== null && issue.type !== filter.type) return false;
  if (filter.priority !== void 0 && filter.priority !== null && issue.priority !== filter.priority) {
    return false;
  }
  if (filter.severity !== void 0 && filter.severity !== null && issue.severity !== filter.severity) {
    return false;
  }
  if (filter.assigneeId !== void 0 && issue.assigneeId !== filter.assigneeId) return false;
  if (filter.categoryId !== void 0 && issue.categoryId !== filter.categoryId) return false;
  if (filter.labelId !== void 0 && filter.labelId !== null) {
    if (!(issue.labelIds ?? []).includes(filter.labelId)) return false;
  }
  if (filter.flagged !== void 0 && issue.flagged !== filter.flagged) return false;
  if (filter.text !== void 0 && filter.text.trim() !== "") {
    if (!issue.title.toLowerCase().includes(filter.text.trim().toLowerCase())) return false;
  }
  return true;
}
function matchesAllFilters(issue, filters) {
  return filters.every((f) => !f || matchesFilter(issue, f));
}
function boardFilterError(value) {
  if (value === null || value === void 0) return null;
  if (typeof value !== "object" || Array.isArray(value)) return "A filter must be an object.";
  const known = /* @__PURE__ */ new Set([
    "type",
    "priority",
    "severity",
    "assigneeId",
    "categoryId",
    "labelId",
    "flagged",
    "text"
  ]);
  for (const [key, entry] of Object.entries(value)) {
    if (!known.has(key)) {
      return `"${key}" is not a filter field \u2014 expected one of ${[...known].join(", ")}.`;
    }
    if (key === "flagged") {
      if (typeof entry !== "boolean" && entry !== void 0) return "`flagged` must be a boolean.";
      continue;
    }
    if (key === "text") {
      if (typeof entry !== "string" && entry !== void 0) return "`text` must be a string.";
      continue;
    }
    if (entry !== null && entry !== void 0 && typeof entry !== "string") {
      return `\`${key}\` must be a string or null.`;
    }
  }
  return null;
}
function isOverWipLimit(issueCount, wipLimit) {
  if (wipLimit === null || wipLimit === void 0) return false;
  return issueCount > wipLimit;
}
function resolveDropStatus(column, currentStatusId, legalTargets) {
  if (column.statusIds.includes(currentStatusId)) return currentStatusId;
  for (const statusId of column.statusIds) {
    if (legalTargets.includes(statusId)) return statusId;
  }
  return null;
}
function unmappedStatusIds(columns, allStatusIds) {
  const mapped = new Set(columns.flatMap((c) => c.statusIds));
  return allStatusIds.filter((id) => !mapped.has(id));
}
function swimlanesFor(mode, issues, names) {
  if (mode === "NONE") return [];
  const key = (issue) => mode === "EPIC" ? issue.epicId : issue.assigneeId;
  const seen = /* @__PURE__ */ new Set();
  let hasNull = false;
  for (const issue of issues) {
    const value = key(issue);
    if (value === null || value === void 0) hasNull = true;
    else seen.add(value);
  }
  const lanes = [...seen].map((id) => ({ id, name: names[id] ?? id })).sort((a, b) => a.name.localeCompare(b.name));
  if (hasNull) {
    lanes.push({ id: "", name: mode === "EPIC" ? "No epic" : "Unassigned" });
  }
  return lanes;
}
function issueInSwimlane(mode, issue, laneId) {
  if (mode === "NONE") return true;
  const value = mode === "EPIC" ? issue.epicId : issue.assigneeId;
  return (value ?? "") === laneId;
}
export {
  CURRENT_SPRINT_SUFFIX,
  DEFAULT_CARD_FIELDS,
  DEFERRED_LABEL,
  HUMAN_GATE_LABEL,
  JIRA_AGENT_MARKER_DESCRIPTIONS,
  JIRA_AGENT_MARKER_LABELS,
  JIRA_BOARD_TYPES,
  JIRA_CARD_FIELDS,
  JIRA_CARD_FIELD_LABELS,
  JIRA_SPRINT_STATUSES,
  JIRA_SWIMLANE_LABELS,
  JIRA_SWIMLANE_MODES,
  MARKERS_REFUSING_TARGETED_CLAIM,
  MARKERS_SKIPPED_BY_QUEUE,
  NEEDS_GPU_LABEL,
  NEEDS_HUMAN_LABEL,
  blockedByKeys,
  blockedByTooltip,
  boardFilterError,
  boardShowsOnlyActiveSprint,
  isAgentMarkerLabel,
  isOverWipLimit,
  issueInSwimlane,
  matchesAllFilters,
  matchesFilter,
  normalizeCardFields,
  resolveDropStatus,
  sprintDisplayName,
  swimlanesFor,
  unmappedStatusIds
};
