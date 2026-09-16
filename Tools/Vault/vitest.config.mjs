import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    include: ['test/**/*.test.{js,mjs,jsx}'],
    testTimeout: 30_000,
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
      ],
      exclude: ['**/*.test.{js,mjs,jsx}'],
    },
  },
});
