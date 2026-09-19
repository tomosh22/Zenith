// Ported from the upstream board application's packages/lib/src/__tests__/workflow.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  applyTransition,
  availableTransitions,
  conditionError,
  DEFAULT_RESOLUTION,
  evaluateTransition,
  findTransition,
  JIRA_ORG_ROLES,
  legalTargetStatusIds,
  orgRoleRank,
  resolutionInvariantError,
  transitionError,
  TRANSITION_CONDITIONS,
  TRANSITION_POST_FUNCTIONS,
  TRANSITION_RULE_KINDS,
  TRANSITION_RULE_LABELS,
  TRANSITION_RULE_TYPES,
  TRANSITION_REQUIRED_FIELDS,
  TRANSITION_VALIDATORS,
  transitionNeedsResolution,
  transitionRuleError,
  transitionsFrom
} from "../../shared/jira/workflow.js";
const TODO = "st_todo";
const READY = "st_ready";
const PROGRESS = "st_progress";
const REVIEW = "st_review";
const DONE = "st_done";
const BLOCKED = "st_blocked";
function edge(from, to, rules = [], id = `${from ?? "any"}->${to}`) {
  return { id, name: id, fromStatusId: from, toStatusId: to, rules };
}
function ctx(overrides = {}, actorRole = "MEMBER") {
  return {
    actor: {
      id: "u_actor",
      orgRole: actorRole,
      projectRoleIds: []
    },
    issue: {
      key: "ZM-7",
      assigneeId: null,
      reporterId: null,
      storyPoints: null,
      dueDate: null,
      severity: null,
      reproducibility: null,
      environment: null,
      categoryId: null,
      description: null,
      resolution: null,
      commentCount: 0,
      openBlockerKeys: [],
      unresolvedChildKeys: [],
      ...overrides
    }
  };
}
describe("rule vocabulary", () => {
  it("maps every kind to its own type list", () => {
    expect(TRANSITION_RULE_TYPES.CONDITION).toBe(TRANSITION_CONDITIONS);
    expect(TRANSITION_RULE_TYPES.VALIDATOR).toBe(TRANSITION_VALIDATORS);
    expect(TRANSITION_RULE_TYPES.POST_FUNCTION).toBe(TRANSITION_POST_FUNCTIONS);
  });
  it("covers every kind exactly once", () => {
    expect(Object.keys(TRANSITION_RULE_TYPES).sort()).toEqual([...TRANSITION_RULE_KINDS].sort());
  });
  it("labels every rule type", () => {
    for (const kind of TRANSITION_RULE_KINDS) {
      for (const type of TRANSITION_RULE_TYPES[kind]) {
        expect(TRANSITION_RULE_LABELS[type], `${type} has no label`).toBeTruthy();
      }
    }
  });
  it("labels every required-field option", () => {
    for (const field of TRANSITION_REQUIRED_FIELDS) {
      expect(TRANSITION_RULE_TYPES.VALIDATOR).toContain("FIELD_REQUIRED");
      expect(field.length).toBeGreaterThan(0);
    }
  });
  it("orders org roles least- to most-privileged", () => {
    expect(JIRA_ORG_ROLES).toEqual(["VIEWER", "MEMBER", "ADMIN", "OWNER"]);
    expect(orgRoleRank("VIEWER")).toBeLessThan(orgRoleRank("MEMBER"));
    expect(orgRoleRank("MEMBER")).toBeLessThan(orgRoleRank("ADMIN"));
    expect(orgRoleRank("ADMIN")).toBeLessThan(orgRoleRank("OWNER"));
  });
});
describe("transitionRuleError", () => {
  it("accepts a well-formed rule of each kind", () => {
    expect(
      transitionRuleError({ kind: "CONDITION", type: "ORG_ROLE", config: { role: "ADMIN" } })
    ).toBeNull();
    expect(
      transitionRuleError({
        kind: "VALIDATOR",
        type: "FIELD_REQUIRED",
        config: { field: "assignee" }
      })
    ).toBeNull();
    expect(
      transitionRuleError({
        kind: "POST_FUNCTION",
        type: "SET_RESOLUTION",
        config: { resolution: "WONT_DO" }
      })
    ).toBeNull();
  });
  it("refuses a type that belongs to another kind", () => {
    expect(transitionRuleError({ kind: "CONDITION", type: "SET_RESOLUTION", config: {} })).toMatch(
      /not a valid CONDITION/
    );
  });
  it("refuses an unknown kind", () => {
    expect(
      transitionRuleError({
        kind: "NONSENSE",
        type: "ORG_ROLE",
        config: {}
      })
    ).toMatch(/Unknown rule kind/);
  });
  it("refuses ORG_ROLE with a missing or unknown role", () => {
    expect(transitionRuleError({ kind: "CONDITION", type: "ORG_ROLE", config: {} })).toMatch(
      /needs a role/
    );
    expect(
      transitionRuleError({ kind: "CONDITION", type: "ORG_ROLE", config: { role: "GOD" } })
    ).toMatch(/needs a role/);
  });
  it("refuses PROJECT_ROLE with no roleId", () => {
    expect(transitionRuleError({ kind: "CONDITION", type: "PROJECT_ROLE", config: {} })).toMatch(
      /needs a roleId/
    );
    expect(
      transitionRuleError({ kind: "CONDITION", type: "PROJECT_ROLE", config: { roleId: "" } })
    ).toMatch(/needs a roleId/);
  });
  it("refuses FIELD_REQUIRED naming a field no issue has", () => {
    expect(
      transitionRuleError({ kind: "VALIDATOR", type: "FIELD_REQUIRED", config: { field: "vibes" } })
    ).toMatch(/needs a field/);
    expect(transitionRuleError({ kind: "VALIDATOR", type: "FIELD_REQUIRED", config: {} })).toMatch(
      /needs a field/
    );
  });
  it("refuses SET_RESOLUTION with an unknown resolution", () => {
    expect(
      transitionRuleError({
        kind: "POST_FUNCTION",
        type: "SET_RESOLUTION",
        config: { resolution: "SHIPPED" }
      })
    ).toMatch(/needs a resolution/);
  });
  it("refuses SET_FLAGGED without a boolean", () => {
    expect(
      transitionRuleError({
        kind: "POST_FUNCTION",
        type: "SET_FLAGGED",
        config: { flagged: "yes" }
      })
    ).toMatch(/boolean/);
  });
  it("accepts the config-free rule types", () => {
    for (const type of ["ASSIGNEE_ONLY", "REPORTER_ONLY"]) {
      expect(transitionRuleError({ kind: "CONDITION", type, config: {} })).toBeNull();
    }
    for (const type of [
      "NO_OPEN_BLOCKERS",
      "NO_UNRESOLVED_CHILDREN",
      "COMMENT_REQUIRED"
    ]) {
      expect(transitionRuleError({ kind: "VALIDATOR", type, config: {} })).toBeNull();
    }
    for (const type of ["CLEAR_RESOLUTION", "ASSIGN_TO_ACTOR", "CLEAR_ASSIGNEE"]) {
      expect(transitionRuleError({ kind: "POST_FUNCTION", type, config: {} })).toBeNull();
    }
  });
});
describe("conditionError", () => {
  const orgRole = {
    kind: "CONDITION",
    type: "ORG_ROLE",
    config: { role: "ADMIN" }
  };
  it("passes when the actor outranks the requirement", () => {
    expect(conditionError(orgRole, ctx({}, "OWNER"))).toBeNull();
    expect(conditionError(orgRole, ctx({}, "ADMIN"))).toBeNull();
  });
  it("fails when the actor is below it", () => {
    expect(conditionError(orgRole, ctx({}, "MEMBER"))).toMatch(/ADMIN role or higher/);
    expect(conditionError(orgRole, ctx({}, "VIEWER"))).toMatch(/ADMIN role or higher/);
  });
  it("checks project-role membership", () => {
    const rule = {
      kind: "CONDITION",
      type: "PROJECT_ROLE",
      config: { roleId: "role_lead" }
    };
    const held = ctx();
    held.actor.projectRoleIds = ["role_lead"];
    expect(conditionError(rule, held)).toBeNull();
    expect(conditionError(rule, ctx())).toMatch(/project role/);
  });
  it("checks assignee and reporter identity", () => {
    const assignee = { kind: "CONDITION", type: "ASSIGNEE_ONLY", config: {} };
    const reporter = { kind: "CONDITION", type: "REPORTER_ONLY", config: {} };
    expect(conditionError(assignee, ctx({ assigneeId: "u_actor" }))).toBeNull();
    expect(conditionError(assignee, ctx({ assigneeId: "u_other" }))).toMatch(/assignee/);
    expect(conditionError(assignee, ctx({ assigneeId: null }))).toMatch(/assignee/);
    expect(conditionError(reporter, ctx({ reporterId: "u_actor" }))).toBeNull();
    expect(conditionError(reporter, ctx({ reporterId: "u_other" }))).toMatch(/reporter/);
  });
  it("ignores a rule type it does not know", () => {
    expect(conditionError({ kind: "CONDITION", type: "FUTURE", config: {} }, ctx())).toBeNull();
  });
});
describe("validatorError", () => {
  it('names the blockers rather than saying "blocked"', () => {
    const t = edge(PROGRESS, DONE, [{ kind: "VALIDATOR", type: "NO_OPEN_BLOCKERS", config: {} }]);
    const error = transitionError(t, ctx({ openBlockerKeys: ["ZM-20", "ZM-21"] }));
    expect(error).toContain("ZM-20");
    expect(error).toContain("ZM-21");
    expect(transitionError(t, ctx())).toBeNull();
  });
  it("names unfinished children", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "VALIDATOR", type: "NO_UNRESOLVED_CHILDREN", config: {} }
    ]);
    expect(transitionError(t, ctx({ unresolvedChildKeys: ["ZM-8"] }))).toContain("ZM-8");
    expect(transitionError(t, ctx())).toBeNull();
  });
  it("requires a comment when asked", () => {
    const t = edge(PROGRESS, DONE, [{ kind: "VALIDATOR", type: "COMMENT_REQUIRED", config: {} }]);
    expect(transitionError(t, ctx({ commentCount: 0 }))).toMatch(/comment is required/);
    expect(transitionError(t, ctx({ commentCount: 1 }))).toBeNull();
  });
  it.each([
    ["assignee", { assigneeId: "u_1" }],
    ["storyPoints", { storyPoints: 3 }],
    ["dueDate", { dueDate: /* @__PURE__ */ new Date("2026-01-01") }],
    ["severity", { severity: "S2_MAJOR" }],
    ["reproducibility", { reproducibility: "ALWAYS" }],
    ["environment", { environment: "Null_vs2022_Debug_Win64_True" }],
    ["category", { categoryId: "cat_1" }],
    ["description", { description: "why" }]
  ])("FIELD_REQUIRED reads %s off the issue", (field, set) => {
    const t = edge(PROGRESS, DONE, [
      { kind: "VALIDATOR", type: "FIELD_REQUIRED", config: { field } }
    ]);
    expect(transitionError(t, ctx())).toMatch(/must be set first/);
    expect(transitionError(t, ctx(set))).toBeNull();
  });
  it("treats zero story points as set", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "VALIDATOR", type: "FIELD_REQUIRED", config: { field: "storyPoints" } }
    ]);
    expect(transitionError(t, ctx({ storyPoints: 0 }))).toBeNull();
  });
  it("treats an empty string as unset", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "VALIDATOR", type: "FIELD_REQUIRED", config: { field: "environment" } }
    ]);
    expect(transitionError(t, ctx({ environment: "" }))).toMatch(/must be set first/);
  });
});
describe("evaluateTransition", () => {
  it("reports conditions and validators separately", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "CONDITION", type: "ORG_ROLE", config: { role: "ADMIN" } },
      { kind: "VALIDATOR", type: "NO_OPEN_BLOCKERS", config: {} }
    ]);
    const verdict = evaluateTransition(t, ctx({ openBlockerKeys: ["ZM-1"] }, "MEMBER"));
    expect(verdict.conditionError).toMatch(/ADMIN/);
    expect(verdict.validatorError).toMatch(/ZM-1/);
  });
  it("prefers the condition reason when both fail", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "CONDITION", type: "ORG_ROLE", config: { role: "ADMIN" } },
      { kind: "VALIDATOR", type: "NO_OPEN_BLOCKERS", config: {} }
    ]);
    expect(transitionError(t, ctx({ openBlockerKeys: ["ZM-1"] }, "MEMBER"))).toMatch(/ADMIN/);
  });
  it("passes a transition with no rules at all", () => {
    expect(transitionError(edge(TODO, PROGRESS), ctx())).toBeNull();
  });
});
describe("graph lookup", () => {
  const defs = [
    edge(TODO, READY),
    edge(READY, PROGRESS),
    edge(PROGRESS, REVIEW),
    edge(REVIEW, DONE),
    edge(null, BLOCKED, [], "global-blocked")
  ];
  it("includes global transitions from every status", () => {
    expect(
      transitionsFrom(defs, TODO).map((t) => t.toStatusId).sort()
    ).toEqual([BLOCKED, READY].sort());
    expect(
      transitionsFrom(defs, REVIEW).map((t) => t.toStatusId).sort()
    ).toEqual([BLOCKED, DONE].sort());
  });
  it("excludes a self-transition, including via a global edge", () => {
    expect(transitionsFrom(defs, BLOCKED).map((t) => t.toStatusId)).not.toContain(BLOCKED);
  });
  it("finds a legal edge and refuses an illegal one", () => {
    expect(findTransition(defs, TODO, READY)?.toStatusId).toBe(READY);
    expect(findTransition(defs, TODO, DONE)).toBeNull();
    expect(findTransition(defs, TODO, BLOCKED)?.id).toBe("global-blocked");
  });
  it("prefers an exact edge over a global one to the same status", () => {
    const specific = edge(REVIEW, BLOCKED, [
      { kind: "CONDITION", type: "ORG_ROLE", config: { role: "ADMIN" } }
    ]);
    const both = [...defs, specific];
    expect(findTransition(both, REVIEW, BLOCKED)?.id).toBe(specific.id);
    expect(findTransition(both, TODO, BLOCKED)?.id).toBe("global-blocked");
  });
});
describe("availableTransitions", () => {
  const defs = [
    edge(PROGRESS, REVIEW),
    edge(PROGRESS, DONE, [{ kind: "CONDITION", type: "ORG_ROLE", config: { role: "ADMIN" } }]),
    edge(PROGRESS, BLOCKED, [{ kind: "VALIDATOR", type: "COMMENT_REQUIRED", config: {} }])
  ];
  it("hides transitions the actor may not make", () => {
    const asMember = availableTransitions(defs, PROGRESS, ctx({}, "MEMBER"));
    expect(asMember.map((v) => v.transition.toStatusId)).toEqual([REVIEW, BLOCKED]);
    const asAdmin = availableTransitions(defs, PROGRESS, ctx({}, "ADMIN"));
    expect(asAdmin.map((v) => v.transition.toStatusId)).toEqual([REVIEW, DONE, BLOCKED]);
  });
  it("keeps a validator failure with its reason so a caller can explain it", () => {
    const blocked = availableTransitions(defs, PROGRESS, ctx({}, "MEMBER")).find(
      (v) => v.transition.toStatusId === BLOCKED
    );
    expect(blocked?.validatorError).toMatch(/comment is required/);
  });
  it("legalTargetStatusIds drops the ones that would fail", () => {
    expect(legalTargetStatusIds(defs, PROGRESS, ctx({}, "MEMBER"))).toEqual([REVIEW]);
    expect(legalTargetStatusIds(defs, PROGRESS, ctx({ commentCount: 2 }, "MEMBER"))).toEqual([
      REVIEW,
      BLOCKED
    ]);
  });
  it("offers each target once when an exact and a global edge overlap", () => {
    const overlapping = [edge(PROGRESS, BLOCKED, [], "exact"), edge(null, BLOCKED, [], "global")];
    const out = availableTransitions(overlapping, PROGRESS, ctx());
    expect(out).toHaveLength(1);
    expect(out[0].transition.id).toBe("exact");
  });
});
describe("resolutionInvariantError", () => {
  it("refuses Done with no resolution \u2014 the Jira pitfall", () => {
    expect(resolutionInvariantError("DONE", null)).toMatch(/must carry a resolution/);
  });
  it("refuses a resolution outside a Done status", () => {
    expect(resolutionInvariantError("TODO", "DONE")).toMatch(/Only an issue in a Done status/);
    expect(resolutionInvariantError("IN_PROGRESS", "WONT_DO")).toMatch(/Only an issue in a Done/);
  });
  it("accepts the two consistent states", () => {
    expect(resolutionInvariantError("DONE", "WONT_DO")).toBeNull();
    expect(resolutionInvariantError("TODO", null)).toBeNull();
    expect(resolutionInvariantError("IN_PROGRESS", null)).toBeNull();
  });
});
describe("transitionNeedsResolution", () => {
  it("is true only for a Done target the workflow does not decide", () => {
    expect(transitionNeedsResolution(edge(PROGRESS, DONE), "DONE")).toBe(true);
    expect(transitionNeedsResolution(edge(PROGRESS, REVIEW), "IN_PROGRESS")).toBe(false);
    expect(
      transitionNeedsResolution(
        edge(PROGRESS, DONE, [
          { kind: "POST_FUNCTION", type: "SET_RESOLUTION", config: { resolution: "DONE" } }
        ]),
        "DONE"
      )
    ).toBe(false);
  });
});
describe("applyTransition", () => {
  it("returns the error and no field changes when a rule fails", () => {
    const t = edge(PROGRESS, DONE, [{ kind: "VALIDATOR", type: "NO_OPEN_BLOCKERS", config: {} }]);
    const result = applyTransition(t, "DONE", ctx({ openBlockerKeys: ["ZM-2"] }));
    expect(result.error).toContain("ZM-2");
    expect(result.patch.resolution).toBeUndefined();
  });
  it("applies the workflow resolution", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "POST_FUNCTION", type: "SET_RESOLUTION", config: { resolution: "WONT_DO" } }
    ]);
    const result = applyTransition(t, "DONE", ctx());
    expect(result.error).toBeNull();
    expect(result.patch).toEqual({ statusId: DONE, resolution: "WONT_DO" });
  });
  it("prefers the workflow resolution over the caller choice", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "POST_FUNCTION", type: "SET_RESOLUTION", config: { resolution: "WONT_DO" } }
    ]);
    const result = applyTransition(t, "DONE", ctx(), { resolutionChoice: "DUPLICATE" });
    expect(result.patch.resolution).toBe("WONT_DO");
  });
  it("uses the caller choice when the workflow has none", () => {
    const result = applyTransition(edge(PROGRESS, DONE), "DONE", ctx(), {
      resolutionChoice: "DUPLICATE"
    });
    expect(result.patch.resolution).toBe("DUPLICATE");
  });
  it("refuses a caller choice that is not a resolution", () => {
    const result = applyTransition(edge(PROGRESS, DONE), "DONE", ctx(), {
      resolutionChoice: "SHIPPED"
    });
    expect(result.error).toMatch(/not a resolution/);
  });
  it("falls back to the default so Done is never unresolved", () => {
    const result = applyTransition(edge(PROGRESS, DONE), "DONE", ctx());
    expect(result.patch.resolution).toBe(DEFAULT_RESOLUTION);
  });
  it("overrules a CLEAR_RESOLUTION aimed into a Done status", () => {
    const t = edge(PROGRESS, DONE, [
      { kind: "POST_FUNCTION", type: "CLEAR_RESOLUTION", config: {} }
    ]);
    const result = applyTransition(t, "DONE", ctx());
    expect(result.patch.resolution).toBe(DEFAULT_RESOLUTION);
  });
  it("clears the resolution on the way back out of Done", () => {
    const result = applyTransition(
      edge(DONE, PROGRESS),
      "IN_PROGRESS",
      ctx({ resolution: "DONE" })
    );
    expect(result.patch.resolution).toBeNull();
  });
  it("leaves resolution out of the patch when it did not move", () => {
    const stay = applyTransition(edge(TODO, PROGRESS), "IN_PROGRESS", ctx({ resolution: null }));
    expect(stay.patch).toEqual({ statusId: PROGRESS });
    const keep = applyTransition(edge(DONE, BLOCKED), "DONE", ctx({ resolution: "DONE" }));
    expect(keep.patch.resolution).toBeUndefined();
  });
  it("applies the assignee and flag post-functions", () => {
    const assign = applyTransition(
      edge(TODO, PROGRESS, [{ kind: "POST_FUNCTION", type: "ASSIGN_TO_ACTOR", config: {} }]),
      "IN_PROGRESS",
      ctx()
    );
    expect(assign.patch.assigneeId).toBe("u_actor");
    const clear = applyTransition(
      edge(PROGRESS, TODO, [{ kind: "POST_FUNCTION", type: "CLEAR_ASSIGNEE", config: {} }]),
      "TODO",
      ctx({ assigneeId: "u_actor" })
    );
    expect(clear.patch.assigneeId).toBeNull();
    const flag = applyTransition(
      edge(PROGRESS, BLOCKED, [
        { kind: "POST_FUNCTION", type: "SET_FLAGGED", config: { flagged: true } }
      ]),
      "TODO",
      ctx()
    );
    expect(flag.patch.flagged).toBe(true);
  });
  it("ignores a post-function type it does not know", () => {
    const result = applyTransition(
      edge(TODO, PROGRESS, [{ kind: "POST_FUNCTION", type: "FUTURE", config: {} }]),
      "IN_PROGRESS",
      ctx()
    );
    expect(result.error).toBeNull();
    expect(result.patch).toEqual({ statusId: PROGRESS });
  });
  it("produces a state the invariant accepts, for every category", () => {
    const cases = [
      [edge(TODO, DONE), "DONE", null],
      [edge(TODO, DONE), "DONE", "WONT_DO"],
      [edge(DONE, TODO), "TODO", "DONE"],
      [edge(DONE, PROGRESS), "IN_PROGRESS", "DUPLICATE"]
    ];
    for (const [t, category, current] of cases) {
      const before = ctx({ resolution: current });
      const { patch, error } = applyTransition(t, category, before);
      expect(error).toBeNull();
      const after = patch.resolution === void 0 ? current : patch.resolution;
      expect(resolutionInvariantError(category, after)).toBeNull();
    }
  });
});
import { transitionNeedsScreen } from "../../shared/jira/workflow.js";
describe("transitionNeedsScreen", () => {
  it("is false when no screen is attached", () => {
    expect(transitionNeedsScreen(edge(TODO, PROGRESS))).toBe(false);
    expect(transitionNeedsScreen({ ...edge(TODO, PROGRESS), screenId: null })).toBe(false);
  });
  it("is true when one is", () => {
    expect(transitionNeedsScreen({ ...edge(TODO, PROGRESS), screenId: "sc1" })).toBe(true);
  });
  it("is independent of the resolution prompt", () => {
    const t = { ...edge(PROGRESS, DONE), screenId: "sc1" };
    expect(transitionNeedsScreen(t)).toBe(true);
    expect(transitionNeedsResolution(t, "DONE")).toBe(true);
  });
});
describe("FIELD_REQUIRED naming a custom field", () => {
  const customRule = (customFieldId) => ({
    kind: "VALIDATOR",
    type: "FIELD_REQUIRED",
    config: { customFieldId }
  });
  it("accepts a custom field id at write time", () => {
    expect(transitionRuleError(customRule("f1"))).toBeNull();
  });
  it("refuses an empty custom field id", () => {
    expect(
      transitionRuleError({
        kind: "VALIDATOR",
        type: "FIELD_REQUIRED",
        config: { customFieldId: "" }
      })
    ).toMatch(/needs a custom field id/);
  });
  it("refuses naming both a built-in and a custom field", () => {
    expect(
      transitionRuleError({
        kind: "VALIDATOR",
        type: "FIELD_REQUIRED",
        config: { field: "severity", customFieldId: "f1" }
      })
    ).toMatch(/not both/);
  });
  it("still refuses naming neither", () => {
    expect(transitionRuleError({ kind: "VALIDATOR", type: "FIELD_REQUIRED", config: {} })).toMatch(
      /needs a field from/
    );
  });
  it("refuses the move when the custom field is unset", () => {
    const t = edge(PROGRESS, DONE, [customRule("f1")]);
    const verdict = evaluateTransition(t, ctx());
    expect(verdict.validatorError).toMatch(/must be set first/);
  });
  it("allows the move when it is set", () => {
    const t = edge(PROGRESS, DONE, [customRule("f1")]);
    const verdict = evaluateTransition(t, {
      ...ctx(),
      issue: { ...ctx().issue, customFieldValues: { f1: ["ship it"] } }
    });
    expect(verdict.validatorError).toBeNull();
  });
  it("treats an empty value list as unset", () => {
    const t = edge(PROGRESS, DONE, [customRule("f1")]);
    const verdict = evaluateTransition(t, {
      ...ctx(),
      issue: { ...ctx().issue, customFieldValues: { f1: [] } }
    });
    expect(verdict.validatorError).toMatch(/must be set first/);
  });
  it("names the field from the context", () => {
    const t = edge(PROGRESS, DONE, [customRule("f1")]);
    const verdict = evaluateTransition(t, {
      ...ctx(),
      customFieldNames: { f1: "Sprint goal" }
    });
    expect(verdict.validatorError).toBe("Sprint goal must be set first.");
  });
  it("falls back to a generic name when the context has none", () => {
    const t = edge(PROGRESS, DONE, [customRule("f1")]);
    expect(evaluateTransition(t, ctx()).validatorError).toBe("That field must be set first.");
  });
});
