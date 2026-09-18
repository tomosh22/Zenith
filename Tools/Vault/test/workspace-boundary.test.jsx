// @vitest-environment jsdom
import React from 'react';
import { cleanup, render, screen } from '@testing-library/react';
import { afterEach, expect, test, vi } from 'vitest';
import '@testing-library/jest-dom/vitest';
import { WorkspaceBoundary } from '../web/WorkspaceBoundary.jsx';

afterEach(() => { cleanup(); vi.restoreAllMocks(); });
test('keeps a working view visible', () => {
  render(<WorkspaceBoundary><p>Project workspace</p></WorkspaceBoundary>);
  expect(screen.getByText('Project workspace')).toBeInTheDocument();
});
test('offers recovery when a child view fails to render', () => {
  vi.spyOn(console, 'error').mockImplementation(() => {});
  function BrokenView() { throw new Error('Invalid catalog'); }
  render(<WorkspaceBoundary><BrokenView /></WorkspaceBoundary>);
  expect(screen.getByRole('alert')).toHaveTextContent('This view could not be displayed');
  expect(screen.getByRole('button', { name: 'Reload workspace' })).toBeVisible();
});
