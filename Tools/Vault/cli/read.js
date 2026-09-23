import {
  EXIT_CODES,
  allTicketRecords,
  blockedBy,
  canonicalOf,
  clone,
  contractErrorFor,
  evaluateContract,
  gateSelectionOf,
  guardrailsOf,
  isMappedProject,
  mappingError,
  projectKeyOf,
  policyOf,
  projectPolicy,
  queueEntry,
  queueReason,
  semanticFlags,
  showPayload,
  sortQueueEntries,
  ticketFor,
} from './compat.mjs';
import { dispatchLocal } from './local.mjs';
import { inspectArguments } from './arguments.mjs';
import { dispatchTransitions } from './transitions.mjs';
import { dispatchReports } from './reports.mjs';
import { dispatchQuery } from './queries.mjs';
import { dispatchHistoryRead as dispatchHistory } from './history.mjs';

const ISSUE_KEY = /^[A-Za-z][A-Za-z0-9_]*-\d+$/;

function result(exitCode, payload) {
  // Payloads are deliberately detached from the index. A caller may sort or
  // annotate a response without changing the watcher's in-memory records.
  return { exitCode, payload: clone(payload) };
}

function option(argv, name) {
  const args = Array.isArray(argv) ? argv : [];
  const exact = args.indexOf(name);
  if (exact >= 0 && exact + 1 < args.length) return args[exact + 1];
  const prefix = args.find(value => typeof value === 'string' && value.startsWith(`${name}=`));
  return prefix ? prefix.slice(name.length + 1) : undefined;
}

function commandKey(argv) {
  const value = Array.isArray(argv) ? argv.find(item => ISSUE_KEY.test(String(item ?? ''))) : undefined;
  return value === undefined ? undefined : String(value);
}

function ticketKey(index, requested) {
  const key = String(requested ?? '');
  const direct = ticketFor(index, key);
  if (direct) return { key, ticket: direct };
  const upper = key.toUpperCase();
  if (upper !== key) {
    const normalized = ticketFor(index, upper);
    if (normalized) return { key: upper, ticket: normalized };
  }
  // Hand-authored test indexes are allowed to use a case-insensitive map.
  const found = allTicketRecords(index).find(record => String(canonicalOf(record).key ?? record?.key ?? '').toUpperCase() === upper);
  return found ? { key: String(canonicalOf(found).key ?? found.key), ticket: found } : { key, ticket: undefined };
}

function mappedTicket(index, requested) {
  const resolved = ticketKey(index, requested);
  const projectKey = projectKeyOf(resolved.ticket, resolved.key);
  if (!projectKey || !isMappedProject(index, projectKey)) {
    return { ...resolved, projectKey, error: mappingError(index, projectKey ?? String(requested ?? '').split('-')[0]) };
  }
  if (!resolved.ticket) return { ...resolved, projectKey, error: `Issue ${resolved.key} was not found` };
  return { ...resolved, projectKey, error: null };
}

function requestedTicket(index, argv, command, invalidMessage) {
  const requested = commandKey(argv) ?? argv?.[1];
  if (requested === undefined || requested === null || String(requested) === '') {
    return {
      requested,
      failure: result(EXIT_CODES.USAGE, { error: `Usage: zagent ${command} <TICKET_KEY>` }),
    };
  }
  const text = String(requested);
  if (!ISSUE_KEY.test(text)) {
    return {
      requested: text,
      failure: result(EXIT_CODES.USAGE, { error: invalidMessage(text) }),
    };
  }
  const resolved = mappedTicket(index, text);
  if (resolved.error) {
    // A valid key in a mapped project has a stable not-found exit.  Unknown
    // project keys retain the configuration diagnostic used by the legacy
    // dispatcher (including isolated audit projects).
    if (!resolved.ticket && resolved.projectKey && isMappedProject(index, resolved.projectKey)) {
      return {
        requested: text,
        resolved,
        failure: result(3, { error: `No ticket ${text}.` }),
      };
    }
    return { requested: text, resolved, failure: result(EXIT_CODES.CONFIGURATION, { error: resolved.error }) };
  }
  return { requested: text, resolved };
}

function invalidRepositoryReason(index) {
  if (index?.valid !== false && !(Array.isArray(index?.errors) && index.errors.length)) return null;
  const diagnostic = Array.isArray(index?.errors) && index.errors[0];
  const detail = diagnostic?.message ?? diagnostic?.code ?? 'one or more vault files are invalid';
  const path = diagnostic?.relativePath ?? diagnostic?.path;
  return path ? `Vault is invalid: ${path}: ${detail}` : `Vault is invalid: ${detail}`;
}

/**
 * The local equivalent of the board's one-ticket-per-repository predicate.
 *
 * A watched index is only a cache.  The caller that needs a write obtains a
 * fresh index under the operation lock; read callers can use this helper on
 * their current index and get the same predicate.  Agent identities are
 * configuration data rather than a hard-coded single email so a vault may
 * explicitly allow more than one local runner.
 */
function agentEmails(index) {
  const policy = policyOf(index);
  const root = policy.root ?? {};
  const candidates = [
    policy.agentEmail,
    policy.policy?.agentEmail,
    policy.policy?.board?.agentEmail,
    root.writes?.agentEmail,
    ...(Array.isArray(root.writes?.agentEmails) ? root.writes.agentEmails : []),
    ...(Array.isArray(policy.policy?.agentEmails) ? policy.policy.agentEmails : []),
    ...(Array.isArray(policy.policy?.board?.agentEmails) ? policy.policy.board.agentEmails : []),
    ...(Array.isArray(root.agentEmails) ? root.agentEmails : []),
  ];
  return new Set(candidates.filter(value => value !== undefined && value !== null && String(value).trim() !== '')
    .map(value => String(value).toLowerCase()));
}

function writesEnabled(index) {
  return policyOf(index).root?.writes?.enabled === true;
}

function updatedAtOf(record) {
  const metadata = canonicalOf(record);
  const source = metadata.source && typeof metadata.source === 'object' ? metadata.source : {};
  const raw = metadata.updatedAt ?? metadata.updated_at ?? record?.fields?.updatedAt ?? record?.fields?.updated_at
    ?? source.updatedAt ?? source.updated_at;
  const value = raw === undefined || raw === null ? NaN : Date.parse(String(raw));
  return Number.isFinite(value) ? value : 0;
}

function scopeBlocked(record) {
  const metadata = canonicalOf(record);
  const labels = Array.isArray(metadata.labels) ? metadata.labels.map(value => String(value).toLowerCase()) : [];
  return metadata.blockedScope === true
    || metadata.blocked_scope === true
    || String(metadata.blockedKind ?? metadata.blocked_kind ?? metadata.blockReasonKind ?? '').toLowerCase() === 'scope'
    || labels.includes('scope')
    || labels.includes('blocked-scope')
    || labels.includes('blocked_scope')
    // `integrate` parks a REGATE-exhausted or board-file-conflicted lane
    // ticket the same way a scope block is parked: it was not a failure of
    // the WORK, so it must not count toward maxConsecutiveBlocked any more
    // than blocked-scope does. See the write.mjs refusal that stops an agent
    // applying this label itself outside a lane row.
    || labels.includes('integration-conflict');
}

/**
 * Durable circuit-breaker decision for the mutable local loop.  It only
 * activates when writes are explicitly enabled, keeping exported snapshots
 * read-only and preserving their historical check answers.  Terminal rows
 * are scoped to the selected project and configured agent identities, then
 * ordered by the canonical updated timestamp.  A scope block is reported but
 * deliberately breaks a run of failure blocks.
 */
export function breakerOpen(index, project, limit = undefined) {
  if (!writesEnabled(index)) return { open: false, recent: [], limit: 0 };
  const wanted = String(project ?? '').toUpperCase();
  const policy = projectPolicy(index, wanted, null);
  const guardrails = guardrailsOf(index, null);
  const configured = limit ?? policy.project?.maxConsecutiveBlocked ?? guardrails.maxConsecutiveBlocked ?? 3;
  const threshold = Number(configured);
  if (!Number.isFinite(threshold) || threshold <= 0) return { open: false, recent: [], limit: threshold };
  const emails = agentEmails(index);
  const terminal = allTicketRecords(index)
    .filter(record => projectKeyOf(record, canonicalOf(record).key) === wanted)
    .filter(record => {
      const metadata = canonicalOf(record);
      const status = String(metadata.status ?? '').toLowerCase();
      const category = String(metadata.statusCategory ?? '').toUpperCase();
      return (status === 'done' || status === 'blocked' || category === 'DONE' || category === 'TODO')
        && emails.has(String(metadata.assignee ?? '').toLowerCase());
    })
    .sort((a, b) => updatedAtOf(b) - updatedAtOf(a)
      || String(canonicalOf(b).key ?? '').localeCompare(String(canonicalOf(a).key ?? '')))
    .slice(0, threshold);
  const open = terminal.length >= threshold && terminal.every(record => {
    const metadata = canonicalOf(record);
    const status = String(metadata.status ?? '').toLowerCase();
    return status === 'blocked' && !scopeBlocked(record);
  });
  return { open, recent: terminal.map(record => String(canonicalOf(record).key ?? record.key ?? record.relativePath)), limit: threshold };
}

/**
 * Return the first agent-owned In Progress ticket that blocks a claim in the
 * given `scope` — `'main'` (the default, and every pre-lanes caller) or a
 * lane's project key.
 *
 * I5 rescoped for parallel `/tick` lanes: `main` still conflicts with
 * EVERYTHING (today's one-ticket-per-repo behaviour, lane or no lane), but a
 * lane conflicts only with its OWN in-flight rows and with main-scope rows
 * (no `lane:` at all, or an explicitly cleared one) — never with another
 * lane's. `exceptKey` keeps the existing self-exclusion: an agent re-claiming
 * its own crashed In-Progress ticket must not block on itself (main-mode I6
 * recovery depends on this).
 */
export function inFlightTicket(index, scope = 'main', exceptKey = undefined) {
  const emails = agentEmails(index);
  const except = String(exceptKey ?? '').toUpperCase();
  const wantedScope = String(scope ?? 'main');
  const isMainScope = wantedScope.toLowerCase() === 'main';
  const wantedProject = wantedScope.toUpperCase();
  return allTicketRecords(index)
    .filter(record => {
      const metadata = canonicalOf(record);
      if (String(metadata.status ?? '') !== 'In Progress') return false;
      if (!emails.has(String(metadata.assignee ?? '').toLowerCase())) return false;
      if (String(metadata.key ?? record.key ?? '').toUpperCase() === except) return false;
      if (isMainScope) return true;
      const rowLane = String(metadata.lane ?? '').toUpperCase();
      return rowLane === '' || rowLane === wantedProject;
    })
    .sort((a, b) => String(canonicalOf(a).key ?? '').localeCompare(String(canonicalOf(b).key ?? '')))[0];
}

/**
 * Shared claim/check decision.  `claim` uses it immediately before preparing
 * its byte patch and `check` uses the same function for its dry-run answer.
 * It deliberately keeps the queue and targeted predicates separate: naming a
 * ticket is allowed to overrule queue timing (sprint/deferred), while the
 * repository in-flight guard applies to both paths.
 */
export function claimDecision(index, ticket, { queue = false } = {}) {
  const metadata = canonicalOf(ticket);
  const projectKey = projectKeyOf(ticket, metadata.key);
  const category = metadata.category ?? null;
  const invalidRepository = invalidRepositoryReason(index);
  // I5 rescoped: a lane's scope is its OWN project — `index.lane` is the one
  // carrier a mutation-time `prepare` sees (runOperation wraps it onto
  // `fresh`; see write.mjs). Every pre-lanes caller has no `index.lane` at
  // all, so `scope` defaults to `'main'` and behaves exactly as before.
  const scope = index?.lane?.project ?? 'main';
  const holding = inFlightTicket(index, scope, metadata.key);
  const holdingKey = holding ? String(canonicalOf(holding).key ?? holding.key) : null;
  const scopeLabel = scope === 'main' ? 'main checkout' : `lane ${scope}`;
  const lockReason = holdingKey
    ? `${projectKey ?? 'This repository'} already has a ticket in flight: ${holdingKey} (${scopeLabel}). One ticket per repo at a time.`
    : null;
  const breaker = breakerOpen(index, projectKey);
  const breakerReason = breaker.open
    ? `${projectKey ?? 'This repository'} circuit breaker is open after ${breaker.limit} consecutive failure blocks: ${breaker.recent.join(', ')}`
    : null;
  const contract = evaluateContract(index, ticket, projectKey, category, { invalidRepository });
  const assignee = metadata.assignee;
  const emails = agentEmails(index);
  const assignedToOther = assignee !== undefined && assignee !== null && String(assignee) !== ''
    && !emails.has(String(assignee).toLowerCase());
  const targetedReason = invalidRepository
    ?? breakerReason
    ?? lockReason
    ?? (assignedToOther ? `it is assigned to ${assignee}` : null)
    ?? contract.error;
  const queueError = invalidRepository
    ?? breakerReason
    ?? lockReason
    ?? queueReason(index, ticket, projectKey, category, { invalidRepository });
  // The same verdict with the I5 repo lock left OUT. The lock is TRANSIENT --
  // it clears when the ticket in flight finishes -- while a contract failure,
  // an EPIC, a needs-human label or an assignment to somebody else is still
  // true afterwards. `claim`/`next` must keep reporting the lock first (they
  // MUTATE, and refusing to steal is the point), but a read-only `check` that
  // reports only "repo busy" hides the durable answer the caller asked for.
  // See ZEN-76.
  const durableReason = invalidRepository
    ?? (assignedToOther ? `it is assigned to ${assignee}` : null)
    ?? contract.error;
  return {
    queue: { would: queueError === null, reason: queueError },
    targeted: { would: targetedReason === null, reason: targetedReason },
    durable: { blocked: durableReason !== null, reason: durableReason },
    inFlight: holdingKey,
    breaker,
    queueRequested: Boolean(queue),
  };
}

function mappedShow(index, requested, options = {}) {
  const resolved = mappedTicket(index, requested);
  if (resolved.error) {
    if (!resolved.ticket && resolved.projectKey && isMappedProject(index, resolved.projectKey)) {
      return result(3, { error: `No ticket ${resolved.key}.` });
    }
    return result(EXIT_CODES.CONFIGURATION, { error: resolved.error });
  }
  return result(EXIT_CODES.OK, showPayload(index, resolved.ticket, resolved.projectKey, options));
}

function checkPayload(index, ticket, projectKey, options = {}) {
  const metadata = canonicalOf(ticket);
  const category = metadata.category ?? null;
  const invalidRepository = invalidRepositoryReason(index);
  const contract = evaluateContract(index, ticket, projectKey, category, { ...options, invalidRepository });
  const payload = showPayload(index, ticket, projectKey, { ...options, invalidRepository });
  // Keep the contract result in sync if a repository-wide diagnostic blocks a
  // claim. Display still exposes the Markdown-backed ticket fields.
  payload.contractValid = contract.valid;
  payload.contractError = contract.error;
  payload.runnable = contract.valid;
  const decision = claimDecision(index, ticket);
  payload.check = {
    queue: decision.queue,
    targeted: decision.targeted,
    durable: decision.durable,
    inFlight: decision.inFlight,
  };
  if (decision.breaker?.limit > 0) payload.check.breaker = decision.breaker;
  // ★ THE DURABLE VERDICT WINS OVER THE TRANSIENT LOCK — for `check` only.
  //
  // `check` is read-only and answers "would a claim take this ticket?". With
  // the lock tested first it answered "repo busy" for an EPIC, a needs-human
  // ticket or a contract failure, i.e. it reported the one obstacle that will
  // go away and hid the one that will not. Measured during the /tick audit:
  // `zagent check ZEN-1` on an EPIC returned 5 while a ticket was in flight,
  // so the tick protocol's own documented behaviour ("an EPIC comes back exit
  // 4 from `zagent check`") was false whenever it mattered.
  //
  // `claim` and `next` are untouched and still return 5 first: they mutate,
  // and refusing to steal an in-flight ticket outranks everything.
  const exitCode = decision.breaker?.open
    ? 6
    : decision.targeted.would
    ? EXIT_CODES.OK
    : decision.durable?.blocked
    ? 4
    : decision.inFlight && decision.inFlight !== metadata.key
      ? EXIT_CODES.NOT_OWNED
      : 4;
  return result(exitCode, payload);
}

function dispatchQueue(index, argv) {
  // Lane mode: a bare `queue` (no `--project`) is scoped to the lane's own
  // project, mirroring `queueCandidate`'s (write.mjs) injection for a bare
  // `next`. An explicit `--project` — including another project's key —
  // still wins: `queue` is a read, and reading another board from a lane is
  // legitimate (unlike `claim`/`next`, which stay refused outright). Outside
  // lane mode `index.lane` is undefined and this is a no-op.
  const laneProject = index?.lane?.project;
  const requestedProject = option(argv, '--project');
  const scopedArgv = requestedProject === undefined && laneProject
    ? [...(Array.isArray(argv) ? argv : []), '--project', laneProject]
    : argv;
  const selection = projectSelection(index, scopedArgv);
  if (selection.failure) return selection.failure;
  const projectKey = selection.projectKey;
  const invalidRepository = invalidRepositoryReason(index);
  const records = allTicketRecords(index)
    .filter(record => {
      const metadata = canonicalOf(record);
      const recordProject = projectKeyOf(record, metadata.key);
      return (!projectKey || recordProject === projectKey) && metadata.status === 'To Do';
    })
    .map(record => queueEntry(index, record, { invalidRepository }));
  return result(EXIT_CODES.OK, sortQueueEntries(records, index));
}

function dispatchOwns(index, argv) {
  const requested = commandKey(argv) ?? argv?.[1];
  if (requested === undefined || requested === null || String(requested) === '') {
    return result(EXIT_CODES.USAGE, { error: 'Usage: zagent owns <TICKET_KEY>' });
  }
  if (!ISSUE_KEY.test(String(requested))) {
    return result(EXIT_CODES.USAGE, { error: `"${requested}" is not a ticket key — expected <PROJECT>-<number>.` });
  }
  const requestedText = String(requested);
  const resolved = ticketKey(index, requested);
  const metadata = canonicalOf(resolved.ticket);
  const policy = projectPolicy(index, projectKeyOf(resolved.ticket, resolved.key), metadata.category);
  const configuredAgents = agentEmails(index);
  const agentEmail = policy.policy.agentEmail;
  const assignee = metadata.assignee;
  // Ownership is a status-and-assignee predicate.  An assigned To Do/Done
  // row is not in flight, so treating the assignee alone as ownership would
  // let a later invocation continue after the board's cancel gesture.
  const owned = Boolean(
    assignee
      && (configuredAgents.size
        ? configuredAgents.has(String(assignee).toLowerCase())
        : agentEmail
          && String(assignee).toLowerCase() === String(agentEmail).toLowerCase())
      && String(metadata.status ?? '') === 'In Progress'
  );
  return result(owned ? EXIT_CODES.OK : EXIT_CODES.NOT_OWNED, { key: requestedText, owned });
}

function dispatchGates(index, argv, files = {}) {
  const ticketRequest = requestedTicket(
    index,
    argv,
    'gates',
    value => `"${value}" is not a ticket key — expected <PROJECT>-<number>.`,
  );
  if (ticketRequest.failure) return ticketRequest.failure;
  const requested = ticketRequest.requested;
  const resolved = ticketRequest.resolved;
  const metadata = canonicalOf(resolved.ticket);
  // `files` is accepted for compatibility with the old dispatcher. Reading a
  // supplied snapshot is safe; the read-only vault never writes a scratch file.
  const changed = files && typeof files === 'object' && typeof files.file === 'string'
    ? files.file.split(/\r?\n/).map(value => value.trim()).filter(Boolean).sort((a, b) => a.localeCompare(b))
    : [];
  const selected = gateSelectionOf(index, resolved.ticket, resolved.projectKey, metadata.category ?? null, changed);
  return result(EXIT_CODES.OK, {
    key: String(metadata.key ?? resolved.key),
    category: metadata.category ?? null,
    gatesSource: selected.gatesSource,
    gates: selected.gates,
    base: selected.base,
    unioned: selected.unioned,
    changed,
  });
}

function metadataTables(index) {
  const tables = {};
  if (index?.tables instanceof Map) {
    for (const [name, rows] of index.tables) {
      if (Array.isArray(rows)) tables[name] = rows;
    }
  } else if (index?.tables && typeof index.tables === 'object') {
    for (const [name, rows] of Object.entries(index.tables)) {
      if (Array.isArray(rows)) tables[name] = rows;
    }
  }
  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  if (Array.isArray(index?.metadataFiles)) records.push(...index.metadataFiles);
  for (const record of records) {
    const value = record?.metadata ?? record?.fields ?? record;
    if (value?.tables && typeof value.tables === 'object') Object.assign(tables, value.tables);
  }
  return tables;
}

function projectSelection(index, argv) {
  const requested = option(argv, '--project');
  const configured = policyOf(index).clientFile.project;
  const projectKey = String(requested ?? configured ?? '').toUpperCase() || undefined;
  if (requested === undefined || projectKey === undefined) return { projectKey };

  // The relational project catalogue includes isolated audit projects which
  // are intentionally absent from the resolved agent mapping. Read queries
  // may still select those projects; only an unknown catalogue key is an
  // error.
  const rows = metadataTables(index)['jira.projects'];
  const known = Array.isArray(rows) && rows.length
    ? rows.some(row => String(row?.key ?? row?.project_key ?? '').toUpperCase() === projectKey)
    : isMappedProject(index, projectKey);
  return known
    ? { projectKey }
    : { projectKey, failure: result(EXIT_CODES.CONFIGURATION, { error: `No Jira project with key ${projectKey}.` }) };
}

function isDoneRecord(record) {
  const metadata = canonicalOf(record);
  return String(metadata.statusCategory ?? '').toUpperCase() === 'DONE'
    || String(metadata.status ?? '').toUpperCase() === 'DONE';
}

function issueNumber(key) {
  const value = Number(/-(\d+)$/.exec(String(key ?? ''))?.[1]);
  return Number.isFinite(value) ? value : Number.MAX_SAFE_INTEGER;
}

function dispatchBlocked(index, argv) {
  const selection = projectSelection(index, argv);
  if (selection.failure) return selection.failure;
  const projectKey = selection.projectKey;
  const output = allTicketRecords(index)
    .filter(ticket => {
      const metadata = canonicalOf(ticket);
      const blockers = blockedBy(index, metadata.key).filter(item => !isDoneRecord(item.source));
      return (!projectKey || projectKeyOf(ticket, metadata.key) === projectKey) && blockers.length > 0;
    })
    .map(ticket => {
      const metadata = canonicalOf(ticket);
      const blockers = blockedBy(index, metadata.key).filter(item => !isDoneRecord(item.source));
      return {
        key: metadata.key,
        title: metadata.title ?? null,
        status: metadata.status ?? null,
        blockedBy: blockers.map(item => canonicalOf(item.source).key).filter(Boolean),
        reasons: blockers.map(item => item.block.reason ?? null),
      };
    })
    .sort((a, b) => issueNumber(a.key) - issueNumber(b.key) || String(a.key).localeCompare(String(b.key)));
  return result(EXIT_CODES.OK, output);
}

/**
 * Dispatch a retained, read-only zagent command against an already-scanned
 * VaultIndex. The function is synchronous so unit callers can use it without
 * a second event loop; the process entry point awaits it harmlessly.
 */
export function dispatchRead(index, argv = [], files = {}) {
  const args = Array.isArray(argv) ? argv.filter(value => value !== '--json') : [];
  const command = String(args[0] ?? '').toLowerCase();
  try {
    const inspected = inspectArguments(args);
    if (inspected.help) {
      const help = dispatchLocal(index, ['help'], files, { dispatchRead });
      return result(help?.exitCode ?? EXIT_CODES.OK, help?.payload ?? { error: 'Help is unavailable' });
    }
    if (inspected.error) return result(EXIT_CODES.USAGE, { error: inspected.error });
    const local = dispatchLocal(index, args, files, { dispatchRead });
    if (local) return result(local.exitCode, local.payload);
    if (command === 'transitions') {
      const transitions = dispatchTransitions(index, ['transitions', ...args.slice(1)]);
      if (transitions) return result(transitions.exitCode, transitions.payload);
    }
    if (command === 'epic' || command === 'report') {
      const reports = dispatchReports(index, [command, ...args.slice(1)]);
      if (reports) return result(reports.exitCode, reports.payload);
    }
    const query = dispatchQuery(index, [command, ...args.slice(1)], files);
    if (query) return result(query.exitCode, query.payload);
    switch (command) {
      case 'show': {
        const ticketRequest = requestedTicket(
          index,
          args,
          'show',
          value => `"${value}" is not a ticket key — expected <PROJECT>-<number>.`,
        );
        if (ticketRequest.failure) return ticketRequest.failure;
        return mappedShow(index, ticketRequest.requested, { files });
      }
      case 'check': {
        const ticketRequest = requestedTicket(
          index,
          args,
          'check',
          value => `"${value}" is not a ticket key — expected <PROJECT>-<number>.`,
        );
        if (ticketRequest.failure) return ticketRequest.failure;
        const resolved = ticketRequest.resolved;
        return checkPayload(index, resolved.ticket, resolved.projectKey, { files });
      }
      case 'gates':
        return dispatchGates(index, args, files);
      case 'queue':
        return dispatchQueue(index, args);
      case 'owns':
        return dispatchOwns(index, args);
      case 'blocked':
        return dispatchBlocked(index, args);
      case 'history':
        return dispatchHistory(index, args);
      default:
        return result(EXIT_CODES.USAGE, { error: `Unknown read command: ${command || '(none)'}` });
    }
  } catch (error) {
    return result(EXIT_CODES.USAGE, { error: error?.message ?? String(error) });
  }
}

export {
  checkPayload,
  dispatchBlocked,
  dispatchGates,
  dispatchHistory,
  dispatchQueue,
  mappedShow,
};

export default dispatchRead;
