// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
// config.routing here is DATA supplied by the caller: Tools/Vault/cli/compat.mjs's policyOf()/legacyConfigOf() load it from vault.config.json's policy.board.routing and pass it in.
import {
  ConfigError,
  projectConfig
} from "./config.mjs";
import { matchesGlob, normalizeChangedPath } from "./guardrails.mjs";
const MODEL_LADDER = ["haiku", "sonnet", "opus"];
function resolveRouting(config, complexity, risk) {
  const base = config.routing.modelByComplexity[complexity];
  const steps = config.routing.riskEscalation[risk];
  const index = MODEL_LADDER.indexOf(base);
  const escalated = MODEL_LADDER[Math.min(index + steps, MODEL_LADDER.length - 1)];
  return { model: escalated, effort: config.routing.effortByComplexity[complexity] };
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
