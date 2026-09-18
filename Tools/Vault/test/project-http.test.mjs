import { expect, test } from 'vitest';
import { readFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { withProjectApp } from '../e2e/helpers/vault-app.mjs';
import { git } from '../server/operations.mjs';

test('project HTTP reads expose a usable catalog and structured missing issue errors', async () => {
  await withProjectApp(async ({ server }) => {
    const projects = await server.app.inject('/api/projects');
    expect(projects.statusCode).toBe(200);
    expect(projects.json().projects.some(project => project.key === 'ZEN')).toBe(true);
    const issue = await server.app.inject('/api/issues/ZEN-1');
    expect(issue.statusCode).toBe(200);
    expect(issue.json().issue.key).toBe('ZEN-1');
    const missing = await server.app.inject('/api/issues/ZEN-999999');
    expect(missing.statusCode).toBe(404);
    expect(missing.json().error.code).toBe('NOT_FOUND');
    const session = (await server.app.inject('/api/session')).json();
    expect(session.actor.email).toBe('test@example.invalid');
  }, { readOnly: true });
});

test('both mutation routes reject missing CSRF and read-only writes', async () => {
  await withProjectApp(async ({ server }) => {
    for (const url of ['/api/commands', '/api/actions']) {
      expect((await server.app.inject({ method: 'POST', url, payload: {} })).statusCode).toBe(403);
      const response = await server.app.inject({ method: 'POST', url, payload: {}, headers: { 'x-csrf-token': server.csrfToken } });
      expect(response.statusCode).toBe(403);
      expect(response.json().error.code).toBe('READ_ONLY');
    }
  }, { readOnly: true });
});

test('browser command rejects impersonation and missing revision without changing ticket bytes', async () => {
  await withProjectApp(async ({ server, root }) => {
    const issue = (await server.app.inject('/api/issues/DP-1')).json();
    const file = path.join(root, issue.issue.path);
    const before = await readFile(file, 'utf8');
    const request = { args: ['comment', 'DP-1', '--file', 'inline.md'], files: { file: 'Must not be saved' }, revisions: issue.revisions, operationId: 'invalid-comment' };
    const headers = { 'x-csrf-token': server.csrfToken };
    const spoofed = await server.app.inject({ method: 'POST', url: '/api/commands', headers, payload: { ...request, args: [...request.args, '--actor-email', 'other@example.invalid'] } });
    expect(spoofed.statusCode).toBe(422);
    const stale = await server.app.inject({ method: 'POST', url: '/api/commands', headers, payload: { ...request, revisions: {} } });
    expect(stale.statusCode).toBe(409);
    expect(await readFile(file, 'utf8')).toBe(before);
  });
});

test('issue HTTP writes persist atomically, replay safely, and reject an old revision', async () => {
  await withProjectApp(async ({ server, root }) => {
    const headers = { 'x-csrf-token': server.csrfToken };
    const opened = (await server.app.inject('/api/issues/ZEN-8')).json();
    const request = { action: 'issue.update', project: 'ZEN', key: 'ZEN-8', data: { title: 'HTTP edited issue' }, revisions: opened.revisions, operationId: 'http-issue-edit' };
    const post = body => server.app.inject({ method: 'POST', url: '/api/actions', headers, payload: body });
    const saved = await post(request);
    expect(saved.statusCode, saved.body).toBe(200);
    expect(saved.json().result.operationId).toBe('http-issue-edit');
    const bytes = await readFile(path.join(root, opened.issue.path), 'utf8');
    expect(bytes).toContain('HTTP edited issue');
    expect((await post(request)).statusCode).toBe(200);
    expect(await readFile(path.join(root, opened.issue.path), 'utf8')).toBe(bytes);
    const stale = await post({ ...request, data: { title: 'Stale edit' }, operationId: 'http-stale-edit' });
    expect(stale.statusCode, stale.body).toBe(409);
    expect(stale.json().error.code).toBe('STALE');
    const changedRetry = await post({ ...request, data: { title: 'Different retry input' } });
    expect(changedRetry.statusCode).not.toBe(200);
    expect(await readFile(path.join(root, opened.issue.path), 'utf8')).toBe(bytes);
    const refreshed = (await server.app.inject('/api/issues/ZEN-8')).json();
    expect(refreshed.issue.title).toBe('HTTP edited issue');
  });
});

test('a held operation lock makes a concurrent write return 423 LOCKED', async () => {
  await withProjectApp(async ({ server, root }) => {
    // Seed the lock ref exactly as acquire() would leave it: a JSON owner
    // blob whose PID is this live test process, so the lock is never
    // reclaimed and the write is refused deterministically.
    const owner = { pid: process.pid, host: os.hostname(), nonce: 'test-lock', heartbeat: new Date().toISOString() };
    const oid = await git(root, ['hash-object', '-w', '--stdin'], JSON.stringify(owner));
    await git(root, ['update-ref', 'refs/vault/operation-lock', oid]);
    try {
      const headers = { 'x-csrf-token': server.csrfToken };
      const opened = (await server.app.inject('/api/issues/ZEN-8')).json();
      const response = await server.app.inject({
        method: 'POST',
        url: '/api/actions',
        headers,
        payload: { action: 'issue.update', project: 'ZEN', key: 'ZEN-8', data: { title: 'Locked edit' }, revisions: opened.revisions, operationId: 'locked-edit' },
      });
      expect(response.statusCode).toBe(423);
      expect(response.json().error.code).toBe('LOCKED');
    } finally {
      // Drop the ref so later tests in this file are unaffected.
      await git(root, ['update-ref', '-d', 'refs/vault/operation-lock', oid]);
    }
  });
});

test('HTTP action schemas reject unrecognized actions and unsafe inputs', async () => {
  await withProjectApp(async ({ server }) => {
    const headers = { 'x-csrf-token': server.csrfToken };
    const base = { action: 'issue.update', project: 'ZEN', key: 'ZEN-8', data: {}, revisions: {}, operationId: 'invalid-request' };
    for (const [payload, status] of [
      [{ ...base, action: 'file.write' }, 422],
      [{ ...base, revisions: { '../outside.md': null } }, 400],
      [{ ...base, operationId: '../outside' }, 400],
      [{ ...base, data: [] }, 422],
    ]) {
      const response = await server.app.inject({ method: 'POST', url: '/api/actions', headers, payload });
      expect(response.statusCode, response.body).toBe(status);
      expect(response.json().error.message).toBeTruthy();
    }
  });
});
