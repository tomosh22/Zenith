#!/usr/bin/env python3
"""
check_conventions.py — Scan STAGED diff hunks for engine convention drift.

Rules enforced on ADDED lines only (the historical backlog is ignored):

  1. std::function forbidden in Zenith/. Allowlist via same-line comment:
         // ZENITH_ALLOW_STD_FUNCTION: <reason>
  2. std::vector forbidden in Zenith/. Use Zenith_Vector. No allowlist (the
     few legitimate exceptions, if any, will be documented per-case once
     they surface; for now the default is strict because the migration is
     in-flight).
  3. std::mutex forbidden in Zenith/. Use Zenith_Mutex.
  4. std::unordered_map / std::unordered_set forbidden in Zenith/. Use
     Zenith_HashMap / Zenith_HashSet. Allowlist via same-line comment:
         // #TODO: Replace with engine hash map   (the legacy form also
     acceptable during migration)
  5. A newly added .cpp file inside Zenith/ must have "#include \"Zenith.h\""
     as its first non-blank, non-comment line.

Scope: only files under Zenith/. Games/, Tools/, UnitTests/, Middleware/,
and ThirdParty paths are excluded because they have intentionally different
conventions (gameplay-heavy STL use, assimp/nlohmann dependencies, etc.).

Used by .githooks/pre-commit. Run manually with:
    python Tools/check_conventions.py --all
    python Tools/check_conventions.py --staged   (default)
"""

import argparse
import os
import re
import subprocess
import sys


# ---- Configuration ---------------------------------------------------------

# All paths below are relative to the repo root (where `git` is invoked).
ENGINE_ROOT = "Zenith/"
EXCLUDED_PREFIXES = (
    "Games/",
    "Tools/",
    "Middleware/",
    "ThirdParty/",
    ".claude/",
    ".opencode/",
    "Zenith/UnitTests/",
)

# Token patterns. Each rule is (name, regex, human_message, allowlist_predicate).
# allowlist_predicate: fn(line:str) -> bool; True means the line is allowed.
STD_FUNCTION_RE  = re.compile(r"\bstd::function\b")
STD_VECTOR_RE    = re.compile(r"\bstd::vector\b")
STD_MUTEX_RE     = re.compile(r"\bstd::mutex\b")
STD_UMAP_RE      = re.compile(r"\bstd::unordered_(?:map|set)\b")

# Legacy migration markers for map/set — while T1.4 is in flight the
# pre-commit hook allows unordered_map/set additions iff the line already
# carries the TODO-to-migrate marker. Once migrations complete we can tighten
# this to reject all new usage.
LEGACY_MAP_MARKER = re.compile(
    r"//\s*(?:#TODO|TODO):\s*Replace\s+(?:std::)?(?:with\s+)?engine\s+hash",
    re.IGNORECASE,
)


# ---- Diff parsing ----------------------------------------------------------

def run_git(args):
    result = subprocess.run(
        ["git"] + args, capture_output=True, text=True, check=False
    )
    if result.returncode not in (0, 1):
        sys.stderr.write(f"git {' '.join(args)} failed:\n{result.stderr}")
        sys.exit(2)
    return result.stdout


def get_staged_file_list():
    """Returns list of (status, path) tuples for staged files. Status letters:
    A=added, M=modified, R=renamed, C=copied. Deletions (D) are skipped."""
    out = run_git(["diff", "--cached", "--name-status", "--diff-filter=ACMR"])
    rows = []
    for line in out.splitlines():
        parts = line.split("\t")
        if not parts:
            continue
        status = parts[0][0]  # first char handles R100, C90, etc.
        # For rename/copy, destination is the last field.
        path = parts[-1]
        rows.append((status, path))
    return rows


def get_staged_added_lines(path):
    """For a staged file, returns the list of ADDED line strings in the
    diff (no leading '+', no hunk headers)."""
    out = run_git(["diff", "--cached", "-U0", "--", path])
    added = []
    for line in out.splitlines():
        if line.startswith("+++"):
            continue
        if line.startswith("+"):
            added.append(line[1:])
    return added


def get_full_file_lines(path):
    """For --all mode, returns all lines from the file on disk."""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read().splitlines()
    except FileNotFoundError:
        return []


# ---- Rule evaluation -------------------------------------------------------

def is_engine_file(path):
    if not path.startswith(ENGINE_ROOT):
        return False
    for prefix in EXCLUDED_PREFIXES:
        if path.startswith(prefix):
            return False
    return True


def _strip_line_comment(line):
    """Return `line` with any trailing // comment removed. Naïve — doesn't
    understand string literals — but good enough for convention token checks."""
    idx = line.find("//")
    return line[:idx] if idx >= 0 else line


def check_line_violations(path, line_index, line):
    """Returns a list of (rule_name, message) violations on the given line."""
    violations = []

    # Skip pure-comment and empty lines early — the patterns below are all
    # about source code. Matches both single-line `//` comments and block
    # comment continuation lines `* ...` (common in /** ... */ blocks).
    stripped = line.lstrip()
    if not stripped or stripped.startswith("//") or stripped.startswith("*"):
        return violations

    # Strip end-of-line comment portion so "/* hint about std::function */" in
    # a docstring doesn't trigger a violation. The allowlist markers themselves
    # live in the comment portion though, so keep the original `line` around
    # for those checks.
    code = _strip_line_comment(line)

    if STD_VECTOR_RE.search(code):
        violations.append((
            "std::vector",
            f"{path}:{line_index + 1}: std::vector is forbidden in Zenith/ "
            f"(use Zenith_Vector — see Collections/CLAUDE.md).",
        ))

    if STD_MUTEX_RE.search(code):
        violations.append((
            "std::mutex",
            f"{path}:{line_index + 1}: std::mutex is forbidden in Zenith/ "
            f"(use Zenith_Mutex).",
        ))

    if STD_UMAP_RE.search(code) and not LEGACY_MAP_MARKER.search(line):
        violations.append((
            "std::unordered_map/set",
            f"{path}:{line_index + 1}: std::unordered_map / std::unordered_set "
            f"is forbidden in Zenith/ (use Zenith_HashMap / Zenith_HashSet — "
            f"see Collections/Zenith_HashMap.h). If the migration for this "
            f"site has not happened yet, add a same-line "
            f"`// #TODO: Replace with engine hash map` marker.",
        ))

    return violations


def check_new_cpp_first_include(path):
    """A newly-added .cpp in engine scope must have Zenith.h as first
    non-trivial line. Returns a list of violations (0 or 1 message)."""
    if not path.endswith(".cpp"):
        return []
    lines = get_full_file_lines(path)
    for line in lines:
        stripped = line.strip()
        if not stripped:
            continue
        if stripped.startswith("//") or stripped.startswith("/*"):
            continue
        # First non-trivial line.
        if stripped != '#include "Zenith.h"':
            return [(
                "missing-pch",
                f"{path}: engine .cpp files must open with "
                f'`#include "Zenith.h"` as the first non-comment line. Got: '
                f"{stripped!r}",
            )]
        return []
    return []


# ---- Entry points ----------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--staged",
        action="store_true",
        help="Scan only lines added in the current staged diff (default).",
    )
    parser.add_argument(
        "--all",
        action="store_true",
        help="Scan the entire working tree (used for bootstrap audits; slow).",
    )
    args = parser.parse_args()

    mode_all = args.all
    if not mode_all:
        # Default mode is --staged whether or not the flag was passed.
        mode_all = False

    all_violations = []

    if mode_all:
        # Full-tree audit — walk Zenith/ on disk.
        for root, _dirs, files in os.walk(ENGINE_ROOT):
            for name in files:
                if not name.endswith((".h", ".cpp")):
                    continue
                path = os.path.join(root, name).replace(os.sep, "/")
                if not is_engine_file(path):
                    continue
                lines = get_full_file_lines(path)
                for i, line in enumerate(lines):
                    all_violations.extend(check_line_violations(path, i, line))
    else:
        # Staged-diff mode — only scan + lines on files under engine scope.
        for status, path in get_staged_file_list():
            if not is_engine_file(path):
                continue
            added_lines = get_staged_added_lines(path)
            for i, line in enumerate(added_lines):
                all_violations.extend(check_line_violations(path, i, line))
            if status == "A":
                all_violations.extend(check_new_cpp_first_include(path))

    if all_violations:
        # De-dup messages (same site can trip multiple rules; keep first).
        seen = set()
        ordered = []
        for rule, msg in all_violations:
            key = (rule, msg)
            if key in seen:
                continue
            seen.add(key)
            ordered.append((rule, msg))

        sys.stderr.write("\n")
        sys.stderr.write("CONVENTION CHECK FAILED\n")
        sys.stderr.write("-" * 60 + "\n")
        for _rule, msg in ordered:
            sys.stderr.write(msg + "\n")
        sys.stderr.write("-" * 60 + "\n")
        sys.stderr.write(
            f"{len(ordered)} violation(s). Fix or bypass with "
            f"`git commit --no-verify` (not recommended).\n"
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
