import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { cp, mkdtemp, readdir, readFile, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { test } from 'vitest';
import { fileURLToPath } from 'node:url';

import { dispatchRead } from '../cli/read.js';
import { loadVault } from '../server/main.js';

const fixtureRoot = fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url));
const cliPath = fileURLToPath(new URL('../cli/zagent.mjs', import.meta.url));

async function copiedVault(t) {
  const root = await mkdtemp(path.join(tmpdir(), 'zenith-vault-acceptance-'));
  await cp(fixtureRoot, root, { recursive: true });
  t.onTestFinished(async () => rm(root, { recursive: true, force: true }));
  return root;
}

function ticketRecords(index) {
  return index?.tickets instanceof Map ? [...index.tickets.values()] : [];
}

function findTicket(index, predicate, description) {
  const record = ticketRecords(index).find(predicate);
  assert.ok(record, `fixture must contain ${description}`);
  return record;
}

function filePathFor(root, record) {
  return path.join(root, ...String(record.relativePath).split('/'));
}

function splitFrontmatter(text) {
  const match = /^(---\r?\n)([\s\S]*?)(\r?\n---(?:\r?\n|$))/.exec(text);
  assert.ok(match, 'ticket must have a delimited YAML frontmatter block');
  const lineEnding = match[1].slice(3);
  return {
    lineEnding,
    lines: match[2].split(lineEnding),
    prefix: match[1],
    suffix: match[3],
    rest: text.slice(match[0].length),
  };
}

async function replaceTopLevelScalar(filePath, field, value) {
  const original = await readFile(filePath, 'utf8');
  const frontmatter = splitFrontmatter(original);
  const line = frontmatter.lines.findIndex(valueLine => (
    valueLine.startsWith(`${field}:`) && !/^\s/.test(valueLine)
  ));
  assert.ok(line >= 0, `ticket must have a top-level ${field} field`);
  frontmatter.lines[line] = `${field}: ${JSON.stringify(value)}`;
  await writeFile(
    filePath,
    `${frontmatter.prefix}${frontmatter.lines.join(frontmatter.lineEnding)}${frontmatter.suffix}${frontmatter.rest}`,
  );
}

async function replaceTopLevelBlocks(filePath, blocks = []) {
  const original = await readFile(filePath, 'utf8');
  const frontmatter = splitFrontmatter(original);
  const line = frontmatter.lines.findIndex(valueLine => (
    valueLine.startsWith('blocks:') && !/^\s/.test(valueLine)
  ));
  assert.ok(line >= 0, 'ticket must have a top-level blocks field');

  let end = line + 1;
  while (end < frontmatter.lines.length && /^\s/.test(frontmatter.lines[end])) end += 1;
  const encoded = Array.isArray(blocks) && blocks.length === 0 ? 'blocks: []' : 'blocks:';
  frontmatter.lines.splice(line, end - line, encoded);
  if (Array.isArray(blocks) && blocks.length > 0) {
    for (const block of blocks) {
      frontmatter.lines.splice(line + 1, 0, `  - key: ${JSON.stringify(block.key)}`);
      if (block.reason !== undefined) {
        frontmatter.lines.splice(line + 2, 0, `    reason: ${JSON.stringify(block.reason)}`);
      }
    }
  }

  await writeFile(
    filePath,
    `${frontmatter.prefix}${frontmatter.lines.join(frontmatter.lineEnding)}${frontmatter.suffix}${frontmatter.rest}`,
  );
}

async function replaceTopLevelSequence(filePath, field, values) {
  const original = await readFile(filePath, 'utf8');
  const frontmatter = splitFrontmatter(original);
  const line = frontmatter.lines.findIndex(valueLine => (
    valueLine.startsWith(`${field}:`) && !/^\s/.test(valueLine)
  ));
  assert.ok(line >= 0, `ticket must have a top-level ${field} field`);
  let end = line + 1;
  while (end < frontmatter.lines.length && /^\s/.test(frontmatter.lines[end])) end += 1;
  const replacement = [`${field}:`];
  for (const value of values) replacement.push(`  - ${JSON.stringify(value)}`);
  if (values.length === 0) replacement[0] = `${field}: []`;
  frontmatter.lines.splice(line, end - line, ...replacement);
  await writeFile(
    filePath,
    `${frontmatter.prefix}${frontmatter.lines.join(frontmatter.lineEnding)}${frontmatter.suffix}${frontmatter.rest}`,
  );
}

async function appendBody(filePath, text) {
  const original = await readFile(filePath, 'utf8');
  const separator = original.endsWith('\n') ? '' : '\n';
  await writeFile(filePath, `${original}${separator}${text}`);
}

async function hashTree(root, current = root, result = []) {
  const entries = await readdir(current, { withFileTypes: true });
  for (const entry of entries) {
    const absolute = path.join(current, entry.name);
    if (entry.isDirectory()) {
      await hashTree(root, absolute, result);
      continue;
    }
    if (!entry.isFile()) continue;
    const digest = createHash('sha256').update(await readFile(absolute)).digest('hex');
    result.push([path.relative(root, absolute).split(path.sep).join('/'), digest]);
  }
  return result.sort(([left], [right]) => left.localeCompare(right));
}

function cliRead(root, args) {
  const child = spawnSync(
    process.execPath,
    [cliPath, '--vault', root, ...args, '--json'],
    {
      cwd: path.dirname(cliPath),
      encoding: 'utf8',
      env: { ...process.env, NODE_NO_WARNINGS: '1' },
      maxBuffer: 1024 * 1024,
      timeout: 30_000,
    },
  );
  const output = `${child.stdout ?? ''}\n${child.stderr ?? ''}`
    .split(/\r?\n/)
    .map(line => line.trim())
    .filter(Boolean);
  const last = output.at(-1);
  let payload;
  try {
    payload = last ? JSON.parse(last) : undefined;
  } catch {
    payload = undefined;
  }
  return { status: child.status, signal: child.signal, payload, output };
}

test('dispatchRead follows canonical ticket title, body, status, assignee, and derived status category edits', async t => {
  const root = await copiedVault(t);
  let index = await loadVault(root);
  const record = findTicket(
    index,
    value => value.project === 'FD'
      && value.metadata.type !== 'EPIC'
      && value.metadata.status === 'To Do'
      && value.metadata.parent
      && index.tickets.has(value.metadata.parent)
      && Array.isArray(value.metadata.blocks)
      && value.metadata.blocks.length === 0,
    'an ordinary FD child ticket with no blockers',
  );
  const key = record.metadata.key;
  const parentKey = record.metadata.parent;
  const ticketPath = filePathFor(root, record);
  const originalTitle = record.metadata.title;
  const originalSourceTitle = record.metadata.source?.title;

  const initialShow = dispatchRead(index, ['show', key], {});
  assert.equal(initialShow.exitCode, 0);
  assert.equal(initialShow.payload.title, originalTitle);
  assert.equal(originalSourceTitle, originalTitle);

  const editedTitle = `Acceptance canonical title (${key})`;
  await replaceTopLevelScalar(ticketPath, 'title', editedTitle);
  index = await loadVault(root);
  const titleShow = dispatchRead(index, ['show', key], {});
  assert.equal(titleShow.exitCode, 0);
  assert.equal(titleShow.payload.title, editedTitle);
  assert.equal(index.getTicket(key).metadata.source.title, originalSourceTitle);

  const bodySentinel = `Acceptance body sentinel for ${key}`;
  await appendBody(ticketPath, `\n## Acceptance\n${bodySentinel}\n`);
  index = await loadVault(root);
  const bodyShow = dispatchRead(index, ['show', key], {});
  assert.equal(bodyShow.exitCode, 0);
  assert.match(bodyShow.payload.body, new RegExp(bodySentinel.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')));

  const queueBefore = dispatchRead(index, ['queue', '--project', 'FD'], {});
  const beforeEntry = queueBefore.payload.find(value => value.key === key);
  assert.ok(beforeEntry, `queue must expose ${key} before the assignee edit`);
  assert.equal(beforeEntry.assigned, false);

  const editedAssignee = `acceptance-${key.toLowerCase()}@example.test`;
  await replaceTopLevelScalar(ticketPath, 'assignee', editedAssignee);
  index = await loadVault(root);
  const queueAfter = dispatchRead(index, ['queue', '--project', 'FD'], {});
  const afterEntry = queueAfter.payload.find(value => value.key === key);
  assert.ok(afterEntry, `queue must expose ${key} after the assignee edit`);
  assert.equal(afterEntry.assigned, true);
  assert.equal(afterEntry.claimable, false);

  await replaceTopLevelScalar(ticketPath, 'status', 'In Progress');
  index = await loadVault(root);
  assert.equal(index.getTicket(key).metadata.status, 'In Progress');
  assert.equal(index.getTicket(key).metadata.statusCategory, 'IN_PROGRESS');
  const epic = dispatchRead(index, ['epic', parentKey], {});
  const child = epic.payload.children.find(value => value.key === key);
  assert.ok(child, `epic ${parentKey} must expose ${key}`);
  assert.equal(child.status, 'In Progress');
  assert.equal(child.statusCategory, 'IN_PROGRESS');

  await replaceTopLevelScalar(ticketPath, 'status', 'Acceptance status does not exist');
  index = await loadVault(root);
  assert.equal(index.valid, false);
  assert.equal(index.getTicket(key).metadata.statusCategory, null);
  const invalidEpic = dispatchRead(index, ['epic', parentKey], {});
  const invalidChild = invalidEpic.payload.children.find(value => value.key === key);
  assert.ok(invalidChild);
  assert.equal(invalidChild.statusCategory, null);
});

test('dispatchRead reflects removal of a canonical blocker edge', async t => {
  const root = await copiedVault(t);
  let index = await loadVault(root);
  const source = findTicket(
    index,
    value => value.project === 'FD'
      && value.metadata.type !== 'EPIC'
      && Array.isArray(value.metadata.blocks)
      && value.metadata.blocks.some(block => block.key === 'FD-101'),
    'an FD ticket that blocks FD-101',
  );
  const sourceKey = source.metadata.key;
  const sourcePath = filePathFor(root, source);

  const before = dispatchRead(index, ['blocked', '--project', 'FD'], {});
  const beforeTarget = before.payload.find(value => value.key === 'FD-101');
  assert.ok(beforeTarget, 'FD-101 must be reported as blocked before edge removal');
  assert.ok(beforeTarget.blockedBy.includes(sourceKey));

  await replaceTopLevelBlocks(sourcePath, []);
  index = await loadVault(root);
  const after = dispatchRead(index, ['blocked', '--project', 'FD'], {});
  const afterTarget = after.payload.find(value => value.key === 'FD-101');
  assert.ok(!afterTarget || !afterTarget.blockedBy.includes(sourceKey));
});

test('cold corruption makes queue and targeted check fail closed across the repository', async t => {
  const root = await copiedVault(t);
  const cleanIndex = await loadVault(root);
  const healthy = findTicket(
    cleanIndex,
    value => value.project === 'FD'
      && value.metadata.type !== 'EPIC'
      && value.metadata.status === 'To Do'
      && dispatchRead(cleanIndex, ['check', value.metadata.key], {}).exitCode === 0,
    'a healthy FD ticket suitable for a repository-wide check',
  );
  const corrupt = findTicket(
    cleanIndex,
    value => value.metadata.key !== healthy.metadata.key,
    'a separate ticket to corrupt',
  );
  await writeFile(filePathFor(root, corrupt), '---\nstatus: [broken\n---\n');

  const index = await loadVault(root);
  assert.equal(index.valid, false);
  const queue = dispatchRead(index, ['queue', '--project', 'FD'], {});
  const healthyEntry = queue.payload.find(value => value.key === healthy.metadata.key);
  assert.ok(healthyEntry, `queue must retain ${healthy.metadata.key} for diagnostic visibility`);
  assert.equal(healthyEntry.claimable, false);

  const check = dispatchRead(index, ['check', healthy.metadata.key], {});
  assert.equal(check.exitCode, 4);
  assert.match(
    String(check.payload.contractError ?? check.payload.check?.queue?.reason ?? ''),
    /Vault is invalid/i,
  );
});

test('cross-file fixVersion, assignee, and canonical link references fail closed', async t => {
  const root = await copiedVault(t);
  const initial = await loadVault(root);
  const record = findTicket(
    initial,
    value => value.project === 'FD'
      && value.metadata.type !== 'EPIC'
      && Array.isArray(value.metadata.fixVersions)
      && value.metadata.fixVersions.length > 0
      && Array.isArray(value.metadata.links),
    'an FD ticket with version and link fields',
  );
  const ticketPath = filePathFor(root, record);
  await replaceTopLevelSequence(ticketPath, 'fixVersions', ['Acceptance version does not exist']);
  await replaceTopLevelScalar(ticketPath, 'assignee', 'missing-acceptance-user@example.test');
  await replaceTopLevelSequence(ticketPath, 'links', ['FD-404']);

  const index = await loadVault(root);
  const references = index.diagnostics.filter(item => item.code === 'DANGLING_REFERENCE');
  assert.ok(references.some(item => item.reference === 'fixVersions' && item.target === 'Acceptance version does not exist'));
  assert.ok(references.some(item => item.reference === 'assignee' && item.target === 'missing-acceptance-user@example.test'));
  assert.ok(references.some(item => item.reference === 'links' && item.target === 'FD-404'));
  assert.equal(index.valid, false);
});

test('CLI refuses nested sprint and rows write commands without changing vault bytes', async t => {
  const root = await copiedVault(t);
  const before = await hashTree(root);
  const commands = [
    ['sprint', 'create', '--project', 'FD', '--name', 'acceptance-sprint'],
    ['rows', 'resolve', '--table', 'issues'],
  ];

  for (const args of commands) {
    const response = cliRead(root, args);
    assert.notEqual(response.status, 0, `${args.join(' ')} must be rejected as read-only`);
    assert.match(
      String(response.payload?.error ?? response.output.at(-1) ?? ''),
      /read.only|unavailable|write/i,
      `${args.join(' ')} must explain the read-only refusal`,
    );
  }

  assert.deepEqual(await hashTree(root), before, 'read-only command attempts must preserve every vault byte');
});
