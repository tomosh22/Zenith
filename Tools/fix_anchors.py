#!/usr/bin/env python3
"""Move ZENITH_TESTING anchor blocks to AFTER the #include "Zenith.h" line."""
import re
import sys
from pathlib import Path

ANCHOR_RE = re.compile(
    r'^#ifdef ZENITH_TESTING\n'
    r'extern "C" void Zenith_TestLink_(\w+)\(\) \{\}\n'
    r'#endif\n',
    re.MULTILINE
)

for arg in sys.argv[1:]:
    p = Path(arg)
    text = p.read_text(encoding='utf-8')
    m = ANCHOR_RE.search(text)
    if not m:
        print(f"SKIP: {p.name} (no anchor found)")
        continue
    name = m.group(1)
    # Remove the anchor block.
    without = ANCHOR_RE.sub('', text, count=1)
    # Find `#include "Zenith.h"` and insert anchor after.
    include_re = re.compile(r'(^#include\s+"Zenith\.h"\s*\n)', re.MULTILINE)
    replacement = f'\\1\n#ifdef ZENITH_TESTING\nextern "C" void Zenith_TestLink_{name}() {{}}\n#endif\n'
    new_text, n = include_re.subn(replacement, without, count=1)
    if n == 0:
        print(f"ERROR: {p.name} has no #include \"Zenith.h\"")
        continue
    p.write_text(new_text, encoding='utf-8')
    print(f"Fixed: {p.name}")
