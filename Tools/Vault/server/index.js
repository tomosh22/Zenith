import { dedupeDiagnostics, diagnostic, isErrorDiagnostic } from './diagnostics.js';
import { canonicalLinkTarget } from './parser.js';
import { scanVault } from './scanner.js';

function asArray(value) {
  return Array.isArray(value) ? value : [];
}

function pathKey(value) {
  return String(value ?? '').replaceAll('\\', '/').replace(/^\.\//, '');
}

function cloneSetMap(value) {
  return new Map([...value].map(([key, values]) => [key, new Set(values)]));
}

function containsTerm(haystack, term) {
  return haystack.includes(term);
}

function countOccurrences(haystack, term) {
  if (!term) return 0;
  let count = 0;
  let offset = 0;
  while (true) {
    const found = haystack.indexOf(term, offset);
    if (found === -1) break;
    count += 1;
    offset = found + Math.max(1, term.length);
  }
  return count;
}

function recordIdentifier(record) {
  return record.kind === 'ticket' && record.metadata?.key ? record.metadata.key : record.relativePath;
}

function recordSearchText(record) {
  const metadata = record.metadata ?? record.fields ?? {};
  let sourceText;
  try {
    sourceText = JSON.stringify(metadata);
  } catch {
    sourceText = '';
  }
  return [
    record.relativePath,
    record.project,
    metadata.key,
    metadata.title,
    record.bodyText,
    sourceText,
  ].filter(value => value !== undefined && value !== null).join('\n').toLocaleLowerCase();
}

function valueString(value) {
  return value === undefined || value === null ? null : String(value);
}

function addMapSet(map, key, value) {
  if (!key) return;
  if (!map.has(key)) map.set(key, new Set());
  map.get(key).add(value);
}

function hasAnyField(fields, names) {
  return names.some(name => Object.prototype.hasOwnProperty.call(fields ?? {}, name));
}

function fieldValue(fields, metadata, names) {
  for (const name of names) {
    if (Object.prototype.hasOwnProperty.call(fields ?? {}, name) && fields[name] !== undefined) return fields[name];
    if (Object.prototype.hasOwnProperty.call(metadata ?? {}, name) && metadata[name] !== undefined) return metadata[name];
  }
  return undefined;
}

function validateStringField(record, metadata, names, canonicalName, diagnostics, { allowNull = true } = {}) {
  const fields = record.fields ?? {};
  if (!hasAnyField(fields, names)) return;
  const value = fieldValue(fields, metadata, names);
  if (value === null && allowNull) return;
  if (typeof value === 'string' && value.trim() !== '') return;
  diagnostics.push(diagnostic('INVALID_FIELD_TYPE', `Ticket ${metadata.key ?? record.relativePath} has an invalid ${canonicalName}; expected a non-empty string`, {
    path: record.path,
    relativePath: record.relativePath,
    project: record.project,
    field: canonicalName,
    expected: 'string',
    origin: 'index',
  }));
}

function workflowStatusCatalog(records) {
  const result = new Map();
  for (const record of records) {
    if (record.kind !== 'config') continue;
    const fields = record.fields ?? {};
    const entries = [];
    if (Array.isArray(fields.statuses)) entries.push(...fields.statuses);
    // A status catalog exported independently uses rows; a workflow catalog's
    // rows are workflow rows, so only consume rows when they carry a status
    // shaped field or the catalog declares itself as statuses.
    if (Array.isArray(fields.rows) && (String(fields.kind ?? '').toLowerCase() === 'statuses' || fields.rows.some(row => row?.status || row?.status_name))) {
      entries.push(...fields.rows);
    }
    if (!entries.length) continue;
    const project = record.project ?? '*';
    if (!result.has(project)) result.set(project, new Map());
    const statuses = result.get(project);
    for (const entry of entries) {
      if (!entry || typeof entry !== 'object') continue;
      const name = entry.name ?? entry.status ?? entry.status_name ?? entry.title;
      const category = entry.category ?? entry.statusCategory ?? entry.status_category;
      if (name === undefined || category === undefined) continue;
      statuses.set(String(name), String(category).toUpperCase());
    }
  }
  return result;
}

function projectCatalogKey(value) {
  const text = value === undefined || value === null ? '' : String(value).trim();
  return text ? text.toUpperCase() : '*';
}

function addCatalogValues(map, project, values) {
  const key = projectCatalogKey(project);
  if (!map.has(key)) map.set(key, new Set());
  const target = map.get(key);
  for (const value of values) {
    if (value === undefined || value === null || value === '') continue;
    target.add(String(value));
    target.add(String(value).toLowerCase());
  }
}

function rowReferenceValues(row) {
  if (row === undefined || row === null || row === '') return [];
  if (typeof row !== 'object' || Array.isArray(row)) return [row];
  return [
    row.key,
    row.name,
    row.id,
    row.version,
    row.version_id,
    row.versionId,
  ].filter(value => value !== undefined && value !== null && value !== '');
}

function versionCatalog(records, tables) {
  const result = new Map();
  for (const record of records) {
    if (record.kind !== 'config') continue;
    const kind = String(record.fields?.kind ?? '').toLowerCase();
    if (kind !== 'versions' && !record.relativePath?.toLowerCase().endsWith('/versions.md')) continue;
    const rows = asArray(record.fields?.rows);
    for (const row of rows) addCatalogValues(result, record.project, rowReferenceValues(row));
  }

  // A metadata export may be the only catalog available in a hand-authored
  // vault. Resolve its project_id through jira.projects where possible.
  const projects = new Map(asArray(tables.get('jira.projects')).map(row => [
    String(row?.id ?? ''), row?.key,
  ]).filter(([id, key]) => id && key));
  for (const name of ['versions', 'jira.versions', 'jira.issue_versions']) {
    for (const row of asArray(tables.get(name))) {
      const project = row?.project ?? row?.project_key ?? row?.projectKey
        ?? projects.get(String(row?.project_id ?? ''));
      addCatalogValues(result, project, rowReferenceValues(row));
    }
  }
  return result;
}

function identityCatalog(tables, records) {
  const result = new Set();
  const add = row => {
    if (!row || typeof row !== 'object' || Array.isArray(row)) return;
    for (const value of [row.email, row.mail, row.username, row.user_name, row.id]) {
      if (value !== undefined && value !== null && value !== '') result.add(String(value).toLowerCase());
    }
  };
  for (const name of ['public.users', 'users', 'members', 'identities', 'assignees']) {
    for (const row of asArray(tables.get(name))) add(row);
  }
  for (const record of records) {
    if (record.kind !== 'config') continue;
    const kind = String(record.fields?.kind ?? '').toLowerCase();
    if (!['users', 'members', 'identities', 'assignees'].includes(kind)) continue;
    for (const row of asArray(record.fields?.rows)) add(row);
  }
  return result;
}

function linkReferenceValues(value) {
  if (value === undefined || value === null || value === '') return [];
  if (typeof value !== 'object' || Array.isArray(value)) return [value];
  return [
    value.key,
    value.target,
    value.ticket,
    value.ticketKey,
    value.slug,
    value.path,
    value.href,
    value.url,
  ].filter(item => item !== undefined && item !== null && item !== '');
}

function isExternalLink(value) {
  return /^[A-Za-z][A-Za-z0-9+.-]*:/.test(String(value ?? '')) || String(value ?? '').startsWith('#');
}

function normalizeReference(value) {
  if (value === undefined || value === null || value === '') return null;
  return String(value);
}

/**
 * Disposable in-memory representation of a vault snapshot.
 *
 * The class intentionally keeps the original file records, including raw
 * bytes, so display/query consumers can use the index without turning it into
 * an authoring layer. `replaceFile` and `removeFile` are the only mutations
 * and are used by the file watcher; they never write to disk.
 */
export class VaultIndex {
  constructor(snapshot = {}, options = {}) {
    this.options = { ...options };
    this.root = snapshot.root ?? snapshot.vaultRoot;
    this.vaultRoot = this.root;
    this.configPath = snapshot.configPath;
    this.configAbsolutePath = snapshot.configAbsolutePath;
    this.vaultConfig = snapshot.config ?? {};
    this.config = this.vaultConfig;
    this.configRecord = snapshot.configRecord ?? null;
    this.boardRoots = asArray(snapshot.boardRoots).map(value => ({ ...value }));
    this.staticDiagnostics = [];
    this.files = new Map();
    this.records = [];
    this.tickets = new Map();
    this.sprints = new Map();
    this.configs = new Map();
    this.metadataFiles = new Map();
    this.tables = new Map();
    this.catalogs = new Map();
    this.knowledge = new Map();
    this.history = new Map();
    this.documents = new Map();
    this.filesByAbsolutePath = new Map();
    this.absoluteFiles = this.filesByAbsolutePath;
    this.issues = this.tickets;
    this.fileMap = this.files;
    this.knowledgeFiles = this.knowledge;
    this.historyFiles = this.history;
    this.backlinks = new Map();
    this.blockedBy = this.backlinks;
    this.wikilinks = new Map();
    this.outgoingLinks = new Map();
    this.diagnostics = [];
    this.errors = [];
    this.valid = true;
    this.claimable = true;
    this.canClaim = true;
    this.version = 0;

    this.staticDiagnostics = asArray(snapshot.diagnostics).filter(item => item.origin !== 'file');
    if (this.configRecord && this.configRecord.diagnostics?.length) {
      this.staticDiagnostics.push(...this.configRecord.diagnostics);
    }
    for (const record of asArray(snapshot.files ?? snapshot.records)) this.files.set(pathKey(record.relativePath), record);
    this.rebuild();
  }

  rebuild() {
    this.records = [...this.files.values()].sort((a, b) => a.relativePath.localeCompare(b.relativePath));
    this.tickets.clear();
    this.sprints.clear();
    this.configs.clear();
    this.metadataFiles.clear();
    this.tables.clear();
    this.catalogs.clear();
    this.knowledge.clear();
    this.history.clear();
    this.documents.clear();
    this.filesByAbsolutePath.clear();
    this.backlinks.clear();
    this.wikilinks.clear();
    this.outgoingLinks.clear();

    const diagnostics = [...this.staticDiagnostics];
    const ticketGroups = new Map();
    const sprintGroups = new Map();

    for (const record of this.records) {
      this.filesByAbsolutePath.set(record.absolutePath, record);
      if (record.kind === 'ticket') {
        const key = valueString(record.metadata?.key ?? record.key);
        if (key) {
          if (!ticketGroups.has(key)) ticketGroups.set(key, []);
          ticketGroups.get(key).push(record);
          // The first record remains addressable so callers can inspect all
          // data even while the repository is invalid.
          if (!this.tickets.has(key)) this.tickets.set(key, record);
        }
      } else if (record.kind === 'sprint') {
        const slug = valueString(record.metadata?.slug ?? record.slug);
        if (slug) {
          if (!sprintGroups.has(slug)) sprintGroups.set(slug, []);
          sprintGroups.get(slug).push(record);
          if (!this.sprints.has(slug)) this.sprints.set(slug, record);
        }
      } else if (record.kind === 'config') {
        this.configs.set(record.relativePath, record);
        const catalogName = record.fields?.kind ?? record.relativePath;
        this.catalogs.set(String(catalogName), record);
        if (Array.isArray(record.fields?.rows)) {
          this.tables.set(String(catalogName), record.fields.rows);
        }
      } else if (record.kind === 'metadata') {
        this.metadataFiles.set(record.relativePath, record);
        // Metadata catalogs are configuration-like read models as well. Keep
        // them in configs so consumers that only enumerate config records do
        // not silently lose the repository-wide table export.
        this.configs.set(record.relativePath, record);
        for (const [name, rows] of Object.entries(record.fields?.tables ?? {})) {
          this.tables.set(name, Array.isArray(rows) ? rows : []);
        }
      } else if (record.kind === 'knowledge') {
        this.knowledge.set(record.relativePath, record);
      } else if (record.kind === 'history') {
        this.history.set(record.relativePath, record);
      } else {
        this.documents.set(record.relativePath, record);
      }

      diagnostics.push(...asArray(record.diagnostics));
    }

    for (const [key, records] of ticketGroups) {
      if (records.length < 2) continue;
      for (const record of records) {
        diagnostics.push(diagnostic('DUPLICATE_KEY', `Ticket key ${key} is declared by more than one file`, {
          path: record.path,
          relativePath: record.relativePath,
          project: record.project,
          key,
          origin: 'index',
        }));
      }
    }
    for (const [slug, records] of sprintGroups) {
      if (records.length < 2) continue;
      for (const record of records) {
        diagnostics.push(diagnostic('DUPLICATE_SPRINT', `Sprint slug ${slug} is declared by more than one file`, {
          path: record.path,
          relativePath: record.relativePath,
          project: record.project,
          slug,
          origin: 'index',
        }));
      }
    }

    const statusCatalogs = workflowStatusCatalog(this.records);
    const versions = versionCatalog(this.records, this.tables);
    const identities = identityCatalog(this.tables, this.records);
    for (const record of this.records) {
      const id = recordIdentifier(record);
      const metadata = record.metadata ?? {};
      if (record.kind === 'ticket') {
        this._validateTicketMetadata(record, metadata, statusCatalogs, diagnostics);
        this._validateTicketReferences(record, metadata, versions, identities, diagnostics);
        this._deriveStatusCategory(record, metadata, statusCatalogs, diagnostics);
        const parent = normalizeReference(metadata.parent);
        if (parent && !this.tickets.has(parent)) {
          diagnostics.push(this._missingReference(record, 'parent', parent));
        }
        const sprint = normalizeReference(metadata.sprint);
        if (sprint && !this.sprints.has(sprint)) {
          diagnostics.push(this._missingReference(record, 'sprint', sprint));
        }
        for (const block of asArray(metadata.blocks)) {
          const target = normalizeReference(block?.key ?? block?.target);
          const reason = normalizeReference(block?.reason);
          if (block?.invalid || !target || !reason || reason.trim() === '') {
            diagnostics.push(diagnostic('INVALID_BLOCK', `Ticket ${metadata.key ?? record.relativePath} has a blocks entry without a key and reason`, {
              path: record.path,
              relativePath: record.relativePath,
              project: record.project,
              reference: 'blocks',
              target,
              origin: 'index',
            }));
          }
          if (target && !this.tickets.has(target)) {
            diagnostics.push(this._missingReference(record, 'blocks', target));
          }
          if (target) addMapSet(this.backlinks, target, id);
        }
      }

      const rawWikilinks = asArray(record.links?.wikilinks);
      this.outgoingLinks.set(record.relativePath, rawWikilinks.slice());
      for (const rawTarget of rawWikilinks) {
        const target = canonicalLinkTarget(rawTarget);
        if (!target) continue;
        addMapSet(this.wikilinks, rawTarget, id);
        if (target !== rawTarget) addMapSet(this.wikilinks, target, id);
        const basename = target.split('/').at(-1);
        if (basename && basename !== target) addMapSet(this.wikilinks, basename, id);
      }
    }

    this.diagnostics = dedupeDiagnostics(diagnostics);
    this.errors = this.diagnostics.filter(isErrorDiagnostic);
    this.valid = this.errors.length === 0;
    this.claimable = this.valid;
    this.canClaim = this.valid;
    this.version += 1;
    return this;
  }

  _missingReference(record, reference, target) {
    return diagnostic('DANGLING_REFERENCE', `Ticket ${record.metadata?.key ?? record.relativePath} references missing ${reference} ${target}`, {
      path: record.path,
      relativePath: record.relativePath,
      project: record.project,
      reference,
      target,
      origin: 'index',
    });
  }

  _validateTicketMetadata(record, metadata, statusCatalogs, diagnostics) {
    const stringFields = [
      [['status', 'state'], 'status'],
      [['type', 'issue_type', 'issueType'], 'type'],
      [['priority'], 'priority'],
      [['statusCategory', 'status_category', 'statusCategoryName'], 'statusCategory'],
    ];
    for (const [names, canonicalName] of stringFields) {
      validateStringField(record, metadata, names, canonicalName, diagnostics);
    }

    const source = record.fields ?? {};
    if (hasAnyField(source, ['assignee', 'assignee_email', 'assigneeEmail'])) {
      const raw = fieldValue(source, metadata, ['assignee', 'assignee_email', 'assigneeEmail']);
      const valid = raw === null || (typeof raw === 'string' && raw.trim() !== '')
        || (raw && typeof raw === 'object' && !Array.isArray(raw) && typeof raw.email === 'string' && raw.email.trim() !== '');
      if (!valid) {
        diagnostics.push(diagnostic('INVALID_FIELD_TYPE', `Ticket ${metadata.key ?? record.relativePath} has an invalid assignee; expected an email string or null`, {
          path: record.path,
          relativePath: record.relativePath,
          project: record.project,
          field: 'assignee',
          expected: 'email|string|null',
          origin: 'index',
        }));
      }
    }

  }

  _validateTicketReferences(record, metadata, versions, identities, diagnostics) {
    const ticketLabel = metadata.key ?? record.relativePath;
    const project = projectCatalogKey(record.project);
    const versionSet = versions.get(project) ?? versions.get('*');
    for (const reference of asArray(metadata.fixVersions)) {
      const values = rowReferenceValues(reference);
      if (!values.length) continue;
      if (!versionSet?.size) {
        diagnostics.push(this._missingReference(record, 'fixVersions', String(values[0])));
        continue;
      }
      const resolved = values.some(value => versionSet.has(String(value)) || versionSet.has(String(value).toLowerCase()));
      if (resolved) continue;
      const target = String(values[0]);
      diagnostics.push(this._missingReference(record, 'fixVersions', target));
    }

    if (identities.size && metadata.assignee !== null && metadata.assignee !== undefined && metadata.assignee !== '') {
      const raw = fieldValue(record.fields ?? {}, metadata, ['assignee', 'assignee_email', 'assigneeEmail']);
      const values = rowReferenceValues(raw);
      const normalized = values.length ? values : [metadata.assignee];
      const resolved = normalized.some(value => identities.has(String(value).toLowerCase()));
      if (!resolved) diagnostics.push(this._missingReference(record, 'assignee', String(normalized[0])));
    }

    const rawLinks = metadata.links === undefined || metadata.links === null
      ? []
      : (Array.isArray(metadata.links) ? metadata.links : [metadata.links]);
    for (const link of rawLinks) {
      for (const rawTarget of linkReferenceValues(link)) {
        if (isExternalLink(rawTarget)) continue;
        const target = canonicalLinkTarget(rawTarget);
        if (!target) continue;
        const targetKey = target.toUpperCase();
        const fileCandidates = new Set([
          pathKey(rawTarget),
          pathKey(target),
          `${pathKey(target)}.md`,
          `${pathKey(target)}.markdown`,
        ]);
        const resolved = this.tickets.has(target)
          || this.tickets.has(targetKey)
          || [...fileCandidates].some(candidate => this.files.has(candidate));
        if (!resolved) diagnostics.push(this._missingReference(record, 'links', String(rawTarget)));
      }
    }
  }

  _deriveStatusCategory(record, metadata, statusCatalogs, diagnostics) {
    const source = record.fields ?? {};
    const status = fieldValue(source, metadata, ['status', 'state']);
    if (typeof status !== 'string' || status.trim() === '') return;
    const categoryNames = ['statusCategory', 'status_category', 'statusCategoryName'];
    const hasExplicitCategory = hasAnyField(source, categoryNames);
    const explicitCategory = fieldValue(source, metadata, categoryNames);
    const catalog = statusCatalogs.get(record.project)
      ?? statusCatalogs.get(String(record.project ?? '').toUpperCase())
      ?? statusCatalogs.get(String(record.project ?? '').toLowerCase())
      ?? statusCatalogs.get('*');
    if (!catalog?.size) return;
    const expectedCategory = catalog.get(status);
    if (!expectedCategory) {
      // Do not carry a previously derived/explicit category forward when the
      // status itself is unknown. The raw frontmatter still preserves the
      // stale value for diagnostics, while the canonical read model fails
      // closed instead of presenting an unknown status as DONE.
      metadata.statusCategory = null;
      record.statusCategory = null;
      diagnostics.push(diagnostic('UNKNOWN_STATUS', `Ticket ${metadata.key ?? record.relativePath} uses status ${status}, which is absent from its workflow`, {
        path: record.path,
        relativePath: record.relativePath,
        project: record.project,
        field: 'status',
        value: status,
        origin: 'index',
      }));
      return;
    }
    const hasUsableExplicitCategory = hasExplicitCategory
      && explicitCategory !== null
      && explicitCategory !== undefined
      && explicitCategory !== '';
    if (hasUsableExplicitCategory && typeof explicitCategory === 'string' && explicitCategory.toUpperCase() !== expectedCategory) {
      diagnostics.push(diagnostic('STATUS_CATEGORY_MISMATCH', `Ticket ${metadata.key ?? record.relativePath} status ${status} belongs to ${expectedCategory}, not ${explicitCategory}`, {
        path: record.path,
        relativePath: record.relativePath,
        project: record.project,
        field: 'statusCategory',
        value: explicitCategory,
        expected: expectedCategory,
        origin: 'index',
      }));
    }
    // The workflow is authoritative for this derived field. The source map
    // and record.fields remain untouched, so an editor can still report the
    // exact bytes/explicit value that caused a mismatch.
    metadata.statusCategory = expectedCategory;
    record.statusCategory = expectedCategory;
  }

  replaceFile(record) {
    if (!record?.relativePath) throw new TypeError('replaceFile requires a vault record');
    this.files.set(pathKey(record.relativePath), record);
    return this.rebuild();
  }

  updateFile(record) {
    return this.replaceFile(record);
  }

  removeFile(filePath) {
    const key = pathKey(filePath);
    const existing = this.files.get(key);
    this.files.delete(key);
    this.rebuild();
    return existing;
  }

  getFile(filePath) {
    if (!filePath) return undefined;
    const key = pathKey(filePath);
    return this.files.get(key) ?? this.filesByAbsolutePath.get(filePath);
  }

  getTicket(key) {
    return this.tickets.get(String(key));
  }

  getSprint(slug) {
    return this.sprints.get(String(slug));
  }

  getBacklinks(target) {
    const key = String(target ?? '');
    const ids = this.backlinks.get(key) ?? new Set();
    return [...ids].map(id => this.tickets.get(id) ?? this.files.get(id)).filter(Boolean);
  }

  backlinksFor(target) {
    return this.getBacklinks(target);
  }

  getWikilinkSources(target) {
    const key = String(target ?? '');
    const ids = this.wikilinks.get(key) ?? this.wikilinks.get(canonicalLinkTarget(key)) ?? new Set();
    return [...ids].map(id => this.tickets.get(id) ?? this.files.get(id)).filter(Boolean);
  }

  wikilinksFor(target) {
    return this.getWikilinkSources(target);
  }

  /**
   * Search all indexed text and metadata. Results are records, ordered by a
   * small deterministic relevance score and then relative path. Use
   * `searchDetailed` when scores/snippets are needed.
   */
  search(query, options = {}) {
    const terms = String(query ?? '').trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
    if (terms.length === 0) return [];
    const kind = options.kind;
    const project = options.project;
    const limit = Number.isFinite(options.limit) ? Math.max(0, options.limit) : Infinity;
    const result = [];
    for (const record of this.records) {
      if (kind && record.kind !== kind) continue;
      if (project && record.project !== project) continue;
      const text = recordSearchText(record);
      if (!terms.every(term => containsTerm(text, term))) continue;
      let score = 0;
      for (const term of terms) {
        score += countOccurrences(text, term);
        const key = String(record.metadata?.key ?? '').toLocaleLowerCase();
        const title = String(record.metadata?.title ?? '').toLocaleLowerCase();
        if (key.includes(term)) score += 10;
        if (title.includes(term)) score += 5;
      }
      result.push({ record, score });
    }
    result.sort((a, b) => b.score - a.score || a.record.relativePath.localeCompare(b.record.relativePath));
    return result.slice(0, limit).map(item => item.record);
  }

  textSearch(query, options = {}) {
    return this.search(query, options);
  }

  searchDetailed(query, options = {}) {
    const records = this.search(query, { ...options, limit: Infinity });
    const terms = String(query ?? '').trim().toLocaleLowerCase().split(/\s+/).filter(Boolean);
    const limit = Number.isFinite(options.limit) ? Math.max(0, options.limit) : Infinity;
    return records.slice(0, limit).map(record => {
      const text = recordSearchText(record);
      return {
        record,
        score: terms.reduce((sum, term) => sum + countOccurrences(text, term), 0),
        matches: terms.filter(term => text.includes(term)),
      };
    });
  }

  queryTickets(filters = {}) {
    let values = [...this.tickets.values()];
    for (const [field, expected] of Object.entries(filters)) {
      if (expected === undefined) continue;
      values = values.filter(record => {
        const actual = record.metadata?.[field] ?? record[field];
        if (Array.isArray(actual)) return actual.includes(expected);
        return actual === expected;
      });
    }
    return values;
  }

  getClaimableTickets(filters = {}) {
    return this.queryTickets(filters).filter(record => record.kind === 'ticket' && record.claimable !== false && !record.isDraft);
  }

  isTicketClaimable(key) {
    const record = this.getTicket(key);
    return Boolean(this.valid && record && record.kind === 'ticket' && record.claimable !== false && !record.isDraft);
  }

  getTable(name) {
    const rows = this.tables.get(String(name));
    return rows ? rows.slice() : [];
  }

  diagnosticsFor(filePath) {
    const key = pathKey(filePath);
    return this.diagnostics.filter(item => pathKey(item.relativePath ?? item.path) === key || item.path === filePath);
  }

  toJSON() {
    return {
      root: this.root,
      boardRoots: this.boardRoots,
      valid: this.valid,
      claimable: this.claimable,
      diagnostics: this.diagnostics,
      tickets: [...this.tickets.values()].map(record => record.metadata),
      sprints: [...this.sprints.values()].map(record => record.metadata),
      files: this.records.map(record => ({
        relativePath: record.relativePath,
        project: record.project,
        kind: record.kind,
        hash: record.hash,
      })),
    };
  }
}

export function createVaultIndex(snapshot, options = {}) {
  return new VaultIndex(snapshot, options);
}

export const createIndex = createVaultIndex;
export const buildIndex = createVaultIndex;

export async function loadVault(vaultRoot, options = {}) {
  const snapshot = await scanVault(vaultRoot, options);
  return createVaultIndex(snapshot, options);
}

export default VaultIndex;
