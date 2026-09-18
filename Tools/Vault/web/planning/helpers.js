import React from 'react';

import { withLockRetry } from '../project-api.js';

let operationSequence = 0;

export function projectKeyOf(project) {
  const value = project?.key ?? project?.projectKey ?? project?.project?.key ?? project;
  return value === undefined || value === null ? '' : String(value).trim().toUpperCase();
}

export function text(value, fallback = '') {
  if (value === undefined || value === null) return fallback;
  const result = String(value).trim();
  return result || fallback;
}

export function array(value) {
  return Array.isArray(value) ? value : [];
}

export function unique(values) {
  return [...new Set(array(values).map(value => text(value)).filter(Boolean))];
}

export function issueKey(issue) {
  return text(issue?.key ?? issue?.metadata?.key ?? issue?.fields?.key ?? issue?.id ?? issue?.path);
}

export function issueTitle(issue) {
  return text(issue?.title ?? issue?.metadata?.title ?? issue?.fields?.title, issueKey(issue) || 'Untitled issue');
}

export function issueType(issue) {
  return text(issue?.type ?? issue?.metadata?.type ?? issue?.fields?.type, 'TASK').toUpperCase();
}

export function issueStatus(issue) {
  return text(issue?.status ?? issue?.metadata?.status ?? issue?.fields?.status);
}

export function issueStatusCategory(issue, statuses = []) {
  const explicit = text(issue?.statusCategory ?? issue?.metadata?.statusCategory ?? issue?.fields?.statusCategory);
  if (explicit) return explicit.toUpperCase();
  const status = issueStatus(issue);
  const catalog = array(statuses).find(item => text(item?.name) === status);
  return text(catalog?.category).toUpperCase();
}

export function issuePriority(issue) {
  return text(issue?.priority ?? issue?.metadata?.priority ?? issue?.fields?.priority, 'UNSET').toUpperCase();
}

export function issueAssignee(issue) {
  const value = issue?.assignee ?? issue?.assigneeEmail ?? issue?.metadata?.assignee ?? issue?.metadata?.assigneeEmail
    ?? issue?.fields?.assignee ?? issue?.fields?.assignee_email;
  if (value && typeof value === 'object') return text(value.email ?? value.name ?? value.id);
  return text(value);
}

export function issueLabels(issue) {
  return unique(issue?.labels ?? issue?.metadata?.labels ?? issue?.fields?.labels);
}

export function issueParent(issue) {
  const value = issue?.parent ?? issue?.metadata?.parent ?? issue?.fields?.parent;
  if (value && typeof value === 'object') return text(value.key ?? value.id ?? value.name);
  return text(value);
}

export function issueSprint(issue) {
  const value = issue?.sprint ?? issue?.sprintId ?? issue?.metadata?.sprint ?? issue?.metadata?.sprintId
    ?? issue?.fields?.sprint ?? issue?.fields?.sprint_id;
  if (value && typeof value === 'object') return text(value.id ?? value.slug ?? value.name);
  return text(value);
}

export function issueVersions(issue) {
  const value = issue?.fixVersions ?? issue?.versions ?? issue?.metadata?.fixVersions ?? issue?.metadata?.versions
    ?? issue?.fields?.fixVersions;
  return array(value).map(item => text(item && typeof item === 'object' ? item.name ?? item.id : item)).filter(Boolean);
}

export function statusId(status) {
  return text(status?.id ?? status?.name);
}

export function statusName(status) {
  return text(status?.name ?? status?.status ?? status?.id);
}

export function sprintId(sprint) {
  return text(sprint?.id ?? sprint?.slug ?? sprint?.name);
}

export function sprintName(sprint) {
  return text(sprint?.name ?? sprint?.title ?? sprint?.slug ?? sprintId(sprint));
}

export function sprintStatus(sprint) {
  return text(sprint?.status, 'PLANNED').toUpperCase();
}

export function versionId(version) {
  return text(version?.id ?? version?.name);
}

export function versionName(version) {
  return text(version?.name ?? version?.title ?? versionId(version));
}

export function isDone(issue, statuses = []) {
  return issueStatusCategory(issue, statuses) === 'DONE' || issueStatus(issue).toUpperCase() === 'DONE';
}

export function numeric(value, fallback = 0) {
  const result = Number(value);
  return Number.isFinite(result) ? result : fallback;
}

export function positionOf(value, fallback = Number.MAX_SAFE_INTEGER) {
  const result = Number(value);
  return Number.isFinite(result) ? result : fallback;
}

/**
 * Project catalog writes are CAS guarded by the revision map returned with
 * the project detail. Keep this normalization in one place so every planning
 * and settings action sends the same baseline to the shared writer.
 */
export function projectRevisionMap(project) {
  const source = project?.revisions ?? project?.revisionMap ?? {};
  if (Array.isArray(source)) {
    return Object.fromEntries(source
      .filter(entry => entry && typeof entry === 'object' && entry.path)
      .map(entry => [entry.path, { hash: entry.hash, size: entry.size, mtimeMs: entry.mtimeMs }]));
  }
  if (!source || typeof source !== 'object') return {};
  return Object.fromEntries(Object.entries(source).map(([path, revision]) => [path, revision ?? null]));
}

export function formatDate(value) {
  if (!value) return '';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return text(value);
  return date.toLocaleDateString(undefined, { year: 'numeric', month: 'short', day: 'numeric' });
}

export function dateInputValue(value) {
  if (!value) return '';
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return text(value).slice(0, 10);
  return date.toISOString().slice(0, 10);
}

export function nextOperationId(prefix = 'planning') {
  operationSequence += 1;
  return `${prefix}-${Date.now().toString(36)}-${operationSequence}`;
}

async function unwrapResponse(response) {
  if (!response) return {};
  if (typeof response.json === 'function') {
    const payload = await response.json();
    if (response.ok === false) {
      const error = new Error(payload?.error?.message ?? payload?.error ?? `Request failed (${response.status})`);
      error.code = payload?.error?.code;
      error.status = response.status;
      throw error;
    }
    return payload;
  }
  if (response.error) {
    const error = new Error(response.error.message ?? response.error);
    error.code = response.error.code;
    error.status = response.status;
    throw error;
  }
  return response;
}

export async function submitCommand(api, args, { files = {}, revisions = {}, operationId, retryDelays } = {}) {
  if (typeof api !== 'function') throw new Error('Planning API is unavailable');
  // The envelope is built once so every 423 retry replays the SAME
  // operationId — the server treats that as one idempotent operation.
  const body = { args, files, revisions, operationId: operationId ?? nextOperationId('command') };
  return withLockRetry(
    () => unwrapResponse(api('/api/commands', { method: 'POST', body })),
    { delays: retryDelays },
  );
}

export async function submitAction(api, action, project, data = {}, { key, revisions = {}, operationId, retryDelays } = {}) {
  if (typeof api !== 'function') throw new Error('Planning API is unavailable');
  const body = {
    action,
    project: projectKeyOf(project),
    key: key ?? data.id ?? data.slug ?? null,
    data,
    revisions,
    operationId: operationId ?? nextOperationId('action'),
  };
  return withLockRetry(
    () => unwrapResponse(api('/api/actions', { method: 'POST', body })),
    { delays: retryDelays },
  );
}

export function usePlanningMutation(api, onChanged) {
  const [pending, setPending] = React.useState(false);
  const [error, setError] = React.useState('');
  const retry = React.useRef(null);

  const execute = React.useCallback(async request => {
    const signature = JSON.stringify(request);
    const operationId = retry.current?.signature === signature
      ? retry.current.operationId
      : request.options?.operationId ?? nextOperationId(request.kind === 'action' ? 'action' : 'command');
    const options = { ...(request.options ?? {}), operationId };
    setPending(true);
    setError('');
    try {
      const response = request.kind === 'action'
        ? await submitAction(api, request.action, request.project, request.data, options)
        : await submitCommand(api, request.args, options);
      await onChanged?.(response);
      if (retry.current?.signature === signature) retry.current = null;
      return response;
    } catch (caught) {
      retry.current = { signature, operationId };
      const message = caught?.message ?? String(caught);
      setError(message);
      throw caught;
    } finally {
      setPending(false);
    }
  }, [api, onChanged]);

  return { execute, pending, error, clearError: () => setError(''), reject: message => setError(text(message, 'Planning change was rejected')) };
}
