// Ported from the upstream board application's packages/lib/src/../../../apps/jira/src/lib/reports.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  buildBurndown,
  averageVelocity,
  epicPercent,
  dayKey,
  eachDay
} from "../../shared/jira/reports.js";
const day = (d) => /* @__PURE__ */ new Date(`${d}T12:00:00.000Z`);
function activity(rows) {
  return rows.map(([issueId, field, fromValue, toValue, date]) => ({
    issueId,
    field,
    fromValue,
    toValue,
    createdAt: day(date)
  }));
}
const DONE = ["Done"];
describe("dayKey / eachDay", () => {
  it("keys on UTC, so a chart does not shift with the reader", () => {
    expect(dayKey(/* @__PURE__ */ new Date("2026-08-15T23:30:00.000Z"))).toBe("2026-08-15");
  });
  it("is inclusive of both ends", () => {
    expect(eachDay(day("2026-08-01"), day("2026-08-03"))).toEqual([
      "2026-08-01",
      "2026-08-02",
      "2026-08-03"
    ]);
  });
  it("returns a single day for a same-day sprint", () => {
    expect(eachDay(day("2026-08-01"), day("2026-08-01"))).toEqual(["2026-08-01"]);
  });
});
describe("buildBurndown", () => {
  const scope = [
    { issueId: "a", storyPoints: 3, done: true },
    { issueId: "b", storyPoints: 5, done: false }
  ];
  it("starts at the full scope and falls as work is closed", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-03"),
      scope,
      activity: activity([["a", "status", "In Progress", "Done", "2026-08-02"]]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.remaining)).toEqual([8, 5, 5]);
    expect(points.map((p) => p.scope)).toEqual([8, 8, 8]);
  });
  it("RAISES the scope line when work is added mid-sprint", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-03"),
      scope: [...scope, { issueId: "c", storyPoints: 8, done: false }],
      activity: activity([["c", "sprint", null, "S8", "2026-08-02"]]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.scope)).toEqual([8, 16, 16]);
    expect(points[0].ideal).toBe(8);
    expect(points[points.length - 1].ideal).toBe(0);
  });
  it("LOWERS the scope line when work leaves the sprint", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-03"),
      scope,
      activity: activity([["b", "sprint", "S8", null, "2026-08-02"]]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.scope)).toEqual([8, 3, 3]);
  });
  it("counts an issue as done from the day it CLOSED, not from today", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-04"),
      scope: [{ issueId: "a", storyPoints: 5, done: true }],
      activity: activity([["a", "status", "To Do", "Done", "2026-08-03"]]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.remaining)).toEqual([5, 5, 0, 0]);
  });
  it("puts a re-opened issue back on the line", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-04"),
      scope: [{ issueId: "a", storyPoints: 5, done: false }],
      activity: activity([
        ["a", "status", "To Do", "Done", "2026-08-02"],
        ["a", "status", "Done", "In Progress", "2026-08-03"]
      ]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.remaining)).toEqual([5, 0, 5, 5]);
  });
  it("treats anything with no activity row as unchanged since the start", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-02"),
      scope: [{ issueId: "a", storyPoints: 5, done: true }],
      activity: [],
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.remaining)).toEqual([0, 0]);
  });
  it("counts an unestimated issue as zero points but keeps it in scope", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-02"),
      scope: [{ issueId: "a", storyPoints: null, done: false }],
      activity: [],
      doneStatusNames: DONE
    });
    expect(points.every((p) => p.scope === 0 && p.remaining === 0)).toBe(true);
  });
  it("ignores a status move that is not into or out of a DONE lane", () => {
    const points = buildBurndown({
      start: day("2026-08-01"),
      end: day("2026-08-02"),
      scope: [{ issueId: "a", storyPoints: 5, done: false }],
      activity: activity([["a", "status", "In Progress", "In Review", "2026-08-01"]]),
      doneStatusNames: DONE
    });
    expect(points.map((p) => p.remaining)).toEqual([5, 5]);
  });
  it("returns nothing rather than inventing a window", () => {
    expect(
      buildBurndown({
        start: day("2026-08-03"),
        end: day("2026-08-01"),
        scope,
        activity: [],
        doneStatusNames: DONE
      })
    ).toEqual([]);
  });
});
describe("averageVelocity", () => {
  it("averages COMPLETED points, not committed ones", () => {
    expect(
      averageVelocity([
        { sprintId: "1", sprintName: "A", committed: 20, completed: 12 },
        { sprintId: "2", sprintName: "B", committed: 20, completed: 18 }
      ])
    ).toBe(15);
  });
  it("is zero with nothing to average, rather than NaN", () => {
    expect(averageVelocity([])).toBe(0);
  });
  it("rounds to one decimal so the number is readable", () => {
    expect(
      averageVelocity([
        { sprintId: "1", sprintName: "A", committed: 10, completed: 10 },
        { sprintId: "2", sprintName: "B", committed: 10, completed: 9 },
        { sprintId: "3", sprintName: "C", committed: 10, completed: 9 }
      ])
    ).toBe(9.3);
  });
});
describe("epicPercent", () => {
  it("uses points when the work is estimated", () => {
    expect(epicPercent({ epicId: "e", total: 4, done: 1, points: 8, donePoints: 2 })).toBe(25);
  });
  it("falls back to issue COUNT when nothing is estimated", () => {
    expect(epicPercent({ epicId: "e", total: 4, done: 3, points: 0, donePoints: 0 })).toBe(75);
  });
  it("is zero for an empty epic rather than dividing by zero", () => {
    expect(epicPercent({ epicId: "e", total: 0, done: 0, points: 0, donePoints: 0 })).toBe(0);
  });
});
