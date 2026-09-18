// @vitest-environment jsdom
import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, describe, expect, it, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';

import { submitAction, submitCommand } from '../web/planning/helpers.js';
import { sendAction, sendCommand } from '../web/issues/model.js';
import { IssuePanel } from '../web/issues/index.jsx';

const FAST_DELAYS = [0, 0, 0, 0];

function lockedError(message = 'Vault is busy') {
  return Object.assign(new Error(message), { status: 423, code: 'LOCKED' });
}

function statusError(status, code, message = 'Refused') {
  return Object.assign(new Error(message), { status, code });
}

function sequenceApi(outcomes) {
  let call = 0;
  return vi.fn(async (_path, options = {}) => {
    const index = Math.min(call, outcomes.length - 1);
    call += 1;
    const outcome = outcomes[index];
    if (outcome instanceof Error) throw outcome;
    return { result: outcome ?? { ok: true }, state: {}, request: options.body };
  });
}

afterEach(() => { cleanup(); vi.restoreAllMocks(); });

describe('planning submit helpers retry 423 LOCKED', () => {
  it('retries a 423 until it succeeds and reuses one operationId across attempts', async () => {
    const api = sequenceApi([lockedError(), lockedError(), { changed: true }]);
    const response = await submitCommand(api, ['move', 'ZEN-1', 'Done'], { operationId: 'op-fixed', retryDelays: FAST_DELAYS });
    expect(response.result).toEqual({ changed: true });
    expect(api).toHaveBeenCalledTimes(3);
    const operationIds = api.mock.calls.map(([, options]) => options.body.operationId);
    expect(operationIds).toEqual(['op-fixed', 'op-fixed', 'op-fixed']);
  });

  it('does not retry a 409 or a 422 and surfaces the error after exactly one attempt', async () => {
    const conflictApi = sequenceApi([statusError(409, 'STALE')]);
    await expect(submitAction(conflictApi, 'sprint.update', 'ZEN', { id: 's1' }, { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 409, code: 'STALE' });
    expect(conflictApi).toHaveBeenCalledTimes(1);

    const invalidApi = sequenceApi([statusError(422, 'INVALID')]);
    await expect(submitCommand(invalidApi, ['nonsense'], { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 422, code: 'INVALID' });
    expect(invalidApi).toHaveBeenCalledTimes(1);
  });

  it('gives up after the retry budget and surfaces the lock error', async () => {
    const api = sequenceApi([lockedError()]);
    await expect(submitCommand(api, ['move', 'ZEN-1', 'Done'], { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 423, code: 'LOCKED' });
    // One initial attempt plus one per injected delay.
    expect(api).toHaveBeenCalledTimes(FAST_DELAYS.length + 1);
  });
});

describe('issue model senders retry 423 LOCKED', () => {
  it('retries a 423 sendCommand and reuses the command operationId on every attempt', async () => {
    const api = sequenceApi([lockedError(), { changed: true }]);
    const command = { args: ['comment', 'ZEN-12', 'note'], files: {}, revisions: {}, operationId: 'issue-comment-1' };
    const response = await sendCommand(api, command, 'issue-comment', { retryDelays: FAST_DELAYS });
    expect(response.result).toEqual({ changed: true });
    expect(api).toHaveBeenCalledTimes(2);
    expect(api.mock.calls.every(([, options]) => options.body.operationId === 'issue-comment-1')).toBe(true);
  });

  it('retries a 423 sendAction and reuses the request operationId on every attempt', async () => {
    const api = sequenceApi([lockedError(), lockedError(), { created: true }]);
    const request = { action: 'comment.create', project: 'ZEN', key: 'ZEN-12', data: { body: 'note' }, revisions: {}, operationId: 'issue-action-1' };
    const response = await sendAction(api, request, { retryDelays: FAST_DELAYS });
    expect(response.result).toEqual({ created: true });
    expect(api).toHaveBeenCalledTimes(3);
    expect(api.mock.calls.every(([, options]) => options.body.operationId === 'issue-action-1')).toBe(true);
  });

  it('does not retry a 409 or a 422 and surfaces the error after exactly one attempt', async () => {
    const conflictApi = sequenceApi([statusError(409, 'STALE')]);
    await expect(sendAction(conflictApi, { action: 'issue.update', revisions: {}, operationId: 'op' }, { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 409, code: 'STALE' });
    expect(conflictApi).toHaveBeenCalledTimes(1);

    const invalidApi = sequenceApi([statusError(422, 'INVALID')]);
    await expect(sendCommand(invalidApi, { args: ['nonsense'], operationId: 'op' }, 'issue', { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 422, code: 'INVALID' });
    expect(invalidApi).toHaveBeenCalledTimes(1);
  });

  it('gives up after the retry budget and surfaces the lock error', async () => {
    const api = sequenceApi([lockedError()]);
    await expect(sendAction(api, { action: 'issue.update', revisions: {}, operationId: 'op' }, { retryDelays: FAST_DELAYS }))
      .rejects.toMatchObject({ status: 423, code: 'LOCKED' });
    expect(api).toHaveBeenCalledTimes(FAST_DELAYS.length + 1);
  });
});

const project = {
  key: 'ZEN',
  name: 'Zenith',
  readOnly: false,
  statuses: [
    { id: 'todo', name: 'To Do', category: 'TODO', position: 0 },
    { id: 'done', name: 'Done', category: 'DONE', position: 1 },
  ],
  users: [],
  tickets: [],
  revisions: {},
};

const issue = {
  key: 'ZEN-12',
  path: 'ZEN/tickets/ZEN-12.md',
  title: 'Old title',
  body: 'Body',
  type: 'TASK',
  status: 'To Do',
  priority: 'MEDIUM',
};

function detailWith(hash, size) {
  return {
    issue: { ...issue },
    revision: { hash, size },
    revisions: { 'ZEN/tickets/ZEN-12.md': { hash, size } },
    comments: [],
    links: { outgoing: [], incoming: [] },
    family: { parent: null, children: [] },
    history: [],
  };
}

describe('issue panel revision refresh', () => {
  it('carries the refreshed revision into the next write after a successful one', async () => {
    const reads = [detailWith('hash-old', 20), detailWith('hash-new', 24)];
    let readCount = 0;
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return reads[Math.min(readCount++, reads.length - 1)];
      if (options.method === 'POST') return { result: { changed: true }, state: {} };
      throw new Error(`Unexpected ${path}`);
    });
    render(<IssuePanel issueKey="ZEN-12" project={project} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    fireEvent.change(screen.getByLabelText('New comment'), { target: { value: 'First note' } });
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));
    await waitFor(() => expect(readCount).toBe(2));

    fireEvent.change(screen.getByLabelText('New comment'), { target: { value: 'Second note' } });
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));
    await waitFor(() => expect(screen.getByText('Second note')).toBeInTheDocument());

    const posts = api.mock.calls.filter(([, options]) => options?.method === 'POST');
    expect(posts).toHaveLength(2);
    expect(posts[0][1].body.revisions['ZEN/tickets/ZEN-12.md']).toEqual({ hash: 'hash-old', size: 20 });
    expect(posts[1][1].body.revisions['ZEN/tickets/ZEN-12.md']).toEqual({ hash: 'hash-new', size: 24 });
  });

  it('recovers from a 409 STALE by reloading and keeps the unsent draft', async () => {
    const detail = detailWith('hash-old', 20);
    const api = vi.fn(async (path, options = {}) => {
      if (path === '/api/issues/ZEN-12') return detail;
      if (options.method === 'POST') {
        throw Object.assign(new Error('File changed since it was opened: ZEN/tickets/ZEN-12.md'), { status: 409, code: 'STALE' });
      }
      throw new Error(`Unexpected ${path}`);
    });
    render(<IssuePanel issueKey="ZEN-12" project={project} api={api} />);
    await waitFor(() => expect(screen.getByTestId('issue-panel')).toBeInTheDocument());

    const textarea = screen.getByLabelText('New comment');
    fireEvent.change(textarea, { target: { value: 'A draft worth keeping' } });
    fireEvent.click(screen.getByRole('button', { name: 'Post comment' }));

    await waitFor(() => expect(screen.getByTestId('stale-notice')).toHaveTextContent('your draft is preserved'));
    expect(textarea).toHaveValue('A draft worth keeping');
    // Initial load plus the recovery reload.
    expect(api.mock.calls.filter(([path]) => path === '/api/issues/ZEN-12')).toHaveLength(2);
  });
});
