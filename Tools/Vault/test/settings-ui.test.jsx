// @vitest-environment jsdom
import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { afterEach, describe, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';

import { BoardSettings, FieldsSettings, SettingsWorkspace, WorkflowSettings } from '../web/settings/index.jsx';

const project = {
  key: 'ZEN',
  name: 'Zenith',
  revisions: { 'ZEN/project-settings.json': { hash: 'settings-v1', size: 42 } },
  statuses: [
    { id: 'todo', name: 'To Do', category: 'TODO', position: 0 },
    { id: 'done', name: 'Done', category: 'DONE', position: 1 },
  ],
  workflow: {
    statuses: [
      { id: 'todo', name: 'To Do', category: 'TODO', position: 0 },
      { id: 'done', name: 'Done', category: 'DONE', position: 1 },
    ],
    transitions: [{ id: 't1', name: 'Finish', fromStatusId: 'todo', toStatusId: 'done', rules: [] }],
  },
  fieldConfiguration: [{ issueType: 'BUG', builtinField: 'severity', requirement: 'REQUIRED' }],
  customFields: [{ id: 'cf-1', key: 'customer-tier', name: 'Customer tier', type: 'SELECT', options: ['Gold', 'Silver'] }],
  screens: [{ id: 'screen-1', name: 'Close screen', fields: [{ builtinField: 'description', customFieldId: null }] }],
  board: {
    type: 'KANBAN',
    swimlaneBy: 'NONE',
    columns: [
      { id: 'column-1', name: 'Queue', statusIds: ['todo'], position: 0, wipLimit: 2 },
      { id: 'column-2', name: 'Shipped', statusIds: ['done'], position: 1, wipLimit: null },
    ],
    cardFields: ['key', 'type'],
    quickFilters: [{ id: 'filter-1', name: 'Open bugs', filter: { type: 'BUG' } }],
  },
};

afterEach(() => {
  cleanup();
  vi.restoreAllMocks();
});

function apiMock({ failAction, failStatus = 500 } = {}) {
  return vi.fn(async (path, options = {}) => {
    if (path === '/api/actions' && options.body?.action === failAction) {
      throw Object.assign(new Error(`Could not save ${failAction}`), { status: failStatus, code: failStatus === 409 ? 'STALE' : 'ERROR' });
    }
    return { result: { ok: true }, state: {} };
  });
}

describe('project settings workspace', () => {
  test('edits workflow status categories, order, and transition edges', async () => {
    const api = apiMock();
    render(<WorkflowSettings project={project} api={api} />);

    fireEvent.change(screen.getByRole('combobox', { name: 'Category for To Do' }), { target: { value: 'IN_PROGRESS' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'workflow.status.update')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'workflow.status.update')[1].body.data).toMatchObject({ id: 'todo', category: 'IN_PROGRESS' });

    fireEvent.click(screen.getByRole('button', { name: 'Move Done up' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'workflow.status.reorder')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'workflow.status.reorder')[1].body.data.order).toEqual(['done', 'todo']);

    fireEvent.click(screen.getByRole('button', { name: 'Add global transition to To Do' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'workflow.transition.create')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'workflow.transition.create')[1].body.data).toMatchObject({ fromStatusId: null, toStatusId: 'todo' });
  });

  test('composes an ORG_ROLE rule with the shared singular role schema', async () => {
    const api = apiMock();
    render(<WorkflowSettings project={project} api={api} />);

    fireEvent.click(screen.getByRole('button', { name: 'Edit rules' }));
    fireEvent.change(screen.getByRole('combobox', { name: 'Rule kind' }), { target: { value: 'CONDITION' } });
    fireEvent.change(screen.getByRole('combobox', { name: 'Rule type' }), { target: { value: 'ORG_ROLE' } });
    fireEvent.change(screen.getByRole('combobox', { name: 'Rule value' }), { target: { value: 'ADMIN' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add rule' }));

    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'workflow.transition.rule.create')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'workflow.transition.rule.create')[1].body.data).toMatchObject({ kind: 'CONDITION', type: 'ORG_ROLE', config: { role: 'ADMIN' } });
  });

  test('keeps workflow and field drafts after a stale settings save', async () => {
    const workflowApi = apiMock({ failAction: 'workflow.status.create', failStatus: 409 });
    render(<WorkflowSettings project={project} api={workflowApi} />);
    const statusInput = screen.getByRole('textbox', { name: 'New status name' });
    fireEvent.change(statusInput, { target: { value: 'Needs review' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add status' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Could not save workflow.status.create'));
    expect(statusInput).toHaveValue('Needs review');
    expect(workflowApi.mock.calls[0][1].body.revisions).toEqual(project.revisions);

    cleanup();
    const fieldApi = apiMock({ failAction: 'field.create' });
    const rendered = render(<FieldsSettings project={project} api={fieldApi} />);
    fireEvent.click(screen.getByRole('button', { name: 'New custom field' }));
    fireEvent.change(screen.getByRole('textbox', { name: 'Field key' }), { target: { value: 'customer-segment' } });
    fireEvent.change(screen.getByRole('textbox', { name: 'Field name' }), { target: { value: 'Customer segment' } });
    rendered.rerender(<FieldsSettings project={{ ...project, revisions: { 'ZEN/project-settings.json': { hash: 'settings-v2', size: 43 } } }} api={fieldApi} />);
    fireEvent.click(screen.getByRole('button', { name: 'Create field' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Could not save field.create'));
    expect(screen.getByRole('textbox', { name: 'Field key' })).toHaveValue('customer-segment');
    expect(screen.getByRole('textbox', { name: 'Field name' })).toHaveValue('Customer segment');
    expect(fieldApi.mock.calls[0][1].body.revisions).toEqual(project.revisions);
  });

  test('updates field requirements, creates screens, and saves screen fields', async () => {
    const api = apiMock();
    render(<FieldsSettings project={project} api={api} />);

    const severity = screen.getByRole('combobox', { name: 'Severity on BUG' });
    expect(severity).toHaveValue('REQUIRED');
    fireEvent.change(severity, { target: { value: 'HIDDEN' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'field.requirement.set')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'field.requirement.set')[1].body.data).toMatchObject({ issueType: 'BUG', builtinField: 'severity', requirement: 'HIDDEN' });

    fireEvent.change(screen.getByRole('textbox', { name: 'Screen name' }), { target: { value: 'Resolve screen' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create screen' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'screen.create')).toBe(true));
    expect(screen.getByRole('textbox', { name: 'Screen name' })).toHaveValue('');

    fireEvent.click(screen.getByRole('button', { name: 'Edit fields' }));
    const editor = screen.getByTestId('settings-screen-editor-screen-1');
    const description = within(editor).getByRole('checkbox', { name: 'Description on Close screen' });
    expect(description).toBeChecked();
    fireEvent.click(description);
    fireEvent.click(within(editor).getByRole('button', { name: 'Save screen fields' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'screen.fields.set')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'screen.fields.set')[1].body.data).toMatchObject({ id: 'screen-1', fields: [] });
  });

  test('configures board type, WIP, status mapping, card fields, and quick filters', async () => {
    const api = apiMock();
    render(<BoardSettings project={project} api={api} />);

    fireEvent.change(screen.getByRole('combobox', { name: 'Board type' }), { target: { value: 'SCRUM' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.settings.update')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'board.settings.update')[1].body.data).toEqual({ type: 'SCRUM' });

    const queue = screen.getByTestId('settings-column-column-1');
    fireEvent.change(within(queue).getByRole('spinbutton', { name: 'WIP limit for Queue' }), { target: { value: '3' } });
    fireEvent.blur(within(queue).getByRole('spinbutton', { name: 'WIP limit for Queue' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.column.update')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'board.column.update')[1].body.data).toMatchObject({ id: 'column-1', wipLimit: 3 });

    fireEvent.click(within(queue).getByRole('button', { name: 'Unmap To Do from Queue' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.column.status.unmap')).toBe(true));
    fireEvent.change(within(queue).getByRole('combobox', { name: 'Add a status to Queue' }), { target: { value: 'done' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.column.status.map')).toBe(true));

    fireEvent.click(screen.getByRole('checkbox', { name: 'Priority' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.settings.update' && options.body.data.cardFields?.includes('priority'))).toBe(true));
    fireEvent.change(screen.getByRole('textbox', { name: 'Quick filter name' }), { target: { value: 'Only bugs' } });
    fireEvent.change(screen.getByRole('combobox', { name: 'Quick filter type' }), { target: { value: 'BUG' } });
    fireEvent.click(screen.getByRole('button', { name: 'Add quick filter' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'board.quick-filter.create')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'board.quick-filter.create')[1].body.data).toEqual({ name: 'Only bugs', filter: { type: 'BUG' } });
  });

  test('routes settings sections and hides all mutation controls in read-only mode', () => {
    render(<SettingsWorkspace section="board-settings" project={{ ...project, readOnly: true }} readOnly />);
    expect(screen.getByTestId('settings-board')).toBeInTheDocument();
    expect(screen.getByRole('combobox', { name: 'Board type' })).toBeDisabled();
    expect(screen.queryByRole('button', { name: 'Add column' })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Add quick filter' })).not.toBeInTheDocument();
  });
});
