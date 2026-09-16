#!/usr/bin/env node

/**
 * Prepare a Vault migration in an isolated directory.
 *
 * This command deliberately has two modes.  The default is a read-only plan
 * printed as JSON.  `--prepare` creates a *new* directory and refuses to use a
 * directory that already exists.  It never copies into the checkout supplied
 * as `--source-repo`; that path is only used for optional living-document
 * copies.  The cutover step which would install these files in a working
 * repository is owned by the launcher integration, not this script.
 */

import { createHash } from 'node:crypto';
import { constants, promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseMarkdown } from '../server/parser.js';
import { auditExport, tableDigests } from './audit-export.mjs';
import { exportVault } from './export.mjs';

export const MAPPED_PROJECT_PATHS = Object.freeze({
  ZEN: 'Zenith/Docs/Board',
  ZM: 'Games/Zenithmon/Docs/Board',
  DP: 'Games/DevilsPlayground/Docs/Board',
  UV: 'Games/Undervault/Docs/Board',
  HE: 'Games/Hearth/Docs/Board',
  FD: 'Games/Foundry/Docs/Board',
});

export const MAPPED_PROJECT_KEYS = Object.freeze(Object.keys(MAPPED_PROJECT_PATHS));
export const LEGACY_ARCHIVE_PATH = 'archive/legacy';
export const MIGRATION_ARTIFACT_PATH = 'migration';

const HASH = /^(?:[0-9a-f]{40}|[0-9a-f]{64})$/i;
const SHA256 = /^[0-9a-f]{64}$/i;

function clone(value) {
  if (value === undefined) return undefined;
  return JSON.parse(JSON.stringify(value));
}

function table(snapshot, name) {
  const values = snapshot?.tables?.[name];
  return Array.isArray(values) ? values : [];
}

function sourceHash(snapshotBytes) {
  return createHash('sha256').update(snapshotBytes).digest('hex');
}

async function readJson(file) {
  const bytes = await fs.readFile(file);
  try {
    return { value: JSON.parse(bytes.toString('utf8')), bytes, sha256: sourceHash(bytes) };
  } catch (error) {
    throw new Error(`Invalid JSON in ${file}: ${error.message}`);
  }
}

function asPath(value, label) {
  if (typeof value !== 'string' || value.trim() === '') throw new TypeError(`${label} must be a non-empty path`);
  return path.resolve(value);
}

function safeRelative(value, label = 'path') {
  if (typeof value !== 'string' || value.trim() === '' || path.isAbsolute(value) || value.includes('\0')) {
    throw new Error(`${label} must be a relative path`);
  }
  const normalized = value.replaceAll('\\', '/');
  if (normalized.split('/').some(part => part === '..' || part === '.git' || part === '.zagent') || /^[A-Za-z]:/.test(normalized)) {
    throw new Error(`${label} escapes the migration root: ${value}`);
  }
  return normalized.replace(/^\.\//, '');
}

function normalizePolicy(policy = {}) {
  if (policy?.resolved?.projects && typeof policy.resolved.projects === 'object') return clone(policy);
  if (policy?.projects && typeof policy.projects === 'object') return { resolved: clone(policy) };
  if (policy?.client?.categories || policy?.client?.file) {
    const client = policy.client.file ?? policy.client;
    const projects = { ZEN: clone(client) };
    for (const category of Object.values(client.categories ?? {})) {
      if (category?.project) projects[category.project] = { ...clone(client), ...clone(category), project: category.project };
    }
    return { resolved: { ...clone(policy.resolved ?? {}), projects } };
  }
  const projects = Object.fromEntries(MAPPED_PROJECT_KEYS.map(key => [key, { project: key }]));
  return { resolved: { projects } };
}

/**
 * The Vault writer operates in the canonical checkout.  A legacy category may
 * still carry the old DP feature-branch setting; copying that setting into a
 * mutable Vault config would make the new writer switch branches around a
 * board commit.  Keep the rest of the resolved policy intact, but make the
 * generated writer policy direct for every project.
 */
export function normalizeMutablePolicy(policy = {}) {
  const normalized = normalizePolicy(policy);
  const resolved = normalized.resolved ?? {};
  const projects = Object.fromEntries(Object.entries(resolved.projects ?? {}).map(([key, value]) => {
    const project = clone(value) ?? {};
    delete project.branchPrefix;
    project.branching = 'direct';
    return [key, project];
  }));
  return { ...normalized, resolved: { ...resolved, projects } };
}

function resolveMappedProjects(snapshot, policy, requested) {
  const projectRows = table(snapshot, 'jira.projects');
  const byKey = new Map(projectRows.map(row => [String(row.key ?? '').toUpperCase(), row]));
  const configured = Object.keys(normalizePolicy(policy).resolved.projects ?? {});
  const selected = requested === undefined
    ? MAPPED_PROJECT_KEYS.filter(key => configured.includes(key) || byKey.has(key))
    : requested.map(value => String(value).toUpperCase());
  const keys = [...new Set(selected)];
  for (const key of keys) {
    if (!/^[A-Z0-9]+$/.test(key)) throw new Error(`Invalid mapped project key: ${key}`);
    if (!MAPPED_PROJECT_PATHS[key]) throw new Error(`No repository board path is configured for project ${key}`);
    if (!byKey.has(key)) throw new Error(`Snapshot does not contain configured project ${key}`);
  }
  return keys;
}

function statusCatalog(snapshot) {
  return new Map(table(snapshot, 'jira.workflow_statuses').map(row => [row.id, row]));
}

function issueProjectMap(snapshot) {
  return new Map(table(snapshot, 'jira.projects').map(row => [row.id, String(row.key ?? '').toUpperCase()]));
}

function issueKey(issue, projectById) {
  const project = projectById.get(issue.project_id);
  return project ? `${project}-${issue.number}` : null;
}

function inProgressIssues(snapshot) {
  const statuses = statusCatalog(snapshot);
  const projectById = issueProjectMap(snapshot);
  return table(snapshot, 'jira.issues').flatMap(issue => {
    const status = statuses.get(issue.status_id);
    const name = String(status?.name ?? '').trim();
    const category = String(status?.category ?? '').toUpperCase();
    // Match the plan and database fence's named status. In Review shares the
    // broad IN_PROGRESS category but is parked for a human decision, not an
    // active worker; it must not silently introduce a second drain condition.
    if (name.toLowerCase() !== 'in progress') return [];
    return [{
      key: issueKey(issue, projectById),
      project: projectById.get(issue.project_id) ?? null,
      status: name || null,
      category: category || null,
      issueId: issue.id ?? null,
    }];
  });
}

function counts(snapshot) {
  return Object.fromEntries(Object.entries(snapshot?.tables ?? {}).map(([name, rows]) => [name, Array.isArray(rows) ? rows.length : 0]));
}

function normalizeEvidence(value) {
  if (value && typeof value === 'object' && !Array.isArray(value)) {
    const result = {};
    if (value.path) result.path = path.resolve(value.path);
    if (value.sha256) result.sha256 = String(value.sha256);
    if (value.kind) result.kind = String(value.kind);
    return Object.keys(result).length ? result : null;
  }
  if (typeof value !== 'string' || value.trim() === '') return null;
  return { value: value.trim() };
}

async function evidenceDetails(value, sourcePath) {
  const evidence = normalizeEvidence(value);
  if (!evidence) return { supplied: false, valid: false, reason: 'A snapshot evidence path or hash is required.' };
  if (evidence.path) {
    try {
      const bytes = await fs.readFile(evidence.path);
      const sha256 = sourceHash(bytes);
      if (evidence.sha256 && evidence.sha256.toLowerCase() !== sha256) {
        return { supplied: true, valid: false, path: evidence.path, sha256, reason: 'Snapshot evidence hash does not match its file.' };
      }
      return { supplied: true, valid: bytes.length > 0, path: evidence.path, sha256, bytes: bytes.length };
    } catch (error) {
      return { supplied: true, valid: false, path: evidence.path, reason: `Cannot read snapshot evidence: ${error.message}` };
    }
  }
  const valueText = evidence.value ?? evidence.sha256;
  // A string evidence argument may be either a SHA-256 value or a path. Try
  // the path first so callers can point at a separately retained capture
  // file; only a real hexadecimal digest is accepted when no file exists.
  if (evidence.value) {
    const candidate = path.resolve(evidence.value);
    try {
      const bytes = await fs.readFile(candidate);
      return { supplied: true, valid: bytes.length > 0, path: candidate, sha256: sourceHash(bytes), bytes: bytes.length };
    } catch (error) {
      if (error.code !== 'ENOENT') {
        return { supplied: true, valid: false, path: candidate, reason: `Cannot read snapshot evidence: ${error.message}` };
      }
    }
  }
  const matchesSource = sourcePath && valueText && path.resolve(sourcePath).toLowerCase() === path.resolve(valueText).toLowerCase();
  const hashValue = evidence.sha256 ?? evidence.value;
  return {
    supplied: true,
    valid: Boolean(valueText && (SHA256.test(valueText) || matchesSource)),
    value: valueText,
    sourcePath: matchesSource ? path.resolve(sourcePath) : undefined,
    reason: valueText && !SHA256.test(hashValue ?? '') && !matchesSource
      ? 'Snapshot evidence must be a readable file or a SHA-256 hash.'
      : valueText ? undefined : 'Snapshot evidence cannot be empty.',
  };
}

function hashRequirement(value, label, pattern) {
  if (typeof value !== 'string' || value.trim() === '') return { supplied: false, valid: false, reason: `${label} is required.` };
  const text = value.trim();
  const valid = pattern.test(text);
  return {
    supplied: true,
    valid,
    value: text,
    format: valid ? 'hash' : 'invalid',
    ...(valid ? {} : { reason: `${label} must be a hexadecimal ${pattern === SHA256 ? 'SHA-256' : 'SHA-1/SHA-256'} hash.` }),
  };
}

async function writerFreezeDetails(value) {
  const supplied = value === true
    || (typeof value === 'string' && value.trim() !== '')
    || Boolean(value && typeof value === 'object' && (value.path || value.sha256));
  if (!supplied) return { supplied: false, valid: false, kind: 'missing', reason: 'Writer-freeze evidence is required before a production-ready migration.' };
  // The CLI flag is useful for an operator preparing an isolated rehearsal,
  // but it is not a durable freeze record and must never certify production.
  if (value === true) return { supplied: true, valid: false, kind: 'operator-attestation', reason: 'Writer-freeze is only an operator attestation; provide a freeze evidence file or SHA-256 hash for production readiness.' };
  const evidence = typeof value === 'string' ? { value: value.trim() } : value;
  const candidate = evidence.path ? path.resolve(evidence.path) : typeof evidence.value === 'string' ? path.resolve(evidence.value) : null;
  if (candidate) {
    try {
      const bytes = await fs.readFile(candidate);
      const sha256 = sourceHash(bytes);
      if (evidence.sha256 && String(evidence.sha256).toLowerCase() !== sha256) return { supplied: true, valid: false, kind: 'file', path: candidate, sha256, reason: 'Writer-freeze evidence hash does not match its file.' };
      let document;
      try { document = JSON.parse(bytes.toString('utf8')); } catch { /* a digest-only file is handled below */ }
      const marked = document && typeof document === 'object'
        && (document.frozen === true || document.writersFrozen === true || document.state === 'frozen' || document.kind === 'writer-freeze');
      const digestFile = SHA256.test(bytes.toString('utf8').trim());
      const valid = bytes.length > 0 && (Boolean(evidence.sha256) || marked || digestFile);
      return {
        supplied: true,
        valid,
        kind: 'file',
        path: candidate,
        sha256,
        bytes: bytes.length,
        ...(valid ? {} : { reason: 'Writer-freeze evidence must be marked frozen in JSON or carry a SHA-256 digest.' }),
      };
    } catch (error) {
      if (error.code !== 'ENOENT') return { supplied: true, valid: false, kind: 'file', path: candidate, reason: `Cannot read writer-freeze evidence: ${error.message}` };
    }
  }
  const hash = String(evidence.sha256 ?? evidence.value ?? '').trim();
  if (SHA256.test(hash)) return { supplied: true, valid: true, kind: 'hash', sha256: hash };
  return { supplied: true, valid: false, kind: 'invalid', reason: 'Writer-freeze evidence must be a readable file or a SHA-256 hash.' };
}

/**
 * Evaluate the freeze/readiness boundary without writing anything.
 * `snapshotEvidence`, `backupSha` and `dumpSha256` are intentionally explicit;
 * a source JSON file alone is not evidence that the cutover backup exists.
 */
export async function assessReadiness(snapshot, options = {}) {
  if (!snapshot || typeof snapshot !== 'object' || !snapshot.tables) {
    return {
      ready: false,
      preparationReady: false,
      productionReady: false,
      reasons: ['Snapshot must contain a tables object.'],
      productionReasons: ['Snapshot must contain a tables object.'],
      inProgress: [],
      issueCount: 0,
      counts: {},
      snapshotEvidence: { supplied: false, valid: false },
      backup: { supplied: false, valid: false },
      dump: { supplied: false, valid: false },
    };
  }
  const inProgress = inProgressIssues(snapshot);
  const snapshotEvidence = await evidenceDetails(options.snapshotEvidence ?? options.evidence, options.sourcePath);
  const backup = hashRequirement(options.backupSha ?? options.backupSHA, 'Backup SHA', HASH);
  const dump = hashRequirement(options.dumpSha256 ?? options.dumpHash ?? options.dumpSHA256, 'Dump SHA-256', SHA256);
  const writersFrozen = await writerFreezeDetails(options.writersFrozen);
  const preparationReasons = [];
  if (inProgress.length) preparationReasons.push(`${inProgress.length} ticket(s) are still In Progress; freeze requires none.`);
  if (!snapshotEvidence.valid) preparationReasons.push(snapshotEvidence.reason ?? 'Snapshot evidence is invalid.');
  if (!backup.valid) preparationReasons.push(backup.reason ?? 'Backup SHA is invalid.');
  if (!dump.valid) preparationReasons.push(dump.reason ?? 'Dump SHA-256 is invalid.');
  const reasons = [...preparationReasons];
  if (options.requireWritersFrozen === true && !writersFrozen.supplied) reasons.push('Writer-freeze evidence is required before a production-ready migration.');
  const productionReasons = [...preparationReasons, ...(writersFrozen.valid ? [] : [writersFrozen.reason ?? 'Writer-freeze evidence is not production-valid.'])];
  const preparationReady = preparationReasons.length === 0;
  const productionReady = productionReasons.length === 0;
  return {
    ready: reasons.length === 0,
    preparationReady,
    productionReady,
    reasons,
    productionReasons,
    issueCount: table(snapshot, 'jira.issues').length,
    counts: counts(snapshot),
    inProgress,
    snapshotEvidence,
    backup,
    dump,
    writersFrozen: { ...writersFrozen, required: options.requireWritersFrozen === true },
  };
}

async function copyTree(source, destination) {
  await fs.cp(source, destination, { recursive: true, errorOnExist: false, force: false });
}

async function listFiles(root, current = root, result = []) {
  for (const entry of await fs.readdir(current, { withFileTypes: true })) {
    const absolute = path.join(current, entry.name);
    if (entry.isDirectory()) await listFiles(root, absolute, result);
    else if (entry.isFile()) result.push(path.relative(root, absolute).replaceAll(path.sep, '/'));
  }
  return result;
}

async function writeFresh(file, value) {
  await fs.mkdir(path.dirname(file), { recursive: true });
  await fs.writeFile(file, typeof value === 'string' || Buffer.isBuffer(value) ? value : `${JSON.stringify(value, null, 2)}\n`, { flag: 'wx' });
}

async function createFreshRoot(root) {
  await fs.mkdir(path.dirname(root), { recursive: true });
  await fs.mkdir(root, { recursive: false });
}

async function livingFiles(options, policy) {
  const explicit = options.supportingFiles ?? options.livingDocs ?? [];
  const values = Array.isArray(explicit) ? explicit : [explicit];
  const result = [];
  for (const value of values) {
    const item = typeof value === 'string' ? value : value && typeof value === 'object' ? value.path ?? value.file : null;
    if (!item) continue;
    const relative = safeRelative(item, 'supporting file');
    if (!result.some(existing => (typeof existing === 'string' ? existing : existing.path).toLowerCase() === relative.toLowerCase())) {
      result.push(typeof value === 'string' ? relative : { ...value, path: relative });
    }
  }
  if (options.includeLivingDocs && options.sourceRepo) {
    const resolved = normalizePolicy(policy).resolved ?? {};
    const dirs = [...(Array.isArray(options.livingDirs) ? options.livingDirs : options.livingDir ? [options.livingDir] : [])];
    for (const project of Object.values(resolved.projects ?? {})) {
      for (const category of Object.values(project?.categories ?? {})) {
        if (category?.docs?.dir) dirs.push(category.docs.dir);
      }
    }
    for (const directory of [...new Set(dirs)]) {
      const relativeDirectory = safeRelative(directory, 'living docs directory');
      const repoRoot = path.resolve(options.sourceRepo);
      const absolute = path.resolve(repoRoot, relativeDirectory);
      if (!absolute.startsWith(repoRoot + path.sep)) throw new Error(`Living docs directory escapes source repository: ${directory}`);
      // A missing docs directory is a configuration error when the caller
      // explicitly requests discovery; do not silently produce a partial map.
      let discovered;
      try {
        discovered = await listFiles(absolute);
      } catch (error) {
        if (error.code === 'ENOENT' && options.allowMissingLivingDocs) continue;
        throw new Error(`Cannot scan living docs directory ${relativeDirectory}: ${error.message}`);
      }
      for (const file of discovered.filter(value => /\.(?:md|markdown)$/i.test(value))) {
        const relative = path.posix.join(relativeDirectory, file);
        if (!result.some(existing => (typeof existing === 'string' ? existing : existing.path).toLowerCase() === relative.toLowerCase())) {
          result.push(relative);
        }
      }
    }
  }
  return result;
}

function boardRootEntries(mappedKeys) {
  return mappedKeys.map(project => ({ project, path: MAPPED_PROJECT_PATHS[project] }));
}

function projectIssueCounts(snapshot) {
  const byProject = issueProjectMap(snapshot);
  const result = new Map();
  for (const issue of table(snapshot, 'jira.issues')) {
    const key = byProject.get(issue.project_id);
    if (!key) continue;
    result.set(key, (result.get(key) ?? 0) + 1);
  }
  return result;
}

async function migrationPlan(snapshot, sourceInfo, options, readiness, mappedKeys) {
  const countsByProject = projectIssueCounts(snapshot);
  const allProjects = [...new Set(table(snapshot, 'jira.projects').map(row => String(row.key ?? '').toUpperCase()).filter(Boolean))];
  // The source has many empty test projects. Only projects carrying issues
  // are archived, so the preservation count remains an explicit nine.
  const archived = allProjects.filter(key => !mappedKeys.includes(key) && (countsByProject.get(key) ?? 0) > 0);
  const mappedIssueCount = mappedKeys.reduce((sum, key) => sum + (countsByProject.get(key) ?? 0), 0);
  const archivedIssueCount = archived.reduce((sum, key) => sum + (countsByProject.get(key) ?? 0), 0);
  const supportingFiles = await livingFiles(options, options.policy);
  const archiveRoots = archived.map(project => ({ project, path: `${LEGACY_ARCHIVE_PATH}/${project}`, readOnly: true, archive: true }));
  return {
    formatVersion: 1,
    mode: options.prepare ? 'prepare' : 'dry-run',
    noOverwrite: true,
    source: {
      path: sourceInfo.path,
      sha256: sourceInfo.sha256,
      issueCount: table(snapshot, 'jira.issues').length,
      counts: readiness.counts,
    },
    readiness,
    mappedProjects: mappedKeys.map(key => ({
      project: key,
      destination: MAPPED_PROJECT_PATHS[key],
      issueCount: countsByProject.get(key) ?? 0,
    })),
    archivedProjects: archived.map(key => ({
      project: key,
      destination: `${LEGACY_ARCHIVE_PATH}/${key}`,
      issueCount: countsByProject.get(key) ?? 0,
    })),
    mappedIssueCount,
    archivedIssueCount,
    supportingFiles,
    boardRoots: [...boardRootEntries(mappedKeys), ...archiveRoots],
    destination: options.destination ? path.resolve(options.destination) : null,
    artifacts: {
      readiness: `${MIGRATION_ARTIFACT_PATH}/readiness.json`,
      audit: `${MIGRATION_ARTIFACT_PATH}/audit.json`,
      manifest: `${MIGRATION_ARTIFACT_PATH}/manifest.json`,
    },
  };
}

/**
 * Build a migration plan from a source snapshot.  Callers may pass an object
 * as `snapshot`; otherwise `source`/`sourcePath` is read as JSON.
 */
export async function planMigration(options = {}) {
  const sourcePath = options.sourcePath ?? options.source;
  let snapshot;
  let bytes;
  let resolvedSourcePath = sourcePath ? path.resolve(sourcePath) : null;
  if (options.snapshot && typeof options.snapshot === 'object') {
    snapshot = clone(options.snapshot);
    bytes = Buffer.from(JSON.stringify(snapshot));
  } else {
    if (!resolvedSourcePath) throw new Error('Migration requires --source SOURCE_JSON.');
    const source = await readJson(resolvedSourcePath);
    snapshot = source.value;
    bytes = source.bytes;
  }
  const policy = normalizeMutablePolicy(options.policy ?? {});
  const readiness = await assessReadiness(snapshot, {
    ...options,
    sourcePath: resolvedSourcePath,
    requireWritersFrozen: options.requireWritersFrozen === true,
  });
  const mappedKeys = resolveMappedProjects(snapshot, policy, options.mappedProjects);
  const sourceInfo = {
    path: resolvedSourcePath ?? '<in-memory snapshot>',
    sha256: sourceHash(bytes),
  };
  const plan = await migrationPlan(snapshot, sourceInfo, { ...options, policy }, readiness, mappedKeys);
  return { ...plan, snapshot, policy };
}

function ensureReady(plan, options = {}) {
  if (plan.readiness.ready) return;
  const onlyUnfrozen = plan.readiness.reasons.length > 0
    && plan.readiness.reasons.every(reason => reason.includes('Writer-freeze evidence'));
  if (options.allowUnfrozen === true && onlyUnfrozen) return;
  const error = new Error(`Migration readiness failed: ${plan.readiness.reasons.join(' ')}`);
  error.code = 'NOT_READY';
  error.readiness = plan.readiness;
  throw error;
}

async function auditPrepared(root, plan) {
  const found = new Map();
  const files = await listFiles(root);
  for (const relative of files) {
    if (!/^(.+\/)?tickets\/[^/]+\.md$/i.test(relative)) continue;
    const bytes = await fs.readFile(path.join(root, relative));
    let parsed;
    try { parsed = parseMarkdown(bytes); } catch (error) {
      throw new Error(`Prepared ticket is not parseable: ${relative}: ${error.message}`);
    }
    const key = parsed.fields?.key;
    if (!key) throw new Error(`Prepared ticket has no key: ${relative}`);
    if (found.has(key)) throw new Error(`Prepared migration contains duplicate ticket ${key}`);
    found.set(key, relative);
  }
  const expected = plan.mappedIssueCount + plan.archivedIssueCount;
  const mapped = [...found.keys()].filter(key => plan.mappedProjects.some(project => key.startsWith(`${project.project}-`))).length;
  const archived = found.size - mapped;
  const errors = [];
  if (found.size !== expected) errors.push(`Expected ${expected} ticket files, found ${found.size}.`);
  if (mapped !== plan.mappedIssueCount) errors.push(`Expected ${plan.mappedIssueCount} mapped tickets, found ${mapped}.`);
  if (archived !== plan.archivedIssueCount) errors.push(`Expected ${plan.archivedIssueCount} archived tickets, found ${archived}.`);
  return { ok: errors.length === 0, ticketCount: found.size, mapped, archived, errors, paths: Object.fromEntries(found) };
}

export async function prepareMigration(options = {}) {
  const destination = asPath(options.destination ?? options.output, 'destination');
  const plan = await planMigration({ ...options, destination, prepare: true, requireWritersFrozen: options.requireWritersFrozen ?? true });
  ensureReady(plan, options);
  const policy = normalizeMutablePolicy(plan.policy);
  const mappedKeys = plan.mappedProjects.map(item => item.project);
  const stagingParent = await fs.mkdtemp(path.join(os.tmpdir(), 'vault-export-'));
  const staging = path.join(stagingParent, 'export');
  let created = false;
  try {
    // exportVault itself also uses exclusive file creation.  The staging
    // export is complete before the destination is created, so a failure
    // cannot leave a half-mapped destination behind.
    await exportVault(plan.snapshot, staging, policy);
    await createFreshRoot(destination);
    created = true;
    for (const project of mappedKeys) {
      const source = path.join(staging, project);
      try { await fs.stat(source); } catch { continue; }
      await fs.mkdir(path.dirname(path.join(destination, MAPPED_PROJECT_PATHS[project])), { recursive: true });
      await copyTree(source, path.join(destination, MAPPED_PROJECT_PATHS[project]));
    }
    for (const archive of plan.archivedProjects) {
      const source = path.join(staging, archive.project);
      try { await fs.stat(source); } catch { continue; }
      await fs.mkdir(path.dirname(path.join(destination, LEGACY_ARCHIVE_PATH, archive.project)), { recursive: true });
      await copyTree(source, path.join(destination, LEGACY_ARCHIVE_PATH, archive.project));
    }
    await fs.copyFile(path.join(staging, 'metadata.md'), path.join(destination, 'metadata.md'), constants.COPYFILE_EXCL);

    const supportingFiles = ['metadata.md', ...plan.supportingFiles];
    const uniqueSupporting = [];
    const seenSupporting = new Set();
    for (const value of supportingFiles) {
      const descriptor = typeof value === 'string' ? value : value && typeof value === 'object' ? value : null;
      if (!descriptor) continue;
      const relative = safeRelative(typeof descriptor === 'string' ? descriptor : descriptor.path, 'supporting file');
      if (seenSupporting.has(relative.toLowerCase())) continue;
      seenSupporting.add(relative.toLowerCase());
      uniqueSupporting.push(typeof descriptor === 'string' ? relative : { ...descriptor, path: relative });
      if (relative === 'metadata.md') continue;
      if (options.sourceRepo) {
        const source = path.resolve(options.sourceRepo, relative);
        const target = path.resolve(destination, relative);
        if (!target.startsWith(destination + path.sep)) throw new Error(`Supporting file escapes destination: ${relative}`);
        try {
          const stat = await fs.stat(source);
          if (!stat.isFile()) throw new Error('not a file');
          await fs.mkdir(path.dirname(target), { recursive: true });
          await fs.copyFile(source, target, constants.COPYFILE_EXCL);
        } catch (error) {
          if (!options.allowMissingLivingDocs) throw new Error(`Cannot copy supporting file ${relative}: ${error.message}`);
        }
      }
    }

    const config = {
      formatVersion: 1,
      boardRoots: plan.boardRoots,
      supportingFiles: uniqueSupporting,
      policy,
      snapshotHash: plan.source.sha256,
      migration: {
        sourceSha256: plan.source.sha256,
        mappedProjects: mappedKeys,
        archivedProjects: plan.archivedProjects.map(item => item.project),
        preparedAt: new Date().toISOString(),
      },
      // A preparation artifact must remain inert if somebody points a reader
      // at it accidentally.  The rehearsal makes this explicit in its copy.
      writes: { enabled: false, commit: true },
      readiness: {
        ready: plan.readiness.ready,
        preparationReady: plan.readiness.preparationReady,
        productionReady: plan.readiness.productionReady,
        writersFrozen: plan.readiness.writersFrozen,
        reasons: plan.readiness.reasons,
        productionReasons: plan.readiness.productionReasons,
      },
    };
    await writeFresh(path.join(destination, 'vault.config.json'), config);
    const ticketAudit = await auditPrepared(destination, plan);
    const entityDifferences = auditExport(destination, tableDigests(plan.snapshot.tables));
    const audit = {
      ...ticketAudit,
      entityDifferences,
      entityCount: Object.keys(plan.snapshot.tables ?? {}).length,
      columnCount: Array.isArray(plan.snapshot.columns) ? plan.snapshot.columns.length : 0,
      sourceIssueCount: plan.source.issueCount,
      scope: { mappedIssueCount: plan.mappedIssueCount, archivedIssueCount: plan.archivedIssueCount },
      ok: ticketAudit.ok && entityDifferences.length === 0,
    };
    if (!audit.ok) {
      const details = [...ticketAudit.errors, ...entityDifferences.map(item => `Entity ${item.table} differs from source.`)];
      const error = new Error(`Prepared migration audit failed: ${details.join(' ')}`);
      error.code = 'AUDIT_FAILED';
      error.audit = audit;
      throw error;
    }
    const result = {
      ...plan,
      snapshot: undefined,
      policy: undefined,
      destination,
      audit,
      config: {
        path: 'vault.config.json',
        boardRoots: plan.boardRoots,
        supportingFiles: uniqueSupporting,
        writes: config.writes,
        readiness: config.readiness,
      },
    };
    await writeFresh(path.join(destination, MIGRATION_ARTIFACT_PATH, 'readiness.json'), plan.readiness);
    await writeFresh(path.join(destination, MIGRATION_ARTIFACT_PATH, 'audit.json'), audit);
    await writeFresh(path.join(destination, MIGRATION_ARTIFACT_PATH, 'manifest.json'), {
      formatVersion: 1,
      source: plan.source,
      mappedProjects: plan.mappedProjects,
      archivedProjects: plan.archivedProjects,
      issueCount: plan.source.issueCount,
      mappedIssueCount: plan.mappedIssueCount,
      archivedIssueCount: plan.archivedIssueCount,
      boardRoots: plan.boardRoots,
      supportingFiles: uniqueSupporting,
      noOverwrite: true,
    });
    return result;
  } catch (error) {
    // A partially-created destination is never silently reused.  Keep it for
    // forensic inspection, but make the failure explicit to the caller.
    if (created) error.destination = destination;
    throw error;
  } finally {
    await fs.rm(stagingParent, { recursive: true, force: true });
  }
}

function optionValue(args, name, { repeat = false } = {}) {
  const values = [];
  for (let index = 0; index < args.length; index += 1) {
    const value = args[index];
    if (value === name && index + 1 < args.length) values.push(args[++index]);
    else if (typeof value === 'string' && value.startsWith(`${name}=`)) values.push(value.slice(name.length + 1));
  }
  return repeat ? values : values.at(-1);
}

function usage() {
  return {
    usage: 'node scripts/migrate.mjs --source SOURCE_JSON --destination FRESH_DIR [--prepare] --snapshot-evidence PATH_OR_HASH --backup-sha SHA --dump-sha256 SHA',
    defaultMode: 'dry-run',
    notes: [
      'Dry-run reads the source only. --prepare creates a new destination and refuses an existing path.',
      'Only ZEN, ZM, DP, UV, HE and FD are mapped to repository board roots; all other issue projects go under archive/legacy/.',
      'This command never installs files into the active checkout or writes to the legacy SaaS repository.',
    ],
  };
}

export async function runMigrationCli(rawArgs = process.argv.slice(2)) {
  const args = [...rawArgs];
  if (args.includes('--help') || args.length === 0) return { exitCode: 0, payload: usage() };
  try {
    const source = optionValue(args, '--source') ?? optionValue(args, '--snapshot');
    if (!source) throw new Error('Missing --source SOURCE_JSON.');
    const policyPath = optionValue(args, '--policy') ?? optionValue(args, '--config');
    let policy = {};
    if (policyPath) policy = (await readJson(path.resolve(policyPath))).value;
    const mappedProjects = optionValue(args, '--project', { repeat: true });
    const destination = optionValue(args, '--destination') ?? optionValue(args, '--output') ?? optionValue(args, '--out');
    const options = {
      sourcePath: path.resolve(source),
      policy,
      mappedProjects: mappedProjects.length ? mappedProjects : undefined,
      destination,
      prepare: args.includes('--prepare'),
      sourceRepo: optionValue(args, '--source-repo') ?? optionValue(args, '--repo'),
      snapshotEvidence: optionValue(args, '--snapshot-evidence') ?? optionValue(args, '--evidence'),
      backupSha: optionValue(args, '--backup-sha') ?? optionValue(args, '--backup-sha256'),
      dumpSha256: optionValue(args, '--dump-sha256') ?? optionValue(args, '--dump-hash'),
      supportingFiles: optionValue(args, '--supporting-file', { repeat: true }),
      livingDirs: optionValue(args, '--living-dir', { repeat: true }),
      includeLivingDocs: args.includes('--include-living-docs'),
      allowMissingLivingDocs: args.includes('--allow-missing-living-docs'),
      writersFrozen: args.includes('--writers-frozen'),
      requireWritersFrozen: args.includes('--require-writers-frozen') || args.includes('--prepare'),
      allowUnfrozen: args.includes('--allow-unfrozen'),
    };
    const result = options.prepare ? await prepareMigration(options) : await planMigration(options);
    // The in-memory snapshot is needed by prepareMigration but is not a
    // migration artifact and can be several megabytes.  Keep the plan JSON
    // reviewable and make the source hash/counts the durable evidence.
    if (result && Object.hasOwn(result, 'snapshot')) {
      const { snapshot: _snapshot, policy: _policy, ...publicResult } = result;
      return { exitCode: 0, payload: publicResult };
    }
    return { exitCode: 0, payload: result };
  } catch (error) {
    return { exitCode: 1, payload: { error: error.message ?? String(error), code: error.code, readiness: error.readiness, audit: error.audit } };
  }
}

const invoked = process.argv[1] ? path.resolve(process.argv[1]) : '';
if (invoked === path.resolve(fileURLToPath(import.meta.url))) {
  const result = await runMigrationCli();
  process.stdout.write(`${JSON.stringify(result.payload, null, 2)}\n`);
  process.exitCode = result.exitCode;
}

export default planMigration;
