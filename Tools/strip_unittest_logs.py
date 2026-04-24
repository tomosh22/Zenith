#!/usr/bin/env python3
"""
Strip every call to Zenith_Log / Zenith_Warning / Zenith_Error whose first
argument is LOG_CATEGORY_UNITTEST from the given files. Handles multi-line
calls via paren matching, preserves indentation, and deletes the trailing
semicolon plus any immediately-adjacent blank line.

Usage:
    python strip_unittest_logs.py <file1.cpp> [<file2.cpp> ...]
"""
import re
import sys
from pathlib import Path

CALL_RE = re.compile(r'\bZenith_(?:Log|Warning|Error)\s*\(\s*LOG_CATEGORY_UNITTEST\b')


def find_matching_paren(text: str, open_pos: int) -> int:
    assert text[open_pos] == '('
    depth = 0
    i = open_pos
    in_str = False
    str_char = ''
    esc = False
    while i < len(text):
        c = text[i]
        if esc:
            esc = False
            i += 1
            continue
        if c == '\\' and in_str:
            esc = True
            i += 1
            continue
        if in_str:
            if c == str_char:
                in_str = False
            i += 1
            continue
        if c in ('"', "'"):
            in_str = True
            str_char = c
            i += 1
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def strip(text: str) -> tuple[str, int]:
    """Remove all matching calls. Returns (new_text, removed_count)."""
    out = []
    i = 0
    removed = 0
    while i < len(text):
        m = CALL_RE.search(text, i)
        if not m:
            out.append(text[i:])
            break
        # Find the opening paren (could have whitespace between name and paren).
        # The regex captures up through and including `(`? No, it matches up to
        # `LOG_CATEGORY_UNITTEST`. We need to back-track to find the `(`.
        # Easier: find the first `(` after `Zenith_Log` etc.
        call_start = m.start()
        # Locate the opening paren.
        open_paren = text.index('(', m.start())
        close_paren = find_matching_paren(text, open_paren)
        if close_paren == -1:
            out.append(text[i:])
            break
        # Look for trailing semicolon (possibly after whitespace).
        end = close_paren + 1
        while end < len(text) and text[end] in ' \t':
            end += 1
        if end < len(text) and text[end] == ';':
            end += 1
        # Swallow a trailing newline if the call occupies its own line.
        # First, back up `call_start` to the start of its line (preserving
        # indentation doesn't matter — we're removing the whole line if it's
        # the only thing there).
        line_start = call_start
        while line_start > 0 and text[line_start - 1] in ' \t':
            line_start -= 1
        if line_start > 0 and text[line_start - 1] != '\n':
            line_start = call_start  # call is NOT at the start of its line; keep left context
        # Swallow the newline after `end` if we removed a whole line.
        if line_start != call_start:
            # We're removing an entire line.
            if end < len(text) and text[end] == '\n':
                end += 1
        out.append(text[i:line_start])
        i = end
        removed += 1
    return ''.join(out), removed


def main():
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        sys.exit(1)
    total = 0
    for arg in sys.argv[1:]:
        p = Path(arg)
        text = p.read_text(encoding='utf-8')
        new_text, n = strip(text)
        if n == 0:
            continue
        p.write_text(new_text, encoding='utf-8')
        print(f"  {p.name}: removed {n} call(s)")
        total += n
    print(f"Total: {total}")


if __name__ == '__main__':
    main()
