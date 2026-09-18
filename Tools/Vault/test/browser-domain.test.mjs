import { readFile } from 'node:fs/promises';
import vm from 'node:vm';
import { transform } from 'esbuild';
import { expect, test } from 'vitest';

test('Jira domain constants run in a browser without Node process globals', async () => {
  const source = await readFile(new URL('../shared/jira/constants.js', import.meta.url), 'utf8');
  const { code } = await transform(source, { format: 'cjs', target: 'es2022' });
  const sandbox = { module: { exports: {} } };
  vm.runInNewContext(code, sandbox);
  expect(sandbox.module.exports.JIRA_ISSUE_TYPES).toContain('SUBTASK');
  expect(sandbox.module.exports.issueParentError('SUBTASK', 'TASK')).toBeNull();
  expect(sandbox.module.exports.PRODUCTS.jira.url).toBe('http://localhost:3001');
});
