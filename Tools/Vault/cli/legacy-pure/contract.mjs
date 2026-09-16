// Generated from saas read-only policy helpers; see provenance.json. No DB or network imports.
import { projectConfig } from "./config.mjs";
import { categoryContractError, resolveRules } from "./routing.mjs";
import { parseTicket, validateTicket, danglingBoardReferences, humanOnlyClaims } from "./ticket.mjs";
import { protectedPathsMentioned } from "./guardrails.mjs";
const NEEDS_HUMAN_LABEL = "needs-human", HUMAN_GATE_LABEL = "human-gate";
function evaluateContract(input) {
  const { config, projectKey, categoryName, body, labels } = input;
  const blockers = input.blockers ?? [];
  const categoryError = categoryContractError(config, projectKey, categoryName);
  const parsed = parseTicket(body);
  const settings = projectConfig(config, projectKey);
  const rules = categoryError ? {
    repo: settings.repo,
    baseBranch: settings.baseBranch,
    branching: settings.branching,
    branchPrefix: settings.branchPrefix,
    gates: settings.gates,
    gatesSource: "project",
    conventions: settings.conventions,
    cleanup: settings.cleanup,
    reviewOn: settings.reviewOn,
    workerTools: settings.workerTools,
    push: settings.merge.push,
    decisionLog: settings.decisionLog,
    decisionIdPrefix: settings.decisionIdPrefix
  } : resolveRules({
    config,
    projectKey,
    category: categoryName,
    ticketGates: parsed.gates
  });
  const specError = categoryError ?? validateTicket(parsed, {
    hasConfiguredGates: rules.gates.length > 0,
    categoryError: null
  });
  const epicError = input.issueType === "EPIC" ? "This is an EPIC \u2014 a container, not a unit of work. Its Definition of Done is satisfied by its children finishing, so there is nothing here for a worker to edit and no gate that could tell you it had. Claim a child (`zagent epic <KEY>` lists them), or convert it to a STORY/TASK if the work really does live on this row." : null;
  const needsHumanError = labels.includes(NEEDS_HUMAN_LABEL) ? `${NEEDS_HUMAN_LABEL}: no machine can produce this deliverable \u2014 it needs a person's judgement, not a better runner. Assign it and do it yourself.` : null;
  const mentions = protectedPathsMentioned(
    config.guardrails.protectedPaths,
    parsed.definitionOfDone.map((d) => d.text).join("\n")
  );
  const goalMentions = mentions.length > 0 ? [] : protectedPathsMentioned(config.guardrails.protectedPaths, parsed.goal);
  const reachabilityWarning = goalMentions.length === 0 ? null : `The Goal names ${goalMentions.map((m) => `\`${m.token}\` (protected by \`${m.pattern}\`)`).join(
    ", "
  )} while the Definition of Done names no path. If the deliverable really is behind that path, the loop cannot produce it and nothing here will say so \u2014 label the ticket \`needs-human\` (or \`needs-gpu\`, if a GPU is all it wants) or rewrite the DoD to name what it actually touches.`;
  const reachabilityError = mentions.length === 0 ? null : `The Definition of Done requires editing ${mentions.map((m) => `\`${m.token}\` (protected by \`${m.pattern}\`)`).join(
    ", "
  )}. \`zagent guard\` refuses those paths, so an agent would do the work and Block on the last step. Split the protected half out as a human task, or point the item at a path the loop can reach.`;
  const dangling = input.linked === void 0 ? [] : danglingBoardReferences(parsed.definitionOfDone, input.linked);
  const boardRefWarning = dangling.length === 0 ? null : `The Definition of Done points at board state this ticket does not have: ${dangling.map((d) => `"${d.phrase}" (expects ${d.what})`).join(", ")}. A worker has no board access and no shell, and the item reaches it verbatim \u2014 so a pointer it cannot follow reads as a missing requirement. Link what it means (\`zagent link\`), or say the thing itself in the item.`;
  const unmarkedHumanClaims = humanOnlyClaims(body).filter(
    (claim) => claim.kind === "sign-off" ? !labels.includes(NEEDS_HUMAN_LABEL) && !labels.includes(HUMAN_GATE_LABEL) : !labels.includes(NEEDS_HUMAN_LABEL)
  );
  const markerWarning = unmarkedHumanClaims.length === 0 ? null : `The body says ${unmarkedHumanClaims.map((c) => `"${c.phrase}"`).join(", ")} while the ticket carries ${labels.length === 0 ? "no labels" : `only \`${labels.join("`, `")}\``}. Nothing stops the loop claiming it, and under I3 that sentence is inlined into the worker's prompt verbatim \u2014 telling the one participant that cannot check the board not to do the ticket it was dispatched for. Add \`${NEEDS_HUMAN_LABEL}\`${unmarkedHumanClaims.every((c) => c.kind === "sign-off") ? ` or \`${HUMAN_GATE_LABEL}\`` : ""} if the sentence is true, or delete the sentence if it is not.`;
  const blockedError = blockers.length === 0 ? null : `blocked by ${blockers.map(
    (b) => `${b.key} (${b.status})${b.reason ? ` \u2014 "${b.reason}"` : " \u2014 no reason recorded"}`
  ).join("; ")}. Finish those first; nothing to fix on this ticket. If the reasoning above does not hold, \`zagent unlink\` is the fix rather than editing this ticket.`;
  const reasons = [epicError, needsHumanError, blockedError, reachabilityError, specError].filter(
    (r) => r !== null
  );
  return {
    parsed,
    rules,
    specError,
    needsHumanError,
    epicError,
    reachabilityError,
    reachabilityWarning,
    boardRefWarning,
    markerWarning,
    blockedError,
    contractError: reasons.length === 0 ? null : reasons.join("\n")
  };
}
export {
  evaluateContract
};
