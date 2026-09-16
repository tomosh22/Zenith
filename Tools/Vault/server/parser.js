import { parseDocument } from 'yaml';

import { diagnostic } from './diagnostics.js';

const UTF8_BOM = '\uFEFF';
const YAML_OPEN = '---';
const YAML_CLOSE = new Set(['---', '...']);

function asBuffer(value) {
  if (Buffer.isBuffer(value)) return Buffer.from(value);
  if (value instanceof Uint8Array) return Buffer.from(value);
  if (typeof value === 'string') return Buffer.from(value, 'utf8');
  throw new TypeError('Markdown input must be a Buffer, Uint8Array or string');
}

function readLine(bytes, start) {
  const newline = bytes.indexOf(0x0A, start);
  const end = newline === -1 ? bytes.length : newline;
  const contentEnd = end > start && bytes[end - 1] === 0x0D ? end - 1 : end;
  return {
    start,
    end: contentEnd,
    next: newline === -1 ? bytes.length : newline + 1,
    text: bytes.subarray(start, contentEnd).toString('utf8'),
  };
}

function markerText(line) {
  return line.text.replace(/^\uFEFF/, '').trim();
}

function errorLocation(error, sourceText) {
  const pos = Array.isArray(error?.pos) ? error.pos[0] : Number.isInteger(error?.pos) ? error.pos : undefined;
  if (pos === undefined) return {};
  const before = sourceText.slice(0, pos);
  const line = before.split(/\r?\n/).length;
  const lastBreak = Math.max(before.lastIndexOf('\n'), before.lastIndexOf('\r'));
  return { line, column: pos - lastBreak };
}

function yamlDiagnostic(error, sourceText, filePath) {
  const location = errorLocation(error, sourceText);
  const code = String(error?.code ?? 'YAML_PARSE_ERROR').toUpperCase();
  return diagnostic(code === 'DUPLICATE_KEY' ? 'DUPLICATE_KEY' : 'YAML_PARSE_ERROR', error?.message ?? String(error), {
    path: filePath,
    ...location,
    yamlCode: code,
    origin: 'file',
  });
}

function parseYaml(text, filePath) {
  const diagnostics = [];
  let document;
  let fields = {};

  try {
    // uniqueKeys is explicit because accepting the last value would make a
    // cold scan dependent on the YAML library's map behaviour.
    document = parseDocument(text, {
      prettyErrors: false,
      strict: true,
      uniqueKeys: true,
      keepSourceTokens: true,
      version: '1.2',
    });

    for (const error of document.errors ?? []) diagnostics.push(yamlDiagnostic(error, text, filePath));
    for (const warning of document.warnings ?? []) {
      diagnostics.push(diagnostic('YAML_WARNING', warning.message ?? String(warning), {
        severity: 'warning',
        path: filePath,
        ...errorLocation(warning, text),
        origin: 'file',
      }));
    }

    if (diagnostics.some(item => item.severity !== 'warning')) {
      return { document, fields: {}, diagnostics };
    }

    const value = document.toJS({ mapAsMap: false });
    if (value === null || value === undefined) {
      fields = {};
    } else if (typeof value !== 'object' || Array.isArray(value)) {
      diagnostics.push(diagnostic('FRONTMATTER_NOT_MAPPING', 'YAML frontmatter must contain a mapping of fields', {
        path: filePath,
        origin: 'file',
      }));
    } else {
      fields = value;
    }
  } catch (error) {
    // yaml can throw while converting a syntactically invalid document (for
    // example an alias expansion), so parsing remains a diagnostic rather than
    // aborting the whole repository scan.
    diagnostics.push(yamlDiagnostic(error, text, filePath));
    fields = {};
  }

  return { document, fields, diagnostics };
}

/**
 * Parse a markdown file without ever rendering or rewriting it.
 *
 * `rawBytes`, `frontmatterRaw` and `bodyRaw` are slices/copies of the bytes
 * supplied by the caller.  The caller may therefore use this result to offer
 * a byte-preserving no-op save or to calculate a content fingerprint.
 */
export function parseMarkdown(input, options = {}) {
  const rawBytes = asBuffer(input);
  const rawText = rawBytes.toString('utf8');
  const filePath = options.path ?? options.filePath;
  const first = readLine(rawBytes, 0);
  const hasFrontmatter = markerText(first) === YAML_OPEN;

  if (!hasFrontmatter) {
    return {
      rawBytes,
      rawText,
      frontmatter: {},
      frontmatterInfo: null,
      frontmatterRange: null,
      frontmatterRaw: Buffer.alloc(0),
      frontmatterText: '',
      bodyRaw: Buffer.from(rawBytes),
      bodyText: rawText,
      fields: {},
      data: {},
      values: {},
      document: null,
      diagnostics: [],
      hasFrontmatter: false,
    };
  }

  let closing;
  let cursor = first.next;
  while (cursor < rawBytes.length) {
    const line = readLine(rawBytes, cursor);
    if (YAML_CLOSE.has(markerText(line))) {
      closing = line;
      break;
    }
    if (line.next === cursor) break;
    cursor = line.next;
  }

  if (!closing) {
    return {
      rawBytes,
      rawText,
      frontmatter: {},
      frontmatterInfo: null,
      frontmatterRange: null,
      frontmatterRaw: Buffer.alloc(0),
      frontmatterText: '',
      bodyRaw: Buffer.from(rawBytes),
      bodyText: rawText,
      fields: {},
      data: {},
      values: {},
      document: null,
      diagnostics: [diagnostic('FRONTMATTER_UNTERMINATED', 'YAML frontmatter has no closing delimiter', {
        path: filePath,
        line: 1,
        origin: 'file',
      })],
      hasFrontmatter: true,
    };
  }

  const contentStart = first.next;
  const contentEnd = closing.start;
  const bodyStart = closing.next;
  const frontmatterRaw = Buffer.from(rawBytes.subarray(contentStart, contentEnd));
  const bodyRaw = Buffer.from(rawBytes.subarray(bodyStart));
  const frontmatterText = frontmatterRaw.toString('utf8');
  const parsed = parseYaml(frontmatterText, filePath);

  return {
    rawBytes,
    rawText,
    frontmatter: parsed.fields,
    frontmatterInfo: {
      openStart: first.start,
      openEnd: first.end,
      contentStart,
      contentEnd,
      closeStart: closing.start,
      closeEnd: closing.end,
      bodyStart,
      closeMarker: markerText(closing),
      raw: frontmatterRaw,
      text: frontmatterText,
    },
    frontmatterRange: {
      openStart: first.start,
      openEnd: first.end,
      contentStart,
      contentEnd,
      closeStart: closing.start,
      closeEnd: closing.end,
      bodyStart,
    },
    frontmatterRaw,
    frontmatterText,
    bodyRaw,
    bodyText: bodyRaw.toString('utf8'),
    fields: parsed.fields,
    data: parsed.fields,
    values: parsed.fields,
    document: parsed.document,
    diagnostics: parsed.diagnostics,
    hasFrontmatter: true,
  };
}

export const parseFrontmatter = parseMarkdown;
export const parseYamlFrontmatter = parseMarkdown;

function firstHeading(bodyText) {
  for (const line of bodyText.split(/\r?\n/)) {
    const match = /^#{1,6}\s+(.+?)\s*#*\s*$/.exec(line);
    if (match) return match[1];
  }
  return undefined;
}

function firstValue(fields, names) {
  for (const name of names) {
    if (Object.prototype.hasOwnProperty.call(fields, name) && fields[name] !== undefined) return fields[name];
  }
  return undefined;
}

function cloneSource(fields) {
  try {
    return structuredClone(fields);
  } catch {
    // The YAML schema used by the vault is JSON-like.  This fallback keeps
    // parser consumers working on older Node versions without mutating the
    // object returned by yaml.
    return JSON.parse(JSON.stringify(fields));
  }
}

function listValue(value) {
  if (value === undefined || value === null || value === '') return [];
  if (Array.isArray(value)) return value.slice();
  return [value];
}

function normaliseAssignee(fields) {
  const raw = firstValue(fields, ['assignee', 'assignee_email', 'assigneeEmail']);
  if (raw && typeof raw === 'object' && !Array.isArray(raw)) {
    return raw.email ?? raw.mail ?? raw.username ?? raw.name ?? null;
  }
  return raw ?? null;
}

function normalisePosition(value) {
  if (typeof value === 'number' && Number.isFinite(value)) return value;
  if (typeof value === 'string' && value.trim() !== '' && Number.isFinite(Number(value))) return Number(value);
  return value ?? null;
}

function normaliseBlocks(value) {
  if (value === undefined || value === null) return [];
  if (!Array.isArray(value)) return [{ key: null, reason: null, raw: value, invalid: true }];
  return value.map(entry => {
    if (typeof entry === 'string' || typeof entry === 'number') {
      return { key: String(entry), reason: null, raw: entry, invalid: true };
    }
    if (!entry || typeof entry !== 'object' || Array.isArray(entry)) {
      return { key: null, reason: null, raw: entry, invalid: true };
    }
    const key = firstValue(entry, ['key', 'target', 'ticket', 'ticketKey']);
    const reason = firstValue(entry, ['reason', 'why', 'description']);
    return {
      ...entry,
      key: key === undefined || key === null ? null : String(key),
      reason: reason === undefined || reason === null ? null : String(reason),
      raw: entry,
      invalid: key === undefined || key === null || reason === undefined || reason === null || String(reason).trim() === '',
    };
  });
}

/**
 * Normalise a ticket's commonly queried fields while retaining every source
 * key under `source` and at the top level.  New inventory fields consequently
 * remain visible until the schema is updated.
 */
export function normaliseTicket(fields = {}, options = {}) {
  const source = fields.source && typeof fields.source === 'object' && !Array.isArray(fields.source)
    ? cloneSource(fields.source)
    : cloneSource(fields);
  const frontmatter = cloneSource(fields);
  const keyValue = firstValue(fields, ['key', 'ticket_key', 'issue_key', 'id']);
  const key = keyValue === undefined || keyValue === null ? null : String(keyValue);
  const normalized = {
    ...frontmatter,
    key,
    type: firstValue(fields, ['type', 'issue_type', 'issueType']) ?? null,
    title: firstValue(fields, ['title', 'summary', 'name']) ?? options.title ?? null,
    status: firstValue(fields, ['status', 'state']) ?? null,
    statusCategory: firstValue(fields, ['statusCategory', 'status_category', 'statusCategoryName']) ?? null,
    category: firstValue(fields, ['category', 'issue_category']) ?? null,
    parent: firstValue(fields, ['parent', 'parent_key', 'parentKey']) ?? null,
    sprint: firstValue(fields, ['sprint', 'sprint_slug', 'sprintSlug']) ?? null,
    assignee: normaliseAssignee(fields),
    priority: firstValue(fields, ['priority']) ?? null,
    complexity: firstValue(fields, ['complexity']) ?? null,
    risk: firstValue(fields, ['risk']) ?? null,
    storyPoints: firstValue(fields, ['storyPoints', 'story_points', 'points']) ?? null,
    position: normalisePosition(firstValue(fields, ['position', 'rank', 'order'])),
    labels: listValue(firstValue(fields, ['labels', 'label'])),
    fixVersions: listValue(firstValue(fields, ['fixVersions', 'fix_versions', 'fixVersion'])),
    blocks: normaliseBlocks(firstValue(fields, ['blocks'])),
    source,
  };

  // A file name that is itself a conventional issue key remains useful for a
  // partially authored ticket, but never silently substitutes for a malformed
  // explicit key in frontmatter.
  if (normalized.key === null && options.filenameKey && !Object.prototype.hasOwnProperty.call(fields, 'key')) {
    normalized.key = options.filenameKey;
  }
  return normalized;
}

export const normalizeTicket = normaliseTicket;

export function normaliseSprint(fields = {}, options = {}) {
  const source = fields.source && typeof fields.source === 'object' && !Array.isArray(fields.source)
    ? cloneSource(fields.source)
    : cloneSource(fields);
  const frontmatter = cloneSource(fields);
  const slugValue = firstValue(fields, ['slug', 'key', 'sprint', 'id']);
  const slug = slugValue === undefined || slugValue === null || slugValue === ''
    ? options.filenameSlug ?? null
    : String(slugValue);
  return {
    ...frontmatter,
    slug,
    key: slug,
    title: firstValue(fields, ['title', 'name']) ?? options.title ?? slug,
    source,
  };
}

export const normalizeSprint = normaliseSprint;

/** Return raw wikilink targets, preserving their order and spelling. */
export function extractWikilinks(text = '') {
  const links = [];
  const pattern = /!?\[\[([^\]\n]+)\]\]/g;
  let match;
  while ((match = pattern.exec(text)) !== null) {
    const raw = match[1].trim();
    if (!raw) continue;
    const separator = raw.indexOf('|');
    const target = (separator === -1 ? raw : raw.slice(0, separator)).trim();
    if (target) links.push({ raw, target, display: separator === -1 ? undefined : raw.slice(separator + 1).trim() });
  }
  return links;
}

export function canonicalLinkTarget(value) {
  let target = String(value ?? '').trim().replaceAll('\\', '/');
  const hash = target.indexOf('#');
  if (hash !== -1) target = target.slice(0, hash);
  const pipe = target.indexOf('|');
  if (pipe !== -1) target = target.slice(0, pipe);
  target = target.replace(/^\.\//, '').replace(/\.md$/i, '');
  return target;
}

export function filenameKey(filePath) {
  const name = String(filePath ?? '').split(/[\\/]/).pop()?.replace(/\.md$/i, '');
  return /^[A-Za-z][A-Za-z0-9_]*-\d+$/.test(name ?? '') ? name : undefined;
}

export function filenameSlug(filePath) {
  const name = String(filePath ?? '').split(/[\\/]/).pop()?.replace(/\.md$/i, '');
  return name || undefined;
}
