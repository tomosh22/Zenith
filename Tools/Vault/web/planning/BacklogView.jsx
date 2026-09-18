import React from 'react';
import {
  array,
  dateInputValue,
  formatDate,
  issueAssignee,
  issueKey,
  issuePriority,
  issueSprint,
  issueStatus,
  issueStatusCategory,
  issueTitle,
  positionOf,
  projectKeyOf,
  sprintId,
  sprintName,
  sprintStatus,
  text,
  usePlanningMutation,
} from './helpers.js';

function slug(value) {
  return text(value, 'item').toLowerCase().replace(/[^a-z0-9]+/g, '-');
}

function sprintPosition(sprint, index) {
  return positionOf(sprint?.position ?? sprint?.metadata?.position, index);
}

function sprintRevision(sprint) {
  if (!sprint?.path || !sprint?.hash) return {};
  return { [sprint.path]: { hash: sprint.hash, size: sprint.size, mtimeMs: sprint.mtimeMs } };
}

function issueRevision(issue) {
  if (!issue?.path || !issue?.hash) return {};
  return { [issue.path]: { hash: issue.hash, size: issue.size, mtimeMs: issue.mtimeMs } };
}

function matchesSprint(issue, sprint) {
  const value = issueSprint(issue);
  return value && [sprintId(sprint), sprintName(sprint), sprint?.slug].map(item => text(item)).includes(value);
}

function sprintIssues(tickets, sprint) {
  return tickets.filter(ticket => matchesSprint(ticket, sprint));
}

function displaySprintName(sprint) {
  return `${sprintName(sprint)}${sprintStatus(sprint) === 'ACTIVE' ? ' (current)' : ''}`;
}

function doneIssue(issue, statuses) {
  return issueStatusCategory(issue, statuses) === 'DONE' || issueStatus(issue).toUpperCase() === 'DONE';
}

function IssueRow({ issue, sprints, readOnly, pending, onOpenIssue, onMove }) {
  const currentRef = issueSprint(issue);
  const currentSprint = sprints.find(sprint => matchesSprint(issue, sprint));
  const current = currentSprint ? sprintId(currentSprint) : (currentRef ? '__unknown__' : '__backlog');
  return <div className="planning-backlog-row" data-testid={`planning-backlog-issue-${slug(issueKey(issue))}`}>
    <button type="button" className="planning-issue-key" onClick={() => onOpenIssue?.(issueKey(issue))}>{issueKey(issue)}</button>
    <button type="button" className="planning-backlog-title" onClick={() => onOpenIssue?.(issueKey(issue))}>{issueTitle(issue)}</button>
    <span className="planning-backlog-detail">{issuePriority(issue)}</span>
    <span className="planning-backlog-detail">{issueAssignee(issue) || 'Unassigned'}</span>
    <span className="planning-backlog-detail">{issueStatus(issue) || 'No status'}</span>
    {!readOnly && <select aria-label={`Move ${issueKey(issue)}`} value={current} disabled={pending} onChange={event => onMove(issue, event.target.value)}>
      <option value="__backlog">Backlog</option>
      {current === '__unknown__' && <option value="__unknown__">{currentRef}</option>}
      {sprints.filter(sprint => sprintStatus(sprint) !== 'COMPLETED').map(sprint => <option key={sprintId(sprint)} value={sprintId(sprint)}>{displaySprintName(sprint)}</option>)}
    </select>}
  </div>;
}

function SprintEditor({ sprint, values, setValues, pending, onCancel, onSubmit }) {
  return <div className="planning-inline-editor" data-testid="planning-sprint-editor">
    <label>Name <input aria-label="Sprint name" value={values.name} onChange={event => setValues(current => ({ ...current, name: event.target.value }))} /></label>
    <label>Goal <input aria-label="Sprint goal" value={values.goal} onChange={event => setValues(current => ({ ...current, goal: event.target.value }))} /></label>
    <label>Start date <input aria-label="Sprint start date" type="date" value={values.startDate} onChange={event => setValues(current => ({ ...current, startDate: event.target.value }))} /></label>
    <label>End date <input aria-label="Sprint end date" type="date" value={values.endDate} onChange={event => setValues(current => ({ ...current, endDate: event.target.value }))} /></label>
    <div className="planning-inline-actions"><button type="button" className="planning-primary" disabled={pending || !text(values.name)} onClick={() => onSubmit(sprint)}>{pending ? 'Saving…' : 'Save sprint'}</button><button type="button" onClick={onCancel}>Cancel</button></div>
  </div>;
}

function Confirmation({ title, children, confirmLabel, pending, onCancel, onConfirm }) {
  return <div className="planning-confirm-backdrop"><div className="planning-confirm" role="dialog" aria-modal="true" aria-labelledby="planning-confirm-title">
    <h3 id="planning-confirm-title">{title}</h3><div className="planning-confirm-copy">{children}</div>
    <div className="planning-inline-actions"><button type="button" onClick={onCancel}>Cancel</button><button type="button" className="planning-danger" disabled={pending} onClick={onConfirm}>{pending ? 'Working…' : confirmLabel}</button></div>
  </div></div>;
}

export function BacklogView({ project, readOnly = false, api, onChanged, onOpenIssue }) {
  const projectKey = projectKeyOf(project);
  const tickets = array(project?.tickets).filter(ticket => !ticket?.project || projectKeyOf(ticket.project) === projectKey || issueKey(ticket).toUpperCase().startsWith(`${projectKey}-`));
  const statuses = array(project?.statuses);
  const sprints = React.useMemo(() => array(project?.sprints).map((sprint, index) => ({ ...sprint, _position: sprintPosition(sprint, index) })).sort((left, right) => left._position - right._position || sprintName(left).localeCompare(sprintName(right))), [project?.sprints]);
  const [search, setSearch] = React.useState('');
  const [collapsed, setCollapsed] = React.useState(() => new Set());
  const [newSprint, setNewSprint] = React.useState({ name: '', goal: '' });
  const [editing, setEditing] = React.useState(null);
  const [editorValues, setEditorValues] = React.useState({ name: '', goal: '', startDate: '', endDate: '' });
  const [confirm, setConfirm] = React.useState(null);
  const mutation = usePlanningMutation(api, onChanged);

  const visibleTickets = React.useMemo(() => {
    const needle = text(search).toLocaleLowerCase();
    return tickets.filter(issue => !needle || `${issueKey(issue)} ${issueTitle(issue)} ${issueStatus(issue)} ${issuePriority(issue)}`.toLocaleLowerCase().includes(needle));
  }, [tickets, search]);
  const activeSprint = sprints.find(sprint => sprintStatus(sprint) === 'ACTIVE');
  const backlog = visibleTickets.filter(issue => !issueSprint(issue));

  const submitNewSprint = async () => {
    const name = text(newSprint.name, `Sprint ${sprints.length + 1}`);
    try {
      await mutation.execute({ kind: 'command', args: ['sprint', 'create', name, '--project', projectKey, ...(text(newSprint.goal) ? ['--goal', text(newSprint.goal)] : [])] });
      setNewSprint({ name: '', goal: '' });
    } catch { /* keep the draft visible for correction/retry */ }
  };

  const openEditor = sprint => {
    setEditing(sprint);
    setEditorValues({ name: sprintName(sprint), goal: text(sprint?.goal), startDate: dateInputValue(sprint?.startDate ?? sprint?.start_date), endDate: dateInputValue(sprint?.endDate ?? sprint?.end_date) });
  };

  const submitEdit = async sprint => {
    try {
      await mutation.execute({ kind: 'action', action: 'sprint.update', project: projectKey, data: { id: sprintId(sprint), name: text(editorValues.name), goal: text(editorValues.goal) || null, startDate: editorValues.startDate || null, endDate: editorValues.endDate || null }, options: { revisions: sprintRevision(sprint) } });
      setEditing(null);
    } catch { /* keep the editor open so a failed draft can be corrected */ }
  };

  const start = sprint => mutation.execute({ kind: 'command', args: ['sprint', 'start', sprintName(sprint), '--project', projectKey], options: { revisions: sprintRevision(sprint) } }).catch(() => {});
  const complete = (sprint, destinationSprintId = null) => {
    const incomplete = sprintIssues(tickets, sprint).filter(issue => !doneIssue(issue, statuses));
    const destination = sprints.find(item => sprintId(item) === destinationSprintId) ?? null;
    const revisions = { ...sprintRevision(sprint) };
    for (const issue of incomplete) Object.assign(revisions, issueRevision(issue));
    if (destination) Object.assign(revisions, sprintRevision(destination));
    return mutation.execute({
      kind: 'action',
      action: 'sprint.complete',
      project: projectKey,
      data: { id: sprintId(sprint), destinationSprintId },
      options: { revisions },
    });
  };
  const beginComplete = sprint => {
    const incomplete = sprintIssues(tickets, sprint).filter(issue => !doneIssue(issue, statuses));
    if (!incomplete.length) {
      complete(sprint, null).catch(() => {});
      return;
    }
    const destinations = sprints.filter(item => sprintStatus(item) === 'PLANNED' && sprintId(item) !== sprintId(sprint));
    setConfirm({ type: 'complete', sprint, incomplete, destinations, destinationSprintId: null });
  };
  const remove = sprint => mutation.execute({ kind: 'action', action: 'sprint.delete', project: projectKey, data: { id: sprintId(sprint) }, options: { revisions: sprintRevision(sprint) } });

  const move = async (issue, target) => {
    try {
      const current = sprints.find(sprint => matchesSprint(issue, sprint));
      if (target === '__backlog') {
        if (!current) return;
        await mutation.execute({ kind: 'command', args: ['sprint', 'remove', sprintName(current), issueKey(issue), '--project', projectKey], options: { revisions: issueRevision(issue) } });
        return;
      }
      const destination = sprints.find(sprint => sprintId(sprint) === target);
      if (!destination || current && sprintId(current) === target) return;
      // `sprint add` replaces the ticket's existing membership atomically.
      // Sending a remove followed by an add could leave the ticket in the
      // backlog if the second request failed, and both commands require the
      // opened ticket revision under the browser protocol.
      await mutation.execute({ kind: 'command', args: ['sprint', 'add', sprintName(destination), issueKey(issue), '--project', projectKey], options: { revisions: issueRevision(issue) } });
    } catch { /* error is rendered in the toolbar */ }
  };

  const reorder = async (sprint, direction) => {
    const index = sprints.findIndex(item => sprintId(item) === sprintId(sprint));
    const target = index + direction;
    if (index < 0 || target < 0 || target >= sprints.length) return;
    const order = sprints.map(item => sprintId(item));
    [order[index], order[target]] = [order[target], order[index]];
    try { await mutation.execute({ kind: 'action', action: 'sprint.reorder', project: projectKey, data: { order } }); } catch { /* error is rendered in the toolbar */ }
  };

  const renderRows = sprint => {
    const rows = visibleTickets.filter(issue => matchesSprint(issue, sprint));
    return rows.length ? rows.map(issue => <IssueRow key={issueKey(issue)} issue={issue} sprints={sprints} readOnly={readOnly} pending={mutation.pending} onOpenIssue={onOpenIssue} onMove={move} />) : <p className="planning-empty">No issues in this sprint.</p>;
  };

  return <section className="planning-surface planning-backlog" data-testid="planning-backlog">
    <header className="planning-surface-header"><div><p className="planning-eyebrow">Planning</p><h2>Backlog</h2><p className="planning-subtitle">{visibleTickets.length} of {tickets.length} project issues</p></div>{!readOnly && <div className="planning-create-row"><input aria-label="New sprint name" placeholder={`Sprint ${sprints.length + 1}`} value={newSprint.name} onChange={event => setNewSprint(current => ({ ...current, name: event.target.value }))} /><input aria-label="New sprint goal" placeholder="Sprint goal (optional)" value={newSprint.goal} onChange={event => setNewSprint(current => ({ ...current, goal: event.target.value }))} /><button type="button" className="planning-primary" disabled={mutation.pending} onClick={submitNewSprint}>Create sprint</button></div>}</header>
    <div className="planning-toolbar"><label>Search <input aria-label="Backlog search" value={search} onChange={event => setSearch(event.target.value)} placeholder="Key, title, status" /></label>{mutation.error && <span role="alert" className="planning-error">{mutation.error}</span>}</div>
    {sprints.map((sprint, index) => {
      const open = !collapsed.has(sprintId(sprint));
      const issues = sprintIssues(tickets, sprint);
      const incomplete = issues.filter(issue => !doneIssue(issue, statuses));
      return <section className="planning-sprint" key={sprintId(sprint)} data-testid={`planning-sprint-${slug(sprintId(sprint))}`} data-sprint-status={sprintStatus(sprint)}>
        <header className="planning-sprint-header"><button type="button" aria-expanded={open} aria-label={`Toggle ${sprintName(sprint)}`} onClick={() => setCollapsed(current => { const next = new Set(current); if (next.has(sprintId(sprint))) next.delete(sprintId(sprint)); else next.add(sprintId(sprint)); return next; })}><span aria-hidden="true">{open ? '▾' : '▸'}</span><strong>{displaySprintName(sprint)}</strong><span>{issues.length} issues</span>{sprint?.goal && <em>{sprint.goal}</em>}{(sprint?.startDate ?? sprint?.start_date) && (sprint?.endDate ?? sprint?.end_date) && <small>{formatDate(sprint.startDate ?? sprint.start_date)} – {formatDate(sprint.endDate ?? sprint.end_date)}</small>}</button><div className="planning-sprint-actions">{!readOnly && <><>{sprintStatus(sprint) === 'PLANNED' && <button type="button" disabled={mutation.pending || Boolean(activeSprint)} onClick={() => start(sprint)}>Start sprint</button>}</>{sprintStatus(sprint) === 'ACTIVE' && <button type="button" disabled={mutation.pending} onClick={() => beginComplete(sprint)}>Complete sprint</button>}<button type="button" disabled={mutation.pending || index === 0} aria-label={`Move ${sprintName(sprint)} earlier`} onClick={() => reorder(sprint, -1)}>↑</button><button type="button" disabled={mutation.pending || index === sprints.length - 1} aria-label={`Move ${sprintName(sprint)} later`} onClick={() => reorder(sprint, 1)}>↓</button><button type="button" disabled={mutation.pending} onClick={() => openEditor(sprint)}>Edit</button>{['PLANNED', 'COMPLETED'].includes(sprintStatus(sprint)) && <button type="button" className="planning-danger-link" disabled={mutation.pending} onClick={() => setConfirm({ type: 'delete', sprint })}>Delete</button>}</>}</div></header>
        {open && <div className="planning-sprint-body">{editing && sprintId(editing) === sprintId(sprint) && <SprintEditor sprint={sprint} values={editorValues} setValues={setEditorValues} pending={mutation.pending} onCancel={() => setEditing(null)} onSubmit={submitEdit} />}{renderRows(sprint)}</div>}
      </section>;
    })}
    <section className="planning-sprint planning-backlog-section" data-testid="planning-backlog-section"><header className="planning-sprint-header"><button type="button" aria-expanded={!collapsed.has('__backlog')} onClick={() => setCollapsed(current => { const next = new Set(current); if (next.has('__backlog')) next.delete('__backlog'); else next.add('__backlog'); return next; })}><span aria-hidden="true">{collapsed.has('__backlog') ? '▸' : '▾'}</span><strong>Backlog</strong><span>{backlog.length} issues</span></button></header>{!collapsed.has('__backlog') && <div className="planning-sprint-body">{backlog.length ? backlog.map(issue => <IssueRow key={issueKey(issue)} issue={issue} sprints={sprints} readOnly={readOnly} pending={mutation.pending} onOpenIssue={onOpenIssue} onMove={move} />) : <p className="planning-empty">No issues in the backlog.</p>}</div>}</section>
    {editing && !sprints.some(sprint => sprintId(sprint) === sprintId(editing)) && <SprintEditor sprint={editing} values={editorValues} setValues={setEditorValues} pending={mutation.pending} onCancel={() => setEditing(null)} onSubmit={submitEdit} />}
    {confirm?.type === 'complete' && <Confirmation title={`Complete ${sprintName(confirm.sprint)}?`} confirmLabel="Complete sprint" pending={mutation.pending} onCancel={() => setConfirm(null)} onConfirm={async () => { try { await complete(confirm.sprint, confirm.destinationSprintId); setConfirm(null); } catch { /* keep the confirmation open after a refused completion */ } }}><p>{confirm.incomplete.length} {confirm.incomplete.length === 1 ? "issue isn't" : "issues aren't"} Done. Choose where to move them.</p><ul data-testid="incomplete-issues-list">{confirm.incomplete.map(issue => <li key={issueKey(issue)}>{issueKey(issue)} — {issueTitle(issue)}</li>)}</ul><fieldset><legend>Move unfinished issues to</legend><label><input type="radio" name="sprint-completion-destination" aria-label="Backlog" checked={confirm.destinationSprintId === null} onChange={() => setConfirm(current => ({ ...current, destinationSprintId: null }))} /> Backlog</label>{confirm.destinations.map(destination => <label key={sprintId(destination)}><input type="radio" name="sprint-completion-destination" aria-label={sprintName(destination)} checked={confirm.destinationSprintId === sprintId(destination)} onChange={() => setConfirm(current => ({ ...current, destinationSprintId: sprintId(destination) }))} /> {sprintName(destination)}</label>)}</fieldset></Confirmation>}
    {confirm?.type === 'delete' && <Confirmation title={`Delete ${sprintName(confirm.sprint)}?`} confirmLabel="Delete sprint" pending={mutation.pending} onCancel={() => setConfirm(null)} onConfirm={async () => { try { await remove(confirm.sprint); setConfirm(null); } catch { /* keep the confirmation open after a refused deletion */ } }}>Issues are returned to the backlog. This cannot be undone.</Confirmation>}
  </section>;
}

export const ProjectBacklog = BacklogView;
