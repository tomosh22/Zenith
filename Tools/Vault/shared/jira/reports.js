// Ported from SaaS packages/lib/src/../../../apps/jira/src/lib/reports.ts; see provenance.json.
function dayKey(date) {
  return date.toISOString().slice(0, 10);
}
function eachDay(start, end) {
  const days = [];
  const cursor = new Date(
    Date.UTC(start.getUTCFullYear(), start.getUTCMonth(), start.getUTCDate())
  );
  const last = Date.UTC(end.getUTCFullYear(), end.getUTCMonth(), end.getUTCDate());
  while (cursor.getTime() <= last) {
    days.push(dayKey(cursor));
    cursor.setUTCDate(cursor.getUTCDate() + 1);
  }
  return days;
}
function buildBurndown(input) {
  const { start, end, scope, activity, doneStatusNames } = input;
  const days = eachDay(start, end);
  if (days.length === 0) return [];
  const pointsOf = new Map(scope.map((r) => [r.issueId, r.storyPoints ?? 0]));
  const doneNames = new Set(doneStatusNames);
  const addedOn = /* @__PURE__ */ new Map();
  const removedOn = /* @__PURE__ */ new Map();
  const doneOn = /* @__PURE__ */ new Map();
  const undoneOn = /* @__PURE__ */ new Map();
  for (const row of activity) {
    const day = dayKey(row.createdAt);
    if (row.field === "sprint") {
      if (row.toValue) addedOn.set(row.issueId, day);
      else removedOn.set(row.issueId, day);
    }
    if (row.field === "status") {
      const wasDone = row.fromValue !== null && doneNames.has(row.fromValue);
      const isDone = row.toValue !== null && doneNames.has(row.toValue);
      if (!wasDone && isDone) doneOn.set(row.issueId, day);
      if (wasDone && !isDone) undoneOn.set(row.issueId, day);
    }
  }
  const startingScope = scope.reduce((sum, r) => {
    const added = addedOn.get(r.issueId);
    if (added && added > days[0]) return sum;
    return sum + (r.storyPoints ?? 0);
  }, 0);
  const lastIndex = days.length - 1;
  return days.map((day, index) => {
    let dayScope = 0;
    let remaining = 0;
    for (const row of scope) {
      const points = pointsOf.get(row.issueId) ?? 0;
      const added = addedOn.get(row.issueId);
      const removed = removedOn.get(row.issueId);
      if (added && added > day) continue;
      if (removed && removed <= day) continue;
      dayScope += points;
      const done = doneOn.get(row.issueId);
      const undone = undoneOn.get(row.issueId);
      const isDone = done !== void 0 && done <= day && (undone === void 0 || undone <= done || undone > day);
      const neverMoved = done === void 0 && undone === void 0 && row.done;
      if (!isDone && !neverMoved) remaining += points;
    }
    return {
      date: day,
      scope: dayScope,
      remaining,
      ideal: lastIndex === 0 ? 0 : Math.round(startingScope * (1 - index / lastIndex) * 100) / 100
    };
  });
}
function averageVelocity(rows) {
  if (rows.length === 0) return 0;
  const total = rows.reduce((sum, r) => sum + r.completed, 0);
  return Math.round(total / rows.length * 10) / 10;
}
function epicPercent(progress) {
  if (progress.points > 0) {
    return Math.round(progress.donePoints / progress.points * 100);
  }
  if (progress.total > 0) {
    return Math.round(progress.done / progress.total * 100);
  }
  return 0;
}
export {
  averageVelocity,
  buildBurndown,
  dayKey,
  eachDay,
  epicPercent
};
