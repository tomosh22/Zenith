import { afterEach, beforeEach, expect, test } from 'vitest';
import { cp, mkdtemp, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, readRevision } from '../server/operations.mjs';
import { dispatchProjectAction } from '../server/project-actions.mjs';
import { allTicketRecords, canonicalOf } from '../cli/compat.mjs';
import { expectedRevision } from '../cli/write.mjs';

let root;

const ZM_ACTIVE_SPRINT = 'zm-g59n0dtrphhiop35dtrj6kcg';
const ZM_PLANNED_SPRINT = 'zm-c7qk1t85ot6nx7twweae48vm';
const ZM_SPRINT_9 = 'zm-sf7gsnc6ngv6c0khqvicm4ot';
const ZM_VERSIONS_PATH = 'ZM/versions.md';

async function revisions(...paths) {
  return Object.fromEntries(await Promise.all(paths.map(async relative => [relative, await readRevision(root, relative)])));
}

function ticketRevisions(index, tickets) {
  return Object.fromEntries(tickets.map(ticket => [ticket.relativePath, expectedRevision(ticket)]));
}

function unfinishedSprintMembers(index, project, slug) {
  return allTicketRecords(index).filter(ticket => String(ticket.project).toUpperCase() === project
    && canonicalOf(ticket).sprint === slug && canonicalOf(ticket).status !== 'Done');
}

async function setupFixture() {
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: true, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  await writeFile(path.join(root, '.gitignore'), '.zagent/\n');
  await git(root, ['init', '-q']);
  await git(root, ['config', 'user.name', 'Project refusal test']);
  await git(root, ['config', 'user.email', 'project-refusals@example.invalid']);
  await git(root, ['add', '.']);
  await git(root, ['commit', '-qm', 'Fixture']);
}

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), 'vault-project-refusals-'));
  await cp(fileURLToPath(new URL('../fixtures/snapshot/vault/', import.meta.url)), root, { recursive: true });
  await setupFixture();
});

afterEach(async () => {
  if (root) await rm(root, { recursive: true, force: true });
  root = null;
});

const rejectsWith = (promise, code) => expect(promise).rejects.toMatchObject({ code });

test('request validation refuses unknown inputs, bad revisions and disabled writes', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchProjectAction(index, null), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', extra: 1 }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.summon' }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'bad key!' }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', data: ['x'] }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: ['x'] }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { '../escape.md': null } }), 'PATH');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { 'C:/escape.md': null } }), 'PATH');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { 'ZEN/tickets/ZEN-7.md': { hash: 5 } } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { 'ZEN/tickets/ZEN-7.md': { hash: 'x', size: -1 } } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { 'ZEN/tickets/ZEN-7.md': { hash: 'x', mtimeMs: 'nope' } } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-7', revisions: { 'ZEN/tickets/ZEN-7.md': { hash: 'x', size: 1.5 } } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-7',
    revisions: { 'ZEN/tickets/ZEN-7.md': { hash: 'x' } },
    operationId: 'bad id!',
  }), 'ID');
  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZZZ', key: 'ZZZ-1' }), 'NOT_FOUND');
});

test('disabled writes refuse every project action', async () => {
  const configPath = path.join(root, 'vault.config.json');
  const config = JSON.parse(await readFile(configPath, 'utf8'));
  config.writes = { enabled: false, commit: false };
  await writeFile(configPath, JSON.stringify(config));
  const index = await loadVault(root);
  const issue = index.getTicket('ZEN-7');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-7',
    data: { title: 'X' },
    revisions: { [issue.relativePath]: expectedRevision(issue) },
  }), 'READ_ONLY');
});

test('issue.create validates the issue payload before allocating a key', async () => {
  const index = await loadVault(root);
  const base = { action: 'issue.create', project: 'ZEN' };

  await rejectsWith(dispatchProjectAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', type: 'WIDGET' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', priority: 'URGENT' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', complexity: 'EXTREME' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', risk: 'UNKNOWN' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', severity: 'VERY_BAD' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', repro: 'MAYBE' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', storyPoints: -1 } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', storyPoints: 1.5 } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', status: 'Sideways' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', parent: 'ZEN-9999' } }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', type: 'EPIC', parent: 'ZEN-7' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', sprint: 'ghost-sprint' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', fixVersions: ['Ghost version'] } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', customFieldValues: { 'ghost-field': 'x' } } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: 'X', labels: ['   '] } }), 'INVALID');

  const created = await dispatchProjectAction(index, {
    ...base,
    data: { title: 'Created by refusals test', type: 'TASK', labels: ['fresh', 'fresh'], status: 'To Do' },
    operationId: 'issue-create-refusals',
  });
  expect(created.result).toMatchObject({ key: 'ZEN-23', created: true, status: 'To Do' });
  const after = await loadVault(root);
  expect(after.getTicket('ZEN-23').metadata.title).toBe('Created by refusals test');
  expect(after.getTicket('ZEN-23').metadata.labels).toEqual(['fresh']);
});

test('issue.update validates the key, the workflow graph and field values', async () => {
  const index = await loadVault(root);
  const issue = index.getTicket('ZEN-7');
  const base = {
    action: 'issue.update',
    project: 'ZEN',
    key: 'ZEN-7',
    revisions: { [issue.relativePath]: expectedRevision(issue) },
  };

  await rejectsWith(dispatchProjectAction(index, { action: 'issue.update', project: 'ZEN', key: 'ZEN-9999', data: {} }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { status: 'Sideways' } }), 'INVALID');
  // To Do → Done is legal through the workflow's global from-null "Done"
  // transition, but To Do → In Review has no edge at all.
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { status: 'In Review' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { storyPoints: -3 } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { title: '' } }), 'INVALID');

  const updated = await dispatchProjectAction(index, {
    ...base,
    data: { priority: 'LOW', storyPoints: 3 },
    operationId: 'issue-update-refusals',
  });
  expect(updated.result).toMatchObject({ key: 'ZEN-7', updated: true });
  const after = await loadVault(root);
  expect(after.getTicket('ZEN-7').metadata.priority).toBe('LOW');
  expect(after.getTicket('ZEN-7').metadata.storyPoints).toBe(3);
});

test('sprint.start refuses unknown, active and blocked starts, then starts a planned sprint', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: 'ghost' },
  }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: ZM_ACTIVE_SPRINT },
    revisions: await revisions(`ZM/sprints/${ZM_ACTIVE_SPRINT}.md`),
  }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'ZM',
    data: { id: ZM_PLANNED_SPRINT },
    revisions: await revisions(`ZM/sprints/${ZM_PLANNED_SPRINT}.md`),
  }), 'INVALID');

  // WEB's sprint is already ACTIVE, so the happy path starts HE's planned
  // sprint — HE has no active sprint to block it.
  const HE_PLANNED_SPRINT = 'he-u2exlj3764t6lnl5ehdwdwea';
  const started = await dispatchProjectAction(index, {
    action: 'sprint.start',
    project: 'HE',
    data: { id: HE_PLANNED_SPRINT, startDate: '2026-09-01T00:00:00.000Z' },
    revisions: await revisions(`HE/sprints/${HE_PLANNED_SPRINT}.md`),
    operationId: 'sprint-start-he',
  });
  expect(started.result).toMatchObject({ status: 'ACTIVE', startDate: '2026-09-01T00:00:00.000Z' });
  const after = await loadVault(root);
  const sprint = [...after.sprints.values()].find(record => record.relativePath === `HE/sprints/${HE_PLANNED_SPRINT}.md`);
  expect(sprint.fields.status).toBe('ACTIVE');
});

test('sprint.complete validates destination and status, then completes with a move', async () => {
  const index = await loadVault(root);
  const sprintPath = `ZM/sprints/${ZM_ACTIVE_SPRINT}.md`;
  const members = unfinishedSprintMembers(index, 'ZM', ZM_ACTIVE_SPRINT);
  expect(members.length).toBeGreaterThan(0);
  const base = {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_ACTIVE_SPRINT },
    revisions: {
      ...(await revisions(sprintPath)),
      ...ticketRevisions(index, members),
    },
  };

  await rejectsWith(dispatchProjectAction(index, { ...base, data: { id: 'ghost' } }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { id: ZM_ACTIVE_SPRINT, destinationSprintId: 42 } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { id: ZM_ACTIVE_SPRINT, destinationSprintId: 'ghost' },
  }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, {
    ...base,
    data: { id: ZM_ACTIVE_SPRINT, destinationSprintId: ZM_ACTIVE_SPRINT },
  }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.complete',
    project: 'ZM',
    data: { id: ZM_PLANNED_SPRINT },
    revisions: await revisions(`ZM/sprints/${ZM_PLANNED_SPRINT}.md`),
  }), 'INVALID');

  const completed = await dispatchProjectAction(index, {
    ...base,
    data: { id: ZM_ACTIVE_SPRINT, destinationSprintId: ZM_SPRINT_9 },
    operationId: 'sprint-complete-zm',
  });
  expect(completed.result).toMatchObject({ id: ZM_ACTIVE_SPRINT, status: 'COMPLETED', movedToSprint: ZM_SPRINT_9 });
  const after = await loadVault(root);
  expect(members.every(ticket => after.getTicket(canonicalOf(ticket).key).metadata.sprint === ZM_SPRINT_9)).toBe(true);
});

test('sprint.update validates dates and applies the patch', async () => {
  const index = await loadVault(root);
  const sprintPath = `ZM/sprints/${ZM_PLANNED_SPRINT}.md`;
  const base = {
    action: 'sprint.update',
    project: 'ZM',
    data: { id: ZM_PLANNED_SPRINT },
    revisions: await revisions(sprintPath),
  };

  // data must be merged into base.data — a spread that replaces `data`
  // drops the sprint id and the request fails validation instead.
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { ...base.data, id: 'ghost' } }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { ...base.data, startDate: 'not-a-date' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { ...base.data, startDate: '2026-10-01T00:00:00.000Z', endDate: '2026-09-01T00:00:00.000Z' } }), 'INVALID');

  const updated = await dispatchProjectAction(index, {
    ...base,
    data: { ...base.data, name: 'Sprint 7 renamed', goal: 'Finish the skeleton', startDate: '2026-09-01T00:00:00.000Z', endDate: '2026-09-30T00:00:00.000Z' },
    operationId: 'sprint-update-zm',
  });
  expect(updated.result).toMatchObject({ id: ZM_PLANNED_SPRINT, name: 'Sprint 7 renamed', updated: true });
  const after = await loadVault(root);
  const sprint = [...after.sprints.values()].find(record => record.relativePath === sprintPath);
  expect(sprint.fields.goal).toBe('Finish the skeleton');
});

test('sprint.delete refuses active sprints and removes a planned one', async () => {
  const index = await loadVault(root);

  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.delete',
    project: 'ZM',
    data: { id: 'ghost' },
  }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, {
    action: 'sprint.delete',
    project: 'ZM',
    data: { id: ZM_ACTIVE_SPRINT },
    revisions: await revisions(`ZM/sprints/${ZM_ACTIVE_SPRINT}.md`),
  }), 'INVALID');

  const deleted = await dispatchProjectAction(index, {
    action: 'sprint.delete',
    project: 'ZM',
    data: { id: ZM_PLANNED_SPRINT },
    revisions: await revisions(`ZM/sprints/${ZM_PLANNED_SPRINT}.md`),
    operationId: 'sprint-delete-zm',
  });
  expect(deleted.result).toMatchObject({ id: ZM_PLANNED_SPRINT, deleted: true });
  const after = await loadVault(root);
  expect([...after.sprints.values()].some(record => record.relativePath === `ZM/sprints/${ZM_PLANNED_SPRINT}.md`)).toBe(false);
});

test('sprint.reorder validates the order list and applies positions', async () => {
  const index = await loadVault(root);
  const slugs = [...index.sprints.values()]
    .filter(record => String(record.project).toUpperCase() === 'ZM')
    .map(record => record.fields.slug);
  expect(slugs).toHaveLength(7);
  const base = { action: 'sprint.reorder', project: 'ZM' };

  await rejectsWith(dispatchProjectAction(index, { ...base, data: { order: [] } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { order: [42] } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { order: slugs.slice(0, 3) }, revisions: await revisions(...slugs.map(slug => `ZM/sprints/${slug}.md`)) }), 'INVALID');

  const reordered = await dispatchProjectAction(index, {
    ...base,
    data: { order: [...slugs].reverse() },
    revisions: await revisions(...slugs.map(slug => `ZM/sprints/${slug}.md`)),
    operationId: 'sprint-reorder-zm',
  });
  expect(reordered.result).toMatchObject({ order: [...slugs].reverse(), updated: true });
});

test('version.update, archive and delete validate the target and apply to the catalog', async () => {
  let index = await loadVault(root);
  // Every ZM version is referenced by tickets as a fixVersion, and renaming
  // or deleting a referenced version fails proposed-vault validation. Strip
  // the "Ship Candidate" references first so the catalog paths can run.
  const referenced = ['ZM-13', 'ZM-43', 'ZM-44', 'ZM-45', 'ZM-46']
    .map(key => index.getTicket(key))
    .filter(Boolean);
  expect(referenced.length).toBeGreaterThan(0);
  for (const ticket of referenced) {
    await dispatchProjectAction(index, {
      action: 'issue.update',
      project: 'ZM',
      key: String(canonicalOf(ticket).key),
      data: { fixVersions: [] },
      revisions: { [ticket.relativePath]: expectedRevision(ticket) },
      operationId: 'strip-fixversions-' + canonicalOf(ticket).key,
    });
  }
  index = await loadVault(root);

  const base = { action: 'version.update', project: 'ZM', data: { id: 'Ship Candidate' }, revisions: await revisions(ZM_VERSIONS_PATH) };

  await rejectsWith(dispatchProjectAction(index, { ...base, data: { id: 'ghost' } }), 'NOT_FOUND');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: {} }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { name: '' } }), 'INVALID');

  const updated = await dispatchProjectAction(index, {
    ...base,
    data: { id: 'Ship Candidate', name: 'Ship Candidate v2', description: 'Updated', releaseDate: '2026-12-01' },
    operationId: 'version-update-zm',
  });
  expect(updated.result).toMatchObject({ id: 'Ship Candidate', name: 'Ship Candidate v2', updated: true });
  let after = await loadVault(root);
  expect(after.configs.get(ZM_VERSIONS_PATH).fields.rows.find(row => row.name === 'Ship Candidate v2').release_date).toBe('2026-12-01');

  await rejectsWith(dispatchProjectAction(after, {
    action: 'version.archive',
    project: 'ZM',
    data: { id: 'ghost' },
    revisions: await revisions(ZM_VERSIONS_PATH),
  }), 'NOT_FOUND');
  const archived = await dispatchProjectAction(after, {
    action: 'version.archive',
    project: 'ZM',
    data: { id: 'Ship Candidate v2' },
    revisions: await revisions(ZM_VERSIONS_PATH),
    operationId: 'version-archive-zm',
  });
  expect(archived.result).toMatchObject({ id: 'Ship Candidate v2', status: 'ARCHIVED', updated: true });
  after = await loadVault(root);
  expect(after.configs.get(ZM_VERSIONS_PATH).fields.rows.find(row => row.name === 'Ship Candidate v2').status).toBe('ARCHIVED');

  await rejectsWith(dispatchProjectAction(after, {
    action: 'version.delete',
    project: 'ZM',
    data: { id: 'ghost' },
    revisions: await revisions(ZM_VERSIONS_PATH),
  }), 'NOT_FOUND');
  const deleted = await dispatchProjectAction(after, {
    action: 'version.delete',
    project: 'ZM',
    data: { id: 'Ship Candidate v2' },
    revisions: await revisions(ZM_VERSIONS_PATH),
    operationId: 'version-delete-zm',
  });
  expect(deleted.result).toMatchObject({ id: 'Ship Candidate v2', deleted: true });
  after = await loadVault(root);
  expect(after.configs.get(ZM_VERSIONS_PATH).fields.rows.some(row => row.name === 'Ship Candidate v2')).toBe(false);
});

test('version.reorder validates the order and repositions rows', async () => {
  const index = await loadVault(root);
  const names = after => after.configs.get(ZM_VERSIONS_PATH).fields.rows.map(row => row.name);
  const base = { action: 'version.reorder', project: 'ZM' };

  await rejectsWith(dispatchProjectAction(index, { ...base, data: { order: [] } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, data: { order: [42] } }), 'INVALID');
  // A partial order list is legal by design: unmatched rows keep their
  // relative position, so only empty/non-string orders are refused.

  const order = [...names(index)].reverse();
  const reordered = await dispatchProjectAction(index, {
    ...base,
    data: { order },
    revisions: await revisions(ZM_VERSIONS_PATH),
    operationId: 'version-reorder-zm',
  });
  expect(reordered.result).toMatchObject({ order, updated: true });
  const after = await loadVault(root);
  const positions = after.configs.get(ZM_VERSIONS_PATH).fields.rows.map(row => Number(row.position));
  expect([...positions].sort((a, b) => a - b)).toEqual([0, 1, 2, 3]);
});

test('project.create refuses a duplicate key and a missing name', async () => {
  const index = await loadVault(root);
  const configRevision = await readRevision(root, 'vault.config.json');
  const base = {
    action: 'project.create',
    revisions: { 'vault.config.json': configRevision },
  };

  await rejectsWith(dispatchProjectAction(index, { ...base, project: 'ZEN', data: { key: 'ZEN', name: 'Dup' } }), 'INVALID');
  await rejectsWith(dispatchProjectAction(index, { ...base, project: 'NEW', data: { key: 'NEW' } }), 'INVALID');
});
