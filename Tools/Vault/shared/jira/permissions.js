// Ported from SaaS packages/lib/src/permissions.ts; see provenance.json.
import { orgRoleRank } from "./workflow.js";
const JIRA_PERMISSIONS = [
  "BROWSE_PROJECT",
  "CREATE_ISSUE",
  "EDIT_ISSUE",
  "DELETE_ISSUE",
  "TRANSITION_ISSUE",
  "ASSIGN_ISSUE",
  "ADD_COMMENTS",
  "MANAGE_SPRINTS",
  "MANAGE_VERSIONS",
  "ADMINISTER_PROJECT"
];
const JIRA_PERMISSION_LABELS = {
  BROWSE_PROJECT: "Browse project",
  CREATE_ISSUE: "Create issues",
  EDIT_ISSUE: "Edit issues",
  DELETE_ISSUE: "Delete issues",
  TRANSITION_ISSUE: "Transition issues",
  ASSIGN_ISSUE: "Assign issues",
  ADD_COMMENTS: "Add comments",
  MANAGE_SPRINTS: "Manage sprints",
  MANAGE_VERSIONS: "Manage versions",
  ADMINISTER_PROJECT: "Administer project"
};
const JIRA_GRANT_TYPES = [
  "PROJECT_ROLE",
  "ORG_ROLE",
  "ANY_LOGGED_IN",
  "ASSIGNEE",
  "REPORTER"
];
const JIRA_GRANT_TYPE_LABELS = {
  PROJECT_ROLE: "Project role",
  ORG_ROLE: "Organisation role (at least)",
  ANY_LOGGED_IN: "Anybody signed in",
  ASSIGNEE: "The assignee",
  REPORTER: "The reporter"
};
const DEFAULT_PROJECT_ROLES = ["Administrators", "Developers", "Viewers"];
function grantSatisfied(grant, ctx) {
  switch (grant.grantType) {
    case "PROJECT_ROLE":
      return grant.value !== null && ctx.projectRoleIds.includes(grant.value);
    case "ORG_ROLE": {
      if (grant.value === null) return false;
      return orgRoleRank(ctx.orgRole) >= orgRoleRank(grant.value);
    }
    case "ANY_LOGGED_IN":
      return true;
    case "ASSIGNEE":
      return Boolean(ctx.issue) && ctx.issue.assigneeId === ctx.userId;
    case "REPORTER":
      return Boolean(ctx.issue) && ctx.issue.reporterId === ctx.userId;
    default:
      return false;
  }
}
function permissionGrantError(grant) {
  if (!JIRA_PERMISSIONS.includes(grant.permission)) {
    return `"${grant.permission}" is not a permission.`;
  }
  if (!JIRA_GRANT_TYPES.includes(grant.grantType)) {
    return `"${grant.grantType}" is not a grant type.`;
  }
  if (grant.grantType === "PROJECT_ROLE" && !grant.value) {
    return "A PROJECT_ROLE grant needs a role.";
  }
  if (grant.grantType === "ORG_ROLE") {
    if (!grant.value) return "An ORG_ROLE grant needs a role.";
    if (orgRoleRank(grant.value) < 0) {
      return `"${grant.value}" is not an organisation role.`;
    }
  }
  return null;
}
function defaultPermission(permission, orgRole) {
  const isAdmin = orgRoleRank(orgRole) >= orgRoleRank("ADMIN");
  const canWrite = orgRoleRank(orgRole) >= orgRoleRank("MEMBER");
  switch (permission) {
    // A VIEWER exists precisely to read a project.
    case "BROWSE_PROJECT":
      return true;
    case "ADMINISTER_PROJECT":
    case "MANAGE_VERSIONS":
      return isAdmin;
    default:
      return canWrite;
  }
}
function resolvePermission(permission, grants, ctx) {
  if (grants === null) return defaultPermission(permission, ctx.orgRole);
  return grants.filter((g) => g.permission === permission).some((grant) => grantSatisfied(grant, ctx));
}
export {
  DEFAULT_PROJECT_ROLES,
  JIRA_GRANT_TYPES,
  JIRA_GRANT_TYPE_LABELS,
  JIRA_PERMISSIONS,
  JIRA_PERMISSION_LABELS,
  defaultPermission,
  grantSatisfied,
  permissionGrantError,
  resolvePermission
};
