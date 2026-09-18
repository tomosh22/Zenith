/*
 * Allowlisted project/settings actions.
 *
 * Every action builds byte changes and delegates the actual write to
 * executeOperation. Project actions therefore use the same lock, revision
 * check, validation, journal, atomic replacement and idempotency as CLI
 * commands and document saves.
 */

import { randomUUID } from 'node:crypto';

import {
  DEFAULT_JIRA_STATUSES,
  DEFAULT_JIRA_TRANSITIONS,
  JIRA_COMPLEXITIES,
  JIRA_ISSUE_TYPES,
  JIRA_PRIORITIES,
  JIRA_REPRODUCIBILITIES,
  JIRA_RESOLUTIONS,
  JIRA_RISKS,
  JIRA_SEVERITIES,
} from '../shared/jira/constants.js';
import { fieldValueError } from '../shared/jira/fields.js';
import {
  applyTransition,
  evaluateTransition,
  resolutionInvariantError,
} from '../shared/jira/workflow.js';
import { canonicalOf, allTicketRecords, sourceOf } from '../cli/compat.mjs';
import { issueParentError } from '../shared/jira/constants.js';
import { isKnownHierarchyType } from '../shared/hierarchy.mjs';
import { bytesOf, patchRecord } from '../cli/write.mjs';
import { executeOperation, readRevision } from './operations.mjs';
import { SETTINGS_ACTIONS, dispatchSettingsAction } from './settings-actions.mjs';
import { ISSUE_INTERACTION_ACTIONS, dispatchIssueInteraction } from './issue-interactions.mjs';

const PROJECT_KEY = /^[A-Z][A-Z0-9]{0,9}$/;
const ISSUE_TYPES = new Set(JIRA_ISSUE_TYPES);
const PRIORITIES = new Set(JIRA_PRIORITIES);
const COMPLEXITIES = new Set(JIRA_COMPLEXITIES);
const RISKS = new Set(JIRA_RISKS);
const RESOLUTIONS = new Set(JIRA_RESOLUTIONS);
const SEVERITIES = new Set(JIRA_SEVERITIES);
const REPRODUCIBILITIES = new Set(JIRA_REPRODUCIBILITIES);
const ACTIONS = new Set([
  'project.create',
  'issue.create', 'issue.update',
  'sprint.start', 'sprint.complete', 'sprint.update', 'sprint.delete', 'sprint.reorder',
  'version.update', 'version.archive', 'version.delete', 'version.reorder',
  'workflow.status.create', 'workflow.status.update', 'workflow.status.delete', 'workflow.status.reorder',
  'workflow.transition.create', 'workflow.transition.delete',
  'workflow.transition.rule.create', 'workflow.transition.rule.delete', 'workflow.transition.screen',
  'field.requirement.set', 'field.create', 'field.delete',
  'screen.create', 'screen.delete', 'screen.fields.set',
  ...SETTINGS_ACTIONS,
  ...ISSUE_INTERACTION_ACTIONS,
]);

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value : [];
}

function text(value, fallback = '') {
  if (value === undefined || value === null) return fallback;
  const result = String(value).trim();
  return result || fallback;
}

function upper(value) { return text(value).toUpperCase(); }

function fail(code, message) {
  const error = new Error(message);
  error.code = code;
  throw error;
}

function assertPlain(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) fail('INVALID', label + ' must be an object');
  return value;
}

function assertText(value, label, { required = true, max = 1000 } = {}) {
  if (value === undefined || value === null) {
    if (required) fail('INVALID', label + ' is required');
    return value;
  }
  if (typeof value !== 'string') fail('INVALID', label + ' must be a string');
  const result = value.trim();
  if (required && !result) fail('INVALID', label + ' is required');
  if (result.length > max) fail('INVALID', label + ' is too long');
  return result;
}

function validateOperationId(value) {
  if (value === undefined || value === null || value === '') return randomUUID();
  if (typeof value !== 'string' || !/^[A-Za-z0-9_-]{1,100}$/.test(value)) fail('ID', 'Invalid operation ID');
  return value;
}

function validateProjectKey(value, label = 'project') {
  const result = upper(value);
  if (!PROJECT_KEY.test(result)) fail('INVALID', label + ' must match /^[A-Z][A-Z0-9]{0,9}$/');
  return result;
}

function pathValue(value) {
  if (typeof value !== 'string' || !value.trim()) fail('INVALID', 'Revision paths must be non-empty strings');
  const normalized = value.replaceAll('\\', '/').replace(/^\.\//, '');
  if (normalized.startsWith('/') || normalized.includes('\0')
    || normalized.split('/').some(part => part === '..' || part === '.git' || part === '.zagent')
    || /^[A-Za-z]:/.test(normalized)) fail('PATH', 'Revision path escapes the vault');
  return normalized;
}

function validateRevisions(value) {
  if (value === undefined) return {};
  assertPlain(value, 'revisions');
  const result = {};
  for (const [rawPath, revision] of Object.entries(value)) {
    const relative = pathValue(rawPath);
    if (revision === null) {
      result[relative] = null;
      continue;
    }
    assertPlain(revision, 'revision for ' + relative);
    if (revision.hash !== null && typeof revision.hash !== 'string') fail('INVALID', 'revision hash for ' + relative + ' must be a string or null');
    if (revision.size !== undefined && (!Number.isInteger(revision.size) || revision.size < 0)) fail('INVALID', 'revision size for ' + relative + ' must be a non-negative integer');
    if (revision.mtimeMs !== undefined && (typeof revision.mtimeMs !== 'number' || !Number.isFinite(revision.mtimeMs))) fail('INVALID', 'revision mtimeMs for ' + relative + ' must be finite');
    result[relative] = {
      hash: revision.hash ?? null,
      ...(revision.size === undefined ? {} : { size: revision.size }),
      ...(revision.mtimeMs === undefined ? {} : { mtimeMs: revision.mtimeMs }),
    };
  }
  return result;
}

function validateRequest(input) {
  const body = assertPlain(input, 'action request');
  const allowed = new Set(['action', 'project', 'key', 'data', 'revisions', 'operationId']);
  for (const key of Object.keys(body)) if (!allowed.has(key)) fail('INVALID', 'Unknown action input "' + key + '"');
  const action = assertText(body.action, 'action', { max: 100 });
  if (!ACTIONS.has(action)) fail('INVALID', 'Unsupported project action: ' + action);
  const data = body.data === undefined ? {} : assertPlain(body.data, 'data');
  const project = body.project === undefined || body.project === null || body.project === '' ? null : validateProjectKey(body.project);
  const key = body.key === undefined || body.key === null || body.key === '' ? null : assertText(body.key, 'key', { max: 200 });
  return { action, project, key, data, revisions: validateRevisions(body.revisions), operationId: validateOperationId(body.operationId) };
}

function configRecords(index, project, kind) {
  const wantedProject = upper(project);
  const wantedKind = String(kind).toLowerCase();
  return [...(index?.configs?.values?.() ?? [])].filter(record => {
    if (upper(record?.project) !== wantedProject) return false;
    const fields = record?.fields ?? record?.metadata ?? {};
    return String(fields.kind ?? fields.entity ?? '').toLowerCase() === wantedKind
      || String(record.relativePath ?? '').toLowerCase().endsWith('/' + wantedKind + '.md');
  });
}

function boardRoot(index, project) {
  const wanted = upper(project);
  return array(index?.boardRoots).find(root => upper(root?.project) === wanted) ?? null;
}

function ensureProject(index, project) {
  const root = boardRoot(index, project);
  const hasTickets = allTicketRecords(index).some(record => upper(record?.project) === upper(project));
  const tables = index?.tables?.get?.('jira.projects') ?? [];
  const hasProjectRow = array(tables).some(row => upper(row?.key) === upper(project));
  if (!root && !hasTickets && !hasProjectRow) fail('NOT_FOUND', 'Project ' + project + ' was not found');
  if (root?.readOnly === true) fail('READ_ONLY', 'Project ' + project + ' is read-only');
  return root;
}

function yamlConfig(fields) {
  return Buffer.from('---\n' + Object.entries(fields).map(([key, value]) => key + ': ' + JSON.stringify(value) + '\n').join('') + '---\n', 'utf8');
}

function fieldsOf(record) { return object(record?.fields ?? record?.metadata ?? {}); }
function rowsOf(record) { return array(fieldsOf(record).rows); }

function tableRows(index, name) {
  const rows = index?.tables?.get?.(name);
  return Array.isArray(rows) ? rows : [];
}

function projectId(index, project) {
  const row = tableRows(index, 'jira.projects').find(item => upper(item?.key ?? item?.project_key) === upper(project));
  return row?.id === undefined || row?.id === null ? null : String(row.id);
}

function belongsToProject(index, project, row) {
  const direct = row?.project ?? row?.project_key ?? row?.projectKey;
  if (direct !== undefined && direct !== null && direct !== '') return upper(direct) === upper(project);
  const id = row?.project_id ?? row?.projectId;
  return id !== undefined && id !== null && projectId(index, project) === String(id);
}

function customFieldDefinitions(index, project) {
  const direct = rowsOf(configRecords(index, project, 'customFields')[0]);
  const relational = tableRows(index, 'jira.custom_fields').filter(row => belongsToProject(index, project, row));
  const optionsByField = new Map();
  for (const option of tableRows(index, 'jira.custom_field_options')) {
    const id = option?.field_id ?? option?.fieldId ?? option?.custom_field_id ?? option?.customFieldId;
    if (id === undefined || id === null) continue;
    const key = String(id);
    if (!optionsByField.has(key)) optionsByField.set(key, []);
    optionsByField.get(key).push(option);
  }
  for (const options of optionsByField.values()) options.sort((a, b) => Number(a?.position ?? 0) - Number(b?.position ?? 0));
  const result = [];
  const seen = new Set();
  for (const row of [...direct, ...relational]) {
    const id = text(row?.id ?? row?.key ?? row?.name);
    if (!id || seen.has(id.toLowerCase())) continue;
    seen.add(id.toLowerCase());
    const options = optionsByField.get(id) ?? array(row?.options);
    result.push({
      ...row,
      id,
      type: upper(row?.type ?? row?.fieldType ?? 'TEXT'),
      options: options.map(option => text(option?.value ?? option?.name ?? option)),
    });
  }
  return result;
}

function fieldRequirementRows(index, project) {
  const direct = rowsOf(configRecords(index, project, 'fieldConfiguration')[0]);
  const relational = tableRows(index, 'jira.field_configurations').filter(row => belongsToProject(index, project, row));
  return [...direct, ...relational];
}

function matchesId(row, wanted) {
  const value = text(wanted);
  if (!value) return false;
  return [row?.id, row?.key, row?.slug, row?.name].some(candidate => String(candidate ?? '').toLowerCase() === value.toLowerCase());
}

function actionExpected(request, targetPaths = []) {
  const expected = { ...request.revisions };
  for (const target of targetPaths) {
    if (Object.hasOwn(expected, target.path)) continue;
    // Existing targets must be opened by the client and sent back as a CAS
    // revision. Filling this from the server's current snapshot would turn a
    // stale browser draft into a silent overwrite.
    if (target.record) fail('STALE', `An opened revision is required for ${target.path}`);
    // New catalog/scaffold paths are protected by an explicit expected-missing
    // revision. executeOperation checks this while holding the shared lock.
    expected[target.path] = null;
  }
  return expected;
}

async function assertMissingPath(root, relative) {
  const revision = await readRevision(root, relative);
  if (revision.hash !== null) fail('STALE', `Path already exists: ${relative}`);
}

function enumInput(value, label, values, fallback = null) {
  if (value === undefined || value === null || value === '') return fallback;
  const result = upper(value);
  if (!values.has(result)) fail('INVALID', label + ' must be one of ' + [...values].join(', '));
  return result;
}

function workflowStatus(index, project, value) {
  const requested = text(value);
  if (!requested) return null;
  const record = configRecords(index, project, 'workflow')[0];
  const statuses = array(fieldsOf(record).statuses);
  const found = statuses.find(status => text(status.name ?? status.status).toLowerCase() === requested.toLowerCase()
    || text(status.id ?? status.statusId ?? status.status_id).toLowerCase() === requested.toLowerCase());
  if (!found) fail('INVALID', 'Status ' + requested + ' is not in the ' + project + ' workflow');
  return text(found.name ?? found.status);
}

function workflowTransition(index, project, fromName, toName) {
  const record = configRecords(index, project, 'workflow')[0];
  const fields = fieldsOf(record);
  const statuses = array(fields.statuses);
  const statusId = value => {
    const requested = text(value);
    const found = statuses.find(status => text(status?.id ?? status?.statusId ?? status?.status_id ?? status?.name).toLowerCase() === requested.toLowerCase()
      || text(status?.name ?? status?.status).toLowerCase() === requested.toLowerCase());
    return found ? text(found.id ?? found.statusId ?? found.status_id ?? found.name) : requested;
  };
  const fromId = statusId(fromName);
  const toId = statusId(toName);
  const transitions = array(fields.transitions).map((edge, position) => {
    const item = object(edge);
    return {
      ...item,
      id: text(item.id ?? item.transitionId, 'transition-' + (position + 1)),
      fromStatusId: item.fromStatusId ?? item.from_status_id ?? item.from ?? null,
      toStatusId: item.toStatusId ?? item.to_status_id ?? item.to ?? null,
      rules: array(item.rules),
    };
  });
  const candidates = transitions.filter(edge => {
    const from = edge.fromStatusId === null || edge.fromStatusId === undefined ? null : statusId(edge.fromStatusId);
    const to = statusId(edge.toStatusId);
    return to.toLowerCase() === toId.toLowerCase() && (from === null || from.toLowerCase() === fromId.toLowerCase());
  });
  const edge = candidates.find(item => item.fromStatusId !== null && item.fromStatusId !== undefined)
    ?? candidates[0];
  if (!edge) return null;
  const relational = tableRows(index, 'jira.workflow_transition_rules')
    .filter(row => String(row?.transition_id ?? row?.transitionId ?? '') === String(edge.id))
    .map(row => ({ ...row, config: object(row.config) }));
  return {
    ...edge,
    fromStatusId: fromId,
    toStatusId: toId,
    rules: [...edge.rules, ...relational].map(rule => ({
      ...object(rule),
      kind: upper(rule?.kind),
      type: upper(rule?.type),
      config: object(rule?.config),
    })),
  };
}

function transitionContext(index, project, issue, next, context = {}) {
  const key = text(canonicalOf(issue).key);
  const tickets = allTicketRecords(index).filter(ticket => upper(ticket.project) === upper(project));
  const done = ticket => upper(ticket.statusCategory ?? canonicalOf(ticket).statusCategory) === 'DONE'
    || upper(canonicalOf(ticket).status) === 'DONE';
  const openBlockerKeys = [];
  for (const ticket of tickets) {
    for (const link of array(canonicalOf(ticket).blocks)) {
      const target = text(link?.key ?? link?.target ?? link?.ticket ?? link);
      if (target.toUpperCase() === key.toUpperCase() && !done(ticket)) openBlockerKeys.push(text(canonicalOf(ticket).key));
    }
  }
  const unresolvedChildKeys = tickets
    .filter(ticket => text(canonicalOf(ticket).parent).toUpperCase() === key.toUpperCase() && !done(ticket))
    .map(ticket => text(canonicalOf(ticket).key));
  const issueId = sourceOf(issue)?.id ?? canonicalOf(issue).id ?? key;
  const commentCount = tableRows(index, 'jira.comments').filter(row => String(row?.issue_id ?? row?.issueId ?? '') === String(issueId)).length;
  const actor = object(context.actor);
  const actorId = actor.id ?? actor.email ?? actor.actorId ?? null;
  return {
    actor: {
      id: actorId,
      orgRole: upper(actor.orgRole ?? actor.role ?? 'OWNER'),
      projectRoleIds: array(actor.projectRoleIds ?? actor.projectRoles),
    },
    issue: {
      id: issueId,
      assigneeId: canonicalOf(issue).assignee ?? null,
      reporterId: canonicalOf(issue).reporter ?? sourceOf(issue)?.reporter_id ?? null,
      storyPoints: next.storyPoints,
      dueDate: next.dueDate,
      severity: next.severity,
      reproducibility: next.reproducibility,
      environment: next.environment,
      categoryId: next.category,
      description: next.body,
      customFieldValues: next.customFieldValues,
      resolution: next.resolution,
      flagged: next.flagged,
      openBlockerKeys,
      unresolvedChildKeys,
      commentCount,
    },
    customFieldNames: Object.fromEntries(customFieldDefinitions(index, project).map(field => [field.id, text(field.name, field.id)])),
  };
}

function enforceTransition(index, project, issue, next, context = {}) {
  const beforeStatus = text(canonicalOf(issue).status);
  if (beforeStatus.toLowerCase() === next.status.toLowerCase()) {
    const category = upper(issue.statusCategory ?? canonicalOf(issue).statusCategory ?? 'TODO');
    const invariant = resolutionInvariantError(category, next.resolution);
    if (invariant) fail('INVALID', invariant);
    return;
  }
  const transition = workflowTransition(index, project, beforeStatus, next.status);
  if (!transition) fail('INVALID', 'Workflow does not permit ' + beforeStatus + ' → ' + next.status);
  const statuses = array(fieldsOf(configRecords(index, project, 'workflow')[0]).statuses);
  const target = statuses.find(status => text(status.id ?? status.name).toLowerCase() === String(transition.toStatusId).toLowerCase()
    || text(status.name).toLowerCase() === next.status.toLowerCase());
  const contextValue = transitionContext(index, project, issue, next, context);
  const verdict = evaluateTransition(transition, contextValue);
  if (verdict.conditionError || verdict.validatorError) fail('INVALID', verdict.conditionError ?? verdict.validatorError);
  const applied = applyTransition(transition, upper(target?.category ?? 'TODO'), contextValue, { resolutionChoice: next.resolution });
  if (applied.error) fail('INVALID', applied.error);
  if (applied.patch.resolution !== undefined) next.resolution = applied.patch.resolution;
  if (applied.patch.assigneeId !== undefined) next.assignee = applied.patch.assigneeId;
  if (applied.patch.flagged !== undefined) next.flagged = applied.patch.flagged;
  const invariant = resolutionInvariantError(upper(target?.category ?? 'TODO'), next.resolution);
  if (invariant) fail('INVALID', invariant);
}

function sprintNameFor(index, project, value) {
  const requested = text(value);
  if (!requested) return null;
  const found = [...(index?.sprints?.values?.() ?? [])].find(record => upper(record.project) === upper(project)
    && [fieldsOf(record).name, fieldsOf(record).slug, fieldsOf(record).key].some(candidate => text(candidate).toLowerCase() === requested.toLowerCase()));
  if (!found) fail('INVALID', 'Sprint ' + requested + ' was not found in ' + project);
  return text(fieldsOf(found).slug ?? fieldsOf(found).key ?? fieldsOf(found).name);
}

function versionNamesFor(index, project, values) {
  const names = array(values).map(value => text(value)).filter(Boolean);
  if (!names.length) return [];
  const catalog = configRecords(index, project, 'versions')[0];
  const rows = rowsOf(catalog);
  for (const name of names) if (!rows.some(row => [row.name, row.id, row.slug].some(candidate => text(candidate).toLowerCase() === name.toLowerCase()))) fail('INVALID', 'Version ' + name + ' was not found in ' + project);
  return [...new Set(names)];
}

function customValuesFor(index, project, value) {
  const source = object(value);
  if (!Object.keys(source).length) return {};
  const definitions = customFieldDefinitions(index, project);
  const byId = new Map(definitions.flatMap(row => [[text(row.id), row], [text(row.key), row]]).filter(([id]) => id));
  const result = {};
  for (const [id, raw] of Object.entries(source)) {
    const definition = byId.get(id);
    if (!definition) fail('INVALID', 'Unknown custom field ' + id);
    const values = Array.isArray(raw) ? raw.map(item => String(item)) : [String(raw)];
    const error = fieldValueError({ name: definition.name ?? definition.key ?? id, type: upper(definition.type ?? 'TEXT'), options: array(definition.options).map(item => text(item?.name ?? item)) }, values);
    if (error) fail('INVALID', error);
    result[definition.id] = values;
  }
  return result;
}

function issueData(index, project, data, current = null) {
  const incoming = object(data);
  const source = current
    ? {
        ...canonicalOf(current),
        ...incoming,
        body: Object.hasOwn(incoming, 'body') ? incoming.body : current.bodyText ?? current.body ?? '',
      }
    : incoming;
  const title = assertText(source.title, 'issue title', { max: 1000 });
  const type = enumInput(source.type, 'issue type', ISSUE_TYPES, current ? upper(canonicalOf(current).type ?? 'TASK') : 'TASK');
  const priority = enumInput(source.priority, 'priority', PRIORITIES, current ? upper(canonicalOf(current).priority ?? 'MEDIUM') : 'MEDIUM');
  const complexity = enumInput(source.complexity, 'complexity', COMPLEXITIES, current ? upper(canonicalOf(current).complexity ?? 'MODERATE') : 'MODERATE');
  const risk = enumInput(source.risk, 'risk', RISKS, current ? upper(canonicalOf(current).risk ?? 'LOW') : 'LOW');
  const severity = enumInput(source.severity, 'severity', SEVERITIES, current ? upper(canonicalOf(current).severity ?? '') || null : null);
  const reproducibility = enumInput(source.reproducibility ?? source.repro, 'reproducibility', REPRODUCIBILITIES, current ? upper(canonicalOf(current).reproducibility ?? '') || null : null);
  const status = workflowStatus(index, project, source.status ?? (current ? canonicalOf(current).status : 'To Do')) ?? 'To Do';
  const points = source.storyPoints === null || source.storyPoints === undefined || source.storyPoints === '' ? null : Number(source.storyPoints);
  if (points !== null && (!Number.isInteger(points) || points < 0)) fail('INVALID', 'storyPoints must be a non-negative integer or null');
  const parent = text(source.parent) || null;
  if (parent) {
    const target = ticketByProject(index, project, parent);
    if (!target) fail('NOT_FOUND', 'Parent ' + parent + ' was not found');
    // issueParentError reports a bogus hierarchy violation for a
    // missing/unknown type, so only consult it when both types are
    // recognised; the parent target's type comes from ticket metadata and
    // can be unknown or empty.
    const parentType = upper(canonicalOf(target).type);
    const parentError = isKnownHierarchyType(type) && isKnownHierarchyType(parentType)
      ? issueParentError(type, parentType)
      : null;
    if (parentError) fail('INVALID', parentError);
  }
  const sprint = sprintNameFor(index, project, source.sprint);
  const fixVersions = versionNamesFor(index, project, source.fixVersions ?? source.versions);
  const labels = array(source.labels).map(item => assertText(String(item), 'label', { max: 200 })).filter(Boolean);
  const customFieldValues = customValuesFor(index, project, source.customFieldValues);
  for (const definition of customFieldDefinitions(index, project)) {
    const requirement = fieldRequirementRows(index, project).find(row => upper(row?.issueType ?? row?.issue_type ?? '') === type
      && [row?.customFieldId, row?.custom_field_id, row?.fieldId, row?.field_id]
        .some(value => text(value).toLowerCase() === text(definition.id).toLowerCase()
          || text(value).toLowerCase() === text(definition.key).toLowerCase()));
    if (upper(requirement?.requirement ?? requirement?.visibility ?? requirement?.mode) === 'REQUIRED'
      && (!Array.isArray(customFieldValues[definition.id]) || customFieldValues[definition.id].length === 0)) {
      fail('INVALID', text(definition.name, definition.id) + ' is required for ' + type);
    }
  }
  return {
    title,
    body: String(source.body ?? ''),
    type,
    status,
    priority,
    complexity,
    risk,
    storyPoints: points,
    resolution: enumInput(source.resolution, 'resolution', RESOLUTIONS, null),
    severity,
    reproducibility,
    dueDate: text(source.dueDate) || null,
    environment: text(source.environment) || null,
    flagged: Boolean(source.flagged),
    category: text(source.category) || null,
    assignee: text(source.assignee) || null,
    labels: [...new Set(labels)],
    fixVersions,
    sprint,
    parent,
    customFieldValues,
  };
}

function ticketByProject(index, project, key) {
  const wanted = upper(key);
  return allTicketRecords(index).find(record => upper(record.project) === upper(project)
    && upper(canonicalOf(record).key) === wanted) ?? null;
}

function issueCreate(index, request) {
  const project = validateProjectKey(request.project);
  const root = ensureProject(index, project);
  return runAction(index, request, 'issue.create', async fresh => {
    const current = issueData(fresh, project, request.data);
    const numbers = allTicketRecords(fresh)
      .filter(record => upper(record.project) === project)
      .map(record => Number(/-(\d+)$/.exec(text(canonicalOf(record).key))?.[1]))
      .filter(Number.isFinite);
    const number = (numbers.length ? Math.max(...numbers) : 0) + 1;
    const key = project + '-' + number;
    const fields = {
      id: randomUUID(), key, type: current.type, title: current.title, category: current.category,
      parent: current.parent, sprint: current.sprint, status: current.status, assignee: current.assignee,
      priority: current.priority, complexity: current.complexity, risk: current.risk, storyPoints: current.storyPoints,
      position: number, labels: current.labels, fixVersions: current.fixVersions, blocks: [],
      updatedAt: new Date().toISOString(), customFieldValues: current.customFieldValues,
    };
    const relativePath = String(root.path ?? project).replaceAll('\\', '/').replace(/\/+$/, '') + '/tickets/' + key + '.md';
    // The number is allocated from the locked fresh index. Still guard the
    // exact path because files can exist outside the scanner's configured
    // roots and must never be overwritten by a create operation.
    await assertMissingPath(fresh.root, relativePath);
    const frontmatter = yamlConfig(fields);
    const bytes = current.body ? Buffer.concat([frontmatter, Buffer.from('\n' + current.body, 'utf8')]) : frontmatter;
    return { changes: [{ path: relativePath, bytes }], result: { key, id: fields.id, created: true, status: current.status } };
  }, { allowPaths: [root.path ?? project] });
}

function issueUpdate(index, request, context = {}) {
  const project = validateProjectKey(request.project ?? text(request.key).split('-')[0]);
  ensureProject(index, project);
  const key = assertText(request.key ?? request.data.key, 'issue key', { max: 200 });
  const current = ticketByProject(index, project, key);
  if (!current) fail('NOT_FOUND', 'Issue ' + key + ' was not found');
  const expected = actionExpected(request, [{ record: current, path: current.relativePath }]);
  return runAction(index, request, 'issue.update', async fresh => {
    const found = ticketByProject(fresh, project, key);
    if (!found) fail('NOT_FOUND', 'Issue ' + key + ' was not found');
    const next = issueData(fresh, project, request.data, found);
    enforceTransition(fresh, project, found, next, context);
    const currentStatus = text(canonicalOf(found).status);
    const currentCategory = workflowCategoryFor(fresh, project, currentStatus)
      || upper(canonicalOf(found).statusCategory ?? found.statusCategory ?? '');
    const nextCategory = workflowCategoryFor(fresh, project, next.status);
    const leavingInProgress = (currentCategory === 'IN_PROGRESS' || currentStatus.toLowerCase() === 'in progress')
      && nextCategory !== 'IN_PROGRESS' && next.status.toLowerCase() !== 'in progress';
    const updates = { title: next.title, type: next.type, status: next.status, priority: next.priority, complexity: next.complexity, risk: next.risk, storyPoints: next.storyPoints, resolution: next.resolution, severity: next.severity, reproducibility: next.reproducibility, dueDate: next.dueDate, environment: next.environment, flagged: next.flagged, category: next.category, assignee: next.assignee, labels: next.labels, fixVersions: next.fixVersions, sprint: next.sprint, parent: next.parent, customFieldValues: next.customFieldValues };
    if (leavingInProgress) {
      // A human board move out of In Progress revokes the active worker
      // attempt.  Clearing both fields atomically prevents a stale worker
      // from appearing to own a ticket that is back in the queue.
      updates.assignee = null;
      updates.claimAttempt = null;
    }
    return { changes: [{ path: found.relativePath, bytes: patchRecord(found, updates, next.body) }], result: { key, updated: true } };
  }, { expected });
}

async function runAction(index, request, kind, prepare, { expected = request.revisions, allowPaths = [] } = {}) {
  if (index?.vaultConfig?.writes?.enabled !== true) fail('READ_ONLY', 'Vault writes are disabled; configure writes.enabled explicitly');
  return executeOperation(index.root, {
    kind,
    id: request.operationId,
    fingerprint: JSON.stringify({ action: request.action, project: request.project, key: request.key, data: request.data, revisions: request.revisions }),
    expected,
    allowPaths,
    prepare,
  });
}

function projectCreate(index, request) {
  const data = request.data;
  const key = validateProjectKey(data.key ?? request.project ?? request.key, 'project key');
  const name = assertText(data.name, 'project name', { max: 200 });
  const description = assertText(data.description, 'project description', { required: false, max: 10000 }) ?? null;
  const configRecord = index.configRecord;
  const configPathValue = index.configPath ?? 'vault.config.json';
  const expected = actionExpected(request, [{ record: configRecord, path: configPathValue }]);
  return runAction(index, request, 'project.create', async fresh => {
    const currentConfig = fresh.configRecord;
    if (!currentConfig) fail('INVALID', 'Project creation requires vault.config.json');
    let config;
    try { config = JSON.parse(bytesOf(currentConfig).toString('utf8')); } catch { fail('INVALID', 'vault.config.json is not valid JSON'); }
    const currentProjectRows = fresh?.tables?.get?.('jira.projects') ?? [];
    if (boardRoot(fresh, key) || array(currentProjectRows).some(row => upper(row?.key) === key)
      || allTicketRecords(fresh).some(ticket => upper(canonicalOf(ticket).key).startsWith(key + '-'))) {
      fail('INVALID', 'Project ' + key + ' already exists');
    }
    const roots = array(config.boardRoots ?? config.boards ?? config.projects).map(item => ({ ...object(item) }));
    if (roots.some(item => upper(item.project ?? item.name) === key)) fail('INVALID', 'Project ' + key + ' already exists');
    roots.push({ project: key, path: key });
    config.boardRoots = roots;
    if (Array.isArray(config.boards)) delete config.boards;
    config.projects = { ...object(config.projects), [key]: { key, name, description } };
    const statuses = DEFAULT_JIRA_STATUSES.map((status, position) => ({ id: status.name.toLowerCase().replace(/[^a-z0-9]+/g, '-'), name: status.name, category: status.category, position }));
    const statusIds = new Map(statuses.map(status => [status.name, status.id]));
    const transitions = DEFAULT_JIRA_TRANSITIONS.map((edge, position) => ({ id: 'transition-' + (position + 1), name: edge.name, from_status_id: edge.from === null ? null : statusIds.get(edge.from) ?? edge.from, to_status_id: statusIds.get(edge.to) ?? edge.to, position }));
    const scaffold = [
      key + '/workflow.md',
      key + '/labels.md',
      key + '/versions.md',
      key + '/config/board.md',
    ];
    for (const relative of scaffold) await assertMissingPath(fresh.root, relative);
    return {
      changes: [
        { path: configPathValue, bytes: Buffer.from(JSON.stringify(config, null, 2) + '\n', 'utf8') },
        { path: key + '/workflow.md', bytes: yamlConfig({ kind: 'workflow', rows: [{ id: key.toLowerCase() + '-default-workflow', name: 'Default' }], statuses, transitions }) },
        { path: key + '/labels.md', bytes: yamlConfig({ kind: 'labels', rows: [] }) },
        { path: key + '/versions.md', bytes: yamlConfig({ kind: 'versions', rows: [] }) },
        { path: key + '/config/board.md', bytes: yamlConfig({ kind: 'board', rows: [], name: 'Board', type: 'KANBAN', columns: statuses.map(status => ({ id: status.id, name: status.name, position: status.position, statusIds: [status.id] })) }) },
      ],
      result: { key, name, description, created: true },
    };
  }, { expected, allowPaths: [key] });
}

function sprintTarget(index, project, id) {
  const wanted = text(id);
  const records = [...(index?.sprints?.values?.() ?? [])].filter(record => upper(record?.project) === upper(project));
  return records.find(record => {
    const fields = fieldsOf(record);
    const source = object(fields.source?.source ?? fields.source ?? sourceOf(record));
    return matchesId(fields, wanted) || matchesId(source, wanted);
  }) ?? null;
}

function sprintStatusOf(record) {
  const fields = fieldsOf(record);
  const source = sourceOf(record);
  return upper(fields.status ?? source.status ?? 'PLANNED');
}

function sprintSlugOf(record) {
  const fields = fieldsOf(record);
  const source = sourceOf(record);
  return text(fields.slug ?? fields.key ?? source.slug ?? source.key ?? fields.name ?? source.name);
}

function sprintRefs(record) {
  const fields = fieldsOf(record);
  const source = sourceOf(record);
  return [fields.slug, fields.key, fields.id, source.slug, source.key, source.id, fields.name, source.name]
    .map(value => text(value).toLowerCase()).filter(Boolean);
}

function ticketIsSprintMember(ticket, sprint) {
  const wanted = new Set(sprintRefs(sprint));
  const value = text(canonicalOf(ticket).sprint).toLowerCase();
  return value !== '' && wanted.has(value);
}

function workflowCategoryFor(index, project, value) {
  const requested = text(value).toLowerCase();
  const record = configRecords(index, project, 'workflow')[0];
  const statuses = array(fieldsOf(record).statuses);
  const found = statuses.find(status => [status?.id, status?.statusId, status?.status_id, status?.name, status?.status]
    .some(candidate => text(candidate).toLowerCase() === requested));
  return upper(found?.category ?? found?.statusCategory ?? found?.status_category ?? '');
}

function issueIsDone(index, project, ticket) {
  const fields = canonicalOf(ticket);
  const source = sourceOf(ticket);
  const category = upper(fields.statusCategory ?? fields.status_category ?? source.statusCategory ?? source.status_category);
  if (category) return category === 'DONE';
  const status = text(fields.status ?? source.status);
  return workflowCategoryFor(index, project, status) === 'DONE' || status.toUpperCase() === 'DONE';
}

function sprintMembers(index, project, sprint) {
  return allTicketRecords(index).filter(ticket => upper(ticket?.project) === upper(project)
    && ticketIsSprintMember(ticket, sprint));
}

function dateValue(value, label) {
  if (value === undefined || value === null || value === '') return null;
  const result = assertText(value, label, { max: 100 });
  const parsed = new Date(result);
  if (Number.isNaN(parsed.getTime())) fail('INVALID', label + ' must be a valid date');
  return parsed.toISOString();
}

function sprintStart(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'sprint id', { max: 200 });
  const current = sprintTarget(index, project, id);
  if (!current) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
  return runAction(index, request, 'sprint.start', async fresh => {
    const found = sprintTarget(fresh, project, id);
    if (!found) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
    const status = sprintStatusOf(found);
    if (status !== 'PLANNED') {
      fail('INVALID', status === 'COMPLETED'
        ? `Sprint ${text(fieldsOf(found).name, id)} is already completed`
        : `Sprint ${text(fieldsOf(found).name, id)} is already active`);
    }
    const active = [...(fresh?.sprints?.values?.() ?? [])].find(record => upper(record?.project) === project
      && sprintStatusOf(record) === 'ACTIVE');
    if (active) fail('INVALID', `Sprint ${text(fieldsOf(active).name, active.relativePath)} is already active; complete it first`);
    const requested = Object.hasOwn(request.data, 'startDate') ? dateValue(request.data.startDate, 'sprint startDate') : null;
    const startDate = requested ?? new Date().toISOString();
    return {
      changes: [{ path: found.relativePath, bytes: patchRecord(found, { status: 'ACTIVE', startDate }) }],
      result: { id, name: text(fieldsOf(found).name, id), status: 'ACTIVE', startDate },
    };
  }, { expected: actionExpected(request, [{ record: current, path: current.relativePath }]) });
}

function sprintComplete(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'sprint id', { max: 200 });
  const current = sprintTarget(index, project, id);
  if (!current) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
  const initialMembers = sprintMembers(index, project, current);
  const initialIncomplete = initialMembers.filter(ticket => !issueIsDone(index, project, ticket));
  const targets = [{ record: current, path: current.relativePath }, ...initialIncomplete.map(ticket => ({ record: ticket, path: ticket.relativePath }))];
  const destinationProvided = Object.hasOwn(request.data, 'destinationSprintId');
  const destinationInput = destinationProvided ? request.data.destinationSprintId : null;
  if (destinationProvided && destinationInput !== null && typeof destinationInput !== 'string') fail('INVALID', 'destinationSprintId must be a sprint ID or null');
  if (!destinationProvided && initialIncomplete.length) fail('INVALID', 'destinationSprintId is required while the sprint has unfinished issues');
  return runAction(index, request, 'sprint.complete', async fresh => {
    const found = sprintTarget(fresh, project, id);
    if (!found) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
    if (sprintStatusOf(found) !== 'ACTIVE') fail('INVALID', `Only an active sprint can be completed (current status: ${sprintStatusOf(found)})`);
    const members = sprintMembers(fresh, project, found);
    const unfinished = members.filter(ticket => !issueIsDone(fresh, project, ticket));
    let destination = null;
    if (unfinished.length) {
      if (!destinationProvided) fail('INVALID', 'destinationSprintId is required while the sprint has unfinished issues');
      if (destinationInput !== null) {
        destination = sprintTarget(fresh, project, destinationInput);
        if (!destination) fail('NOT_FOUND', 'Destination sprint ' + destinationInput + ' was not found in ' + project);
        if (destination.relativePath === found.relativePath) fail('INVALID', 'A sprint cannot carry unfinished issues into itself');
        if (sprintStatusOf(destination) !== 'PLANNED') fail('INVALID', 'Incomplete issues can only move to a planned sprint');
      }
    } else if (destinationInput !== null && destinationProvided) {
      destination = sprintTarget(fresh, project, destinationInput);
      if (!destination) fail('NOT_FOUND', 'Destination sprint ' + destinationInput + ' was not found in ' + project);
      if (destination.relativePath === found.relativePath) fail('INVALID', 'A sprint cannot carry unfinished issues into itself');
      if (sprintStatusOf(destination) !== 'PLANNED') fail('INVALID', 'Incomplete issues can only move to a planned sprint');
    }
    const destinationRef = destination ? sprintSlugOf(destination) : null;
    const changes = [{ path: found.relativePath, bytes: patchRecord(found, { status: 'COMPLETED', completedAt: new Date().toISOString() }) }];
    for (const ticket of unfinished) changes.push({ path: ticket.relativePath, bytes: patchRecord(ticket, { sprint: destinationRef }) });
    return {
      changes,
      result: {
        id,
        name: text(fieldsOf(found).name, id),
        status: 'COMPLETED',
        movedToBacklog: destination ? 0 : unfinished.length,
        movedToSprint: destination ? destinationRef : null,
      },
    };
  }, { expected: actionExpected(request, targets) });
}

function sprintUpdate(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'sprint id', { max: 200 });
  const current = sprintTarget(index, project, id);
  if (!current) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
  const existing = fieldsOf(current);
  const name = request.data.name === undefined ? assertText(text(existing.name, id), 'sprint name', { max: 200 }) : assertText(request.data.name, 'sprint name', { max: 200 });
  const goal = request.data.goal === undefined ? (existing.goal ?? null) : assertText(request.data.goal, 'sprint goal', { required: false, max: 10000 });
  const startDate = request.data.startDate === undefined ? (existing.startDate ?? existing.start_date ?? null) : dateValue(request.data.startDate, 'sprint startDate');
  const endDate = request.data.endDate === undefined ? (existing.endDate ?? existing.end_date ?? null) : dateValue(request.data.endDate, 'sprint endDate');
  if (startDate && endDate && new Date(startDate) > new Date(endDate)) fail('INVALID', 'Sprint startDate must be on or before endDate');
  return runAction(index, request, 'sprint.update', async fresh => {
    const found = sprintTarget(fresh, project, id);
    if (!found) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
    return { changes: [{ path: found.relativePath, bytes: patchRecord(found, { name, goal: goal || null, startDate: startDate || null, endDate: endDate || null }) }], result: { id, name, updated: true } };
  }, { expected: actionExpected(request, [{ record: current, path: current.relativePath }]) });
}

function sprintDelete(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'sprint id', { max: 200 });
  const current = sprintTarget(index, project, id);
  if (!current) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
  const currentStatus = sprintStatusOf(current);
  if (currentStatus === 'ACTIVE') fail('INVALID', 'Cannot delete an active sprint. Complete it first.');
  if (!['PLANNED', 'COMPLETED'].includes(currentStatus)) fail('INVALID', 'Only planned or completed sprints can be deleted');
  return runAction(index, request, 'sprint.delete', async fresh => {
    const found = sprintTarget(fresh, project, id);
    if (!found) fail('NOT_FOUND', 'Sprint ' + id + ' was not found in ' + project);
    const status = sprintStatusOf(found);
    if (status === 'ACTIVE') fail('INVALID', 'Cannot delete an active sprint. Complete it first.');
    if (!['PLANNED', 'COMPLETED'].includes(status)) fail('INVALID', 'Only planned or completed sprints can be deleted');
    const slug = text(fieldsOf(found).slug ?? fieldsOf(found).key ?? id);
    const changes = [{ path: found.relativePath, bytes: null }];
    for (const ticket of allTicketRecords(fresh)) {
      if (upper(ticket.project) === project && text(canonicalOf(ticket).sprint) === slug) changes.push({ path: ticket.relativePath, bytes: patchRecord(ticket, { sprint: null }) });
    }
    return { changes, result: { id, deleted: true } };
  }, { expected: actionExpected(request, [{ record: current, path: current.relativePath }]) });
}

function sprintReorder(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const order = array(request.data.order);
  if (!order.length || order.some(item => typeof item !== 'string' || !item.trim())) fail('INVALID', 'sprint order must be a non-empty array of IDs');
  const initialRecords = [...(index.sprints?.values?.() ?? [])].filter(record => upper(record.project) === project);
  const reorderTargets = initialRecords.map(record => ({ record, path: record.relativePath }));
  return runAction(index, request, 'sprint.reorder', async fresh => {
    const records = [...(fresh.sprints?.values?.() ?? [])].filter(record => upper(record.project) === project);
    const byId = new Map();
    for (const record of records) {
      const fields = fieldsOf(record);
      const source = object(fields.source?.source ?? fields.source ?? sourceOf(record));
      for (const id of [fields.slug, fields.key, source.id]) if (id) byId.set(String(id), record);
    }
    if (order.length !== records.length || new Set(order.map(item => String(item).toLowerCase())).size !== records.length
      || records.some(record => !order.some(item => sprintRefs(record).includes(String(item).toLowerCase())))) {
      fail('INVALID', 'sprint order must list every project sprint exactly once');
    }
    const changes = [];
    for (const [position, item] of order.entries()) {
      const record = byId.get(String(item));
      if (!record || Number(fieldsOf(record).position) === position) continue;
      changes.push({ path: record.relativePath, bytes: patchRecord(record, { position }) });
    }
    return { changes, result: { order, updated: true } };
  }, { expected: actionExpected(request, reorderTargets) });
}

function versionTarget(index, project, id) {
  const record = configRecords(index, project, 'versions')[0];
  if (!record) return { record: null, row: null, path: null };
  return { record, row: rowsOf(record).find(item => matchesId(item, id)), path: record.relativePath };
}

function versionUpdate(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'version id', { max: 200 });
  const target = versionTarget(index, project, id);
  if (!target.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
  const name = assertText(request.data.name, 'version name', { max: 200 });
  const description = assertText(request.data.description, 'version description', { required: false, max: 10000 });
  const releaseDate = assertText(request.data.releaseDate, 'version releaseDate', { required: false, max: 100 });
  return runAction(index, request, 'version.update', async fresh => {
    const found = versionTarget(fresh, project, id);
    if (!found.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
    const rows = rowsOf(found.record).map(row => matchesId(row, id) ? { ...row, name, description: description || null, releaseDate: releaseDate || null, release_date: releaseDate || null } : row);
    return { changes: [{ path: found.path, bytes: patchRecord(found.record, { rows }) }], result: { id, name, updated: true } };
  }, { expected: actionExpected(request, [{ record: target.record, path: target.path }]) });
}

function versionStatus(index, request, status) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'version id', { max: 200 });
  const target = versionTarget(index, project, id);
  if (!target.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
  return runAction(index, request, 'version.' + status.toLowerCase(), async fresh => {
    const found = versionTarget(fresh, project, id);
    if (!found.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
    const rows = rowsOf(found.record).map(row => matchesId(row, id) ? { ...row, status } : row);
    return { changes: [{ path: found.path, bytes: patchRecord(found.record, { rows }) }], result: { id, status, updated: true } };
  }, { expected: actionExpected(request, [{ record: target.record, path: target.path }]) });
}

function versionDelete(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const id = assertText(request.data.id ?? request.key, 'version id', { max: 200 });
  const target = versionTarget(index, project, id);
  if (!target.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
  const name = text(target.row.name);
  return runAction(index, request, 'version.delete', async fresh => {
    const found = versionTarget(fresh, project, id);
    if (!found.row) fail('NOT_FOUND', 'Version ' + id + ' was not found in ' + project);
    const changes = [{ path: found.path, bytes: patchRecord(found.record, { rows: rowsOf(found.record).filter(row => !matchesId(row, id)) }) }];
    for (const ticket of allTicketRecords(fresh)) {
      if (upper(ticket.project) !== project) continue;
      const fields = canonicalOf(ticket);
      const updates = {};
      for (const field of ['fixVersions', 'affectsVersions']) {
        if (!Array.isArray(fields[field])) continue;
        if (fields[field].some(value => text(value?.name ?? value?.id ?? value).toLowerCase() === name.toLowerCase())) {
          updates[field] = fields[field].filter(value => text(value?.name ?? value?.id ?? value).toLowerCase() !== name.toLowerCase());
        }
      }
      if (Object.keys(updates).length) changes.push({ path: ticket.relativePath, bytes: patchRecord(ticket, updates) });
    }
    return { changes, result: { id, deleted: true } };
  }, { expected: actionExpected(request, [{ record: target.record, path: target.path }]) });
}

function versionReorder(index, request) {
  const project = validateProjectKey(request.project);
  ensureProject(index, project);
  const order = array(request.data.order);
  if (!order.length || order.some(item => typeof item !== 'string' || !item.trim())) fail('INVALID', 'version order must be a non-empty array of IDs');
  const target = versionTarget(index, project, order[0]);
  if (!target.record) fail('NOT_FOUND', 'No versions catalog for ' + project);
  return runAction(index, request, 'version.reorder', async fresh => {
    const record = configRecords(fresh, project, 'versions')[0];
    if (!record) fail('NOT_FOUND', 'No versions catalog for ' + project);
    const rows = rowsOf(record).map((row, position) => {
      const next = order.findIndex(item => matchesId(row, item));
      return { ...row, position: next < 0 ? position : next };
    }).sort((a, b) => Number(a.position) - Number(b.position));
    return { changes: [{ path: record.relativePath, bytes: patchRecord(record, { rows }) }], result: { order, updated: true } };
  }, { expected: actionExpected(request, [{ record: target.record, path: target.path }]) });
}

export async function dispatchProjectAction(index, input, context = {}) {
  const request = validateRequest(input);
  if (ISSUE_INTERACTION_ACTIONS.has(request.action)) return dispatchIssueInteraction(index, request, context);
  if (SETTINGS_ACTIONS.has(request.action)) return dispatchSettingsAction(index, request, context);
  if (request.action === 'project.create') return projectCreate(index, request);
  if (request.action === 'issue.create') return issueCreate(index, request);
  if (request.action === 'issue.update') return issueUpdate(index, request, context);
  if (request.action.startsWith('sprint.')) {
    if (request.action === 'sprint.start') return sprintStart(index, request);
    if (request.action === 'sprint.complete') return sprintComplete(index, request);
    if (request.action === 'sprint.update') return sprintUpdate(index, request);
    if (request.action === 'sprint.delete') return sprintDelete(index, request);
    return sprintReorder(index, request);
  }
  if (request.action.startsWith('version.')) {
    if (request.action === 'version.update') return versionUpdate(index, request);
    if (request.action === 'version.archive') return versionStatus(index, request, 'ARCHIVED');
    if (request.action === 'version.delete') return versionDelete(index, request);
    return versionReorder(index, request);
  }
  fail('INVALID', 'Unsupported project action: ' + request.action);
}

export { ACTIONS, validateRequest };
export default dispatchProjectAction;
