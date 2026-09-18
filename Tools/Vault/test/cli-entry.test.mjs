import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test, vi } from 'vitest';

import { runCli } from '../cli/zagent.mjs';
import { git } from '../server/operations.mjs';

const fixtureRoot = fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url));

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: blocked, name: Blocked, category: TODO}
  - {id: review, name: In Review, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
  - {from_status_id: progress, to_status_id: blocked}
  - {from_status_id: progress, to_status_id: review}
  - {from_status_id: progress, to_status_id: done}
  - {from_status_id: progress, to_status_id: todo}
  - {from_status_id: blocked, to_status_id: progress}
  - {from_status_id: review, to_status_id: progress}
  - {from_status_id: review, to_status_id: done}
  - {from_status_id: done, to_status_id: progress}
---
`;

const BODY = `## Goal
Exercise the local entry point.

## Definition of Done
- [ ] The CLI entry is observable

## Gates
- node --version
`;

function ticket({ id, key, title = key, type = 'TASK', status = 'To Do', assignee = null, parent = null, sprint = null, labels = [] }) {
  const fields = {
    id, key, type, title, category: 'Local', parent, sprint, status, assignee,
    priority: 'MEDIUM', complexity: 'SIMPLE', risk: 'LOW', storyPoints: 1,
    position: Number(key.split('-').at(-1)), labels, fixVersions: [], blocks: [], links: [],
    source: { id: `source-${id}`, title: `source ${title}`, updated_at: null },
  };
  return `---\n${Object.entries(fields).map(([name, value]) => `${name}: ${JSON.stringify(value)}`).join('\n')}\n---\n${BODY}`;
}

async function makeVault(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-cli-entry-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await fs.mkdir(path.join(root, 'FD', 'sprints'), { recursive: true });
  await fs.mkdir(path.join(root, 'FD', 'knowledge'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: ['metadata.md'],
    writes: { enabled: true, commit: true, agentEmail: 'agent@example.test' },
    policy: {
      board: { agentEmail: 'agent@example.test', guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 } },
      client: { file: { project: 'FD', baseBranch: 'master', branching: 'direct', merge: { push: false } } },
      resolved: {
        routing: {}, guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 },
        projects: { FD: { repo: root, baseBranch: 'master', branching: 'direct', gates: [], merge: { push: false }, reviewOn: [] } },
      },
    },
  };
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify(config, null, 2));
  await fs.writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await fs.writeFile(path.join(root, 'FD', 'versions.md'), `---\nkind: catalog\nrows:\n  - {id: v1, name: "1.0", status: UNRELEASED}\n---\n`);
  await fs.writeFile(path.join(root, 'FD', 'sprints', 'fd-s1.md'), `---\nkind: sprint\nslug: fd-s1\nname: S1\nstatus: PLANNED\n---\n`);
  await fs.writeFile(path.join(root, 'metadata.md'), `---\nkind: metadata\ntables: {}\n---\n`);
  for (const name of ['questions', 'decisions', 'shortfalls', 'changelog', 'suggestions']) {
    await fs.writeFile(path.join(root, 'FD', 'knowledge', `${name}.md`), `---\nkind: knowledge\ndatabase: ${name}\nrows: []\n---\n`);
  }
  for (const spec of [
    { id: 'one', key: 'FD-1', title: 'One' },
    { id: 'two', key: 'FD-2', title: 'Two' },
    { id: 'three', key: 'FD-3', title: 'Three' },
    { id: 'four', key: 'FD-4', title: 'Four' },
    { id: 'epic', key: 'FD-9', title: 'Epic', type: 'EPIC' },
  ]) {
    await fs.writeFile(path.join(root, 'FD', 'tickets', `${spec.key}.md`), ticket(spec));
  }
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'CLI entry tests']);
  await git(root, ['config', 'user.email', 'cli-entry@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

async function capture(fn) {
  const rows = [];
  const spy = vi.spyOn(process.stdout, 'write').mockImplementation(chunk => {
    rows.push(String(chunk));
    return true;
  });
  try {
    const response = await fn();
    return { response, output: rows.join('') };
  } finally {
    spy.mockRestore();
  }
}

test('help is answered before the vault gate and reports the configured write mode', async t => {
  const root = await makeVault(t);

  const bare = await capture(() => runCli(['--help']));
  assert.equal(bare.response.exitCode, 0);
  assert.equal(bare.response.payload.readOnly, true);
  assert.ok(bare.response.payload.commands.includes('gates'));
  assert.match(bare.output, /"usage":/);

  const enabled = await capture(() => runCli(['claim', '--help', '--vault', root]));
  assert.equal(enabled.response.exitCode, 0);
  assert.equal(enabled.response.payload.readOnly, false);

  const disabledRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-cli-readonly-'));
  t.onTestFinished(() => fs.rm(disabledRoot, { recursive: true, force: true }));
  await fs.writeFile(path.join(disabledRoot, 'vault.config.json'), JSON.stringify({ writes: { enabled: false } }));
  const disabled = await capture(() => runCli(['help', '--vault', disabledRoot]));
  assert.equal(disabled.response.payload.readOnly, true);

  const malformedRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-cli-broken-'));
  t.onTestFinished(() => fs.rm(malformedRoot, { recursive: true, force: true }));
  await fs.writeFile(path.join(malformedRoot, 'vault.config.json'), '{not json');
  const malformed = await capture(() => runCli(['show', '--help', '--vault', malformedRoot]));
  assert.equal(malformed.response.exitCode, 0);
  assert.equal(malformed.response.payload.readOnly, true);
});

test('missing vault, unsupported commands, and flag errors fail closed', async t => {
  const root = await makeVault(t);

  const noVault = await capture(() => runCli(['queue']));
  assert.equal(noVault.response.exitCode, 1);
  assert.equal(noVault.response.payload.error, 'Missing required --vault ROOT');

  for (const command of ['auth', 'provision', 'notion', 'contract']) {
    const refused = await capture(() => runCli([command, '--vault', root]));
    assert.equal(refused.response.exitCode, 1, command);
    assert.match(String(refused.response.payload.error), /is unavailable/);
  }

  for (const sub of ['sync', 'write']) {
    const refused = await capture(() => runCli(['docs', sub, '--vault', root]));
    assert.equal(refused.response.exitCode, 1, sub);
    assert.match(String(refused.response.payload.error), /docs .* is unavailable/);
  }

  const typo = await capture(() => runCli(['queue', '--projectt', 'FD', '--vault', root]));
  assert.equal(typo.response.exitCode, 1);
  assert.match(String(typo.response.payload.error), /Unknown flag/);

  const writeTypo = await capture(() => runCli(['update', 'FD-1', '--bogus', 'x', '--vault', root]));
  assert.equal(writeTypo.response.exitCode, 1);
  assert.match(String(writeTypo.response.payload.error), /Unknown flag.*Vault writes require writes\.enabled=true/s);
});

test('read commands dispatch through the shared read path and the vault= prefix is accepted', async t => {
  const root = await makeVault(t);

  const queue = await capture(() => runCli(['--json', 'queue', `--vault=${root}`]));
  assert.equal(queue.response.exitCode, 0);
  assert.ok(Array.isArray(queue.response.payload));
  assert.ok(queue.response.payload.some(row => row.key === 'FD-1'));

  const show = await capture(() => runCli(['show', 'FD-1', '--vault', root]));
  assert.equal(show.response.exitCode, 0);
  assert.equal(show.response.payload.key, 'FD-1');

  const check = await capture(() => runCli(['check', 'FD-1', '--vault', root]));
  assert.equal(check.response.exitCode, 0);
  assert.equal(check.response.payload.check.targeted.would, true);

  const unknown = await capture(() => runCli(['teleport', '--vault', root]));
  assert.equal(unknown.response.exitCode, 1);
  assert.match(String(unknown.response.payload.error), /Unknown read command/);
});

test('gates and guard read caller files before dispatch', async t => {
  const scratch = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-cli-files-'));
  t.onTestFinished(() => fs.rm(scratch, { recursive: true, force: true }));
  const changedPath = path.join(scratch, 'changed.txt');
  const gatesPath = path.join(scratch, 'gates.json');
  await fs.writeFile(changedPath, 'Games/Foundry/Source/SimCore/FD_Core.cpp\n');
  await fs.writeFile(gatesPath, JSON.stringify({ gates: [] }));

  const gates = await capture(() => runCli(['gates', 'FD-11', '--file', changedPath, '--vault', fixtureRoot]));
  assert.equal(gates.response.exitCode, 0);
  assert.ok(Array.isArray(gates.response.payload.gates));
  assert.ok(gates.response.payload.gates.length > 0);
  assert.deepEqual(gates.response.payload.changed, ['Games/Foundry/Source/SimCore/FD_Core.cpp']);

  // guard replays the recorded gate selection: a fresh selection passes, a
  // stale one is refused with the re-run instruction.
  await fs.writeFile(gatesPath, `${JSON.stringify(gates.response.payload, null, 2)}\n`);
  const guard = await capture(() => runCli(['guard', 'FD-11', '--file', changedPath, '--gates', gatesPath, '--vault', fixtureRoot]));
  assert.equal(guard.response.exitCode, 0, guard.output);
  assert.equal(guard.response.payload.gateSelectionStale, null);

  await fs.writeFile(gatesPath, JSON.stringify({ gates: [] }));
  const staleGuard = await capture(() => runCli(['guard', 'FD-11', '--file', changedPath, '--gates', gatesPath, '--vault', fixtureRoot]));
  assert.equal(staleGuard.response.exitCode, 1);
  assert.match(String(staleGuard.response.payload.gateSelectionStale ?? staleGuard.response.payload.stale ?? ''), /no longer describes|Re-run/);
});

test('write commands persist durable claim scratch inside the vault', async t => {
  const root = await makeVault(t);

  const claim = await capture(() => runCli(['claim', 'FD-1', '--vault', root]));
  assert.equal(claim.response.exitCode, 0, claim.output);
  assert.equal(claim.response.payload.status, 'In Progress');
  const last = JSON.parse(await fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8'));
  assert.equal(last.key, 'FD-1');
  const runTicket = JSON.parse(await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'ticket.json'), 'utf8'));
  assert.equal(runTicket.claimAttempt, claim.response.payload.claimAttempt);
  const runBody = await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'body.md'), 'utf8');
  assert.match(runBody, /## Goal/);
  const attempt = await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'attempt'), 'utf8');
  assert.equal(attempt.trim(), claim.response.payload.claimAttempt);

  const next = await capture(() => runCli(['next', '--vault', root]));
  assert.equal(next.response.exitCode, 3, 'the in-flight claim holds the one-ticket-per-repo lock');
  assert.equal(next.response.payload.error, 'Nothing in "To Do".');

  const finish = await capture(() => runCli([
    'finish', 'FD-1', '--status', 'Done', '--attempt', claim.response.payload.claimAttempt,
    '--comment', 'measured', '--worklog', 'done', '--vault', root,
  ]));
  assert.equal(finish.response.exitCode, 0, finish.output);

  const nextAgain = await capture(() => runCli(['next', '--vault', root]));
  assert.equal(nextAgain.response.exitCode, 0, nextAgain.output);
  assert.equal(nextAgain.response.payload.key, 'FD-2');
  assert.ok(await fs.stat(path.join(root, '.zagent', 'run', 'FD-2', 'attempt')));

  const comment = await capture(() => runCli(['comment', 'FD-2', '--file', 'inline evidence', '--vault', root]));
  assert.equal(comment.response.exitCode, 0, comment.output);
  assert.equal(comment.response.payload.commented, true);
  const commentLast = JSON.parse(await fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8'));
  assert.equal(commentLast.commented, true);

  const drained = await capture(() => runCli(['next', '--vault', root]));
  assert.equal(drained.response.exitCode, 3);
  assert.equal(drained.response.payload.error, 'Nothing in "To Do".');
});

test('history merges board commits and failures are reported as exit 1 payloads', async t => {
  const root = await makeVault(t);
  const claim = await capture(() => runCli(['claim', 'FD-1', '--vault', root]));
  assert.equal(claim.response.exitCode, 0);

  const history = await capture(() => runCli(['history', 'FD-1', '--vault', root]));
  assert.equal(history.response.exitCode, 0, history.output);
  assert.ok(Array.isArray(history.response.payload));
  const statusEvents = history.response.payload.filter(row => row.field === 'status');
  assert.ok(statusEvents.some(row => row.toValue === 'In Progress'));

  const missing = await capture(() => runCli(['show', 'FD-1', '--vault', path.join(root, 'does-not-exist')]));
  assert.equal(missing.response.exitCode, 1);
  assert.ok(String(missing.response.payload.error).length > 0);
});
