/*
 * ZEN-90 — adopt the rows of a same-titled but NON-live knowledge database
 * into the one every read command actually resolves.
 *
 * WHAT WENT WRONG. `metadata.md` carries more than one database per knowledge
 * title. The reader (`databaseModel` in cli/queries.mjs) resolves the one
 * belonging to the configured organisation; the writers used to take the first
 * title match instead, so `zagent decide` reported exit 0, a rowId and a
 * commit while `zagent rows decisions` could never see the row. The write path
 * was fixed to call the reader's own resolver (see the ZEN-90 comment at
 * cli/write.mjs's `knowledgeDatabase`), which stops NEW rows being stranded —
 * but says nothing about the ones already sitting in the other database. This
 * script is the "or migrated" half of that ticket's
 * "either migrated or still readable".
 *
 * ★ IT REMAPS BY PROPERTY NAME, NEVER BY MOVING `database_id` ALONE. The two
 * databases share their nine property NAMES and share not one property ID, so
 * a row whose `database_id` is rewritten and whose `values` are left alone
 * arrives keyed by ids the destination does not define: every cell reads as
 * empty while the row itself still lists. That is silent total data loss that
 * passes a row-count check, which is why this refuses rather than drops
 * whenever a name cannot be matched.
 *
 * ★ IT GOES THROUGH THE VAULT'S OWN WRITE MACHINERY — `executeOperation` plus
 * `patchRecord` — and never serialises `metadata.md` itself. Two reasons, and
 * both are recorded rules rather than taste:
 *   - `Tools/Vault/test/decide-reference.test.mjs` (the null-ID paragraph):
 *     repairing a stored knowledge row goes through the ordinary write path or
 *     "a dedicated migration written the same way", NEVER a manual edit to
 *     `metadata.md`, which races the same live storage `zagent decide` commits
 *     to.
 *   - ZEN-D-007: `executeOperation` calls `prepare` before it writes a journal
 *     byte or a commit, so validation placed INSIDE `prepare` is atomic for
 *     free. Everything this script checks, it checks there.
 * `patchRecord` also rewrites only the `tables:` field of the frontmatter and
 * leaves every other byte of the file alone, so the migration adds no
 * reformatting of its own — reformatting being the ticket's own secondary
 * complaint.
 *
 * ★ IT DOES NOT TOUCH NULL-ID ROWS, AND DELIBERATELY SO. The live decisions
 * database holds rows whose `ID` is null. Per the same passage in
 * decide-reference.test.mjs, repairing those is a human judgement call (a
 * byte-identical duplicate of an already-minted row must NOT get a second
 * reference minted for it), not a script. This script mints nothing, deletes
 * nothing and edits no row that is already in the live database.
 *
 * ★ NO THIRD RESOLUTION OF "WHICH DATABASE IS LIVE". The destination is
 * whatever `databaseModel` returns, imported from the reader — the same single
 * resolution ZEN-90 left behind. Re-deriving the organisation here to
 * "double-check" it would reintroduce exactly the two-answers-to-one-question
 * defect this ticket is about. The dry run is the default precisely so the
 * plan is read by a human before anything moves.
 *
 * Usage:
 *   node scripts/adopt-stale-knowledge-rows.mjs --vault ROOT
 *   node scripts/adopt-stale-knowledge-rows.mjs --vault ROOT --apply
 *   node scripts/adopt-stale-knowledge-rows.mjs --vault ROOT --database decisions --apply
 *
 * Without `--apply` nothing is written: the plan is printed and the vault is
 * left byte-identical. With `--apply` the move lands as one atomic operation
 * and (when the vault commits) one `board: adopt-stale-knowledge-rows` commit.
 * A second `--apply` finds nothing to move and exits 0.
 */

import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { clone } from '../cli/compat.mjs';
import { DATABASE_KEYS, databaseModel, rowsFor } from '../cli/queries.mjs';
import { expectedRevision, patchRecord } from '../cli/write.mjs';
import { executeOperation, withAuthoritativeIndex } from '../server/operations.mjs';

export class AdoptionError extends Error {
  constructor(message) {
    super(message);
    this.name = 'AdoptionError';
  }
}

// Distinguishes "--database was not passed" (plan over all five) from
// "--database was passed with nothing after it" (a typo, which must refuse).
// A bare `undefined` cannot tell those apart, and the difference decides
// whether one database or five get swept.
export const EMPTY_NARROWING = Symbol('--database given no value');

const array = value => (Array.isArray(value) ? value : []);
const object = value => (value && typeof value === 'object' && !Array.isArray(value) ? value : {});

// The writer's own matching rule (`lowerFieldMap` in cli/write.mjs): a
// property is addressed by NAME, case- and separator-insensitively, so
// "Tests that lock it" matches however either side spells it.
const nameKey = name => String(name ?? '').toLowerCase().replace(/[ _-]/g, '');

// Same rule as `metadataRecord` in cli/write.mjs, which is not exported.
function metadataRecordOf(index) {
  const records = [...(index?.metadataFiles?.values?.() ?? [])];
  return records.find(record => String(record.relativePath).toLowerCase() === 'metadata.md') ?? records[0];
}

// A SELECT cell is stored as an option id, and option ids are NOT guaranteed to
// agree across two same-titled databases the way their property ids are
// guaranteed to differ. (Measured in this vault on 2026-09-21: the two
// `Agent · Decisions` databases happen to share option ids AND labels for
// Category/Repo/Reversibility, so the round trip below is the identity here.
// An earlier revision of this comment asserted the option ids "differ exactly
// as their property ids do", which is false for the one vault it ran against —
// the code is right, the stated reason was not.) So resolve the source id to
// its label, then to the destination's own id for that label. When the destination offers
// no matching option the LABEL is carried through as the raw value: the reader
// (`readKnowledgeCell`) shows an unmatched value verbatim, so the human-legible
// text survives — dropping the cell or refusing the whole row would both lose
// more than they protect.
function labelOf(property, raw) {
  if (String(property?.type ?? '').toUpperCase() !== 'SELECT') return raw;
  const option = array(property?.options).find(item => String(item?.id ?? '') === String(raw));
  return option ? (option.label ?? raw) : raw;
}

function cellFor(property, label) {
  if (String(property?.type ?? '').toUpperCase() !== 'SELECT') return label;
  const option = array(property?.options).find(item => String(item?.label ?? '').toLowerCase() === String(label).toLowerCase());
  return option ? option.id : label;
}

/** Normalise `--database a,b` / `['a','b']` / omitted into a key list. */
export function requestedKeys(requested) {
  // OMITTED means all five, deliberately. But a PRESENT-yet-empty narrowing --
  // `--database` as the last argument (the CLI then reads `undefined` from
  // `args[i + 1]`), `--database ""`, or `--database ,,` -- is a typo, and
  // widening a typo from one database to all five is the opposite of what the
  // caller asked for. `Tools/CLAUDE.md`: refuse rather than guess. The two
  // cases are distinguished by the sentinel below, not by truthiness, because
  // `undefined` legitimately means both "flag absent" and "flag given no value".
  if (requested === undefined || requested === null) return [...DATABASE_KEYS];
  if (requested === EMPTY_NARROWING || String(requested).trim() === '') {
    throw new AdoptionError('--database was given no value. Name at least one of: '
      + `${DATABASE_KEYS.join(', ')} — or omit --database entirely to plan over all of them.`);
  }
  const keys = (Array.isArray(requested) ? requested : String(requested).split(','))
    .map(value => String(value).trim().toLowerCase())
    .filter(Boolean);
  const unknown = keys.filter(key => !DATABASE_KEYS.includes(key));
  if (unknown.length) {
    throw new AdoptionError(`Unknown knowledge database(s): ${unknown.join(', ')}. Known: ${DATABASE_KEYS.join(', ')}.`);
  }
  if (!keys.length) {
    throw new AdoptionError('--database listed no usable database name. Name at least one of: '
      + `${DATABASE_KEYS.join(', ')} — or omit --database entirely to plan over all of them.`);
  }
  return keys;
}

/**
 * Compute the whole migration against one index, and throw rather than guess.
 * Pure: it reads the index and returns the next `tables` object, touching no
 * file. Called once for the preview and again inside `prepare` (ZEN-D-007), so
 * every refusal below happens before a change set exists.
 */
export function planFor(index, keys = [...DATABASE_KEYS]) {
  const record = metadataRecordOf(index);
  if (!record) {
    throw new AdoptionError('This vault carries no metadata catalog, so there is no knowledge database to adopt rows into.');
  }
  const tables = clone(object(record.fields?.tables));
  const rows = array(tables['notion.database_rows']);
  const relocated = new Map();
  const moves = [];

  for (const key of keys) {
    let model;
    try {
      model = databaseModel(index, key);
    } catch {
      continue; // this vault carries no database with that title at all
    }
    const target = model.database;
    const title = String(target?.title ?? '');
    const strayIds = new Set(rowsFor(index, 'notion.databases')
      .filter(database => String(database?.title ?? '') === title && String(database?.id ?? '') !== String(target?.id ?? ''))
      .map(database => String(database.id)));
    if (!strayIds.size) continue;

    const sourceProperties = new Map();
    for (const property of rowsFor(index, 'notion.database_properties')) {
      if (strayIds.has(String(property?.database_id ?? ''))) sourceProperties.set(String(property?.id ?? ''), property);
    }
    // A Map keyed by nameKey silently keeps the LAST property when two names
    // normalise together ("Tests that lock it" / "tests_that_lock_it"), and the
    // earlier one then has no destination -- a value dropped without a refusal,
    // which is precisely what this script's header promises never to do. Two
    // colliding SOURCE names would be worse: both would write the same
    // destination id, last-wins. Unreachable in this vault (all nine names in
    // all five live databases normalise distinctly, verified 2026-09-21), so
    // this is a guard against a future property rename, not a live defect.
    const targetByName = new Map();
    for (const property of model.properties) {
      const normalised = nameKey(property?.name);
      const clash = targetByName.get(normalised);
      if (clash) {
        throw new AdoptionError(`${key}: the destination database defines two properties whose names collide when normalised — `
          + `"${clash?.name}" and "${property?.name}". A value cannot be routed unambiguously; rename one before migrating.`);
      }
      targetByName.set(normalised, property);
    }
    const sourceNameCounts = new Map();
    for (const property of sourceProperties.values()) {
      const normalised = nameKey(property?.name);
      sourceNameCounts.set(normalised, (sourceNameCounts.get(normalised) ?? 0) + 1);
    }
    for (const [normalised, count] of sourceNameCounts) {
      if (count > 1) {
        throw new AdoptionError(`${key}: a source database defines ${count} properties whose names collide when normalised `
          + `("${normalised}"). Both would write the same destination cell, last one wins; resolve by hand before migrating.`);
      }
    }
    const known = model.properties.map(property => String(property?.name ?? '')).join(', ');

    // A reference may name exactly one decision. Collect what the destination
    // already carries so a move can never mint a second row under an existing
    // ID — the hazard decide-reference.test.mjs's null-ID paragraph names.
    const idProperty = model.properties.find(property => nameKey(property?.name) === 'id');
    const claimed = new Map();
    if (idProperty) {
      for (const row of rows) {
        if (String(row?.database_id ?? '') !== String(target.id)) continue;
        const value = String(object(row?.values)[idProperty.id] ?? '').trim();
        if (value) claimed.set(value.toUpperCase(), String(row?.id ?? ''));
      }
    }

    let found = 0;
    for (const [at, row] of rows.entries()) {
      if (!strayIds.has(String(row?.database_id ?? ''))) continue;
      found += 1;
      const cells = {};
      const carried = [];
      for (const [propertyId, raw] of Object.entries(object(row?.values))) {
        const source = sourceProperties.get(String(propertyId));
        if (!source) {
          throw new AdoptionError(`${key}: row ${row?.id} carries a value under property id "${propertyId}", which no "${title}" database defines — refusing rather than dropping it.`);
        }
        const destination = targetByName.get(nameKey(source?.name));
        if (!destination) {
          throw new AdoptionError(`${key}: the live "${title}" database has no property named "${source?.name}", carried by row ${row?.id} — refusing rather than dropping the value. Known: ${known}.`);
        }
        cells[destination.id] = cellFor(destination, labelOf(source, raw));
        carried.push(String(destination?.name ?? ''));
      }
      let reference = null;
      if (idProperty) {
        reference = String(cells[idProperty.id] ?? '').trim() || null;
        if (reference) {
          const owner = claimed.get(reference.toUpperCase());
          if (owner) {
            throw new AdoptionError(`${key}: "${reference}" is already carried by row ${owner} in the live database — two rows cannot share one reference. Resolve by hand before migrating.`);
          }
          claimed.set(reference.toUpperCase(), String(row?.id ?? ''));
        }
      }
      // Only `database_id` and the `values` keys change. `id`, `created_at`
      // and anything else the row carries survive verbatim, so a citation of
      // the row id keeps resolving.
      relocated.set(at, { ...row, database_id: String(target.id), values: cells });
      moves.push({
        database: key,
        rowId: String(row?.id ?? ''),
        from: String(row?.database_id ?? ''),
        to: String(target.id),
        reference,
        properties: carried,
      });
    }

    // `tablesOf` resolves ONE array per table name (`index.tables` first, then
    // records in scan order), which need not be the record patched here if a
    // vault ever carries a second metadata file. If the reader can see a stray
    // row this file does not hold, the plan describes bytes this script is not
    // writing — refuse rather than half-migrate.
    const visible = rowsFor(index, 'notion.database_rows')
      .filter(row => strayIds.has(String(row?.database_id ?? ''))).length;
    if (visible !== found) {
      throw new AdoptionError(`${key}: the catalog exposes ${visible} stray row(s) but ${record.relativePath} holds ${found} — some live in another metadata file. Refusing rather than migrating half of them.`);
    }
  }

  if (!moves.length) return { record, moves, tables: null };
  return {
    record,
    moves,
    tables: { ...tables, 'notion.database_rows': rows.map((row, at) => relocated.get(at) ?? row) },
  };
}

/**
 * Plan (default) or perform the adoption.
 *
 * Returns `{ applied, moved, moves, file }`. `applied` is false for a dry run
 * AND for an `--apply` that found nothing to move, which is the idempotent
 * second run: same shape, exit 0, no operation opened and so no commit.
 */
export async function adoptStaleKnowledgeRows(root, options = {}) {
  const keys = requestedKeys(options.databases);
  const apply = options.apply === true;

  // The preview runs under the same lock and recovery a mutation would, so a
  // plan is never computed against a half-applied journal.
  const preview = await withAuthoritativeIndex(root, index => {
    const plan = planFor(index, keys);
    return { moves: plan.moves, file: plan.record.relativePath, revision: expectedRevision(plan.record) };
  });

  if (!apply) {
    return { applied: false, moved: 0, moves: preview.moves, file: preview.file, dryRun: true };
  }
  if (!preview.moves.length) {
    return {
      applied: false,
      moved: 0,
      moves: [],
      file: preview.file,
      note: 'nothing to adopt — every same-titled non-live knowledge database is already empty',
    };
  }

  const response = await executeOperation(root, {
    kind: 'adopt-stale-knowledge-rows',
    expected: { [preview.file]: preview.revision },
    prepare: async fresh => {
      // ZEN-D-007: re-planned here, from the authoritative index, BEFORE the
      // change set exists. Any refusal above throws out of `prepare` and the
      // vault stays byte-identical with no journal and no commit.
      const plan = planFor(fresh, keys);
      if (!plan.moves.length) return { changes: [], result: { applied: false, moved: 0, moves: [], file: plan.record.relativePath } };
      return {
        changes: [{ path: plan.record.relativePath, bytes: patchRecord(plan.record, { tables: plan.tables }) }],
        result: { applied: true, moved: plan.moves.length, moves: plan.moves, file: plan.record.relativePath },
      };
    },
  });
  return { ...object(response?.result), commit: response?.commit ?? null };
}

if (path.resolve(process.argv[1] ?? '') === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const at = args.indexOf('--vault');
  const atDatabase = args.indexOf('--database');
  if (at < 0 || !args[at + 1]) {
    console.error('Usage: node scripts/adopt-stale-knowledge-rows.mjs --vault ROOT [--database decisions[,questions,...]] [--apply]');
    process.exitCode = 1;
  } else {
    try {
      const result = await adoptStaleKnowledgeRows(args[at + 1], {
        databases: atDatabase < 0
          ? undefined
          : (args[atDatabase + 1] ?? EMPTY_NARROWING),
        apply: args.includes('--apply'),
      });
      console.log(JSON.stringify(result, null, 2));
    } catch (error) {
      console.error(JSON.stringify({ error: error?.message ?? String(error) }, null, 2));
      process.exitCode = 1;
    }
  }
}

export default adoptStaleKnowledgeRows;
