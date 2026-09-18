/*
 * Issue interactions that do not belong to the issue property writer.
 *
 * Comments added by the local UI live in the issue's frontmatter under the
 * `comments` field.  The exported Jira rows remain in metadata.md and are
 * deliberately never rewritten: the read helper joins both sources at the
 * API boundary.  Keeping local comments beside the issue gives them a
 * durable, revisioned file and lets the normal operation engine provide the
 * same lock, CAS, validation, journal, and commit guarantees as other writes.
 */

import { createHash, randomUUID } from 'node:crypto';

import { allTicketRecords, canonicalOf, idAndNumber, projectKeyOf, sourceOf } from '../cli/compat.mjs';
import { rowsFor } from '../cli/queries.mjs';
import { patchRecord } from '../cli/write.mjs';
import { executeOperation } from './operations.mjs';

export const ISSUE_INTERACTION_ACTIONS = new Set([
  'comment.create',
  'comment.delete',
  'issue.delete',
]);

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;
const OPERATION_ID = /^[A-Za-z0-9_-]{1,100}$/;
const LOCAL_COMMENT_FIELDS = ['comments', 'localComments', 'vaultComments', 'issueComments'];
const PARENT_FIELDS = ['parent', 'parent_key', 'parentKey'];
const RELATION_FIELDS = ['blocks', 'links', 'blockedBy', 'blocked_by', 'blockedByKeys'];

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value : [];
}

function text(value, fallback = '') {
  if (value === undefined || value === null) return fallback;
  return String(value);
}

function nonEmpty(value, label) {
  if (typeof value !== 'string' || !value.trim()) fail('INVALID', `${label} is required`);
  return value.trim();
}

function fail(code, message) {
  const error = new Error(message);
  error.code = code;
  throw error;
}

function assertPlain(value, label) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) fail('INVALID', `${label} must be an object`);
  return value;
}

function operationId(value) {
  if (value === undefined || value === null || value === '') return randomUUID();
  if (typeof value !== 'string' || !OPERATION_ID.test(value)) fail('ID', 'Invalid operation ID');
  return value;
}

function normalizeRevisions(value) {
  if (value === undefined) return {};
  const revisions = assertPlain(value, 'revisions');
  for (const [relative, revision] of Object.entries(revisions)) {
    if (!relative || typeof relative !== 'string' || relative.includes('\\') || relative.startsWith('/')
      || relative.includes('\0') || relative.split('/').some(part => part === '..' || part === '.git' || part === '.zagent')) {
      fail('PATH', 'Revision path escapes the vault');
    }
    if (revision !== null && (!revision || typeof revision !== 'object' || Array.isArray(revision))) {
      fail('INVALID', `revision for ${relative} must be an object or null`);
    }
  }
  return { ...revisions };
}

function normalizeRequest(input) {
  const body = assertPlain(input, 'action request');
  const allowed = new Set(['action', 'project', 'key', 'data', 'revisions', 'operationId']);
  for (const name of Object.keys(body)) if (!allowed.has(name)) fail('INVALID', `Unknown action input "${name}"`);
  const action = nonEmpty(body.action, 'action');
  if (!ISSUE_INTERACTION_ACTIONS.has(action)) fail('INVALID', `Unsupported issue interaction: ${action}`);
  const project = body.project === undefined || body.project === null || body.project === ''
    ? null : nonEmpty(body.project, 'project');
  const key = body.key === undefined || body.key === null || body.key === ''
    ? null : nonEmpty(body.key, 'issue key');
  if (key && !ISSUE_KEY.test(key)) fail('INVALID', `Invalid issue key: ${key}`);
  const data = body.data === undefined ? {} : assertPlain(body.data, 'data');
  const revisions = normalizeRevisions(body.revisions);
  return { action, project, key, data, revisions, operationId: operationId(body.operationId) };
}

function validateData(request) {
  const keys = Object.keys(request.data);
  const allowed = request.action === 'comment.create'
    ? new Set(['body'])
    : request.action === 'comment.delete'
      ? new Set(['commentId'])
      : new Set();
  for (const key of keys) if (!allowed.has(key)) fail('INVALID', `Unknown ${request.action} data field "${key}"`);
  if (request.action === 'comment.create') {
    const body = nonEmpty(request.data.body, 'comment body');
    if (body.length > 100000) fail('INVALID', 'comment body is too long');
    request.data = { body };
  } else if (request.action === 'comment.delete') {
    request.data = { commentId: nonEmpty(request.data.commentId, 'commentId') };
  } else {
    request.data = {};
  }
}

function fieldsOf(record) {
  return canonicalOf(record);
}

function issueKey(record) {
  return text(fieldsOf(record).key ?? record?.key).trim();
}

function findIssue(index, requested) {
  const wanted = text(requested).trim().toUpperCase();
  if (!wanted) return null;
  const direct = index?.getTicket?.(requested) ?? index?.getTicket?.(wanted);
  if (direct) return direct;
  return allTicketRecords(index).find(record => issueKey(record).toUpperCase() === wanted) ?? null;
}

function projectOf(record) {
  return String(record?.project ?? projectKeyOf(record, issueKey(record).split('-')[0]) ?? '').toUpperCase() || null;
}

function assertIssueScope(request, record) {
  if (!record) return;
  if (request.project && projectOf(record) !== request.project.toUpperCase()) {
    fail('NOT_FOUND', `Issue ${request.key} was not found in project ${request.project}`);
  }
}

function actorValue(context) {
  const raw = context?.actor;
  if (raw === undefined || raw === null || raw === '') fail('AUTH', 'A named actor is required for issue interactions');
  if (typeof raw === 'string') {
    const email = raw.trim();
    if (!email) fail('AUTH', 'A named actor is required for issue interactions');
    return { id: `local:${email.toLowerCase()}`, email, name: email };
  }
  if (!raw || typeof raw !== 'object' || Array.isArray(raw)) fail('AUTH', 'A named actor is required for issue interactions');
  const email = text(raw.email ?? raw.username ?? raw.login).trim();
  const id = text(raw.id ?? raw.userId ?? raw.actorId).trim() || (email ? `local:${email.toLowerCase()}` : '');
  if (!id) fail('AUTH', 'A named actor is required for issue interactions');
  return {
    id,
    ...(text(raw.name ?? raw.displayName).trim() ? { name: text(raw.name ?? raw.displayName).trim() } : {}),
    ...(email ? { email } : {}),
  };
}

function publicActor(actor) {
  return {
    id: text(actor?.id) || null,
    name: text(actor?.name ?? actor?.displayName ?? actor?.email) || null,
    email: text(actor?.email) || null,
  };
}

function actorIdentities(actor) {
  const value = actorValue({ actor });
  const ids = new Set([value.id].filter(Boolean).map(item => String(item).trim().toLowerCase()));
  const email = text(value.email).trim().toLowerCase();
  if (email) {
    ids.add(email);
    ids.add(`local:${email}`);
  }
  return { value, ids, email };
}

function commentAuthor(row) {
  const author = object(row?.author);
  const id = text(author.id ?? author.userId ?? row?.authorId ?? row?.author_id).trim();
  const email = text(author.email ?? author.username ?? row?.authorEmail ?? row?.author_email).trim();
  const name = text(author.name ?? author.displayName ?? row?.authorName ?? row?.author_name).trim();
  return {
    ...(id ? { id } : {}),
    ...(name ? { name } : {}),
    ...(email ? { email } : {}),
  };
}

function sameActor(author, actor) {
  if (actor === undefined || actor === null) return false;
  const normalized = actorIdentities(actor);
  const authorId = text(author?.id ?? author?.userId).trim().toLowerCase();
  const authorEmail = text(author?.email ?? author?.username).trim().toLowerCase();
  if (authorId && normalized.ids.has(authorId)) return true;
  if (authorEmail && normalized.email && authorEmail === normalized.email) return true;
  return false;
}

function decodeBody(value) {
  if (Buffer.isBuffer(value) || value instanceof Uint8Array) return Buffer.from(value).toString('utf8');
  if (value === undefined || value === null) return '';
  const result = String(value);
  if (/^\\x[0-9a-f]+$/i.test(result) && (result.length - 2) % 2 === 0) {
    try { return Buffer.from(result.slice(2), 'hex').toString('utf8'); } catch { return result; }
  }
  return result;
}

function stableImportedId(row, index) {
  if (row?.id !== undefined && row?.id !== null && String(row.id) !== '') return String(row.id);
  const encoded = JSON.stringify(row, (_, value) => Buffer.isBuffer(value) ? value.toString('base64') : value);
  return `imported-comment-${createHash('sha256').update(`${encoded}:${index}`).digest('hex').slice(0, 20)}`;
}

function sourceIdentity(record) {
  const identity = idAndNumber(record);
  const source = sourceOf(record);
  const value = identity.id ?? source.id;
  return value === undefined || value === null || value === '' ? null : String(value);
}

function localCommentField(record) {
  const fields = fieldsOf(record);
  for (const name of LOCAL_COMMENT_FIELDS) {
    if (Array.isArray(fields[name])) return { name, rows: fields[name] };
  }
  return { name: 'comments', rows: [] };
}

function localCommentRow(row, index, actor, sequence = index) {
  if (!row || typeof row !== 'object' || Array.isArray(row)) return null;
  const id = text(row.id ?? row.commentId).trim();
  const body = decodeBody(row.body ?? row.content ?? row.text);
  if (!id || !body) return null;
  const author = commentAuthor(row);
  const createdAt = row.createdAt ?? row.created_at ?? row.date ?? null;
  const updatedAt = row.updatedAt ?? row.updated_at ?? createdAt;
  return {
    id,
    body,
    author,
    createdAt,
    updatedAt,
    canDelete: row.canDelete !== false && sameActor(author, actor),
    origin: 'local',
    _index: index,
    _sequence: sequence,
  };
}

function importedCommentRow(row, index, issue, users) {
  if (!row || typeof row !== 'object' || Array.isArray(row)) return null;
  const authorId = row.author_id ?? row.authorId;
  const user = users.find(candidate => String(candidate?.id ?? '') === String(authorId ?? ''));
  const author = Object.keys(object(row.author)).length
    ? commentAuthor(row)
    : user
      ? { id: user.id ?? null, name: user.name ?? user.display_name ?? null, email: user.email ?? null }
      : { id: authorId ?? null, name: null, email: null };
  return {
    id: stableImportedId(row, index),
    body: decodeBody(row.content ?? row.body ?? row.text),
    author,
    createdAt: row.created_at ?? row.createdAt ?? row.date ?? null,
    updatedAt: row.updated_at ?? row.updatedAt ?? row.created_at ?? row.createdAt ?? null,
    canDelete: false,
    deletable: false,
    origin: 'imported',
    _index: index,
    _sequence: index,
    _issue: issue,
  };
}

function dateSortValue(value) {
  if (!value) return Number.MAX_SAFE_INTEGER;
  const parsed = Date.parse(String(value));
  return Number.isFinite(parsed) ? parsed : Number.MAX_SAFE_INTEGER;
}

/**
 * Join immutable imported Jira comment rows with local comments persisted in
 * the ticket frontmatter.  Imported rows are projected only; no caller can
 * accidentally delete or rewrite them through the interaction actions.
 */
export function issueComments(index, record, actor = undefined) {
  if (!record) return [];
  const sourceId = sourceIdentity(record);
  const key = issueKey(record).toUpperCase();
  const users = rowsFor(index, 'public.users');
  const imported = rowsFor(index, 'jira.comments')
    .filter(row => {
      const rowIssueId = row?.issue_id ?? row?.issueId;
      if (sourceId && rowIssueId !== undefined && rowIssueId !== null && String(rowIssueId) === sourceId) return true;
      const rowKey = row?.issue_key ?? row?.issueKey ?? row?.key;
      return Boolean(rowKey && String(rowKey).toUpperCase() === key);
    })
    .map((row, index) => importedCommentRow(row, index, record, users))
    .filter(Boolean);
  const local = localCommentField(record).rows
    .map((row, index) => localCommentRow(row, index, actor, imported.length + index))
    .filter(Boolean);
  return [...imported, ...local]
    .sort((left, right) => dateSortValue(left.createdAt) - dateSortValue(right.createdAt)
      || left._sequence - right._sequence)
    .map(({ _index, _sequence, _issue, ...comment }) => comment);
}

function revisionFor(record, revisions) {
  if (!record?.relativePath) fail('INVALID', 'Issue has no editable path');
  if (!Object.hasOwn(revisions, record.relativePath)) {
    const wanted = record.relativePath.toLowerCase();
    const alias = Object.keys(revisions).find(key => key.toLowerCase() === wanted);
    if (alias === undefined) fail('STALE', `An opened revision is required for ${record.relativePath}`);
    return { [record.relativePath]: revisions[alias] };
  }
  return { [record.relativePath]: revisions[record.relativePath] };
}

function pathFromRequest(index, request) {
  const candidates = Object.keys(request.revisions).filter(relative => {
    const normalized = relative.replaceAll('\\', '/').toLowerCase();
    const key = request.key.toLowerCase();
    return normalized.endsWith(`/tickets/${key}.md`) || normalized.endsWith(`/tickets/${key}.markdown`) || normalized.endsWith(`/${key}.md`) || normalized.endsWith(`/${key}.markdown`);
  });
  if (candidates.length === 1) return candidates[0];
  const root = array(index?.boardRoots).find(item => String(item?.project ?? '').toUpperCase() === String(request.project ?? request.key.split('-')[0]).toUpperCase());
  if (root?.path) return `${String(root.path).replaceAll('\\', '/').replace(/\/+$/, '')}/tickets/${request.key}.md`;
  return candidates[0] ?? null;
}

function targetForRequest(index, request) {
  const record = findIssue(index, request.key);
  assertIssueScope(request, record);
  return record;
}

function localRowsForIssue(record) {
  const field = localCommentField(record);
  return { field: field.name, rows: field.rows.map(row => row && typeof row === 'object' ? { ...row } : row) };
}

function localCommentById(record, id) {
  const { rows } = localRowsForIssue(record);
  return rows.find(row => row && typeof row === 'object' && text(row.id ?? row.commentId) === id && row.origin !== 'imported' && row.source !== 'imported') ?? null;
}

function publicLocalComment(row, actor) {
  const normalized = localCommentRow(row, 0, actor);
  if (!normalized) return null;
  const { _index, _sequence, ...comment } = normalized;
  return comment;
}

function relationTarget(value) {
  if (value === null || value === undefined) return '';
  if (typeof value === 'string' || typeof value === 'number') return String(value).trim();
  const row = object(value);
  return text(row.key ?? row.target ?? row.targetKey ?? row.ticket ?? row.ticketKey ?? row.issueKey ?? row.issue_key).trim();
}

function sameIssue(value, key) {
  return relationTarget(value).toUpperCase() === key.toUpperCase();
}

function removeIssueReferences(value, key) {
  if (!Array.isArray(value)) return value;
  return value.filter(item => !sameIssue(item, key));
}

function cleanupReferences(index, deletedKey, deletedRecord) {
  const changes = [];
  for (const candidate of allTicketRecords(index)) {
    if (candidate === deletedRecord || issueKey(candidate).toUpperCase() === deletedKey.toUpperCase()) continue;
    const current = fieldsOf(candidate);
    const updates = {};
    for (const field of PARENT_FIELDS) {
      if (Object.hasOwn(current, field) && sameIssue(current[field], deletedKey)) updates[field] = null;
    }
    for (const field of RELATION_FIELDS) {
      if (!Object.hasOwn(current, field) || !Array.isArray(current[field])) continue;
      const next = removeIssueReferences(current[field], deletedKey);
      if (next.length !== current[field].length) updates[field] = next;
    }
    if (Object.keys(updates).length) changes.push({ path: candidate.relativePath, bytes: patchRecord(candidate, updates) });
  }
  return changes;
}

function operationOptions(request, actor, { expected, prepare, kind, commit } = {}) {
  return {
    kind,
    id: request.operationId,
    fingerprint: JSON.stringify({ action: request.action, project: request.project, key: request.key, data: request.data, revisions: expected, actor: actor.id }),
    expected,
    prepare,
    ...(typeof commit === 'boolean' ? { commit } : {}),
  };
}

async function commentCreate(index, request, actor, context) {
  const current = targetForRequest(index, request);
  const expected = revisionFor(current, request.revisions);
  const result = await executeOperation(index.root, operationOptions(request, actor, {
    kind: 'comment.create',
    expected,
    commit: context.commit,
    prepare: async fresh => {
      const issue = targetForRequest(fresh, request);
      const { field, rows } = localRowsForIssue(issue);
      const createdAt = new Date().toISOString();
      const comment = {
        id: `vault-comment-${request.operationId}`,
        body: request.data.body,
        author: publicActor(actor),
        createdAt,
        updatedAt: createdAt,
        origin: 'local',
      };
      const bytes = patchRecord(issue, { [field]: [...rows, comment] });
      return { changes: [{ path: issue.relativePath, bytes }], result: { key: issueKey(issue), id: comment.id, created: true, comment: publicLocalComment(comment, actor) } };
    },
  }));
  return result;
}

async function commentDelete(index, request, actor, context) {
  const current = targetForRequest(index, request);
  const expected = revisionFor(current, request.revisions);
  const commentId = request.data.commentId;
  const local = localCommentById(current, commentId);
  if (!local) {
    const imported = issueComments(index, current, actor).find(comment => comment.id === commentId && comment.origin === 'imported');
    if (imported) fail('PERMISSION', 'Imported comments cannot be deleted');
    // Keep a missing comment retryable: executeOperation returns the prior
    // journal before checking expected revisions when this is an idempotent
    // retry. A fresh request with the current revision reaches this branch in
    // the prepare callback below and is rejected as NOT_FOUND.
  } else if (!sameActor(commentAuthor(local), actor)) {
    fail('PERMISSION', 'Only the comment author can delete this comment');
  }
  return executeOperation(index.root, operationOptions(request, actor, {
    kind: 'comment.delete',
    expected,
    commit: context.commit,
    prepare: async fresh => {
      const issue = targetForRequest(fresh, request);
      const { field, rows } = localRowsForIssue(issue);
      const found = rows.find(row => row && typeof row === 'object' && text(row.id ?? row.commentId) === commentId && row.origin !== 'imported' && row.source !== 'imported');
      if (!found) fail('NOT_FOUND', `Comment ${commentId} was not found on ${issueKey(issue)}`);
      if (!sameActor(commentAuthor(found), actor)) fail('PERMISSION', 'Only the comment author can delete this comment');
      const bytes = patchRecord(issue, { [field]: rows.filter(row => row !== found) });
      return { changes: [{ path: issue.relativePath, bytes }], result: { key: issueKey(issue), commentId, deleted: true } };
    },
  }));
}

async function issueDelete(index, request, actor, context) {
  const current = targetForRequest(index, request);
  const relative = current?.relativePath ?? pathFromRequest(index, request);
  if (!relative) fail('STALE', 'An opened revision is required to delete an issue');
  if (!Object.hasOwn(request.revisions, relative)) {
    const alias = Object.keys(request.revisions).find(key => key.toLowerCase() === relative.toLowerCase());
    if (alias === undefined) fail('STALE', `An opened revision is required for ${relative}`);
  }
  const expected = { [relative]: request.revisions[relative] ?? request.revisions[Object.keys(request.revisions).find(key => key.toLowerCase() === relative.toLowerCase())] };
  return executeOperation(index.root, operationOptions(request, actor, {
    kind: 'issue.delete',
    expected,
    commit: context.commit,
    prepare: async fresh => {
      const issue = targetForRequest(fresh, request);
      if (!issue) fail('NOT_FOUND', `Issue ${request.key} was not found`);
      const changes = [
        { path: issue.relativePath, bytes: null },
        ...cleanupReferences(fresh, issueKey(issue), issue),
      ];
      return { changes, result: { key: issueKey(issue), deleted: true } };
    },
  }));
}

/** Dispatch one allowlisted issue interaction through the shared operation engine. */
export async function dispatchIssueInteraction(index, input, context = {}) {
  const request = normalizeRequest(input);
  validateData(request);
  const actor = actorValue(context);
  if (!index?.root) fail('INVALID', 'A loaded Vault index is required');
  if (index?.vaultConfig?.writes?.enabled !== true) fail('READ_ONLY', 'Vault writes are disabled; configure writes.enabled explicitly');
  if (!request.key) fail('INVALID', 'An issue key is required');
  if (request.action === 'comment.create') return commentCreate(index, request, actor, context);
  if (request.action === 'comment.delete') return commentDelete(index, request, actor, context);
  return issueDelete(index, request, actor, context);
}

export default { ISSUE_INTERACTION_ACTIONS, dispatchIssueInteraction, issueComments };
