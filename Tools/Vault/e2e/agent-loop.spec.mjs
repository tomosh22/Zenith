import { execFile as callbackExecFile } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { promisify } from 'node:util';
import { expect, test } from '@playwright/test';
import { withProjectApp } from './helpers/vault-app.mjs';

const execFile = promisify(callbackExecFile);
const cli = fileURLToPath(new URL('../cli/zagent.mjs', import.meta.url));
async function command(root, args) {
  const { stdout } = await execFile(process.execPath, [cli, '--vault', root, ...args, '--json'], { windowsHide: true });
  return JSON.parse(stdout.trim().split(/\r?\n/).at(-1));
}

test('agent claims and comments appear live and a human move revokes the claim', async ({ page }) => {
  await withProjectApp(async ({ root, url }) => {
    await page.goto(`${url}/?project=ZEN`);
    const card = page.getByTestId('planning-issue-card-zen-8');
    await expect(card).toBeVisible();
    const claim = await command(root, ['claim', 'ZEN-8']);
    // CLI write -> watcher -> websocket -> re-fetch -> re-render; generous timeout under load.
    await expect(card.getByRole('combobox', { name: 'Status for ZEN-8' })).toHaveValue('In Progress', { timeout: 30_000 });
    await command(root, ['comment', 'ZEN-8', '--text', 'Agent progress visible through the live watcher']);
    await card.getByRole('button', { name: 'ZEN-8', exact: true }).click();
    await expect(page.getByTestId('issue-panel')).toContainText('Agent progress visible through the live watcher', { timeout: 30_000 });
    await page.getByRole('button', { name: 'Close', exact: true }).click();
    await card.getByRole('combobox', { name: 'Status for ZEN-8' }).selectOption('To Do');
    await expect(card.getByRole('combobox', { name: 'Status for ZEN-8' })).toHaveValue('To Do', { timeout: 30_000 });
    const detail = await (await page.request.get(`${url}/api/issues/ZEN-8`)).json();
    expect(detail.issue.metadata.claimAttempt).toBeFalsy();
    expect(detail.issue.metadata.assignee).toBeFalsy();
    const attempt = claim.attempt ?? claim.claimAttempt ?? claim.result?.attempt;
    expect(attempt).toBeTruthy();
    await expect(command(root, ['move', 'ZEN-8', 'Done', '--attempt', attempt])).rejects.toThrow();
  });
});

test('agent worklog updates preserve specification and replace the previous worklog', async ({ page }) => {
  await withProjectApp(async ({ root, url }) => {
    await page.goto(`${url}/?project=ZEN&issue=ZEN-8`);
    await expect(page.getByTestId('issue-panel')).toBeVisible();
    const before = await (await page.request.get(`${url}/api/issues/ZEN-8`)).json();
    await command(root, ['claim', 'ZEN-8']);
    await command(root, ['finish', 'ZEN-8', '--status', 'Blocked', '--comment', 'First pass', '--worklog', 'First worklog entry', '--actor', 'human']);
    await expect(page.getByTestId('issue-panel')).toContainText('First worklog entry', { timeout: 30_000 });
    await command(root, ['finish', 'ZEN-8', '--status', 'Blocked', '--comment', 'Second pass', '--worklog', 'Replacement worklog entry', '--actor', 'human']);
    await expect(page.getByTestId('issue-panel')).toContainText('Replacement worklog entry', { timeout: 30_000 });
    await expect(page.getByTestId('issue-panel')).not.toContainText('First worklog entry', { timeout: 30_000 });
    const after = await (await page.request.get(`${url}/api/issues/ZEN-8`)).json();
    expect(after.issue.body).toContain(before.issue.body.trim());
  });
});
