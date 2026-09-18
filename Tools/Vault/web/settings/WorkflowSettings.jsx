import React from 'react';
import {
  JIRA_ORG_ROLES,
  TRANSITION_REQUIRED_FIELDS,
  TRANSITION_RULE_KINDS,
  TRANSITION_RULE_LABELS,
  TRANSITION_RULE_TYPES,
} from '../../shared/jira/workflow.js';
import { JIRA_RESOLUTIONS, JIRA_STATUS_CATEGORIES } from '../../shared/jira/constants.js';
import { array, positionOf, projectKeyOf, projectRevisionMap, text, usePlanningMutation } from '../planning/helpers.js';

const CATEGORY_LABELS = { TODO: 'To do', IN_PROGRESS: 'In progress', DONE: 'Done' };

function statusId(status) { return text(status?.id ?? status?.name); }
function statusName(status) { return text(status?.name ?? status?.status ?? status?.id); }
function statusCategory(status) { return text(status?.category, 'TODO').toUpperCase(); }
function transitionFrom(transition) { return transition?.fromStatusId ?? transition?.from_status_id ?? transition?.from ?? null; }
function transitionTo(transition) { return transition?.toStatusId ?? transition?.to_status_id ?? transition?.to ?? null; }
function transitionId(transition) { return text(transition?.id ?? `${transitionFrom(transition) ?? 'global'}-${transitionTo(transition)}`); }
function statusCatalog(project) {
  const source = array(project?.workflow?.statuses).length ? project.workflow.statuses : project?.statuses;
  return array(source).map((status, index) => ({ ...status, id: statusId(status), name: statusName(status), category: statusCategory(status), _position: positionOf(status?.position, index) })).sort((left, right) => left._position - right._position || left.name.localeCompare(right.name));
}
function transitionCatalog(project) {
  return array(project?.workflow?.transitions).map(transition => ({ ...transition, id: transitionId(transition), fromStatusId: transitionFrom(transition), toStatusId: transitionTo(transition), rules: array(transition?.rules) }));
}

function RuleEditor({ transition, screens, pending, onAdd, onScreen }) {
  const [kind, setKind] = React.useState('VALIDATOR');
  const [ruleType, setRuleType] = React.useState(TRANSITION_RULE_TYPES.VALIDATOR?.[0] ?? 'NO_OPEN_BLOCKERS');
  const [value, setValue] = React.useState('');
  const types = TRANSITION_RULE_TYPES[kind] ?? [];
  const changeKind = next => { setKind(next); setRuleType(TRANSITION_RULE_TYPES[next]?.[0] ?? ''); setValue(''); };
  const options = ruleType === 'ORG_ROLE' ? JIRA_ORG_ROLES.map(role => ({ value: role, label: role }))
    : ruleType === 'FIELD_REQUIRED' ? TRANSITION_REQUIRED_FIELDS.map(field => ({ value: field, label: field }))
      : ruleType === 'SET_RESOLUTION' ? JIRA_RESOLUTIONS.map(valueItem => ({ value: valueItem, label: valueItem }))
        : ruleType === 'SET_FLAGGED' ? [{ value: 'true', label: 'Flagged' }, { value: 'false', label: 'Not flagged' }] : null;
  const config = ruleType === 'ORG_ROLE' ? { role: value } : ruleType === 'FIELD_REQUIRED' ? { field: value } : ruleType === 'SET_RESOLUTION' ? { resolution: value } : ruleType === 'SET_FLAGGED' ? { flagged: value === 'true' } : {};
  return <div className="settings-rule-editor">
    <label>Kind <select aria-label="Rule kind" value={kind} disabled={pending} onChange={event => changeKind(event.target.value)}>{TRANSITION_RULE_KINDS.map(item => <option key={item} value={item}>{item}</option>)}</select></label>
    <label>Rule <select aria-label="Rule type" value={ruleType} disabled={pending} onChange={event => { setRuleType(event.target.value); setValue(''); }}>{types.map(item => <option key={item} value={item}>{TRANSITION_RULE_LABELS[item] ?? item}</option>)}</select></label>
    {options && <label>Value <select aria-label="Rule value" value={value} disabled={pending} onChange={event => setValue(event.target.value)}><option value="">Choose…</option>{options.map(item => <option key={item.value} value={item.value}>{item.label}</option>)}</select></label>}
    <button type="button" className="planning-primary" disabled={pending || Boolean(options && !value)} onClick={() => onAdd({ transitionId: transitionId(transition), kind, type: ruleType, config })}>Add rule</button>
    <label>Screen <select aria-label={`Screen for ${transitionId(transition)}`} disabled={pending} value={text(transition?.screenId)} onChange={event => onScreen(transition, event.target.value || null)}><option value="">None</option>{array(screens).map(screen => <option key={text(screen?.id)} value={text(screen?.id)}>{text(screen?.name, screen?.id)}</option>)}</select></label>
  </div>;
}

export function WorkflowSettings({ project, api, onChanged, readOnly = false }) {
  const projectKey = projectKeyOf(project);
  const revisions = projectRevisionMap(project);
  const statuses = statusCatalog(project);
  const transitions = transitionCatalog(project);
  const screens = array(project?.screens);
  const [newStatus, setNewStatus] = React.useState({ name: '', category: 'TODO' });
  const [renaming, setRenaming] = React.useState(null);
  const [renameValue, setRenameValue] = React.useState('');
  const [openRules, setOpenRules] = React.useState(null);
  const newStatusRevision = React.useRef(null);
  const renameRevision = React.useRef(null);
  const mutation = usePlanningMutation(api, onChanged);
  const run = request => mutation.execute({ ...request, options: { ...(request.options ?? {}), revisions: { ...revisions, ...(request.options?.revisions ?? {}) } } }).catch(() => null);
  const edgeAt = (from, to) => transitions.find(transition => transition.fromStatusId === from && transition.toStatusId === to);
  const addStatus = async () => {
    const name = text(newStatus.name);
    if (!name) return;
    const result = await run({ kind: 'action', action: 'workflow.status.create', project: projectKey, data: { name, category: newStatus.category }, options: { revisions: newStatusRevision.current ?? revisions } });
    if (result) { setNewStatus({ name: '', category: 'TODO' }); newStatusRevision.current = null; }
  };
  const rename = async status => {
    const name = text(renameValue);
    if (!name) return;
    const result = await run({ kind: 'action', action: 'workflow.status.update', project: projectKey, data: { id: status.id, name }, options: { revisions: renameRevision.current ?? revisions } });
    if (result) { setRenaming(null); renameRevision.current = null; }
  };
  const updateCategory = (status, category) => run({ kind: 'action', action: 'workflow.status.update', project: projectKey, data: { id: status.id, category } });
  const reorder = (index, direction) => {
    const target = index + direction;
    if (target < 0 || target >= statuses.length) return;
    const order = statuses.map(status => status.id);
    [order[index], order[target]] = [order[target], order[index]];
    run({ kind: 'action', action: 'workflow.status.reorder', project: projectKey, data: { order } });
  };
  const addTransition = (fromStatusId, toStatusId) => run({ kind: 'action', action: 'workflow.transition.create', project: projectKey, data: { name: `To ${statuses.find(status => status.id === toStatusId)?.name ?? toStatusId}`, fromStatusId, toStatusId } });
  const removeTransition = transition => run({ kind: 'action', action: 'workflow.transition.delete', project: projectKey, data: { id: transitionId(transition) } });
  const addRule = rule => run({ kind: 'action', action: 'workflow.transition.rule.create', project: projectKey, data: rule });
  const removeRule = rule => run({ kind: 'action', action: 'workflow.transition.rule.delete', project: projectKey, data: { id: text(rule?.id) } });
  const setScreen = (transition, screenId) => run({ kind: 'action', action: 'workflow.transition.screen', project: projectKey, data: { transitionId: transitionId(transition), screenId } });

  return <section className="settings-surface settings-workflow" data-testid="settings-workflow">
    <header className="settings-header"><div><p className="planning-eyebrow">Project settings</p><h2>Workflow</h2><p className="planning-subtitle">Statuses, legal moves, transition rules, and screens</p></div>{mutation.error && <span role="alert" className="planning-error">{mutation.error}</span>}</header>
    <section className="settings-card"><header><div><h3>Statuses</h3><p>Arrange the states that describe work in this project.</p></div></header><ul className="settings-status-list">{statuses.map((status, index) => <li key={status.id} data-testid={`settings-status-${status.id}`}><strong>{status.name}</strong><span className="settings-status-category">{CATEGORY_LABELS[status.category] ?? status.category}</span>{!readOnly && <div className="settings-row-actions"><select aria-label={`Category for ${status.name}`} value={status.category} disabled={mutation.pending} onChange={event => updateCategory(status, event.target.value)}>{JIRA_STATUS_CATEGORIES.map(category => <option key={category} value={category}>{CATEGORY_LABELS[category] ?? category}</option>)}</select><button type="button" aria-label={`Move ${status.name} up`} disabled={mutation.pending || index === 0} onClick={() => reorder(index, -1)}>↑</button><button type="button" aria-label={`Move ${status.name} down`} disabled={mutation.pending || index === statuses.length - 1} onClick={() => reorder(index, 1)}>↓</button>{renaming === status.id ? <><input aria-label={`Rename ${status.name}`} value={renameValue} onChange={event => setRenameValue(event.target.value)} /><button type="button" disabled={mutation.pending || !text(renameValue)} onClick={() => rename(status)}>Save</button><button type="button" onClick={() => { setRenaming(null); renameRevision.current = null; }}>Cancel</button></> : <button type="button" onClick={() => { renameRevision.current = revisions; setRenaming(status.id); setRenameValue(status.name); }}>Rename</button>}<button type="button" className="planning-danger-link" disabled={mutation.pending} aria-label={`Delete ${status.name}`} onClick={() => run({ kind: 'action', action: 'workflow.status.delete', project: projectKey, data: { id: status.id } })}>Delete</button></div>}</li>)}</ul>{!readOnly && <div className="settings-create-row"><input aria-label="New status name" placeholder="New status name" value={newStatus.name} onChange={event => { if (!newStatusRevision.current) newStatusRevision.current = revisions; setNewStatus(current => ({ ...current, name: event.target.value })); }} /><select aria-label="New status category" value={newStatus.category} onChange={event => setNewStatus(current => ({ ...current, category: event.target.value }))}>{JIRA_STATUS_CATEGORIES.map(category => <option key={category} value={category}>{CATEGORY_LABELS[category] ?? category}</option>)}</select><button type="button" className="planning-primary" disabled={mutation.pending || !text(newStatus.name)} onClick={addStatus}>Add status</button></div>}</section>
    <section className="settings-card"><header><div><h3>Transition graph</h3><p>Choose the moves that are legal from each status. A checked cell is an existing transition.</p></div></header><div className="settings-table-wrap"><table className="settings-transition-table"><caption className="sr-only">Workflow transitions</caption><thead><tr><th>From \ To</th>{statuses.map(status => <th key={status.id}>{status.name}</th>)}</tr></thead><tbody>{statuses.map(from => <tr key={from.id}><th>{from.name}</th>{statuses.map(to => { const edge = edgeAt(from.id, to.id); return <td key={to.id}><button type="button" aria-label={`${edge ? 'Remove' : 'Add'} transition ${from.name} to ${to.name}`} aria-pressed={Boolean(edge)} disabled={readOnly || mutation.pending || from.id === to.id} onClick={() => edge ? removeTransition(edge) : addTransition(from.id, to.id)}>{edge ? '✓' : from.id === to.id ? '—' : '+'}</button></td>; })}</tr>)}<tr><th>From anywhere</th>{statuses.map(to => { const edge = transitions.find(item => item.fromStatusId === null && item.toStatusId === to.id); return <td key={to.id}><button type="button" aria-label={`${edge ? 'Remove' : 'Add'} global transition to ${to.name}`} aria-pressed={Boolean(edge)} disabled={readOnly || mutation.pending} onClick={() => edge ? removeTransition(edge) : addTransition(null, to.id)}>{edge ? '✓' : '+'}</button></td>; })}</tr></tbody></table></div></section>
    <section className="settings-card"><header><div><h3>Rules and transition screens</h3><p>Conditions decide who can move an issue; validators decide whether it is ready.</p></div></header><ul className="settings-transition-list">{transitions.map(transition => <li key={transitionId(transition)}><div className="settings-transition-heading"><strong>{transition.fromStatusId === null ? 'From anywhere' : statuses.find(status => status.id === transition.fromStatusId)?.name ?? transition.fromStatusId} → {statuses.find(status => status.id === transition.toStatusId)?.name ?? transition.toStatusId}</strong><span>{text(transition.name, 'Transition')}</span>{!readOnly && <button type="button" onClick={() => setOpenRules(openRules === transitionId(transition) ? null : transitionId(transition))}>{openRules === transitionId(transition) ? 'Close' : 'Edit rules'}</button>}</div>{array(transition.rules).length > 0 && <ul className="settings-rule-list">{transition.rules.map(rule => <li key={text(rule?.id, JSON.stringify(rule))}><span>{text(rule?.kind)} · {TRANSITION_RULE_LABELS[rule?.type] ?? text(rule?.type)}</span>{!readOnly && <button type="button" aria-label="Remove rule" onClick={() => removeRule(rule)}>×</button>}</li>)}</ul>}{!readOnly && openRules === transitionId(transition) && <RuleEditor transition={transition} screens={screens} pending={mutation.pending} onAdd={addRule} onScreen={setScreen} />}</li>)}</ul></section>
  </section>;
}

export const WorkflowEditor = WorkflowSettings;
