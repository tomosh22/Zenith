import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchRead } from '../cli/read.js';
import { dispatchWrite, isWriteCommand, appendSection, bytesOf, expectedRevision, replaceSections } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';
import { patchFields } from '../server/text-edit.mjs';

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

const PLAIN_BODY = 'Just text, with no Goal or Definition of Done headings.\n';

function ticket({ id, key, title = key, type = 'TASK', status = 'To Do', assignee = null, parent = null, sprint = null, labels = [], body = BODY }) {
  const fields = {
    id, key, type, title, category: 'Local', parent, sprint, status, assignee,
    priority: 'MEDIUM', complexity: 'SIMPLE', risk: 'LOW', storyPoints: 1,
    position: Number(key.split('-').at(-1)), labels, fixVersions: [], blocks: [], links: [],
    source: { id: `source-${id}`, title: `source ${title}`, updated_at: null },
  };
  return `---\n${Object.entries(fields).map(([name, value]) => `${name}: ${JSON.stringify(value)}`).join('\n')}\n---\n${body}`;
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
    - id: suggestion-resolution
      database_id: db-suggestions
      name: Resolution
      type: TEXT
  notion.database_rows:
    - id: suggestion-1
      database_id: db-suggestions
      values: {suggestion-status: new}
    - id: row-a1
      database_id: db-suggestions
      values: {suggestion-status: new}
    - id: row-a2
      database_id: db-suggestions
      values: {suggestion-status: new}
---
`;
}

async function makeVault(t, { knowledge = true, metadataFile = true, plainTicket = false } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-vault-write-branches-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await fs.mkdir(path.join(root, 'FD', 'sprints'), { recursive: true });
  if (knowledge) await fs.mkdir(path.join(root, 'FD', 'knowledge'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: metadataFile ? ['metadata.md'] : [],
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
  if (metadataFile) await fs.writeFile(path.join(root, 'metadata.md'), metadata());
  if (knowledge) {
    for (const name of ['questions', 'decisions', 'shortfalls', 'changelog', 'suggestions']) {
      await fs.writeFile(path.join(root, 'FD', 'knowledge', `${name}.md`), `---\nkind: knowledge\ndatabase: ${name}\nrows: []\n---\n`);
    }
  }
  const specs = [
    { id: 'one', key: 'FD-1', title: 'One' },
    { id: 'two', key: 'FD-2', title: 'Two' },
    { id: 'three', key: 'FD-3', title: 'Three' },
    { id: 'four', key: 'FD-4', title: 'Four' },
    { id: 'epic', key: 'FD-9', title: 'Epic', type: 'EPIC' },
  ];
  if (plainTicket) specs.push({ id: 'plain', key: 'FD-5', title: 'Plain', body: PLAIN_BODY });
  for (const spec of specs) {
    await fs.writeFile(path.join(root, 'FD', 'tickets', `${spec.key}.md`), ticket(spec));
  }
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Write branch tests']);
  await git(root, ['config', 'user.email', 'write-branches@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

async function indexAt(root) {
  return loadVault(root);
}

async function write(root, args, files = {}, context = {}) {
  return dispatchWrite(await indexAt(root), args, files, context);
}

function codeOf(result) {
  return `${result.exitCode} ${JSON.stringify(result.payload)}`;
}

test('pure byte helpers cover every input shape', () => {
  assert.deepEqual(bytesOf({ rawBytes: Buffer.from('abc') }).toString(), 'abc');
  assert.deepEqual(bytesOf({ bytes: Buffer.from('bytes') }).toString(), 'bytes');
  assert.deepEqual(bytesOf({ rawBytes: new Uint8Array([104, 105]) }).toString(), 'hi');
  assert.deepEqual(bytesOf({ rawText: 'raw text' }).toString(), 'raw text');
  assert.deepEqual(bytesOf({}).toString(), '');
  assert.deepEqual(bytesOf(null).toString(), '');

  assert.equal(appendSection('', 'Comments', 'first'), '\n## Comments\n\nfirst\n');
  assert.equal(appendSection('no heading yet', 'Comments', 'first'), 'no heading yet\n\n## Comments\n\nfirst\n');
  assert.equal(appendSection('already ends\n', 'Comments', 'second'), 'already ends\n\n## Comments\n\nsecond\n');
  assert.equal(appendSection('## Comments\nexisting\n', 'Comments', 'inserted'), '## Comments\ninserted\nexisting\n');
  assert.equal(appendSection('body', 'Comments', '   '), 'body');

  assert.throws(() => replaceSections('no headings here', { Goal: 'x' }), /Body has no ## Goal section/);

  const revision = expectedRevision({ rawText: 'abc' });
  assert.equal(revision.size, 3);
  assert.ok(revision.hash);
  assert.equal(expectedRevision(null), null);
  assert.equal(expectedRevision({ hash: 'h', size: 2 }).hash, 'h');
});

test('usage, empty, and configuration refusals never reach the operation engine', async t => {
  const root = await makeVault(t);
  assert.equal((await write(root, ['queue'])), null, 'read commands are retained by the read dispatcher');

  assert.equal((await write(root, ['claim'])).exitCode, 1, 'claim without a key is a usage error');
  assert.equal((await write(root, ['update', 'nope', '--title', 'x'])).exitCode, 1);
  assert.match((await write(root, ['update', 'nope', '--title', 'x'])).payload.error, /not a ticket key/);
  assert.equal((await write(root, ['update', 'FD-999', '--title', 'x'])).exitCode, 3);
  assert.equal((await write(root, ['update', 'FD-999', '--title', 'x'])).payload.code, 'EMPTY');
  assert.equal((await write(root, ['comment'])).exitCode, 1);
  assert.equal((await write(root, ['comment', 'FD-1'])).exitCode, 1, 'comment without text is a usage error');
  assert.equal((await write(root, ['finish', 'FD-1', '--status', 'Done'])).exitCode, 1, 'finish needs comment and worklog');
  assert.equal((await write(root, ['move', 'FD-1'])).exitCode, 1);
  assert.equal((await write(root, ['parent'])).exitCode, 1);
  assert.equal((await write(root, ['parent', 'FD-1'])).exitCode, 1);
  assert.equal((await write(root, ['link', 'FD-1'])).exitCode, 1);
  assert.equal((await write(root, ['create', '--title', 'No project key is fine but no title is not', '--project', 'FD'])).exitCode, 0);
  assert.equal((await write(root, ['create'])).exitCode, 1, 'create without a title is a usage error');
  assert.equal((await write(root, ['sprint', 'start'])).exitCode, 1);
  assert.equal((await write(root, ['version', 'release'])).exitCode, 1);
  assert.equal((await write(root, ['version', 'set', '1.0'])).exitCode, 1);
  assert.equal((await write(root, ['update', 'FD-1'])).exitCode, 1);
  assert.match((await write(root, ['update', 'FD-1'])).payload.error, /Nothing to update/);
  assert.equal((await write(root, ['estimate', 'FD-1', 'abc'])).exitCode, 1);
  assert.equal((await write(root, ['resolve', 'FD-1', 'BOGUS'])).exitCode, 1);
  assert.equal((await write(root, ['create', '--project', 'NOPE', '--title', 'X'])).exitCode, 1);
  assert.equal((await write(root, ['create', '--project', 'NOPE', '--title', 'X'])).payload.code, 'CONFIGURATION');
});

test('field values accept the = prefix form, "none" clears, and labels merge', async t => {
  const root = await makeVault(t);
  const prefixed = await write(root, ['update', 'FD-1', '--title=Prefixed title']);
  assert.equal(prefixed.exitCode, 0, codeOf(prefixed));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.title, 'Prefixed title');

  assert.equal((await write(root, ['update', 'FD-1', '--points', 'none'])).exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.storyPoints, null);
  assert.equal((await write(root, ['update', 'FD-1', '--due', 'none'])).exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.dueDate, null);
  assert.equal((await write(root, ['update', 'FD-1', '--category', 'none'])).exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.category, null);
  assert.equal((await write(root, ['update', 'FD-1', '--resolution', 'none'])).exitCode, 0);

  const labels = await write(root, ['update', 'FD-1', '--label', 'alpha', '--unlabel', 'beta']);
  assert.equal(labels.exitCode, 0, codeOf(labels));
  assert.deepEqual((await indexAt(root)).getTicket('FD-1').metadata.labels, ['alpha']);

  const flagged = await write(root, ['update', 'FD-1', '--flag']);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.flagged, true);
  assert.equal((await write(root, ['update', 'FD-1', '--flag', '--clear'])).exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.flagged, false);
  assert.equal(prefixed.exitCode, 0);
  assert.equal(labels.exitCode, 0);
});

test('goal and dod section edits replace, refuse conflicts, and refuse missing headings', async t => {
  const root = await makeVault(t, { plainTicket: true });

  const goal = await write(root, ['update', 'FD-1', '--goal', 'Replaced goal text']);
  assert.equal(goal.exitCode, 0, codeOf(goal));
  assert.match((await indexAt(root)).getTicket('FD-1').bodyText, /## Goal\nReplaced goal text/);

  const dod = await write(root, ['update', 'FD-1', '--dod', '- [ ] Replaced DoD']);
  assert.equal(dod.exitCode, 0, codeOf(dod));
  assert.match((await indexAt(root)).getTicket('FD-1').bodyText, /- \[ \] Replaced DoD/);

  const conflict = await write(root, ['update', 'FD-1', '--body', 'Whole body', '--goal', 'Also this']);
  assert.equal(conflict.exitCode, 1);
  assert.match(conflict.payload.error, /not both/);

  const noHeading = await write(root, ['update', 'FD-5', '--goal', 'No Goal section here']);
  assert.equal(noHeading.exitCode, 1);
  assert.match(noHeading.payload.error, /Body has no ## Goal section/);
});

test('status validation, workflow transitions, and the human gate refuse illegal moves', async t => {
  const root = await makeVault(t);

  const unknown = await write(root, ['move', 'FD-1', 'Nowhere']);
  assert.equal(unknown.exitCode, 4);
  assert.equal(unknown.payload.code, 'INVALID');
  assert.match(unknown.payload.error, /Unknown status "Nowhere"/);

  const illegal = await write(root, ['move', 'FD-1', 'Done']);
  assert.equal(illegal.exitCode, 4);
  assert.match(illegal.payload.error, /Workflow does not permit To Do → Done/);

  // An empty --status reaches ensureStatus through update, not move.
  const emptyStatus = await write(root, ['update', 'FD-1', '--status', '']);
  assert.equal(emptyStatus.exitCode, 1);
  assert.match(emptyStatus.payload.error, /Status must be a non-empty string/);

  const gateRoot = await makeVault(t);
  await fs.writeFile(path.join(gateRoot, 'FD', 'tickets', 'FD-6.md'), ticket({ id: 'six', key: 'FD-6', title: 'Gated', labels: ['human-gate'] }));
  await git(gateRoot, ['add', '.']);
  await git(gateRoot, ['commit', '-qm', 'gated ticket']);
  const claim = await write(gateRoot, ['claim', 'FD-6']);
  assert.equal(claim.exitCode, 0, codeOf(claim));
  const review = await write(gateRoot, ['move', 'FD-6', 'In Review', '--attempt', claim.payload.claimAttempt]);
  assert.equal(review.exitCode, 0, codeOf(review));
  const closeout = await write(gateRoot, ['finish', 'FD-6', '--status', 'Done', '--attempt', claim.payload.claimAttempt], { comment: 'x', worklog: 'y' });
  assert.equal(closeout.exitCode, 4);
  assert.equal(closeout.payload.code, 'HUMAN_GATE');
  assert.match(closeout.payload.error, /human-gate/);
  const humanClose = await write(gateRoot, ['move', 'FD-6', 'Done', '--human']);
  assert.equal(humanClose.exitCode, 0, codeOf(humanClose));
  assert.equal((await indexAt(gateRoot)).getTicket('FD-6').metadata.status, 'Done');
});

test('ownership and attempt tokens gate every agent mutation', async t => {
  const root = await makeVault(t);

  const assignedOther = await write(root, ['update', 'FD-3', '--assignee', 'human@example.test'], {}, { actor: 'human' });
  assert.equal(assignedOther.exitCode, 0, codeOf(assignedOther));
  const stolen = await write(root, ['update', 'FD-3', '--title', 'x']);
  assert.equal(stolen.exitCode, 5);
  assert.equal(stolen.payload.code, 'OWNERSHIP');
  assert.match(stolen.payload.error, /assigned to somebody else/);

  // An unassigned ticket may be field-edited, but an agent cannot MOVE it
  // into In Progress: ownership is required to start work.
  const unassigned = await write(root, ['move', 'FD-2', 'In Progress']);
  assert.equal(unassigned.exitCode, 5);
  assert.match(unassigned.payload.error, /no longer assigned/);

  const claim = await write(root, ['claim', 'FD-1']);
  assert.equal(claim.exitCode, 0, codeOf(claim));
  const stale = await write(root, ['update', 'FD-1', '--title', 'x', '--attempt', 'wrong-attempt']);
  assert.equal(stale.exitCode, 5);
  assert.match(stale.payload.error, /claim attempt is stale/);
  const current = await write(root, ['update', 'FD-1', '--title', 'x', '--attempt', claim.payload.claimAttempt]);
  assert.equal(current.exitCode, 0, codeOf(current));

  const backToQueue = await write(root, ['move', 'FD-1', 'To Do', '--attempt', claim.payload.claimAttempt]);
  assert.equal(backToQueue.exitCode, 0, codeOf(backToQueue));
  const cleared = (await indexAt(root)).getTicket('FD-1').metadata;
  assert.equal(cleared.assignee, null);
  assert.equal(cleared.status, 'To Do');

  // A pre-token ticket (In Progress, agent-assigned, no claimAttempt) is
  // still finishable: the first local claim introduces the token.
  const record = (await indexAt(root)).getTicket('FD-4');
  const bytes = patchFields(record.rawBytes, { status: 'In Progress', assignee: 'agent@example.test' });
  await fs.writeFile(path.join(root, record.relativePath), bytes);
  const legacy = await write(root, ['update', 'FD-4', '--title', 'legacy owner']);
  assert.equal(legacy.exitCode, 0, codeOf(legacy));
});

test('the circuit breaker stops queue claims with exit 6', async t => {
  const root = await makeVault(t);
  const now = new Date().toISOString();
  for (const key of ['FD-1', 'FD-2', 'FD-3']) {
    const record = (await indexAt(root)).getTicket(key);
    const bytes = patchFields(record.rawBytes, { status: 'Blocked', assignee: 'agent@example.test', updatedAt: now });
    await fs.writeFile(path.join(root, record.relativePath), bytes);
  }
  const queued = await write(root, ['next']);
  assert.equal(queued.exitCode, 6);
  assert.equal(queued.payload.code, 'BREAKER');
  assert.match(queued.payload.error, /circuit breaker is open after 3/);
});

test('create writes the ticket, its parent rule, and reverse blocked-by edges', async t => {
  const root = await makeVault(t);

  const missingReason = await write(root, ['create', '--project', 'FD', '--title', 'Blocked', '--blocks', 'FD-1']);
  assert.equal(missingReason.exitCode, 4);
  assert.match(missingReason.payload.error, /needs --reason/);

  const created = await write(root, ['create', '--project', 'FD', '--title', 'Created', '--file body text', '--type', 'BUG',
    '--parent', 'FD-9', '--blocks', 'FD-1', '--blocked-by', 'FD-2', '--reason', 'order', '--sprint', 'fd-s1',
    '--fix-version', '1.0', '--points', '3', '--severity', 'S1_CRITICAL', '--repro', 'ALWAYS', '--due', '2026-01-02',
    '--environment', 'lab', '--label', 'a,b', '--assignee', 'someone@example.test', '--category', 'cat',
    '--status', 'Blocked', '--flag']);
  assert.equal(created.exitCode, 0, codeOf(created));
  assert.equal(created.payload.key, 'FD-10');
  const fresh = (await indexAt(root)).getTicket('FD-10').metadata;
  assert.equal(fresh.type, 'BUG');
  assert.equal(fresh.parent, 'FD-9');
  assert.equal(fresh.sprint, 'fd-s1');
  assert.deepEqual(fresh.fixVersions, ['1.0']);
  assert.equal(fresh.storyPoints, 3);
  assert.equal(fresh.flagged, true);
  assert.equal(fresh.severity, 'S1_CRITICAL');
  assert.equal(fresh.reproducibility, 'ALWAYS');
  assert.equal(fresh.dueDate, '2026-01-02T00:00:00.000Z');
  assert.equal(fresh.environment, 'lab');
  assert.deepEqual(fresh.blocks, [{ key: 'FD-1', reason: 'order', invalid: false, raw: { key: 'FD-1', reason: 'order' } }]);
  const blocker = (await indexAt(root)).getTicket('FD-2').metadata.blocks;
  assert.deepEqual(blocker, [{ key: 'FD-10', reason: 'order', invalid: false, raw: { key: 'FD-10', reason: 'order' } }]);

  // A create without the bug fields writes the null/false defaults.
  const plain = await write(root, ['create', '--project', 'FD', '--title', 'Plain create']);
  assert.equal(plain.exitCode, 0, codeOf(plain));
  const plainFields = (await indexAt(root)).getTicket('FD-11').metadata;
  assert.equal(plainFields.flagged, false);
  assert.equal(plainFields.severity, null);
  assert.equal(plainFields.reproducibility, null);
  assert.equal(plainFields.dueDate, null);
  assert.equal(plainFields.environment, null);

  const badParent = await write(root, ['create', '--project', 'FD', '--title', 'Bad parent', '--parent', 'FD-1']);
  assert.equal(badParent.exitCode, 4);
  assert.match(badParent.payload.error, /hierarchy/);

  const dangling = await write(root, ['create', '--project', 'FD', '--title', 'Dangling', '--blocks', 'FD-999', '--reason', 'r']);
  assert.equal(dangling.exitCode, 3);
});

test('link and unlink cover created, backfilled, ignored, and removed outcomes', async t => {
  const root = await makeVault(t);

  const noReason = await write(root, ['link', 'FD-1', 'blocks', 'FD-2']);
  assert.equal(noReason.exitCode, 4);
  assert.match(noReason.payload.error, /needs --reason/);

  const usage = await write(root, ['link', 'FD-1', 'bogus-verb', 'FD-2']);
  assert.equal(usage.exitCode, 1);
  assert.match(usage.payload.error, /Usage: zagent link/);

  const self = await write(root, ['link', 'FD-1', 'relates-to', 'FD-1']);
  assert.equal(self.exitCode, 4);
  assert.match(self.payload.error, /cannot be linked to itself/);

  const dangling = await write(root, ['link', 'FD-1', 'relates-to', 'FD-999']);
  assert.equal(dangling.exitCode, 3);

  const blocks = await write(root, ['link', 'FD-1', 'blocks', 'FD-2', '--reason', 'first']);
  assert.equal(blocks.exitCode, 0, codeOf(blocks));
  assert.equal(blocks.payload.created, true);
  assert.equal(blocks.payload.reasonOutcome, 'recorded');
  const again = await write(root, ['link', 'FD-1', 'blocks', 'FD-2', '--reason', 'other']);
  assert.equal(again.payload.created, false);
  assert.equal(again.payload.reasonOutcome, 'ignored');

  const relates = await write(root, ['link', 'FD-1', 'relates-to', 'FD-3']);
  assert.equal(relates.payload.created, true);
  assert.equal(relates.payload.reasonOutcome, 'unchanged');
  const relatesAgain = await write(root, ['link', 'FD-1', 'relates-to', 'FD-3', '--reason', 'why']);
  assert.equal(relatesAgain.payload.created, false);
  assert.equal(relatesAgain.payload.reasonOutcome, 'ignored');

  // A blocks row recorded without a reason gains one on relink.
  const record = (await indexAt(root)).getTicket('FD-3');
  const bytes = patchFields(record.rawBytes, { blocks: [{ key: 'FD-2' }] });
  await fs.writeFile(path.join(root, record.relativePath), bytes);
  const backfill = await write(root, ['link', 'FD-3', 'blocks', 'FD-2', '--reason', 'late reason']);
  assert.equal(backfill.exitCode, 0, codeOf(backfill));
  assert.equal(backfill.payload.created, false);
  assert.equal(backfill.payload.reasonOutcome, 'recorded');

  const removed = await write(root, ['unlink', 'FD-1', 'blocks', 'FD-2']);
  assert.equal(removed.exitCode, 0, codeOf(removed));
  assert.equal(removed.payload.removed, true);
  const removedAgain = await write(root, ['unlink', 'FD-1', 'blocks', 'FD-2']);
  assert.equal(removedAgain.payload.removed, false);
});

test('parent accepts epic parents, refuses cycles and hierarchy violations', async t => {
  const root = await makeVault(t);

  const set = await write(root, ['parent', 'FD-1', 'FD-9']);
  assert.equal(set.exitCode, 0, codeOf(set));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.parent, 'FD-9');

  const cleared = await write(root, ['parent', 'FD-1', 'none']);
  assert.equal(cleared.exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.parent, null);

  const self = await write(root, ['parent', 'FD-1', 'FD-1']);
  assert.equal(self.exitCode, 4);
  assert.match(self.payload.error, /its own parent/);

  const epicChild = await write(root, ['parent', 'FD-9', 'FD-1']);
  assert.equal(epicChild.exitCode, 4);
  assert.match(epicChild.payload.error, /hierarchy/);

  const dangling = await write(root, ['parent', 'FD-1', 'FD-999']);
  assert.equal(dangling.exitCode, 3);
});

test('sprint lifecycle covers create, start, membership, gate, and completion', async t => {
  const root = await makeVault(t);

  assert.equal(await write(root, ['sprint', 'list']), null, 'sprint list is retained by the read dispatcher');

  const usage = await write(root, ['sprint', 'start']);
  assert.equal(usage.exitCode, 1);

  const created = await write(root, ['sprint', 'create', 'Second Sprint']);
  assert.equal(created.exitCode, 0, codeOf(created));
  assert.equal(created.payload.created, true);
  const createdAgain = await write(root, ['sprint', 'create', 'Second Sprint']);
  assert.equal(createdAgain.payload.created, false);

  const missing = await write(root, ['sprint', 'start', 'Missing']);
  assert.equal(missing.exitCode, 3);

  const started = await write(root, ['sprint', 'start', 'fd-s1']);
  assert.equal(started.exitCode, 0, codeOf(started));
  assert.equal(started.payload.status, 'ACTIVE');
  const startedAgain = await write(root, ['sprint', 'start', 'fd-s1']);
  assert.equal(startedAgain.exitCode, 4);
  assert.match(startedAgain.payload.error, /already active/);

  const addEpic = await write(root, ['sprint', 'add', 'fd-s1', 'FD-9']);
  assert.equal(addEpic.exitCode, 4);
  assert.match(addEpic.payload.error, /EPIC/);

  const add = await write(root, ['sprint', 'add', 'fd-s1', 'FD-1']);
  assert.equal(add.exitCode, 0, codeOf(add));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.sprint, 'fd-s1');

  const incomplete = await write(root, ['sprint', 'complete', 'fd-s1']);
  assert.equal(incomplete.exitCode, 4);
  assert.match(incomplete.payload.error, /unfinished tickets/);

  const plannedComplete = await write(root, ['sprint', 'complete', 'Second Sprint']);
  assert.equal(plannedComplete.exitCode, 4);
  assert.match(plannedComplete.payload.error, /Only an active sprint/);

  const missingDestination = await write(root, ['sprint', 'complete', 'fd-s1', '--destination-sprint', 'Missing']);
  assert.equal(missingDestination.exitCode, 3);

  const selfDestination = await write(root, ['sprint', 'complete', 'fd-s1', '--destination-sprint', 'fd-s1']);
  assert.equal(selfDestination.exitCode, 4);
  assert.match(selfDestination.payload.error, /into itself/);

  const carried = await write(root, ['sprint', 'complete', 'fd-s1', '--destination-sprint', 'Second Sprint']);
  assert.equal(carried.exitCode, 0, codeOf(carried));
  assert.equal(carried.payload.status, 'COMPLETED');
  assert.equal(carried.payload.movedToSprint, 'fd-second-sprint');
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.sprint, 'fd-second-sprint');

  // A completed sprint's scope is frozen: completing Second Sprint (which
  // now carries unfinished FD-1) needs --force, and afterwards membership
  // is a historical record. The sprint must be ACTIVE first — --force does
  // not bypass the active-sprint rule.
  const startSecond = await write(root, ['sprint', 'start', 'Second Sprint']);
  assert.equal(startSecond.exitCode, 0, codeOf(startSecond));
  const frozenComplete = await write(root, ['sprint', 'complete', 'Second Sprint', '--force']);
  assert.equal(frozenComplete.exitCode, 0, codeOf(frozenComplete));
  const frozenScope = await write(root, ['sprint', 'add', 'Second Sprint', 'FD-2']);
  assert.equal(frozenScope.exitCode, 4);
  assert.match(frozenScope.payload.error, /historical record/);

  const gate = await write(root, ['sprint', 'gate', 'Second Sprint', 'FD-2']);
  assert.equal(gate.exitCode, 0, codeOf(gate));
  assert.equal(gate.payload.gate, 'FD-2');
  const gateCleared = await write(root, ['sprint', 'gate', 'Second Sprint', 'none']);
  assert.equal(gateCleared.payload.gate, null);

  const remove = await write(root, ['sprint', 'remove', 'Second Sprint', 'FD-1']);
  assert.equal(remove.exitCode, 0, codeOf(remove));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.sprint, null);

  const unknownSub = await write(root, ['sprint', 'bogus', 'Second Sprint']);
  assert.equal(unknownSub.exitCode, 1);
  assert.match(unknownSub.payload.error, /Usage: zagent sprint list/);

  const missingTicket = await write(root, ['sprint', 'add', 'Second Sprint']);
  assert.equal(missingTicket.exitCode, 1);
});

test('sprint complete carries unfinished work to the backlog with --force', async t => {
  const root = await makeVault(t);
  await write(root, ['sprint', 'start', 'fd-s1']);
  await write(root, ['sprint', 'add', 'fd-s1', 'FD-1']);
  const forced = await write(root, ['sprint', 'complete', 'fd-s1', '--force']);
  assert.equal(forced.exitCode, 0, codeOf(forced));
  assert.equal(forced.payload.movedToBacklog, 1);
  assert.equal(forced.payload.movedToSprint, null);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.sprint, null);
});

test('version commands cover set, create, release, and their refusals', async t => {
  const root = await makeVault(t);

  assert.equal(await write(root, ['version', 'list']), null, 'version list is retained by the read dispatcher');

  const missingCatalogEntry = await write(root, ['version', 'set', 'Missing', 'FD-1']);
  assert.equal(missingCatalogEntry.exitCode, 3);

  const set = await write(root, ['version', 'set', '1.0', 'FD-1']);
  assert.equal(set.exitCode, 0, codeOf(set));
  assert.deepEqual((await indexAt(root)).getTicket('FD-1').metadata.fixVersions, ['1.0']);

  const affects = await write(root, ['version', 'set', '1.0', 'FD-2', '--kind', 'affects']);
  assert.equal(affects.exitCode, 0, codeOf(affects));
  assert.deepEqual((await indexAt(root)).getTicket('FD-2').metadata.affectsVersions, ['1.0']);

  // Setting the same version again keeps it present exactly once.
  const setAgain = await write(root, ['version', 'set', '1.0', 'FD-1']);
  assert.equal(setAgain.exitCode, 0, codeOf(setAgain));
  assert.deepEqual((await indexAt(root)).getTicket('FD-1').metadata.fixVersions, ['1.0']);

  // `version set none` clears the field entirely, for both kinds.
  const cleared = await write(root, ['version', 'set', 'none', 'FD-1']);
  assert.equal(cleared.exitCode, 0, codeOf(cleared));
  assert.deepEqual((await indexAt(root)).getTicket('FD-1').metadata.fixVersions, []);
  const affectsCleared = await write(root, ['version', 'set', 'none', 'FD-2', '--kind', 'affects']);
  assert.equal(affectsCleared.exitCode, 0, codeOf(affectsCleared));
  assert.deepEqual((await indexAt(root)).getTicket('FD-2').metadata.affectsVersions, []);

  const created = await write(root, ['version', 'create', '2.0', '--description', 'Next']);
  assert.equal(created.exitCode, 0, codeOf(created));
  assert.equal(created.payload.created, true);
  const createdAgain = await write(root, ['version', 'create', '2.0']);
  assert.equal(createdAgain.payload.created, false);

  const released = await write(root, ['version', 'release', '1.0']);
  assert.equal(released.exitCode, 0, codeOf(released));
  assert.equal(released.payload.status, 'RELEASED');

  const releaseMissing = await write(root, ['version', 'release', 'Missing']);
  assert.equal(releaseMissing.exitCode, 3);

  const unknownSub = await write(root, ['version', 'bogus', '1.0']);
  assert.equal(unknownSub.exitCode, 1);
  assert.match(unknownSub.payload.error, /Usage: zagent version/);
});

test('estimate, flag, and resolve shorthands map onto update', async t => {
  const root = await makeVault(t);

  const estimate = await write(root, ['estimate', 'FD-1', '5']);
  assert.equal(estimate.exitCode, 0, codeOf(estimate));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.storyPoints, 5);
  const estimateNone = await write(root, ['estimate', 'FD-1', 'none']);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.storyPoints, null);

  const flag = await write(root, ['flag', 'FD-1']);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.flagged, true);
  const unflag = await write(root, ['flag', 'FD-1', '--clear']);
  assert.equal(unflag.exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.flagged, false);

  const resolve = await write(root, ['resolve', 'FD-1', 'WONT_DO']);
  assert.equal(resolve.exitCode, 0, codeOf(resolve));
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.resolution, 'WONT_DO');
  const resolveClear = await write(root, ['resolve', 'FD-1', '--clear']);
  assert.equal(resolveClear.exitCode, 0);
  assert.equal((await indexAt(root)).getTicket('FD-1').metadata.resolution, null);
});

test('knowledge commands append rows, parse JSON, and refuse malformed input', async t => {
  const root = await makeVault(t);

  const noInput = await write(root, ['ask']);
  assert.equal(noInput.exitCode, 1);

  const asked = await write(root, ['ask', '--ticket', 'FD-1', '--text', 'What about the edge case?']);
  assert.equal(asked.exitCode, 0, codeOf(asked));
  assert.equal(asked.payload.database, 'questions');
  assert.equal(asked.payload.values.Ticket, 'FD-1');
  assert.equal(asked.payload.values.Status, 'open');

  // decide and shortfall require a structured JSON body; only ask,
  // changelog and suggest accept a plain --text line.
  const decided = await write(root, ['decide'], { file: JSON.stringify({ decision: 'Use the shared writer' }) });
  assert.equal(decided.exitCode, 0, codeOf(decided));
  assert.equal(decided.payload.database, 'decisions');
  assert.equal(decided.payload.values.decision, 'Use the shared writer');
  assert.equal(decided.payload.values.Reversibility, 'easy');

  const shortfall = await write(root, ['shortfall'], { file: JSON.stringify({ shortfall: 'No GPU available' }) });
  assert.equal(shortfall.exitCode, 0, codeOf(shortfall));
  assert.equal(shortfall.payload.values.Impact, 'medium');
  const changelog = await write(root, ['changelog', '--text', 'Added branch tests']);
  assert.equal(changelog.exitCode, 0);
  const suggested = await write(root, ['suggest', '--text', 'Split the writer tests']);
  assert.equal(suggested.exitCode, 0);
  assert.equal(suggested.payload.database, 'suggestions');

  const json = await write(root, ['ask', '--ticket', 'FD-2'], { file: JSON.stringify({ question: 'Structured?' }) });
  assert.equal(json.exitCode, 0, codeOf(json));
  assert.equal(json.payload.values.question, 'Structured?');

  const badJson = await write(root, ['ask'], { file: '{not json' });
  assert.equal(badJson.exitCode, 1);
  assert.match(badJson.payload.error, /must contain JSON/);

  const unknownCommandShape = await write(root, ['rows', 'suggestions', 'resolve', 'suggestion-1', 'done']);
  assert.equal(unknownCommandShape.exitCode, 1, 'resolve without --reason is refused');
});

test('knowledge falls back to the metadata catalog and to a new knowledge file', async t => {
  const metadataRoot = await makeVault(t, { knowledge: false });
  const viaMetadata = await write(metadataRoot, ['suggest', '--project', 'FD', '--text', 'Catalog fallback']);
  assert.equal(viaMetadata.exitCode, 0, codeOf(viaMetadata));
  assert.equal(viaMetadata.payload.database, 'suggestions');
  assert.equal(viaMetadata.payload.values.suggestion, 'Catalog fallback');
  const updated = await fs.readFile(path.join(metadataRoot, 'metadata.md'), 'utf8');
  assert.match(updated, /"notion\.database_rows"/);
  assert.match(updated, /created_at/);

  const bareRoot = await makeVault(t, { knowledge: false, metadataFile: false });
  const newFile = await write(bareRoot, ['suggest', '--project', 'FD', '--text', 'Fresh file']);
  assert.equal(newFile.exitCode, 0, codeOf(newFile));
  assert.match(await fs.readFile(path.join(bareRoot, 'FD', 'knowledge', 'suggestions.md'), 'utf8'), /Fresh file/);

  const unanchored = await write(bareRoot, ['ask', '--text', 'Where does this go?']);
  assert.equal(unanchored.exitCode, 1);
  assert.equal(unanchored.payload.code, 'CONFIGURATION');
});

test('rows resolve moves a suggestion with a mandatory reason', async t => {
  const root = await makeVault(t);

  const usage = await write(root, ['rows', 'suggestions', 'resolve', 'suggestion-1']);
  assert.equal(usage.exitCode, 1);
  const noReason = await write(root, ['rows', 'suggestions', 'resolve', 'suggestion-1', 'done']);
  assert.equal(noReason.exitCode, 1);
  assert.match(noReason.payload.error, /needs --reason/);

  const unknownDatabase = await write(root, ['rows', 'decisions', 'resolve', 'x', 'done', '--reason', 'r']);
  assert.equal(unknownDatabase.exitCode, 3);

  const unknownRow = await write(root, ['rows', 'suggestions', 'resolve', 'missing-row', 'done', '--reason', 'r']);
  assert.equal(unknownRow.exitCode, 3);

  const ambiguous = await write(root, ['rows', 'suggestions', 'resolve', 'row-a', 'done', '--reason', 'r']);
  assert.equal(ambiguous.exitCode, 1);
  assert.match(ambiguous.payload.error, /matches 2 rows/);

  const resolved = await write(root, ['suggestions', 'resolve', 'suggestion-1', 'done', '--reason', 'landed']);
  assert.equal(resolved.exitCode, 0, codeOf(resolved));
  assert.equal(resolved.payload.to, 'done');
  const updated = await fs.readFile(path.join(root, 'metadata.md'), 'utf8');
  assert.match(updated, /landed/);
});

test('dispatchWrite context plumbing and read-only refusal', async t => {
  const root = await makeVault(t);
  const claim = await write(root, ['claim', 'FD-1']);
  assert.equal(claim.exitCode, 0, codeOf(claim));

  // The scratch runner passes the attempt out of band through context.
  const contextAttempt = dispatchWrite(await indexAt(root), ['finish', 'FD-1', '--status', 'Done'], { comment: 'c', worklog: 'w' }, { attempt: claim.payload.claimAttempt });
  assert.equal((await contextAttempt).exitCode, 0, codeOf(await contextAttempt));

  const humanMove = dispatchWrite(await indexAt(root), ['move', 'FD-2', 'In Progress'], {}, { actor: 'human' });
  assert.equal((await humanMove).exitCode, 0, codeOf(await humanMove));

  const disabled = await indexAt(root);
  disabled.vaultConfig.writes.enabled = false;
  const refused = dispatchWrite(disabled, ['claim', 'FD-3']);
  assert.equal((await refused).exitCode, 1);
  assert.equal((await refused).payload.code, 'READ_ONLY');

  assert.equal(isWriteCommand(['sprint', 'list']), false);
  assert.equal(isWriteCommand(['sprint', 'start', 'x']), true);
  assert.equal(isWriteCommand(['version', 'list']), false);
  assert.equal(isWriteCommand(['rows', 'suggestions', 'resolve', 'x']), true);
  assert.equal(isWriteCommand(['suggestions', 'resolve', 'x']), true);
  assert.equal(isWriteCommand(['queue']), false);
});
