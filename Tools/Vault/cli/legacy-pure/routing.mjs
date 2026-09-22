// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
// config.routing here is DATA supplied by the caller. MEASURED 2026-09-22
// (Tools/zagent/Test-HarnessParity.ps1, assertion 10) against a live
// vault.config.json rather than assumed from this file's own name-shape:
// Tools/Vault/cli/compat.mjs's legacyConfigOf() returns
// withProjectFileOverlay(policy.resolved, index) whenever policy.resolved is
// non-empty -- which it is in this repo's vault.config.json -- and only
// falls back to `{routing: policy.board.routing, ...}` for a bare test
// fixture that hands policyOf() an empty `resolved` block. So in PRACTICE
// the block actually read and passed to resolveRouting() below is
// policy.resolved.routing, not policy.board.routing as an earlier version
// of this comment claimed. The two blocks are kept equal by hand today
// (Test-HarnessParity.ps1 asserts that too, so a future edit to one and
// not the other reds instead of silently forking which config a fixture
// vs. a real checkout each resolve against).
import {
  ConfigError,
  projectConfig
} from "./config.mjs";
import { matchesGlob, normalizeChangedPath } from "./guardrails.mjs";
const MODEL_LADDER = ["haiku", "sonnet", "opus"];
// `TRIVIAL` was retired from the complexity vocabulary in a concurrent
// change (write-time validation now rejects it), which made the ORIGINAL
// shape of this function unreachable through any validated write path --
// but unreachable-through-validation is not the same as impossible, and a
// caller that hands this a stale or hand-built config (a fixture, a
// migration, a future vocabulary edit) still reaches it directly. The
// original body read `config.routing.modelByComplexity[complexity]` and
// `config.routing.effortByComplexity[complexity]` with no existence check,
// so an unknown complexity produced `{model: undefined, effort: undefined}`
// -- two silently-undefined fields, no thrown error, nothing for the
// caller's own try/catch (see `routingOf` in cli/compat.mjs) to catch. That
// is a FAIL-OPEN: a ticket would dispatch to `undefined` model at
// `undefined` effort rather than being refused. Unknown complexity or risk
// now fails LOUDLY instead -- `routingOf` already wraps this call in
// try/catch and substitutes a safe default on ANY throw, so this change
// only replaces one silent-undefined path with one that caller already
// handles; it adds no new unhandled exception anywhere in the tree (see
// Tools/Vault/test/compat-branches.test.mjs's "fall back when the project
// is not mapped" case, which already relies on that catch for a different
// missing-config shape).
function resolveRouting(config, complexity, risk) {
  const modelByComplexity = config.routing?.modelByComplexity ?? {};
  const effortByComplexity = config.routing?.effortByComplexity ?? {};
  const riskEscalation = config.routing?.riskEscalation ?? {};
  if (!Object.prototype.hasOwnProperty.call(modelByComplexity, complexity)) {
    throw new ConfigError(
      `resolveRouting: unknown complexity "${complexity}" -- not in routing.modelByComplexity (known: ${Object.keys(modelByComplexity).join(", ") || "(none)"}). Refusing to dispatch with an undefined model.`
    );
  }
  if (!Object.prototype.hasOwnProperty.call(riskEscalation, risk)) {
    throw new ConfigError(
      `resolveRouting: unknown risk "${risk}" -- not in routing.riskEscalation (known: ${Object.keys(riskEscalation).join(", ") || "(none)"}). Refusing to dispatch with an undefined escalation.`
    );
  }
  if (!Object.prototype.hasOwnProperty.call(effortByComplexity, complexity)) {
    throw new ConfigError(
      `resolveRouting: unknown complexity "${complexity}" -- not in routing.effortByComplexity (known: ${Object.keys(effortByComplexity).join(", ") || "(none)"}). Refusing to dispatch with an undefined effort.`
    );
  }
  const base = modelByComplexity[complexity];
  const steps = riskEscalation[risk];
  const index = MODEL_LADDER.indexOf(base);
  const escalated = MODEL_LADDER[Math.min(index + steps, MODEL_LADDER.length - 1)];
  return { model: escalated, effort: effortByComplexity[complexity] };
}
// `project.conventions` is UNIVERSAL -- true of every category regardless of
// language ("Root CLAUDE.md is binding", "Never run Sharpmake in a
// worktree") -- and is ALWAYS emitted. `project.languageConventions` is the
// C++-specific half (std:: / Zenith.h / tabs / Zenith_Assert). A category
// with `suppressLanguageConventions: true` (e.g. Tooling, which is Node/
// PowerShell, not C++) drops ONLY the language clause; the universal one and
// its own `conventions` still compose in. Every other category leaves the
// flag unset (default false) and gets universal + language + its own,
// unchanged from before the split.
function resolveRules(input) {
  const project = projectConfig(input.config, input.projectKey);
  const category = resolveCategory(project, input.projectKey, input.category);
  const ticketGates = input.ticketGates ?? null;
  const gatesSource = ticketGates && ticketGates.length > 0 ? "ticket" : category?.gates && category.gates.length > 0 ? "category" : "project";
  const gates = gatesSource === "ticket" ? ticketGates : gatesSource === "category" ? category.gates : project.gates;
  return {
    repo: project.repo,
    baseBranch: project.baseBranch,
    branching: category?.branching ?? project.branching,
    branchPrefix: category?.branchPrefix ?? project.branchPrefix,
    gates,
    gatesSource,
    conventions: concatConventions(
      project.conventions,
      category?.suppressLanguageConventions ? null : project.languageConventions,
      category?.conventions
    ),
    cleanup: category?.cleanup ?? project.cleanup,
    reviewOn: category?.reviewOn ?? project.reviewOn,
    workerTools: category?.workerTools ?? project.workerTools,
    push: project.merge.push,
    decisionLog: category?.decisionLog ?? project.decisionLog,
    decisionIdPrefix: category?.decisionIdPrefix ?? project.decisionIdPrefix
  };
}
function effectiveGates(input) {
  const project = projectConfig(input.config, input.projectKey);
  const gates = [...input.baseGates];
  const unioned = [];
  for (const [name, category] of Object.entries(project.categories ?? {})) {
    if (name === input.category) continue;
    const patterns = category.paths ?? [];
    if (patterns.length === 0) continue;
    const categoryGates = category.gates ?? [];
    if (categoryGates.length === 0) continue;
    const matched = input.changedPaths.filter(
      (path) => patterns.some((pattern) => matchesGlob(normalizeChangedPath(path), pattern))
    );
    if (matched.length === 0) continue;
    const added = categoryGates.filter((line) => !gates.includes(line));
    gates.push(...added);
    unioned.push({ category: name, matched, gates: categoryGates });
  }
  return { gates, base: input.baseGates, unioned };
}
function concatConventions(...parts) {
  return parts.filter((s) => s && s.trim().length > 0).join("\n\n");
}
function categoryContractError(config, projectKey, category) {
  const project = projectConfig(config, projectKey);
  const configured = project.categories;
  if (!configured) return null;
  const names = Object.keys(configured);
  if (!category) {
    return `Project ${projectKey} splits work by category, so a ticket must carry one before the agent can pick gates for it. Configured categories: ${names.join(", ")}.`;
  }
  if (!configured[category]) {
    return `Category "${category}" has no entry in agent.config.json for project ${projectKey}. Configured categories: ${names.join(", ")}.`;
  }
  return null;
}
function resolveCategory(project, projectKey, category) {
  if (!project.categories) return null;
  if (!category) {
    throw new ConfigError(
      `Project ${projectKey} configures categories but the ticket is uncategorized \u2014 the contract check should have refused it before routing.`
    );
  }
  const entry = project.categories[category];
  if (!entry) {
    throw new ConfigError(
      `Category "${category}" is not configured for project ${projectKey} \u2014 the contract check should have refused it before routing.`
    );
  }
  return entry;
}
export {
  MODEL_LADDER,
  categoryContractError,
  effectiveGates,
  resolveRouting,
  resolveRules
};
