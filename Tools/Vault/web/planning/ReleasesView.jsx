import React from 'react';
import {
  array,
  dateInputValue,
  formatDate,
  issueKey,
  issueVersions,
  numeric,
  positionOf,
  projectKeyOf,
  text,
  usePlanningMutation,
  versionId,
  versionName,
} from './helpers.js';

function slug(value) {
  return text(value, 'version').toLowerCase().replace(/[^a-z0-9]+/g, '-');
}

function versionStatus(version) {
  return text(version?.status, 'UNRELEASED').toUpperCase();
}

function versionRevision(version) {
  if (!version?.path || !version.hash) return {};
  return { [version.path]: { hash: version.hash, size: version.size, mtimeMs: version.mtimeMs } };
}

function versionsCatalogRevision(project) {
  const revisions = project?.revisions && typeof project.revisions === 'object' ? project.revisions : {};
  const entry = Object.entries(revisions).find(([path]) => /(?:^|\/)versions\.md$/i.test(path));
  return entry ? { [entry[0]]: entry[1] } : {};
}

function progressFor(version, tickets) {
  const id = versionId(version);
  const rows = array(version?.progress ?? version?.stats ?? tickets?.progress);
  const explicit = rows.find(row => text(row?.versionId ?? row?.version_id ?? row?.id) === id);
  const source = explicit ?? version;
  const members = array(tickets).filter(issue => issueVersions(issue).some(item => item.toLowerCase() === versionName(version).toLowerCase()));
  const total = numeric(source?.total ?? source?.issues ?? source?.issueCount, members.length);
  const done = numeric(source?.done ?? source?.completed ?? source?.doneIssues, 0);
  const points = numeric(source?.points ?? source?.storyPoints, members.reduce((sum, issue) => sum + numeric(issue?.storyPoints ?? issue?.points ?? issue?.metadata?.storyPoints), 0));
  const donePoints = numeric(source?.donePoints ?? source?.completedPoints, 0);
  return { total, done, points, donePoints };
}

function Progress({ version, stats }) {
  const percent = stats.points > 0
    ? Math.round((stats.donePoints / stats.points) * 100)
    : stats.total > 0 ? Math.round((stats.done / stats.total) * 100) : 0;
  return <div className="planning-progress" aria-label={`${versionName(version)} progress`}>
    <span style={{ width: `${Math.max(0, Math.min(100, percent))}%` }} />
    <small>{percent}%</small>
  </div>;
}

function VersionEditor({ values, setValues, pending, onSave, onCancel }) {
  return <div className="planning-inline-editor" data-testid="planning-version-editor">
    <label>Name <input aria-label="Version name" value={values.name} onChange={event => setValues(current => ({ ...current, name: event.target.value }))} /></label>
    <label>Description <textarea aria-label="Version description" value={values.description} onChange={event => setValues(current => ({ ...current, description: event.target.value }))} rows="2" /></label>
    <label>Release date <input aria-label="Version release date" type="date" value={values.releaseDate} onChange={event => setValues(current => ({ ...current, releaseDate: event.target.value }))} /></label>
    <div className="planning-inline-actions"><button type="button" className="planning-primary" disabled={pending || !text(values.name)} onClick={onSave}>{pending ? 'Saving…' : 'Save version'}</button><button type="button" onClick={onCancel}>Cancel</button></div>
  </div>;
}

function Confirmation({ title, children, pending, onCancel, onConfirm }) {
  return <div className="planning-confirm-backdrop"><div className="planning-confirm" role="dialog" aria-modal="true" aria-labelledby="planning-version-confirm-title">
    <h3 id="planning-version-confirm-title">{title}</h3><div className="planning-confirm-copy">{children}</div>
    <div className="planning-inline-actions"><button type="button" onClick={onCancel}>Cancel</button><button type="button" className="planning-danger" disabled={pending} onClick={onConfirm}>{pending ? 'Working…' : 'Delete version'}</button></div>
  </div></div>;
}

export function ReleasesView({ project, readOnly = false, api, onChanged, onOpenIssue }) {
  const projectKey = projectKeyOf(project);
  const versions = React.useMemo(() => array(project?.versions).map((version, index) => ({ ...version, _position: positionOf(version?.position ?? version?.metadata?.position, index) })).sort((left, right) => left._position - right._position || versionName(left).localeCompare(versionName(right))), [project?.versions]);
  const tickets = array(project?.tickets);
  const [creating, setCreating] = React.useState(false);
  const [newValues, setNewValues] = React.useState({ name: '', description: '' });
  const [editing, setEditing] = React.useState(null);
  const [editValues, setEditValues] = React.useState({ name: '', description: '', releaseDate: '' });
  const [confirm, setConfirm] = React.useState(null);
  const mutation = usePlanningMutation(api, onChanged);

  const create = async () => {
    const name = text(newValues.name);
    if (!name) return;
    try {
      await mutation.execute({ kind: 'command', args: ['version', 'create', name, '--project', projectKey, ...(text(newValues.description) ? ['--description', text(newValues.description)] : [])], options: { revisions: versionsCatalogRevision(project) } });
      setNewValues({ name: '', description: '' });
      setCreating(false);
    } catch { /* keep the draft visible for correction/retry */ }
  };

  const openEditor = version => {
    setEditing(version);
    setEditValues({ name: versionName(version), description: text(version?.description), releaseDate: dateInputValue(version?.releaseDate ?? version?.release_date) });
  };

  const save = async version => {
    try {
      await mutation.execute({ kind: 'action', action: 'version.update', project: projectKey, data: { id: versionId(version), name: text(editValues.name), description: text(editValues.description) || null, releaseDate: editValues.releaseDate || null }, options: { revisions: versionRevision(version) } });
      setEditing(null);
    } catch { /* keep the editor open so a failed draft can be corrected */ }
  };

  const release = version => mutation.execute({ kind: 'command', args: ['version', 'release', versionName(version), '--project', projectKey], options: { revisions: { ...versionsCatalogRevision(project), ...versionRevision(version) } } }).catch(() => {});
  const archive = version => mutation.execute({ kind: 'action', action: 'version.archive', project: projectKey, data: { id: versionId(version) }, options: { revisions: versionRevision(version) } }).catch(() => {});
  const remove = version => mutation.execute({ kind: 'action', action: 'version.delete', project: projectKey, data: { id: versionId(version) }, options: { revisions: versionRevision(version) } });
  const reorder = async (version, direction) => {
    const index = versions.findIndex(item => versionId(item) === versionId(version));
    const target = index + direction;
    if (index < 0 || target < 0 || target >= versions.length) return;
    const order = versions.map(item => versionId(item));
    [order[index], order[target]] = [order[target], order[index]];
    try { await mutation.execute({ kind: 'action', action: 'version.reorder', project: projectKey, data: { order } }); } catch { /* error is rendered above the list */ }
  };

  return <section className="planning-surface planning-releases" data-testid="planning-releases">
    <header className="planning-surface-header"><div><p className="planning-eyebrow">Planning</p><h2>Releases</h2><p className="planning-subtitle">Milestones and the work they contain</p></div>{!readOnly && <button type="button" className="planning-primary" onClick={() => setCreating(current => !current)}>{creating ? 'Cancel' : 'New release'}</button>}</header>
    {creating && !readOnly && <div className="planning-create-panel" data-testid="planning-version-create"><label>Name <input aria-label="New version name" value={newValues.name} onChange={event => setNewValues(current => ({ ...current, name: event.target.value }))} placeholder="1.0" /></label><label>Description <input aria-label="New version description" value={newValues.description} onChange={event => setNewValues(current => ({ ...current, description: event.target.value }))} placeholder="What ships" /></label><button type="button" className="planning-primary" disabled={mutation.pending || !text(newValues.name)} onClick={create}>Create release</button></div>}
    {mutation.error && <div role="alert" className="planning-error">{mutation.error}</div>}
    {!versions.length && <p className="planning-empty">No releases yet. Create a milestone for work that should ship together.</p>}
    <div className="planning-release-list">{versions.map((version, index) => {
      const status = versionStatus(version);
      const archived = status === 'ARCHIVED';
      const stats = progressFor(version, tickets);
      return <article key={versionId(version)} className={`planning-release-card${archived ? ' is-archived' : ''}`} data-testid={`planning-version-${slug(versionId(version))}`} data-version-status={status}>
        <header className="planning-release-header"><span className="planning-release-mark" aria-hidden="true">◈</span><div className="planning-release-heading"><h3>{versionName(version)}</h3><span className="planning-status-badge">{status === 'UNRELEASED' ? 'Unreleased' : status[0] + status.slice(1).toLowerCase()}</span></div><span className="planning-release-count">{stats.done}/{stats.total} issues{stats.points > 0 ? ` · ${stats.donePoints}/${stats.points} pts` : ''}</span></header>
        {version?.description && <p className="planning-release-description">{version.description}</p>}
        {version?.releaseDate && <p className="planning-release-date">Released {formatDate(version.releaseDate)}</p>}
        <Progress version={version} stats={stats} />
        {!readOnly && <div className="planning-release-actions">{status === 'UNRELEASED' && <button type="button" disabled={mutation.pending} onClick={() => release(version)}>Release</button>}{!archived && <button type="button" disabled={mutation.pending} onClick={() => archive(version)}>Archive</button>}<button type="button" disabled={mutation.pending} onClick={() => openEditor(version)}>Edit</button><button type="button" disabled={mutation.pending || index === 0} aria-label={`Move ${versionName(version)} earlier`} onClick={() => reorder(version, -1)}>↑</button><button type="button" disabled={mutation.pending || index === versions.length - 1} aria-label={`Move ${versionName(version)} later`} onClick={() => reorder(version, 1)}>↓</button><button type="button" className="planning-danger-link" disabled={mutation.pending} onClick={() => setConfirm(version)}>Delete</button></div>}
        {editing && versionId(editing) === versionId(version) && <VersionEditor values={editValues} setValues={setEditValues} pending={mutation.pending} onSave={() => save(version)} onCancel={() => setEditing(null)} />}
        {stats.total > 0 && <button type="button" className="planning-release-issue-count" onClick={() => tickets.filter(issue => issueVersions(issue).some(item => item.toLowerCase() === versionName(version).toLowerCase())).slice(0, 1).forEach(issue => onOpenIssue?.(issueKey(issue)))}>Open included issues</button>}
      </article>;
    })}</div>
    {confirm && <Confirmation title={`Delete ${versionName(confirm)}?`} pending={mutation.pending} onCancel={() => setConfirm(null)} onConfirm={async () => { try { await remove(confirm); setConfirm(null); } catch { /* keep the confirmation open after a refused deletion */ } }}>Issues keep their documents; only this release membership is removed.</Confirmation>}
  </section>;
}

export const ProjectReleases = ReleasesView;
