import { randomBytes } from 'node:crypto';
import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

import Fastify from 'fastify';
import fastifyStatic from '@fastify/static';
import fastifyWebsocket from '@fastify/websocket';

import { loadVault, watchVault } from './main.js';
import { readDocument, resolveAuthority, saveDocument } from './operations.mjs';
import { dispatchWrite } from '../cli/write.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const defaultWebRoot = path.resolve(here, '..', 'web', 'dist');
const LOCAL_HOSTS = new Set(['localhost', '127.0.0.1', '::1']);

function jsonSafe(value) {
  if (value === undefined) return null;
  if (typeof value === 'bigint') return Number(value);
  if (Buffer.isBuffer(value)) return value.toString('base64');
  if (Array.isArray(value)) return value.map(jsonSafe);
  if (value && typeof value === 'object') {
    const result = {};
    for (const [key, item] of Object.entries(value)) {
      if (key === 'rawBytes' || key === 'bytes' || key === 'stat' || key === 'absolutePath' || key === 'path') continue;
      result[key] = jsonSafe(item);
    }
    return result;
  }
  return value;
}

function hostName(value) {
  if (!value) return '';
  const text = String(value).trim().toLowerCase();
  if (text.startsWith('[')) return text.slice(1, text.indexOf(']'));
  return text.split(':')[0];
}

function validLocalOrigin(origin, hostHeader) {
  if (!origin) return true;
  try {
    const parsed = new URL(origin);
    if (!['http:', 'ws:'].includes(parsed.protocol) || !LOCAL_HOSTS.has(parsed.hostname.toLowerCase())) return false;
    const expectedHost = String(hostHeader ?? '').toLowerCase();
    if (!expectedHost) return false;
    const expectedName = hostName(expectedHost);
    if (expectedName !== parsed.hostname.toLowerCase()) return false;
    const explicitPort = expectedHost.startsWith('[')
      ? expectedHost.slice(expectedHost.indexOf(']') + 1).replace(/^:/, '')
      : expectedHost.includes(':') ? expectedHost.slice(expectedHost.lastIndexOf(':') + 1) : '';
    const defaultPort = parsed.protocol.endsWith('s:') ? '443' : '80';
    const originPort = parsed.port || defaultPort;
    return originPort === (explicitPort || '80');
  } catch {
    return false;
  }
}

function relativePath(value) {
  if (typeof value !== 'string' || value.trim() === '') {
    const error = new Error('A relative vault path is required');
    error.code = 'PATH';
    throw error;
  }
  const normalized = value.replaceAll('\\', '/');
  if (normalized.startsWith('/') || normalized.includes('\0')
    || normalized.split('/').some(part => part === '..' || part === '.git' || part === '.zagent')
    || /^[A-Za-z]:/.test(normalized)) {
    const error = new Error('Path escapes the vault');
    error.code = 'PATH';
    throw error;
  }
  return normalized.replace(/^\.\//, '');
}

async function safeFile(root, value) {
  const relative = relativePath(value);
  const full = path.resolve(root, relative);
  if (full === root || !full.startsWith(`${root}${path.sep}`)) {
    const error = new Error('Path escapes the vault');
    error.code = 'PATH';
    throw error;
  }
  let current = root;
  for (const part of relative.split('/')) {
    current = path.join(current, part);
    try {
      if ((await fs.lstat(current)).isSymbolicLink()) {
        const error = new Error('Symlink paths are not served');
        error.code = 'PATH';
        throw error;
      }
    } catch (error) {
      if (error.code === 'ENOENT') break;
      throw error;
    }
  }
  return { relative, full };
}

function metadataFor(record) {
  return jsonSafe(record?.metadata ?? record?.fields ?? {});
}

function recordFor(record, index) {
  if (!record) return null;
  const backlinks = [...(index.backlinks?.get(record.metadata?.key ?? record.relativePath) ?? [])];
  const linkKeys = [record.relativePath, record.metadata?.key, path.basename(record.relativePath, path.extname(record.relativePath))];
  const incomingSet = new Set(backlinks);
  for (const key of linkKeys.filter(Boolean)) for (const source of index.wikilinks?.get(key) ?? []) incomingSet.add(source);
  const incoming = [...incomingSet];
  return {
    path: record.relativePath,
    kind: record.kind,
    project: record.project ?? null,
    title: record.metadata?.title ?? record.title ?? path.basename(record.relativePath, path.extname(record.relativePath)),
    status: record.metadata?.status ?? record.status ?? null,
    statusCategory: record.statusCategory ?? record.metadata?.statusCategory ?? null,
    metadata: metadataFor(record),
    body: record.bodyText ?? '',
    links: jsonSafe(record.links ?? {}),
    wikilinks: jsonSafe(record.links?.wikilinks ?? []),
    backlinks,
    incoming,
    diagnostics: jsonSafe(record.diagnostics ?? []),
    hash: record.hash ?? null,
    size: record.size ?? record.rawBytes?.length ?? null,
    mtimeMs: record.mtimeMs ?? record.stat?.mtimeMs ?? null,
  };
}

function workflowStatusesByProject(index) {
  const projects = new Map();
  let order = 0;
  for (const record of index.configs?.values?.() ?? []) {
    const kind = String(record.fields?.kind ?? record.metadata?.kind ?? '').trim().toLowerCase();
    if (kind !== 'workflow' || !record.project) continue;
    const entries = Array.isArray(record.fields?.statuses) ? record.fields.statuses : [];
    if (!entries.length) continue;
    const project = String(record.project).trim();
    if (!project) continue;
    if (!projects.has(project)) projects.set(project, new Map());
    const statuses = projects.get(project);
    for (const entry of entries) {
      const value = entry?.name ?? entry?.status ?? entry?.status_name ?? entry?.title;
      const name = value === undefined || value === null ? '' : String(value).trim();
      if (!name) { order += 1; continue; }
      const position = Number(entry?.position);
      const candidate = {
        name,
        position: Number.isFinite(position) ? position : Number.POSITIVE_INFINITY,
        order,
      };
      const previous = statuses.get(name);
      if (!previous || candidate.position < previous.position
        || (candidate.position === previous.position && candidate.order < previous.order)) {
        statuses.set(name, candidate);
      }
      order += 1;
    }
  }
  return Object.fromEntries([...projects.entries()]
    .sort(([left], [right]) => left.localeCompare(right))
    .map(([project, statuses]) => [project, [...statuses.values()]
      .sort((left, right) => left.position - right.position || left.order - right.order || left.name.localeCompare(right.name))
      .map(status => status.name)]));
}

function stateFor(index) {
  const files = index.records.map(record => recordFor(record, index));
  const tickets = files.filter(record => record.kind === 'ticket');
  const documents = files.filter(record => !['ticket', 'config', 'metadata', 'sprint'].includes(record.kind));
  const projects = [...new Set(files.map(record => record.project).filter(Boolean))].sort();
  const statusesByProject = workflowStatusesByProject(index);
  const statusSet = new Set(tickets.map(record => record.status).filter(Boolean));
  for (const statuses of Object.values(statusesByProject)) {
    for (const status of statuses) statusSet.add(status);
  }
  const statuses = [...statusSet].sort();
  return {
    version: index.version,
    valid: index.valid,
    claimable: index.claimable,
    boardRoots: jsonSafe(index.boardRoots ?? []),
    projects,
    statuses,
    statusesByProject,
    files,
    tickets,
    documents,
    diagnostics: jsonSafe(index.diagnostics ?? []),
  };
}

function errorPayload(error) {
  const code = error?.code ?? 'ERROR';
  const status = code === 'STALE' ? 409
    : code === 'READ_ONLY' ? 403
      : code === 'PATH' || code === 'ID' ? 400
        : code === 'LOCKED' ? 423
          : code === 'INVALID' ? 422
            : 500;
  return { status, body: { error: { code, message: error?.message ?? String(error) } } };
}

function csrfRequired(request, reply, token) {
  if (request.headers['x-csrf-token'] !== token) {
    reply.code(403).send({ error: { code: 'CSRF', message: 'A valid per-launch CSRF token is required' } });
    return false;
  }
  return true;
}

function revisionHeader(revision) {
  return revision?.hash ? `"${revision.hash}"` : '"missing"';
}

/**
 * Construct the localhost Vault application. The returned object owns the
 * watcher and must be closed by callers that do not start a listening server.
 */
export async function createVaultServer(options = {}) {
  if (!options.vaultRoot && !options.root) throw new TypeError('vaultRoot is required');
  const requestedRoot = path.resolve(options.vaultRoot ?? options.root);
  const root = await resolveAuthority(requestedRoot);
  const index = await loadVault(root);
  const csrfToken = randomBytes(24).toString('hex');
  const sockets = new Set();
  const context = {
    root,
    index,
    csrfToken,
    sockets,
    watcher: null,
    writesEnabled: index.vaultConfig?.writes?.enabled === true,
  };

  const app = Fastify({ logger: options.logger ?? false, routerOptions: { ignoreTrailingSlash: true } });
  await app.register(fastifyWebsocket);

  app.addHook('onRequest', async (request, reply) => {
    const host = hostName(request.headers.host);
    if (host && !LOCAL_HOSTS.has(host)) {
      return reply.code(400).send({ error: { code: 'HOST', message: 'Only localhost requests are accepted' } });
    }
    if (!validLocalOrigin(request.headers.origin, request.headers.host)) {
      return reply.code(403).send({ error: { code: 'ORIGIN', message: 'Cross-origin requests are not accepted' } });
    }
    return undefined;
  });

  app.get('/api/health', async () => ({ ok: true, valid: context.index.valid, readOnly: !context.writesEnabled }));
  app.get('/api/session', async () => ({
    csrfToken,
    readOnly: !context.writesEnabled,
    writeEnabled: context.writesEnabled,
    vault: path.basename(root),
  }));
  app.get('/api/state', async () => stateFor(context.index));
  app.get('/api/search', async request => {
    const query = String(request.query?.q ?? '').trim();
    const limit = Number.isFinite(Number(request.query?.limit)) ? Number(request.query.limit) : 100;
    if (!query) return { query, results: [] };
    const records = context.index.search(query, {
      kind: request.query?.kind || undefined,
      project: request.query?.project || undefined,
      limit: Math.max(0, Math.min(500, limit)),
    });
    return { query, results: records.map(record => recordFor(record, context.index)) };
  });

  app.get('/api/document', async (request, reply) => {
    try {
      const { relative, full } = await safeFile(root, request.query?.path);
      if (!/\.(?:md|markdown)$/i.test(relative) || !context.index.getFile(relative)) {
        return reply.code(404).send({ error: { code: 'NOT_FOUND', message: 'Only indexed Markdown documents are served' } });
      }
      const document = await readDocument(root, relative);
      if (!document.bytes) return reply.code(404).send({ error: { code: 'NOT_FOUND', message: 'Document not found' } });
      const { bytes, revision } = document;
      const etag = revisionHeader(revision);
      reply.header('ETag', etag);
      if (request.headers['if-none-match'] === etag) return reply.code(304).send();
      return {
        path: relative,
        text: bytes.toString('utf8'),
        bytesBase64: bytes.toString('base64'),
        revision,
        record: recordFor(context.index.getFile(relative), context.index),
      };
    } catch (error) {
      if (error.code === 'ENOENT') return reply.code(404).send({ error: { code: 'NOT_FOUND', message: 'Document not found' } });
      const result = errorPayload(error);
      return reply.code(result.status).send(result.body);
    }
  });

  async function emitChange(relative) {
    await context.watcher?.handle('change', path.join(root, relative));
  }

  app.put('/api/document', async (request, reply) => {
    if (!csrfRequired(request, reply, csrfToken)) return;
    const body = request.body ?? {};
    try {
      const { relative } = await safeFile(root, body.path);
      if (body.revision === undefined) {
        const error = new Error('A document revision is required');
        error.code = 'STALE';
        throw error;
      }
      const bytes = body.bytesBase64 !== undefined
        ? Buffer.from(String(body.bytesBase64), 'base64')
        : Buffer.from(String(body.text ?? ''), 'utf8');
      const result = await saveDocument(root, relative, bytes, body.revision);
      await emitChange(relative);
      reply.header('ETag', revisionHeader(result.revision));
      return result;
    } catch (error) {
      const result = errorPayload(error);
      return reply.code(result.status).send(result.body);
    }
  });

  app.post('/api/status', async (request, reply) => {
    if (!csrfRequired(request, reply, csrfToken)) return;
    const body = request.body ?? {};
    try {
      const key = typeof body.key === 'string' ? body.key : null;
      const requestedPath = body.path ?? (key ? context.index.getTicket(key)?.relativePath : null);
      const { relative } = await safeFile(root, requestedPath);
      const nextStatus = String(body.status ?? '').trim();
      if (!nextStatus) {
        const error = new Error('A status is required');
        error.code = 'INVALID';
        throw error;
      }
      const expected = body.revision;
      const current = await loadVault(root);
      const currentRecord = current.getFile(relative);
      const actual = currentRecord ? {
        hash: currentRecord.hash,
        size: currentRecord.size ?? currentRecord.rawBytes?.length,
        mtimeMs: currentRecord.mtimeMs ?? currentRecord.stat?.mtimeMs,
      } : { hash: null, size: 0, mtimeMs: null };
      if (!expected || actual.hash !== expected.hash || (expected.size !== undefined && actual.size !== expected.size)
        || (expected.mtimeMs !== undefined && actual.mtimeMs !== expected.mtimeMs)) {
        const error = new Error(`File changed since it was opened: ${relative}`);
        error.code = 'STALE';
        throw error;
      }
      if (key && currentRecord?.metadata?.key !== key) {
        const error = new Error('The ticket key and document path identify different tickets');
        error.code = 'INVALID';
        throw error;
      }
      const ticketKey = key ?? current.getFile(relative)?.metadata?.key;
      if (!ticketKey) {
        const error = new Error('Status changes require a ticket document');
        error.code = 'INVALID';
        throw error;
      }
      const result = await dispatchWrite(current, ['move', ticketKey, nextStatus, '--human']);
      if (!result || result.exitCode !== 0) {
        const error = new Error(result?.payload?.error ?? 'Status change was rejected');
        error.code = result?.payload?.code ?? 'INVALID';
        throw error;
      }
      await emitChange(relative);
      return result.payload;
    } catch (error) {
      const result = errorPayload(error);
      return reply.code(result.status).send(result.body);
    }
  });

  app.get('/api/events', { websocket: true }, (socket, request) => {
    sockets.add(socket);
    socket.send(JSON.stringify({ type: 'ready', version: context.index.version }));
    socket.on('close', () => sockets.delete(socket));
    socket.on('error', () => sockets.delete(socket));
    socket.on('message', raw => {
      // The browser currently uses WS for notifications. Refuse mutation
      // messages unless they carry the same launch token as HTTP writes.
      try {
        const message = JSON.parse(String(raw));
        if (message?.type === 'ping') socket.send(JSON.stringify({ type: 'pong' }));
        else if (message?.type === 'mutate') socket.send(JSON.stringify({ type: 'error', code: 'CSRF', message: 'Use the HTTP mutation API' }));
      } catch {
        socket.send(JSON.stringify({ type: 'error', code: 'BAD_MESSAGE', message: 'Expected JSON' }));
      }
    });
  });

  const webRoot = path.resolve(options.webRoot ?? defaultWebRoot);
  try {
    const stat = await fs.stat(webRoot);
    if (stat.isDirectory()) {
      await app.register(fastifyStatic, { root: webRoot, index: ['index.html'], wildcard: false });
      app.get('/*', async (request, reply) => {
        if (request.url.startsWith('/api/')) return reply.code(404).send({ error: { code: 'NOT_FOUND', message: 'API route not found' } });
        return reply.sendFile('index.html');
      });
    }
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
    app.get('/', async (_request, reply) => reply.code(503).send('Vault web app is not built'));
  }

  context.watcher = await watchVault(root, {
    onUpdate: async update => {
      context.index = update.index;
      context.writesEnabled = context.index.vaultConfig?.writes?.enabled === true;
      const payload = JSON.stringify({
        type: 'update',
        event: update.type,
        path: update.relativePath,
        version: context.index.version,
        record: recordFor(update.record, context.index),
        diagnostics: jsonSafe(context.index.diagnostics),
      });
      for (const socket of sockets) {
        try { socket.send(payload); } catch { sockets.delete(socket); }
      }
    },
    onError: error => {
      const payload = JSON.stringify({ type: 'error', code: error?.code ?? 'WATCH', message: error?.message ?? String(error) });
      for (const socket of sockets) {
        try { socket.send(payload); } catch { sockets.delete(socket); }
      }
    },
  });

  return {
    app,
    context,
    root,
    csrfToken,
    async start(listenOptions = {}) {
      return app.listen({ host: listenOptions.host ?? options.host ?? '127.0.0.1', port: listenOptions.port ?? options.port ?? 0 });
    },
    async close() {
      await context.watcher?.close();
      await app.close();
    },
  };
}

export default createVaultServer;

async function main() {
  const argv = process.argv.slice(2);
  const vaultIndex = argv.findIndex(arg => arg === '--vault');
  const vaultRoot = vaultIndex >= 0 ? argv[vaultIndex + 1] : undefined;
  const portIndex = argv.findIndex(arg => arg === '--port');
  const port = portIndex >= 0 ? Number(argv[portIndex + 1]) : 0;
  if (!vaultRoot) {
    console.error('Usage: node server/http.mjs --vault ROOT [--port PORT]');
    process.exitCode = 1;
    return;
  }
  const server = await createVaultServer({ vaultRoot, port });
  const address = await server.start();
  console.log(JSON.stringify({ ok: true, url: address, readOnly: !server.context.writesEnabled }));
  const stop = async () => { await server.close(); process.exit(0); };
  process.once('SIGINT', stop);
  process.once('SIGTERM', stop);
}

if (process.argv[1] && path.resolve(process.argv[1]) === path.resolve(fileURLToPath(import.meta.url))) main().catch(error => {
  console.error(error.stack ?? error);
  process.exitCode = 1;
});
