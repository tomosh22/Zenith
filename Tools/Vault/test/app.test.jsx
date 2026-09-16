// @vitest-environment jsdom
import React from 'react';
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, describe, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';

import { App, sanitizeMarkdown } from '../web/App.jsx';

const state = {
  files: [], tickets: [], documents: [], statuses: ['To Do', 'In Progress', 'Done'], projects: [], diagnostics: [], valid: true,
};

afterEach(() => {
  vi.restoreAllMocks();
  delete globalThis.WebSocket;
});

describe('Vault web surface', () => {
  test('sanitizes rendered Markdown before it reaches the DOM', () => {
    const html = sanitizeMarkdown('# Title\n\n<script>alert(1)</script><img src=x onerror="alert(1)">\n\n**safe**');
    expect(html).toContain('<h1>Title</h1>');
    expect(html).toContain('<strong>safe</strong>');
    expect(html).not.toContain('<script');
    expect(html).not.toContain('onerror');
  });

  test('loads session/state and exposes search, view navigation and read-only state', async () => {
    const fetchMock = vi.spyOn(globalThis, 'fetch').mockImplementation(async url => ({
      ok: true,
      json: async () => String(url).includes('/session')
        ? { csrfToken: 'token', readOnly: true, writeEnabled: false, vault: 'test-vault' }
        : state,
    }));
    globalThis.WebSocket = class MockSocket {
      constructor() { queueMicrotask(() => this.onopen?.()); }
      close() { this.onclose?.(); }
    };

    render(<App />);
    await waitFor(() => expect(screen.getByText('Zenith Vault')).toBeInTheDocument());
    expect(screen.getByText('Read only')).toBeInTheDocument();
    expect(screen.getByRole('textbox', { name: 'Search vault' })).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Board' })).toBeInTheDocument();
    expect(fetchMock).toHaveBeenCalled();
  });

  test('limits a ticket status dropdown to its project workflow', async () => {
    const ticket = {
      path: 'ZEN/tickets/ZEN-23.md',
      kind: 'ticket',
      project: 'ZEN',
      title: 'Status regression',
      status: 'In Progress',
      metadata: { key: 'ZEN-23', title: 'Status regression', status: 'In Progress' },
      body: '', backlinks: [], incoming: [], links: {}, wikilinks: [], diagnostics: [], hash: 'ticket-hash', size: 32, mtimeMs: 1,
    };
    const fixtureState = {
      ...state,
      files: [ticket],
      tickets: [ticket],
      projects: ['ZEN'],
      statuses: ['Blocked', 'Content Complete', 'Done', 'In Progress', 'To Do', 'engine-gap'],
      statusesByProject: { ZEN: ['To Do', 'In Progress', 'In Review', 'Blocked', 'Done'] },
    };
    const fetchMock = vi.spyOn(globalThis, 'fetch').mockImplementation(async url => {
      const value = String(url);
      if (value.includes('/session')) {
        return { ok: true, json: async () => ({ csrfToken: 'token', readOnly: true, writeEnabled: false, vault: 'test-vault' }) };
      }
      if (value.includes('/state')) return { ok: true, json: async () => fixtureState };
      if (value.includes('/document')) {
        return {
          ok: true,
          json: async () => ({
            path: ticket.path,
            text: '---\nkey: ZEN-23\ntitle: Status regression\nstatus: In Progress\n---\n',
            revision: { hash: ticket.hash, size: ticket.size, mtimeMs: ticket.mtimeMs },
            record: ticket,
          }),
        };
      }
      throw new Error(`Unexpected request: ${value}`);
    });
    globalThis.WebSocket = class MockSocket {
      constructor() { queueMicrotask(() => this.onopen?.()); }
      close() { this.onclose?.(); }
    };

    render(<App />);
    fireEvent.click(await screen.findByTitle(ticket.path));
    const select = await screen.findByRole('combobox', { name: 'Ticket status' });
    const options = [...select.options].map(option => option.value);
    expect(options).toEqual(['', 'To Do', 'In Progress', 'In Review', 'Blocked', 'Done']);
    expect(options).not.toContain('engine-gap');
    expect(options).not.toContain('Content Complete');
    expect(select).toHaveValue('In Progress');
    expect(fetchMock).toHaveBeenCalledWith(expect.stringContaining('/api/document?path='));
  });
});
