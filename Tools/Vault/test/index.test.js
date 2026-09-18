import assert from 'node:assert/strict';
import { mkdtemp, mkdir, open, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { loadVault, parseMarkdown, watchVault } from '../server/main.js';

async function fixture() {
  const root = await mkdtemp(path.join(tmpdir(), 'zenith-vault-index-'));
  await mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await mkdir(path.join(root, 'FD', 'sprints'), { recursive: true });
  await mkdir(path.join(root, 'FD', 'knowledge'), { recursive: true });
  await mkdir(path.join(root, 'FD', 'history'), { recursive: true });
  await writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [{ project: 'FD', path: 'FD' }],
  }));
  return root;
}

async function dispose(root) {
  await rm(root, { recursive: true, force: true });
}

async function waitFor(predicate, message, timeout = 4_000) {
  const deadline = Date.now() + timeout;
  while (!predicate()) {
    if (Date.now() > deadline) throw new Error(message);
    await new Promise(resolve => setTimeout(resolve, 25));
  }
}

function ticket(key, extra = '', body = '') {
  return `---\nkey: ${key}\ntitle: ${key} title\nstatus: To Do\n${extra}---\n${body}`;
}

test('frontmatter parsing preserves exact bytes and unknown source fields', () => {
  const bytes = Buffer.from('---\r\nkey: FD-1\r\ncustom_snake: keep-me\r\n---\r\nBody\r\n');
  const parsed = parseMarkdown(bytes, { path: 'FD/tickets/FD-1.md' });
  assert.deepEqual(parsed.fields, { key: 'FD-1', custom_snake: 'keep-me' });
  assert.equal(Buffer.compare(parsed.rawBytes, bytes), 0);
  assert.equal(parsed.frontmatterRaw.toString(), 'key: FD-1\r\ncustom_snake: keep-me\r\n');
  assert.equal(parsed.bodyRaw.toString(), 'Body\r\n');
  assert.equal(parsed.diagnostics.length, 0);
});

test('a malformed file on a cold scan makes the whole index invalid', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket('FD-1'));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), '---\nkey: [not closed\n---\ncorrupt');

  const index = await loadVault(root);
  assert.equal(index.tickets.has('FD-1'), true);
  assert.equal(index.valid, false);
  assert.equal(index.claimable, false);
  assert.ok(index.diagnostics.some(item => item.code === 'YAML_PARSE_ERROR'));
  assert.ok(index.diagnostics.some(item => item.relativePath === 'FD/tickets/FD-2.md'));
});

test('dangling parent, sprint and blocks references are repo-wide diagnostics', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket(
    'FD-1',
    'parent: FD-404\nsprint: missing-sprint\nblocks:\n  - key: FD-405\n    reason: waiting\n',
  ));
  const index = await loadVault(root);
  const refs = index.diagnostics.filter(item => item.code === 'DANGLING_REFERENCE');
  assert.equal(refs.length, 3);
  assert.deepEqual(new Set(refs.map(item => item.reference)), new Set(['parent', 'sprint', 'blocks']));
  assert.equal(index.valid, false);
});

test('duplicate ticket keys are reported even though one remains inspectable', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket('FD-1'));
  await writeFile(path.join(root, 'FD', 'tickets', 'draft.md'), ticket('FD-1'));
  const index = await loadVault(root);
  const duplicates = index.diagnostics.filter(item => item.code === 'DUPLICATE_KEY');
  assert.equal(duplicates.length, 2);
  assert.equal(index.getTicket('FD-1').metadata.title, 'FD-1 title');
  assert.equal(index.valid, false);
});

test('canonical field shape and workflow status/category mismatches fail closed', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'workflow.md'), '---\nkind: workflow\nstatuses:\n  - name: To Do\n    category: TODO\n  - name: Done\n    category: DONE\n---\n');
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), '---\nkey: FD-1\ntitle: FD-1 title\nstatus: Unknown\nstatusCategory: DONE\n---\n');
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), '---\nkey: FD-2\ntitle: FD-2 title\nstatus: To Do\nstatusCategory: DONE\npriority: [HIGH]\n---\n');
  const index = await loadVault(root);
  assert.ok(index.diagnostics.some(item => item.code === 'UNKNOWN_STATUS' && item.target === undefined));
  assert.ok(index.diagnostics.some(item => item.code === 'STATUS_CATEGORY_MISMATCH'));
  assert.ok(index.diagnostics.some(item => item.code === 'INVALID_FIELD_TYPE' && item.field === 'priority'));
  assert.equal(index.valid, false);
});

test('parent hierarchy diagnostics only fire for recognised types on both sides', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-9.md'), ticket('FD-9', 'type: EPIC\n'));
  // A TASK under an EPIC is legal and must stay silent.
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket('FD-1', 'type: TASK\nparent: FD-9\n'));
  // An EPIC under an EPIC is a real error-severity INVALID_PARENT.
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), ticket('FD-2', 'type: EPIC\nparent: FD-9\n'));
  const index = await loadVault(root);
  const invalid = index.diagnostics.filter(item => item.code === 'INVALID_PARENT');
  assert.equal(invalid.length, 1);
  assert.equal(invalid[0].severity, 'error');
  assert.equal(invalid[0].relativePath, 'FD/tickets/FD-2.md');
});

test('a ticket with no type and a parent produces no INVALID_PARENT and the index stays valid', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-9.md'), ticket('FD-9', 'type: EPIC\n'));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket('FD-1', 'parent: FD-9\n'));
  const index = await loadVault(root);
  assert.equal(index.diagnostics.some(item => item.code === 'INVALID_PARENT'), false);
  assert.equal(index.valid, true);
});

test('statusCategory is derived from the workflow when omitted and recomputed after a status edit', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'workflow.md'), '---\nkind: workflow\nstatuses:\n  - name: To Do\n    category: TODO\n  - name: Done\n    category: DONE\n---\n');
  const ticketPath = path.join(root, 'FD', 'tickets', 'FD-1.md');
  await writeFile(ticketPath, '---\nkey: FD-1\ntitle: FD-1 title\nstatus: To Do\n---\n');
  const index = await loadVault(root);
  assert.equal(index.getTicket('FD-1').metadata.statusCategory, 'TODO');
  assert.equal(index.valid, true);
  await writeFile(ticketPath, '---\nkey: FD-1\ntitle: FD-1 title\nstatus: Done\n---\n');
  const next = await loadVault(root);
  assert.equal(next.getTicket('FD-1').metadata.statusCategory, 'DONE');
  assert.equal(next.valid, true);
});

test('keyless drafts remain visible but are explicitly excluded from claimable tickets', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await mkdir(path.join(root, 'FD', 'tickets', 'drafts'), { recursive: true });
  await writeFile(path.join(root, 'FD', 'tickets', 'drafts', 'spike.md'), '---\ntitle: Unfinished spike\nstatus: To Do\n---\n');
  const index = await loadVault(root);
  const draft = index.getFile('FD/tickets/drafts/spike.md');
  assert.equal(draft.isDraft, true);
  assert.equal(draft.claimable, false);
  assert.ok(index.diagnostics.some(item => item.code === 'DRAFT_KEYLESS' && item.severity === 'warning'));
  assert.equal(index.valid, true);
});

test('blocks backlinks, wikilinks, text search and knowledge/history collections are indexed', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-1.md'), ticket(
    'FD-1',
    'blocks:\n  - key: FD-2\n    reason: prerequisite\n',
    'Read [[FD-2|the second ticket]].\n',
  ));
  await writeFile(path.join(root, 'FD', 'tickets', 'FD-2.md'), ticket('FD-2', '', 'Belt compression notes\n'));
  await writeFile(path.join(root, 'FD', 'knowledge', 'decisions.md'), '# Belt decisions\n');
  await writeFile(path.join(root, 'FD', 'history', 'pre-migration.md'), '# History\n');
  const index = await loadVault(root);
  assert.deepEqual([...index.backlinks.get('FD-2')], ['FD-1']);
  assert.deepEqual([...index.wikilinks.get('FD-2')], ['FD-1']);
  assert.equal(index.search('compression').at(0).metadata.key, 'FD-2');
  assert.equal(index.knowledge.size, 1);
  assert.equal(index.history.size, 1);
  assert.equal(index.valid, true);
});

test('watcher incrementally invalidates a ticket after external corruption and closes cleanly', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  const ticketPath = path.join(root, 'FD', 'tickets', 'FD-1.md');
  await writeFile(ticketPath, ticket('FD-1'));
  const updates = [];
  const watcher = await watchVault(root, update => updates.push(update));
  t.onTestFinished(() => watcher.close());
  await watcher.ready;
  await writeFile(ticketPath, '---\nkey: FD-1\nstatus: [broken\n---\n');
  await new Promise((resolve, reject) => {
    const deadline = Date.now() + 4000;
    const poll = () => {
      if (watcher.index.diagnostics.some(item => item.code === 'YAML_PARSE_ERROR')) return resolve();
      if (Date.now() > deadline) return reject(new Error('watcher did not observe corruption'));
      setTimeout(poll, 25);
    };
    poll();
  });
  assert.equal(watcher.index.valid, false);
  assert.ok(updates.some(item => item.type === 'change'));
  await watcher.close();
  assert.equal(watcher.closed, true);
  // Reading the file confirms the watcher never rewrites the authoritative bytes.
  assert.equal((await readFile(ticketPath)).toString(), '---\nkey: FD-1\nstatus: [broken\n---\n');
});

test('watcher observes final bytes when truncate and write are separate native events', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  const ticketPath = path.join(root, 'FD', 'tickets', 'FD-1.md');
  await writeFile(ticketPath, ticket('FD-1'));
  const watcher = await watchVault(root, { awaitWriteFinish: false });
  t.onTestFinished(() => watcher.close());
  await watcher.ready;

  const file = await open(ticketPath, 'w');
  // Keep the truncate and final write far enough apart to reproduce the
  // native events that chokidar's high-level 50ms throttle coalesces.
  await new Promise(resolve => setTimeout(resolve, 10));
  await file.write('---\nkey: FD-1\nstatus: [broken\n---\n');
  await file.close();

  await new Promise((resolve, reject) => {
    const deadline = Date.now() + 4000;
    const poll = () => {
      if (watcher.index.diagnostics.some(item => item.code === 'YAML_PARSE_ERROR')) return resolve();
      if (Date.now() > deadline) return reject(new Error('watcher did not observe final corrupted bytes'));
      setTimeout(poll, 25);
    };
    poll();
  });
  assert.equal(watcher.index.valid, false);
  assert.equal((await readFile(ticketPath)).toString(), '---\nkey: FD-1\nstatus: [broken\n---\n');
});

test('watcher tracks optional root metadata supporting files', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  const updates = [];
  const watcher = await watchVault(root, update => updates.push(update));
  t.onTestFinished(() => watcher.close());
  await watcher.ready;
  await new Promise(resolve => setTimeout(resolve, 20));
  await writeFile(path.join(root, 'metadata.md'), '---\nkind: metadata\ntables:\n  jira.comments:\n    - id: c1\n---\n');
  await new Promise((resolve, reject) => {
    const deadline = Date.now() + 4000;
    const poll = () => {
      if (watcher.index.metadataFiles.has('metadata.md')) return resolve();
      if (Date.now() > deadline) return reject(new Error('watcher did not observe metadata'));
      setTimeout(poll, 25);
    };
    poll();
  });
  assert.equal(watcher.index.tables.get('jira.comments')[0].id, 'c1');
  assert.ok(updates.some(item => item.relativePath === 'metadata.md'));

  await rm(path.join(root, 'metadata.md'));
  await waitFor(
    () => !watcher.index.metadataFiles.has('metadata.md'),
    'watcher did not remove optional metadata',
  );
  assert.equal(watcher.index.valid, true);
  await watcher.close();
});

test('watcher resynchronizes edits made before its ready promise resolves', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  const ticketPath = path.join(root, 'FD', 'tickets', 'FD-1.md');
  await writeFile(ticketPath, ticket('FD-1', '', 'before ready\n'));

  const watcher = await watchVault(root);
  t.onTestFinished(() => watcher.close());
  await writeFile(ticketPath, ticket('FD-1', '', 'written before ready\n'));
  await watcher.ready;

  await waitFor(
    () => watcher.index.getTicket('FD-1')?.bodyText.includes('written before ready'),
    'ready rescan did not observe the latest ticket bytes',
  );
  assert.match(watcher.index.getTicket('FD-1').bodyText, /written before ready/);
  await watcher.close();
});

test('required supporting file deletion matches a cold scan and recovers on restore', async t => {
  const root = await fixture();
  t.onTestFinished(() => dispose(root));
  await writeFile(path.join(root, 'vault.config.json'), JSON.stringify({
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: [{ path: 'catalog.md', kind: 'metadata' }],
  }));
  const supportingPath = path.join(root, 'catalog.md');
  const supportingText = '---\nkind: metadata\ntables:\n  jira.comments:\n    - id: c1\n---\n';
  await writeFile(supportingPath, supportingText);

  const cold = await loadVault(root);
  assert.equal(cold.valid, true);
  const watcher = await watchVault(root);
  t.onTestFinished(() => watcher.close());
  await watcher.ready;

  await rm(supportingPath);
  await waitFor(
    () => watcher.index.diagnostics.some(item => (
      item.code === 'FILE_READ_ERROR' && item.relativePath === 'catalog.md'
    )),
    'watcher did not retain the required supporting-file read diagnostic',
  );
  const coldMissing = await loadVault(root);
  assert.equal(watcher.index.valid, false);
  assert.equal(coldMissing.valid, false);
  assert.ok(coldMissing.diagnostics.some(item => (
    item.code === 'FILE_READ_ERROR' && item.relativePath === 'catalog.md'
  )));

  await writeFile(supportingPath, supportingText);
  await waitFor(
    () => watcher.index.valid && watcher.index.metadataFiles.has('catalog.md'),
    'watcher did not recover after restoring the required supporting file',
  );
  assert.equal(watcher.index.tables.get('jira.comments')[0].id, 'c1');
  await watcher.close();
});
