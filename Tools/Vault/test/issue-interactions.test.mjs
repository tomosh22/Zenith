import { test, expect } from 'vitest';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';

import { loadVault } from '../server/main.js';
import { dispatchIssueInteraction, ISSUE_INTERACTION_ACTIONS, issueComments } from '../server/issue-interactions.mjs';
import { git } from '../server/operations.mjs';
import { expectedRevision } from '../cli/write.mjs';

async function fixture(t) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-issue-interactions-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'DP/tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [{ project: 'DP', path: 'DP' }],
    writes: { enabled: true, commit: false },
  }));
  await fs.writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await fs.writeFile(path.join(root, 'metadata.md'), [
    '---',
    'kind: metadata',
    'tables:',
    '  jira.comments:',
    '    - id: imported-1',
    '      issue_id: source-1',
    '      author_id: user-1',
    '      content: Imported comment is immutable',
    '      created_at: 2026-01-01T00:00:00.000Z',
    '      updated_at: 2026-01-01T00:00:00.000Z',
    '  public.users:',
    '    - id: user-1',
    '      name: Imported Author',
    '      email: imported@example.test',
    '---',
    '',
  ].join('\n'));
  await fs.writeFile(path.join(root, 'DP/tickets/DP-1.md'), [
    '---',
    'key: DP-1',
    'id: source-1',
    'type: EPIC',
    'title: Parent issue',
    'status: To Do',
    'parent: null',
    'blocks:',
    '  - key: DP-2',
    '    reason: Parent work first',
    'links:',
    '  - key: DP-3',
    '    type: RELATES_TO',
    '---',
    'Original body stays byte-for-byte except frontmatter edits.',
    '',
  ].join('\n'));
  await fs.writeFile(path.join(root, 'DP/tickets/DP-2.md'), [
    '---',
    'key: DP-2',
    'id: source-2',
    'type: TASK',
    'title: Child issue',
    'status: To Do',
    'parent: DP-1',
    'blocks: []',
    'links: []',
    '---',
    'Child body',
    '',
  ].join('\n'));
  await fs.writeFile(path.join(root, 'DP/tickets/DP-3.md'), [
    '---',
    'key: DP-3',
    'id: source-3',
    'type: TASK',
    'title: Related issue',
    'status: To Do',
    'parent: null',
    'blocks: []',
    'links:',
    '  - key: DP-1',
    '    type: RELATES_TO',
    '---',
    'Related body',
    '',
  ].join('\n'));
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Issue interaction tests']);
  await git(root, ['config', 'user.email', 'issue-interactions@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
  return root;
}

function actor(email = 'alice@example.test', name = 'Alice') {
  return { id: `local:${email}`, email, name };
}

function requestFor(index, action, key, data, operationId) {
  const issue = index.getTicket(key);
  return {
    action,
    project: 'DP',
    key,
    data,
    revisions: { [issue.relativePath]: expectedRevision(issue) },
    operationId,
  };
}

test('allowlist and read helper join local comments without rewriting imported rows', async t => {
  expect(ISSUE_INTERACTION_ACTIONS).toEqual(new Set(['comment.create', 'comment.delete', 'issue.delete']));
  const root = await fixture(t);
  const beforeMetadata = await fs.readFile(path.join(root, 'metadata.md'));
  const index = await loadVault(root);
  const issue = index.getTicket('DP-1');
  const create = requestFor(index, 'comment.create', 'DP-1', { body: 'Local comment survives refresh' }, 'comment-create-once');
  const first = await dispatchIssueInteraction(index, create, { actor: actor(), commit: false });
  expect(first.result).toMatchObject({ key: 'DP-1', created: true, comment: { id: 'vault-comment-comment-create-once', body: 'Local comment survives refresh' } });

  const next = await loadVault(root);
  const comments = issueComments(next, next.getTicket('DP-1'), actor());
  expect(comments).toHaveLength(2);
  expect(comments[0]).toMatchObject({ id: 'imported-1', body: 'Imported comment is immutable', canDelete: false, origin: 'imported' });
  expect(comments[1]).toMatchObject({ id: 'vault-comment-comment-create-once', body: 'Local comment survives refresh', canDelete: true, origin: 'local' });
  expect(await fs.readFile(path.join(root, 'metadata.md'))).toEqual(beforeMetadata);

  const retry = await dispatchIssueInteraction(next, create, { actor: actor(), commit: false });
  expect(retry).toEqual(first);
  expect(issueComments(await loadVault(root), (await loadVault(root)).getTicket('DP-1'), actor()).filter(row => row.origin === 'local')).toHaveLength(1);
  expect(issue.bodyText).toContain('Original body');
});

test('comment deletion uses strict CAS, local author identity, and idempotent operation IDs', async t => {
  const root = await fixture(t);
  const alice = actor();
  let index = await loadVault(root);
  const created = await dispatchIssueInteraction(index, requestFor(index, 'comment.create', 'DP-1', { body: 'Alice only' }, 'comment-delete-target'), { actor: alice, commit: false });
  index = await loadVault(root);
  const deleteRequest = requestFor(index, 'comment.delete', 'DP-1', { commentId: created.result.id }, 'comment-delete-once');
  const before = await fs.readFile(path.join(root, 'DP/tickets/DP-1.md'));
  await expect(dispatchIssueInteraction(index, deleteRequest, { actor: actor('bob@example.test', 'Bob'), commit: false })).rejects.toMatchObject({ code: 'PERMISSION' });
  expect(await fs.readFile(path.join(root, 'DP/tickets/DP-1.md'))).toEqual(before);

  const first = await dispatchIssueInteraction(index, deleteRequest, { actor: alice, commit: false });
  expect(first.result).toMatchObject({ key: 'DP-1', commentId: created.result.id, deleted: true });
  const after = await loadVault(root);
  expect(issueComments(after, after.getTicket('DP-1'), alice).some(row => row.id === created.result.id)).toBe(false);
  expect(issueComments(after, after.getTicket('DP-1'), alice).some(row => row.id === 'imported-1')).toBe(true);
  expect(await dispatchIssueInteraction(after, deleteRequest, { actor: alice, commit: false })).toEqual(first);

  const importedDelete = requestFor(after, 'comment.delete', 'DP-1', { commentId: 'imported-1' }, 'cannot-delete-imported');
  await expect(dispatchIssueInteraction(after, importedDelete, { actor: alice, commit: false })).rejects.toMatchObject({ code: 'PERMISSION' });

  const stale = requestFor(after, 'comment.create', 'DP-1', { body: 'must refuse stale write' }, 'stale-comment');
  stale.revisions[after.getTicket('DP-1').relativePath] = { hash: 'stale', size: 1 };
  await expect(dispatchIssueInteraction(after, stale, { actor: alice, commit: false })).rejects.toMatchObject({ code: 'STALE' });
});

test('interaction requests require a named actor and reject unknown data without touching the issue', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const issue = index.getTicket('DP-1');
  const before = await fs.readFile(path.join(root, issue.relativePath));
  const request = requestFor(index, 'comment.create', 'DP-1', { body: 'invalid request', extra: true }, 'invalid-interaction');
  await expect(dispatchIssueInteraction(index, request, { actor: actor(), commit: false })).rejects.toMatchObject({ code: 'INVALID' });
  const valid = requestFor(index, 'comment.create', 'DP-1', { body: 'anonymous request' }, 'anonymous-interaction');
  await expect(dispatchIssueInteraction(index, valid, { commit: false })).rejects.toMatchObject({ code: 'AUTH' });
  expect(await fs.readFile(path.join(root, issue.relativePath))).toEqual(before);
});

test('issue deletion removes parent and link references in the same operation', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const request = requestFor(index, 'issue.delete', 'DP-1', {}, 'issue-delete-once');
  const result = await dispatchIssueInteraction(index, request, { actor: actor(), commit: false });
  expect(result.result).toEqual({ key: 'DP-1', deleted: true });

  const next = await loadVault(root);
  expect(next.getTicket('DP-1')).toBeUndefined();
  expect(next.getTicket('DP-2').metadata.parent).toBeNull();
  expect(next.getTicket('DP-3').metadata.links).toEqual([]);
  await expect(fs.stat(path.join(root, 'DP/tickets/DP-1.md'))).rejects.toMatchObject({ code: 'ENOENT' });
  expect(await dispatchIssueInteraction(next, request, { actor: actor(), commit: false })).toEqual(result);
});

// --- Branch coverage for the request/actor/revision gates -------------------

test('normalizeRequest rejects malformed payloads before any vault access', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const actorValue = actor();
  const base = requestFor(index, 'comment.create', 'DP-1', { body: 'x' }, 'normalize-1');
  // Non-object bodies.
  await expect(dispatchIssueInteraction(index, null, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /action request/ });
  await expect(dispatchIssueInteraction(index, [base], { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /action request/ });
  // Unknown top-level input names.
  await expect(dispatchIssueInteraction(index, { ...base, surprise: 1 }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Unknown action input "surprise"/ });
  // Missing/blank action and unsupported action names.
  await expect(dispatchIssueInteraction(index, { ...base, action: '  ' }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /action is required/ });
  await expect(dispatchIssueInteraction(index, { ...base, action: 'issue.move' }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Unsupported issue interaction/ });
  // Malformed issue keys.
  await expect(dispatchIssueInteraction(index, { ...base, key: 'DP1' }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Invalid issue key/ });
  // Non-object data and revisions.
  await expect(dispatchIssueInteraction(index, { ...base, data: 'nope' }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /data must be an object/ });
  await expect(dispatchIssueInteraction(index, { ...base, revisions: [] }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /revisions must be an object/ });
  // Revision paths that escape the vault.
  for (const bad of ['..\\escape.md', '/abs.md', 'a\\b.md', 'ok/../..git/x.md', 'ok/.zagent/x.md', 'ok/\0.md']) {
    await expect(dispatchIssueInteraction(index, { ...base, revisions: { [bad]: null } }, { actor: actorValue })).rejects.toMatchObject({ code: 'PATH', message: /escapes the vault/ });
  }
  // Revision values must be objects or null.
  await expect(dispatchIssueInteraction(index, { ...base, revisions: { 'DP/tickets/DP-1.md': 5 } }, { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /must be an object or null/ });
  // Operation IDs must match the allowlist shape; blank falls back to a UUID.
  await expect(dispatchIssueInteraction(index, { ...base, operationId: 'bad id!' }, { actor: actorValue })).rejects.toMatchObject({ code: 'ID', message: /Invalid operation ID/ });
  const generated = await dispatchIssueInteraction(index, { ...base, operationId: '' }, { actor: actorValue, commit: false });
  expect(generated.operationId).toMatch(/^[a-f0-9-]{36}$/);
});

test('validateData enforces the per-action data vocabulary', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const actorValue = actor();
  // comment.create: body required, whitespace-only refused, over-long refused.
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.create', 'DP-1', {}, 'data-1'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /comment body is required/ });
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.create', 'DP-1', { body: '   ' }, 'data-2'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /comment body is required/ });
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.create', 'DP-1', { body: 'x'.repeat(100001) }, 'data-3'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /too long/ });
  // comment.delete: commentId required and unknown fields refused.
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.delete', 'DP-1', {}, 'data-4'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /commentId is required/ });
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.delete', 'DP-1', { commentId: 'c', extra: 1 }, 'data-5'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Unknown comment.delete data field "extra"/ });
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.create', 'DP-1', { body: 'x', extra: 1 }, 'data-6'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Unknown comment.create data field "extra"/ });
  // issue.delete accepts no data fields at all.
  await expect(dispatchIssueInteraction(index, requestFor(index, 'issue.delete', 'DP-1', { anything: 1 }, 'data-7'), { actor: actorValue })).rejects.toMatchObject({ code: 'INVALID', message: /Unknown issue.delete data field "anything"/ });
});

test('actorValue accepts string and object actors and refuses anonymous ones', async t => {
  const root = await fixture(t);
  let index = await loadVault(root);
  let sequence = 0;
  // Each create mutates the ticket file, so reload the index and rebuild the
  // CAS revision before every attempt.
  const request = async () => {
    index = await loadVault(root);
    return requestFor(index, 'comment.create', 'DP-1', { body: 'actor shapes' }, `actor-shapes-${++sequence}`);
  };
  // String actors are lowercased into a local id.
  const viaString = await dispatchIssueInteraction(index, await request(), { actor: '  Bob@Example.test  ', commit: false });
  expect(viaString.result.comment.author).toEqual({ id: 'local:bob@example.test', name: 'Bob@Example.test', email: 'Bob@Example.test' });
  // Object actors may omit name; id can be derived from the email.
  const viaObject = await dispatchIssueInteraction(index, await request(), { actor: { email: 'carol@example.test' }, commit: false });
  expect(viaObject.result.comment.author).toMatchObject({ id: 'local:carol@example.test', email: 'carol@example.test' });
  expect(viaObject.result.comment.author.name).toBe('carol@example.test');
  // An object actor with an explicit id keeps it.
  const withId = await dispatchIssueInteraction(index, await request(), { actor: { id: 'u-9', name: 'Dan', email: 'dan@example.test' }, commit: false });
  expect(withId.result.comment.author).toMatchObject({ id: 'u-9', name: 'Dan', email: 'dan@example.test' });
  // Refusals: missing, blank, non-string-non-object, object without identity.
  await expect(dispatchIssueInteraction(index, await request(), { commit: false })).rejects.toMatchObject({ code: 'AUTH' });
  await expect(dispatchIssueInteraction(index, await request(), { actor: '   ', commit: false })).rejects.toMatchObject({ code: 'AUTH' });
  await expect(dispatchIssueInteraction(index, await request(), { actor: 42, commit: false })).rejects.toMatchObject({ code: 'AUTH' });
  await expect(dispatchIssueInteraction(index, await request(), { actor: {}, commit: false })).rejects.toMatchObject({ code: 'AUTH' });
  await expect(dispatchIssueInteraction(index, await request(), { actor: [actor()], commit: false })).rejects.toMatchObject({ code: 'AUTH' });
});

test('cross-project scoping and missing issues refuse before a write', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const before = await fs.readFile(path.join(root, 'DP/tickets/DP-1.md'));
  // DP-1 exists but not in project AB.
  await expect(dispatchIssueInteraction(index, { ...requestFor(index, 'comment.create', 'DP-1', { body: 'x' }, 'scope-1'), project: 'AB' }, { actor: actor(), commit: false })).rejects.toMatchObject({ code: 'NOT_FOUND', message: /not found in project AB/ });
  // Unknown keys refuse before the revision gate: with no record there is no
  // editable path, so the code is INVALID rather than STALE.
  const unknownKey = { action: 'comment.create', project: 'DP', key: 'DP-999', data: { body: 'x' }, revisions: {}, operationId: 'scope-2' };
  await expect(dispatchIssueInteraction(index, unknownKey, { actor: actor(), commit: false })).rejects.toMatchObject({ code: 'INVALID', message: /no editable path/ });
  // A missing index root refuses as INVALID.
  await expect(dispatchIssueInteraction({}, requestFor(index, 'comment.create', 'DP-1', { body: 'x' }, 'scope-3'), { actor: actor() })).rejects.toMatchObject({ code: 'INVALID', message: /loaded Vault index/ });
  // Writes disabled refuse as READ_ONLY.
  const readOnly = { ...index, vaultConfig: { ...index.vaultConfig, writes: { enabled: false } } };
  await expect(dispatchIssueInteraction(readOnly, requestFor(index, 'comment.create', 'DP-1', { body: 'x' }, 'scope-4'), { actor: actor() })).rejects.toMatchObject({ code: 'READ_ONLY', message: /writes are disabled/i });
  expect(await fs.readFile(path.join(root, 'DP/tickets/DP-1.md'))).toEqual(before);
});

test('comment deletion covers missing comments and revision aliasing', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const alice = actor();
  // A comment id that matches nothing refuses as NOT_FOUND inside prepare.
  await expect(dispatchIssueInteraction(index, requestFor(index, 'comment.delete', 'DP-1', { commentId: 'no-such-comment' }, 'missing-comment'), { actor: alice, commit: false })).rejects.toMatchObject({ code: 'NOT_FOUND', message: /was not found on DP-1/ });
  // A revision supplied under a case-variant path still matches the record.
  const issue = index.getTicket('DP-1');
  const aliased = requestFor(index, 'comment.create', 'DP-1', { body: 'aliased revision' }, 'aliased-revision');
  const upperPath = issue.relativePath.toUpperCase();
  aliased.revisions = { [upperPath]: expectedRevision(issue) };
  const created = await dispatchIssueInteraction(index, aliased, { actor: alice, commit: false });
  expect(created.result.created).toBe(true);
  // A record without an editable path refuses.
  const pathless = { ...index.getTicket('DP-2'), relativePath: undefined };
  const pathlessIndex = { ...index, getTicket: key => key === 'DP-2' ? pathless : index.getTicket(key) };
  await expect(dispatchIssueInteraction(pathlessIndex, requestFor(pathlessIndex, 'comment.create', 'DP-2', { body: 'x' }, 'pathless'), { actor: alice, commit: false })).rejects.toMatchObject({ code: 'INVALID', message: /no editable path/ });
});

test('issue.delete refuses without any opened revision and honours case-variant aliases', async t => {
  const root = await fixture(t);
  const index = await loadVault(root);
  const issue = index.getTicket('DP-3');
  // No revisions at all: STALE before the operation engine runs.
  await expect(dispatchIssueInteraction(index, { ...requestFor(index, 'issue.delete', 'DP-3', {}, 'delete-no-rev'), revisions: {} }, { actor: actor(), commit: false })).rejects.toMatchObject({ code: 'STALE', message: /opened revision is required/ });
  // A case-variant revision key is accepted through the alias lookup.
  const aliased = requestFor(index, 'issue.delete', 'DP-3', {}, 'delete-aliased');
  aliased.revisions = { [issue.relativePath.toUpperCase()]: expectedRevision(issue) };
  const result = await dispatchIssueInteraction(index, aliased, { actor: actor(), commit: false });
  expect(result.result).toEqual({ key: 'DP-3', deleted: true });
  expect((await loadVault(root)).getTicket('DP-3')).toBeUndefined();
});

test('issueComments tolerates malformed rows and decodes escaped bodies', async t => {
  // issueComments is a pure join over the index tables, so a hand-built index
  // exercises the row shapes a real vault can carry.
  const fakeIndex = {
    tables: new Map(Object.entries({
      'jira.comments': [
        { id: 'imp-1', issue_id: 'src-1', author_id: 'user-1', content: 'Joined author', created_at: '2026-01-01T00:00:00.000Z' },
        { issue_key: 'DP-1', author_id: 'ghost', content: 'Bare author', created_at: '2026-01-02T00:00:00.000Z' },
        { id: 'hexy', issue_key: 'DP-1', content: '\\x4869', created_at: '2026-01-03T00:00:00.000Z' },
      ],
      'public.users': [{ id: 'user-1', name: 'Imported Author', email: 'imported@example.test' }],
    })),
  };
  const record = { project: 'DP', relativePath: 'DP/tickets/DP-1.md', metadata: { key: 'DP-1', id: 'src-1' } };
  // No record: empty list.
  expect(issueComments(fakeIndex, null)).toEqual([]);
  const comments = issueComments(fakeIndex, record, actor());
  expect(comments.map(row => row.id)).toEqual(['imp-1', expect.stringMatching(/^imported-comment-/), 'hexy']);
  // The users table join fills the author; an unknown author id is kept bare.
  expect(comments[0].author).toEqual({ id: 'user-1', name: 'Imported Author', email: 'imported@example.test' });
  expect(comments[1].author).toEqual({ id: 'ghost', name: null, email: null });
  // \x-escaped bodies decode; imported rows are never deletable.
  expect(comments[2]).toMatchObject({ id: 'hexy', body: 'Hi', canDelete: false, origin: 'imported' });
  // Local rows that are not objects or lack id/body are skipped; an explicit
  // canDelete: false wins even for the matching actor.
  const local = {
    ...record,
    metadata: {
      ...record.metadata,
      comments: [
        'not-an-object',
        { body: 'no id' },
        { id: 'only-id' },
        { id: 'locked', body: 'kept but locked', author: { email: 'alice@example.test' }, canDelete: false },
        { id: 'open', body: 'deletable', author: { email: 'alice@example.test' } },
      ],
    },
  };
  const localComments = issueComments(fakeIndex, local, actor()).filter(row => row.origin === 'local');
  expect(localComments).toHaveLength(2);
  expect(localComments.find(row => row.id === 'locked')).toMatchObject({ canDelete: false, origin: 'local' });
  expect(localComments.find(row => row.id === 'open')).toMatchObject({ canDelete: true });
  // A different actor cannot delete a local comment they did not write.
  expect(issueComments(fakeIndex, local, actor('bob@example.test')).find(row => row.id === 'open').canDelete).toBe(false);
});
