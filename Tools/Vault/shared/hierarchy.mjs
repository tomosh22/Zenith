// The "is this a recognised hierarchy type" guard shared by every
// `issueParentError` call site. `shared/jira/constants.js` is vendored
// verbatim (see shared/jira/provenance.json), so this lives beside it rather
// than inside it: `issueParentError` mishandles a missing/unknown type
// (`HIERARCHY_LEVEL[undefined]` is `undefined`, so it reports a hierarchy
// violation that does not exist), so callers must consult it only when BOTH
// the child and parent types are recognised.
export const KNOWN_HIERARCHY_TYPES = new Set(['EPIC', 'STORY', 'TASK', 'BUG', 'SUBTASK']);

export function isKnownHierarchyType(value) {
  return KNOWN_HIERARCHY_TYPES.has(String(value ?? '').toUpperCase());
}
