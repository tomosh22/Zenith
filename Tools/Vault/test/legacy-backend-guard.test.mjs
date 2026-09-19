// Guards against the retired legacy HTTP zagent backend reappearing in
// tracked files. `Tools/zagent/backend.json` used to select between two
// backends -- "vault" (this checkout, live) and "legacy" (a remote-HTTP
// PowerShell client needing `ZAGENT_URL`/`ZAGENT_TOKEN` on the machine).
// The legacy backend, its transport, and everything reachable only
// through it were removed from `Tools/zagent/` -- there is now exactly
// one backend and nothing left to select.
//
// Scans git's TRACKED files only (`git grep`), never walks the filesystem --
// `.claude/worktrees` holds dozens of full worktree copies and a filesystem
// walk over it is pathologically slow.
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { test } from 'vitest';

const here = path.dirname(fileURLToPath(import.meta.url));
// Tools/Vault/test -> Tools/Vault -> Tools -> repo root.
const repoRoot = path.resolve(here, '..', '..', '..');

// The bare word "legacy" is deliberately NOT hunted here: this repo uses it
// constantly for unrelated things ("legacy game asset", "legacy code",
// "legacy .zanim schema", vendored ImGui's own "legacy backend" meaning a
// pre-1.92 renderer API). Those are real, unrelated uses and widening this
// guard to the bare word would just teach everyone to ignore it. The three
// terms below are specific to the HTTP transport this task removed and are
// not known to occur naturally anywhere else in this repo's own content.
const HUNTED_PATTERN = 'ZAGENT_URL|ZAGENT_TOKEN|legacy backend|legacy zagent';

// The documentation follow-up this guard's header once deferred to has
// landed: `.claude/commands/tick.md`, `CLAUDE.md` and both `Board.md` docs
// were rewritten to describe the current, single-backend reality, so there
// are no more path-specific exceptions here. Widening this past an exact
// path match is how a real reintroduction would go unnoticed, so any future
// exception stays a short, explicit list rather than a directory or pattern
// match.
const ALLOWED_PATHS = [];

// "legacy backend" also occurs, completely unrelated, in vendored ImGui
// (docking branch): a pre-1.92 RENDERER backend, nothing to do with this
// board's HTTP transport. It lives entirely under `Middleware/`, so that
// prefix is excluded the same way saas-guard.test.mjs excludes it for its
// own unrelated "MSAA" false positive -- a short, explicit prefix rather
// than a pattern match, so a real hit elsewhere still fails loudly.
const ALLOWED_VENDOR_PREFIXES = ['Middleware/'];

function isAllowed(relPath) {
  const normalised = relPath.replace(/\\/g, '/');
  return (
    ALLOWED_PATHS.includes(normalised) ||
    ALLOWED_VENDOR_PREFIXES.some(prefix => normalised.startsWith(prefix))
  );
}

// This file necessarily contains the very terms it hunts for (in the
// comments above, and in its own hunted pattern/allowlist strings), so it
// must exempt itself or it fails the moment it is committed -- the same
// trap saas-guard.test.mjs's header warns about: a check whose result
// depends on whether it has been committed yet is worse than no check.
const SELF_RELATIVE_PATH = path
  .relative(repoRoot, fileURLToPath(import.meta.url))
  .replace(/\\/g, '/');

function isSelf(relPath) {
  return relPath.replace(/\\/g, '/') === SELF_RELATIVE_PATH;
}

test('no tracked file (outside the documented exceptions) mentions the retired legacy zagent backend', () => {
  let stdout;
  try {
    // -i: case-insensitive. -I: skip files git considers binary.
    // -l: list matching file paths only, one per line. -E: extended regex.
    stdout = execFileSync('git', ['grep', '-iIlE', HUNTED_PATTERN], {
      cwd: repoRoot,
      encoding: 'utf8',
    });
  } catch (error) {
    if (error.code === 'ENOENT') {
      console.warn('[legacy-backend-guard] git executable not found on PATH; skipping this check.');
      return;
    }
    if (typeof error.status === 'number' && error.status === 1) {
      // `git grep` exits 1 when no file matched -- that is the clean state.
      stdout = '';
    } else {
      // Anything else (not inside a git repo, git present but broken, etc.)
      // is an environment problem this guard cannot diagnose -- skip rather
      // than fail the suite over something unrelated to the repo's content.
      console.warn(`[legacy-backend-guard] could not run \`git grep\`; skipping this check. (${error.message})`);
      return;
    }
  }

  const offenders = stdout
    .split('\n')
    .map(line => line.trim())
    .filter(Boolean)
    .filter(relPath => !isAllowed(relPath) && !isSelf(relPath));

  assert.deepEqual(
    offenders,
    [],
    `legacy zagent backend terms (ZAGENT_URL/ZAGENT_TOKEN/"legacy backend"/"legacy zagent") reappeared in tracked file(s) outside the documented exceptions:\n${offenders.join('\n')}`,
  );
});

test('the exception allowlist stays narrow and does not swallow a real hit', () => {
  // Sanity-checks the allowlist predicate in isolation, independent of the
  // repo's current content. The documentation follow-up landed, so the
  // former path exceptions are no longer allowed -- only the unrelated
  // vendored ImGui prefix stays exempt.
  assert.equal(isAllowed('.claude/commands/tick.md'), false);
  assert.equal(isAllowed('CLAUDE.md'), false);
  assert.equal(isAllowed('Games/Foundry/Docs/Board.md'), false);
  assert.equal(isAllowed('Games/Zenithmon/Docs/Board.md'), false);
  assert.equal(isAllowed('Middleware/imgui-docking/imgui.cpp'), true);
  assert.equal(isAllowed('Middleware\\imgui-docking\\imgui.cpp'), true);
  assert.equal(isAllowed('Tools/zagent/README.md'), false);
  assert.equal(isAllowed('Tools/zagent/zagent.ps1'), false);
});

test('the self-exemption matches this file and nothing else', () => {
  assert.equal(isSelf(SELF_RELATIVE_PATH), true);
  assert.equal(isSelf(SELF_RELATIVE_PATH.replace(/\\/g, '/')), true);
  assert.equal(isSelf('Tools/Vault/test/legacy-backend-guard-other.test.mjs'), false);
  assert.equal(isSelf('Tools/zagent/README.md'), false);
  assert.match(SELF_RELATIVE_PATH, /^Tools\/Vault\/test\//);
});
