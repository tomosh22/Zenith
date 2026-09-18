import React, { useCallback, useEffect, useRef, useState } from 'react';

import {
  buildActionRequest,
  callApi,
  configuredIssueAction,
  issueKeyOf,
  makeOperationId,
  normalizeIssue,
  projectReadOnly,
  sendAction,
} from './model.js';
import { IssueEditForm } from './IssueForm.jsx';
import { CommentsSection, IssueFamily, IssueHistory, IssueLinks } from './IssueSections.jsx';

function messageOf(cause) {
  return cause?.message ?? String(cause);
}

// A 409 STALE means the file moved on since the panel opened. The write is
// NOT replayed automatically — a dirty draft must never silently overwrite a
// newer change — so reload the detail (which rebuilds the revision map for
// the next write) and tell the user their draft survived.
const STALE_NOTICE = 'This issue changed and was reloaded — your draft is preserved, try submitting again.';

function dateText(value) {
  if (!value) return '';
  const date = new Date(value);
  return Number.isNaN(date.getTime()) ? String(value) : date.toLocaleString();
}

function metadata(issue) {
  return issue?.metadata ?? issue?.fields ?? {};
}

/**
 * Issue detail surface.  The shell owns navigation and supplies the already
 * loaded project catalog; the panel owns the issue request and never reads a
 * local file directly.  Keeping this boundary explicit makes a selected
 * issue safe to replace while a previous request or save is still pending.
 */
export function IssuePanel({ issueKey, project, api, onChanged, onOpenIssue, onClose, readOnly }) {
  const [detail, setDetail] = useState(null);
  const [loading, setLoading] = useState(Boolean(issueKey));
  const [error, setError] = useState('');
  const [staleNotice, setStaleNotice] = useState('');
  const [deleting, setDeleting] = useState(false);
  const [pendingWrites, setPendingWrites] = useState(0);
  const deleteOperation = useRef(null);
  const requestId = useRef(0);

  const load = useCallback(async () => {
    if (!issueKey) {
      setDetail(null);
      setLoading(false);
      return null;
    }
    const id = ++requestId.current;
    setLoading(true);
    setError('');
    try {
      const response = await callApi(api, `/api/issues/${encodeURIComponent(issueKey)}`);
      if (id !== requestId.current) return null;
      const issue = normalizeIssue(response?.issue ?? response, issueKey);
      const next = { ...response, issue };
      setDetail(next);
      return next;
    } catch (cause) {
      if (id === requestId.current) {
        setDetail(null);
        setError(messageOf(cause));
      }
      return null;
    } finally {
      if (id === requestId.current) setLoading(false);
    }
  }, [api, issueKey]);

  useEffect(() => {
    setStaleNotice('');
    load();
    return () => { requestId.current += 1; };
  }, [load]);

  // WorkspaceApp refreshes the project catalog when a watcher notification
  // arrives. Re-read the selected issue when its revision map changes; the
  // form's saved fingerprint keeps a dirty local draft in place while this
  // request completes, so an external write becomes a normal stale-revision
  // response instead of silently replacing the user's text. The fingerprint
  // is only marked seen once a reload can actually run: a change landing
  // while the opening fetch is still in flight (detail still null) stays
  // pending and is consumed when the detail arrives, rather than being
  // swallowed here and permanently suppressing the reload.
  const projectRevisionFingerprint = JSON.stringify(project?.revisions ?? {});
  const previousProjectRevision = useRef(projectRevisionFingerprint);
  useEffect(() => {
    if (previousProjectRevision.current === projectRevisionFingerprint) return;
    if (!detail) return;
    previousProjectRevision.current = projectRevisionFingerprint;
    load();
  }, [detail, load, projectRevisionFingerprint]);

  const afterChanged = useCallback(async response => {
    setStaleNotice('');
    try { await onChanged?.(response); } catch (cause) { setError(messageOf(cause)); }
    await load();
  }, [load, onChanged]);

  // Children submit through this wrapper so a 409 STALE from any mutation
  // refreshes the panel's revision map before the user can try again. The
  // error still propagates to the section that sent it, keeping its draft.
  // In-flight POSTs are counted so sibling sections can hold their own
  // buttons while a write (and the refresh it triggers) is still moving —
  // a link submitted beside an in-flight comment write would otherwise be
  // built from pre-write revisions, and its 423 retry would then land as a
  // 409 STALE against the file the write just moved.
  const panelApi = useCallback(async (path, options = {}) => {
    if (options?.method === 'POST') setPendingWrites(value => value + 1);
    try {
      return await api(path, options);
    } catch (cause) {
      if (cause?.status === 409 && options?.method === 'POST') {
        setStaleNotice(STALE_NOTICE);
        await load();
      }
      throw cause;
    } finally {
      if (options?.method === 'POST') setPendingWrites(value => value - 1);
    }
  }, [api, load]);

  async function removeIssue(revisions) {
    const descriptor = configuredIssueAction(project, 'delete');
    if (!descriptor || deleting || projectReadOnly(project, readOnly)) return;
    setDeleting(true);
    setError('');
    try {
      const signature = `${issueKey}:${JSON.stringify(revisions ?? {})}`;
      let request = deleteOperation.current?.signature === signature ? deleteOperation.current.request : null;
      if (!request) {
        request = buildActionRequest(descriptor.action, project?.key ?? issueKey.split('-')[0], issueKey, {}, revisions ?? {}, makeOperationId('issue-delete'));
        deleteOperation.current = { signature, request };
      }
      const response = await sendAction(api, request);
      deleteOperation.current = null;
      await onChanged?.(response);
      onClose?.();
    } catch (cause) {
      if (cause?.status === 409) {
        setStaleNotice(STALE_NOTICE);
        await load();
      }
      setError(messageOf(cause));
    } finally {
      setDeleting(false);
    }
  }

  if (!issueKey) return <div className="vault-issue-panel vault-issue-empty">Select an issue.</div>;
  if (loading && !detail) return <div className="vault-issue-panel" role="status">Loading issue {issueKey}…</div>;
  if (error && !detail) return <div className="vault-issue-panel" role="alert">Could not load {issueKey}: {error}</div>;
  if (!detail) return <div className="vault-issue-panel" role="alert">Issue {issueKey} was not found.</div>;

  const issue = detail.issue;
  const projectKey = project?.key ?? issue.project ?? issueKey.split('-')[0];
  const locked = projectReadOnly(project, readOnly);
  const revisions = detail.revisions ?? (detail.revision && issue.path ? { [issue.path]: detail.revision } : {});
  const busy = loading || pendingWrites > 0;

  return (
    <article className="vault-issue-panel" data-testid="issue-panel" aria-labelledby="issue-panel-heading">
      <header className="vault-issue-panel-header">
        <div>
          <p className="vault-issue-key">{issueKeyOf(issue, issueKey)}</p>
          <h1 id="issue-panel-heading" className="vault-issue-heading">{issue.title || issueKey}</h1>
          <p className="vault-issue-updated">Updated {dateText(issue.updatedAt ?? metadata(issue).updatedAt)}</p>
        </div>
        <div className="vault-issue-panel-actions">
          {configuredIssueAction(project, 'delete') && !locked ? <button type="button" className="vault-issue-button vault-issue-button-danger" onClick={() => removeIssue(revisions)} disabled={deleting}>{deleting ? 'Deleting…' : 'Delete issue'}</button> : null}
          {onClose ? <button type="button" className="vault-issue-button" onClick={onClose}>Close</button> : null}
          {loading ? <span className="vault-issue-saving" role="status">Refreshing…</span> : null}
        </div>
      </header>

      <IssueEditForm
        issue={{ ...issue, revisions }}
        project={project}
        api={panelApi}
        readOnly={locked}
        onSaved={afterChanged}
        onOpenIssue={onOpenIssue}
      />

      <div className="vault-issue-panel-lower">
        <IssueFamily family={detail.family ?? { parent: null, children: [] }} onOpenIssue={onOpenIssue} />
        <IssueLinks
          issueKey={issueKey}
          links={detail.links ?? { outgoing: [], incoming: [] }}
          project={project}
          api={panelApi}
          revisions={revisions}
          readOnly={locked}
          busy={busy}
          onChanged={afterChanged}
          onOpenIssue={onOpenIssue}
        />
        <CommentsSection
          issueKey={issueKey}
          comments={detail.comments ?? []}
          api={panelApi}
          project={project}
          revisions={revisions}
          readOnly={locked}
          busy={busy}
          onChanged={afterChanged}
          onOpenIssue={onOpenIssue}
        />
        <IssueHistory history={detail.history ?? []} />
      </div>
      {error ? <p className="vault-issue-error" role="alert">{error}</p> : null}
      {staleNotice ? <p className="vault-issue-error" role="alert" data-testid="stale-notice">{staleNotice}</p> : null}
    </article>
  );
}

// A descriptive alias helps callers that prefer the old Jira component name.
export const IssueDetail = IssuePanel;
