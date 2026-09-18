/*
 * Project settings actions.
 *
 * Settings are part of the Vault document model, so the mutation boundary is
 * the same executeOperation() boundary used by the issue writer.  Imported
 * relational data lives in metadata.md; edits below update that source in
 * place when it exists.  This is deliberate: writing a second overlay for a
 * row that came from the import would make deletes reappear on the next
 * reload and would make the UI show duplicate fields, screens, or columns.
 */

import { randomUUID } from 'node:crypto';

import { allTicketRecords, canonicalOf, clone } from '../cli/compat.mjs';
import { bytesOf, patchRecord } from '../cli/write.mjs';
import { executeOperation } from './operations.mjs';
import {
  JIRA_ISSUE_TYPES,
  JIRA_STATUS_CATEGORIES,
} from '../shared/jira/constants.js';
import {
  BUILTIN_FIELDS,
  FIELD_REQUIREMENTS,
  customFieldError,
  defaultRequirement,
  fieldConfigError,
  fieldRefError,
  fieldRefKey,
} from '../shared/jira/fields.js';
import {
  JIRA_BOARD_TYPES,
  JIRA_CARD_FIELDS,
  JIRA_SWIMLANE_MODES,
  boardFilterError,
  normalizeCardFields,
} from '../shared/jira/board.js';
import {
  TRANSITION_RULE_KINDS,
  TRANSITION_RULE_TYPES,
  transitionRuleError,
} from '../shared/jira/workflow.js';

const PROJECT_KEY = /^[A-Z][A-Z0-9]{0,9}$/;
const REQUIREMENTS = new Set(FIELD_REQUIREMENTS);
const ISSUE_TYPES = new Set(JIRA_ISSUE_TYPES);
const STATUS_CATEGORIES = new Set(JIRA_STATUS_CATEGORIES);
const BOARD_TYPES = new Set(JIRA_BOARD_TYPES);
const SWIMLANES = new Set(JIRA_SWIMLANE_MODES);
const CARD_FIELDS = new Set(JIRA_CARD_FIELDS);
const TABLE_KINDS = Object.freeze({
  customFields: 'customFields',
  fieldConfiguration: 'fieldConfiguration',
  screens: 'screens',
  board: 'board',
});

export const SETTINGS_ACTIONS = new Set([
  'workflow.status.create',
  'workflow.status.update',
  'workflow.status.delete',
  'workflow.status.reorder',
  'workflow.transition.create',
  'workflow.transition.delete',
  'workflow.transition.rule.create',
  'workflow.transition.rule.delete',
  'workflow.transition.screen',
  'field.requirement.set',
  'field.create',
  'field.delete',
  'screen.create',
  'screen.delete',
  'screen.fields.set',
  'board.settings.update',
  'board.column.create',
  'board.column.update',
  'board.column.delete',
  'board.column.reorder',
  'board.column.status.map',
  'board.column.status.unmap',
  'board.quick-filter.create',
  'board.quick-filter.delete',
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

function projectKey(value) {
  const key = upper(value);
  if (!PROJECT_KEY.test(key)) fail('INVALID', 'project must match /^[A-Z][A-Z0-9]{0,9}$/');
  return key;
}

function recordFields(record) {
  return object(record?.fields ?? record?.metadata ?? {});
}

function rootFor(index, project) {
  const key = upper(project);
  return array(index?.boardRoots).find(item => upper(item?.project) === key) ?? null;
}

function ensureProject(index, project) {
  const root = rootFor(index, project);
  const hasTicket = allTicketRecords(index).some(record => upper(record?.project) === upper(project));
  const hasProject = tableRows(index, 'jira.projects').some(row => upper(row?.key ?? row?.project_key) === upper(project));
  if (!root && !hasTicket && !hasProject) fail('NOT_FOUND', `Project ${project} was not found`);
  if (root?.readOnly === true) fail('READ_ONLY', `Project ${project} is read-only`);
  return root;
}

function projectId(index, project) {
  const row = tableRows(index, 'jira.projects').find(item => upper(item?.key ?? item?.project_key ?? item?.projectKey) === upper(project));
  return row?.id === undefined || row?.id === null ? null : String(row.id);
}

function tableRows(index, name) {
  const rows = index?.tables?.get?.(name);
  return Array.isArray(rows) ? rows : [];
}

function relativeProjectPath(root, suffix) {
  const base = String(root?.path ?? '.').replaceAll('\\', '/').replace(/\/+$/, '');
  return (base ? base + '/' : '') + suffix.replace(/^\/+/, '');
}

function yamlConfig(fields) {
  return Buffer.from('---\n' + Object.entries(fields)
    .map(([key, value]) => key + ': ' + JSON.stringify(value) + '\n').join('') + '---\n', 'utf8');
}

function configRecords(index, project, kind) {
  const wanted = String(kind).toLowerCase();
  const owner = upper(project);
  return [...(index?.configs?.values?.() ?? [])].filter(record => {
    if (record?.kind === 'metadata') return false;
    const fields = recordFields(record);
    const recordKind = String(fields.kind ?? fields.entity ?? '').toLowerCase();
    if (recordKind !== wanted && !String(record.relativePath ?? '').toLowerCase().endsWith('/' + wanted + '.md')) return false;
    const recordOwner = upper(record.project ?? fields.project);
    return recordOwner === owner;
  });
}

function metadataRecords(index) {
  const records = [...(index?.metadataFiles?.values?.() ?? [])];
  if (records.length) return records;
  return [...(index?.configs?.values?.() ?? [])].filter(record => record?.kind === 'metadata');
}

function metadataRecord(index, table) {
  return metadataRecords(index).find(record => Object.hasOwn(recordFields(record).tables ?? {}, table)) ?? null;
}

function fileRecord(index, relativePath) {
  return index?.getFile?.(relativePath) ?? index?.files?.get?.(relativePath) ?? null;
}

function workflowRecord(index, project) {
  const record = configRecords(index, project, 'workflow')[0];
  if (!record) fail('NOT_FOUND', `No workflow catalog for ${project}`);
  return record;
}

function fallbackConfigPath(index, project, kind) {
  const root = rootFor(index, project);
  if (!root) fail('NOT_FOUND', `Project ${project} was not found`);
  return relativeProjectPath(root, `config/${kind}.md`);
}

function targetForTable(index, project, table, fallbackKind = table) {
  const metadata = metadataRecord(index, table);
  if (metadata) return { path: metadata.relativePath, record: metadata, mode: 'metadata', table };
  const config = configRecords(index, project, fallbackKind)[0];
  if (config) return { path: config.relativePath, record: config, mode: 'config', table, kind: fallbackKind };
  return { path: fallbackConfigPath(index, project, fallbackKind), record: null, mode: 'config', table, kind: fallbackKind };
}

function targetForMetadataTableIfPresent(index, project, table, fallbackKind = table) {
  const metadata = metadataRecord(index, table);
  return metadata ? targetForTable(index, project, table, fallbackKind) : null;
}

function targetForWorkflow(index, project) {
  const record = workflowRecord(index, project);
  return { path: record.relativePath, record, mode: 'workflow' };
}

function uniqueTargets(targets) {
  const map = new Map();
  for (const target of targets.filter(Boolean)) {
    if (!map.has(String(target.path).toLowerCase())) map.set(String(target.path).toLowerCase(), target);
  }
  return [...map.values()];
}

function expectedFor(request, targets) {
  const expected = { ...object(request.revisions) };
  for (const target of uniqueTargets(targets)) {
    if (Object.hasOwn(expected, target.path)) continue;
    if (target.record) fail('STALE', `An opened revision is required for ${target.path}`);
    expected[target.path] = null;
  }
  return expected;
}

function operationId(request) {
  const value = request.operationId;
  if (value === undefined || value === null || value === '') return randomUUID();
  if (typeof value !== 'string' || !/^[A-Za-z0-9_-]{1,100}$/.test(value)) fail('ID', 'Invalid operation ID');
  return value;
}

function runOperation(index, request, action, targets, prepare, { allowPaths = [] } = {}) {
  ensureProject(index, request.project);
  const actualTargets = uniqueTargets(targets);
  const expected = expectedFor(request, actualTargets);
  const id = operationId(request);
  return executeOperation(index.root, {
    kind: 'settings:' + action,
    id,
    fingerprint: JSON.stringify({ action, project: request.project, key: request.key ?? null, data: request.data ?? {}, revisions: request.revisions ?? {} }),
    expected,
    allowPaths: [rootFor(index, request.project)?.path ?? request.project, ...allowPaths],
    prepare,
  });
}

function sourceRows(index, target) {
  if (target.mode === 'metadata') return array(recordFields(target.record).tables?.[target.table]);
  return array(recordFields(target.record).rows);
}

function writeTableChanges(index, project, updates, kinds = {}) {
  const targets = uniqueTargets(Object.keys(updates).map(table => targetForTable(index, project, table, kinds[table] ?? table)));
  const grouped = new Map();
  for (const table of Object.keys(updates)) {
    const target = targetForTable(index, project, table, kinds[table] ?? table);
    const key = target.path.toLowerCase();
    if (!grouped.has(key)) grouped.set(key, { target, updates: {} });
    grouped.get(key).updates[table] = clone(updates[table]);
  }
  const changes = [];
  for (const { target, updates: tableUpdates } of grouped.values()) {
    if (target.mode === 'metadata') {
      const fields = clone(recordFields(target.record));
      fields.tables = clone(fields.tables ?? {});
      for (const [table, rows] of Object.entries(tableUpdates)) fields.tables[table] = rows;
      changes.push({ path: target.path, bytes: patchRecord(target.record, { tables: fields.tables }) });
    } else {
      const fields = target.record ? clone(recordFields(target.record)) : { kind: target.kind ?? target.table, rows: [] };
      const first = Object.keys(tableUpdates)[0];
      fields.kind ??= target.kind ?? first;
      fields.rows = clone(tableUpdates[first]);
      changes.push({ path: target.path, bytes: target.record ? patchRecord(target.record, fields) : yamlConfig(fields) });
    }
  }
  return { targets, changes };
}

function tableTargetRows(index, project, table, fallbackKind = table) {
  const target = targetForTable(index, project, table, fallbackKind);
  return { target, rows: clone(sourceRows(index, target)) };
}

function directOwner(row, project, pid) {
  const direct = row?.project ?? row?.project_key ?? row?.projectKey;
  if (direct !== undefined && direct !== null && direct !== '') return upper(direct) === upper(project);
  const rowPid = row?.project_id ?? row?.projectId;
  return rowPid !== undefined && rowPid !== null && pid !== null && String(rowPid) === String(pid);
}

function boardContext(index, project) {
  const pid = projectId(index, project);
  const boards = tableRows(index, 'jira.boards').filter(row => directOwner(row, project, pid));
  const boardIds = new Set(boards.map(row => text(row?.id)).filter(Boolean));
  const columns = tableRows(index, 'jira.board_columns').filter(row => boardIds.has(text(row?.board_id ?? row?.boardId)));
  const columnIds = new Set(columns.map(row => text(row?.id)).filter(Boolean));
  return { projectId: pid, boards, boardIds, columns, columnIds, board: boards[0] ?? null };
}

function workflowValues(index, project) {
  const record = workflowRecord(index, project);
  const fields = recordFields(record);
  const statuses = array(fields.statuses).map((row, position) => ({ ...object(row), _position: Number.isFinite(Number(row?.position)) ? Number(row.position) : position }));
  const transitions = array(fields.transitions).map((row, position) => ({ ...object(row), _position: Number.isFinite(Number(row?.position)) ? Number(row.position) : position }));
  return { record, fields, statuses, transitions };
}

function statusId(status) { return text(status?.id ?? status?.statusId ?? status?.status_id ?? status?.name); }
function statusName(status) { return text(status?.name ?? status?.status ?? status?.status_name ?? status?.id); }
function statusCategory(status) { return upper(status?.category ?? status?.statusCategory ?? status?.status_category ?? 'TODO'); }
function transitionId(edge, position = 0) { return text(edge?.id ?? edge?.transitionId, 'transition-' + (position + 1)); }
function fromStatus(edge) { return edge?.fromStatusId ?? edge?.from_status_id ?? edge?.from ?? null; }
function toStatus(edge) { return edge?.toStatusId ?? edge?.to_status_id ?? edge?.to ?? null; }

function findStatus(statuses, value) {
  const wanted = text(value).toLowerCase();
  return statuses.find((row, position) => statusId(row).toLowerCase() === wanted
    || statusName(row).toLowerCase() === wanted
    || String(position + 1) === wanted) ?? null;
}

function findTransition(transitions, value) {
  const wanted = text(value).toLowerCase();
  return transitions.find((row, position) => transitionId(row, position).toLowerCase() === wanted) ?? null;
}

function sameRef(left, right) {
  return text(left).toLowerCase() === text(right).toLowerCase();
}

function customFieldRows(index, project) {
  const target = targetForTable(index, project, 'jira.custom_fields', TABLE_KINDS.customFields);
  return sourceRows(index, target).filter(row => directOwner(row, project, projectId(index, project)));
}

function customFieldById(index, project, value) {
  const wanted = text(value).toLowerCase();
  return customFieldRows(index, project).find(row => [row?.id, row?.key, row?.name].some(candidate => text(candidate).toLowerCase() === wanted)) ?? null;
}

function screenRows(index, project) {
  const target = targetForTable(index, project, 'jira.screens', TABLE_KINDS.screens);
  return sourceRows(index, target).filter(row => directOwner(row, project, projectId(index, project)));
}

function screenById(index, project, value) {
  const wanted = text(value).toLowerCase();
  return screenRows(index, project).find(row => [row?.id, row?.key, row?.name].some(candidate => text(candidate).toLowerCase() === wanted)) ?? null;
}

function screenIdOf(row) { return text(row?.id ?? row?.key ?? row?.name); }

function fieldReference(data) {
  const builtinField = data?.builtinField ?? data?.builtin_field ?? null;
  const customFieldId = data?.customFieldId ?? data?.custom_field_id ?? null;
  return {
    builtinField: builtinField === null || builtinField === '' ? null : assertText(builtinField, 'builtinField', { max: 100 }),
    customFieldId: customFieldId === null || customFieldId === '' ? null : assertText(customFieldId, 'customFieldId', { max: 200 }),
  };
}

function ensureFieldReference(index, project, reference) {
  const invalid = fieldRefError(reference);
  if (invalid) fail('INVALID', invalid);
  if (reference.customFieldId && !customFieldById(index, project, reference.customFieldId)) fail('NOT_FOUND', 'Custom field ' + reference.customFieldId + ' was not found in ' + project);
  return reference;
}

function fieldRefMatches(row, reference) {
  const rowRef = {
    builtinField: row?.builtinField ?? row?.builtin_field ?? null,
    customFieldId: row?.customFieldId ?? row?.custom_field_id ?? null,
  };
  return fieldRefKey(rowRef) === fieldRefKey(reference);
}

function tableNamesIfPresent(index, names) {
  return names.filter(name => Boolean(metadataRecord(index, name)));
}

function updateTicketsForStatus(index, project, oldName, nextName, category) {
  const changes = [];
  for (const ticket of allTicketRecords(index)) {
    if (upper(ticket?.project) !== upper(project)) continue;
    const fields = canonicalOf(ticket);
    if (!sameRef(fields.status, oldName)) continue;
    changes.push({ path: ticket.relativePath, bytes: patchRecord(ticket, { status: nextName, statusCategory: category }) });
  }
  return changes;
}

function ticketUsesStatus(index, project, name) {
  return allTicketRecords(index).some(ticket => upper(ticket?.project) === upper(project) && sameRef(canonicalOf(ticket).status, name));
}

function validateRule(data) {
  const kind = assertText(data.kind, 'rule kind', { max: 50 }).toUpperCase();
  const type = assertText(data.type ?? data.ruleType, 'rule type', { max: 100 }).toUpperCase();
  const config = object(data.config);
  if (!TRANSITION_RULE_KINDS.includes(kind)) fail('INVALID', `Unknown rule kind "${kind}"`);
  if (!TRANSITION_RULE_TYPES[kind]?.includes(type)) fail('INVALID', `"${type}" is not a valid ${kind}`);
  const invalid = transitionRuleError({ kind, type, config });
  if (invalid) fail('INVALID', invalid);
  return { kind, type, config };
}

function normalizeRequest(input) {
  const request = object(input);
  const action = assertText(request.action, 'action', { max: 100 });
  if (!SETTINGS_ACTIONS.has(action)) fail('INVALID', 'Unsupported settings action: ' + action);
  const project = projectKey(request.project ?? request.key);
  const data = request.data === undefined ? {} : object(request.data);
  if (request.data !== undefined && (!request.data || typeof request.data !== 'object' || Array.isArray(request.data))) fail('INVALID', 'data must be an object');
  return { ...request, action, project, data, revisions: object(request.revisions) };
}

function settingsTargets(index, request) {
  const { action, project, data } = request;
  const targets = [];
  const addWorkflow = () => targets.push(targetForWorkflow(index, project));
  const addTables = (names, kinds = {}) => {
    for (const name of names) targets.push(targetForTable(index, project, name, kinds[name] ?? name));
  };

  if (action.startsWith('workflow.')) {
    addWorkflow();
    if (action === 'workflow.transition.rule.create' || action === 'workflow.transition.rule.delete'
      || action === 'workflow.transition.delete' || action === 'workflow.status.delete') {
      const ruleTarget = targetForMetadataTableIfPresent(index, project, 'jira.workflow_transition_rules');
      if (ruleTarget) targets.push(ruleTarget);
    }
    return targets;
  }
  if (action.startsWith('board.')) {
    // Relational board tables share metadata.md in an imported snapshot.
    // A hand-authored project may instead have a project config/board.md.
    if (metadataRecord(index, 'jira.boards')) {
      addTables(['jira.boards', 'jira.board_columns', 'jira.board_column_statuses', 'jira.board_quick_filters']);
    } else {
      targets.push(targetForTable(index, project, 'board', TABLE_KINDS.board));
    }
    return targets;
  }
  if (action === 'field.create') {
    addTables(['jira.custom_fields'], { 'jira.custom_fields': TABLE_KINDS.customFields });
    if (metadataRecord(index, 'jira.custom_field_options')) addTables(['jira.custom_field_options']);
    return targets;
  }
  if (action === 'field.delete') {
    addTables(['jira.custom_fields'], { 'jira.custom_fields': TABLE_KINDS.customFields });
    // Config mode writes jira.screens (embedded screen-field cleanup) and can
    // rewrite jira.field_configurations, so those records are CAS-protected
    // when present; an expected-absent target passes when the file does not
    // exist.
    targets.push(targetForTable(index, project, 'jira.screens', TABLE_KINDS.screens));
    targets.push(targetForTable(index, project, 'jira.field_configurations', TABLE_KINDS.fieldConfiguration));
    addTables(tableNamesIfPresent(index, [
      'jira.custom_field_options', 'jira.custom_field_values',
      'jira.screen_fields',
      'jira.workflow_transition_rules',
    ]));
    addWorkflowIfNeeded(index, project, targets);
    return targets;
  }
  if (action === 'field.requirement.set') {
    addTables(['jira.field_configurations'], { 'jira.field_configurations': TABLE_KINDS.fieldConfiguration });
    return targets;
  }
  if (action === 'screen.create') {
    addTables(['jira.screens']);
    if (metadataRecord(index, 'jira.screen_fields')) addTables(['jira.screen_fields']);
    return targets;
  }
  if (action === 'screen.fields.set') {
    addTables(['jira.screens']);
    if (metadataRecord(index, 'jira.screen_fields')) addTables(['jira.screen_fields']);
    return targets;
  }
  if (action === 'screen.delete') {
    addTables(['jira.screens']);
    if (metadataRecord(index, 'jira.screen_fields')) addTables(['jira.screen_fields']);
    addWorkflowIfNeeded(index, project, targets);
    return targets;
  }
  return targets;
}

function addWorkflowIfNeeded(index, project, targets) {
  const workflow = configRecords(index, project, 'workflow')[0];
  if (workflow) targets.push({ path: workflow.relativePath, record: workflow, mode: 'workflow' });
}

function boardConfigFallback(index, project) {
  const record = configRecords(index, project, TABLE_KINDS.board)[0];
  if (!record) return null;
  const fields = recordFields(record);
  return {
    record,
    path: record.relativePath,
    fields: clone(fields),
    columns: array(fields.columns).map(row => ({ ...object(row) })),
    quickFilters: array(fields.quickFilters ?? fields.quick_filters).map(row => ({ ...object(row) })),
  };
}

function boardRowsFromConfig(index, project) {
  const fallback = boardConfigFallback(index, project);
  if (!fallback) return null;
  const board = {
    id: text(fallback.fields.id, upper(project).toLowerCase() + '-board'),
    name: text(fallback.fields.name, 'Board'),
    type: upper(fallback.fields.type ?? 'KANBAN'),
    swimlane_by: upper(fallback.fields.swimlaneBy ?? fallback.fields.swimlane_by ?? 'NONE'),
    filter: fallback.fields.filter ?? null,
    card_fields: fallback.fields.cardFields ?? fallback.fields.card_fields ?? null,
  };
  return { fallback, board, columns: fallback.columns, quickFilters: fallback.quickFilters };
}

function boardInfo(index, project) {
  const relational = boardContext(index, project);
  if (relational.board) return { mode: 'metadata', ...relational };
  const config = boardRowsFromConfig(index, project);
  if (config) return { mode: 'config', ...config, projectId: projectId(index, project) };
  return { mode: 'metadata', ...relational };
}

function boardColumnId(row) { return text(row?.id ?? row?.key ?? row?.name); }
function boardFilterOf(row) { return row?.filter ?? null; }
function columnBoardId(row) { return text(row?.board_id ?? row?.boardId); }
function mappingColumnId(row) { return text(row?.column_id ?? row?.columnId); }
function mappingStatusId(row) { return text(row?.status_id ?? row?.statusId); }

function normalizeIssueType(value) {
  const result = upper(assertText(value, 'issueType', { max: 30 }));
  if (!ISSUE_TYPES.has(result)) fail('INVALID', `Unknown issue type ${result}`);
  return result;
}

function validateRequirement(data, index, project) {
  const issueType = normalizeIssueType(data.issueType ?? data.issue_type);
  const reference = ensureFieldReference(index, project, fieldReference(data));
  const requirement = upper(assertText(data.requirement, 'requirement', { max: 30 }));
  if (!REQUIREMENTS.has(requirement)) fail('INVALID', 'Unknown field requirement ' + requirement);
  const invalid = fieldConfigError({ ...reference, issueType, requirement });
  if (invalid) fail('INVALID', invalid);
  return { issueType, reference, requirement };
}

function requirementIdentity(row) {
  return upper(row?.issueType ?? row?.issue_type) + '\u0000' + fieldRefKey({
    builtinField: row?.builtinField ?? row?.builtin_field ?? null,
    customFieldId: row?.customFieldId ?? row?.custom_field_id ?? null,
  });
}

function tableRowsForProject(index, project, table, kind = table) {
  const { target, rows } = tableTargetRows(index, project, table, kind);
  return { target, rows };
}

function rowId(row, fallback = '') { return text(row?.id ?? row?.key ?? row?.name, fallback); }

function relationSets(index, project) {
  const pid = projectId(index, project);
  const boards = tableRows(index, 'jira.boards').filter(row => directOwner(row, project, pid));
  const boardIds = new Set(boards.map(row => rowId(row)).filter(Boolean));
  const columns = tableRows(index, 'jira.board_columns').filter(row => boardIds.has(columnBoardId(row)));
  const columnIds = new Set(columns.map(row => rowId(row)).filter(Boolean));
  const screens = tableRows(index, 'jira.screens').filter(row => directOwner(row, project, pid));
  const screenIds = new Set(screens.map(row => rowId(row)).filter(Boolean));
  const fields = tableRows(index, 'jira.custom_fields').filter(row => directOwner(row, project, pid));
  const fieldIds = new Set(fields.map(row => rowId(row)).filter(Boolean));
  let transitionIds = new Set();
  const workflow = configRecords(index, project, 'workflow')[0];
  if (workflow) transitionIds = new Set(array(recordFields(workflow).transitions).map((row, position) => transitionId(row, position)));
  return { pid, boardIds, columnIds, screenIds, fieldIds, transitionIds };
}

function rowBelongs(index, project, table, row, sets = relationSets(index, project)) {
  if (directOwner(row, project, sets.pid)) return true;
  switch (table) {
    case 'jira.board_columns': return sets.boardIds.has(columnBoardId(row));
    case 'jira.board_column_statuses': return sets.columnIds.has(mappingColumnId(row));
    case 'jira.board_quick_filters': return sets.boardIds.has(text(row?.board_id ?? row?.boardId));
    case 'jira.screen_fields': return sets.screenIds.has(text(row?.screen_id ?? row?.screenId));
    case 'jira.custom_field_options': return sets.fieldIds.has(text(row?.field_id ?? row?.fieldId ?? row?.custom_field_id ?? row?.customFieldId));
    case 'jira.custom_field_values': return sets.fieldIds.has(text(row?.field_id ?? row?.fieldId ?? row?.custom_field_id ?? row?.customFieldId));
    case 'jira.workflow_transition_rules': return sets.transitionIds.has(text(row?.transition_id ?? row?.transitionId));
    default: return false;
  }
}

function replaceEmbeddedFieldRefs(rows, oldId) {
  return rows.map(row => {
    if (!Array.isArray(row?.fields)) return row;
    return { ...row, fields: row.fields.filter(ref => !sameRef(ref?.customFieldId ?? ref?.custom_field_id, oldId)) };
  });
}

function replaceEmbeddedRules(transitions, oldId) {
  return transitions.map(edge => ({
    ...edge,
    rules: array(edge?.rules).filter(rule => !sameRef(rule?.config?.customFieldId ?? rule?.config?.custom_field_id, oldId)),
  }));
}

function workflowChangeRecord(record, statuses, transitions) {
  const fields = recordFields(record);
  return patchRecord(record, { statuses, transitions });
}

function workflowAction(index, request) {
  const project = request.project;
  const action = request.action;
  const current = workflowValues(index, project);
  const targets = settingsTargets(index, request);
  if (action === 'workflow.status.update') {
    const status = findStatus(current.statuses, request.data.id);
    if (!status) fail('NOT_FOUND', `Status ${request.data.id} was not found in ${project}`);
    const nextName = request.data.name === undefined ? statusName(status) : assertText(request.data.name, 'status name', { max: 100 });
    if (!sameRef(nextName, statusName(status))) {
      for (const ticket of allTicketRecords(index)) {
        if (upper(ticket?.project) === project && sameRef(canonicalOf(ticket).status, statusName(status))) {
          targets.push({ path: ticket.relativePath, record: ticket, mode: 'ticket' });
        }
      }
    }
  }
  return runOperation(index, request, action, targets, async fresh => {
    const values = workflowValues(fresh, project);
    let statuses = values.statuses.map(row => ({ ...row }));
    let transitions = values.transitions.map(row => ({ ...row }));
    const data = request.data;
    const changes = [];
    const tableUpdates = {};
    const statusAt = value => findStatus(statuses, value);
    const transitionAt = value => findTransition(transitions, value);

    if (action === 'workflow.status.create') {
      const name = assertText(data.name, 'status name', { max: 100 });
      const category = upper(data.category ?? 'TODO');
      if (!STATUS_CATEGORIES.has(category)) fail('INVALID', 'Unknown status category ' + category);
      if (statuses.some(row => sameRef(statusName(row), name))) fail('INVALID', `Status ${name} already exists`);
      statuses.push({ id: randomUUID(), name, category, position: statuses.length });
    } else if (action === 'workflow.status.update') {
      const status = statusAt(data.id);
      if (!status) fail('NOT_FOUND', `Status ${data.id} was not found in ${project}`);
      const oldName = statusName(status);
      const name = data.name === undefined ? oldName : assertText(data.name, 'status name', { max: 100 });
      const category = data.category === undefined ? statusCategory(status) : upper(data.category);
      if (!STATUS_CATEGORIES.has(category)) fail('INVALID', 'Unknown status category ' + category);
      if (statuses.some(row => row !== status && sameRef(statusName(row), name))) fail('INVALID', `Status ${name} already exists`);
      if (category !== statusCategory(status) && (category === 'DONE' || statusCategory(status) === 'DONE') && ticketUsesStatus(fresh, project, oldName)) {
        fail('INVALID', 'Move issues out of this status before changing its Done category');
      }
      status.name = name;
      status.category = category;
      delete status._position;
      statuses = statuses.map((row, position) => ({ ...row, position }));
      if (!sameRef(oldName, name)) changes.push(...updateTicketsForStatus(fresh, project, oldName, name, category));
    } else if (action === 'workflow.status.delete') {
      const status = statusAt(data.id);
      if (!status) fail('NOT_FOUND', `Status ${data.id} was not found in ${project}`);
      if (statuses.length <= 1) fail('INVALID', 'A workflow needs at least one status');
      const name = statusName(status);
      if (ticketUsesStatus(fresh, project, name)) fail('INVALID', `Status ${name} is still used by an issue`);
      const removedIds = new Set([statusId(status), name]);
      const removedTransitions = transitions.filter(edge => removedIds.has(text(fromStatus(edge))) || removedIds.has(text(toStatus(edge))));
      const removedTransitionIds = new Set(removedTransitions.map((edge, position) => transitionId(edge, position)));
      statuses = statuses.filter(row => row !== status).map((row, position) => ({ ...row, position }));
      transitions = transitions.filter(edge => !removedIds.has(text(fromStatus(edge))) && !removedIds.has(text(toStatus(edge)))
        && !sameRef(fromStatus(edge), name) && !sameRef(toStatus(edge), name));
      if (metadataRecord(fresh, 'jira.workflow_transition_rules')) {
        tableUpdates['jira.workflow_transition_rules'] = tableRows(fresh, 'jira.workflow_transition_rules')
          .filter(row => !removedTransitionIds.has(text(row?.transition_id ?? row?.transitionId)));
      }
    } else if (action === 'workflow.status.reorder') {
      const order = array(data.order).map(value => text(value));
      const known = new Map(statuses.map(row => [statusId(row).toLowerCase(), row]));
      if (order.length !== statuses.length || order.some(value => !known.has(value.toLowerCase())) || new Set(order.map(value => value.toLowerCase())).size !== statuses.length) {
        fail('INVALID', 'The order must list every status exactly once');
      }
      statuses = order.map(value => known.get(value.toLowerCase())).map((row, position) => ({ ...row, position }));
    } else if (action === 'workflow.transition.create') {
      const from = data.fromStatusId === undefined || data.fromStatusId === null || data.fromStatusId === '' ? null : assertText(data.fromStatusId, 'fromStatusId', { max: 200 });
      const to = assertText(data.toStatusId, 'toStatusId', { max: 200 });
      const fromStatusRow = from === null ? null : statusAt(from);
      const toStatusRow = statusAt(to);
      if (!toStatusRow || (from !== null && !fromStatusRow)) fail('INVALID', 'Transition must reference statuses in this project');
      const fromId = fromStatusRow ? statusId(fromStatusRow) : null;
      const toId = statusId(toStatusRow);
      if (fromId !== null && sameRef(fromId, toId)) fail('INVALID', 'A transition to the same status would be a no-op');
      if (transitions.some(edge => sameRef(fromStatus(edge) ?? '', fromId ?? '') && sameRef(toStatus(edge), toId))) fail('INVALID', 'That transition already exists');
      transitions.push({ id: randomUUID(), name: assertText(data.name, 'transition name', { required: false, max: 200 }) || `To ${statusName(toStatusRow)}`, from_status_id: fromId, to_status_id: toId, position: transitions.length, rules: [] });
    } else if (action === 'workflow.transition.delete') {
      const transition = transitionAt(data.id);
      if (!transition) fail('NOT_FOUND', `Transition ${data.id} was not found in ${project}`);
      const id = transitionId(transition, transitions.indexOf(transition));
      transitions = transitions.filter(row => row !== transition).map((row, position) => ({ ...row, position }));
      if (metadataRecord(fresh, 'jira.workflow_transition_rules')) {
        tableUpdates['jira.workflow_transition_rules'] = tableRows(fresh, 'jira.workflow_transition_rules')
          .filter(row => !sameRef(row?.transition_id ?? row?.transitionId, id));
      }
    } else if (action === 'workflow.transition.rule.create') {
      const transition = transitionAt(data.transitionId);
      if (!transition) fail('NOT_FOUND', `Transition ${data.transitionId} was not found in ${project}`);
      const candidate = validateRule(data);
      if (candidate.config.customFieldId && !customFieldById(fresh, project, candidate.config.customFieldId)) fail('NOT_FOUND', `Custom field ${candidate.config.customFieldId} was not found in ${project}`);
      const transitionKey = transitionId(transition, transitions.indexOf(transition));
      const rule = { id: randomUUID(), kind: candidate.kind, type: candidate.type, config: candidate.config };
      if (metadataRecord(fresh, 'jira.workflow_transition_rules')) {
        const rows = tableRows(fresh, 'jira.workflow_transition_rules');
        const owned = rows.filter(row => sameRef(row?.transition_id ?? row?.transitionId, transitionKey));
        tableUpdates['jira.workflow_transition_rules'] = [...rows, {
          id: rule.id,
          transition_id: transitionKey,
          kind: candidate.kind,
          rule_type: candidate.type,
          type: candidate.type,
          config: candidate.config,
          position: owned.length,
        }];
      } else {
        transition.rules = [...array(transition.rules), rule];
      }
    } else if (action === 'workflow.transition.rule.delete') {
      const id = assertText(data.id, 'rule id', { max: 200 });
      let removed = false;
      transitions = transitions.map(edge => ({ ...edge, rules: array(edge.rules).filter(rule => {
        const keep = !sameRef(rule?.id, id);
        if (!keep) removed = true;
        return keep;
      }) }));
      if (metadataRecord(fresh, 'jira.workflow_transition_rules')) {
        const rows = tableRows(fresh, 'jira.workflow_transition_rules');
        const next = rows.filter(row => {
          const keep = !sameRef(row?.id, id);
          if (!keep) removed = true;
          return keep;
        });
        tableUpdates['jira.workflow_transition_rules'] = next;
      }
      if (!removed) fail('NOT_FOUND', `Rule ${id} was not found in ${project}`);
    } else if (action === 'workflow.transition.screen') {
      const transition = transitionAt(data.transitionId);
      if (!transition) fail('NOT_FOUND', `Transition ${data.transitionId} was not found in ${project}`);
      const screenId = data.screenId === undefined || data.screenId === null || data.screenId === '' ? null : assertText(data.screenId, 'screenId', { max: 200 });
      if (screenId && !screenById(fresh, project, screenId)) fail('NOT_FOUND', `Screen ${screenId} was not found in ${project}`);
      transition.screen_id = screenId;
    }

    statuses = statuses.map((row, position) => {
      const next = { ...row };
      delete next._position;
      if (next.position === undefined) next.position = position;
      return next;
    });
    transitions = transitions.map((row, position) => {
      const next = { ...row };
      delete next._position;
      if (next.position === undefined) next.position = position;
      return next;
    });
    changes.unshift({ path: values.record.relativePath, bytes: workflowChangeRecord(values.record, statuses, transitions) });
    if (Object.keys(tableUpdates).length) {
      const tablePlan = writeTableChanges(fresh, project, tableUpdates);
      changes.push(...tablePlan.changes);
    }
    return { changes, result: { action, project, updated: true } };
  });
}

function fieldAction(index, request) {
  const { action, project, data } = request;
  const targets = settingsTargets(index, request);
  if (action === 'field.delete') {
    const field = customFieldById(index, project, data.id);
    if (!field) fail('NOT_FOUND', `Custom field ${data.id} was not found in ${project}`);
  }
  return runOperation(index, request, action, targets, async fresh => {
    const updates = {};
    const changes = [];
    const pid = projectId(fresh, project) ?? project;
    const fieldsSource = tableRowsForProject(fresh, project, 'jira.custom_fields', TABLE_KINDS.customFields);
    let fieldsRows = fieldsSource.rows.map(row => ({ ...object(row) }));

    if (action === 'field.create') {
      const definition = {
        key: assertText(data.key, 'field key', { max: 100 }),
        name: assertText(data.name, 'field name', { max: 200 }),
        type: upper(assertText(data.type, 'field type', { max: 50 })),
        description: assertText(data.description, 'field description', { required: false, max: 10000 }) || null,
        options: array(data.options).map(value => assertText(value, 'field option', { max: 200 })),
      };
      const invalid = customFieldError(definition);
      if (invalid) fail('INVALID', invalid);
      if (fieldsRows.some(row => directOwner(row, project, pid) && sameRef(row?.key, definition.key))) fail('INVALID', `A field with the key "${definition.key}" already exists`);
      const owned = fieldsRows.filter(row => directOwner(row, project, pid));
      const id = randomUUID();
      fieldsRows.push({ id, project_id: pid, key: definition.key, name: definition.name, type: definition.type, description: definition.description, position: owned.length, options: definition.options });
      updates['jira.custom_fields'] = fieldsRows;
      if (metadataRecord(fresh, 'jira.custom_field_options')) {
        const options = tableRows(fresh, 'jira.custom_field_options').slice();
        updates['jira.custom_field_options'] = options.concat(definition.options.map((value, position) => ({ id: randomUUID(), field_id: id, value, position })));
      }
      return finishTableAction(fresh, project, request, updates, changes, { id, key: definition.key, created: true });
    }

    if (action === 'field.delete') {
      const id = assertText(data.id, 'field id', { max: 200 });
      const field = fieldsRows.find(row => directOwner(row, project, pid) && [row?.id, row?.key, row?.name].some(value => sameRef(value, id)));
      if (!field) fail('NOT_FOUND', `Custom field ${id} was not found in ${project}`);
      const fieldId = rowId(field);
      updates['jira.custom_fields'] = fieldsRows.filter(row => row !== field).map((row, position) => directOwner(row, project, pid) ? { ...row, position } : row);
      if (metadataRecord(fresh, 'jira.custom_field_options')) updates['jira.custom_field_options'] = tableRows(fresh, 'jira.custom_field_options').filter(row => !sameRef(row?.field_id ?? row?.fieldId ?? row?.custom_field_id ?? row?.customFieldId, fieldId));
      if (metadataRecord(fresh, 'jira.custom_field_values')) updates['jira.custom_field_values'] = tableRows(fresh, 'jira.custom_field_values').filter(row => !sameRef(row?.field_id ?? row?.fieldId ?? row?.custom_field_id ?? row?.customFieldId, fieldId));
      if (metadataRecord(fresh, 'jira.field_configurations')) updates['jira.field_configurations'] = tableRows(fresh, 'jira.field_configurations').filter(row => !sameRef(row?.custom_field_id ?? row?.customFieldId, fieldId));
      if (metadataRecord(fresh, 'jira.screen_fields')) updates['jira.screen_fields'] = tableRows(fresh, 'jira.screen_fields').filter(row => !sameRef(row?.custom_field_id ?? row?.customFieldId, fieldId));
      if (metadataRecord(fresh, 'jira.screens')) {
        updates['jira.screens'] = replaceEmbeddedFieldRefs(tableRows(fresh, 'jira.screens'), fieldId);
      } else {
        const screensSource = tableRowsForProject(fresh, project, 'jira.screens', TABLE_KINDS.screens);
        if (screensSource.rows.length) updates['jira.screens'] = replaceEmbeddedFieldRefs(screensSource.rows.map(row => ({ ...object(row) })), fieldId);
      }
      const workflow = configRecords(fresh, project, 'workflow')[0];
      if (workflow) {
        const wf = recordFields(workflow);
        const nextTransitions = replaceEmbeddedRules(array(wf.transitions), fieldId);
        changes.push({ path: workflow.relativePath, bytes: patchRecord(workflow, { transitions: nextTransitions }) });
      }
      if (metadataRecord(fresh, 'jira.workflow_transition_rules')) {
        updates['jira.workflow_transition_rules'] = tableRows(fresh, 'jira.workflow_transition_rules').filter(row => !sameRef(row?.config?.customFieldId ?? row?.config?.custom_field_id, fieldId));
      }
      return finishTableAction(fresh, project, request, updates, changes, { id: fieldId, deleted: true });
    }

    if (action === 'field.requirement.set') {
      const requirement = validateRequirement(data, fresh, project);
      const source = tableRowsForProject(fresh, project, 'jira.field_configurations', TABLE_KINDS.fieldConfiguration);
      let rows = source.rows.map(row => ({ ...object(row) }));
      const identity = requirement.issueType + '\u0000' + fieldRefKey(requirement.reference);
      const existing = rows.find(row => directOwner(row, project, pid) && requirementIdentity(row) === identity);
      const defaultValue = defaultRequirement(requirement.issueType, requirement.reference);
      if (requirement.requirement === defaultValue) {
        rows = rows.filter(row => row !== existing);
      } else if (existing) {
        existing.requirement = requirement.requirement;
      } else {
        rows.push({ id: randomUUID(), project_id: pid, issue_type: requirement.issueType, issueType: requirement.issueType, builtin_field: requirement.reference.builtinField, builtinField: requirement.reference.builtinField, custom_field_id: requirement.reference.customFieldId, customFieldId: requirement.reference.customFieldId, requirement: requirement.requirement });
      }
      updates['jira.field_configurations'] = rows;
      return finishTableAction(fresh, project, request, updates, changes, { updated: true, action });
    }

    fail('INVALID', 'Unsupported field action: ' + action);
  });
}

function finishTableAction(index, project, request, updates, changes = [], result = {}) {
  const plan = writeTableChanges(index, project, updates, {
    'jira.custom_fields': TABLE_KINDS.customFields,
    'jira.field_configurations': TABLE_KINDS.fieldConfiguration,
    'jira.custom_field_options': 'customFieldOptions',
    'jira.custom_field_values': 'customFieldValues',
    'jira.screen_fields': 'screenFields',
    'jira.screens': TABLE_KINDS.screens,
    'jira.workflow_transition_rules': 'workflowTransitionRules',
  });
  return { changes: [...changes, ...plan.changes], result: { action: request.action, project, ...result } };
}

function normalizeScreenFields(index, project, values) {
  if (!Array.isArray(values)) fail('INVALID', 'screen fields must be an array');
  const seen = new Set();
  const result = values.map(value => {
    const ref = ensureFieldReference(index, project, fieldReference(value));
    const key = fieldRefKey(ref);
    if (seen.has(key)) fail('INVALID', 'A screen lists each field once.');
    seen.add(key);
    return ref;
  });
  return result;
}

function screenAction(index, request) {
  const { action, project, data } = request;
  const targets = settingsTargets(index, request);
  return runOperation(index, request, action, targets, async fresh => {
    const pid = projectId(fresh, project) ?? project;
    const screenSource = tableRowsForProject(fresh, project, 'jira.screens', TABLE_KINDS.screens);
    let screens = screenSource.rows.map(row => ({ ...object(row) }));
    const updates = {};
    const changes = [];
    const screenAt = value => screens.find(row => directOwner(row, project, pid) && [row?.id, row?.key, row?.name].some(candidate => sameRef(candidate, value)));

    if (action === 'screen.create') {
      const name = assertText(data.name, 'screen name', { max: 200 });
      if (screens.some(row => directOwner(row, project, pid) && sameRef(row?.name, name))) fail('INVALID', `A screen with the name "${name}" already exists`);
      const refs = data.fields === undefined ? [] : normalizeScreenFields(fresh, project, data.fields);
      const id = randomUUID();
      const owned = screens.filter(row => directOwner(row, project, pid));
      screens.push({ id, project_id: pid, name, description: assertText(data.description, 'screen description', { required: false, max: 10000 }) || null, position: owned.length, fields: metadataRecord(fresh, 'jira.screen_fields') ? [] : refs });
      updates['jira.screens'] = screens;
      if (metadataRecord(fresh, 'jira.screen_fields')) updates['jira.screen_fields'] = tableRows(fresh, 'jira.screen_fields').concat(refs.map((ref, position) => ({ id: randomUUID(), screen_id: id, builtin_field: ref.builtinField, custom_field_id: ref.customFieldId, position })));
      return finishTableAction(fresh, project, request, updates, changes, { id, name, created: true });
    }

    if (action === 'screen.fields.set') {
      const screen = screenAt(data.id);
      if (!screen) fail('NOT_FOUND', `Screen ${data.id} was not found in ${project}`);
      const refs = normalizeScreenFields(fresh, project, data.fields);
      const id = screenIdOf(screen);
      if (metadataRecord(fresh, 'jira.screen_fields')) {
        screens = screens.map(row => row === screen ? { ...row, fields: [] } : row);
        updates['jira.screen_fields'] = tableRows(fresh, 'jira.screen_fields').filter(row => !sameRef(row?.screen_id ?? row?.screenId, id)).concat(refs.map((ref, position) => ({ id: randomUUID(), screen_id: id, builtin_field: ref.builtinField, custom_field_id: ref.customFieldId, position })));
      } else {
        screens = screens.map(row => row === screen ? { ...row, fields: refs } : row);
      }
      updates['jira.screens'] = screens;
      return finishTableAction(fresh, project, request, updates, changes, { id, updated: true });
    }

    if (action === 'screen.delete') {
      const screen = screenAt(data.id);
      if (!screen) fail('NOT_FOUND', `Screen ${data.id} was not found in ${project}`);
      const id = screenIdOf(screen);
      screens = screens.filter(row => row !== screen).map((row, position) => directOwner(row, project, pid) ? { ...row, position } : row);
      updates['jira.screens'] = screens;
      if (metadataRecord(fresh, 'jira.screen_fields')) updates['jira.screen_fields'] = tableRows(fresh, 'jira.screen_fields').filter(row => !sameRef(row?.screen_id ?? row?.screenId, id));
      const workflow = configRecords(fresh, project, 'workflow')[0];
      if (workflow) {
        const fields = recordFields(workflow);
        const transitions = array(fields.transitions).map(edge => {
          const next = { ...object(edge) };
          if (sameRef(next.screenId ?? next.screen_id, id)) {
            next.screen_id = null;
            delete next.screenId;
          }
          return next;
        });
        changes.push({ path: workflow.relativePath, bytes: patchRecord(workflow, { transitions }) });
      }
      return finishTableAction(fresh, project, request, updates, changes, { id, deleted: true });
    }

    fail('INVALID', 'Unsupported screen action: ' + action);
  });
}

function finishBoardConfig(index, project, request, target, fields, result = {}) {
  const bytes = target.record ? patchRecord(target.record, fields) : yamlConfig({ kind: TABLE_KINDS.board, ...fields });
  return { changes: [{ path: target.path, bytes }], result: { action: request.action, project, ...result } };
}

function boardColumnFromConfig(columns, id) {
  const wanted = text(id).toLowerCase();
  return columns.find((column, position) => boardColumnId(column).toLowerCase() === wanted || `column-${position + 1}` === wanted) ?? null;
}

function boardConfigAction(index, request) {
  const project = request.project;
  const target = targetForTable(index, project, 'board', TABLE_KINDS.board);
  return runOperation(index, request, request.action, settingsTargets(index, request), async fresh => {
    const current = boardRowsFromConfig(fresh, project);
    if (!current) fail('NOT_FOUND', `No board catalog for ${project}`);
    const fields = clone(current.fallback.fields);
    let columns = current.columns.map(row => ({ ...row }));
    let quickFilters = current.quickFilters.map(row => ({ ...row }));
    const data = request.data;
    if (request.action === 'board.settings.update') {
      if (data.name !== undefined) fields.name = assertText(data.name, 'board name', { max: 200 });
      if (data.type !== undefined) {
        const type = upper(assertText(data.type, 'board type', { max: 20 }));
        if (!BOARD_TYPES.has(type)) fail('INVALID', `Unknown board type ${type}`);
        fields.type = type;
      }
      if (data.swimlaneBy !== undefined || data.swimlane_by !== undefined) {
        const mode = upper(data.swimlaneBy ?? data.swimlane_by);
        if (!SWIMLANES.has(mode)) fail('INVALID', `Unknown swimlane mode ${mode}`);
        fields.swimlaneBy = mode;
      }
      if (data.cardFields !== undefined || data.card_fields !== undefined) {
        const cardFields = normalizeCardFields(data.cardFields ?? data.card_fields);
        if (array(data.cardFields ?? data.card_fields).some(value => !CARD_FIELDS.has(value))) fail('INVALID', 'cardFields contains an unknown field');
        fields.cardFields = cardFields;
      }
      if (data.filter !== undefined) {
        const invalid = boardFilterError(data.filter);
        if (invalid) fail('INVALID', invalid);
        fields.filter = data.filter;
      }
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { updated: true });
    }
    if (request.action === 'board.column.create') {
      const name = assertText(data.name, 'column name', { max: 200 });
      const id = randomUUID();
      columns.push({ id, name, position: columns.length, statusIds: [] });
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { id, name, created: true });
    }
    if (request.action === 'board.column.update') {
      const column = boardColumnFromConfig(columns, data.id);
      if (!column) fail('NOT_FOUND', `Column ${data.id} was not found on this board`);
      if (data.name !== undefined) column.name = assertText(data.name, 'column name', { max: 200 });
      if (data.wipLimit !== undefined || data.wip_limit !== undefined) {
        const value = data.wipLimit ?? data.wip_limit;
        if (value !== null && (!Number.isInteger(value) || value < 0)) fail('INVALID', 'A WIP limit must be a non-negative whole number');
        column.wipLimit = value;
      }
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { id: boardColumnId(column), updated: true });
    }
    if (request.action === 'board.column.delete') {
      const column = boardColumnFromConfig(columns, data.id);
      if (!column) fail('NOT_FOUND', `Column ${data.id} was not found on this board`);
      if (columns.length <= 1) fail('INVALID', 'A board needs at least one column');
      const id = boardColumnId(column);
      columns = columns.filter(row => row !== column).map((row, position) => ({ ...row, position }));
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { id, deleted: true });
    }
    if (request.action === 'board.column.reorder') {
      const order = array(data.order).map(value => text(value));
      const known = new Map(columns.map((row, position) => [boardColumnId(row) || `column-${position + 1}`, row]));
      if (order.length !== columns.length || order.some(value => !known.has(value)) || new Set(order).size !== columns.length) fail('INVALID', 'The order must list every column exactly once');
      columns = order.map(value => known.get(value)).map((row, position) => ({ ...row, position }));
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { order, updated: true });
    }
    if (request.action === 'board.column.status.map' || request.action === 'board.column.status.unmap') {
      const column = boardColumnFromConfig(columns, data.columnId);
      if (!column) fail('NOT_FOUND', `Column ${data.columnId} was not found on this board`);
      const { statuses } = workflowValues(fresh, project);
      const status = findStatus(statuses, data.statusId);
      if (!status) fail('NOT_FOUND', `Status ${data.statusId} was not found in ${project}`);
      const statusIdValue = statusId(status);
      if (request.action.endsWith('.map')) {
        columns = columns.map(row => ({ ...row, statusIds: array(row.statusIds ?? row.status_ids).filter(id => !sameRef(id, statusIdValue)) }));
        const updated = columns.find(row => boardColumnId(row) === boardColumnId(column));
        updated.statusIds = [...array(updated.statusIds), statusIdValue];
      } else {
        column.statusIds = array(column.statusIds ?? column.status_ids).filter(id => !sameRef(id, statusIdValue));
      }
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { columnId: boardColumnId(column), statusId: statusIdValue, updated: true });
    }
    if (request.action === 'board.quick-filter.create') {
      const name = assertText(data.name, 'quick filter name', { max: 200 });
      const invalid = boardFilterError(data.filter ?? {});
      if (invalid) fail('INVALID', invalid);
      const id = randomUUID();
      quickFilters.push({ id, name, filter: data.filter ?? {}, position: quickFilters.length });
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { id, name, created: true });
    }
    if (request.action === 'board.quick-filter.delete') {
      const id = assertText(data.id, 'quick filter id', { max: 200 });
      const before = quickFilters.length;
      quickFilters = quickFilters.filter(row => !sameRef(row?.id ?? row?.key ?? row?.name, id)).map((row, position) => ({ ...row, position }));
      if (before === quickFilters.length) fail('NOT_FOUND', `Quick filter ${id} was not found on this board`);
      return finishBoardConfig(fresh, project, request, target, { ...fields, columns, quickFilters }, { id, deleted: true });
    }
    fail('INVALID', 'Unsupported board action: ' + request.action);
  });
}

function boardAction(index, request) {
  if (!boardContext(index, request.project).board && boardConfigFallback(index, request.project)) return boardConfigAction(index, request);
  const project = request.project;
  const targets = settingsTargets(index, request);
  return runOperation(index, request, request.action, targets, async fresh => {
    const info = boardInfo(fresh, project);
    if (info.mode !== 'metadata' || !info.board) fail('NOT_FOUND', `No board catalog for ${project}`);
    const pid = info.projectId;
    const board = info.board;
    const boardId = rowId(board);
    const allBoards = tableRows(fresh, 'jira.boards').map(row => ({ ...row }));
    const allColumns = tableRows(fresh, 'jira.board_columns').map(row => ({ ...row }));
    const allMappings = tableRows(fresh, 'jira.board_column_statuses').map(row => ({ ...row }));
    const allFilters = tableRows(fresh, 'jira.board_quick_filters').map(row => ({ ...row }));
    const columns = allColumns.filter(row => text(row?.board_id ?? row?.boardId) === boardId);
    const updates = {};
    const data = request.data;

    if (request.action === 'board.settings.update') {
      const patch = {};
      if (data.name !== undefined) patch.name = assertText(data.name, 'board name', { max: 200 });
      if (data.type !== undefined) {
        const value = upper(assertText(data.type, 'board type', { max: 20 }));
        if (!BOARD_TYPES.has(value)) fail('INVALID', `Unknown board type ${value}`);
        patch.type = value;
      }
      if (data.swimlaneBy !== undefined || data.swimlane_by !== undefined) {
        const value = upper(data.swimlaneBy ?? data.swimlane_by);
        if (!SWIMLANES.has(value)) fail('INVALID', `Unknown swimlane mode ${value}`);
        patch.swimlane_by = value;
      }
      if (data.cardFields !== undefined || data.card_fields !== undefined) {
        const raw = data.cardFields ?? data.card_fields;
        if (!Array.isArray(raw) || raw.some(value => !CARD_FIELDS.has(value))) fail('INVALID', 'cardFields contains an unknown field');
        patch.card_fields = normalizeCardFields(raw);
      }
      if (data.filter !== undefined) {
        const invalid = boardFilterError(data.filter);
        if (invalid) fail('INVALID', invalid);
        patch.filter = data.filter;
      }
      updates['jira.boards'] = allBoards.map(row => rowId(row) === boardId ? { ...row, ...patch } : row);
      return finishTableAction(fresh, project, request, updates, [], { id: boardId, updated: true });
    }

    if (request.action === 'board.column.create') {
      const name = assertText(data.name, 'column name', { max: 200 });
      const id = randomUUID();
      const position = columns.reduce((max, row) => Math.max(max, Number(row?.position) || -1), -1) + 1;
      updates['jira.board_columns'] = [...allColumns, { id, board_id: boardId, name, position, wip_limit: null }];
      return finishTableAction(fresh, project, request, updates, [], { id, name, created: true });
    }

    if (request.action === 'board.column.update') {
      const column = columns.find(row => sameRef(rowId(row), data.id));
      if (!column) fail('NOT_FOUND', `Column ${data.id} was not found on this board`);
      const patch = {};
      if (data.name !== undefined) patch.name = assertText(data.name, 'column name', { max: 200 });
      if (data.wipLimit !== undefined || data.wip_limit !== undefined) {
        const value = data.wipLimit ?? data.wip_limit;
        if (value !== null && (!Number.isInteger(value) || value < 0)) fail('INVALID', 'A WIP limit must be a non-negative whole number');
        patch.wip_limit = value;
      }
      updates['jira.board_columns'] = allColumns.map(row => rowId(row) === rowId(column) ? { ...row, ...patch } : row);
      return finishTableAction(fresh, project, request, updates, [], { id: rowId(column), updated: true });
    }

    if (request.action === 'board.column.delete') {
      const column = columns.find(row => sameRef(rowId(row), data.id));
      if (!column) fail('NOT_FOUND', `Column ${data.id} was not found on this board`);
      if (columns.length <= 1) fail('INVALID', 'A board needs at least one column');
      const id = rowId(column);
      updates['jira.board_columns'] = allColumns.filter(row => rowId(row) !== id).map(row => {
        if (text(row?.board_id ?? row?.boardId) !== boardId) return row;
        const position = columns.filter(item => rowId(item) !== id).sort((a, b) => (Number(a.position) || 0) - (Number(b.position) || 0)).findIndex(item => rowId(item) === rowId(row));
        return { ...row, position };
      });
      updates['jira.board_column_statuses'] = allMappings.filter(row => !sameRef(row?.column_id ?? row?.columnId, id));
      return finishTableAction(fresh, project, request, updates, [], { id, deleted: true });
    }

    if (request.action === 'board.column.reorder') {
      const order = array(data.order).map(value => text(value));
      const known = new Map(columns.map(row => [rowId(row), row]));
      if (order.length !== columns.length || order.some(value => !known.has(value)) || new Set(order).size !== columns.length) fail('INVALID', 'The order must list every column exactly once');
      const position = new Map(order.map((value, index) => [value, index]));
      updates['jira.board_columns'] = allColumns.map(row => position.has(rowId(row)) ? { ...row, position: position.get(rowId(row)) } : row);
      return finishTableAction(fresh, project, request, updates, [], { order, updated: true });
    }

    if (request.action === 'board.column.status.map' || request.action === 'board.column.status.unmap') {
      const column = columns.find(row => sameRef(rowId(row), data.columnId));
      if (!column) fail('NOT_FOUND', `Column ${data.columnId} was not found on this board`);
      const workflow = workflowValues(fresh, project);
      const status = findStatus(workflow.statuses, data.statusId);
      if (!status) fail('NOT_FOUND', `Status ${data.statusId} was not found in ${project}`);
      const statusKey = statusId(status);
      if (request.action.endsWith('.unmap')) {
        updates['jira.board_column_statuses'] = allMappings.filter(row => !(sameRef(row?.column_id ?? row?.columnId, rowId(column)) && sameRef(row?.status_id ?? row?.statusId, statusKey)));
      } else {
        const withoutStatus = allMappings.filter(row => !(info.columnIds.has(mappingColumnId(row)) && sameRef(mappingStatusId(row), statusKey)));
        const ownMappings = withoutStatus.filter(row => sameRef(mappingColumnId(row), rowId(column)));
        const position = ownMappings.reduce((max, row) => Math.max(max, Number(row?.position) || -1), -1) + 1;
        updates['jira.board_column_statuses'] = [...withoutStatus, { id: randomUUID(), column_id: rowId(column), status_id: statusKey, position }];
      }
      return finishTableAction(fresh, project, request, updates, [], { columnId: rowId(column), statusId: statusKey, updated: true });
    }

    if (request.action === 'board.quick-filter.create') {
      const name = assertText(data.name, 'quick filter name', { max: 200 });
      const filter = data.filter ?? {};
      const invalid = boardFilterError(filter);
      if (invalid) fail('INVALID', invalid);
      const own = allFilters.filter(row => text(row?.board_id ?? row?.boardId) === boardId);
      const id = randomUUID();
      const position = own.reduce((max, row) => Math.max(max, Number(row?.position) || -1), -1) + 1;
      updates['jira.board_quick_filters'] = [...allFilters, { id, board_id: boardId, name, filter, position }];
      return finishTableAction(fresh, project, request, updates, [], { id, name, created: true });
    }

    if (request.action === 'board.quick-filter.delete') {
      const filter = allFilters.find(row => text(row?.board_id ?? row?.boardId) === boardId && sameRef(rowId(row), data.id));
      if (!filter) fail('NOT_FOUND', `Quick filter ${data.id} was not found on this board`);
      updates['jira.board_quick_filters'] = allFilters.filter(row => row !== filter);
      return finishTableAction(fresh, project, request, updates, [], { id: rowId(filter), deleted: true });
    }
    fail('INVALID', 'Unsupported board action: ' + request.action);
  });
}

export async function dispatchSettingsAction(index, input, context = {}) {
  const request = normalizeRequest(input);
  ensureProject(index, request.project);
  if (request.action.startsWith('workflow.')) return workflowAction(index, request, context);
  if (request.action.startsWith('field.')) return fieldAction(index, request, context);
  if (request.action.startsWith('screen.')) return screenAction(index, request, context);
  if (request.action.startsWith('board.')) return boardAction(index, request, context);
  fail('INVALID', 'Unsupported settings action: ' + request.action);
}

export default dispatchSettingsAction;
