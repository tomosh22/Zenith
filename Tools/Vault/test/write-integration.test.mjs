import { test } from 'vitest';
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';
import { dispatchRead } from '../cli/read.js';
import { dispatchWrite } from '../cli/write.mjs';
import { runClient } from '../cli/client.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-write-integration-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await fs.readFile(configPath)); config.writes = { enabled: true };
  await fs.writeFile(configPath, JSON.stringify(config));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']); await git(root, ['config', 'user.name', 'Vault integration']); await git(root, ['config', 'user.email', 'test@example.invalid']);
  await git(root, ['add', '.']); await git(root, ['commit', '-qm', 'Snapshot']);
  return root;
}

test('real frozen ticket can be claimed, finished and inspected through the shared dispatcher', async t => {
  const root = await fixture(t);
  const claim = await dispatchWrite(await loadVault(root), ['claim', 'ZEN-7']);
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const claimed = await loadVault(root);
  assert.equal(claimed.getTicket('ZEN-7').metadata.status, 'In Progress');
  const attempt = claimed.getTicket('ZEN-7').metadata.claimAttempt;
  assert.ok(attempt, 'Claim must persist its attempt identity');
  assert.equal(dispatchRead(claimed, ['owns', 'ZEN-7']).payload.owned, true);
  const finish = await dispatchWrite(claimed, ['finish', 'ZEN-7', '--status', 'Done', '--attempt', attempt], { comment: 'Integration test completed.', worklog: 'Validated the isolated claim and finish path.\n\n**Durable findings**: none.' });
  assert.equal(finish.exitCode, 0, JSON.stringify(finish.payload));
  assert.equal((await loadVault(root)).getTicket('ZEN-7').metadata.status, 'Done');
  assert.equal(await git(root, ['status', '--porcelain']), '');
});

test('the real launcher client carries claims, automatic gate inputs, and file-based closeout', async t => {
  const root = await fixture(t);
  const invoke = args => runClient(['--vault', root, ...args, '--json'], { cwd: root });
  const claim = await invoke(['claim', 'ZEN-7']);
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const scratch = path.join(root, '.zagent/run/ZEN-7');
  const ticket = JSON.parse(await fs.readFile(path.join(scratch, 'ticket.json'), 'utf8'));
  assert.equal(ticket.claimAttempt, claim.payload.claimAttempt);
  assert.ok(ticket.gates);
  await fs.writeFile(path.join(root, 'worker-change.cpp'), '// isolated integration input\n');
  const gates = await invoke(['gates', 'ZEN-7']);
  assert.equal(gates.exitCode, 0, JSON.stringify(gates.payload));
  assert.ok(gates.payload.changed.includes('worker-change.cpp'));
  // `guard`'s freshness fold (tick-checks.mjs `gateVerdictFresh`) reads
  // `.zagent/run/<KEY>/tick_gates.json` — a real tick writes it via
  // `Tools/tick_gates.ps1` between `gates` and `guard`; reproduce that here
  // so this integration test still exercises a realistic sequence.
  const headSha = await git(root, ['rev-parse', 'HEAD']);
  await fs.writeFile(path.join(scratch, 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 0, headSha, steps: [], skipped: [] }));
  const guard = await invoke(['guard', 'ZEN-7']);
  assert.equal(guard.exitCode, 0, JSON.stringify(guard.payload));
  assert.equal(guard.payload.docsMirrorNeeded, false);
  // `guard` now writes `changed.txt` itself (it already computed this exact
  // set to do its own comparison — see tick.md step 6.4 and CLAUDE.md), and
  // `finish --status Done` cross-checks it against HEAD: a file it recorded
  // as changed but absent from HEAD is exactly the "commit -am" failure mode
  // (tick.md step 7). A real tick commits between guard and finish; do the
  // same here rather than leaving the worker's file uncommitted.
  await git(root, ['add', 'worker-change.cpp']);
  await git(root, ['commit', '-qm', 'ZEN-7: isolated integration input']);
  await fs.writeFile(path.join(scratch, 'result.md'), 'Real client integration completed.');
  await fs.writeFile(path.join(scratch, 'worklog.md'), 'Isolated adapter and scratch test; no application gates claimed.\n\n**Durable findings**: none.');
  const finish = await invoke(['finish', 'ZEN-7', '--status', 'Done', '--comment', '.zagent/run/ZEN-7/result.md', '--worklog', '.zagent/run/ZEN-7/worklog.md']);
  assert.equal(finish.exitCode, 0, JSON.stringify(finish.payload));
  const final = (await loadVault(root)).getTicket('ZEN-7');
  assert.equal(final.metadata.status, 'Done');
  assert.match(final.bodyText, /Real client integration completed/);
  assert.equal((await fs.readFile(path.join(root, '.zagent/last.exit'), 'utf8')).trim(), '0');
  assert.equal(await git(root, ['status', '--porcelain']), '');
});

test('human takeover rejects the old attempt but still accepts an abandonment comment', async t => {
  const root = await fixture(t);
  const claim = await dispatchWrite(await loadVault(root), ['claim', 'ZEN-7']);
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  const current = await loadVault(root), attempt = current.getTicket('ZEN-7').metadata.claimAttempt;
  const takeover = await dispatchWrite(current, ['update', 'ZEN-7', '--assignee', 'alice@example.com', '--actor', 'human']);
  assert.equal(takeover.exitCode, 0, JSON.stringify(takeover.payload));
  const finish = await dispatchWrite(await loadVault(root), ['finish', 'ZEN-7', '--status', 'Done', '--attempt', attempt], { comment: 'must not close', worklog: 'must not close' });
  assert.equal(finish.exitCode, 5, JSON.stringify(finish.payload));
  const comment = await dispatchWrite(await loadVault(root), ['comment', 'ZEN-7', '--file', 'abandon.md'], { file: 'Abandoned after takeover.' });
  assert.equal(comment.exitCode, 0, JSON.stringify(comment.payload));
  const final = (await loadVault(root)).getTicket('ZEN-7');
  assert.equal(final.metadata.assignee, 'alice@example.com');
  assert.equal(final.metadata.status, 'In Progress');
  assert.match(final.bodyText, /Abandoned after takeover/);
});
