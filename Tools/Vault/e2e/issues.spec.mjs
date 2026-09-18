import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { expect, test } from '@playwright/test';

import { withProjectApp } from './helpers/vault-app.mjs';

test.describe('Vault issue workflows', () => {
  test('creates and edits an issue, then records a comment and dependency', async ({ page }) => {
    await withProjectApp(async ({ root, url }) => {
      await page.goto(`${url}?project=DP`);
      await expect(page.getByRole('button', { name: 'Create issue' })).toBeEnabled();
      await page.getByRole('button', { name: 'Create issue' }).click();
      const create = page.getByTestId('issue-create-form');
      await create.getByLabel('Issue title').fill('Browser created issue');
      await create.getByLabel('Issue type').selectOption('BUG');
      await create.getByLabel('Priority').selectOption('HIGH');
      await create.getByLabel('Complexity').selectOption('SIMPLE');
      await create.getByLabel('Risk').selectOption('LOW');
      await create.getByLabel('Labels').fill('ui, bug');
      await create.getByRole('button', { name: 'Create issue' }).click();

      const panel = page.getByRole('dialog', { name: 'Issue DP-2' });
      await expect(panel).toBeVisible();
      await expect(panel.getByLabel('Issue title')).toHaveValue('Browser created issue');
      await expect(panel.getByLabel('Priority')).toHaveValue('HIGH');
      await expect.poll(async () => (await readFile(path.join(root, 'DP', 'tickets', 'DP-2.md'), 'utf8'))).toContain('Browser created issue');

      await panel.getByLabel('Priority').selectOption('CRITICAL');
      const editor = panel.getByTestId('issue-description-editor');
      await editor.click();
      await page.keyboard.press('Control+End');
      await page.keyboard.insertText('\nBrowser body sentinel');
      await panel.getByRole('button', { name: 'Save issue' }).click();
      await expect.poll(async () => (await readFile(path.join(root, 'DP', 'tickets', 'DP-2.md'), 'utf8'))).toContain('Browser body sentinel');
      await expect(panel.getByTestId('issue-history')).toContainText('priority');

      await panel.getByLabel('New comment').fill('A browser comment');
      await panel.getByRole('button', { name: 'Post comment' }).click();
      await expect(panel.getByText('A browser comment')).toBeVisible();

      await panel.getByRole('button', { name: 'Add link' }).click();
      await panel.getByLabel('Issue key to link').fill('DP-1');
      await panel.getByLabel('Why this blocks (required)').fill('The epic must land first');
      await panel.getByRole('button', { name: 'Link issue' }).click();
      await expect(panel.getByTestId('issue-links').getByRole('button', { name: 'DP-1' })).toBeVisible();
    }, { commit: true });
  });

  test('persists a local comment, exposes only own deletion, and deletes an issue with cleanup', async ({ page }) => {
    await withProjectApp(async ({ root, url }) => {
      await page.goto(`${url}?project=DP&issue=DP-1`);
      const panel = page.getByTestId('issue-panel');
      await expect(panel).toBeVisible();

      await panel.getByLabel('New comment').fill('A persisted browser comment');
      await panel.getByRole('button', { name: 'Post comment' }).click();
      const comment = panel.getByTestId('comment').filter({ hasText: 'A persisted browser comment' });
      await expect(comment).toBeVisible();
      await expect(comment.getByRole('button', { name: 'Delete comment' })).toBeVisible();

      await page.reload();
      const reloaded = page.getByTestId('issue-panel');
      const persisted = reloaded.getByTestId('comment').filter({ hasText: 'A persisted browser comment' });
      await expect(persisted).toBeVisible();
      await persisted.getByRole('button', { name: 'Delete comment' }).click();
      await expect(persisted).not.toBeVisible();

      await reloaded.getByRole('button', { name: 'Delete issue' }).click();
      await expect(page.getByTestId('issue-panel')).not.toBeVisible();
      await expect.poll(async () => {
        try { await readFile(path.join(root, 'DP', 'tickets', 'DP-1.md'), 'utf8'); return true; } catch (error) { return error.code !== 'ENOENT'; }
      }).toBe(false);
    }, { commit: true });
  });
});
