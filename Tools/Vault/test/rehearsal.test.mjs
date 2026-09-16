import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { patchFields } from '../server/text-edit.mjs';
import { executeOperation, recoverOperations } from '../server/operations.mjs';
import { runRehearsal } from '../scripts/rehearse.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-rehearsal-test-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'ZEN', 'tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), `${JSON.stringify({
    formatVersion: 1,
    boardRoots: [{ project: 'ZEN', path: 'ZEN' }],
    policy: { resolved: { projects: { ZEN: { project: 'ZEN' } } } },
    writes: { enabled: false },
  })}\n`);
  await fs.writeFile(path.join(root, 'ZEN', 'tickets', 'ZEN-1.md'), '---\nkey: ZEN-1\ntype: TASK\ntitle: Rehearsal\nstatus: To Do\n---\nBody\n');
  return root;
}

function mutation(status, kind) {
  return async ({ root, key, id }) => executeOperation(root, {
    id,
    kind,
    prepare: async index => {
      const record = index.getTicket(key);
      return { changes: [{ path: record.relativePath, bytes: patchFields(record.rawBytes, { status }) }], result: { key, status } };
    },
  });
}

test('rehearsal uses a disposable git checkout and reverses finish before claim', async t => {
  const source = await fixture(t);
  const sourceBytes = await fs.readFile(path.join(source, 'ZEN', 'tickets', 'ZEN-1.md'));
  const result = await runRehearsal({
    vaultRoot: source,
    key: 'ZEN-1',
    operations: {
      claim: mutation('In Progress', 'rehearsal claim'),
      finish: mutation('Done', 'rehearsal finish'),
      recover: async ({ root }) => recoverOperations(root),
    },
  });
  assert.equal(result.temporary, true);
  assert.equal(result.noLiveWrites, true);
  assert.equal(result.git.switched, false);
  assert.deepEqual(result.ticket, { key: 'ZEN-1', actor: 'vault-rehearsal@example.invalid', before: 'To Do', claimed: 'In Progress', finished: 'Done', restored: 'To Do' });
  assert.equal(result.recovery.forwardRecovery.reverted, 2);
  assert.equal(result.recovery.forwardRecovery.restored, true);
  assert.deepEqual(await fs.readFile(path.join(source, 'ZEN', 'tickets', 'ZEN-1.md')), sourceBytes);
});
