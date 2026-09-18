import { expect, test } from '@playwright/test';
import { withProjectApp } from './helpers/vault-app.mjs';

test.describe('Vault project planning', () => {
  test('keeps the planning sections project-scoped and navigable', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      await page.goto(`${url}/?project=ZEN&section=board`);
      await expect(page.getByRole('combobox', { name: 'Project', exact: true })).toHaveValue('ZEN');
      await expect(page.getByTestId('planning-board')).toBeVisible();

      const cards = page.locator('[data-testid^="planning-issue-card-"]');
      const project = await page.request.get(`${url}/api/projects/ZEN`).then(response => response.json());
      const expectedKeys = new Set((project.tickets ?? []).map(ticket => ticket.key).filter(Boolean));
      const renderedKeys = await cards.evaluateAll(elements => elements.flatMap(element => Array.from((element.textContent ?? '').matchAll(/[A-Z][A-Z0-9]+-\d+/g), match => match[0])));
      for (const key of renderedKeys) {
        expect(expectedKeys.has(key)).toBeTruthy();
      }

      await page.getByRole('combobox', { name: 'Board grouping' }).selectOption('priority');
      await expect(page.locator('.planning-columns[data-grouping="priority"]')).toBeVisible();
      await page.getByRole('button', { name: 'Backlog', exact: true }).click();
      await expect(page.getByTestId('planning-backlog')).toBeVisible();
      await page.getByRole('button', { name: 'Roadmap', exact: true }).click();
      await expect(page.getByTestId('planning-roadmap')).toBeVisible();
      await page.getByRole('button', { name: 'Releases', exact: true }).click();
      await expect(page.getByTestId('planning-releases')).toBeVisible();
      await page.getByRole('button', { name: 'Reports', exact: true }).click();
      await expect(page.getByTestId('planning-reports')).toBeVisible();
    });
  });

  test('read-only planning hides sprint and release mutations while retaining board controls', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      await page.goto(`${url}/?project=ZEN&section=backlog`);
      await expect(page.getByTestId('planning-backlog')).toBeVisible();
      await expect(page.getByRole('button', { name: 'Create issue', exact: true })).toBeDisabled();
      await expect(page.getByRole('button', { name: 'New release', exact: true })).toHaveCount(0);
      await expect(page.getByRole('textbox', { name: 'New sprint name', exact: true })).toHaveCount(0);
      await page.getByRole('button', { name: 'Releases', exact: true }).click();
      await expect(page.getByTestId('planning-releases')).toBeVisible();
      await expect(page.getByRole('button', { name: 'New release', exact: true })).toHaveCount(0);
    }, { readOnly: true });
  });

  test('moves a board card by native drag and drop with the opened ticket revision', async ({ page }) => {
    await withProjectApp(async ({ url }) => {
      await page.goto(`${url}/?project=ZEN&section=board`);
      const card = page.getByTestId('planning-issue-card-zen-1');
      const doneColumn = page.getByTestId('planning-column-done');
      await expect(card).toBeVisible();
      await expect(doneColumn).toBeVisible();

      const command = page.waitForRequest(request => request.url().endsWith('/api/commands') && request.method() === 'POST');
      await card.dragTo(doneColumn, { targetPosition: { x: 20, y: 20 } });
      const request = await command;
      const payload = request.postDataJSON();
      expect(payload.args).toEqual(['move', 'ZEN-1', 'Done', '--project', 'ZEN']);
      expect(payload.revisions['ZEN/tickets/ZEN-1.md']).toEqual(expect.objectContaining({ hash: expect.any(String) }));
    });
  });
});
