import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import { test } from 'vitest';
import { fileURLToPath } from 'node:url';

import { dispatchRead } from '../cli/read.js';
import { loadVault } from '../server/main.js';

const fixtureRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../fixtures/snapshot');
const priority = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'TRIVIAL'];

function normalize(value, argv, index) {
  const output = JSON.parse(JSON.stringify(value));
  if (argv[0] === 'queue' && Array.isArray(output)) {
    // Queue order is the one declared deterministic difference in the frozen
    // reference: priority, position, then key.
    output.sort((left, right) => priority.indexOf(left.priority) - priority.indexOf(right.priority)
      || (index.tickets.get(left.key)?.metadata.position ?? 0)
        - (index.tickets.get(right.key)?.metadata.position ?? 0)
      || left.key.localeCompare(right.key));
  }
  return output;
}

test('all frozen CLI parity cases remain compatible', async () => {
  const index = await loadVault(path.join(fixtureRoot, 'vault'));
  const reference = JSON.parse(await readFile(path.join(fixtureRoot, 'reference.json'), 'utf8'));
  assert.equal(reference.cases.length, 1_860);

  for (const [number, caseValue] of reference.cases.entries()) {
    const actual = await dispatchRead(index, caseValue.argv, caseValue.files);
    assert.equal(actual.exitCode, caseValue.exitCode, `case ${number} exit code`);
    assert.deepEqual(
      JSON.parse(JSON.stringify(actual.payload)),
      normalize(caseValue.payload, caseValue.argv, index),
      `case ${number}: ${caseValue.argv.join(' ')}`,
    );
  }
});
