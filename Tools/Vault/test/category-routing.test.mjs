// Exercises the REAL selector in cli/legacy-pure/routing.mjs -- resolveRules()
// and effectiveGates() -- against both a small synthetic config (to pin the
// exact mechanism) and the repo's own committed zagent.project.json (to catch
// drift in the real category data). Nothing here reimplements matchesGlob's
// globbing; every glob-matching assertion goes through the real function.
//
// Covers three zagent.project.json fixes, all routed through routing.mjs:
//
//  (A) The root `conventions` string was split into a UNIVERSAL part
//      (`conventions` -- true of every category, e.g. "Root CLAUDE.md is
//      binding", "Never run Sharpmake in a worktree") and a LANGUAGE part
//      (`languageConventions` -- the C++-specific rules). A category with
//      `suppressLanguageConventions: true` (Tooling) drops ONLY the
//      language part; the universal clauses and its own conventions still
//      compose in. Every other category keeps getting universal + language
//      + its own (flag unset/false, unchanged from before the split).
//  (B) Engine now declares its own `decisionLog` / `decisionIdPrefix`
//      (`Zenith/Docs/EngineDecisionLog.md`, `ZEN-D-`) instead of falling
//      through to an undefined project-level default.
//  (C) `Build/**` joined the Engine category's `paths`, so a Sharpmake-only
//      diff unions in the Engine gate list via effectiveGates().
import { existsSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, test } from 'vitest';

import { effectiveGates, resolveRules } from '../cli/legacy-pure/routing.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const repoRoot = path.resolve(here, '../../..');

function stripCommentKeys(value) {
  if (Array.isArray(value)) return value.map(stripCommentKeys);
  if (!value || typeof value !== 'object') return value;
  const out = {};
  for (const [key, inner] of Object.entries(value)) {
    if (key.startsWith('$')) continue;
    out[key] = stripCommentKeys(inner);
  }
  return out;
}

/**
 * The committed zagent.project.json, read fresh and stripped of `$`-prefixed
 * comment keys -- the same transform cli/compat.mjs applies before handing
 * the file to routing.mjs. Its top-level fields (conventions, categories,
 * ...) are exactly what PROJECT_FILE_OWNED overlays onto a project's routing
 * config in production, so wrapping it as `{ projects: { ZEN: <file> } }`
 * gives resolveRules()/effectiveGates() the real shape without needing a
 * full vault.config.json snapshot.
 */
function realConfig() {
  const raw = JSON.parse(readFileSync(path.join(repoRoot, 'zagent.project.json'), 'utf8'));
  return { projects: { ZEN: stripCommentKeys(raw) } };
}

function rulesFor(config, category, ticketGates = null) {
  return resolveRules({ config, projectKey: 'ZEN', category, ticketGates });
}

// ── (A) suppressLanguageConventions -- synthetic config, pins the mechanism ─
describe('suppressLanguageConventions', () => {
  function configWith(categories) {
    return {
      projects: {
        ZEN: {
          conventions: 'UNIVERSAL RULES',
          languageConventions: 'CPP RULES',
          gates: [],
          merge: { push: false },
          categories,
        },
      },
    };
  }

  test('a suppressing category gets universal + its own, but NOT the language clause', () => {
    const config = configWith({
      Tooling: { suppressLanguageConventions: true, conventions: 'TOOLING RULES', gates: ['g'] },
    });
    const rules = rulesFor(config, 'Tooling');
    expect(rules.conventions).toBe('UNIVERSAL RULES\n\nTOOLING RULES');
    expect(rules.conventions).toContain('UNIVERSAL RULES');
    expect(rules.conventions).not.toContain('CPP RULES');
  });

  test('a category with the flag unset (default false) still gets universal + language + its own', () => {
    const config = configWith({
      Zenithmon: { conventions: 'ZM RULES', gates: ['g'] },
    });
    const rules = rulesFor(config, 'Zenithmon');
    expect(rules.conventions).toBe('UNIVERSAL RULES\n\nCPP RULES\n\nZM RULES');
  });

  test('a category with the flag explicitly false behaves identically to unset', () => {
    const config = configWith({
      Zenithmon: { suppressLanguageConventions: false, conventions: 'ZM RULES', gates: ['g'] },
    });
    const rules = rulesFor(config, 'Zenithmon');
    expect(rules.conventions).toBe('UNIVERSAL RULES\n\nCPP RULES\n\nZM RULES');
  });

  test('a suppressing category with no conventions text of its own still keeps the universal clause', () => {
    const config = configWith({
      Bare: { suppressLanguageConventions: true, gates: ['g'] },
    });
    const rules = rulesFor(config, 'Bare');
    expect(rules.conventions).toBe('UNIVERSAL RULES');
  });

  test('real file: Tooling conventions carry the universal clauses but not the C++ language clause', () => {
    const rules = rulesFor(realConfig(), 'Tooling');
    // Universal: kept.
    expect(rules.conventions).toMatch(/Root CLAUDE\.md is binding/);
    expect(rules.conventions).toMatch(/Never run Sharpmake in a linked worktree unless it is a registered lane/);
    // Language: suppressed.
    expect(rules.conventions).not.toMatch(/No std::/);
    expect(rules.conventions).not.toMatch(/Zenith_Assert is the real assert/);
    // Its own.
    expect(rules.conventions).toMatch(/Tools\/Vault is Node/);
  });

  test('real file: Engine (flag unset) opens with the universal clauses, then the language clause, then its own', () => {
    const rules = rulesFor(realConfig(), 'Engine');
    const universalIdx = rules.conventions.indexOf('Root CLAUDE.md is binding');
    const languageIdx = rules.conventions.indexOf('No std::');
    const ownIdx = rules.conventions.indexOf('Engine work moves EVERY game');
    expect(universalIdx).toBeGreaterThanOrEqual(0);
    expect(languageIdx).toBeGreaterThan(universalIdx);
    expect(ownIdx).toBeGreaterThan(languageIdx);
  });

  test('real file: Zenithmon (flag unset) still gets universal + language + its own', () => {
    const rules = rulesFor(realConfig(), 'Zenithmon');
    expect(rules.conventions.startsWith('Root CLAUDE.md is binding.')).toBe(true);
    expect(rules.conventions).toMatch(/No std::/);
    expect(rules.conventions).toMatch(/ZM_ prefix/);
  });
});

// ── (B) Engine decisionLog / decisionIdPrefix ──────────────────────────────
describe('Engine decisionLog', () => {
  test('resolves a decisionLog and decisionIdPrefix for the Engine category', () => {
    const rules = rulesFor(realConfig(), 'Engine');
    expect(rules.decisionLog).toBe('Zenith/Docs/EngineDecisionLog.md');
    expect(rules.decisionIdPrefix).toBe('ZEN-D-');
  });

  test('the declared decisionLog file actually exists (never point at a file that is not there)', () => {
    const rules = rulesFor(realConfig(), 'Engine');
    expect(existsSync(path.join(repoRoot, rules.decisionLog))).toBe(true);
  });

  test('Zenithmon keeps its own pre-existing decisionLog untouched', () => {
    const rules = rulesFor(realConfig(), 'Zenithmon');
    expect(rules.decisionLog).toBe('Games/Zenithmon/Docs/DecisionLog.md');
    expect(rules.decisionIdPrefix).toBe('ZM-D-');
    expect(existsSync(path.join(repoRoot, rules.decisionLog))).toBe(true);
  });

  test('a category with no declared decisionLog and no project-level fallback resolves undefined, not a wrong prefix', () => {
    // Deliberate: no top-level project decisionLog/decisionIdPrefix exists in
    // zagent.project.json (see that file's Engine $decisionLog comment for
    // why one was not added) -- so Tooling, which files no decisions of its
    // own, must fall through to undefined rather than silently inheriting
    // Engine's or Zenithmon's prefix.
    const rules = rulesFor(realConfig(), 'Tooling');
    expect(rules.decisionLog).toBeUndefined();
    expect(rules.decisionIdPrefix).toBeUndefined();
  });

  test('synthetic: category decisionLog wins over an undefined project-level default', () => {
    const config = {
      projects: {
        ZEN: {
          gates: [],
          merge: { push: false },
          categories: {
            Engine: { decisionLog: 'Zenith/Docs/EngineDecisionLog.md', decisionIdPrefix: 'ZEN-D-', gates: ['g'] },
          },
        },
      },
    };
    const rules = rulesFor(config, 'Engine');
    expect(rules.decisionLog).toBe('Zenith/Docs/EngineDecisionLog.md');
    expect(rules.decisionIdPrefix).toBe('ZEN-D-');
  });
});

// ── (C) Build/** joins the Engine category's paths ─────────────────────────
describe('Build/** union into Engine gates', () => {
  function unionFor(config, category, changedPaths) {
    const rules = rulesFor(config, category);
    return effectiveGates({ config, projectKey: 'ZEN', category, baseGates: rules.gates, changedPaths });
  }

  test('synthetic: a Build/-only diff filed under a foreign category unions in Engine', () => {
    const config = {
      projects: {
        ZEN: {
          gates: [],
          merge: { push: false },
          categories: {
            Engine: { paths: ['Zenith/**', 'Build/**'], gates: ['engine-gate'] },
            Zenithmon: { paths: ['Games/Zenithmon/**'], gates: ['zm-gate'] },
          },
        },
      },
    };
    const result = unionFor(config, 'Zenithmon', ['Build/Sharpmake_Zenith.cs']);
    expect(result.unioned.map((u) => u.category)).toContain('Engine');
    expect(result.gates).toContain('engine-gate');
  });

  test('synthetic: without Build/** in Engine paths, the same diff unions in nothing', () => {
    const config = {
      projects: {
        ZEN: {
          gates: [],
          merge: { push: false },
          categories: {
            Engine: { paths: ['Zenith/**'], gates: ['engine-gate'] },
            Zenithmon: { paths: ['Games/Zenithmon/**'], gates: ['zm-gate'] },
          },
        },
      },
    };
    const result = unionFor(config, 'Zenithmon', ['Build/Sharpmake_Zenith.cs']);
    expect(result.unioned).toEqual([]);
    expect(result.gates).not.toContain('engine-gate');
  });

  test('real file: Build/Sharpmake_Zenith.cs-only diff, filed under Zenithmon, unions in the Engine gate list', () => {
    const config = realConfig();
    const result = unionFor(config, 'Zenithmon', ['Build/Sharpmake_Zenith.cs']);
    expect(result.unioned.map((u) => u.category)).toContain('Engine');
    // Measured 2026-09-20 before this fix: base 5, total 5, unioned [] --
    // a Build-only diff pulled in nothing. After: it matches the same union
    // a Zenith/** file gets.
    const engineGateCount = rulesFor(config, 'Engine').gates.length;
    expect(result.gates.length).toBeGreaterThanOrEqual(engineGateCount);
  });

  test('real file: Build/-only diff resolves the identical union a Zenith/** diff gets', () => {
    const config = realConfig();
    const buildResult = unionFor(config, 'Zenithmon', ['Build/Sharpmake_Zenith.cs']);
    const zenithResult = unionFor(config, 'Zenithmon', ['Zenith/ZenithECS/Zenith_Scene.h']);
    expect(buildResult.gates.length).toBe(zenithResult.gates.length);
    expect(buildResult.unioned.map((u) => u.category).sort())
      .toEqual(zenithResult.unioned.map((u) => u.category).sort());
  });

  test('real file: a Tools/Vault-only diff does NOT union in Engine (Build/** must not swallow Tools/**)', () => {
    const config = realConfig();
    const result = unionFor(config, 'Zenithmon', ['Tools/Vault/cli/client.mjs']);
    expect(result.unioned.map((u) => u.category)).not.toContain('Engine');
    expect(result.unioned.map((u) => u.category)).toContain('Tooling');
  });
});
