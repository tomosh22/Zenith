import { describe, expect, it, vi } from 'vitest';

import {
  buildActionRequest,
  buildCommentCommand,
  buildCreateCommand,
  buildLinkCommand,
  buildParentCommand,
  buildSprintCommand,
  buildUnlinkCommand,
  buildUpdateCommand,
  buildVersionCommand,
  callApi,
  catalogOptions,
  configuredIssueAction,
  customFieldDefinitions,
  customFieldValuesEqual,
  fieldConfigurationRules,
  fieldRequired,
  fieldVisible,
  issueActionData,
  issueKeyOf,
  issueMetadata,
  issueRevisions,
  issueValue,
  legalStatusOptions,
  linkLabel,
  linkTypeFor,
  makeOperationId,
  normalizeCustomFieldValues,
  normalizeIssue,
  optionEmail,
  optionId,
  optionKey,
  optionName,
  parentOptions,
  projectCatalog,
  projectReadOnly,
  relationSource,
  relationTarget,
  screenFieldReferences,
  sendAction,
  sendCommand,
  statusOptions,
  workflowTransitionFor,
} from '../web/issues/model.js';

describe('option helpers', () => {
  it('walk each alias chain and fall back for plain values', () => {
    expect(optionId({ id: 'a' })).toBe('a');
    expect(optionId({ key: 'k' })).toBe('k');
    expect(optionId({ slug: 's' })).toBe('s');
    expect(optionId({ value: 'v' })).toBe('v');
    expect(optionId({ name: 'n' })).toBe('n');
    expect(optionId(null, 'fallback')).toBe('fallback');
    expect(optionId('plain')).toBe('plain');
    expect(optionName({ name: 'n' })).toBe('n');
    expect(optionName({ label: 'l' })).toBe('l');
    expect(optionName({ title: 't' })).toBe('t');
    expect(optionName({ value: 'v' })).toBe('v');
    expect(optionName({ key: 'k' })).toBe('k');
    expect(optionName({ slug: 's' })).toBe('s');
    expect(optionName({ id: 'i' })).toBe('i');
    expect(optionName(undefined, 'Fallback')).toBe('Fallback');
    expect(optionEmail({ email: 'e' })).toBe('e');
    expect(optionEmail({ username: 'u' })).toBe('u');
    expect(optionEmail({ name: 'n' })).toBe('n');
    expect(optionEmail({ id: 'i' })).toBe('i');
    expect(optionEmail('ada@example.test')).toBe('ada@example.test');
    expect(optionKey({ key: 'k' })).toBe('k');
    expect(optionKey({ issueKey: 'ik' })).toBe('ik');
    expect(optionKey({ name: 'n' })).toBe('n');
    expect(optionKey({ slug: 's' })).toBe('s');
    expect(optionKey({ id: 'i' })).toBe('i');
    expect(optionKey(7)).toBe('7');
  });

  it('catalogOptions normalises rows and strings with positional fallbacks', () => {
    expect(catalogOptions([{ name: 'One' }, 'two', null])).toEqual([
      { id: 'One', name: 'One' },
      { id: 'two', name: 'two' },
      { id: 'option-3', name: 'Option 3' },
    ]);
  });
});

describe('custom field definitions and configuration rules', () => {
  it('reads custom fields from every legacy location', () => {
    const fields = [{ key: 'cf-1', label: 'Team', fieldType: 'select', values: [{ name: 'A' }, 'B'] }];
    expect(customFieldDefinitions({ customFields: fields })[0]).toMatchObject({ id: 'cf-1', name: 'Team', type: 'SELECT', options: ['A', 'B'] });
    expect(customFieldDefinitions({ fields: { customFields: fields } })).toHaveLength(1);
    expect(customFieldDefinitions({ fieldConfiguration: { customFields: fields } })).toHaveLength(1);
    expect(customFieldDefinitions({})).toEqual([]);
    // A row with no id/key/slug still gets a positional id.
    expect(customFieldDefinitions({ customFields: [{ name: 'X' }] })[0].id).toBe('custom-field-1');
  });

  it('normalises field rules from arrays, rules/fields bags, and per-type maps', () => {
    const arrayForm = fieldConfigurationRules({ fieldConfiguration: [{ builtinField: 'assignee', requirement: 'required', issue_type: 'BUG' }] });
    expect(arrayForm[0]).toMatchObject({ builtinField: 'assignee', customFieldId: null, requirement: 'REQUIRED', issueType: 'BUG' });
    expect(fieldConfigurationRules({ fieldConfig: { rules: [{ customFieldId: 'cf-1' }] } })[0]).toMatchObject({ customFieldId: 'cf-1', requirement: 'OPTIONAL' });
    expect(fieldConfigurationRules({ fields: { fieldConfiguration: { fields: [{ builtin: 'priority' }] } } })[0]).toMatchObject({ builtinField: 'priority' });
    const perType = fieldConfigurationRules({
      fieldConfiguration: {
        BUG: [{ builtinField: 'severity', requirement: 'REQUIRED' }],
        TASK: { builtin: { dueDate: 'HIDDEN' }, custom: { 'cf-9': 'REQUIRED' } },
        customFields: 'skipped',
      },
    });
    expect(perType).toEqual(expect.arrayContaining([
      expect.objectContaining({ issueType: 'BUG', builtinField: 'severity', requirement: 'REQUIRED' }),
      expect.objectContaining({ issueType: 'TASK', builtinField: 'dueDate', requirement: 'HIDDEN' }),
      expect.objectContaining({ issueType: 'TASK', customFieldId: 'cf-9', requirement: 'REQUIRED' }),
    ]));
    // String and object field references both resolve, and a string under
    // `field` gets the same `custom:` prefix split as a top-level string.
    const refs = fieldConfigurationRules({ fieldConfiguration: [{ field: 'custom:cf-2' }, { field: 'priority' }, { ref: { id: 'cf-3' } }] });
    expect(refs[0]).toMatchObject({ builtinField: null, customFieldId: 'cf-2' });
    expect(refs[1]).toMatchObject({ builtinField: 'priority', customFieldId: null });
    expect(refs[2]).toMatchObject({ builtinField: null, customFieldId: 'cf-3' });
  });

  it('screenFieldReferences resolves by id and filters empty references', () => {
    const project = { screens: [{ id: 's1', fields: ['assignee', { customFieldId: 'cf-1' }, { builtinField: null, customFieldId: null }] }] };
    expect(screenFieldReferences(project, undefined)).toBeUndefined();
    expect(screenFieldReferences(project, 's1')).toEqual([
      { builtinField: 'assignee', customFieldId: null },
      { builtinField: null, customFieldId: 'cf-1' },
    ]);
    expect(screenFieldReferences(project, { fields: ['priority'] })).toEqual([{ builtinField: 'priority', customFieldId: null }]);
  });
});

describe('normalizeCustomFieldValues', () => {
  it('converts row lists and plain objects into id -> string[] maps', () => {
    const rowList = normalizeCustomFieldValues({
      customFieldValues: [
        { customFieldId: 'cf-1', values: [{ name: 'A' }, 'B'] },
        { fieldId: 'cf-2', value: 'single' },
        { id: 'cf-3', selected: null },
        { key: 'cf-4' },
        { customFieldId: 'cf-5', values: [] },
        { customFieldId: 'cf-6', values: '' },
        { values: 'orphan' },
      ],
    });
    expect(rowList).toEqual({
      'cf-1': ['A', 'B'],
      'cf-2': ['single'],
      // Null, missing, empty-string and empty-array values all normalise to
      // an empty list rather than a wrapped empty option name.
      'cf-3': [],
      'cf-4': [],
      'cf-5': [],
      'cf-6': [],
      // A row with no id key is skipped entirely.
    });
    const objectForm = normalizeCustomFieldValues({ customFields: { 'cf-9': { values: ['X'] }, 'cf-10': 'Y', 'cf-11': null, 'cf-12': { values: [] } } });
    expect(objectForm).toEqual({ 'cf-9': ['X'], 'cf-10': ['Y'], 'cf-11': [], 'cf-12': [] });
    expect(normalizeCustomFieldValues({})).toEqual({});
  });

  it('customFieldValuesEqual compares normalised shapes', () => {
    expect(customFieldValuesEqual({ 'cf-1': ['A'] }, [{ customFieldId: 'cf-1', values: ['A'] }])).toBe(true);
    expect(customFieldValuesEqual({ 'cf-1': ['A'] }, { 'cf-1': ['B'] })).toBe(false);
  });
});

describe('normalizeIssue', () => {
  it('applies defaults, alias chains, and relation shapes', () => {
    const issue = normalizeIssue({
      metadata: { key: 'ZEN-1', issueType: 'BUG', story_points: 'abc', flag: 1, due_date: '2026-01-01', repro: 'RARELY' },
      description: 'Body text',
      labels: ['ui', { name: 'api' }, ''],
      blocks: ['ZEN-2', { target: 'ZEN-3', why: 'needs' }, {}],
      links: [9, { ticket: 'ZEN-4', type: 'causes', reason: 'r' }],
      fix_versions: ['1.0', { name: '2.0' }],
      assigneeEmail: 'ada@example.test',
      sprintId: 's1',
      parentId: 'ZEN-9',
      categoryName: 'Product',
    });
    expect(issue).toMatchObject({
      key: 'ZEN-1',
      path: null,
      title: 'ZEN-1',
      body: 'Body text',
      type: 'BUG',
      status: '',
      priority: 'MEDIUM',
      complexity: 'MODERATE',
      risk: 'LOW',
      storyPoints: null,
      flagged: true,
      dueDate: '2026-01-01',
      reproducibility: 'RARELY',
      category: 'Product',
      assignee: 'ada@example.test',
      sprint: 's1',
      parent: 'ZEN-9',
      labels: ['ui', 'api'],
      fixVersions: ['1.0', '2.0'],
      blocks: [
        { key: 'ZEN-2', reason: null },
        { target: 'ZEN-3', key: 'ZEN-3', reason: 'needs' },
      ],
      links: [
        { key: '9', type: 'RELATES_TO', reason: null },
        { ticket: 'ZEN-4', key: 'ZEN-4', type: 'CAUSES', reason: 'r' },
      ],
    });
    expect(issueMetadata(issue)).toMatchObject({ key: 'ZEN-1' });
    // Numeric points parse; empty points stay null.
    expect(normalizeIssue({ storyPoints: 5 }).storyPoints).toBe(5);
    expect(normalizeIssue({ storyPoints: '' }).storyPoints).toBeNull();
    expect(normalizeIssue({ storyPoints: null }).storyPoints).toBeNull();
    expect(normalizeIssue(null, 'ZEN-2').key).toBe('ZEN-2');
  });

  it('issueValue prefers the issue body over metadata and walks aliases', () => {
    expect(issueValue({ key: 'top', metadata: { key: 'meta' } }, 'key')).toBe('top');
    expect(issueValue({ metadata: { key: 'meta' } }, 'key')).toBe('meta');
    expect(issueValue({ issueKey: 'alias' }, 'key', ['issueKey'])).toBe('alias');
    expect(issueValue(null, 'key')).toBeUndefined();
    expect(issueKeyOf({ metadata: { issueKey: 'ZEN-9' } }, 'fallback')).toBe('ZEN-9');
    expect(issueKeyOf(null, 'fallback')).toBe('fallback');
  });

  it('issueRevisions merges the detail revision under the issue path', () => {
    const detail = {
      revisions: { 'other.md': { hash: 'a' } },
      issue: { path: 'ZEN/tickets/ZEN-1.md', revision: { hash: 'b' } },
    };
    expect(issueRevisions(detail)).toEqual({
      'other.md': { hash: 'a' },
      'ZEN/tickets/ZEN-1.md': { hash: 'b' },
    });
    // An existing revision for the path is never overwritten.
    expect(issueRevisions({ revisions: { 'ZEN/tickets/ZEN-1.md': { hash: 'kept' } }, issue: detail.issue })['ZEN/tickets/ZEN-1.md']).toEqual({ hash: 'kept' });
  });
});

describe('command builders', () => {
  it('buildCreateCommand spells none for cleared optional fields and dedupes versions', () => {
    const command = buildCreateCommand(' ZEN ', {
      title: '  T  ', resolution: null, dueDate: null, assignee: null, sprint: null, parent: null,
      labels: [' a ', '', 'b'], blocks: ['ZEN-2'], blockedBy: ['ZEN-3'], reason: ' why ',
      fixVersions: ['1.0', '1.0', ''], customFieldValues: { 'cf-1': ['A'] }, body: 'Body',
    });
    expect(command.args).toEqual(expect.arrayContaining([
      'create', '--project', 'ZEN', '--title', 'T', '--resolution', 'none', '--due', 'none',
      '--assignee', 'none', '--sprint', 'none', '--parent', 'none', '--label', 'a,b',
      '--blocks', 'ZEN-2', '--blocked-by', 'ZEN-3', '--reason', 'why', '--fix-version', '1.0',
    ]));
    expect(command.versions).toEqual(['1.0']);
    expect(command.files).toEqual({ file: 'Body' });
    expect(command.customFieldValues).toEqual({ 'cf-1': ['A'] });
    // Undefined options are omitted entirely; flagged adds --flag.
    const minimal = buildCreateCommand('ZEN', { title: 'M', flagged: true, environment: 'CI' });
    expect(minimal.args).toContain('--flag');
    expect(minimal.args).toEqual(expect.arrayContaining(['--environment', 'CI']));
    expect(minimal.args).not.toContain('--type');
  });

  it('buildUpdateCommand toggles the flag explicitly and computes unlabel', () => {
    const current = normalizeIssue({ key: 'ZEN-1', labels: ['ui', 'old'], flagged: false });
    const update = buildUpdateCommand(current, { labels: ['ui', 'new'], flagged: true, resolution: null, dueDate: null, category: null, assignee: null });
    expect(update.args).toEqual(expect.arrayContaining(['--flag', '--resolution', 'none', '--due', 'none', '--category', 'none', '--assignee', 'none', '--label', 'ui,new', '--unlabel', 'old']));
    expect(update.version).toBeNull();
    // Clearing the flag emits --flag --clear.
    const flagged = normalizeIssue({ key: 'ZEN-1', flagged: true });
    const cleared = buildUpdateCommand(flagged, { flagged: false });
    expect(cleared.args).toEqual(expect.arrayContaining(['--flag', '--clear']));
    // An unchanged flag emits nothing.
    const untouched = buildUpdateCommand(flagged, { flagged: true });
    expect(untouched.args).not.toContain('--flag');
    expect(untouched.versions).toEqual([]);
  });

  it('small builders keep the CLI spellings including the none sentinel', () => {
    expect(buildCommentCommand('ZEN-1', 'text', { 'a.md': {} })).toEqual({
      args: ['comment', 'ZEN-1', '--file', 'comment.md'],
      files: { file: 'text' },
      revisions: { 'a.md': {} },
    });
    expect(buildUnlinkCommand('ZEN-1', 'BLOCKS', ' ZEN-2 ')).toMatchObject({ args: ['unlink', 'ZEN-1', 'blocks', 'ZEN-2'] });
    expect(buildParentCommand('ZEN-1', null)).toMatchObject({ args: ['parent', 'ZEN-1', 'none'] });
    expect(buildParentCommand('ZEN-1', 'ZEN-9')).toMatchObject({ args: ['parent', 'ZEN-1', 'ZEN-9'] });
    expect(buildSprintCommand('ZEN-1', 'Sprint 1')).toMatchObject({ args: ['sprint', 'add', 'Sprint 1', 'ZEN-1'] });
    expect(buildSprintCommand('ZEN-1', null)).toMatchObject({ args: ['sprint', 'remove', 'none', 'ZEN-1'] });
    expect(buildVersionCommand('ZEN-1', '1.0')).toMatchObject({ args: ['version', 'set', '1.0', 'ZEN-1'] });
    expect(buildVersionCommand('ZEN-1', '')).toMatchObject({ args: ['version', 'set', 'none', 'ZEN-1'] });
    expect(buildLinkCommand('ZEN-1', 'BLOCKS', 'ZEN-2', '  ')).toMatchObject({ args: ['link', 'ZEN-1', 'blocks', 'ZEN-2'] });
    expect(() => buildLinkCommand('ZEN-1', 'relates-to', 'ZEN-1')).toThrow(/itself/i);
  });
});

describe('link and relation helpers', () => {
  it('linkTypeFor maps known types and defaults to relates-to', () => {
    expect(linkTypeFor('BLOCKS')).toBe('blocks');
    expect(linkTypeFor('duplicates')).toBe('duplicates');
    expect(linkTypeFor('CAUSES')).toBe('causes');
    expect(linkTypeFor('RELATES_TO')).toBe('relates-to');
    expect(linkTypeFor(undefined)).toBe('relates-to');
  });

  it('linkLabel resolves outgoing, incoming, and unknown types', () => {
    expect(linkLabel('BLOCKS')).toBe('blocks');
    expect(linkLabel('BLOCKS', 'incoming')).toBe('is blocked by');
    expect(linkLabel('DUPLICATES', 'incoming')).toBe('is duplicated by');
    expect(linkLabel('CAUSES', 'incoming')).toBe('is caused by');
    expect(linkLabel('UNKNOWN_TYPE')).toBe('relates to');
    expect(linkLabel(null)).toBe('relates to');
  });

  it('relationTarget and relationSource read every alias and direction', () => {
    expect(relationTarget({ key: 'k' })).toBe('k');
    expect(relationTarget({ targetKey: 'tk' })).toBe('tk');
    expect(relationTarget({ target: 't' })).toBe('t');
    expect(relationTarget({ ticketKey: 'tick' })).toBe('tick');
    expect(relationTarget({ issueKey: 'ik' })).toBe('ik');
    expect(relationTarget({ sourceKey: 'sk' }, 'incoming')).toBe('sk');
    expect(relationTarget({})).toBe('');
    expect(relationSource({ sourceKey: 'sk' })).toBe('sk');
    expect(relationSource({ source: 's' })).toBe('s');
    expect(relationSource({ key: 'k' }, 'incoming')).toBe('k');
    expect(relationSource({}, 'outgoing', 'ZEN-1')).toBe('ZEN-1');
  });
});

describe('status and parent options', () => {
  const project = {
    statuses: [{ id: 'todo', name: 'To Do', category: 'TODO' }, 'Done'],
    workflow: { statuses: [{ id: 'w1', name: 'Workflow status' }] },
  };

  it('statusOptions prefers the status list and falls back to the workflow catalog', () => {
    const options = statusOptions(project);
    expect(options[0]).toMatchObject({ id: 'todo', name: 'To Do', category: 'TODO', position: 0 });
    expect(options[1]).toMatchObject({ id: 'Done', name: 'Done', position: 1 });
    const fromWorkflow = statusOptions({ workflow: { statuses: [{ id: 'w1', name: 'Workflow status' }] } });
    expect(fromWorkflow[0]).toMatchObject({ id: 'w1', name: 'Workflow status', position: 0 });
    expect(statusOptions({}).length).toBe(0);
  });

  it('legalStatusOptions keeps the current status and filters EPIC In Progress', () => {
    const options = legalStatusOptions(project, { type: 'EPIC', status: 'To Do' });
    expect(options.map(option => option.name)).toEqual(['To Do', 'Done']);
    // No draft: every status is legal.
    expect(legalStatusOptions(project, null)).toHaveLength(2);
  });

  it('parentOptions filters by hierarchy and excludes the current issue', () => {
    const tickets = [
      { key: 'ZEN-1', type: 'EPIC' },
      { key: 'ZEN-2', type: 'TASK' },
      { key: 'ZEN-3', type: 'SUBTASK' },
      { type: 'EPIC' },
    ];
    // Without a selected issue only epics are offered (the current key is
    // always excluded, so pick one that is not itself an epic).
    expect(parentOptions({ tickets }, 'ZEN-9').map(ticket => ticket.key)).toEqual(['ZEN-1']);
    // A known child type consults the hierarchy: a task may only sit under an
    // epic, and the current issue is excluded by key.
    const withCurrent = { ...project, currentIssue: { type: 'TASK' }, tickets: tickets.slice(0, 3) };
    expect(parentOptions(withCurrent, 'ZEN-2').map(ticket => ticket.key)).toEqual(['ZEN-1']);
    // Unknown types stay selectable without a hierarchy verdict.
    const unknownChild = { ...withCurrent, currentIssue: { type: 'MYSTERY' } };
    expect(parentOptions(unknownChild, 'ZEN-2').map(ticket => ticket.key)).toEqual(['ZEN-1', 'ZEN-3']);
  });
});

describe('workflowTransitionFor', () => {
  const project = {
    statuses: [
      { id: 'todo', name: 'To Do', category: 'TODO' },
      { id: 'done', name: 'Done', category: 'DONE' },
    ],
    workflow: {
      transitions: [
        { id: 't1', from: null, to: 'done', rules: [{ kind: 'POST_FUNCTION', type: 'SET_RESOLUTION' }], screenId: 'screen-1' },
      ],
    },
  };

  it('resolves the matching transition with resolution/screen needs', () => {
    const result = workflowTransitionFor(project, { status: 'To Do' }, 'Done');
    expect(result).toMatchObject({
      transition: { id: 't1' },
      target: { id: 'done' },
      needsResolution: false,
      needsScreen: true,
    });
  });

  it('returns null when either status or the transition table is missing', () => {
    expect(workflowTransitionFor(project, { status: 'Unknown' }, 'Done')).toBeNull();
    expect(workflowTransitionFor(project, { status: 'To Do' }, 'Unknown')).toBeNull();
    expect(workflowTransitionFor({}, { status: 'To Do' }, 'Done')).toBeNull();
  });

  it('flags a missing SET_RESOLUTION post function on a Done transition', () => {
    const bare = {
      statuses: project.statuses,
      workflow: { transitions: [{ id: 't2', from: null, to: 'done', rules: [] }] },
    };
    expect(workflowTransitionFor(bare, { status: 'To Do' }, 'Done').needsResolution).toBe(true);
  });
});

describe('project helpers', () => {
  it('projectReadOnly reads the explicit flag then the project and board', () => {
    expect(projectReadOnly({}, true)).toBe(true);
    expect(projectReadOnly({ readOnly: true })).toBe(true);
    expect(projectReadOnly({ board: { readOnly: true } })).toBe(true);
    expect(projectReadOnly({}, false)).toBe(false);
    expect(projectReadOnly({})).toBe(false);
  });

  it('projectCatalog walks the alias table', () => {
    expect(projectCatalog({ users: [{ id: 'u1' }] }, 'users')).toEqual([{ id: 'u1' }]);
    expect(projectCatalog({ members: [{ id: 'u2' }] }, 'users')).toEqual([{ id: 'u2' }]);
    expect(projectCatalog({ labelCatalog: ['bug'] }, 'labels')).toEqual(['bug']);
    expect(projectCatalog({ categoryCatalog: ['Product'] }, 'categories')).toEqual(['Product']);
    expect(projectCatalog({ fixVersions: ['1.0'] }, 'versions')).toEqual(['1.0']);
    expect(projectCatalog({ sprintCatalog: ['S1'] }, 'sprints')).toEqual(['S1']);
    expect(projectCatalog({ issues: [] }, 'tickets')).toEqual([]);
    expect(projectCatalog({}, 'tickets')).toEqual([]);
  });
});

describe('configuredIssueAction', () => {
  it('resolves string and object aliases and refuses unnamed objects', () => {
    expect(configuredIssueAction({ actions: { create: 'issue.create' } }, 'create')).toEqual({ action: 'issue.create' });
    expect(configuredIssueAction({ actionSchemas: { 'issue.update': { action: 'issue.update', revisions: true } } }, 'update'))
      .toEqual({ action: 'issue.update', revisions: true });
    expect(configuredIssueAction({ issueActions: { deleteIssue: { name: 'issue.delete' } } }, 'delete'))
      .toEqual({ name: 'issue.delete', action: 'issue.delete' });
    expect(configuredIssueAction({ actions: { createComment: {} } }, 'createComment')).toBeNull();
    expect(configuredIssueAction({}, 'transition')).toBeNull();
    expect(configuredIssueAction({ actions: { customFieldValues: 'x' } }, 'customFields')).toEqual({ action: 'x' });
    expect(configuredIssueAction({ actions: { versions: 'issue.fixVersions' } }, 'fixVersions')).toEqual({ action: 'issue.fixVersions' });
  });
});

describe('action envelopes and transport', () => {
  it('buildActionRequest trims names and generates a derived operation id', () => {
    const request = buildActionRequest(' comment.create ', ' ZEN ', ' ZEN-1 ', { body: 'x' }, { 'a.md': {} });
    expect(request).toMatchObject({ action: 'comment.create', project: 'ZEN', key: 'ZEN-1', data: { body: 'x' } });
    expect(request.operationId).toMatch(/^issue-action-comment-create-/);
    expect(buildActionRequest('stable', 'ZEN', 'ZEN-1', {}, {}, 'fixed-id').operationId).toBe('fixed-id');
    expect(() => buildActionRequest('  ', 'ZEN', 'ZEN-1')).toThrow(/not available/);
    // Non-object data and revisions are coerced to empty objects.
    expect(buildActionRequest('a', 'ZEN', 'ZEN-1', 'nope', 5).data).toEqual({});
  });

  it('makeOperationId prefixes a uuid with a timestamp fallback', () => {
    expect(makeOperationId('op')).toMatch(/^op-[a-f0-9-]{36}$/);
    const original = globalThis.crypto;
    Object.defineProperty(globalThis, 'crypto', { value: {}, configurable: true });
    expect(makeOperationId('op')).toMatch(/^op-\w+-\w+$/);
    Object.defineProperty(globalThis, 'crypto', { value: original, configurable: true });
  });

  it('callApi unwraps json responses and surfaces structured errors', async () => {
    await expect(callApi('not-a-function')).rejects.toThrow(/API callback is required/);
    const ok = { ok: true, status: 200, json: async () => ({ value: 1 }) };
    await expect(callApi(() => ok, '/api/x')).resolves.toEqual({ value: 1 });
    const bad = { ok: false, status: 422, json: async () => ({ error: { code: 'INVALID', message: 'bad' } }) };
    await expect(callApi(() => bad, '/api/x')).rejects.toMatchObject({ status: 422, code: 'INVALID', message: 'bad' });
    const unnamed = { ok: false, status: 500, json: async () => ({}) };
    await expect(callApi(() => unnamed, '/api/x')).rejects.toMatchObject({ message: 'Request failed (500)' });
    // A plain object response passes straight through.
    await expect(callApi(() => ({ plain: true }), '/api/x')).resolves.toEqual({ plain: true });
  });

  it('sendCommand builds one payload and replays it on 423 with the same operation id', async () => {
    const api = vi.fn()
      .mockRejectedValueOnce(Object.assign(new Error('locked'), { status: 423 }))
      .mockResolvedValue({ result: 'ok' });
    const result = await sendCommand(api, { args: ['update', 'ZEN-1'], files: { body: 'x' }, revisions: {}, operationId: 'stable' }, 'issue', { retryDelays: [0] });
    expect(result).toEqual({ result: 'ok' });
    expect(api).toHaveBeenCalledTimes(2);
    const payload = api.mock.calls[0][1].body;
    expect(payload).toMatchObject({ args: ['update', 'ZEN-1'], files: { body: 'x' }, operationId: 'stable' });
    expect(api.mock.calls[1][1].body.operationId).toBe('stable');
    // A non-423 error surfaces immediately (fresh mock — the one above is exhausted).
    const failing = vi.fn().mockRejectedValue(Object.assign(new Error('boom'), { status: 500 }));
    await expect(sendCommand(failing, { args: [] }, 'issue', { retryDelays: [0, 0] })).rejects.toMatchObject({ status: 500 });
  });

  it('sendAction posts the action envelope to /api/actions', async () => {
    const api = vi.fn().mockResolvedValue({ ok: true });
    await sendAction(api, { action: 'issue.delete', key: 'ZEN-1' }, { retryDelays: [] });
    const [path, options] = api.mock.calls[0];
    expect(path).toBe('/api/actions');
    expect(options.body.action).toBe('issue.delete');
  });
});

describe('issueActionData', () => {
  it('trims strings, defaults empties to null, and dedupes versions', () => {
    const data = issueActionData({
      title: '  T  ', body: 'B', labels: [' a ', ''], fixVersions: ['1.0', '1.0'],
      resolution: '', severity: 0, flagged: 1, customFieldValues: { 'cf-1': ['X'] },
    });
    expect(data).toMatchObject({
      title: 'T', body: 'B', labels: ['a'], fixVersions: ['1.0'], resolution: null,
      severity: null, flagged: true, customFieldValues: { 'cf-1': ['X'] },
    });
    expect(data.type).toBeNull();
    expect(data.sprint).toBeNull();
    expect(data.parent).toBeNull();
  });
});

describe('field layout helpers', () => {
  it('fieldVisible and fieldRequired consult the resolved layout keys', () => {
    const layout = [
      { key: 'builtin:assignee', requirement: 'REQUIRED' },
      { key: 'custom:cf-1', requirement: 'OPTIONAL' },
    ];
    expect(fieldVisible(layout, 'assignee')).toBe(true);
    expect(fieldVisible(layout, 'priority')).toBe(false);
    expect(fieldRequired(layout, 'assignee')).toBe(true);
    expect(fieldRequired(layout, 'custom:cf-1')).toBe(false);
    expect(fieldRequired(layout, { customFieldId: 'cf-1' })).toBe(false);
    expect(fieldRequired(layout, { builtinField: 'assignee' })).toBe(true);
  });

  it('legalStatusOptions falls back to from-status filtering when no verdicts apply', () => {
    const restricted = {
      statuses: [
        { id: 'todo', name: 'To Do', category: 'TODO' },
        { id: 'done', name: 'Done', category: 'DONE' },
        { id: 'blocked', name: 'Blocked', category: 'TODO' },
      ],
      viewer: { orgRole: 'VIEWER' },
      // The ORG_ROLE condition fails for a VIEWER, so no verdict applies and
      // legalStatusOptions falls back to from-status filtering: the todo->
      // done transition is offered, but Blocked is reachable from nowhere.
      workflow: { transitions: [{ id: 't', from: 'todo', to: 'done', rules: [{ kind: 'CONDITION', type: 'ORG_ROLE', config: { role: 'ADMIN' } }] }] },
    };
    const options = legalStatusOptions(restricted, { type: 'TASK', status: 'To Do' });
    expect(options.map(option => option.name)).toEqual(['To Do', 'Done']);
  });
});
