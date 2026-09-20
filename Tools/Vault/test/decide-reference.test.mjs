// Regression tests for `zagent decide`'s reference handling.
//
// Three defects, all found by running /tick three times on ZEN and all of
// them silent — every one reported exit 0, a rowId and a commit:
//
//   1. `--text` was refused ("needs a JSON body") although the command's own
//      usage line advertises it, so the documented invocation failed and the
//      working one (a JSON --file) was undocumented.
//   2. No reference was minted. `ID` was the caller's to supply and nothing
//      said so, so a body omitting it stored `ID: null` — five of the seven
//      rows that existed when this was written are null-ID for that reason.
//      `zagent finish` REFUSES a terminal status whose work log names neither
//      "none" nor a PROJ-D-N reference, so a tick that learned something
//      durable could not close.
//   3. ZEN-90: writes landed in a DIFFERENT database than reads. `metadata.md`
//      carries two databases titled "Agent · Decisions" — one for the live
//      organisation and one left over from the migration fixture — and the
//      reader prefers the organisation's while the writer took the first
//      title match. Rows written were invisible to every read path.
//
// The fixture below reproduces (3) structurally: two same-titled databases,
// the live one deliberately listed SECOND so a first-match writer fails.
import assert from 'node:assert/strict';
import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { test } from 'vitest';

import { dispatchRead } from '../cli/read.js';
import { dispatchWrite } from '../cli/write.mjs';
import { loadVault } from '../server/main.js';
import { git } from '../server/operations.mjs';

const WORKFLOW = `---
kind: workflow
statuses:
  - {id: todo, name: To Do, category: TODO}
  - {id: progress, name: In Progress, category: IN_PROGRESS}
  - {id: done, name: Done, category: DONE}
transitions:
  - {from_status_id: todo, to_status_id: progress}
  - {from_status_id: progress, to_status_id: done}
---
`;

// Two "Agent · Decisions" databases. `db-decisions-stale` belongs to a
// project in another organisation and is listed FIRST, so a writer that
// takes the first title match writes somewhere no reader looks.
function metadata({ seededRowId = null } = {}) {
  const seeded = seededRowId
    ? `
    - id: ${seededRowId}
      database_id: db-decisions-live
      values: {decision-id: "FD-D-004", decision-text: "A seeded decision."}`
    : '';
  return `---
kind: metadata
tables:
  public.organizations:
    - {id: org-stale, slug: acme-corp}
    - {id: org-live, slug: pink-goat}
  notion.projects:
    - {id: proj-stale, organization_id: org-stale}
    - {id: proj-live, organization_id: org-live}
  notion.databases:
    - {id: db-decisions-stale, title: "Agent · Decisions", project_id: proj-stale}
    - {id: db-decisions-live, title: "Agent · Decisions", project_id: proj-live}
  notion.database_properties:
    - {id: decision-id, database_id: db-decisions-live, name: ID, type: TEXT}
    - {id: decision-text, database_id: db-decisions-live, name: Decision, type: TEXT}
    - {id: decision-date, database_id: db-decisions-live, name: Date, type: TEXT}
    - {id: stale-id, database_id: db-decisions-stale, name: ID, type: TEXT}
    - {id: stale-text, database_id: db-decisions-stale, name: Decision, type: TEXT}
  notion.database_rows:${seeded}
---
`;
}

async function makeVault(t, { seededRowId = null, ticketBody = 'Body.' } = {}) {
  const root = await fs.mkdtemp(path.join(os.tmpdir(), 'zenith-decide-ref-'));
  t.onTestFinished(() => fs.rm(root, { recursive: true, force: true }));
  await fs.mkdir(path.join(root, 'FD', 'tickets'), { recursive: true });
  const config = {
    formatVersion: 1,
    boardRoots: [{ project: 'FD', path: 'FD' }],
    supportingFiles: ['metadata.md'],
    writes: { enabled: true, commit: false, agentEmail: 'agent@example.test' },
    policy: {
      orgSlug: 'pink-goat',
      board: { agentEmail: 'agent@example.test', guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 } },
      client: { file: { project: 'FD', baseBranch: 'master', branching: 'direct', merge: { push: false } } },
      resolved: {
        routing: {}, guardrails: { protectedPaths: [], maxConsecutiveBlocked: 3 },
        projects: { FD: { repo: root, baseBranch: 'master', branching: 'direct', gates: [], merge: { push: false }, reviewOn: [] } },
      },
    },
  };
  await fs.writeFile(path.join(root, 'vault.config.json'), JSON.stringify(config, null, 2));
  await fs.writeFile(path.join(root, 'FD', 'workflow.md'), WORKFLOW);
  await fs.writeFile(path.join(root, 'metadata.md'), metadata({ seededRowId }));
  await fs.writeFile(
    path.join(root, 'FD', 'tickets', 'FD-1.md'),
    `---\nkey: FD-1\ntype: TASK\ntitle: One\nstatus: "To Do"\nassignee: null\ncategory: Engine\n---\n\n${ticketBody}\n`,
  );
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Decide reference tests']);
  await git(root, ['config', 'user.email', 'decide-reference@example.test']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'baseline']);
  return root;
}

const write = async (root, args) => dispatchWrite(await loadVault(root), args, {});
const readRows = async root => (await dispatchRead(await loadVault(root), ['rows', 'decisions'])).payload;

test('decide accepts plain --text, the form its own usage line advertises', async t => {
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'Plain prose, no JSON.', '--project', 'FD']);
  assert.equal(res.exitCode, 0, res.payload?.error);
  assert.equal(res.payload.values.Decision, 'Plain prose, no JSON.');
});

test('decide mints a reference when the body omits one', async t => {
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'First decision.', '--project', 'FD']);
  assert.equal(res.exitCode, 0);
  assert.equal(res.payload.values.ID, 'FD-D-001');
});

test('an explicitly supplied reference still wins', async t => {
  const root = await makeVault(t);
  const body = JSON.stringify({ ID: 'FD-D-042', Decision: 'Renumbered by hand.' });
  const res = await write(root, ['decide', '--text', body, '--project', 'FD']);
  assert.equal(res.exitCode, 0);
  assert.equal(res.payload.values.ID, 'FD-D-042');
});

test('minting continues from the highest reference already stored as a row', async t => {
  const root = await makeVault(t, { seededRowId: 'seed-1' });
  const res = await write(root, ['decide', '--text', 'Next one.', '--project', 'FD']);
  assert.equal(res.exitCode, 0);
  assert.equal(res.payload.values.ID, 'FD-D-005');
});

test('minting also avoids a reference cited only in a ticket body', async t => {
  // ZEN-D-001..004 are cited in ZEN-12's body and exist as no row at all, so
  // a row-only scan minted ZEN-D-001 a second time and pointed two different
  // decisions at one reference.
  const root = await makeVault(t, { ticketBody: 'Supersedes FD-D-009 and FD-D-010, see those.' });
  const res = await write(root, ['decide', '--text', 'After the cited ones.', '--project', 'FD']);
  assert.equal(res.exitCode, 0);
  assert.equal(res.payload.values.ID, 'FD-D-011');
});

test('ZEN-90: a decision written is a decision readable', async t => {
  // The load-bearing one. With two same-titled databases, the write must land
  // in the organisation's — the one `rows decisions` returns — not the first
  // title match. Before the fix this wrote to db-decisions-stale and read
  // back an empty list, with exit 0 and a rowId to say it had worked.
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'Must be readable.', '--project', 'FD']);
  assert.equal(res.exitCode, 0);

  const rows = await readRows(root);
  assert.equal(rows.length, 1, 'the written decision is visible to the reader');
  assert.equal(rows[0].values.Decision, 'Must be readable.');
  assert.equal(rows[0].values.ID, 'FD-D-001');
});

test('the stale-organisation database is left untouched', async t => {
  // Proves the previous test is not passing because BOTH databases got a row.
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'Only one home.', '--project', 'FD']);
  assert.equal(res.exitCode, 0);

  // Read the store back through the vault rather than by scanning text: a
  // write rewrites metadata.md as single-line JSON, so splitting on newlines
  // matches every database at once and would assert nothing.
  const index = await loadVault(root);
  const record = [...index.metadataFiles.values()].find(item => item.relativePath === 'metadata.md');
  const rows = record.fields.tables['notion.database_rows'] ?? [];

  const live = rows.filter(row => row.database_id === 'db-decisions-live');
  const stale = rows.filter(row => row.database_id === 'db-decisions-stale');
  assert.equal(live.length, 1, 'exactly one row, in the organisation\'s database');
  assert.equal(stale.length, 0, 'nothing was appended to the stale organisation\'s database');
});
