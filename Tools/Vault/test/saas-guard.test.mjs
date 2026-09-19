// Guards against the retired "saas" repo name reappearing anywhere in the
// tree. Two earlier phases plus a final source-file batch removed every
// occurrence; this is the mechanical check that keeps it at zero.
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

// The ONLY known false positive: `MSAASamples` (multisample anti-aliasing,
// a Vulkan config knob) contains the substring "saas" but has nothing to do
// with the retired repo. It lives in vendored ImGui backends/examples under
// `Middleware/` and in the engine's own Vulkan backend. Do not "fix" these --
// widening this allowlist is how a real hit would go unnoticed, so it stays
// a short, explicit list of path prefixes rather than a pattern match.
const ALLOWED_MSAA_PREFIXES = ['Middleware/', 'Zenith/Vulkan/'];

function isAllowedMsaaFalsePositive(relPath) {
  const normalised = relPath.replace(/\\/g, '/');
  return ALLOWED_MSAA_PREFIXES.some(prefix => normalised.startsWith(prefix));
}

// This file necessarily contains the very string it hunts for, so it must
// exempt itself or it fails the moment it is committed.
//
// ★ That is not hypothetical: every run before the first commit PASSED,
// because `git grep` only searches TRACKED files and the new test was still
// untracked. It went red on the next run for no reason connected to the
// repo's actual contents. A check whose result depends on whether it has been
// committed yet is worse than no check, because it looks green exactly when
// you are deciding to commit it.
//
// Derived from `import.meta.url` rather than hardcoded, so renaming or moving
// this file cannot silently re-break it.
const SELF_RELATIVE_PATH = path
  .relative(repoRoot, fileURLToPath(import.meta.url))
  .replace(/\\/g, '/');

function isSelf(relPath) {
  return relPath.replace(/\\/g, '/') === SELF_RELATIVE_PATH;
}

test('no tracked file mentions the retired "saas" repo name', () => {
  let stdout;
  try {
    // -i: case-insensitive. -I: skip files git considers binary.
    // -l: list matching file paths only, one per line.
    stdout = execFileSync('git', ['grep', '-iIl', 'saas'], {
      cwd: repoRoot,
      encoding: 'utf8',
    });
  } catch (error) {
    if (error.code === 'ENOENT') {
      console.warn('[saas-guard] git executable not found on PATH; skipping this check.');
      return;
    }
    if (typeof error.status === 'number' && error.status === 1) {
      // `git grep` exits 1 when no file matched -- that is the clean state.
      stdout = '';
    } else {
      // Anything else (not inside a git repo, git present but broken, etc.)
      // is an environment problem this guard cannot diagnose -- skip rather
      // than fail the suite over something unrelated to the repo's content.
      console.warn(`[saas-guard] could not run \`git grep\`; skipping this check. (${error.message})`);
      return;
    }
  }

  const offenders = stdout
    .split('\n')
    .map(line => line.trim())
    .filter(Boolean)
    .filter(relPath => !isAllowedMsaaFalsePositive(relPath) && !isSelf(relPath));

  assert.deepEqual(
    offenders,
    [],
    `"saas" (case-insensitive) reappeared in tracked file(s):\n${offenders.join('\n')}`,
  );
});

test('the MSAA allowlist itself stays narrow and does not swallow a real hit', () => {
  // Sanity-checks the allowlist predicate in isolation, independent of the
  // repo's current content, so the branch that skips MSAA paths is covered
  // even on a tree where git is unavailable in CI sandboxing.
  assert.equal(isAllowedMsaaFalsePositive('Middleware/imgui-docking/backends/imgui_impl_vulkan.cpp'), true);
  assert.equal(isAllowedMsaaFalsePositive('Zenith/Vulkan/Zenith_Vulkan.cpp'), true);
  assert.equal(isAllowedMsaaFalsePositive('Zenith\\Vulkan\\Zenith_Vulkan.cpp'), true);
  assert.equal(isAllowedMsaaFalsePositive('Tools/Vault/shared/jira/board.js'), false);
  assert.equal(isAllowedMsaaFalsePositive('Zenith/VulkanExtra/not_really_vulkan.cpp'), false);
});

test('the self-exemption matches this file and nothing else', () => {
  // Pins the exemption to exactly one path. If it ever widened to a prefix or
  // a directory it would hide a real hit in a sibling test, which is the only
  // way this guard can fail silently rather than loudly.
  assert.equal(isSelf(SELF_RELATIVE_PATH), true);
  assert.equal(isSelf(SELF_RELATIVE_PATH.replace(/\\/g, '/')), true);
  assert.equal(isSelf('Tools/Vault/test/saas-guard-other.test.mjs'), false);
  assert.equal(isSelf('Tools/Vault/shared/jira/board.js'), false);
  assert.match(SELF_RELATIVE_PATH, /^Tools\/Vault\/test\//);
});
