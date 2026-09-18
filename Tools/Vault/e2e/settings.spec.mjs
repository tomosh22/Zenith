import { expect, test } from '@playwright/test';
import { withProjectApp } from './helpers/vault-app.mjs';

test.describe('Vault project settings', () => {
  test('renders project scoped workflow, fields, and board settings', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      for (const section of ['workflow', 'fields', 'board-settings']) {
        await page.goto(`${url}/?project=ZEN&section=${section}`);
        await expect(page.getByRole('combobox', { name: 'Project', exact: true })).toHaveValue('ZEN');
        await expect(page.getByTestId(`settings-${section === 'board-settings' ? 'board' : section}`)).toBeVisible();
      }
    }, { readOnly: true });
  });

  test('keeps settings controls disabled or hidden for a read-only project', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      await page.goto(`${url}/?project=ZEN&section=workflow`);
      await expect(page.getByTestId('settings-workflow')).toBeVisible();
      await expect(page.getByRole('textbox', { name: 'New status name' })).toHaveCount(0);
      await expect(page.getByRole('button', { name: /Rename/ })).toHaveCount(0);

      await page.goto(`${url}/?project=ZEN&section=fields`);
      await expect(page.getByTestId('settings-fields')).toBeVisible();
      await expect(page.getByRole('button', { name: 'New custom field' })).toHaveCount(0);
      await expect(page.getByRole('textbox', { name: 'Screen name' })).toHaveCount(0);

      await page.goto(`${url}/?project=ZEN&section=board-settings`);
      await expect(page.getByTestId('settings-board')).toBeVisible();
      await expect(page.getByRole('combobox', { name: 'Board type' })).toBeDisabled();
      await expect(page.getByRole('combobox', { name: 'Swimlanes' })).toBeDisabled();
      await expect(page.getByRole('button', { name: 'Add column' })).toHaveCount(0);
      await expect(page.getByRole('button', { name: 'Add quick filter' })).toHaveCount(0);
    }, { readOnly: true });
  });

  test('submits a workflow setting change from the isolated writable fixture', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      await page.goto(`${url}/?project=ZEN&section=workflow`);
      await expect(page.getByTestId('settings-workflow')).toBeVisible();
      const name = page.getByRole('textbox', { name: 'New status name' });
      await name.fill('Awaiting review');
      const response = page.waitForResponse(value => value.url().endsWith('/api/actions') && value.request().postDataJSON()?.action === 'workflow.status.create');
      await page.getByRole('button', { name: 'Add status' }).click();
      expect((await response).ok()).toBe(true);
      await expect(name).toHaveValue('');
    });
  });
});
