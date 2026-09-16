// @vitest-environment jsdom
import React from 'react';
import { render, screen, waitFor } from '@testing-library/react';
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
});
