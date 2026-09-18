// Ported from SaaS packages/lib/src/workflow.ts; see provenance.json.
import { JIRA_RESOLUTIONS } from "./constants.js";
const JIRA_ORG_ROLES = ["VIEWER", "MEMBER", "ADMIN", "OWNER"];
function orgRoleRank(role) {
  return JIRA_ORG_ROLES.indexOf(role);
}
const TRANSITION_RULE_KINDS = ["CONDITION", "VALIDATOR", "POST_FUNCTION"];
const TRANSITION_CONDITIONS = [
  "ORG_ROLE",
  "PROJECT_ROLE",
  "ASSIGNEE_ONLY",
  "REPORTER_ONLY"
];
const TRANSITION_VALIDATORS = [
  "FIELD_REQUIRED",
  "NO_OPEN_BLOCKERS",
  "NO_UNRESOLVED_CHILDREN",
  "COMMENT_REQUIRED"
];
const TRANSITION_POST_FUNCTIONS = [
  "SET_RESOLUTION",
  "CLEAR_RESOLUTION",
  "ASSIGN_TO_ACTOR",
  "CLEAR_ASSIGNEE",
  "SET_FLAGGED"
];
const TRANSITION_RULE_TYPES = {
  CONDITION: TRANSITION_CONDITIONS,
  VALIDATOR: TRANSITION_VALIDATORS,
  POST_FUNCTION: TRANSITION_POST_FUNCTIONS
};
const TRANSITION_REQUIRED_FIELDS = [
  "assignee",
  "storyPoints",
  "dueDate",
  "severity",
  "reproducibility",
  "environment",
  "category",
  "description"
];
const TRANSITION_REQUIRED_FIELD_LABELS = {
  assignee: "Assignee",
  storyPoints: "Story points",
  dueDate: "Due date",
  severity: "Severity",
  reproducibility: "Reproducibility",
  environment: "Environment",
  category: "Category",
  description: "Description"
};
const TRANSITION_RULE_LABELS = {
  ORG_ROLE: "Organisation role is at least",
  PROJECT_ROLE: "Holds project role",
  ASSIGNEE_ONLY: "Only the assignee",
  REPORTER_ONLY: "Only the reporter",
  FIELD_REQUIRED: "Field must be set",
  NO_OPEN_BLOCKERS: "No unfinished blockers",
  NO_UNRESOLVED_CHILDREN: "No unfinished children",
  COMMENT_REQUIRED: "At least one comment",
  SET_RESOLUTION: "Set resolution",
  CLEAR_RESOLUTION: "Clear resolution",
  ASSIGN_TO_ACTOR: "Assign to whoever transitions it",
  CLEAR_ASSIGNEE: "Clear the assignee",
  SET_FLAGGED: "Set the flag"
};
function transitionRuleError(rule) {
  const types = TRANSITION_RULE_TYPES[rule.kind];
  if (!types) return `Unknown rule kind "${rule.kind}".`;
  if (!types.includes(rule.type)) {
    return `"${rule.type}" is not a valid ${rule.kind} \u2014 expected one of ${types.join(", ")}.`;
  }
  const config = rule.config ?? {};
  switch (rule.type) {
    case "ORG_ROLE": {
      const role = config.role;
      if (typeof role !== "string" || !JIRA_ORG_ROLES.includes(role)) {
        return `ORG_ROLE needs a role from ${JIRA_ORG_ROLES.join(", ")}.`;
      }
      return null;
    }
    case "PROJECT_ROLE": {
      if (typeof config.roleId !== "string" || config.roleId.length === 0) {
        return "PROJECT_ROLE needs a roleId.";
      }
      return null;
    }
    case "FIELD_REQUIRED": {
      const field = config.field;
      const customFieldId = config.customFieldId;
      const hasBuiltin = field !== void 0 && field !== null;
      const hasCustom = customFieldId !== void 0 && customFieldId !== null;
      if (hasBuiltin && hasCustom) {
        return "FIELD_REQUIRED names a built-in field or a custom field, not both.";
      }
      if (hasCustom) {
        return typeof customFieldId === "string" && customFieldId.length > 0 ? null : "FIELD_REQUIRED needs a custom field id.";
      }
      if (typeof field !== "string" || !TRANSITION_REQUIRED_FIELDS.includes(field)) {
        return `FIELD_REQUIRED needs a field from ${TRANSITION_REQUIRED_FIELDS.join(", ")}.`;
      }
      return null;
    }
    case "SET_RESOLUTION": {
      const resolution = config.resolution;
      if (typeof resolution !== "string" || !JIRA_RESOLUTIONS.includes(resolution)) {
        return `SET_RESOLUTION needs a resolution from ${JIRA_RESOLUTIONS.join(", ")}.`;
      }
      return null;
    }
    case "SET_FLAGGED": {
      if (typeof config.flagged !== "boolean") return "SET_FLAGGED needs a boolean `flagged`.";
      return null;
    }
    default:
      return null;
  }
}
function fieldValue(issue, field) {
  switch (field) {
    case "assignee":
      return issue.assigneeId;
    case "storyPoints":
      return issue.storyPoints;
    case "dueDate":
      return issue.dueDate;
    case "severity":
      return issue.severity;
    case "reproducibility":
      return issue.reproducibility;
    case "environment":
      return issue.environment;
    case "category":
      return issue.categoryId;
    case "description":
      return issue.description;
    default:
      return void 0;
  }
}
function conditionError(rule, ctx) {
  switch (rule.type) {
    case "ORG_ROLE": {
      const required = rule.config.role;
      if (orgRoleRank(ctx.actor.orgRole) < orgRoleRank(required)) {
        return `Requires the ${required} role or higher.`;
      }
      return null;
    }
    case "PROJECT_ROLE": {
      const roleId = rule.config.roleId;
      if (!ctx.actor.projectRoleIds.includes(roleId)) {
        return "Requires a project role you do not hold.";
      }
      return null;
    }
    case "ASSIGNEE_ONLY":
      return ctx.issue.assigneeId === ctx.actor.id ? null : "Only the assignee can make this move.";
    case "REPORTER_ONLY":
      return ctx.issue.reporterId === ctx.actor.id ? null : "Only the reporter can make this move.";
    default:
      return null;
  }
}
function validatorError(rule, ctx) {
  switch (rule.type) {
    case "FIELD_REQUIRED": {
      const customFieldId = rule.config.customFieldId;
      if (customFieldId) {
        const values = ctx.issue.customFieldValues?.[customFieldId] ?? [];
        if (values.length === 0) {
          const name = ctx.customFieldNames?.[customFieldId] ?? "That field";
          return `${name} must be set first.`;
        }
        return null;
      }
      const field = rule.config.field;
      const value = fieldValue(ctx.issue, field);
      if (value === null || value === void 0 || value === "") {
        return `${TRANSITION_REQUIRED_FIELD_LABELS[field] ?? field} must be set first.`;
      }
      return null;
    }
    case "NO_OPEN_BLOCKERS": {
      const open = ctx.issue.openBlockerKeys;
      if (open.length === 0) return null;
      return `Blocked by ${open.join(", ")} \u2014 finish ${open.length === 1 ? "it" : "those"} first.`;
    }
    case "NO_UNRESOLVED_CHILDREN": {
      const open = ctx.issue.unresolvedChildKeys;
      if (open.length === 0) return null;
      return `${open.join(", ")} ${open.length === 1 ? "is" : "are"} not finished yet.`;
    }
    case "COMMENT_REQUIRED":
      return ctx.issue.commentCount > 0 ? null : "A comment is required before this move.";
    default:
      return null;
  }
}
function evaluateTransition(transition, ctx) {
  let condition = null;
  let validator = null;
  for (const rule of transition.rules) {
    if (rule.kind === "CONDITION" && condition === null) condition = conditionError(rule, ctx);
    if (rule.kind === "VALIDATOR" && validator === null) validator = validatorError(rule, ctx);
  }
  return { transition, conditionError: condition, validatorError: validator };
}
function transitionError(transition, ctx) {
  const verdict = evaluateTransition(transition, ctx);
  return verdict.conditionError ?? verdict.validatorError;
}
function transitionsFrom(defs, fromStatusId) {
  return defs.filter(
    (t) => (t.fromStatusId === fromStatusId || t.fromStatusId === null) && t.toStatusId !== fromStatusId
  );
}
function findTransition(defs, fromStatusId, toStatusId) {
  const candidates = transitionsFrom(defs, fromStatusId).filter((t) => t.toStatusId === toStatusId);
  if (candidates.length === 0) return null;
  return candidates.find((t) => t.fromStatusId === fromStatusId) ?? candidates[0];
}
function availableTransitions(defs, fromStatusId, ctx) {
  const seen = /* @__PURE__ */ new Set();
  const out = [];
  for (const t of transitionsFrom(defs, fromStatusId)) {
    if (seen.has(t.toStatusId)) continue;
    seen.add(t.toStatusId);
    const chosen = findTransition(defs, fromStatusId, t.toStatusId);
    const verdict = evaluateTransition(chosen, ctx);
    if (verdict.conditionError) continue;
    out.push(verdict);
  }
  return out;
}
function legalTargetStatusIds(defs, fromStatusId, ctx) {
  return availableTransitions(defs, fromStatusId, ctx).filter((v) => v.validatorError === null).map((v) => v.transition.toStatusId);
}
const DEFAULT_RESOLUTION = "DONE";
function transitionNeedsResolution(transition, targetCategory) {
  if (targetCategory !== "DONE") return false;
  return !transition.rules.some((r) => r.kind === "POST_FUNCTION" && r.type === "SET_RESOLUTION");
}
function transitionNeedsScreen(transition) {
  return Boolean(transition.screenId);
}
function resolutionInvariantError(category, resolution) {
  if (category === "DONE" && resolution === null) {
    return "An issue in a Done status must carry a resolution.";
  }
  if (category !== "DONE" && resolution !== null) {
    return "Only an issue in a Done status may carry a resolution.";
  }
  return null;
}
function applyTransition(transition, targetCategory, ctx, options = {}) {
  const patch = { statusId: transition.toStatusId };
  const error = transitionError(transition, ctx);
  if (error) return { patch, error };
  let resolution = ctx.issue.resolution;
  let resolutionDecided = false;
  for (const rule of transition.rules) {
    if (rule.kind !== "POST_FUNCTION") continue;
    switch (rule.type) {
      case "SET_RESOLUTION":
        resolution = rule.config.resolution;
        resolutionDecided = true;
        break;
      case "CLEAR_RESOLUTION":
        resolution = null;
        resolutionDecided = true;
        break;
      case "ASSIGN_TO_ACTOR":
        patch.assigneeId = ctx.actor.id;
        break;
      case "CLEAR_ASSIGNEE":
        patch.assigneeId = null;
        break;
      case "SET_FLAGGED":
        patch.flagged = rule.config.flagged;
        break;
      default:
        break;
    }
  }
  if (targetCategory === "DONE") {
    if (!resolutionDecided) {
      const choice = options.resolutionChoice;
      if (typeof choice === "string") {
        if (!JIRA_RESOLUTIONS.includes(choice)) {
          return { patch, error: `"${choice}" is not a resolution.` };
        }
        resolution = choice;
      }
    }
    resolution = resolution ?? DEFAULT_RESOLUTION;
  } else {
    resolution = null;
  }
  if (resolution !== ctx.issue.resolution) patch.resolution = resolution;
  return { patch, error: null };
}
export {
  DEFAULT_RESOLUTION,
  JIRA_ORG_ROLES,
  TRANSITION_CONDITIONS,
  TRANSITION_POST_FUNCTIONS,
  TRANSITION_REQUIRED_FIELDS,
  TRANSITION_REQUIRED_FIELD_LABELS,
  TRANSITION_RULE_KINDS,
  TRANSITION_RULE_LABELS,
  TRANSITION_RULE_TYPES,
  TRANSITION_VALIDATORS,
  applyTransition,
  availableTransitions,
  conditionError,
  evaluateTransition,
  findTransition,
  legalTargetStatusIds,
  orgRoleRank,
  resolutionInvariantError,
  transitionError,
  transitionNeedsResolution,
  transitionNeedsScreen,
  transitionRuleError,
  transitionsFrom,
  validatorError
};
