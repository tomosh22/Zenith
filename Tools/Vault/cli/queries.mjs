/*
 * Auxiliary read queries for the exported vault.
 *
 * These functions are intentionally read-only.  The exporter keeps the
 * canonical fields on ticket/sprint files and the source-preserving SQL rows
 * in metadata.md.  A query therefore joins the two representations in
 * memory, using source ids for relations and canonical values for display.
 */

import {
  EXIT_CODES,
  allTicketRecords,
  blockedBy,
  canonicalOf,
  clone,
  idAndNumber,
  outgoingLinks,
  policyOf,
  projectKeyOf,
  sourceOf,
  ticketFor,
} from './compat.mjs';

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;
const DATABASE_KEYS = ['decisions', 'questions', 'shortfalls', 'changelog', 'suggestions'];

const LINK_LABELS = Object.freeze({
  BLOCKS: 'blocks',
  RELATES_TO: 'relates to',
  DUPLICATES: 'duplicates',
  CAUSES: 'causes',
});

const LINK_INVERSE_LABELS = Object.freeze({
  BLOCKS: 'is blocked by',
  RELATES_TO: 'relates to',
  DUPLICATES: 'is duplicated by',
  CAUSES: 'is caused by',
});

const DATABASE_TITLES = Object.freeze({
  decisions: 'Agent · Decisions',
  questions: 'Agent · Questions',
  shortfalls: 'Agent · Shortfalls',
  changelog: 'Agent · Changelog',
  suggestions: 'Agent · Suggestions',
});

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value : [];
}

function result(exitCode, payload) {
  return { exitCode, payload: clone(payload) };
}

function option(argv, name) {
  const args = Array.isArray(argv) ? argv : [];
  const at = args.indexOf(name);
  if (at >= 0 && at + 1 < args.length) return args[at + 1];
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return prefix === undefined ? undefined : prefix.slice(name.length + 1);
}

function hasFlag(argv, name) {
  return Array.isArray(argv) && argv.some(value => value === name || (typeof value === 'string' && value.startsWith(`${name}=`)));
}

function numberOption(argv, name, defaultValue) {
  const raw = option(argv, name);
  if (raw === undefined) return defaultValue;
  const value = Number(raw);
  if (!Number.isInteger(value) || value < 0) {
    throw new Error(`${name} must be a whole number — got "${raw}".`);
  }
  return value;
}

function requestedKey(argv) {
  if (!Array.isArray(argv)) return undefined;
  return argv.find(value => ISSUE_KEY.test(String(value ?? '')));
}

function keyOf(ticket) {
  const metadata = canonicalOf(ticket);
  return String(metadata.key ?? ticket?.key ?? '');
}

function ticketByRequested(index, requested) {
  const text = String(requested ?? '');
  const direct = ticketFor(index, text) ?? ticketFor(index, text.toUpperCase());
  if (direct) return direct;
  const upper = text.toUpperCase();
  return allTicketRecords(index).find(ticket => keyOf(ticket).toUpperCase() === upper);
}

function tablesOf(index) {
  const out = new Map();
  const add = (name, rows) => {
    if (name === undefined || name === null || !Array.isArray(rows)) return;
    // The direct index map is already the scanner's latest view.  Metadata is
    // still consulted for archivedTables and hand-authored indexes.
    if (!out.has(String(name))) out.set(String(name), rows);
  };
  if (index?.tables instanceof Map) {
    for (const [name, rows] of index.tables) add(name, rows);
  } else if (index?.tables && typeof index.tables === 'object') {
    for (const [name, rows] of Object.entries(index.tables)) add(name, rows);
  }
  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  if (Array.isArray(index?.metadataFiles)) records.push(...index.metadataFiles);
  if (Array.isArray(index?.files)) records.push(...index.files);
  for (const record of records) {
    const fields = record?.fields ?? record?.metadata ?? record;
    for (const [name, rows] of Object.entries(fields?.tables ?? {})) add(name, rows);
    for (const [name, rows] of Object.entries(fields?.archivedTables ?? {})) add(name, rows);
  }
  // Config catalog files (versions.md, labels.md, workflow.md) are exposed
  // as `index.tables` by VaultIndex, but include them for small custom indexes.
  if (index?.configs instanceof Map) {
    for (const record of index.configs.values()) {
      const fields = record?.fields ?? record?.metadata ?? record;
      if (fields?.kind && Array.isArray(fields.rows)) add(fields.kind, fields.rows);
    }
  }
  return out;
}

function rowsFor(index, name) {
  return array(tablesOf(index).get(name));
}

function namedRows(index, name) {
  const out = [];
  const seen = new Set();
  const add = row => {
    if (!row || typeof row !== 'object') return;
    const identity = row.id === undefined ? JSON.stringify(row) : String(row.id);
    if (seen.has(identity)) return;
    seen.add(identity);
    out.push(row);
  };
  for (const row of rowsFor(index, name)) add(row);
  if (index?.configs instanceof Map) {
    for (const record of index.configs.values()) {
      const fields = record?.fields ?? record?.metadata ?? record;
      if (String(fields?.kind ?? '').toLowerCase() === String(name).toLowerCase()) {
        for (const row of array(fields.rows)) add(row);
      }
    }
  }
  return out;
}

function sourceIdentity(ticket) {
  const source = sourceOf(ticket);
  const id = idAndNumber(ticket).id ?? source.id;
  return id === undefined || id === null ? null : String(id);
}

function sourceIdMap(index) {
  const out = new Map();
  for (const ticket of allTicketRecords(index)) {
    const id = sourceIdentity(ticket);
    if (id) out.set(id, ticket);
  }
  return out;
}

function projectRows(index) {
  const out = new Map();
  for (const row of rowsFor(index, 'jira.projects')) {
    if (row?.id !== undefined) out.set(String(row.id), row);
  }
  return out;
}

function projectKeyFromId(index, id) {
  const row = projectRows(index).get(String(id ?? ''));
  const key = row?.key ?? row?.project_key ?? row?.projectKey;
  return key === undefined || key === null ? null : String(key).toUpperCase();
}

function projectExists(index, key) {
  const wanted = String(key ?? '').toUpperCase();
  if (rowsFor(index, 'jira.projects').some(row => String(row?.key ?? '').toUpperCase() === wanted)) return true;
  return array(index?.boardRoots).some(root => String(root?.project ?? '').toUpperCase() === wanted);
}

function projectError(index, key) {
  void index;
  return { exitCode: EXIT_CODES.CONFIGURATION, payload: { error: `No Jira project with key ${key}.` } };
}

function projectOfTicket(ticket) {
  const metadata = canonicalOf(ticket);
  return projectKeyOf(ticket, metadata.key);
}

function isDone(ticket) {
  const metadata = canonicalOf(ticket);
  const category = String(metadata.statusCategory ?? '').toUpperCase();
  if (category) return category === 'DONE';
  const status = String(metadata.status ?? '').toUpperCase();
  return status === 'DONE';
}

function statusCategoryOf(ticket) {
  const metadata = canonicalOf(ticket);
  if (metadata.statusCategory !== undefined && metadata.statusCategory !== null) {
    return String(metadata.statusCategory).toUpperCase();
  }
  return isDone(ticket) ? 'DONE' : (String(metadata.status ?? '').toUpperCase() === 'TO DO' ? 'TODO' : 'IN_PROGRESS');
}

function normalizeDate(value) {
  if (value === undefined || value === null || value === '') return null;
  const parsed = new Date(value);
  if (Number.isNaN(parsed.getTime())) return value;
  return parsed.toISOString();
}

function numeric(value) {
  const resultValue = Number(value);
  return Number.isFinite(resultValue) ? resultValue : 0;
}

function sortNumber(value) {
  const number = Number(value);
  return Number.isFinite(number) ? number : Number.MAX_SAFE_INTEGER;
}

function linkRowPayload(row, direction, target, relation = row) {
  const metadata = canonicalOf(target);
  const type = String(relation?.type ?? row?.type ?? 'RELATES_TO');
  const labels = direction === 'outgoing' ? LINK_LABELS : LINK_INVERSE_LABELS;
  return {
    id: row?.id ?? null,
    type,
    direction,
    key: metadata.key ?? target?.key ?? null,
    title: metadata.title ?? null,
    status: metadata.status ?? null,
    statusCategory: metadata.statusCategory ?? null,
    // Canonical link metadata is authoritative.  The source row is retained
    // only to carry the stable legacy id/order when the edge existed at
    // export time.
    reason: relation?.reason ?? null,
    label: labels[type] ?? type,
  };
}

function linkOrdering(index, issueId) {
  const candidates = [];
  if (index?.ordering && typeof index.ordering === 'object') candidates.push(index.ordering);
  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  for (const record of records) {
    const fields = record?.fields ?? record?.metadata ?? record;
    if (fields?.ordering && typeof fields.ordering === 'object') candidates.push(fields.ordering);
  }
  for (const candidate of candidates) {
    const links = candidate.links;
    const ids = Array.isArray(links) ? links : links && typeof links === 'object' ? links[String(issueId)] : undefined;
    if (Array.isArray(ids)) return new Map(ids.map((id, rank) => [String(id), rank]));
  }
  return null;
}

/** `links KEY`: outgoing links are followed by incoming links, as in SQL UNION ALL. */
export function dispatchLinks(index, argv = []) {
  const requested = requestedKey(argv) ?? argv[1];
  if (requested === undefined || requested === null || requested === '') {
    return result(EXIT_CODES.USAGE, { error: 'Usage: zagent links <TICKET_KEY>' });
  }
  if (!ISSUE_KEY.test(String(requested))) {
    return result(EXIT_CODES.USAGE, { error: `"${requested}" is not a ticket key (expected something like ZM-14).` });
  }
  const ticket = ticketByRequested(index, requested);
  if (!ticket) return result(3, { error: `No ticket ${requested}.` });
  const id = sourceIdentity(ticket);
  const rows = rowsFor(index, 'jira.issue_links');
  const rowByEdge = new Map();
  for (const row of rows) {
    const source = String(row?.source_issue_id ?? row?.sourceIssueId ?? '');
    const target = String(row?.target_issue_id ?? row?.targetIssueId ?? '');
    const type = String(row?.type ?? 'RELATES_TO').toUpperCase();
    const edge = `${source}\u0000${target}\u0000${type}`;
    if (!rowByEdge.has(edge)) rowByEdge.set(edge, row);
  }
  const outgoing = [];
  const incoming = [];
  const canonicalOutgoing = outgoingLinks(ticket);
  for (const relation of canonicalOutgoing) {
    const target = ticketByRequested(index, relation?.key);
    if (!target) continue;
    const targetId = sourceIdentity(target);
    const type = String(relation?.type ?? 'RELATES_TO').toUpperCase();
    const sourceRow = rowByEdge.get(`${id ?? ''}\u0000${targetId ?? ''}\u0000${type}`);
    outgoing.push(linkRowPayload(sourceRow, 'outgoing', target, { ...relation, type }));
  }
  // Incoming edges are derived from every source ticket's canonical
  // declaration.  This makes a frontmatter removal/addition/reason edit
  // visible immediately, even though metadata.md still contains the frozen
  // source relation table.
  const currentKey = keyOf(ticket);
  for (const sourceTicket of allTicketRecords(index)) {
    if (sourceTicket === ticket) continue;
    const sourceId = sourceIdentity(sourceTicket);
    for (const relation of outgoingLinks(sourceTicket)) {
      if (String(relation?.key ?? '').toUpperCase() !== currentKey.toUpperCase()) continue;
      const type = String(relation?.type ?? 'RELATES_TO').toUpperCase();
      const sourceRow = rowByEdge.get(`${sourceId ?? ''}\u0000${id ?? ''}\u0000${type}`);
      incoming.push(linkRowPayload(sourceRow, 'incoming', sourceTicket, { ...relation, type }));
    }
  }
  // A hand-authored vault may have canonical links but no source table.  The
  // exporter always has the table, so no source row is required for a new
  // canonical edge (its payload id remains null).
  const output = [...outgoing, ...incoming];
  const ordering = linkOrdering(index, id);
  if (ordering) {
    output.forEach((row, position) => { row._sourcePosition = position; });
    output.sort((a, b) => (ordering.get(String(a.id)) ?? Number.MAX_SAFE_INTEGER)
      - (ordering.get(String(b.id)) ?? Number.MAX_SAFE_INTEGER)
      || a._sourcePosition - b._sourcePosition);
    for (const row of output) delete row._sourcePosition;
  }
  return result(EXIT_CODES.OK, output);
}

function sprintSource(record) {
  const metadata = canonicalOf(record);
  return object(metadata.source?.source ?? metadata.source ?? sourceOf(record));
}

function sprintProject(record, index) {
  const source = sprintSource(record);
  return String(record?.project ?? projectKeyFromId(index, source.project_id) ?? '').toUpperCase() || null;
}

function sprintMembers(index, record) {
  const metadata = canonicalOf(record);
  const source = sprintSource(record);
  const sprintId = source.id ?? metadata.id;
  const slug = metadata.slug ?? metadata.key;
  return allTicketRecords(index).filter(ticket => {
    const ticketSource = sourceOf(ticket);
    // Canonical frontmatter is authoritative after export.  Keep the source
    // id read above only for gate/catalog joins; a user editing `sprint:` must
    // immediately change every aggregate query.
    if (slug !== undefined && slug !== null && slug !== '') {
      return String(canonicalOf(ticket).sprint ?? '') === String(slug);
    }
    return sprintId !== undefined && sprintId !== null
      && String(ticketSource.sprint_id ?? '') === String(sprintId);
  });
}

function sqlTimestamp(value) {
  if (value === undefined || value === null || value === '') return null;
  // PostgreSQL timestamp columns are returned by the old CLI as a text value
  // with a space separator and the original fractional precision.  The
  // exporter stores ISO text, so restore the legacy representation here.
  return String(value).replace(/^([0-9]{4}-[0-9]{2}-[0-9]{2})T/, '$1 ');
}

function gateForSprint(index, record) {
  const canonical = canonicalOf(record);
  const source = sprintSource(record);
  const gateId = source.gate_issue_id ?? source.gateIssueId;
  const ticket = Object.hasOwn(canonical,'gate')
    ? ticketByRequested(index,canonical.gate?.key??canonical.gate)
    : sourceIdMap(index).get(String(gateId??''));
  if (!ticket) return null;
  const metadata = canonicalOf(ticket);
  return {
    key: metadata.key ?? null,
    title: metadata.title ?? null,
    status: metadata.status ?? null,
    unmet: !isDone(ticket),
  };
}

/** `sprint list`: exact legacy aggregate, with epic containers excluded. */
export function dispatchSprint(index, argv = []) {
  const wantedProject = option(argv, '--project') ?? policyOf(index).clientFile.project;
  if (wantedProject === undefined || wantedProject === null || wantedProject === '') return result(EXIT_CODES.OK, []);
  const project = String(wantedProject).toUpperCase();
  if (!projectExists(index, project)) return projectError(index, project);
  const records = index?.sprints instanceof Map
    ? [...index.sprints.values()]
    : array(index?.sprints);
  const values = [];
  for (const record of records) {
    const source = sprintSource(record);
    const owner = sprintProject(record, index);
    if (project && owner !== project) continue;
    const metadata = canonicalOf(record);
    const members = sprintMembers(index, record).filter(ticket => String(canonicalOf(ticket).type ?? '').toUpperCase() !== 'EPIC');
    const points = members.reduce((sum, ticket) => sum + numeric(canonicalOf(ticket).storyPoints), 0);
    const donePoints = members.filter(isDone).reduce((sum, ticket) => sum + numeric(canonicalOf(ticket).storyPoints), 0);
    values.push({
      id: source.id ?? metadata.id ?? null,
      name: metadata.name ?? metadata.title ?? source.name ?? null,
      goal: Object.hasOwn(metadata,'goal') ? metadata.goal : source.goal ?? null,
      status: metadata.status ?? source.status ?? null,
      startDate: sqlTimestamp(Object.hasOwn(metadata,'startDate') ? metadata.startDate : source.start_date ?? source.startDate),
      endDate: sqlTimestamp(Object.hasOwn(metadata,'endDate') ? metadata.endDate : source.end_date ?? source.endDate),
      issues: members.length,
      points,
      donePoints,
      gate: gateForSprint(index, record),
      _position: sortNumber(metadata.position ?? source.position),
    });
  }
  values.sort((a, b) => a._position - b._position || String(a.name ?? '').localeCompare(String(b.name ?? '')));
  return result(EXIT_CODES.OK, values.map(({ _position, ...value }) => value));
}

function versionRows(index) {
  const direct = namedRows(index, 'versions');
  if (direct.length) return direct;
  return rowsFor(index, 'jira.versions');
}

function versionProject(row, index) {
  return String(row?.project ?? row?.project_key ?? row?.projectKey ?? projectKeyFromId(index, row?.project_id)).toUpperCase() || null;
}

/** `version list`: source catalog rows joined through FIX memberships. */
export function dispatchVersion(index, argv = []) {
  const wantedProject = option(argv, '--project') ?? policyOf(index).clientFile.project;
  if (wantedProject === undefined || wantedProject === null || wantedProject === '') return result(EXIT_CODES.OK, []);
  const project = String(wantedProject).toUpperCase();
  if (!projectExists(index, project)) return projectError(index, project);
  const values = [];
  for (const row of versionRows(index)) {
    const owner = versionProject(row, index);
    if (project && owner !== project) continue;
    // `fixVersions` is canonical frontmatter.  The source issue_versions
    // table remains an audit trail and catalog join aid, but must not make a
    // removed edge live or hide a newly authored one.
    const members = allTicketRecords(index).filter(ticket => {
      if (owner && projectOfTicket(ticket) !== owner) return false;
      return array(canonicalOf(ticket).fixVersions)
        .some(name => String(name).toLowerCase() === String(row?.name ?? '').toLowerCase());
    });
    const done = members.filter(isDone);
    values.push({
      id: row?.id ?? null,
      name: row?.name ?? null,
      description: row?.description ?? null,
      status: row?.status ?? null,
      releaseDate: sqlTimestamp(row?.release_date ?? row?.releaseDate),
      issues: members.length,
      done: done.length,
      points: members.reduce((sum, ticket) => sum + numeric(canonicalOf(ticket).storyPoints), 0),
      donePoints: done.reduce((sum, ticket) => sum + numeric(canonicalOf(ticket).storyPoints), 0),
      _position: sortNumber(row?.position),
    });
  }
  values.sort((a, b) => a._position - b._position || String(a.name ?? '').localeCompare(String(b.name ?? '')));
  return result(EXIT_CODES.OK, values.map(({ _position, ...value }) => value));
}

function orgSlug(index) {
  const root = object(index?.vaultConfig ?? index?.config);
  const policy = object(root.policy);
  return String(policy.orgSlug ?? root.orgSlug ?? 'pink-goat');
}

function databaseModel(index, key) {
  const wanted = String(key ?? '').toLowerCase();
  if (!DATABASE_KEYS.includes(wanted)) {
    throw new Error(`Usage: zagent rows <${DATABASE_KEYS.join('|')}> — "${key ?? ''}" is not one of them.`);
  }
  const title = DATABASE_TITLES[wanted];
  const databases = rowsFor(index, 'notion.databases').filter(row => String(row?.title ?? '') === title);
  const projects = new Map(rowsFor(index, 'notion.projects').map(row => [String(row?.id ?? ''), row]));
  const organizations = new Map(rowsFor(index, 'public.organizations').map(row => [String(row?.id ?? ''), row]));
  const wantedOrg = orgSlug(index).toLowerCase();
  const preferred = databases.find(row => {
    const project = projects.get(String(row?.project_id ?? ''));
    const organization = organizations.get(String(project?.organization_id ?? ''));
    return String(organization?.slug ?? '').toLowerCase() === wantedOrg;
  });
  const populated = databases.find(row => rowsFor(index, 'notion.database_rows').some(item => String(item?.database_id ?? '') === String(row?.id ?? '')));
  const database = preferred ?? populated ?? databases[0];
  if (!database) throw new Error(`No Notion database called "${title}" is present in the vault.`);
  const properties = rowsFor(index, 'notion.database_properties')
    .filter(row => String(row?.database_id ?? '') === String(database.id))
    .sort((a, b) => sortNumber(a?.position) - sortNumber(b?.position) || String(a?.name ?? '').localeCompare(String(b?.name ?? '')));
  return { key: wanted, database, properties };
}

function readKnowledgeCell(property, raw) {
  if (raw === undefined || raw === null || raw === '') return null;
  if (String(property?.type ?? '').toUpperCase() !== 'SELECT') return String(raw);
  for (const optionValue of array(property?.options)) {
    if (String(optionValue?.id ?? '') === String(raw)) return optionValue?.label ?? String(raw);
  }
  // Preserve an orphaned SELECT id.  Showing it is more useful than making a
  // deleted option appear as an empty cell.
  return String(raw);
}

function parseWhere(raw) {
  if (raw === undefined) return {};
  const output = {};
  for (const part of String(raw).split(',')) {
    const clause = part.trim();
    if (!clause) continue;
    const at = clause.indexOf('=');
    if (at <= 0) throw new Error(`--where takes Property=value pairs — got "${clause}".`);
    output[clause.slice(0, at).trim()] = clause.slice(at + 1).trim();
  }
  return output;
}

/** `rows DATABASE`: expose property names and resolve SELECT ids to labels. */
export function dispatchRows(index, argv = []) {
  const model = databaseModel(index, argv[1]);
  const limit = numberOption(argv, '--limit', undefined);
  const where = parseWhere(option(argv, '--where'));
  const names = new Map(model.properties.map(property => [String(property.name), property]));
  for (const name of Object.keys(where)) {
    if (!names.has(name)) {
      throw new Error(`"${model.database.title}" has no property named "${name}". Known: ${model.properties.map(property => property.name).join(', ')}.`);
    }
  }
  const rows = rowsFor(index, 'notion.database_rows')
    .filter(row => String(row?.database_id ?? '') === String(model.database.id))
    .slice()
    .sort((a, b) => String(b?.created_at ?? b?.createdAt ?? '').localeCompare(String(a?.created_at ?? a?.createdAt ?? '')));
  const url = `http://localhost:3004/${orgSlug(index)}/database/${model.database.id}`;
  const output = [];
  for (const row of rows) {
    const rawValues = object(row?.values);
    const values = {};
    for (const property of model.properties) values[property.name] = readKnowledgeCell(property, rawValues[property.id]);
    const matches = Object.entries(where).every(([name, wanted]) => String(values[name] ?? '').toLowerCase() === String(wanted).toLowerCase());
    if (!matches) continue;
    output.push({ id: row.id, url, values });
    if (limit !== undefined && output.length >= limit) break;
  }
  return result(EXIT_CODES.OK, output);
}

/** `suggestions` is the legacy shorthand for rows suggestions --where Status=new. */
export function dispatchSuggestions(index, argv = []) {
  const args = ['rows', 'suggestions'];
  if (!hasFlag(argv, '--all')) args.push('--where', 'Status=new');
  const limit = option(argv, '--limit');
  if (limit !== undefined) args.push('--limit', String(limit));
  return dispatchRows(index, args);
}

/**
 * Root read.js owns the core commands.  It calls this dispatcher before its
 * switch and keeps its own handlers for transitions/history/epic/reports.
 * Returning null for those commands avoids two incompatible implementations.
 */
export function dispatchQuery(index, argv = [], files = {}) {
  void files;
  const command = String(argv?.[0] ?? '').toLowerCase();
  try {
    switch (command) {
      case 'links': return dispatchLinks(index, argv);
      case 'sprint':
        return String(argv?.[1] ?? '').toLowerCase() === 'list' || argv?.[1] === undefined
          ? dispatchSprint(index, argv)
          : null;
      case 'version':
        return String(argv?.[1] ?? '').toLowerCase() === 'list' || argv?.[1] === undefined
          ? dispatchVersion(index, argv)
          : null;
      case 'rows':
        return String(argv?.[2] ?? '').toLowerCase() === 'resolve' ? null : dispatchRows(index, argv);
      case 'suggestions':
        return String(argv?.[1] ?? '').toLowerCase() === 'resolve' ? null : dispatchSuggestions(index, argv);
      // These command families have one owner elsewhere in the CLI.
      case 'blocked':
      case 'transitions':
      case 'history':
      case 'epic':
      case 'report':
        return null;
      default:
        return null;
    }
  } catch (error) {
    return result(EXIT_CODES.USAGE, { error: error?.message ?? String(error) });
  }
}

export {
  DATABASE_KEYS,
  LINK_INVERSE_LABELS,
  LINK_LABELS,
  databaseModel,
  namedRows,
  rowsFor,
  tablesOf,
};

export default dispatchQuery;
