import React from 'react';
import {
  BUILTIN_FIELDS,
  BUILTIN_FIELD_LABELS,
  FIELD_REQUIREMENTS,
  JIRA_CUSTOM_FIELD_TYPES,
  JIRA_CUSTOM_FIELD_TYPE_LABELS,
  customFieldError,
  defaultRequirement,
  fieldRefKey as sharedFieldRefKey,
  fieldTakesOptions,
} from '../../shared/jira/fields.js';
import { JIRA_ISSUE_TYPES } from '../../shared/jira/constants.js';
import { array, projectKeyOf, projectRevisionMap, text, usePlanningMutation } from '../planning/helpers.js';

function customId(field) { return text(field?.id ?? field?.key); }
function fieldLabel(field) { return text(field?.name ?? field?.key); }
function requirementLabel(value) { return value === 'HIDDEN' ? 'Hidden' : value === 'REQUIRED' ? 'Required' : 'Optional'; }
function fieldRefKey(value) {
  const customFieldId = text(value?.customFieldId ?? value?.custom_field_id);
  const builtinField = text(value?.builtinField ?? value?.builtin_field);
  return sharedFieldRefKey({ customFieldId: customFieldId || null, builtinField: builtinField || null });
}

function fieldRows(project) {
  return [
    ...BUILTIN_FIELDS.map(name => ({ key: `builtin:${name}`, label: BUILTIN_FIELD_LABELS[name] ?? name, builtinField: name, customFieldId: null })),
    ...array(project?.customFields).map(field => ({ key: `custom:${customId(field)}`, label: fieldLabel(field), builtinField: null, customFieldId: customId(field), custom: field })),
  ];
}

function fieldConfiguration(project) {
  return array(project?.fieldConfiguration ?? project?.fieldConfig ?? project?.fields?.configuration);
}

function requirementFor(rules, issueType, row) {
  const found = rules.find(rule => text(rule?.issueType ?? rule?.issue_type).toUpperCase() === issueType && fieldRefKey(rule) === row.key);
  return text(found?.requirement, defaultRequirement(issueType, row));
}

function CustomFieldForm({ values, setValues, pending, onSave, onCancel }) {
  const takesOptions = fieldTakesOptions(values.type);
  return <div className="settings-create-panel" data-testid="settings-custom-field-form"><label>Key <input aria-label="Field key" value={values.key} onChange={event => setValues(current => ({ ...current, key: event.target.value }))} placeholder="customer-tier" /></label><label>Name <input aria-label="Field name" value={values.name} onChange={event => setValues(current => ({ ...current, name: event.target.value }))} placeholder="Customer tier" /></label><label>Type <select aria-label="Field type" value={values.type} onChange={event => setValues(current => ({ ...current, type: event.target.value, options: [] }))}>{JIRA_CUSTOM_FIELD_TYPES.map(type => <option key={type} value={type}>{JIRA_CUSTOM_FIELD_TYPE_LABELS[type] ?? type}</option>)}</select></label>{takesOptions && <label>Options <input aria-label="Field options" value={values.options.join(', ')} onChange={event => setValues(current => ({ ...current, options: event.target.value.split(',').map(item => item.trim()).filter(Boolean) }))} placeholder="Gold, Silver" /></label>}<div className="settings-inline-actions"><button type="button" className="planning-primary" disabled={pending || Boolean(customFieldError(values))} onClick={onSave}>Create field</button><button type="button" onClick={onCancel}>Cancel</button></div>{customFieldError(values) && <span role="alert" className="settings-inline-error">{customFieldError(values)}</span>}</div>;
}

function ScreenEditor({ screen, rows, pending, onSave }) {
  const initial = array(screen?.fields ?? screen?.fieldRefs).map(field => fieldRefKey(field));
  const [selected, setSelected] = React.useState(() => new Set(initial));
  const toggle = key => setSelected(current => { const next = new Set(current); if (next.has(key)) next.delete(key); else next.add(key); return next; });
  return <div className="settings-screen-fields" data-testid={`settings-screen-editor-${customId(screen)}`}><p>Fields shown on this screen</p><div>{rows.map(row => <label key={row.key}><input type="checkbox" checked={selected.has(row.key)} disabled={pending} aria-label={`${row.label} on ${text(screen?.name, 'screen')}`} onChange={() => toggle(row.key)} />{row.label}</label>)}</div><button type="button" className="planning-primary" disabled={pending} onClick={() => onSave(screen, [...selected].map(key => { const row = rows.find(item => item.key === key); return { builtinField: row?.builtinField ?? null, customFieldId: row?.customFieldId ?? null }; }))}>Save screen fields</button></div>;
}

export function FieldsSettings({ project, api, onChanged, readOnly = false }) {
  const projectKey = projectKeyOf(project);
  const revisions = projectRevisionMap(project);
  const rows = fieldRows(project);
  const rules = fieldConfiguration(project);
  const screens = array(project?.screens);
  const [creatingField, setCreatingField] = React.useState(false);
  const [fieldValues, setFieldValues] = React.useState({ key: '', name: '', type: 'TEXT', description: '', options: [] });
  const [screenName, setScreenName] = React.useState('');
  const [openScreen, setOpenScreen] = React.useState(null);
  const fieldDraftRevision = React.useRef(null);
  const screenDraftRevision = React.useRef(null);
  const mutation = usePlanningMutation(api, onChanged);
  const run = request => mutation.execute({ ...request, options: { ...(request.options ?? {}), revisions: { ...revisions, ...(request.options?.revisions ?? {}) } } }).catch(() => null);
  const setRequirement = (issueType, row, requirement) => run({ kind: 'action', action: 'field.requirement.set', project: projectKey, data: { issueType, builtinField: row.builtinField, customFieldId: row.customFieldId, requirement } });
  const createField = async () => { const definition = { key: text(fieldValues.key), name: text(fieldValues.name), type: fieldValues.type, description: text(fieldValues.description) || null, options: fieldValues.options }; if (customFieldError(definition)) return; const result = await run({ kind: 'action', action: 'field.create', project: projectKey, data: definition, options: { revisions: fieldDraftRevision.current ?? revisions } }); if (result) { setFieldValues({ key: '', name: '', type: 'TEXT', description: '', options: [] }); setCreatingField(false); fieldDraftRevision.current = null; } };
  const createScreen = async () => { const name = text(screenName); if (!name) return; const result = await run({ kind: 'action', action: 'screen.create', project: projectKey, data: { name, description: null, fields: [] }, options: { revisions: screenDraftRevision.current ?? revisions } }); if (result) { setScreenName(''); screenDraftRevision.current = null; } };
  const saveScreenFields = (screen, fields) => run({ kind: 'action', action: 'screen.fields.set', project: projectKey, data: { id: customId(screen), fields }, options: { revisions: screenDraftRevision.current ?? revisions } }).then(result => { if (result) screenDraftRevision.current = null; return result; });

  return <section className="settings-surface settings-fields" data-testid="settings-fields">
    <header className="settings-header"><div><p className="planning-eyebrow">Project settings</p><h2>Fields</h2><p className="planning-subtitle">Field visibility, required values, custom fields, and screens</p></div>{mutation.error && <span role="alert" className="planning-error">{mutation.error}</span>}</header>
    <section className="settings-card"><header><div><h3>Field configuration</h3><p>Set which fields each issue type shows and requires. Defaults stay visible when no override exists.</p></div>{!readOnly && <button type="button" onClick={() => { setCreatingField(current => { if (!current) fieldDraftRevision.current = revisions; else fieldDraftRevision.current = null; return !current; }); }}>{creatingField ? 'Cancel' : 'New custom field'}</button>}</header>{!readOnly && creatingField && <CustomFieldForm values={fieldValues} setValues={setFieldValues} pending={mutation.pending} onSave={createField} onCancel={() => { setCreatingField(false); fieldDraftRevision.current = null; }} />}<div className="settings-table-wrap"><table className="settings-field-table"><caption className="sr-only">Field requirement matrix</caption><thead><tr><th>Field</th>{JIRA_ISSUE_TYPES.map(type => <th key={type}>{type}</th>)}</tr></thead><tbody>{rows.map(row => <tr key={row.key}><th>{row.label}{row.custom && <small>{row.custom.key}</small>}</th>{JIRA_ISSUE_TYPES.map(type => <td key={type}><select aria-label={`${row.label} on ${type}`} value={requirementFor(rules, type, row)} disabled={readOnly || mutation.pending} onChange={event => setRequirement(type, row, event.target.value)}>{FIELD_REQUIREMENTS.map(value => <option key={value} value={value}>{requirementLabel(value)}</option>)}</select></td>)}</tr>)}</tbody></table></div></section>
    <section className="settings-card"><header><div><h3>Custom fields</h3><p>Definitions are reusable across issue types and screens.</p></div></header>{array(project?.customFields).length ? <ul className="settings-simple-list">{array(project.customFields).map(field => <li key={customId(field)}><div><strong>{fieldLabel(field)}</strong><span>{customId(field)} · {text(field?.type)}</span>{text(field?.description) && <small>{field.description}</small>}</div>{!readOnly && <button type="button" className="planning-danger-link" aria-label={`Delete ${fieldLabel(field)}`} disabled={mutation.pending} onClick={() => run({ kind: 'action', action: 'field.delete', project: projectKey, data: { id: customId(field) } })}>Delete</button>}</li>)}</ul> : <p className="planning-empty">No custom fields have been defined.</p>}</section>
    <section className="settings-card"><header><div><h3>Transition screens</h3><p>Define ordered field sets here; attach them to workflow transitions in Workflow.</p></div></header><ul className="settings-simple-list">{screens.map(screen => <li key={customId(screen)} className="settings-screen-row"><div><strong>{text(screen?.name, customId(screen))}</strong>{text(screen?.description) && <small>{screen.description}</small>}</div>{!readOnly && <div className="settings-row-actions"><button type="button" onClick={() => { const next = openScreen === customId(screen) ? null : customId(screen); if (next) screenDraftRevision.current = revisions; else screenDraftRevision.current = null; setOpenScreen(next); }}>{openScreen === customId(screen) ? 'Close' : 'Edit fields'}</button><button type="button" className="planning-danger-link" onClick={() => run({ kind: 'action', action: 'screen.delete', project: projectKey, data: { id: customId(screen) } })}>Delete</button></div>}{openScreen === customId(screen) && <ScreenEditor screen={screen} rows={rows} pending={mutation.pending} onSave={saveScreenFields} />}</li>)}</ul>{!readOnly && <div className="settings-create-row"><input aria-label="Screen name" placeholder="Screen name" value={screenName} onChange={event => { if (text(event.target.value) && !screenDraftRevision.current) screenDraftRevision.current = revisions; setScreenName(event.target.value); }} /><button type="button" className="planning-primary" disabled={mutation.pending || !text(screenName)} onClick={createScreen}>Create screen</button></div>}</section>
  </section>;
}

export const FieldsEditor = FieldsSettings;
