import { test } from 'vitest';
import assert from 'node:assert/strict';

import {
  dispatchLinks,
  dispatchQuery,
  dispatchRows,
  dispatchSuggestions,
  dispatchSprint,
  dispatchVersion,
} from '../cli/queries.mjs';

function rec(key, fields = {}, body = 'Body.') {
  return {
    project: 'FD',
    relativePath: `FD/tickets/${key}.md`,
    metadata: {
      key,
      id: `id-${key}`,
      type: 'TASK',
      title: `Title ${key}`,
      status: 'To Do',
      statusCategory: 'TODO',
      assignee: null,
      category: 'Foundry',
      parent: null,
      sprint: null,
      labels: [],
      blocks: [],
      links: [],
      position: Number(key.split('-').at(-1)),
      storyPoints: 3,
      ...fields,
    },
    bodyText: body,
  };
}

function sprintRecord(slug, fields = {}, source = {}) {
  return {
    project: 'FD',
    relativePath: `FD/sprints/${slug}.md`,
    metadata: { slug, name: slug, status: 'PLANNED', ...fields, ...(Object.keys(source).length ? { source } : {}) },
  };
}

function makeIndex({ tickets = {}, sprints = {}, tables = {}, configs, vaultConfig } = {}) {
  const index = {
    boardRoots: [{ project: 'FD', path: 'FD' }],
    tickets: new Map(tickets instanceof Map ? tickets : Object.entries(tickets)),
    sprints: new Map(sprints instanceof Map ? sprints : Object.entries(sprints)),
    tables: new Map(tables instanceof Map ? tables : Object.entries(tables)),
  };
  if (configs) index.configs = configs;
  index.vaultConfig = vaultConfig ?? { policy: { orgSlug: 'acme', client: { file: { project: 'FD' } } } };
  return index;
}

const KNOWLEDGE_TABLES = {
  'notion.databases': [{ id: 'db-s', title: 'Agent · Suggestions', project_id: 'p1' }],
  'notion.projects': [{ id: 'p1', organization_id: 'o1' }],
  'public.organizations': [{ id: 'o1', slug: 'acme' }],
  'notion.database_properties': [
    { id: 'prop-status', database_id: 'db-s', name: 'Status', type: 'SELECT', position: 1, options: [{ id: 'new', label: 'new' }, { id: 'done', label: 'done' }] },
    { id: 'prop-note', database_id: 'db-s', name: 'Note', type: 'TEXT', position: 2 },
  ],
  'notion.database_rows': [
    { id: 'r1', database_id: 'db-s', values: { 'prop-status': 'new', 'prop-note': 'hello' }, created_at: '2026-01-02T00:00:00Z' },
    { id: 'r2', database_id: 'db-s', values: { 'prop-status': 'orphan-option' }, created_at: '2026-01-01T00:00:00Z' },
  ],
};

test('links refuses missing, malformed, and unknown keys, and reads case-insensitively', () => {
  const index = makeIndex({ tickets: { 'FD-1': rec('FD-1') } });
  assert.deepEqual(dispatchLinks(index, ['links']).payload, { error: 'Usage: zagent links <TICKET_KEY>' });
  assert.equal(dispatchLinks(index, ['links']).exitCode, 1);
  assert.match(dispatchLinks(index, ['links', 'nope']).payload.error, /is not a ticket key/);
  assert.equal(dispatchLinks(index, ['links', 'FD-999']).exitCode, 3);
  // The requested key is matched case-insensitively through the record scan.
  const links = dispatchLinks(index, ['links', 'fd-1']);
  assert.equal(links.exitCode, 0);
  assert.deepEqual(links.payload, []);
});

test('links covers label variants, canonical edges without source rows, and ordering', () => {
  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { blocks: [{ key: 'FD-2', reason: 'order' }], links: [{ key: 'FD-3', type: 'CAUSES' }, { key: 'FD-4', type: 'DUPLICATES' }, { key: 'FD-999' }] }),
      'FD-2': rec('FD-2'),
      'FD-3': rec('FD-3'),
      'FD-4': rec('FD-4'),
      'FD-5': rec('FD-5', { blocks: [{ key: 'FD-1', reason: 'back' }] }),
    },
    tables: {
      'jira.issue_links': [
        { id: 'l1', source_issue_id: 'id-FD-1', target_issue_id: 'id-FD-2', type: 'BLOCKS' },
        { id: 'l2', source_issue_id: 'id-FD-1', target_issue_id: 'id-FD-3', type: 'CAUSES' },
      ],
    },
  });
  const links = dispatchLinks(index, ['links', 'FD-1']).payload;
  const outgoing = links.filter(row => row.direction === 'outgoing');
  const incoming = links.filter(row => row.direction === 'incoming');
  assert.deepEqual(outgoing.map(row => [row.key, row.label, row.id]), [
    ['FD-2', 'blocks', 'l1'],
    ['FD-3', 'causes', 'l2'],
    ['FD-4', 'duplicates', null],
  ]);
  assert.equal(outgoing[0].reason, 'order');
  assert.deepEqual(incoming.map(row => [row.key, row.label]), [['FD-5', 'is blocked by']]);
  // Unknown link targets are skipped silently.
  assert.equal(links.some(row => row.key === 'FD-999'), false);

  // Inverse labels for the remaining types.
  const caused = dispatchLinks(index, ['links', 'FD-3']).payload.find(row => row.direction === 'incoming');
  assert.equal(caused.label, 'is caused by');
  const duplicated = dispatchLinks(index, ['links', 'FD-4']).payload.find(row => row.direction === 'incoming');
  assert.equal(duplicated.label, 'is duplicated by');

  // An index-level ordering map reorders the union by legacy link id.
  const ordered = makeIndex({
    tickets: index.tickets,
    tables: index.tables,
  });
  ordered.ordering = { links: { 'id-FD-1': ['l2', 'l1'] } };
  const reordered = dispatchLinks(ordered, ['links', 'FD-1']).payload;
  // The canonical edge with no source row has a null id and sorts last.
  assert.deepEqual(reordered.filter(row => row.direction === 'outgoing').map(row => row.id), ['l2', 'l1', null]);

  // A metadata record can carry the ordering instead of the index.
  const viaRecord = makeIndex({ tickets: index.tickets, tables: index.tables });
  viaRecord.metadataFiles = new Map([['m.md', { metadata: { ordering: { links: { 'id-FD-1': ['l2', 'l1'] } } } }]]);
  assert.deepEqual(
    dispatchLinks(viaRecord, ['links', 'FD-1']).payload.filter(row => row.direction === 'outgoing').map(row => row.id),
    ['l2', 'l1', null],
  );
});

test('sprint list covers project resolution, refusals, gates, and member joins', () => {
  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { sprint: 'fd-s1', storyPoints: 5 }),
      'FD-2': rec('FD-2', { sprint: 'fd-s1', status: 'Done', statusCategory: 'DONE', storyPoints: 'abc' }),
      'FD-3': rec('FD-3', { sprint: 'fd-s1', type: 'EPIC' }),
      'FD-4': rec('FD-4', { sprint: 'fd-s1' }),
    },
    sprints: {
      'fd-s1': sprintRecord('fd-s1', { goal: 'Ship it', startDate: '2026-01-01T05:06:07.888Z', gate: 'FD-2' }, { id: 'sprint-1' }),
      'fd-other': { project: 'AB', metadata: { slug: 'fd-other', name: 'Other' } },
    },
  });
  const list = dispatchSprint(index, ['sprint', 'list', '--project=FD']);
  assert.equal(list.exitCode, 0);
  assert.equal(list.payload.length, 1, 'sprints owned by other projects are filtered out');
  const row = list.payload[0];
  assert.equal(row.id, 'sprint-1');
  assert.equal(row.name, 'fd-s1');
  assert.equal(row.goal, 'Ship it');
  assert.equal(row.startDate, '2026-01-01 05:06:07.888Z', 'the legacy timestamp shape is restored');
  assert.equal(row.issues, 3, 'epic containers are excluded from membership');
  assert.equal(row.points, 8, 'non-numeric points count as zero');
  assert.equal(row.donePoints, 0, 'a Done ticket with unparseable points adds nothing');
  assert.deepEqual(row.gate, { key: 'FD-2', title: 'Title FD-2', status: 'Done', unmet: false });

  // A gate recorded only in the source snapshot resolves through the id map.
  const sourceGate = makeIndex({
    tickets: { 'FD-1': rec('FD-1', { sprint: 'fd-s1' }) },
    sprints: { 'fd-s1': sprintRecord('fd-s1', {}, { id: 'sprint-1', gate_issue_id: 'id-FD-1' }) },
  });
  assert.deepEqual(dispatchSprint(sourceGate, ['sprint', 'list']).payload[0].gate, {
    key: 'FD-1', title: 'Title FD-1', status: 'To Do', unmet: true,
  });
  // A gate that names no ticket is null.
  const missingGate = makeIndex({
    tickets: { 'FD-1': rec('FD-1', { sprint: 'fd-s1' }) },
    sprints: { 'fd-s1': sprintRecord('fd-s1', { gate: 'FD-999' }) },
  });
  assert.equal(dispatchSprint(missingGate, ['sprint', 'list']).payload[0].gate, null);

  // Membership falls back to the source sprint id when no slug exists.
  const bySourceId = makeIndex({
    tickets: { 'FD-1': rec('FD-1', { source: { id: 'id-FD-1', sprint_id: 'sprint-9' } }) },
    sprints: { s9: sprintRecord('fd-s1', {}, { id: 'sprint-9' }) },
  });
  delete bySourceId.sprints.get('s9').metadata.slug;
  assert.equal(dispatchSprint(bySourceId, ['sprint', 'list']).payload[0].issues, 1);

  // Refusals and empty shapes.
  assert.deepEqual(dispatchSprint(makeIndex({ vaultConfig: {} }), ['sprint', 'list']).payload, []);
  assert.equal(dispatchSprint(index, ['sprint', 'list', '--project', 'ZZ']).exitCode, 1);
  assert.match(dispatchSprint(index, ['sprint', 'list', '--project', 'ZZ']).payload.error, /No Jira project with key ZZ/);
  // A project known only from the jira.projects table passes the existence check.
  const tabled = makeIndex({ tables: { 'jira.projects': [{ id: 'p1', key: 'AB' }] } });
  assert.deepEqual(dispatchSprint(tabled, ['sprint', 'list', '--project', 'AB']).payload, []);
  // sprints held in a plain array are still read.
  const arraySprints = makeIndex({ tickets: { 'FD-1': rec('FD-1', { sprint: 'fd-s1' }) } });
  arraySprints.sprints = [sprintRecord('fd-s1')];
  assert.equal(dispatchSprint(arraySprints, ['sprint', 'list']).payload.length, 1);
});

test('version list covers catalog sources, project joins, and refusals', () => {
  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { fixVersions: ['1.0'], storyPoints: 2 }),
      'FD-2': rec('FD-2', { fixVersions: ['1.0'], status: 'Done', statusCategory: 'DONE' }),
      // projectKeyOf reads the record-level project first, so the AB ticket
      // must carry it there rather than only in metadata.
      'AB-1': { ...rec('AB-1', { fixVersions: ['1.0'] }), project: 'AB' },
    },
    tables: {
      'jira.versions': [
        { id: 'v1', name: '1.0', description: 'First', status: 'UNRELEASED', release_date: '2026-02-03T04:05:06Z', project: 'FD', position: 2 },
        { id: 'v2', name: '0.9', project_id: 'p-ab' },
      ],
      'jira.projects': [{ id: 'p-ab', key: 'ab' }],
    },
  });
  const list = dispatchVersion(index, ['version', 'list', '--project', 'FD']);
  assert.equal(list.exitCode, 0);
  assert.deepEqual(list.payload.map(row => row.name), ['1.0'], 'other projects and unjoined rows are filtered out');
  const row = list.payload[0];
  assert.equal(row.description, 'First');
  assert.equal(row.releaseDate, '2026-02-03 04:05:06Z');
  assert.equal(row.issues, 2);
  assert.equal(row.done, 1);
  // FD-1 carries 2 points, FD-2 (the Done one) carries the rec() default of 3:
  // points = 2 + 3 = 5, donePoints = 3. AB-1's 3 points are excluded — it is
  // not an FD member.
  assert.equal(row.points, 5);
  assert.equal(row.donePoints, 3);

  // A versions catalog exposed through index.configs wins over the table.
  const catalog = makeIndex({
    tickets: { 'FD-1': rec('FD-1', { fixVersions: ['2.0'] }) },
    configs: new Map([['versions.md', { fields: { kind: 'versions', rows: [{ id: 'v2', name: '2.0', project: 'FD' }] } }]]),
  });
  const catalogList = dispatchVersion(catalog, ['version', 'list']);
  assert.deepEqual(catalogList.payload.map(row => row.name), ['2.0']);

  assert.equal(dispatchVersion(index, ['version', 'list', '--project', 'ZZ']).exitCode, 1);
  assert.deepEqual(dispatchVersion(makeIndex({ vaultConfig: {} }), ['version', 'list']).payload, []);
  const empty = makeIndex({});
  assert.deepEqual(dispatchVersion(empty, ['version', 'list']).payload, []);
});

test('rows covers database selection, filters, limits, and malformed input', () => {
  const index = makeIndex({ tables: KNOWLEDGE_TABLES });

  const all = dispatchRows(index, ['rows', 'suggestions']);
  assert.equal(all.exitCode, 0);
  assert.deepEqual(all.payload.map(row => row.id), ['r1', 'r2'], 'rows sort newest first');
  assert.equal(all.payload[0].values.Status, 'new', 'SELECT ids resolve to labels');
  assert.equal(all.payload[0].values.Note, 'hello');
  assert.equal(all.payload[1].values.Status, 'orphan-option', 'an orphaned SELECT id is preserved');
  assert.match(all.payload[0].url, /localhost:3004\/acme\/database\/db-s/);

  // Property names are matched exactly; values compare case-insensitively.
  const filtered = dispatchRows(index, ['rows', 'suggestions', '--where', 'Status=NEW', '--limit', '1']);
  assert.deepEqual(filtered.payload.map(row => row.id), ['r1'], 'where matches are case-insensitive and limit applies');

  // An empty database is an error; an unknown database is a usage error.
  // dispatchRows throws; dispatchQuery is the wrapper that turns that into a
  // usage result.
  assert.match(dispatchQuery(index, ['rows', 'bogus']).payload.error, /Usage: zagent rows/);
  const bare = makeIndex({});
  assert.match(dispatchQuery(bare, ['rows', 'suggestions']).payload.error, /No Notion database called/);

  // Malformed where clauses and unknown properties are usage errors.
  assert.match(dispatchQuery(index, ['rows', 'suggestions', '--where', 'x']).payload.error, /Property=value pairs/);
  assert.match(dispatchQuery(index, ['rows', 'suggestions', '--where', '=x']).payload.error, /Property=value pairs/);
  assert.match(dispatchQuery(index, ['rows', 'suggestions', '--where', 'Nope=1']).payload.error, /no property named "Nope"/);

  // A bad --limit is a usage error through the shared dispatcher.
  assert.match(dispatchQuery(index, ['rows', 'suggestions', '--limit', 'abc']).payload.error, /must be a whole number/);
  assert.match(dispatchQuery(index, ['rows', 'suggestions', '--limit=-2']).payload.error, /must be a whole number/);
  const limited = dispatchQuery(index, ['rows', 'suggestions', '--limit=1']);
  assert.equal(limited.exitCode, 0);
  assert.equal(limited.payload.length, 1);

  // The preferred database is the one whose org matches the policy slug;
  // otherwise a populated database wins; otherwise the first row.
  const multi = makeIndex({
    tables: {
      ...KNOWLEDGE_TABLES,
      'notion.databases': [
        { id: 'db-empty', title: 'Agent · Suggestions', project_id: 'p-other' },
        { id: 'db-s', title: 'Agent · Suggestions', project_id: 'p1' },
      ],
      'notion.projects': [
        { id: 'p1', organization_id: 'o1' },
        { id: 'p-other', organization_id: 'o2' },
      ],
      'public.organizations': [{ id: 'o1', slug: 'acme' }, { id: 'o2', slug: 'other' }],
    },
  });
  assert.deepEqual(dispatchRows(multi, ['rows', 'suggestions']).payload.map(row => row.id), ['r1', 'r2']);
  const unpopulated = makeIndex({
    tables: { ...KNOWLEDGE_TABLES, 'notion.database_rows': [] },
  });
  assert.deepEqual(dispatchRows(unpopulated, ['rows', 'suggestions']).payload, []);
});

test('suggestions is rows suggestions with the Status=new default', () => {
  const index = makeIndex({ tables: KNOWLEDGE_TABLES });
  const fresh = dispatchSuggestions(index, ['suggestions']);
  assert.deepEqual(fresh.payload.map(row => row.id), ['r1']);
  const everything = dispatchSuggestions(index, ['suggestions', '--all']);
  assert.deepEqual(everything.payload.map(row => row.id), ['r1', 'r2']);
  const limited = dispatchSuggestions(index, ['suggestions', '--all', '--limit', '1']);
  assert.equal(limited.payload.length, 1);
});

test('dispatchQuery routes families to their owners and wraps errors', () => {
  const index = makeIndex({ tickets: { 'FD-1': rec('FD-1') }, tables: KNOWLEDGE_TABLES });
  assert.equal(dispatchQuery(index, ['sprint', 'start', 'x']), null);
  assert.equal(dispatchQuery(index, ['version', 'set', '1.0', 'FD-1']), null);
  assert.equal(dispatchQuery(index, ['rows', 'suggestions', 'resolve', 'r1', 'done']), null);
  assert.equal(dispatchQuery(index, ['suggestions', 'resolve', 'r1', 'done']), null);
  assert.equal(dispatchQuery(index, ['blocked', '--project', 'FD']), null);
  assert.equal(dispatchQuery(index, ['transitions']), null);
  assert.equal(dispatchQuery(index, ['history']), null);
  assert.equal(dispatchQuery(index, ['epic', 'FD-1']), null);
  assert.equal(dispatchQuery(index, ['report', 'epic']), null);
  assert.equal(dispatchQuery(index, ['nope']), null);
  assert.equal(dispatchQuery(index, ['sprint']).exitCode, 0);
  assert.equal(dispatchQuery(index, ['version']).exitCode, 0);
  assert.equal(dispatchQuery(index, ['links', 'FD-1']).exitCode, 0);
  // Errors from the nested dispatchers come back as usage results.
  const bad = dispatchQuery(index, ['rows', 'bogus']);
  assert.equal(bad.exitCode, 1);
  assert.match(bad.payload.error, /Usage: zagent rows/);
});
