/*
 * The one sanctioned way to ask "what changed in the working tree".
 *
 * `.claude/commands/tick.md` documents that both obvious `git` spellings for
 * this question are wrong in ways nothing used to catch: `<base>...HEAD` is
 * empty because a tick's edits are never committed yet when `gates`/`guard`
 * run, and `git diff --name-only` cannot see a file the worker CREATED
 * (nothing to diff against — it is untracked). `git status --porcelain
 * -uall` is the one spelling that sees a new file, a modified file and a
 * rename in one pass.
 *
 * Extracted out of client.mjs so tick-checks.mjs (the excursion/commit
 * checks) can reuse the exact same derivation without importing client.mjs
 * itself, which would close an import cycle through write.mjs (client.mjs ->
 * write.mjs -> tick-checks.mjs -> client.mjs). client.mjs re-exports this
 * for the callers that already import `workingTreeChanges` from it.
 */

import { execFile as execFileCallback } from 'node:child_process';
import path from 'node:path';
import { promisify } from 'node:util';

const execFile = promisify(execFileCallback);

export function parseWorkingTreeChanges(stdout) {
  const paths = [];
  for (const line of String(stdout ?? '').split(/\r?\n/)) {
    if (line.length <= 3) continue;
    for (const part of line.slice(3).split(' -> ')) {
      let value = part.trim();
      if (value.length >= 2 && value.startsWith('"') && value.endsWith('"')) value = value.slice(1, -1);
      if (value && !paths.includes(value)) paths.push(value);
    }
  }
  return paths;
}

/** Return modified, deleted, renamed, and untracked files in the client repo. */
export async function workingTreeChanges(repoRoot) {
  try {
    const { stdout } = await execFile('git', ['-C', path.resolve(repoRoot), 'status', '--porcelain', '-uall'], { encoding: 'utf8' });
    return parseWorkingTreeChanges(stdout);
  } catch {
    // A gates call may legitimately run before the first edit. A guard call
    // handles the empty result explicitly; neither path falls back to a
    // commit range which would hide newly-created files.
    return [];
  }
}

export default workingTreeChanges;
