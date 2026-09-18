import React from 'react';
import {
  array,
  issueAssignee,
  issueKey,
  issueLabels,
  issuePriority,
  issueSprint,
  issueStatus,
  issueTitle,
  issueType,
  positionOf,
  projectKeyOf,
  sprintId,
  sprintName,
  statusName,
  submitCommand,
  text,
  unique,
  usePlanningMutation,
} from './helpers.js';
import { resolveDropStatus } from '../../shared/jira/board.js';

function slug(value) {
  return text(value, 'group').toLowerCase().replace(/[^a-z0-9]+/g, '-');
}

function statusCategory(status) {
  return text(status?.category).toUpperCase();
}

function boardStatusRows(project) {
  const statuses = array(project?.statuses).map((status, index) => ({
    id: text(status?.id, statusName(status) || `status-${index}`),
    name: statusName(status) || `Status ${index + 1}`,
    category: statusCategory(status),
    position: positionOf(status?.position, index),
  }));
  statuses.sort((left, right) => left.position - right.position || left.name.localeCompare(right.name));
  return statuses;
}

function boardColumns(project, statuses) {
  const configured = array(project?.board?.columns);
  if (!configured.length) return statuses.map(status => ({
    id: status.id,
    name: status.name,
    statusIds: [status.id],
    statusNames: [status.name],
    position: status.position,
    wipLimit: null,
  }));

  const byId = new Map(statuses.map(status => [status.id, status]));
  const byName = new Map(statuses.map(status => [status.name.toLowerCase(), status]));
  return configured.map((column, index) => {
    const references = array(column.statusIds ?? column.status_ids ?? (column.statusId ? [column.statusId] : column.statuses));
    const resolved = references.map(reference => {
      const value = text(reference && typeof reference === 'object' ? reference.id ?? reference.name : reference);
      return byId.get(value) ?? byName.get(value.toLowerCase());
    }).filter(Boolean);
    const fallback = text(column.name) && (byName.get(text(column.name).toLowerCase()) ?? null);
    const statusesForColumn = resolved.length ? resolved : (fallback ? [fallback] : []);
    return {
      id: text(column.id, `column-${index}`),
      name: text(column.name, statusesForColumn[0]?.name ?? `Column ${index + 1}`),
      statusIds: statusesForColumn.map(status => status.id),
      statusNames: statusesForColumn.map(status => status.name),
      position: positionOf(column.position, index),
      wipLimit: column.wipLimit ?? column.wip_limit ?? null,
    };
  }).sort((left, right) => left.position - right.position || left.name.localeCompare(right.name));
}

function filterValue(issue, field, sprints, users) {
  if (field === 'status') return issueStatus(issue);
  if (field === 'type') return issueType(issue);
  if (field === 'priority') return issuePriority(issue);
  if (field === 'assignee') return issueAssignee(issue) || '__unassigned';
  if (field === 'sprint') {
    const ref = issueSprint(issue);
    const sprint = array(sprints).find(item => sprintId(item) === ref || sprintName(item) === ref);
    return sprint ? sprintId(sprint) : (ref || '__backlog');
  }
  if (field === 'label') return issueLabels(issue);
  void users;
  return '';
}

function matchesFilter(issue, filters, boardFilter, sprints, users) {
  const merged = { ...(boardFilter && typeof boardFilter === 'object' ? boardFilter : {}), ...filters };
  const search = text(merged.search).toLocaleLowerCase();
  if (search && !`${issueKey(issue)} ${issueTitle(issue)} ${issueType(issue)} ${issueLabels(issue).join(' ')}`.toLocaleLowerCase().includes(search)) return false;
  for (const field of ['status', 'type', 'priority', 'assignee', 'sprint']) {
    if (merged[field] && filterValue(issue, field, sprints, users).toLowerCase() !== String(merged[field]).toLowerCase()) return false;
  }
  if (merged.label && !filterValue(issue, 'label', sprints, users).some(label => label.toLowerCase() === String(merged.label).toLowerCase())) return false;
  return true;
}

function orderTickets(tickets, order, users) {
  return [...tickets].sort((left, right) => {
    if (order === 'priority') {
      const rank = { CRITICAL: 0, HIGH: 1, MEDIUM: 2, LOW: 3, TRIVIAL: 4, UNSET: 5 };
      const byPriority = (rank[issuePriority(left)] ?? 9) - (rank[issuePriority(right)] ?? 9);
      if (byPriority) return byPriority;
    } else if (order === 'updated') {
      const byDate = Date.parse(text(right?.updatedAt ?? right?.metadata?.updatedAt ?? right?.metadata?.updated_at))
        - Date.parse(text(left?.updatedAt ?? left?.metadata?.updatedAt ?? left?.metadata?.updated_at));
      if (Number.isFinite(byDate) && byDate) return byDate;
    } else if (order === 'title') {
      const byTitle = issueTitle(left).localeCompare(issueTitle(right));
      if (byTitle) return byTitle;
    } else {
      const byPosition = positionOf(left?.position ?? left?.metadata?.position, 0)
        - positionOf(right?.position ?? right?.metadata?.position, 0);
      if (byPosition) return byPosition;
    }
    return issueKey(left).localeCompare(issueKey(right));
  });
}

function groupName(issue, grouping, sprints, users) {
  if (grouping === 'assignee') {
    const assignee = issueAssignee(issue);
    if (!assignee) return 'Unassigned';
    const user = array(users).find(item => text(item?.id) === assignee || text(item?.email) === assignee);
    return text(user?.name, assignee);
  }
  if (grouping === 'priority') return issuePriority(issue);
  if (grouping === 'sprint') {
    const ref = issueSprint(issue);
    const sprint = array(sprints).find(item => sprintId(item) === ref || sprintName(item) === ref);
    return sprint ? sprintName(sprint) : 'Backlog';
  }
  return issueStatus(issue) || 'No status';
}

function revisionsFor(ticket) {
  if (!ticket?.path || !ticket.hash) return {};
  return { [ticket.path]: { hash: ticket.hash, size: ticket.size, mtimeMs: ticket.mtimeMs } };
}

function IssueCard({ issue, statuses, sprints, users, readOnly, onOpenIssue, onStatus, pending, cardFields, draggable = false, onDragStart, onDragEnd }) {
  const key = issueKey(issue);
  const assignee = issueAssignee(issue);
  const user = array(users).find(item => text(item?.id) === assignee || text(item?.email) === assignee);
  const show = field => !array(cardFields).length || cardFields.includes(field);
  return (
    <article className="planning-card" draggable={draggable} data-testid={`planning-issue-card-${slug(key)}`} onDragStart={onDragStart} onDragEnd={onDragEnd}>
      <div className="planning-card-topline">
        <button type="button" className="planning-issue-key" onClick={() => onOpenIssue?.(key)}>{key}</button>
        <select
          aria-label={`Status for ${key}`}
          value={issueStatus(issue)}
          disabled={readOnly || pending || !statuses.length}
          onChange={event => onStatus(issue, event.target.value)}
        >
          {statuses.map(status => <option key={status.id} value={status.name}>{status.name}</option>)}
        </select>
      </div>
      <button type="button" className="planning-card-title" onClick={() => onOpenIssue?.(key)}>{issueTitle(issue)}</button>
      <div className="planning-card-meta">
        {show('type') && <span>{issueType(issue)}</span>}
        {show('priority') && <span>{issuePriority(issue)}</span>}
        {show('assignee') && <span>{text(user?.name, assignee || 'Unassigned')}</span>}
        {show('sprint') && <span>{array(sprints).find(item => sprintId(item) === issueSprint(issue) || sprintName(item) === issueSprint(issue))?.name ?? (issueSprint(issue) ? issueSprint(issue) : 'Backlog')}</span>}
      </div>
      {show('labels') && issueLabels(issue).length > 0 && <div className="planning-chip-row">{issueLabels(issue).map(label => <span className="planning-chip" key={label}>{label}</span>)}</div>}
    </article>
  );
}

export function BoardView({ project, readOnly = false, api, onChanged, onOpenIssue, onCreateIssue }) {
  const projectKey = projectKeyOf(project);
  const tickets = array(project?.tickets).filter(ticket => !ticket?.project || projectKeyOf(ticket?.project) === projectKey || issueKey(ticket).toUpperCase().startsWith(`${projectKey}-`));
  const statuses = boardStatusRows(project);
  const columns = boardColumns(project, statuses);
  const sprints = array(project?.sprints);
  const users = array(project?.users);
  const [filters, setFilters] = React.useState({ search: '', status: '', type: '', priority: '', assignee: '', sprint: '', label: '' });
  const [grouping, setGrouping] = React.useState('status');
  const [order, setOrder] = React.useState('position');
  const [draggingKey, setDraggingKey] = React.useState(null);
  const [dropColumnId, setDropColumnId] = React.useState(null);
  const mutation = usePlanningMutation(api, onChanged);

  const values = React.useMemo(() => ({
    types: unique(tickets.map(issueType)),
    priorities: unique(tickets.map(issuePriority)),
    assignees: unique(tickets.map(issueAssignee).filter(Boolean)),
    labels: unique(tickets.flatMap(issueLabels)),
  }), [tickets]);

  const filtered = React.useMemo(() => orderTickets(
    tickets.filter(ticket => matchesFilter(ticket, filters, project?.board?.filter, sprints, users)),
    order,
    users,
  ), [tickets, filters, project?.board?.filter, sprints, users, order]);

  const grouped = React.useMemo(() => {
    if (grouping === 'status') {
      return columns.map(column => ({ ...column, issues: orderTickets(filtered.filter(issue => column.statusIds.includes(issueStatus(issue)) || column.statusNames.includes(issueStatus(issue))), order, users) }));
    }
    const map = new Map();
    for (const issue of filtered) {
      const name = groupName(issue, grouping, sprints, users);
      if (!map.has(name)) map.set(name, []);
      map.get(name).push(issue);
    }
    return [...map.entries()].sort(([left], [right]) => left.localeCompare(right)).map(([name, issues], index) => ({
      id: `${grouping}-${slug(name)}`,
      name,
      statusIds: [],
      statusNames: [],
      position: index,
      wipLimit: null,
      issues: orderTickets(issues, order, users),
    }));
  }, [columns, filtered, grouping, order, sprints, users]);

  const setFilter = (field, value) => setFilters(current => ({ ...current, [field]: value }));
  const changeStatus = async (issue, nextStatus) => {
    if (issueStatus(issue) === nextStatus || readOnly) return;
    const destination = statuses.find(status => status.name === nextStatus);
    // The local workflow policy names only the exact `In Progress` status as
    // forbidden for epics. Other statuses may share the in-progress category
    // (for example `In Review`) and remain valid destinations.
    if (issueType(issue) === 'EPIC' && destination?.name?.toLowerCase() === 'in progress') {
      mutation.reject('Epic issues cannot enter In Progress; move a child issue instead.');
      return;
    }
    try { await mutation.execute({ kind: 'command', args: ['move', issueKey(issue), nextStatus, '--project', projectKey], options: { revisions: revisionsFor(issue) } }); } catch { /* error is rendered in the toolbar */ }
  };

  const draggedIssue = draggingKey ? tickets.find(issue => issueKey(issue) === draggingKey) : null;
  const dropStatusFor = (column, issue) => {
    if (!issue || grouping !== 'status') return null;
    const current = statuses.find(status => status.name.toLowerCase() === issueStatus(issue).toLowerCase());
    const legalTargets = statuses.map(status => status.id);
    const currentStatusId = current?.id ?? issueStatus(issue);
    const target = resolveDropStatus({ ...column, statusIds: column.statusIds }, currentStatusId, legalTargets);
    const destination = statuses.find(status => status.id === target);
    if (issueType(issue) === 'EPIC' && destination?.name.toLowerCase() === 'in progress') return null;
    return destination?.name ?? null;
  };
  const dragStart = (issue, event) => {
    if (readOnly || grouping !== 'status') return;
    setDraggingKey(issueKey(issue));
    event.dataTransfer?.setData('text/plain', issueKey(issue));
    if (event.dataTransfer) event.dataTransfer.effectAllowed = 'move';
  };
  const dragEnd = () => { setDraggingKey(null); setDropColumnId(null); };
  const dragOverColumn = (column, event) => {
    if (!draggedIssue || readOnly || grouping !== 'status' || !dropStatusFor(column, draggedIssue)) return;
    event.preventDefault();
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'move';
    setDropColumnId(column.id);
  };
  const dropColumn = async (column, event) => {
    event.preventDefault();
    const issue = draggedIssue ?? tickets.find(item => issueKey(item) === event.dataTransfer?.getData('text/plain'));
    const destination = dropStatusFor(column, issue);
    setDropColumnId(null);
    if (!issue || !destination) return;
    await changeStatus(issue, destination);
  };

  return <section className="planning-surface planning-board" data-testid="planning-board">
    <header className="planning-surface-header">
      <div><p className="planning-eyebrow">Planning</p><h2>Board</h2><p className="planning-subtitle">{filtered.length} of {tickets.length} project issues</p></div>
      {onCreateIssue && !readOnly && <button type="button" className="planning-primary" onClick={() => onCreateIssue(projectKey)}>Add issue to board</button>}
    </header>
    <div className="planning-toolbar" aria-label="Board controls">
      <label>Search <input aria-label="Planning search" value={filters.search} onChange={event => setFilter('search', event.target.value)} placeholder="Key, title, label" /></label>
      <label>Status <select aria-label="Board status filter" value={filters.status} onChange={event => setFilter('status', event.target.value)}><option value="">All</option>{statuses.map(status => <option key={status.id} value={status.name}>{status.name}</option>)}</select></label>
      <label>Type <select aria-label="Board type filter" value={filters.type} onChange={event => setFilter('type', event.target.value)}><option value="">All</option>{values.types.map(value => <option key={value} value={value}>{value}</option>)}</select></label>
      <label>Priority <select aria-label="Board priority filter" value={filters.priority} onChange={event => setFilter('priority', event.target.value)}><option value="">All</option>{values.priorities.map(value => <option key={value} value={value}>{value}</option>)}</select></label>
      <label>Assignee <select aria-label="Board assignee filter" value={filters.assignee} onChange={event => setFilter('assignee', event.target.value)}><option value="">All</option><option value="__unassigned">Unassigned</option>{values.assignees.map(value => <option key={value} value={value}>{value}</option>)}</select></label>
      <label>Grouping <select aria-label="Board grouping" value={grouping} onChange={event => setGrouping(event.target.value)}><option value="status">Status</option><option value="assignee">Assignee</option><option value="priority">Priority</option><option value="sprint">Sprint</option></select></label>
      <label>Order <select aria-label="Board order" value={order} onChange={event => setOrder(event.target.value)}><option value="position">Position</option><option value="priority">Priority</option><option value="updated">Updated</option><option value="title">Title</option></select></label>
      {mutation.error && <span role="alert" className="planning-error">{mutation.error}</span>}
    </div>
    <div className="planning-columns" data-grouping={grouping}>
      {grouped.map(column => <section className={`planning-column${dropColumnId === column.id ? ' is-drop-target' : ''}`} key={column.id} data-testid={`planning-column-${slug(column.name)}`} onDragOver={event => dragOverColumn(column, event)} onDragLeave={() => { if (dropColumnId === column.id) setDropColumnId(null); }} onDrop={event => dropColumn(column, event)}>
        <header><h3>{column.name}</h3><span>{column.issues.length}{column.wipLimit !== null && column.wipLimit !== undefined ? ` / ${column.wipLimit}` : ''}</span></header>
        <div className="planning-column-cards">
          {column.issues.map(issue => <IssueCard key={issueKey(issue)} issue={issue} statuses={statuses} sprints={sprints} users={users} readOnly={readOnly} onOpenIssue={onOpenIssue} onStatus={changeStatus} pending={mutation.pending} cardFields={project?.board?.cardFields} draggable={!readOnly && grouping === 'status'} onDragStart={event => dragStart(issue, event)} onDragEnd={dragEnd} />)}
          {!column.issues.length && <p className="planning-empty">No issues</p>}
        </div>
      </section>)}
      {!grouped.length && <p className="planning-empty">No project issues match the current filters.</p>}
    </div>
  </section>;
}

export const ProjectBoard = BoardView;
