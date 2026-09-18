// Ported from SaaS packages/lib/src/__tests__/references.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import { parseReference, extractReferences } from "../../shared/jira/utils/references.js";
import { PRODUCTS, issueUrl, pageUrl, diagramUrl, databaseUrl } from "../../shared/jira/constants.js";
describe("parseReference", () => {
  it("parses a Jira issue absolute URL", () => {
    expect(parseReference("http://localhost:3001/acme-corp/project/WEB/issue/3")).toEqual({
      app: "jira",
      kind: "issue",
      orgSlug: "acme-corp",
      projectKey: "WEB",
      issueNumber: 3
    });
  });
  it("parses a Jira issue relative path", () => {
    expect(parseReference("/acme-corp/project/WEB/issue/42")).toEqual({
      app: "jira",
      kind: "issue",
      orgSlug: "acme-corp",
      projectKey: "WEB",
      issueNumber: 42
    });
  });
  it("tolerates a trailing slash", () => {
    expect(parseReference("/acme-corp/project/WEB/issue/1/")).toEqual({
      app: "jira",
      kind: "issue",
      orgSlug: "acme-corp",
      projectKey: "WEB",
      issueNumber: 1
    });
  });
  it("parses a Notion page URL", () => {
    expect(parseReference("http://localhost:3004/acme-corp/page/abc123")).toEqual({
      app: "notion",
      kind: "page",
      orgSlug: "acme-corp",
      pageId: "abc123"
    });
  });
  it("parses a Mermaid diagram URL", () => {
    expect(parseReference("http://localhost:3003/acme-corp/diagram/xyz789")).toEqual({
      app: "mermaid",
      kind: "diagram",
      orgSlug: "acme-corp",
      diagramId: "xyz789"
    });
  });
  it("returns null for an unrelated URL", () => {
    expect(parseReference("https://example.com/some/path")).toBeNull();
  });
  it("returns null for an unrecognised in-app path", () => {
    expect(parseReference("/acme-corp/settings")).toBeNull();
  });
  it("returns null for malformed input", () => {
    expect(parseReference("")).toBeNull();
    expect(parseReference("not a url")).toBeNull();
  });
});
describe("extractReferences", () => {
  it("extracts a single recognised URL from a sentence", () => {
    const out = extractReferences(
      "See http://localhost:3001/acme-corp/project/WEB/issue/3 for details."
    );
    expect(out).toHaveLength(1);
    expect(out[0]?.ref.app).toBe("jira");
  });
  it("extracts multiple URLs in order", () => {
    const out = extractReferences(
      "Both http://localhost:3001/acme-corp/project/A/issue/1 and http://localhost:3004/acme-corp/page/p2 are relevant."
    );
    expect(out.map((x) => x.ref.app)).toEqual(["jira", "notion"]);
  });
  it("skips URLs that do not parse", () => {
    const out = extractReferences(
      "Try https://example.com or http://localhost:3001/acme-corp/project/WEB/issue/9"
    );
    expect(out).toHaveLength(1);
    expect(out[0]?.ref.kind).toBe("issue");
  });
  it("returns an empty array when nothing matches", () => {
    expect(extractReferences("Hello world")).toEqual([]);
  });
});
describe("URL builders round-trip through parseReference", () => {
  it("issueUrl", () => {
    expect(parseReference(issueUrl("acme-corp", "WEB", 42))).toEqual({
      app: "jira",
      kind: "issue",
      orgSlug: "acme-corp",
      projectKey: "WEB",
      issueNumber: 42
    });
  });
  it("issueUrl with a project key containing digits", () => {
    expect(parseReference(issueUrl("acme-corp", "ZEN2", 7))).toMatchObject({
      projectKey: "ZEN2",
      issueNumber: 7
    });
  });
  it("pageUrl", () => {
    expect(parseReference(pageUrl("acme-corp", "pg_123-abc"))).toEqual({
      app: "notion",
      kind: "page",
      orgSlug: "acme-corp",
      pageId: "pg_123-abc"
    });
  });
  it("diagramUrl", () => {
    expect(parseReference(diagramUrl("acme-corp", "dg_9"))).toEqual({
      app: "mermaid",
      kind: "diagram",
      orgSlug: "acme-corp",
      diagramId: "dg_9"
    });
  });
  it("every builder produces an absolute URL on its own product origin", () => {
    expect(issueUrl("o", "K", 1).startsWith(PRODUCTS.jira.url)).toBe(true);
    expect(pageUrl("o", "p").startsWith(PRODUCTS.notion.url)).toBe(true);
    expect(diagramUrl("o", "d").startsWith(PRODUCTS.mermaid.url)).toBe(true);
    expect(databaseUrl("o", "db").startsWith(PRODUCTS.notion.url)).toBe(true);
  });
  it("a built URL is extracted from prose as a reference", () => {
    const body = `See ${issueUrl("acme-corp", "WEB", 3)} and ${pageUrl("acme-corp", "pg1")}.`;
    expect(extractReferences(body).map((r) => r.ref.kind)).toEqual(["issue", "page"]);
  });
  it("databaseUrl points at the database view \u2014 rows have no route of their own", () => {
    expect(databaseUrl("acme-corp", "db_1")).toBe(`${PRODUCTS.notion.url}/acme-corp/database/db_1`);
    expect(parseReference(databaseUrl("acme-corp", "db_1"))).toBeNull();
  });
});
describe("extractReferences \u2014 sentence punctuation", () => {
  it("finds a link at the end of a sentence", () => {
    const refs = extractReferences(`Filed as ${issueUrl("acme-corp", "WEB", 3)}.`);
    expect(refs).toHaveLength(1);
    expect(refs[0].url.endsWith(".")).toBe(false);
    expect(refs[0].ref).toMatchObject({ issueNumber: 3 });
  });
  it("finds a link wrapped in parentheses", () => {
    const refs = extractReferences(`(see ${pageUrl("acme-corp", "pg1")})`);
    expect(refs).toHaveLength(1);
    expect(refs[0].url.endsWith(")")).toBe(false);
  });
  it("still finds a bare link with no trailing punctuation", () => {
    expect(extractReferences(issueUrl("acme-corp", "WEB", 3))).toHaveLength(1);
  });
  it("a trailing slash is still legal \u2014 it is part of the path, not punctuation", () => {
    expect(extractReferences(`${issueUrl("acme-corp", "WEB", 3)}/`)).toHaveLength(1);
  });
});
