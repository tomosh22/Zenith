import assert from 'node:assert/strict';
import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';
import { parseDocument } from 'yaml';

import { patchFields, replaceBody, replaceDocument } from '../server/text-edit.mjs';

const testDirectory = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(testDirectory, '../../..');

async function collectMarkdown(directory, result = []) {
  let entries;
  try {
    entries = await readdir(directory, { withFileTypes: true });
  } catch (error) {
    if (error.code === 'ENOENT') return result;
    throw error;
  }
  for (const entry of entries) {
    const absolute = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      await collectMarkdown(absolute, result);
    } else if (entry.isFile() && /\.(?:md|markdown)$/i.test(entry.name)) {
      result.push(absolute);
    }
  }
  return result;
}

async function livingMarkdownFiles() {
  const files = [];
  const rootDocuments = [
    'README.md',
    'CLAUDE.md',
  ];
  for (const relative of rootDocuments) {
    const absolute = path.join(repoRoot, relative);
    try {
      const entries = await readdir(path.dirname(absolute), { withFileTypes: true });
      if (entries.some(entry => entry.isFile() && entry.name === path.basename(absolute))) files.push(absolute);
    } catch (error) {
      if (error.code !== 'ENOENT') throw error;
    }
  }

  for (const relative of ['Docs', 'Tools/Vault/spec']) {
    await collectMarkdown(path.join(repoRoot, relative), files);
  }

  const gamesRoot = path.join(repoRoot, 'Games');
  let games;
  try {
    games = await readdir(gamesRoot, { withFileTypes: true });
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
    games = [];
  }
  for (const game of games.filter(entry => entry.isDirectory())) {
    const gameRoot = path.join(gamesRoot, game.name);
    for (const entry of await readdir(gameRoot, { withFileTypes: true })) {
      if (entry.isFile() && /\.(?:md|markdown)$/i.test(entry.name)) files.push(path.join(gameRoot, entry.name));
    }
    await collectMarkdown(path.join(gameRoot, 'Docs'), files);
  }
  return [...new Set(files)].sort();
}

test('living repository documents are byte-stable for no-op saves', async () => {
  const files = await livingMarkdownFiles();
  assert.ok(files.length >= 3, 'the living repository corpus should contain several Markdown documents');

  for (const file of files) {
    const input = await readFile(file);
    const output = replaceDocument(input, input);
    assert.ok(output.equals(input), path.relative(repoRoot, file));
  }
});

test('BOM, CRLF, comments, Unicode block scalars, anchors and empty values retain fidelity', () => {
  const input = Buffer.from(
    '\uFEFF---\r\n'
      + '# retain this comment\r\n'
      + 'status: To Do # retain status comment\r\n'
      + 'title: | # retain title comment\r\n'
      + '  日本語の題名\r\n'
      + '  café and naïve\r\n'
      + 'empty:\r\n'
      + 'commented: # retain empty comment\r\n'
      + 'defaults: &defaults\r\n'
      + '  label: 原文\r\n'
      + 'copy: *defaults\r\n'
      + '---\r\n'
      + '<!-- keep body -->\r\n'
      + 'Body with Ελληνικά and 日本語\r\n',
  );

  const patched = patchFields(input, {
    status: 'Done',
    title: '新しい題名',
    empty: 'now populated',
    commented: 'comment value',
    defaults: { label: '変更後' },
  });
  const output = patched.toString();

  assert.ok(output.startsWith('\uFEFF---\r\n'));
  assert.match(output, /^# retain this comment\r?\n/m);
  assert.match(output, /^status: "Done" # retain status comment\r?$/m);
  assert.match(output, /^title: "新しい題名" # retain title comment\r?$/m);
  assert.match(output, /^empty: "now populated"\r?$/m);
  assert.match(output, /^commented: "comment value" # retain empty comment\r?$/m);
  assert.match(output, /^defaults: &defaults\r?\n  \{"label":"変更後"\}\r?$/m);
  assert.match(output, /^copy: \*defaults\r?$/m);
  assert.match(output, /<!-- keep body -->\r?\nBody with Ελληνικά and 日本語\r?\n$/);
  assert.ok(output.includes('\r\n'));
  assert.equal(output.includes('\n\n'), false);

  const parsed = parseDocument(output.slice(output.indexOf('---\r\n') + 5, output.lastIndexOf('\r\n---\r\n')), {
    uniqueKeys: true,
  });
  assert.equal(parsed.errors.length, 0);
  assert.deepEqual(parsed.toJS(), {
    status: 'Done',
    title: '新しい題名',
    empty: 'now populated',
    commented: 'comment value',
    defaults: { label: '変更後' },
    copy: { label: '変更後' },
  });

  const newBody = 'Replaced body: 世界\r\n';
  const bodyPatched = replaceBody(patched, newBody);
  assert.equal(bodyPatched.toString(), `${output.slice(0, output.indexOf('<!-- keep body -->'))}${newBody}`);
  assert.ok(replaceDocument(input, input).equals(input));
});
