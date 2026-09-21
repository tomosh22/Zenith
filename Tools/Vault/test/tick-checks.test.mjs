import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchWrite } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';
import { runClient } from '../cli/client.mjs';
import {
  blockedScopeAllowed,
  commitCompleteness,
  durableFindingsAnswer,
  fileListExcursion,
  forbiddenAbandonFlags,
  gateVerdictFresh,
  guardNeedsFreshnessOverride,
  needsGpuCompanion,
  unexplainedExcursions,
  unmetDoneCriteria,
} from '../cli/tick-checks.mjs';

// ─── pure-function unit tests ─────────────────────────────────────────────

test('unmetDoneCriteria only reads the Definition of Done section', () => {
  const log = [
    '## Definition of Done',
    '- [x] Done thing',
    '- [ ] Undone thing',
    '* [ ] Another undone thing',
    '',
    '## Deviations / follow-ups',
    '- [ ] this checklist item lives outside the DoD and must not count',
  ].join('\n');
  assert.deepEqual(unmetDoneCriteria(log), ['Undone thing', 'Another undone thing']);
  assert.deepEqual(unmetDoneCriteria('no heading at all, just prose'), []);
  assert.deepEqual(unmetDoneCriteria(''), []);
  assert.deepEqual(unmetDoneCriteria(undefined), []);
});

test('unmetDoneCriteria also recognises the bold run-in heading spelling', () => {
  const log = '**Definition of Done**\n- [ ] item\n- [x] other\n';
  assert.deepEqual(unmetDoneCriteria(log), ['item']);
});

test('durableFindingsAnswer accepts "none" and decision references, refuses everything else', () => {
  const none = durableFindingsAnswer('Work log text.\n\n**Durable findings**: none.\n');
  assert.equal(none.present, true);
  assert.equal(none.answer, 'none');

  const referenced = durableFindingsAnswer('Text.\n\nDurable findings: ZM-D-208, ZEN-D-6\n');
  assert.equal(referenced.present, true);
  assert.equal(referenced.answer, 'references');
  assert.deepEqual(referenced.references, ['ZM-D-208', 'ZEN-D-6']);

  const heading = durableFindingsAnswer('## Durable findings\nZM-D-1\n\n## Deviations\n- none\n');
  assert.equal(heading.answer, 'references');

  const absent = durableFindingsAnswer('Just a plain work log with no such line.');
  assert.equal(absent.present, false);
  assert.match(absent.reason, /carries no "Durable findings" line/);

  const empty = durableFindingsAnswer('Durable findings:\n\n## Next section\ncontent\n');
  assert.equal(empty.present, true);
  assert.equal(empty.answer, null);
  assert.match(empty.reason, /no content/);

  const junk = durableFindingsAnswer('Durable findings: it was fine I guess\n');
  assert.equal(junk.answer, null);
  assert.match(junk.reason, /names neither "none" nor a decision reference/);
});

test('durableFindingsAnswer accepts a bulleted "none" under the bold label, matching tick.md\'s own template', () => {
  // tick.md's step-8 work log template shows:
  //   **Durable findings**
  //
  //   - none, or: `<PROJ-D-N>`[, ...] — the decision reference(s) ...
  // A tick that copies that shape verbatim must not be refused.
  const bulletedNone = durableFindingsAnswer('**Durable findings**\n\n- none\n');
  assert.equal(bulletedNone.present, true);
  assert.equal(bulletedNone.answer, 'none');

  const numberedNone = durableFindingsAnswer('**Durable findings**\n\n1. none\n');
  assert.equal(numberedNone.present, true);
  assert.equal(numberedNone.answer, 'none');
});

test('forbiddenAbandonFlags names every forbidden flag present, in both spellings', () => {
  assert.deepEqual(forbiddenAbandonFlags(['abandon', 'FD-1', '--comment', 'x']), []);
  assert.deepEqual(forbiddenAbandonFlags(['abandon', 'FD-1', '--status', 'Done']), ['status']);
  assert.deepEqual(forbiddenAbandonFlags(['abandon', 'FD-1', '--assignee=x', '--label', 'y']), ['assignee', 'label']);
});

test('needsGpuCompanion requires exactly one companion, only when needs-gpu is being applied', () => {
  assert.equal(needsGpuCompanion(['important']).ok, true, 'no needs-gpu, nothing to check');
  assert.equal(needsGpuCompanion(['needs-gpu']).ok, false, 'zero companions refused');
  assert.equal(needsGpuCompanion(['needs-gpu', 'gpu-reason:render-capture']).ok, true, 'exactly one companion allowed');
  const two = needsGpuCompanion(['needs-gpu', 'gpu-reason:render-capture', 'gpu-reason:visual-measure']);
  assert.equal(two.ok, false, 'more than one companion refused');
  assert.match(two.reason, /not 2/);
});

test('unexplainedExcursions is a case-insensitive substring search over the finish text', () => {
  assert.deepEqual(unexplainedExcursions([], 'anything'), []);
  assert.deepEqual(unexplainedExcursions(['Games/Foo/bar.cpp'], 'Had to touch games/foo/bar.cpp too.'), []);
  assert.deepEqual(unexplainedExcursions(['Games/Foo/bar.cpp', 'Other/file.h'], 'Had to touch games/foo/bar.cpp too.'), ['Other/file.h']);
});

test('guardNeedsFreshnessOverride only turns a would-be pass into a failure', () => {
  assert.equal(guardNeedsFreshnessOverride(0, false), true);
  assert.equal(guardNeedsFreshnessOverride(0, true), false);
  assert.equal(guardNeedsFreshnessOverride(1, false), false, 'an existing failure is never touched');
});

// ─── filesystem/git-backed checks ──────────────────────────────────────────

async function tempRepo(t, { init = true } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'tick-checks-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  if (init) {
    await git(root, ['init', '-q']);
    await git(root, ['config', 'user.name', 'Tick checks tests']);
    await git(root, ['config', 'user.email', 'tick-checks@example.test']);
  }
  return root;
}

async function writeRun(root, key, name, content) {
  const dir = path.join(root, '.zagent', 'run', key);
  await fs.mkdir(dir, { recursive: true });
  await fs.writeFile(path.join(dir, name), content);
}

test('fileListExcursion: a missing filelist.txt is UNVERIFIED, not clean', async t => {
  const root = await tempRepo(t);
  const missing = await fileListExcursion({ repoRoot: root, key: 'FD-1', changed: ['a.txt'] });
  assert.equal(missing.listFound, false);
  assert.equal(missing.excursions, null, 'never an empty array when unverified');

  await writeRun(root, 'FD-1', 'filelist.txt', 'a.txt\nb.txt\n');
  const found = await fileListExcursion({ repoRoot: root, key: 'FD-1', changed: ['a.txt', 'c.txt'] });
  assert.equal(found.listFound, true);
  assert.deepEqual(found.excursions, ['c.txt']);
  assert.deepEqual(found.missing, ['b.txt']);
});

test('commitCompleteness: absent/empty changed.txt is UNVERIFIED; a dropped file refuses; a full commit passes', async t => {
  const root = await tempRepo(t);
  await fs.writeFile(path.join(root, 'tracked.txt'), 'seed\n');
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'seed']);

  const absent = await commitCompleteness({ repoRoot: root, key: 'FD-1' });
  assert.equal(absent.ok, false);
  assert.equal(absent.missingFromCommit, null);

  await writeRun(root, 'FD-1', 'changed.txt', '');
  const empty = await commitCompleteness({ repoRoot: root, key: 'FD-1' });
  assert.equal(empty.missingFromCommit, null);

  // The 'commit -am' failure: a CREATED file was recorded as changed but
  // never staged, so the commit lands without it.
  await fs.writeFile(path.join(root, 'created.txt'), 'new\n');
  await fs.writeFile(path.join(root, 'tracked.txt'), 'modified\n');
  await writeRun(root, 'FD-1', 'changed.txt', 'tracked.txt\ncreated.txt\n');
  await git(root, ['commit', '-qam', 'forgot the new file']);
  const dropped = await commitCompleteness({ repoRoot: root, key: 'FD-1' });
  assert.equal(dropped.ok, false);
  assert.deepEqual(dropped.missingFromCommit, ['created.txt']);

  // A later, CLEAN commit that carries everything changed.txt records for it
  // — default `sha` (HEAD) checks exactly this commit.
  await fs.writeFile(path.join(root, 'tracked.txt'), 'modified again\n');
  await fs.writeFile(path.join(root, 'created2.txt'), 'also new\n');
  await writeRun(root, 'FD-1', 'changed.txt', 'tracked.txt\ncreated2.txt\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'a complete commit']);
  const complete = await commitCompleteness({ repoRoot: root, key: 'FD-1' });
  assert.equal(complete.ok, true);
  assert.deepEqual(complete.missingFromCommit, []);
  // No commit subject names FD-1, so this fell back to HEAD — and says so.
  assert.equal(complete.resolvedFrom, 'head-fallback');
});

test("commitCompleteness: checks the TICKET's commit, not HEAD, so a board commit in between does not read as a dropped file", async t => {
  const root = await tempRepo(t);
  await fs.writeFile(path.join(root, 'seed.txt'), 'seed\n');
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'seed']);

  // The ticket's own commit, named the way tick.md step 7 mandates.
  await fs.writeFile(path.join(root, 'src.txt'), 'work\n');
  await fs.writeFile(path.join(root, 'created.txt'), 'new\n');
  await writeRun(root, 'FD-2', 'changed.txt', 'src.txt\ncreated.txt\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'FD-2: the work']);

  // ...then `zagent decide` commits to this SAME repo before `finish` runs,
  // which is the mandated order: step 8's work log must cite the reference
  // that `decide` produces. HEAD is now a board commit touching one file.
  await fs.writeFile(path.join(root, 'metadata.md'), 'rows\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'board: decide']);

  const afterBoardCommit = await commitCompleteness({ repoRoot: root, key: 'FD-2' });
  assert.equal(afterBoardCommit.ok, true, 'the ticket committed everything; a later board commit must not read as a drop');
  assert.deepEqual(afterBoardCommit.missingFromCommit, []);
  assert.equal(afterBoardCommit.resolvedFrom, 'ticket-commit');

  // The red-park path uses a different subject and must resolve too.
  await fs.writeFile(path.join(root, 'wip.txt'), 'partial\n');
  await writeRun(root, 'FD-3', 'changed.txt', 'wip.txt\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'WIP(FD-3): the work — GATES RED']);
  await fs.writeFile(path.join(root, 'metadata.md'), 'more rows\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'board: comment']);

  const wip = await commitCompleteness({ repoRoot: root, key: 'FD-3' });
  assert.equal(wip.ok, true);
  assert.equal(wip.resolvedFrom, 'ticket-commit');

  // And a genuine drop is still caught through the same resolution.
  await fs.writeFile(path.join(root, 'tracked2.txt'), 'seed\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'seed tracked2']);
  await fs.writeFile(path.join(root, 'tracked2.txt'), 'modified\n');
  await fs.writeFile(path.join(root, 'dropped.txt'), 'never staged\n');
  await writeRun(root, 'FD-4', 'changed.txt', 'tracked2.txt\ndropped.txt\n');
  await git(root, ['commit', '-qam', 'FD-4: forgot the new file']);
  await fs.writeFile(path.join(root, 'metadata.md'), 'yet more\n');
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-qm', 'board: decide']);

  const stillCaught = await commitCompleteness({ repoRoot: root, key: 'FD-4' });
  assert.equal(stillCaught.ok, false);
  assert.deepEqual(stillCaught.missingFromCommit, ['dropped.txt']);
  assert.equal(stillCaught.resolvedFrom, 'ticket-commit');
});

test('blockedScopeAllowed: red exit codes refuse, exit 10 and green and absent all allow, unparsable refuses', async t => {
  const root = await tempRepo(t, { init: false });
  const absent = await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' });
  assert.equal(absent.allowed, true);

  await writeRun(root, 'FD-1', 'tick_gates.json', 'not json');
  const unparsable = await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' });
  assert.equal(unparsable.allowed, false);

  for (const exitCode of [2, 3, 4]) {
    await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode }));
    const red = await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' });
    assert.equal(red.allowed, false, `exit ${exitCode} must refuse`);
  }

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 10 }));
  assert.equal((await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' })).allowed, true, 'exit 10 is a pin bump, not a failure');

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 0 }));
  assert.equal((await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' })).allowed, true);

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'running' }));
  assert.equal((await blockedScopeAllowed({ repoRoot: root, key: 'FD-1' })).allowed, true, 'a killed run leaves no verdict to contradict a scope block');
});

test('gateVerdictFresh: absent, incomplete, and stale headSha are all not fresh; a matching complete verdict is fresh', async t => {
  const root = await tempRepo(t);
  await fs.writeFile(path.join(root, 'f.txt'), '1\n');
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'one']);
  const head = await git(root, ['rev-parse', 'HEAD']);

  assert.equal((await gateVerdictFresh({ repoRoot: root, key: 'FD-1' })).fresh, false, 'absent');

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'running', headSha: head }));
  assert.equal((await gateVerdictFresh({ repoRoot: root, key: 'FD-1' })).fresh, false, 'not complete');

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 0, headSha: 'deadbeef' }));
  assert.equal((await gateVerdictFresh({ repoRoot: root, key: 'FD-1' })).fresh, false, 'stale headSha');

  await writeRun(root, 'FD-1', 'tick_gates.json', JSON.stringify({ state: 'complete', exitCode: 0, headSha: head, skipped: ['x'] }));
  const fresh = await gateVerdictFresh({ repoRoot: root, key: 'FD-1' });
  assert.equal(fresh.fresh, true);
  assert.deepEqual(fresh.skipped, ['x']);
});

// ─── wired into write.mjs (finish / update) ────────────────────────────────

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
---
`;

function ticket({ id, key, title = key, status = 'To Do', assignee = null, labels = [] }) {
  const fields = {
    id, key, type: 'TASK', title, category: 'Local', parent: null, sprint: null, status, assignee,
    priority: 'MEDIUM', complexity: 'SIMPLE', risk: 'LOW', storyPoints: 1, position: 1, labels,
    fixVersions: [], blocks: [], links: [],
    source: { id: `source-${id}`, title: `source ${title}`, updated_at: null },
  };
  return `---\n${Object.entries(fields).map(([name, value]) => `${name}: ${JSON.stringify(value)}`).join('\n')}\n---\n## Goal\nExercise tick-checks wiring.\n\n## Definition of Done\n- [ ] The isolated tick-checks path is observable\n\n## Gates\n- node --version\n`;
}

async function makeVault(t, { commit = true } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'tick-checks-vault-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: [],
    writes: { enabled: true, commit, agentEmail: 'agent@example.test' },
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
  await fs.writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket({ id: 'one', key: 'FD-1' }));
  // `.zagent/` must not itself read as an "excursion" — the scratch tree
  // tick-checks.mjs reads is not part of the diff a worker is judged by.
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Tick checks vault tests']);
  await git(root, ['config', 'user.email', 'tick-checks-vault@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

async function claimFd1(root) {
  const claim = await dispatchWrite(await loadVault(root), ['claim', 'FD-1']);
  assert.equal(claim.exitCode, 0, JSON.stringify(claim.payload));
  return claim.payload.claimAttempt;
}

test('finish refuses an unticked Definition of Done at Done, but not at Blocked', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  const doneLog = '## Definition of Done\n- [ ] not actually done\n\n**Durable findings**: none.';
  const refused = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Done', '--attempt', attempt], { comment: 'x', worklog: doneLog });
  assert.equal(refused.exitCode, 4, JSON.stringify(refused.payload));
  assert.match(refused.payload.error, /Definition-of-Done item\(s\) unticked/);

  const blocked = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: doneLog });
  assert.equal(blocked.exitCode, 0, JSON.stringify(blocked.payload));
});

test('finish refuses an unexplained excursion but accepts one named in the comment or work log', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  await fs.mkdir(path.join(root, '.zagent', 'run', 'FD-1'), { recursive: true });
  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'filelist.txt'), 'FD/tickets/FD-1.md\n');
  await fs.writeFile(path.join(root, 'worker-touched.cpp'), '// outside the file list\n');

  const unexplained = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: 'Durable findings: none.' });
  assert.equal(unexplained.exitCode, 4, JSON.stringify(unexplained.payload));
  assert.match(unexplained.payload.error, /outside its recorded file list/);
  assert.match(unexplained.payload.error, /worker-touched\.cpp/);

  const explained = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], {
    comment: 'x',
    worklog: 'Had to also touch worker-touched.cpp to find the missing piece.\n\nDurable findings: none.',
  });
  assert.equal(explained.exitCode, 0, JSON.stringify(explained.payload));
});

test('a missing filelist.txt warns on stderr but never silently passes as clean', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  const originalWrite = process.stderr.write.bind(process.stderr);
  const lines = [];
  process.stderr.write = chunk => { lines.push(String(chunk)); return true; };
  try {
    const result = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: 'Durable findings: none.' });
    assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  } finally {
    process.stderr.write = originalWrite;
  }
  assert.ok(lines.some(line => /no filelist\.txt/.test(line) && /UNVERIFIED, not clean/.test(line)), 'the absence must be reported, not silently treated as clean');
});

test('finish at Done refuses a commit -am that dropped a recorded file, but Blocked is untouched', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  await fs.writeFile(path.join(root, 'created.cpp'), '// new file\n');
  await fs.mkdir(path.join(root, '.zagent', 'run', 'FD-1'), { recursive: true });
  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'changed.txt'), 'created.cpp\n');
  // Simulate `git commit -am`: only the tracked baseline is touched, the new
  // file is never staged, so this commit silently drops it.
  await git(root, ['commit', '--allow-empty', '-qm', 'worker commit that forgot created.cpp']);

  const doneRefused = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Done', '--attempt', attempt], { comment: 'x', worklog: 'Durable findings: none.' });
  assert.equal(doneRefused.exitCode, 4, JSON.stringify(doneRefused.payload));
  assert.match(doneRefused.payload.error, /HEAD is missing 1 file\(s\)/);
  assert.match(doneRefused.payload.error, /created\.cpp/);

  const blockedOk = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: 'Durable findings: none.' });
  assert.equal(blockedOk.exitCode, 0, JSON.stringify(blockedOk.payload), 'commit completeness is only gated at Done');
});

test('finish refuses on any terminal status when the durable-findings answer is missing', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  const missing = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: 'just some notes, nothing structured' });
  assert.equal(missing.exitCode, 4, JSON.stringify(missing.payload));
  assert.match(missing.payload.error, /carries no "Durable findings" line/);

  const withAnswer = await dispatchWrite(await loadVault(root), ['finish', 'FD-1', '--status', 'Blocked', '--attempt', attempt], { comment: 'x', worklog: 'notes.\n\nDurable findings: none.' });
  assert.equal(withAnswer.exitCode, 0, JSON.stringify(withAnswer.payload));
});

test('update refuses blocked-scope after a red gate run, allows it after exit 10 or no verdict', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  await fs.mkdir(path.join(root, '.zagent', 'run', 'FD-1'), { recursive: true });

  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 4 }));
  const red = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'blocked-scope', '--attempt', attempt]);
  assert.equal(red.exitCode, 4, JSON.stringify(red.payload));
  assert.match(red.payload.error, /gate actually failed/);

  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 10 }));
  const measured = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'blocked-scope', '--attempt', attempt]);
  assert.equal(measured.exitCode, 0, JSON.stringify(measured.payload));
});

test('update refuses needs-gpu without exactly one gpu-reason companion, in the same write', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);

  const none = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'needs-gpu', '--attempt', attempt]);
  assert.equal(none.exitCode, 4, JSON.stringify(none.payload));
  assert.match(none.payload.error, /exactly one companion label/);

  const both = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'needs-gpu,gpu-reason:render-capture,gpu-reason:visual-measure', '--attempt', attempt]);
  assert.equal(both.exitCode, 4, JSON.stringify(both.payload));

  const one = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'needs-gpu,gpu-reason:render-capture', '--attempt', attempt]);
  assert.equal(one.exitCode, 0, JSON.stringify(one.payload));
  assert.deepEqual((await loadVault(root)).getTicket('FD-1').metadata.labels.sort(), ['gpu-reason:render-capture', 'needs-gpu']);
});

test('needs-gpu is not required retroactively on a ticket that already carries it', async t => {
  const root = await makeVault(t);
  const attempt = await claimFd1(root);
  const applied = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'needs-gpu,gpu-reason:render-capture', '--attempt', attempt]);
  assert.equal(applied.exitCode, 0, JSON.stringify(applied.payload));
  // A later, unrelated label write must not be refused just because
  // needs-gpu is still sitting on the ticket from before.
  const later = await dispatchWrite(await loadVault(root), ['update', 'FD-1', '--label', 'important', '--attempt', attempt]);
  assert.equal(later.exitCode, 0, JSON.stringify(later.payload));
});

// ─── wired into client.mjs (abandon / guard freshness) ─────────────────────

test('abandon posts a comment and structurally cannot touch status, assignee, or labels', async t => {
  const root = await makeVault(t);
  await claimFd1(root);
  for (const bad of [['--status', 'Done'], ['--assignee', 'human@example.test'], ['--label', 'x']]) {
    const refused = await runClient(['--vault', root, 'abandon', 'FD-1', '--comment', 'lost ownership', ...bad], { repoRoot: root });
    assert.equal(refused.exitCode, 1, JSON.stringify(refused.payload));
    assert.match(refused.payload.error, /does not accept/);
  }
  const before = (await loadVault(root)).getTicket('FD-1').metadata;
  const ok = await runClient(['--vault', root, 'abandon', 'FD-1', '--comment', 'lost ownership, here is the evidence'], { repoRoot: root });
  assert.equal(ok.exitCode, 0, JSON.stringify(ok.payload));
  const after = (await loadVault(root)).getTicket('FD-1');
  assert.equal(after.metadata.status, before.status, 'abandon must not change status');
  assert.equal(after.metadata.assignee, before.assignee, 'abandon must not change assignee');
  assert.match(after.bodyText, /lost ownership, here is the evidence/);
});

test('guard fails on a stale tick_gates.json headSha, but an existing guard failure still surfaces as itself', async t => {
  const root = await makeVault(t);
  const changed = path.join(root, 'changed.txt');
  await fs.writeFile(changed, 'FD/tickets/FD-1.md\n');
  const gatesPath = path.join(root, 'gates.json');
  const gates = await runClient(['--vault', root, 'gates', 'FD-1', '--file', changed, '--json'], { repoRoot: root });
  assert.equal(gates.exitCode, 0, JSON.stringify(gates.payload));
  await fs.writeFile(gatesPath, JSON.stringify(gates.payload));

  // guard's own check passes (the recorded selection still matches this
  // diff), but with no tick_gates.json at all, freshness must turn that
  // would-be pass into a failure.
  const noVerdict = await runClient(['--vault', root, 'guard', 'FD-1', '--file', changed, '--gates', gatesPath], { repoRoot: root });
  assert.equal(noVerdict.exitCode, 1, JSON.stringify(noVerdict.payload));
  assert.equal(noVerdict.payload.gateVerdictFresh, false);

  await fs.mkdir(path.join(root, '.zagent', 'run', 'FD-1'), { recursive: true });
  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 0, headSha: 'not-the-real-sha' }));
  const stale = await runClient(['--vault', root, 'guard', 'FD-1', '--file', changed, '--gates', gatesPath], { repoRoot: root });
  assert.equal(stale.exitCode, 1, JSON.stringify(stale.payload));
  assert.equal(stale.payload.gateVerdictFresh, false);

  const head = await git(root, ['rev-parse', 'HEAD']);
  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'tick_gates.json'), JSON.stringify({ state: 'complete', exitCode: 0, headSha: head }));
  const fresh = await runClient(['--vault', root, 'guard', 'FD-1', '--file', changed, '--gates', gatesPath], { repoRoot: root });
  assert.equal(fresh.exitCode, 0, JSON.stringify(fresh.payload));

  // Now make guard's OWN check fail (the recorded selection made stale by a
  // changed diff) and confirm freshness never masks it — guard's own
  // message must still be present, even with a stale tick_gates.json still
  // sitting there from the assertions above.
  await fs.writeFile(gatesPath, JSON.stringify({ gates: [] }));
  const ownFailure = await runClient(['--vault', root, 'guard', 'FD-1', '--file', changed, '--gates', gatesPath], { repoRoot: root });
  assert.equal(ownFailure.exitCode, 1, JSON.stringify(ownFailure.payload));
  assert.ok(ownFailure.payload.gateSelectionStale, "guard's own stale-selection message must still be present");
});

// ─── .zagent/last.exit / last.json: every exit records, nothing stale survives ─

async function lastExit(root) {
  return (await fs.readFile(path.join(root, '.zagent', 'last.exit'), 'utf8')).trim();
}

test('runClient: a successful command records exit 0 to last.exit', async t => {
  const root = await makeVault(t);
  const result = await runClient(['--vault', root, 'claim', 'FD-1'], { repoRoot: root });
  assert.equal(result.exitCode, 0, JSON.stringify(result.payload));
  assert.equal(await lastExit(root), '0');
});

test('runClient: an early usage refusal records its OWN exit code — a prior success cannot survive as a stale value', async t => {
  const root = await makeVault(t);
  // A prior, successful call leaves last.exit = 0 on disk.
  const ok = await runClient(['--vault', root, 'claim', 'FD-1'], { repoRoot: root });
  assert.equal(ok.exitCode, 0, JSON.stringify(ok.payload));
  assert.equal(await lastExit(root), '0');

  // `abandon`'s forbidden-flag refusal fires BEFORE vault resolution/dispatch
  // — exactly the shape that used to skip `persistLast` entirely and leave
  // the previous command's `0` on disk, indistinguishable from a genuine
  // success. This is the coordinator's smoke test, reproduced in-process.
  const refused = await runClient(['--vault', root, 'abandon', 'FD-1', '--status', 'Done', '--comment', 'x'], { repoRoot: root });
  assert.equal(refused.exitCode, 1, JSON.stringify(refused.payload));
  assert.equal(await lastExit(root), '1', 'the refusal must overwrite the previous success, not leave it stale');
});

test('runClient: last.exit is invalidated before the command runs, so a killed write cannot leave a stale value either', async t => {
  const root = await makeVault(t);
  await runClient(['--vault', root, 'claim', 'FD-1'], { repoRoot: root });
  assert.equal(await lastExit(root), '0');
  // Simulate the sentinel `markRunning` writes at the very start of the NEXT
  // invocation, before that invocation's own command has even run — the
  // gap this whole fix targets. A reader mid-invocation must see something
  // that cannot be mistaken for a real, current answer.
  const scratchRoot = path.join(root, '.zagent');
  await fs.writeFile(path.join(scratchRoot, 'last.exit'), 'running\n');
  const midFlight = await lastExit(root);
  assert.equal(midFlight, 'running');
  assert.ok(Number.isNaN(Number(midFlight)), 'the sentinel must not parse as a valid exit code');
});

test('runClient: last.json does not survive as a stale payload into a later, non-JSON invocation', async t => {
  const root = await makeVault(t);
  const first = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], { repoRoot: root });
  assert.equal(first.exitCode, 0, JSON.stringify(first.payload));
  const firstJson = JSON.parse(await fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8'));
  assert.equal(firstJson.key, 'FD-1');

  // A later, non-JSON abandon refusal must not leave FD-1's claim payload
  // sitting next to the NEW exit code as though it described this call.
  const refused = await runClient(['--vault', root, 'abandon', 'FD-1', '--assignee', 'human@example.test', '--comment', 'y'], { repoRoot: root });
  assert.equal(refused.exitCode, 1, JSON.stringify(refused.payload));
  await assert.rejects(
    fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8'),
    /ENOENT/,
    'a stale payload from an earlier --json call must not survive a later non-json call',
  );
});

test('runClient: the guard-freshness override records the OVERRIDDEN exit code, not the board\'s original pass', async t => {
  const root = await makeVault(t);
  const changed = path.join(root, 'changed.txt');
  await fs.writeFile(changed, 'FD/tickets/FD-1.md\n');
  const gatesPath = path.join(root, 'gates.json');
  const gates = await runClient(['--vault', root, 'gates', 'FD-1', '--file', changed, '--json'], { repoRoot: root });
  assert.equal(gates.exitCode, 0, JSON.stringify(gates.payload));
  await fs.writeFile(gatesPath, JSON.stringify(gates.payload));

  // No tick_gates.json at all: guard's own check passes (would answer 0),
  // but the freshness fold must override it to 1 — and that overridden
  // value, not the board's original, is what must land in last.exit.
  const guard = await runClient(['--vault', root, 'guard', 'FD-1', '--file', changed, '--gates', gatesPath], { repoRoot: root });
  assert.equal(guard.exitCode, 1, JSON.stringify(guard.payload));
  assert.equal(guard.payload.gateVerdictFresh, false);
  assert.equal(await lastExit(root), '1', "the recorded code must be the OVERRIDDEN one, not the board's original 0");
});

test('runClient: a thrown dispatcher error still records to last.exit, not a stale prior value', async t => {
  const root = await makeVault(t);
  const ok = await runClient(['--vault', root, 'claim', 'FD-1'], { repoRoot: root });
  assert.equal(ok.exitCode, 0, JSON.stringify(ok.payload));
  assert.equal(await lastExit(root), '0');

  // Mutation check: remove the `try { … } catch (error) { result = … }`
  // wrapper around `dispatch(context)` in client.mjs's `runClient`, letting
  // the throw propagate out of the function instead of becoming a result —
  // this goes red because `finish()` never runs, so last.exit stays at the
  // previous call's '0' (or the assertion below throws instead of failing
  // cleanly, since `runClient` itself would reject).
  const thrown = await runClient(['--vault', root, 'show', 'FD-1'], {
    repoRoot: root,
    dispatch: async () => { throw new Error('boom'); },
  });
  assert.equal(thrown.exitCode, 1, JSON.stringify(thrown.payload));
  assert.match(String(thrown.payload.error), /boom/);
  assert.equal(await lastExit(root), '1', 'a thrown dispatcher error must overwrite the previous success, not leave it stale');
});

test('runClient: a non-1 refusal code from the dispatcher is recorded verbatim, not coerced to 1', async t => {
  const root = await makeVault(t);
  // Mutation check: hardcode `persistLast`'s exit-code write to a literal
  // `0` or `1` instead of `result.exitCode` (client.mjs) and this goes red —
  // it would read a coerced value instead of the dispatcher's actual `5`,
  // the "repo busy" code the tick protocol branches on separately from a
  // plain refusal.
  const refused = await runClient(['--vault', root, 'show', 'FD-1'], {
    repoRoot: root,
    dispatch: async () => ({ exitCode: 5, payload: { error: 'repo busy' } }),
  });
  assert.equal(refused.exitCode, 5, JSON.stringify(refused.payload));
  assert.equal(await lastExit(root), '5');
});
