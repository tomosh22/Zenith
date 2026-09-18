import React from 'react';
import {
  array,
  issueAssignee,
  issueKey,
  issueLabels,
  issueParent,
  issuePriority,
  issueStatus,
  issueStatusCategory,
  issueTitle,
  issueType,
  numeric,
  positionOf,
  projectKeyOf,
  sprintName,
  text,
} from './helpers.js';

function slug(value) {
  return text(value, 'item').toLowerCase().replace(/[^a-z0-9]+/g, '-');
}

function storyPoints(issue) {
  return numeric(issue?.storyPoints ?? issue?.story_points ?? issue?.points ?? issue?.metadata?.storyPoints ?? issue?.metadata?.story_points, 0);
}

function projectTickets(project) {
  const key = projectKeyOf(project);
  return array(project?.tickets).filter(issue => !issue?.project || projectKeyOf(issue.project) === key || issueKey(issue).toUpperCase().startsWith(`${key}-`));
}

function hasParent(issue) {
  return Boolean(issueParent(issue));
}

function done(issue, statuses) {
  return issueStatusCategory(issue, statuses) === 'DONE' || issueStatus(issue).toUpperCase() === 'DONE';
}

function childSort(left, right) {
  return positionOf(left?.position ?? left?.metadata?.position, Number.MAX_SAFE_INTEGER) - positionOf(right?.position ?? right?.metadata?.position, Number.MAX_SAFE_INTEGER)
    || issueKey(left).localeCompare(issueKey(right));
}

function progress(issues, statuses) {
  const total = issues.length;
  const finished = issues.filter(issue => done(issue, statuses));
  const points = issues.reduce((sum, issue) => sum + storyPoints(issue), 0);
  const donePoints = finished.reduce((sum, issue) => sum + storyPoints(issue), 0);
  return { total, done: finished.length, points, donePoints, percent: points > 0 ? Math.round(donePoints / points * 100) : total ? Math.round(finished.length / total * 100) : 0 };
}

function statusColor(category) {
  if (category === 'DONE') return 'is-done';
  if (category === 'IN_PROGRESS') return 'is-progress';
  return 'is-todo';
}

function ChildRow({ issue, statuses, onOpenIssue }) {
  const category = issueStatusCategory(issue, statuses);
  return <li className="planning-roadmap-child" data-testid={`planning-roadmap-child-${slug(issueKey(issue))}`}>
    <button type="button" className="planning-issue-key" onClick={() => onOpenIssue?.(issueKey(issue))}>{issueKey(issue)}</button>
    <button type="button" className={`planning-roadmap-child-title${category === 'DONE' ? ' is-done' : ''}`} onClick={() => onOpenIssue?.(issueKey(issue))}>{issueTitle(issue)}</button>
    <span className="planning-roadmap-type">{issueType(issue)}</span>
    <span className="planning-roadmap-priority">{issuePriority(issue)}</span>
    <span className={`planning-roadmap-status ${statusColor(category)}`}>{issueStatus(issue) || 'No status'}</span>
    {storyPoints(issue) > 0 && <span className="planning-roadmap-points">{storyPoints(issue)} pts</span>}
    {issueAssignee(issue) && <span className="planning-roadmap-assignee">{issueAssignee(issue)}</span>}
    {issue?.blockedBy?.length > 0 && <span className="planning-chip planning-chip-warning">Blocked</span>}
  </li>;
}

function EpicCard({ epic, children, statuses, versions, collapsed, onToggle, onOpenIssue }) {
  const stats = progress(children, statuses);
  const explicit = epic?.progress ?? epic?.stats;
  const shown = explicit && typeof explicit === 'object' ? {
    total: numeric(explicit.total, stats.total),
    done: numeric(explicit.done ?? explicit.completed, stats.done),
    points: numeric(explicit.points, stats.points),
    donePoints: numeric(explicit.donePoints ?? explicit.completedPoints, stats.donePoints),
  } : stats;
  const percent = shown.points > 0 ? Math.round(shown.donePoints / shown.points * 100) : shown.total ? Math.round(shown.done / shown.total * 100) : 0;
  const key = issueKey(epic);
  const version = array(versions).find(item => issueVersionsFor(epic).some(value => value.toLowerCase() === text(item?.name ?? item?.id).toLowerCase()));
  return <section className="planning-roadmap-epic" data-testid={`planning-roadmap-epic-${slug(key)}`}>
    <header className="planning-roadmap-epic-header"><button type="button" aria-expanded={!collapsed} aria-label={`${collapsed ? 'Expand' : 'Collapse'} ${issueTitle(epic)}`} onClick={onToggle}><span aria-hidden="true">{collapsed ? '▸' : '▾'}</span></button><span className="planning-roadmap-epic-mark" aria-hidden="true">◆</span><button type="button" className="planning-issue-key" onClick={() => onOpenIssue?.(key)}>{key}</button><button type="button" className="planning-roadmap-epic-title" onClick={() => onOpenIssue?.(key)}>{issueTitle(epic)}</button>{version && <span className="planning-chip">{versionNameFor(version)}</span>}<span className="planning-roadmap-count">{shown.done}/{shown.total} done{shown.points > 0 ? ` · ${shown.donePoints}/${shown.points} pts` : ''}</span></header>
    <div className="planning-progress" aria-label={`${issueTitle(epic)} progress`}><span style={{ width: `${Math.max(0, Math.min(100, percent))}%` }} /><small>{percent}%</small></div>
    {!collapsed && <div className="planning-roadmap-children">{children.length ? <ul>{children.map(child => <ChildRow key={issueKey(child)} issue={child} statuses={statuses} onOpenIssue={onOpenIssue} />)}</ul> : <p className="planning-empty">No child issues yet.</p>}</div>}
  </section>;
}

function issueVersionsFor(issue) {
  const value = issue?.fixVersions ?? issue?.versions ?? issue?.metadata?.fixVersions ?? issue?.metadata?.versions;
  return array(value).map(item => text(item && typeof item === 'object' ? item.name ?? item.id : item)).filter(Boolean);
}

function versionNameFor(version) {
  return text(version?.name ?? version?.title ?? version?.id);
}

function normalizeExplicitRoadmap(project, tickets, statuses) {
  const explicit = project?.roadmap;
  if (!explicit || typeof explicit !== 'object') return null;
  const epics = array(explicit.epics).map((item, index) => {
    const key = issueKey(item) || text(item?.key, `EPIC-${index + 1}`);
    const children = array(item.children).map(child => tickets.find(ticket => issueKey(ticket) === issueKey(child)) ?? child).sort(childSort);
    return { ...item, key, children, _position: positionOf(item.position, index) };
  }).sort((left, right) => left._position - right._position || issueKey(left).localeCompare(issueKey(right)));
  const orphans = array(explicit.orphans).map(child => tickets.find(ticket => issueKey(ticket) === issueKey(child)) ?? child).sort(childSort);
  return { epics, orphans };
}

export function RoadmapView({ project, readOnly = false, onOpenIssue }) {
  void readOnly;
  const tickets = projectTickets(project);
  const statuses = array(project?.statuses);
  const versions = array(project?.versions);
  const explicit = normalizeExplicitRoadmap(project, tickets, statuses);
  const epics = explicit?.epics ?? tickets.filter(issue => issueType(issue) === 'EPIC').sort(childSort);
  const epicKeys = new Set(epics.map(issue => issueKey(issue)));
  const childrenByEpic = new Map(epics.map(epic => [issueKey(epic), []]));
  const orphans = explicit?.orphans ?? [];
  if (!explicit) {
    for (const issue of tickets) {
      if (issueType(issue) === 'EPIC') continue;
      const parent = issueParent(issue);
      if (parent && epicKeys.has(parent)) childrenByEpic.get(parent).push(issue);
    }
    for (const issue of tickets) {
      if (issueType(issue) !== 'EPIC' && (!hasParent(issue) || !epicKeys.has(issueParent(issue)))) orphans.push(issue);
    }
    for (const values of childrenByEpic.values()) values.sort(childSort);
    orphans.sort(childSort);
  }
  const [collapsed, setCollapsed] = React.useState(() => new Set());
  const toggle = key => setCollapsed(current => { const next = new Set(current); if (next.has(key)) next.delete(key); else next.add(key); return next; });

  return <section className="planning-surface planning-roadmap" data-testid="planning-roadmap">
    <header className="planning-surface-header"><div><p className="planning-eyebrow">Planning</p><h2>Roadmap</h2><p className="planning-subtitle">Epics, child work, and progress by issue completion</p></div><span className="planning-roadmap-summary">{epics.length} epics · {orphans.length} ungrouped issues</span></header>
    {!epics.length && !orphans.length && <p className="planning-empty">No epics yet. Create an EPIC and give it child issues to build a roadmap.</p>}
    <div className="planning-roadmap-list">{epics.map(epic => <EpicCard key={issueKey(epic)} epic={epic} children={childrenByEpic.get(issueKey(epic)) ?? array(epic.children)} statuses={statuses} versions={versions} collapsed={collapsed.has(issueKey(epic))} onToggle={() => toggle(issueKey(epic))} onOpenIssue={onOpenIssue} />)}</div>
    {orphans.length > 0 && <section className="planning-roadmap-orphans" data-testid="planning-roadmap-orphans"><header><h3>Not in an epic</h3><span>{orphans.length} issues</span></header><ul>{orphans.map(issue => <ChildRow key={issueKey(issue)} issue={issue} statuses={statuses} onOpenIssue={onOpenIssue} />)}</ul></section>}
  </section>;
}

export const ProjectRoadmap = RoadmapView;

