// Ported from SaaS packages/lib/src/__tests__/permissions.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import {
  DEFAULT_PROJECT_ROLES,
  defaultPermission,
  grantSatisfied,
  JIRA_GRANT_TYPES,
  JIRA_GRANT_TYPE_LABELS,
  JIRA_PERMISSIONS,
  JIRA_PERMISSION_LABELS,
  permissionGrantError,
  resolvePermission
} from "../../shared/jira/permissions.js";
function ctx(overrides = {}) {
  return {
    userId: "u_actor",
    orgRole: "MEMBER",
    projectRoleIds: [],
    issue: null,
    ...overrides
  };
}
function grant(overrides = {}) {
  return { permission: "EDIT_ISSUE", grantType: "ANY_LOGGED_IN", value: null, ...overrides };
}
describe("permission vocabulary", () => {
  it("labels every permission and grant type", () => {
    for (const p of JIRA_PERMISSIONS) expect(JIRA_PERMISSION_LABELS[p]).toBeTruthy();
    for (const g of JIRA_GRANT_TYPES) expect(JIRA_GRANT_TYPE_LABELS[g]).toBeTruthy();
  });
  it("ships three default project roles", () => {
    expect(DEFAULT_PROJECT_ROLES).toEqual(["Administrators", "Developers", "Viewers"]);
  });
});
describe("grantSatisfied", () => {
  it("matches a project role the actor holds", () => {
    const rule = grant({ grantType: "PROJECT_ROLE", value: "role_dev" });
    expect(grantSatisfied(rule, ctx({ projectRoleIds: ["role_dev"] }))).toBe(true);
    expect(grantSatisfied(rule, ctx({ projectRoleIds: ["role_other"] }))).toBe(false);
    expect(grantSatisfied(rule, ctx())).toBe(false);
  });
  it("treats ORG_ROLE as a floor, not an equality", () => {
    const rule = grant({ grantType: "ORG_ROLE", value: "MEMBER" });
    expect(grantSatisfied(rule, ctx({ orgRole: "MEMBER" }))).toBe(true);
    expect(grantSatisfied(rule, ctx({ orgRole: "ADMIN" }))).toBe(true);
    expect(grantSatisfied(rule, ctx({ orgRole: "OWNER" }))).toBe(true);
    expect(grantSatisfied(rule, ctx({ orgRole: "VIEWER" }))).toBe(false);
  });
  it("matches anybody signed in", () => {
    expect(grantSatisfied(grant({ grantType: "ANY_LOGGED_IN" }), ctx({ orgRole: "VIEWER" }))).toBe(
      true
    );
  });
  it("matches the assignee and the reporter by identity", () => {
    const assignee = grant({ grantType: "ASSIGNEE" });
    const reporter = grant({ grantType: "REPORTER" });
    const mine = ctx({ issue: { assigneeId: "u_actor", reporterId: "u_other" } });
    expect(grantSatisfied(assignee, mine)).toBe(true);
    expect(grantSatisfied(reporter, mine)).toBe(false);
  });
  it("does NOT match an issue-relative grant when there is no issue", () => {
    expect(grantSatisfied(grant({ grantType: "ASSIGNEE" }), ctx({ issue: null }))).toBe(false);
    expect(grantSatisfied(grant({ grantType: "REPORTER" }), ctx({ issue: void 0 }))).toBe(false);
  });
  it("refuses a role-shaped grant with no value rather than matching", () => {
    expect(grantSatisfied(grant({ grantType: "PROJECT_ROLE", value: null }), ctx())).toBe(false);
    expect(grantSatisfied(grant({ grantType: "ORG_ROLE", value: null }), ctx())).toBe(false);
  });
});
describe("permissionGrantError", () => {
  it("accepts well-formed grants", () => {
    expect(permissionGrantError(grant())).toBeNull();
    expect(
      permissionGrantError(grant({ grantType: "PROJECT_ROLE", value: "role_dev" }))
    ).toBeNull();
    expect(permissionGrantError(grant({ grantType: "ORG_ROLE", value: "ADMIN" }))).toBeNull();
  });
  it("refuses an unknown permission or grant type", () => {
    expect(permissionGrantError(grant({ permission: "FLY" }))).toMatch(/not a permission/);
    expect(
      permissionGrantError(grant({ grantType: "VIBES" }))
    ).toMatch(/not a grant type/);
  });
  it("refuses a role grant with no role", () => {
    expect(permissionGrantError(grant({ grantType: "PROJECT_ROLE", value: null }))).toMatch(
      /needs a role/
    );
    expect(permissionGrantError(grant({ grantType: "ORG_ROLE", value: null }))).toMatch(
      /needs a role/
    );
    expect(permissionGrantError(grant({ grantType: "ORG_ROLE", value: "GOD" }))).toMatch(
      /not an organisation role/
    );
  });
});
describe("defaultPermission \u2014 the behaviour before schemes existed", () => {
  it("lets a VIEWER browse and nothing else", () => {
    expect(defaultPermission("BROWSE_PROJECT", "VIEWER")).toBe(true);
    expect(defaultPermission("EDIT_ISSUE", "VIEWER")).toBe(false);
    expect(defaultPermission("TRANSITION_ISSUE", "VIEWER")).toBe(false);
    expect(defaultPermission("CREATE_ISSUE", "VIEWER")).toBe(false);
  });
  it("lets a MEMBER do the work but not administer", () => {
    expect(defaultPermission("TRANSITION_ISSUE", "MEMBER")).toBe(true);
    expect(defaultPermission("EDIT_ISSUE", "MEMBER")).toBe(true);
    expect(defaultPermission("MANAGE_SPRINTS", "MEMBER")).toBe(true);
    expect(defaultPermission("MANAGE_VERSIONS", "MEMBER")).toBe(false);
    expect(defaultPermission("ADMINISTER_PROJECT", "MEMBER")).toBe(false);
  });
  it("lets an ADMIN and an OWNER do everything", () => {
    for (const role of ["ADMIN", "OWNER"]) {
      for (const permission of JIRA_PERMISSIONS) {
        expect(defaultPermission(permission, role), `${role}/${permission}`).toBe(true);
      }
    }
  });
});
describe("resolvePermission", () => {
  it("falls back to the org-role defaults with NO scheme", () => {
    expect(resolvePermission("TRANSITION_ISSUE", null, ctx({ orgRole: "MEMBER" }))).toBe(true);
    expect(resolvePermission("TRANSITION_ISSUE", null, ctx({ orgRole: "VIEWER" }))).toBe(false);
  });
  it("grants NOTHING with an empty scheme, even to an owner", () => {
    expect(resolvePermission("TRANSITION_ISSUE", [], ctx({ orgRole: "OWNER" }))).toBe(false);
  });
  it("reads only the grants for the permission asked about", () => {
    const grants = [
      grant({ permission: "EDIT_ISSUE", grantType: "ANY_LOGGED_IN" }),
      grant({ permission: "TRANSITION_ISSUE", grantType: "ORG_ROLE", value: "ADMIN" })
    ];
    expect(resolvePermission("EDIT_ISSUE", grants, ctx({ orgRole: "VIEWER" }))).toBe(true);
    expect(resolvePermission("TRANSITION_ISSUE", grants, ctx({ orgRole: "MEMBER" }))).toBe(false);
    expect(resolvePermission("TRANSITION_ISSUE", grants, ctx({ orgRole: "ADMIN" }))).toBe(true);
    expect(resolvePermission("DELETE_ISSUE", grants, ctx({ orgRole: "OWNER" }))).toBe(false);
  });
  it("is satisfied by ANY one matching grant", () => {
    const grants = [
      grant({ permission: "TRANSITION_ISSUE", grantType: "PROJECT_ROLE", value: "role_lead" }),
      grant({ permission: "TRANSITION_ISSUE", grantType: "ASSIGNEE" })
    ];
    expect(
      resolvePermission(
        "TRANSITION_ISSUE",
        grants,
        ctx({ issue: { assigneeId: "u_actor", reporterId: null } })
      )
    ).toBe(true);
    expect(
      resolvePermission("TRANSITION_ISSUE", grants, ctx({ projectRoleIds: ["role_lead"] }))
    ).toBe(true);
    expect(resolvePermission("TRANSITION_ISSUE", grants, ctx())).toBe(false);
  });
});
