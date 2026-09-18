// Development-only import. The generated modules have no SaaS runtime dependency.
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { transform } from 'esbuild';

const root = fileURLToPath(new URL('../', import.meta.url));
const source = process.argv[2];
if (!source) throw new Error('Usage: node scripts/import-jira-domain.mjs <saas/packages/lib/src>');
const modules = ['constants', 'fields', 'workflow', 'board', 'permissions', 'utils/mentions', 'utils/references'];
const suites = ['agile', 'fields', 'workflow', 'board', 'permissions', 'mentions', 'references'];
const provenance = [];
for (const [relative, destination, isTest] of [
  ...modules.map(name => [`${name}.ts`, `shared/jira/${name}.js`, false]),
  ...suites.map(name => [`__tests__/${name}.test.ts`, `test/jira-domain/${name}.test.js`, true]),
  ['../../../apps/jira/src/lib/reports.ts', 'shared/jira/reports.js', false],
  ['../../../apps/jira/src/lib/reports.test.ts', 'test/jira-domain/reports.test.js', true],
]) {
  const input = await readFile(path.join(source, relative), 'utf8');
  let { code } = await transform(input, { loader: 'ts', format: 'esm', target: 'es2022', legalComments: 'inline' });
  if (code.includes('process.env.')) {
    code = `const legacyEnvironment = typeof process === 'undefined' ? {} : process.env;\n${code.replaceAll('process.env.', 'legacyEnvironment.')}`;
  }
  code = code.replace(/from (["'])(\.{1,2}\/[^"']+)\1/g, (_match, quote, imported) => {
    const target = isTest ? `../../shared/jira/${imported.replace(/^\.{1,2}\//, '')}.js` : `${imported}.js`;
    return `from ${quote}${target}${quote}`;
  });
  const target = path.join(root, destination);
  await mkdir(path.dirname(target), { recursive: true });
  await writeFile(target, `// Ported from SaaS packages/lib/src/${relative}; see provenance.json.\n${code}`);
  provenance.push({ source: relative, target: destination, sha256: createHash('sha256').update(input).digest('hex') });
}
await writeFile(path.join(root, 'shared/jira/provenance.json'), `${JSON.stringify({ sourceRepository: 'saas', description: 'Pure Jira domain rules and their original assertions. TypeScript is transpiled and optional process.env reads are guarded for browsers; domain semantics are unchanged. Runtime is entirely local to Vault.', files: provenance }, null, 2)}\n`);
