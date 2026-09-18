import React from 'react';
import {
  JIRA_BOARD_TYPES,
  JIRA_CARD_FIELDS,
  JIRA_CARD_FIELD_LABELS,
  JIRA_SWIMLANE_LABELS,
  JIRA_SWIMLANE_MODES,
  normalizeCardFields,
} from '../../shared/jira/board.js';
import { JIRA_ISSUE_TYPES, JIRA_PRIORITIES } from '../../shared/jira/constants.js';
import { array, positionOf, projectKeyOf, projectRevisionMap, text, usePlanningMutation } from '../planning/helpers.js';

function statusId(status) { return text(status?.id ?? status?.name); }
function statusName(status) { return text(status?.name ?? status?.status ?? status?.id); }

function statusCatalog(project) {
  const source = array(project?.statuses).length ? project.statuses : project?.workflow?.statuses;
  return array(source).map((status, index) => ({
    id: statusId(status),
    name: statusName(status),
    position: positionOf(status?.position, index),
  })).filter(status => status.id).sort((left, right) => left.position - right.position || left.name.localeCompare(right.name));
}

function boardConfig(project, statuses) {
  const source = project?.board ?? {};
  const byId = new Map(statuses.map(status => [status.id, status]));
  const byName = new Map(statuses.map(status => [status.name.toLocaleLowerCase(), status]));
  const columns = array(source.columns ?? source.columnConfig).map((column, index) => {
    const refs = array(column?.statusIds ?? column?.status_ids ?? column?.statuses ?? (column?.statusId ? [column.statusId] : []));
    const resolved = refs.map(ref => {
      const value = text(ref && typeof ref === 'object' ? ref.id ?? ref.name : ref);
      return byId.get(value) ?? byName.get(value.toLocaleLowerCase());
    }).filter(Boolean);
    return {
      id: text(column?.id, `column-${index + 1}`),
      name: text(column?.name, resolved[0]?.name ?? `Column ${index + 1}`),
      statusIds: [...new Set(resolved.map(status => status.id))],
      wipLimit: column?.wipLimit ?? column?.wip_limit ?? null,
      position: positionOf(column?.position, index),
    };
  }).sort((left, right) => left.position - right.position || left.name.localeCompare(right.name));
  const quickFilters = array(source.quickFilters ?? source.quick_filters).map((filter, index) => ({
    id: text(filter?.id ?? filter?.key, `quick-filter-${index + 1}`),
    name: text(filter?.name, `Quick filter ${index + 1}`),
    filter: filter?.filter && typeof filter.filter === 'object' ? filter.filter : {},
  }));
  const cardFields = normalizeCardFields(source.cardFields ?? source.card_fields);
  return {
    type: text(source.type, 'KANBAN').toUpperCase(),
    swimlaneBy: text(source.swimlaneBy ?? source.swimlane_by, 'NONE').toUpperCase(),
    columns,
    cardFields,
    quickFilters,
  };
}

function revisionsFor(value) {
  if (!value?.path || !value.hash) return {};
  return { [value.path]: { hash: value.hash, size: value.size, mtimeMs: value.mtimeMs } };
}

function filterSummary(filter) {
  const entries = Object.entries(filter ?? {}).filter(([, value]) => value !== '' && value !== null && value !== undefined);
  return entries.length ? entries.map(([key, value]) => `${key}=${String(value)}`).join(', ') : 'All issues';
}

function columnStatusName(column, statusMap) {
  return column.statusIds.map(id => statusMap.get(id)?.name ?? id).join(', ');
}

export function BoardSettings({ project, api, onChanged, readOnly = false }) {
  const projectKey = projectKeyOf(project);
  const revisions = projectRevisionMap(project);
  const statuses = statusCatalog(project);
  const statusMap = new Map(statuses.map(status => [status.id, status]));
  const board = boardConfig(project, statuses);
  const mutation = usePlanningMutation(api, onChanged);
  const [newColumn, setNewColumn] = React.useState('');
  const [quickName, setQuickName] = React.useState('');
  const [quickType, setQuickType] = React.useState('');
  const [quickPriority, setQuickPriority] = React.useState('');
  const newColumnRevision = React.useRef(null);
  const quickFilterRevision = React.useRef(null);
  const columnDraftRevisions = React.useRef(new Map());

  const run = request => mutation.execute({ ...request, options: { ...(request.options ?? {}), revisions: { ...revisions, ...(request.options?.revisions ?? {}) } } }).catch(() => null);
  const updateSettings = data => run({ kind: 'action', action: 'board.settings.update', project: projectKey, data });
  const updateColumn = (column, data, baseline = null) => run({ kind: 'action', action: 'board.column.update', project: projectKey, data: { id: column.id, ...data }, options: { revisions: baseline ?? revisionsFor(column) } });
  const moveColumn = (index, direction) => {
    const target = index + direction;
    if (target < 0 || target >= board.columns.length) return;
    const order = board.columns.map(column => column.id);
    [order[index], order[target]] = [order[target], order[index]];
    run({ kind: 'action', action: 'board.column.reorder', project: projectKey, data: { order } });
  };
  const mapStatus = (column, statusId) => run({ kind: 'action', action: 'board.column.status.map', project: projectKey, data: { columnId: column.id, statusId } });
  const unmapStatus = (column, statusId) => run({ kind: 'action', action: 'board.column.status.unmap', project: projectKey, data: { columnId: column.id, statusId } });
  const createColumn = async () => {
    const name = text(newColumn);
    if (!name) return;
    const result = await run({ kind: 'action', action: 'board.column.create', project: projectKey, data: { name }, options: { revisions: newColumnRevision.current ?? revisions } });
    if (result) { setNewColumn(''); newColumnRevision.current = null; }
  };
  const createQuickFilter = async () => {
    const name = text(quickName);
    if (!name) return;
    const filter = {};
    if (quickType) filter.type = quickType;
    if (quickPriority) filter.priority = quickPriority;
    const result = await run({ kind: 'action', action: 'board.quick-filter.create', project: projectKey, data: { name, filter }, options: { revisions: quickFilterRevision.current ?? revisions } });
    if (result) {
      setQuickName('');
      setQuickType('');
      setQuickPriority('');
      quickFilterRevision.current = null;
    }
  };

  return <section className="settings-surface settings-board" data-testid="settings-board">
    <header className="settings-header"><div><p className="planning-eyebrow">Project settings</p><h2>Board settings</h2><p className="planning-subtitle">Columns, status mapping, swimlanes, card fields, and quick filters</p></div>{mutation.error && <span role="alert" className="planning-error">{mutation.error}</span>}</header>
    <section className="settings-card settings-board-options"><header><div><h3>Board behavior</h3><p>Board type controls which issues are shown; swimlanes only changes how visible issues are grouped.</p></div></header><div className="settings-option-grid">
      <label>Board type<select aria-label="Board type" value={board.type} disabled={readOnly || mutation.pending} onChange={event => updateSettings({ type: event.target.value })}>{JIRA_BOARD_TYPES.map(type => <option key={type} value={type}>{type}</option>)}</select><small>{board.type === 'SCRUM' ? 'Show active sprint issues.' : 'Show all project issues.'}</small></label>
      <label>Swimlanes<select aria-label="Swimlanes" value={board.swimlaneBy} disabled={readOnly || mutation.pending} onChange={event => updateSettings({ swimlaneBy: event.target.value })}>{JIRA_SWIMLANE_MODES.map(mode => <option key={mode} value={mode}>{JIRA_SWIMLANE_LABELS[mode] ?? mode}</option>)}</select></label>
    </div></section>

    <section className="settings-card"><header><div><h3>Columns</h3><p>A column draws one or more workflow statuses. Mapping changes the board layout only.</p></div></header><ul className="settings-column-list">
      {board.columns.map((column, index) => <li key={column.id} data-testid={`settings-column-${column.id}`}><div className="settings-column-heading"><div><strong>{column.name}</strong><span>{column.statusIds.length ? columnStatusName(column, statusMap) : 'Draws no statuses'}</span></div>{!readOnly && <div className="settings-row-actions"><label className="settings-wip">WIP <input type="number" min="0" aria-label={`WIP limit for ${column.name}`} defaultValue={column.wipLimit ?? ''} disabled={mutation.pending} onFocus={() => { if (!columnDraftRevisions.current.has(column.id)) columnDraftRevisions.current.set(column.id, revisions); }} onBlur={event => { const raw = event.target.value.trim(); const wipLimit = raw === '' ? null : Number(raw); if ((column.wipLimit ?? null) === wipLimit || !Number.isFinite(wipLimit) || wipLimit < 0) return; const baseline = columnDraftRevisions.current.get(column.id) ?? revisions; updateColumn(column, { wipLimit }, baseline); columnDraftRevisions.current.delete(column.id); }} /></label><button type="button" aria-label={`Move ${column.name} earlier`} disabled={mutation.pending || index === 0} onClick={() => moveColumn(index, -1)}>↑</button><button type="button" aria-label={`Move ${column.name} later`} disabled={mutation.pending || index === board.columns.length - 1} onClick={() => moveColumn(index, 1)}>↓</button><button type="button" className="planning-danger-link" disabled={mutation.pending} aria-label={`Delete ${column.name}`} onClick={() => run({ kind: 'action', action: 'board.column.delete', project: projectKey, data: { id: column.id }, options: { revisions: revisionsFor(column) } })}>Delete</button></div>}</div>{!readOnly && <div className="settings-column-mapping"><div className="settings-chip-row">{column.statusIds.map(statusId => <span className="settings-chip" key={statusId}>{statusMap.get(statusId)?.name ?? statusId}<button type="button" aria-label={`Unmap ${statusMap.get(statusId)?.name ?? statusId} from ${column.name}`} disabled={mutation.pending} onClick={() => unmapStatus(column, statusId)}>×</button></span>)}{!column.statusIds.length && <small>No statuses mapped</small>}</div><select aria-label={`Add a status to ${column.name}`} value="" disabled={mutation.pending} onChange={event => { if (event.target.value) mapStatus(column, event.target.value); }}><option value="">Add a status…</option>{statuses.filter(status => !column.statusIds.includes(status.id)).map(status => <option key={status.id} value={status.id}>{status.name}</option>)}</select></div>}</li>)}
      {!board.columns.length && <li className="planning-empty">No columns configured. Add one to draw workflow statuses.</li>}
    </ul>{!readOnly && <div className="settings-create-row"><input aria-label="New column name" placeholder="New column name" value={newColumn} onChange={event => { if (text(event.target.value) && !newColumnRevision.current) newColumnRevision.current = revisions; setNewColumn(event.target.value); }} /><button type="button" className="planning-primary" disabled={mutation.pending || !text(newColumn)} onClick={createColumn}>Add column</button></div>}</section>

    <section className="settings-card"><header><div><h3>Card layout</h3><p>Choose the information shown on each board card.</p></div></header><div className="settings-checkbox-grid">{JIRA_CARD_FIELDS.map(field => <label key={field}><input type="checkbox" aria-label={JIRA_CARD_FIELD_LABELS[field] ?? field} checked={board.cardFields.includes(field)} disabled={readOnly || mutation.pending} onChange={event => { const next = new Set(board.cardFields); if (event.target.checked) next.add(field); else next.delete(field); updateSettings({ cardFields: [...next] }); }} />{JIRA_CARD_FIELD_LABELS[field] ?? field}</label>)}</div></section>

    <section className="settings-card"><header><div><h3>Quick filters</h3><p>Saved filters narrow the board view and never widen the project scope.</p></div></header><ul className="settings-simple-list">{board.quickFilters.length ? board.quickFilters.map(filter => <li key={filter.id}><div><strong>{filter.name}</strong><span>{filterSummary(filter.filter)}</span></div>{!readOnly && <button type="button" className="planning-danger-link" aria-label={`Delete quick filter ${filter.name}`} disabled={mutation.pending} onClick={() => run({ kind: 'action', action: 'board.quick-filter.delete', project: projectKey, data: { id: filter.id } })}>Delete</button>}</li>) : <li className="planning-empty">No quick filters have been defined.</li>}</ul>{!readOnly && <div className="settings-create-row settings-quick-filter-form"><input aria-label="Quick filter name" placeholder="My open bugs" value={quickName} onChange={event => { if (text(event.target.value) && !quickFilterRevision.current) quickFilterRevision.current = revisions; setQuickName(event.target.value); }} /><select aria-label="Quick filter type" value={quickType} onChange={event => setQuickType(event.target.value)}><option value="">Any type</option>{JIRA_ISSUE_TYPES.map(type => <option key={type} value={type}>{type}</option>)}</select><select aria-label="Quick filter priority" value={quickPriority} onChange={event => setQuickPriority(event.target.value)}><option value="">Any priority</option>{JIRA_PRIORITIES.map(priority => <option key={priority} value={priority}>{priority}</option>)}</select><button type="button" className="planning-primary" disabled={mutation.pending || !text(quickName)} onClick={createQuickFilter}>Add quick filter</button></div>}</section>
  </section>;
}

export const BoardEditor = BoardSettings;
