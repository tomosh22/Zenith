import { expect, test } from '@playwright/test';
import { withProjectApp } from './helpers/vault-app.mjs';

test('project navigation and issue deep links survive reload and browser history', async ({ page }) => {
  await withProjectApp(async ({ url }) => {
    await page.goto(`${url}/?project=ZEN`);
    await expect(page.getByRole('combobox', { name: 'Project', exact: true })).toHaveValue('ZEN');
    await expect(page.getByTestId('planning-board')).toBeVisible();
    await page.screenshot({ path: 'artifacts/project-board.png', fullPage: true });
    await page.getByRole('button', { name: 'Backlog', exact: true }).click();
    await expect(page).toHaveURL(/section=backlog/);
    await page.reload();
    await expect(page.getByRole('button', { name: 'Backlog', exact: true })).toHaveAttribute('aria-current', 'page');
    await page.getByRole('searchbox', { name: 'Search issues and documents' }).fill('ZEN-1');
    await page.locator('.project-search-results button').filter({ hasText: /^ZEN-1 ·/ }).click();
    await expect(page.getByRole('dialog', { name: 'Issue ZEN-1', exact: true })).toBeVisible();
    await page.screenshot({ path: 'artifacts/project-issue.png' });
    await page.reload();
    await expect(page.getByTestId('issue-panel')).toBeVisible();
    await expect(page.getByRole('dialog')).toHaveCount(0);
    await page.getByRole('button', { name: 'Close', exact: true }).click();
    await expect(page.getByRole('dialog')).toHaveCount(0);
    await page.goBack();
    await expect(page.getByRole('dialog', { name: 'Issue ZEN-1', exact: true })).toBeVisible();
  }, { readOnly: true });
});

test('creates a project with a persisted default board and workflow', async ({ page }) => {
  await withProjectApp(async ({ url }) => {
    await page.goto(url);
    await page.getByRole('button', { name: 'New project', exact: true }).click();
    const dialog = page.getByRole('dialog', { name: 'Create project', exact: true });
    await dialog.getByLabel('Name', { exact: true }).fill('Browser project');
    await dialog.getByLabel('Key', { exact: true }).fill('BROW');
    await dialog.getByLabel('Description', { exact: true }).fill('Created through the project interface.');
    await dialog.getByRole('button', { name: 'Create project', exact: true }).click();
    await expect(page).toHaveURL(/project=BROW/);
    await expect(page.getByRole('heading', { name: 'Browser project', exact: true })).toBeVisible();
    await page.reload();
    await expect(page.getByRole('combobox', { name: 'Project', exact: true })).toHaveValue('BROW');
    const response = await page.request.get(`${url}/api/projects/BROW`);
    expect(response.ok()).toBeTruthy();
    const project = await response.json();
    expect(project.statuses.map(status => status.name)).toEqual(['To Do', 'In Progress', 'In Review', 'Done', 'Blocked']);
    expect(project.description).toBe('Created through the project interface.');
    expect(project.workflow.transitions.length).toBeGreaterThan(0);
  });
});

test('read-only workspace disables project and issue creation', async ({ page }) => {
  await withProjectApp(async ({ url }) => {
    await page.goto(`${url}/?project=ZEN`);
    await expect(page.getByRole('heading', { name: 'Zenith Engine', exact: true })).toBeVisible();
    await expect(page.getByRole('button', { name: 'New project', exact: true })).toBeDisabled();
    await expect(page.getByRole('button', { name: 'Create issue', exact: true })).toBeDisabled();
    await expect(page.getByText('Read only', { exact: true })).toBeVisible();
  }, { readOnly: true });
});

test('direct issue pages can navigate to project sections and theme survives reload', async ({ page }) => {
  await withProjectApp(async ({ url }) => {
    await page.goto(`${url}/?project=ZEN&issue=ZEN-8`);
    await expect(page.getByTestId('issue-panel')).toBeVisible();
    await expect(page).toHaveTitle('ZEN-8 · ZEN · Zenith Vault');
    await page.getByRole('button', { name: 'Backlog', exact: true }).click();
    await expect(page.getByTestId('issue-panel')).toHaveCount(0);
    await expect(page.getByTestId('planning-backlog')).toBeVisible();
    await page.getByRole('button', { name: 'Switch to dark theme' }).click();
    await expect(page.locator('.project-app')).toHaveAttribute('data-theme', 'dark');
    await page.reload();
    await expect(page.getByRole('button', { name: 'Switch to light theme' })).toBeVisible();
    await page.screenshot({ path: 'artifacts/project-dark-backlog.png' });
  }, { readOnly: true });
});

test('a missing issue is recoverable through project navigation', async ({ page }) => {
  await withProjectApp(async ({ url }) => {
    await page.goto(`${url}/?project=ZEN&issue=ZEN-999999`);
    await expect(page.getByRole('alert')).toContainText('Could not load ZEN-999999');
    await page.getByRole('button', { name: 'Board', exact: true }).click();
    await expect(page.getByTestId('planning-board')).toBeVisible();
    await expect(page.getByRole('alert')).toHaveCount(0);
  }, { readOnly: true });
});
