#!/usr/bin/env node

/**
 * Exercise the local write boundary in a disposable Git repository.
 *
 * Rehearsal copies a prepared vault (or prepares one from a snapshot), enables
 * writes only in that copy, and runs a small claim -> finish -> recovery flow.
 * It never switches branches, merges, writes the supplied vault, or contacts
 * the legacy SaaS checkout.  The injected operation adapter is useful while
 * the CLI dispatcher is being integrated; the default adapter uses the shared
 * server/operations.mjs engine directly.
 */

import { createHash } from 'node:crypto';
import { constants, promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { loadVault } from '../server/main.js';
import { git, recoverOperations } from '../server/operations.mjs';
import { dispatchWrite } from '../cli/write.mjs';
import { prepareMigration } from './migrate.mjs';

const SKIP_NAMES = new Set(['.git', '.zagent', 'node_modules', 'artifacts']);

async function hashTree(root, current = root, rows = []) {
  let entries;
  try { entries = await fs.readdir(current, { withFileTypes: true }); } catch (error) {
    if (error.code === 'ENOENT') return rows.sort(([a], [b]) => a.localeCompare(b));
    throw error;
  }
  for (const entry of entries) {
    if (SKIP_NAMES.has(entry.name)) continue;
    const absolute = path.join(current, entry.name);
    if (entry.isDirectory()) await hashTree(root, absolute, rows);
    else if (entry.isFile()) {
      const bytes = await fs.readFile(absolute);
      rows.push([path.relative(root, absolute).replaceAll(path.sep, '/'), createHash('sha256').update(bytes).digest('hex')]);
    }
  }
  return rows.sort(([a], [b]) => a.localeCompare(b));
}

async function copyTree(source, destination) {
  await fs.mkdir(destination, { recursive: true });
  let entries = await fs.readdir(source, { withFileTypes: true });
  entries = entries.filter(entry => !SKIP_NAMES.has(entry.name));
  for (const entry of entries) {
    const from = path.join(source, entry.name);
    const to = path.join(destination, entry.name);
    if (entry.isDirectory()) await copyTree(from, to);
    else if (entry.isFile()) {
      await fs.mkdir(path.dirname(to), { recursive: true });
      await fs.copyFile(from, to, constants.COPYFILE_EXCL);
    }
  }
}

async function writeFile(file, text) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  await fs.writeFile(file, text, { flag: 'wx' });
}

async function enableWrites(root, options = {}) {
  const file = path.join(root, 'vault.config.json');
  const config = JSON.parse(await fs.readFile(file, 'utf8'));
  const actor = options.actor
    ?? config.writes?.agentEmail
    ?? config.policy?.board?.agentEmail
    ?? config.policy?.resolved?.agentEmail
    ?? 'vault-rehearsal@example.invalid';
  config.writes = {
    ...(config.writes ?? {}),
    enabled: true,
    commit: options.commit ?? true,
    testFaults: options.testFaults ?? true,
    agentEmail: actor,
  };
  await fs.writeFile(file, `${JSON.stringify(config, null, 2)}\n`, { flag: 'w' });
  return actor;
}

async function initialiseGit(root) {
  await git(root, ['init', '--initial-branch', 'master']);
  await git(root, ['config', 'user.email', 'vault-rehearsal@example.invalid']);
  await git(root, ['config', 'user.name', 'Vault rehearsal']);
  await git(root, ['add', '-A']);
  await git(root, ['commit', '-m', 'rehearsal baseline']);
  return {
    branch: await git(root, ['symbolic-ref', '--short', 'HEAD']),
    head: await git(root, ['rev-parse', 'HEAD']),
  };
}

function ticketRecord(index, key) {
  const wanted = String(key ?? '').toUpperCase();
  for (const record of index.tickets.values()) {
    if (String(record.metadata?.key ?? '').toUpperCase() === wanted) return record;
  }
  throw new Error(`Rehearsal ticket was not found: ${key}`);
}

function defaultKey(index, requested) {
  if (requested) return ticketRecord(index, requested).metadata.key;
  const candidate = [...index.tickets.values()]
    .filter(record => !['EPIC'].includes(String(record.metadata?.type ?? '').toUpperCase()))
    .find(record => String(record.metadata?.status ?? '').toLowerCase() === 'to do')
    ?? [...index.tickets.values()].find(record => !['EPIC'].includes(String(record.metadata?.type ?? '').toUpperCase()))
    ?? [...index.tickets.values()][0];
  if (!candidate) throw new Error('Rehearsal vault contains no ticket files.');
  return candidate.metadata.key;
}

async function operationResult(adapter, name, context, fallback) {
  const fn = adapter?.[name];
  return fn ? fn(context) : fallback();
}

async function makeWorkspace(options) {
  const parent = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-rehearsal-'));
  const root = path.join(parent, 'repo');
  await fs.mkdir(root);
  let sourceRoot = null;
  let migration = null;
  let preparationParent = null;
  if (options.source || options.sourcePath) {
    preparationParent = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-rehearsal-migration-'));
    const preparedRoot = path.join(preparationParent, 'prepared');
    migration = await prepareMigration({ ...options, sourcePath: options.sourcePath ?? options.source, destination: preparedRoot, prepare: true });
    await copyTree(preparedRoot, root);
    sourceRoot = options.sourceRepo ? path.resolve(options.sourceRepo) : null;
  } else if (options.vaultRoot || options.root) {
    sourceRoot = path.resolve(options.vaultRoot ?? options.root);
    await copyTree(sourceRoot, root);
  } else {
    await writeFile(path.join(root, 'vault.config.json'), `${JSON.stringify({ formatVersion: 1, boardRoots: [{ project: 'ZEN', path: 'ZEN' }], supportingFiles: [], policy: { resolved: { projects: { ZEN: { project: 'ZEN' } } } }, writes: { enabled: false } }, null, 2)}\n`);
    await writeFile(path.join(root, 'ZEN', 'tickets', 'ZEN-1.md'), '---\nkey: ZEN-1\ntype: TASK\ntitle: Rehearsal ticket\ncategory: Engine\nstatus: To Do\nassignee: null\nlabels: []\nblocks: []\nlinks: []\n---\nGoal\n\n## Definition of Done\n- [ ] Rehearsal\n');
  }
  const actor = await enableWrites(root, options);
  const baseline = await initialiseGit(root);
  return { parent, root, sourceRoot, migration, baseline, preparationParent, actor };
}

async function statusOf(root, key) {
  const index = await loadVault(root);
  return ticketRecord(index, key).metadata.status;
}

async function revertCommits(root, commits) {
  const valid = commits.filter(commit => typeof commit === 'string' && commit.length > 0);
  // The caller supplies newest first (finish, then claim), which is the
  // chronological inverse Git requires for a clean forward-recovery.
  for (const commit of valid) await git(root, ['revert', '--no-edit', commit]);
  return valid.length;
}

function operationPayload(value) {
  return value?.payload ?? value?.result ?? value ?? {};
}

function operationCommit(value) {
  const payload = operationPayload(value);
  return value?.commit ?? payload?.commit ?? payload?.operation?.commit ?? null;
}

async function defaultCliMutation(root, key, status, actor, id, attempt) {
  const index = await loadVault(root);
  const argv = status === 'In Progress'
    ? ['claim', key, '--operation-id', id]
    : ['finish', key, '--status', status, '--comment', 'rehearsal comment', '--worklog', 'rehearsal work log', '--operation-id', id, ...(attempt ? ['--attempt', attempt] : [])];
  return dispatchWrite(index, argv, status === 'In Progress' ? {} : { comment: 'rehearsal comment', worklog: 'rehearsal work log' });
}

/**
 * Run a complete disposable rehearsal.
 *
 * Adapter callbacks receive `{root, key, actor, id}`.  `claim` and `finish`
 * should return the normal operation result (`{operationId, result, commit}`);
 * `recover` receives the same root and may wrap a project-specific recovery
 * call.  Omitting the adapter uses the shared operation engine.
 */
export async function runRehearsal(options = {}) {
  const workspace = await makeWorkspace(options);
  const { root, sourceRoot } = workspace;
  const sourceBefore = sourceRoot ? await hashTree(sourceRoot) : null;
  let result;
  try {
    const beforeIndex = await loadVault(root);
    if (!beforeIndex.valid) throw new Error(`Rehearsal baseline is invalid: ${beforeIndex.diagnostics.map(item => item.message).join('; ')}`);
    const key = defaultKey(beforeIndex, options.key);
    const actor = options.actor ?? workspace.actor;
    const claimContext = { root, key, actor, id: options.claimId ?? 'rehearsal-claim' };
    const claim = await operationResult(options.operations, 'claim', claimContext, () => defaultCliMutation(root, key, 'In Progress', actor, claimContext.id));
    const claimedStatus = await statusOf(root, key);
    if (claimedStatus !== 'In Progress') throw new Error(`Rehearsal claim did not set ${key} to In Progress.`);

    const recoveredAfterClaim = options.operations?.recover
      ? await options.operations.recover({ root, phase: 'after-claim', key })
      : await recoverOperations(root);

    const claimPayload = operationPayload(claim);
    const claimAttempt = claimPayload.claimAttempt ?? claimPayload.attempt ?? null;
    const finishContext = { root, key, actor, id: options.finishId ?? 'rehearsal-finish', attempt: claimAttempt };
    const finish = await operationResult(options.operations, 'finish', finishContext, () => defaultCliMutation(root, key, 'Done', actor, finishContext.id, claimAttempt));
    const finishedStatus = await statusOf(root, key);
    if (finishedStatus !== 'Done') throw new Error(`Rehearsal finish did not set ${key} to Done.`);

    const journalRecovery = options.operations?.recover
      ? await options.operations.recover({ root, phase: 'after-finish', key })
      : await recoverOperations(root);

    // The shared mutating CLI returns `{exitCode, payload: {commit}}`, while
    // injected rehearsal adapters may return the engine's `{result, commit}`.
    // Normalize both shapes before forward recovery so a successful write can
    // never be reported as unrecoverable merely because it crossed the CLI
    // seam.
    const commits = [operationCommit(finish), operationCommit(claim)].filter(Boolean);
    let reverted = 0;
    let restoredStatus = finishedStatus;
    if (options.forwardRecovery !== false && commits.length === 2) {
      reverted = await revertCommits(root, commits);
      restoredStatus = await statusOf(root, key);
      if (restoredStatus !== String(beforeIndex.tickets.get(key)?.metadata?.status ?? 'To Do')) {
        throw new Error(`Forward recovery did not restore ${key} to its baseline status.`);
      }
    }
    const after = await git(root, ['rev-parse', 'HEAD']);
    result = {
      formatVersion: 1,
      temporary: true,
      noLiveWrites: true,
      workspaceRoot: options.keep ? root : null,
      migration: workspace.migration ? {
        source: workspace.migration.source,
        mappedProjects: workspace.migration.mappedProjects,
        archivedProjects: workspace.migration.archivedProjects,
        audit: workspace.migration.audit,
      } : null,
      git: { baseline: workspace.baseline, after, branch: workspace.baseline.branch, switched: false },
      ticket: { key, actor, before: beforeIndex.tickets.get(key)?.metadata?.status ?? null, claimed: claimedStatus, finished: finishedStatus, restored: restoredStatus },
      recovery: { afterClaim: recoveredAfterClaim, afterFinish: journalRecovery, forwardRecovery: { attempted: commits.length === 2, reverted, restored: restoredStatus === (beforeIndex.tickets.get(key)?.metadata?.status ?? null) } },
    };
    if (sourceRoot) {
      const sourceAfter = await hashTree(sourceRoot);
      result.noLiveWrites = JSON.stringify(sourceBefore) === JSON.stringify(sourceAfter);
      if (!result.noLiveWrites) throw new Error('Rehearsal changed its source vault; refusing to report success.');
    }
    return result;
  } finally {
    if (workspace.preparationParent) await fs.rm(workspace.preparationParent, { recursive: true, force: true });
    if (!options.keep) await fs.rm(workspace.parent, { recursive: true, force: true });
  }
}

function optionValue(args, name) {
  const at = args.indexOf(name);
  if (at >= 0 && at + 1 < args.length) return args[at + 1];
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return prefix ? prefix.slice(name.length + 1) : undefined;
}

export async function runRehearsalCli(rawArgs = process.argv.slice(2)) {
  const args = [...rawArgs];
  if (args.includes('--help') || args.length === 0) return {
    exitCode: 0,
    payload: {
      usage: 'node scripts/rehearse.mjs --vault PREPARED_VAULT [--key KEY] [--keep]',
      notes: ['Runs in a temporary canonical checkout.', 'Claim, finish and recovery never touch the supplied vault.', 'No branch switch or merge is performed.'],
    },
  };
  try {
    const vaultRoot = optionValue(args, '--vault') ?? optionValue(args, '--root');
    const source = optionValue(args, '--source');
    if (!vaultRoot && !source) throw new Error('Provide --vault PREPARED_VAULT or --source SOURCE_JSON.');
    const result = await runRehearsal({
      vaultRoot,
      sourcePath: source,
      policy: optionValue(args, '--policy') ? JSON.parse(await fs.readFile(path.resolve(optionValue(args, '--policy')), 'utf8')) : undefined,
      snapshotEvidence: optionValue(args, '--snapshot-evidence'),
      backupSha: optionValue(args, '--backup-sha'),
      dumpSha256: optionValue(args, '--dump-sha256'),
      key: optionValue(args, '--key'),
      actor: optionValue(args, '--actor'),
      keep: args.includes('--keep'),
      forwardRecovery: !args.includes('--no-forward-recovery'),
    });
    return { exitCode: 0, payload: result };
  } catch (error) {
    return { exitCode: 1, payload: { error: error.message ?? String(error), code: error.code } };
  }
}

const invoked = process.argv[1] ? path.resolve(process.argv[1]) : '';
if (invoked === path.resolve(fileURLToPath(import.meta.url))) {
  const result = await runRehearsalCli();
  process.stdout.write(`${JSON.stringify(result.payload, null, 2)}\n`);
  process.exitCode = result.exitCode;
}

export default runRehearsal;
