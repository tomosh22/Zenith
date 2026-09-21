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
import { adoptStaleKnowledgeRows } from '../scripts/adopt-stale-knowledge-rows.mjs';
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
function metadata({ seededRowId = null, staleRow = null, staleExtraProperty = false } = {}) {
  const seeded = seededRowId
    ? `
    - id: ${seededRowId}
      database_id: db-decisions-live
      values: {decision-id: "FD-D-004", decision-text: "A seeded decision."}`
    : '';
  // A row already sitting in the stale database, keyed by the STALE side's
  // property ids — the shape ZEN-90 left behind on the live board.
  const stranded = staleRow
    ? `
    - id: ${staleRow.id}
      database_id: db-decisions-stale
      created_at: "2026-09-19T10:00:00.000Z"
      values: {${staleRow.values}}`
    : '';
  // A property the stale database defines and the live one does not, so a
  // migration that remapped by name has nowhere to put its value.
  const extraProperty = staleExtraProperty
    ? `
    - {id: stale-extra, database_id: db-decisions-stale, name: Rationale, type: TEXT}`
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
    - {id: stale-text, database_id: db-decisions-stale, name: Decision, type: TEXT}${extraProperty}
  notion.database_rows:${seeded}${stranded}
---
`;
}

async function makeVault(t, { seededRowId = null, ticketBody = 'Body.', staleRow = null, staleExtraProperty = false } = {}) {
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
  await fs.writeFile(path.join(root, 'metadata.md'), metadata({ seededRowId, staleRow, staleExtraProperty }));
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

// ZEN ticket (2026-09-20): `appendKnowledgeRow` minted a reference only when
// `--project` was passed explicitly, unlike the four other derivations in
// this same file (`queueCandidate` :594, `create` :844, `sprint` :1006 and
// `version` :1103), which also fall back through
// `policyOf(index).clientFile.project` (the checkout's declared default
// project, e.g. `zagent.project.json`'s `project: "ZEN"`) and then
// `index.boardRoots?.[0]?.project`. So `zagent decide --file x.md` with
// neither `--ticket` nor `--project` minted no ID at all and stored the row
// with `ID: null` -- silently unciteable, which is the exact failure the
// minting in `mintDecisionId` was added to eliminate. `zagent finish`
// refuses to close a ticket whose "Durable findings" line cannot name a
// `PROJ-D-N` reference, so this made the omission unrecoverable at closing
// time rather than at filing time.
//
// The fix gives ONLY the minted-ID prefix that wider fallback -- the
// variable that decides where the row is WRITTEN, and that refuses the
// write outright when nothing names a project and no catalog exists, keeps
// its original narrow derivation (`write.mjs`'s `appendKnowledgeRow`, the
// `project` vs `mintProject` split). Widening the routing variable too once
// turned a deliberate refusal into a silent write to a guessed project --
// see `write-branches.test.mjs`'s "knowledge falls back to the metadata
// catalog and to a new knowledge file" case, which exists to catch exactly
// that.
//
// Repairing an already-written null-ID row is NOT mechanical, and the two
// shapes are not interchangeable:
//   - A null-ID row that is a BYTE-IDENTICAL duplicate of a row already
//     carrying a minted ID elsewhere must not get a second ID minted for
//     it -- that puts two different decision references on one decision,
//     which a citing ticket cannot disambiguate. The repair there is a
//     human judgement call about which row (if either) to remove or
//     annotate, not a script.
//   - A null-ID row that is a genuine orphan (no minted twin) can go
//     through the ordinary write path (or a dedicated migration written the
//     same way) to acquire one -- never a manual edit to `metadata.md`,
//     which races the same live storage `zagent decide` itself commits to.
test('decide with neither --ticket nor --project mints from the checkout default project', async t => {
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'No flags at all.']);
  assert.equal(res.exitCode, 0, res.payload?.error);
  assert.equal(res.payload.values.ID, 'FD-D-001', 'minted from policy.client.file.project, not left null');
});

test('an explicit --project still wins over the checkout default when both could apply', async t => {
  const root = await makeVault(t);
  const res = await write(root, ['decide', '--text', 'Explicit project.', '--project', 'ZZ']);
  assert.equal(res.exitCode, 0, res.payload?.error);
  assert.equal(res.payload.values.ID, 'ZZ-D-001', 'the explicit --project prefixes the minted ID, not the checkout default (FD)');
});

test('an explicitly supplied ID still wins even with no --ticket and no --project', async t => {
  const root = await makeVault(t);
  const body = JSON.stringify({ ID: 'FD-D-099', Decision: 'Renumbered by hand, no flags.' });
  const res = await write(root, ['decide', '--text', body]);
  assert.equal(res.exitCode, 0, res.payload?.error);
  assert.equal(res.payload.values.ID, 'FD-D-099');
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

// ZEN-90, the other half: fixing the WRITE path stops new rows being stranded
// and does nothing for the ones already stranded. Four real ZEN decisions were
// filed into the non-live database before the fix landed and no documented read
// command can retrieve any of them — including ZEN-D-004, which the tick
// protocol's scene-authoring reasoning cites.
//
// `scripts/adopt-stale-knowledge-rows.mjs` moves them, remapping every value by
// property NAME. The remap is the whole risk: the two databases share their
// property names and share not one property ID, so a migration that rewrote
// `database_id` alone would produce rows that LIST correctly and read as
// entirely empty. Assert the values, never the count.
const STRANDED = 'stale-id: "FD-D-001", stale-text: "Filed before the write path was fixed."';
const metadataBytes = root => fs.readFile(path.join(root, 'metadata.md'));

async function storedRows(root) {
  const index = await loadVault(root);
  const record = [...index.metadataFiles.values()].find(item => item.relativePath === 'metadata.md');
  return record.fields.tables['notion.database_rows'] ?? [];
}

test('a stranded row is adopted into the live database, values intact, and the move is idempotent', async t => {
  const root = await makeVault(t, { staleRow: { id: 'stranded-1', values: STRANDED } });
  assert.equal((await readRows(root)).length, 0, 'the stranded row is invisible to the reader to begin with');

  const first = await adoptStaleKnowledgeRows(root, { apply: true });
  assert.equal(first.applied, true);
  assert.equal(first.moved, 1);
  assert.equal(first.moves[0].reference, 'FD-D-001');

  const rows = await readRows(root);
  assert.equal(rows.length, 1, 'the adopted row is visible to the ordinary reader');
  assert.equal(rows[0].id, 'stranded-1', 'the row keeps its identity, so a citation of it still resolves');
  // Keyed by the LIVE database's property ids now: a `database_id`-only move
  // would leave every one of these null while the row still listed.
  assert.equal(rows[0].values.ID, 'FD-D-001');
  assert.equal(rows[0].values.Decision, 'Filed before the write path was fixed.');
  assert.equal(rows[0].values.Date, null, 'a property the source never carried is not invented');

  const stored = await storedRows(root);
  assert.equal(stored.filter(row => row.database_id === 'db-decisions-stale').length, 0, 'nothing is left behind in the stale database');
  const moved = stored.find(row => row.id === 'stranded-1');
  assert.equal(moved.database_id, 'db-decisions-live');
  assert.equal(moved.created_at, '2026-09-19T10:00:00.000Z', 'non-`values` fields survive the move verbatim');

  const second = await adoptStaleKnowledgeRows(root, { apply: true });
  assert.equal(second.applied, false, 'a second run finds nothing to move');
  assert.equal(second.moved, 0);
  assert.equal((await readRows(root)).length, 1, 'and duplicates nothing');
});

test('the default is a dry run: it reports the move and leaves the vault byte-identical', async t => {
  const root = await makeVault(t, { staleRow: { id: 'stranded-1', values: STRANDED } });
  const before = await metadataBytes(root);

  const plan = await adoptStaleKnowledgeRows(root);
  assert.equal(plan.applied, false);
  assert.equal(plan.moves.length, 1);
  assert.deepEqual(plan.moves[0].properties, ['ID', 'Decision'], 'the plan names the destination properties the values land in');

  assert.deepEqual(await metadataBytes(root), before, 'a plan writes nothing');
  assert.equal((await readRows(root)).length, 0);
});

test('a property the live database lacks is refused by name, and the refusal writes nothing', async t => {
  // The same plan runs in the preview and again inside `prepare` (ZEN-D-007),
  // so a refusal leaves the work tree byte-identical whichever one catches it.
  // Dropping the value instead would be silent data loss that every row-count
  // assertion would pass.
  const root = await makeVault(t, {
    staleExtraProperty: true,
    staleRow: { id: 'stranded-1', values: `${STRANDED}, stale-extra: "Why it was decided."` },
  });
  const before = await metadataBytes(root);

  await assert.rejects(() => adoptStaleKnowledgeRows(root, { apply: true }), /Rationale/);
  assert.deepEqual(await metadataBytes(root), before, 'the refusal left metadata.md untouched');
});
