import { expect, test } from 'vitest';
import { browserCommand, browserOperationId, browserRevisions } from '../server/browser-protocol.mjs';
const revision = { hash: 'a'.repeat(64), size: 12 };
const record = { relativePath: 'ZEN/tickets/ZEN-1.md' };
const index = { getTicket: key => key === 'ZEN-1' ? record : null, configs: new Map(), sprints: new Map() };
const input = extra => ({ args: ['comment', 'ZEN-1', '--file', 'comment.md'], files: { file: 'Comment body' }, revisions: { [record.relativePath]: revision }, operationId: 'one-comment', ...extra });

test('browser comments use supplied text and the exact opened revision', () => {
  expect(browserCommand(input(), index)).toEqual(input());
});
test.each(['--actor', '--actor-email=other@example.invalid', '--human', '--operation-id', '--attempt', '--vault', '--remote', '--commit'])('rejects server-controlled flag %s', flag => {
  expect(() => browserCommand(input({ args: ['update', 'ZEN-1', flag, 'value'] }), index)).toThrow(/controlled by the server/);
});
test.each(['claim', 'next', 'finish', 'rows', 'suggestions', 'unknown'])('does not expose agent or arbitrary table command %s', command => {
  expect(() => browserCommand(input({ args: [command, 'ZEN-1'] }), index)).toThrow(/not available/);
});
test('refuses a file argument without inline text instead of reading the host filesystem', () => {
  expect(() => browserCommand(input({ files: {} }), index)).toThrow(/inline/);
});
test('refuses missing issue revisions and malformed operation IDs', () => {
  expect(() => browserCommand(input({ revisions: {} }), index)).toThrow(/opened revision/);
  expect(() => browserOperationId('../outside')).toThrow(/operationId/);
});
test.each(['../outside.md', 'C:/outside.md', '/outside.md', '.git/config', '.zagent/state', 'ZEN\\ticket.md'])('rejects revision path %s', path => {
  expect(() => browserRevisions({ [path]: revision })).toThrow(/vault-relative/);
});
test('refuses invalid hashes, sizes, flags and missing projects', () => {
  expect(() => browserRevisions({ 'file.md': { hash: 'bad', size: 1 } })).toThrow(/SHA-256/);
  expect(() => browserRevisions({ 'file.md': { ...revision, size: -1 } })).toThrow(/revision size/);
  expect(() => browserCommand(input({ args: ['update', 'ZEN-1', '--unknown', 'value'] }), index)).toThrow(/Unknown browser flag/);
  expect(() => browserCommand(input({ args: ['create', '--title', 'New'] }), index)).toThrow(/explicit project/);
});
test('release changes require the revision of their existing catalog', () => {
  const catalog = { project: 'ZEN', relativePath: 'ZEN/versions.md' };
  const catalogIndex = { ...index, configs: new Map([['versions', catalog]]) };
  expect(() => browserCommand(input({ args: ['version', 'create', 'v2', '--project', 'ZEN'] }), catalogIndex)).toThrow(/ZEN\/versions.md/);
  expect(browserCommand(input({ args: ['version', 'create', 'v2', '--project', 'ZEN', '--description', 'Release notes'], revisions: { [catalog.relativePath]: revision } }), catalogIndex).args).toContain('Release notes');
});
