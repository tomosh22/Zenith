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
import {
  adoptStaleKnowledgeRows,
  DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION,
  EMPTY_NARROWING,
  resolveMaxMoves,
} from '../scripts/adopt-stale-knowledge-rows.mjs';
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
function metadata({
  seededRowId = null,
  staleRow = null,
  staleRows = [],
  staleExtraProperty = false,
  selectProperty = false,
  staleProjectId = 'proj-stale',
  duplicateOrgDatabase = false,
  duplicateStaleOrgDatabase = false,
  mixedCaseOrgSlug = false,
} = {}) {
  const seeded = seededRowId
    ? `
    - id: ${seededRowId}
      database_id: db-decisions-live
      values: {decision-id: "FD-D-004", decision-text: "A seeded decision."}`
    : '';
  // A row already sitting in the stale database, keyed by the STALE side's
  // property ids — the shape ZEN-90 left behind on the live board. `staleRow`
  // (one) and `staleRows` (many, ZEN-104 DoD 2) compose rather than compete —
  // existing callers only ever pass the former.
  const allStaleRows = [...(staleRow ? [staleRow] : []), ...staleRows];
  const stranded = allStaleRows
    .map(row => `
    - id: ${row.id}
      database_id: db-decisions-stale
      created_at: "2026-09-19T10:00:00.000Z"
      values: {${row.values}}`)
    .join('');
  // ZEN-104 DoD 3: a THIRD same-titled database in the SAME organisation as
  // the live one (both under `proj-live`) — the shape a move must refuse
  // rather than silently merge, because title alone cannot tell it from the
  // live database. Review round 3, finding 3: this now carries its own
  // property set and a row, so that if the refusal below were REMOVED the
  // row would actually be merged in and change `metadata.md` -- otherwise
  // the byte-identity assertion in the test passes whether or not the guard
  // exists, because a topology with nothing to move takes the "nothing to
  // adopt" early return regardless.
  const duplicateDatabase = duplicateOrgDatabase
    ? `
    - {id: db-decisions-live-dup, title: "Agent · Decisions", project_id: proj-live}`
    : '';
  const duplicateProperties = duplicateOrgDatabase
    ? `
    - {id: dup-id, database_id: db-decisions-live-dup, name: ID, type: TEXT}
    - {id: dup-text, database_id: db-decisions-live-dup, name: Decision, type: TEXT}`
    : '';
  const duplicateRow = duplicateOrgDatabase
    ? `
    - id: dup-row-1
      database_id: db-decisions-live-dup
      values: {dup-id: "FD-D-777", dup-text: "Should never merge silently."}`
    : '';
  // Review round 3, finding 1: TWO same-titled databases in the SAME
  // organisation as EACH OTHER (both under `proj-stale`), neither of which
  // matches the destination's organisation. The old destination-vs-source-only
  // comparison never fires here; the widened pairwise comparison must.
  const duplicateStaleDatabase = duplicateStaleOrgDatabase
    ? `
    - {id: db-decisions-stale-dup, title: "Agent · Decisions", project_id: proj-stale}`
    : '';
  const duplicateStaleProperties = duplicateStaleOrgDatabase
    ? `
    - {id: dup2-id, database_id: db-decisions-stale-dup, name: ID, type: TEXT}
    - {id: dup2-text, database_id: db-decisions-stale-dup, name: Decision, type: TEXT}`
    : '';
  const duplicateStaleRow = duplicateStaleOrgDatabase
    ? `
    - id: dup2-row-1
      database_id: db-decisions-stale-dup
      values: {dup2-id: "FD-D-778", dup2-text: "Also should never merge silently."}`
    : '';
  // Review round 4, finding 2's own regression test: an organisation whose
  // slug differs from the live one ONLY IN CASE. `databaseModel`
  // (cli/queries.mjs:493) lower-cases when it picks the live database, so
  // `{slug: "pink-goat"}` and `{slug: "Pink-Goat"}` are ONE organisation to
  // the resolver -- the guard must agree, or it fails open in exactly the
  // ambiguity it exists to catch (replacing the guard's `.toLowerCase()`
  // with a bare `===` restored this bug and every existing test still
  // passed). Own organisation/project so the id chain is genuinely
  // independent of `org-live`/`proj-live`, and its own property set + row so
  // both assertions are load-bearing, same as the other DoD 3 fixtures.
  const mixedCaseOrg = mixedCaseOrgSlug
    ? `
    - {id: org-live-mixed, slug: "Pink-Goat"}`
    : '';
  const mixedCaseProject = mixedCaseOrgSlug
    ? `
    - {id: proj-live-mixed, organization_id: org-live-mixed}`
    : '';
  const mixedCaseDatabase = mixedCaseOrgSlug
    ? `
    - {id: db-decisions-live-mixed, title: "Agent · Decisions", project_id: proj-live-mixed}`
    : '';
  const mixedCaseProperties = mixedCaseOrgSlug
    ? `
    - {id: mixed-id, database_id: db-decisions-live-mixed, name: ID, type: TEXT}
    - {id: mixed-text, database_id: db-decisions-live-mixed, name: Decision, type: TEXT}`
    : '';
  const mixedCaseRow = mixedCaseOrgSlug
    ? `
    - id: mixed-row-1
      database_id: db-decisions-live-mixed
      values: {mixed-id: "FD-D-779", mixed-text: "Should never merge silently either."}`
    : '';
  // A property the stale database defines and the live one does not, so a
  // migration that remapped by name has nowhere to put its value.
  const extraProperty = staleExtraProperty
    ? `
    - {id: stale-extra, database_id: db-decisions-stale, name: Rationale, type: TEXT}`
    : '';
  // ZEN-104 DoD 4/5: a SELECT property present on BOTH sides but with
  // DIFFERENT option ids for the shared "Testing" label, plus an option
  // ("Orphaned") the live side does not define at all -- the live data this
  // migration actually ran against happened to share option ids, so only a
  // fixture can exercise the id-remap and the raw-label fall-through.
  const selectProperties = selectProperty
    ? `
    - {id: live-category, database_id: db-decisions-live, name: Category, type: SELECT, options: [{id: opt-live-testing, label: Testing}]}
    - {id: stale-category, database_id: db-decisions-stale, name: Category, type: SELECT, options: [{id: opt-stale-testing, label: Testing}, {id: opt-stale-orphan, label: Orphaned}]}`
    : '';
  return `---
kind: metadata
tables:
  public.organizations:
    - {id: org-stale, slug: acme-corp}
    - {id: org-live, slug: pink-goat}${mixedCaseOrg}
  notion.projects:
    - {id: proj-stale, organization_id: org-stale}
    - {id: proj-live, organization_id: org-live}${mixedCaseProject}
  notion.databases:
    - {id: db-decisions-stale, title: "Agent · Decisions", project_id: ${staleProjectId}}
    - {id: db-decisions-live, title: "Agent · Decisions", project_id: proj-live}${duplicateDatabase}${duplicateStaleDatabase}${mixedCaseDatabase}
  notion.database_properties:
    - {id: decision-id, database_id: db-decisions-live, name: ID, type: TEXT}
    - {id: decision-text, database_id: db-decisions-live, name: Decision, type: TEXT}
    - {id: decision-date, database_id: db-decisions-live, name: Date, type: TEXT}
    - {id: stale-id, database_id: db-decisions-stale, name: ID, type: TEXT}
    - {id: stale-text, database_id: db-decisions-stale, name: Decision, type: TEXT}${extraProperty}${selectProperties}${duplicateProperties}${duplicateStaleProperties}${mixedCaseProperties}
  notion.database_rows:${seeded}${stranded}${duplicateRow}${duplicateStaleRow}${mixedCaseRow}
---
`;
}

async function makeVault(t, {
  seededRowId = null,
  ticketBody = 'Body.',
  staleRow = null,
  staleRows = [],
  staleExtraProperty = false,
  selectProperty = false,
  staleProjectId = 'proj-stale',
  duplicateOrgDatabase = false,
  duplicateStaleOrgDatabase = false,
  mixedCaseOrgSlug = false,
} = {}) {
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
  await fs.writeFile(path.join(root, 'metadata.md'), metadata({
    seededRowId,
    staleRow,
    staleRows,
    staleExtraProperty,
    selectProperty,
    staleProjectId,
    duplicateOrgDatabase,
    duplicateStaleOrgDatabase,
    mixedCaseOrgSlug,
  }));
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

// ZEN-104 DoD 2's fixtures need MORE stray rows than one, to cross the
// `--max-moves` threshold. Each gets its own row id and its own `ID` value,
// prefixed "9" so none collide with each other, with STRANDED's "FD-D-001",
// or with the hardcoded "FD-D-004" a `seededRowId` fixture always carries
// (DoD 2b pairs this helper with `seededRowId` deliberately).
function manyStaleRows(count) {
  return Array.from({ length: count }, (_, at) => {
    const n = at + 1;
    return {
      id: `stranded-many-${n}`,
      values: `stale-id: "FD-D-9${String(n).padStart(2, '0')}", stale-text: "Stray row ${n}."`,
    };
  });
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

// ZEN-104 DoD 4. The live vault this migration actually ran against happened
// to share SELECT option ids between the two databases (ZEN-D-029 corollary
// 2), so the id -> label -> id round trip was never exercised as an actual
// remap there. This fixture gives the two sides DIFFERENT option ids for the
// same label, so a naive `database_id`-only move (or a move that carried the
// source's raw option id across) would be caught here: the stored cell must
// be the DESTINATION's own id.
test('a SELECT cell is remapped to the destination\'s own option id, not the source\'s', async t => {
  const root = await makeVault(t, {
    selectProperty: true,
    staleRow: { id: 'stranded-select-1', values: `${STRANDED}, stale-category: "opt-stale-testing"` },
  });

  const result = await adoptStaleKnowledgeRows(root, { apply: true });
  assert.equal(result.applied, true);
  assert.equal(result.moved, 1);

  const stored = await storedRows(root);
  const moved = stored.find(row => row.id === 'stranded-select-1');
  assert.equal(moved.values['live-category'], 'opt-live-testing',
    "the destination's own option id is stored, not the source database's id for the same label");

  // And the ordinary reader shows the shared label, proving the round trip
  // landed on the right option rather than merely a different-but-wrong one.
  const rows = await readRows(root);
  assert.equal(rows[0].values.Category, 'Testing');
});

// ZEN-104 DoD 5. `cellFor`'s fall-through is deliberate (see the comment
// above it in adopt-stale-knowledge-rows.mjs): when the destination defines
// no option for the source's label, the human-legible label itself is
// carried across raw rather than the cell being dropped or the row refused.
// Three of the four rows ZEN-90 actually migrated hit exactly this path for
// real (Category "Testing", an option neither live database defines) and it
// was verified only by re-reading the moved rows afterwards, empirically --
// this fixture (label "Orphaned", chosen so it can't collide with the other
// test's "Testing") pins the same fall-through structurally instead.
test('a SELECT option the destination does not define survives as the raw label', async t => {
  const root = await makeVault(t, {
    selectProperty: true,
    staleRow: { id: 'stranded-select-2', values: `${STRANDED}, stale-category: "opt-stale-orphan"` },
  });

  const result = await adoptStaleKnowledgeRows(root, { apply: true });
  assert.equal(result.applied, true);

  const stored = await storedRows(root);
  const moved = stored.find(row => row.id === 'stranded-select-2');
  assert.equal(moved.values['live-category'], 'Orphaned',
    'no matching option in the destination -- the raw label survives, not the source id and not null');
});

// ZEN-104 DoD 1. The organisation each side belongs to must be visible in the
// plan itself, not just inferrable by reading `metadata.md` by hand -- a dry
// run is only a safeguard if the thing it needs to show is actually shown.
test('DoD 1: the plan states which organisation each side belongs to', async t => {
  const root = await makeVault(t, { staleRow: { id: 'stranded-1', values: STRANDED } });

  const plan = await adoptStaleKnowledgeRows(root);
  assert.equal(plan.organisations.length, 1);
  const [entry] = plan.organisations;
  assert.equal(entry.database, 'decisions');
  assert.equal(entry.to.id, 'db-decisions-live');
  assert.equal(entry.to.organisation, 'pink-goat');
  assert.equal(entry.from.length, 1);
  assert.equal(entry.from[0].id, 'db-decisions-stale');
  assert.equal(entry.from[0].organisation, 'acme-corp');
});

// The dangerous configuration ZEN-D-029 corollary 3 warns about is exactly a
// broken or absent organisation link -- this is the branch that must not
// guess. `organisationOf` reports the reason it could not resolve instead of
// defaulting to `null` silently or, worse, to whichever slug happens to sort
// first.
test('DoD 1: an unresolved organisation link is reported, not guessed at', async t => {
  const root = await makeVault(t, {
    staleRow: { id: 'stranded-1', values: STRANDED },
    staleProjectId: 'proj-does-not-exist',
  });

  const plan = await adoptStaleKnowledgeRows(root);
  const [entry] = plan.organisations;
  assert.equal(entry.from[0].organisation, null, 'unresolvable, not guessed at as either real slug');
  assert.match(entry.from[0].unresolved, /no project links to database/);
  assert.equal(entry.to.organisation, 'pink-goat', 'the resolvable side is unaffected by the other side breaking');
  assert.equal(entry.to.unresolved, undefined);
});

// ZEN-104 DoD 3. A title cannot disambiguate two databases in the SAME
// organisation, so a move must refuse rather than merge them -- naming both,
// so whoever reads the refusal knows which two to go look at.
//
// Review round 3, finding 3: the fixture carries its own property set and a
// row on `db-decisions-live-dup` (see the comment in `metadata()`), so both
// assertions below are load-bearing. Without that row, `planFor` would
// return `moves: []` regardless of whether the guard exists (there would be
// nothing to migrate either way), and the byte-identity assertion would pass
// even with the guard deleted -- proving only that the refusal fires on the
// topology, never that it PREVENTS anything.
test('DoD 3: two same-titled databases in the same organisation are refused, naming both', async t => {
  const root = await makeVault(t, { duplicateOrgDatabase: true });
  const before = await metadataBytes(root);

  await assert.rejects(
    () => adoptStaleKnowledgeRows(root, { apply: true }),
    /same organisation \("pink-goat"\).*db-decisions-live and db-decisions-live-dup/,
  );
  assert.deepEqual(await metadataBytes(root), before, 'the refusal left metadata.md untouched');
  assert.equal((await readRows(root)).length, 0, 'the duplicate-organisation database\'s row was not merged in');

  const stored = await storedRows(root);
  const dupRow = stored.find(row => row.id === 'dup-row-1');
  assert.ok(dupRow, 'the row still exists');
  assert.equal(dupRow.database_id, 'db-decisions-live-dup', 'and was never relocated');

  // The negative arm -- two same-titled databases in DIFFERENT organisations
  // must still migrate normally -- is already covered structurally by every
  // other adoption test in this file: their fixture IS exactly that shape
  // (db-decisions-stale in acme-corp, db-decisions-live in pink-goat), and
  // "a stranded row is adopted..." above asserts the move succeeds. Not
  // duplicated here.
});

// Review round 3, finding 1. The guard used to compare each source only
// against the DESTINATION, so two SOURCES sharing an organisation with each
// other (but not with the destination) were never compared and their rows
// would have been silently combined into the destination -- two same-titled
// databases in ONE organisation merged, which is exactly what DoD 3 exists
// to prevent. This topology -- destination in pink-goat, TWO stray databases
// both in acme-corp -- is what the old destination-vs-source-only check
// missed and the widened pairwise check must catch.
test('DoD 3: two same-titled SOURCE databases in the same organisation are refused too, not just source-vs-destination', async t => {
  const root = await makeVault(t, { duplicateStaleOrgDatabase: true });
  const before = await metadataBytes(root);

  await assert.rejects(
    () => adoptStaleKnowledgeRows(root, { apply: true }),
    /same organisation \("acme-corp"\).*db-decisions-stale and db-decisions-stale-dup/,
  );
  assert.deepEqual(await metadataBytes(root), before, 'the refusal left metadata.md untouched');
  assert.equal((await readRows(root)).length, 0, 'neither acme-corp database\'s row was merged in');

  const stored = await storedRows(root);
  const dupRow = stored.find(row => row.id === 'dup2-row-1');
  assert.ok(dupRow, 'the row still exists');
  assert.equal(dupRow.database_id, 'db-decisions-stale-dup', 'and was never relocated');
});

// Review round 4, finding 2's own regression: replacing the guard's
// `a.slug.toLowerCase() === b.slug.toLowerCase()` with a bare `a.slug ===
// b.slug` restored the exact fails-open bug the round-3 fix closed, and all
// 23 tests at the time still passed -- nothing pinned it. `databaseModel`
// (cli/queries.mjs:493) lower-cases when it picks the live database, so
// `{slug: "pink-goat"}` and `{slug: "Pink-Goat"}` are ONE organisation to the
// resolver, which would pick whichever database comes first between them;
// a case-sensitive guard sees two DIFFERENT organisations and stays silent.
// The guard must agree with the resolver about what counts as "the same
// organisation", or it fails open in precisely the ambiguity it exists to
// catch.
test('DoD 3: two same-titled databases whose organisation slugs differ only in case are refused', async t => {
  const root = await makeVault(t, { mixedCaseOrgSlug: true });
  const before = await metadataBytes(root);

  await assert.rejects(
    () => adoptStaleKnowledgeRows(root, { apply: true }),
    /same organisation \("pink-goat"\).*db-decisions-live and db-decisions-live-mixed/,
  );
  assert.deepEqual(await metadataBytes(root), before, 'the refusal left metadata.md untouched');
  assert.equal((await readRows(root)).length, 0, 'the mixed-case-organisation database\'s row was not merged in');

  const stored = await storedRows(root);
  const mixedRow = stored.find(row => row.id === 'mixed-row-1');
  assert.ok(mixedRow, 'the row still exists');
  assert.equal(mixedRow.database_id, 'db-decisions-live-mixed', 'and was never relocated');
});

// ZEN-104 DoD 2. Nothing exercised this guard before now: the condition is
// "the resolved destination holds none of its own rows AND the source is
// bigger than --max-moves", which is exactly the shape a resolver inversion
// takes (the side `databaseModel` picked as "live" turns out to be the empty
// one). Four sub-cases, per the coordinator's list.
test('DoD 2a: refuses a move into an empty destination once the stray count exceeds --max-moves', async t => {
  const staleRows = manyStaleRows(DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION + 1);
  const root = await makeVault(t, { staleRows });
  const before = await metadataBytes(root);

  await assert.rejects(
    () => adoptStaleKnowledgeRows(root, { apply: true }),
    /holds zero rows of its own/,
  );
  assert.deepEqual(await metadataBytes(root), before, 'the refusal left metadata.md untouched');
  assert.equal((await readRows(root)).length, 0, 'nothing was adopted');
});

// The anti-vacuity arm: the SAME stray count, refused above, goes through
// once the destination already holds a row of its own -- proving the guard
// keys on the destination being EMPTY, not merely on how many rows would
// move. Without this arm, a guard that fired on stray count alone (which
// would also refuse a legitimate large first-time migration into a
// populated live database) would pass the (a) test just as well.
test('DoD 2b: the same stray count is NOT refused once the destination already holds a row of its own', async t => {
  const count = DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION + 1;
  const staleRows = manyStaleRows(count);
  const root = await makeVault(t, { seededRowId: 'seed-1', staleRows });

  const result = await adoptStaleKnowledgeRows(root, { apply: true });
  assert.equal(result.applied, true);
  assert.equal(result.moved, count, 'the guard keys on emptiness, not merely on count');
});

test('DoD 2c: --max-moves raises the limit and lets the same refused move through', async t => {
  const count = DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION + 1;
  const staleRows = manyStaleRows(count);
  const root = await makeVault(t, { staleRows });

  const result = await adoptStaleKnowledgeRows(root, { apply: true, maxMoves: count });
  assert.equal(result.applied, true);
  assert.equal(result.moved, count);
});

// DoD 2d. The default IS the safety guard, so a malformed override must
// refuse rather than silently keep the default -- the same "typo must not
// silently widen" rule `requestedKeys` already applies to `--database`.
test('DoD 2d: resolveMaxMoves refuses a malformed override rather than silently keeping the default', () => {
  assert.equal(resolveMaxMoves(undefined), DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION);
  assert.equal(resolveMaxMoves(null), DEFAULT_MAX_MOVES_INTO_EMPTY_DESTINATION);
  assert.throws(() => resolveMaxMoves(EMPTY_NARROWING), /given no value/);
  assert.throws(() => resolveMaxMoves(''), /given no value/);
  assert.throws(() => resolveMaxMoves('-1'), /non-negative integer/);
  assert.throws(() => resolveMaxMoves('3.5'), /non-negative integer/);
  assert.throws(() => resolveMaxMoves('not-a-number'), /non-negative integer/);
});
