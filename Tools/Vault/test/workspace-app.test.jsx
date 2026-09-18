// @vitest-environment jsdom
import React from 'react';
import { act, cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, beforeEach, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';
vi.mock('../web/planning/index.jsx', () => ({ PlanningWorkspace: ({ section, project, onOpenIssue }) => <div><p>{project.key} {section} workspace</p><button onClick={() => onOpenIssue(`${project.key}-1`)}>Open first issue</button></div> }));
vi.mock('../web/issues/index.jsx', () => ({ IssuePanel: ({ issueKey, onClose }) => <div>{issueKey} detail<button onClick={onClose}>Close issue</button></div>, CreateIssueForm: ({ onCancel }) => <div>Issue form<button onClick={onCancel}>Cancel creation</button></div> }));
vi.mock('../web/App.jsx', () => ({ App: () => <div>Document editor</div> }));
import { WorkspaceApp } from '../web/WorkspaceApp.jsx';

const projects = [{ key: 'ZEN', name: 'Zenith' }, { key: 'OLD', name: 'Archive', readOnly: true }];
let sockets;
beforeEach(() => {
  localStorage.clear();
  window.history.replaceState({}, '', '/'); sockets = [];
  vi.stubGlobal('WebSocket', class { constructor(url) { this.url = url; sockets.push(this); } close() {} });
  vi.spyOn(globalThis, 'fetch').mockImplementation(async url => {
    let payload;
    if (url === '/api/session') payload = { csrfToken: 'token', readOnly: false };
    else if (url === '/api/projects') payload = { projects };
    else if (String(url).startsWith('/api/search')) payload = { results: [{ path: 'OLD/OLD-1.md', project: 'OLD', metadata: { key: 'OLD-1' }, title: 'Archived issue' }] };
    else payload = { ...projects.find(item => url === `/api/projects/${item.key}`), tickets: [] };
    return { ok: true, json: async () => payload };
  });
});
afterEach(() => { cleanup(); vi.restoreAllMocks(); vi.unstubAllGlobals(); });

test('opens the default project and preserves section selection in the URL', async () => {
  render(<WorkspaceApp />);
  expect(await screen.findByText('ZEN board workspace')).toBeInTheDocument();
  fireEvent.click(screen.getByRole('button', { name: 'Backlog' }));
  expect(screen.getByText('ZEN backlog workspace')).toBeInTheDocument();
  expect(window.location.search).toContain('section=backlog');
  expect(sockets[0].url).toContain('/api/events');
  expect(document.title).toBe('Backlog · ZEN · Zenith Vault');
});

test('persists the selected theme across workspace mounts', async () => {
  const first = render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.click(screen.getByRole('button', { name: 'Switch to dark theme' }));
  expect(document.querySelector('.project-app')).toHaveAttribute('data-theme', 'dark');
  expect(localStorage.getItem('vault.theme')).toBe('dark');
  first.unmount();
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  expect(screen.getByRole('button', { name: 'Switch to light theme' })).toBeVisible();
});
test('direct issue deep links show a full page and close back to the project view', async () => {
  window.history.replaceState({}, '', '/?project=ZEN&issue=ZEN-1');
  render(<WorkspaceApp />);
  expect(await screen.findByText('ZEN-1 detail')).toBeInTheDocument();
  expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
  fireEvent.click(screen.getByRole('button', { name: 'Close issue' }));
  expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
  expect(window.location.search).not.toContain('issue=');
});

test('project navigation leaves a directly opened issue page', async () => {
  window.history.replaceState({}, '', '/?project=ZEN&issue=ZEN-1');
  render(<WorkspaceApp />);
  await screen.findByText('ZEN-1 detail');
  fireEvent.click(screen.getByRole('button', { name: 'Backlog' }));
  expect(screen.queryByText('ZEN-1 detail')).not.toBeInTheDocument();
  expect(screen.getByText('ZEN backlog workspace')).toBeInTheDocument();
  expect(window.location.search).not.toContain('issue=');
});

test('in-app issue navigation opens a modal and Escape returns to the mounted board', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.click(screen.getByRole('button', { name: 'Open first issue' }));
  const dialog = await screen.findByRole('dialog', { name: 'Issue ZEN-1' });
  expect(screen.getByText('ZEN board workspace')).toBeInTheDocument();
  expect(document.body.style.overflow).toBe('hidden');
  fireEvent.keyDown(dialog, { key: 'Escape' });
  expect(screen.queryByRole('dialog')).not.toBeInTheDocument();
  expect(document.body.style.overflow).not.toBe('hidden');
});
test('disables create on an archived project', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.change(screen.getByRole('combobox', { name: 'Project' }), { target: { value: 'OLD' } });
  await screen.findByText('OLD board workspace');
  expect(screen.getByRole('button', { name: 'Create issue' })).toBeDisabled();
  expect(screen.getByRole('status')).toHaveTextContent('Read only');
});
test('search opens an issue in its owning project', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.change(screen.getByRole('searchbox', { name: 'Search issues and documents' }), { target: { value: 'Archived' } });
  fireEvent.click(await screen.findByRole('button', { name: 'OLD-1 · Archived issue' }));
  expect(await screen.findByRole('dialog', { name: 'Issue OLD-1' })).toBeInTheDocument();
  expect(window.location.search).toContain('project=OLD');
});
test('keeps document workspace accessible and returns to projects', async () => {
  window.history.replaceState({}, '', '/?workspace=documents');
  render(<WorkspaceApp />);
  expect(screen.getByText('Document editor')).toBeInTheDocument();
  fireEvent.click(screen.getByRole('button', { name: 'Projects' }));
  expect(await screen.findByText('ZEN board workspace')).toBeInTheDocument();
});
test('shows a failed project read and retries it', async () => {
  const original = globalThis.fetch.getMockImplementation();
  let failed = false;
  globalThis.fetch.mockImplementation(async (url, options) => {
    if (url === '/api/projects/ZEN' && !failed) { failed = true; return { ok: false, status: 503, json: async () => ({ error: { message: 'Temporarily unavailable' } }) }; }
    return original(url, options);
  });
  render(<WorkspaceApp />);
  expect(await screen.findByRole('alert')).toHaveTextContent('Temporarily unavailable');
  fireEvent.click(screen.getByRole('button', { name: 'Retry' }));
  await waitFor(() => expect(screen.getByText('ZEN board workspace')).toBeInTheDocument());
});

test('ignores a delayed project response after switching projects', async () => {
  const original = globalThis.fetch.getMockImplementation();
  let resolveZenith;
  globalThis.fetch.mockImplementation((url, options) => url === '/api/projects/ZEN'
    ? new Promise(resolve => { resolveZenith = resolve; }) : original(url, options));
  render(<WorkspaceApp />);
  await waitFor(() => expect(resolveZenith).toBeTypeOf('function'));
  fireEvent.change(screen.getByRole('combobox', { name: 'Project' }), { target: { value: 'OLD' } });
  await screen.findByText('OLD board workspace');
  resolveZenith({ ok: true, json: async () => ({ key: 'ZEN', name: 'Zenith' }) });
  await waitFor(() => expect(screen.getByText('OLD board workspace')).toBeInTheDocument());
  expect(screen.queryByText('ZEN board workspace')).not.toBeInTheDocument();
});

test('can retry a failed session handshake', async () => {
  const original = globalThis.fetch.getMockImplementation(); let first = true;
  globalThis.fetch.mockImplementation(async (url, options) => {
    if (url === '/api/session' && first) { first = false; throw new Error('Connection lost'); }
    return original(url, options);
  });
  render(<WorkspaceApp />);
  expect(await screen.findByRole('alert')).toHaveTextContent('Connection lost');
  fireEvent.click(screen.getByRole('button', { name: 'Retry' }));
  expect(await screen.findByText('ZEN board workspace')).toBeInTheDocument();
});

test('project directory links open the selected board', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.click(screen.getByRole('button', { name: 'Projects', exact: true }));
  expect(screen.getByRole('heading', { name: 'Projects' })).toBeInTheDocument();
  fireEvent.click(screen.getByRole('button', { name: 'Archive OLD · Read only' }));
  expect(await screen.findByText('OLD board workspace')).toBeInTheDocument();
});

test('refreshes project data for live changes and reports connection errors', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  const reads = () => globalThis.fetch.mock.calls.filter(([url]) => url === '/api/projects/ZEN').length;
  const before = reads();
  act(() => sockets[0].onopen());
  await waitFor(() => expect(reads()).toBeGreaterThan(before));
  expect(screen.getByText('Live')).toBeInTheDocument();
  const opened = reads();
  act(() => sockets[0].onmessage({ data: JSON.stringify({ type: 'change' }) }));
  await waitFor(() => expect(reads()).toBeGreaterThan(opened));
  act(() => sockets[0].onmessage({ data: JSON.stringify({ type: 'error', message: 'Watcher unavailable' }) }));
  expect(screen.getByRole('alert')).toHaveTextContent('Watcher unavailable');
});

test('global search opens document results and supports keyboard focus and clear', async () => {
  const original = globalThis.fetch.getMockImplementation();
  globalThis.fetch.mockImplementation(async (url, options) => String(url).startsWith('/api/search')
    ? { ok: true, json: async () => ({ results: [{ path: 'Docs/Guide.md', title: 'Guide', kind: 'document' }, { path: 'config.md', title: 'Private config', kind: 'config' }] }) }
    : original(url, options));
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  fireEvent.keyDown(window, { key: 'k', ctrlKey: true });
  const input = screen.getByRole('searchbox');
  expect(input).toHaveFocus();
  fireEvent.change(input, { target: { value: 'Guide' } });
  fireEvent.click(await screen.findByRole('button', { name: 'Document · Guide' }));
  expect(screen.getByText('Document editor')).toBeInTheDocument();
  expect(window.location.search).toContain('path=Docs%2FGuide.md');
  expect(screen.queryByText('Private config')).not.toBeInTheDocument();
});

test('modal keyboard focus stays inside and returns to the opener', async () => {
  render(<WorkspaceApp />);
  await screen.findByText('ZEN board workspace');
  const opener = screen.getByRole('button', { name: 'Open first issue' });
  opener.focus();
  fireEvent.click(opener);
  const dialog = await screen.findByRole('dialog');
  expect(dialog).toHaveFocus();
  fireEvent.keyDown(dialog, { key: 'Tab', shiftKey: true });
  expect(screen.getByRole('button', { name: 'Close issue' })).toHaveFocus();
  fireEvent.keyDown(document.activeElement, { key: 'Tab' });
  expect(screen.getByRole('button', { name: 'Close issue' })).toHaveFocus();
  fireEvent.keyDown(dialog, { key: 'Escape' });
  expect(opener).toHaveFocus();
});
