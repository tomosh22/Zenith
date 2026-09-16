/*
 * Read-side compatibility helpers for the former zagent dispatcher.
 *
 * The board used to assemble these values from several SQL joins.  The vault
 * keeps the same policy in vault.config.json and keeps issue columns in ticket
 * frontmatter.  These helpers intentionally have no filesystem, database, or
 * network side effects.  They also keep canonical frontmatter separate from
 * the nested `source` snapshot: source is useful for legacy identity fields,
 * but it must never overwrite an edited canonical field.
 */

import { evaluateContract as legacyEvaluateContract } from './legacy-pure/contract.mjs';
import {
  citationsIn as legacyCitationsIn,
  parseTicket as legacyParseTicket,
  reviewDecision as legacyReviewDecision,
} from './legacy-pure/ticket.mjs';
import {
  effectiveGates as legacyEffectiveGates,
  resolveRouting as legacyResolveRouting,
  resolveRules as legacyResolveRules,
} from './legacy-pure/routing.mjs';

const DEFAULT_ORG_SLUG = 'pink-goat';
const DEFAULT_AGENT_EMAIL = 'agent@saas.invalid';

export const EXIT_CODES = Object.freeze({
  OK: 0,
  CONFIGURATION: 1,
  // The legacy dispatcher uses one for usage, unknown commands, and
  // unsupported/deferred operations.  Keep that distinction at the payload
  // level; callers branch on the same stable process code.
  USAGE: 1,
  NOT_OWNED: 5,
});

export function clone(value) {
  if (value === undefined) return undefined;
  try {
    return structuredClone(value);
  } catch {
    return JSON.parse(JSON.stringify(value));
  }
}

function object(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function array(value) {
  return Array.isArray(value) ? value.slice() : [];
}

/** Return the canonical metadata namespace for a scanned ticket record. */
export function canonicalOf(record) {
  return object(record?.metadata ?? record?.fields ?? record);
}

/**
 * Exported tickets carry the SQL issue row below one or two `source` keys:
 * normaliseTicket retains the entire frontmatter map as `metadata.source`,
 * which itself contains the exporter `source` map.  Find that row without
 * treating it as authoritative for canonical fields.
 */
export function sourceOf(record) {
  const metadata = canonicalOf(record);
  const first = object(metadata.source);
  if (first.source && typeof first.source === 'object' && !Array.isArray(first.source)) {
    return first.source;
  }
  if (first.id !== undefined || first.number !== undefined || first.project_id !== undefined) return first;
  const fields = object(record?.fields);
  const nested = object(fields.source);
  if (nested.source && typeof nested.source === 'object' && !Array.isArray(nested.source)) return nested.source;
  if (nested.id !== undefined || nested.number !== undefined || nested.project_id !== undefined) return nested;
  return object(record?.source);
}

function configRoot(index) {
  return object(index?.vaultConfig ?? index?.config);
}

/**
 * Read a source-preserving metadata table without making metadata.md part of
 * the ticket model.  The index exposes the table map directly in current
 * snapshots, while hand-authored fixtures may only expose metadata records.
 */
function metadataRows(index, name) {
  const direct = index?.tables instanceof Map
    ? index.tables.get(name)
    : index?.tables?.[name];
  if (Array.isArray(direct)) return direct;
  const records = [];
  if (index?.metadataFiles instanceof Map) records.push(...index.metadataFiles.values());
  if (index?.files instanceof Map) records.push(...index.files.values());
  if (Array.isArray(index?.metadataFiles)) records.push(...index.metadataFiles);
  for (const record of records) {
    const value = record?.metadata ?? record?.fields ?? record;
    const rows = value?.tables?.[name];
    if (Array.isArray(rows)) return rows;
  }
  return [];
}

/**
 * Resolve the organization from source identities and the mapped board roots.
 * The old dispatcher emitted a slug in every URL, so silently hardcoding the
 * fixture's current organization would make a copied vault point at the
 * wrong board.  A tiny fallback remains for minimal unit fixtures that have
 * no relational metadata at all.
 */
function organizationSlugOf(index, projects) {
  const organizations = metadataRows(index, 'public.organizations');
  const projectRows = metadataRows(index, 'jira.projects');
  if (!organizations.length || !projectRows.length) return undefined;
  const mapped = new Set(Object.keys(projects ?? {}).map(key => String(key).toUpperCase()));
  const project = projectRows.find(row => mapped.has(String(row?.key ?? '').toUpperCase()))
    ?? projectRows.find(row => String(row?.key ?? '').toUpperCase() === String(index?.boardRoots?.[0]?.project ?? '').toUpperCase());
  if (!project) return undefined;
  const organization = organizations.find(row => String(row?.id ?? '') === String(project.organization_id ?? project.organizationId ?? ''))
    ?? organizations.find(row => String(row?.slug ?? '').toLowerCase() === 'pink-goat');
  return organization?.slug ?? organization?.name;
}

/** Resolve the exported policy regardless of whether callers pass vault or fixture config. */
export function policyOf(index) {
  const root = configRoot(index);
  const policy = object(root.policy);
  const direct = Object.keys(policy).length ? policy : root;
  const resolved = object(direct.resolved ?? root.resolved);
  const client = object(direct.client ?? root.client);
  const clientFile = object(client.file);
  const board = object(direct.board ?? root.board);
  const boardGuardrails = object(board.guardrails ?? direct.guardrails ?? root.guardrails);
  const globalProjects = object(resolved.projects ?? direct.projects ?? root.projects);
  const categories = object(clientFile.categories ?? client.categories ?? direct.categories ?? root.categories);

  return {
    root,
    policy: direct,
    resolved,
    board,
    boardGuardrails,
    client,
    clientFile,
    projects: globalProjects,
    categories,
    agentEmail: direct.agentEmail ?? board.agentEmail ?? resolved.agentEmail ?? DEFAULT_AGENT_EMAIL,
    orgSlug: direct.orgSlug ?? root.orgSlug ?? organizationSlugOf(index, globalProjects) ?? DEFAULT_ORG_SLUG,
  };
}

/** Config shape expected by the vendored pure helpers. */
export function legacyConfigOf(index) {
  const policy = policyOf(index);
  if (Object.keys(policy.resolved).length) return policy.resolved;
  // A test may provide the pinned policy directly instead of wrapping it in
  // vault.config.json. Keep this fallback deliberately small and immutable.
  return {
    routing: clone(policy.board.routing ?? {}),
    guardrails: clone(policy.boardGuardrails),
    projects: clone(policy.projects),
  };
}

export function projectKeyOf(record, fallbackKey) {
  const metadata = canonicalOf(record);
  const key = String(metadata.key ?? fallbackKey ?? record?.key ?? '');
  const fromKey = /^([A-Za-z][A-Za-z0-9_]*)-(\d+)$/.exec(key)?.[1];
  return String(record?.project ?? fromKey ?? '').toUpperCase() || fromKey?.toUpperCase() || null;
}

function mergeObjects(base, overlay) {
  return { ...object(base), ...object(overlay) };
}

/** Return project policy with a ticket category overlay. */
export function projectPolicy(index, projectKey, category) {
  const policy = policyOf(index);
  const base = object(policy.projects[projectKey]);
  // In the resolved export each project inherits the same category map.  The
  // fallback also supports a small hand-authored fixture with only client.file.
  const categoryMap = object(base.categories);
  const categoryPolicy = category ? object(categoryMap[category] ?? policy.categories[category]) : {};
  const merged = { ...base, ...categoryPolicy };
  merged.merge = mergeObjects(base.merge, categoryPolicy.merge);
  merged.docs = mergeObjects(base.docs, categoryPolicy.docs);
  return { policy, project: merged, category: categoryPolicy };
}

export function knownProjectKeys(index) {
  const policy = policyOf(index);
  const keys = Object.keys(policy.projects);
  if (keys.length) return keys;
  return (Array.isArray(index?.boardRoots) ? index.boardRoots : [])
    .map(root => String(root?.project ?? '').toUpperCase())
    .filter(Boolean);
}

export function mappingError(index, projectKey) {
  const known = knownProjectKeys(index);
  return `Project ${projectKey} is not mapped in agent.config.json. Known: ${known.join(', ')}`;
}

export function ticketFor(index, key) {
  const value = String(key ?? '');
  if (typeof index?.getTicket === 'function') return index.getTicket(value);
  if (index?.tickets instanceof Map) return index.tickets.get(value);
  if (index?.issues instanceof Map) return index.issues.get(value);
  if (index?.tickets && typeof index.tickets === 'object') return index.tickets[value];
  if (index?.issues && typeof index.issues === 'object') return index.issues[value];
  return undefined;
}

export function allTicketRecords(index) {
  if (index?.tickets instanceof Map) return [...index.tickets.values()];
  if (index?.issues instanceof Map) return [...index.issues.values()];
  if (Array.isArray(index?.tickets)) return index.tickets.slice();
  if (Array.isArray(index?.issues)) return index.issues.slice();
  return [];
}

export function sprintFor(index, slug) {
  if (!slug) return undefined;
  if (typeof index?.getSprint === 'function') return index.getSprint(slug);
  if (index?.sprints instanceof Map) return index.sprints.get(String(slug));
  if (index?.sprints && typeof index.sprints === 'object') return index.sprints[String(slug)];
  return undefined;
}

function projectHasActiveStage(index, projectKey) {
  const wanted = String(projectKey ?? '').toUpperCase();
  if (!wanted) return false;
  const sprints = index?.sprints instanceof Map ? [...index.sprints.values()] : [];
  return sprints.some(record => {
    const metadata = canonicalOf(record);
    const source = sourceOf(record);
    const owner = String(record?.project ?? source.project_key ?? source.projectKey ?? '').toUpperCase();
    return owner === wanted
      && String(metadata.status ?? source.status ?? '').toUpperCase() === 'ACTIVE'
      && !(metadata.continuous ?? source.continuous ?? false);
  });
}

function isDoneTicket(ticket) {
  const metadata = canonicalOf(ticket);
  return String(metadata.statusCategory ?? '').toUpperCase() === 'DONE'
    || String(metadata.status ?? '').toUpperCase() === 'DONE';
}

export function sprintPayload(index, slug, projectKey) {
  if (!slug) return null;
  const record = sprintFor(index, slug);
  const metadata = object(record?.metadata ?? record?.fields ?? record);
  if (!record && !Object.keys(metadata).length) return null;
  const source = object(metadata.source?.source ?? metadata.source);
  const status = metadata.status ?? source.status ?? null;
  const continuous = metadata.continuous ?? source.continuous ?? false;
  const inScope = metadata.inScope ?? (projectHasActiveStage(index, projectKey)
    ? status === 'ACTIVE' && !continuous
    : status === 'ACTIVE' || continuous);
  return {
    name: metadata.name ?? metadata.title ?? source.name ?? String(slug),
    status,
    continuous: Boolean(continuous),
    inScope: Boolean(inScope),
  };
}

export function labelsOf(ticket) {
  return array(canonicalOf(ticket).labels).map(value => String(value));
}

export function semanticFlags(ticket) {
  const labels = new Set(labelsOf(ticket).map(label => label.toLowerCase()));
  return {
    humanGate: labels.has('human-gate'),
    needsHuman: labels.has('needs-human'),
    needsGpu: labels.has('needs-gpu'),
    deferred: labels.has('deferred'),
  };
}

export function blocksOf(ticket) {
  const blocks = canonicalOf(ticket).blocks;
  return array(blocks).map(entry => {
    if (typeof entry === 'string' || typeof entry === 'number') return { key: String(entry), reason: null };
    const value = object(entry);
    return {
      ...clone(value),
      key: value.key ?? value.target ?? value.ticket ?? value.ticketKey ?? null,
      reason: value.reason ?? value.why ?? value.description ?? null,
    };
  });
}

export function blockedBy(index, key) {
  const target = String(key ?? '');
  const result = [];
  for (const record of allTicketRecords(index)) {
    const source = canonicalOf(record);
    const sourceKey = String(source.key ?? record?.key ?? '');
    if (!sourceKey || sourceKey === target) continue;
    for (const block of blocksOf(record)) {
      if (String(block.key ?? '') === target) result.push({ source: record, block });
    }
  }
  // The legacy query's result order follows the numeric issue number.  A
  // plain string sort would put FD-100 before FD-50 and change both the
  // queue row and the human-readable blocker contract.
  result.sort((a, b) => {
    const left = String(canonicalOf(a.source).key ?? '');
    const right = String(canonicalOf(b.source).key ?? '');
    const leftNumber = Number(/-(\d+)$/.exec(left)?.[1]);
    const rightNumber = Number(/-(\d+)$/.exec(right)?.[1]);
    if (Number.isFinite(leftNumber) && Number.isFinite(rightNumber) && leftNumber !== rightNumber) {
      return leftNumber - rightNumber;
    }
    return left.localeCompare(right);
  });
  return result;
}

export function outgoingLinks(ticket) {
  const metadata = canonicalOf(ticket);
  const links = [];
  for (const block of blocksOf(ticket)) links.push({ ...block, type: 'BLOCKS', label: 'blocks' });
  for (const link of array(metadata.links)) {
    const value = typeof link === 'string' ? { key: link } : object(link);
    links.push({
      ...clone(value),
      key: value.key ?? value.target ?? value.ticket ?? value.ticketKey ?? null,
      type: value.type ?? 'RELATES_TO',
      label: value.label ?? String(value.type ?? 'relates to').toLowerCase(),
    });
  }
  return links.filter(link => link.key);
}

export function idAndNumber(ticket) {
  const metadata = canonicalOf(ticket);
  const source = sourceOf(ticket);
  const key = String(metadata.key ?? ticket?.key ?? '');
  const numberMatch = /-(\d+)$/.exec(key);
  const numberValue = metadata.number ?? source.number ?? (numberMatch ? Number(numberMatch[1]) : null);
  return {
    id: metadata.id ?? source.id ?? null,
    number: typeof numberValue === 'string' && /^\d+$/.test(numberValue) ? Number(numberValue) : numberValue,
  };
}

/**
 * Build the small relation projection consumed by the legacy contract
 * checker.  `linkedIssuesOf` on the board returned incoming BLOCKS rows,
 * every relation as a generic linked row, and the parent row.  Keep that
 * distinction here so phrases such as "the blocking bug" remain type and
 * direction checked after the move to Markdown.
 */
export function linkedOf(index, ticket) {
  const targetKey = String(canonicalOf(ticket).key ?? ticket?.key ?? '');
  const byId = new Map();
  for (const candidate of allTicketRecords(index)) {
    const { id } = idAndNumber(candidate);
    if (id !== null && id !== undefined) byId.set(String(id), candidate);
  }
  const linked = [];
  const add = (relation, candidate) => {
    if (!candidate) return;
    const metadata = canonicalOf(candidate);
    const key = String(metadata.key ?? candidate.key ?? '');
    if (!key || key === targetKey) return;
    linked.push({ relation, key, type: String(metadata.type ?? '').toUpperCase() });
  };
  const rows = metadataRows(index, 'jira.issue_links');
  const id = idAndNumber(ticket).id;
  for (const row of rows) {
    const source = byId.get(String(row?.source_issue_id ?? row?.sourceIssueId ?? ''));
    const target = byId.get(String(row?.target_issue_id ?? row?.targetIssueId ?? ''));
    if (!source || !target) continue;
    const sourceKey = String(canonicalOf(source).key ?? '');
    const targetRowKey = String(canonicalOf(target).key ?? '');
    const type = String(row?.type ?? '').toUpperCase();
    if (targetRowKey === targetKey && type === 'BLOCKS') add('blocks', source);
    if (sourceKey === targetKey) add('linked', target);
    if (targetRowKey === targetKey) add('linked', source);
  }
  if (rows.length === 0) {
    for (const block of blocksOf(ticket)) add('linked', ticketFor(index, block.key));
    for (const link of array(canonicalOf(ticket).links)) {
      const value = typeof link === 'string' ? link : link?.key ?? link?.target ?? link?.ticket;
      add('linked', ticketFor(index, value));
    }
    for (const item of blockedBy(index, targetKey)) add('blocks', item.source);
  }
  const parent = canonicalOf(ticket).parent;
  if (parent) add('parent', ticketFor(index, parent));
  return linked;
}

export function bodyOf(ticket) {
  // A Markdown body is the only authoritative description.  In particular,
  // do not fall back to source.description: the exporter deliberately removes
  // that column from the source snapshot.
  return typeof ticket?.bodyText === 'string'
    ? ticket.bodyText
    : typeof ticket?.body === 'string'
      ? ticket.body
      : '';
}

function section(body, heading) {
  const lines = String(body ?? '').split(/\r?\n/);
  const wanted = new RegExp(`^##\\s+${heading.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\s*$`, 'i');
  const start = lines.findIndex(line => wanted.test(line.trim()));
  if (start < 0) return '';
  const end = lines.slice(start + 1).findIndex(line => /^##\s+/.test(line.trim()));
  const content = end < 0 ? lines.slice(start + 1) : lines.slice(start + 1, start + 1 + end);
  return content.join('\n').trim();
}

export function goalOf(ticket) {
  const metadata = canonicalOf(ticket);
  if (typeof metadata.goal === 'string') return metadata.goal;
  return legacyParseTicket(bodyOf(ticket)).goal;
}

export function definitionOfDoneOf(ticket) {
  const metadata = canonicalOf(ticket);
  if (Array.isArray(metadata.definitionOfDone)) return clone(metadata.definitionOfDone);
  return legacyParseTicket(bodyOf(ticket)).definitionOfDone;
}

function pathLike(value) {
  if (!value || /\s/.test(value) && !/[./\\]/.test(value)) return false;
  return /[./\\]/.test(value) || /\.md$|\.cpp$|\.h$|\.ps1$|\.yml$|\.yaml$|\.json$|\.zscen$/i.test(value);
}

export function citationsOf(ticket) {
  const metadata = canonicalOf(ticket);
  if (metadata.citations && typeof metadata.citations === 'object') return clone(metadata.citations);
  return legacyCitationsIn(bodyOf(ticket));
}

export function routingOf(index, ticket, projectKey, category) {
  const config = legacyConfigOf(index);
  const complexity = String(canonicalOf(ticket).complexity ?? 'MODERATE').toUpperCase();
  const risk = String(canonicalOf(ticket).risk ?? 'LOW').toUpperCase();
  try {
    return clone(legacyResolveRouting(config, complexity, risk));
  } catch {
    const boardRouting = object(config.routing);
    return {
      model: boardRouting.modelByComplexity?.[complexity] ?? 'sonnet',
      effort: boardRouting.effortByComplexity?.[complexity] ?? 'medium',
    };
  }
}

export function gatesOf(index, ticket, projectKey, category) {
  const metadata = canonicalOf(ticket);
  const config = legacyConfigOf(index);
  try {
    const parsed = legacyParseTicket(bodyOf(ticket));
    return clone(legacyResolveRules({ config, projectKey, category, ticketGates: parsed.gates }));
  } catch {
    const { project, category: categoryPolicy } = projectPolicy(index, projectKey, category);
    const categoryGates = Array.isArray(categoryPolicy.gates) ? categoryPolicy.gates : null;
    const projectGates = Array.isArray(project.gates) ? project.gates : [];
    return { gatesSource: categoryGates ? 'category' : 'project', gates: clone(categoryGates ?? projectGates) };
  }
}

/**
 * Select the ticket's base gates and union category gates selected by the
 * caller's changed-file set.  `effectiveGates` is intentionally invoked even
 * when the set is empty so its `base`/`unioned` contract stays explicit.
 */
export function gateSelectionOf(index, ticket, projectKey, category, changedPaths = []) {
  const config = legacyConfigOf(index);
  const paths = array(changedPaths).map(value => String(value)).filter(Boolean);
  try {
    const parsed = legacyParseTicket(bodyOf(ticket));
    const base = legacyResolveRules({
      config,
      projectKey,
      category,
      ticketGates: parsed.gates,
    });
    const selected = legacyEffectiveGates({
      config,
      projectKey,
      category,
      baseGates: base.gates,
      changedPaths: paths,
    });
    return {
      gatesSource: base.gatesSource,
      gates: selected.gates,
      base: selected.base,
      unioned: selected.unioned,
    };
  } catch {
    const base = gatesOf(index, ticket, projectKey, category);
    return {
      gatesSource: base.gatesSource,
      gates: clone(base.gates ?? []),
      base: clone(base.gates ?? []),
      unioned: [],
    };
  }
}

export function guardrailsOf(index, ticket) {
  const metadata = canonicalOf(ticket);
  const { policy, project } = projectPolicy(index, projectKeyOf(ticket), metadata.category);
  const global = policy.boardGuardrails;
  const protectedPaths = array(project.protectedPaths ?? global.protectedPaths ?? policy.clientFile.protectedPaths);
  const timeoutMap = object(global.ticketTimeoutMinutes);
  const complexity = String(metadata.complexity ?? 'MODERATE').toUpperCase();
  const timeout = project.ticketTimeoutMinutes ?? timeoutMap[complexity] ?? 30;
  return {
    protectedPaths: clone(protectedPaths),
    ticketTimeoutMinutes: timeout,
    fixForwardAttempts: project.fixForwardAttempts ?? global.fixForwardAttempts ?? 3,
    maxConsecutiveBlocked: project.maxConsecutiveBlocked ?? global.maxConsecutiveBlocked ?? 3,
  };
}

export function reviewOf(index, ticket, projectKey, category) {
  const rules = gatesOf(index, ticket, projectKey, category);
  const { project, policy } = projectPolicy(index, projectKey, category);
  const reviewOn = array(rules.reviewOn ?? project.reviewOn ?? policy.clientFile.reviewOn);
  try {
    return clone(legacyReviewDecision({
      reviewOn,
      risk: canonicalOf(ticket).risk,
      definitionOfDone: legacyParseTicket(bodyOf(ticket)).definitionOfDone,
    }));
  } catch {
    const risk = canonicalOf(ticket).risk;
    return { required: reviewOn.includes(risk), reasons: reviewOn.includes(risk) ? [`risk ${risk} is in reviewOn`] : [] };
  }
}

export function projectField(index, projectKey, category, field, fallback = null) {
  const { project, category: categoryPolicy, policy } = projectPolicy(index, projectKey, category);
  const value = categoryPolicy[field] ?? project[field] ?? policy.clientFile[field];
  return value === undefined ? fallback : clone(value);
}

export function isMappedProject(index, projectKey) {
  const keys = knownProjectKeys(index).map(key => key.toUpperCase());
  return keys.includes(String(projectKey).toUpperCase());
}

export function contractErrorFor(index, ticket, projectKey, category, options = {}) {
  const metadata = canonicalOf(ticket);
  const blockers = blockedBy(index, metadata.key ?? ticket?.key)
    .filter(item => String(canonicalOf(item.source).status ?? '') !== 'Done')
    .map(item => ({
      key: canonicalOf(item.source).key,
      status: canonicalOf(item.source).status,
      reason: item.block.reason,
    }));
  try {
    const evaluation = legacyEvaluateContract({
      config: legacyConfigOf(index),
      projectKey,
      categoryName: category,
      body: bodyOf(ticket),
      labels: labelsOf(ticket),
      issueType: metadata.type,
      blockers,
      linked: options.linked ?? linkedOf(index, ticket),
    });
    return evaluation.contractError;
  } catch (error) {
    // Keep a useful fail-closed result for hand-authored indexes that do not
    // carry a complete resolved policy. Production exports always use the
    // vendored path above.
    if (options.invalidRepository) return options.invalidRepository;
    if (error?.message) return error.message;
    return 'Unable to evaluate the ticket contract';
  }
}

export function evaluateContract(index, ticket, projectKey = projectKeyOf(ticket), category = canonicalOf(ticket).category, options = {}) {
  const metadata = canonicalOf(ticket);
  const blockers = blockedBy(index, metadata.key ?? ticket?.key)
    .filter(item => String(canonicalOf(item.source).status ?? '') !== 'Done')
    .map(item => ({ key: canonicalOf(item.source).key, status: canonicalOf(item.source).status, reason: item.block.reason }));
  try {
    const evaluation = legacyEvaluateContract({
      config: legacyConfigOf(index),
      projectKey,
      categoryName: category,
      body: bodyOf(ticket),
      labels: labelsOf(ticket),
      issueType: metadata.type,
      blockers,
      linked: options.linked ?? linkedOf(index, ticket),
    });
    const error = options.invalidRepository ?? evaluation.contractError;
    return { valid: error === null, error, evaluation };
  } catch {
    const error = contractErrorFor(index, ticket, projectKey, category, options);
    return { valid: error === null, error };
  }
}

export function queueReason(index, ticket, projectKey = projectKeyOf(ticket), category = canonicalOf(ticket).category, options = {}) {
  const metadata = canonicalOf(ticket);
  const key = String(metadata.key ?? ticket?.key ?? '');
  const type = String(metadata.type ?? '').toUpperCase();
  if (options.invalidRepository) return options.invalidRepository;
  if (type === 'EPIC') {
    // Targeted checks report every contract reason.  In particular an
    // uncategorized epic in a project with category routing must name both
    // the epic refusal and the missing category, just like `show` does.
    const contract = evaluateContract(index, ticket, projectKey, category, options);
    return contract.error ?? 'This is an EPIC — a container, not a unit of work. Its Definition of Done is satisfied by its children finishing, so there is nothing here for a worker to edit and no gate that could tell you it had. Claim a child (`zagent epic <KEY>` lists them), or convert it to a STORY/TASK if the work really does live on this row.';
  }
  const contract = evaluateContract(index, ticket, projectKey, category, options);
  if (!contract.valid) return contract.error;
  if (metadata.status !== 'To Do') return `it is in ${metadata.status}, and the queue reads To Do`;
  const assignee = metadata.assignee;
  if (assignee) return `it is assigned to ${assignee}`;
  const flags = semanticFlags(ticket);
  if (flags.deferred) return 'it carries `deferred` — the queue skips it; claiming it by name is how you overrule the timing';
  if (flags.humanGate) return 'it carries `human-gate` — the queue skips it; claiming it by name is how you overrule the timing';
  const blocking = blockedBy(index, key).filter(item => String(canonicalOf(item.source).status ?? '') !== 'Done');
  if (blocking.length) return contractErrorFor(index, ticket, projectKey, category, options);
  const sprint = sprintPayload(index, metadata.sprint, projectKey);
  if (sprint && !sprint.inScope) return `it is in ${sprint.name} (${sprint.status}), not the active sprint`;
  return null;
}

export function queueOutOfScope(index, ticket) {
  const metadata = canonicalOf(ticket);
  const projectKey = projectKeyOf(ticket, metadata.key);
  const activeStage = projectHasActiveStage(index, projectKey);
  const sprint = sprintPayload(index, metadata.sprint, projectKey);
  // When a non-continuous ACTIVE stage exists, the old queue admits only
  // that stage.  A ticket with no sprint has no SprintRef to carry `inScope`,
  // but it is still outside the active stage and must be reported as such.
  if (!sprint) return activeStage;
  return !sprint.inScope;
}

export function queueEntry(index, ticket, options = {}) {
  const metadata = canonicalOf(ticket);
  const projectKey = projectKeyOf(ticket, metadata.key);
  const category = metadata.category ?? null;
  const flags = semanticFlags(ticket);
  const blocking = blockedBy(index, metadata.key).filter(item => !isDoneTicket(item.source));
  const contract = evaluateContract(index, ticket, projectKey, category, options);
  const reason = queueReason(index, ticket, projectKey, category, options);
  const assigned = Boolean(metadata.assignee);
  const isEpic = String(metadata.type ?? '').toUpperCase() === 'EPIC';
  const outOfScopeSprint = queueOutOfScope(index, ticket);
  return {
    key: String(metadata.key ?? ticket?.key ?? ''),
    title: metadata.title ?? null,
    status: metadata.status ?? null,
    priority: metadata.priority ?? null,
    complexity: metadata.complexity ?? null,
    risk: metadata.risk ?? null,
    category,
    type: metadata.type ?? null,
    sprint: sprintPayload(index, metadata.sprint, projectKey),
    assigned,
    isEpic,
    parent: metadata.parent ?? null,
    outOfScopeSprint,
    needsHuman: flags.needsHuman,
    needsGpu: flags.needsGpu,
    deferred: flags.deferred,
    humanGate: flags.humanGate,
    blockedBy: blocking
      .map(item => canonicalOf(item.source).key)
      .filter(Boolean),
    // Queue claimability mirrors the queue query's structural predicates.
    // Contract validity is evaluated separately by targeted `check`; the
    // legacy queue intentionally listed malformed/uncategorized rows so a
    // human could see and repair them.
    claimable: !assigned
      && !isEpic
      && !outOfScopeSprint
      && !flags.needsHuman
      && !flags.deferred
      && blocking.length === 0,
  };
}

export function priorityRank(value) {
  const rank = { CRITICAL: 0, HIGH: 1, MEDIUM: 2, LOW: 3, TRIVIAL: 4 };
  return rank[String(value ?? '').toUpperCase()] ?? 99;
}

export function sortQueueEntries(entries, index) {
  return entries.slice().sort((a, b) => priorityRank(a.priority) - priorityRank(b.priority)
    || (Number(canonicalOf(ticketFor(index, a.key))?.position ?? 0) - Number(canonicalOf(ticketFor(index, b.key))?.position ?? 0))
    || String(a.key).localeCompare(String(b.key)));
}

export function showPayload(index, ticket, projectKey = projectKeyOf(ticket), options = {}) {
  const metadata = canonicalOf(ticket);
  const category = metadata.category ?? null;
  const { id, number } = idAndNumber(ticket);
  const flags = semanticFlags(ticket);
  const policy = projectPolicy(index, projectKey, category);
  const contract = evaluateContract(index, ticket, projectKey, category, options);
  const rules = contract.evaluation?.rules ?? gatesOf(index, ticket, projectKey, category);
  const parsed = contract.evaluation?.parsed ?? legacyParseTicket(bodyOf(ticket));
  const project = policy.project;
  const branchMode = project.branching ?? 'direct';
  const branchPrefix = project.branchPrefix ?? 'agent/';
  const guardrails = guardrailsOf(index, ticket);
  const reviewOn = array(rules.reviewOn ?? project.reviewOn ?? policy.policy.clientFile.reviewOn);
  const payload = {
    key: String(metadata.key ?? ticket?.key ?? ''),
    id,
    number,
    title: metadata.title ?? null,
    type: metadata.type ?? null,
    priority: metadata.priority ?? null,
    complexity: metadata.complexity ?? null,
    risk: metadata.risk ?? null,
    category,
    labels: labelsOf(ticket),
    humanGate: flags.humanGate,
    needsHuman: flags.needsHuman,
    needsGpu: flags.needsGpu,
    deferred: flags.deferred,
    sprint: sprintPayload(index, metadata.sprint, projectKey),
    reporterId: metadata.reporterId ?? sourceOf(ticket).reporter_id ?? null,
    routing: routingOf(index, ticket, projectKey, category),
    contractValid: contract.valid,
    contractError: contract.error,
    reachabilityWarning: contract.evaluation?.reachabilityWarning ?? null,
    boardRefWarning: contract.evaluation?.boardRefWarning ?? null,
    markerWarning: contract.evaluation?.markerWarning ?? null,
    gates: clone(rules.gates ?? []),
    gatesSource: rules.gatesSource ?? 'project',
    conventions: clone(rules.conventions ?? project.conventions ?? policy.policy.clientFile.conventions ?? ''),
    cleanup: clone(rules.cleanup ?? project.cleanup ?? policy.policy.clientFile.cleanup ?? []),
    reviewOn: clone(reviewOn),
    review: reviewOf(index, ticket, projectKey, category),
    workerTools: clone(rules.workerTools ?? project.workerTools ?? policy.policy.clientFile.workerTools ?? 'Read,Edit,Write,Grep,Glob'),
    repo: rules.repo ?? project.repo ?? policy.policy.clientFile.repo ?? null,
    baseBranch: rules.baseBranch ?? project.baseBranch ?? policy.policy.clientFile.baseBranch ?? null,
    branching: rules.branching ?? branchMode,
    branch: (rules.branching ?? branchMode) === 'branch' ? `${rules.branchPrefix ?? branchPrefix}${metadata.key}` : null,
    push: rules.push ?? project.merge?.push ?? project.push ?? policy.policy.clientFile.merge?.push ?? false,
    projectKey,
    orgSlug: policy.policy.orgSlug,
    issueUrl: metadata.issueUrl ?? sourceOf(ticket).issue_url ?? `http://localhost:3001/${policy.policy.orgSlug}/project/${projectKey}/issue/${number}`,
    bodyPath: metadata.bodyPath ?? '',
    guardrails,
    goal: parsed.goal ?? goalOf(ticket),
    definitionOfDone: clone(parsed.definitionOfDone ?? definitionOfDoneOf(ticket)),
    citations: citationsOf(ticket),
    body: bodyOf(ticket),
    status: metadata.status ?? null,
  };
  const decisionLog = rules.decisionLog ?? policy.category.decisionLog ?? project.decisionLog;
  const decisionIdPrefix = rules.decisionIdPrefix ?? policy.category.decisionIdPrefix ?? project.decisionIdPrefix;
  if (decisionLog !== undefined) payload.decisionLog = clone(decisionLog);
  if (decisionIdPrefix !== undefined) payload.decisionIdPrefix = clone(decisionIdPrefix);
  return payload;
}
