// Ported from the upstream board application's packages/lib/src/fields.ts; see provenance.json.
const JIRA_CUSTOM_FIELD_TYPES = [
  "TEXT",
  "TEXTAREA",
  "NUMBER",
  "DATE",
  "SELECT",
  "MULTI_SELECT",
  "CHECKBOX",
  "USER",
  "URL"
];
const JIRA_CUSTOM_FIELD_TYPE_LABELS = {
  TEXT: "Text",
  TEXTAREA: "Paragraph",
  NUMBER: "Number",
  DATE: "Date",
  SELECT: "Select",
  MULTI_SELECT: "Multi-select",
  CHECKBOX: "Checkbox",
  USER: "User",
  URL: "URL"
};
function fieldTakesOptions(type) {
  return type === "SELECT" || type === "MULTI_SELECT";
}
function fieldIsMultiValued(type) {
  return type === "MULTI_SELECT";
}
const CUSTOM_FIELD_TEXT_MAX = 255;
const CUSTOM_FIELD_TEXTAREA_MAX = 32768;
const BUILTIN_FIELDS = [
  "description",
  "assignee",
  "reporter",
  "priority",
  "complexity",
  "risk",
  "storyPoints",
  "dueDate",
  "category",
  "sprint",
  "flagged",
  "severity",
  "reproducibility",
  "environment",
  "labels",
  "versions",
  "parent"
];
const BUILTIN_FIELD_LABELS = {
  description: "Description",
  assignee: "Assignee",
  reporter: "Reporter",
  priority: "Priority",
  complexity: "Complexity",
  risk: "Risk",
  storyPoints: "Story points",
  dueDate: "Due date",
  category: "Category",
  sprint: "Sprint",
  flagged: "Flagged",
  severity: "Severity",
  reproducibility: "Reproducibility",
  environment: "Environment",
  labels: "Labels",
  versions: "Versions",
  parent: "Parent"
};
function isBuiltinField(value) {
  return BUILTIN_FIELDS.includes(value);
}
const DEFAULT_BUG_ONLY_FIELDS = [
  "severity",
  "reproducibility",
  "environment"
];
function fieldRefError(ref) {
  const hasBuiltin = ref.builtinField !== null && ref.builtinField !== void 0;
  const hasCustom = ref.customFieldId !== null && ref.customFieldId !== void 0;
  if (hasBuiltin && hasCustom) {
    return "A field reference names a built-in field or a custom field, not both.";
  }
  if (!hasBuiltin && !hasCustom) {
    return "A field reference must name a built-in field or a custom field.";
  }
  if (hasBuiltin && !isBuiltinField(ref.builtinField)) {
    return `"${ref.builtinField}" is not a configurable built-in field.`;
  }
  return null;
}
function fieldRefKey(ref) {
  return ref.customFieldId ? `custom:${ref.customFieldId}` : `builtin:${ref.builtinField}`;
}
function sameFieldRef(a, b) {
  return fieldRefKey(a) === fieldRefKey(b);
}
const KEY_PATTERN = /^[a-z][a-z0-9-]*$/;
const CUSTOM_FIELD_KEY_MAX = 60;
function customFieldKeyError(key) {
  if (!key) return "A custom field needs a key.";
  if (key.length > CUSTOM_FIELD_KEY_MAX) {
    return `A field key is at most ${CUSTOM_FIELD_KEY_MAX} characters.`;
  }
  if (!KEY_PATTERN.test(key)) {
    return "A field key is lower-case letters, digits and hyphens, starting with a letter.";
  }
  return null;
}
function customFieldError(def) {
  const keyError = customFieldKeyError(def.key);
  if (keyError) return keyError;
  if (!def.name.trim()) return "A custom field needs a name.";
  if (!JIRA_CUSTOM_FIELD_TYPES.includes(def.type)) {
    return `"${def.type}" is not a custom field type.`;
  }
  const type = def.type;
  const options = def.options ?? [];
  if (fieldTakesOptions(type)) {
    if (options.length === 0) {
      return `A ${JIRA_CUSTOM_FIELD_TYPE_LABELS[type]} field needs at least one option.`;
    }
    const seen = /* @__PURE__ */ new Set();
    for (const option of options) {
      if (!option.trim()) return "An option cannot be empty.";
      if (seen.has(option)) return `Duplicate option "${option}".`;
      seen.add(option);
    }
  } else if (options.length > 0) {
    return `A ${JIRA_CUSTOM_FIELD_TYPE_LABELS[type]} field does not take options.`;
  }
  return null;
}
const DATE_PATTERN = /^(\d{4})-(\d{2})-(\d{2})$/;
function isRealDate(value) {
  const match = DATE_PATTERN.exec(value);
  if (!match) return false;
  const y = Number(match[1]);
  const m = Number(match[2]);
  const d = Number(match[3]);
  const date = new Date(Date.UTC(y, m - 1, d));
  return date.getUTCFullYear() === y && date.getUTCMonth() === m - 1 && date.getUTCDate() === d;
}
function singleValueError(field, value) {
  const label = field.name;
  switch (field.type) {
    case "TEXT":
      if (!value.trim()) return `${label} cannot be blank.`;
      if (value.length > CUSTOM_FIELD_TEXT_MAX) {
        return `${label} is at most ${CUSTOM_FIELD_TEXT_MAX} characters.`;
      }
      return null;
    case "TEXTAREA":
      if (!value.trim()) return `${label} cannot be blank.`;
      if (value.length > CUSTOM_FIELD_TEXTAREA_MAX) {
        return `${label} is at most ${CUSTOM_FIELD_TEXTAREA_MAX} characters.`;
      }
      return null;
    case "NUMBER":
      if (!value.trim() || !Number.isFinite(Number(value))) {
        return `${label} must be a number.`;
      }
      return null;
    case "DATE":
      if (!isRealDate(value)) return `${label} must be a date, as YYYY-MM-DD.`;
      return null;
    case "CHECKBOX":
      if (value !== "true" && value !== "false") {
        return `${label} must be true or false.`;
      }
      return null;
    case "USER":
      if (!value.trim()) return `${label} must name a user.`;
      return null;
    case "URL": {
      let parsed;
      try {
        parsed = new URL(value);
      } catch {
        return `${label} must be a URL.`;
      }
      if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
        return `${label} must be an http or https URL.`;
      }
      return null;
    }
    case "SELECT":
    case "MULTI_SELECT":
      if (!field.options.includes(value)) {
        return `"${value}" is not an option for ${label}.`;
      }
      return null;
    default:
      return null;
  }
}
function fieldValueError(field, values) {
  if (values.length === 0) return null;
  if (!fieldIsMultiValued(field.type) && values.length > 1) {
    return `${field.name} holds one value, not ${values.length}.`;
  }
  const seen = /* @__PURE__ */ new Set();
  for (const value of values) {
    if (seen.has(value)) return `${field.name} lists "${value}" twice.`;
    seen.add(value);
    const error = singleValueError(field, value);
    if (error) return error;
  }
  return null;
}
const FIELD_REQUIREMENTS = ["HIDDEN", "OPTIONAL", "REQUIRED"];
const FIELD_REQUIREMENT_LABELS = {
  HIDDEN: "Hidden",
  OPTIONAL: "Optional",
  REQUIRED: "Required"
};
function defaultRequirement(issueType, ref) {
  if (ref.customFieldId) return "OPTIONAL";
  const name = ref.builtinField;
  if (DEFAULT_BUG_ONLY_FIELDS.includes(name)) {
    return issueType === "BUG" ? "OPTIONAL" : "HIDDEN";
  }
  return "OPTIONAL";
}
function resolveFieldLayout(input) {
  const { issueType, customFields, config, screen } = input;
  const rules = /* @__PURE__ */ new Map();
  for (const rule of config) {
    if (rule.issueType !== issueType) continue;
    rules.set(fieldRefKey(rule), rule.requirement);
  }
  const candidates = [];
  for (const name of BUILTIN_FIELDS) {
    const ref = { builtinField: name, customFieldId: null };
    const key = fieldRefKey(ref);
    const requirement = rules.get(key) ?? defaultRequirement(issueType, ref);
    if (requirement === "HIDDEN") continue;
    candidates.push({ ref, key, label: BUILTIN_FIELD_LABELS[name], requirement, custom: null });
  }
  for (const field of customFields) {
    const ref = { builtinField: null, customFieldId: field.id };
    const key = fieldRefKey(ref);
    const requirement = rules.get(key) ?? defaultRequirement(issueType, ref);
    if (requirement === "HIDDEN") continue;
    candidates.push({ ref, key, label: field.name, requirement, custom: field });
  }
  if (!screen) return candidates;
  const byKey = new Map(candidates.map((c) => [c.key, c]));
  const ordered = [];
  const placed = /* @__PURE__ */ new Set();
  for (const ref of screen) {
    const key = fieldRefKey(ref);
    const candidate = byKey.get(key);
    if (!candidate || placed.has(key)) continue;
    placed.add(key);
    ordered.push(candidate);
  }
  for (const candidate of candidates) {
    if (candidate.requirement === "REQUIRED" && !placed.has(candidate.key)) {
      ordered.push(candidate);
    }
  }
  return ordered;
}
function fieldConfigError(rule) {
  const refError = fieldRefError(rule);
  if (refError) return refError;
  if (!FIELD_REQUIREMENTS.includes(rule.requirement)) {
    return `"${rule.requirement}" is not a field requirement.`;
  }
  return null;
}
export {
  BUILTIN_FIELDS,
  BUILTIN_FIELD_LABELS,
  CUSTOM_FIELD_KEY_MAX,
  CUSTOM_FIELD_TEXTAREA_MAX,
  CUSTOM_FIELD_TEXT_MAX,
  DEFAULT_BUG_ONLY_FIELDS,
  FIELD_REQUIREMENTS,
  FIELD_REQUIREMENT_LABELS,
  JIRA_CUSTOM_FIELD_TYPES,
  JIRA_CUSTOM_FIELD_TYPE_LABELS,
  customFieldError,
  customFieldKeyError,
  defaultRequirement,
  fieldConfigError,
  fieldIsMultiValued,
  fieldRefError,
  fieldRefKey,
  fieldTakesOptions,
  fieldValueError,
  isBuiltinField,
  resolveFieldLayout,
  sameFieldRef
};
