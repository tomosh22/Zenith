import path from 'node:path';
import { isDeepStrictEqual, promisify } from 'node:util';
import { execFile } from 'node:child_process';
import { loadVault } from '../server/main.js';
import { git, resolveAuthority } from '../server/operations.mjs';
import { parseMarkdown } from '../server/parser.js';

const display = value => value === undefined || value === null ? null : typeof value === 'string' ? value : JSON.stringify(value);
const exec = promisify(execFile);
const committedBytes = async (repo, object) => (await exec('git', ['show', object], {
  cwd: repo, encoding: 'buffer', windowsHide: true, maxBuffer: 64 * 1024 * 1024,
})).stdout;

/** Frozen activity is immutable; new history is derived from committed snapshots. */
export async function appendGitHistory(root, key, frozen = [], limit = 50) {
  root = await resolveAuthority(root);
  const index = await loadVault(root);
  if (index.config.writes?.enabled !== true) return frozen;
  const ticket = index.getTicket(key);
  if (!ticket) return frozen;
  const repo = await git(root, ['rev-parse', '--show-toplevel']);
  const file = path.relative(repo, path.join(root, ticket.relativePath)).replaceAll('\\', '/');
  const commits = (await git(repo, ['log', '--format=%H', '--', file])).split('\n').filter(Boolean);
  const events = [];
  for (const commit of commits) {
    const message = await git(repo, ['show', '-s', '--format=%B', commit]);
    const managed = /^Vault-Operation: /m.test(message);
    const actor = await git(repo, ['show', '-s', '--format=%an', commit]);
    const createdAt = new Date(await git(repo, ['show', '-s', '--format=%aI', commit])).toISOString();
    const after = parseMarkdown(await committedBytes(repo, `${commit}:${file}`));
    let before = { fields: {}, bodyText: '' }, existed = false;
    try { before = parseMarkdown(await committedBytes(repo, `${commit}^:${file}`)); existed = true; } catch {}
    // Importing the frozen record is the boundary, not a new activity for every
    // source field. Later external commits and explicit reverts remain history.
    if (!managed && !existed) continue;
    const names = new Set([...Object.keys(before.fields), ...Object.keys(after.fields)]);
    for (const field of names) {
      if (field === 'source' || isDeepStrictEqual(before.fields[field], after.fields[field])) continue;
      events.push({ field, fromValue: display(before.fields[field]), toValue: display(after.fields[field]), actor, createdAt });
    }
    if (before.bodyText !== after.bodyText) events.push({ field: 'description', fromValue: before.bodyText || null, toValue: after.bodyText || null, actor, createdAt });
    if (events.length >= limit) break;
  }
  return [...events, ...frozen].slice(0, limit);
}
