import assert from 'node:assert/strict';
import { test } from 'vitest';

import {
  allTicketRecords,
  blockedBy,
  blocksOf,
  bodyOf,
  canonicalOf,
  citationsOf,
  clone,
  contractErrorFor,
  definitionOfDoneOf,
  evaluateContract,
  gatesOf,
  gateSelectionOf,
  goalOf,
  guardrailsOf,
  idAndNumber,
  isMappedProject,
  knownProjectKeys,
  labelsOf,
  legacyConfigOf,
  linkedOf,
  mappingError,
  outgoingLinks,
  policyOf,
  priorityRank,
  projectField,
  projectKeyOf,
  projectPolicy,
  queueEntry,
  queueOutOfScope,
  queueReason,
  reviewOf,
  routingOf,
  semanticFlags,
  showPayload,
  sortQueueEntries,
  sourceOf,
  sprintFor,
  sprintPayload,
  ticketFor,
} from '../cli/compat.mjs';

const BODY = `## Goal
Exercise the compat layer.

## Definition of Done
- [ ] The synthetic index is observable
- [ ] No regressions in the helpers

## Gates
- node --version
`;

const PLAIN_BODY = 'A body without any contract sections at all.\n';

const RESOLVED = {
  routing: {
    modelByComplexity: { SIMPLE: 'haiku', MODERATE: 'sonnet', COMPLEX: 'opus' },
    effortByComplexity: { SIMPLE: 'low', MODERATE: 'medium', COMPLEX: 'high' },
    riskEscalation: { LOW: 0, MEDIUM: 1, HIGH: 2 },
  },
  guardrails: { protectedPaths: ['Tools/unit_baselines.json'], maxConsecutiveBlocked: 3, fixForwardAttempts: 2, ticketTimeoutMinutes: { SIMPLE: 15 } },
  projects: {
    FD: {
      repo: 'C:/repo', baseBranch: 'master', branching: 'direct', branchPrefix: 'agent/',
      gates: ['node --version'], conventions: 'Be careful', cleanup: ['wipe scratch'],
      reviewOn: ['HIGH'], workerTools: 'Read', merge: { push: false },
      ticketTimeoutMinutes: 45,
      categories: {
        Foundry: {
          gates: ['pwsh -File gate.ps1'], paths: ['FD/**'], branching: 'branch', branchPrefix: 'wip/',
          conventions: 'Foundry rules', cleanup: ['clean FD'], reviewOn: ['HIGH'],
          workerTools: 'Read,Edit', decisionLog: 'FD/Docs/DecisionLog.md', decisionIdPrefix: 'FD-D',
        },
        Engine: { gates: ['engine gate'], paths: ['Engine/**'] },
      },
    },
  },
};

function makeIndex({ tickets = {}, sprints = {}, tables, vaultConfig } = {}) {
  const index = {
    boardRoots: [{ project: 'FD', path: 'FD' }],
    tickets: new Map(Object.entries(tickets)),
    sprints: new Map(Object.entries(sprints)),
  };
  if (tables) index.tables = tables;
  index.vaultConfig = vaultConfig ?? {
    writes: { enabled: true },
    policy: {
      resolved: RESOLVED,
      board: { agentEmail: 'agent@example.invalid', guardrails: RESOLVED.guardrails },
    },
  };
  return index;
}

function rec(key, fields = {}, body = BODY) {
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
      priority: 'MEDIUM',
      complexity: 'SIMPLE',
      risk: 'LOW',
      ...fields,
    },
    bodyText: body,
  };
}

test('clone, canonicalOf, and sourceOf cover every record shape', () => {
  assert.equal(clone(undefined), undefined);
  assert.deepEqual(clone({ a: 1 }), { a: 1 });
  // structuredClone refuses functions; the JSON fallback drops them.
  assert.deepEqual(clone({ fn: () => 1 }), {});

  assert.deepEqual(canonicalOf(null), {});
  assert.deepEqual(canonicalOf({ fields: { a: 1 } }), { a: 1 });
  assert.deepEqual(canonicalOf({ metadata: { b: 2 } }), { b: 2 });
  assert.deepEqual(canonicalOf('nope'), {});

  const nested = { metadata: { source: { source: { id: 'inner' } } } };
  assert.deepEqual(sourceOf(nested), { id: 'inner' });
  assert.deepEqual(sourceOf({ metadata: { source: { id: 'flat' } } }), { id: 'flat' });
  assert.deepEqual(sourceOf({ metadata: { source: { number: 7 } } }), { number: 7 });
  assert.deepEqual(sourceOf({ fields: { source: { source: { id: 'deep' } } } }), { id: 'deep' });
  assert.deepEqual(sourceOf({ fields: { source: { project_id: 'p' } } }), { project_id: 'p' });
  assert.deepEqual(sourceOf({ source: { id: 'top' } }), { id: 'top' });
  assert.deepEqual(sourceOf({}), {});
});

test('metadataRows reads tables from every index shape', () => {
  const org = [{ id: 'o1', slug: 'acme' }];
  const proj = [{ key: 'FD', organization_id: 'o1' }];
  const tables = { 'public.organizations': org, 'jira.projects': proj };
  const roots = [{ project: 'FD', path: 'FD' }];
  assert.equal(policyOf({ tables, boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ tables: new Map(Object.entries(tables)), boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ metadataFiles: [{ metadata: { tables } }], boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ metadataFiles: [{ fields: { tables } }], boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ metadataFiles: new Map([['m.md', { metadata: { tables } }]]), boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ files: new Map([['m.md', { fields: { tables } }]]), boardRoots: roots }).orgSlug, 'acme');
  assert.equal(policyOf({ boardRoots: roots }).orgSlug, 'pink-goat', 'no tables at all keeps the default slug');
});

function organizationSlugVia(index) {
  return policyOf(index).orgSlug;
}

test('policyOf resolves org slug from organizations joined to projects', () => {
  const tables = {
    'public.organizations': [
      { id: 'org-1', slug: 'acme' },
      { id: 'org-2', slug: 'pink-goat' },
      { id: 'org-3', slug: 'other' },
    ],
    'jira.projects': [{ key: 'FD', organization_id: 'org-1' }],
  };
  assert.equal(organizationSlugVia(makeIndex({ tables })), 'acme');
  // Falls back to the pink-goat row when the project's org is absent.
  const fallbackTables = {
    'public.organizations': [{ id: 'org-9', slug: 'pink-goat' }],
    'jira.projects': [{ key: 'FD', organization_id: 'missing' }],
  };
  assert.equal(organizationSlugVia(makeIndex({ tables: fallbackTables })), 'pink-goat');
  // A joined org with neither slug nor usable match degrades to the default.
  const unmatched = {
    'public.organizations': [{ id: 'org-3', name: 'Unmatched' }],
    'jira.projects': [{ key: 'FD', organization_id: 'org-3' }],
  };
  assert.equal(organizationSlugVia(makeIndex({ tables: unmatched })), 'Unmatched');
  const noJoin = {
    'public.organizations': [{ id: 'org-3', slug: 'x' }],
    'jira.projects': [{ key: 'ZZ', organization_id: 'org-3' }],
  };
  assert.equal(organizationSlugVia(makeIndex({ tables: noJoin })), 'pink-goat');
  // No relational metadata at all: the tiny fallback remains.
  assert.equal(organizationSlugVia(makeIndex({})), 'pink-goat');
});

test('policyOf accepts the pinned-policy shape without a vault wrapper', () => {
  const flat = {
    resolved: RESOLVED,
    client: { file: { project: 'FD', categories: { Client: { gates: ['client gate'] } } } },
    board: { agentEmail: 'board@example.invalid', guardrails: { protectedPaths: [] } },
    orgSlug: 'flat-org',
  };
  const policy = policyOf({ vaultConfig: flat });
  assert.equal(policy.agentEmail, 'board@example.invalid');
  assert.equal(policy.orgSlug, 'flat-org');
  assert.equal(policy.clientFile.project, 'FD');
  assert.deepEqual(policy.categories.Client.gates, ['client gate']);
  assert.deepEqual(legacyConfigOf({ vaultConfig: flat }), RESOLVED);
});

test('legacyConfigOf falls back to the pinned board policy', () => {
  const index = {
    vaultConfig: {
      policy: {
        board: { routing: { modelByComplexity: { MODERATE: 'sonnet' } } },
        guardrails: { protectedPaths: ['x'] },
        projects: { FD: { repo: 'r' } },
      },
    },
  };
  assert.deepEqual(legacyConfigOf(index), {
    routing: { modelByComplexity: { MODERATE: 'sonnet' } },
    guardrails: { protectedPaths: ['x'] },
    projects: { FD: { repo: 'r' } },
  });
});

test('project key and mapping helpers cover key, fallback, and boardRoots', () => {
  const index = makeIndex({});
  assert.equal(projectKeyOf(rec('FD-12')), 'FD');
  assert.equal(projectKeyOf({ metadata: {} }, 'AB-3'), 'AB');
  assert.equal(projectKeyOf({ project: 'zz', metadata: { key: 'weird' } }), 'ZZ');
  assert.equal(projectKeyOf({ metadata: { key: 'nope' } }), null);
  assert.deepEqual(knownProjectKeys(index), ['FD']);
  assert.deepEqual(knownProjectKeys({ boardRoots: [{ project: 'fd' }, {}] }), ['FD']);
  assert.match(mappingError(index, 'ZZ'), /Project ZZ is not mapped.*Known: FD/);
  assert.equal(isMappedProject(index, 'fd'), true);
  assert.equal(isMappedProject(index, 'ZZ'), false);
});

test('projectPolicy overlays the category and merges merge/docs maps', () => {
  const index = makeIndex({});
  const { project, category } = projectPolicy(index, 'FD', 'Foundry');
  assert.equal(category.gates[0], 'pwsh -File gate.ps1');
  assert.equal(project.branching, 'branch', 'the category overlay wins');
  assert.deepEqual(project.merge, { push: false });
  assert.deepEqual(project.docs, {});
  // A category that only exists on policy.categories (client.file level).
  const clientIndex = makeIndex({});
  clientIndex.vaultConfig.policy.resolved = { ...RESOLVED, projects: { FD: { repo: 'r', merge: { push: true } } } };
  clientIndex.vaultConfig.policy.client = { file: { categories: { Client: { gates: ['client gate'], merge: { push: true } } } } };
  const clientPolicy = projectPolicy(clientIndex, 'FD', 'Client');
  assert.deepEqual(clientPolicy.category.gates, ['client gate']);
  assert.deepEqual(clientPolicy.project.merge, { push: true });
  // Unknown category: the base project policy passes through untouched.
  const bare = projectPolicy(index, 'FD', 'Nope');
  assert.deepEqual(bare.category, {});
  assert.equal(bare.project.gates[0], 'node --version');
  assert.equal(projectField(index, 'FD', 'Foundry', 'workerTools'), 'Read,Edit');
  assert.equal(projectField(index, 'FD', 'Foundry', 'missing', 'fallback'), 'fallback');
});

test('ticket, sprint, and record lookup cover every container shape', () => {
  const ticket = rec('FD-1');
  assert.equal(ticketFor({ getTicket: () => ticket }, 'FD-1'), ticket);
  assert.equal(ticketFor({ tickets: new Map([['FD-1', ticket]]) }, 'FD-1'), ticket);
  assert.equal(ticketFor({ issues: new Map([['FD-1', ticket]]) }, 'FD-1'), ticket);
  assert.equal(ticketFor({ tickets: { 'FD-1': ticket } }, 'FD-1'), ticket);
  assert.equal(ticketFor({ issues: { 'FD-1': ticket } }, 'FD-1'), ticket);
  assert.equal(ticketFor({}, 'FD-1'), undefined);

  assert.deepEqual(allTicketRecords({ tickets: new Map([['FD-1', ticket]]) }), [ticket]);
  assert.deepEqual(allTicketRecords({ issues: new Map([['FD-1', ticket]]) }), [ticket]);
  assert.deepEqual(allTicketRecords({ tickets: [ticket] }), [ticket]);
  assert.deepEqual(allTicketRecords({ issues: [ticket] }), [ticket]);
  assert.deepEqual(allTicketRecords({}), []);

  assert.equal(sprintFor(null, 'x'), undefined);
  assert.equal(sprintFor({}, undefined), undefined);
  const sprint = { metadata: { slug: 'fd-s1', status: 'ACTIVE' } };
  assert.equal(sprintFor({ getSprint: () => sprint }, 'fd-s1'), sprint);
  assert.equal(sprintFor({ sprints: new Map([['fd-s1', sprint]]) }, 'fd-s1'), sprint);
  assert.equal(sprintFor({ sprints: { 'fd-s1': sprint } }, 'fd-s1'), sprint);
  assert.equal(sprintFor({ sprints: {} }, 'fd-s1'), undefined);
});

test('sprintPayload scopes by the active stage and the continuous flag', () => {
  assert.equal(sprintPayload(makeIndex({}), null), null);
  assert.equal(sprintPayload(makeIndex({}), 'missing'), null);

  const planned = { metadata: { slug: 'fd-s1', name: 'S1', status: 'PLANNED' } };
  const noStage = makeIndex({ sprints: { 'fd-s1': planned } });
  assert.deepEqual(sprintPayload(noStage, 'fd-s1', 'FD'), {
    name: 'S1', status: 'PLANNED', continuous: false, inScope: false,
  });

  // With an active non-continuous stage, only the ACTIVE sprint is in scope.
  const activeStage = makeIndex({
    sprints: {
      'fd-s1': planned,
      'fd-s2': { project: 'FD', metadata: { slug: 'fd-s2', name: 'S2', status: 'ACTIVE' } },
    },
  });
  assert.equal(sprintPayload(activeStage, 'fd-s1', 'FD').inScope, false);
  assert.equal(sprintPayload(activeStage, 'fd-s2', 'FD').inScope, true);

  // Without an active stage a continuous sprint is in scope.
  const continuous = makeIndex({
    sprints: { 'fd-c': { project: 'FD', metadata: { slug: 'fd-c', status: 'PLANNED', continuous: true } } },
  });
  assert.equal(sprintPayload(continuous, 'fd-c', 'FD').inScope, true);
  assert.equal(sprintPayload(continuous, 'fd-c', 'FD').continuous, true);

  // An explicit inScope on the record wins, and source carries the fallbacks.
  const explicit = makeIndex({
    sprints: { 'fd-x': { metadata: { slug: 'fd-x', inScope: true, source: { status: 'ACTIVE' } } } },
  });
  const payload = sprintPayload(explicit, 'fd-x', 'FD');
  assert.equal(payload.inScope, true);
  assert.equal(payload.status, 'ACTIVE');
  assert.equal(payload.name, 'fd-x', 'slug is the last name fallback');
});

test('labels, semantic flags, and blocks cover every entry shape', () => {
  const ticket = rec('FD-1', {
    labels: ['Needs-Human', 'human-gate', 'needs-gpu', 'deferred'],
    blocks: ['FD-2', 3, { key: 'FD-4', reason: 'r' }, { target: 'FD-5' }, { ticket: 'FD-6', why: 'w' }, { ticketKey: 'FD-7', description: 'd' }, {}],
  });
  assert.deepEqual(labelsOf(ticket), ['Needs-Human', 'human-gate', 'needs-gpu', 'deferred']);
  assert.deepEqual(semanticFlags(ticket), {
    humanGate: true, needsHuman: true, needsGpu: true, deferred: true, approved: false,
  });
  assert.deepEqual(semanticFlags(rec('FD-2')), {
    humanGate: false, needsHuman: false, needsGpu: false, deferred: false, approved: false,
  });
  const blocks = blocksOf(ticket);
  assert.deepEqual(blocks[0], { key: 'FD-2', reason: null });
  assert.deepEqual(blocks[1], { key: '3', reason: null });
  assert.deepEqual(blocks[2], { key: 'FD-4', reason: 'r' });
  assert.equal(blocks[3].key, 'FD-5');
  assert.equal(blocks[4].reason, 'w');
  assert.equal(blocks[5].key, 'FD-7');
  assert.equal(blocks[6].key, null);
});

test('blockedBy sorts by numeric issue number, not by string', () => {
  const index = makeIndex({
    tickets: {
      'FD-2': rec('FD-2', { blocks: [{ key: 'FD-100' }, { key: 'FD-50' }] }),
      'FD-100': rec('FD-100'),
      'FD-50': rec('FD-50'),
      'FD-7': rec('FD-7', { blocks: [{ key: 'FD-2' }] }),
    },
  });
  const blockers = blockedBy(index, 'FD-2');
  assert.deepEqual(blockers.map(item => item.source.metadata.key), ['FD-7']);
  const wide = blockedBy(index, 'FD-100');
  assert.deepEqual(wide.map(item => item.source.metadata.key), ['FD-2']);
  // Self-references are excluded.
  const selfIndex = makeIndex({ tickets: { 'FD-2': rec('FD-2', { blocks: [{ key: 'FD-2' }] }) } });
  assert.deepEqual(blockedBy(selfIndex, 'FD-2'), []);
  assert.deepEqual(blockedBy(index, ''), []);
});

test('outgoingLinks merges blocks and links with type and label defaults', () => {
  const ticket = rec('FD-1', {
    blocks: [{ key: 'FD-2', reason: 'order' }],
    links: ['FD-3', { key: 'FD-4', type: 'CAUSES', label: 'causes' }, { target: 'FD-5' }, { ticketKey: 'FD-6', type: 'DUPLICATES' }, { nokey: true }],
  });
  const links = outgoingLinks(ticket);
  assert.deepEqual(links[0], { key: 'FD-2', reason: 'order', type: 'BLOCKS', label: 'blocks' });
  assert.deepEqual(links[1], { key: 'FD-3', type: 'RELATES_TO', label: 'relates to' });
  assert.deepEqual(links[2], { key: 'FD-4', type: 'CAUSES', label: 'causes' });
  assert.equal(links[3].key, 'FD-5');
  assert.equal(links[4].type, 'DUPLICATES');
  assert.equal(links.length, 5, 'keyless links are filtered out');
});

test('idAndNumber prefers metadata, then source, then the key suffix', () => {
  assert.deepEqual(idAndNumber(rec('FD-12')), { id: 'id-FD-12', number: 12 });
  assert.deepEqual(idAndNumber(rec('FD-12', { number: '34' })), { id: 'id-FD-12', number: 34 });
  assert.deepEqual(idAndNumber({ metadata: { key: 'FD-12' }, fields: {} }), { id: null, number: 12 });
  assert.deepEqual(idAndNumber({ metadata: { key: 'FD-12', source: { number: 'x' } } }), { id: null, number: 'x' });
  assert.deepEqual(idAndNumber({ metadata: { key: 'weird' } }), { id: null, number: null });
});

test('linkedOf reads the issue_links table when it exists', () => {
  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { parent: 'FD-9' }),
      'FD-2': rec('FD-2'),
      'FD-3': rec('FD-3'),
      'FD-9': rec('FD-9', { type: 'EPIC' }),
    },
    tables: {
      'jira.issue_links': [
        { source_issue_id: 'id-FD-2', target_issue_id: 'id-FD-1', type: 'BLOCKS' },
        { source_issue_id: 'id-FD-1', target_issue_id: 'id-FD-3', type: 'RELATES_TO' },
        { source_issue_id: 'id-FD-3', target_issue_id: 'id-FD-1', type: 'BLOCKS' },
        { source_issue_id: 'ghost', target_issue_id: 'id-FD-1', type: 'BLOCKS' },
      ],
    },
  });
  const linked = linkedOf(index, index.tickets.get('FD-1'));
  const byRelation = {};
  for (const row of linked) (byRelation[row.relation] ??= []).push(row.key);
  assert.deepEqual(byRelation.blocks, ['FD-2', 'FD-3']);
  assert.deepEqual(byRelation.linked, ['FD-2', 'FD-3', 'FD-3'], 'every row naming this ticket adds a linked entry');
  assert.deepEqual(byRelation.parent, ['FD-9']);
  assert.equal(linked.every(row => row.key !== 'FD-1'), true, 'self rows are skipped');
});

test('linkedOf falls back to frontmatter relations without a links table', () => {
  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { parent: 'FD-9', links: ['FD-3'], blocks: [{ key: 'FD-2' }] }),
      'FD-2': rec('FD-2', { blocks: [{ key: 'FD-1' }] }),
      'FD-3': rec('FD-3'),
      'FD-9': rec('FD-9', { type: 'EPIC' }),
    },
  });
  const linked = linkedOf(index, index.tickets.get('FD-1'));
  const byRelation = {};
  for (const row of linked) (byRelation[row.relation] ??= []).push(row.key);
  assert.deepEqual(byRelation.linked, ['FD-2', 'FD-3']);
  assert.deepEqual(byRelation.blocks, ['FD-2']);
  assert.deepEqual(byRelation.parent, ['FD-9']);
});

test('body, goal, DoD, and citations prefer canonical metadata', () => {
  assert.equal(bodyOf({ bodyText: 'a' }), 'a');
  assert.equal(bodyOf({ body: 'b' }), 'b');
  assert.equal(bodyOf({}), '');

  assert.equal(goalOf({ metadata: { goal: 'explicit' } }), 'explicit');
  assert.equal(goalOf(rec('FD-1')), 'Exercise the compat layer.');
  assert.equal(goalOf({ metadata: {}, bodyText: PLAIN_BODY }), '');

  const dod = [{ text: 'item', done: false }];
  assert.deepEqual(definitionOfDoneOf({ metadata: { definitionOfDone: dod } }), dod);
  assert.deepEqual(definitionOfDoneOf(rec('FD-1')), [
    { text: 'The synthetic index is observable', done: false },
    { text: 'No regressions in the helpers', done: false },
  ]);

  assert.deepEqual(citationsOf({ metadata: { citations: { a: ['x'] } } }), { a: ['x'] });
  const legacy = citationsOf(rec('FD-1', {}, 'See cli/write.mjs for the writer.'));
  assert.ok(legacy && Object.keys(legacy).length > 0, 'legacy citations come from the body');
});

test('routing, gates, and review resolve through the vendored helpers', () => {
  const index = makeIndex({});
  const ticket = rec('FD-1');
  assert.deepEqual(routingOf(index, ticket, 'FD', 'Foundry'), { model: 'haiku', effort: 'low' });

  const base = gatesOf(index, ticket, 'FD', 'Foundry');
  assert.equal(base.gatesSource, 'ticket', 'the ticket body carries a Gates section');
  assert.deepEqual(base.gates, ['- node --version'], 'the body gate line keeps its bullet');

  const plain = rec('FD-2', {}, PLAIN_BODY);
  assert.equal(gatesOf(index, plain, 'FD', 'Foundry').gatesSource, 'category');
  assert.deepEqual(gatesOf(index, plain, 'FD', 'Foundry').gates, ['pwsh -File gate.ps1']);

  const selection = gateSelectionOf(index, plain, 'FD', 'Foundry', ['Engine/anything.mjs']);
  assert.equal(selection.gatesSource, 'category');
  assert.ok(selection.unioned.some(row => row.category === 'Engine'), 'changed paths union the Engine gates');
  assert.ok(selection.gates.includes('engine gate'));
  const emptySelection = gateSelectionOf(index, plain, 'FD', 'Foundry', []);
  assert.deepEqual(emptySelection.unioned, []);

  const risky = rec('FD-5', { risk: 'HIGH' }, PLAIN_BODY);
  const review = reviewOf(index, risky, 'FD', 'Foundry');
  assert.equal(review.required, true, 'HIGH risk is in reviewOn');
  assert.ok(review.reasons.some(reason => /risk HIGH is in reviewOn/.test(reason)));

  const trigger = rec('FD-3', { risk: 'LOW' }, '## Goal\nG\n\n## Definition of Done\n- [ ] No regressions anywhere\n');
  const triggerReview = reviewOf(index, trigger, 'FD', 'Foundry');
  assert.equal(triggerReview.required, true, 'a DoD absence assertion demands review');
  assert.ok(triggerReview.reasons.some(reason => /ABSENCE/.test(reason)));

  assert.deepEqual(guardrailsOf(index, ticket), {
    protectedPaths: ['Tools/unit_baselines.json'],
    ticketTimeoutMinutes: 45,
    fixForwardAttempts: 2,
    maxConsecutiveBlocked: 3,
  });
  const plainGuardrails = guardrailsOf(index, plain);
  assert.equal(plainGuardrails.ticketTimeoutMinutes, 45, 'the project timeout wins over the complexity map');
});

test('routing and gates fall back when the project is not mapped', () => {
  const unmapped = makeIndex({});
  unmapped.vaultConfig.policy.resolved = { projects: {} };
  const ticket = rec('FD-1');

  assert.deepEqual(routingOf(unmapped, ticket, 'FD', 'Foundry'), { model: 'sonnet', effort: 'medium' });
  const gates = gatesOf(unmapped, ticket, 'FD', 'Foundry');
  assert.equal(gates.gatesSource, 'project');
  assert.deepEqual(gates.gates, []);
  const selection = gateSelectionOf(unmapped, ticket, 'FD', 'Foundry', ['Engine/x']);
  assert.deepEqual(selection, { gatesSource: 'project', gates: [], base: [], unioned: [] });
  const review = reviewOf(unmapped, ticket, 'FD', 'Foundry');
  assert.equal(review.required, true, 'the DoD absence assertion still demands review');
  assert.ok(review.reasons.some(reason => /ABSENCE/.test(reason)));
});

test('contract evaluation covers epic, blocked, malformed, and unmapped outcomes', () => {
  const index = makeIndex({
    tickets: {
      'FD-0': rec('FD-0'),
      'FD-1': rec('FD-1'),
      'FD-2': rec('FD-2', { status: 'In Progress' }),
      'FD-9': rec('FD-9', { type: 'EPIC' }),
    },
  });
  index.tickets.get('FD-2').metadata.blocks = [{ key: 'FD-1', reason: 'order' }];

  assert.equal(evaluateContract(index, index.tickets.get('FD-0'), 'FD', 'Foundry').valid, true);

  const blocked = evaluateContract(index, index.tickets.get('FD-1'), 'FD', 'Foundry');
  assert.equal(blocked.valid, false);
  assert.match(blocked.error, /blocked by FD-2 \(In Progress\)/);

  const epic = evaluateContract(index, index.tickets.get('FD-9'), 'FD', 'Foundry');
  assert.equal(epic.valid, false);
  assert.match(epic.error, /EPIC/);

  const blocker = evaluateContract(index, index.tickets.get('FD-2'), 'FD', 'Foundry');
  assert.equal(blocker.valid, true, 'blocking another ticket does not invalidate your own contract');

  const malformed = evaluateContract(index, rec('FD-4', {}, PLAIN_BODY), 'FD', 'Foundry');
  assert.equal(malformed.valid, false);
  assert.match(malformed.error, /no `## Goal` section/i);

  const unmapped = makeIndex({});
  unmapped.vaultConfig.policy.resolved = { projects: {} };
  const fallback = evaluateContract(unmapped, rec('FD-1'), 'FD', 'Foundry');
  assert.equal(fallback.valid, false);
  assert.match(fallback.error, /not mapped/);
  assert.equal(
    evaluateContract(unmapped, rec('FD-1'), 'FD', 'Foundry', { invalidRepository: 'bad repo' }).error,
    'bad repo',
  );
  assert.equal(
    contractErrorFor(unmapped, rec('FD-1'), 'FD', 'Foundry', { invalidRepository: 'bad repo' }),
    'bad repo',
  );
});

test('queue reasons cover every refusal the queue can report', () => {
  const index = makeIndex({
    tickets: {
      'FD-0': rec('FD-0'),
      'FD-1': rec('FD-1'),
      'FD-2': rec('FD-2', { status: 'In Progress', statusCategory: 'IN_PROGRESS' }),
      'FD-3': rec('FD-3', { assignee: 'human@example.test' }),
      'FD-4': rec('FD-4', { labels: ['deferred'] }),
      'FD-5': rec('FD-5', { labels: ['human-gate'] }),
      'FD-6': rec('FD-6', { sprint: 'fd-s1' }),
      'FD-7': rec('FD-7', { type: 'EPIC' }),
      'FD-8': rec('FD-8', {}, PLAIN_BODY),
    },
    sprints: { 'fd-s1': { metadata: { slug: 'fd-s1', name: 'S1', status: 'PLANNED' } } },
  });
  index.tickets.get('FD-2').metadata.blocks = [{ key: 'FD-1', reason: 'order' }];

  assert.equal(queueReason(index, index.tickets.get('FD-0'), 'FD', 'Foundry'), null);
  assert.match(queueReason(index, index.tickets.get('FD-1'), 'FD', 'Foundry'), /blocked by FD-2 \(In Progress\)/);
  assert.match(queueReason(index, index.tickets.get('FD-2'), 'FD', 'Foundry'), /it is in In Progress, and the queue reads To Do/);
  assert.match(queueReason(index, index.tickets.get('FD-3'), 'FD', 'Foundry'), /assigned to human@example\.test/);
  assert.match(queueReason(index, index.tickets.get('FD-4'), 'FD', 'Foundry'), /`deferred`/);
  assert.match(queueReason(index, index.tickets.get('FD-5'), 'FD', 'Foundry'), /human-gate/);
  assert.match(queueReason(index, index.tickets.get('FD-6'), 'FD', 'Foundry'), /in S1 \(PLANNED\), not the active sprint/);
  assert.match(queueReason(index, index.tickets.get('FD-7'), 'FD', 'Foundry'), /EPIC/);
  assert.match(queueReason(index, index.tickets.get('FD-8'), 'FD', 'Foundry'), /no `## Goal` section/i);
  assert.equal(queueReason(index, rec('FD-9'), 'FD', 'Foundry', { invalidRepository: 'bad' }), 'bad');

  // A ticket in a status other than To Do is out of the queue's read.
  assert.match(
    queueReason(index, rec('FD-10', { status: 'In Progress', statusCategory: 'IN_PROGRESS' }), 'FD', 'Foundry'),
    /it is in In Progress, and the queue reads To Do/,
  );
});

test('queue scope and entries expose the structural predicates', () => {
  const activeStage = makeIndex({
    tickets: { 'FD-1': rec('FD-1'), 'FD-2': rec('FD-2', { sprint: 'fd-s2' }) },
    sprints: { 'fd-s2': { project: 'FD', metadata: { slug: 'fd-s2', name: 'S2', status: 'ACTIVE' } } },
  });
  assert.equal(queueOutOfScope(activeStage, activeStage.tickets.get('FD-1')), true, 'no sprint during an active stage is out of scope');
  assert.equal(queueOutOfScope(activeStage, activeStage.tickets.get('FD-2')), false);

  const quiet = makeIndex({ tickets: { 'FD-1': rec('FD-1') } });
  assert.equal(queueOutOfScope(quiet, quiet.tickets.get('FD-1')), false, 'no sprint and no stage is in scope');
  const planned = makeIndex({
    tickets: { 'FD-1': rec('FD-1', { sprint: 'fd-s1' }) },
    sprints: { 'fd-s1': { metadata: { slug: 'fd-s1', status: 'PLANNED' } } },
  });
  assert.equal(queueOutOfScope(planned, planned.tickets.get('FD-1')), true);

  const index = makeIndex({
    tickets: {
      'FD-1': rec('FD-1', { priority: 'HIGH' }),
      'FD-2': rec('FD-2', { type: 'EPIC', priority: 'CRITICAL' }),
      'FD-3': rec('FD-3', { assignee: 'a@b.c', priority: 'HIGH' }),
      'FD-4': rec('FD-4', { labels: ['needs-human', 'needs-gpu'], priority: 'LOW' }),
      'FD-5': rec('FD-5', { blocks: [{ key: 'FD-1', reason: 'r' }], priority: 'MEDIUM' }),
    },
  });
  const entries = Object.fromEntries(
    ['FD-1', 'FD-2', 'FD-3', 'FD-4', 'FD-5'].map(key => [key, queueEntry(index, index.tickets.get(key))]),
  );
  assert.deepEqual(entries['FD-1'].blockedBy, ['FD-5']);
  assert.equal(entries['FD-1'].claimable, false, 'being blocked suppresses claiming');
  assert.equal(entries['FD-2'].isEpic, true);
  assert.equal(entries['FD-2'].claimable, false);
  assert.equal(entries['FD-3'].assigned, true);
  assert.equal(entries['FD-3'].claimable, false);
  assert.deepEqual(entries['FD-4'].blockedBy, []);
  assert.equal(entries['FD-4'].needsHuman, true);
  assert.equal(entries['FD-4'].needsGpu, true);
  assert.equal(entries['FD-4'].claimable, false, 'needs-human suppresses claiming');
  assert.deepEqual(entries['FD-5'].blockedBy, [], 'blockedBy is incoming, so a blocker has none');
  assert.equal(entries['FD-5'].claimable, true);
  assert.equal(entries['FD-1'].sprint, null);

  const sorted = sortQueueEntries(Object.values(entries), index);
  assert.deepEqual(sorted.map(row => row.key), ['FD-2', 'FD-1', 'FD-3', 'FD-5', 'FD-4']);
  assert.equal(priorityRank('critical'), 0);
  assert.equal(priorityRank('bogus'), 99);
  assert.equal(priorityRank(undefined), 99);
});

test('showPayload assembles the full targeted-check projection', () => {
  const index = makeIndex({ tickets: { 'FD-1': rec('FD-1') } });
  const payload = showPayload(index, index.tickets.get('FD-1'), 'FD');
  assert.equal(payload.key, 'FD-1');
  assert.equal(payload.contractValid, true);
  assert.equal(payload.gatesSource, 'ticket');
  assert.deepEqual(payload.gates, ['- node --version']);
  assert.equal(payload.branching, 'branch', 'the Foundry category branches');
  assert.equal(payload.branch, 'wip/FD-1', 'the category branch prefix wins');
  assert.equal(payload.decisionLog, 'FD/Docs/DecisionLog.md');
  assert.equal(payload.decisionIdPrefix, 'FD-D');
  assert.equal(payload.conventions, 'Be careful\n\nFoundry rules');
  assert.deepEqual(payload.cleanup, ['clean FD']);
  assert.equal(payload.review.required, true);
  assert.equal(payload.repo, 'C:/repo');
  assert.equal(payload.baseBranch, 'master');
  assert.equal(payload.push, false);
  assert.equal(payload.orgSlug, 'pink-goat');
  assert.match(payload.issueUrl, /localhost:3001\/pink-goat\/project\/FD\/issue\/1/);
  assert.equal(payload.goal, 'Exercise the compat layer.');
  assert.equal(payload.definitionOfDone.length, 2);
  assert.equal(payload.status, 'To Do');
  assert.deepEqual(payload.guardrails.protectedPaths, ['Tools/unit_baselines.json']);

  // A direct-branching project without category routing takes the project
  // defaults, and the branch field stays null.
  const direct = makeIndex({ tickets: { 'FD-1': rec('FD-1', { category: null }) } });
  const directPayload = showPayload(direct, direct.tickets.get('FD-1'), 'FD');
  assert.equal(directPayload.branching, 'direct');
  assert.equal(directPayload.branch, null);
  assert.equal(directPayload.gatesSource, 'project');
  assert.equal(directPayload.category, null);
});
