import assert from 'node:assert/strict';
import { execFile as execFileCallback } from 'node:child_process';
import { mkdir, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { promisify } from 'node:util';
import { test } from 'vitest';

import { createVaultServer } from '../server/http.mjs';

const fixtureRoot = new URL('../fixtures/snapshot/vault/', import.meta.url).pathname.replace(/^\/(?:[A-Za-z]:)/, match => match.slice(1));
const execFile = promisify(execFileCallback);

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
---
`;

async function git(root, ...args) {
  await execFile('git', ['-C', root, ...args], { windowsHide: true });
}

async function writableVault() {
  const root = await mkdtemp(path.join(os.tmpdir(), 'zenith-vault-http-write-'));
  await mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await writeFile(path.join(root, 'vault.config.json'), `${JSON.stringify({
    boardRoots: [{ project: 'FD', path: 'FD' }],
    writes: { enabled: true, commit: false },
  }, null, 2)}\n`);
  await writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), '---\nkey: FD-1\ntitle: One\nstatus: To Do\npriority: HIGH\n---\n\n# One\n');
  await writeFile(path.join(root, 'secret.md'), '# Outside board\n');
  await git(root, 'init', '-q');
  await git(root, 'config', 'user.name', 'Vault HTTP tests');
  await git(root, 'config', 'user.email', 'vault-http@example.invalid');
  await git(root, 'add', '.');
  await git(root, 'commit', '-qm', 'baseline');
  return root;
}

async function withServer(callback) {
  const server = await createVaultServer({ vaultRoot: fixtureRoot });
  try { return await callback(server); } finally { await server.close(); }
}

test('HTTP read model exposes session, state, documents and ETags', async () => {
  await withServer(async server => {
    const headers = { host: 'localhost' };
    const health = await server.app.inject({ method: 'GET', url: '/api/health', headers });
    assert.equal(health.statusCode, 200);
    assert.deepEqual(health.json(), { ok: true, valid: true, readOnly: true });

    const session = await server.app.inject({ method: 'GET', url: '/api/session', headers });
    assert.equal(session.statusCode, 200);
    assert.equal(session.json().readOnly, true);
    assert.match(session.json().csrfToken, /^[0-9a-f]{48}$/);

    const state = await server.app.inject({ method: 'GET', url: '/api/state', headers });
    assert.equal(state.statusCode, 200);
    assert.equal(state.json().tickets.length, 244);
    assert.ok(state.json().files.some(file => file.path === 'FD/tickets/FD-1.md'));

    const document = await server.app.inject({ method: 'GET', url: '/api/document?path=FD%2Ftickets%2FFD-1.md', headers });
    assert.equal(document.statusCode, 200);
    assert.equal(document.headers.etag, `"${document.json().revision.hash}"`);
    assert.match(document.json().text, /^---/);

    const cached = await server.app.inject({
      method: 'GET', url: '/api/document?path=FD%2Ftickets%2FFD-1.md',
      headers: { ...headers, 'if-none-match': document.headers.etag },
    });
    assert.equal(cached.statusCode, 304);
  });
});

test('HTTP boundary rejects hostile hosts/origins, escapes, and writes without CSRF', async () => {
  await withServer(async server => {
    const hostileHost = await server.app.inject({ method: 'GET', url: '/api/state', headers: { host: 'evil.example' } });
    assert.equal(hostileHost.statusCode, 400);
    const hostileOrigin = await server.app.inject({ method: 'GET', url: '/api/state', headers: { host: 'localhost', origin: 'https://evil.example' } });
    assert.equal(hostileOrigin.statusCode, 403);
    const wrongPort = await server.app.inject({ method: 'GET', url: '/api/state', headers: { host: 'localhost:4311', origin: 'http://localhost:4312' } });
    assert.equal(wrongPort.statusCode, 403);

    const escape = await server.app.inject({ method: 'GET', url: '/api/document?path=..%2F..%2Fsecret.md', headers: { host: 'localhost' } });
    assert.equal(escape.statusCode, 400);

    const write = await server.app.inject({
      method: 'PUT', url: '/api/document', headers: { host: 'localhost', 'content-type': 'application/json' },
      payload: { path: 'FD/tickets/FD-1.md', text: 'should not write', revision: { hash: 'nope' } },
    });
    assert.equal(write.statusCode, 403);
    assert.equal(write.json().error.code, 'CSRF');
  });
});

test('search endpoint uses the indexed model and returns deterministic records', async () => {
  await withServer(async server => {
    const response = await server.app.inject({
      method: 'GET', url: '/api/search?q=FD-1&kind=ticket&limit=500', headers: { host: '127.0.0.1' },
    });
    assert.equal(response.statusCode, 200);
    const payload = response.json();
    assert.equal(payload.query, 'FD-1');
    assert.ok(payload.results.length >= 1);
    assert.ok(payload.results.some(result => result.metadata.key === 'FD-1'));
  });
});

test('managed HTTP status writes enforce graph edges, revisions, identity and indexed Markdown boundaries', async () => {
  const root = await writableVault();
  const server = await createVaultServer({ vaultRoot: root });
  try {
    const headers = { host: 'localhost' };
    const session = (await server.app.inject({ method: 'GET', url: '/api/session', headers })).json();
    const documentResponse = await server.app.inject({ method: 'GET', url: '/api/document?path=FD%2Ftickets%2FFD-1.md', headers });
    const document = documentResponse.json();
    const mutationHeaders = { ...headers, 'content-type': 'application/json', 'x-csrf-token': session.csrfToken };

    const success = await server.app.inject({
      method: 'POST', url: '/api/status', headers: mutationHeaders,
      payload: { path: document.path, key: 'FD-1', status: 'In Progress', revision: document.revision },
    });
    assert.equal(success.statusCode, 200, success.body);
    assert.equal(success.json().status, 'In Progress');
    assert.match(await readFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), 'utf8'), /status:\s+(?:"In Progress"|In Progress)/);

    const fresh = (await server.app.inject({ method: 'GET', url: '/api/document?path=FD%2Ftickets%2FFD-1.md', headers })).json();
    const forbidden = await server.app.inject({
      method: 'POST', url: '/api/status', headers: mutationHeaders,
      payload: { path: fresh.path, key: 'FD-1', status: 'Done', revision: fresh.revision },
    });
    assert.equal(forbidden.statusCode, 422);
    assert.equal(forbidden.json().error.code, 'INVALID');

    const stale = await server.app.inject({
      method: 'POST', url: '/api/status', headers: mutationHeaders,
      payload: { path: fresh.path, key: 'FD-1', status: 'To Do', revision: document.revision },
    });
    assert.equal(stale.statusCode, 409);
    assert.equal(stale.json().error.code, 'STALE');

    const mismatched = await server.app.inject({
      method: 'POST', url: '/api/status', headers: mutationHeaders,
      payload: { path: fresh.path, key: 'FD-404', status: 'To Do', revision: fresh.revision },
    });
    assert.equal(mismatched.statusCode, 422);
    assert.equal(mismatched.json().error.code, 'INVALID');

    const outside = await server.app.inject({ method: 'GET', url: '/api/document?path=secret.md', headers });
    assert.equal(outside.statusCode, 404);
    const config = await server.app.inject({ method: 'GET', url: '/api/document?path=vault.config.json', headers });
    assert.equal(config.statusCode, 404);
  } finally {
    await server.close();
    await rm(root, { recursive: true, force: true });
  }
});
