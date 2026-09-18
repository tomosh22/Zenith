// Ported from SaaS packages/lib/src/constants.ts; see provenance.json.
const legacyEnvironment = typeof process === 'undefined' ? {} : process.env;
const APP_NAME = "SaaS Suite";
const PRODUCTS = {
  web: {
    name: "Home",
    description: "Dashboard & Settings",
    url: legacyEnvironment.NEXT_PUBLIC_WEB_URL ?? "http://localhost:3000",
    icon: "Home"
  },
  jira: {
    name: "Projects",
    description: "Project Management",
    url: legacyEnvironment.NEXT_PUBLIC_JIRA_URL ?? "http://localhost:3001",
    icon: "KanbanSquare"
  },
  figma: {
    name: "Design",
    description: "Collaborative Design",
    url: legacyEnvironment.NEXT_PUBLIC_FIGMA_URL ?? "http://localhost:3002",
    icon: "Pen"
  },
  mermaid: {
    name: "Diagrams",
    description: "Diagram Editor",
    url: legacyEnvironment.NEXT_PUBLIC_MERMAID_URL ?? "http://localhost:3003",
    icon: "GitFork"
  },
  notion: {
    name: "Docs",
    description: "Documents & Notes",
    url: legacyEnvironment.NEXT_PUBLIC_NOTION_URL ?? "http://localhost:3004",
    icon: "FileText"
  }
};
function issueUrl(orgSlug, projectKey, issueNumber) {
  return `${PRODUCTS.jira.url}/${orgSlug}/project/${projectKey}/issue/${issueNumber}`;
}
function pageUrl(orgSlug, pageId) {
  return `${PRODUCTS.notion.url}/${orgSlug}/page/${pageId}`;
}
function diagramUrl(orgSlug, diagramId) {
  return `${PRODUCTS.mermaid.url}/${orgSlug}/diagram/${diagramId}`;
}
function databaseUrl(orgSlug, databaseId) {
  return `${PRODUCTS.notion.url}/${orgSlug}/database/${databaseId}`;
}
const MEMBERSHIP_ROLES = ["OWNER", "ADMIN", "MEMBER", "VIEWER"];
const JIRA_STATUS_CATEGORIES = ["TODO", "IN_PROGRESS", "DONE"];
const DEFAULT_JIRA_STATUS_CATEGORY = JIRA_STATUS_CATEGORIES[0];
const DEFAULT_JIRA_STATUSES = [
  { name: "To Do", category: "TODO" },
  { name: "In Progress", category: "IN_PROGRESS" },
  { name: "In Review", category: "IN_PROGRESS" },
  { name: "Done", category: "DONE" },
  { name: "Blocked", category: "TODO" }
];
const DEFAULT_JIRA_TRANSITIONS = [
  { name: "Start progress", from: "To Do", to: "In Progress" },
  { name: "Stop progress", from: "In Progress", to: "To Do" },
  { name: "Ready for review", from: "In Progress", to: "In Review" },
  { name: "Back to progress", from: "In Review", to: "In Progress" },
  { name: "Reopen", from: "Done", to: "To Do" },
  { name: "Reopen", from: "Done", to: "In Progress" },
  { name: "Unblock", from: "Blocked", to: "To Do" },
  { name: "Unblock", from: "Blocked", to: "In Progress" },
  // The two globals, legal from anywhere including statuses added later.
  //
  // Jira's own templates carry an "All → Done" transition, and a board
  // where a card cannot be dragged from To Do straight to Done is a
  // PARITY REGRESSION rather than a stricter workflow — finishing
  // something small without first starting it is an ordinary gesture.
  // (The e2e suite found this: `agent-loop.spec.ts` releases its
  // tickets with `move <KEY> Done`, which silently failed from To Do
  // and left the repo busy for every following spec.)
  //
  // A project that wants a rule on ONE Done edge adds that edge in the
  // workflow editor: `findTransition` prefers an exact edge over a
  // global one, so the specific rules win where they exist.
  { name: "Done", from: null, to: "Done" },
  { name: "Block", from: null, to: "Blocked" }
];
const JIRA_ISSUE_TYPES = ["EPIC", "STORY", "TASK", "BUG", "SUBTASK"];
const JIRA_PRIORITIES = ["CRITICAL", "HIGH", "MEDIUM", "LOW", "TRIVIAL"];
const JIRA_COMPLEXITIES = ["TRIVIAL", "SIMPLE", "MODERATE", "COMPLEX"];
const JIRA_RISKS = ["LOW", "MEDIUM", "HIGH"];
const DEFAULT_JIRA_ISSUE_VALUES = {
  type: "TASK",
  priority: "MEDIUM",
  complexity: "MODERATE",
  risk: "LOW"
};
const JIRA_LINK_TYPES = ["BLOCKS", "RELATES_TO", "DUPLICATES", "CAUSES"];
const JIRA_RESOLUTIONS = [
  "DONE",
  "WONT_DO",
  "DUPLICATE",
  "CANNOT_REPRODUCE",
  "DECLINED"
];
const JIRA_SEVERITIES = ["S1_CRITICAL", "S2_MAJOR", "S3_MINOR", "S4_TRIVIAL"];
const JIRA_REPRODUCIBILITIES = [
  "ALWAYS",
  "INTERMITTENT",
  "ONCE",
  "CANNOT_REPRODUCE"
];
const JIRA_VERSION_STATUSES = ["UNRELEASED", "RELEASED", "ARCHIVED"];
const JIRA_VERSION_KINDS = ["FIX", "AFFECTS"];
const JIRA_LINK_INVERSE_LABELS = {
  BLOCKS: "is blocked by",
  RELATES_TO: "relates to",
  DUPLICATES: "is duplicated by",
  CAUSES: "is caused by"
};
const JIRA_LINK_LABELS = {
  BLOCKS: "blocks",
  RELATES_TO: "relates to",
  DUPLICATES: "duplicates",
  CAUSES: "causes"
};
const JIRA_LINK_INVERSE_PAST_LABELS = {
  BLOCKS: "was blocked by"
};
const JIRA_STORY_POINTS = [1, 2, 3, 5, 8, 13, 21];
const HIERARCHY_LEVEL = {
  EPIC: 0,
  STORY: 1,
  TASK: 1,
  BUG: 1,
  SUBTASK: 2
};
function issueParentError(childType, parentType) {
  const child = HIERARCHY_LEVEL[childType];
  const parent = HIERARCHY_LEVEL[parentType];
  if (parent === 0 && child === 0) {
    return "An epic cannot be inside another epic.";
  }
  if (child <= parent) {
    return `A ${childType} cannot be a child of a ${parentType} \u2014 the hierarchy is EPIC > STORY/TASK/BUG > SUBTASK.`;
  }
  if (child - parent !== 1) {
    return `A ${childType} cannot hang directly off a ${parentType}; it needs an intermediate STORY, TASK or BUG.`;
  }
  return null;
}
function issueLinkError(sourceKey, targetKey) {
  if (sourceKey === targetKey) {
    return `${sourceKey} cannot be linked to itself.`;
  }
  return null;
}
export {
  APP_NAME,
  DEFAULT_JIRA_ISSUE_VALUES,
  DEFAULT_JIRA_STATUSES,
  DEFAULT_JIRA_STATUS_CATEGORY,
  DEFAULT_JIRA_TRANSITIONS,
  JIRA_COMPLEXITIES,
  JIRA_ISSUE_TYPES,
  JIRA_LINK_INVERSE_LABELS,
  JIRA_LINK_INVERSE_PAST_LABELS,
  JIRA_LINK_LABELS,
  JIRA_LINK_TYPES,
  JIRA_PRIORITIES,
  JIRA_REPRODUCIBILITIES,
  JIRA_RESOLUTIONS,
  JIRA_RISKS,
  JIRA_SEVERITIES,
  JIRA_STATUS_CATEGORIES,
  JIRA_STORY_POINTS,
  JIRA_VERSION_KINDS,
  JIRA_VERSION_STATUSES,
  MEMBERSHIP_ROLES,
  PRODUCTS,
  databaseUrl,
  diagramUrl,
  issueLinkError,
  issueParentError,
  issueUrl,
  pageUrl
};
