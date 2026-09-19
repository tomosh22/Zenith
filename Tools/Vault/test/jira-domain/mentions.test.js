// Ported from the upstream board application's packages/lib/src/__tests__/mentions.test.ts; see provenance.json.
import { describe, it, expect } from "vitest";
import { extractMentions } from "../../shared/jira/utils/mentions.js";
describe("extractMentions", () => {
  it("returns an empty array for blank input", () => {
    expect(extractMentions("")).toEqual([]);
  });
  it("returns an empty array when no @-mentions are present", () => {
    expect(extractMentions("Hello world, no mentions.")).toEqual([]);
  });
  it("extracts a single email mention", () => {
    expect(extractMentions("Ping @bob@example.com please")).toEqual(["bob@example.com"]);
  });
  it("extracts multiple email mentions in order", () => {
    const out = extractMentions("@alice@example.com and @bob@example.com");
    expect(out.sort()).toEqual(["alice@example.com", "bob@example.com"]);
  });
  it("lowercases extracted emails", () => {
    expect(extractMentions("hey @Bob@Example.COM")).toEqual(["bob@example.com"]);
  });
  it("dedupes repeated mentions of the same address", () => {
    expect(extractMentions("@x@y.com hi @x@y.com again @x@y.com")).toEqual(["x@y.com"]);
  });
  it("ignores stray @ signs without an email", () => {
    expect(extractMentions("@nobody and @just text")).toEqual([]);
  });
  it("matches in the middle of a sentence and after punctuation", () => {
    expect(extractMentions("(see @a@b.co), then @c@d.io.")).toEqual(["a@b.co", "c@d.io"]);
  });
  it("does not match email-like strings without the leading @", () => {
    expect(extractMentions("plain bob@example.com mention")).toEqual([]);
  });
});
