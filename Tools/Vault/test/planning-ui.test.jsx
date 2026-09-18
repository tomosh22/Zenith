// @vitest-environment jsdom
import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { afterEach, describe, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';

import { BacklogView, BoardView, PlanningWorkspace, ReleasesView, ReportsView, RoadmapView } from '../web/planning/index.jsx';
import { usePlanningMutation } from '../web/planning/helpers.js';

const statuses = [
  { id: 'todo', name: 'To Do', category: 'TODO', position: 0 },
  { id: 'progress', name: 'In Progress', category: 'IN_PROGRESS', position: 1 },
  { id: 'review', name: 'Review', category: 'IN_PROGRESS', position: 2 },
  { id: 'done', name: 'Done', category: 'DONE', position: 3 },
];

const tickets = [
  { key: 'ZEN-1', project: 'ZEN', title: 'First task', type: 'TASK', status: 'To Do', priority: 'HIGH', position: 2, storyPoints: 3, labels: ['ui'], path: 'ZEN/tickets/ZEN-1.md', hash: 'a'.repeat(64), size: 128 },
  { key: 'ZEN-2', project: 'ZEN', title: 'In progress task', type: 'BUG', status: 'In Progress', priority: 'LOW', position: 1, storyPoints: 2, sprint: 's1', assignee: 'ada@example.test' },
  { key: 'ZEN-3', project: 'ZEN', title: 'Review task', type: 'STORY', status: 'Review', priority: 'MEDIUM', position: 3, storyPoints: 1, sprint: 's1' },
  { key: 'ZEN-4', project: 'ZEN', title: 'Finished task', type: 'TASK', status: 'Done', priority: 'MEDIUM', position: 4, storyPoints: 5, fixVersions: ['1.0'] },
  { key: 'OTHER-1', project: 'OTHER', title: 'Foreign task', type: 'TASK', status: 'To Do', priority: 'HIGH' },
];

const baseProject = {
  key: 'ZEN', name: 'Zenith', readOnly: false, statuses, tickets,
  users: [{ id: 'u1', name: 'Ada', email: 'ada@example.test' }],
  sprints: [
    { id: 's1', slug: 'sprint-one', name: 'Sprint One', status: 'ACTIVE', position: 1, goal: 'Ship it', startDate: '2026-09-01', endDate: '2026-09-14' },
    { id: 's2', slug: 'sprint-two', name: 'Sprint Two', status: 'PLANNED', position: 2 },
    { id: 's3', slug: 'sprint-three', name: 'Sprint Three', status: 'COMPLETED', position: 3 },
  ],
  versions: [
    { id: 'v1', name: '1.0', status: 'UNRELEASED', position: 1, description: 'First cut' },
    { id: 'v2', name: '0.9', status: 'ARCHIVED', position: 2 },
  ],
  board: {
    columns: [
      { id: 'todo-column', name: 'Queue', statusIds: ['todo'], position: 1 },
      { id: 'active-column', name: 'Active work', statusIds: ['progress', 'review'], position: 2 },
      { id: 'done-column', name: 'Shipped', statusIds: ['done'], position: 3 },
    ],
    cardFields: ['type', 'priority', 'assignee', 'sprint', 'labels'],
  },
  reports: {
    burndown: [
      { date: '2026-09-01', scope: 8, remaining: 8, ideal: 8 },
      { date: '2026-09-07', scope: 8, remaining: 5, ideal: 4 },
      { date: '2026-09-14', scope: 8, remaining: 0, ideal: 0 },
    ],
    velocity: { rows: [{ sprintId: 's3', sprintName: 'Sprint Three', committed: 8, completed: 5 }], average: 5 },
    epics: [{ key: 'ZEN-9', title: 'Platform', total: 3, done: 1, points: 8, donePoints: 3 }],
  },
};

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

function apiMock() {
  return vi.fn(async (path, options = {}) => ({ result: { path, ...(options.body ?? {}) }, state: {} }));
}

function RetryProbe({ api }) {
  const mutation = usePlanningMutation(api);
  return <><button type="button" onClick={() => mutation.execute({ kind: 'action', action: 'sprint.update', project: 'ZEN', data: { id: 's1', goal: 'retry me' } }).catch(() => {})}>Retry action</button>{mutation.error && <span role="alert">{mutation.error}</span>}</>;
}

describe('project planning workspace', () => {
  test('renders configured board columns by stable status ids, filters foreign tickets, and moves a card', async () => {
    const api = apiMock();
    render(<BoardView project={baseProject} api={api} />);

    expect(screen.getByTestId('planning-board')).toBeInTheDocument();
    expect(screen.getByTestId('planning-column-active-work')).toHaveTextContent('ZEN-2');
    expect(screen.getByTestId('planning-column-active-work')).toHaveTextContent('ZEN-3');
    expect(screen.queryByText('OTHER-1')).not.toBeInTheDocument();

    fireEvent.change(screen.getByRole('combobox', { name: 'Board grouping' }), { target: { value: 'priority' } });
    expect(screen.getByTestId('planning-column-high')).toHaveTextContent('ZEN-1');
    expect(screen.getByTestId('planning-column-low')).toHaveTextContent('ZEN-2');

    fireEvent.change(screen.getByRole('combobox', { name: 'Board grouping' }), { target: { value: 'status' } });
    const status = within(screen.getByTestId('planning-issue-card-zen-1')).getByRole('combobox', { name: 'Status for ZEN-1' });
    fireEvent.change(status, { target: { value: 'In Progress' } });
    await waitFor(() => expect(api).toHaveBeenCalledWith('/api/commands', expect.objectContaining({ method: 'POST' })));
    expect(api.mock.calls.at(-1)[1].body.args).toEqual(['move', 'ZEN-1', 'In Progress', '--project', 'ZEN']);
  });

  test('supports native drag and drop into a mapped column and sends the resolved status', async () => {
    const api = apiMock();
    const dataTransfer = { setData: vi.fn(), getData: vi.fn(() => 'ZEN-1'), effectAllowed: '', dropEffect: '' };
    render(<BoardView project={baseProject} api={api} />);

    const card = screen.getByTestId('planning-issue-card-zen-1');
    const shipped = screen.getByTestId('planning-column-shipped');
    fireEvent.dragStart(card, { dataTransfer });
    fireEvent.dragOver(shipped, { dataTransfer });
    fireEvent.drop(shipped, { dataTransfer });

    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.join('|') === 'move|ZEN-1|Done|--project|ZEN')).toBe(true));
    expect(dataTransfer.setData).toHaveBeenCalledWith('text/plain', 'ZEN-1');
  });

  test('rejects moving an epic into an in-progress status before issuing a write', async () => {
    const api = apiMock();
    const project = { ...baseProject, tickets: [{ key: 'ZEN-9', project: 'ZEN', title: 'Platform epic', type: 'EPIC', status: 'To Do', position: 1 }] };
    render(<BoardView project={project} api={api} />);
    const status = within(screen.getByTestId('planning-issue-card-zen-9')).getByRole('combobox', { name: 'Status for ZEN-9' });
    fireEvent.change(status, { target: { value: 'In Progress' } });
    expect(screen.getByRole('alert')).toHaveTextContent('Epic issues cannot enter In Progress');
    expect(api).not.toHaveBeenCalled();
  });

  test('allows an epic to move into another status in the in-progress category', async () => {
    const api = apiMock();
    const project = { ...baseProject, tickets: [{ key: 'ZEN-9', project: 'ZEN', title: 'Platform epic', type: 'EPIC', status: 'To Do', position: 1 }] };
    render(<BoardView project={project} api={api} />);
    const status = within(screen.getByTestId('planning-issue-card-zen-9')).getByRole('combobox', { name: 'Status for ZEN-9' });
    fireEvent.change(status, { target: { value: 'Review' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.join('|') === 'move|ZEN-9|Review|--project|ZEN')).toBe(true));
    expect(screen.queryByRole('alert')).not.toBeInTheDocument();
  });

  test('reuses an operation id for a retry of the same uncertain mutation', async () => {
    let attempts = 0;
    const api = vi.fn(async (_path, options) => {
      attempts += 1;
      if (attempts === 1) throw new Error('Connection lost after submit');
      return { result: { ok: true }, state: {} };
    });
    render(<RetryProbe api={api} />);
    fireEvent.click(screen.getByRole('button', { name: 'Retry action' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Connection lost after submit'));
    fireEvent.click(screen.getByRole('button', { name: 'Retry action' }));
    await waitFor(() => expect(api).toHaveBeenCalledTimes(2));
    expect(api.mock.calls[1][1].body.operationId).toBe(api.mock.calls[0][1].body.operationId);
  });

  test('supports sprint creation, sprint controls, edit, and moving backlog work', async () => {
    const api = apiMock();
    render(<BacklogView project={baseProject} api={api} />);

    expect(screen.getByTestId('planning-sprint-s1')).toHaveAttribute('data-sprint-status', 'ACTIVE');
    expect(screen.getByTestId('planning-sprint-s2')).toHaveAttribute('data-sprint-status', 'PLANNED');
    expect(screen.getByRole('button', { name: 'Complete sprint' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Start sprint' })).toBeDisabled();
    expect(screen.getByTestId('planning-sprint-s3')).not.toHaveTextContent('Start sprint');

    fireEvent.change(screen.getByRole('textbox', { name: 'New sprint name' }), { target: { value: 'Sprint Four' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create sprint' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.slice(0, 3).join('|') === 'sprint|create|Sprint Four')).toBe(true));

    const move = within(screen.getByTestId('planning-backlog-issue-zen-1')).getByRole('combobox', { name: 'Move ZEN-1' });
    fireEvent.change(move, { target: { value: 's2' } });
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.slice(0, 3).join('|') === 'sprint|add|Sprint Two')).toBe(true));
    const sprintMove = api.mock.calls.find(([, options]) => options.body?.args?.slice(0, 3).join('|') === 'sprint|add|Sprint Two');
    expect(sprintMove[1].body.revisions).toEqual({ 'ZEN/tickets/ZEN-1.md': { hash: 'a'.repeat(64), size: 128 } });
    expect(api.mock.calls.some(([, options]) => options.body?.args?.[0] === 'sprint' && options.body?.args?.[1] === 'remove')).toBe(false);

    fireEvent.click(screen.getAllByRole('button', { name: 'Edit' })[0]);
    expect(screen.getByTestId('planning-sprint-editor')).toBeInTheDocument();
    fireEvent.change(screen.getByRole('textbox', { name: 'Sprint goal' }), { target: { value: 'Updated goal' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save sprint' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'sprint.update')).toBe(true));
    expect(api.mock.calls.find(([, options]) => options.body?.action === 'sprint.update')[1].body.data).toMatchObject({ id: 's1', goal: 'Updated goal' });
  });

  test('completes an all-done sprint immediately with the atomic action', async () => {
    const api = apiMock();
    const project = {
      ...baseProject,
      tickets: [{ key: 'ZEN-10', project: 'ZEN', title: 'Finished sprint issue', type: 'TASK', status: 'Done', statusCategory: 'DONE', sprint: 's1', path: 'ZEN/tickets/ZEN-10.md', hash: 'd'.repeat(64), size: 96 }],
      sprints: baseProject.sprints.map(sprint => ({ ...sprint, path: `ZEN/sprints/${sprint.id}.md`, hash: 'e'.repeat(64), size: 80 })),
    };
    render(<BacklogView project={project} api={api} />);

    fireEvent.click(within(screen.getByTestId('planning-sprint-s1')).getByRole('button', { name: 'Complete sprint' }));
    expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'sprint.complete')).toBe(true));
    const request = api.mock.calls.find(([, options]) => options.body?.action === 'sprint.complete')[1].body;
    expect(request.data).toEqual({ id: 's1', destinationSprintId: null });
    expect(request.revisions).toEqual(expect.objectContaining({ 'ZEN/sprints/s1.md': expect.any(Object) }));
  });

  test('lists every unfinished issue, excludes completed destinations, and sends selected planned destination', async () => {
    const api = apiMock();
    const project = {
      ...baseProject,
      tickets: [
        { key: 'ZEN-20', project: 'ZEN', title: 'First unfinished', type: 'TASK', status: 'To Do', sprint: 's1', path: 'ZEN/tickets/ZEN-20.md', hash: 'f'.repeat(64), size: 101 },
        { key: 'ZEN-21', project: 'ZEN', title: 'Second unfinished', type: 'BUG', status: 'Review', sprint: 's1', path: 'ZEN/tickets/ZEN-21.md', hash: '1'.repeat(64), size: 102 },
      ],
      sprints: baseProject.sprints.map(sprint => ({ ...sprint, path: `ZEN/sprints/${sprint.id}.md`, hash: '2'.repeat(64), size: 80 })),
    };
    render(<BacklogView project={project} api={api} />);

    fireEvent.click(within(screen.getByTestId('planning-sprint-s1')).getByRole('button', { name: 'Complete sprint' }));
    const dialog = screen.getByRole('dialog');
    expect(dialog).toHaveTextContent("2 issues aren't Done");
    expect(within(screen.getByTestId('incomplete-issues-list')).getAllByRole('listitem')).toHaveLength(2);
    expect(screen.getByLabelText('Sprint Two')).toBeInTheDocument();
    expect(screen.queryByLabelText('Sprint Three')).not.toBeInTheDocument();

    fireEvent.click(screen.getByLabelText('Sprint Two'));
    fireEvent.click(within(dialog).getByRole('button', { name: 'Complete sprint' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'sprint.complete')).toBe(true));
    const request = api.mock.calls.find(([, options]) => options.body?.action === 'sprint.complete')[1].body;
    expect(request.data).toEqual({ id: 's1', destinationSprintId: 's2' });
    expect(request.revisions).toEqual(expect.objectContaining({
      'ZEN/sprints/s1.md': expect.any(Object),
      'ZEN/sprints/s2.md': expect.any(Object),
      'ZEN/tickets/ZEN-20.md': expect.any(Object),
      'ZEN/tickets/ZEN-21.md': expect.any(Object),
    }));
  });

  test('allows completed sprint deletion while keeping active deletion unavailable', async () => {
    const api = apiMock();
    const project = { ...baseProject, sprints: baseProject.sprints.map(sprint => ({ ...sprint, path: `ZEN/sprints/${sprint.id}.md`, hash: '3'.repeat(64), size: 80 })) };
    render(<BacklogView project={project} api={api} />);

    const completed = within(screen.getByTestId('planning-sprint-s3'));
    fireEvent.click(completed.getByRole('button', { name: 'Delete' }));
    const dialog = screen.getByRole('dialog');
    fireEvent.click(within(dialog).getByRole('button', { name: 'Delete sprint' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'sprint.delete' && options.body?.data?.id === 's3')).toBe(true));
    expect(within(screen.getByTestId('planning-sprint-s1')).queryByRole('button', { name: 'Delete' })).not.toBeInTheDocument();
  });

  test('hides a partial sprint date instead of rendering a half range', () => {
    const project = { ...baseProject, sprints: baseProject.sprints.map(sprint => sprint.id === 's2' ? { ...sprint, startDate: '2026-10-01' } : sprint) };
    render(<BacklogView project={project} api={apiMock()} />);
    expect(screen.getByTestId('planning-sprint-s2').querySelector('small')).toBeNull();
  });

  test('renders releases, calls create/release/archive actions, and preserves read-only affordances', async () => {
    const api = apiMock();
    render(<ReleasesView project={baseProject} api={api} />);
    expect(screen.getByTestId('planning-version-v1')).toHaveTextContent('1.0');
    expect(screen.getByTestId('planning-version-v2')).toHaveAttribute('data-version-status', 'ARCHIVED');

    fireEvent.click(screen.getByRole('button', { name: 'New release' }));
    fireEvent.change(screen.getByRole('textbox', { name: 'New version name' }), { target: { value: '2.0' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create release' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.slice(0, 3).join('|') === 'version|create|2.0')).toBe(true));

    fireEvent.click(screen.getByRole('button', { name: 'Release' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.args?.slice(0, 3).join('|') === 'version|release|1.0')).toBe(true));
    fireEvent.click(screen.getByRole('button', { name: 'Archive' }));
    await waitFor(() => expect(api.mock.calls.some(([, options]) => options.body?.action === 'version.archive')).toBe(true));

    cleanup();
    const readOnly = { ...baseProject, readOnly: true };
    render(<ReleasesView project={readOnly} readOnly api={api} />);
    expect(screen.queryByRole('button', { name: 'New release' })).not.toBeInTheDocument();
    expect(screen.queryByRole('button', { name: 'Release' })).not.toBeInTheDocument();
  });

  test('builds an epic tree with child and orphan issue views and supports collapse', () => {
    const project = { ...baseProject, tickets: [
      { key: 'ZEN-9', title: 'Platform epic', type: 'EPIC', status: 'In Progress', position: 1 },
      { key: 'ZEN-10', title: 'Child one', type: 'STORY', parent: 'ZEN-9', status: 'Done', statusCategory: 'DONE', storyPoints: 3 },
      { key: 'ZEN-11', title: 'Child two', type: 'TASK', parent: 'ZEN-9', status: 'To Do', storyPoints: 2 },
      { key: 'ZEN-12', title: 'Orphan task', type: 'TASK', status: 'To Do' },
    ] };
    render(<RoadmapView project={project} onOpenIssue={vi.fn()} />);
    expect(screen.getByTestId('planning-roadmap-epic-zen-9')).toHaveTextContent('1/2 done');
    expect(screen.getByTestId('planning-roadmap-epic-zen-9')).toHaveTextContent('Child one');
    expect(screen.getByTestId('planning-roadmap-orphans')).toHaveTextContent('Orphan task');
    fireEvent.click(screen.getByRole('button', { name: 'Collapse Platform epic' }));
    expect(screen.queryByText('Child one')).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Expand Platform epic' })).toBeInTheDocument();
  });

  test('renders burndown, velocity, epic report, and sprint selector through the workspace switch', () => {
    const onOpenIssue = vi.fn();
    render(<PlanningWorkspace section="reports" project={baseProject} onOpenIssue={onOpenIssue} />);
    expect(screen.getByTestId('planning-reports')).toBeInTheDocument();
    expect(screen.getByRole('img', { name: /Burndown over 3 days/ })).toBeInTheDocument();
    expect(screen.getByText('Velocity')).toBeInTheDocument();
    expect(screen.getByText(/points completed per sprint/)).toBeInTheDocument();
    fireEvent.change(screen.getByRole('combobox', { name: 'Charted sprint' }), { target: { value: 's2' } });
    expect(screen.getByRole('combobox', { name: 'Charted sprint' })).toHaveValue('s2');
    expect(screen.queryByRole('img', { name: /Burndown over/ })).not.toBeInTheDocument();
    expect(screen.getByText('No sprint with dated history is available to chart.')).toBeInTheDocument();
  });
});
