// Generated from the upstream board application's read-only policy helpers; see provenance.json. No DB or network imports.
function checkProtectedPaths(config, changedPaths) {
  const patterns = config.guardrails.protectedPaths;
  const violations = [];
  for (const raw of changedPaths) {
    const path = normalize(raw);
    if (!path) continue;
    const pattern = patterns.find((p) => matchesGlob(path, p));
    if (pattern) violations.push({ path, pattern });
  }
  return { ok: violations.length === 0, violations };
}
function normalizeChangedPath(path) {
  return normalize(path);
}
function normalize(path) {
  return path.trim().replace(/\\/g, "/").replace(/^\.\//, "");
}
function matchesGlob(path, pattern) {
  const normalized = normalize(pattern);
  if (!normalized) return false;
  if (!/[*?]/.test(normalized)) {
    return path === normalized || path.startsWith(`${normalized}/`);
  }
  return globToRegExp(normalized).test(path);
}
function globToRegExp(pattern) {
  let out = "";
  for (let i = 0; i < pattern.length; i++) {
    const char = pattern[i];
    if (char === "*") {
      if (pattern[i + 1] === "*") {
        const rest = pattern.slice(i + 2);
        if (rest === "" || rest === "/") {
          out = out.replace(/\\\/$/, "(?:\\/.*)?");
          i = pattern.length;
          continue;
        }
        out += ".*";
        i++;
        continue;
      }
      out += "[^/]*";
      continue;
    }
    if (char === "?") {
      out += "[^/]";
      continue;
    }
    out += char.replace(/[.+^${}()|[\]\\/]/g, "\\$&");
  }
  return new RegExp(`^${out}$`);
}
function protectedPathsMentioned(patterns, text) {
  const seen = /* @__PURE__ */ new Set();
  const out = [];
  for (const raw of text.split(/[\s,;()[\]{}<>"'`|]+/)) {
    const token = normalizeChangedPath(raw).replace(/^[*+-]+/, "").replace(/[.:!?]+$/, "");
    if (!token || !isPathLike(token, patterns)) continue;
    const path = token.replace(/\/+$/, "");
    if (!path || seen.has(path)) continue;
    seen.add(path);
    const pattern = patterns.find((p) => matchesGlob(path, p) || coversPattern(path, p));
    if (pattern) out.push({ token: path, pattern });
  }
  return out;
}
function isPathLike(token, patterns) {
  if (/[/*]/.test(token)) return true;
  return patterns.some((p) => !/[*?]/.test(p) && normalize(p) === token);
}
function coversPattern(token, pattern) {
  const literal = normalizeChangedPath(pattern).replace(/\/?\*+.*$/, "");
  if (!literal) return false;
  return token === literal;
}
function docsMirrorNeeded(changedPaths) {
  return changedPaths.map((p) => normalizeChangedPath(p)).filter((p) => p.length > 0).filter((p) => /(^|\/)Docs\//.test(p) || /(^|\/)CLAUDE\.md$/.test(p));
}
export {
  checkProtectedPaths,
  docsMirrorNeeded,
  matchesGlob,
  normalizeChangedPath,
  protectedPathsMentioned
};
