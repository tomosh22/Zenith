import React, { useMemo, useState } from 'react';

import {
  buildActionRequest,
  buildCommentCommand,
  buildLinkCommand,
  buildUnlinkCommand,
  configuredIssueAction,
  linkLabel,
  linkTypeFor,
  normalizeIssue,
  relationSource,
  relationTarget,
  makeOperationId,
  sendAction,
  sendCommand,
} from './model.js';
import { MarkdownPreview } from './MarkdownPreview.jsx';

function rows(value) {
  return Array.isArray(value) ? value : [];
}

function text(value, fallback = '') {
  return value === undefined || value === null ? fallback : String(value);
}

function issueKey(row) {
  return text(row?.key ?? row?.issueKey ?? row?.targetKey ?? row?.sourceKey ?? row?.number, 'Unknown issue');
}

function dateText(value) {
  if (!value) return '';
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? text(value) : date.toLocaleString();
}

function errorMessage(cause) {
  return cause?.message ?? String(cause);
}

export function CommentsSection({ issueKey, comments = [], api, project, revisions = {}, readOnly = false, busy = false, onChanged, onOpenIssue }) {
  const [body, setBody] = useState('');
  const [saving, setSaving] = useState(false);
  const [deletingComment, setDeletingComment] = useState(null);
  const [error, setError] = useState('');
  const pendingOperation = React.useRef(null);
  const pendingDeletes = React.useRef(new Map());

  const deleteAction = configuredIssueAction(project, 'deleteComment');

  function commentCanDelete(comment) {
    // The action is intentionally advertised per project.  A server may also
    // annotate a comment as non-deletable after applying its actor policy;
    // honour that decision while keeping old/imported comments usable when
    // the backend does not include an optional capability flag.
    if (!deleteAction || !comment?.id || comment?.canDelete === false || comment?.deletable === false) return false;
    const viewer = project?.viewer ?? project?.currentUser ?? project?.actor ?? null;
    const author = comment?.author ?? {};
    const viewerId = viewer?.id ?? viewer?.userId;
    const viewerEmail = viewer?.email ?? viewer?.username;
    // When the API exposes the local actor, apply the same own-comment rule
    // as Jira. If it does not, the allowlisted server action remains the
    // authority and can enforce ownership from its session actor.
    if (viewerId !== undefined && viewerId !== null && author?.id !== undefined && author?.id !== null) {
      return String(viewerId) === String(author.id);
    }
    if (viewerEmail && (author?.email || author?.username)) {
      return String(viewerEmail).toLowerCase() === String(author.email ?? author.username).toLowerCase();
    }
    return true;
  }

  async function submit(event) {
    event?.preventDefault?.();
    if (readOnly || busy || saving || !body.trim()) return;
    setSaving(true);
    setError('');
    try {
      const signature = `comment:${issueKey}:${body}:${JSON.stringify(revisions ?? {})}`;
      const createAction = configuredIssueAction(project, 'createComment');
      let pending = pendingOperation.current?.signature === signature ? pendingOperation.current : null;
      if (!pending) {
        if (createAction) {
          pending = {
            signature,
            kind: 'action',
            request: buildActionRequest(
              createAction.action,
              project?.key ?? project?.project ?? issueKey.split('-')[0],
              issueKey,
              { body },
              revisions,
              makeOperationId('issue-comment'),
            ),
          };
        } else {
          pending = {
            signature,
            kind: 'command',
            request: { ...buildCommentCommand(issueKey, body, revisions), operationId: makeOperationId('issue-comment') },
          };
        }
        pendingOperation.current = pending;
      }
      const response = pending.kind === 'action'
        ? await sendAction(api, pending.request)
        : await sendCommand(api, pending.request, 'issue-comment');
      setBody('');
      pendingOperation.current = null;
      await onChanged?.(response);
    } catch (cause) {
      setError(errorMessage(cause));
    } finally {
      setSaving(false);
    }
  }

  async function removeComment(comment) {
    if (readOnly || busy || saving || deletingComment || !commentCanDelete(comment)) return;
    const commentId = String(comment.id);
    const signature = `comment-delete:${issueKey}:${commentId}:${JSON.stringify(revisions ?? {})}`;
    setDeletingComment(commentId);
    setError('');
    try {
      let request = pendingDeletes.current.get(signature);
      if (!request) {
        request = buildActionRequest(
          deleteAction.action,
          project?.key ?? project?.project ?? issueKey.split('-')[0],
          issueKey,
          { commentId },
          revisions,
          makeOperationId('issue-comment-delete'),
        );
        pendingDeletes.current.set(signature, request);
      }
      const response = await sendAction(api, request);
      pendingDeletes.current.delete(signature);
      await onChanged?.(response);
    } catch (cause) {
      // Keep the request and its operation ID so a retry after an uncertain
      // response cannot create a second logical deletion.
      setError(errorMessage(cause));
    } finally {
      setDeletingComment(null);
    }
  }

  return (
    <section className="vault-issue-section" data-testid="issue-comments" aria-labelledby="issue-comments-heading">
      <h2 id="issue-comments-heading" className="vault-issue-section-heading">Comments</h2>
      {rows(comments).length ? (
        <ol className="vault-issue-comment-list">
          {rows(comments).map((comment, index) => (
            <li key={comment?.id ?? `comment-${index}`} className="vault-issue-comment" data-testid="comment">
              <div className="vault-issue-comment-meta">
                <strong>{text(comment?.author?.name ?? comment?.author?.email ?? comment?.authorName, 'Unknown author')}</strong>
                <time dateTime={comment?.createdAt ?? comment?.created_at ?? undefined}>{dateText(comment?.createdAt ?? comment?.created_at)}</time>
                {commentCanDelete(comment) ? (
                  <button
                    type="button"
                    className="vault-issue-button vault-issue-button-small vault-issue-button-danger"
                    aria-label="Delete comment"
                    onClick={() => removeComment(comment)}
                    disabled={Boolean(busy || saving || deletingComment)}
                  >
                    {deletingComment === String(comment.id) ? 'Deleting…' : 'Delete'}
                  </button>
                ) : null}
              </div>
              <div className="vault-issue-comment-body">
                <MarkdownPreview body={text(comment?.body ?? comment?.text)} references={comment?.references} onOpenIssue={onOpenIssue} label={`Comment ${index + 1}`} />
              </div>
            </li>
          ))}
        </ol>
      ) : <p className="vault-issue-empty">No comments yet.</p>}
      {!readOnly ? (
        <form className="vault-issue-comment-form" onSubmit={submit}>
          <textarea
            aria-label="New comment"
            className="vault-issue-control"
            placeholder="Leave a comment…"
            value={body}
            onChange={event => setBody(event.target.value)}
            rows={4}
            disabled={saving}
          />
          {error ? <p className="vault-issue-error" role="alert">{error}</p> : null}
          <button type="submit" className="vault-issue-button vault-issue-button-primary" disabled={busy || saving || !body.trim()}>{saving ? 'Posting…' : 'Post comment'}</button>
        </form>
      ) : <p className="vault-issue-readonly">Comments are read-only.</p>}
    </section>
  );
}

function normalizeLinkRows(value) {
  if (Array.isArray(value)) return { outgoing: value, incoming: [] };
  return { outgoing: rows(value?.outgoing), incoming: rows(value?.incoming) };
}

function addBlockingEdge(graph, source, target) {
  const from = String(source ?? '').trim().toUpperCase();
  const to = String(target ?? '').trim().toUpperCase();
  if (!from || !to) return;
  if (!graph.has(from)) graph.set(from, new Set());
  graph.get(from).add(to);
}

function hasPath(graph, start, wanted) {
  const queue = [start];
  const seen = new Set();
  while (queue.length) {
    const current = queue.shift();
    if (current === wanted) return true;
    if (seen.has(current)) continue;
    seen.add(current);
    for (const next of graph.get(current) ?? []) queue.push(next);
  }
  return false;
}

/**
 * Check the known project graph before asking the writer to add BLOCKS. The
 * server remains authoritative, but this gives the browser the same useful
 * failure as Jira when a complete project catalog is available.
 */
function wouldCreateBlockingCycle(project, links, currentKey, relation, targetKey) {
  if (relation !== 'blocks' && relation !== 'blocked-by') return false;
  const graph = new Map();
  for (const ticket of rows(project?.tickets)) {
    const issue = normalizeIssue(ticket);
    for (const row of issue.blocks) addBlockingEdge(graph, issue.key, row.key);
    for (const row of issue.links) {
      if (String(row.type ?? '').toUpperCase() === 'BLOCKS') addBlockingEdge(graph, issue.key, row.key);
    }
  }
  const normalized = normalizeLinkRows(links);
  for (const row of normalized.outgoing) {
    if (linkTypeFor(row?.type) === 'blocks') addBlockingEdge(graph, currentKey, relationTarget(row, 'outgoing'));
  }
  for (const row of normalized.incoming) {
    if (linkTypeFor(row?.type) === 'blocks') addBlockingEdge(graph, relationSource(row, 'incoming', currentKey), currentKey);
  }
  const source = relation === 'blocked-by' ? targetKey : currentKey;
  const target = relation === 'blocked-by' ? currentKey : targetKey;
  addBlockingEdge(graph, source, target);
  return hasPath(graph, String(target).trim().toUpperCase(), String(source).trim().toUpperCase());
}

function rowTitle(row) {
  return text(row?.title ?? row?.summary ?? row?.name, issueKey(row));
}

export function IssueLinks({ issueKey, links = {}, project, api, revisions = {}, readOnly = false, busy = false, onChanged, onOpenIssue }) {
  const normalized = normalizeLinkRows(links);
  const [adding, setAdding] = useState(false);
  const [relation, setRelation] = useState('blocks');
  const [target, setTarget] = useState('');
  const [reason, setReason] = useState('');
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  const pendingOperation = React.useRef(null);
  const reasonRequired = relation === 'blocks' || relation === 'blocked-by';

  const grouped = useMemo(() => [
    ...normalized.outgoing.map(row => ({ row, direction: 'outgoing' })),
    ...normalized.incoming.map(row => ({ row, direction: 'incoming' })),
  ], [normalized.outgoing, normalized.incoming]);

  async function addLink(event) {
    event?.preventDefault?.();
    if (readOnly || busy || saving || !target.trim() || (reasonRequired && !reason.trim())) return;
    if (wouldCreateBlockingCycle(project, links, issueKey, relation, target)) {
      setError('That would create a circular dependency.');
      return;
    }
    setSaving(true);
    setError('');
    try {
      const signature = `link:${issueKey}:${relation}:${target.trim()}:${reason.trim()}:${JSON.stringify(revisions ?? {})}`;
      let command = pendingOperation.current?.signature === signature ? pendingOperation.current.command : null;
      if (!command) {
        command = { ...buildLinkCommand(issueKey, relation, target, reason, revisions), operationId: makeOperationId('issue-link') };
        pendingOperation.current = { signature, command };
      }
      const response = await sendCommand(api, command, 'issue-link');
      setTarget('');
      setReason('');
      setAdding(false);
      pendingOperation.current = null;
      await onChanged?.(response);
    } catch (cause) {
      setError(errorMessage(cause));
    } finally {
      setSaving(false);
    }
  }

  async function removeLink(row, direction) {
    if (readOnly || busy || saving) return;
    const type = linkTypeFor(row?.type);
    const source = relationSource(row, direction, issueKey);
    const other = relationTarget(row, direction);
    if (!source || !other) {
      setError('This link has no complete source and target key.');
      return;
    }
    setSaving(true);
    setError('');
    try {
      const signature = `unlink:${source}:${type}:${other}:${JSON.stringify(revisions ?? {})}`;
      let command = pendingOperation.current?.signature === signature ? pendingOperation.current.command : null;
      if (!command) {
        command = { ...buildUnlinkCommand(source, type, other, revisions), operationId: makeOperationId('issue-unlink') };
        pendingOperation.current = { signature, command };
      }
      const response = await sendCommand(api, command, 'issue-unlink');
      pendingOperation.current = null;
      await onChanged?.(response);
    } catch (cause) {
      setError(errorMessage(cause));
    } finally {
      setSaving(false);
    }
  }

  return (
    <section className="vault-issue-section" data-testid="issue-links" aria-labelledby="issue-links-heading">
      <div className="vault-issue-section-header">
        <h2 id="issue-links-heading" className="vault-issue-section-heading">Linked issues</h2>
        {!readOnly && !adding ? <button type="button" className="vault-issue-button" onClick={() => setAdding(true)} disabled={busy}>Add link</button> : null}
      </div>
      {adding ? (
        <form className="vault-issue-link-form" onSubmit={addLink}>
          <label>
            Relationship
            <select aria-label="Link type" className="vault-issue-control" value={relation} onChange={event => setRelation(event.target.value)} disabled={saving}>
              <option value="blocks">blocks</option>
              <option value="blocked-by">is blocked by</option>
              <option value="relates-to">relates to</option>
              <option value="duplicates">duplicates</option>
              <option value="causes">causes</option>
            </select>
          </label>
          <label>
            Issue key
            <input aria-label="Issue key to link" className="vault-issue-control" value={target} onChange={event => setTarget(event.target.value)} placeholder="ZEN-12" disabled={saving} />
          </label>
          <label>
            Reason {reasonRequired ? '(required)' : '(optional)'}
            <input aria-label={reasonRequired ? 'Why this blocks (required)' : 'Reason (optional)'} className="vault-issue-control" value={reason} onChange={event => setReason(event.target.value)} placeholder={reasonRequired ? 'Why does this dependency exist?' : 'Optional context'} disabled={saving} />
          </label>
          {error ? <p className="vault-issue-error" role="alert">{error}</p> : null}
          <div className="vault-issue-form-actions">
            <button type="submit" className="vault-issue-button vault-issue-button-primary" disabled={busy || saving || !target.trim() || (reasonRequired && !reason.trim())}>Link issue</button>
            <button type="button" className="vault-issue-button" onClick={() => { setAdding(false); setTarget(''); setReason(''); setError(''); }} disabled={saving}>Cancel</button>
          </div>
        </form>
      ) : null}
      {grouped.length ? (
        <ul className="vault-issue-link-list">
          {grouped.map(({ row, direction }, index) => {
            const targetKey = relationTarget(row, direction);
            const label = linkLabel(row?.type, direction);
            return (
              <li key={row?.id ?? `${direction}-${targetKey}-${index}`} className="vault-issue-link-row">
                <span className="vault-issue-link-label">{label}</span>
                <button type="button" className="vault-issue-link-key" onClick={() => onOpenIssue?.(targetKey)} disabled={!targetKey}>{targetKey || 'Unknown issue'}</button>
                <span className="vault-issue-link-title">{rowTitle(row)}</span>
                {row?.reason ? <small className="vault-issue-link-reason">{row.reason}</small> : null}
                {!readOnly ? <button type="button" className="vault-issue-button vault-issue-button-small" onClick={() => removeLink(row, direction)} disabled={busy || saving}>Remove</button> : null}
              </li>
            );
          })}
        </ul>
      ) : <p className="vault-issue-empty">No linked issues.</p>}
    </section>
  );
}

function familyKey(row) {
  return text(row?.key ?? row?.issueKey ?? row?.number, 'Unknown issue');
}

export function IssueFamily({ family = {}, onOpenIssue }) {
  const parent = family?.parent ?? null;
  const children = rows(family?.children);
  if (!parent && !children.length) return null;
  const done = children.filter(row => String(row?.statusCategory ?? row?.status_category ?? '').toUpperCase() === 'DONE' || String(row?.status ?? '').toLowerCase() === 'done').length;
  const totalPoints = children.reduce((sum, row) => sum + (Number(row?.storyPoints ?? row?.story_points) || 0), 0);
  const donePoints = children.filter(row => String(row?.statusCategory ?? row?.status_category ?? '').toUpperCase() === 'DONE').reduce((sum, row) => sum + (Number(row?.storyPoints ?? row?.story_points) || 0), 0);
  return (
    <section className="vault-issue-section" data-testid="issue-family" aria-labelledby="issue-family-heading">
      <h2 id="issue-family-heading" className="vault-issue-section-heading">Issue hierarchy</h2>
      {parent ? <button type="button" className="vault-issue-family-parent" onClick={() => onOpenIssue?.(familyKey(parent))}>Parent: {familyKey(parent)} — {rowTitle(parent)}</button> : null}
      {children.length ? (
        <>
          <p className="vault-issue-family-summary">Child issues — {done}/{children.length} done{totalPoints ? ` · ${donePoints}/${totalPoints} points` : ''}</p>
          <ul className="vault-issue-family-list">
            {children.map((child, index) => <li key={child?.id ?? `${familyKey(child)}-${index}`}><button type="button" onClick={() => onOpenIssue?.(familyKey(child))}>{familyKey(child)}</button><span>{rowTitle(child)}</span><small>{text(child?.statusName ?? child?.status, '')}</small></li>)}
          </ul>
        </>
      ) : null}
    </section>
  );
}

const HISTORY_LABELS = {
  storyPoints: 'story points',
  fixVersions: 'fix version',
  statusCategory: 'status category',
};

export function IssueHistory({ history = [] }) {
  const list = rows(history);
  return (
    <section className="vault-issue-section" data-testid="issue-history" aria-labelledby="issue-history-heading">
      <h2 id="issue-history-heading" className="vault-issue-section-heading">History</h2>
      {list.length ? <ol className="vault-issue-history-list">{list.map((row, index) => {
        const field = HISTORY_LABELS[row?.field] ?? text(row?.field, 'field');
        const from = row?.fromValue ?? row?.from ?? null;
        const to = row?.toValue ?? row?.to ?? null;
        return <li key={row?.id ?? `history-${index}`}><strong>{text(row?.actorName ?? row?.actorEmail, 'Someone')}</strong> changed <b>{field}</b>{from !== null ? <> from <code>{text(from)}</code></> : null} to <code>{text(to, 'none')}</code><time>{dateText(row?.createdAt ?? row?.created_at)}</time></li>;
      })}</ol> : <p className="vault-issue-empty">No changes recorded yet.</p>}
    </section>
  );
}
