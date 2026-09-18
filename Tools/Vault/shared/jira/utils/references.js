// Ported from SaaS packages/lib/src/utils/references.ts; see provenance.json.
import { PRODUCTS } from "../constants.js";
function pathFor(url) {
  if (!url) return null;
  const trimmed = url.trim();
  if (trimmed.startsWith("/")) return trimmed;
  for (const product of Object.values(PRODUCTS)) {
    if (trimmed.startsWith(product.url + "/")) {
      return trimmed.slice(product.url.length);
    }
    if (trimmed === product.url) {
      return "/";
    }
  }
  try {
    const u = new URL(trimmed);
    return u.pathname;
  } catch {
    return null;
  }
}
const ISSUE_RE = /^\/([a-z0-9-]+)\/project\/([A-Z0-9_-]+)\/issue\/(\d+)\/?$/;
const NOTION_PAGE_RE = /^\/([a-z0-9-]+)\/page\/([A-Za-z0-9_-]+)\/?$/;
const MERMAID_DIAGRAM_RE = /^\/([a-z0-9-]+)\/diagram\/([A-Za-z0-9_-]+)\/?$/;
function parseReference(url) {
  const path = pathFor(url);
  if (!path) return null;
  const issueMatch = ISSUE_RE.exec(path);
  if (issueMatch) {
    const [, orgSlug, projectKey, issueNum] = issueMatch;
    if (!orgSlug || !projectKey || !issueNum) return null;
    return {
      app: "jira",
      kind: "issue",
      orgSlug,
      projectKey,
      issueNumber: parseInt(issueNum, 10)
    };
  }
  const pageMatch = NOTION_PAGE_RE.exec(path);
  if (pageMatch) {
    const [, orgSlug, pageId] = pageMatch;
    if (!orgSlug || !pageId) return null;
    return { app: "notion", kind: "page", orgSlug, pageId };
  }
  const diagramMatch = MERMAID_DIAGRAM_RE.exec(path);
  if (diagramMatch) {
    const [, orgSlug, diagramId] = diagramMatch;
    if (!orgSlug || !diagramId) return null;
    return { app: "mermaid", kind: "diagram", orgSlug, diagramId };
  }
  return null;
}
function extractReferences(body) {
  if (!body) return [];
  const URL_RE = /https?:\/\/[^\s<]+/g;
  const out = [];
  let m;
  while ((m = URL_RE.exec(body)) !== null) {
    const url = trimTrailingPunctuation(m[0]);
    const ref = parseReference(url);
    if (ref) out.push({ url, ref });
  }
  return out;
}
const TRAILING_PUNCTUATION = /[.,;:!?)\]}'"]+$/;
function trimTrailingPunctuation(url) {
  return url.replace(TRAILING_PUNCTUATION, "");
}
export {
  extractReferences,
  parseReference
};
