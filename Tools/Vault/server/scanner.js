import { createHash } from 'node:crypto';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseDocument } from 'yaml';

import { diagnostic } from './diagnostics.js';
import {
  canonicalLinkTarget,
  filenameKey,
  filenameSlug,
  normaliseSprint,
  normaliseTicket,
  parseMarkdown,
  extractWikilinks,
} from './parser.js';

const DEFAULT_CONFIG_PATH = 'vault.config.json';
const STRUCTURED_EXTENSIONS = new Set(['.md', '.markdown']);
const SKIP_DIRECTORIES = new Set(['.git', '.hg', '.svn', 'node_modules', 'dist', 'build']);
const CONFIG_BASENAMES = new Set(['workflow.md', 'labels.md', 'versions.md', 'config.md']);

function asPath(value) {
  if (value instanceof URL) return path.resolve(fileURLToPath(value));
  if (typeof value !== 'string' || value.trim() === '') throw new TypeError('vaultRoot must be an explicit non-empty path');
  return path.resolve(value);
}

function toPosix(value) {
  return value.split(path.sep).join('/');
}

function isInside(parent, candidate) {
  const relative = path.relative(parent, candidate);
  return relative === '' || (relative !== '..' && !relative.startsWith(`..${path.sep}`) && !path.isAbsolute(relative));
}

function relativePath(root, absolutePath) {
  return toPosix(path.relative(root, absolutePath));
}

function addPathDetails(item, context) {
  return {
    ...item,
    path: item.path ?? context.relativePath ?? context.absolutePath,
    absolutePath: item.absolutePath ?? context.absolutePath,
    relativePath: item.relativePath ?? context.relativePath,
    project: item.project ?? context.project,
    origin: item.origin ?? 'file',
  };
}

function structuredFileKind(boardRelativePath, fields = {}) {
  const normalized = toPosix(boardRelativePath).replace(/^\/+/, '');
  const segments = normalized.split('/');
  const first = segments[0]?.toLowerCase();
  const basename = segments.at(-1)?.toLowerCase();
  const declared = String(fields.kind ?? fields.entity ?? '').toLowerCase();

  if (first === 'tickets') return 'ticket';
  if (first === 'sprints' || first === 'sprint') return 'sprint';
  if (first === 'knowledge') return 'knowledge';
  if (first === 'history') return 'history';
  if (first === 'config' || first === 'configs') return 'config';
  if (CONFIG_BASENAMES.has(basename)) return 'config';
  if (declared === 'metadata') return 'metadata';
  if (['catalog', 'workflow', 'labels', 'versions', 'config'].includes(declared)) return 'config';
  return 'document';
}

function normalizeKindOverride(value) {
  if (!value) return undefined;
  const normalized = String(value).toLowerCase();
  if (normalized === 'catalog' || normalized === 'config' || normalized === 'configuration') return 'config';
  if (normalized === 'metadata') return 'metadata';
  if (normalized === 'knowledge') return 'knowledge';
  if (normalized === 'history') return 'history';
  if (normalized === 'ticket') return 'ticket';
  if (normalized === 'sprint') return 'sprint';
  return value;
}

function isDraftTicketPath(boardRelativePath) {
  const normalized = toPosix(boardRelativePath).replace(/^\/+/, '').toLowerCase();
  return normalized === 'tickets/drafts' || normalized.startsWith('tickets/drafts/');
}

function parseConfigBytes(rawBytes, absolutePath) {
  const text = rawBytes.toString('utf8');
  const diagnostics = [];
  let value;

  // parseDocument is used as a duplicate-key detector. JSON.parse then keeps
  // the accepted syntax exactly JSON, rather than silently accepting YAML.
  try {
    const document = parseDocument(text, {
      prettyErrors: false,
      strict: true,
      uniqueKeys: true,
      keepSourceTokens: true,
      version: '1.2',
    });
    for (const error of document.errors ?? []) {
      const code = String(error.code ?? '').toUpperCase() === 'DUPLICATE_KEY' ? 'DUPLICATE_KEY' : 'CONFIG_PARSE_ERROR';
      diagnostics.push(diagnostic(code, error.message ?? String(error), {
        path: absolutePath,
        origin: 'file',
        yamlCode: error.code,
      }));
    }
    if (diagnostics.length === 0) value = JSON.parse(text);
  } catch (error) {
    diagnostics.push(diagnostic('CONFIG_PARSE_ERROR', error.message ?? String(error), {
      path: absolutePath,
      origin: 'file',
    }));
  }

  if (diagnostics.length === 0 && (value === null || typeof value !== 'object' || Array.isArray(value))) {
    diagnostics.push(diagnostic('CONFIG_NOT_MAPPING', 'vault.config.json must contain a JSON object', {
      path: absolutePath,
      origin: 'file',
    }));
  }
  return { value: diagnostics.length === 0 ? value : {}, diagnostics };
}

async function readBytes(filePath) {
  const [rawBytes, stats] = await Promise.all([fs.readFile(filePath), fs.stat(filePath)]);
  return {
    rawBytes,
    stats,
    hash: createHash('sha256').update(rawBytes).digest('hex'),
  };
}

async function listMarkdownFiles(directory) {
  const result = [];
  let entries;
  try {
    entries = await fs.readdir(directory, { withFileTypes: true });
  } catch (error) {
    if (error.code === 'ENOENT') return result;
    throw error;
  }
  entries.sort((a, b) => a.name.localeCompare(b.name));
  for (const entry of entries) {
    if (entry.name === '.' || entry.name === '..') continue;
    const absolutePath = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      if (SKIP_DIRECTORIES.has(entry.name)) continue;
      result.push(...await listMarkdownFiles(absolutePath));
    } else if (entry.isFile() && STRUCTURED_EXTENSIONS.has(path.extname(entry.name).toLowerCase())) {
      result.push(absolutePath);
    }
  }
  return result;
}

function boardRootPath(root, entry, index) {
  if (typeof entry === 'string') return { project: path.basename(entry) || `board-${index + 1}`, path: entry };
  if (!entry || typeof entry !== 'object') return null;
  const boardPath = entry.path ?? entry.root ?? entry.directory ?? entry.dir;
  if (typeof boardPath !== 'string' || boardPath.trim() === '') return null;
  const project = String(entry.project ?? entry.name ?? path.basename(boardPath) ?? `board-${index + 1}`);
  return {
    project,
    path: boardPath,
    ...(entry.readOnly === true ? { readOnly: true } : {}),
    ...(entry.archive === true ? { archive: true } : {}),
  };
}

function normalizeBoardRoots(root, config, options, diagnostics) {
  const configured = options.boardRoots ?? config.boardRoots ?? config.boards ?? config.projects;
  const values = configured === undefined ? [{ project: options.project ?? path.basename(root), path: '.' }] : configured;
  if (!Array.isArray(values)) {
    diagnostics.push(diagnostic('BOARD_ROOTS_NOT_ARRAY', 'vault boardRoots must be an array', {
      path: path.join(root, options.configPath ?? DEFAULT_CONFIG_PATH),
      origin: 'layout',
    }));
    return [];
  }

  const roots = [];
  const seen = new Set();
  values.forEach((entry, index) => {
    const normalized = boardRootPath(root, entry, index);
    if (!normalized) {
      diagnostics.push(diagnostic('INVALID_BOARD_ROOT', `Invalid board root at index ${index}`, {
        path: path.join(root, options.configPath ?? DEFAULT_CONFIG_PATH),
        origin: 'layout',
      }));
      return;
    }
    const absolutePath = path.resolve(root, normalized.path);
    if (!isInside(root, absolutePath)) {
      diagnostics.push(diagnostic('BOARD_ROOT_OUTSIDE_VAULT', `Board root ${normalized.path} is outside the explicit vault root`, {
        path: absolutePath,
        origin: 'layout',
        project: normalized.project,
      }));
      return;
    }
    const canonical = absolutePath.toLowerCase();
    if (seen.has(canonical)) {
      diagnostics.push(diagnostic('DUPLICATE_BOARD_ROOT', `Board root ${normalized.path} is listed more than once`, {
        path: absolutePath,
        origin: 'layout',
        project: normalized.project,
      }));
      return;
    }
    seen.add(canonical);
    roots.push({
      ...normalized,
      path: toPosix(normalized.path === '.' ? '.' : normalized.path.replaceAll('\\', '/')),
      absolutePath,
    });
  });
  return roots;
}

function normalizeSupportingFiles(root, config, options, diagnostics) {
  // metadata.md is the export's repository-wide table store. It lives beside
  // board roots and is optional for small hand-authored vaults; callers can
  // replace the default with `supportingFiles: []` or their own list.
  const configured = options.supportingFiles ?? config.supportingFiles ?? ['metadata.md'];
  if (configured === false || configured === null) return [];
  const values = Array.isArray(configured) ? configured : [configured];
  const result = [];
  const seen = new Set();
  for (const entry of values) {
    const descriptor = typeof entry === 'string'
      ? { path: entry, kind: undefined }
      : entry && typeof entry === 'object'
        ? { path: entry.path ?? entry.file, kind: entry.kind }
        : null;
    if (!descriptor || typeof descriptor.path !== 'string' || descriptor.path.trim() === '') {
      diagnostics.push(diagnostic('INVALID_SUPPORTING_FILE', 'supportingFiles entries must name a relative file', {
        path: root,
        origin: 'layout',
      }));
      continue;
    }
    const absolutePath = path.resolve(root, descriptor.path);
    if (!isInside(root, absolutePath)) {
      diagnostics.push(diagnostic('SUPPORTING_FILE_OUTSIDE_VAULT', `Supporting file ${descriptor.path} is outside the explicit vault root`, {
        path: absolutePath,
        origin: 'layout',
      }));
      continue;
    }
    const relative = relativePathForRecord(root, absolutePath);
    const key = absolutePath.toLowerCase();
    if (seen.has(key)) continue;
    seen.add(key);
    result.push({
      path: toPosix(descriptor.path),
      relativePath: relative,
      absolutePath,
      kind: descriptor.kind,
      optional: typeof entry === 'string' && descriptor.path === 'metadata.md',
    });
  }
  return result;
}

async function readConfigRecord(root, configPath) {
  const absolutePath = path.resolve(root, configPath);
  const relativePath = relativePathForRecord(root, absolutePath);
  try {
    const { rawBytes, stats, hash } = await readBytes(absolutePath);
    const parsed = parseConfigBytes(rawBytes, absolutePath);
    const diagnostics = parsed.diagnostics.map(item => addPathDetails(item, { absolutePath, relativePath }));
    return {
      path: absolutePath,
      absolutePath,
      relativePath,
      boardPath: relativePath,
      project: null,
      kind: 'config',
      category: 'config',
      rawBytes,
      bytes: rawBytes,
      rawText: rawBytes.toString('utf8'),
      frontmatter: null,
      fields: parsed.value,
      metadata: parsed.value,
      source: parsed.value,
      bodyRaw: rawBytes,
      bodyText: rawBytes.toString('utf8'),
      frontmatterRaw: Buffer.alloc(0),
      links: { wikilinks: [], blocks: [] },
      config: parsed.value,
      diagnostics,
      hash,
      size: stats.size,
      mtimeMs: stats.mtimeMs,
      stat: stats,
    };
  } catch (error) {
    // A config file is optional when callers provide boardRoots explicitly (or
    // intentionally use the vault root as one board). Missing configuration is
    // therefore an empty layout, rather than a corrupt structured file.
    if (error?.code === 'ENOENT') return null;
    return makeReadErrorRecord({ root, absolutePath, relativePath, kind: 'config', project: null }, error);
  }
}

function relativePathForRecord(root, absolutePath) {
  return relativePath(root, absolutePath) || path.basename(absolutePath);
}

function makeReadErrorRecord(context, error) {
  const message = error?.code === 'ENOENT' ? 'File disappeared while scanning' : error?.message ?? String(error);
  const bytes = Buffer.alloc(0);
  return {
    path: context.absolutePath,
    absolutePath: context.absolutePath,
    relativePath: context.relativePath,
    boardPath: context.boardPath ?? context.relativePath,
    project: context.project ?? null,
    kind: context.kind ?? 'document',
    category: context.kind ?? 'document',
    rawBytes: bytes,
    bytes,
    rawText: '',
    frontmatter: null,
    fields: {},
    metadata: {},
    source: {},
    bodyRaw: bytes,
    bodyText: '',
    frontmatterRaw: Buffer.alloc(0),
    links: { wikilinks: [], blocks: [] },
    diagnostics: [diagnostic('FILE_READ_ERROR', message, {
      path: context.absolutePath,
      relativePath: context.relativePath,
      project: context.project,
      origin: 'file',
      fsCode: error?.code,
    })],
    hash: createHash('sha256').update(bytes).digest('hex'),
    size: 0,
    mtimeMs: undefined,
    stat: undefined,
  };
}

function makeRecord({ root, boardRoot, absolutePath, project, boardRelativePath, rawBytes, stats, hash, kindOverride }) {
  const parsed = parseMarkdown(rawBytes, { path: absolutePath });
  const kind = normalizeKindOverride(kindOverride) ?? structuredFileKind(boardRelativePath, parsed.fields);
  const isDraft = kind === 'ticket' && isDraftTicketPath(boardRelativePath);
  const base = {
    path: absolutePath,
    absolutePath,
    relativePath: relativePathForRecord(root, absolutePath),
    boardPath: toPosix(boardRelativePath),
    project,
    kind,
    category: kind,
    rawBytes,
    bytes: rawBytes,
    rawText: parsed.rawText,
    frontmatter: parsed.frontmatter,
    frontmatterInfo: parsed.frontmatterInfo,
    frontmatterRange: parsed.frontmatterRange,
    frontmatterRaw: parsed.frontmatterRaw,
    frontmatterText: parsed.frontmatterText,
    fields: parsed.fields,
    bodyRaw: parsed.bodyRaw,
    bodyText: parsed.bodyText,
    hash,
    size: stats.size,
    mtimeMs: stats.mtimeMs,
    stat: stats,
    hasFrontmatter: parsed.hasFrontmatter,
    isDraft,
    claimable: kind === 'ticket' ? !isDraft : false,
    diagnostics: parsed.diagnostics.map(item => addPathDetails(item, {
      absolutePath,
      relativePath: relativePathForRecord(root, absolutePath),
      project,
    })),
  };

  const linkObjects = extractWikilinks(parsed.bodyText);
  const wikilinks = linkObjects.map(item => item.target);
  const keyFromFile = filenameKey(boardRelativePath);
  const slugFromFile = filenameSlug(boardRelativePath);
  let metadata;
  if (kind === 'ticket') {
    metadata = normaliseTicket(parsed.fields, {
      bodyText: parsed.bodyText,
      title: undefined,
      filenameKey: keyFromFile,
    });
    if (!parsed.hasFrontmatter) {
      base.diagnostics.push(diagnostic('TICKET_FRONTMATTER_REQUIRED', 'Ticket files must have YAML frontmatter', {
        severity: isDraft ? 'warning' : 'error',
        path: absolutePath,
        relativePath: base.relativePath,
        project,
        origin: 'file',
      }));
    }
    if (!Object.prototype.hasOwnProperty.call(parsed.fields, 'key')) {
      base.diagnostics.push(diagnostic(isDraft ? 'DRAFT_KEYLESS' : 'MISSING_KEY', isDraft ? 'Draft ticket has no key and is excluded from claimable tickets' : 'Ticket frontmatter must define key', {
        severity: isDraft ? 'warning' : 'error',
        path: absolutePath,
        relativePath: base.relativePath,
        project,
        origin: 'file',
      }));
    }
  } else if (kind === 'sprint') {
    metadata = normaliseSprint(parsed.fields, { filenameSlug: slugFromFile });
    if (!parsed.hasFrontmatter) {
      base.diagnostics.push(diagnostic('SPRINT_FRONTMATTER_REQUIRED', 'Sprint files must have YAML frontmatter', {
        path: absolutePath,
        relativePath: base.relativePath,
        project,
        origin: 'file',
      }));
    }
  } else if (kind === 'config' || kind === 'metadata') {
    metadata = {
      ...parsed.fields,
      source: parsed.fields,
      title: parsed.fields.title ?? parsed.fields.name ?? undefined,
      kind: parsed.fields.kind ?? kind,
    };
    if (!parsed.hasFrontmatter) {
      base.diagnostics.push(diagnostic('CONFIG_FRONTMATTER_REQUIRED', 'Structured catalog files must have YAML frontmatter', {
        path: absolutePath,
        relativePath: base.relativePath,
        project,
        origin: 'file',
      }));
    }
  } else {
    metadata = {
      ...parsed.fields,
      source: parsed.fields,
      title: parsed.fields.title ?? parsed.fields.name ?? undefined,
      kind: parsed.fields.kind ?? kind,
    };
  }

  const blocks = kind === 'ticket' ? metadata.blocks : [];
  return {
    ...base,
    metadata,
    source: metadata.source ?? parsed.fields,
    // Flatten canonical ticket fields for callers that want a lightweight
    // record, while retaining metadata as the unambiguous namespace.
    ...(kind === 'ticket' ? metadata : {}),
    config: kind === 'config' ? parsed.fields : undefined,
    links: {
      wikilinks,
      wikilinkObjects: linkObjects,
      blocks,
    },
  };
}

/** Read one already-classified markdown file for incremental watcher updates. */
export async function readVaultFile(absolutePath, layoutOrOptions = {}) {
  const root = asPath(layoutOrOptions.root ?? layoutOrOptions.vaultRoot);
  const boardRoots = layoutOrOptions.boardRoots ?? [];
  const descriptor = boardRoots
    .map(value => value.absolutePath ? value : ({ ...value, absolutePath: path.resolve(root, value.path ?? '.') }))
    .filter(value => isInside(value.absolutePath, path.resolve(absolutePath)))
    .sort((a, b) => b.absolutePath.length - a.absolutePath.length)[0];
  const absolute = path.resolve(absolutePath);
  const supporting = (layoutOrOptions.supportingFiles ?? [])
    .map(value => value.absolutePath ? value : ({ ...value, absolutePath: path.resolve(root, value.path ?? value.relativePath) }))
    .find(value => path.resolve(value.absolutePath).toLowerCase() === absolute.toLowerCase());
  if (!descriptor && !supporting) return null;
  if (!isInside(root, absolute)) return null;
  if (!supporting && !STRUCTURED_EXTENSIONS.has(path.extname(absolute).toLowerCase())) return null;
  if (supporting) {
    try {
      const { rawBytes, stats, hash } = await readBytes(absolute);
      const relative = relativePathForRecord(root, absolute);
      return makeRecord({
        root,
        boardRoot: { absolutePath: root, path: '.', project: null },
        absolutePath: absolute,
        project: null,
        boardRelativePath: relative,
        kindOverride: supporting.kind ?? (path.basename(absolute).toLowerCase() === 'metadata.md' ? 'metadata' : undefined),
        rawBytes,
        stats,
        hash,
      });
    } catch (error) {
      return makeReadErrorRecord({
        root,
        absolutePath: absolute,
        relativePath: relativePathForRecord(root, absolute),
        boardPath: relativePathForRecord(root, absolute),
        project: null,
        kind: supporting.kind ?? (path.basename(absolute).toLowerCase() === 'metadata.md' ? 'metadata' : 'document'),
      }, error);
    }
  }
  const boardRelativePath = toPosix(path.relative(descriptor.absolutePath, absolute));
  try {
    const { rawBytes, stats, hash } = await readBytes(absolute);
    return makeRecord({
      root,
      boardRoot: descriptor,
      absolutePath: absolute,
      project: descriptor.project,
      boardRelativePath,
      rawBytes,
      stats,
      hash,
    });
  } catch (error) {
    return makeReadErrorRecord({
      root,
      absolutePath: absolute,
      relativePath: relativePathForRecord(root, absolute),
      boardPath: boardRelativePath,
      project: descriptor.project,
      kind: structuredFileKind(boardRelativePath),
    }, error);
  }
}

/** Resolve the explicit vault root and configured board roots without scanning files. */
export async function resolveVaultLayout(vaultRoot, options = {}) {
  const root = asPath(vaultRoot);
  const rootStats = await fs.stat(root);
  if (!rootStats.isDirectory()) throw new TypeError(`Vault root is not a directory: ${root}`);
  const configPath = options.configPath ?? DEFAULT_CONFIG_PATH;
  const configAbsolutePath = path.resolve(root, configPath);
  if (!isInside(root, configAbsolutePath)) throw new TypeError('configPath must stay inside the explicit vault root');

  let config = {};
  let configRecord = null;
  const diagnostics = [];
  try {
    configRecord = await readConfigRecord(root, configPath);
    config = configRecord?.config ?? {};
    diagnostics.push(...(configRecord?.diagnostics ?? []));
  } catch (error) {
    diagnostics.push(diagnostic('CONFIG_READ_ERROR', error.message ?? String(error), {
      path: configAbsolutePath,
      relativePath: relativePathForRecord(root, configAbsolutePath),
      origin: 'layout',
    }));
  }

  const boardRoots = normalizeBoardRoots(root, config, options, diagnostics);
  const supportingFiles = normalizeSupportingFiles(root, config, options, diagnostics);
  for (const boardRoot of boardRoots) {
    try {
      const stats = await fs.stat(boardRoot.absolutePath);
      if (!stats.isDirectory()) {
        diagnostics.push(diagnostic('BOARD_ROOT_NOT_DIRECTORY', `Board root is not a directory: ${boardRoot.path}`, {
          path: boardRoot.absolutePath,
          project: boardRoot.project,
          origin: 'layout',
        }));
      }
    } catch (error) {
      diagnostics.push(diagnostic('BOARD_ROOT_READ_ERROR', error.message ?? String(error), {
        path: boardRoot.absolutePath,
        project: boardRoot.project,
        origin: 'layout',
      }));
    }
  }

  return {
    root,
    vaultRoot: root,
    rootStats,
    configPath,
    configAbsolutePath,
    config,
    configRecord,
    boardRoots,
    supportingFiles,
    diagnostics,
  };
}

/**
 * Cold scan every configured board root.  The scan returns data and
 * diagnostics even when individual files are corrupt; callers can then fail
 * closed for repo-wide operations while still displaying the rest of the
 * vault.
 */
export async function scanVault(vaultRoot, options = {}) {
  const layout = await resolveVaultLayout(vaultRoot, options);
  const files = [];
  const seen = new Set();
  if (layout.configRecord) {
    seen.add(layout.configRecord.absolutePath.toLowerCase());
  }
  const supportingRecords = [];
  for (const supporting of layout.supportingFiles) {
    if (seen.has(supporting.absolutePath.toLowerCase())) continue;
    try {
      const { rawBytes, stats, hash } = await readBytes(supporting.absolutePath);
      supportingRecords.push(makeRecord({
        root: layout.root,
        boardRoot: { absolutePath: layout.root, path: '.', project: null },
        absolutePath: supporting.absolutePath,
        project: null,
        boardRelativePath: supporting.relativePath,
        kindOverride: supporting.kind ?? (path.basename(supporting.absolutePath).toLowerCase() === 'metadata.md' ? 'metadata' : undefined),
        rawBytes,
        stats,
        hash,
      }));
      seen.add(supporting.absolutePath.toLowerCase());
    } catch (error) {
      if (error?.code !== 'ENOENT' || !supporting.optional) {
        supportingRecords.push(makeReadErrorRecord({
          root: layout.root,
          absolutePath: supporting.absolutePath,
          relativePath: supporting.relativePath,
          boardPath: supporting.relativePath,
          project: null,
          kind: supporting.kind ?? 'document',
        }, error));
      }
    }
  }
  for (const boardRoot of layout.boardRoots) {
    let markdownFiles = [];
    try {
      markdownFiles = await listMarkdownFiles(boardRoot.absolutePath);
    } catch (error) {
      layout.diagnostics.push(diagnostic('BOARD_ROOT_READ_ERROR', error.message ?? String(error), {
        path: boardRoot.absolutePath,
        project: boardRoot.project,
        origin: 'layout',
      }));
      continue;
    }
    for (const absolutePath of markdownFiles) {
      const canonical = absolutePath.toLowerCase();
      if (seen.has(canonical)) continue;
      seen.add(canonical);
      const boardRelativePath = toPosix(path.relative(boardRoot.absolutePath, absolutePath));
      try {
        const { rawBytes, stats, hash } = await readBytes(absolutePath);
        files.push(makeRecord({
          root: layout.root,
          boardRoot,
          absolutePath,
          project: boardRoot.project,
          boardRelativePath,
          rawBytes,
          stats,
          hash,
        }));
      } catch (error) {
        files.push(makeReadErrorRecord({
          root: layout.root,
          absolutePath,
          relativePath: relativePathForRecord(layout.root, absolutePath),
          boardPath: boardRelativePath,
          project: boardRoot.project,
          kind: structuredFileKind(boardRelativePath),
        }, error));
      }
    }
  }
  files.sort((a, b) => a.relativePath.localeCompare(b.relativePath));

  return {
    ...layout,
    files: [
      ...(layout.configRecord ? [layout.configRecord] : []),
      ...supportingRecords,
      ...files,
    ],
    records: [
      ...(layout.configRecord ? [layout.configRecord] : []),
      ...supportingRecords,
      ...files,
    ],
    scannedAt: new Date(),
    diagnostics: [
      ...layout.diagnostics,
      ...supportingRecords.flatMap(file => file.diagnostics ?? []),
      ...files.flatMap(file => file.diagnostics ?? []),
    ],
  };
}

/** Convenience entry point; the index module is loaded lazily to avoid a module cycle. */
export async function loadVault(vaultRoot, options = {}) {
  const snapshot = await scanVault(vaultRoot, options);
  const { createVaultIndex } = await import('./index.js');
  return createVaultIndex(snapshot, options);
}

export const classifyPath = structuredFileKind;
export { normalizeBoardRoots, isInside, toPosix, relativePath };
