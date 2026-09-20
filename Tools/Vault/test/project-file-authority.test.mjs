// `zagent.project.json` is the authority for gates and categories, read FRESH.
//
// The repo's own CLAUDE.md states both the rule and the reason:
//
//   "The file is sent every time rather than registered once. A stored copy
//    goes stale, and a stale gate list is exactly how a green run ratchets the
//    wrong pinned baseline."
//
// The Vault cutover broke that. `vault.config.json` carries a SNAPSHOT of the
// file — in TWO places, `policy.client.file` and `policy.resolved.projects.
// <KEY>` — taken by the migration, with nothing to resync either. Measured
// 2026-09-20: the on-disk file declared 15 Engine gate lines and a `Tooling`
// category while `zagent gates ZEN-14` resolved 13 and no `Tooling`, so two
// gate-coverage fixes committed that morning were silently INERT.
//
// It is silent by construction: a gate list that is merely SHORTER still goes
// green. Nothing reports it, and the fix it was hiding was itself a fix for
// "no gate runs non-C++ code".
//
// The two-copy detail is what makes this worth a test rather than a comment.
// Overriding `policy.client.file` alone makes `policyOf` report the new
// categories while `gatesOf` — which reads `policy.resolved` — keeps the stale
// list, so the config appears to work and the tick still runs the wrong gates.
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { gatesOf, policyOf, legacyConfigOf } from '../cli/compat.mjs';
import { loadVault } from '../server/main.js';

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
---
`;

// Both embedded copies carry the STALE category, so a fix to either one alone
// leaves the other visibly wrong.
function vaultConfig(root) {
  const staleCategories = { Engine: { paths: ['Zenith/**'], gates: ['stale-gate'] } };
  const project = {
    repo: root,
    baseBranch: 'master',
    branching: 'direct',
    gates: [],
    merge: { push: false },
    reviewOn: [],
    categories: staleCategories,
  };
  return {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: [],
    writes: { enabled: true, commit: false, agentEmail: 'agent@example.test' },
    policy: {
      board: { agentEmail: 'agent@example.test', guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 } },
      client: { file: { project: 'FD', baseBranch: 'master', branching: 'direct', merge: { push: false }, categories: staleCategories } },
      resolved: {
        routing: {},
        guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 },
        projects: { FD: project },
      },
    },
  };
}

// What a human would edit, with a category the snapshot has never heard of.
const PROJECT_FILE = {
  $comment: 'a comment key, which must be stripped rather than surfaced',
  project: 'FD',
  baseBranch: 'master',
  branching: 'direct',
  merge: { push: false },
  reviewOn: [],
  protectedPaths: ['.claude/**'],
  categories: {
    Engine: {
      $gates: ['a comment array that must NOT be mistaken for gates'],
      paths: ['Zenith/**'],
      gates: ['fresh-engine-gate', 'fresh-engine-gate-2'],
    },
    Tooling: {
      paths: ['Tools/**'],
      gates: ['fresh-tooling-gate'],
    },
  },
};

async function makeVault(t, { writeProjectFile = true } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-project-authority-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify(vaultConfig(root), null, 2));
  await fs.writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await fs.writeFile(
    path.join(root, 'FD', 'tickets', 'FD-1.md'),
    '---\nkey: FD-1\ntype: TASK\ntitle: One\nstatus: "To Do"\nassignee: null\ncategory: Engine\ncomplexity: SIMPLE\nrisk: LOW\n---\n\n## Goal\n\nX.\n\n## Definition of Done\n\n- [ ] Y.\n',
  );
  if (writeProjectFile) {
    await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify(PROJECT_FILE, null, 2));
  }
  return root;
}

const ticketOf = index => index.getTicket('FD-1');

test('gate selection reads the repo file, not the embedded snapshot', async t => {
  // The load-bearing one: this is what the tick actually runs.
  const root = await makeVault(t);
  const index = await loadVault(root);
  const resolved = gatesOf(index, ticketOf(index), 'FD', 'Engine');
  assert.deepEqual(resolved.gates, ['fresh-engine-gate', 'fresh-engine-gate-2']);
  assert.ok(!resolved.gates.includes('stale-gate'), 'the snapshot gate must not survive');
});

test('a category the snapshot has never seen is available for the union', async t => {
  // `Tooling` exists only in the repo file. Without the overlay reaching
  // `policy.resolved`, a diff touching Tools/** unions in nothing.
  const root = await makeVault(t);
  const index = await loadVault(root);
  const config = legacyConfigOf(index);
  const categories = config.projects.FD.categories;
  assert.ok(categories.Tooling, 'Tooling reached the resolved config');
  assert.deepEqual(categories.Tooling.gates, ['fresh-tooling-gate']);
});

test('BOTH embedded copies are overridden, not just policy.client.file', async t => {
  // Fixing one and not the other is how a config edit appears to work while the
  // gates stay stale: policyOf reports the new categories, gatesOf runs the old.
  const root = await makeVault(t);
  const index = await loadVault(root);
  assert.deepEqual(policyOf(index).categories.Engine.gates, ['fresh-engine-gate', 'fresh-engine-gate-2']);
  assert.deepEqual(legacyConfigOf(index).projects.FD.categories.Engine.gates, ['fresh-engine-gate', 'fresh-engine-gate-2']);
});

test('$-prefixed comment keys are stripped, so $gates cannot be read as gates', async t => {
  const root = await makeVault(t);
  const index = await loadVault(root);
  const engine = legacyConfigOf(index).projects.FD.categories.Engine;
  assert.equal(engine.$gates, undefined, 'the comment array is gone');
  assert.equal(policyOf(index).clientFile.$comment, undefined);
  assert.deepEqual(engine.gates, ['fresh-engine-gate', 'fresh-engine-gate-2']);
});

test('machine-local `repo` is NOT taken from the repo file', async t => {
  // The checkout path belongs to vault.config.json alone; the committed file
  // has no business carrying one and must not blank it.
  const root = await makeVault(t);
  const index = await loadVault(root);
  assert.equal(legacyConfigOf(index).projects.FD.repo, root);
});

test('with no repo file, the embedded snapshot still stands', async t => {
  // Every fixture vault in this suite is in exactly this shape, so the
  // fallback is load-bearing for the rest of the tests.
  const root = await makeVault(t, { writeProjectFile: false });
  const index = await loadVault(root);
  assert.deepEqual(gatesOf(index, ticketOf(index), 'FD', 'Engine').gates, ['stale-gate']);
});

test('an edit to the repo file is picked up without reloading the process', async t => {
  // "Read fresh" is the whole point: a cached parse keyed only by path would
  // reproduce the defect one process-lifetime at a time.
  const root = await makeVault(t);
  const first = await loadVault(root);
  assert.deepEqual(gatesOf(first, ticketOf(first), 'FD', 'Engine').gates, ['fresh-engine-gate', 'fresh-engine-gate-2']);

  const edited = JSON.parse(JSON.stringify(PROJECT_FILE));
  edited.categories.Engine.gates = ['edited-gate'];
  await fs.writeFile(path.join(root, 'zagent.project.json'), JSON.stringify(edited, null, 2));
  // Touch the mtime forward: the cache is keyed on mtime+size, and a rewrite
  // within the same millisecond of the same length would otherwise be missed.
  const future = new Date(Date.now() + 2000);
  await fs.utimes(path.join(root, 'zagent.project.json'), future, future);

  const second = await loadVault(root);
  assert.deepEqual(gatesOf(second, ticketOf(second), 'FD', 'Engine').gates, ['edited-gate']);
});

test('a malformed repo file falls back instead of taking the board down', async t => {
  // It is committed, and `zagent doctor` is where a human should learn it is
  // bad — not by every read command throwing.
  const root = await makeVault(t);
  await fs.writeFile(path.join(root, 'zagent.project.json'), '{ this is not json');
  const index = await loadVault(root);
  assert.deepEqual(gatesOf(index, ticketOf(index), 'FD', 'Engine').gates, ['stale-gate']);
});
