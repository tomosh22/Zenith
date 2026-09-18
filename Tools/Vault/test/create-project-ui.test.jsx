// @vitest-environment jsdom
import React from 'react';
import { cleanup, fireEvent, render, screen, waitFor } from '@testing-library/react';
import { afterEach, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';
import { CreateProjectForm } from '../web/CreateProjectForm.jsx';
afterEach(cleanup);

test('validates empty names and unsafe keys before sending a mutation', () => {
  const api = vi.fn(); render(<CreateProjectForm api={api} onCancel={() => {}} />);
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  expect(screen.getByRole('alert')).toHaveTextContent('Enter a project name');
  fireEvent.change(screen.getByLabelText('Name'), { target: { value: 'Project' } });
  fireEvent.change(screen.getByLabelText('Key'), { target: { value: '../PATH' } });
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  expect(screen.getByRole('alert')).toHaveTextContent('starting with a letter');
  expect(api).not.toHaveBeenCalled();
});
test('keeps an edited key, submits trimmed fields and navigates after creation', async () => {
  const api = vi.fn().mockResolvedValue({ result: { key: 'XY' } }); const onCreated = vi.fn();
  render(<CreateProjectForm api={api} onCreated={onCreated} revisions={{ 'vault.config.json': { hash: 'old', size: 5 } }} />);
  fireEvent.change(screen.getByLabelText('Name'), { target: { value: 'First' } });
  fireEvent.change(screen.getByLabelText('Key'), { target: { value: 'xy' } });
  fireEvent.change(screen.getByLabelText('Name'), { target: { value: ' Final name ' } });
  fireEvent.change(screen.getByLabelText('Description'), { target: { value: ' A description ' } });
  expect(screen.getByLabelText('Key')).toHaveValue('XY');
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  await waitFor(() => expect(onCreated).toHaveBeenCalledWith('XY'));
  expect(api.mock.calls[0][1].body).toMatchObject({ action: 'project.create', project: 'XY', data: { key: 'XY', name: 'Final name', description: 'A description' }, revisions: { 'vault.config.json': { hash: 'old', size: 5 } } });
});
test('preserves failed draft and operation identity for an unchanged retry', async () => {
  const api = vi.fn().mockRejectedValueOnce(new Error('Connection interrupted')).mockResolvedValue({ result: { key: 'P' } });
  render(<CreateProjectForm api={api} onCreated={() => {}} />);
  fireEvent.change(screen.getByLabelText('Name'), { target: { value: 'Project' } });
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  await screen.findByText('Connection interrupted');
  expect(screen.getByLabelText('Name')).toHaveValue('Project');
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  await waitFor(() => expect(api).toHaveBeenCalledTimes(2));
  expect(api.mock.calls[0][1].body.operationId).toBe(api.mock.calls[1][1].body.operationId);
});

test('pins the opened catalog revision while a project draft is being edited', async () => {
  const api = vi.fn().mockResolvedValue({ result: { key: 'P' } });
  const original = { 'vault.config.json': { hash: 'opened', size: 5 } };
  const { rerender } = render(<CreateProjectForm api={api} onCreated={() => {}} revisions={original} />);
  fireEvent.change(screen.getByLabelText('Name'), { target: { value: 'Project' } });
  rerender(<CreateProjectForm api={api} onCreated={() => {}} revisions={{ 'vault.config.json': { hash: 'remote', size: 10 } }} />);
  fireEvent.click(screen.getByRole('button', { name: 'Create project' }));
  await waitFor(() => expect(api).toHaveBeenCalled());
  expect(api.mock.calls[0][1].body.revisions).toEqual(original);
});
