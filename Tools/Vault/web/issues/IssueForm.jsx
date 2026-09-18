import React, { useEffect, useMemo, useRef, useState } from 'react';

import {
  buildCreateCommand,
  buildParentCommand,
  buildSprintCommand,
  buildUpdateCommand,
  buildVersionCommand,
  buildActionRequest,
  catalogOptions,
  configuredIssueAction,
  customFieldDefinitions,
  customFieldErrors,
  customFieldValuesEqual,
  fieldRequired,
  fieldVisible,
  issueKeyOf,
  issueFieldLayout,
  issueActionData,
  makeOperationId,
  normalizeCustomFieldValues,
  normalizeIssue,
  optionEmail,
  optionKey,
  optionName,
  parentOptions,
  projectCatalog,
  projectReadOnly,
  legalStatusOptions,
  sendAction,
  sendCommand,
  statusOptions,
  workflowTransitionFor,
  ISSUE_TYPES,
  PRIORITIES,
  COMPLEXITIES,
  RISKS,
  STORY_POINTS,
  RESOLUTIONS,
  SEVERITIES,
  REPRODUCIBILITIES,
} from './model.js';
import { RawMarkdownEditor } from './RawMarkdownEditor.jsx';
import { MarkdownPreview } from './MarkdownPreview.jsx';

function valueOrEmpty(value) {
  return value === undefined || value === null ? '' : String(value);
}

function uniqueValues(values) {
  return [...new Set((Array.isArray(values) ? values : []).map(value => String(value).trim()).filter(Boolean))];
}

function catalogValue(values, value, fallback = '') {
  const text = valueOrEmpty(value);
  if (!text) return fallback;
  const match = (Array.isArray(values) ? values : []).find(option => {
    return [optionName(option), optionIdOf(option), optionKey(option), optionEmail(option)]
      .some(candidate => candidate && candidate.toLowerCase() === text.toLowerCase());
  });
  return match ? optionName(match) : text;
}

function optionIdOf(option) {
  if (option && typeof option === 'object') return String(option.id ?? option.value ?? option.key ?? option.name ?? '');
  return String(option ?? '');
}

export function draftFromIssue(issue, project) {
  const current = normalizeIssue(issue);
  const categories = catalogOptions(projectCatalog(project, 'categories'));
  const users = projectCatalog(project, 'users');
  const sprints = projectCatalog(project, 'sprints');
  const versions = projectCatalog(project, 'versions');
  return {
    key: current.key,
    title: current.title,
    body: current.body,
    type: current.type,
    status: current.status,
    priority: current.priority,
    complexity: current.complexity,
    risk: current.risk,
    storyPoints: current.storyPoints,
    resolution: valueOrEmpty(current.resolution),
    severity: valueOrEmpty(current.severity),
    reproducibility: valueOrEmpty(current.reproducibility),
    flagged: current.flagged,
    dueDate: current.dueDate ? String(current.dueDate).slice(0, 10) : '',
    environment: valueOrEmpty(current.environment),
    category: catalogValue(categories, current.category),
    assignee: catalogValue(users, current.assignee, ''),
    labels: uniqueValues(current.labels),
    fixVersions: uniqueValues(current.fixVersions).map(value => catalogValue(versions, value, value)),
    sprint: catalogValue(sprints, current.sprint, ''),
    parent: valueOrEmpty(current.parent),
    blocks: current.blocks.map(row => row.key),
    blockedBy: [],
    reason: '',
    customFieldValues: normalizeCustomFieldValues(issue),
  };
}

function initialCreateDraft(project) {
  const statuses = statusOptions(project);
  const customFieldValues = Object.fromEntries(
    customFieldDefinitions(project).map(field => [field.id, []]),
  );
  return {
    key: '',
    title: '',
    body: '',
    type: 'TASK',
    status: statuses[0]?.name ?? 'To Do',
    priority: 'MEDIUM',
    complexity: 'MODERATE',
    risk: 'LOW',
    storyPoints: null,
    resolution: '',
    severity: '',
    reproducibility: '',
    flagged: false,
    dueDate: '',
    environment: '',
    category: '',
    assignee: '',
    labels: [],
    fixVersions: [],
    sprint: '',
    parent: '',
    blocks: [],
    blockedBy: [],
    reason: '',
    customFieldValues,
  };
}

function fieldId(label) {
  return `issue-${String(label).toLowerCase().replace(/[^a-z0-9]+/g, '-')}`;
}

function Field({ label, children, hint }) {
  return (
    <div className="vault-issue-field">
      <label htmlFor={fieldId(label)} className="vault-issue-label">{label}</label>
      {children}
      {hint ? <small className="vault-issue-hint">{hint}</small> : null}
    </div>
  );
}

function SelectField({ label, ariaLabel = label, value, onChange, options, placeholder = 'None', disabled = false, getValue = optionName, getLabel = optionName, multiple = false, required = false }) {
  const id = fieldId(label);
  return (
    <Field label={label}>
      <select
        id={id}
        aria-label={ariaLabel}
        className="vault-issue-control"
        value={value}
        multiple={multiple}
        required={required}
        onChange={event => {
          if (!multiple) {
            onChange(event.target.value);
            return;
          }
          const selected = event.target.selectedOptions
            ? Array.from(event.target.selectedOptions).map(option => option.value)
            : (event.target.value ? [event.target.value] : []);
          onChange(selected);
        }}
        disabled={disabled}
      >
        {!multiple ? <option value="">{placeholder}</option> : null}
        {(options ?? []).map((option, index) => {
          const optionValue = String(getValue(option) ?? `option-${index + 1}`);
          return <option key={`${optionValue}-${index}`} value={optionValue}>{String(getLabel(option) ?? optionValue)}</option>;
        })}
      </select>
    </Field>
  );
}

function CustomFieldInput({ field, values = [], onChange, disabled }) {
  const custom = field?.custom;
  if (!custom) return null;
  const label = String(field.label ?? custom.name ?? custom.id);
  const required = field.requirement === 'REQUIRED';
  const id = `issue-custom-${String(custom.id).replace(/[^a-z0-9_-]+/gi, '-')}`;
  const single = values[0] ?? '';
  const setSingle = value => onChange(value === '' ? [] : [String(value)]);
  const options = (custom.options ?? []).map(optionName).filter(Boolean);
  const ariaLabel = required ? `${label} (required)` : label;

  return (
    <div className="vault-issue-field vault-issue-custom-field" data-testid={`custom-field-${custom.id}`}>
      {custom.type === 'CHECKBOX' ? (
        <label className="vault-issue-checkbox" htmlFor={id}>
          <input
            id={id}
            type="checkbox"
            aria-label={ariaLabel}
            checked={single === 'true'}
            onChange={event => onChange(event.target.checked ? ['true'] : ['false'])}
            disabled={disabled}
          />
          {label}{required ? ' *' : ''}
        </label>
      ) : (
        <label htmlFor={id} className="vault-issue-label">{label}{required ? ' *' : ''}</label>
      )}
      {custom.type === 'TEXTAREA' ? (
        <textarea id={id} aria-label={ariaLabel} className="vault-issue-control" rows={3} value={single} onChange={event => setSingle(event.target.value)} disabled={disabled} required={required} />
      ) : custom.type === 'SELECT' ? (
        <select id={id} aria-label={ariaLabel} className="vault-issue-control" value={single} onChange={event => setSingle(event.target.value)} disabled={disabled} required={required}>
          <option value="">None</option>
          {options.map(option => <option key={option} value={option}>{option}</option>)}
        </select>
      ) : custom.type === 'MULTI_SELECT' ? (
        <fieldset className="vault-issue-custom-options" aria-label={ariaLabel}>
          <legend className="vault-issue-sr-only">{ariaLabel}</legend>
          {options.map(option => (
            <label key={option} className="vault-issue-checkbox">
              <input type="checkbox" aria-label={`${label}: ${option}`} checked={values.includes(option)} onChange={event => onChange(event.target.checked ? [...values, option] : values.filter(value => value !== option))} disabled={disabled} />
              {option}
            </label>
          ))}
        </fieldset>
      ) : custom.type === 'CHECKBOX' ? null : (
        <input
          id={id}
          type={custom.type === 'NUMBER' ? 'number' : custom.type === 'DATE' ? 'date' : 'text'}
          aria-label={ariaLabel}
          className="vault-issue-control"
          value={single}
          onChange={event => setSingle(event.target.value)}
          disabled={disabled}
          required={required}
        />
      )}
    </div>
  );
}

/**
 * Shared create/edit issue form.  It deliberately keeps the body as a raw
 * string in a textarea; the parent Vault editor and this issue editor never
 * pass Markdown through a normalising serializer.
 */
export function IssueForm({
  issue = null,
  project,
  api,
  readOnly,
  onSaved,
  onCancel,
  onOpenIssue,
  submitLabel,
  className = '',
  initialValues = {},
}) {
  const editing = Boolean(issue);
  const canWrite = !projectReadOnly(project, readOnly);
  const createDraft = () => ({ ...initialCreateDraft(project), ...initialValues });
  const [draft, setDraft] = useState(() => editing ? draftFromIssue(issue, project) : createDraft());
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  const identity = editing ? issueKeyOf(issue) : String(project?.key ?? project?.project ?? 'create');
  const draftRef = useRef(draft);
  const savedFingerprint = useRef(JSON.stringify(draft));
  const identityRef = useRef(identity);
  const pendingOperations = useRef(null);
  // Once a user starts editing, this is the compare-and-set snapshot that
  // authorised the draft.  The shell may hand us a newer `issue` object after
  // a websocket refresh; submitting against that newer revision would let a
  // dirty draft overwrite somebody else's change.
  const baselineRef = useRef(editing ? { issue, project } : null);
  draftRef.current = draft;

  useEffect(() => {
    const next = editing ? draftFromIssue(issue, project) : createDraft();
    const nextFingerprint = JSON.stringify(next);
    // The project catalog is refreshed after websocket events. A new
    // project object must not erase an in-progress issue draft. Re-seed
    // only when this is another issue/create target or when the user has no
    // local changes relative to the last committed snapshot.
    if (identityRef.current !== identity) {
      identityRef.current = identity;
      savedFingerprint.current = nextFingerprint;
      baselineRef.current = editing ? { issue, project } : null;
      pendingOperations.current = null;
      setDraft(next);
      return;
    }
    if (JSON.stringify(draftRef.current) === savedFingerprint.current) {
      savedFingerprint.current = nextFingerprint;
      baselineRef.current = editing ? { issue, project } : null;
      pendingOperations.current = null;
      if (JSON.stringify(draftRef.current) !== nextFingerprint) setDraft(next);
    }
  }, [editing, identity, initialValues, issue, project]);

  const statuses = useMemo(() => {
    const base = editing ? legalStatusOptions(project, { ...issue, type: draft.type, status: draft.status }) : statusOptions(project);
    const legal = draft.type === 'EPIC' ? base.filter(option => option.name.toLowerCase() !== 'in progress') : base;
    if (draft.status && !legal.some(option => option.name === draft.status)) legal.push({ id: draft.status, name: draft.status, category: '', position: legal.length });
    return legal;
  }, [draft.status, draft.type, editing, issue, project]);
  const categories = useMemo(() => catalogOptions(projectCatalog(project, 'categories')), [project]);
  const users = useMemo(() => catalogOptions(projectCatalog(project, 'users')), [project]);
  const sprints = useMemo(() => catalogOptions(projectCatalog(project, 'sprints')), [project]);
  const versions = useMemo(() => catalogOptions(projectCatalog(project, 'versions')), [project]);
  const parents = useMemo(() => parentOptions({ ...project, currentIssue: issue ?? { key: draft.key, type: draft.type } }, draft.key), [project, issue, draft.key, draft.type]);
  const fieldLayout = useMemo(() => issueFieldLayout(project, draft.type), [project, draft.type]);
  const transition = useMemo(() => editing ? workflowTransitionFor(project, issue, draft.status) : null, [draft.status, editing, issue, project]);
  const transitionLayout = useMemo(() => transition?.needsScreen ? issueFieldLayout(project, draft.type, transition.transition.screenId) : [], [draft.type, project, transition]);
  const transitionCustomIds = useMemo(() => new Set(transitionLayout.filter(field => field.custom).map(field => field.custom.id)), [transitionLayout]);
  const customFields = useMemo(() => fieldLayout.filter(field => field.custom && !transitionCustomIds.has(field.custom.id)), [fieldLayout, transitionCustomIds]);
  const transitionCustomFields = useMemo(() => transitionLayout.filter(field => field.custom), [transitionLayout]);

  const set = (name, value) => setDraft(previous => ({ ...previous, [name]: value }));
  const setStatus = value => setDraft(previous => {
    const selected = statuses.find(option => option.name === value || option.id === value);
    const category = String(selected?.category ?? selected?.statusCategory ?? '').toUpperCase();
    return {
      ...previous,
      status: value,
      // A resolution describes a completed issue. Clearing it as soon as a
      // draft leaves a Done status keeps the atomic update compatible with
      // the writer's resolution invariant and avoids submitting a stale
      // resolution from the previous status.
      ...(category && category !== 'DONE' ? { resolution: '' } : {}),
    };
  });
  const setLabels = value => set('labels', String(value).split(',').map(item => item.trim()).filter(Boolean));
  const setPoints = value => set('storyPoints', value === '' ? null : Number(value));
  const setCustomField = (id, values) => setDraft(previous => ({
    ...previous,
    customFieldValues: { ...(previous.customFieldValues ?? {}), [id]: [...values] },
  }));

  function retainedCommand(signature, name, builder, prefix) {
    let pending = pendingOperations.current;
    if (!pending || pending.signature !== signature) {
      pending = { signature, commands: new Map() };
      pendingOperations.current = pending;
    }
    if (!pending.commands.has(name)) {
      pending.commands.set(name, { ...builder(), operationId: makeOperationId(prefix) });
    }
    return pending.commands.get(name);
  }

  async function submit(event) {
    event?.preventDefault?.();
    if (!canWrite || saving) return;
    const title = String(draft.title ?? '').trim();
    if (!title) {
      setError('A title is required.');
      return;
    }
    const builtinValues = {
      description: draft.body,
      assignee: draft.assignee,
      priority: draft.priority,
      complexity: draft.complexity,
      risk: draft.risk,
      storyPoints: draft.storyPoints,
      dueDate: draft.dueDate,
      category: draft.category,
      sprint: draft.sprint,
      flagged: draft.flagged,
      severity: draft.severity,
      reproducibility: draft.reproducibility,
      environment: draft.environment,
      labels: draft.labels,
      versions: draft.fixVersions,
      parent: draft.parent,
    };
    const requiredBuiltin = fieldLayout.find(field => {
      if (field.custom || field.requirement !== 'REQUIRED') return false;
      const value = builtinValues[field.ref?.builtinField];
      return Array.isArray(value) ? value.length === 0 : value === undefined || value === null || String(value).trim() === '';
    });
    if (requiredBuiltin) {
      setError(`${requiredBuiltin.label} is required.`);
      return;
    }
    const customErrors = customFieldErrors(project, draft.type, draft.customFieldValues ?? {});
    const firstCustomError = Object.values(customErrors)[0];
    if (firstCustomError) {
      setError(firstCustomError);
      return;
    }
    if (editing && transition?.needsResolution && !String(draft.resolution ?? '').trim()) {
      setError('Choose a resolution before moving this issue to a completed status.');
      return;
    }
    const requiredTransition = transitionCustomFields.find(field => field.requirement === 'REQUIRED' && (draft.customFieldValues?.[field.custom.id] ?? []).length === 0);
    if (requiredTransition) {
      setError(`${requiredTransition.label} is required for this transition.`);
      return;
    }
    setError('');
    setSaving(true);
    const intent = { ...draft, title };
    const signature = JSON.stringify({ editing, key: issueKeyOf(issue, draft.key), intent });
    try {
      if (!editing) {
        const createAction = configuredIssueAction(project, 'create');
        const projectKey = project?.key ?? project?.project ?? '';
        const selectedCustomValues = Object.values(draft.customFieldValues ?? {}).some(values => Array.isArray(values) ? values.length > 0 : values !== undefined && values !== null && values !== '');
        if (!createAction && (selectedCustomValues || (draft.fixVersions ?? []).length > 1)) {
          throw new Error('This Vault does not advertise an atomic create action for custom fields or multiple fix versions. Refresh the project before retrying.');
        }
        const response = createAction
          ? await sendAction(api, retainedCommand(
            signature,
            'create-action',
            () => buildActionRequest(createAction.action, projectKey, '', issueActionData({ ...draft, title }), draft.revisions ?? {}),
            'issue-create',
          ))
          : await sendCommand(api, retainedCommand(
            signature,
            'create',
            () => buildCreateCommand(projectKey, { ...draft, title }),
            'issue-create',
          ), 'issue-create');
        savedFingerprint.current = JSON.stringify({ ...draft, title });
        pendingOperations.current = null;
        onSaved?.(response, draft);
        return;
      }

      const baselineIssue = baselineRef.current?.issue ?? issue;
      const baselineProject = baselineRef.current?.project ?? project;
      const revisions = baselineIssue?.revisions ?? {};
      const current = draftFromIssue(baselineIssue, baselineProject);
      const changedParent = draft.parent !== current.parent;
      const changedSprint = draft.sprint !== current.sprint;
      const nextVersions = [...new Set((draft.fixVersions ?? []).map(value => String(value).trim()).filter(Boolean))];
      const currentVersions = [...new Set((current.fixVersions ?? []).map(value => String(value).trim()).filter(Boolean))];
      const changedVersions = JSON.stringify(nextVersions) !== JSON.stringify(currentVersions);
      const changedCustomFields = !customFieldValuesEqual(draft.customFieldValues ?? {}, current.customFieldValues ?? {});
      const updateAction = configuredIssueAction(project, 'update');
      if (!updateAction && draft.status !== current.status && transition?.needsScreen) {
        throw new Error('This Vault does not advertise an atomic transition action for this workflow screen. Refresh the project before retrying.');
      }
      if (!updateAction && (changedParent || changedSprint || changedCustomFields)) {
        throw new Error('This Vault does not advertise an atomic issue update action for parent, sprint, or custom fields. Refresh the project before retrying.');
      }

      const update = updateAction
        ? retainedCommand(
          signature,
          'update-action',
          () => buildActionRequest(updateAction.action, project?.key ?? project?.project ?? '', issueKeyOf(baselineIssue, draft.key), issueActionData({ ...draft, title }), revisions),
          'issue-update',
        )
        : retainedCommand(
          signature,
          'update',
          () => buildUpdateCommand(baselineIssue, { ...draft, title, revisions }),
          'issue-update',
        );
      // The shared writer owns validation and the revision check. The UI
      // sends all issue fields in one update so title/body/property edits
      // cannot appear half-saved to a watcher.
      const response = updateAction
        ? await sendAction(api, update)
        : await sendCommand(api, update, 'issue-update');
      let latest = response;
      const issueKey = issueKeyOf(baselineIssue, draft.key);
      if (!updateAction && changedParent) {
        latest = await sendCommand(api, retainedCommand(
          signature,
          `parent:${draft.parent || 'none'}`,
          () => buildParentCommand(issueKey, draft.parent || null, revisions),
          'issue-parent',
        ), 'issue-parent');
      }
      if (!updateAction && changedSprint) {
        latest = await sendCommand(api, retainedCommand(
          signature,
          `sprint:${draft.sprint || 'none'}`,
          () => buildSprintCommand(issueKey, draft.sprint || null, revisions),
          'issue-sprint',
        ), 'issue-sprint');
      }
      if (!updateAction && changedVersions) {
        // The legacy version command is set-like, so every edit is a clear
        // followed by one add per selected value (an empty selection is just
        // the clear). A project that advertises a dedicated action can
        // replace this sequence when its schema is wired by the backend.
        const versionValues = [null, ...nextVersions];
        for (const [index, version] of versionValues.entries()) {
          latest = await sendCommand(api, retainedCommand(
            signature,
            `version:${index}:${version || 'none'}`,
            () => buildVersionCommand(issueKey, version, revisions),
            'issue-version',
          ), 'issue-version');
        }
      }
      savedFingerprint.current = JSON.stringify({ ...draft, title });
      pendingOperations.current = null;
      onSaved?.(latest, draft);
    } catch (cause) {
      setError(cause?.message ?? String(cause));
    } finally {
      setSaving(false);
    }
  }

  return (
    <form className={`vault-issue-form ${className}`.trim()} onSubmit={submit} data-testid={editing ? 'issue-edit-form' : 'issue-create-form'}>
      <div className="vault-issue-form-grid">
        <div className="vault-issue-form-main">
          <Field label="Issue title">
            <input
              id={fieldId('Issue title')}
              aria-label="Issue title"
              className="vault-issue-control vault-issue-title-control"
              value={draft.title}
              onChange={event => set('title', event.target.value)}
              disabled={!canWrite || saving}
              required
            />
          </Field>
          {fieldVisible(fieldLayout, 'description') ? <Field label="Issue description">
            <RawMarkdownEditor
              value={draft.body}
              onChange={value => set('body', value)}
              disabled={!canWrite || saving}
              label="Issue description"
              testId="issue-description-editor"
            />
            <MarkdownPreview body={draft.body} references={issue?.references} onOpenIssue={onOpenIssue} label="Rendered issue description" />
          </Field> : null}
        </div>
        <aside className="vault-issue-form-side" aria-label="Issue properties">
          <SelectField label="Issue type" value={draft.type} onChange={value => set('type', value)} options={ISSUE_TYPES} disabled={!canWrite || saving} />
          <SelectField label="Issue status" value={draft.status} onChange={setStatus} options={statuses} disabled={!canWrite || saving} getValue={optionName} getLabel={option => optionName(option)} />
          {fieldVisible(fieldLayout, 'priority') ? <SelectField label="Priority" value={draft.priority} onChange={value => set('priority', value)} options={PRIORITIES} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'priority')} /> : null}
          {fieldVisible(fieldLayout, 'complexity') ? <SelectField label="Complexity" value={draft.complexity} onChange={value => set('complexity', value)} options={COMPLEXITIES} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'complexity')} /> : null}
          {fieldVisible(fieldLayout, 'risk') ? <SelectField label="Risk" value={draft.risk} onChange={value => set('risk', value)} options={RISKS} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'risk')} /> : null}
          {fieldVisible(fieldLayout, 'storyPoints') ? <SelectField label="Story points" value={draft.storyPoints === null ? '' : String(draft.storyPoints)} onChange={setPoints} options={STORY_POINTS} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'storyPoints')} /> : null}
          <SelectField label="Resolution" value={draft.resolution} onChange={value => set('resolution', value)} options={RESOLUTIONS} disabled={!canWrite || saving} />
          {fieldVisible(fieldLayout, 'severity') ? <SelectField label="Severity" value={draft.severity} onChange={value => set('severity', value)} options={SEVERITIES} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'severity')} /> : null}
          {fieldVisible(fieldLayout, 'reproducibility') ? <SelectField label="Reproducibility" value={draft.reproducibility} onChange={value => set('reproducibility', value)} options={REPRODUCIBILITIES} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'reproducibility')} /> : null}
          {fieldVisible(fieldLayout, 'dueDate') ? <Field label="Due date">
            <input id={fieldId('Due date')} aria-label="Due date" type="date" className="vault-issue-control" value={draft.dueDate} onChange={event => set('dueDate', event.target.value)} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'dueDate')} />
          </Field> : null}
          {fieldVisible(fieldLayout, 'environment') ? <Field label="Environment">
            <input id={fieldId('Environment')} aria-label="Environment" className="vault-issue-control" value={draft.environment} onChange={event => set('environment', event.target.value)} disabled={!canWrite || saving} required={fieldRequired(fieldLayout, 'environment')} />
          </Field> : null}
          {fieldVisible(fieldLayout, 'flagged') ? <label className="vault-issue-checkbox"><input type="checkbox" aria-label="Flagged" checked={Boolean(draft.flagged)} onChange={event => set('flagged', event.target.checked)} disabled={!canWrite || saving} /> Flagged</label> : null}
          {fieldVisible(fieldLayout, 'category') ? <SelectField label="Category" value={draft.category} onChange={value => set('category', value)} options={categories} disabled={!canWrite || saving} getValue={optionName} getLabel={optionName} required={fieldRequired(fieldLayout, 'category')} /> : null}
          {fieldVisible(fieldLayout, 'assignee') ? <SelectField label="Assignee" value={draft.assignee} onChange={value => set('assignee', value)} options={users} disabled={!canWrite || saving} getValue={optionEmail} getLabel={option => option.name ?? option.email ?? optionName(option)} required={fieldRequired(fieldLayout, 'assignee')} /> : null}
          {fieldVisible(fieldLayout, 'labels') ? <Field label="Labels" hint="Separate labels with commas.">
            <input
              id={fieldId('Labels')}
              aria-label="Labels"
              className="vault-issue-control"
              value={draft.labels.join(', ')}
              onChange={event => setLabels(event.target.value)}
              disabled={!canWrite || saving}
              required={fieldRequired(fieldLayout, 'labels')}
            />
          </Field> : null}
          {fieldVisible(fieldLayout, 'versions') ? <SelectField label="Fix versions" ariaLabel="Fix version" value={draft.fixVersions} onChange={value => set('fixVersions', value)} options={versions} disabled={!canWrite || saving} getValue={optionName} getLabel={option => option.name ?? optionName(option)} multiple /> : null}
          {fieldVisible(fieldLayout, 'sprint') ? <SelectField label="Sprint" value={draft.sprint} onChange={value => set('sprint', value)} options={sprints} disabled={!canWrite || saving} getValue={optionName} getLabel={option => option.name ?? optionName(option)} required={fieldRequired(fieldLayout, 'sprint')} /> : null}
          {fieldVisible(fieldLayout, 'parent') ? <SelectField label="Parent issue" value={draft.parent} onChange={value => set('parent', value)} options={parents} disabled={!canWrite || saving} getValue={optionKey} getLabel={option => `${optionKey(option)}${option.title ? ` — ${option.title}` : ''}`} required={fieldRequired(fieldLayout, 'parent')} /> : null}
          {customFields.length ? (
            <div className="vault-issue-custom-fields" aria-label="Custom fields">
              <h3 className="vault-issue-subheading">Custom fields</h3>
              {customFields.map(field => <CustomFieldInput key={field.custom.id} field={field} values={draft.customFieldValues?.[field.custom.id] ?? []} onChange={values => setCustomField(field.custom.id, values)} disabled={!canWrite || saving} />)}
            </div>
          ) : null}
          {transitionCustomFields.length ? (
            <div className="vault-issue-custom-fields vault-issue-transition-screen" data-testid="issue-transition-screen" aria-label="Transition fields">
              <h3 className="vault-issue-subheading">Transition fields</h3>
              <p className="vault-issue-hint">Complete the fields required by this status transition.</p>
              {transitionCustomFields.map(field => <CustomFieldInput key={`transition-${field.custom.id}`} field={field} values={draft.customFieldValues?.[field.custom.id] ?? []} onChange={values => setCustomField(field.custom.id, values)} disabled={!canWrite || saving} />)}
            </div>
          ) : null}
        </aside>
      </div>
      {error ? <p className="vault-issue-error" role="alert">{error}</p> : null}
      {!canWrite ? <p className="vault-issue-readonly">This project is read-only.</p> : null}
      <div className="vault-issue-form-actions">
        {onCancel ? <button type="button" className="vault-issue-button" onClick={onCancel} disabled={saving}>Cancel</button> : null}
        {canWrite ? <button type="submit" className="vault-issue-button vault-issue-button-primary" disabled={saving}>{saving ? 'Saving…' : (submitLabel ?? (editing ? 'Save issue' : 'Create issue'))}</button> : null}
      </div>
    </form>
  );
}

export function CreateIssueForm({ project, api, onCreated, onCancel, readOnly, initialValues, onOpenIssue }) {
  const created = (response, draft) => onCreated?.(response?.result ?? response?.issue ?? response, draft);
  return <IssueForm project={project} api={api} readOnly={readOnly} onSaved={created} onCancel={onCancel} onOpenIssue={onOpenIssue} submitLabel="Create issue" initialValues={initialValues} />;
}

export function IssueEditForm({ issue, project, api, onSaved, onCancel, readOnly, onOpenIssue }) {
  return <IssueForm issue={issue} project={project} api={api} readOnly={readOnly} onSaved={onSaved} onCancel={onCancel} onOpenIssue={onOpenIssue} submitLabel="Save issue" />;
}
