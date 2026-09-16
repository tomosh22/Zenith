import { defineConfig } from '@playwright/test';

export default defineConfig({
  testDir: './e2e',
  testMatch: '**/*.spec.mjs',
  fullyParallel: false,
  workers: 1,
  timeout: 60_000,
  expect: {
    timeout: 10_000,
  },
  reporter: [
    ['list'],
    ['json', { outputFile: 'artifacts/playwright/results.json' }],
    ['html', { outputFolder: 'artifacts/playwright/html', open: 'never' }],
  ],
  outputDir: 'artifacts/playwright/test-results',
  use: {
    trace: 'retain-on-failure',
  },
});
