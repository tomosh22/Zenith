import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';
import { patchFields, replaceDocument, replaceBody } from '../server/text-edit.mjs';

test('field patches preserve BOM, CRLF, inline comments, unknown fields and raw body', () => {
  const input = Buffer.from('\uFEFF---\r\nstatus: To Do # keep\r\nunknown: yes\r\n---\r\n<!--keep-->\r\n```cpp extra words\r\nraw\r\n```\r\n');
  assert.deepEqual(patchFields(input, { status: 'To Do' }), input);
  assert.equal(patchFields(input, { status: 'Done' }).toString(), input.toString().replace('To Do', '"Done"'));
  assert.deepEqual(replaceDocument(input, input), input);
  assert.equal(replaceBody(input, 'New body\r\n').toString(), input.toString().split('<!--keep-->')[0] + 'New body\r\n');
});

test('collection updates, new fields and multiline values preserve surrounding text', () => {
  const input = Buffer.from('---\ntitle: |\n  old title\nblocks:\n  - key: FD-2\n    reason: prerequisite\nunknown: keep\n---\nBody\n');
  const output = patchFields(input, { title: 'New title', blocks: [], assignee: null }).toString();
  assert.match(output, /unknown: keep\nassignee: null\n---\nBody\n$/);
  assert.deepEqual(patchFields(Buffer.from(output), { title: 'New title', blocks: [], assignee: null }), Buffer.from(output));
  assert.throws(() => patchFields(Buffer.from('---\na: 1\na: 2\n---\n'), { a: 3 }), /unique/i);
  assert.throws(() => replaceDocument(Buffer.from([255]), Buffer.from('ok')), /UTF-8/);
});

test('all 244 exported tickets preserve bytes on no-op and change exactly one status line', async () => {
  const root = fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url));
  let count = 0;
  for (const project of await readdir(root, { withFileTypes: true })) {
    if (!project.isDirectory()) continue;
    const directory = path.join(root, project.name, 'tickets');
    let files;
    try { files = await readdir(directory); } catch (error) { if (error.code === 'ENOENT') continue; throw error; }
    for (const file of files.filter(name => name.endsWith('.md'))) {
      const input = await readFile(path.join(directory, file));
      assert.deepEqual(replaceDocument(input, input), input);
      const before = input.toString().split('\n');
      const after = patchFields(input, { status: '__fidelity_test__' }).toString().split('\n');
      assert.equal(after.length, before.length);
      const differences = before.flatMap((line, i) => line === after[i] ? [] : [i]);
      assert.equal(differences.length, 1);
      assert.match(before[differences[0]], /^status:/);
      count += 1;
    }
  }
  assert.equal(count, 244);
});
