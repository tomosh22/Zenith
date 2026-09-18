/*
 * Project and issue read models for the Vault Jira surface.
 *
 * The scanner remains the only source of truth.  This module projects its
 * records and exported relational tables into the small, stable shapes the
 * browser needs; it never exposes raw bytes or absolute paths.
 */

import path from 'node:path';

import {
  allTicketRecords,
  canonicalOf,
  idAndNumber,
  outgoingLinks,
  projectKeyOf,
  sourceOf,
} from '../cli/compat.mjs';
import { namedRows, rowsFor } from '../cli/queries.mjs';
import { appendGitHistory } from '../cli/git-history.mjs';
import { averageVelocity, buildBurndown, epicPercent } from '../shared/jira/reports.js';
import { issueComments } from './issue-interactions.mjs';

const gitHistoryCache = new Map();

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

function upper(value) {
  return text(value).toUpperCase();
}

function number(value, fallback = null) {
  const result = Number(value);
  return Number.isFinite(result) ? result : fallback;
}

function publicValue(value, seen = new WeakSet()) {
  if (value === undefined) return undefined;
  if (value === null || typeof value !== 'string' && typeof value !== 'object') return value;
  if (typeof value === 'string') return value;
  if (Buffer.isBuffer(value) || value instanceof Uint8Array) return undefined;
  if (seen.has(value)) return undefined;
  seen.add(value);
  if (Array.isArray(value)) return value.map(item => publicValue(item, seen)).filter(item => item !== undefined);
  const result = {};
  for (const [key, item] of Object.entries(value)) {
    if (['rawBytes', 'bytes', 'stat', 'absolutePath', 'pathOnDisk'].includes(key)) continue;
    const next = publicValue(item, seen);
    if (next !== undefined) result[key] = next;
  }
  return result;
}

function revisionOf(record) {
  if (!record) return { hash: null, size: 0, mtimeMs: null };
  return {
    hash: record.hash ?? null,
    size: record.size ?? record.rawBytes?.length ?? 0,
    ...(record.mtimeMs === undefined && record.stat?.mtimeMs === undefined
      ? {}
      : { mtimeMs: record.mtimeMs ?? record.stat?.mtimeMs ?? null }),
  };
}

function revisionMap(records) {
  const result = {};
  for (const record of records ?? []) {
    if (!record?.relativePath) continue;
    result[record.relativePath] = revisionOf(record);
  }
  return result;
}

function recordFields(record) {
  return object(record?.metadata ?? record?.fields ?? record);
}

function recordProject(record, fallback = null) {
  const metadata = recordFields(record);
  const key = projectKeyOf(record, metadata.key);
  return upper(record?.project ?? key ?? fallback) || null;
}

function sourceId(record) {
  const identity = idAndNumber(record);
  const source = sourceOf(record);
  const value = identity.id ?? source.id;
  return value === undefined || value === null || value === '' ? null : String(value);
}

function projectRows(index) {
  const result = new Map();
  for (const row of rowsFor(index, 'jira.projects')) {
    const key = upper(row?.key ?? row?.project_key ?? row?.projectKey);
    if (key) result.set(key, row);
  }
  return result;
}

function projectIds(index) {
  const result = new Map();
  for (const [key, row] of projectRows(index)) {
    if (row?.id !== undefined && row?.id !== null) result.set(String(row.id), key);
  }
  return result;
}

function rowProject(row, ids) {
  const direct = row?.project ?? row?.project_key ?? row?.projectKey ?? row?.project_code;
  return upper(direct || ids.get(String(row?.project_id ?? row?.projectId ?? '')) || '') || null;
}

/** True when the row carries any project reference, resolvable or not. */
function rowHasProjectRef(row) {
  return [row?.project, row?.project_key, row?.projectKey, row?.project_code, row?.project_id, row?.projectId]
    .some(value => value !== undefined && value !== null && value !== '');
}

function configuredRoot(index, key) {
  const wanted = upper(key);
  return array(index?.boardRoots).find(root => upper(root?.project) === wanted) ?? null;
}

function configRecords(index, kind, project = null) {
  const wanted = text(kind).toLowerCase();
  const owner = project ? upper(project) : null;
  return [...(index?.configs?.values?.() ?? [])].filter(record => {
    const fields = recordFields(record);
    const recordKind = text(fields.kind ?? fields.entity ?? '').toLowerCase();
    if (recordKind !== wanted && !String(record.relativePath ?? '').toLowerCase().endsWith(`/${wanted}.md`)) return false;
    return !owner || recordProject(record) === owner;
  });
}

function catalogRows(index, names, project = null) {
  const wanted = new Set(array(names).map(name => String(name).toLowerCase()));
  const ids = projectIds(index);
  const owner = project ? upper(project) : null;
  const values = [];
  const seen = new Set();
  const add = (row, source = null) => {
    if (!row || typeof row !== 'object' || Array.isArray(row)) return;
    if (owner && rowProject(row, ids) !== owner && !source) return;
    const id = row.id ?? row.key ?? row.name;
    const identity = `${String(id ?? JSON.stringify(row))}\u0000${rowProject(row, ids) ?? source ?? ''}`;
    if (seen.has(identity)) return;
    seen.add(identity);
    values.push({ ...row, ...(source ? { sourcePath: source } : {}) });
  };
  for (const record of index?.configs?.values?.() ?? []) {
    const fields = recordFields(record);
    const kind = text(fields.kind ?? fields.entity ?? '').toLowerCase();
    if (!wanted.has(kind) && !wanted.has(String(record.relativePath ?? '').split('/').at(-1)?.replace(/\.md$/i, '').toLowerCase())) continue;
    const recordOwner = recordProject(record);
    if (owner && recordOwner !== owner) continue;
    for (const row of array(fields.rows)) add(row, record.relativePath);
  }
  for (const name of names) {
    for (const row of rowsFor(index, name)) add(row, null);
  }
  return values;
}

function customFieldRows(index, project) {
  const fields = catalogRows(index, ['customFields', 'custom_fields', 'jira.custom_fields'], project);
  const optionsByField = new Map();
  for (const option of rowsFor(index, 'jira.custom_field_options')) {
    const fieldId = option?.field_id ?? option?.fieldId ?? option?.custom_field_id ?? option?.customFieldId;
    if (fieldId === undefined || fieldId === null) continue;
    const key = String(fieldId);
    if (!optionsByField.has(key)) optionsByField.set(key, []);
    optionsByField.get(key).push(option);
  }
  for (const options of optionsByField.values()) {
    options.sort((left, right) => number(left?.position, Number.MAX_SAFE_INTEGER)
      - number(right?.position, Number.MAX_SAFE_INTEGER)
      || String(left?.id ?? '').localeCompare(String(right?.id ?? '')));
  }
  return fields.map(field => {
    const value = publicValue(field) ?? {};
    const id = field?.id ?? field?.key ?? field?.name;
    const relational = optionsByField.get(String(id)) ?? [];
    const sourceOptions = relational.length ? relational : array(field?.options);
    return {
      ...value,
      ...(id === undefined ? {} : { id: String(id) }),
      options: sourceOptions.map(option => text(option?.value ?? option?.name ?? option)),
    };
  });
}

function screenRows(index, project) {
  const screens = catalogRows(index, ['screens', 'jira.screens'], project);
  const fieldsByScreen = new Map();
  for (const field of rowsFor(index, 'jira.screen_fields')) {
    const screenId = field?.screen_id ?? field?.screenId;
    if (screenId === undefined || screenId === null) continue;
    const key = String(screenId);
    if (!fieldsByScreen.has(key)) fieldsByScreen.set(key, []);
    fieldsByScreen.get(key).push(field);
  }
  for (const fields of fieldsByScreen.values()) {
    fields.sort((left, right) => number(left?.position, Number.MAX_SAFE_INTEGER)
      - number(right?.position, Number.MAX_SAFE_INTEGER)
      || String(left?.id ?? '').localeCompare(String(right?.id ?? '')));
  }
  return screens.map(screen => {
    const value = publicValue(screen) ?? {};
    const id = screen?.id ?? screen?.key ?? screen?.name;
    const relational = fieldsByScreen.get(String(id)) ?? [];
    const sourceFields = relational.length ? relational : array(screen?.fields);
    const fields = sourceFields.map((field, position) => ({
      ...publicValue(field),
      id: field?.id ?? null,
      builtinField: field?.builtinField ?? field?.builtin_field ?? null,
      customFieldId: field?.customFieldId ?? field?.custom_field_id ?? null,
      position: number(field?.position, position),
    }));
    return {
      ...value,
      ...(id === undefined ? {} : { id: String(id) }),
      fields,
    };
  });
}

function statusRows(index, project) {
  const workflow = configRecords(index, 'workflow', project)[0];
  const fields = recordFields(workflow);
  const values = array(fields.statuses ?? fields.rows);
  return values.map((row, index) => {
    const item = object(row);
    const name = text(item.name ?? item.status ?? item.status_name ?? item.title ?? row, `Status ${index + 1}`);
    return {
      ...publicValue(item),
      id: text(item.id ?? item.statusId ?? item.status_id, name.toLowerCase().replace(/[^a-z0-9]+/g, '-')),
      name,
      category: upper(item.category ?? item.statusCategory ?? item.status_category ?? 'TODO'),
      position: number(item.position, index),
    };
  }).sort((a, b) => a.position - b.position || a.name.localeCompare(b.name));
}

function workflowModel(index, project) {
  const record = configRecords(index, 'workflow', project)[0] ?? null;
  const fields = recordFields(record);
  const statuses = statusRows(index, project);
  const statusIds = new Map(statuses.map(status => [status.name.toLowerCase(), status.id]));
  const transitions = array(fields.transitions).map((edge, index) => {
    const item = object(edge);
    const from = item.fromStatusId ?? item.from_status_id ?? item.from ?? null;
    const to = item.toStatusId ?? item.to_status_id ?? item.to ?? null;
    const resolve = value => value === null || value === undefined ? null : statusIds.get(String(value).toLowerCase()) ?? String(value);
    return {
      ...publicValue(item),
      id: text(item.id ?? item.transitionId, `transition-${index + 1}`),
      name: text(item.name, ''),
      fromStatusId: resolve(from),
      toStatusId: resolve(to),
      rules: array(item.rules).map(rule => publicValue(rule)),
      screenId: item.screenId ?? item.screen_id ?? null,
    };
  }).filter(edge => edge.toStatusId !== null);
  const allRules = rowsFor(index, 'jira.workflow_transition_rules');
  const rules = allRules.filter(rule => {
    const transition = transitions.find(edge => String(edge.id) === String(rule?.transition_id ?? rule?.transitionId));
    return Boolean(transition);
  }).map(rule => publicValue(rule));
  const rulesByTransition = new Map();
  for (const rule of rules) {
    const id = String(rule.transition_id ?? rule.transitionId ?? '');
    if (!rulesByTransition.has(id)) rulesByTransition.set(id, []);
    rulesByTransition.get(id).push(rule);
  }
  for (const transition of transitions) {
    const attached = rulesByTransition.get(String(transition.id));
    if (attached?.length) transition.rules = [...transition.rules, ...attached];
  }
  return {
    path: record?.relativePath ?? null,
    hash: record?.hash ?? null,
    size: record?.size ?? null,
    statuses,
    transitions,
    rules: publicValue(rules),
    source: publicValue(fields),
  };
}

function serializeTicket(record) {
  if (!record) return null;
  const metadata = publicValue(recordFields(record));
  const key = text(metadata.key ?? record.key, path.basename(record.relativePath ?? '', path.extname(record.relativePath ?? '')));
  const source = publicValue(sourceOf(record));
  const result = {
    path: record.relativePath ?? null,
    project: recordProject(record),
    key,
    title: text(metadata.title ?? record.title, key),
    status: metadata.status ?? metadata.state ?? null,
    statusCategory: record.statusCategory ?? metadata.statusCategory ?? metadata.status_category ?? null,
    metadata,
    fields: publicValue(record.fields ?? metadata),
    body: record.bodyText ?? record.body ?? '',
    hash: record.hash ?? null,
    size: record.size ?? record.rawBytes?.length ?? null,
    mtimeMs: record.mtimeMs ?? record.stat?.mtimeMs ?? null,
    updatedAt: metadata.updatedAt ?? metadata.updated_at ?? source?.updatedAt ?? source?.updated_at ?? null,
  };
  for (const field of ['type', 'priority', 'complexity', 'risk', 'assignee', 'labels', 'fixVersions', 'affectsVersions', 'parent', 'sprint', 'storyPoints', 'category', 'resolution']) {
    if (metadata[field] !== undefined) result[field] = publicValue(metadata[field]);
  }
  const identity = idAndNumber(record);
  if (identity.id !== null && identity.id !== undefined) result.id = identity.id;
  if (identity.number !== null && identity.number !== undefined) result.number = identity.number;
  return result;
}

function compactTicket(record) {
  const value = serializeTicket(record);
  if (!value) return null;
  return {
    id: value.id ?? value.key,
    key: value.key,
    title: value.title,
    type: value.type ?? value.metadata?.type ?? null,
    status: value.status,
    statusCategory: value.statusCategory,
    storyPoints: value.storyPoints ?? value.metadata?.storyPoints ?? null,
    path: value.path,
  };
}

function ticketsFor(index, project) {
  const owner = upper(project);
  return allTicketRecords(index).filter(record => recordProject(record) === owner
    || upper(recordFields(record).key).startsWith(`${owner}-`));
}

function sprintRecordOwner(record, ids) {
  const metadata = recordFields(record);
  const source = object(metadata.source?.source ?? metadata.source ?? sourceOf(record));
  return recordProject(record, ids.get(String(source.project_id ?? source.projectId ?? '')));
}

function sprintRows(index, project) {
  const ids = projectIds(index);
  const owner = upper(project);
  const tickets = ticketsFor(index, project);
  return [...(index?.sprints?.values?.() ?? [])]
    .filter(record => sprintRecordOwner(record, ids) === owner)
    .map((record, index) => {
      const metadata = recordFields(record);
      const source = object(metadata.source?.source ?? metadata.source ?? sourceOf(record));
      const id = text(source.id ?? metadata.id ?? metadata.slug ?? metadata.key, `sprint-${index + 1}`);
      const slug = text(metadata.slug ?? metadata.key ?? id);
      const members = tickets.filter(ticket => String(recordFields(ticket).sprint ?? '') === slug);
      const done = members.filter(ticket => upper(ticket.statusCategory ?? recordFields(ticket).statusCategory) === 'DONE' || upper(recordFields(ticket).status) === 'DONE');
      return {
        id,
        slug,
        key: slug,
        name: text(metadata.name ?? metadata.title ?? source.name, slug),
        goal: metadata.goal ?? source.goal ?? null,
        status: upper(metadata.status ?? source.status ?? 'PLANNED'),
        startDate: metadata.startDate ?? metadata.start_date ?? source.startDate ?? source.start_date ?? null,
        endDate: metadata.endDate ?? metadata.end_date ?? source.endDate ?? source.end_date ?? null,
        completedAt: metadata.completedAt ?? metadata.completed_at ?? source.completedAt ?? source.completed_at ?? null,
        continuous: Boolean(metadata.continuous ?? source.continuous),
        position: number(metadata.position ?? source.position, index),
        issues: members.length,
        points: members.reduce((sum, ticket) => sum + (number(recordFields(ticket).storyPoints ?? recordFields(ticket).story_points, 0) ?? 0), 0),
        donePoints: done.reduce((sum, ticket) => sum + (number(recordFields(ticket).storyPoints ?? recordFields(ticket).story_points, 0) ?? 0), 0),
        gate: metadata.gate ?? source.gate_issue_id ?? source.gateIssueId ?? null,
        path: record.relativePath,
        hash: record.hash ?? null,
        size: record.size ?? null,
        mtimeMs: record.mtimeMs ?? record.stat?.mtimeMs ?? null,
        source: publicValue(source),
      };
    })
    .sort((a, b) => a.position - b.position || a.name.localeCompare(b.name));
}

function versionRows(index, project) {
  const ids = projectIds(index);
  const owner = upper(project);
  const tickets = ticketsFor(index, project);
  const records = configRecords(index, 'versions', project);
  const values = [];
  const seen = new Set();
  const add = (row, sourceRecord = null, trusted = false) => {
    if (!row || typeof row !== 'object') return;
    const resolved = rowProject(row, ids);
    if (trusted) {
      // Own catalog records are already project-scoped, so a row whose
      // reference cannot be resolved stays; only a row that resolves to
      // another project is dropped.
      if (resolved && resolved !== owner) return;
    } else if (rowHasProjectRef(row) && resolved !== owner) {
      // Mirror boardModel's scoping: a row that carries a project reference
      // must belong to the owner — whether the reference resolves elsewhere
      // or not at all. A row with no project reference is global and stays.
      return;
    }
    const id = text(row.id ?? row.name ?? row.slug);
    const identity = id || JSON.stringify(row);
    if (seen.has(identity)) return;
    seen.add(identity);
    const name = text(row.name ?? row.slug ?? row.id, identity);
    const members = tickets.filter(ticket => array(recordFields(ticket).fixVersions ?? recordFields(ticket).fix_versions)
      .some(value => text(value?.name ?? value?.id ?? value).toLowerCase() === name.toLowerCase()));
    const done = members.filter(ticket => upper(ticket.statusCategory ?? recordFields(ticket).statusCategory) === 'DONE' || upper(recordFields(ticket).status) === 'DONE');
    values.push({
      id: id || name,
      name,
      description: row.description ?? null,
      status: upper(row.status ?? 'UNRELEASED'),
      releaseDate: row.releaseDate ?? row.release_date ?? null,
      startDate: row.startDate ?? row.start_date ?? null,
      position: number(row.position, values.length),
      issues: members.length,
      done: done.length,
      points: members.reduce((sum, ticket) => sum + (number(recordFields(ticket).storyPoints ?? recordFields(ticket).story_points, 0) ?? 0), 0),
      donePoints: done.reduce((sum, ticket) => sum + (number(recordFields(ticket).storyPoints ?? recordFields(ticket).story_points, 0) ?? 0), 0),
      path: sourceRecord?.relativePath ?? null,
      hash: sourceRecord?.hash ?? null,
      size: sourceRecord?.size ?? null,
      mtimeMs: sourceRecord?.mtimeMs ?? sourceRecord?.stat?.mtimeMs ?? null,
      source: publicValue(row),
    });
  };
  for (const record of records) for (const row of array(recordFields(record).rows)) add(row, record);
  for (const row of namedRows(index, 'versions')) add(row, records[0] ?? null);
  for (const row of rowsFor(index, 'jira.versions')) add(row, null);
  return values.sort((a, b) => a.position - b.position || a.name.localeCompare(b.name));
}

function usersFor(index, project) {
  const ids = projectIds(index);
  const rows = [
    ...rowsFor(index, 'public.users'),
    ...catalogRows(index, ['users', 'members', 'identities', 'assignees'], project),
  ];
  const result = [];
  const seen = new Set();
  for (const row of rows) {
    const id = row?.id ?? row?.user_id ?? row?.userId ?? row?.email;
    const key = String(id ?? row?.name ?? '');
    if (!key || seen.has(key)) continue;
    seen.add(key);
    result.push({
      id: row?.id ?? row?.user_id ?? row?.userId ?? null,
      name: row?.name ?? row?.display_name ?? row?.displayName ?? row?.username ?? row?.email ?? key,
      email: row?.email ?? row?.mail ?? row?.username ?? null,
      project: rowProject(row, ids),
    });
  }
  return result.sort((a, b) => String(a.name).localeCompare(String(b.name)));
}

function boardModel(index, project, statuses) {
  const ids = projectIds(index);
  const owner = upper(project);
  const projectIdsForBoard = new Set([...ids.entries()].filter(([, key]) => key === owner).map(([id]) => id));
  const boards = rowsFor(index, 'jira.boards').filter(row => !row?.project_id || projectIdsForBoard.has(String(row.project_id)) || rowProject(row, ids) === owner);
  const columns = rowsFor(index, 'jira.board_columns');
  const columnStatuses = rowsFor(index, 'jira.board_column_statuses');
  const quickFilters = rowsFor(index, 'jira.board_quick_filters');
  const resultBoards = boards.map((board, index) => {
    const boardId = String(board.id ?? `${owner.toLowerCase()}-board-${index + 1}`);
    const boardColumns = columns.filter(column => String(column.board_id ?? column.boardId ?? '') === boardId)
      .sort((a, b) => number(a.position, 0) - number(b.position, 0))
      .map(column => ({
        ...publicValue(column),
        id: column.id,
        name: column.name,
        position: number(column.position, 0),
        wipLimit: column.wipLimit ?? column.wip_limit ?? null,
        statusIds: columnStatuses.filter(row => String(row.column_id ?? row.columnId ?? '') === String(column.id)).sort((a, b) => number(a.position, 0) - number(b.position, 0)).map(row => row.status_id ?? row.statusId),
      }));
    return { ...publicValue(board), id: board.id ?? `${owner.toLowerCase()}-board-${index + 1}`, name: board.name ?? 'Board', type: board.type ?? 'KANBAN', columns: boardColumns };
  });
  if (!resultBoards.length) {
    resultBoards.push({
      id: `${owner.toLowerCase()}-board`,
      name: 'Board',
      type: 'KANBAN',
      columns: statuses.map(status => ({ id: status.id, name: status.name, position: status.position, statusIds: [status.id] })),
    });
  }
  return { boards: resultBoards, columns: resultBoards[0].columns, quickFilters: publicValue(quickFilters.filter(row => rowProject(row, ids) === owner || !rowProject(row, ids))) };
}

function historyRows(index, project, issue = null) {
  const wantedProject = upper(project);
  const id = issue ? sourceId(issue) : null;
  const key = issue ? upper(recordFields(issue).key) : null;
  const users = new Map(rowsFor(index, 'public.users').map(row => [String(row?.id ?? ''), row]));
  const values = [];
  for (const record of index?.history?.values?.() ?? []) {
    if (recordProject(record) !== wantedProject) continue;
    for (const row of array(recordFields(record).rows)) {
      if (issue) {
        const rowIssue = String(row?.issue_id ?? row?.issueId ?? row?.issue_key ?? row?.issueKey ?? '');
        if (rowIssue !== String(id ?? '') && upper(rowIssue) !== key) continue;
      }
      const actor = users.get(String(row?.actor_id ?? row?.actorId ?? ''));
      values.push({
        ...publicValue(row),
        id: row?.id ?? null,
        issueId: row?.issue_id ?? row?.issueId ?? null,
        field: row?.field ?? null,
        fromValue: row?.fromValue ?? row?.from_value ?? row?.from ?? null,
        toValue: row?.toValue ?? row?.to_value ?? row?.to ?? null,
        actorId: row?.actor_id ?? row?.actorId ?? null,
        actorName: actor?.name ?? actor?.display_name ?? null,
        actorEmail: actor?.email ?? null,
        createdAt: row?.createdAt ?? row?.created_at ?? null,
      });
    }
  }
  return values.sort((a, b) => String(a.createdAt ?? '').localeCompare(String(b.createdAt ?? '')));
}

function decodeContent(value) {
  const textValue = value === undefined || value === null ? '' : String(value);
  if (/^\\x[0-9a-f]+$/i.test(textValue) && (textValue.length - 2) % 2 === 0) {
    try { return Buffer.from(textValue.slice(2), 'hex').toString('utf8'); } catch { return textValue; }
  }
  return textValue;
}

function linksFor(index, issue) {
  const issueId = sourceId(issue);
  const key = upper(recordFields(issue).key);
  const byId = new Map(allTicketRecords(index).map(record => [sourceId(record), record]).filter(([id]) => id));
  const outgoing = [];
  const incoming = [];
  const seen = new Set();
  const add = (direction, row, target, type = 'RELATES_TO') => {
    if (!target) return;
    const metadata = recordFields(target);
    const targetKey = text(metadata.key);
    if (!targetKey) return;
    const identity = `${direction}:${row?.id ?? ''}:${targetKey}:${type}`;
    if (seen.has(identity)) return;
    seen.add(identity);
    const payload = {
      id: row?.id ?? null,
      type: upper(type || row?.type || 'RELATES_TO'),
      direction,
      key: targetKey,
      title: text(metadata.title, targetKey),
      status: metadata.status ?? null,
      statusCategory: target.statusCategory ?? metadata.statusCategory ?? null,
      reason: row?.reason ?? null,
      label: direction === 'outgoing' ? { BLOCKS: 'blocks', RELATES_TO: 'relates to', DUPLICATES: 'duplicates', CAUSES: 'causes' }[upper(type)] ?? upper(type) : { BLOCKS: 'is blocked by', RELATES_TO: 'relates to', DUPLICATES: 'is duplicated by', CAUSES: 'is caused by' }[upper(type)] ?? upper(type),
    };
    (direction === 'outgoing' ? outgoing : incoming).push(payload);
  };
  for (const row of rowsFor(index, 'jira.issue_links')) {
    const source = byId.get(String(row?.source_issue_id ?? row?.sourceIssueId ?? ''));
    const target = byId.get(String(row?.target_issue_id ?? row?.targetIssueId ?? ''));
    if (!source || !target) continue;
    if (source === issue || upper(recordFields(source).key) === key) add('outgoing', row, target, row?.type);
    if (target === issue || upper(recordFields(target).key) === key) add('incoming', row, source, row?.type);
  }
  for (const row of outgoingLinks(issue)) {
    const target = allTicketRecords(index).find(candidate => upper(recordFields(candidate).key) === upper(row.key));
    add('outgoing', row, target, row.type);
  }
  return { outgoing, incoming };
}

function reportsFor(index, project, tickets, sprints, statuses) {
  const history = historyRows(index, project);
  const byId = new Map(tickets.map(ticket => [sourceId(ticket) ?? text(recordFields(ticket).key), ticket]));
  const doneStatusNames = statuses.filter(status => status.category === 'DONE').map(status => status.name);
  const sprintValueMatches = (value, sprint) => {
    const candidate = text(value).toLowerCase();
    if (!candidate) return false;
    return [sprint.id, sprint.slug, sprint.key, sprint.name].some(item => text(item).toLowerCase() === candidate);
  };
  const issueIdOf = ticket => sourceId(ticket) ?? text(recordFields(ticket).key);
  const activity = history
    .map(row => ({
      ...row,
      issueId: String(row.issueId ?? ''),
      createdAt: new Date(row.createdAt),
    }))
    .filter(row => byId.has(row.issueId) && !Number.isNaN(row.createdAt.getTime()));
  const velocityRows = sprints.filter(sprint => sprint.status === 'COMPLETED').map(sprint => ({
    id: sprint.id,
    name: sprint.name,
    committed: sprint.points || 0,
    planned: sprint.points || 0,
    completed: sprint.donePoints || sprint.done || 0,
    donePoints: sprint.donePoints || sprint.done || 0,
    points: sprint.points || 0,
  }));
  const epics = tickets.filter(ticket => upper(recordFields(ticket).type) === 'EPIC').map(epic => {
    const epicKey = text(recordFields(epic).key);
    const children = tickets.filter(ticket => String(recordFields(ticket).parent ?? '').toUpperCase() === epicKey.toUpperCase());
    const points = children.reduce((sum, child) => sum + (number(recordFields(child).storyPoints ?? recordFields(child).story_points, 0) ?? 0), 0);
    const done = children.filter(child => upper(child.statusCategory ?? recordFields(child).statusCategory) === 'DONE' || upper(recordFields(child).status) === 'DONE');
    return { id: sourceId(epic) ?? epicKey, key: epicKey, title: text(recordFields(epic).title, epicKey), total: children.length, done: done.length, points, donePoints: done.reduce((sum, child) => sum + (number(recordFields(child).storyPoints ?? recordFields(child).story_points, 0) ?? 0), 0), percent: epicPercent({ total: children.length, done: done.length, points, donePoints: done.reduce((sum, child) => sum + (number(recordFields(child).storyPoints ?? recordFields(child).story_points, 0) ?? 0), 0) }) };
  });
  const burndownBySprint = {};
  for (const sprint of sprints) {
    const start = new Date(sprint.startDate);
    const end = new Date(sprint.endDate ?? sprint.completedAt);
    if (!sprint.startDate || Number.isNaN(start.getTime()) || Number.isNaN(end.getTime())) continue;
    const currentMembers = tickets.filter(ticket => sprintValueMatches(recordFields(ticket).sprint, sprint));
    const memberIds = new Set(currentMembers.map(issueIdOf));
    // A ticket that left the sprint is still part of its historical scope.
    for (const row of activity) {
      if (row.field !== 'sprint' || !sprintValueMatches(row.fromValue, sprint) && !sprintValueMatches(row.toValue, sprint)) continue;
      const ticket = byId.get(row.issueId);
      if (ticket) { currentMembers.push(ticket); memberIds.add(issueIdOf(ticket)); }
    }
    const scope = [...memberIds].map(id => byId.get(id)).filter(Boolean).map(ticket => ({
      issueId: issueIdOf(ticket),
      storyPoints: number(recordFields(ticket).storyPoints ?? recordFields(ticket).story_points, 0) ?? 0,
      done: doneStatusNames.some(name => text(recordFields(ticket).status).toLowerCase() === text(name).toLowerCase())
        || upper(ticket.statusCategory ?? recordFields(ticket).statusCategory) === 'DONE',
    }));
    const points = buildBurndown({ start, end, scope, activity, doneStatusNames });
    burndownBySprint[sprint.id] = points;
  }
  const current = sprints.find(sprint => sprint.status === 'ACTIVE') ?? sprints[0];
  const burndown = current ? (burndownBySprint[current.id] ?? []) : [];
  return { burndown, burndownBySprint, velocityRows, average: averageVelocity(velocityRows), epics };
}

function projectSummary(index, key) {
  const wanted = upper(key);
  const row = projectRows(index).get(wanted);
  const root = configuredRoot(index, wanted);
  const configProject = object(index?.vaultConfig?.projects ?? index?.config?.projects)[wanted]
    ?? object(index?.vaultConfig?.projects ?? index?.config?.projects)[key];
  if (!row && !root && !configProject) return null;
  const readOnly = index?.vaultConfig?.writes?.enabled !== true || root?.readOnly === true || !root;
  return {
    key: wanted,
    name: text(row?.name ?? configProject?.name ?? root?.name, `Project ${wanted}`),
    description: row?.description ?? configProject?.description ?? null,
    readOnly,
    path: root?.path ?? null,
  };
}

export class ProjectApiError extends Error {
  constructor(code, message) { super(message); this.code = code; }
}

export function projectList(index) {
  const keys = new Set(array(index?.boardRoots).map(root => upper(root?.project)).filter(Boolean));
  for (const row of rowsFor(index, 'jira.projects')) {
    const key = upper(row?.key ?? row?.project_key ?? row?.projectKey);
    if (key) keys.add(key);
  }
  for (const key of Object.keys(object(index?.vaultConfig?.projects ?? index?.config?.projects))) keys.add(upper(key));
  const projects = [...keys].map(key => projectSummary(index, key)).filter(Boolean).sort((a, b) => a.key.localeCompare(b.key));
  return { projects, revisions: revisionMap([index?.configRecord].filter(Boolean)) };
}

export function projectDetail(index, key) {
  const project = projectSummary(index, key);
  if (!project) throw new ProjectApiError('NOT_FOUND', `Project ${key} was not found`);
  const tickets = ticketsFor(index, project.key);
  const statuses = statusRows(index, project.key);
  const workflow = workflowModel(index, project.key);
  const sprints = sprintRows(index, project.key);
  const versions = versionRows(index, project.key);
  const users = usersFor(index, project.key);
  const labels = catalogRows(index, ['labels', 'jira.labels'], project.key).map(row => ({ ...publicValue(row), id: row.id ?? row.name, name: row.name ?? row.label ?? row.id }));
  const categories = catalogRows(index, ['categories', 'jira.categories'], project.key).map(row => ({ ...publicValue(row), id: row.id ?? row.name, name: row.name ?? row.label ?? row.id }));
  const customFields = customFieldRows(index, project.key);
  const fieldConfiguration = catalogRows(index, ['fieldConfiguration', 'field_configurations', 'jira.field_configurations'], project.key);
  const screens = screenRows(index, project.key);
  const board = boardModel(index, project.key, statuses);
  const projectRecords = [
    index?.configRecord,
    ...[...(index?.configs?.values?.() ?? [])].filter(record => record?.project === project.key || record?.project === null),
    ...tickets,
    ...sprints.map(sprint => index.getFile(sprint.path)).filter(Boolean),
    ...configRecords(index, 'workflow', project.key),
    ...configRecords(index, 'labels', project.key),
    ...configRecords(index, 'versions', project.key),
    ...configRecords(index, 'customFields', project.key),
    ...configRecords(index, 'fieldConfiguration', project.key),
    ...configRecords(index, 'screens', project.key),
    ...[...(index?.metadataFiles?.values?.() ?? [])],
  ];
  const reports = reportsFor(index, project.key, tickets, sprints, statuses);
  return {
    ...project,
    tickets: tickets.map(serializeTicket),
    statuses,
    labels,
    categories,
    users,
    sprints,
    versions,
    board,
    workflow,
    customFields,
    fieldConfiguration,
    screens,
    revisions: revisionMap(projectRecords),
    reports,
    actions: {
      create: { action: 'issue.create' },
      update: { action: 'issue.update' },
      delete: { action: 'issue.delete' },
      createComment: { action: 'comment.create' },
      deleteComment: { action: 'comment.delete' },
    },
  };
}

export function issueDetail(index, requestedKey, { actor } = {}) {
  const wanted = text(requestedKey);
  const issue = allTicketRecords(index).find(record => upper(recordFields(record).key) === upper(wanted));
  if (!issue) throw new ProjectApiError('NOT_FOUND', `Issue ${wanted} was not found`);
  const project = projectSummary(index, recordProject(issue));
  const tickets = ticketsFor(index, project?.key ?? recordProject(issue));
  const parentKey = text(recordFields(issue).parent);
  const parent = parentKey ? tickets.find(record => upper(recordFields(record).key) === upper(parentKey)) : null;
  const children = tickets.filter(record => upper(recordFields(record).parent) === upper(recordFields(issue).key));
  const revisions = revisionMap([issue]);
  return {
    issue: serializeTicket(issue),
    revision: revisionOf(issue),
    revisions,
    comments: issueComments(index, issue, actor),
    links: linksFor(index, issue),
    family: { parent: compactTicket(parent), children: children.map(compactTicket) },
    history: historyRows(index, recordProject(issue), issue),
    project,
  };
}

/**
 * Add committed activity after the imported snapshot boundary. The ordinary
 * read model stays synchronous for fixture/domain callers; HTTP may opt into
 * this async variant for live writable vaults. Cache by repository HEAD,
 * issue and scanner version so a project page never performs a serial
 * history walk for every ticket.
 */
export async function issueDetailAsync(index, requestedKey, { root = index?.root, actor } = {}) {
  const detail = issueDetail(index, requestedKey, { actor });
  if (!root || index?.vaultConfig?.writes?.enabled !== true) return detail;
  let head;
  try {
    const { git } = await import('./operations.mjs');
    head = await git(root, ['rev-parse', 'HEAD']);
  } catch {
    return detail;
  }
  const cacheKey = [root, head, index.version ?? '', detail.issue?.key ?? requestedKey].join('|');
  if (!gitHistoryCache.has(cacheKey)) {
    gitHistoryCache.set(cacheKey, appendGitHistory(root, detail.issue.key, detail.history));
  }
  detail.history = await gitHistoryCache.get(cacheKey);
  return detail;
}

export function projectRecord(index, key) {
  return projectDetail(index, key);
}

export default { projectList, projectDetail, issueDetail, issueDetailAsync, revisionOf };
