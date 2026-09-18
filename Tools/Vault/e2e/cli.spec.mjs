import { spawn } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  cp,
  mkdir,
  mkdtemp,
  readFile,
  readdir,
  writeFile,
} from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { expect, test } from '@playwright/test';

import { removeTempVault } from './helpers/vault-app.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const vaultFixture = path.resolve(here, '..', 'fixtures', 'snapshot', 'vault');
const cliPath = path.resolve(here, '..', 'cli', 'zagent.mjs');
const cliCwd = path.dirname(cliPath);

function commandLine(root, args, vaultStyle = 'separate') {
  const vault = vaultStyle === 'equals' ? `--vault=${root}` : '--vault';
  return [cliPath, vault, ...(vaultStyle === 'equals' ? [] : [root]), ...args, '--json'];
}

function parsePayload(stdout) {
  const lines = String(stdout ?? '')
    .split(/\r?\n/)
    .map(line => line.trim())
    .filter(Boolean);
  const last = lines.at(-1);
  if (last === undefined) return undefined;
  try {
    return JSON.parse(last);
  } catch {
    return undefined;
  }
}

function runProcess(argv, { timeout = 30_000 } = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(process.execPath, argv, {
      cwd: cliCwd,
      env: { ...process.env, NODE_NO_WARNINGS: '1' },
      windowsHide: true,
    });
    let stdout = '';
    let stderr = '';
    let timedOut = false;
    const timer = setTimeout(() => {
      timedOut = true;
      child.kill();
    }, timeout);
    child.stdout.on('data', chunk => { stdout += chunk; });
    child.stderr.on('data', chunk => { stderr += chunk; });
    child.once('error', error => {
      clearTimeout(timer);
      reject(error);
    });
    child.once('close', (status, signal) => {
      clearTimeout(timer);
      resolve({
        argv,
        status: timedOut ? null : status,
        signal,
        timedOut,
        stdout,
        stderr,
        payload: parsePayload(stdout),
      });
    });
  });
}

function runCli(root, args, options = {}) {
  return runProcess(commandLine(root, args, options.vaultStyle), options);
}

async function copiedVault() {
  const root = await mkdtemp(path.join(os.tmpdir(), 'zenith-vault-playwright-'));
  await cp(vaultFixture, root, { recursive: true });
  return root;
}

async function withVault(callback) {
  const root = await copiedVault();
  try {
    return await callback(root);
  } finally {
    await removeTempVault(root);
  }
}

async function hashTree(root, current = root, result = []) {
  const entries = await readdir(current, { withFileTypes: true });
  for (const entry of entries) {
    const absolute = path.join(current, entry.name);
    if (entry.isDirectory()) {
      await hashTree(root, absolute, result);
    } else if (entry.isFile()) {
      result.push([
        path.relative(root, absolute).split(path.sep).join('/'),
        createHash('sha256').update(await readFile(absolute)).digest('hex'),
      ]);
    }
  }
  return result.sort(([left], [right]) => left.localeCompare(right));
}

function ticketPath(root, key) {
  const project = key.slice(0, key.lastIndexOf('-'));
  return path.join(root, project, 'tickets', `${key}.md`);
}

function escapedRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

async function replaceFrontmatterScalar(filePath, field, value) {
  const original = await readFile(filePath, 'utf8');
  const line = new RegExp(`^${escapedRegExp(field)}:[^\\r\\n]*$`, 'm');
  expect(original).toMatch(line);
  const encoded = value === null ? 'null' : JSON.stringify(value);
  await writeFile(filePath, original.replace(line, `${field}: ${encoded}`));
}

async function replaceBlocks(filePath, blocks) {
  const original = await readFile(filePath, 'utf8');
  const lines = original.split(/\r?\n/);
  const start = lines.findIndex(line => /^blocks:\s*$/.test(line));
  expect(start).toBeGreaterThanOrEqual(0);
  let end = start + 1;
  while (end < lines.length && /^\s/.test(lines[end])) end += 1;
  const replacement = blocks.length === 0
    ? ['blocks: []']
    : ['blocks:', ...blocks.flatMap(block => [
      `  - key: ${JSON.stringify(block.key)}`,
      `    reason: ${JSON.stringify(block.reason)}`,
    ])];
  lines.splice(start, end - start, ...replacement);
  await writeFile(filePath, lines.join('\n'));
}

async function addLocalDocuments(root) {
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.supportingFiles = [...new Set([...(config.supportingFiles ?? []), 'Docs/Roadmap.md'])];
  await writeFile(configPath, `${JSON.stringify(config, null, 2)}\n`);
  await mkdir(path.join(root, 'Docs'), { recursive: true });
  const roadmap = '# Acceptance Roadmap\n\n[FD-18] Belt transport line work\n';
  await writeFile(path.join(root, 'Docs', 'Roadmap.md'), roadmap);
  return roadmap;
}

async function addHistoryFixture(root) {
  const historyPath = path.join(root, 'FD', 'history', 'pre-migration.md');
  const text = `---
kind: history
rows:
  - id: e2e-history-1
    issue_id: qlpipe65z25i8de8bvzv73tj
    field: status
    from_value: To Do
    to_value: In Progress
    actor: e2e-runner
    created_at: 2026-01-02T03:04:05.123456
---
# E2E history
`;
  await writeFile(historyPath, text);
}

test.describe('Vault CLI process workflows', () => {
  test('uses explicit vault roots and returns JSON process contracts', async () => {
    await withVault(async root => {
      const separate = await runCli(root, ['doctor']);
      expect(separate.status).toBe(0);
      expect(separate.signal).toBeNull();
      expect(separate.timedOut).toBe(false);
      expect(separate.payload).toMatchObject({ ok: true, ticketCount: 244 });

      const equals = await runCli(root, ['doctor'], { vaultStyle: 'equals' });
      expect(equals.status).toBe(0);
      expect(equals.payload).toMatchObject({ ok: true, ticketCount: 244 });

      const help = await runCli(root, ['claim', '--help']);
      expect(help.status).toBe(0);
      expect(help.payload).toMatchObject({ readOnly: true });
      expect(help.payload.commands).toContain('history');
    });

    const missingRoot = await runProcess([cliPath, 'doctor', '--vault']);
    expect(missingRoot.status).toBe(1);
    expect(missingRoot.payload).toEqual({ error: 'Missing required --vault ROOT' });

    const missingArgument = await runProcess([cliPath, 'doctor', '--json']);
    expect(missingArgument.status).toBe(1);
    expect(missingArgument.payload).toEqual({ error: 'Missing required --vault ROOT' });
  });

  test('rejects unknown commands and flags with a nonzero JSON result', async () => {
    await withVault(async root => {
      const unknownCommand = await runCli(root, ['not-a-command']);
      expect(unknownCommand.status).toBe(1);
      expect(unknownCommand.payload.error).toMatch(/unknown read command/i);

      const unknownFlag = await runCli(root, ['queue', '--projectt', 'FD']);
      expect(unknownFlag.status).toBe(1);
      expect(unknownFlag.payload.error).toMatch(/unknown flag.*projectt/i);
    });
  });

  test('reflects canonical edits and blocker removal in fresh CLI processes', async () => {
    await withVault(async root => {
      const key = 'FD-18';
      const file = ticketPath(root, key);
      const initial = await runCli(root, ['show', key]);
      expect(initial.status).toBe(0);
      const originalTitle = initial.payload.title;
      expect(initial.payload.status).toBe('To Do');

      const editedTitle = `E2E canonical title for ${key}`;
      await replaceFrontmatterScalar(file, 'title', editedTitle);
      const title = await runCli(root, ['show', key]);
      expect(title.status).toBe(0);
      expect(title.payload.title).toBe(editedTitle);
      expect(title.payload.title).not.toBe(originalTitle);

      const bodySentinel = `E2E body sentinel ${Date.now()}`;
      const originalBody = await readFile(file, 'utf8');
      await writeFile(file, `${originalBody}\n${bodySentinel}\n`);
      const body = await runCli(root, ['show', key]);
      expect(body.status).toBe(0);
      expect(body.payload.body).toContain(bodySentinel);

      await replaceFrontmatterScalar(file, 'assignee', 'agent@saas.invalid');
      const queue = await runCli(root, ['queue', '--project', 'FD']);
      expect(queue.status).toBe(0);
      const queueEntry = queue.payload.find(entry => entry.key === key);
      expect(queueEntry).toMatchObject({ key, assigned: true, claimable: false });
      const doctor = await runCli(root, ['doctor']);
      expect(doctor.status).toBe(0);
      expect(doctor.payload).toMatchObject({ ok: true });

      await replaceFrontmatterScalar(file, 'status', 'In Progress');
      const progress = await runCli(root, ['show', key]);
      expect(progress.status).toBe(0);
      expect(progress.payload).toMatchObject({ key, status: 'In Progress' });
      const epic = await runCli(root, ['epic', 'FD-1']);
      expect(epic.status).toBe(0);
      expect(epic.payload.children).toEqual(expect.arrayContaining([
        expect.objectContaining({ key, status: 'In Progress', statusCategory: 'IN_PROGRESS' }),
      ]));

      const blockedBefore = await runCli(root, ['blocked', '--project', 'FD']);
      expect(blockedBefore.status).toBe(0);
      const targetBefore = blockedBefore.payload.find(entry => entry.key === 'FD-19');
      expect(targetBefore?.blockedBy).toContain(key);

      await replaceBlocks(file, []);
      const blockedAfter = await runCli(root, ['blocked', '--project', 'FD']);
      expect(blockedAfter.status).toBe(0);
      const targetAfter = blockedAfter.payload.find(entry => entry.key === 'FD-19');
      expect(targetAfter?.blockedBy ?? []).not.toContain(key);
    });
  });

  test('fails closed on corruption and preserves bytes for refused writes', async () => {
    await withVault(async root => {
      const corruptPath = ticketPath(root, 'FD-18');
      await writeFile(corruptPath, '---\nstatus: [broken\n---\n');

      const doctor = await runCli(root, ['doctor']);
      expect(doctor.status).toBe(1);
      expect(doctor.payload.ok).toBe(false);
      expect(doctor.payload.diagnostics).toEqual(expect.arrayContaining([
        expect.objectContaining({ code: 'YAML_PARSE_ERROR', relativePath: 'FD/tickets/FD-18.md' }),
      ]));

      const queue = await runCli(root, ['queue', '--project', 'FD']);
      expect(queue.status).toBe(0);
      expect(queue.payload.find(entry => entry.key === 'FD-11')).toMatchObject({ claimable: false });

      const check = await runCli(root, ['check', 'FD-11']);
      expect(check.status).toBe(4);
      expect(check.payload.contractError).toMatch(/vault is invalid/i);
    });

    await withVault(async root => {
      const before = await hashTree(root);
      const writes = [
        ['auth'],
        ['provision'],
        ['next'],
        ['claim', 'FD-18'],
        ['comment', 'FD-18', 'e2e comment'],
        ['move', 'FD-18', 'Done'],
        ['finish', 'FD-18'],
        ['create', 'FD', '--title', 'must not write'],
        ['link', 'FD-18', 'FD-19'],
        ['unlink', 'FD-18', 'FD-19'],
        ['parent', 'FD-18', 'FD-1'],
        ['update', 'FD-18', '--title', 'must not write'],
        ['estimate', 'FD-18', '5'],
        ['flag', 'FD-18'],
        ['resolve', 'FD-18'],
        ['ask', 'FD-18', 'question'],
        ['decide', 'FD-18', 'decision'],
        ['shortfall', 'FD-18', 'shortfall'],
        ['changelog', 'FD-18', 'entry'],
        ['suggest', 'FD-18', 'suggestion'],
        ['notion', 'sync'],
        ['contract', 'FD-18'],
        ['sprint', 'create', '--project', 'FD', '--name', 'e2e-sprint'],
        ['version', 'create', '--project', 'FD', '--name', 'e2e-version'],
        ['docs', 'sync'],
        ['docs', 'write', 'Docs/Roadmap.md'],
        ['suggestions', 'resolve', 'e2e-suggestion'],
        ['rows', 'resolve', '--table', 'issues'],
        ['rows', 'decisions', 'resolve', 'e2e-row', 'Done'],
      ];
      for (const args of writes) {
        const response = await runCli(root, args);
        expect(response.status, args.join(' ')).toBe(1);
        expect(response.payload.error, args.join(' ')).toMatch(/read.only|unavailable|write/i);
      }
      expect(await hashTree(root)).toEqual(before);
    });
  });

  test('covers local documents, history, reports, and auxiliary read commands', async () => {
    await withVault(async root => {
      const roadmap = await addLocalDocuments(root);
      await addHistoryFixture(root);

      const docsList = await runCli(root, ['docs', 'ls']);
      expect(docsList.status).toBe(0);
      expect(docsList.payload).toEqual(expect.arrayContaining([
        expect.objectContaining({ path: 'Docs/Roadmap.md' }),
      ]));

      const docsRead = await runCli(root, ['docs', 'read', 'Docs/Roadmap.md']);
      expect(docsRead.status).toBe(0);
      expect(docsRead.payload).toEqual({ path: 'Docs/Roadmap.md', markdown: roadmap });

      const docsSearch = await runCli(root, ['docs', 'search', 'acceptance']);
      expect(docsSearch.status).toBe(0);
      expect(docsSearch.payload).toEqual(expect.arrayContaining([
        expect.objectContaining({ path: 'Docs/Roadmap.md' }),
      ]));

      const history = await runCli(root, ['history', 'FD-18']);
      expect(history.status).toBe(0);
      expect(history.payload).toEqual([{
        field: 'status',
        fromValue: 'To Do',
        toValue: 'In Progress',
        createdAt: '2026-01-02T03:04:05.123Z',
        actor: 'e2e-runner',
      }]);

      const reports = await runCli(root, ['report', 'epic', '--project', 'FD']);
      expect(reports.status).toBe(0);
      expect(reports.payload).toEqual(expect.arrayContaining([
        expect.objectContaining({ key: 'FD-1', total: expect.any(Number) }),
      ]));

      const velocity = await runCli(root, ['report', 'velocity', '--project', 'FD']);
      expect(velocity.status).toBe(0);
      expect(velocity.payload).toEqual(expect.objectContaining({ rows: expect.any(Array), average: expect.any(Number) }));

      const commandCases = [
        { args: ['links', 'FD-18'], check: payload => expect(payload).toEqual(expect.any(Array)) },
        { args: ['blocked', '--project', 'FD'], check: payload => expect(payload).toEqual(expect.any(Array)) },
        { args: ['transitions', 'FD-18'], check: payload => expect(payload).toMatchObject({ key: 'FD-18', transitions: expect.any(Array) }) },
        { args: ['sprint', 'list', '--project', 'FD'], check: payload => expect(payload).toEqual(expect.any(Array)) },
        { args: ['version', 'list', '--project', 'FD'], check: payload => expect(payload).toEqual(expect.any(Array)) },
        { args: ['rows', 'decisions', '--limit', '1'], check: payload => expect(payload).toEqual(expect.any(Array)) },
        { args: ['suggestions', '--limit', '1'], check: payload => expect(payload).toEqual(expect.any(Array)) },
      ];
      for (const { args, check } of commandCases) {
        const response = await runCli(root, args);
        expect(response.status, args.join(' ')).toBe(0);
        check(response.payload);
      }
    });
  });
});
