import { promises as fs } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { git, resolveAuthority, recoverOperations, hashBytes } from '../server/operations.mjs';

export async function inspectOperations(root) {
  root = await resolveAuthority(root);
  const common = path.resolve(root, await git(root, ['rev-parse', '--git-common-dir']));
  const directory = path.join(common, 'vault/operations');
  let names;
  try { names = await fs.readdir(directory); } catch (e) { if (e.code === 'ENOENT') return []; throw e; }
  const result = [];
  for (const name of names.filter(n => n.endsWith('.json'))) {
    const journal = JSON.parse(await fs.readFile(path.join(directory, name), 'utf8'));
    if (journal.phase === 'complete') continue;
    const files = [];
    for (const change of journal.changes) {
      const relative = path.relative(root, path.resolve(root, change.path));
      if (relative.startsWith('..') || path.isAbsolute(relative)) throw new Error('Invalid journal path');
      let actual;
      try { actual = hashBytes(await fs.readFile(path.join(root, relative))); } catch (e) { if (e.code !== 'ENOENT') throw e; actual = null; }
      files.push({ path: change.path, state: actual === change.afterHash ? 'after' : actual === change.beforeHash ? 'before' : 'conflict' });
    }
    result.push({ id: journal.id, kind: journal.kind, phase: journal.phase, commit: journal.commit ?? null, files });
  }
  return result;
}

if (path.resolve(process.argv[1] ?? '') === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2), at = args.indexOf('--vault');
  if (at < 0 || !args[at + 1]) throw new Error('Usage: recover.mjs --vault ROOT [--apply]');
  try { console.log(JSON.stringify(args.includes('--apply') ? await recoverOperations(args[at + 1]) : await inspectOperations(args[at + 1]), null, 2)); }
  catch (e) { console.error(JSON.stringify({ code: e.code, error: e.message })); process.exitCode = 1; }
}
