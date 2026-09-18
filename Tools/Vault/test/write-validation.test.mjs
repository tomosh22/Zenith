import { afterAll, beforeAll, describe, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';
import { dispatchWrite } from '../cli/write.mjs';

let root, index, before;
const signature = value => [...value.files.values()].map(item => [item.relativePath, item.hash]).sort(([a], [b]) => a.localeCompare(b));
beforeAll(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-invalid-write-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Invalid write test']);
  await git(root, ['config', 'user.email', 'test@example.invalid']);
  await git(root, ['add', '.']); await git(root, ['commit', '-qm', 'Fixture']);
  index = await loadVault(root); before = signature(index);
});
afterAll(async () => { if (root) await rm(root, { recursive: true, force: true }); });

describe('invalid inputs cannot modify the shared ticket writer', () => {
  test.each([
    ['type', 'UNKNOWN'], ['priority', 'URGENT'], ['complexity', 'EXTREME'],
    ['risk', 'UNKNOWN'], ['severity', 'VERY_BAD'], ['repro', 'MAYBE'],
    ['points', '-1'], ['points', '1.5'], ['points', 'NaN'], ['points', 'Infinity'],
    ['due', 'not-a-date'],
  ])('rejects invalid --%s %s', async (flag, value) => {
    const result = await dispatchWrite(index, ['update', 'ZEN-7', `--${flag}`, value], {}, { actor: 'human' });
    expect(result.exitCode).not.toBe(0);
    expect(result.payload).toBeTruthy();
  });
  test.each([
    ['create', '--project', 'ZEN'],
    ['create', '--project', 'ZEN', '--title', 'Invalid', '--type', 'UNKNOWN'],
    ['create', '--project', 'ZEN', '--title', 'Invalid', '--blocks', 'ZEN-7'],
    ['create', '--project', 'ZEN', '--title', 'Invalid', '--parent', 'ZEN-999999'],
    ['update', 'ZEN-999999', '--title', 'Missing'],
    ['move', 'ZEN-7', 'Unknown status'],
    ['parent', 'ZEN-7', 'ZEN-7'],
    ['parent', 'ZEN-7', 'ZEN-999999'],
    ['link', 'ZEN-7', 'relates-to', 'ZEN-7'],
    ['link', 'ZEN-7', 'unknown-link', 'ZEN-8'],
    ['link', 'ZEN-7', 'relates-to', 'ZEN-999999'],
    ['sprint', 'start', 'Missing sprint', '--project', 'ZEN'],
    ['version', 'release', 'Missing version', '--project', 'ZEN'],
  ])('rejects malformed or dangling command %j', async (...args) => {
    const result = await dispatchWrite(index, args, {}, { actor: 'human' });
    expect(result.exitCode, JSON.stringify({ args, result })).not.toBe(0);
  });
  test('every rejected command leaves all indexed bytes unchanged', async () => {
    expect(signature(await loadVault(root))).toEqual(before);
    expect(await git(root, ['status', '--porcelain'])).toBe('');
  });
});
