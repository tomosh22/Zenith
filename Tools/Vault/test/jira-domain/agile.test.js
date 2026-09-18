// Ported from SaaS packages/lib/src/__tests__/agile.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  JIRA_ISSUE_TYPES,
  JIRA_STATUS_CATEGORIES,
  JIRA_COMPLEXITIES,
  JIRA_RISKS,
  DEFAULT_JIRA_ISSUE_VALUES,
  JIRA_LINK_TYPES,
  JIRA_SEVERITIES,
  JIRA_LINK_LABELS,
  JIRA_LINK_INVERSE_LABELS,
  JIRA_LINK_INVERSE_PAST_LABELS,
  JIRA_STORY_POINTS,
  issueParentError,
  issueLinkError
} from "../../shared/jira/constants.js";
describe("JIRA_SEVERITIES", () => {
  it("is severest-first", () => {
    expect(JIRA_SEVERITIES).toEqual(["S1_CRITICAL", "S2_MAJOR", "S3_MINOR", "S4_TRIVIAL"]);
  });
});
describe("shared Jira vocabularies", () => {
  it("owns the status, routing, and issue-default values", () => {
    expect(JIRA_STATUS_CATEGORIES).toEqual(["TODO", "IN_PROGRESS", "DONE"]);
    expect(JIRA_COMPLEXITIES).toEqual(["TRIVIAL", "SIMPLE", "MODERATE", "COMPLEX"]);
    expect(JIRA_RISKS).toEqual(["LOW", "MEDIUM", "HIGH"]);
    expect(DEFAULT_JIRA_ISSUE_VALUES).toEqual({
      type: "TASK",
      priority: "MEDIUM",
      complexity: "MODERATE",
      risk: "LOW"
    });
  });
});
describe("link labels", () => {
  it("names every link type in both directions", () => {
    for (const type of JIRA_LINK_TYPES) {
      expect(JIRA_LINK_LABELS[type]).toBeTruthy();
      expect(JIRA_LINK_INVERSE_LABELS[type]).toBeTruthy();
    }
  });
  it("inverts BLOCKS rather than repeating it", () => {
    expect(JIRA_LINK_LABELS.BLOCKS).toBe("blocks");
    expect(JIRA_LINK_INVERSE_LABELS.BLOCKS).toBe("is blocked by");
  });
  it("leaves the symmetric type symmetric", () => {
    expect(JIRA_LINK_LABELS.RELATES_TO).toBe(JIRA_LINK_INVERSE_LABELS.RELATES_TO);
  });
  it("gives only the expiring link a past tense", () => {
    expect(JIRA_LINK_INVERSE_PAST_LABELS.BLOCKS).toBe("was blocked by");
    expect(JIRA_LINK_INVERSE_PAST_LABELS.RELATES_TO).toBeUndefined();
    expect(JIRA_LINK_INVERSE_PAST_LABELS.DUPLICATES).toBeUndefined();
    expect(JIRA_LINK_INVERSE_PAST_LABELS.CAUSES).toBeUndefined();
  });
});
describe("JIRA_STORY_POINTS", () => {
  it("is a strictly ascending Fibonacci ladder", () => {
    const points = [...JIRA_STORY_POINTS];
    expect(points).toEqual([...points].sort((a, b) => a - b));
    for (let i = 2; i < points.length; i++) {
      expect(points[i]).toBe(points[i - 1] + points[i - 2]);
    }
  });
});
describe("issueParentError", () => {
  it("allows a STORY, TASK or BUG under an EPIC", () => {
    expect(issueParentError("STORY", "EPIC")).toBeNull();
    expect(issueParentError("TASK", "EPIC")).toBeNull();
    expect(issueParentError("BUG", "EPIC")).toBeNull();
  });
  it("allows a SUBTASK under any mid-level type", () => {
    expect(issueParentError("SUBTASK", "STORY")).toBeNull();
    expect(issueParentError("SUBTASK", "TASK")).toBeNull();
    expect(issueParentError("SUBTASK", "BUG")).toBeNull();
  });
  it("refuses an epic inside an epic", () => {
    expect(issueParentError("EPIC", "EPIC")).toMatch(/cannot be inside another epic/);
  });
  it("refuses an epic under anything", () => {
    expect(issueParentError("EPIC", "STORY")).toBeTruthy();
    expect(issueParentError("EPIC", "SUBTASK")).toBeTruthy();
  });
  it("refuses a sibling as a parent", () => {
    expect(issueParentError("TASK", "STORY")).toMatch(/hierarchy is EPIC/);
    expect(issueParentError("BUG", "TASK")).toBeTruthy();
  });
  it("refuses a subtask hanging directly off an epic", () => {
    expect(issueParentError("SUBTASK", "EPIC")).toMatch(/intermediate/);
  });
  it("refuses a subtask under a subtask", () => {
    expect(issueParentError("SUBTASK", "SUBTASK")).toBeTruthy();
  });
  it("has an opinion about every ordered pair", () => {
    for (const child of JIRA_ISSUE_TYPES) {
      for (const parent of JIRA_ISSUE_TYPES) {
        const result = issueParentError(child, parent);
        expect(result === null || typeof result === "string").toBe(true);
      }
    }
  });
});
describe("issueLinkError", () => {
  it("refuses a self-link", () => {
    expect(issueLinkError("ZM-4", "ZM-4")).toMatch(/itself/);
  });
  it("allows a link between two issues", () => {
    expect(issueLinkError("ZM-4", "ZM-5")).toBeNull();
  });
});
