import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';

import { dispatchRead } from '../cli/read.js';
import { dispatchWrite } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git, readRevision, saveDocument } from '../server/operations.mjs';
import { patchFields } from '../server/text-edit.mjs';

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
Exercise the local writer.

## Definition of Done
- [ ] The isolated mutation path is observable

## Gates
- node --version
`;

function ticket({ id, key, title = key, type = 'TASK', status = 'To Do', assignee = null, parent = null, sprint = null, blocks = [], links = [], fixVersions = [], updatedAt, labels = [] }) {
  const fields = {
    id, key, type, title, category: 'Local', parent, sprint, status, assignee,
    priority: 'MEDIUM', complexity: 'SIMPLE', risk: 'LOW', storyPoints: 1,
    position: Number(key.split('-').at(-1)), labels, fixVersions, blocks, links,
    ...(updatedAt ? { updatedAt } : {}),
    source: { id: `source-${id}`, title: `source ${title}`, updated_at: updatedAt ?? null },
  };
  return `---\n${Object.entries(fields).map(([name, value]) => `${name}: ${JSON.stringify(value)}`).join('\n')}\n---\n${BODY}`;
}

function metadata() {
  return `---
kind: metadata
tables:
  notion.databases:
    - {id: db-suggestions, title: "Agent · Suggestions"}
  notion.database_properties:
    - id: suggestion-status
      database_id: db-suggestions
      name: Status
      type: SELECT
      options:
        - {id: new, label: new}
        - {id: done, label: done}
  notion.database_rows:
    - id: suggestion-1
      database_id: db-suggestions
      values: {suggestion-status: new}
---
`;
}

async function makeVault(t, { copyFixture = false } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-mutations-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  if (copyFixture) {
    await fs.cp(fixtureRoot, root, { recursive: true });
  } else {
    await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
    await fs.mkdir(path.join(root, 'FD', 'sprints'), { recursive: true });
    await fs.mkdir(path.join(root, 'FD', 'knowledge'), { recursive: true });
    const config = {
      formatVersion: 1,
      boardRoots: [{ project: 'FD', path: 'FD' }],
      supportingFiles: ['metadata.md'],
      writes: { enabled: true, commit: false, agentEmail: 'agent@example.test' },
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
    await fs.writeFile(path.join(root, 'metadata.md'), metadata());
    await fs.writeFile(path.join(root, 'FD', 'knowledge', 'questions.md'), `---\nkind: knowledge\ndatabase: questions\nrows: []\n---\n`);
    await fs.writeFile(path.join(root, 'FD', 'knowledge', 'decisions.md'), `---\nkind: knowledge\ndatabase: decisions\nrows: []\n---\n`);
    await fs.writeFile(path.join(root, 'FD', 'knowledge', 'shortfalls.md'), `---\nkind: knowledge\ndatabase: shortfalls\nrows: []\n---\n`);
    await fs.writeFile(path.join(root, 'FD', 'knowledge', 'changelog.md'), `---\nkind: knowledge\ndatabase: changelog\nrows: []\n---\n`);
    await fs.writeFile(path.join(root, 'FD', 'knowledge', 'suggestions.md'), `---\nkind: knowledge\ndatabase: suggestions\nrows: []\n---\n`);
    for (const spec of [
      { id: 'one', key: 'FD-1', title: 'One' },
      { id: 'two', key: 'FD-2', title: 'Two' },
      { id: 'three', key: 'FD-3', title: 'Three' },
      { id: 'four', key: 'FD-4', title: 'Four' },
      { id: 'epic', key: 'FD-9', title: 'Epic', type: 'EPIC' },
    ]) {
      await fs.writeFile(path.join(root, 'FD', 'tickets', `${spec.key}.md`), ticket(spec));
    }
  }
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Vault mutation tests']);
  await git(root, ['config', 'user.email', 'mutations@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

async function indexAt(root) {
  return loadVault(root);
}

async function write(root, args, files = {}) {
  return dispatchWrite(await indexAt(root), args, files);
}

test('claim, attempt CAS, human takeover comments, and mutable breaker are durable', async t => {
  const root = await makeVault(t);
  const first = await write(root, ['claim', 'FD-1']);
  assert.equal(first.exitCode, 0, JSON.stringify(first.payload));
  assert.equal(first.payload.previousStatus, 'To Do');
  assert.equal(first.payload.status, 'In Progress');
  assert.ok(first.payload.gates, 'claim returns the complete show payload');
  assert.ok(first.payload.claimAttempt);
  const attempt = first.payload.claimAttempt;

  const stale = await write(root, ['finish', 'FD-1', '--status', 'Done', '--attempt', 'old-attempt'], { comment: 'x', worklog: 'x' });
  assert.equal(stale.exitCode, 5);
  const valid = await write(root, ['finish', 'FD-1', '--status', 'Done', '--attempt', attempt], { comment: 'done', worklog: 'measured' });
  assert.equal(valid.exitCode, 0, JSON.stringify(valid.payload));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.status, 'Done');

  const second = await write(root, ['claim', 'FD-2']);
  assert.equal(second.exitCode, 0, JSON.stringify(second.payload));
  const secondAttempt = second.payload.claimAttempt;
  const takeover = await write(root, ['update', 'FD-2', '--assignee', 'human@example.test', '--human']);
  assert.equal(takeover.exitCode, 0, JSON.stringify(takeover.payload));
  assert.equal((await write(root, ['finish', 'FD-2', '--status', 'Done', '--attempt', secondAttempt], { comment: 'lost', worklog: 'lost' })).exitCode, 5);
  const comment = await write(root, ['comment', 'FD-2', '--file', 'abandon.md'], { file: 'abandoned worker evidence' });
  assert.equal(comment.exitCode, 0, JSON.stringify(comment.payload));
  const afterComment = (await indexAt(root)).getTicket('FD-2');
  assert.equal(afterComment.metadata.assignee, 'human@example.test');
  assert.match(afterComment.bodyText, /abandoned worker evidence/);

  // Three recent failure blocks open the breaker. A scope marker breaks the
  // run, matching the loop's distinction between a failed attempt and a
  // ticket parked for human scope clarification.
  const now = new Date().toISOString();
  for (const [key, labels] of [['FD-1', []], ['FD-2', []], ['FD-3', []]]) {
    const record = (await indexAt(root)).getTicket(key);
    const bytes = (await import('../server/text-edit.mjs')).patchFields(record.rawBytes, { status: 'Blocked', assignee: 'agent@example.test', labels, updatedAt: now });
    await fs.writeFile(path.join(root, record.relativePath), bytes);
  }
  let current = await indexAt(root);
  const blockedCheck = dispatchRead(current, ['check', 'FD-4'], {});
  assert.equal(blockedCheck.exitCode, 6);
  assert.equal(blockedCheck.payload.check.breaker.open, true);
  assert.equal((await write(root, ['claim', 'FD-4'])).exitCode, 6, 'targeted claims use the shared breaker exit code');
  assert.equal((await write(root, ['next'])).exitCode, 6, 'queue claims use the shared breaker exit code');
  const scopeRecord = current.getTicket('FD-3');
  await fs.writeFile(path.join(root, scopeRecord.relativePath), (await import('../server/text-edit.mjs')).patchFields(scopeRecord.rawBytes, { labels: ['blocked-scope'] }));
  current = await indexAt(root);
  assert.equal(dispatchRead(current, ['check', 'FD-4'], {}).exitCode, 0);
});

test('human-gate tickets stop agents at In Review and require a human to close', async t => {
  const root = await makeVault(t);
  const { patchFields } = await import('../server/text-edit.mjs');
  let current = await indexAt(root);
  const gated = current.getTicket('FD-4');
  await fs.writeFile(path.join(root, gated.relativePath), patchFields(gated.rawBytes, { labels: ['human-gate'] }));

  const claimed = await write(root, ['claim', 'FD-4']);
  assert.equal(claimed.exitCode, 0, JSON.stringify(claimed.payload));
  const attempt = claimed.payload.claimAttempt;
  const before = await fs.readFile(path.join(root, gated.relativePath));

  const finishDone = await write(root, ['finish', 'FD-4', '--status', 'Done', '--attempt', attempt], { comment: 'green', worklog: 'checked' });
  assert.equal(finishDone.exitCode, 4, JSON.stringify(finishDone.payload));
  assert.match(finishDone.payload.error, /human-gate.*In Review/i);
  const moveDone = await write(root, ['move', 'FD-4', 'Done', '--attempt', attempt]);
  assert.equal(moveDone.exitCode, 4, JSON.stringify(moveDone.payload));
  const updateDone = await write(root, ['update', 'FD-4', '--status', 'Done', '--attempt', attempt]);
  assert.equal(updateDone.exitCode, 4, JSON.stringify(updateDone.payload));
  assert.deepEqual(await fs.readFile(path.join(root, gated.relativePath)), before, 'refused closeout must not write');

  const review = await write(root, ['finish', 'FD-4', '--status', 'In Review', '--attempt', attempt], { comment: 'ready for sign-off', worklog: 'all checks green' });
  assert.equal(review.exitCode, 0, JSON.stringify(review.payload));
  current = await indexAt(root);
  assert.equal(current.getTicket('FD-4').metadata.status, 'In Review');
  const human = await write(root, ['finish', 'FD-4', '--status', 'Done', '--human'], { comment: 'approved', worklog: 'human sign-off' });
  assert.equal(human.exitCode, 0, JSON.stringify(human.payload));
  assert.equal((await indexAt(root)).getTicket('FD-4').metadata.status, 'Done');
});

test('epics cannot enter In Progress through human moves or raw saves, while child tickets can', async t => {
  const root = await makeVault(t);
  const epic = (await indexAt(root)).getTicket('FD-9');
  const epicPath = path.join(root, epic.relativePath);
  const original = await fs.readFile(epicPath);

  const move = await write(root, ['move', 'FD-9', 'In Progress', '--human']);
  assert.equal(move.exitCode, 4, JSON.stringify(move.payload));
  assert.match(move.payload.error, /Epic FD-9 cannot be In Progress/i);
  assert.deepEqual(await fs.readFile(epicPath), original, 'a refused move must preserve the epic bytes');

  const raw = patchFields(original, { status: 'In Progress' });
  await assert.rejects(
    saveDocument(root, epic.relativePath, raw, await readRevision(root, epic.relativePath)),
    error => error.code === 'INVALID' && /Epic FD-9 cannot be In Progress/i.test(error.message),
  );
  assert.deepEqual(await fs.readFile(epicPath), original, 'a refused raw save must preserve the epic bytes');

  const progressChild = await write(root, ['move', 'FD-2', 'In Progress', '--human']);
  assert.equal(progressChild.exitCode, 0, JSON.stringify(progressChild.payload));
  const progressRecord = (await indexAt(root)).getTicket('FD-2');
  const converted = patchFields(
    await fs.readFile(path.join(root, progressRecord.relativePath)),
    { type: 'EPIC' },
  );
  await assert.rejects(
    saveDocument(root, progressRecord.relativePath, converted, await readRevision(root, progressRecord.relativePath)),
    error => error.code === 'INVALID' && /Epic FD-2 cannot be In Progress/i.test(error.message),
  );
  assert.equal((await indexAt(root)).getTicket('FD-2').metadata.type, 'TASK');

  const child = await write(root, ['move', 'FD-1', 'In Progress', '--human']);
  assert.equal(child.exitCode, 0, JSON.stringify(child.payload));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.status, 'In Progress');

  // Existing imported epics may be corrected after the writer cutover. Seed
  // the legacy state directly, then verify a raw correction is accepted.
  await fs.writeFile(epicPath, raw);
  const corrected = patchFields(raw, { status: 'To Do' });
  await saveDocument(root, epic.relativePath, corrected, await readRevision(root, epic.relativePath));
  assert.equal((await indexAt(root)).getTicket('FD-9').metadata.status, 'To Do');
});

test('ticket field, relation, parent, and all status mutation families use the operation protocol', async t => {
  const root = await makeVault(t);
  assert.equal((await write(root, ['claim', 'FD-1'])).exitCode, 0);
  let current = await indexAt(root);
  const attempt = current.getTicket('FD-1').metadata.claimAttempt;
  assert.equal((await write(root, ['update', 'FD-1', '--title', 'Updated', '--label', 'important', '--attempt', attempt])).exitCode, 0);
  assert.equal((await write(root, ['estimate', 'FD-1', '8', '--attempt', attempt])).exitCode, 0);
  assert.equal((await write(root, ['flag', 'FD-1', '--attempt', attempt])).exitCode, 0);
  assert.equal((await write(root, ['resolve', 'FD-1', 'DONE', '--attempt', attempt])).exitCode, 0);
  assert.equal((await write(root, ['move', 'FD-1', 'In Review', '--attempt', attempt])).exitCode, 0);
  assert.equal((await write(root, ['move', 'FD-1', 'In Progress', '--attempt', attempt, '--human'])).exitCode, 0);
  assert.equal((await write(root, ['link', 'FD-1', 'relates', 'FD-2', '--reason', 'context'])).exitCode, 0);
  assert.equal((await write(root, ['link', 'FD-1', 'blocks', 'FD-2', '--reason', 'must land first'])).exitCode, 0);
  assert.equal((await write(root, ['unlink', 'FD-1', 'blocks', 'FD-2'])).exitCode, 0);
  assert.equal((await write(root, ['parent', 'FD-1', 'FD-9'])).exitCode, 0);
  current = await indexAt(root);
  assert.equal(current.getTicket('FD-1').metadata.title, 'Updated');
  assert.equal(current.getTicket('FD-1').metadata.parent, 'FD-9');
  assert.deepEqual(current.getTicket('FD-1').metadata.source.title, 'source One');
  assert.equal((await write(root, ['finish', 'FD-1', '--status', 'Blocked', '--attempt', current.getTicket('FD-1').metadata.claimAttempt], { comment: 'blocked', worklog: 'work' })).exitCode, 0);

  const invalid = await write(root, ['move', 'FD-2', 'In Review']);
  assert.equal(invalid.exitCode, 4);
});

test('parent accepts a typeless ticket on either side of the relation', async t => {
  const root = await makeVault(t);
  await fs.writeFile(path.join(root, 'FD', 'tickets', 'FD-5.md'), '---\nkey: FD-5\ntitle: Typeless\nstatus: To Do\n---\nBody\n');
  // A typeless child parented onto a typed EPIC is not refused.
  const typelessChild = await write(root, ['parent', 'FD-5', 'FD-9']);
  assert.equal(typelessChild.exitCode, 0, JSON.stringify(typelessChild.payload));
  // The mirror case: a typed child parented onto a typeless ticket.
  const typelessParent = await write(root, ['parent', 'FD-1', 'FD-5']);
  assert.equal(typelessParent.exitCode, 0, JSON.stringify(typelessParent.payload));
  const index = await indexAt(root);
  assert.equal(index.getTicket('FD-5').metadata.parent, 'FD-9');
  assert.equal(index.getTicket('FD-1').metadata.parent, 'FD-5');
});

test('create, sprint, version, knowledge, rows, and suggestions mutations cover catalog families', async t => {
  const root = await makeVault(t);
  const created = await write(root, ['create', '--project', 'FD', '--title', 'New ticket', '--text', BODY, '--fix-version', '1.0']);
  assert.equal(created.exitCode, 0, JSON.stringify(created.payload));
  assert.match(created.payload.key, /^FD-\d+$/);
  const createdAgain = await write(root, ['create', '--project', 'FD', '--title', 'New ticket', '--text', BODY]);
  assert.equal(createdAgain.exitCode, 0, JSON.stringify(createdAgain.payload));
  assert.notEqual(createdAgain.payload.key, created.payload.key, 'omitted operation ids represent new create intents');

  assert.equal((await write(root, ['sprint', 'create', 'Release 2', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['sprint', 'start', 'Release 2', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['sprint', 'add', 'Release 2', 'FD-1', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['sprint', 'gate', 'Release 2', 'FD-1', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['sprint', 'remove', 'Release 2', 'FD-1', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['sprint', 'complete', 'Release 2', '--project', 'FD'])).exitCode, 0);

  assert.equal((await write(root, ['version', 'create', '2.0', '--project', 'FD', '--description', 'next'])).exitCode, 0);
  assert.equal((await write(root, ['version', 'release', '2.0', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['version', 'set', '2.0', 'FD-1', '--project', 'FD'])).exitCode, 0);

  for (const [command, body] of [
    ['ask', 'Why?'], ['decide', JSON.stringify({ Decision: 'Use local files' })],
    ['shortfall', JSON.stringify({ Gap: 'none' })], ['changelog', 'Added writer'],
    ['suggest', 'Expose a dry run'],
  ]) {
    const response = await write(root, [command, '--project', 'FD', '--text', body]);
    assert.equal(response.exitCode, 0, `${command}: ${JSON.stringify(response.payload)}`);
    assert.ok(response.payload.rowId);
  }
  const resolved = await write(root, ['rows', 'suggestions', 'resolve', 'suggestion-1', 'done', '--reason', 'implemented']);
  assert.equal(resolved.exitCode, 0, JSON.stringify(resolved.payload));
  const resolvedSuggestion = await write(root, ['suggestions', 'resolve', 'suggestion-1', 'done', '--reason', 'implemented']);
  assert.equal(resolvedSuggestion.exitCode, 0, JSON.stringify(resolvedSuggestion.payload));
});

test('queue claim allocates atomically and explicit operation ids are retry-safe', async t => {
  const root = await makeVault(t);
  const first = await write(root, ['next', '--project', 'FD', '--operation-id', 'queue-retry'], {});
  assert.equal(first.exitCode, 0, JSON.stringify(first.payload));
  const again = await write(root, ['next', '--project', 'FD', '--operation-id', 'queue-retry'], {});
  assert.deepEqual(again.payload.claimAttempt, first.payload.claimAttempt);
  const mismatch = await write(root, ['next', '--project', 'FD', '--operation-id', 'queue-retry', '--category', 'different'], {});
  assert.notEqual(mismatch.exitCode, 0);
  assert.match(String(mismatch.payload.error), /already been used|different inputs|inputs/i);
});

test('concurrent targeted claims have one winner and one stale loser', async t => {
  const root = await makeVault(t);
  const [left, right] = await Promise.all([
    write(root, ['claim', 'FD-1', '--operation-id', 'claim-left']),
    write(root, ['claim', 'FD-1', '--operation-id', 'claim-right']),
  ]);
  const responses = [left, right];
  assert.equal(responses.filter(value => value.exitCode === 0).length, 1, JSON.stringify(responses));
  assert.equal(responses.filter(value => value.exitCode === 5).length, 1, JSON.stringify(responses));
  const final = (await indexAt(root)).getTicket('FD-1');
  assert.equal(final.metadata.status, 'In Progress');
  assert.equal(final.metadata.assignee, 'agent@example.test');
  assert.ok(final.metadata.claimAttempt);
});

test('an agent move onward from In Progress keeps its claim and can still finish', async t => {
  const root = await makeVault(t);
  const claimed = await write(root, ['claim', 'FD-1']);
  assert.equal(claimed.exitCode, 0, JSON.stringify(claimed.payload));
  const attempt = claimed.payload.claimAttempt;

  const moved = await write(root, ['move', 'FD-1', 'In Review', '--attempt', attempt]);
  assert.equal(moved.exitCode, 0, JSON.stringify(moved.payload));
  const record = (await indexAt(root)).getTicket('FD-1');
  assert.equal(record.metadata.status, 'In Review');
  assert.equal(record.metadata.assignee, 'agent@example.test', 'agent progress keeps ownership');
  assert.equal(record.metadata.claimAttempt, attempt, 'agent progress keeps the attempt token');

  // Only a human can act on a ticket parked at In Review; moving it back to
  // In Progress clears nothing because it is not a handbrake out of In
  // Progress.  The agent's ORIGINAL attempt token must still finish the work.
  const resumed = await write(root, ['move', 'FD-1', 'In Progress', '--human']);
  assert.equal(resumed.exitCode, 0, JSON.stringify(resumed.payload));
  const finished = await write(root, ['finish', 'FD-1', '--status', 'Done', '--attempt', attempt], { comment: 'done', worklog: 'measured' });
  assert.equal(finished.exitCode, 0, JSON.stringify(finished.payload));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.status, 'Done');
});

test('a human move out of In Progress clears assignee and claimAttempt and revokes the claim', async t => {
  const root = await makeVault(t);
  const claimed = await write(root, ['claim', 'FD-2']);
  assert.equal(claimed.exitCode, 0, JSON.stringify(claimed.payload));
  const attempt = claimed.payload.claimAttempt;

  const moved = await write(root, ['move', 'FD-2', 'In Review', '--human']);
  assert.equal(moved.exitCode, 0, JSON.stringify(moved.payload));
  const record = (await indexAt(root)).getTicket('FD-2');
  assert.equal(record.metadata.status, 'In Review');
  assert.equal(record.metadata.assignee, null, 'human handbrake returns ownership to the queue');
  assert.equal(record.metadata.claimAttempt, null, 'human handbrake clears the attempt token');

  const stale = await write(root, ['finish', 'FD-2', '--status', 'Done', '--attempt', attempt], { comment: 'lost', worklog: 'lost' });
  assert.equal(stale.exitCode, 5, JSON.stringify(stale.payload));
});
