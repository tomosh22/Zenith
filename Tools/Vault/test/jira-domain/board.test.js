// Ported from SaaS packages/lib/src/__tests__/board.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  blockedByKeys,
  blockedByTooltip,
  boardFilterError,
  boardShowsOnlyActiveSprint,
  DEFAULT_CARD_FIELDS,
  isOverWipLimit,
  issueInSwimlane,
  JIRA_BOARD_TYPES,
  JIRA_CARD_FIELDS,
  JIRA_CARD_FIELD_LABELS,
  JIRA_SWIMLANE_LABELS,
  JIRA_SWIMLANE_MODES,
  matchesAllFilters,
  matchesFilter,
  normalizeCardFields,
  resolveDropStatus,
  sprintDisplayName,
  swimlanesFor,
  unmappedStatusIds
} from "../../shared/jira/board.js";
function issue(overrides = {}) {
  return {
    title: "Fix the thing",
    type: "TASK",
    priority: "MEDIUM",
    severity: null,
    assigneeId: null,
    categoryId: null,
    labelIds: [],
    flagged: false,
    ...overrides
  };
}
function column(overrides = {}) {
  return { id: "c1", name: "To Do", position: 0, wipLimit: null, statusIds: ["s1"], ...overrides };
}
describe("board vocabulary", () => {
  it("labels every swimlane mode and card field", () => {
    for (const mode of JIRA_SWIMLANE_MODES) expect(JIRA_SWIMLANE_LABELS[mode]).toBeTruthy();
    for (const field of JIRA_CARD_FIELDS) expect(JIRA_CARD_FIELD_LABELS[field]).toBeTruthy();
  });
  it("gives SCRUM and KANBAN different behaviour", () => {
    expect(JIRA_BOARD_TYPES).toEqual(["KANBAN", "SCRUM"]);
    expect(boardShowsOnlyActiveSprint("SCRUM")).toBe(true);
    expect(boardShowsOnlyActiveSprint("KANBAN")).toBe(false);
  });
  it("defaults a card to every field, blocked included", () => {
    expect(DEFAULT_CARD_FIELDS).toEqual([...JIRA_CARD_FIELDS]);
    expect(DEFAULT_CARD_FIELDS).toContain("blocked");
  });
});
describe("normalizeCardFields", () => {
  it("falls back to the default when the setting is missing or malformed", () => {
    expect(normalizeCardFields(void 0)).toEqual(DEFAULT_CARD_FIELDS);
    expect(normalizeCardFields(null)).toEqual(DEFAULT_CARD_FIELDS);
    expect(normalizeCardFields("key,type")).toEqual(DEFAULT_CARD_FIELDS);
  });
  it("keeps an empty array as an empty array", () => {
    expect(normalizeCardFields([])).toEqual([]);
  });
  it("drops a key nothing knows how to draw", () => {
    expect(normalizeCardFields(["key", "vibes", "assignee"])).toEqual(["key", "assignee"]);
  });
});
describe("matchesFilter", () => {
  it("matches everything when the filter is empty", () => {
    expect(matchesFilter(issue(), {})).toBe(true);
  });
  it("filters on each scalar field", () => {
    expect(matchesFilter(issue({ type: "BUG" }), { type: "BUG" })).toBe(true);
    expect(matchesFilter(issue({ type: "TASK" }), { type: "BUG" })).toBe(false);
    expect(matchesFilter(issue({ priority: "HIGH" }), { priority: "HIGH" })).toBe(true);
    expect(matchesFilter(issue({ priority: "LOW" }), { priority: "HIGH" })).toBe(false);
    expect(matchesFilter(issue({ severity: "S1_CRITICAL" }), { severity: "S1_CRITICAL" })).toBe(
      true
    );
    expect(matchesFilter(issue({ categoryId: "c1" }), { categoryId: "c1" })).toBe(true);
    expect(matchesFilter(issue({ flagged: true }), { flagged: true })).toBe(true);
    expect(matchesFilter(issue({ flagged: false }), { flagged: true })).toBe(false);
  });
  it('tells "unassigned" apart from "no constraint"', () => {
    expect(matchesFilter(issue({ assigneeId: null }), { assigneeId: null })).toBe(true);
    expect(matchesFilter(issue({ assigneeId: "u1" }), { assigneeId: null })).toBe(false);
    expect(matchesFilter(issue({ assigneeId: "u1" }), {})).toBe(true);
    expect(matchesFilter(issue({ assigneeId: null }), {})).toBe(true);
  });
  it("treats an explicit null scalar as no constraint", () => {
    expect(matchesFilter(issue({ type: "BUG" }), { type: null })).toBe(true);
    expect(matchesFilter(issue({ severity: null }), { severity: null })).toBe(true);
  });
  it("filters on labels through the junction list", () => {
    expect(matchesFilter(issue({ labelIds: ["l1", "l2"] }), { labelId: "l2" })).toBe(true);
    expect(matchesFilter(issue({ labelIds: ["l1"] }), { labelId: "l2" })).toBe(false);
    expect(matchesFilter(issue({}), { labelId: "l2" })).toBe(false);
  });
  it("matches title text case-insensitively, ignoring blank input", () => {
    expect(matchesFilter(issue({ title: "Fix the Thing" }), { text: "thing" })).toBe(true);
    expect(matchesFilter(issue({ title: "Fix the Thing" }), { text: "other" })).toBe(false);
    expect(matchesFilter(issue({ title: "Fix" }), { text: "   " })).toBe(true);
  });
});
describe("matchesAllFilters", () => {
  it("composes by AND, so a quick filter can only narrow", () => {
    const board = { type: "BUG" };
    const quick = { priority: "CRITICAL" };
    expect(matchesAllFilters(issue({ type: "BUG", priority: "CRITICAL" }), [board, quick])).toBe(
      true
    );
    expect(matchesAllFilters(issue({ type: "BUG", priority: "LOW" }), [board, quick])).toBe(false);
    expect(matchesAllFilters(issue({ type: "STORY", priority: "CRITICAL" }), [board, quick])).toBe(
      false
    );
  });
  it("ignores absent filters", () => {
    expect(matchesAllFilters(issue(), [null, void 0, {}])).toBe(true);
  });
});
describe("boardFilterError", () => {
  it("accepts an absent or well-formed filter", () => {
    expect(boardFilterError(void 0)).toBeNull();
    expect(boardFilterError(null)).toBeNull();
    expect(boardFilterError({})).toBeNull();
    expect(
      boardFilterError({ type: "BUG", assigneeId: null, flagged: true, text: "x" })
    ).toBeNull();
  });
  it("refuses an unknown field", () => {
    expect(boardFilterError({ asignee: "u1" })).toMatch(/not a filter field/);
  });
  it("refuses a wrongly-typed value", () => {
    expect(boardFilterError({ flagged: "yes" })).toMatch(/must be a boolean/);
    expect(boardFilterError({ text: 3 })).toMatch(/must be a string/);
    expect(boardFilterError({ type: 7 })).toMatch(/must be a string or null/);
  });
  it("refuses a non-object", () => {
    expect(boardFilterError([])).toMatch(/must be an object/);
    expect(boardFilterError("type=BUG")).toMatch(/must be an object/);
  });
});
describe("isOverWipLimit", () => {
  it("fires above the limit, not at it", () => {
    expect(isOverWipLimit(3, 3)).toBe(false);
    expect(isOverWipLimit(4, 3)).toBe(true);
    expect(isOverWipLimit(0, 0)).toBe(false);
    expect(isOverWipLimit(1, 0)).toBe(true);
  });
  it("never fires without a limit", () => {
    expect(isOverWipLimit(99, null)).toBe(false);
    expect(isOverWipLimit(99, void 0)).toBe(false);
  });
});
describe("resolveDropStatus", () => {
  const twoStatus = column({ statusIds: ["s1", "s2"] });
  it("changes nothing when the issue is already in the column", () => {
    expect(resolveDropStatus(twoStatus, "s2", ["s1"])).toBe("s2");
    expect(resolveDropStatus(twoStatus, "s1", [])).toBe("s1");
  });
  it("picks the first mapped status the workflow will accept", () => {
    expect(resolveDropStatus(twoStatus, "s9", ["s1", "s2"])).toBe("s1");
    expect(resolveDropStatus(twoStatus, "s9", ["s2"])).toBe("s2");
  });
  it("returns null when no mapped status is reachable", () => {
    expect(resolveDropStatus(twoStatus, "s9", ["s7"])).toBeNull();
    expect(resolveDropStatus(twoStatus, "s9", [])).toBeNull();
  });
  it("handles a column that maps nothing", () => {
    expect(resolveDropStatus(column({ statusIds: [] }), "s1", ["s1"])).toBeNull();
  });
});
describe("unmappedStatusIds", () => {
  it("names the statuses no column draws", () => {
    const columns = [column({ statusIds: ["s1"] }), column({ id: "c2", statusIds: ["s2", "s3"] })];
    expect(unmappedStatusIds(columns, ["s1", "s2", "s3", "s4"])).toEqual(["s4"]);
    expect(unmappedStatusIds(columns, ["s1", "s2", "s3"])).toEqual([]);
    expect(unmappedStatusIds([], ["s1"])).toEqual(["s1"]);
  });
});
describe("swimlanesFor", () => {
  const issues = [
    { assigneeId: "u2", epicId: "e1" },
    { assigneeId: "u1", epicId: null },
    { assigneeId: null, epicId: "e1" }
  ];
  const names = { u1: "Alice", u2: "Bob", e1: "ZM-9 Vertical slice" };
  it("draws nothing in NONE mode", () => {
    expect(swimlanesFor("NONE", issues, names)).toEqual([]);
  });
  it("groups by assignee, named and sorted", () => {
    expect(swimlanesFor("ASSIGNEE", issues, names)).toEqual([
      { id: "u1", name: "Alice" },
      { id: "u2", name: "Bob" },
      { id: "", name: "Unassigned" }
    ]);
  });
  it("groups by epic", () => {
    expect(swimlanesFor("EPIC", issues, names)).toEqual([
      { id: "e1", name: "ZM-9 Vertical slice" },
      { id: "", name: "No epic" }
    ]);
  });
  it("puts the null lane last, and omits it when empty", () => {
    const owned = [{ assigneeId: "u1", epicId: "e1" }];
    expect(swimlanesFor("ASSIGNEE", owned, names)).toEqual([{ id: "u1", name: "Alice" }]);
  });
  it("falls back to the id when a name is missing", () => {
    expect(swimlanesFor("ASSIGNEE", [{ assigneeId: "u9", epicId: null }], names)).toEqual([
      { id: "u9", name: "u9" }
    ]);
  });
});
describe("issueInSwimlane", () => {
  it("places every issue in every lane when swimlanes are off", () => {
    expect(issueInSwimlane("NONE", { assigneeId: "u1", epicId: null }, "anything")).toBe(true);
  });
  it("routes null values to the empty-id lane", () => {
    expect(issueInSwimlane("ASSIGNEE", { assigneeId: null, epicId: null }, "")).toBe(true);
    expect(issueInSwimlane("ASSIGNEE", { assigneeId: "u1", epicId: null }, "")).toBe(false);
    expect(issueInSwimlane("EPIC", { assigneeId: null, epicId: "e1" }, "e1")).toBe(true);
    expect(issueInSwimlane("EPIC", { assigneeId: null, epicId: "e1" }, "e2")).toBe(false);
  });
});
describe("blocked-by rendering", () => {
  it("names every blocker, in order", () => {
    expect(
      blockedByKeys([
        { key: "ZM-20", reason: null },
        { key: "ZM-25", reason: null }
      ])
    ).toBe("ZM-20, ZM-25");
  });
  it("points the tooltip at where the link can be removed", () => {
    expect(blockedByTooltip([{ key: "ZM-20", reason: null }])).toBe(
      "Blocked by ZM-20 \u2014 open this issue to see or remove the link"
    );
  });
  it("names each reason against its own blocker", () => {
    const tooltip = blockedByTooltip([
      { key: "ZM-20", reason: "seam triggers have to land first" },
      { key: "ZM-25", reason: "shares the save format" }
    ]);
    expect(tooltip).toContain("ZM-20: seam triggers have to land first");
    expect(tooltip).toContain("ZM-25: shares the save format");
  });
  it("stays a single line when no link recorded a reason", () => {
    expect(blockedByTooltip([{ key: "ZM-20", reason: null }])).not.toContain("\n");
  });
  it("mentions only the blockers that HAVE a reason", () => {
    const tooltip = blockedByTooltip([
      { key: "ZM-20", reason: "seam triggers first" },
      { key: "ZM-25", reason: null }
    ]);
    expect(tooltip).toContain("Blocked by ZM-20, ZM-25");
    expect(tooltip).toContain("ZM-20: seam triggers first");
    expect(tooltip).not.toContain("ZM-25:");
  });
  it("treats a whitespace-only reason as no reason", () => {
    expect(blockedByTooltip([{ key: "ZM-20", reason: "   " }])).not.toContain("\n");
  });
  it("trims a reason rather than reproducing stray padding", () => {
    expect(blockedByTooltip([{ key: "ZM-20", reason: "  padded  " }])).toContain("ZM-20: padded");
  });
});
describe("sprintDisplayName", () => {
  it("marks the ACTIVE sprint as the current one", () => {
    expect(sprintDisplayName("Sprint 4", "ACTIVE")).toBe("Sprint 4 (current)");
  });
  it("leaves every other sprint name exactly as stored", () => {
    expect(sprintDisplayName("Sprint 5", "PLANNED")).toBe("Sprint 5");
    expect(sprintDisplayName("Sprint 3", "COMPLETED")).toBe("Sprint 3");
  });
  it("says nothing when the status is unknown", () => {
    expect(sprintDisplayName("Sprint 6", null)).toBe("Sprint 6");
    expect(sprintDisplayName("Sprint 6", void 0)).toBe("Sprint 6");
  });
});
