// Ported from SaaS packages/lib/src/utils/mentions.ts; see provenance.json.
function extractMentions(body) {
  if (!body) return [];
  const re = /@([a-zA-Z0-9._+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})/g;
  const out = /* @__PURE__ */ new Set();
  let m;
  while ((m = re.exec(body)) !== null) {
    if (m[1]) out.add(m[1].toLowerCase());
  }
  return [...out];
}
export {
  extractMentions
};
