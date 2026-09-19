import { defineConfig } from 'vitest/config';

/**
 * Vite's SSR transform chokes on a `#!/usr/bin/env node` shebang in an ES
 * module it is asked to IMPORT, failing the whole suite file with a bare
 * `SyntaxError: Invalid or unexpected token` and no stack — which points at
 * nothing and cost a long bisect to find. Node imports those files happily and
 * `esbuild.transformSync` accepts them, so every obvious check says the source
 * is fine; only the import through vitest fails.
 *
 * Three modules are both CLI entry points (hence the shebang) and importable
 * (hence imported by tests): `cli/client.mjs`, `scripts/migrate.mjs` and
 * `scripts/rehearse.mjs`. Between them they took out six test files.
 *
 * Comment the shebang out rather than deleting it: `#!` becomes `//`, which is
 * the same byte length on the same line, so line numbers and source maps stay
 * aligned and the files remain executable on disk. Fixing it here rather than
 * banning shebangs in importable modules means the next such file is covered
 * without anyone having to remember the rule.
 */
const stripShebang = {
  name: 'zenith-strip-shebang',
  enforce: 'pre',
  transform(code, id) {
    if (!code.startsWith('#!') || !/\.(mjs|js)$/.test(id.split('?')[0])) return null;
    return { code: `//${code.slice(2)}`, map: null };
  },
};

export default defineConfig({
  plugins: [stripShebang],
  test: {
    include: ['test/**/*.test.{js,mjs,jsx}'],
    testTimeout: 120_000,
    hookTimeout: 30_000,
    teardownTimeout: 30_000,
    coverage: {
      provider: 'v8',
      reportsDirectory: './artifacts/coverage',
      reporter: ['text', 'json', 'html'],
      all: true,
      include: [
        'cli/**/*.{js,mjs}',
        'server/**/*.{js,mjs}',
        'scripts/**/*.{js,mjs}',
        'web/**/*.{js,jsx}',
        'shared/**/*.{js,mjs}',
      ],
      exclude: ['**/*.test.{js,mjs,jsx}', 'web/dist/**'],
      reportOnFailure: true,
      thresholds: {
        statements: 76,
        branches: 72,
        functions: 63,
        lines: 80,
      },
    },
  },
});
