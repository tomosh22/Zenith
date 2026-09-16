/*
 * Local Vault mutations.
 *
 * The dispatcher deliberately contains no filesystem mutation code.  It
 * builds byte changes from a fresh VaultIndex and hands them to
 * server/operations.mjs, which owns the lock, revision checks, validation,
 * atomic replacement, journal and commit.  Keeping this module at the CLI
 * seam makes it possible to exercise every command without a board or a
 * database, while preserving the same operation protocol for a future UI.
 */

import { createHash, randomUUID } from 'node:crypto';

import { patchFields, replaceBody } from '../server/text-edit.mjs';
import {
  allTicketRecords,
  canonicalOf,
  clone,
  policyOf,
  projectKeyOf,
  semanticFlags,
  showPayload,
  sprintFor,
  ticketFor,
} from './compat.mjs';
import {
  breakerOpen,
  claimDecision,
  dispatchQueue,
} from './read.js';
import { databaseModel, rowsFor } from './queries.mjs';

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;
const STATUS_TODO = 'To Do';
const STATUS_IN_PROGRESS = 'In Progress';
const STATUS_BLOCKED = 'Blocked';
const STATUS_DONE = 'Done';
const STATUS_IN_REVIEW = 'In Review';

const ISSUE_TYPES = ['EPIC', 'STORY', 'TASK', 'BUG', 'SUBTASK'];
const PRIORITIES = ['CRITICAL', 'HIGH', 'MEDIUM', 'LOW', 'TRIVIAL'];
const COMPLEXITIES = ['TRIVIAL', 'SIMPLE', 'MODERATE', 'COMPLEX'];
const RISKS = ['LOW', 'MEDIUM', 'HIGH'];
const RESOLUTIONS = ['DONE', 'WONT_DO', 'DUPLICATE', 'CANNOT_REPRODUCE'];
const SEVERITIES = ['S1_CRITICAL', 'S2_MAJOR', 'S3_MINOR', 'S4_TRIVIAL'];
const REPRODUCIBILITIES = ['ALWAYS', 'SOMETIMES', 'RARELY', 'UNABLE'];

const LINK_TYPES = Object.freeze({
  blocks: 'BLOCKS',
  'blocked-by': 'BLOCKS',
  relates: 'RELATES_TO',
  'relates-to': 'RELATES_TO',
  duplicates: 'DUPLICATES',
  causes: 'CAUSES',
});

const KNOWLEDGE_FILES = Object.freeze({
  ask: 'questions',
  decide: 'decisions',
  shortfall: 'shortfalls',
  changelog: 'changelog',
  suggest: 'suggestions',
});

// Input bytes are not part of the argv array. Keep them in a per-index side
// channel so an explicit operation id cannot accidentally retry a command
// with a different `--file` payload.
const operationInputs = new WeakMap();

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value : [];
}

function valueOf(argv, name) {
  const args = Array.isArray(argv) ? argv : [];
  const index = args.indexOf(`--${name}`);
  if (index >= 0 && index + 1 < args.length && !String(args[index + 1]).startsWith('--')) {
    return args[index + 1];
  }
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`--${name}=`));
  return prefix === undefined ? undefined : prefix.slice(name.length + 3);
}

function hasFlag(argv, name) {
  return Array.isArray(argv) && argv.some(value => value === `--${name}` || (typeof value === 'string' && value.startsWith(`--${name}=`)));
}

function positional(argv, index) {
  const values = [];
  const args = Array.isArray(argv) ? argv : [];
  for (let i = 0; i < args.length; i += 1) {
    const value = String(args[i] ?? '');
    if (value.startsWith('--')) {
      if (!value.includes('=') && i + 1 < args.length && !String(args[i + 1]).startsWith('--')) i += 1;
      continue;
    }
    values.push(args[i]);
  }
  return values[index];
}

function errorResult(message, exitCode = 1, code = undefined) {
  const payload = { error: String(message) };
  if (code) payload.code = code;
  return { exitCode, payload };
}

function writeEnabled(index) {
  const root = object(index?.vaultConfig ?? index?.config);
  // The top-level switch is deliberately the only write capability bit.  A
  // policy or project payload may describe writes, but it must never enable
  // mutation accidentally when an exported vault is copied for review.
  return root.writes?.enabled === true;
}

function disabledResult() {
  return errorResult('Vault writes are disabled; configure writes.enabled explicitly', 1, 'READ_ONLY');
}

function bytesOf(record) {
  if (Buffer.isBuffer(record?.rawBytes)) return Buffer.from(record.rawBytes);
  if (Buffer.isBuffer(record?.bytes)) return Buffer.from(record.bytes);
  if (record?.rawBytes instanceof Uint8Array) return Buffer.from(record.rawBytes);
  if (record?.rawText !== undefined) return Buffer.from(String(record.rawText), 'utf8');
  return Buffer.alloc(0);
}

function hash(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

function expectedRevision(record) {
  if (!record) return null;
  const bytes = bytesOf(record);
  return {
    hash: record.hash ?? hash(bytes),
    size: record.size ?? bytes.length,
    ...(record.mtimeMs === undefined ? {} : { mtimeMs: record.mtimeMs }),
  };
}

function keyFor(record) {
  return String(canonicalOf(record).key ?? record?.key ?? '');
}

function findTicket(index, requested) {
  const text = String(requested ?? '');
  const direct = ticketFor(index, text) ?? ticketFor(index, text.toUpperCase());
  if (direct) return direct;
  const upper = text.toUpperCase();
  return allTicketRecords(index).find(record => keyFor(record).toUpperCase() === upper);
}

function requireTicket(index, requested) {
  const key = String(requested ?? '');
  if (!ISSUE_KEY.test(key)) throw new WriteError(`"${key}" is not a ticket key — expected <PROJECT>-<number>.`, 'USAGE', 1);
  const record = findTicket(index, key);
  if (!record) throw new WriteError(`No ticket ${key}.`, 'EMPTY', 3);
  return record;
}

function projectOf(index, record, requested) {
  const key = projectKeyOf(record, requested);
  if (key) return String(key).toUpperCase();
  const match = /^([A-Za-z][A-Za-z0-9_]*)-\d+$/.exec(String(requested ?? ''));
  return match?.[1]?.toUpperCase() ?? null;
}

function boardRootFor(index, project) {
  const wanted = String(project ?? '').toUpperCase();
  const root = array(index?.boardRoots).find(item => String(item?.project ?? '').toUpperCase() === wanted);
  if (!root) throw new WriteError(`Project ${wanted || '(none)'} is not configured in this vault`, 'CONFIGURATION', 1);
  return root;
}

function pathJoin(...parts) {
  return parts
    .filter(part => part !== undefined && part !== null && String(part) !== '')
    .map((part, index) => {
      const value = String(part).replaceAll('\\', '/');
      return index === 0 ? value.replace(/\/+$/, '') : value.replace(/^\/+|\/+$/g, '');
    })
    .filter(Boolean)
    .join('/');
}

function agentEmail(index, argv = []) {
  const root = object(index?.vaultConfig ?? index?.config);
  const policy = policyOf(index);
  return String(
    valueOf(argv, 'actor-email')
      ?? root.writes?.agentEmail
      ?? root.policy?.writes?.agentEmail
      ?? policy.policy.agentEmail
      ?? policy.policy.board?.agentEmail
      ?? 'agent@saas.invalid',
  );
}

function actorOf(index, argv = []) {
  const raw = String(valueOf(argv, 'actor') ?? '').toLowerCase();
  const human = raw === 'human' || hasFlag(argv, 'human');
  return { kind: human ? 'human' : 'agent', email: agentEmail(index, argv) };
}

function attemptOf(argv = []) {
  const attempt = valueOf(argv, 'attempt') ?? valueOf(argv, 'claim-attempt');
  return attempt === undefined ? undefined : String(attempt);
}

function enumValue(argv, name, allowed) {
  const raw = valueOf(argv, name);
  if (raw === undefined) return undefined;
  const match = allowed.find(value => value.toLowerCase() === String(raw).toLowerCase());
  if (!match) throw new WriteError(`--${name} must be one of ${allowed.join(', ')} — got "${raw}"`, 'USAGE', 1);
  return match;
}

function scalarValue(argv, name) {
  const value = valueOf(argv, name);
  return value === undefined ? undefined : String(value);
}

function pointsValue(argv, name = 'points') {
  const raw = valueOf(argv, name);
  if (raw === undefined) return undefined;
  if (String(raw).toLowerCase() === 'none') return null;
  const parsed = Number(raw);
  if (!Number.isInteger(parsed) || parsed < 0) throw new WriteError(`--${name} must be a whole number or "none" — got "${raw}"`, 'USAGE', 1);
  return parsed;
}

function dateValue(argv, name) {
  const raw = valueOf(argv, name);
  if (raw === undefined) return undefined;
  if (String(raw).toLowerCase() === 'none') return null;
  const parsed = new Date(raw);
  if (Number.isNaN(parsed.getTime())) throw new WriteError(`--${name} must be a date — got "${raw}"`, 'USAGE', 1);
  return parsed.toISOString();
}

function listValue(argv, name) {
  const raw = valueOf(argv, name);
  if (raw === undefined) return undefined;
  return [...new Set(String(raw).split(',').map(value => value.trim()).filter(Boolean))];
}

function bodyValue(argv, files, names, { required = false } = {}) {
  for (const name of names) {
    if (files?.[name] !== undefined) return String(files[name]);
    const value = valueOf(argv, name);
    if (value !== undefined) return String(value);
  }
  if (required) throw new WriteError(`Pass --${names[0]} <path> or --text "…".`, 'USAGE', 1);
  return undefined;
}

function changedFields(argv, current, files = {}) {
  const updates = {};
  const title = scalarValue(argv, 'title');
  const type = enumValue(argv, 'type', ISSUE_TYPES);
  const priority = enumValue(argv, 'priority', PRIORITIES);
  const complexity = enumValue(argv, 'complexity', COMPLEXITIES);
  const risk = enumValue(argv, 'risk', RISKS);
  const severity = enumValue(argv, 'severity', SEVERITIES);
  const reproducibility = enumValue(argv, 'repro', REPRODUCIBILITIES);
  const points = pointsValue(argv);
  const due = dateValue(argv, 'due');
  const assignee = valueOf(argv, 'assignee');
  const environment = scalarValue(argv, 'environment');

  if (title !== undefined) updates.title = title;
  if (type !== undefined) updates.type = type;
  if (priority !== undefined) updates.priority = priority;
  if (complexity !== undefined) updates.complexity = complexity;
  if (risk !== undefined) updates.risk = risk;
  if (severity !== undefined) updates.severity = severity;
  if (reproducibility !== undefined) updates.reproducibility = reproducibility;
  if (points !== undefined) updates.storyPoints = points;
  if (due !== undefined) updates.dueDate = due;
  if (environment !== undefined) updates.environment = environment;
  if (hasFlag(argv, 'flag')) updates.flagged = !hasFlag(argv, 'clear');
  if (assignee !== undefined) updates.assignee = String(assignee).toLowerCase() === 'none' ? null : String(assignee);
  const status = scalarValue(argv, 'status');
  if (status !== undefined) updates.status = status;
  const resolution = valueOf(argv, 'resolution');
  if (resolution !== undefined) updates.resolution = String(resolution).toLowerCase() === 'none' ? null : resolution;

  const labels = listValue(argv, 'label');
  const unlabel = listValue(argv, 'unlabel');
  if (labels !== undefined || unlabel !== undefined) {
    const existing = array(canonicalOf(current).labels).map(value => String(value));
    const remove = new Set(unlabel ?? []);
    const next = existing.filter(value => !remove.has(value));
    for (const label of labels ?? []) if (!next.includes(label)) next.push(label);
    updates.labels = next;
  }

  let body = bodyValue(argv, files, ['body']);
  const goal = bodyValue(argv, files, ['goal']);
  const dod = bodyValue(argv, files, ['dod']);
  if (body !== undefined && (goal !== undefined || dod !== undefined)) {
    throw new WriteError('Pass --body, or --goal/--dod, not both.', 'USAGE', 1);
  }
  if (body === undefined && (goal !== undefined || dod !== undefined)) {
    body = replaceSections(bodyTextOf(current), { Goal: goal, 'Definition of Done': dod });
  }
  return { updates, body };
}

function bodyTextOf(record) {
  return String(record?.bodyText ?? '');
}

function replaceSections(body, sections) {
  let result = String(body ?? '');
  for (const [heading, replacement] of Object.entries(sections)) {
    if (replacement === undefined) continue;
    const pattern = new RegExp(`(^|\\r?\\n)(#{1,6}\\s+${escapeRegExp(heading)}\\s*#*\\s*)(?:\\r?\\n|$)`, 'im');
    const match = pattern.exec(result);
    if (!match) throw new WriteError(`Body has no ## ${heading} section`, 'USAGE', 1);
    const start = match.index + match[1].length;
    const contentStart = match.index + match[0].length;
    const next = /\r?\n#{1,6}\s+.+?(?:\r?\n|$)/g;
    next.lastIndex = contentStart;
    const boundary = next.exec(result);
    const end = boundary ? boundary.index + 1 : result.length;
    const newline = result.includes('\r\n') ? '\r\n' : '\n';
    const text = String(replacement).replace(/(?:\r?\n)*$/, '');
    result = result.slice(0, start) + result.slice(start, contentStart) + text + newline + result.slice(end);
  }
  return result;
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}

function appendSection(body, heading, content) {
  const text = String(content ?? '').trim();
  if (!text) return String(body ?? '');
  const source = String(body ?? '');
  const newline = source.includes('\r\n') ? '\r\n' : '\n';
  const separator = source.length === 0 ? '' : source.endsWith('\n') ? '' : newline;
  const headingPattern = new RegExp(`(^|\\r?\\n)#{1,6}\\s+${escapeRegExp(heading)}\\s*#*\\s*(?:\\r?\\n|$)`, 'im');
  const match = headingPattern.exec(source);
  if (!match) return `${source}${separator}${newline}## ${heading}${newline}${newline}${text}${newline}`;
  const insertAt = match.index + match[0].length;
  return `${source.slice(0, insertAt)}${text}${newline}${source.slice(insertAt)}`;
}

function patchRecord(record, updates = {}, body = undefined) {
  let bytes = bytesOf(record);
  if (body !== undefined) bytes = replaceBody(bytes, Buffer.from(body, 'utf8'));
  const nextUpdates = { ...updates };
  if (record?.kind === 'ticket' && !Object.prototype.hasOwnProperty.call(nextUpdates, 'updatedAt')) {
    nextUpdates.updatedAt = new Date().toISOString();
  }
  if (Object.keys(nextUpdates).length) bytes = patchFields(bytes, nextUpdates);
  return bytes;
}

function statusNames(index, project) {
  const names = new Set();
  const wanted = String(project ?? '').toUpperCase();
  for (const record of index?.configs?.values?.() ?? []) {
    const fields = record.fields ?? {};
    for (const row of [...array(fields.statuses), ...array(fields.rows)]) {
      const name = row?.name ?? row?.status ?? row?.status_name;
      if (name === undefined) continue;
      const owner = String(row?.project ?? row?.project_key ?? record.project ?? '').toUpperCase();
      if (!owner || owner === wanted || owner === '*') names.add(String(name));
    }
  }
  for (const row of rowsFor(index, 'jira.workflow_statuses')) {
    const name = row?.name ?? row?.status ?? row?.status_name;
    if (name !== undefined) names.add(String(name));
  }
  return names;
}

function ensureStatus(index, project, status) {
  if (status === undefined || status === null || String(status).trim() === '') {
    throw new WriteError('Status must be a non-empty string', 'USAGE', 1);
  }
  const names = statusNames(index, project);
  if (names.size && ![...names].some(value => value.toLowerCase() === String(status).toLowerCase())) {
    throw new WriteError(`Unknown status "${status}" in ${project}; legal values: ${[...names].join(', ')}`, 'INVALID', 4);
  }
  return [...names].find(value => value.toLowerCase() === String(status).toLowerCase()) ?? String(status);
}

function workflowFor(index, project) {
  const wanted = String(project ?? '').toUpperCase();
  return [...(index?.configs?.values?.() ?? [])].find(record => {
    const kind = String(record.fields?.kind ?? '').toLowerCase();
    if (kind !== 'workflow' && !record.relativePath?.toLowerCase().endsWith('/workflow.md')) return false;
    const owner = String(record.project ?? record.fields?.project ?? '').toUpperCase();
    return !owner || owner === wanted || owner === '*';
  }) ?? null;
}

function transitionAllowed(index, project, fromStatus, toStatus) {
  if (String(fromStatus ?? '') === String(toStatus ?? '')) return true;
  const workflow = workflowFor(index, project);
  const statuses = array(workflow?.fields?.statuses ?? workflow?.fields?.rows);
  const transitions = array(workflow?.fields?.transitions);
  // Hand-authored vaults without a transition graph retain the useful field
  // validation above.  Once a graph exists, status edits must follow it.
  if (!workflow || !transitions.length || !statuses.length) return true;
  const statusId = value => {
    const text = String(value ?? '').toLowerCase();
    return statuses.find(row => String(row?.name ?? row?.status ?? '').toLowerCase() === text)?.id;
  };
  const from = statusId(fromStatus);
  const to = statusId(toStatus);
  if (from === undefined || to === undefined) return false;
  return transitions.some(edge => {
    const edgeFrom = edge?.from_status_id ?? edge?.fromStatusId ?? edge?.from;
    const edgeTo = edge?.to_status_id ?? edge?.toStatusId ?? edge?.to;
    return (edgeFrom === null || edgeFrom === undefined || String(edgeFrom) === String(from))
      && String(edgeTo) === String(to);
  });
}

function ensureTransition(index, project, current, next) {
  if (!transitionAllowed(index, project, current, next)) {
    throw new WriteError(`Workflow does not permit ${current} → ${next}`, 'INVALID', 4);
  }
  return next;
}

function isDoneStatus(index, project, status) {
  if (String(status ?? '').toLowerCase() === STATUS_DONE.toLowerCase()) return true;
  const workflow = workflowFor(index, project);
  const statuses = array(workflow?.fields?.statuses ?? workflow?.fields?.rows);
  const row = statuses.find(value => String(value?.name ?? value?.status ?? '').toLowerCase() === String(status ?? '').toLowerCase());
  return String(row?.category ?? row?.status_category ?? '').toUpperCase() === 'DONE';
}

function ensureHumanGateCloseout(index, record, project, actor, status) {
  if (actor.kind === 'human' || !semanticFlags(record).humanGate || !isDoneStatus(index, project, status)) return;
  throw new WriteError(
    `${keyFor(record)} carries human-gate; an agent must park it at In Review for human sign-off`,
    'HUMAN_GATE',
    4,
  );
}

function isAgentOwner(index, record, actor) {
  if (actor.kind === 'human') return true;
  const metadata = canonicalOf(record);
  return String(metadata.status ?? '') === STATUS_IN_PROGRESS
    && String(metadata.assignee ?? '').toLowerCase() === actor.email.toLowerCase();
}

function requireOwner(index, record, actor, { allowUnassigned = false, attemptRequired = false, attempt } = {}) {
  if (actor.kind === 'human') return;
  const metadata = canonicalOf(record);
  if (isAgentOwner(index, record, actor)) return;
  if (allowUnassigned && (metadata.assignee === null || metadata.assignee === undefined || metadata.assignee === '')) return;
  if (metadata.assignee && String(metadata.assignee).toLowerCase() !== actor.email.toLowerCase()) {
    throw new WriteError(`${keyFor(record)} is assigned to somebody else; the agent does not write to a ticket it lost`, 'OWNERSHIP', 5);
  }
  throw new WriteError(`${keyFor(record)} is no longer assigned to the agent — abandoning without writing`, 'OWNERSHIP', 5);
}

function requireAttempt(record, actor, attempt) {
  if (actor.kind === 'human') return;
  const expected = canonicalOf(record).claimAttempt ?? record?.fields?.claimAttempt;
  // Tickets written before claimAttempt was introduced are still finishable;
  // the first local claim adds the token.  Once present, every agent mutation
  // that can advance or rewrite the ticket must carry the exact token.
  if (expected !== undefined && expected !== null && String(expected) !== String(attempt ?? '')) {
    throw new WriteError(`${keyFor(record)} claim attempt is stale or missing; recover the current claim from durable scratch and retry`, 'OWNERSHIP', 5);
  }
}

function requireAgentOwnership(index, record, actor, options = {}) {
  requireOwner(index, record, actor, options);
  if (options.attemptRequired !== false) requireAttempt(record, actor, options.attempt);
}

function operationId(kind, argv, expected = {}) {
  const supplied = valueOf(argv, 'operation-id') ?? valueOf(argv, 'op-id');
  if (supplied) return supplied;
  // An omitted id is a new intent.  Reusing a deterministic argv-derived id
  // made two legitimate `create` or `next` invocations look like retries.
  return randomUUID();
}

async function engine() {
  return import('../server/operations.mjs');
}

async function runOperation(index, kind, argv, expected, prepare, fingerprintInput = undefined) {
  if (!writeEnabled(index)) return disabledResult();
  try {
    const { executeOperation } = await engine();
    const id = operationId(kind, argv, expected);
    const fingerprint = valueOf(argv, 'operation-id') ?? valueOf(argv, 'op-id')
      ? createHash('sha256').update(JSON.stringify({ kind, argv, input: fingerprintInput ?? operationInputs.get(index) ?? null })).digest('hex')
      : undefined;
    const response = await executeOperation(index.root ?? index.vaultRoot, {
      id,
      kind,
      expected,
      fingerprint,
      commit: undefined,
      prepare,
    });
    const payload = response?.result;
    if (payload && typeof payload === 'object' && !Array.isArray(payload)) {
      return {
        exitCode: 0,
        payload: {
          ...clone(payload),
          operationId: response.operationId ?? id,
          commit: response.commit ?? null,
        },
      };
    }
    return {
      exitCode: 0,
      payload: { result: clone(payload), operationId: response.operationId ?? id, commit: response.commit ?? null },
    };
  } catch (error) {
    const code = error?.code;
    const exitCode = error?.exitCode
      ?? (code === 'EMPTY' ? 3 : code === 'OWNERSHIP' || code === 'STALE' || code === 'LOCKED' || code === 'RECOVERY_CONFLICT' ? 5 : code === 'INVALID' ? 4 : 1);
    return errorResult(error?.message ?? String(error), exitCode, code);
  }
}

class WriteError extends Error {
  constructor(message, code = 'USAGE', exitCode = 1) {
    super(message);
    this.code = code;
    this.exitCode = exitCode;
  }
}

function queueCandidate(index, argv) {
  const configuredProject = String(valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project ?? '').toUpperCase();
  const breaker = breakerOpen(index, configuredProject);
  if (breaker.open) {
    throw new WriteError(
      `${configuredProject || 'This repository'} circuit breaker is open after ${breaker.limit} consecutive failure blocks: ${breaker.recent.join(', ')}`,
      'BREAKER',
      6,
    );
  }
  const queue = dispatchQueue(index, argv);
  if (queue.exitCode !== 0) throw new WriteError(queue.payload?.error ?? 'Unable to inspect the queue', 'INVALID', queue.exitCode);
  for (const candidate of array(queue.payload)) {
    if (!candidate?.claimable || candidate.assigned) continue;
    const record = findTicket(index, candidate.key);
    if (!record) continue;
    const decision = claimDecision(index, record, { queue: true });
    if (decision.queue.would) return record;
  }
  throw new WriteError('Nothing in "To Do".', 'EMPTY', 3);
}

async function claim(index, argv, files, queueMode) {
  const requested = queueMode ? undefined : positional(argv, 1);
  if (!queueMode && !requested) return errorResult('Usage: zagent claim <TICKET_KEY>', 1);
  const initial = requested ? findTicket(index, requested) : undefined;
  const expected = initial ? { [initial.relativePath]: expectedRevision(initial) } : {};
  const actor = actorOf(index, argv);
  return runOperation(index, queueMode ? 'next' : 'claim', argv, expected, async fresh => {
    if (!fresh.valid) throw new WriteError('Vault is invalid; claim refused', 'INVALID', 4);
    const selected = queueMode ? queueCandidate(fresh, argv) : requireTicket(fresh, requested);
    const project = projectOf(fresh, selected, keyFor(selected));
    const decision = claimDecision(fresh, selected, { queue: queueMode });
    if (decision.breaker?.open) {
      throw new WriteError(
        `${project ?? 'This repository'} circuit breaker is open after ${decision.breaker.limit} consecutive failure blocks: ${decision.breaker.recent.join(', ')}`,
        'BREAKER',
        6,
      );
    }
    const verdict = queueMode ? decision.queue : decision.targeted;
    // An explicit human takeover is a deliberate escape hatch for a stale
    // agent assignment. It still requires a valid repository and workflow;
    // only the agent-in-flight/assigned-to-other predicates are bypassed.
    if (!verdict.would && actor.kind !== 'human') {
      throw new WriteError(verdict.reason ?? `Cannot claim ${keyFor(selected)}`, decision.inFlight && decision.inFlight !== keyFor(selected) ? 'OWNERSHIP' : 'INVALID', decision.inFlight && decision.inFlight !== keyFor(selected) ? 5 : 4);
    }
    const metadata = canonicalOf(selected);
    ensureTransition(fresh, project, metadata.status, STATUS_IN_PROGRESS);
    if (actor.kind !== 'human' && metadata.assignee && String(metadata.assignee).toLowerCase() !== actor.email.toLowerCase()) {
      throw new WriteError(`${keyFor(selected)} is assigned to somebody else; the loop never steals a ticket`, 'OWNERSHIP', 5);
    }
    const claimAttempt = randomUUID();
    const payload = showPayload(fresh, selected, project);
    payload.previousStatus = metadata.status ?? null;
    payload.status = STATUS_IN_PROGRESS;
    payload.assignee = actor.email;
    payload.claimAttempt = claimAttempt;
    payload.attempt = claimAttempt;
    return {
      changes: [{ path: selected.relativePath, bytes: patchRecord(selected, { status: STATUS_IN_PROGRESS, assignee: actor.email, claimAttempt }) }],
      result: payload,
    };
  });
}

async function comment(index, argv, files) {
  const key = positional(argv, 1);
  if (!key) return errorResult('Usage: zagent comment <TICKET_KEY> --file <path>', 1);
  const text = bodyValue(argv, files, ['comment', 'text', 'file'], { required: true });
  const record = findTicket(index, key);
  const expected = record ? { [record.relativePath]: expectedRevision(record) } : {};
  return runOperation(index, 'comment', argv, expected, async fresh => {
    const current = requireTicket(fresh, key);
    // A comment is the recovery channel when an agent discovers that it lost
    // ownership. Preserve that path so the human can see abandoned evidence;
    // status and fields remain protected by ownership.
    return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { updatedAt: new Date().toISOString() }, appendSection(bodyTextOf(current), 'Comments', text)) }], result: { key: keyFor(current), commented: true } };
  });
}

async function move(index, argv) {
  const key = positional(argv, 1);
  const requestedStatus = positional(argv, 2) ?? valueOf(argv, 'status');
  if (!key || !requestedStatus) return errorResult('Usage: zagent move <TICKET_KEY> "<Status>"', 1);
  const actor = actorOf(index, argv);
  const record = findTicket(index, key);
  const expected = record ? { [record.relativePath]: expectedRevision(record) } : {};
  return runOperation(index, 'move', argv, expected, async fresh => {
    const current = requireTicket(fresh, key);
    const project = projectOf(fresh, current, key);
    const status = ensureStatus(fresh, project, requestedStatus);
    ensureTransition(fresh, project, canonicalOf(current).status, status);
    ensureHumanGateCloseout(fresh, current, project, actor, status);
    requireAgentOwnership(fresh, current, actor, { allowUnassigned: status === STATUS_TODO || status === STATUS_BLOCKED, attempt: attemptOf(argv) });
    const updates = { status };
    if (status === STATUS_TODO && String(canonicalOf(current).assignee ?? '').toLowerCase() === actor.email.toLowerCase()) updates.assignee = null;
    return { changes: [{ path: current.relativePath, bytes: patchRecord(current, updates) }], result: { key: keyFor(current), status } };
  });
}

async function finish(index, argv, files) {
  const key = positional(argv, 1);
  if (!key) return errorResult('Usage: zagent finish <TICKET_KEY> --status … --comment … --worklog …', 1);
  const statusRequested = valueOf(argv, 'status') ?? STATUS_BLOCKED;
  const commentText = bodyValue(argv, files, ['comment'], { required: true });
  const worklog = bodyValue(argv, files, ['worklog'], { required: true });
  const actor = actorOf(index, argv);
  const record = findTicket(index, key);
  const expected = record ? { [record.relativePath]: expectedRevision(record) } : {};
  return runOperation(index, 'finish', argv, expected, async fresh => {
    const current = requireTicket(fresh, key);
    const project = projectOf(fresh, current, key);
    const status = ensureStatus(fresh, project, statusRequested);
    ensureTransition(fresh, project, canonicalOf(current).status, status);
    ensureHumanGateCloseout(fresh, current, project, actor, status);
    requireAgentOwnership(fresh, current, actor, { attempt: attemptOf(argv) });
    let body = bodyTextOf(current);
    body = appendSection(body, 'Comments', commentText);
    body = appendSection(body, 'Work Log', worklog);
    return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { status, claimAttempt: null }, body) }], result: { key: keyFor(current), status } };
  });
}

function nextIssueNumber(index, project) {
  const numbers = allTicketRecords(index)
    .filter(record => projectOf(index, record, keyFor(record)) === project)
    .map(record => Number(/-(\d+)$/.exec(keyFor(record))?.[1]))
    .filter(Number.isFinite);
  return (numbers.length ? Math.max(...numbers) : 0) + 1;
}

function createTicketBytes(fields, body) {
  const ordered = ['id', 'key', 'type', 'title', 'category', 'parent', 'sprint', 'status', 'assignee', 'priority', 'complexity', 'risk', 'storyPoints', 'position', 'labels', 'fixVersions', 'blocks', 'updatedAt'];
  const lines = ['---'];
  for (const name of ordered) {
    if (!Object.prototype.hasOwnProperty.call(fields, name)) continue;
    lines.push(`${name}: ${JSON.stringify(fields[name])}`);
  }
  for (const [name, value] of Object.entries(fields)) {
    if (ordered.includes(name)) continue;
    lines.push(`${name}: ${JSON.stringify(value)}`);
  }
  lines.push('---', '');
  return Buffer.from(`${lines.join('\n')}${String(body ?? '')}`, 'utf8');
}

function parseKeyList(value) {
  return [...new Set(String(value ?? '').split(',').map(item => item.trim()).filter(Boolean))];
}

async function create(index, argv, files) {
  const project = String(valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project ?? '').toUpperCase();
  const title = valueOf(argv, 'title');
  if (!project || !title) return errorResult('Usage: zagent create --project KEY --title TEXT --file BODY', 1);
  const body = bodyValue(argv, files, ['file', 'body', 'text']) ?? '';
  const type = enumValue(argv, 'type', ISSUE_TYPES) ?? 'TASK';
  const priority = enumValue(argv, 'priority', PRIORITIES) ?? 'MEDIUM';
  const complexity = enumValue(argv, 'complexity', COMPLEXITIES) ?? 'MODERATE';
  const risk = enumValue(argv, 'risk', RISKS) ?? 'LOW';
  const category = valueOf(argv, 'category') ?? null;
  const parentRaw = valueOf(argv, 'parent');
  const parent = parentRaw === undefined || ['none', 'off', 'null'].includes(String(parentRaw).toLowerCase()) ? null : String(parentRaw);
  const assigneeRaw = valueOf(argv, 'assignee');
  const assignee = assigneeRaw === undefined || String(assigneeRaw).toLowerCase() === 'none' ? null : String(assigneeRaw);
  const labels = listValue(argv, 'label') ?? [];
  const points = pointsValue(argv) ?? null;
  const reason = valueOf(argv, 'reason');
  const blocks = parseKeyList(valueOf(argv, 'blocks'));
  const blockedBy = parseKeyList(valueOf(argv, 'blocked-by'));
  if ((blocks.length || blockedBy.length) && !String(reason ?? '').trim()) return errorResult('A blocks link needs --reason "…".', 4, 'INVALID');
  const sprintRaw = valueOf(argv, 'sprint');
  const sprint = sprintRaw === undefined || ['none', 'off', 'null'].includes(String(sprintRaw).toLowerCase()) ? null : String(sprintRaw);
  const fixVersion = valueOf(argv, 'fix-version');
  return runOperation(index, 'create', argv, {}, async fresh => {
    const number = nextIssueNumber(fresh, project);
    const key = `${project}-${number}`;
    const root = boardRootFor(fresh, project);
    const relativePath = pathJoin(root.path, 'tickets', `${key}.md`);
    if (findTicket(fresh, key)) throw new WriteError(`Ticket ${key} already exists`, 'INVALID', 1);
    const status = ensureStatus(fresh, project, valueOf(argv, 'status') ?? STATUS_TODO);
    if (parent) {
      const parentRecord = requireTicket(fresh, parent);
      if (String(canonicalOf(parentRecord).type ?? '').toUpperCase() !== 'EPIC') throw new WriteError(`${parent} is not an EPIC`, 'INVALID', 4);
    }
    for (const target of blocks) requireTicket(fresh, target);
    for (const target of blockedBy) requireTicket(fresh, target);
    const fields = {
      id: randomUUID(), key, type, title: String(title), category, parent, sprint, status, assignee,
      priority, complexity, risk, storyPoints: points, position: number, labels,
      fixVersions: fixVersion ? [fixVersion] : [],
      updatedAt: new Date().toISOString(),
      blocks: blocks.map(target => ({ key: target, reason })),
    };
    const changes = [{ path: relativePath, bytes: createTicketBytes(fields, body) }];
    for (const sourceKey of blockedBy) {
      const source = requireTicket(fresh, sourceKey);
      const sourceFields = canonicalOf(source);
      const sourceBlocks = array(sourceFields.blocks).slice();
      if (!sourceBlocks.some(item => String(item?.key ?? '') === key)) sourceBlocks.push({ key, reason });
      changes.push({ path: source.relativePath, bytes: patchRecord(source, { blocks: sourceBlocks }) });
    }
    return { changes, result: { key, id: fields.id, created: true, status: fields.status, assignee: fields.assignee } };
  });
}

function linkOperands(argv) {
  const source = positional(argv, 1);
  const verb = String(positional(argv, 2) ?? '').toLowerCase();
  const target = positional(argv, 3);
  const type = LINK_TYPES[verb];
  if (!source || !target || !type) throw new WriteError(`Usage: zagent link|unlink <KEY> ${Object.keys(LINK_TYPES).join('|')} <KEY>`, 'USAGE', 1);
  return verb === 'blocked-by' ? { source: target, target: source, verb, type } : { source, target, verb, type };
}

async function link(index, argv, unlink = false) {
  const { source: sourceKey, target: targetKey, verb, type } = linkOperands(argv);
  const reason = valueOf(argv, 'reason');
  if (!unlink && type === 'BLOCKS' && !String(reason ?? '').trim()) return errorResult('A blocks link needs --reason "…".', 4, 'INVALID');
  const source = findTicket(index, sourceKey);
  const expected = source ? { [source.relativePath]: expectedRevision(source) } : {};
  return runOperation(index, unlink ? 'unlink' : 'link', argv, expected, async fresh => {
    const record = requireTicket(fresh, sourceKey);
    requireTicket(fresh, targetKey);
    const metadata = canonicalOf(record);
    const field = type === 'BLOCKS' ? 'blocks' : 'links';
    const rows = array(metadata[field]).map(item => (typeof item === 'string' ? { key: item } : { ...object(item) }));
    const matches = item => String(item.key ?? item.target ?? item.ticket ?? '').toUpperCase() === targetKey.toUpperCase()
      && (type === 'BLOCKS' || String(item.type ?? 'RELATES_TO').toUpperCase() === type);
    const existing = rows.find(matches);
    let next = rows;
    let result;
    if (unlink) {
      next = rows.filter(item => !matches(item));
      result = { removed: next.length !== rows.length, sourceKey: keyFor(record), targetKey, type };
    } else if (existing) {
      if (type === 'BLOCKS' && !existing.reason && reason) {
        next = rows.map(item => matches(item) ? { ...item, reason } : item);
        result = { created: false, sourceKey: keyFor(record), targetKey, type, reason, reasonOutcome: 'recorded' };
      } else {
        result = { created: false, sourceKey: keyFor(record), targetKey, type, reason: existing.reason ?? null, reasonOutcome: reason && reason !== existing.reason ? 'ignored' : 'unchanged' };
      }
    } else {
      next = [...rows, type === 'BLOCKS' ? { key: targetKey, reason } : { key: targetKey, type, ...(reason ? { reason } : {}) }];
      result = { created: true, sourceKey: keyFor(record), targetKey, type, reason: reason ?? null, reasonOutcome: reason ? 'recorded' : 'unchanged' };
    }
    if (JSON.stringify(next) === JSON.stringify(rows)) return { changes: [], result };
    return { changes: [{ path: record.relativePath, bytes: patchRecord(record, { [field]: next }) }], result };
  });
}

async function parent(index, argv) {
  const key = positional(argv, 1);
  const parentKey = positional(argv, 2);
  if (!key || parentKey === undefined) return errorResult('Usage: zagent parent <TICKET_KEY> <PARENT_KEY|none>', 1);
  const record = findTicket(index, key);
  const expected = record ? { [record.relativePath]: expectedRevision(record) } : {};
  return runOperation(index, 'parent', argv, expected, async fresh => {
    const current = requireTicket(fresh, key);
    const value = ['none', 'off', 'null'].includes(String(parentKey).toLowerCase()) ? null : String(parentKey);
    if (value) {
      if (value.toUpperCase() === keyFor(current).toUpperCase()) throw new WriteError('A ticket cannot be its own parent', 'INVALID', 4);
      const target = requireTicket(fresh, value);
      if (String(canonicalOf(target).type ?? '').toUpperCase() !== 'EPIC') throw new WriteError(`${value} is not an EPIC`, 'INVALID', 4);
    }
    return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { parent: value }) }], result: { key: keyFor(current), parent: value } };
  });
}

function slug(value) {
  const text = String(value).normalize('NFKC').trim().toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
  return text || `sprint-${Date.now()}`;
}

function sprintRecord(index, project, name) {
  const wanted = String(name ?? '').trim().toLowerCase();
  const owner = String(project ?? '').toUpperCase();
  return [...(index?.sprints?.values?.() ?? [])].find(record => String(record.project ?? '').toUpperCase() === owner
    && (String(canonicalOf(record).name ?? '').trim().toLowerCase() === wanted
    || String(canonicalOf(record).slug ?? '').trim().toLowerCase() === wanted));
}

function configRecord(index, project, kind) {
  const wanted = String(project ?? '').toUpperCase();
  return [...(index?.configs?.values?.() ?? [])].find(record => String(record.project ?? '').toUpperCase() === wanted
    && (String(record.fields?.kind ?? '').toLowerCase() === kind || record.relativePath.toLowerCase().endsWith(`/${kind}.md`)));
}

async function sprint(index, argv) {
  const sub = String(positional(argv, 1) ?? 'list').toLowerCase();
  if (sub === 'list') return null;
  const project = String(valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project ?? '').toUpperCase();
  const name = positional(argv, 2);
  if (!name) return errorResult(`Usage: zagent sprint ${sub} <NAME>`, 1);
  if (sub === 'create') {
    const existing = sprintRecord(index, project, name);
    if (existing) return { exitCode: 0, payload: { name, created: false, operationId: null, commit: null } };
    return runOperation(index, 'sprint-create', argv, {}, async fresh => {
      if (sprintRecord(fresh, project, name)) return { changes: [], result: { name, created: false } };
      const root = boardRootFor(fresh, project);
      const base = slug(name);
      const used = new Set([...((fresh?.sprints?.keys?.() ?? []))].map(value => String(value).toLowerCase()));
      let sprintSlug = `${String(project).toLowerCase()}-${base}`;
      let suffix = 2;
      while (used.has(sprintSlug.toLowerCase())) sprintSlug = `${String(project).toLowerCase()}-${base}-${suffix++}`;
      const relativePath = pathJoin(root.path, 'sprints', `${sprintSlug}.md`);
      const fields = { kind: 'sprint', slug: sprintSlug, name: String(name), status: 'PLANNED', continuous: hasFlag(argv, 'continuous'), ...(valueOf(argv, 'goal') !== undefined ? { goal: valueOf(argv, 'goal') } : {}) };
      return { changes: [{ path: relativePath, bytes: createTicketBytes(fields, '') }], result: { id: sprintSlug, name, created: true } };
    });
  }
  const initial = sprintRecord(index, project, name);
  const expected = initial ? { [initial.relativePath]: expectedRevision(initial) } : {};
  return runOperation(index, `sprint-${sub}`, argv, expected, async fresh => {
    const current = sprintRecord(fresh, project, name);
    if (!current) throw new WriteError(`No sprint called "${name}" in ${project}.`, 'EMPTY', 3);
    const fields = canonicalOf(current);
    if (sub === 'start') return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { status: 'ACTIVE', startDate: new Date().toISOString() }) }], result: { name: fields.name ?? name, status: 'ACTIVE' } };
    if (sub === 'complete') {
      const changes = [{ path: current.relativePath, bytes: patchRecord(current, { status: 'COMPLETED', completedAt: new Date().toISOString() }) }];
      let movedToBacklog = 0;
      const unfinished = allTicketRecords(fresh).filter(ticket => {
        if (String(canonicalOf(ticket).sprint ?? '') !== String(fields.slug ?? fields.key ?? '')) return false;
        if (String(canonicalOf(ticket).statusCategory ?? '').toUpperCase() === 'DONE' || String(canonicalOf(ticket).status ?? '').toUpperCase() === STATUS_DONE.toUpperCase()) return false;
        return true;
      });
      if (unfinished.length && !hasFlag(argv, 'force')) {
        throw new WriteError(`Sprint ${fields.name ?? name} has unfinished tickets: ${unfinished.map(ticket => keyFor(ticket)).join(', ')}; pass --force to carry them to the backlog`, 'INVALID', 4);
      }
      for (const ticket of unfinished) {
        changes.push({ path: ticket.relativePath, bytes: patchRecord(ticket, { sprint: null }) });
        movedToBacklog += 1;
      }
      return { changes, result: { name: fields.name ?? name, status: 'COMPLETED', movedToBacklog } };
    }
    if (sub === 'add' || sub === 'remove') {
      const key = positional(argv, 3);
      if (!key) throw new WriteError(`Usage: zagent sprint ${sub} <NAME> <TICKET_KEY>`, 'USAGE', 1);
      const ticket = requireTicket(fresh, key);
      const sprint = sub === 'add' ? (fields.slug ?? fields.key ?? name) : null;
      return { changes: [{ path: ticket.relativePath, bytes: patchRecord(ticket, { sprint }) }], result: { key: keyFor(ticket), sprint } };
    }
    if (sub === 'gate') {
      const key = positional(argv, 3);
      const gate = !key || ['none', 'off'].includes(String(key).toLowerCase()) ? null : key;
      if (gate) requireTicket(fresh, gate);
      return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { gate }) }], result: { sprint: fields.name ?? name, gate } };
    }
    throw new WriteError('Usage: zagent sprint list|create|start|complete|add|remove|gate', 'USAGE', 1);
  });
}

function versionRows(record) {
  return array(record?.fields?.rows ?? record?.metadata?.rows);
}

function versionMatch(row, name) {
  return String(row?.name ?? row?.slug ?? row?.id ?? '').toLowerCase() === String(name ?? '').toLowerCase();
}

async function version(index, argv) {
  const sub = String(positional(argv, 1) ?? 'list').toLowerCase();
  if (sub === 'list') return null;
  const project = String(valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project ?? '').toUpperCase();
  const name = positional(argv, 2);
  if (!name) return errorResult(`Usage: zagent version ${sub} <NAME>`, 1);
  if (sub === 'set') {
    const key = positional(argv, 3);
    if (!key) return errorResult('Usage: zagent version set <NAME|none> <TICKET_KEY>', 1);
    const ticket = findTicket(index, key);
    const expected = ticket ? { [ticket.relativePath]: expectedRevision(ticket) } : {};
    return runOperation(index, 'version-set', argv, expected, async fresh => {
      const current = requireTicket(fresh, key);
      const versionName = String(name).toLowerCase() === 'none' ? null : String(name);
      if (versionName) {
        const catalog = configRecord(fresh, project, 'versions');
        if (!catalog || !versionRows(catalog).some(row => versionMatch(row, versionName))) throw new WriteError(`No version called "${versionName}" in ${project}.`, 'EMPTY', 3);
      }
      const field = String(valueOf(argv, 'kind') ?? 'FIX').toUpperCase() === 'AFFECTS' ? 'affectsVersions' : 'fixVersions';
      const values = array(canonicalOf(current)[field]).filter(value => String(value).toLowerCase() !== String(versionName ?? '').toLowerCase());
      if (versionName) values.push(versionName);
      return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { [field]: values }) }], result: { key: keyFor(current), version: versionName, kind: field } };
    });
  }
  const catalog = configRecord(index, project, 'versions');
  const expected = catalog ? { [catalog.relativePath]: expectedRevision(catalog) } : {};
  return runOperation(index, `version-${sub}`, argv, expected, async fresh => {
    const current = configRecord(fresh, project, 'versions');
    if (!current) throw new WriteError(`No versions catalog for ${project}.`, 'EMPTY', 3);
    const rows = versionRows(current).map(row => ({ ...object(row) }));
    if (sub === 'create') {
      if (rows.some(row => versionMatch(row, name))) return { changes: [], result: { name, created: false } };
      const row = { id: randomUUID(), name: String(name), description: valueOf(argv, 'description') ?? null, status: 'UNRELEASED', position: rows.length + 1 };
      return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { rows: [...rows, row] }) }], result: { id: row.id, name: row.name, created: true } };
    }
    const found = rows.find(row => versionMatch(row, name));
    if (!found) throw new WriteError(`No version called "${name}" in ${project}.`, 'EMPTY', 3);
    if (sub === 'release') {
      const releasedAt = new Date().toISOString();
      const next = rows.map(row => versionMatch(row, name) ? { ...row, status: 'RELEASED', releaseDate: releasedAt, release_date: releasedAt } : row);
      return { changes: [{ path: current.relativePath, bytes: patchRecord(current, { rows: next }) }], result: { name: found.name, status: 'RELEASED' } };
    }
    throw new WriteError('Usage: zagent version list|create|release|set', 'USAGE', 1);
  });
}

async function update(index, argv, files, shorthand = null) {
  const key = positional(argv, 1);
  if (!key) return errorResult(`Usage: zagent ${shorthand ?? 'update'} <TICKET_KEY> …`, 1);
  const actor = actorOf(index, argv);
  const record = findTicket(index, key);
  const expected = record ? { [record.relativePath]: expectedRevision(record) } : {};
  return runOperation(index, shorthand ?? 'update', argv, expected, async fresh => {
    const current = requireTicket(fresh, key);
    const { updates, body } = changedFields(argv, current, files);
    if (shorthand === 'estimate') {
      const raw = positional(argv, 2);
      const points = pointsValue(argv) ?? (raw?.toLowerCase() === 'none' ? null : Number(raw));
      if (points !== null && (!Number.isInteger(points) || points < 0)) throw new WriteError('Usage: zagent estimate <TICKET_KEY> <POINTS|none>', 'USAGE', 1);
      updates.storyPoints = points;
    }
    if (shorthand === 'flag') updates.flagged = !hasFlag(argv, 'clear');
    if (shorthand === 'resolve') {
      const raw = hasFlag(argv, 'clear') ? null : (valueOf(argv, 'resolution') ?? positional(argv, 2) ?? 'DONE');
      if (raw !== null) {
        const canonical = RESOLUTIONS.find(value => value.toLowerCase() === String(raw).toLowerCase());
        if (!canonical) throw new WriteError(`Resolution must be one of ${RESOLUTIONS.join(', ')} — got "${raw}"`, 'USAGE', 1);
        updates.resolution = canonical;
        updates.resolvedAt = new Date().toISOString();
      } else {
        updates.resolution = null;
        updates.resolvedAt = null;
      }
    }
    const hasBody = body !== undefined;
    const meaningful = Object.keys(updates).length > 0 || hasBody;
    if (!meaningful) throw new WriteError('Nothing to update — pass at least one field.', 'USAGE', 1);
    if (updates.status !== undefined) {
      updates.status = ensureStatus(fresh, projectOf(fresh, current, key), updates.status);
      const project = projectOf(fresh, current, key);
      ensureTransition(fresh, project, canonicalOf(current).status, updates.status);
      ensureHumanGateCloseout(fresh, current, project, actor, updates.status);
      if (actor.kind === 'agent' && String(updates.status) !== String(canonicalOf(current).status)) {
        requireAgentOwnership(fresh, current, actor, { attempt: attemptOf(argv) });
      }
    }
    if (actor.kind === 'agent' && (canonicalOf(current).assignee || canonicalOf(current).status === STATUS_IN_PROGRESS)) {
      requireAgentOwnership(fresh, current, actor, { attempt: attemptOf(argv) });
    }
    return { changes: [{ path: current.relativePath, bytes: patchRecord(current, updates, body) }], result: { key: keyFor(current), changed: Object.keys(updates) } };
  });
}

function parseKnowledgeInput(command, argv, files) {
  const raw = bodyValue(argv, files, ['file', 'text'], { required: true }).trim();
  if (raw.startsWith('{') || raw.startsWith('[')) {
    try { return object(JSON.parse(raw)); } catch (error) { throw new WriteError(`--file must contain JSON — ${error.message}`, 'USAGE', 1); }
  }
  if (command === 'ask') return { question: raw };
  if (command === 'changelog') return { summary: raw };
  if (command === 'suggest') return { area: 'other', suggestion: raw, friction: '(not recorded — filed as a one-line suggestion)' };
  throw new WriteError(`zagent ${command} needs a JSON body`, 'USAGE', 1);
}

function knowledgeRecord(index, project, command) {
  const base = KNOWLEDGE_FILES[command];
  const wanted = String(project ?? '').toUpperCase();
  return [...(index?.knowledge?.values?.() ?? [])].find(record => String(record.project ?? '').toUpperCase() === wanted && record.relativePath.toLowerCase().includes(`/knowledge/${base}`))
    ?? [...(index?.knowledge?.values?.() ?? [])].find(record => record.relativePath.toLowerCase().endsWith(`/knowledge/${base}.md`));
}

function metadataRecord(index) {
  return [...(index?.metadataFiles?.values?.() ?? [])].find(record => String(record.relativePath).toLowerCase() === 'metadata.md')
    ?? [...(index?.metadataFiles?.values?.() ?? [])][0];
}

function knowledgeValues(command, input, argv, ticketKey) {
  const category = valueOf(argv, 'category') ?? null;
  const values = { ...input };
  if (ticketKey) values.Ticket = ticketKey;
  if (category && values.Category === undefined) values.Category = category;
  if (command === 'ask') {
    values.Category ??= 'Uncategorized'; values.Asked ??= new Date().toISOString().slice(0, 10); values.Status ??= 'open'; values['Cost if wrong'] ??= 'medium';
  } else if (command === 'decide') {
    values.Category ??= 'Uncategorized'; values.Date ??= new Date().toISOString().slice(0, 10); values.Reversibility ??= 'easy';
  } else if (command === 'shortfall') {
    values.Impact ??= 'medium'; values.Status ??= 'open';
  } else if (command === 'changelog') {
    values.Date ??= new Date().toISOString().slice(0, 10); values.Category ??= 'Uncategorized';
  } else if (command === 'suggest') {
    values.Severity ??= 'slows-work'; values.Status ??= 'new';
  }
  return values;
}

function lowerFieldMap(values) {
  return new Map(Object.entries(values).map(([key, value]) => [key.toLowerCase().replace(/[ _-]/g, ''), value]));
}

function appendKnowledgeRow(index, command, input, ticketKey, argv = []) {
  const project = projectOf(index, findTicket(index, ticketKey), ticketKey) ?? String(valueOf(argv, 'project') ?? '').toUpperCase();
  const direct = knowledgeRecord(index, project, command);
  const values = knowledgeValues(command, input, argv, ticketKey);
  if (direct) {
    const rows = array(direct.fields?.rows).slice();
    rows.push({ id: randomUUID(), values, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() });
    return { record: direct, updates: { rows }, result: { database: KNOWLEDGE_FILES[command], rowId: rows.at(-1).id, values } };
  }
  const metadata = metadataRecord(index);
  if (!metadata) {
    if (!project) throw new WriteError('Knowledge writes need --project or --ticket when no catalog exists', 'CONFIGURATION', 1);
    const root = boardRootFor(index, project);
    const relativePath = pathJoin(root.path, 'knowledge', `${KNOWLEDGE_FILES[command]}.md`);
    const row = { id: randomUUID(), values, createdAt: new Date().toISOString(), updatedAt: new Date().toISOString() };
    const fields = { kind: 'knowledge', database: KNOWLEDGE_FILES[command], rows: [row], updatedAt: new Date().toISOString() };
    return { newFile: true, path: relativePath, bytes: createTicketBytes(fields, ''), result: { database: KNOWLEDGE_FILES[command], rowId: row.id, values } };
  }
  const tables = clone(metadata.fields?.tables ?? {});
  const databases = array(tables['notion.databases']);
  const title = `Agent · ${KNOWLEDGE_FILES[command][0].toUpperCase()}${KNOWLEDGE_FILES[command].slice(1)}`;
  const database = databases.find(row => String(row?.title ?? '').toLowerCase() === title.toLowerCase()) ?? databases.find(row => String(row?.title ?? '').toLowerCase().includes(KNOWLEDGE_FILES[command]));
  if (!database) throw new WriteError(`No Notion database for ${KNOWLEDGE_FILES[command]} is present in the vault`, 'CONFIGURATION', 1);
  const properties = array(tables['notion.database_properties']).filter(row => String(row?.database_id ?? '') === String(database.id));
  const byName = lowerFieldMap(values);
  const cells = {};
  for (const property of properties) {
    const key = String(property?.name ?? '').toLowerCase().replace(/[ _-]/g, '');
    if (!byName.has(key)) continue;
    let value = byName.get(key);
    if (String(property?.type ?? '').toUpperCase() === 'SELECT') {
      const option = array(property.options).find(item => String(item?.label ?? '').toLowerCase() === String(value).toLowerCase());
      if (option) value = option.id;
    }
    cells[property.id] = value;
  }
  const row = { id: randomUUID(), database_id: database.id, values: cells, created_at: new Date().toISOString(), updated_at: new Date().toISOString() };
  const rows = [...array(tables['notion.database_rows']), row];
  tables['notion.database_rows'] = rows;
  return { record: metadata, updates: { tables }, result: { database: KNOWLEDGE_FILES[command], rowId: row.id, values } };
}

async function knowledge(index, argv, files, command) {
  const input = parseKnowledgeInput(command, argv, files);
  const ticketKey = valueOf(argv, 'ticket');
  const project = String(valueOf(argv, 'project') ?? '').toUpperCase();
  const direct = knowledgeRecord(index, project, command);
  const metadata = direct ?? metadataRecord(index);
  const expected = metadata ? { [metadata.relativePath]: expectedRevision(metadata) } : {};
  return runOperation(index, command, argv, expected, async fresh => {
    const row = appendKnowledgeRow(fresh, command, input, ticketKey, argv);
    if (row.newFile) return { changes: [{ path: row.path, bytes: row.bytes }], result: row.result };
    return { changes: [{ path: row.record.relativePath, bytes: patchRecord(row.record, row.updates) }], result: row.result };
  });
}

function findKnowledgeRow(index, databaseKey, rowRef) {
  const metadata = metadataRecord(index);
  if (!metadata) throw new WriteError('No knowledge catalog is present in the vault', 'CONFIGURATION', 1);
  const tables = metadata.fields?.tables ?? {};
  const databases = array(tables['notion.databases']);
  const title = `Agent · ${databaseKey[0].toUpperCase()}${databaseKey.slice(1)}`;
  const database = databases.find(row => String(row?.title ?? '').toLowerCase() === title.toLowerCase())
    ?? databases.find(row => String(row?.title ?? '').toLowerCase().includes(databaseKey));
  if (!database) throw new WriteError(`No Notion database for ${databaseKey} is present in the vault`, 'EMPTY', 3);
  const rows = array(tables['notion.database_rows']).filter(row => String(row?.database_id ?? '') === String(database.id));
  const matches = rows.filter(row => String(row?.id ?? '').toLowerCase() === String(rowRef).toLowerCase() || String(row?.id ?? '').toLowerCase().startsWith(String(rowRef).toLowerCase()));
  if (matches.length === 0) throw new WriteError(`No row in ${databaseKey} starts with "${rowRef}"`, 'EMPTY', 3);
  if (matches.length > 1) throw new WriteError(`"${rowRef}" matches ${matches.length} rows in ${databaseKey}; use more characters`, 'USAGE', 1);
  return { metadata, tables, database, row: matches[0] };
}

async function resolveRow(index, argv) {
  const command = String(argv[0] ?? '').toLowerCase();
  const database = command === 'suggestions' ? 'suggestions' : positional(argv, 1)?.toLowerCase();
  const rowRef = command === 'suggestions'
    ? positional(argv, 2)
    : (positional(argv, 3) ?? positional(argv, 2));
  const status = command === 'suggestions'
    ? positional(argv, 3)
    : (positional(argv, 4) ?? positional(argv, 3));
  const reason = valueOf(argv, 'reason');
  if (!database || !rowRef || !status) return errorResult('Usage: zagent rows <DATABASE> resolve <ID> <STATUS> --reason TEXT', 1);
  if (!String(reason ?? '').trim()) return errorResult('Resolving a row needs --reason "…".', 1);
  const current = metadataRecord(index);
  const expected = current ? { [current.relativePath]: expectedRevision(current) } : {};
  return runOperation(index, 'rows-resolve', argv, expected, async fresh => {
    const found = findKnowledgeRow(fresh, database, rowRef);
    const properties = array(found.tables['notion.database_properties']).filter(row => String(row?.database_id ?? '') === String(found.database.id));
    const statusProp = properties.find(row => String(row?.name ?? '').toLowerCase() === 'status');
    const resolutionProp = properties.find(row => String(row?.name ?? '').toLowerCase() === 'resolution');
    if (!statusProp) throw new WriteError(`"${database}" has no Status property to move`, 'INVALID', 4);
    const option = array(statusProp.options).find(row => String(row?.label ?? '').toLowerCase() === String(status).toLowerCase());
    if (!option && array(statusProp.options).length) throw new WriteError(`"${status}" is not a Status in ${database}`, 'INVALID', 4);
    const row = { ...found.row, values: { ...object(found.row.values) } };
    row.values[statusProp.id] = option?.id ?? status;
    if (resolutionProp) row.values[resolutionProp.id] = String(reason).trim();
    const nextRows = array(found.tables['notion.database_rows']).map(item => String(item?.id) === String(row.id) ? row : item);
    const tables = { ...found.tables, 'notion.database_rows': nextRows };
    return { changes: [{ path: found.metadata.relativePath, bytes: patchRecord(found.metadata, { tables }) }], result: { rowId: row.id, database, to: status, reason: String(reason).trim() } };
  });
}

export function isWriteCommand(argv = []) {
  const command = String(argv[0] ?? '').toLowerCase();
  if (['next', 'claim', 'comment', 'move', 'finish', 'create', 'link', 'unlink', 'parent', 'update', 'estimate', 'flag', 'resolve', 'ask', 'decide', 'shortfall', 'changelog', 'suggest'].includes(command)) return true;
  if (command === 'sprint') return String(argv[1] ?? 'list').toLowerCase() !== 'list';
  if (command === 'version') return String(argv[1] ?? 'list').toLowerCase() !== 'list';
  if (command === 'rows') return argv.slice(1).some(value => String(value).toLowerCase() === 'resolve');
  if (command === 'suggestions') return String(argv[1] ?? '').toLowerCase() === 'resolve';
  return false;
}

/** Dispatch one local mutation. Returns null for retained read commands. */
export async function dispatchWrite(index, argv = [], files = {}, context = {}) {
  if (!isWriteCommand(argv)) return null;
  if (!writeEnabled(index)) return disabledResult();
  const effectiveArgv = Array.isArray(argv) ? argv.slice() : [];
  // The worker's scratch runner passes its claim token out of band. Keep the
  // command line spelling available for the pure mutation helpers, while
  // preserving an explicit CLI token when callers supplied one themselves.
  if (attemptOf(effectiveArgv) === undefined && context?.attempt !== undefined) {
    effectiveArgv.push('--attempt', String(context.attempt));
  }
  const contextActor = typeof context?.actor === 'object' ? context.actor : null;
  const contextHuman = context?.actor === 'human' || contextActor?.kind === 'human';
  const contextEmail = context?.actorEmail ?? contextActor?.email;
  if (contextHuman && !hasFlag(effectiveArgv, 'human') && valueOf(effectiveArgv, 'actor') === undefined) effectiveArgv.push('--human');
  if (contextEmail !== undefined && valueOf(effectiveArgv, 'actor-email') === undefined) effectiveArgv.push('--actor-email', String(contextEmail));
  if (index && typeof index === 'object') operationInputs.set(index, clone({ files, context }));
  try {
    const command = String(effectiveArgv[0] ?? '').toLowerCase();
    if (command === 'next') return claim(index, effectiveArgv, files, true);
    if (command === 'claim') return claim(index, effectiveArgv, files, false);
    if (command === 'comment') return comment(index, effectiveArgv, files);
    if (command === 'move') return move(index, effectiveArgv);
    if (command === 'finish') return finish(index, effectiveArgv, files);
    if (command === 'create') return create(index, effectiveArgv, files);
    if (command === 'link') return link(index, effectiveArgv, false);
    if (command === 'unlink') return link(index, effectiveArgv, true);
    if (command === 'parent') return parent(index, effectiveArgv);
    if (command === 'sprint') return sprint(index, effectiveArgv);
    if (command === 'version') return version(index, effectiveArgv);
    if (command === 'update') return update(index, effectiveArgv, files);
    if (command === 'estimate' || command === 'flag' || command === 'resolve') return update(index, effectiveArgv, files, command);
    if (['ask', 'decide', 'shortfall', 'changelog', 'suggest'].includes(command)) return knowledge(index, effectiveArgv, files, command);
    if (command === 'rows' || command === 'suggestions') return resolveRow(index, effectiveArgv);
    return null;
  } catch (error) {
    if (error instanceof WriteError) return errorResult(error.message, error.exitCode, error.code);
    return errorResult(error?.message ?? String(error), 1);
  }
}

export {
  appendSection,
  bytesOf,
  expectedRevision,
  findTicket,
  patchRecord,
  replaceSections,
};

export default dispatchWrite;
