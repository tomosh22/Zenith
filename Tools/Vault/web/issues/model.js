/*
 * The issue UI speaks the Vault command protocol rather than reaching into
 * the filesystem.  This file contains the small amount of shape tolerance
 * needed while exported ticket frontmatter and hand-authored fixtures are
 * both in use.  The command builders are deliberately pure so browser and
 * unit tests can verify the exact CLI spelling without starting a server.
 */

import {
  JIRA_COMPLEXITIES,
  JIRA_ISSUE_TYPES,
  JIRA_LINK_INVERSE_LABELS,
  JIRA_LINK_LABELS,
  JIRA_PRIORITIES,
  JIRA_REPRODUCIBILITIES,
  JIRA_RESOLUTIONS,
  JIRA_RISKS,
  JIRA_SEVERITIES,
  JIRA_STORY_POINTS,
  issueLinkError,
  issueParentError,
} from '../../shared/jira/constants.js';
import { isKnownHierarchyType } from '../../shared/hierarchy.mjs';
import {
  fieldValueError,
  resolveFieldLayout,
} from '../../shared/jira/fields.js';
import {
  availableTransitions,
  transitionNeedsResolution,
  transitionNeedsScreen,
} from '../../shared/jira/workflow.js';
import { extractMentions } from '../../shared/jira/utils/mentions.js';
import { extractReferences } from '../../shared/jira/utils/references.js';
import { withLockRetry } from '../project-api.js';

// Keep the public names used by the Vault UI, while taking the actual domain
// vocabulary from the shared Jira implementation.  The legacy
// reproducibility spellings are accepted for hand-authored/imported tickets;
// the writer remains the final validator for a persisted value.
export const ISSUE_TYPES = Object.freeze([...JIRA_ISSUE_TYPES]);
export const PRIORITIES = Object.freeze([...JIRA_PRIORITIES]);
export const COMPLEXITIES = Object.freeze([...JIRA_COMPLEXITIES]);
export const RISKS = Object.freeze([...JIRA_RISKS]);
export const RESOLUTIONS = Object.freeze([...JIRA_RESOLUTIONS]);
export const SEVERITIES = Object.freeze([...JIRA_SEVERITIES]);
export const REPRODUCIBILITIES = Object.freeze([...new Set([
  ...JIRA_REPRODUCIBILITIES,
  'SOMETIMES',
  'RARELY',
  'UNABLE',
])]);
export const LINK_TYPES = Object.freeze([
  { value: 'blocks', label: 'blocks', inverse: 'is blocked by', type: 'BLOCKS' },
  { value: 'blocked-by', label: 'is blocked by', inverse: 'blocks', type: 'BLOCKS' },
  { value: 'relates-to', label: 'relates to', inverse: 'relates to', type: 'RELATES_TO' },
  { value: 'duplicates', label: 'duplicates', inverse: 'is duplicated by', type: 'DUPLICATES' },
  { value: 'causes', label: 'causes', inverse: 'is caused by', type: 'CAUSES' },
]);

export const STORY_POINTS = Object.freeze([...new Set(['0', ...JIRA_STORY_POINTS.map(String)])]);

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value : [];
}

function list(value) {
  return Array.isArray(value) ? value : value === undefined || value === null || value === '' ? [] : [value];
}

export function issueMetadata(issue) {
  return object(issue?.metadata ?? issue?.fields ?? issue);
}

export function issueValue(issue, name, aliases = []) {
  const metadata = issueMetadata(issue);
  const names = [name, ...aliases];
  for (const candidate of names) {
    if (issue && issue[candidate] !== undefined) return issue[candidate];
    if (metadata[candidate] !== undefined) return metadata[candidate];
  }
  return undefined;
}

export function issueKeyOf(issue, fallback = '') {
  return String(issueValue(issue, 'key', ['issueKey']) ?? fallback ?? '').trim();
}

export function optionId(option, fallback = '') {
  if (option && typeof option === 'object') {
    return String(option.id ?? option.key ?? option.slug ?? option.value ?? option.name ?? fallback);
  }
  return String(option ?? fallback);
}

export function optionName(option, fallback = '') {
  if (option && typeof option === 'object') {
    return String(option.name ?? option.label ?? option.title ?? option.value ?? option.key ?? option.slug ?? option.id ?? fallback);
  }
  return String(option ?? fallback);
}

export function optionEmail(option, fallback = '') {
  if (option && typeof option === 'object') {
    return String(option.email ?? option.username ?? option.name ?? option.id ?? fallback);
  }
  return String(option ?? fallback);
}

export function optionKey(option, fallback = '') {
  if (option && typeof option === 'object') {
    return String(option.key ?? option.issueKey ?? option.name ?? option.slug ?? option.id ?? fallback);
  }
  return String(option ?? fallback);
}

export function catalogOptions(values) {
  return array(values).map((value, index) => ({
    ...object(value),
    id: optionId(value, `option-${index + 1}`),
    name: optionName(value, `Option ${index + 1}`),
  }));
}

/**
 * Return the project's custom-field definitions in the shape consumed by
 * `resolveFieldLayout`.  Exported snapshots have used both `options` and
 * `values`, and older hand-authored fixtures sometimes use option objects;
 * normalising here keeps the renderer and validator on one vocabulary.
 */
export function customFieldDefinitions(project) {
  const source = project?.customFields
    ?? project?.fields?.customFields
    ?? project?.fieldConfiguration?.customFields
    ?? [];
  return array(source).map((value, index) => {
    const row = object(value);
    const id = String(row.id ?? row.key ?? row.slug ?? `custom-field-${index + 1}`);
    const type = String(row.type ?? row.fieldType ?? 'TEXT').toUpperCase();
    const options = array(row.options ?? row.values ?? row.choices)
      .map(option => optionName(option))
      .filter(Boolean);
    return {
      ...row,
      id,
      name: String(row.name ?? row.label ?? row.key ?? id),
      type,
      options,
    };
  }).filter(field => field.id);
}

function normaliseFieldReference(value) {
  if (typeof value === 'string') {
    return value.startsWith('custom:')
      ? { builtinField: null, customFieldId: value.slice('custom:'.length) }
      : { builtinField: value, customFieldId: null };
  }
  const row = object(value);
  const nested = object(row.ref ?? row.fieldRef ?? row.field);
  // A string under `field` gets the same `custom:` prefix split as a
  // top-level string reference.
  const fieldRef = typeof row.field === 'string' ? normaliseFieldReference(row.field) : null;
  const builtin = row.builtinField
    ?? row.builtin_field
    ?? row.builtin
    ?? fieldRef?.builtinField
    ?? nested.builtinField
    ?? nested.builtin_field
    ?? nested.name;
  const custom = row.customFieldId
    ?? row.custom_field_id
    ?? row.fieldId
    ?? row.custom
    ?? fieldRef?.customFieldId
    ?? nested.customFieldId
    ?? nested.custom_field_id
    ?? nested.id;
  return {
    builtinField: builtin === undefined || builtin === null || typeof builtin === 'object' ? null : String(builtin),
    customFieldId: custom === undefined || custom === null || typeof custom === 'object' ? null : String(custom),
  };
}

function normaliseFieldRule(value, issueTypeHint = null) {
  const row = object(value);
  const ref = normaliseFieldReference(row);
  const requirement = String(row.requirement ?? row.visibility ?? row.mode ?? 'OPTIONAL').toUpperCase();
  return {
    ...row,
    issueType: String(row.issueType ?? row.issue_type ?? row.type ?? issueTypeHint ?? '').toUpperCase(),
    builtinField: ref.builtinField,
    customFieldId: ref.customFieldId,
    requirement,
  };
}

/** Normalise the few field-configuration encodings produced by old exports. */
export function fieldConfigurationRules(project) {
  const source = project?.fieldConfiguration
    ?? project?.fieldConfig
    ?? project?.fields?.fieldConfiguration
    ?? [];
  if (Array.isArray(source)) return source.map(rule => normaliseFieldRule(rule));
  if (Array.isArray(source?.rules)) return source.rules.map(rule => normaliseFieldRule(rule));
  if (Array.isArray(source?.fields)) return source.fields.map(rule => normaliseFieldRule(rule));

  const rules = [];
  for (const [issueType, value] of Object.entries(object(source))) {
    if (issueType === 'customFields' || issueType === 'rules' || issueType === 'fields') continue;
    if (Array.isArray(value)) {
      for (const rule of value) rules.push(normaliseFieldRule(rule, issueType));
      continue;
    }
    const config = object(value);
    for (const [builtinField, requirement] of Object.entries(object(config.builtin ?? config.builtins))) {
      rules.push(normaliseFieldRule({ builtinField, requirement }, issueType));
    }
    for (const [customFieldId, requirement] of Object.entries(object(config.custom ?? config.customFields))) {
      rules.push(normaliseFieldRule({ customFieldId, requirement }, issueType));
    }
  }
  return rules;
}

export function screenFieldReferences(project, screen) {
  if (!screen) return undefined;
  const source = typeof screen === 'object'
    ? screen
    : array(project?.screens).find(row => optionId(row) === String(screen));
  const fields = array(source?.fields ?? source?.fieldRefs ?? source?.screenFields ?? source);
  return fields.map(normaliseFieldReference).filter(ref => ref.builtinField || ref.customFieldId);
}

/** Resolve visibility and requiredness once for both create and edit forms. */
export function issueFieldLayout(project, issueType, screen = undefined) {
  const customFields = customFieldDefinitions(project);
  return resolveFieldLayout({
    issueType: String(issueType ?? 'TASK').toUpperCase(),
    customFields,
    config: fieldConfigurationRules(project),
    screen: screenFieldReferences(project, screen),
  });
}

export function fieldVisible(layout, builtinField) {
  return layout.some(field => field.key === `builtin:${builtinField}`);
}

export function fieldRequired(layout, ref) {
  const key = typeof ref === 'string'
    ? (ref.startsWith('custom:') ? ref : `builtin:${ref}`)
    : `${ref?.customFieldId ? 'custom' : 'builtin'}:${ref?.customFieldId ?? ref?.builtinField}`;
  return layout.find(field => field.key === key)?.requirement === 'REQUIRED';
}

function customValuesSource(issue) {
  return issueValue(issue, 'customFieldValues', ['custom_field_values'])
    ?? issueValue(issue, 'customFields');
}

/** Convert object or row-list custom values into id -> string[] values. */
export function normalizeCustomFieldValues(issue) {
  const source = customValuesSource(issue);
  const result = {};
  if (Array.isArray(source)) {
    for (const value of source) {
      const row = object(value);
      const id = row.customFieldId ?? row.fieldId ?? row.id ?? row.key;
      if (id === undefined || id === null) continue;
      // Keep the raw value so null/undefined/''/empty-array all normalise to
      // []; only a non-array scalar wraps into a single-element list.
      const raw = row.values ?? row.value ?? row.selected;
      const selected = Array.isArray(raw) ? raw : (raw === null || raw === undefined || raw === '' ? [] : [raw]);
      result[String(id)] = selected.map(optionName);
    }
    return result;
  }
  for (const [id, value] of Object.entries(object(source))) {
    const raw = object(value).values ?? object(value).value ?? value;
    const selected = Array.isArray(raw) ? raw : (raw === null || raw === undefined || raw === '' ? [] : [raw]);
    result[String(id)] = selected.map(optionName);
  }
  return result;
}

export function customFieldValuesEqual(left, right) {
  const a = normalizeCustomFieldValues({ customFieldValues: left });
  const b = normalizeCustomFieldValues({ customFieldValues: right });
  return JSON.stringify(a) === JSON.stringify(b);
}

export function customFieldErrors(project, issueType, values) {
  const layout = issueFieldLayout(project, issueType);
  const errors = {};
  for (const field of layout) {
    if (!field.custom) continue;
    const list = array(values?.[field.custom.id]).map(value => String(value));
    if (field.requirement === 'REQUIRED' && list.length === 0) {
      errors[field.custom.id] = `${field.label} is required.`;
      continue;
    }
    const error = fieldValueError(field.custom, list);
    if (error) errors[field.custom.id] = error;
  }
  return errors;
}

export function statusOptions(project) {
  const statuses = array(project?.statuses);
  if (statuses.length) {
    return statuses.map((status, index) => ({
      ...object(status),
      id: optionId(status, `status-${index + 1}`),
      name: optionName(status, `Status ${index + 1}`),
      category: String(status?.category ?? status?.statusCategory ?? ''),
      position: Number.isFinite(Number(status?.position)) ? Number(status.position) : index,
    }));
  }
  const workflow = object(project?.workflow);
  return catalogOptions(workflow.statuses ?? workflow.rows).map((status, index) => ({
    ...status,
    category: String(status.category ?? status.statusCategory ?? ''),
    position: index,
  }));
}

export function legalStatusOptions(project, issueOrDraft = null) {
  const all = statusOptions(project);
  const issue = issueOrDraft ? normalizeIssue(issueOrDraft) : null;
  if (!issue) return all;
  const current = all.find(status => status.name.toLowerCase() === String(issue.status ?? '').toLowerCase() || status.id === String(issue.status ?? ''));
  const transitions = array(project?.workflow?.transitions ?? project?.transitions);
  let legal = all;
  if (transitions.length && current) {
    const defs = transitions.map((edge, index) => ({
      ...object(edge),
      id: String(edge?.id ?? edge?.transitionId ?? `transition-${index + 1}`),
      fromStatusId: edge?.fromStatusId ?? edge?.from_status_id ?? edge?.from ?? null,
      toStatusId: edge?.toStatusId ?? edge?.to_status_id ?? edge?.to,
      rules: array(edge?.rules),
    })).filter(edge => edge.toStatusId !== undefined && edge.toStatusId !== null);
    const transitionContext = {
      actor: {
        id: issue.assigneeId ?? issue.assignee ?? '',
        orgRole: String(project?.viewer?.orgRole ?? project?.actor?.orgRole ?? 'OWNER').toUpperCase(),
        projectRoleIds: array(project?.viewer?.projectRoleIds ?? project?.actor?.projectRoleIds),
      },
      issue: {
        ...issue,
        assigneeId: issue.assigneeId ?? issue.assignee ?? null,
        reporterId: issue.reporterId ?? issue.reporter ?? null,
        commentCount: Number(issue.commentCount ?? 0),
        openBlockerKeys: array(issue.openBlockerKeys),
        unresolvedChildKeys: array(issue.unresolvedChildKeys),
        customFieldValues: normalizeCustomFieldValues(issue),
      },
      customFieldNames: Object.fromEntries(customFieldDefinitions(project).map(field => [field.id, field.name])),
    };
    const evaluated = availableTransitions(defs, current.id, transitionContext);
    const targets = evaluated.length
      ? evaluated.map(verdict => verdict.transition.toStatusId)
      : defs.filter(edge => {
        const from = edge.fromStatusId;
        return from === null || from === undefined || String(from) === String(current.id) || String(from).toLowerCase() === current.name.toLowerCase();
      }).map(edge => edge.toStatusId).filter(Boolean);
    if (targets.length) {
      const targetSet = new Set(targets.map(value => String(value).toLowerCase()));
      legal = all.filter(status => status.name.toLowerCase() === current.name.toLowerCase() || targetSet.has(status.id.toLowerCase()) || targetSet.has(status.name.toLowerCase()));
    }
  }
  // The writer's explicit invariant names only "In Progress".  Keep other
  // IN_PROGRESS-category statuses (for example "In Review") available.
  if (issue.type === 'EPIC') legal = legal.filter(status => status.name.toLowerCase() !== 'in progress');
  return legal;
}

export function workflowTransitionFor(project, issueOrDraft, targetStatus) {
  const issue = normalizeIssue(issueOrDraft);
  const statuses = statusOptions(project);
  const current = statuses.find(status => status.name.toLowerCase() === String(issue.status ?? '').toLowerCase() || status.id === String(issue.status ?? ''));
  const target = statuses.find(status => status.name.toLowerCase() === String(targetStatus ?? '').toLowerCase() || status.id === String(targetStatus ?? ''));
  const raw = array(project?.workflow?.transitions ?? project?.transitions);
  if (!current || !target || !raw.length) return null;
  const defs = raw.map((edge, index) => ({
    ...object(edge),
    id: String(edge?.id ?? edge?.transitionId ?? `transition-${index + 1}`),
    fromStatusId: edge?.fromStatusId ?? edge?.from_status_id ?? edge?.from ?? null,
    toStatusId: edge?.toStatusId ?? edge?.to_status_id ?? edge?.to,
    rules: array(edge?.rules),
  })).filter(edge => edge.toStatusId !== undefined && edge.toStatusId !== null);
  const matches = defs.filter(edge => {
    const from = edge.fromStatusId;
    const to = edge.toStatusId;
    const fromMatches = from === null || from === undefined || String(from) === String(current.id) || String(from).toLowerCase() === current.name.toLowerCase();
    const toMatches = String(to) === String(target.id) || String(to).toLowerCase() === target.name.toLowerCase();
    return fromMatches && toMatches;
  });
  const transition = matches.find(edge => String(edge.fromStatusId) === String(current.id)) ?? matches[0];
  if (!transition) return null;
  return {
    transition,
    target,
    needsResolution: transitionNeedsResolution(transition, String(target.category ?? '').toUpperCase()),
    needsScreen: transitionNeedsScreen(transition),
  };
}

export function normalizeIssue(issue, fallbackKey = '') {
  const metadata = issueMetadata(issue);
  const rawLabels = issueValue(issue, 'labels') ?? [];
  const labels = list(rawLabels).map(value => optionName(value)).filter(Boolean);
  const blocks = list(issueValue(issue, 'blocks')).map(value => {
    if (typeof value === 'string' || typeof value === 'number') return { key: String(value), reason: null };
    const row = object(value);
    return { ...row, key: row.key ?? row.target ?? row.ticket ?? row.ticketKey ?? '', reason: row.reason ?? row.why ?? null };
  }).filter(row => row.key);
  const links = list(issueValue(issue, 'links')).map(value => {
    if (typeof value === 'string' || typeof value === 'number') return { key: String(value), type: 'RELATES_TO', reason: null };
    const row = object(value);
    return { ...row, key: row.key ?? row.target ?? row.ticket ?? row.ticketKey ?? '', type: String(row.type ?? 'RELATES_TO').toUpperCase(), reason: row.reason ?? null };
  }).filter(row => row.key);
  const fixes = issueValue(issue, 'fixVersions', ['fix_versions', 'versions']);
  const key = issueKeyOf(issue, fallbackKey);
  const parsedPoints = issueValue(issue, 'storyPoints', ['story_points', 'points']);
  const points = parsedPoints === null || parsedPoints === undefined || parsedPoints === '' ? null : Number(parsedPoints);
  return {
    ...object(issue),
    key,
    path: issue?.path ?? issue?.relativePath ?? null,
    title: String(issueValue(issue, 'title') ?? key),
    body: String(issue?.body ?? issue?.description ?? issue?.content ?? ''),
    type: String(issueValue(issue, 'type', ['issueType']) ?? 'TASK').toUpperCase(),
    status: String(issueValue(issue, 'status') ?? ''),
    priority: String(issueValue(issue, 'priority') ?? 'MEDIUM').toUpperCase(),
    complexity: String(issueValue(issue, 'complexity') ?? 'MODERATE').toUpperCase(),
    risk: String(issueValue(issue, 'risk') ?? 'LOW').toUpperCase(),
    storyPoints: Number.isFinite(points) ? points : null,
    resolution: issueValue(issue, 'resolution') ?? null,
    severity: issueValue(issue, 'severity') ?? null,
    reproducibility: issueValue(issue, 'reproducibility', ['repro']) ?? null,
    flagged: Boolean(issueValue(issue, 'flagged', ['flag'])),
    dueDate: issueValue(issue, 'dueDate', ['due_date']) ?? null,
    environment: issueValue(issue, 'environment') ?? null,
    category: issueValue(issue, 'category', ['categoryName', 'categoryId']) ?? null,
    assignee: issueValue(issue, 'assignee', ['assigneeName', 'assigneeEmail', 'assigneeId']) ?? null,
    labels,
    fixVersions: list(fixes).map(value => optionName(value)).filter(Boolean),
    sprint: issueValue(issue, 'sprint', ['sprintName', 'sprintId']) ?? null,
    parent: issueValue(issue, 'parent', ['parentKey', 'parentId']) ?? null,
    blocks,
    links,
    customFieldValues: normalizeCustomFieldValues(issue),
    metadata,
  };
}

export function issueRevisions(detail) {
  const revisions = { ...object(detail?.revisions) };
  const issue = detail?.issue ?? detail;
  const path = issue?.path ?? issue?.relativePath;
  const revision = detail?.revision ?? issue?.revision;
  if (path && revision && revisions[path] === undefined) revisions[path] = revision;
  return revisions;
}

function addOption(args, flag, value, { allowNull = false } = {}) {
  if (value === undefined) return;
  if (value === null && !allowNull) return;
  const text = value === null || (allowNull && String(value) === '') ? 'none' : String(value);
  if (!text && !allowNull) return;
  args.push(flag, text);
}

function addList(args, flag, values) {
  const list = array(values).map(value => String(value).trim()).filter(Boolean);
  if (list.length) args.push(flag, list.join(','));
}

function titleOfDraft(draft) {
  return String(draft?.title ?? '').trim();
}

export function buildCreateCommand(projectKey, draft = {}) {
  const args = ['create', '--project', String(projectKey ?? '').trim(), '--title', titleOfDraft(draft)];
  addOption(args, '--type', draft.type);
  addOption(args, '--status', draft.status);
  addOption(args, '--priority', draft.priority);
  addOption(args, '--complexity', draft.complexity);
  addOption(args, '--risk', draft.risk);
  addOption(args, '--points', draft.storyPoints);
  addOption(args, '--resolution', draft.resolution, { allowNull: true });
  addOption(args, '--severity', draft.severity);
  addOption(args, '--repro', draft.reproducibility);
  addOption(args, '--due', draft.dueDate, { allowNull: true });
  if (draft.environment !== undefined && draft.environment !== null) args.push('--environment', String(draft.environment));
  if (draft.flagged === true) args.push('--flag');
  addOption(args, '--category', draft.category);
  addOption(args, '--assignee', draft.assignee, { allowNull: true });
  addOption(args, '--sprint', draft.sprint, { allowNull: true });
  addOption(args, '--parent', draft.parent, { allowNull: true });
  addList(args, '--label', draft.labels);
  addOption(args, '--fix-version', array(draft.fixVersions)[0]);
  addList(args, '--blocks', draft.blocks);
  addList(args, '--blocked-by', draft.blockedBy);
  const reason = String(draft.reason ?? '').trim();
  if (reason) addOption(args, '--reason', reason);
  return {
    args,
    files: { file: String(draft.body ?? '') },
    revisions: { ...object(draft.revisions) },
    // The current CLI accepts one `--fix-version`; keep the complete value
    // set available to an explicitly advertised action/backend extension.
    versions: [...new Set(array(draft.fixVersions).map(value => String(value).trim()).filter(Boolean))],
    customFieldValues: object(draft.customFieldValues),
  };
}

export function buildUpdateCommand(issue, draft = {}) {
  const current = normalizeIssue(issue);
  const key = issueKeyOf(issue, draft.key);
  const args = ['update', key];
  addOption(args, '--title', draft.title);
  addOption(args, '--type', draft.type);
  addOption(args, '--status', draft.status);
  addOption(args, '--priority', draft.priority);
  addOption(args, '--complexity', draft.complexity);
  addOption(args, '--risk', draft.risk);
  addOption(args, '--points', draft.storyPoints);
  addOption(args, '--resolution', draft.resolution, { allowNull: true });
  addOption(args, '--severity', draft.severity);
  addOption(args, '--repro', draft.reproducibility);
  addOption(args, '--due', draft.dueDate, { allowNull: true });
  if (draft.environment !== undefined && draft.environment !== null) args.push('--environment', String(draft.environment));
  if (draft.flagged !== undefined && Boolean(draft.flagged) !== Boolean(current.flagged)) {
    args.push('--flag');
    if (!draft.flagged) args.push('--clear');
  }
  addOption(args, '--category', draft.category, { allowNull: true });
  addOption(args, '--assignee', draft.assignee, { allowNull: true });
  addList(args, '--label', draft.labels);
  const oldLabels = current.labels;
  const nextLabels = array(draft.labels).map(value => String(value).trim()).filter(Boolean);
  const removed = oldLabels.filter(value => !nextLabels.includes(value));
  addList(args, '--unlabel', removed);
  const versions = array(draft.fixVersions).map(value => String(value).trim()).filter(Boolean);
  // The issue writer has a dedicated version command.  Keep the body update
  // pure and let IssuePanel issue the version operation separately.
  return {
    args,
    files: { body: String(draft.body ?? '') },
    revisions: { ...object(draft.revisions) },
    version: versions[0] ?? null,
    versions,
    customFieldValues: object(draft.customFieldValues),
  };
}

export function buildCommentCommand(issueKey, body, revisions = {}) {
  return {
    args: ['comment', String(issueKey ?? ''), '--file', 'comment.md'],
    files: { file: String(body ?? '') },
    revisions: { ...object(revisions) },
  };
}

export function buildLinkCommand(issueKey, relation, targetKey, reason, revisions = {}) {
  const relationValue = String(relation ?? 'relates-to').toLowerCase();
  const source = String(issueKey ?? '').trim();
  const target = String(targetKey ?? '').trim();
  const selfLinkError = issueLinkError(source, target);
  if (selfLinkError) throw new Error(selfLinkError);
  const args = ['link', source, relationValue, target];
  const text = String(reason ?? '').trim();
  if (text) addOption(args, '--reason', text);
  return { args, files: {}, revisions: { ...object(revisions) } };
}

export function buildUnlinkCommand(sourceKey, relation, targetKey, revisions = {}) {
  return {
    args: ['unlink', String(sourceKey ?? ''), String(relation ?? 'relates-to').toLowerCase(), String(targetKey ?? '').trim()],
    files: {},
    revisions: { ...object(revisions) },
  };
}

export function buildParentCommand(issueKey, parentKey, revisions = {}) {
  return {
    args: ['parent', String(issueKey ?? ''), parentKey ? String(parentKey) : 'none'],
    files: {},
    revisions: { ...object(revisions) },
  };
}

export function buildSprintCommand(issueKey, sprint, revisions = {}) {
  const name = sprint ? String(sprint) : 'none';
  return {
    args: ['sprint', sprint ? 'add' : 'remove', name, String(issueKey ?? '')],
    files: {},
    revisions: { ...object(revisions) },
  };
}

export function buildVersionCommand(issueKey, version, revisions = {}) {
  return {
    args: ['version', 'set', version ? String(version) : 'none', String(issueKey ?? '')],
    files: {},
    revisions: { ...object(revisions) },
  };
}

export function makeOperationId(prefix = 'issue') {
  const uuid = globalThis.crypto?.randomUUID?.();
  if (uuid) return `${prefix}-${uuid}`;
  return `${prefix}-${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`;
}

export function projectReadOnly(project, readOnly) {
  return Boolean(readOnly ?? project?.readOnly ?? project?.board?.readOnly);
}

export function projectCatalog(project, key) {
  const aliases = {
    users: ['members', 'assignees'],
    labels: ['labelCatalog'],
    categories: ['categoryCatalog'],
    versions: ['fixVersions', 'versionCatalog'],
    sprints: ['sprintCatalog'],
    tickets: ['issues'],
  };
  const value = project?.[key] ?? aliases[key]?.map(alias => project?.[alias]).find(Array.isArray);
  return array(value);
}

export function parentOptions(project, currentKey = '') {
  const selected = project?.currentIssue ?? project?.issue ?? null;
  const childType = selected ? normalizeIssue(selected).type : null;
  return projectCatalog(project, 'tickets').filter(ticket => {
    const normalized = normalizeIssue(ticket);
    if (!normalized.key || normalized.key === currentKey) return false;
    // When the shell has not supplied the selected issue, retain the old
    // create/edit default (only epics are safe parents for an unknown child).
    // issueParentError reports a bogus hierarchy violation for a
    // missing/unknown type, so only consult it when both types are
    // recognised; an unknown-type candidate stays selectable, consistent
    // with the guarded writers.
    if (!childType) return normalized.type === 'EPIC';
    if (!isKnownHierarchyType(childType) || !isKnownHierarchyType(normalized.type)) return true;
    return issueParentError(childType, normalized.type) === null;
  });
}

export function linkTypeFor(value) {
  const text = String(value ?? '').toUpperCase();
  if (text === 'BLOCKS') return 'blocks';
  if (text === 'DUPLICATES') return 'duplicates';
  if (text === 'CAUSES') return 'causes';
  return 'relates-to';
}

export function linkLabel(value, direction = 'outgoing') {
  const type = String(value ?? '').toUpperCase();
  const labels = direction === 'incoming' ? JIRA_LINK_INVERSE_LABELS : JIRA_LINK_LABELS;
  return labels[type] ?? JIRA_LINK_LABELS.RELATES_TO ?? 'relates to';
}

export function relationTarget(row, direction = 'outgoing') {
  return String(row?.key ?? row?.targetKey ?? row?.target ?? row?.ticketKey ?? row?.issueKey
    ?? (direction === 'incoming' ? row?.sourceKey : row?.targetKey) ?? '').trim();
}

export function relationSource(row, direction = 'outgoing', currentKey = '') {
  return String(row?.sourceKey ?? row?.source ?? (direction === 'incoming' ? relationTarget(row, direction) : currentKey)).trim();
}

export async function callApi(api, path, options = {}) {
  if (typeof api !== 'function') throw new Error('An issue API callback is required');
  const response = await api(path, options);
  if (response && typeof response.json === 'function') {
    const payload = await response.json();
    if (!response.ok) {
      const error = new Error(payload?.error?.message ?? payload?.error ?? `Request failed (${response.status})`);
      error.status = response.status;
      error.code = payload?.error?.code;
      throw error;
    }
    return payload;
  }
  return response;
}

export async function sendCommand(api, command, prefix = 'issue', { retryDelays } = {}) {
  // Built once so every 423 retry replays the SAME operationId — the server
  // treats that replay as one idempotent operation, not a second write.
  const payload = {
    args: array(command?.args).map(value => String(value)),
    files: object(command?.files),
    revisions: object(command?.revisions),
    operationId: command?.operationId ?? makeOperationId(prefix),
  };
  return withLockRetry(
    () => callApi(api, '/api/commands', { method: 'POST', body: payload }),
    { delays: retryDelays },
  );
}

/**
 * Build the separately allowlisted action envelope.  The UI never invents an
 * action name: callers pass the name advertised by the project payload (or
 * by an explicit component prop), which keeps an absent backend capability a
 * visible, reviewable gap instead of a guessed mutation.
 */
export function buildActionRequest(action, project, key, data = {}, revisions = {}, operationId = undefined) {
  const name = String(action ?? '').trim();
  if (!name) throw new Error('This issue action is not available.');
  return {
    action: name,
    project: String(project ?? '').trim(),
    key: String(key ?? '').trim(),
    data: object(data),
    revisions: object(revisions),
    operationId: operationId ?? makeOperationId(`issue-action-${name.replace(/[^a-z0-9]+/gi, '-').toLowerCase()}`),
  };
}

export async function sendAction(api, request, { retryDelays } = {}) {
  return withLockRetry(
    () => callApi(api, '/api/actions', { method: 'POST', body: request }),
    { delays: retryDelays },
  );
}

export function configuredIssueAction(project, kind) {
  const actions = project?.actions ?? project?.actionSchemas ?? project?.issueActions ?? {};
  const aliases = {
    create: ['create', 'issue.create', 'issueCreate'],
    update: ['update', 'issue.update', 'issueUpdate'],
    delete: ['delete', 'issue.delete', 'issueDelete', 'deleteIssue'],
    createComment: ['createComment', 'comment.create', 'issue.comment.create'],
    deleteComment: ['deleteComment', 'comment.delete', 'issue.comment.delete'],
    customFields: ['customFields', 'issue.customFields', 'issueCustomFields', 'customFieldValues'],
    transition: ['transition', 'issue.transition', 'issueTransition'],
    fixVersions: ['fixVersions', 'issue.fixVersions', 'issueFixVersions', 'versions'],
  };
  const raw = aliases[kind]?.map(name => actions?.[name]).find(value => value !== undefined);
  if (typeof raw === 'string') return { action: raw };
  if (raw && typeof raw === 'object') {
    const action = raw.action ?? raw.name;
    return action ? { ...raw, action: String(action) } : null;
  }
  return null;
}

export function issueActionData(draft = {}) {
  return {
    title: String(draft.title ?? '').trim(),
    body: String(draft.body ?? ''),
    type: draft.type ?? null,
    status: draft.status ?? null,
    priority: draft.priority ?? null,
    complexity: draft.complexity ?? null,
    risk: draft.risk ?? null,
    storyPoints: draft.storyPoints ?? null,
    resolution: draft.resolution || null,
    severity: draft.severity || null,
    reproducibility: draft.reproducibility || null,
    dueDate: draft.dueDate || null,
    environment: draft.environment || null,
    flagged: Boolean(draft.flagged),
    category: draft.category || null,
    assignee: draft.assignee || null,
    labels: array(draft.labels).map(value => String(value).trim()).filter(Boolean),
    fixVersions: [...new Set(array(draft.fixVersions).map(value => String(value).trim()).filter(Boolean))],
    sprint: draft.sprint || null,
    parent: draft.parent || null,
    customFieldValues: normalizeCustomFieldValues({ customFieldValues: draft.customFieldValues }),
  };
}
