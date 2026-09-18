// @vitest-environment jsdom
import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, describe, expect, it, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';

import {
  CreateIssueForm,
  IssuePanel,
  buildCreateCommand,
  buildLinkCommand,
  buildUpdateCommand,
  legalStatusOptions,
} from '../web/issues/index.jsx';

const project = {
  key: 'ZEN',
  name: 'Zenith',
  readOnly: false,
  statuses: [
    { id: 'todo', name: 'To Do', category: 'TODO', position: 0 },
    { id: 'progress', name: 'In Progress', category: 'IN_PROGRESS', position: 1 },
    { id: 'done', name: 'Done', category: 'DONE', position: 2 },
  ],
  labels: [{ id: 'bug', name: 'bug' }, { id: 'ui', name: 'ui' }],
  categories: [{ id: 'product', name: 'Product' }],
  users: [{ id: 'u1', name: 'Ada', email: 'ada@example.test' }],
  sprints: [{ id: 's1', name: 'Sprint 1', status: 'ACTIVE' }],
  versions: [{ id: 'v1', name: '1.0', status: 'UNRELEASED' }],
  tickets: [{ key: 'ZEN-9', title: 'Roadmap epic', type: 'EPIC', status: 'To Do' }],
  revisions: {},
};

const issue = {
  key: 'ZEN-12',
  path: 'ZEN/tickets/ZEN-12.md',
  title: 'Old title',
  body: '## Goal\n\nKeep this body raw.\r\n',
  type: 'TASK',
  status: 'To Do',
  priority: 'MEDIUM',
  complexity: 'MODERATE',
  risk: 'LOW',
  storyPoints: 3,
  category: 'Product',
  assignee: 'ada@example.test',
  labels: ['ui'],
  fixVersions: ['1.0'],
  sprint: 'Sprint 1',
  parent: null,
  updatedAt: '2026-09-16T10:00:00.000Z',
};

const detail = {
  issue,
  revision: { hash: 'issue-hash', size: 20 },
  revisions: { 'ZEN/tickets/ZEN-12.md': { hash: 'issue-hash', size: 20 } },
  comments: [{ id: 'c1', body: 'Imported comment', author: { name: 'Grace' }, createdAt: '2026-09-15T10:00:00.000Z' }],
  links: {
    outgoing: [{ id: 'l1', key: 'ZEN-13', title: 'Downstream task', type: 'BLOCKS', reason: 'Needs this first' }],
    incoming: [{ id: 'l2', key: 'ZEN-11', title: 'Upstream task', type: 'BLOCKS', statusCategory: 'TODO' }],
  },
  family: {
    parent: null,
    children: [{ id: 'child-1', key: 'ZEN-14', title: 'Child task', status: 'Done', statusCategory: 'DONE', storyPoints: 2 }],
  },
  history: [{ id: 'h1', field: 'priority', fromValue: 'LOW', toValue: 'MEDIUM', actorName: 'Ada', createdAt: '2026-09-15T11:00:00.000Z' }],
};

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

function postCalls(api) {
  return api.mock.calls.filter(([, options]) => options?.method === 'POST');
}

describe('issue command builders', () => {
  it('keeps create body inline and includes the requested Jira fields', () => {
    const command = buildCreateCommand('ZEN', {
      title: 'Ship issue workflow', body: 'raw\r\nbody', type: 'STORY', status: 'To Do', priority: 'HIGH',
      complexity: 'SIMPLE', risk: 'LOW', storyPoints: 5, category: 'Product', assignee: 'ada@example.test',
      labels: ['ui', 'workflow'], fixVersions: ['1.0'], sprint: 'Sprint 1', parent: 'ZEN-9',
    });
    expect(command.args).toEqual(expect.arrayContaining([
      'create', '--project', 'ZEN', '--title', 'Ship issue workflow', '--type', 'STORY', '--status', 'To Do',
      '--priority', 'HIGH', '--complexity', 'SIMPLE', '--risk', 'LOW', '--points', '5', '--category', 'Product',
      '--assignee', 'ada@example.test', '--label', 'ui,workflow', '--fix-version', '1.0', '--sprint', 'Sprint 1', '--parent', 'ZEN-9',
    ]));
    expect(command.files).toEqual({ file: 'raw\r\nbody' });
  });

  it('uses the shared CLI relation spelling and removes labels explicitly on update', () => {
    expect(buildLinkCommand('ZEN-12', 'blocked-by', 'ZEN-3', 'waits on API')).toMatchObject({
      args: ['link', 'ZEN-12', 'blocked-by', 'ZEN-3', '--reason', 'waits on API'],
    });
    expect(buildUpdateCommand({ ...issue, labels: ['ui', 'old'] }, { ...issue, labels: ['ui', 'new'] }).args)
      .toEqual(expect.arrayContaining(['--label', 'ui,new', '--unlabel', 'old']));
  });

  it('keeps all selected fix versions and rejects self-links before a write', () => {
    expect(buildCreateCommand('ZEN', { title: 'Multi version', fixVersions: ['1.0', '2.0'] }).versions)
      .toEqual(['1.0', '2.0']);
    expect(() => buildLinkCommand('ZEN-12', 'relates-to', 'ZEN-12')).toThrow(/itself/i);
  });

  it('uses the shared EPIC invariant precisely while retaining In Review', () => {
    const options = legalStatusOptions({
      statuses: [
        { id: 'todo', name: 'To Do', category: 'TODO' },
        { id: 'progress', name: 'In Progress', category: 'IN_PROGRESS' },
        { id: 'review', name: 'In Review', category: 'IN_PROGRESS' },
      ],
      workflow: { transitions: [{ from: null, to: 'To Do' }, { from: null, to: 'In Progress' }, { from: null, to: 'In Review' }] },
    }, { type: 'EPIC', status: 'To Do' });
    expect(options.map(option => option.name)).toEqual(['To Do', 'In Review']);
  });
});

describe('CreateIssueForm', () => {
  it('renders the full create workflow and posts the exact command payload', async () => {
    const api = vi.fn().mockResolvedValue({ result: { key: 'ZEN-15', created: true }, state: {} });
    const onCreated = vi.fn();
    render(<CreateIssueForm project={project} api={api} onCreated={onCreated} initialValues={{ body: 'Body\r\nwith raw EOL' }} />);

    fireEvent.change(screen.getByLabelText('Issue title'), { target: { value: 'New issue' } });
    fireEvent.change(screen.getByLabelText('Issue type'), { target: { value: 'BUG' } });
    fireEvent.change(screen.getByLabelText('Issue status'), { target: { value: 'In Progress' } });
    fireEvent.change(screen.getByLabelText('Priority'), { target: { value: 'HIGH' } });
    fireEvent.change(screen.getByLabelText('Complexity'), { target: { value: 'SIMPLE' } });
    fireEvent.change(screen.getByLabelText('Risk'), { target: { value: 'HIGH' } });
    fireEvent.change(screen.getByLabelText('Story points'), { target: { value: '5' } });
    fireEvent.change(screen.getByLabelText('Category'), { target: { value: 'Product' } });
    fireEvent.change(screen.getByLabelText('Assignee'), { target: { value: 'ada@example.test' } });
    fireEvent.change(screen.getByLabelText('Labels'), { target: { value: 'ui, workflow' } });
    fireEvent.change(screen.getByLabelText('Fix version'), { target: { value: '1.0' } });
    fireEvent.change(screen.getByLabelText('Sprint'), { target: { value: 'Sprint 1' } });
    fireEvent.change(screen.getByLabelText('Parent issue'), { target: { value: 'ZEN-9' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create issue' }));

    await waitFor(() => expect(onCreated).toHaveBeenCalledWith(expect.objectContaining({ key: 'ZEN-15' }), expect.any(Object)));
    expect(postCalls(api)).toHaveLength(1);
    const request = postCalls(api)[0][1].body;
    expect(request.args).toEqual(expect.arrayContaining(['create', '--project', 'ZEN', '--title', 'New issue', '--type', 'BUG', '--status', 'In Progress', '--priority', 'HIGH']));
    expect(request.files.file).toBe('Body\r\nwith raw EOL');
    expect(request.operationId).toMatch(/^issue-create-/);
  });

  it('preserves the draft and shows a structured API refusal', async () => {
    const api = vi.fn().mockRejectedValue(Object.assign(new Error('Stale issue revision'), { code: 'STALE', status: 409 }));
    render(<CreateIssueForm project={project} api={api} />);
    const title = screen.getByLabelText('Issue title');
    fireEvent.change(title, { target: { value: 'Keep this draft' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create issue' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Stale issue revision'));
    expect(title).toHaveValue('Keep this draft');
  });

  it('reuses the operation id when a create response is uncertain and the draft is unchanged', async () => {
    let calls = 0;
    const api = vi.fn(async (path, options = {}) => {
      if (options.method !== 'POST') throw new Error(`Unexpected ${path}`);
      calls += 1;
      if (calls === 1) throw new Error('Connection lost after submit');
      return { result: { key: 'ZEN-16', created: true }, state: {} };
    });
    const onCreated = vi.fn();
    render(<CreateIssueForm project={project} api={api} onCreated={onCreated} />);
    fireEvent.change(screen.getByLabelText('Issue title'), { target: { value: 'Retry me' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create issue' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Connection lost'));
    fireEvent.click(screen.getByRole('button', { name: 'Create issue' }));
    await waitFor(() => expect(onCreated).toHaveBeenCalled());
    expect(api.mock.calls[0][1].body.operationId).toBe(api.mock.calls[1][1].body.operationId);
  });

  it('renders typed custom fields from the per-type layout and submits the advertised atomic action', async () => {
    const customProject = {
      ...project,
      customFields: [{ id: 'cf-team', name: 'Team', type: 'SELECT', options: ['Core', 'UI'] }],
      fieldConfiguration: [{ issue_type: 'TASK', custom_field_id: 'cf-team', requirement: 'REQUIRED' }],
      actions: { create: 'issue.create' },
    };
    const api = vi.fn().mockResolvedValue({ result: { key: 'ZEN-17', created: true }, state: {} });
    const onCreated = vi.fn();
    render(<CreateIssueForm project={customProject} api={api} onCreated={onCreated} />);
    fireEvent.change(screen.getByLabelText('Issue title'), { target: { value: 'Typed custom field' } });
    fireEvent.change(screen.getByLabelText('Team (required)'), { target: { value: 'UI' } });
    fireEvent.click(screen.getByRole('button', { name: 'Create issue' }));
    await waitFor(() => expect(onCreated).toHaveBeenCalled());
    const request = postCalls(api)[0][1].body;
    expect(request.action).toBe('issue.create');
    expect(request.data.customFieldValues).toEqual({ 'cf-team': ['UI'] });
  });
});

describe('IssuePanel', () => {
  it('loads detail, renders family/links/comments/history, and updates through commands', async () => {
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    const onChanged = vi.fn();
    const onOpenIssue = vi.fn();
    render(<IssuePanel issueKey="ZEN-12" project={project} api={api} onChanged={onChanged} onOpenIssue={onOpenIssue} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    expect(screen.getByText('ZEN-12')).toBeInTheDocument();
    expect(screen.getByDisplayValue('Old title')).toBeInTheDocument();
    expect(screen.getByTestId('issue-description-editor')).toHaveTextContent('Keep this body raw.');
    expect(screen.getByText('Imported comment')).toBeInTheDocument();
    expect(screen.getByText('Needs this first')).toBeInTheDocument();
    expect(screen.getByText('Child task')).toBeInTheDocument();
    expect(screen.getByText('priority')).toBeInTheDocument();

    fireEvent.change(screen.getByLabelText('Priority'), { target: { value: 'HIGH' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save issue' }));
    await waitFor(() => expect(postCalls(api).length).toBeGreaterThan(0));
    const update = postCalls(api)[0][1].body;
    expect(update.args).toEqual(expect.arrayContaining(['update', 'ZEN-12', '--priority', 'HIGH']));
    expect(update.revisions['ZEN/tickets/ZEN-12.md']).toEqual(detail.revisions['ZEN/tickets/ZEN-12.md']);
    expect(onChanged).toHaveBeenCalled();

    fireEvent.click(screen.getByRole('button', { name: 'Add link' }));
    fireEvent.change(screen.getByLabelText('Issue key to link'), { target: { value: 'ZEN-20' } });
    fireEvent.change(screen.getByLabelText('Why this blocks (required)'), { target: { value: 'Needs this issue first' } });
    fireEvent.click(screen.getByRole('button', { name: 'Link issue' }));
    await waitFor(() => expect(postCalls(api).some(([, options]) => options.body.args?.[0] === 'link')).toBe(true));

    fireEvent.change(screen.getByLabelText('New comment'), { target: { value: 'A useful note' } });
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));
    await waitFor(() => expect(postCalls(api).some(([, options]) => options.body.args?.[0] === 'comment')).toBe(true));
  });

  it('sends the legacy version commands after the update when no atomic action is advertised', async () => {
    const versionProject = {
      ...project,
      versions: [
        { id: 'v1', name: '1.0', status: 'UNRELEASED' },
        { id: 'v2', name: '2.0', status: 'UNRELEASED' },
      ],
    };
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    render(<IssuePanel issueKey="ZEN-12" project={versionProject} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    // The project advertises no actions.update, so a version edit cannot ride
    // the atomic update. The plain issue update goes first (it never carries
    // version args), then the legacy set-like version command clears the old
    // selection and re-adds the new one.
    fireEvent.change(screen.getByLabelText('Fix version'), { target: { value: '2.0' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save issue' }));
    await waitFor(() => expect(postCalls(api)).toHaveLength(3));
    const sequences = postCalls(api).map(([, options]) => options.body.args);
    expect(sequences[0]).toEqual(expect.arrayContaining(['update', 'ZEN-12']));
    expect(sequences[1]).toEqual(['version', 'set', 'none', 'ZEN-12']);
    expect(sequences[2]).toEqual(['version', 'set', '2.0', 'ZEN-12']);
  });

  it('keeps a dirty body when the project revision refreshes', async () => {
    const first = { ...detail };
    const api = vi.fn(async path => {
      if (path === '/api/issues/ZEN-12') return first;
      throw new Error('unexpected');
    });
    const { rerender } = render(<IssuePanel issueKey="ZEN-12" project={project} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-description-editor')).toBeInTheDocument());
    const editor = screen.getByLabelText('Issue title');
    fireEvent.change(editor, { target: { value: 'Local unsaved title' } });
    rerender(<IssuePanel issueKey="ZEN-12" project={{ ...project, revisions: { changed: { hash: 'new', size: 2 } } }} api={api} />);
    await waitFor(() => expect(editor).toHaveValue('Local unsaved title'));
  });

  it('submits a dirty draft against its pinned baseline after an external refresh', async () => {
    const newer = {
      ...detail,
      issue: { ...issue, title: 'Remote title', body: 'Remote body', updatedAt: '2026-09-16T10:05:00.000Z' },
      revision: { hash: 'new-hash', size: 21 },
      revisions: { 'ZEN/tickets/ZEN-12.md': { hash: 'new-hash', size: 21 } },
    };
    let reads = 0;
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return reads++ === 0 ? detail : newer;
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    const { rerender } = render(<IssuePanel issueKey="ZEN-12" project={project} api={api} />);
    await waitFor(() => expect(screen.getByDisplayValue('Old title')).toBeInTheDocument());
    const title = screen.getByLabelText('Issue title');
    fireEvent.change(title, { target: { value: 'Local title wins if still current' } });
    rerender(<IssuePanel issueKey="ZEN-12" project={{ ...project, revisions: { watcher: { hash: 'remote', size: 1 } } }} api={api} />);
    await waitFor(() => expect(title).toHaveValue('Local title wins if still current'));
    fireEvent.click(screen.getByRole('button', { name: 'Save issue' }));
    await waitFor(() => expect(postCalls(api).length).toBeGreaterThan(0));
    expect(postCalls(api)[0][1].body.revisions['ZEN/tickets/ZEN-12.md']).toEqual(detail.revisions['ZEN/tickets/ZEN-12.md']);
  });

  it('reloads when a revision broadcast lands while the opening fetch is still in flight', async () => {
    // Reproduces the open-vs-broadcast race: the panel's first GET is slow,
    // a websocket-driven project refresh (new revisions) arrives while the
    // detail is still null, and only then does the first fetch resolve. The
    // fingerprint change must stay pending until a reload can run — not be
    // consumed while the panel cannot act on it.
    let resolveFirst;
    const first = new Promise(resolve => { resolveFirst = resolve; });
    const withoutComment = { ...detail, comments: [] };
    const withComment = {
      ...detail,
      comments: [{ id: 'c2', body: 'Broadcast comment', author: { name: 'Ada' }, createdAt: '2026-09-16T11:00:00.000Z' }],
    };
    let reads = 0;
    const api = vi.fn(async path => {
      if (path !== '/api/issues/ZEN-12') throw new Error(`Unexpected ${path}`);
      reads += 1;
      if (reads === 1) return first;
      return withComment;
    });
    const refreshed = { ...project, revisions: { watcher: { hash: 'remote', size: 1 } } };
    const { rerender } = render(<IssuePanel issueKey="ZEN-12" project={project} api={api} />);

    // Broadcast lands while the opening fetch is pending (detail still null).
    rerender(<IssuePanel issueKey="ZEN-12" project={refreshed} api={api} />);
    resolveFirst(withoutComment);

    // The pending fingerprint change must trigger exactly one reload, which
    // this stub answers WITH the comment.
    await waitFor(() => expect(screen.getByText('Broadcast comment')).toBeInTheDocument());
    expect(reads).toBe(2);

    // One fingerprint change, one reload: re-rendering with the same
    // revisions must not fetch again (the unfixed effect would have consumed
    // the change above and never reloaded at all).
    rerender(<IssuePanel issueKey="ZEN-12" project={refreshed} api={api} />);
    await waitFor(() => expect(screen.getByText('Broadcast comment')).toBeInTheDocument());
    expect(reads).toBe(2);
  });

  it('requires a resolution for a Done transition before sending the atomic update', async () => {
    const workflowProject = {
      ...project,
      actions: { update: 'issue.update' },
      workflow: {
        transitions: [{ id: 'finish', fromStatusId: 'todo', toStatusId: 'done', rules: [] }],
      },
    };
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    render(<IssuePanel issueKey="ZEN-12" project={workflowProject} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    fireEvent.change(screen.getByLabelText('Issue status'), { target: { value: 'Done' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save issue' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Choose a resolution'));
    expect(postCalls(api)).toHaveLength(0);

    fireEvent.change(screen.getByLabelText('Resolution'), { target: { value: 'DONE' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save issue' }));
    await waitFor(() => expect(postCalls(api)).toHaveLength(1));
    expect(postCalls(api)[0][1].body).toMatchObject({
      action: 'issue.update',
      data: { status: 'Done', resolution: 'DONE' },
    });
  });

  it('refuses a BLOCKS link that would close a known project cycle before posting', async () => {
    const graphProject = {
      ...project,
      tickets: [
        { key: 'ZEN-12', type: 'TASK', blocks: [{ key: 'ZEN-13' }] },
        { key: 'ZEN-13', type: 'TASK', blocks: [{ key: 'ZEN-14' }] },
        { key: 'ZEN-14', type: 'TASK', blocks: [] },
      ],
    };
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    render(<IssuePanel issueKey="ZEN-12" project={graphProject} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());
    fireEvent.click(screen.getByRole('button', { name: 'Add link' }));
    fireEvent.change(screen.getByLabelText('Link type'), { target: { value: 'blocked-by' } });
    fireEvent.change(screen.getByLabelText('Issue key to link'), { target: { value: 'ZEN-14' } });
    fireEvent.change(screen.getByLabelText('Why this blocks (required)'), { target: { value: 'Would close the loop' } });
    fireEvent.click(screen.getByRole('button', { name: 'Link issue' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('circular dependency'));
    expect(postCalls(api)).toHaveLength(0);
  });

  it('deletes only deletable comments through the advertised action and retries with one operation id', async () => {
    let posts = 0;
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return {
        ...detail,
        comments: [
          ...detail.comments,
          { id: 'c2', body: 'Another author', canDelete: false, author: { name: 'Other' } },
        ],
      };
      if (path === '/api/actions') {
        posts += 1;
        if (posts === 1) throw new Error('Connection lost after delete');
        return { result: { deleted: true }, state: {} };
      }
      throw new Error(`Unexpected ${path}`);
    });
    const deleteProject = { ...project, actions: { deleteComment: 'issue.comment.delete' } };
    render(<IssuePanel issueKey="ZEN-12" project={deleteProject} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    expect(screen.getAllByRole('button', { name: 'Delete comment' })).toHaveLength(1);
    fireEvent.click(screen.getByRole('button', { name: 'Delete comment' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Connection lost after delete'));
    fireEvent.click(screen.getByRole('button', { name: 'Delete comment' }));
    await waitFor(() => expect(posts).toBe(2));
    const requests = api.mock.calls.filter(([, options]) => options?.method === 'POST');
    expect(requests[0][1].body).toMatchObject({
      action: 'issue.comment.delete',
      project: 'ZEN',
      key: 'ZEN-12',
      data: { commentId: 'c1' },
    });
    expect(requests[0][1].body.operationId).toBe(requests[1][1].body.operationId);
  });

  it('posts comments through the advertised interaction action and retries with one operation id', async () => {
    let posts = 0;
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (path === '/api/actions') {
        posts += 1;
        if (posts === 1) throw new Error('Connection lost after comment create');
        return { result: { created: true }, state: {} };
      }
      throw new Error(`Unexpected ${path}`);
    });
    const actionProject = { ...project, actions: { createComment: 'comment.create' } };
    render(<IssuePanel issueKey="ZEN-12" project={actionProject} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());
    fireEvent.change(screen.getByLabelText('New comment'), { target: { value: 'A durable comment' } });
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));
    await waitFor(() => expect(screen.getByRole('alert')).toHaveTextContent('Connection lost after comment create'));
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));
    await waitFor(() => expect(posts).toBe(2));
    const requests = postCalls(api);
    expect(requests).toHaveLength(2);
    expect(requests[0][1].body).toMatchObject({
      action: 'comment.create',
      project: 'ZEN',
      key: 'ZEN-12',
      data: { body: 'A durable comment' },
    });
    expect(requests[0][1].body.operationId).toBe(requests[1][1].body.operationId);
  });
});
