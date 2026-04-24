#!/usr/bin/env python3
"""
Migrate Zenith test .cpp files from the class-based RunAllTests pattern to
the ZENITH_TEST macro / ZENITH_ASSERT_* framework.

Usage:
    python migrate_tests.py <file.cpp> <Category> [--class=ClassName]

The Category is what appears in ZENITH_TEST(Category, Name). ClassName defaults
to guessing from the file name (e.g. Zenith_PhysicsTests.cpp -> Zenith_PhysicsTests).
"""
import re
import sys
import argparse
from pathlib import Path


# --- Assertion conversion patterns ----------------------------------------

def convert_zenith_assert(match: re.Match) -> str:
    """
    Convert a single Zenith_Assert(cond, args...) call into the right
    ZENITH_ASSERT_* macro. `match` matches the full call including parens.
    """
    full = match.group(0)
    # Extract the content inside the outermost parens.
    inside = match.group(1).strip()

    # Split on top-level commas (not inside nested parens/strings).
    args = split_top_level_args(inside)
    if len(args) == 0:
        return full
    cond = args[0].strip()
    rest = args[1:]

    # Build the "msg, ..." suffix that gets passed to the macro.
    if rest:
        rest_str = ", " + ", ".join(rest)
    else:
        rest_str = ""

    # Pattern: ApproxEqual(a, b) or ApproxEqual(a, b, eps)
    m = re.match(r'^ApproxEqual\s*\((.+)\)$', cond)
    if m:
        inner = split_top_level_args(m.group(1))
        if len(inner) == 2:
            return f"ZENITH_ASSERT_EQ_FLOAT({inner[0]}, {inner[1]}, 0.01f{rest_str})"
        elif len(inner) == 3:
            return f"ZENITH_ASSERT_EQ_FLOAT({inner[0]}, {inner[1]}, {inner[2]}{rest_str})"

    # Pattern: strcmp(a, b) == 0  or  !strcmp(a, b)
    m = re.match(r'^!strcmp\s*\((.+)\)$', cond)
    if m:
        inner = split_top_level_args(m.group(1))
        if len(inner) == 2:
            return f"ZENITH_ASSERT_STREQ({inner[0]}, {inner[1]}{rest_str})"
    m = re.match(r'^strcmp\s*\((.+)\)\s*==\s*0$', cond)
    if m:
        inner = split_top_level_args(m.group(1))
        if len(inner) == 2:
            return f"ZENITH_ASSERT_STREQ({inner[0]}, {inner[1]}{rest_str})"

    # Pattern: std::fabs(a - b) < eps  or  fabs(a - b) < eps  or  abs(a - b) < eps
    # Extract the argument to fabs/abs via paren matching, then split on the
    # top-level `-` (not `->`).
    m = re.match(r'^(?:std::)?(?:fabs|abs)\s*\((.*)\)\s*<\s*(.+)$', cond)
    if m:
        # The arg string may have nested parens; split on top-level '-' only.
        arg = m.group(1)
        eps = m.group(2)
        # Find top-level '-' not preceded by '<' and not followed by '>' (->).
        depth = 0
        template_depth = 0
        in_string = False
        string_char = None
        i = 0
        while i < len(arg):
            c = arg[i]
            if in_string:
                if c == '\\' and i + 1 < len(arg):
                    i += 2
                    continue
                if c == string_char:
                    in_string = False
                i += 1
                continue
            if c in ('"', "'"):
                in_string = True
                string_char = c
                i += 1
                continue
            if c in '([{':
                depth += 1
            elif c in ')]}':
                depth -= 1
            elif c == '<' and i > 0 and (arg[i - 1].isalnum() or arg[i - 1] == '_'):
                template_depth += 1
            elif c == '>' and template_depth > 0:
                template_depth -= 1
            elif c == '-' and depth == 0 and template_depth == 0:
                # Skip `->` (member access) and `--` (decrement).
                if i + 1 < len(arg) and arg[i + 1] in '>-':
                    i += 1
                    continue
                # Skip leading unary minus at start of arg.
                if i == 0:
                    i += 1
                    continue
                lhs = arg[:i].strip()
                rhs = arg[i + 1:].strip()
                return f"ZENITH_ASSERT_EQ_FLOAT({lhs}, {rhs}, {eps}{rest_str})"
            i += 1
        # Couldn't split — leave as TRUE assertion.

    # If the condition contains top-level `&&` or `||`, leave as ZENITH_ASSERT_TRUE
    # (we can't split these into multiple asserts mechanically).
    if split_on_top_level_op(cond, '&&') is not None or \
       split_on_top_level_op(cond, '||') is not None:
        return f"ZENITH_ASSERT_TRUE({cond}{rest_str})"

    # Pattern: ptr != nullptr  or  ptr != NULL (not 0 - that's ambiguous)
    m = re.match(r'^(.+?)\s*!=\s*(?:nullptr|NULL)$', cond)
    if m:
        return f"ZENITH_ASSERT_NOT_NULL({m.group(1).strip()}{rest_str})"
    # Pattern: nullptr != ptr (rare)
    m = re.match(r'^(?:nullptr|NULL)\s*!=\s*(.+)$', cond)
    if m:
        return f"ZENITH_ASSERT_NOT_NULL({m.group(1).strip()}{rest_str})"

    # Pattern: ptr == nullptr (not == 0 - that's ambiguous)
    m = re.match(r'^(.+?)\s*==\s*(?:nullptr|NULL)$', cond)
    if m:
        return f"ZENITH_ASSERT_NULL({m.group(1).strip()}{rest_str})"
    m = re.match(r'^(?:nullptr|NULL)\s*==\s*(.+)$', cond)
    if m:
        return f"ZENITH_ASSERT_NULL({m.group(1).strip()}{rest_str})"

    # Pattern: a == b  or  a != b  (not nullptr) → ZENITH_ASSERT_EQ / NE
    parts = split_on_top_level_op(cond, '==')
    if parts is not None:
        return f"ZENITH_ASSERT_EQ({parts[0]}, {parts[1]}{rest_str})"
    parts = split_on_top_level_op(cond, '!=')
    if parts is not None:
        return f"ZENITH_ASSERT_NE({parts[0]}, {parts[1]}{rest_str})"

    # Comparison operators — keep in order of longest-match-first so >= beats >.
    # We don't convert `==`/`!=` to ZENITH_ASSERT_EQ/NE automatically because
    # those are more complex (types must match, etc.). Leave those as
    # ZENITH_ASSERT_TRUE(a == b) which always works.
    for op, macro in [('>=', 'GE'), ('<=', 'LE'), ('>', 'GT'), ('<', 'LT')]:
        # Find top-level op (not inside nested parens).
        parts = split_on_top_level_op(cond, op)
        if parts is not None:
            lhs, rhs = parts
            return f"ZENITH_ASSERT_{macro}({lhs}, {rhs}{rest_str})"

    # Pattern: !expr  ->  ZENITH_ASSERT_FALSE(expr)
    if cond.startswith('!') and not cond.startswith('!='):
        inner = cond[1:].strip()
        # Strip outer parens if they enclose the whole expression.
        if inner.startswith('(') and inner.endswith(')') and matches_parens(inner):
            inner = inner[1:-1].strip()
        return f"ZENITH_ASSERT_FALSE({inner}{rest_str})"

    # Default: ZENITH_ASSERT_TRUE with the whole expression.
    return f"ZENITH_ASSERT_TRUE({cond}{rest_str})"


def matches_parens(s: str) -> bool:
    """Check that the first '(' matches the last ')'."""
    if not (s.startswith('(') and s.endswith(')')):
        return False
    depth = 0
    for i, c in enumerate(s):
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0 and i != len(s) - 1:
                return False
    return depth == 0


def split_top_level_args(s: str) -> list[str]:
    """Split a comma-separated arg list at top level (not inside parens or strings)."""
    parts = []
    current = []
    depth = 0
    in_string = False
    string_char = None
    escape = False
    for c in s:
        if escape:
            current.append(c)
            escape = False
            continue
        if c == '\\' and in_string:
            current.append(c)
            escape = True
            continue
        if in_string:
            current.append(c)
            if c == string_char:
                in_string = False
            continue
        if c in ('"', "'"):
            in_string = True
            string_char = c
            current.append(c)
            continue
        if c in '([{<':
            # Only track parens/brackets; < is tricky (template vs less-than)
            if c in '([{':
                depth += 1
            current.append(c)
            continue
        if c in ')]}':
            depth -= 1
            current.append(c)
            continue
        if c == ',' and depth == 0:
            parts.append(''.join(current).strip())
            current = []
            continue
        current.append(c)
    if current:
        parts.append(''.join(current).strip())
    return parts


def split_on_top_level_op(s: str, op: str):
    """
    Split `s` on the first top-level occurrence of `op`. Returns (lhs, rhs)
    or None if not found. Operator must not be inside strings, parens, or
    what looks like template argument lists.
    """
    depth = 0
    template_depth = 0
    in_string = False
    string_char = None
    escape = False
    i = 0
    op_len = len(op)
    while i < len(s):
        c = s[i]
        if escape:
            escape = False
            i += 1
            continue
        if c == '\\' and in_string:
            escape = True
            i += 1
            continue
        if in_string:
            if c == string_char:
                in_string = False
            i += 1
            continue
        if c in ('"', "'"):
            in_string = True
            string_char = c
            i += 1
            continue
        if c in '([{':
            depth += 1
            i += 1
            continue
        if c in ')]}':
            depth -= 1
            i += 1
            continue
        # Heuristic: '<' preceded by a word char looks like a template opener.
        # Track template depth separately so we don't match operator '<' / '>'
        # inside template argument lists.
        if c == '<' and i > 0 and (s[i - 1].isalnum() or s[i - 1] == '_'):
            template_depth += 1
            i += 1
            continue
        if c == '>' and template_depth > 0:
            template_depth -= 1
            i += 1
            continue
        if depth == 0 and template_depth == 0 and s[i:i + op_len] == op:
            # Disambiguate `>` from `>=`, `<` from `<=`, etc.
            if op in ('>', '<') and i + op_len < len(s) and s[i + op_len] == '=':
                i += 1
                continue
            if op in ('>', '<') and i > 0 and s[i - 1] in '<>=!-+':
                i += 1
                continue
            # Disambiguate `=` in `==` / `!=` from assignment (not relevant here
            # because we only call this for comparison ops, but be safe).
            if op == '==' and i > 0 and s[i - 1] in '<>=!':
                i += 1
                continue
            if op == '!=' and i > 0 and s[i - 1] == '!':
                i += 1
                continue
            lhs = s[:i].strip()
            rhs = s[i + op_len:].strip()
            return lhs, rhs
        i += 1
    return None


# --- Per-file driver ------------------------------------------------------

def find_matching_paren(text: str, open_pos: int) -> int:
    """Return the index of the ')' matching the '(' at open_pos, or -1 if none."""
    assert text[open_pos] == '('
    depth = 0
    i = open_pos
    in_string = False
    string_char = None
    escape = False
    while i < len(text):
        c = text[i]
        if escape:
            escape = False
            i += 1
            continue
        if c == '\\' and in_string:
            escape = True
            i += 1
            continue
        if in_string:
            if c == string_char:
                in_string = False
            i += 1
            continue
        if c in ('"', "'"):
            in_string = True
            string_char = c
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


def convert_all_asserts(text: str) -> str:
    """Find every Zenith_Assert(...) call and replace it."""
    out = []
    i = 0
    pattern = re.compile(r'\bZenith_Assert\s*\(')
    while i < len(text):
        m = pattern.search(text, i)
        if not m:
            out.append(text[i:])
            break
        out.append(text[i:m.start()])
        # Find matching paren.
        open_paren = m.end() - 1
        close_paren = find_matching_paren(text, open_paren)
        if close_paren == -1:
            out.append(text[m.start():])
            break
        # Full call text.
        call = text[m.start():close_paren + 1]
        inside = text[open_paren + 1:close_paren]
        # Wrap in a fake match object for convert_zenith_assert.
        fake = type('M', (), {'group': lambda self, n: call if n == 0 else inside})()
        out.append(convert_zenith_assert(fake))
        i = close_paren + 1
    return ''.join(out)


def convert_method_defs(text: str, class_name: str, category: str, forward: bool) -> str:
    """
    Convert test method definitions.
    - forward=False: `void ClassName::TestXxx()` → `ZENITH_TEST(Category, Xxx)`
      (turns into free function, loses friend access).
    - forward=True:  keep method signature, insert
      `ZENITH_TEST(Category, Xxx) { ClassName::TestXxx(); }` BEFORE the
      method definition (preserves friend access; ZENITH_TEST calls through).
    """
    escaped_cls = re.escape(class_name)
    pattern = re.compile(
        r'^(\s*)void\s+' + escaped_cls + r'::(\w+)\s*\(\s*\)\s*',
        re.MULTILINE
    )
    def repl(m: re.Match) -> str:
        indent = m.group(1)
        method = m.group(2)
        short = method[4:] if method.startswith('Test') else method
        if forward:
            # ZENITH_TEST wrapper at file scope; forwards to class member which
            # has friend access. Placed BEFORE the method definition so the
            # method body follows unchanged.
            return (
                f'{indent}ZENITH_TEST({category}, {short}) {{ {class_name}::{method}(); }}\n'
                f'{indent}void {class_name}::{method}()'
            )
        return f'{indent}ZENITH_TEST({category}, {short})\n'
    return pattern.sub(repl, text)


def strip_runallatests(text: str, class_name: str) -> str:
    """
    Delete the `void ClassName::RunAllTests() { ... }` function entirely.
    """
    escaped_cls = re.escape(class_name)
    pattern = re.compile(
        r'^void\s+' + escaped_cls + r'::RunAllTests\s*\(\s*\)\s*',
        re.MULTILINE
    )
    m = pattern.search(text)
    if not m:
        return text
    # Find the opening brace.
    brace_start = text.find('{', m.end())
    if brace_start == -1:
        return text
    # Find matching close brace.
    depth = 0
    i = brace_start
    in_string = False
    string_char = None
    escape = False
    in_line_comment = False
    in_block_comment = False
    while i < len(text):
        c = text[i]
        cnext = text[i + 1] if i + 1 < len(text) else ''
        if escape:
            escape = False
            i += 1
            continue
        if in_line_comment:
            if c == '\n':
                in_line_comment = False
            i += 1
            continue
        if in_block_comment:
            if c == '*' and cnext == '/':
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue
        if in_string:
            if c == '\\':
                escape = True
                i += 1
                continue
            if c == string_char:
                in_string = False
            i += 1
            continue
        if c == '/' and cnext == '/':
            in_line_comment = True
            i += 2
            continue
        if c == '/' and cnext == '*':
            in_block_comment = True
            i += 2
            continue
        if c in ('"', "'"):
            in_string = True
            string_char = c
            i += 1
            continue
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[:m.start()] + text[i + 1:].lstrip('\n')
        i += 1
    return text


def strip_self_include(text: str, header: str) -> str:
    """Delete `#include "UnitTests/<header>"` and `#include "<header>"` lines."""
    patterns = [
        r'^\s*#include\s*"UnitTests/' + re.escape(header) + r'"\s*\n',
        r'^\s*#include\s*"' + re.escape(header) + r'"\s*\n',
    ]
    for p in patterns:
        text = re.sub(p, '', text, flags=re.MULTILINE)
    return text


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('file')
    parser.add_argument('category')
    parser.add_argument('--class', dest='class_names', action='append', default=[],
        help='Class name(s) whose methods to convert. May be specified multiple times. '
             'Defaults to the file stem.')
    parser.add_argument('--forward', action='store_true',
        help='Keep methods as class members and emit ZENITH_TEST forwarders. '
             'Preserves friend access on test classes. Default: free-function rewrite.')
    args = parser.parse_args()

    path = Path(args.file)
    text = path.read_text(encoding='utf-8')

    # Derive class name from filename if none provided.
    class_names = args.class_names if args.class_names else [path.stem]
    header = path.stem + '.h'

    # 1. Strip self-include.
    text = strip_self_include(text, header)
    # 2. Delete RunAllTests() for each class.
    for cls in class_names:
        text = strip_runallatests(text, cls)
    # 3. Convert method definitions to ZENITH_TEST (forwarding or inline).
    for cls in class_names:
        text = convert_method_defs(text, cls, args.category, args.forward)
    # 4. Convert Zenith_Assert calls.
    text = convert_all_asserts(text)

    path.write_text(text, encoding='utf-8')
    print(f"Migrated: {path}")


if __name__ == '__main__':
    main()
