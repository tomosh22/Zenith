import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { injectPersistedClaimAttempt, runClient } from '../cli/client.mjs';
import { git } from '../server/operations.mjs';

test('client reads path-valued gates input and persists only compatibility scratch', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-client-test-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  const changed = path.join(root, 'changed.txt');
  await fs.writeFile(changed, 'Games/Foundry/Source/SimCore/FD_Core.cpp\n');
  let received;
  const result = await runClient(
    ['--vault', root, 'gates', 'FD-1', '--file', changed, '--json'],
    {
      repoRoot: root,
      dispatch: async context => {
        received = context;
        return {
          exitCode: 0,
          payload: { state: 'complete', fileBytes: context.files.file.length },
          writes: [{ base: 'scratch', path: 'gates/result.json', content: '{"ok":true}\n' }],
        };
      },
    },
  );
  assert.equal(result.exitCode, 0);
  assert.equal(received.files.file, 'Games/Foundry/Source/SimCore/FD_Core.cpp\n');
  assert.equal(result.payload.fileBytes, received.files.file.length);
  assert.equal(await fs.readFile(path.join(root, '.zagent', 'run', 'gates', 'result.json'), 'utf8'), '{"ok":true}\n');
  assert.equal(await fs.readFile(path.join(root, '.zagent', 'last.exit'), 'utf8'), '0\n');
  assert.deepEqual(JSON.parse(await fs.readFile(path.join(root, '.zagent', 'last.json'), 'utf8')), result.payload);
});

test('claim success creates ticket/body scratch and missing gate files fail before dispatch', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-client-claim-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  let calls = 0;
  const claimed = await runClient(['--vault', root, 'claim', 'FD-1', '--json'], {
    repoRoot: root,
    dispatch: async () => {
      calls += 1;
      return { exitCode: 0, payload: { key: 'FD-1', status: 'In Progress', body: 'Original body\n' }, writes: [] };
    },
  });
  assert.equal(claimed.exitCode, 0);
  assert.equal(calls, 1);
  assert.equal(JSON.parse(await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'ticket.json'), 'utf8')).key, 'FD-1');
  assert.equal(await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'body.md'), 'utf8'), 'Original body\n');

  let calledForMissing = false;
  const missing = await runClient(['--vault', root, 'gates', 'FD-1', '--file', path.join(root, 'missing.txt')], {
    repoRoot: root,
    dispatch: async () => { calledForMissing = true; return { exitCode: 0, payload: {} }; },
  });
  assert.equal(missing.exitCode, 1);
  assert.equal(calledForMissing, false);
  assert.match(missing.payload.error, /Cannot read --file/);
});

test('help is available before a vault is loaded', async () => {
  const result = await runClient(['claim', '--help', '--json']);
  assert.equal(result.exitCode, 0);
  assert.match(result.payload.usage, /--vault ROOT/);
});

test('old claim then finish syntax reuses the persisted attempt and stale scratch stays stale', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-client-attempt-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  const calls = [];
  const dispatch = async context => {
    calls.push(context.args);
    if (context.args[0] === 'claim') return {
      exitCode: 0,
      payload: { key: 'FD-1', status: 'In Progress', claimAttempt: 'attempt-1', body: 'Body\n' },
      writes: [],
    };
    const attempt = context.args[context.args.indexOf('--attempt') + 1];
    return { exitCode: attempt === 'attempt-1' ? 0 : 5, payload: { args: context.args } };
  };
  const claim = await runClient(['--vault', root, 'claim', 'FD-1'], { repoRoot: root, dispatch });
  assert.equal(claim.exitCode, 0);
  const finish = await runClient(['--vault', root, 'finish', 'FD-1', '--status', 'Done', '--comment', 'old comment', '--worklog', 'old log'], { repoRoot: root, dispatch });
  assert.equal(finish.exitCode, 0);
  assert.deepEqual(calls[1].slice(-2), ['--attempt', 'attempt-1']);

  await fs.writeFile(path.join(root, '.zagent', 'run', 'FD-1', 'ticket.json'), '{"key":"FD-1","claimAttempt":"stale-token"}\n');
  const stale = await runClient(['--vault', root, 'finish', 'FD-1', '--status', 'Done', '--comment', 'old comment', '--worklog', 'old log'], { repoRoot: root, dispatch });
  assert.equal(stale.exitCode, 5, 'the stale token reaches the writer and is rejected');
  assert.deepEqual(calls[2].slice(-2), ['--attempt', 'stale-token']);

  const explicit = await injectPersistedClaimAttempt(path.join(root, '.zagent'), ['finish', 'FD-1', '--attempt', 'caller-token']);
  assert.deepEqual(explicit, ['finish', 'FD-1', '--attempt', 'caller-token']);
});

test('gates and guard compute the working-tree set and persist the gate selection', async t => {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-client-gates-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.email', 'client-test@example.invalid']);
  await git(root, ['config', 'user.name', 'Client test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  await fs.writeFile(path.join(root, 'new-file.cpp'), 'created by worker\n');
  const calls = [];
  const dispatch = async context => {
    calls.push({ command: context.args[0], files: context.files });
    if (context.args[0] === 'gates') return { exitCode: 0, payload: { key: 'FD-1', gates: ['build'], changed: context.files.file.split(/\r?\n/).filter(Boolean) } };
    return { exitCode: 0, payload: { key: 'FD-1', protected: true, hasGates: Boolean(context.files.gates) } };
  };
  const gates = await runClient(['--vault', root, 'gates', 'FD-1'], { repoRoot: root, dispatch });
  assert.equal(gates.exitCode, 0);
  assert.ok(calls[0].files.file.includes('new-file.cpp'));
  assert.equal(JSON.parse(await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'gates.json'), 'utf8')).changed[0], 'new-file.cpp');
  const guarded = await runClient(['--vault', root, 'guard', 'FD-1'], { repoRoot: root, dispatch });
  assert.equal(guarded.exitCode, 0);
  assert.equal(calls[1].files.gates !== undefined, true);
  // guard already computes this changed set to do its own comparison; it
  // must write it for the orchestrator rather than relying on tick.md's
  // step 6.4 being followed by hand every time (ZEN-10 finish tripped on
  // exactly this being missing).
  assert.equal(
    await fs.readFile(path.join(root, '.zagent', 'run', 'FD-1', 'changed.txt'), 'utf8'),
    calls[1].files.file,
  );

  await git(root, ['add', 'new-file.cpp']);
  await git(root, ['commit', '-qm', 'worker change']);
  let calledOnEmpty = false;
  const empty = await runClient(['--vault', root, 'guard', 'FD-1'], { repoRoot: root, dispatch: async context => { calledOnEmpty = true; return dispatch(context); } });
  assert.equal(empty.exitCode, 1);
  assert.equal(calledOnEmpty, false);
  assert.match(empty.payload.error, /nothing has changed/i);
});
