// Ported from the upstream board application's packages/lib/src/__tests__/fields.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  BUILTIN_FIELDS,
  BUILTIN_FIELD_LABELS,
  CUSTOM_FIELD_KEY_MAX,
  CUSTOM_FIELD_TEXTAREA_MAX,
  CUSTOM_FIELD_TEXT_MAX,
  customFieldError,
  customFieldKeyError,
  DEFAULT_BUG_ONLY_FIELDS,
  fieldIsMultiValued,
  fieldRefError,
  fieldRefKey,
  fieldTakesOptions,
  fieldValueError,
  isBuiltinField,
  JIRA_CUSTOM_FIELD_TYPES,
  JIRA_CUSTOM_FIELD_TYPE_LABELS,
  sameFieldRef
} from "../../shared/jira/fields.js";
function field(overrides = {}) {
  return {
    id: "f_1",
    key: "sprint-goal",
    name: "Sprint goal",
    type: "TEXT",
    options: [],
    ...overrides
  };
}
describe("custom field vocabulary", () => {
  it("labels every type", () => {
    for (const type of JIRA_CUSTOM_FIELD_TYPES) {
      expect(JIRA_CUSTOM_FIELD_TYPE_LABELS[type]).toBeTruthy();
    }
    expect(Object.keys(JIRA_CUSTOM_FIELD_TYPE_LABELS)).toHaveLength(JIRA_CUSTOM_FIELD_TYPES.length);
  });
  it("marks exactly the option-backed types", () => {
    const optionBacked = JIRA_CUSTOM_FIELD_TYPES.filter(fieldTakesOptions);
    expect(optionBacked).toEqual(["SELECT", "MULTI_SELECT"]);
  });
  it("marks MULTI_SELECT as the only multi-valued type", () => {
    const multi = JIRA_CUSTOM_FIELD_TYPES.filter(fieldIsMultiValued);
    expect(multi).toEqual(["MULTI_SELECT"]);
  });
});
describe("built-in field vocabulary", () => {
  it("labels every built-in field", () => {
    for (const name of BUILTIN_FIELDS) {
      expect(BUILTIN_FIELD_LABELS[name]).toBeTruthy();
    }
    expect(Object.keys(BUILTIN_FIELD_LABELS)).toHaveLength(BUILTIN_FIELDS.length);
  });
  it("cannot name title, type or status", () => {
    expect(isBuiltinField("title")).toBe(false);
    expect(isBuiltinField("type")).toBe(false);
    expect(isBuiltinField("status")).toBe(false);
  });
  it("names the bug triage trio as the default BUG-only set", () => {
    expect(DEFAULT_BUG_ONLY_FIELDS).toEqual(["severity", "reproducibility", "environment"]);
    for (const name of DEFAULT_BUG_ONLY_FIELDS) {
      expect(isBuiltinField(name)).toBe(true);
    }
  });
});
describe("fieldRefError", () => {
  it("accepts one side set", () => {
    expect(fieldRefError({ builtinField: "severity", customFieldId: null })).toBeNull();
    expect(fieldRefError({ builtinField: null, customFieldId: "f_1" })).toBeNull();
  });
  it("refuses both sides set", () => {
    expect(fieldRefError({ builtinField: "severity", customFieldId: "f_1" })).toMatch(/not both/);
  });
  it("refuses neither side set", () => {
    expect(fieldRefError({ builtinField: null, customFieldId: null })).toMatch(/must name/);
  });
  it("refuses an unknown built-in field", () => {
    expect(fieldRefError({ builtinField: "nonsense", customFieldId: null })).toMatch(
      /not a configurable/
    );
  });
  it("refuses a real column that is not configurable", () => {
    expect(fieldRefError({ builtinField: "title", customFieldId: null })).toMatch(
      /not a configurable/
    );
  });
});
describe("fieldRefKey", () => {
  it("distinguishes the two sides", () => {
    expect(fieldRefKey({ builtinField: "severity", customFieldId: null })).toBe("builtin:severity");
    expect(fieldRefKey({ builtinField: null, customFieldId: "f_1" })).toBe("custom:f_1");
  });
  it("cannot collide across the two namespaces", () => {
    expect(fieldRefKey({ builtinField: null, customFieldId: "severity" })).not.toBe(
      fieldRefKey({ builtinField: "severity", customFieldId: null })
    );
  });
  it("compares references by identity", () => {
    expect(
      sameFieldRef(
        { builtinField: "severity", customFieldId: null },
        { builtinField: "severity", customFieldId: null }
      )
    ).toBe(true);
    expect(
      sameFieldRef(
        { builtinField: "severity", customFieldId: null },
        { builtinField: "priority", customFieldId: null }
      )
    ).toBe(false);
  });
});
describe("customFieldKeyError", () => {
  it("accepts a slug", () => {
    expect(customFieldKeyError("sprint-goal")).toBeNull();
    expect(customFieldKeyError("a")).toBeNull();
    expect(customFieldKeyError("field-2")).toBeNull();
  });
  it("refuses an empty key", () => {
    expect(customFieldKeyError("")).toMatch(/needs a key/);
  });
  it("refuses upper case, spaces and leading digits", () => {
    expect(customFieldKeyError("SprintGoal")).toMatch(/lower-case/);
    expect(customFieldKeyError("sprint goal")).toMatch(/lower-case/);
    expect(customFieldKeyError("2fast")).toMatch(/lower-case/);
    expect(customFieldKeyError("-leading")).toMatch(/lower-case/);
  });
  it("refuses an over-long key", () => {
    expect(customFieldKeyError("a".repeat(CUSTOM_FIELD_KEY_MAX))).toBeNull();
    expect(customFieldKeyError("a".repeat(CUSTOM_FIELD_KEY_MAX + 1))).toMatch(/at most/);
  });
});
describe("customFieldError", () => {
  it("accepts a plain text field", () => {
    expect(customFieldError({ key: "sprint-goal", name: "Sprint goal", type: "TEXT" })).toBeNull();
  });
  it("refuses an unknown type", () => {
    expect(customFieldError({ key: "k", name: "K", type: "RICH_TEXT" })).toMatch(
      /not a custom field type/
    );
  });
  it("refuses a blank name", () => {
    expect(customFieldError({ key: "k", name: "   ", type: "TEXT" })).toMatch(/needs a name/);
  });
  it("refuses a SELECT with no options", () => {
    expect(customFieldError({ key: "k", name: "K", type: "SELECT", options: [] })).toMatch(
      /at least one option/
    );
    expect(customFieldError({ key: "k", name: "K", type: "MULTI_SELECT" })).toMatch(
      /at least one option/
    );
  });
  it("accepts a SELECT with options", () => {
    expect(
      customFieldError({ key: "k", name: "K", type: "SELECT", options: ["a", "b"] })
    ).toBeNull();
  });
  it("refuses duplicate and empty options", () => {
    expect(customFieldError({ key: "k", name: "K", type: "SELECT", options: ["a", "a"] })).toMatch(
      /Duplicate/
    );
    expect(customFieldError({ key: "k", name: "K", type: "SELECT", options: ["a", " "] })).toMatch(
      /cannot be empty/
    );
  });
  it("refuses options on a non-option type", () => {
    expect(customFieldError({ key: "k", name: "K", type: "NUMBER", options: ["a"] })).toMatch(
      /does not take options/
    );
  });
});
describe("fieldValueError \u2014 arity", () => {
  it("accepts an empty list for every type", () => {
    for (const type of JIRA_CUSTOM_FIELD_TYPES) {
      expect(fieldValueError(field({ type, options: ["a"] }), [])).toBeNull();
    }
  });
  it("refuses two values on a single-valued field", () => {
    expect(fieldValueError(field({ type: "TEXT" }), ["a", "b"])).toMatch(/holds one value, not 2/);
  });
  it("accepts several values on MULTI_SELECT", () => {
    expect(
      fieldValueError(field({ type: "MULTI_SELECT", options: ["a", "b"] }), ["a", "b"])
    ).toBeNull();
  });
  it("refuses a repeated value", () => {
    expect(fieldValueError(field({ type: "MULTI_SELECT", options: ["a"] }), ["a", "a"])).toMatch(
      /twice/
    );
  });
});
describe("fieldValueError \u2014 per type", () => {
  it("TEXT rejects blank and over-long", () => {
    expect(fieldValueError(field({ type: "TEXT" }), ["ok"])).toBeNull();
    expect(fieldValueError(field({ type: "TEXT" }), ["  "])).toMatch(/cannot be blank/);
    expect(
      fieldValueError(field({ type: "TEXT" }), ["x".repeat(CUSTOM_FIELD_TEXT_MAX)])
    ).toBeNull();
    expect(
      fieldValueError(field({ type: "TEXT" }), ["x".repeat(CUSTOM_FIELD_TEXT_MAX + 1)])
    ).toMatch(/at most/);
  });
  it("TEXTAREA carries the larger cap", () => {
    expect(
      fieldValueError(field({ type: "TEXTAREA" }), ["x".repeat(CUSTOM_FIELD_TEXT_MAX + 1)])
    ).toBeNull();
    expect(
      fieldValueError(field({ type: "TEXTAREA" }), ["x".repeat(CUSTOM_FIELD_TEXTAREA_MAX + 1)])
    ).toMatch(/at most/);
  });
  it("NUMBER accepts decimals and negatives, rejects prose", () => {
    expect(fieldValueError(field({ type: "NUMBER" }), ["3"])).toBeNull();
    expect(fieldValueError(field({ type: "NUMBER" }), ["-2.5"])).toBeNull();
    expect(fieldValueError(field({ type: "NUMBER" }), ["0"])).toBeNull();
    expect(fieldValueError(field({ type: "NUMBER" }), ["three"])).toMatch(/must be a number/);
    expect(fieldValueError(field({ type: "NUMBER" }), [""])).toMatch(/must be a number/);
    expect(fieldValueError(field({ type: "NUMBER" }), [" "])).toMatch(/must be a number/);
    expect(fieldValueError(field({ type: "NUMBER" }), ["Infinity"])).toMatch(/must be a number/);
  });
  it("DATE demands a real YYYY-MM-DD", () => {
    expect(fieldValueError(field({ type: "DATE" }), ["2026-08-19"])).toBeNull();
    expect(fieldValueError(field({ type: "DATE" }), ["2026-02-29"])).toMatch(/must be a date/);
    expect(fieldValueError(field({ type: "DATE" }), ["2024-02-29"])).toBeNull();
    expect(fieldValueError(field({ type: "DATE" }), ["2026-13-01"])).toMatch(/must be a date/);
    expect(fieldValueError(field({ type: "DATE" }), ["19/08/2026"])).toMatch(/must be a date/);
    expect(fieldValueError(field({ type: "DATE" }), ["2026-08-19T10:00:00Z"])).toMatch(
      /must be a date/
    );
  });
  it("CHECKBOX takes only true or false", () => {
    expect(fieldValueError(field({ type: "CHECKBOX" }), ["true"])).toBeNull();
    expect(fieldValueError(field({ type: "CHECKBOX" }), ["false"])).toBeNull();
    expect(fieldValueError(field({ type: "CHECKBOX" }), ["1"])).toMatch(/true or false/);
    expect(fieldValueError(field({ type: "CHECKBOX" }), ["TRUE"])).toMatch(/true or false/);
  });
  it("URL demands http or https", () => {
    expect(fieldValueError(field({ type: "URL" }), ["https://example.com/x"])).toBeNull();
    expect(fieldValueError(field({ type: "URL" }), ["http://example.com"])).toBeNull();
    expect(fieldValueError(field({ type: "URL" }), ["example.com"])).toMatch(/must be a URL/);
    expect(fieldValueError(field({ type: "URL" }), ["javascript:alert(1)"])).toMatch(
      /http or https/
    );
    expect(fieldValueError(field({ type: "URL" }), ["file:///etc/passwd"])).toMatch(
      /http or https/
    );
  });
  it("USER wants a non-empty id and leaves existence to the writer", () => {
    expect(fieldValueError(field({ type: "USER" }), ["u_1"])).toBeNull();
    expect(fieldValueError(field({ type: "USER" }), ["  "])).toMatch(/must name a user/);
  });
  it("SELECT confines the value to the option list", () => {
    const def = field({ type: "SELECT", options: ["Low", "High"] });
    expect(fieldValueError(def, ["Low"])).toBeNull();
    expect(fieldValueError(def, ["Medium"])).toMatch(/is not an option/);
  });
  it("MULTI_SELECT confines every value to the option list", () => {
    const def = field({ type: "MULTI_SELECT", options: ["a", "b"] });
    expect(fieldValueError(def, ["a", "b"])).toBeNull();
    expect(fieldValueError(def, ["a", "c"])).toMatch(/is not an option/);
  });
  it("validates something for every declared type", () => {
    const nonsense = {
      TEXT: "   ",
      TEXTAREA: "   ",
      NUMBER: "three",
      DATE: "yesterday",
      SELECT: "missing",
      MULTI_SELECT: "missing",
      CHECKBOX: "yes",
      USER: "   ",
      URL: "not a url"
    };
    for (const type of JIRA_CUSTOM_FIELD_TYPES) {
      expect(fieldValueError(field({ type, options: ["a"] }), [nonsense[type]])).toBeTruthy();
    }
  });
});
import {
  defaultRequirement,
  fieldConfigError,
  FIELD_REQUIREMENTS,
  FIELD_REQUIREMENT_LABELS,
  resolveFieldLayout
} from "../../shared/jira/fields.js";
function builtin(name) {
  return { builtinField: name, customFieldId: null };
}
function customRef(id) {
  return { builtinField: null, customFieldId: id };
}
function rule(issueType, ref, requirement) {
  return { ...ref, issueType, requirement };
}
function layoutKeys(...args) {
  return resolveFieldLayout(...args).map((f) => f.key);
}
describe("field requirement vocabulary", () => {
  it("labels every requirement", () => {
    for (const r of FIELD_REQUIREMENTS) expect(FIELD_REQUIREMENT_LABELS[r]).toBeTruthy();
    expect(Object.keys(FIELD_REQUIREMENT_LABELS)).toHaveLength(FIELD_REQUIREMENTS.length);
  });
});
describe("no configuration reproduces the panel as it was", () => {
  it("hides the bug triage trio on a STORY", () => {
    const keys = layoutKeys({ issueType: "STORY", customFields: [], config: [] });
    expect(keys).not.toContain("builtin:severity");
    expect(keys).not.toContain("builtin:reproducibility");
    expect(keys).not.toContain("builtin:environment");
    expect(keys).toContain("builtin:priority");
    expect(keys).toContain("builtin:assignee");
  });
  it("shows the bug triage trio on a BUG", () => {
    const keys = layoutKeys({ issueType: "BUG", customFields: [], config: [] });
    expect(keys).toContain("builtin:severity");
    expect(keys).toContain("builtin:reproducibility");
    expect(keys).toContain("builtin:environment");
  });
  it("marks everything OPTIONAL, so nothing newly blocks a save", () => {
    const layout = resolveFieldLayout({ issueType: "BUG", customFields: [], config: [] });
    expect(layout.every((f) => f.requirement === "OPTIONAL")).toBe(true);
  });
  it("agrees with defaultRequirement", () => {
    expect(defaultRequirement("STORY", builtin("severity"))).toBe("HIDDEN");
    expect(defaultRequirement("BUG", builtin("severity"))).toBe("OPTIONAL");
    expect(defaultRequirement("STORY", builtin("priority"))).toBe("OPTIONAL");
    expect(defaultRequirement("STORY", customRef("f1"))).toBe("OPTIONAL");
  });
});
describe("resolveFieldLayout \u2014 configuration", () => {
  it("drops a HIDDEN field", () => {
    const keys = layoutKeys({
      issueType: "STORY",
      customFields: [],
      config: [rule("STORY", builtin("priority"), "HIDDEN")]
    });
    expect(keys).not.toContain("builtin:priority");
  });
  it("shows a bug-only field on a STORY when configured to", () => {
    const keys = layoutKeys({
      issueType: "STORY",
      customFields: [],
      config: [rule("STORY", builtin("severity"), "OPTIONAL")]
    });
    expect(keys).toContain("builtin:severity");
  });
  it("carries REQUIRED through", () => {
    const layout = resolveFieldLayout({
      issueType: "BUG",
      customFields: [],
      config: [rule("BUG", builtin("severity"), "REQUIRED")]
    });
    expect(layout.find((f) => f.key === "builtin:severity")?.requirement).toBe("REQUIRED");
  });
  it("ignores a rule for a different issue type", () => {
    const keys = layoutKeys({
      issueType: "STORY",
      customFields: [],
      config: [rule("BUG", builtin("priority"), "HIDDEN")]
    });
    expect(keys).toContain("builtin:priority");
  });
  it("includes custom fields, in their own position order", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields: [
        { id: "f1", key: "a", name: "A", type: "TEXT", options: [] },
        { id: "f2", key: "b", name: "B", type: "NUMBER", options: [] }
      ],
      config: []
    });
    expect(keys.slice(-2)).toEqual(["custom:f1", "custom:f2"]);
  });
  it("hides a custom field when configured to", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields: [{ id: "f1", key: "a", name: "A", type: "TEXT", options: [] }],
      config: [rule("TASK", customRef("f1"), "HIDDEN")]
    });
    expect(keys).not.toContain("custom:f1");
  });
});
describe("resolveFieldLayout \u2014 screens", () => {
  const customFields = [{ id: "f1", key: "a", name: "A", type: "TEXT", options: [] }];
  it("filters and orders by the screen", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [],
      screen: [customRef("f1"), builtin("assignee")]
    });
    expect(keys).toEqual(["custom:f1", "builtin:assignee"]);
  });
  it("does not show a HIDDEN field the screen names", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [rule("TASK", builtin("assignee"), "HIDDEN")],
      screen: [builtin("assignee"), builtin("priority")]
    });
    expect(keys).toEqual(["builtin:priority"]);
  });
  it("appends a REQUIRED field the screen forgot", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [rule("TASK", builtin("dueDate"), "REQUIRED")],
      screen: [builtin("assignee")]
    });
    expect(keys).toEqual(["builtin:assignee", "builtin:dueDate"]);
  });
  it("does not append an OPTIONAL field the screen omitted", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [],
      screen: [builtin("assignee")]
    });
    expect(keys).toEqual(["builtin:assignee"]);
  });
  it("skips a screen entry naming a field that no longer exists", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [],
      screen: [customRef("deleted"), builtin("assignee")]
    });
    expect(keys).toEqual(["builtin:assignee"]);
  });
  it("renders a repeated screen entry once", () => {
    const keys = layoutKeys({
      issueType: "TASK",
      customFields,
      config: [],
      screen: [builtin("assignee"), builtin("assignee")]
    });
    expect(keys).toEqual(["builtin:assignee"]);
  });
  it("treats a null screen as no screen", () => {
    const withNull = layoutKeys({ issueType: "TASK", customFields, config: [], screen: null });
    const without = layoutKeys({ issueType: "TASK", customFields, config: [] });
    expect(withNull).toEqual(without);
    expect(withNull.length).toBeGreaterThan(1);
  });
  it("treats an empty screen as a bare form", () => {
    const keys = layoutKeys({ issueType: "TASK", customFields, config: [], screen: [] });
    expect(keys).toEqual([]);
  });
});
describe("fieldConfigError", () => {
  it("accepts a well-formed rule", () => {
    expect(fieldConfigError(rule("BUG", builtin("severity"), "REQUIRED"))).toBeNull();
    expect(fieldConfigError(rule("BUG", customRef("f1"), "HIDDEN"))).toBeNull();
  });
  it("refuses a malformed reference", () => {
    expect(
      fieldConfigError(rule("BUG", { builtinField: "nope", customFieldId: null }, "HIDDEN"))
    ).toMatch(/not a configurable/);
    expect(
      fieldConfigError(rule("BUG", { builtinField: null, customFieldId: null }, "HIDDEN"))
    ).toMatch(/must name/);
  });
  it("refuses an unknown requirement", () => {
    expect(
      fieldConfigError({ ...builtin("severity"), issueType: "BUG", requirement: "MAYBE" })
    ).toMatch(/not a field requirement/);
  });
});
