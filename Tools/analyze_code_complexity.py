#!/usr/bin/env python3
"""
Code Complexity and Maintainability Analyzer for C/C++ Codebases

This script analyzes C/C++ source files and computes various metrics:
- Cyclomatic Complexity (independent paths through code)
- Cognitive Complexity (human effort to understand)
- Halstead Metrics (operators/operands analysis)
- Lines of Code (LOC, SLOC, comment lines, blank lines)
- Maintainability Index

Results are visualized by directory and source file.
"""

import os
import re
import sys
import math
import argparse
from pathlib import Path
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple, Set
from collections import defaultdict

# Try to import visualization libraries
try:
    import matplotlib.pyplot as plt
    import matplotlib.patches as mpatches
    from matplotlib.colors import LinearSegmentedColormap
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Warning: matplotlib not found. Install with 'pip install matplotlib numpy' for visualizations.")

# Optional tree-sitter parser — opt-in via `--parser tree-sitter`. Default
# remains the regex parser so the tool stays stdlib-only out of the box.
# We import lazily inside the helper so worker processes also initialize it.
try:
    import tree_sitter  # noqa: F401
    import tree_sitter_cpp  # noqa: F401
    HAS_TREE_SITTER = True
except ImportError:
    HAS_TREE_SITTER = False


_TS_LANGUAGE = None
_TS_PARSER = None


def _get_ts_parser():
    """Lazy-initialize (and cache per-process) the tree-sitter C++ parser."""
    global _TS_LANGUAGE, _TS_PARSER
    if _TS_PARSER is not None:
        return _TS_PARSER
    if not HAS_TREE_SITTER:
        return None
    from tree_sitter import Language, Parser
    import tree_sitter_cpp as tscpp
    _TS_LANGUAGE = Language(tscpp.language())
    _TS_PARSER = Parser(_TS_LANGUAGE)
    return _TS_PARSER


# Control-flow keywords that open a nestable block and contribute to cognitive complexity.
# Kept as a tuple so iteration order is stable; lookup via set for O(1).
_COGNITIVE_NESTING_KEYWORDS: Tuple[str, ...] = (
    'if', 'else', 'for', 'while', 'do', 'switch', 'catch', 'try'
)
_COGNITIVE_NESTING_SET: Set[str] = set(_COGNITIVE_NESTING_KEYWORDS)
_COGNITIVE_KEYWORD_PATTERNS: Tuple[re.Pattern, ...] = tuple(
    re.compile(rf'\b{kw}\b') for kw in _COGNITIVE_NESTING_KEYWORDS
)
_LOGICAL_OP_PATTERN: re.Pattern = re.compile(r'&&|\|\|')
_GOTO_PATTERN: re.Pattern = re.compile(r'\bgoto\b')
# Cyclomatic complexity decision points (one regex per construct to match the existing counting semantics).
_CC_PATTERNS: Tuple[re.Pattern, ...] = tuple(re.compile(p) for p in (
    r'\bif\b', r'\bfor\b', r'\bwhile\b',
    r'\bdo\b', r'\bcase\b', r'\bcatch\b',
    r'(?<![?!=<>])\?(?![?=])',  # ternary operator
    r'&&', r'\|\|',
))
_RETURN_PATTERN: re.Pattern = re.compile(r'\breturn\b')
_CASE_PATTERN: re.Pattern = re.compile(r'\bcase\b')
# Tech-debt markers: TODO / FIXME / HACK / XXX. Captured so we can embed the surrounding text.
_TECH_DEBT_PATTERN: re.Pattern = re.compile(
    r'\b(TODO|FIXME|HACK|XXX)\b[:\s]?[ \t]*(.{0,120}?)(?=$|\n)'
)
# Local `#include "..."` — system `<...>` includes are intentionally skipped
# so the graph reflects only this codebase's internal coupling.
_LOCAL_INCLUDE_PATTERN: re.Pattern = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.MULTILINE)

# Anti-pattern thresholds (overridable via --thresholds JSON). Values picked to be
# loud enough that firing means something, quiet enough not to drown out signal.
DEFAULT_LLM_MODEL = "claude-sonnet-4-6"

DEFAULT_THRESHOLDS: Dict[str, float] = {
    'long_function_loc': 80,
    'very_long_function_loc': 200,
    'deep_nesting': 5,
    'complex_branching_cc': 15,
    'hard_to_follow_cognitive': 25,
    'many_params': 6,
    'many_returns': 6,
    'big_switch_cases': 10,
    'big_switch_max_nesting': 3,
    'nesting_hell_depth': 5,
    'nesting_hell_max_cases': 5,
    'low_mi': 40,
    'god_file_loc': 1000,
    'god_file_funcs': 40,
    'extract_candidate_depth': 4,
    'extract_candidate_min_lines': 15,
    # Priority score blend weights (must sum to 1.0).
    'priority_weight_cognitive': 0.30,
    'priority_weight_cyclomatic': 0.25,
    'priority_weight_nesting': 0.20,
    'priority_weight_length': 0.15,
    'priority_weight_params': 0.10,
}

# Tag -> short human hint used in the markdown refactoring queue. Kept short and
# conservative — the tags themselves, not the prose, are the actionable signal.
_TAG_SUGGESTIONS: Dict[str, str] = {
    'nesting-hell': 'flatten with guard clauses / early returns; extract inner branches into helpers',
    'big-switch': 'consider a dispatch table or polymorphism if the switch is on a type/enum',
    'long-function': 'extract cohesive blocks into helpers',
    'very-long-function': 'split into multiple functions; identify the 2-3 core responsibilities',
    'deep-nesting': 'extract inner blocks; use early returns to reduce depth',
    'complex-branching': 'extract branches into named helpers or strategy objects',
    'hard-to-follow': 'break up into smaller, named steps',
    'many-params': 'introduce a parameter object / struct',
    'many-returns': 'check whether returns can be consolidated via result-holding variable',
    'low-mi': 'general maintainability issue; start with the worst functions in this file',
    'god-file': 'file is doing too much; split by responsibility into separate files',
}


@dataclass
class HalsteadMetrics:
    """Halstead complexity metrics"""
    n1: int = 0  # Number of distinct operators
    n2: int = 0  # Number of distinct operands
    N1: int = 0  # Total number of operators
    N2: int = 0  # Total number of operands
    
    @property
    def vocabulary(self) -> int:
        """Program vocabulary (n = n1 + n2)"""
        return self.n1 + self.n2
    
    @property
    def length(self) -> int:
        """Program length (N = N1 + N2)"""
        return self.N1 + self.N2
    
    @property
    def calculated_length(self) -> float:
        """Calculated program length"""
        if self.n1 == 0 or self.n2 == 0:
            return 0
        return self.n1 * math.log2(self.n1) + self.n2 * math.log2(self.n2)
    
    @property
    def volume(self) -> float:
        """Program volume (V = N * log2(n))"""
        if self.vocabulary == 0:
            return 0
        return self.length * math.log2(self.vocabulary)
    
    @property
    def difficulty(self) -> float:
        """Program difficulty (D = (n1/2) * (N2/n2))"""
        if self.n2 == 0:
            return 0
        return (self.n1 / 2) * (self.N2 / self.n2)
    
    @property
    def effort(self) -> float:
        """Programming effort (E = D * V)"""
        return self.difficulty * self.volume
    
    @property
    def time_to_program(self) -> float:
        """Time to program in seconds (T = E / 18)"""
        return self.effort / 18
    
    @property
    def bugs_delivered(self) -> float:
        """Estimated bugs (B = V / 3000)"""
        return self.volume / 3000


@dataclass
class FileMetrics:
    """Metrics for a single source file"""
    filepath: str = ''
    total_lines: int = 0
    code_lines: int = 0
    comment_lines: int = 0
    blank_lines: int = 0
    cyclomatic_complexity: int = 1
    cognitive_complexity: int = 0
    halstead: HalsteadMetrics = field(default_factory=HalsteadMetrics)
    function_count: int = 0
    class_count: int = 0
    max_nesting_depth: int = 0
    # Populated during post-analysis; blends file-level metrics with the worst function
    # score so "one awful function in a small file" doesn't hide behind averages.
    priority_score: float = 0.0
    priority_factors: Dict[str, float] = field(default_factory=dict)
    worst_function_score: float = 0.0
    # Tech-debt markers (TODO/FIXME/HACK/XXX); `tech_debt_markers` holds location+text.
    todo_count: int = 0
    fixme_count: int = 0
    hack_count: int = 0
    tech_debt_markers: List[Dict] = field(default_factory=list)
    tags: List[str] = field(default_factory=list)
    # Include-graph coupling. `includes` is the basenames of `#include "..."` targets
    # captured during parsing (system `<...>` includes are skipped — they're not
    # part of this codebase). `fan_in` / `fan_out` / `in_cycle` are populated by
    # `_compute_include_graph` after all files are analyzed.
    includes: List[str] = field(default_factory=list)
    fan_in: int = 0
    fan_out: int = 0
    in_cycle: bool = False
    # Per-class LCOM5 cohesion + counts. Each entry: {name, start_line, method_count,
    # field_count, lcom5}. Computed once per file in analyze_file; classes with <2
    # inline methods or no `m_` fields are skipped (nothing to measure).
    classes: List[Dict] = field(default_factory=list)
    
    @property
    def maintainability_index(self) -> float:
        """
        Maintainability Index (MI)
        MI = 171 - 5.2 * ln(V) - 0.23 * CC - 16.2 * ln(LOC)
        Scaled to 0-100 range
        """
        v = max(self.halstead.volume, 1)
        cc = max(self.cyclomatic_complexity, 1)
        loc = max(self.code_lines, 1)
        
        mi = 171 - 5.2 * math.log(v) - 0.23 * cc - 16.2 * math.log(loc)
        # Scale to 0-100
        mi = max(0, min(100, mi * 100 / 171))
        return mi
    
    @property
    def comment_ratio(self) -> float:
        """Ratio of comment lines to code lines"""
        if self.code_lines == 0:
            return 0
        return self.comment_lines / self.code_lines


@dataclass
class DirectoryMetrics:
    """Aggregated metrics for a directory"""
    path: str
    file_count: int = 0
    total_lines: int = 0
    code_lines: int = 0
    comment_lines: int = 0
    blank_lines: int = 0
    avg_cyclomatic: float = 0
    avg_cognitive: float = 0
    avg_maintainability: float = 0
    # Priority-score distribution across the directory's files. Unweighted mean is
    # misleading when one godfile sits among many trivial files — use percentiles
    # plus the worst file reference so "directory looks fine on average" can't hide
    # a single catastrophic offender.
    max_priority: float = 0.0
    p90_priority: float = 0.0
    p50_priority: float = 0.0
    worst_file: str = ''
    total_functions: int = 0
    total_classes: int = 0
    files: List[FileMetrics] = field(default_factory=list)


@dataclass
class FunctionMetrics:
    """Metrics for a single function"""
    name: str = ''
    filepath: str = ''
    start_line: int = 0
    end_line: int = 0
    cyclomatic_complexity: int = 1
    cognitive_complexity: int = 0
    max_nesting_depth: int = 0
    code_lines: int = 0
    # Readability signals computed cheaply from cleaned function source.
    param_count: int = 0
    return_count: int = 0
    case_count: int = 0
    # Populated during post-analysis; higher score = stronger refactoring candidate.
    priority_score: float = 0.0
    # Per-dimension contribution (% of priority_score each dimension contributed).
    # Keys: cognitive, cyclomatic, nesting, length, params. Sums to ~100 when score>0.
    priority_factors: Dict[str, float] = field(default_factory=dict)
    tags: List[str] = field(default_factory=list)
    extract_candidates: List[Dict] = field(default_factory=list)
    # Shingles for duplicate detection. Frozenset of 5-gram token tuples from
    # the normalized body. Empty for functions below the min-token threshold.
    shingles: frozenset = field(default_factory=frozenset)


_PRIORITY_WEIGHTS: Dict[str, float] = {
    'cognitive': 0.30,
    'cyclomatic': 0.25,
    'nesting': 0.20,
    'length': 0.15,
    'params': 0.10,
}


def _percentile(sorted_values: List[float], p: float) -> float:
    """Linear-interpolation percentile. Input must be pre-sorted ascending."""
    if not sorted_values:
        return 0.0
    if len(sorted_values) == 1:
        return float(sorted_values[0])
    k = (len(sorted_values) - 1) * (p / 100.0)
    lo = int(k)
    hi = min(lo + 1, len(sorted_values) - 1)
    return float(sorted_values[lo] + (sorted_values[hi] - sorted_values[lo]) * (k - lo))


# --- Duplicate detection --------------------------------------------------

# Tokens worth matching literally (i.e. not collapsed to `_ID`). Anything that
# appears in here keeps its identity so control-flow shape survives normalization.
_DUP_KEEP_TOKENS: Set[str] = {
    'if', 'else', 'for', 'while', 'do', 'switch', 'case', 'default', 'break',
    'continue', 'return', 'goto', 'try', 'catch', 'throw',
    'class', 'struct', 'enum', 'namespace', 'template', 'typename',
    'public', 'private', 'protected', 'virtual', 'override', 'static',
    'const', 'constexpr', 'inline', 'noexcept', 'new', 'delete', 'this',
    'true', 'false', 'nullptr', 'auto', 'void',
}
_DUP_TOKEN_PATTERN = re.compile(
    r'(\b[a-zA-Z_][a-zA-Z0-9_]*\b|'       # identifier / keyword
    r'\b\d+\.?\d*[fFuUlL]*\b|'            # number
    r'==|!=|<=|>=|<<|>>|&&|\|\||\+\+|--|->\*?|::|\.\*|'  # multi-char ops
    r'[+\-*/%=<>!&|^~?:;,.(){}\[\]])'     # single-char punct/ops
)

_DUP_MIN_TOKENS = 30                   # skip tiny functions — too noisy
_DUP_SHINGLE_SIZE = 5
_DUP_JACCARD_THRESHOLD = 0.85


def _tokenize_for_dedup(cleaned_code: str) -> List[str]:
    """
    Tokenize cleaned (comment-free, string-free) source for duplicate detection.

    Identifiers outside `_DUP_KEEP_TOKENS` collapse to `_ID`, numbers collapse to
    `_N`. This means two functions that differ only in variable names still match,
    while keywords and operators preserve control-flow shape.
    """
    tokens: List[str] = []
    for m in _DUP_TOKEN_PATTERN.finditer(cleaned_code):
        tok = m.group(0)
        first = tok[0]
        if first.isalpha() or first == '_':
            tokens.append(tok if tok in _DUP_KEEP_TOKENS else '_ID')
        elif first.isdigit():
            tokens.append('_N')
        else:
            tokens.append(tok)
    return tokens


def _shingle_set(tokens: List[str], n: int = _DUP_SHINGLE_SIZE) -> frozenset:
    """Return the set of n-gram tuples drawn from `tokens`. Empty if too short.

    Callers that guard with `_DUP_MIN_TOKENS` (>= 30) will never reach the
    short-circuit here for n=5, so this is a belt-and-suspenders guard for
    direct callers (e.g. tests) that may pass smaller token lists.
    """
    if len(tokens) < n:
        return frozenset()
    return frozenset(tuple(tokens[i:i + n]) for i in range(len(tokens) - n + 1))


def _jaccard(a: frozenset, b: frozenset) -> float:
    if not a or not b:
        return 0.0
    inter = len(a & b)
    if inter == 0:
        return 0.0
    return inter / (len(a) + len(b) - inter)


def _cluster_duplicates(
    entries: List[Tuple[int, frozenset]],
    threshold: float = _DUP_JACCARD_THRESHOLD,
) -> List[List[int]]:
    """
    Union-find clustering: any two entries with Jaccard ≥ threshold land in the
    same cluster. Input is (function_index, shingle_set). Returns list of
    clusters of size ≥ 2, each as a list of indices sorted ascending.

    Naive O(n²) pairwise — fine for the typical engine's 1K–5K functions.
    """
    parent: List[int] = list(range(len(entries)))

    def find(i: int) -> int:
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    def union(a: int, b: int) -> None:
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    for i in range(len(entries)):
        _, si = entries[i]
        if not si:
            continue
        for j in range(i + 1, len(entries)):
            _, sj = entries[j]
            if not sj:
                continue
            if _jaccard(si, sj) >= threshold:
                union(i, j)

    groups: Dict[int, List[int]] = defaultdict(list)
    for i in range(len(entries)):
        groups[find(i)].append(entries[i][0])
    return [sorted(g) for g in groups.values() if len(g) >= 2]


def compute_function_priority_score(
    cognitive_complexity: int,
    cyclomatic_complexity: int,
    max_nesting_depth: int,
    code_lines: int,
    param_count: int = 0,
    weights: Optional[Dict[str, float]] = None,
) -> Tuple[float, Dict[str, float]]:
    """
    Composite 0-100 priority score + per-dimension contribution breakdown.

    Blends cognitive complexity (the signal most correlated with human difficulty),
    cyclomatic complexity (branch count), nesting depth (visual indentation pain),
    function length, and parameter count. Each dimension is soft-capped so beyond a
    threshold more doesn't matter more, and the cap is normalized to 0-1 before
    weighting — so the score is interpretable: 0 = clean, ~50 = starting to hurt,
    80+ = priority refactoring target.

    Returns (score, factors) where `factors` maps dimension name -> % of the final
    score it contributed (sums to ~100% when the score is nonzero). Lets a consumer
    see at a glance whether to attack depth first or extraction first.
    """
    w = weights if weights is not None else _PRIORITY_WEIGHTS
    normalised: Dict[str, float] = {
        'cognitive': min(cognitive_complexity / 40.0, 1.0),
        'cyclomatic': min(cyclomatic_complexity / 30.0, 1.0),
        'nesting': min(max_nesting_depth / 8.0, 1.0),
        'length': min(code_lines / 200.0, 1.0),
        'params': min(param_count / 8.0, 1.0),
    }
    weighted: Dict[str, float] = {k: w[k] * normalised[k] for k in normalised}
    total = sum(weighted.values())
    score = round(total * 100.0, 1)

    # Percentage contribution per dimension. When total is ~0, everything is 0%.
    if total > 0:
        factors = {k: round(100.0 * weighted[k] / total, 1) for k in weighted}
    else:
        factors = {k: 0.0 for k in weighted}
    return score, factors


def _analyze_file_worker(args: Tuple[str, str, Dict[str, float], List[str], bool, str]):
    """
    Process-pool worker. Must be a top-level function so it's picklable. Reconstructs
    a minimal CppAnalyzer in the child process, analyzes one file, and returns
    (FileMetrics, List[FunctionMetrics]) for the main process to merge.
    """
    filepath_str, root_path, thresholds, ignore_macros, exclude_generated, parser_backend = args
    analyzer = CppAnalyzer(
        root_path,
        thresholds=thresholds,
        ignore_macros=ignore_macros,
        exclude_generated=exclude_generated,
        parser_backend=parser_backend,
    )
    # analyze_file populates analyzer.function_metrics as a side effect; capture the
    # slice that belongs to this file.
    before = len(analyzer.function_metrics)
    file_m = analyzer.analyze_file(Path(filepath_str))
    func_ms = analyzer.function_metrics[before:]
    return file_m, func_ms


class CppAnalyzer:
    """Analyzer for C/C++ source code"""
    
    # C/C++ operators for Halstead metrics
    OPERATORS = {
        # Arithmetic
        '+', '-', '*', '/', '%', '++', '--',
        # Relational
        '==', '!=', '<', '>', '<=', '>=', '<=>',
        # Logical
        '&&', '||', '!',
        # Bitwise
        '&', '|', '^', '~', '<<', '>>',
        # Assignment
        '=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=',
        # Member access
        '.', '->', '.*', '->*', '::',
        # Other
        '?', ':', ',', ';', '(', ')', '[', ']', '{', '}',
        # Keywords as operators
        'sizeof', 'alignof', 'typeid', 'new', 'delete', 'throw', 'co_await', 'co_yield',
    }
    
    # Control-flow keywords that contribute to cognitive complexity + nestable blocks.
    # See module-level _COGNITIVE_NESTING_KEYWORDS for the canonical list.
    COGNITIVE_NESTING: Set[str] = _COGNITIVE_NESTING_SET
    
    def __init__(
        self,
        root_path: str,
        exclude_dirs: List[str] = None,
        thresholds: Optional[Dict[str, float]] = None,
        ignore_macros: Optional[List[str]] = None,
        exclude_generated: bool = False,
        parser_backend: str = 'regex',
    ):
        self.root_path = Path(root_path)
        self.exclude_dirs = set(exclude_dirs or [])
        self.thresholds: Dict[str, float] = dict(DEFAULT_THRESHOLDS)
        if thresholds:
            self.thresholds.update(thresholds)
        self.ignore_macros: List[str] = list(ignore_macros or [])
        self._ignore_macro_pattern: Optional[re.Pattern] = (
            re.compile(r'\b(?:' + '|'.join(re.escape(m) for m in self.ignore_macros) + r')\b')
            if self.ignore_macros else None
        )
        self.exclude_generated = exclude_generated
        # Parser backend: 'regex' (default, stdlib-only) or 'tree-sitter' (opt-in).
        # Tree-sitter handles nested templates, C++20 `requires`, function-try-blocks,
        # and macro-generated signatures that the regex misses, at the cost of
        # requiring `tree-sitter` + `tree-sitter-cpp` to be installed.
        if parser_backend == 'tree-sitter' and not HAS_TREE_SITTER:
            print("Warning: --parser tree-sitter requested but tree_sitter / "
                  "tree_sitter_cpp not installed. Falling back to regex parser. "
                  "Install with: pip install tree-sitter tree-sitter-cpp")
            parser_backend = 'regex'
        self.parser_backend = parser_backend
        self.file_metrics: List[FileMetrics] = []
        self.function_metrics: List[FunctionMetrics] = []
        self.directory_metrics: Dict[str, DirectoryMetrics] = {}
        # Populated by _find_duplicates(); list of clusters, each cluster a list of
        # (filepath, start_line, name, priority_score) for every function in the cluster.
        self.duplicate_clusters: List[List[Dict]] = []
        # Populated by _compute_include_graph(); list of include cycles as filepaths.
        self.include_cycles: List[List[str]] = []
        # Populated by generate_llm_suggestions() (if --llm-suggestions is used).
        self.llm_suggestions: List[Dict] = []
        
    def should_exclude(self, path: Path) -> bool:
        """Check if path should be excluded"""
        for part in path.parts:
            if part in self.exclude_dirs:
                return True
        return False
    
    _SOURCE_EXTENSIONS: Set[str] = frozenset({
        '.cpp', '.c', '.h', '.hpp', '.cc', '.cxx', '.hxx', '.inl'
    })
    _GENERATED_NAME_HINTS: Tuple[str, ...] = ('.pb.', '.generated.', '_generated.')
    _GENERATED_HEADER_HINTS: Tuple[str, ...] = (
        'automatically generated', 'do not edit', 'auto-generated', 'autogenerated',
    )

    def find_source_files(self) -> List[Path]:
        """
        Find all C/C++ source files. One rglob over '*' is faster than one rglob per
        extension on deep trees, because each rglob independently walks the directory
        tree.
        """
        files: List[Path] = []
        for file in self.root_path.rglob('*'):
            if file.suffix.lower() not in self._SOURCE_EXTENSIONS:
                continue
            rel = file.relative_to(self.root_path)
            if self.should_exclude(rel):
                continue
            if self.exclude_generated and self._looks_generated(file):
                continue
            files.append(file)
        return sorted(files)

    @classmethod
    def _looks_generated(cls, path: Path) -> bool:
        """Heuristic: filename markers (.pb.cc, .generated.cpp) or `DO NOT EDIT` banners."""
        name_lower = path.name.lower()
        for hint in cls._GENERATED_NAME_HINTS:
            if hint in name_lower:
                return True
        try:
            with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                head = ''.join(f.readline() for _ in range(5)).lower()
        except OSError:
            return False
        return any(hint in head for hint in cls._GENERATED_HEADER_HINTS)

    def _strip_ignored_macros(self, code: str) -> str:
        """
        Replace balanced `MACRO(...)` invocations with empty parens for every macro
        listed in `ignore_macros`. Used to suppress assert-like macros from inflating
        CC / cognitive complexity. No-op when no macros are configured.
        """
        if not self._ignore_macro_pattern:
            return code
        out = []
        i = 0
        for match in self._ignore_macro_pattern.finditer(code):
            out.append(code[i:match.start()])
            # Strip the macro name and its balanced argument list.
            j = match.end()
            # Skip whitespace before '('.
            while j < len(code) and code[j] in ' \t':
                j += 1
            if j >= len(code) or code[j] != '(':
                # Macro without args; just drop the name.
                out.append(match.group(0))
                i = match.end()
                continue
            # Walk balanced parens.
            depth = 1
            k = j + 1
            while k < len(code) and depth > 0:
                c = code[k]
                if c == '(':
                    depth += 1
                elif c == ')':
                    depth -= 1
                k += 1
            # Replace with spaces (preserving newlines) to keep line numbers intact.
            out.append(' ' * len(match.group(0)))
            for c in code[match.end():k]:
                out.append('\n' if c == '\n' else ' ')
            i = k
        out.append(code[i:])
        return ''.join(out)
    
    def remove_comments_and_strings(self, code: str) -> Tuple[str, Set[int]]:
        """
        Remove comments and string literals, returning (cleaned_code, comment_line_numbers).

        Strings (including raw strings R"(...)", u8R"...", LR"...", etc.) are replaced
        with spaces so line numbers are preserved. Comments are stripped entirely from the
        line (but the line itself is preserved). The returned set contains 1-indexed line
        numbers of any line that had comment content on it.

        Code-line detection is left to the caller: any line in the cleaned output with
        non-whitespace content is a code line (allows a line to count as both code and
        comment when code is followed by `// trailing comment`).
        """
        result = []
        comment_lines: Set[int] = set()
        i = 0
        line_num = 1
        in_string = False
        string_char = None

        while i < len(code):
            # Track line numbers
            if code[i] == '\n':
                line_num += 1
                result.append(code[i])
                i += 1
                continue

            # Raw string literals must be checked before normal string handling, because
            # they may contain unescaped quote characters, `//`, and `/*` within their body.
            if not in_string:
                raw_end = self._try_match_raw_string(code, i)
                if raw_end is not None:
                    # Replace the entire raw string with spaces, preserving newlines so
                    # line numbers stay accurate.
                    for j in range(i, raw_end):
                        if code[j] == '\n':
                            line_num += 1
                            result.append('\n')
                        else:
                            result.append(' ')
                    i = raw_end
                    continue

            # Handle normal string literals
            if not in_string and code[i] in '"\'':
                in_string = True
                string_char = code[i]
                result.append(' ')
                i += 1
                continue

            if in_string:
                if code[i] == '\\' and i + 1 < len(code):
                    # Preserve newline if we skip over one inside a string continuation
                    if code[i + 1] == '\n':
                        result.append('\n')
                        line_num += 1
                    i += 2
                elif code[i] == string_char:
                    in_string = False
                    i += 1
                else:
                    if code[i] == '\n':
                        result.append('\n')
                        line_num += 1
                    i += 1
                continue

            # Handle single-line comments
            if code[i:i+2] == '//':
                comment_lines.add(line_num)
                while i < len(code) and code[i] != '\n':
                    i += 1
                continue

            # Handle multi-line comments
            if code[i:i+2] == '/*':
                comment_lines.add(line_num)
                i += 2
                while i < len(code) and code[i:i+2] != '*/':
                    if code[i] == '\n':
                        line_num += 1
                        comment_lines.add(line_num)
                        result.append('\n')  # preserve newline for line counts
                    i += 1
                if i < len(code):
                    i += 2  # Skip */
                continue

            result.append(code[i])
            i += 1

        return ''.join(result), comment_lines

    @staticmethod
    def _try_match_raw_string(code: str, i: int) -> Optional[int]:
        """
        If code[i:] begins a C++ raw string literal (R"delim(...)delim", u8R"...", LR"...",
        uR"...", UR"..."), return the index just past the closing delimiter. Otherwise None.
        """
        start = i
        # Optional encoding prefix
        if code[i:i+3] == 'u8R' and i + 3 < len(code) and code[i+3] == '"':
            i += 3
        elif i + 2 < len(code) and code[i] in 'uUL' and code[i+1] == 'R' and code[i+2] == '"':
            i += 2
        elif i + 1 < len(code) and code[i] == 'R' and code[i+1] == '"':
            i += 1
        else:
            return None

        # Require the '"' we just validated
        if i >= len(code) or code[i] != '"':
            return None
        i += 1

        # Delimiter: any chars except ' ', '(', ')', '\\', '"', '\n' (per C++ spec, simplified)
        delim_start = i
        while i < len(code) and code[i] != '(':
            if code[i] in ' \t\n()\\"':
                return None
            i += 1
        if i >= len(code):
            return None
        delim = code[delim_start:i]
        end_marker = ')' + delim + '"'
        i += 1  # skip '('

        end = code.find(end_marker, i)
        if end == -1:
            # Malformed raw string: skip to end of file rather than falling through to
            # the string handler (which would then misinterpret `"` characters inside).
            return len(code)
        return end + len(end_marker)
    
    def count_lines(self, raw_code: str, cleaned_code: str, comment_lines: Set[int]) -> Tuple[int, int, int, int]:
        """
        Count total, code, comment, and blank lines.

        A line with both code and a trailing comment (`int x = 5; // note`) counts in both
        `code` and `comment`; the four returned values therefore are NOT required to sum to
        `total` (matches `cloc`'s convention). `code` is derived from the cleaned code so
        that comment-only lines don't contribute — any non-whitespace after cleaning is real
        source content.
        """
        raw_lines = raw_code.split('\n')
        cleaned_lines = cleaned_code.split('\n')
        total = len(raw_lines)
        blank = sum(1 for line in raw_lines if not line.strip())
        code_line_count = sum(1 for line in cleaned_lines if line.strip())
        return total, code_line_count, len(comment_lines), blank
    
    def calculate_cyclomatic_complexity(self, cleaned_code: str) -> int:
        """Calculate McCabe's Cyclomatic Complexity using precompiled decision-point patterns."""
        cc = 1  # Base complexity
        for pattern in _CC_PATTERNS:
            cc += len(pattern.findall(cleaned_code))
        return cc
    
    def calculate_cognitive_complexity(self, cleaned_code: str) -> int:
        """Cognitive Complexity (SonarSource). See `_analyze_control_flow` for the algorithm."""
        return self._analyze_control_flow(cleaned_code)[0]

    def _analyze_control_flow(self, cleaned_code: str) -> Tuple[int, int]:
        """
        Walk `cleaned_code` once, returning (cognitive_complexity, max_nesting_depth).

        Cognitive complexity follows SonarSource's rule: each control-flow keyword adds
        `1 + current_nesting`, logical operators (&& / ||) each add 1, `goto` adds 1.

        Nesting only increments for control-flow `{` — class/struct/namespace/function
        bodies and lambda/initializer braces don't count. A `{` is control flow when:
          - multi-line: a keyword on an earlier line is still pending (pending_control_flow), or
          - same-line: a control-flow keyword appears earlier on the same line before the `{`

        An earlier version of this routine treated *any* `{` preceded by `)` as control
        flow, which caused function bodies (`void f(int x) {`), lambdas (`[&]() { ... }`),
        and initializer lists to inflate nesting counts. That heuristic was removed.
        """
        cognitive = 0
        nesting = 0
        max_nesting = 0
        pending_control_flow = False

        for line in cleaned_code.split('\n'):
            stripped = line.strip()
            if not stripped:
                continue

            # Count every control-flow keyword on the line — not just the first pattern
            # that matches. Cost per keyword is (1 + line-start nesting); single-line
            # nested keywords are thus undercounted slightly, but that's rare in C++
            # and the error is bounded.
            total_keywords = 0
            for pattern in _COGNITIVE_KEYWORD_PATTERNS:
                total_keywords += len(pattern.findall(stripped))
            has_control_flow_on_line = total_keywords > 0
            if has_control_flow_on_line:
                cognitive += total_keywords * (1 + nesting)
                pending_control_flow = True

            cognitive += len(_LOGICAL_OP_PATTERN.findall(stripped))
            if _GOTO_PATTERN.search(stripped):
                cognitive += 1

            for i, ch in enumerate(stripped):
                if ch == '{':
                    is_control_flow = pending_control_flow
                    if not is_control_flow and has_control_flow_on_line:
                        before = stripped[:i].rstrip()
                        remaining = before.strip()
                        if remaining and remaining[-1] not in '({':
                            for pattern in _COGNITIVE_KEYWORD_PATTERNS:
                                if pattern.search(before):
                                    is_control_flow = True
                                    break

                    if is_control_flow:
                        nesting += 1
                        if nesting > max_nesting:
                            max_nesting = nesting
                        pending_control_flow = False
                elif ch == '}':
                    nesting = max(0, nesting - 1)

        return cognitive, max_nesting
    
    def calculate_halstead_metrics(self, cleaned_code: str) -> HalsteadMetrics:
        """Calculate Halstead complexity metrics"""
        operators: Dict[str, int] = defaultdict(int)
        operands: Dict[str, int] = defaultdict(int)
        
        # Find all identifiers and numbers (operands)
        identifiers = re.findall(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b', cleaned_code)
        numbers = re.findall(r'\b\d+\.?\d*[fFuUlL]*\b', cleaned_code)
        
        # C++ keywords (not operands)
        keywords = {
            'auto', 'break', 'case', 'char', 'const', 'continue', 'default',
            'do', 'double', 'else', 'enum', 'extern', 'float', 'for', 'goto',
            'if', 'int', 'long', 'register', 'return', 'short', 'signed',
            'sizeof', 'static', 'struct', 'switch', 'typedef', 'union',
            'unsigned', 'void', 'volatile', 'while', 'class', 'public',
            'private', 'protected', 'virtual', 'override', 'final', 'const',
            'constexpr', 'nullptr', 'true', 'false', 'template', 'typename',
            'namespace', 'using', 'try', 'catch', 'throw', 'new', 'delete',
            'inline', 'static_cast', 'dynamic_cast', 'const_cast', 'reinterpret_cast',
            'explicit', 'friend', 'mutable', 'operator', 'this', 'bool',
            'noexcept', 'decltype', 'alignas', 'alignof', 'static_assert',
            'thread_local', 'concept', 'requires', 'co_await', 'co_return', 'co_yield'
        }
        
        # Count operators from keywords
        for kw in ['sizeof', 'new', 'delete', 'throw', 'return']:
            count = len(re.findall(rf'\b{kw}\b', cleaned_code))
            if count > 0:
                operators[kw] += count
        
        # Count identifiers as operands (excluding keywords)
        for ident in identifiers:
            if ident not in keywords:
                operands[ident] += 1
        
        # Count numbers as operands
        for num in numbers:
            operands[num] += 1
        
        # Count symbol operators
        sorted_ops = sorted(
            [op for op in self.OPERATORS if not op.isalpha()],
            key=len, reverse=True
        )
        
        temp_code = cleaned_code
        for op in sorted_ops:
            escaped_op = re.escape(op)
            count = len(re.findall(escaped_op, temp_code))
            if count > 0:
                operators[op] += count
                # Remove to avoid double counting
                temp_code = re.sub(escaped_op, ' ', temp_code)
        
        return HalsteadMetrics(
            n1=len(operators),
            n2=len(operands),
            N1=sum(operators.values()),
            N2=sum(operands.values())
        )
    
    # Names that the function regex can match as identifiers but which are actually
    # control-flow constructs (`if (x) { }`, `catch (e) { }`, etc.). Filter after match.
    _CONTROL_FLOW_NAMES: Set[str] = frozenset({
        'if', 'else', 'for', 'while', 'switch', 'catch',
        'return', 'sizeof', 'alignof', 'new', 'delete', 'do', 'try'
    })

    # Cached compiled regex for function detection. Supports:
    #   - leading whitespace / indentation on the line
    #   - optional template<...> header
    #   - modifiers: inline, constexpr, static, virtual, explicit, friend
    #   - optional return type with flat template args (std::pair<int,int>)
    #   - function names: normal identifiers, qualified (Foo::Bar), destructors (~Foo),
    #     operator overloads including qualified (Foo::operator==)
    #   - const / volatile / noexcept / override / final
    #   - trailing return type (-> T)
    #   - constructor member-initializer lists (: m_x(0), m_y(0))
    #
    # Approximations: nested templates (<std::pair<int,int>>), C++20 `requires` clauses,
    # function-try-blocks, macro-generated signatures, and `throw(...)` specifications
    # are not handled. Missing a function here just means it's absent from per-function
    # metrics; file-level metrics are unaffected.
    _FUNC_PATTERN: re.Pattern = re.compile(
        r'(?:^|\n)[ \t]*'
        r'(?:template\s*<[^>]*>\s*)?'
        r'(?:(?:inline|constexpr|static|virtual|explicit|friend)\s+)*'
        r'('                                                          # group 1: signature
        r'(?:[\w:]+(?:\s*<[^<>;{}\n]*>)?(?:\s*[*&])*\s+)?'           # optional return type
        r'(?P<name>[\w:]*?operator\s*(?:<<|>>|\(\)|\[\]|[+\-*/%^&|~!=<>,]+)|[~\w:]+)'  # name
        r'\s*\([^)]*\)'                                               # params (no nested parens)
        r'(?:\s*const)?(?:\s*volatile)?'
        r'(?:\s*noexcept(?:\s*\([^)]*\))?)?'
        r'(?:\s*override)?(?:\s*final)?'
        r'(?:\s*->\s*[\w:&*<>,\s]+)?'                                 # trailing return type
        r'(?:\s*:\s*[\w:]+\s*\([^)]*\)(?:\s*,\s*[\w:]+\s*\([^)]*\))*)?'  # init list
        r')'
        r'\s*\{',
        re.MULTILINE
    )

    def _find_function_boundaries(self, code: str) -> List[Dict]:
        """
        Find function boundaries and dispatch to the configured parser backend.

        Returns a list of dicts with keys: name, start_line, end_line, start_pos,
        brace_start, brace_end. Line numbers are 1-indexed.
        """
        if self.parser_backend == 'tree-sitter':
            return self._find_function_boundaries_ts(code)
        return self._find_function_boundaries_regex(code)

    def _find_function_boundaries_regex(self, code: str) -> List[Dict]:
        """
        Find function boundaries via regex + brace matching.

        Approximate — see `_FUNC_PATTERN` comment for known limitations.
        """
        functions = []
        for match in self._FUNC_PATTERN.finditer(code):
            func_name = match.group('name')
            # The name regex accepts control-flow keywords like `if`, which would match
            # things like `if (cond) { ... }`. Reject those here.
            if func_name in self._CONTROL_FLOW_NAMES:
                continue

            start_pos = match.start()
            start_line = code[:start_pos].count('\n') + 1

            brace_pos = match.end() - 1  # position of '{'

            # Find matching closing brace (strings/comments inside function bodies are
            # rare enough at this granularity that a naive scan is acceptable).
            brace_count = 1
            pos = brace_pos + 1
            while pos < len(code) and brace_count > 0:
                if code[pos] == '{':
                    brace_count += 1
                elif code[pos] == '}':
                    brace_count -= 1
                pos += 1

            if brace_count != 0:
                continue  # unbalanced; skip rather than emit garbage

            end_line = code[:pos - 1].count('\n') + 1
            functions.append({
                'name': func_name,
                'start_line': start_line,
                'end_line': end_line,
                'start_pos': start_pos,
                'brace_start': brace_pos,
                'brace_end': pos - 1,
            })

        return functions

    @staticmethod
    def _ts_extract_function_name(node, code_bytes: bytes) -> str:
        """
        Walk a tree-sitter function_definition node's declarator chain to find
        the function name. Handles pointer/reference declarators, qualified names
        (`Foo::Bar`), operator overloads, destructors, and constructor names.
        Returns '' if the structure is unexpected.
        """
        # Find the function_declarator child, possibly nested in pointer/reference.
        def _find_decl(n):
            for child in n.children:
                if child.type == 'function_declarator':
                    return child
                if child.type in ('pointer_declarator', 'reference_declarator'):
                    found = _find_decl(child)
                    if found is not None:
                        return found
            return None

        decl = _find_decl(node)
        if decl is None:
            return ''
        # The name is the first non-parameter-list child.
        for child in decl.children:
            if child.type == 'parameter_list':
                break
            if child.type in (
                'identifier', 'field_identifier', 'qualified_identifier',
                'destructor_name', 'operator_name', 'type_identifier',
                'template_function',
            ):
                return code_bytes[child.start_byte:child.end_byte].decode(
                    'utf-8', errors='ignore'
                ).strip()
        return ''

    def _find_function_boundaries_ts(self, code: str) -> List[Dict]:
        """
        Find function boundaries using tree-sitter-cpp. Returns the same dict
        shape as the regex variant so downstream code is parser-agnostic.

        Covers constructs the regex misses: nested template args, C++20
        `requires`, function-try-blocks, and most macro-generated signatures
        (provided the expanded code is still valid C++). Lambdas inside function
        bodies are deliberately not returned as top-level functions — they're
        counted as part of their enclosing function, matching the regex path.
        """
        parser = _get_ts_parser()
        if parser is None:
            return self._find_function_boundaries_regex(code)

        code_bytes = code.encode('utf-8', errors='replace')
        tree = parser.parse(code_bytes)

        # Tree-sitter returns byte offsets; downstream callers slice `code` as a
        # str. On ASCII-only source (the common case) the offsets match, but to
        # be correct we translate. Build the mapping lazily.
        is_ascii = len(code_bytes) == len(code)

        def b2c(byte_offset: int) -> int:
            if is_ascii:
                return byte_offset
            # Decoding a prefix is O(n) but only called a few times per function.
            return len(code_bytes[:byte_offset].decode('utf-8', errors='ignore'))

        functions: List[Dict] = []
        seen_ranges: Set[int] = set()  # dedupe by start_byte

        # Recursive walk rather than a traversal cursor to stay simple. For a
        # 2K-line file this is instant; for 50K-line files still well under 100ms.
        def walk(node, inside_function: bool) -> None:
            if node.type == 'function_definition':
                if not inside_function:
                    # Find the compound_statement (body); skip functions without a body
                    # (e.g. `= default` / pure virtual / `= 0`).
                    body = None
                    for child in node.children:
                        if child.type == 'compound_statement':
                            body = child
                            break
                    if body is not None:
                        name = self._ts_extract_function_name(node, code_bytes)
                        if name and name not in self._CONTROL_FLOW_NAMES:
                            start_byte = node.start_byte
                            if start_byte not in seen_ranges:
                                seen_ranges.add(start_byte)
                                functions.append({
                                    'name': name,
                                    'start_line': node.start_point[0] + 1,
                                    'end_line': body.end_point[0] + 1,
                                    'start_pos': b2c(start_byte),
                                    'brace_start': b2c(body.start_byte),
                                    'brace_end': b2c(body.end_byte) - 1,
                                })
                # Nested lambdas/local functions are absorbed by the enclosing
                # function body (matches regex behaviour).
                for child in node.children:
                    walk(child, True)
                return
            for child in node.children:
                walk(child, inside_function)

        walk(tree.root_node, False)
        # Sort by start_byte for stable output.
        functions.sort(key=lambda f: f['start_pos'])
        return functions
    
    def _calculate_function_complexities(self, code: str, func_boundaries: List[Dict], cleaned_code: str) -> List[FunctionMetrics]:
        """Calculate per-function complexity metrics using `_analyze_control_flow` once per function."""
        if not func_boundaries:
            return []

        results = []
        source_lines = code.split('\n')

        for func in func_boundaries:
            start_l = func['start_line'] - 1  # 0-indexed
            end_l = func['end_line']
            func_code = '\n'.join(source_lines[start_l:end_l])

            func_cleaned, _ = self.remove_comments_and_strings(func_code)
            func_cc = self.calculate_cyclomatic_complexity(func_cleaned)
            func_cognitive, func_max_nesting = self._analyze_control_flow(func_cleaned)

            param_count = self._count_parameters(code, func['brace_start'])
            return_count = len(_RETURN_PATTERN.findall(func_cleaned))
            case_count = len(_CASE_PATTERN.findall(func_cleaned))

            fm = FunctionMetrics(
                name=func['name'],
                filepath='',
                start_line=func['start_line'],
                end_line=end_l,
                cyclomatic_complexity=func_cc,
                cognitive_complexity=func_cognitive,
                max_nesting_depth=func_max_nesting,
                code_lines=end_l - start_l,
                param_count=param_count,
                return_count=return_count,
                case_count=case_count,
            )
            # Extract-method candidates (line ranges where nesting stays deep) are cheap
            # to compute and tell Claude where to look first, so bundle them here.
            fm.extract_candidates = self._find_extract_candidates(
                func_cleaned,
                base_line=func['start_line'],
                min_depth=int(self.thresholds['extract_candidate_depth']),
                min_lines=int(self.thresholds['extract_candidate_min_lines']),
            )
            # Shingles for duplicate detection. Skip tiny functions to cut noise.
            tokens = _tokenize_for_dedup(func_cleaned)
            if len(tokens) >= _DUP_MIN_TOKENS:
                fm.shingles = _shingle_set(tokens)
            results.append(fm)

        return results

    @staticmethod
    def _count_parameters(code: str, brace_start: int) -> int:
        """
        Count parameters in the function signature preceding `brace_start`.

        Walks backwards from the `{` to find the signature's `(...)` pair, then counts
        top-level commas. Returns 0 for empty, `(void)`, or signatures that can't be
        parsed (malformed or missing-paren cases).
        """
        # Scan backwards for the closing ')' of the param list.
        close_pos = code.rfind(')', 0, brace_start)
        if close_pos == -1:
            return 0
        # Find matching '(' — walk backwards counting parens.
        depth = 1
        open_pos = close_pos - 1
        while open_pos >= 0 and depth > 0:
            c = code[open_pos]
            if c == ')':
                depth += 1
            elif c == '(':
                depth -= 1
            open_pos -= 1
        if depth != 0:
            return 0
        open_pos += 1  # index of the matching '('
        params_src = code[open_pos + 1:close_pos].strip()
        if not params_src or params_src == 'void':
            return 0

        # Count top-level commas (ignore commas inside nested <>, (), {}, []).
        count = 1
        depth_angle = depth_paren = depth_brace = depth_brack = 0
        for ch in params_src:
            if ch == '<':
                depth_angle += 1
            elif ch == '>' and depth_angle > 0:
                depth_angle -= 1
            elif ch == '(':
                depth_paren += 1
            elif ch == ')' and depth_paren > 0:
                depth_paren -= 1
            elif ch == '{':
                depth_brace += 1
            elif ch == '}' and depth_brace > 0:
                depth_brace -= 1
            elif ch == '[':
                depth_brack += 1
            elif ch == ']' and depth_brack > 0:
                depth_brack -= 1
            elif ch == ',' and depth_angle == depth_paren == depth_brace == depth_brack == 0:
                count += 1
        return count

    @staticmethod
    def _find_extract_candidates(
        cleaned_func_code: str,
        base_line: int,
        min_depth: int,
        min_lines: int,
    ) -> List[Dict]:
        """
        Scan the function body for contiguous runs where nesting depth stays at or above
        `min_depth` for at least `min_lines` lines. Each such run is a candidate block to
        extract into a helper. `base_line` is the 1-indexed line number of the function
        start in the source file, used to translate function-local line indices back to
        source-file line numbers.

        Uses the same brace-context detection as `_analyze_control_flow` but tracks depth
        per-line instead of cumulatively computing cognitive complexity.
        """
        candidates: List[Dict] = []
        lines = cleaned_func_code.split('\n')
        depth = 0
        pending_control_flow = False
        run_start: Optional[int] = None  # 0-indexed inclusive line where the run began
        run_peak = 0

        for idx, line in enumerate(lines):
            stripped = line.strip()

            line_has_keyword = False
            if stripped:
                for pattern in _COGNITIVE_KEYWORD_PATTERNS:
                    if pattern.search(stripped):
                        line_has_keyword = True
                        pending_control_flow = True
                        break

            # Track depth over this line (at end-of-line). Matches the logic in
            # `_analyze_control_flow` so extract candidates and nesting agree.
            for i, ch in enumerate(stripped):
                if ch == '{':
                    is_control_flow = pending_control_flow
                    if not is_control_flow and line_has_keyword:
                        before = stripped[:i].rstrip()
                        remaining = before.strip()
                        if remaining and remaining[-1] not in '({':
                            for pat in _COGNITIVE_KEYWORD_PATTERNS:
                                if pat.search(before):
                                    is_control_flow = True
                                    break
                    if is_control_flow:
                        depth += 1
                        pending_control_flow = False
                elif ch == '}':
                    depth = max(0, depth - 1)

            if depth >= min_depth:
                if run_start is None:
                    run_start = idx
                    run_peak = depth
                elif depth > run_peak:
                    run_peak = depth
            else:
                if run_start is not None:
                    run_len = idx - run_start
                    if run_len >= min_lines:
                        candidates.append({
                            'start_line': base_line + run_start,
                            'end_line': base_line + idx - 1,
                            'peak_depth': run_peak,
                            'line_count': run_len,
                        })
                    run_start = None

        # Handle a run that extends to the end of the function.
        if run_start is not None:
            run_len = len(lines) - run_start
            if run_len >= min_lines:
                candidates.append({
                    'start_line': base_line + run_start,
                    'end_line': base_line + len(lines) - 1,
                    'peak_depth': run_peak,
                    'line_count': run_len,
                })

        return candidates

    @staticmethod
    def _find_tech_debt_markers(raw_code: str) -> List[Dict]:
        """
        Find TODO/FIXME/HACK/XXX markers in raw source (not cleaned — markers are in
        comments, which cleaning strips). Returns list of {line, marker, text}.
        """
        markers = []
        for match in _TECH_DEBT_PATTERN.finditer(raw_code):
            line_num = raw_code[:match.start()].count('\n') + 1
            markers.append({
                'line': line_num,
                'marker': match.group(1),
                'text': match.group(2).strip(),
            })
        return markers

    def _tag_function(self, fm: FunctionMetrics) -> List[str]:
        """Apply the reliable anti-pattern tag rules. Order-stable for stable output."""
        t = self.thresholds
        tags: List[str] = []
        if fm.code_lines > t['very_long_function_loc']:
            tags.append('very-long-function')
        elif fm.code_lines > t['long_function_loc']:
            tags.append('long-function')
        if fm.max_nesting_depth >= t['deep_nesting']:
            tags.append('deep-nesting')
        if fm.cyclomatic_complexity >= t['complex_branching_cc']:
            tags.append('complex-branching')
        if fm.cognitive_complexity >= t['hard_to_follow_cognitive']:
            tags.append('hard-to-follow')
        if fm.param_count >= t['many_params']:
            tags.append('many-params')
        if fm.return_count >= t['many_returns']:
            tags.append('many-returns')
        if fm.case_count >= t['big_switch_cases'] and fm.max_nesting_depth <= t['big_switch_max_nesting']:
            tags.append('big-switch')
        if fm.max_nesting_depth >= t['nesting_hell_depth'] and fm.case_count < t['nesting_hell_max_cases']:
            tags.append('nesting-hell')
        return tags

    def _tag_file(self, fm: FileMetrics) -> List[str]:
        """Apply file-level anti-pattern tags. Functions get tagged separately."""
        t = self.thresholds
        tags: List[str] = []
        if fm.maintainability_index < t['low_mi']:
            tags.append('low-mi')
        if fm.code_lines > t['god_file_loc'] or fm.function_count > t['god_file_funcs']:
            tags.append('god-file')
        return tags
    
    def _count_classes(self, cleaned_code: str) -> int:
        """Count class/struct declarations in the code"""
        class_pattern = r'\b(?:class|struct)\s+\w+'
        return len(re.findall(class_pattern, cleaned_code))

    # class/struct definitions that open a body: `class Foo : public Bar {` etc.
    # Forward declarations (`class Foo;`) and template specializations are skipped
    # by requiring the `{` opener. One-level base-class list is tolerated.
    _CLASS_BODY_PATTERN: re.Pattern = re.compile(
        r'\b(?:class|struct)\s+([A-Za-z_]\w*)'
        r'(?:\s*:\s*(?:public|private|protected|virtual|[\w:<>,\s])+)?'
        r'\s*\{',
        re.MULTILINE,
    )
    _MEMBER_FIELD_PATTERN: re.Pattern = re.compile(r'\bm_\w+\b')

    def _compute_class_lcom(self, cleaned_code: str) -> List[Dict]:
        """
        Compute per-class LCOM5 (Henderson-Sellers) cohesion using the engine's
        strict `m_` member-variable convention as a shortcut — any `m_foo` reference
        inside a method body counts as touching field `m_foo`.

        LCOM5 = 1 - (sum over fields of distinct methods touching field) / (methods * fields)

        Range 0..1. 0 = every method touches every field (tight cohesion, unlikely);
        1 = no method touches any field (incohesive or stateless). Returns a list of
        dicts {name, start_line, method_count, field_count, lcom5} per class, skipping
        classes with <2 inline methods or no `m_*` fields (nothing to measure).

        Scope approximations: nested classes are treated independently at their own
        outer match; template specializations and friend definitions may be missed.
        """
        results: List[Dict] = []
        for m in self._CLASS_BODY_PATTERN.finditer(cleaned_code):
            name = m.group(1)
            brace_pos = m.end() - 1
            depth = 1
            pos = brace_pos + 1
            while pos < len(cleaned_code) and depth > 0:
                if cleaned_code[pos] == '{':
                    depth += 1
                elif cleaned_code[pos] == '}':
                    depth -= 1
                pos += 1
            if depth != 0:
                continue  # unbalanced
            body = cleaned_code[brace_pos + 1:pos - 1]
            start_line = cleaned_code[:m.start()].count('\n') + 1

            methods = self._find_function_boundaries(body)
            if len(methods) < 2:
                continue
            all_fields: Set[str] = set(self._MEMBER_FIELD_PATTERN.findall(body))
            if not all_fields:
                continue

            # Distinct methods touching each field.
            field_to_methods: Dict[str, Set[int]] = defaultdict(set)
            for mi, method in enumerate(methods):
                method_body = body[method['brace_start']:method['brace_end'] + 1]
                for field_name in self._MEMBER_FIELD_PATTERN.findall(method_body):
                    if field_name in all_fields:
                        field_to_methods[field_name].add(mi)

            sum_accesses = sum(len(ms) for ms in field_to_methods.values())
            denom = len(methods) * len(all_fields)
            lcom5 = round(1.0 - (sum_accesses / denom), 3) if denom else 0.0
            results.append({
                'name': name,
                'start_line': start_line,
                'method_count': len(methods),
                'field_count': len(all_fields),
                'lcom5': lcom5,
            })
        return results
    
    def analyze_file(self, filepath: Path) -> FileMetrics:
        """Analyze a single source file"""
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                code = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            fm = FileMetrics(filepath=str(filepath.relative_to(self.root_path)))
            fm.max_nesting_depth = 0
            return fm

        cleaned_code, comment_line_set = self.remove_comments_and_strings(code)
        # Macro stripping runs on cleaned code (strings/comments already gone) to avoid
        # accidentally matching macro names inside strings.
        scan_code = self._strip_ignored_macros(cleaned_code)

        total, code_lines, comment_lines, blank = self.count_lines(code, cleaned_code, comment_line_set)
        cc = self.calculate_cyclomatic_complexity(scan_code)
        cognitive, max_nesting = self._analyze_control_flow(scan_code)
        halstead = self.calculate_halstead_metrics(scan_code)
        classes = self._count_classes(cleaned_code)

        # Extract per-function metrics (operates on raw code for signature detection and
        # on cleaned code for body metrics).
        func_boundaries = self._find_function_boundaries(code)
        func_metrics = self._calculate_function_complexities(code, func_boundaries, cleaned_code)

        # TODO/FIXME/HACK markers come from raw source — they're inside comments.
        tech_debt = self._find_tech_debt_markers(code)
        todo_count = sum(1 for m in tech_debt if m['marker'] == 'TODO')
        fixme_count = sum(1 for m in tech_debt if m['marker'] == 'FIXME')
        hack_count = sum(1 for m in tech_debt if m['marker'] in ('HACK', 'XXX'))

        # Local include targets — basenames only so downstream matching is robust to
        # path differences between includers (`#include "Flux/Foo.h"` vs just `"Foo.h"`).
        includes = [Path(m).name for m in _LOCAL_INCLUDE_PATTERN.findall(code)]

        # Per-class LCOM5 cohesion. Uses the cleaned code so comments/strings don't
        # inflate method bodies or field references.
        class_metrics = self._compute_class_lcom(cleaned_code)

        relative_path = str(filepath.relative_to(self.root_path))
        for fm in func_metrics:
            fm.filepath = relative_path
            self.function_metrics.append(fm)

        file_m = FileMetrics(
            filepath=relative_path,
            total_lines=total,
            code_lines=code_lines,
            comment_lines=comment_lines,
            blank_lines=blank,
            cyclomatic_complexity=cc,
            cognitive_complexity=cognitive,
            halstead=halstead,
            function_count=len(func_boundaries),
            class_count=classes,
            max_nesting_depth=max_nesting,
            todo_count=todo_count,
            fixme_count=fixme_count,
            hack_count=hack_count,
            tech_debt_markers=tech_debt,
            includes=includes,
            classes=class_metrics,
        )
        return file_m
    
    def analyze(self, workers: int = 0) -> None:
        """
        Analyze all source files.

        When `workers` is 0, parallelizes across cores for >=200 files and runs serially
        below that threshold (process-pool startup dominates on small trees). `workers=1`
        forces serial; `workers>1` forces that many workers regardless of file count.
        """
        files = self.find_source_files()
        print(f"Found {len(files)} source files to analyze...")

        auto_parallel = workers == 0 and len(files) >= 200
        use_parallel = workers > 1 or auto_parallel

        if use_parallel:
            import concurrent.futures
            worker_count = workers if workers > 1 else max(1, (os.cpu_count() or 2) - 1)
            print(f"  Parallelizing across {worker_count} workers...")
            # We pickle a lightweight (root_path, thresholds, ignore_macros, exclude_generated)
            # bundle and reconstruct the analyzer in each worker; function_metrics come back
            # alongside FileMetrics via a top-level helper.
            args = [(str(f), str(self.root_path), dict(self.thresholds),
                     list(self.ignore_macros), self.exclude_generated,
                     self.parser_backend) for f in files]
            with concurrent.futures.ProcessPoolExecutor(max_workers=worker_count) as executor:
                for i, result in enumerate(executor.map(_analyze_file_worker, args)):
                    if (i + 1) % 50 == 0:
                        print(f"  Analyzed {i + 1}/{len(files)} files...")
                    file_m, func_ms = result
                    self.file_metrics.append(file_m)
                    self.function_metrics.extend(func_ms)
        else:
            for i, filepath in enumerate(files):
                if (i + 1) % 50 == 0:
                    print(f"  Analyzed {i + 1}/{len(files)} files...")
                metrics = self.analyze_file(filepath)
                self.file_metrics.append(metrics)

        print(f"Analyzed {len(files)} files.")
        self._compute_priority_scores()
        self._compute_include_graph()
        self._find_duplicates()
        self._aggregate_by_directory()

    def _compute_priority_scores(self) -> None:
        """
        Assign `priority_score` and `tags` to every FunctionMetrics and FileMetrics.

        File-level score blends the file's own metrics with the worst contained function
        so "one awful function in a small file" doesn't hide behind averages. Uses 70/30
        weighting between file-intrinsic signals and the worst function's score.
        """
        pw = {
            'cognitive':  self.thresholds.get('priority_weight_cognitive',  _PRIORITY_WEIGHTS['cognitive']),
            'cyclomatic': self.thresholds.get('priority_weight_cyclomatic', _PRIORITY_WEIGHTS['cyclomatic']),
            'nesting':    self.thresholds.get('priority_weight_nesting',    _PRIORITY_WEIGHTS['nesting']),
            'length':     self.thresholds.get('priority_weight_length',     _PRIORITY_WEIGHTS['length']),
            'params':     self.thresholds.get('priority_weight_params',     _PRIORITY_WEIGHTS['params']),
        }
        functions_by_file: Dict[str, List[FunctionMetrics]] = defaultdict(list)
        for fm in self.function_metrics:
            fm.priority_score, fm.priority_factors = compute_function_priority_score(
                cognitive_complexity=fm.cognitive_complexity,
                cyclomatic_complexity=fm.cyclomatic_complexity,
                max_nesting_depth=fm.max_nesting_depth,
                code_lines=fm.code_lines,
                param_count=fm.param_count,
                weights=pw,
            )
            fm.tags = self._tag_function(fm)
            functions_by_file[fm.filepath].append(fm)

        for file_m in self.file_metrics:
            worst = 0.0
            for func in functions_by_file.get(file_m.filepath, ()):
                if func.priority_score > worst:
                    worst = func.priority_score
            file_m.worst_function_score = worst

            file_intrinsic, file_factors = compute_function_priority_score(
                cognitive_complexity=file_m.cognitive_complexity,
                cyclomatic_complexity=file_m.cyclomatic_complexity,
                max_nesting_depth=file_m.max_nesting_depth,
                code_lines=file_m.code_lines,
                weights=pw,
            )
            # Scale file-intrinsic down a touch (files naturally score higher than
            # functions because they accumulate), then blend in the worst function.
            file_m.priority_score = round(0.7 * file_intrinsic + 0.3 * worst, 1)
            # Factors reflect the intrinsic score's composition; the "worst-function"
            # bump is reported separately via worst_function_score.
            file_m.priority_factors = file_factors
            file_m.tags = self._tag_file(file_m)
    
    def _compute_include_graph(self) -> None:
        """
        Populate `fan_in`, `fan_out`, `in_cycle` on every FileMetrics and store
        detected cycles on `self.include_cycles`.

        Matching is by basename: `#include "Flux/Foo.h"` and `#include "Foo.h"` both
        point to `Foo.h`. If multiple files share a basename we link to all of them
        (rare in Zenith but possible). System `<...>` includes are skipped at parse
        time so they don't inflate fan-out.

        Cycle detection uses Tarjan's SCC; any SCC of size ≥ 2, plus self-loops,
        marks its members as `in_cycle`. Cycles matter because they're the strongest
        single signal of architectural entanglement.
        """
        by_basename: Dict[str, List[int]] = defaultdict(list)
        for i, fm in enumerate(self.file_metrics):
            by_basename[Path(fm.filepath).name].append(i)

        # Adjacency list: node index -> list of included node indices (deduped).
        adj: List[List[int]] = [[] for _ in self.file_metrics]
        for i, fm in enumerate(self.file_metrics):
            seen: Set[int] = set()
            for inc in fm.includes:
                for target in by_basename.get(inc, ()):
                    if target != i and target not in seen:
                        seen.add(target)
                        adj[i].append(target)
            fm.fan_out = len(adj[i])

        fan_in_count = [0] * len(self.file_metrics)
        for i, targets in enumerate(adj):
            for t in targets:
                fan_in_count[t] += 1
        for i, fm in enumerate(self.file_metrics):
            fm.fan_in = fan_in_count[i]

        # Tarjan's SCC (iterative to avoid recursion limits on large graphs).
        index_counter = [0]
        stack: List[int] = []
        on_stack: List[bool] = [False] * len(self.file_metrics)
        index: List[int] = [-1] * len(self.file_metrics)
        lowlink: List[int] = [0] * len(self.file_metrics)
        sccs: List[List[int]] = []

        def strongconnect(start: int) -> None:
            work: List[Tuple[int, int]] = [(start, 0)]
            call_stack: List[int] = []
            while work:
                v, pi = work[-1]
                if pi == 0:
                    index[v] = index_counter[0]
                    lowlink[v] = index_counter[0]
                    index_counter[0] += 1
                    stack.append(v)
                    on_stack[v] = True
                neighbours = adj[v]
                if pi < len(neighbours):
                    work[-1] = (v, pi + 1)
                    w = neighbours[pi]
                    if index[w] == -1:
                        work.append((w, 0))
                    elif on_stack[w]:
                        if index[w] < lowlink[v]:
                            lowlink[v] = index[w]
                    continue
                # Finished v — pop and update parent lowlink.
                work.pop()
                if call_stack:
                    parent = call_stack[-1]
                    if lowlink[v] < lowlink[parent]:
                        lowlink[parent] = lowlink[v]
                if lowlink[v] == index[v]:
                    scc: List[int] = []
                    while True:
                        w = stack.pop()
                        on_stack[w] = False
                        scc.append(w)
                        if w == v:
                            break
                    sccs.append(scc)
                call_stack.append(v)
            # Drain remaining call_stack parents — already processed via lowlink updates.

        for v in range(len(self.file_metrics)):
            if index[v] == -1:
                strongconnect(v)

        self.include_cycles: List[List[str]] = []
        for scc in sccs:
            is_cycle = len(scc) > 1
            # Also flag self-loops (a file that somehow includes itself by basename).
            if not is_cycle and scc and scc[0] in adj[scc[0]]:
                is_cycle = True
            if is_cycle:
                for node in scc:
                    self.file_metrics[node].in_cycle = True
                self.include_cycles.append(
                    sorted(self.file_metrics[n].filepath for n in scc)
                )

        # Largest cycles first for reporting.
        self.include_cycles.sort(key=lambda c: -len(c))
        if self.include_cycles:
            print(f"  Found {len(self.include_cycles)} include cycle(s) "
                  f"(SCC size >= 2 or self-loop).")

    def _find_duplicates(self) -> None:
        """
        Populate `self.duplicate_clusters` with near-duplicate function groups.

        Uses token-normalized 5-gram shingles + pairwise Jaccard clustering. Functions
        below `_DUP_MIN_TOKENS` were already given empty shingle sets and are skipped.
        Each cluster lists every member function with file/line/name/score so consumers
        can act on the finding without a second pass.
        """
        entries: List[Tuple[int, frozenset]] = [
            (i, fm.shingles) for i, fm in enumerate(self.function_metrics) if fm.shingles
        ]
        if len(entries) < 2:
            self.duplicate_clusters = []
            return

        clusters = _cluster_duplicates(entries)
        output: List[List[Dict]] = []
        for group in clusters:
            members: List[Dict] = []
            for idx in group:
                fm = self.function_metrics[idx]
                members.append({
                    'file': fm.filepath,
                    'line': fm.start_line,
                    'name': fm.name,
                    'code_lines': fm.code_lines,
                    'priority_score': fm.priority_score,
                })
            # Sort cluster members by priority desc so the most painful one leads.
            members.sort(key=lambda m: -m['priority_score'])
            output.append(members)
        # Order clusters by size desc, then by leader's priority score.
        output.sort(key=lambda c: (-len(c), -c[0]['priority_score']))
        self.duplicate_clusters = output
        if output:
            print(f"  Found {len(output)} near-duplicate cluster(s) "
                  f"(>= {int(_DUP_JACCARD_THRESHOLD * 100)}% Jaccard).")

    def _aggregate_by_directory(self) -> None:
        """Aggregate metrics by directory"""
        dir_files: Dict[str, List[FileMetrics]] = defaultdict(list)

        for fm in self.file_metrics:
            dir_path = str(Path(fm.filepath).parent)
            dir_files[dir_path].append(fm)

        for dir_path, files in dir_files.items():
            if not files:
                continue

            priority_scores = sorted(f.priority_score for f in files)
            worst_file_entry = max(files, key=lambda f: f.priority_score)

            dm = DirectoryMetrics(
                path=dir_path,
                file_count=len(files),
                total_lines=sum(f.total_lines for f in files),
                code_lines=sum(f.code_lines for f in files),
                comment_lines=sum(f.comment_lines for f in files),
                blank_lines=sum(f.blank_lines for f in files),
                avg_cyclomatic=sum(f.cyclomatic_complexity for f in files) / len(files),
                avg_cognitive=sum(f.cognitive_complexity for f in files) / len(files),
                avg_maintainability=sum(f.maintainability_index for f in files) / len(files),
                max_priority=round(priority_scores[-1], 1),
                p90_priority=round(_percentile(priority_scores, 90), 1),
                p50_priority=round(_percentile(priority_scores, 50), 1),
                worst_file=worst_file_entry.filepath,
                total_functions=sum(f.function_count for f in files),
                total_classes=sum(f.class_count for f in files),
                files=files
            )
            self.directory_metrics[dir_path] = dm
    
    def get_summary(self) -> Dict:
        """
        Get overall summary statistics.

        Headline signal is the priority-score distribution across files — not MI, and
        not Halstead "estimated bugs" / "time to program". Those exist as inputs to
        priority scoring and in per-file CSV output only, not as headline numbers, because
        MI's formula is dated (ADA/FORTRAN 1994) and Halstead B/T constants are 1977-era
        estimates that sound authoritative without guiding any refactoring decision.
        """
        if not self.file_metrics:
            return {}

        total_files = len(self.file_metrics)
        total_loc = sum(f.code_lines for f in self.file_metrics)
        total_lines = sum(f.total_lines for f in self.file_metrics)

        # Priority score distribution — the actionable headline.
        priority_scores = sorted((f.priority_score for f in self.file_metrics), reverse=True)
        high_count = sum(1 for s in priority_scores if s >= 70)
        medium_count = sum(1 for s in priority_scores if 40 <= s < 70)
        low_count = total_files - high_count - medium_count
        high_pct = 100.0 * high_count / total_files if total_files else 0.0

        sorted_asc = sorted(priority_scores)
        p50 = _percentile(sorted_asc, 50)
        p90 = _percentile(sorted_asc, 90)

        return {
            'total_files': total_files,
            'total_lines': total_lines,
            'total_code_lines': total_loc,
            'total_comment_lines': sum(f.comment_lines for f in self.file_metrics),
            'total_blank_lines': sum(f.blank_lines for f in self.file_metrics),
            'avg_cyclomatic': sum(f.cyclomatic_complexity for f in self.file_metrics) / total_files,
            'max_cyclomatic': max(f.cyclomatic_complexity for f in self.file_metrics),
            'avg_cognitive': sum(f.cognitive_complexity for f in self.file_metrics) / total_files,
            'max_cognitive': max(f.cognitive_complexity for f in self.file_metrics),
            'total_functions': sum(f.function_count for f in self.file_metrics),
            'total_classes': sum(f.class_count for f in self.file_metrics),
            'priority_high_count': high_count,
            'priority_medium_count': medium_count,
            'priority_low_count': low_count,
            'priority_high_pct': round(high_pct, 1),
            'priority_p50': round(p50, 1),
            'priority_p90': round(p90, 1),
        }
    
    def print_report(self) -> None:
        """Print a text report of the analysis"""
        summary = self.get_summary()
        
        print("\n" + "=" * 80)
        print("CODE COMPLEXITY AND MAINTAINABILITY REPORT")
        print("=" * 80)
        
        print("\n=== OVERALL SUMMARY ===")
        print("-" * 40)
        print(f"  Total Files:          {summary['total_files']:,}")
        print(f"  Total Lines:          {summary['total_lines']:,}")
        print(f"  Code Lines (SLOC):    {summary['total_code_lines']:,}")
        print(f"  Comment Lines:        {summary['total_comment_lines']:,}")
        print(f"  Blank Lines:          {summary['total_blank_lines']:,}")
        print(f"  Total Functions:      {summary['total_functions']:,}")
        print(f"  Total Classes:        {summary['total_classes']:,}")
        
        print("\n=== COMPLEXITY METRICS ===")
        print("-" * 40)
        print(f"  Avg Cyclomatic:       {summary['avg_cyclomatic']:.2f}")
        print(f"  Max Cyclomatic:       {summary['max_cyclomatic']}")
        print(f"  Avg Cognitive:        {summary['avg_cognitive']:.2f}")
        print(f"  Max Cognitive:        {summary['max_cognitive']}")

        print("\n=== PRIORITY SCORE DISTRIBUTION ===")
        print("-" * 40)
        print(f"  High priority (>=70): {summary['priority_high_count']} files "
              f"({summary['priority_high_pct']:.1f}%)")
        print(f"  Medium (40-69):       {summary['priority_medium_count']} files")
        print(f"  Low (<40):            {summary['priority_low_count']} files")
        print(f"  P50 file score:       {summary['priority_p50']}")
        print(f"  P90 file score:       {summary['priority_p90']}")

        # Directory breakdown — sort by worst-file priority_score, not by LOC.
        print("\n=== TOP DIRECTORIES BY WORST FILE ===")
        print("-" * 40)
        sorted_dirs = sorted(
            self.directory_metrics.values(),
            key=lambda d: getattr(d, 'max_priority', 0.0),
            reverse=True
        )[:10]

        for dm in sorted_dirs:
            max_p = getattr(dm, 'max_priority', 0.0)
            p50_p = getattr(dm, 'p50_priority', 0.0)
            print(f"  {dm.path}")
            print(f"    Files: {dm.file_count}, LOC: {dm.code_lines:,}, "
                  f"Max priority: {max_p:.1f}, P50: {p50_p:.1f}, Avg CC: {dm.avg_cyclomatic:.1f}")

        # Files needing attention — by priority_score.
        print("\n=== TOP 10 FILES NEEDING ATTENTION (by priority score) ===")
        print("-" * 40)
        sorted_files = sorted(
            self.file_metrics,
            key=lambda f: f.priority_score,
            reverse=True
        )[:10]

        for fm in sorted_files:
            print(f"  {fm.filepath}")
            print(f"    score={fm.priority_score:.1f}  CC={fm.cyclomatic_complexity}  "
                  f"cog={fm.cognitive_complexity}  nesting={fm.max_nesting_depth}  "
                  f"LOC={fm.code_lines}")

        # Coupling — top fan-in + cycle count.
        high_fanin = sorted(
            [f for f in self.file_metrics if f.fan_in > 0],
            key=lambda f: -f.fan_in
        )[:10]
        if high_fanin:
            print("\n=== TOP 10 HIGH FAN-IN HEADERS ===")
            print("-" * 40)
            for f in high_fanin:
                cycle = "  [in cycle]" if f.in_cycle else ""
                print(f"  {f.filepath}  fan_in={f.fan_in}  fan_out={f.fan_out}{cycle}")
        if self.include_cycles:
            print(f"\n  Include cycles detected: {len(self.include_cycles)}")

        # LLM-generated refactor suggestions (if --llm-suggestions was used).
        if self.llm_suggestions:
            print(f"\n=== AI REFACTOR SUGGESTIONS ({len(self.llm_suggestions)} function(s)) ===")
            print("-" * 40)
            for entry in self.llm_suggestions:
                print(f"  {entry['file']}:{entry['line']}  {entry['name']}")
                if entry.get('summary'):
                    print(f"    {entry['summary']}")
                for rec in entry.get('recommendations', [])[:3]:
                    approach = rec.get('approach', '')
                    lines = rec.get('target_lines', '?')
                    print(f"    - {approach} (lines {lines})")

        # Near-duplicate clusters — show leader of each of the top 10 clusters.
        if self.duplicate_clusters:
            print(f"\n=== NEAR-DUPLICATE CLUSTERS ({len(self.duplicate_clusters)} total) ===")
            print("-" * 40)
            for i, cluster in enumerate(self.duplicate_clusters[:10], 1):
                leader = cluster[0]
                print(f"  {i}. {len(cluster)} functions similar, led by:")
                print(f"     {leader['file']}:{leader['line']}  {leader['name']}")

        # Per-function refactoring queue: the highest-ROI section for Claude/human
        # reviewers. Sorted by priority_score, formatted as `file:line` so it's
        # trivially clickable and parseable.
        if self.function_metrics:
            print("\n=== TOP 20 FUNCTION HOTSPOTS (refactoring queue) ===")
            print("-" * 40)
            top_funcs = sorted(
                self.function_metrics,
                key=lambda f: f.priority_score,
                reverse=True
            )[:20]
            for i, func in enumerate(top_funcs, 1):
                print(f"  {i:2d}. {func.filepath}:{func.start_line}  {func.name}")
                print(f"      score={func.priority_score:.1f}  "
                      f"CC={func.cyclomatic_complexity}  cog={func.cognitive_complexity}  "
                      f"nesting={func.max_nesting_depth}  LOC={func.code_lines}")
                if func.priority_factors:
                    ordered = sorted(func.priority_factors.items(), key=lambda kv: -kv[1])
                    factors_str = '  '.join(f"{k}={v:.0f}%" for k, v in ordered if v > 0)
                    if factors_str:
                        print(f"      breakdown: {factors_str}")


class MetricsVisualizer:
    """Visualize code metrics"""

    # Colorblind-friendly palette (blue-salmon-red, avoids red-green confusion)
    COLOR_GOOD = '#2166ac'      # Blue (good)
    COLOR_MODERATE = '#f4a582'  # Light salmon (moderate)
    COLOR_POOR = '#b2182b'      # Dark red (poor)
    COLOR_PRIMARY = '#4393c3'   # Steel blue
    COLOR_SECONDARY = '#d6604d' # Coral
    COLOR_TERTIARY = '#92c5de'  # Light blue
    COLOR_NEUTRAL = '#d9d9d9'   # Light gray

    def __init__(self, analyzer: CppAnalyzer, output_dir: str = None):
        self.analyzer = analyzer
        self.output_dir = Path(output_dir) if output_dir else Path.cwd()
        self.output_dir.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def _short_dir_name(path: str, max_parts: int = 2) -> str:
        """Get a short but unique directory label using up to max_parts path components."""
        parts = path.replace('\\', '/').split('/')
        relevant = parts[-max_parts:] if len(parts) >= max_parts else parts
        result = '/'.join(relevant)
        return result or path

    @staticmethod
    def _short_file_name(filepath: str, max_parts: int = 2) -> str:
        """Get a short but unique file label using parent directory + filename."""
        parts = Path(filepath).parts
        relevant = parts[-max_parts:] if len(parts) >= max_parts else parts
        return '/'.join(relevant)
    
    def create_all_visualizations(self) -> None:
        """Create all visualizations"""
        if not HAS_MATPLOTLIB:
            print("Skipping visualizations (matplotlib not installed)")
            return
        
        print("\nGenerating visualizations...")
        
        self.plot_directory_comparison()
        self.plot_complexity_distribution()
        self.plot_maintainability_heatmap()
        self.plot_loc_breakdown()
        self.plot_halstead_metrics()
        self.plot_top_complex_files()
        self.plot_complexity_vs_loc_scatter()
        self.plot_nesting_depth_analysis()
        self.plot_comment_ratio_by_directory()
        self.plot_file_size_distribution()
        self.plot_complexity_per_function()
        self.plot_dashboard_summary()

        print(f"Visualizations saved to: {self.output_dir}")
    
    def plot_directory_comparison(self) -> None:
        """Bar chart comparing directories by various metrics"""
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: d.code_lines,
            reverse=True
        )[:15]
        
        if not dirs:
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(16, 12))
        fig.suptitle('Directory Metrics Comparison', fontsize=14, fontweight='bold')
        
        names = [self._short_dir_name(d.path) for d in dirs]

        # Code lines
        ax = axes[0, 0]
        values = [d.code_lines for d in dirs]
        bars = ax.barh(names, values, color=self.COLOR_PRIMARY)
        ax.set_xlabel('Lines of Code')
        ax.set_title('Code Lines by Directory')
        ax.invert_yaxis()

        # Avg Cyclomatic Complexity
        ax = axes[0, 1]
        values = [d.avg_cyclomatic for d in dirs]
        colors = [self.COLOR_GOOD if v < 10 else self.COLOR_MODERATE if v < 20 else self.COLOR_POOR for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Average Cyclomatic Complexity')
        ax.set_title('Avg Cyclomatic Complexity by Directory')
        ax.axvline(x=10, color=self.COLOR_MODERATE, linestyle='--', alpha=0.7, label='Moderate (10)')
        ax.axvline(x=20, color=self.COLOR_POOR, linestyle='--', alpha=0.7, label='High (20)')
        ax.legend()
        ax.invert_yaxis()

        # Avg Cognitive Complexity
        ax = axes[1, 0]
        values = [d.avg_cognitive for d in dirs]
        colors = [self.COLOR_GOOD if v < 15 else self.COLOR_MODERATE if v < 30 else self.COLOR_POOR for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Average Cognitive Complexity')
        ax.set_title('Avg Cognitive Complexity by Directory')
        ax.invert_yaxis()

        # Maintainability Index
        ax = axes[1, 1]
        values = [d.avg_maintainability for d in dirs]
        colors = [self.COLOR_GOOD if v >= 65 else self.COLOR_MODERATE if v >= 40 else self.COLOR_POOR for v in values]
        ax.barh(names, values, color=colors)
        ax.set_xlabel('Maintainability Index (0-100)')
        ax.set_title('Avg Maintainability Index by Directory')
        ax.axvline(x=65, color=self.COLOR_GOOD, linestyle='--', alpha=0.7, label='Good (65)')
        ax.axvline(x=40, color=self.COLOR_MODERATE, linestyle='--', alpha=0.7, label='Moderate (40)')
        ax.legend()
        ax.invert_yaxis()
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'directory_comparison.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_complexity_distribution(self) -> None:
        """Histogram of complexity distributions"""
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Complexity Metric Distributions', fontsize=14, fontweight='bold')
        
        cc_values = [f.cyclomatic_complexity for f in self.analyzer.file_metrics]
        cog_values = [f.cognitive_complexity for f in self.analyzer.file_metrics]
        mi_values = [f.maintainability_index for f in self.analyzer.file_metrics]
        vol_values = [f.halstead.volume for f in self.analyzer.file_metrics if f.halstead.volume > 0]
        
        # Cyclomatic Complexity Distribution
        ax = axes[0, 0]
        ax.hist(cc_values, bins=30, color=self.COLOR_PRIMARY, edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(cc_values), color=self.COLOR_POOR, linestyle='--', label=f'Mean: {np.mean(cc_values):.1f}')
        ax.axvline(np.median(cc_values), color=self.COLOR_MODERATE, linestyle='--', label=f'Median: {np.median(cc_values):.1f}')
        ax.set_xlabel('Cyclomatic Complexity')
        ax.set_ylabel('Number of Files')
        ax.set_title('Cyclomatic Complexity Distribution')
        ax.legend()
        
        # Cognitive Complexity Distribution
        ax = axes[0, 1]
        ax.hist(cog_values, bins=30, color=self.COLOR_SECONDARY, edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(cog_values), color=self.COLOR_POOR, linestyle='--', label=f'Mean: {np.mean(cog_values):.1f}')
        ax.axvline(np.median(cog_values), color=self.COLOR_PRIMARY, linestyle='--', label=f'Median: {np.median(cog_values):.1f}')
        ax.set_xlabel('Cognitive Complexity')
        ax.set_ylabel('Number of Files')
        ax.set_title('Cognitive Complexity Distribution')
        ax.legend()
        
        # Maintainability Index Distribution
        ax = axes[1, 0]
        ax.hist(mi_values, bins=30, color=self.COLOR_GOOD, edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(mi_values), color=self.COLOR_POOR, linestyle='--', label=f'Mean: {np.mean(mi_values):.1f}')
        ax.axvline(65, color=self.COLOR_GOOD, linestyle=':', label='Good threshold (65)')
        ax.axvline(40, color=self.COLOR_MODERATE, linestyle=':', label='Moderate threshold (40)')
        ax.set_xlabel('Maintainability Index')
        ax.set_ylabel('Number of Files')
        ax.set_title('Maintainability Index Distribution')
        ax.legend()
        
        # Halstead Volume Distribution (log scale)
        ax = axes[1, 1]
        if vol_values:
            ax.hist(vol_values, bins=30, color=self.COLOR_PRIMARY, edgecolor='black', alpha=0.7)
            ax.set_xlabel('Halstead Volume')
            ax.set_ylabel('Number of Files')
            ax.set_title('Halstead Volume Distribution')
            ax.set_xscale('log')
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'complexity_distribution.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_maintainability_heatmap(self) -> None:
        """Create a heatmap showing maintainability by directory structure"""
        dirs = list(self.analyzer.directory_metrics.values())
        if not dirs:
            return
        
        # Sort by path for logical grouping
        dirs = sorted(dirs, key=lambda d: d.path)
        
        fig, ax = plt.subplots(figsize=(14, max(8, len(dirs) * 0.3)))
        
        names = [d.path for d in dirs]
        mi_values = [d.avg_maintainability for d in dirs]
        
        # Create color-coded horizontal bars
        cmap = LinearSegmentedColormap.from_list('mi', [self.COLOR_POOR, self.COLOR_MODERATE, self.COLOR_GOOD])
        normalized = [v / 100 for v in mi_values]
        colors = [cmap(n) for n in normalized]
        
        y_pos = range(len(names))
        bars = ax.barh(y_pos, mi_values, color=colors, edgecolor='black', alpha=0.8)
        
        ax.set_yticks(y_pos)
        ax.set_yticklabels(names, fontsize=8)
        ax.set_xlabel('Maintainability Index')
        ax.set_title('Maintainability Index by Directory\n(Blue=Good, Salmon=Moderate, Red=Poor)')
        ax.set_xlim(0, 100)

        # Add threshold lines
        ax.axvline(x=65, color=self.COLOR_GOOD, linestyle='--', alpha=0.5)
        ax.axvline(x=40, color=self.COLOR_MODERATE, linestyle='--', alpha=0.5)
        
        # Add value labels
        for bar, val in zip(bars, mi_values):
            ax.text(bar.get_width() + 1, bar.get_y() + bar.get_height()/2,
                   f'{val:.1f}', va='center', fontsize=7)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'maintainability_heatmap.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_loc_breakdown(self) -> None:
        """Pie chart and bar chart of LOC breakdown"""
        summary = self.analyzer.get_summary()
        
        fig, axes = plt.subplots(1, 2, figsize=(14, 6))
        fig.suptitle('Lines of Code Analysis', fontsize=14, fontweight='bold')
        
        # Pie chart of line types
        ax = axes[0]
        sizes = [summary['total_code_lines'], summary['total_comment_lines'], summary['total_blank_lines']]
        labels = ['Code Lines', 'Comment Lines', 'Blank Lines']
        colors = [self.COLOR_PRIMARY, self.COLOR_TERTIARY, self.COLOR_NEUTRAL]
        explode = (0.05, 0, 0)
        
        ax.pie(sizes, explode=explode, labels=labels, colors=colors, autopct='%1.1f%%',
               shadow=True, startangle=90)
        ax.set_title('Line Type Distribution')
        
        # Bar chart of LOC by top directories
        ax = axes[1]
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: d.total_lines,
            reverse=True
        )[:12]
        
        names = [self._short_dir_name(d.path) for d in dirs]
        code = [d.code_lines for d in dirs]
        comments = [d.comment_lines for d in dirs]
        blank = [d.blank_lines for d in dirs]

        x = np.arange(len(names))
        width = 0.25

        ax.bar(x - width, code, width, label='Code', color=self.COLOR_PRIMARY)
        ax.bar(x, comments, width, label='Comments', color=self.COLOR_TERTIARY)
        ax.bar(x + width, blank, width, label='Blank', color=self.COLOR_NEUTRAL)
        
        ax.set_xlabel('Directory')
        ax.set_ylabel('Lines')
        ax.set_title('LOC Breakdown by Directory')
        ax.set_xticks(x)
        ax.set_xticklabels(names, rotation=45, ha='right')
        ax.legend()
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'loc_breakdown.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_halstead_metrics(self) -> None:
        """Visualize Halstead metrics"""
        files = [f for f in self.analyzer.file_metrics if f.halstead.volume > 0]
        if not files:
            return
        
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        fig.suptitle('Halstead Metrics Analysis', fontsize=14, fontweight='bold')
        
        # Volume vs Difficulty scatter
        ax = axes[0, 0]
        volumes = [f.halstead.volume for f in files]
        difficulties = [f.halstead.difficulty for f in files]
        ax.scatter(volumes, difficulties, alpha=0.5, c=self.COLOR_PRIMARY)
        ax.set_xlabel('Halstead Volume')
        ax.set_ylabel('Halstead Difficulty')
        ax.set_title('Volume vs Difficulty')
        ax.set_xscale('log')
        
        # Effort distribution
        ax = axes[0, 1]
        efforts = [f.halstead.effort for f in files if f.halstead.effort > 0]
        ax.hist(efforts, bins=30, color=self.COLOR_SECONDARY, edgecolor='black', alpha=0.7)
        ax.set_xlabel('Halstead Effort')
        ax.set_ylabel('Number of Files')
        ax.set_title('Programming Effort Distribution')
        ax.set_xscale('log')
        
        # Estimated bugs by directory
        ax = axes[1, 0]
        dirs = sorted(
            self.analyzer.directory_metrics.values(),
            key=lambda d: sum(f.halstead.bugs_delivered for f in d.files),
            reverse=True
        )[:10]
        
        names = [self._short_dir_name(d.path) for d in dirs]
        bugs = [sum(f.halstead.bugs_delivered for f in d.files) for d in dirs]

        ax.barh(names, bugs, color=self.COLOR_POOR, alpha=0.7)
        ax.set_xlabel('Estimated Bugs (Halstead B)')
        ax.set_title('Estimated Bugs by Directory')
        ax.invert_yaxis()
        
        # Time to program
        ax = axes[1, 1]
        times = [f.halstead.time_to_program / 3600 for f in files if f.halstead.time_to_program > 0]  # Convert to hours
        ax.hist(times, bins=30, color=self.COLOR_TERTIARY, edgecolor='black', alpha=0.7)
        ax.set_xlabel('Estimated Time to Program (hours)')
        ax.set_ylabel('Number of Files')
        ax.set_title('Estimated Programming Time Distribution')
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'halstead_metrics.png', dpi=150, bbox_inches='tight')
        plt.close()
    
    def plot_top_complex_files(self) -> None:
        """Bar chart of most complex files"""
        fig, axes = plt.subplots(1, 2, figsize=(16, 8))
        fig.suptitle('Files Requiring Attention', fontsize=14, fontweight='bold')
        
        # Top by Cyclomatic Complexity
        ax = axes[0]
        files = sorted(self.analyzer.file_metrics, key=lambda f: f.cyclomatic_complexity, reverse=True)[:15]
        names = [self._short_file_name(f.filepath) for f in files]
        values = [f.cyclomatic_complexity for f in files]
        colors = [self.COLOR_GOOD if v < 10 else self.COLOR_MODERATE if v < 20 else self.COLOR_POOR for v in values]

        bars = ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.set_xlabel('Cyclomatic Complexity')
        ax.set_title('Top 15 Files by Cyclomatic Complexity')
        ax.invert_yaxis()

        # Add threshold annotations
        ax.axvline(x=10, color=self.COLOR_MODERATE, linestyle='--', alpha=0.7)
        ax.axvline(x=20, color=self.COLOR_POOR, linestyle='--', alpha=0.7)

        # Lowest Maintainability
        ax = axes[1]
        files = sorted(self.analyzer.file_metrics, key=lambda f: f.maintainability_index)[:15]
        names = [self._short_file_name(f.filepath) for f in files]
        values = [f.maintainability_index for f in files]
        colors = [self.COLOR_POOR if v < 40 else self.COLOR_MODERATE if v < 65 else self.COLOR_GOOD for v in values]
        
        bars = ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.set_xlabel('Maintainability Index')
        ax.set_title('Top 15 Files with Lowest Maintainability')
        ax.invert_yaxis()
        ax.set_xlim(0, 100)
        
        plt.tight_layout()
        plt.savefig(self.output_dir / 'files_requiring_attention.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_complexity_vs_loc_scatter(self) -> None:
        """Risk quadrant: Complexity vs LOC scatter, colored by maintainability"""
        files = self.analyzer.file_metrics
        if not files:
            return

        fig, ax = plt.subplots(figsize=(12, 8))

        locs = [f.code_lines for f in files]
        ccs = [f.cyclomatic_complexity for f in files]
        mis = [f.maintainability_index for f in files]

        scatter = ax.scatter(locs, ccs, c=mis, cmap='RdYlBu', alpha=0.6,
                             edgecolors='black', linewidth=0.3, s=30,
                             vmin=0, vmax=100)

        # Quadrant dividers at median
        loc_med = np.median(locs)
        cc_med = np.median(ccs)
        ax.axvline(x=loc_med, color='gray', linestyle='--', alpha=0.4)
        ax.axhline(y=cc_med, color='gray', linestyle='--', alpha=0.4)

        # Label quadrants
        x_min, x_max = ax.get_xlim()
        y_min, y_max = ax.get_ylim()
        ax.text(x_min + (loc_med - x_min) * 0.5, y_max * 0.95, 'Small & Complex',
                ha='center', va='top', fontsize=9, color='gray', alpha=0.7)
        ax.text(x_max * 0.75, y_max * 0.95, 'Large & Complex',
                ha='center', va='top', fontsize=9, color='gray', alpha=0.7)
        ax.text(x_min + (loc_med - x_min) * 0.5, y_min + (cc_med - y_min) * 0.1, 'Small & Simple',
                ha='center', va='bottom', fontsize=9, color='gray', alpha=0.7)
        ax.text(x_max * 0.75, y_min + (cc_med - y_min) * 0.1, 'Large & Simple',
                ha='center', va='bottom', fontsize=9, color='gray', alpha=0.7)

        # Annotate top 5 riskiest files (highest LOC * CC product)
        risk_scores = [(l * c, i) for i, (l, c) in enumerate(zip(locs, ccs))]
        risk_scores.sort(reverse=True)
        for _, idx in risk_scores[:5]:
            fm = files[idx]
            ax.annotate(self._short_file_name(fm.filepath),
                        (locs[idx], ccs[idx]), fontsize=7, alpha=0.8,
                        xytext=(5, 5), textcoords='offset points')

        plt.colorbar(scatter, label='Maintainability Index')
        ax.set_xlabel('Lines of Code')
        ax.set_ylabel('Cyclomatic Complexity')
        ax.set_title('Risk Quadrant: Complexity vs Size (colored by Maintainability)')

        plt.tight_layout()
        plt.savefig(self.output_dir / 'risk_quadrant.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_nesting_depth_analysis(self) -> None:
        """Nesting depth: top files bar chart + distribution histogram"""
        files = self.analyzer.file_metrics
        if not files:
            return

        fig, axes = plt.subplots(1, 2, figsize=(16, 8))
        fig.suptitle('Nesting Depth Analysis', fontsize=14, fontweight='bold')

        # Left: top 15 files by nesting depth
        ax = axes[0]
        top_files = sorted(files, key=lambda f: f.max_nesting_depth, reverse=True)[:15]
        names = [self._short_file_name(f.filepath) for f in top_files]
        values = [f.max_nesting_depth for f in top_files]
        colors = [self.COLOR_GOOD if v <= 4 else self.COLOR_MODERATE if v <= 7 else self.COLOR_POOR for v in values]

        ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.set_xlabel('Max Nesting Depth')
        ax.set_title('Top 15 Files by Nesting Depth')
        ax.axvline(x=4, color=self.COLOR_MODERATE, linestyle='--', alpha=0.7, label='Moderate (4)')
        ax.axvline(x=7, color=self.COLOR_POOR, linestyle='--', alpha=0.7, label='High (7)')
        ax.legend()
        ax.invert_yaxis()

        # Right: histogram of all nesting depths
        ax = axes[1]
        all_depths = [f.max_nesting_depth for f in files]
        if not all_depths:
            return
        ax.hist(all_depths, bins=max(1, max(all_depths) - min(all_depths)),
                color=self.COLOR_PRIMARY, edgecolor='black', alpha=0.7)
        ax.axvline(np.mean(all_depths), color=self.COLOR_POOR, linestyle='--',
                   label=f'Mean: {np.mean(all_depths):.1f}')
        ax.axvline(np.median(all_depths), color=self.COLOR_MODERATE, linestyle='--',
                   label=f'Median: {np.median(all_depths):.1f}')
        ax.set_xlabel('Max Nesting Depth')
        ax.set_ylabel('Number of Files')
        ax.set_title('Nesting Depth Distribution')
        ax.legend()

        plt.tight_layout()
        plt.savefig(self.output_dir / 'nesting_depth.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_comment_ratio_by_directory(self) -> None:
        """Comment-to-code ratio by directory, highlights under-documented areas"""
        # Filter to directories with at least 5 files to reduce noise
        dirs = [d for d in self.analyzer.directory_metrics.values()
                if d.file_count >= 5 and d.code_lines > 0]
        if not dirs:
            return

        dirs = sorted(dirs, key=lambda d: d.comment_lines / d.code_lines)

        fig, ax = plt.subplots(figsize=(12, max(6, len(dirs) * 0.35)))

        names = [self._short_dir_name(d.path) for d in dirs]
        ratios = [d.comment_lines / d.code_lines for d in dirs]

        colors = [self.COLOR_POOR if r < 0.05 else self.COLOR_MODERATE if r < 0.1 else self.COLOR_GOOD for r in ratios]
        ax.barh(names, ratios, color=colors, edgecolor='black', alpha=0.8)
        ax.axvline(x=0.1, color=self.COLOR_GOOD, linestyle='--', alpha=0.7, label='10% target')
        ax.set_xlabel('Comment / Code Ratio')
        ax.set_title('Comment Ratio by Directory (min 5 files)\n(Blue=Good, Salmon=Low, Red=Very Low)')
        ax.legend()

        # Add percentage labels
        for i, (bar_val, name) in enumerate(zip(ratios, names)):
            ax.text(bar_val + 0.005, i, f'{bar_val:.1%}', va='center', fontsize=7)

        plt.tight_layout()
        plt.savefig(self.output_dir / 'comment_ratio.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_file_size_distribution(self) -> None:
        """Histogram of file sizes with percentile markers"""
        files = self.analyzer.file_metrics
        if not files:
            return

        fig, ax = plt.subplots(figsize=(12, 7))

        sizes = [f.code_lines for f in files]
        sizes_arr = np.array(sizes)

        ax.hist(sizes, bins=50, color=self.COLOR_PRIMARY, edgecolor='black', alpha=0.7)

        # Percentile lines
        percentiles = [50, 75, 90, 95]
        line_styles = ['--', '-.', ':', '-']
        for pct, ls in zip(percentiles, line_styles):
            val = np.percentile(sizes_arr, pct)
            ax.axvline(x=val, color=self.COLOR_SECONDARY, linestyle=ls, alpha=0.8,
                       label=f'P{pct}: {val:.0f}')

        # Stats text box
        stats_text = (f'Files: {len(sizes)}\n'
                      f'Mean: {np.mean(sizes_arr):.0f}\n'
                      f'Median: {np.median(sizes_arr):.0f}\n'
                      f'P90: {np.percentile(sizes_arr, 90):.0f}\n'
                      f'Max: {np.max(sizes_arr):.0f}')
        ax.text(0.97, 0.97, stats_text, transform=ax.transAxes, fontsize=9,
                verticalalignment='top', horizontalalignment='right',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))

        ax.set_xlabel('Code Lines per File')
        ax.set_ylabel('Number of Files')
        ax.set_title('File Size Distribution (Code Lines)')
        ax.legend(loc='upper left')

        plt.tight_layout()
        plt.savefig(self.output_dir / 'file_size_distribution.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_complexity_per_function(self) -> None:
        """Top files by cyclomatic complexity per function"""
        files = [f for f in self.analyzer.file_metrics if f.function_count > 0]
        if not files:
            return

        # Compute CC per function
        files_with_ratio = [(f, f.cyclomatic_complexity / f.function_count) for f in files]
        files_with_ratio.sort(key=lambda x: x[1], reverse=True)
        top = files_with_ratio[:15]

        fig, ax = plt.subplots(figsize=(12, 8))

        names = [self._short_file_name(f.filepath) for f, _ in top]
        values = [r for _, r in top]
        colors = [self.COLOR_GOOD if v < 5 else self.COLOR_MODERATE if v < 10 else self.COLOR_POOR for v in values]

        ax.barh(names, values, color=colors, edgecolor='black', alpha=0.8)
        ax.axvline(x=5, color=self.COLOR_MODERATE, linestyle='--', alpha=0.7, label='Moderate (5)')
        ax.axvline(x=10, color=self.COLOR_POOR, linestyle='--', alpha=0.7, label='High (10)')
        ax.set_xlabel('Cyclomatic Complexity per Function')
        ax.set_title('Top 15 Files by Average Function Complexity')
        ax.legend()
        ax.invert_yaxis()

        plt.tight_layout()
        plt.savefig(self.output_dir / 'complexity_per_function.png', dpi=150, bbox_inches='tight')
        plt.close()

    def plot_dashboard_summary(self) -> None:
        """Health check dashboard with key metrics as big numbers"""
        summary = self.analyzer.get_summary()
        if not summary:
            return

        # Worst file is now the one with highest priority score — that's the "most
        # in need of refactoring" signal, and the rest of the report ranks by it too.
        worst_file = max(self.analyzer.file_metrics, key=lambda f: f.priority_score)
        max_cc_file = max(self.analyzer.file_metrics, key=lambda f: f.cyclomatic_complexity)

        # Health colouring: priority is 0-100 where low is good. Invert for the
        # shared thresholds below (65+ = good, 40-64 = moderate, <40 = poor).
        inv_p50 = max(0.0, 100.0 - summary['priority_p50'])
        inv_p90 = max(0.0, 100.0 - summary['priority_p90'])
        inv_worst = max(0.0, 100.0 - worst_file.priority_score)

        metrics = [
            ('Total Files', f"{summary['total_files']:,}", None),
            ('Code Lines (SLOC)', f"{summary['total_code_lines']:,}", None),
            ('High-priority files',
             f"{summary['priority_high_count']}\n({summary['priority_high_pct']:.1f}%)",
             100 - summary['priority_high_pct']),
            ('P50 file score', f"{summary['priority_p50']:.1f}", inv_p50),
            ('P90 file score', f"{summary['priority_p90']:.1f}", inv_p90),
            ('Worst file',
             f"{self._short_file_name(worst_file.filepath)}\nscore: {worst_file.priority_score:.1f}",
             inv_worst),
        ]

        fig = plt.figure(figsize=(16, 8))
        fig.suptitle('Codebase Health Dashboard', fontsize=16, fontweight='bold', y=0.98)

        for i, (label, value, health_val) in enumerate(metrics):
            ax = fig.add_subplot(2, 3, i + 1)
            ax.axis('off')

            # Background color based on health
            if health_val is not None:
                if health_val >= 65:
                    bg_color = self.COLOR_GOOD + '22'  # with alpha via hex
                    text_color = self.COLOR_GOOD
                elif health_val >= 40:
                    bg_color = self.COLOR_MODERATE + '22'
                    text_color = '#b35900'
                else:
                    bg_color = self.COLOR_POOR + '22'
                    text_color = self.COLOR_POOR
            else:
                bg_color = self.COLOR_NEUTRAL + '44'
                text_color = '#333333'

            ax.set_facecolor(bg_color)
            for spine in ax.spines.values():
                spine.set_visible(True)
                spine.set_color('#cccccc')

            # Render value
            fontsize = 28 if len(value) < 10 else 18 if len(value) < 25 else 13
            ax.text(0.5, 0.45, value, transform=ax.transAxes,
                    fontsize=fontsize, ha='center', va='center',
                    fontweight='bold', color=text_color)
            ax.text(0.5, 0.88, label, transform=ax.transAxes,
                    fontsize=11, ha='center', va='center', color='#666666')

        plt.tight_layout(rect=[0, 0, 1, 0.95])
        plt.savefig(self.output_dir / 'dashboard_summary.png', dpi=150, bbox_inches='tight')
        plt.close()


def export_csv(analyzer: CppAnalyzer, output_dir: Path) -> None:
    """Export metrics to CSV files"""
    import csv
    
    # File metrics CSV
    file_csv = output_dir / 'file_metrics.csv'
    with open(file_csv, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            'File', 'Total Lines', 'Code Lines', 'Comment Lines', 'Blank Lines',
            'Cyclomatic Complexity', 'Cognitive Complexity', 'Maintainability Index',
            'Functions', 'Classes', 'Max Nesting', 'Halstead Volume', 
            'Halstead Difficulty', 'Halstead Effort', 'Estimated Bugs'
        ])
        
        for fm in analyzer.file_metrics:
            writer.writerow([
                fm.filepath, fm.total_lines, fm.code_lines, fm.comment_lines, fm.blank_lines,
                fm.cyclomatic_complexity, fm.cognitive_complexity, f'{fm.maintainability_index:.2f}',
                fm.function_count, fm.class_count, fm.max_nesting_depth,
                f'{fm.halstead.volume:.2f}', f'{fm.halstead.difficulty:.2f}',
                f'{fm.halstead.effort:.2f}', f'{fm.halstead.bugs_delivered:.3f}'
            ])
    
    print(f"  File metrics exported to: {file_csv}")
    
    # Directory metrics CSV
    dir_csv = output_dir / 'directory_metrics.csv'
    with open(dir_csv, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow([
            'Directory', 'File Count', 'Total Lines', 'Code Lines', 'Comment Lines',
            'Avg Cyclomatic', 'Avg Cognitive', 'Avg Maintainability',
            'Total Functions', 'Total Classes'
        ])
        
        for dm in analyzer.directory_metrics.values():
            writer.writerow([
                dm.path, dm.file_count, dm.total_lines, dm.code_lines, dm.comment_lines,
                f'{dm.avg_cyclomatic:.2f}', f'{dm.avg_cognitive:.2f}', f'{dm.avg_maintainability:.2f}',
                dm.total_functions, dm.total_classes
            ])
    
    print(f"  Directory metrics exported to: {dir_csv}")
    
    # Function metrics CSV
    if analyzer.function_metrics:
        func_csv = output_dir / 'function_metrics.csv'
        with open(func_csv, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow([
                'Function Name', 'File', 'Start Line', 'End Line',
                'Cyclomatic Complexity', 'Cognitive Complexity',
                'Max Nesting Depth', 'Code Lines', 'Priority Score'
            ])

            for fm in analyzer.function_metrics:
                writer.writerow([
                    fm.name, fm.filepath, fm.start_line, fm.end_line,
                    fm.cyclomatic_complexity, fm.cognitive_complexity,
                    fm.max_nesting_depth, fm.code_lines,
                    f'{fm.priority_score:.1f}'
                ])

        print(f"  Function metrics exported to: {func_csv}")


JSON_SCHEMA_VERSION = '1.0'

JSON_CAVEATS = (
    "Function-level metrics come from regex-based C++ parsing, which is approximate: "
    "function-try-blocks, C++20 `requires` clauses, macro-generated signatures, nested "
    "templates (e.g. std::vector<std::pair<int,int>>), and `throw(...)` exception specs "
    "may cause functions to be missed. File-level metrics are unaffected. Missing "
    "functions means they are absent from per-function lists, not that the file is simple."
)


def export_json(analyzer: CppAnalyzer, output_dir: Path) -> None:
    """Export comprehensive metrics to a JSON file for machine-readable analysis."""
    import json

    summary = analyzer.get_summary()

    # Group functions by filepath so we can embed top functions per file.
    functions_by_file: Dict[str, List[FunctionMetrics]] = defaultdict(list)
    for func in analyzer.function_metrics:
        functions_by_file[func.filepath].append(func)

    def function_to_dict(func: FunctionMetrics) -> dict:
        return {
            'name': func.name,
            'file': func.filepath,
            'start_line': func.start_line,
            'end_line': func.end_line,
            'cyclomatic_complexity': func.cyclomatic_complexity,
            'cognitive_complexity': func.cognitive_complexity,
            'max_nesting_depth': func.max_nesting_depth,
            'code_lines': func.code_lines,
            'param_count': func.param_count,
            'return_count': func.return_count,
            'case_count': func.case_count,
            'priority_score': func.priority_score,
            'priority_factors': func.priority_factors,
            'tags': func.tags,
            'extract_candidates': func.extract_candidates,
        }

    def file_to_dict(fm: FileMetrics, include_top_functions: bool = False) -> dict:
        out = {
            'filepath': fm.filepath,
            'total_lines': fm.total_lines,
            'code_lines': fm.code_lines,
            'comment_lines': fm.comment_lines,
            'blank_lines': fm.blank_lines,
            'cyclomatic_complexity': fm.cyclomatic_complexity,
            'cognitive_complexity': fm.cognitive_complexity,
            'maintainability_index': round(fm.maintainability_index, 2),
            'max_nesting_depth': fm.max_nesting_depth,
            'function_count': fm.function_count,
            'class_count': fm.class_count,
            'halstead_volume': round(fm.halstead.volume, 2),
            'halstead_difficulty': round(fm.halstead.difficulty, 2),
            'halstead_effort': round(fm.halstead.effort, 2),
            'estimated_bugs': round(fm.halstead.bugs_delivered, 3),
            'priority_score': fm.priority_score,
            'priority_factors': fm.priority_factors,
            'worst_function_score': fm.worst_function_score,
            'fan_in': fm.fan_in,
            'fan_out': fm.fan_out,
            'in_cycle': fm.in_cycle,
            'classes': fm.classes,
            'tags': fm.tags,
            'todo_count': fm.todo_count,
            'fixme_count': fm.fixme_count,
            'hack_count': fm.hack_count,
        }
        if include_top_functions:
            top = sorted(
                functions_by_file.get(fm.filepath, ()),
                key=lambda f: f.priority_score,
                reverse=True,
            )[:5]
            out['top_functions'] = [function_to_dict(f) for f in top]
        return out

    # Ranked lists (top 20 each). `top_priority_files` and `top_complex_files`
    # embed per-file top functions so downstream consumers (including Claude) can see
    # which part of a file is the problem without a second pass.
    top_priority = sorted(analyzer.file_metrics, key=lambda f: f.priority_score, reverse=True)[:20]
    top_complex = sorted(analyzer.file_metrics, key=lambda f: f.cyclomatic_complexity, reverse=True)[:20]
    deepest_nesting = sorted(analyzer.file_metrics, key=lambda f: f.max_nesting_depth, reverse=True)[:20]
    largest = sorted(analyzer.file_metrics, key=lambda f: f.code_lines, reverse=True)[:20]

    # Function hotspots: top 50 by priority_score. The refactoring queue.
    function_hotspots = sorted(
        analyzer.function_metrics,
        key=lambda f: f.priority_score,
        reverse=True,
    )[:50]

    # Low-cohesion classes: classes with LCOM5 >= 0.7 (at least one "split by
    # responsibility" candidate). Include file path so the link is clickable.
    low_cohesion: List[Dict] = []
    for fm in analyzer.file_metrics:
        for cls in fm.classes:
            if cls['lcom5'] >= 0.7 and cls['method_count'] >= 3 and cls['field_count'] >= 3:
                low_cohesion.append({**cls, 'file': fm.filepath})
    low_cohesion.sort(key=lambda c: (-c['lcom5'], -c['method_count']))
    low_cohesion = low_cohesion[:30]

    # Directories — ranked by worst file, not code lines, so the first entries are
    # the places that need attention.
    dir_list = []
    for dm in sorted(analyzer.directory_metrics.values(), key=lambda d: d.max_priority, reverse=True):
        comment_ratio = dm.comment_lines / dm.code_lines if dm.code_lines > 0 else 0
        dir_list.append({
            'path': dm.path,
            'file_count': dm.file_count,
            'total_lines': dm.total_lines,
            'code_lines': dm.code_lines,
            'comment_lines': dm.comment_lines,
            'blank_lines': dm.blank_lines,
            'avg_cyclomatic': round(dm.avg_cyclomatic, 2),
            'avg_cognitive': round(dm.avg_cognitive, 2),
            'avg_maintainability': round(dm.avg_maintainability, 2),
            'max_priority': dm.max_priority,
            'p90_priority': dm.p90_priority,
            'p50_priority': dm.p50_priority,
            'worst_file': dm.worst_file,
            'total_functions': dm.total_functions,
            'total_classes': dm.total_classes,
            'comment_ratio': round(comment_ratio, 4),
        })

    # Flatten tech-debt markers across all files into a single worklist (bounded to 500
    # to keep the JSON manageable on large codebases).
    tech_debt = []
    for fm in analyzer.file_metrics:
        for marker in fm.tech_debt_markers:
            tech_debt.append({
                'file': fm.filepath,
                'line': marker['line'],
                'marker': marker['marker'],
                'text': marker['text'],
            })
    # Order: FIXME/HACK before TODO, then by file path for stable output.
    _marker_priority = {'FIXME': 0, 'HACK': 1, 'XXX': 1, 'TODO': 2}
    tech_debt.sort(key=lambda m: (_marker_priority.get(m['marker'], 3), m['file'], m['line']))

    report = {
        'schema_version': JSON_SCHEMA_VERSION,
        'caveats': JSON_CAVEATS,
        'thresholds': analyzer.thresholds,
        'summary': summary,
        'directories': dir_list,
        'function_hotspots': [function_to_dict(f) for f in function_hotspots],
        'low_cohesion_classes': low_cohesion,
        'llm_suggestions': analyzer.llm_suggestions,
        'duplicate_clusters': analyzer.duplicate_clusters,
        'include_cycles': analyzer.include_cycles,
        'tech_debt_markers': tech_debt[:500],
        'top_priority_files': [file_to_dict(f, include_top_functions=True) for f in top_priority],
        'top_complex_files': [file_to_dict(f, include_top_functions=True) for f in top_complex],
        'deepest_nesting_files': [file_to_dict(f) for f in deepest_nesting],
        'largest_files': [file_to_dict(f) for f in largest],
        'all_files': [file_to_dict(f) for f in analyzer.file_metrics],
    }

    json_path = output_dir / 'analysis_report.json'
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2, default=str)

    print(f"  JSON report exported to: {json_path}")


def _suggest_refactor(tags: List[str]) -> str:
    """Derive a short suggested-approach string from a function's tags."""
    if not tags:
        return ''
    # Pick up to 3 most informative tags for the suggestion, preferring the composites
    # (nesting-hell / big-switch) over individual signals since they imply a shape.
    priority_order = [
        'nesting-hell', 'big-switch', 'very-long-function', 'long-function',
        'deep-nesting', 'many-params', 'many-returns',
        'hard-to-follow', 'complex-branching',
    ]
    hints = []
    for t in priority_order:
        if t in tags and t in _TAG_SUGGESTIONS:
            hints.append(_TAG_SUGGESTIONS[t])
            if len(hints) == 2:
                break
    return '; '.join(hints)


def export_markdown(analyzer: CppAnalyzer, output_dir: Path) -> None:
    """
    Export a human-readable markdown report. Structured so each hotspot entry is easily
    quotable in a PR comment and every file reference is a markdown link to `file:line`.
    """
    summary = analyzer.get_summary()
    out: List[str] = []
    out.append('# Code Complexity Report\n')
    out.append(f'Root: `{analyzer.root_path}`\n')
    out.append('')
    out.append('## Summary\n')
    if summary:
        out.append(f'- Files: **{summary["total_files"]:,}**, SLOC: **{summary["total_code_lines"]:,}**, '
                   f'Functions: **{summary["total_functions"]:,}**, Classes: **{summary["total_classes"]:,}**')
        out.append(f'- Avg Cyclomatic: **{summary["avg_cyclomatic"]:.1f}** (max {summary["max_cyclomatic"]}), '
                   f'Avg Cognitive: **{summary["avg_cognitive"]:.1f}** (max {summary["max_cognitive"]})')
        out.append(f'- Priority score distribution: **{summary["priority_high_count"]}** high (>=70, '
                   f'{summary["priority_high_pct"]:.1f}%), **{summary["priority_medium_count"]}** medium, '
                   f'**{summary["priority_low_count"]}** low. P50={summary["priority_p50"]}, '
                   f'P90={summary["priority_p90"]}.')
    out.append('')

    out.append('## Suggested refactoring queue (top 20 functions)\n')
    out.append('Sorted by composite priority score. Each entry is a single function — the '
               '`file:line` link goes directly to its first line.\n')
    top_funcs = sorted(analyzer.function_metrics,
                       key=lambda f: f.priority_score, reverse=True)[:20]
    if not top_funcs:
        out.append('_No function-level metrics collected. Regex-based C++ parsing may have '
                   'missed functions — see `caveats` in the JSON report._\n')
    for i, func in enumerate(top_funcs, 1):
        loc_ref = f'`{func.filepath}:{func.start_line}`'
        out.append(f'### {i}. {loc_ref} — `{func.name}` (score: {func.priority_score:.1f})')
        if func.tags:
            out.append('Tags: ' + ', '.join(f'`{t}`' for t in func.tags))
        out.append(
            f'CC={func.cyclomatic_complexity}, cognitive={func.cognitive_complexity}, '
            f'nesting={func.max_nesting_depth}, LOC={func.code_lines}, '
            f'params={func.param_count}, returns={func.return_count}'
        )
        if func.priority_factors:
            # Show factors sorted by contribution so the dominant one is first.
            ordered = sorted(func.priority_factors.items(), key=lambda kv: -kv[1])
            factors_str = ', '.join(f'{k}={v:.0f}%' for k, v in ordered if v > 0)
            if factors_str:
                out.append(f'Score breakdown: {factors_str}')
        if func.extract_candidates:
            out.append('')
            out.append('Extract candidates:')
            for cand in func.extract_candidates[:3]:
                out.append(f'- Lines {cand["start_line"]}-{cand["end_line"]} '
                           f'(depth {cand["peak_depth"]}+ for {cand["line_count"]} lines)')
            # Concrete refactor hint: point at the worst extract candidate (most lines,
            # tie-broken by peak depth). Better than generic "flatten with guard clauses".
            worst = max(func.extract_candidates, key=lambda c: (c['line_count'], c['peak_depth']))
            out.append('')
            out.append(
                f'Suggested approach: extract `{func.filepath}:{worst["start_line"]}-'
                f'{worst["end_line"]}` (the depth-{worst["peak_depth"]}+ block of '
                f'{worst["line_count"]} lines) into a helper.'
            )
        else:
            suggestion = _suggest_refactor(func.tags)
            if suggestion:
                out.append('')
                out.append(f'Suggested approach: {suggestion}.')
        out.append('')

    # File-level trouble spots
    tagged_files = [f for f in analyzer.file_metrics if f.tags]
    if tagged_files:
        out.append('## Tagged files\n')
        tagged_files.sort(key=lambda f: f.priority_score, reverse=True)
        out.append('| File | Tags | Priority | CC | Cog | Nesting | LOC | Funcs |')
        out.append('| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |')
        for f in tagged_files[:25]:
            tags = ', '.join(f'`{t}`' for t in f.tags)
            out.append(
                f'| `{f.filepath}` | {tags} | {f.priority_score:.1f} | '
                f'{f.cyclomatic_complexity} | {f.cognitive_complexity} | '
                f'{f.max_nesting_depth} | {f.code_lines} | {f.function_count} |'
            )
        out.append('')

    # LLM-generated refactor suggestions (opt-in via --llm-suggestions).
    if analyzer.llm_suggestions:
        out.append(f'## AI-suggested refactors ({len(analyzer.llm_suggestions)} function(s))\n')
        out.append('Generated by the Anthropic API. One structured recommendation per top '
                   'hotspot; re-running with unchanged source hits the disk cache (no '
                   'repeat API calls).\n')
        for entry in analyzer.llm_suggestions:
            out.append(f'### `{entry["file"]}:{entry["line"]}` — `{entry["name"]}` '
                       f'(score {entry["priority_score"]:.1f})')
            if entry.get('summary'):
                out.append(f'**Summary:** {entry["summary"]}')
            for rec in entry.get('recommendations', []):
                out.append(f'- **{rec.get("approach", "")}** '
                           f'(lines {rec.get("target_lines", "?")}): '
                           f'{rec.get("expected_benefit", "")}')
            out.append('')

    # Low-cohesion classes — LCOM5 ≥ 0.7 means most methods touch few fields.
    # These are classes that probably want splitting by responsibility.
    low_cohesion: List[Dict] = []
    for fm in analyzer.file_metrics:
        for cls in fm.classes:
            if cls['lcom5'] >= 0.7 and cls['method_count'] >= 3 and cls['field_count'] >= 3:
                low_cohesion.append({**cls, 'file': fm.filepath})
    low_cohesion.sort(key=lambda c: (-c['lcom5'], -c['method_count']))
    if low_cohesion:
        out.append('## Low-cohesion classes (LCOM5 >= 0.7, top 15)\n')
        out.append('LCOM5 = 1 - (method/field touches) / (methods * fields). Near 1.0 means '
                   'methods mostly use disjoint subsets of the fields — a split-responsibilities '
                   'candidate.\n')
        out.append('| Class | File | LCOM5 | Methods | Fields |')
        out.append('| --- | --- | ---: | ---: | ---: |')
        for c in low_cohesion[:15]:
            out.append(
                f'| `{c["name"]}` | `{c["file"]}:{c["start_line"]}` | '
                f'{c["lcom5"]:.2f} | {c["method_count"]} | {c["field_count"]} |'
            )
        out.append('')

    # Coupling hotspots — highest fan-in headers are the ones most widely depended on;
    # changing them ripples through the codebase.
    high_fanin = sorted(
        [f for f in analyzer.file_metrics if f.fan_in > 0],
        key=lambda f: -f.fan_in
    )[:15]
    if high_fanin:
        out.append('## Coupling: highest fan-in (top 15)\n')
        out.append('Files included by the most other files. High fan-in = change risk.\n')
        out.append('| File | Fan-in | Fan-out | In cycle |')
        out.append('| --- | ---: | ---: | :---: |')
        for f in high_fanin:
            cycle_mark = '**yes**' if f.in_cycle else ''
            out.append(f'| `{f.filepath}` | {f.fan_in} | {f.fan_out} | {cycle_mark} |')
        out.append('')

    if analyzer.include_cycles:
        out.append(f'## Include cycles ({len(analyzer.include_cycles)} total)\n')
        out.append('Strongly-connected components in the `#include` graph. Any cycle means '
                   'the files in it are mutually dependent — the clearest architectural red '
                   'flag in a C++ codebase.\n')
        for i, cycle in enumerate(analyzer.include_cycles[:10], 1):
            out.append(f'### Cycle {i} ({len(cycle)} files)')
            for path in cycle:
                out.append(f'- `{path}`')
            out.append('')

    # Duplicate clusters — token-normalized Jaccard ≥ threshold. Each cluster is a
    # group of functions that look near-identical at a shape level.
    if analyzer.duplicate_clusters:
        out.append(f'## Near-duplicate functions ({len(analyzer.duplicate_clusters)} cluster(s))\n')
        out.append('Token-normalized Jaccard similarity over 5-gram shingles '
                   f'(threshold >= {int(_DUP_JACCARD_THRESHOLD * 100)}%). Identifiers and '
                   'numbers are normalized, so variables can differ without breaking a match.\n')
        for i, cluster in enumerate(analyzer.duplicate_clusters[:20], 1):
            out.append(f'### Cluster {i} ({len(cluster)} functions)')
            for m in cluster:
                out.append(f'- `{m["file"]}:{m["line"]}` — `{m["name"]}` '
                           f'(score {m["priority_score"]:.1f}, LOC {m["code_lines"]})')
            out.append('')

    # Tech-debt markers
    all_markers = [
        (fm.filepath, m) for fm in analyzer.file_metrics for m in fm.tech_debt_markers
    ]
    if all_markers:
        _mp = {'FIXME': 0, 'HACK': 1, 'XXX': 1, 'TODO': 2}
        all_markers.sort(key=lambda p: (_mp.get(p[1]['marker'], 3), p[0], p[1]['line']))
        out.append(f'## Tech-debt markers ({len(all_markers)} total; showing first 30)\n')
        for filepath, marker in all_markers[:30]:
            text = marker['text'] or ''
            out.append(f'- `{filepath}:{marker["line"]}` **{marker["marker"]}** {text}')
        out.append('')

    # Directory overview — ranked by worst file so one godfile can't hide in a big dir.
    out.append('## Directories (top 15 by worst file)\n')
    out.append('| Directory | Files | LOC | Max priority | P50 priority | Avg CC | Worst file |')
    out.append('| --- | ---: | ---: | ---: | ---: | ---: | --- |')
    dirs = sorted(analyzer.directory_metrics.values(),
                  key=lambda d: getattr(d, 'max_priority', 0.0), reverse=True)[:15]
    for d in dirs:
        max_p = getattr(d, 'max_priority', 0.0)
        p50_p = getattr(d, 'p50_priority', 0.0)
        worst_file = getattr(d, 'worst_file', '')
        out.append(
            f'| `{d.path}` | {d.file_count} | {d.code_lines:,} | '
            f'{max_p:.1f} | {p50_p:.1f} | {d.avg_cyclomatic:.1f} | '
            f'`{worst_file}` |'
        )
    out.append('')

    out.append('## Caveats\n')
    out.append('> ' + JSON_CAVEATS + '\n')

    md_path = output_dir / 'analysis_report.md'
    md_path.write_text('\n'.join(out), encoding='utf-8')
    print(f"  Markdown report exported to: {md_path}")


_HTML_TEMPLATE = '''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Code Complexity Report</title>
<style>
:root {
  --bg: #0f1419;
  --panel: #1b2028;
  --panel-2: #232a34;
  --border: #2f3742;
  --text: #e6edf3;
  --muted: #8b949e;
  --accent: #4393c3;
  --good: #56d364;
  --warn: #e3b341;
  --bad: #f85149;
  --mono: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
}
* { box-sizing: border-box; }
body { margin: 0; font: 14px/1.5 system-ui, -apple-system, sans-serif; background: var(--bg); color: var(--text); }
header { padding: 20px 32px; border-bottom: 1px solid var(--border); background: var(--panel); }
header h1 { margin: 0 0 4px; font-size: 20px; font-weight: 600; }
header .root { font-family: var(--mono); font-size: 12px; color: var(--muted); }
main { padding: 24px 32px; max-width: 1400px; margin: 0 auto; }
.summary { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px; margin-bottom: 32px; }
.card { background: var(--panel); border: 1px solid var(--border); border-radius: 6px; padding: 16px; }
.card .label { font-size: 12px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; }
.card .value { font-size: 28px; font-weight: 600; margin-top: 6px; }
.card.bad .value { color: var(--bad); }
.card.warn .value { color: var(--warn); }
.card.good .value { color: var(--good); }
section { margin-bottom: 40px; }
section h2 { font-size: 18px; font-weight: 600; margin: 0 0 12px; padding-bottom: 8px; border-bottom: 1px solid var(--border); }
section .hint { color: var(--muted); font-size: 13px; margin: 0 0 12px; }
.filter { margin-bottom: 12px; padding: 6px 10px; background: var(--panel); border: 1px solid var(--border); border-radius: 4px; color: var(--text); font-family: inherit; width: 100%; max-width: 420px; }
table { width: 100%; border-collapse: collapse; background: var(--panel); border: 1px solid var(--border); border-radius: 6px; overflow: hidden; }
th, td { padding: 8px 12px; text-align: left; border-bottom: 1px solid var(--border); }
th { background: var(--panel-2); font-weight: 600; font-size: 12px; text-transform: uppercase; letter-spacing: 0.03em; color: var(--muted); cursor: pointer; user-select: none; white-space: nowrap; }
th:hover { color: var(--text); }
th .arrow { opacity: 0.4; font-size: 10px; margin-left: 4px; }
th.sorted .arrow { opacity: 1; color: var(--accent); }
tr:last-child td { border-bottom: none; }
tr:hover td { background: var(--panel-2); }
td.mono, td .path { font-family: var(--mono); font-size: 12px; }
td.num { text-align: right; font-variant-numeric: tabular-nums; }
td.bad { color: var(--bad); font-weight: 600; }
td.warn { color: var(--warn); font-weight: 600; }
td.good { color: var(--good); }
.cluster, .cycle { background: var(--panel); border: 1px solid var(--border); border-radius: 6px; padding: 12px 16px; margin-bottom: 8px; }
.cluster summary, .cycle summary { cursor: pointer; font-weight: 600; }
.cluster ul, .cycle ul { margin: 10px 0 0; padding-left: 20px; }
.cluster li, .cycle li { font-family: var(--mono); font-size: 12px; color: var(--muted); }
.factors { font-family: var(--mono); font-size: 11px; color: var(--muted); margin-top: 2px; }
footer { text-align: center; color: var(--muted); font-size: 12px; padding: 24px 0 32px; }
</style>
</head>
<body>
<header>
  <h1>Code Complexity Report</h1>
  <div class="root" id="root-path"></div>
</header>
<main>
  <section class="summary" id="summary"></section>

  <section>
    <h2>Refactoring queue <span id="hotspot-count"></span></h2>
    <p class="hint">Top function hotspots by composite priority score. Click a column header to re-sort; type in the filter box to narrow the list.</p>
    <input class="filter" id="hotspot-filter" placeholder="Filter by file or function name...">
    <table id="hotspot-table"><thead></thead><tbody></tbody></table>
  </section>

  <section id="llm-section" hidden>
    <h2>AI-suggested refactors <span id="llm-count"></span></h2>
    <p class="hint">Generated by the Anthropic API on the top hotspots. Disk-cached &mdash; re-runs on unchanged code cost nothing.</p>
    <div id="llm-list"></div>
  </section>

  <section id="low-cohesion-section" hidden>
    <h2>Low-cohesion classes <span id="lcom-count"></span></h2>
    <p class="hint">LCOM5 &ge; 0.7 — methods use mostly disjoint subsets of fields. Candidates to split by responsibility.</p>
    <table id="lcom-table"><thead></thead><tbody></tbody></table>
  </section>

  <section id="coupling-section" hidden>
    <h2>Coupling: highest fan-in</h2>
    <p class="hint">Files most widely included. High fan-in = changes here ripple through the tree.</p>
    <table id="fanin-table"><thead></thead><tbody></tbody></table>
  </section>

  <section id="cycles-section" hidden>
    <h2>Include cycles <span id="cycle-count"></span></h2>
    <p class="hint">Strongly-connected components in the <code>#include</code> graph. Any cycle is a direct architectural red flag.</p>
    <div id="cycles-list"></div>
  </section>

  <section id="duplicates-section" hidden>
    <h2>Near-duplicate functions <span id="dup-count"></span></h2>
    <p class="hint">Token-normalized 5-gram Jaccard similarity. Variables can differ without breaking a match.</p>
    <div id="dup-list"></div>
  </section>

  <section id="techdebt-section" hidden>
    <h2>Tech-debt markers <span id="td-count"></span></h2>
    <input class="filter" id="td-filter" placeholder="Filter by file or text...">
    <table id="td-table"><thead></thead><tbody></tbody></table>
  </section>
</main>
<footer>Generated by analyze_code_complexity.py &mdash; this file is self-contained.</footer>
<script>
const data = /*__DATA__*/;

function setText(id, t) { const el = document.getElementById(id); if (el) el.textContent = t; }
function show(id) { const el = document.getElementById(id); if (el) el.hidden = false; }
function el(tag, attrs, children) {
  const n = document.createElement(tag);
  if (attrs) for (const k in attrs) { if (k === 'class') n.className = attrs[k]; else if (k === 'hidden') n.hidden = attrs[k]; else n.setAttribute(k, attrs[k]); }
  if (children) for (const c of children) n.appendChild(typeof c === 'string' ? document.createTextNode(c) : c);
  return n;
}
function scoreClass(v) { return v >= 70 ? 'bad' : v >= 40 ? 'warn' : 'good'; }

// --- Summary ---
setText('root-path', data.root || '');
const s = data.summary || {};
const summaryEl = document.getElementById('summary');
const cards = [
  ['Files', s.total_files, null],
  ['Code lines', (s.total_code_lines || 0).toLocaleString(), null],
  ['High-priority files', `${s.priority_high_count || 0} (${(s.priority_high_pct||0).toFixed(1)}%)`, (s.priority_high_pct || 0) >= 10 ? 'bad' : 'warn'],
  ['P50 file score', s.priority_p50, scoreClass(s.priority_p50 || 0)],
  ['P90 file score', s.priority_p90, scoreClass(s.priority_p90 || 0)],
  ['Max cyclomatic', s.max_cyclomatic, s.max_cyclomatic > 50 ? 'bad' : s.max_cyclomatic > 20 ? 'warn' : 'good'],
];
for (const [label, value, cls] of cards) {
  if (value === undefined || value === null) continue;
  const card = el('div', { class: 'card' + (cls ? ' ' + cls : '') }, [
    el('div', { class: 'label' }, [label]),
    el('div', { class: 'value' }, [String(value)]),
  ]);
  summaryEl.appendChild(card);
}

// --- Sortable table helper ---
function buildTable(tableEl, rows, columns, formatters) {
  const thead = tableEl.querySelector('thead');
  const tbody = tableEl.querySelector('tbody');
  thead.innerHTML = '';
  const hr = el('tr');
  let sortCol = 0, sortAsc = false;
  for (let i = 0; i < columns.length; i++) {
    const th = el('th', {}, [columns[i].label, el('span', { class: 'arrow' }, ['\u25BC'])]);
    th.addEventListener('click', () => {
      if (sortCol === i) sortAsc = !sortAsc; else { sortCol = i; sortAsc = columns[i].numeric ? false : true; }
      render();
    });
    hr.appendChild(th);
  }
  thead.appendChild(hr);
  let filter = '';

  function render() {
    const sorted = rows.slice().filter(r => {
      if (!filter) return true;
      return columns.some(c => String(r[c.key] || '').toLowerCase().includes(filter));
    });
    sorted.sort((a, b) => {
      const av = a[columns[sortCol].key], bv = b[columns[sortCol].key];
      const c = columns[sortCol].numeric ? (av || 0) - (bv || 0) : String(av || '').localeCompare(String(bv || ''));
      return sortAsc ? c : -c;
    });
    // Update header arrow state
    const ths = thead.querySelectorAll('th');
    for (let i = 0; i < ths.length; i++) {
      ths[i].classList.toggle('sorted', i === sortCol);
      ths[i].querySelector('.arrow').textContent = sortCol === i ? (sortAsc ? '\u25B2' : '\u25BC') : '\u25BC';
    }
    tbody.innerHTML = '';
    for (const r of sorted) {
      const tr = el('tr');
      for (const c of columns) {
        const raw = r[c.key];
        const td = el('td', { class: (c.numeric ? 'num ' : '') + (c.mono ? 'mono ' : '') + (c.colour ? c.colour(raw) || '' : '') });
        if (formatters && formatters[c.key]) formatters[c.key](td, raw, r); else td.textContent = raw == null ? '' : String(raw);
        tr.appendChild(td);
      }
      tbody.appendChild(tr);
    }
  }
  render();
  return { setFilter: v => { filter = (v || '').toLowerCase(); render(); } };
}

// --- Hotspots table ---
const hotspots = data.function_hotspots || [];
setText('hotspot-count', `(${hotspots.length})`);
const hotspotTable = buildTable(
  document.getElementById('hotspot-table'),
  hotspots,
  [
    { key: 'priority_score', label: 'Score', numeric: true, colour: v => scoreClass(v) },
    { key: 'file', label: 'File', mono: true },
    { key: 'name', label: 'Function', mono: true },
    { key: 'cyclomatic_complexity', label: 'CC', numeric: true },
    { key: 'cognitive_complexity', label: 'Cog', numeric: true },
    { key: 'max_nesting_depth', label: 'Nest', numeric: true },
    { key: 'code_lines', label: 'LOC', numeric: true },
  ],
  {
    file: (td, v, r) => { td.textContent = `${v}:${r.start_line}`; },
    name: (td, v, r) => {
      td.appendChild(document.createTextNode(v || ''));
      if (r.priority_factors) {
        const pf = Object.entries(r.priority_factors).filter(([,x]) => x > 0).sort((a,b) => b[1]-a[1]);
        if (pf.length) {
          const fact = el('div', { class: 'factors' }, [pf.map(([k,v]) => `${k}=${Math.round(v)}%`).join(' ')]);
          td.appendChild(fact);
        }
      }
    },
  }
);
document.getElementById('hotspot-filter').addEventListener('input', e => hotspotTable.setFilter(e.target.value));

// --- LLM suggestions ---
const llm = data.llm_suggestions || [];
if (llm.length) {
  show('llm-section');
  setText('llm-count', `(${llm.length})`);
  const list = document.getElementById('llm-list');
  for (const entry of llm) {
    const recs = (entry.recommendations || []).map(r => el('li', {}, [
      `${r.approach} (lines ${r.target_lines}): ${r.expected_benefit}`,
    ]));
    const d = el('details', { class: 'cluster' }, [
      el('summary', {}, [`${entry.file}:${entry.line} \u2014 ${entry.name}`]),
    ]);
    if (entry.summary) d.appendChild(el('p', {}, [entry.summary]));
    if (recs.length) d.appendChild(el('ul', {}, recs));
    list.appendChild(d);
  }
}

// --- Low-cohesion classes ---
const lcom = data.low_cohesion_classes || [];
if (lcom.length) {
  show('low-cohesion-section');
  setText('lcom-count', `(${lcom.length})`);
  buildTable(
    document.getElementById('lcom-table'),
    lcom,
    [
      { key: 'lcom5', label: 'LCOM5', numeric: true, colour: v => v >= 0.85 ? 'bad' : 'warn' },
      { key: 'name', label: 'Class', mono: true },
      { key: 'file', label: 'File', mono: true },
      { key: 'method_count', label: 'Methods', numeric: true },
      { key: 'field_count', label: 'Fields', numeric: true },
    ],
    { file: (td, v, r) => { td.textContent = `${v}:${r.start_line}`; } }
  );
}

// --- Coupling fan-in ---
const files = data.all_files || [];
const fanin = files.filter(f => f.fan_in > 0).sort((a,b) => b.fan_in - a.fan_in).slice(0, 30);
if (fanin.length) {
  show('coupling-section');
  buildTable(
    document.getElementById('fanin-table'),
    fanin,
    [
      { key: 'fan_in', label: 'Fan-in', numeric: true, colour: v => v > 50 ? 'bad' : v > 20 ? 'warn' : 'good' },
      { key: 'fan_out', label: 'Fan-out', numeric: true },
      { key: 'filepath', label: 'File', mono: true },
      { key: 'in_cycle', label: 'In cycle', colour: v => v ? 'bad' : '' },
    ],
    { in_cycle: (td, v) => { td.textContent = v ? 'yes' : ''; } }
  );
}

// --- Include cycles ---
const cycles = data.include_cycles || [];
if (cycles.length) {
  show('cycles-section');
  setText('cycle-count', `(${cycles.length})`);
  const list = document.getElementById('cycles-list');
  for (const c of cycles.slice(0, 30)) {
    const d = el('details', { class: 'cycle' }, [
      el('summary', {}, [`Cycle of ${c.length} files`]),
      el('ul', {}, c.map(p => el('li', {}, [p]))),
    ]);
    list.appendChild(d);
  }
}

// --- Duplicate clusters ---
const clusters = data.duplicate_clusters || [];
if (clusters.length) {
  show('duplicates-section');
  setText('dup-count', `(${clusters.length} cluster${clusters.length === 1 ? '' : 's'})`);
  const list = document.getElementById('dup-list');
  for (let i = 0; i < Math.min(clusters.length, 50); i++) {
    const c = clusters[i];
    const items = c.map(m => el('li', {}, [`${m.file}:${m.line} \u2014 ${m.name} (score ${m.priority_score.toFixed(1)}, LOC ${m.code_lines})`]));
    const d = el('details', { class: 'cluster' }, [
      el('summary', {}, [`Cluster ${i + 1}: ${c.length} functions`]),
      el('ul', {}, items),
    ]);
    list.appendChild(d);
  }
}

// --- Tech debt ---
const td = data.tech_debt_markers || [];
if (td.length) {
  show('techdebt-section');
  setText('td-count', `(${td.length})`);
  const tdTable = buildTable(
    document.getElementById('td-table'),
    td,
    [
      { key: 'marker', label: 'Kind', colour: v => v === 'FIXME' || v === 'HACK' ? 'bad' : 'warn' },
      { key: 'file', label: 'File', mono: true },
      { key: 'line', label: 'Line', numeric: true },
      { key: 'text', label: 'Text' },
    ],
    { file: (td, v, r) => { td.textContent = `${v}:${r.line}`; } }
  );
  document.getElementById('td-filter').addEventListener('input', e => tdTable.setFilter(e.target.value));
}
</script>
</body>
</html>
'''


def export_html(analyzer: "CppAnalyzer", output_dir: Path) -> None:
    """
    Export a self-contained HTML dashboard. Embeds the full JSON report inline so
    the file works offline, no CDN / external assets. Uses vanilla JS for sortable
    tables and filter boxes — zero dependencies, reviewer opens the file and it
    works everywhere.

    The embedded JSON is the exact same payload as analysis_report.json so all
    fields are available client-side; the page just chooses which to surface.
    """
    import json
    # Reuse the JSON builder — write to a buffer, parse back, inject into template.
    # Slightly wasteful but keeps the two exports perfectly in sync.
    tmp = output_dir / '_html_data.json'
    export_json(analyzer, output_dir)
    json_path = output_dir / 'analysis_report.json'
    data = json_path.read_text(encoding='utf-8')
    # Add a `root` key for the dashboard header — the JSON report doesn't
    # serialize root_path at the top level, so inject it here.
    parsed = json.loads(data)
    parsed['root'] = str(analyzer.root_path)
    injected = json.dumps(parsed, default=str)
    html = _HTML_TEMPLATE.replace('/*__DATA__*/', injected)
    html_path = output_dir / 'analysis_report.html'
    html_path.write_text(html, encoding='utf-8')
    # Cleanup the unused temp file (never actually created — guard against it anyway).
    if tmp.exists():
        try:
            tmp.unlink()
        except OSError:
            pass
    print(f"  HTML dashboard exported to: {html_path}")


_SARIF_RULES: Dict[str, Dict[str, str]] = {
    'priority-high': {
        'name': 'HighPriorityRefactor',
        'short': 'Function scored >=70 on composite priority index',
        'level': 'warning',
    },
    'nesting-hell': {
        'name': 'NestingHell',
        'short': 'Deep nesting with few branches — flatten with guard clauses',
        'level': 'warning',
    },
    'very-long-function': {
        'name': 'VeryLongFunction',
        'short': 'Function exceeds very-long-function threshold',
        'level': 'warning',
    },
    'long-function': {
        'name': 'LongFunction',
        'short': 'Function exceeds long-function threshold',
        'level': 'note',
    },
    'big-switch': {
        'name': 'BigSwitch',
        'short': 'Large switch — consider dispatch table or polymorphism',
        'level': 'note',
    },
    'complex-branching': {
        'name': 'ComplexBranching',
        'short': 'High cyclomatic complexity — too many branches',
        'level': 'warning',
    },
    'hard-to-follow': {
        'name': 'HardToFollow',
        'short': 'High cognitive complexity',
        'level': 'warning',
    },
    'many-params': {
        'name': 'ManyParameters',
        'short': 'Long parameter list — introduce a parameter object',
        'level': 'note',
    },
    'low-cohesion': {
        'name': 'LowCohesion',
        'short': 'Class has LCOM5 >= 0.7; methods use disjoint fields',
        'level': 'warning',
    },
    'include-cycle': {
        'name': 'IncludeCycle',
        'short': 'File participates in an #include cycle',
        'level': 'error',
    },
    'near-duplicate': {
        'name': 'NearDuplicate',
        'short': 'Near-duplicate function detected (Jaccard >= 85%)',
        'level': 'note',
    },
}


def export_sarif(analyzer: CppAnalyzer, output_dir: Path) -> None:
    """
    Export findings in SARIF 2.1.0 format so IDEs and GitHub Code Scanning can
    consume them. Findings:
      - one per function hotspot with priority_score >= 70 (rule: priority-high)
      - one per tagged function per tag (rule: matching tag name)
      - one per low-cohesion class (rule: low-cohesion)
      - one per include-cycle member (rule: include-cycle)
      - one per duplicate cluster leader (rule: near-duplicate)

    SARIF's `ruleId` maps to `_SARIF_RULES`; each rule carries a level and short
    description. Absolute paths are emitted only when the analyzer's filepaths
    are already absolute; otherwise a relative `uri` is used with `uriBaseId`
    pointing at the analysis root.
    """
    import json

    rules = []
    for rule_id, info in _SARIF_RULES.items():
        rules.append({
            'id': rule_id,
            'name': info['name'],
            'shortDescription': {'text': info['short']},
            'defaultConfiguration': {'level': info['level']},
        })

    def _location(path: str, line: int) -> Dict:
        return {
            'physicalLocation': {
                'artifactLocation': {'uri': path.replace('\\', '/'), 'uriBaseId': 'SRCROOT'},
                'region': {'startLine': max(1, int(line or 1))},
            }
        }

    results: List[Dict] = []

    # High-priority functions + per-tag findings.
    for fm in analyzer.function_metrics:
        if fm.priority_score >= 70:
            results.append({
                'ruleId': 'priority-high',
                'level': _SARIF_RULES['priority-high']['level'],
                'message': {
                    'text': (
                        f'{fm.name}: priority {fm.priority_score:.1f} '
                        f'(CC={fm.cyclomatic_complexity}, cog={fm.cognitive_complexity}, '
                        f'nesting={fm.max_nesting_depth}, LOC={fm.code_lines})'
                    )
                },
                'locations': [_location(fm.filepath, fm.start_line)],
            })
        for tag in fm.tags:
            if tag not in _SARIF_RULES:
                continue
            results.append({
                'ruleId': tag,
                'level': _SARIF_RULES[tag]['level'],
                'message': {'text': f'{fm.name}: {_SARIF_RULES[tag]["short"]}'},
                'locations': [_location(fm.filepath, fm.start_line)],
            })

    # Low-cohesion classes.
    for fm in analyzer.file_metrics:
        for cls in fm.classes:
            if cls['lcom5'] >= 0.7 and cls['method_count'] >= 3 and cls['field_count'] >= 3:
                results.append({
                    'ruleId': 'low-cohesion',
                    'level': _SARIF_RULES['low-cohesion']['level'],
                    'message': {
                        'text': (
                            f'{cls["name"]}: LCOM5={cls["lcom5"]:.2f}, '
                            f'{cls["method_count"]} methods, {cls["field_count"]} fields'
                        )
                    },
                    'locations': [_location(fm.filepath, cls['start_line'])],
                })

    # Include cycles — one finding per file in each cycle.
    for cycle in analyzer.include_cycles:
        msg = f'In include cycle with {len(cycle)} file(s): ' + ', '.join(Path(p).name for p in cycle)
        for path in cycle:
            results.append({
                'ruleId': 'include-cycle',
                'level': _SARIF_RULES['include-cycle']['level'],
                'message': {'text': msg},
                'locations': [_location(path, 1)],
            })

    # Duplicate clusters — one finding on each member, tagged with leader ref.
    for cluster in analyzer.duplicate_clusters:
        if len(cluster) < 2:
            continue
        leader = cluster[0]
        msg = (
            f'Near-duplicate of {leader["name"]} at '
            f'{leader["file"]}:{leader["line"]} — cluster size {len(cluster)}'
        )
        for member in cluster:
            results.append({
                'ruleId': 'near-duplicate',
                'level': _SARIF_RULES['near-duplicate']['level'],
                'message': {'text': msg},
                'locations': [_location(member['file'], member['line'])],
            })

    sarif = {
        '$schema': 'https://json.schemastore.org/sarif-2.1.0.json',
        'version': '2.1.0',
        'runs': [{
            'tool': {
                'driver': {
                    'name': 'analyze_code_complexity',
                    'version': JSON_SCHEMA_VERSION,
                    'informationUri': 'https://sarifweb.azurewebsites.net/',
                    'rules': rules,
                }
            },
            'originalUriBaseIds': {
                'SRCROOT': {'uri': str(analyzer.root_path).replace('\\', '/') + '/'}
            },
            'results': results,
        }],
    }

    sarif_path = output_dir / 'analysis_report.sarif'
    sarif_path.write_text(json.dumps(sarif, indent=2, default=str), encoding='utf-8')
    print(f"  SARIF report exported to: {sarif_path}  ({len(results)} finding(s))")


_LLM_SYSTEM_PROMPT = """You are a senior C++ engineer reviewing code for refactoring.

You will be given a single function from a C++ codebase along with complexity metrics.
Return a concise refactor recommendation as structured JSON.

Rules:
- Be concrete. Point at specific line ranges and patterns when possible.
- Favour guard clauses / early returns, extract-method, strategy pattern, parameter objects.
- Avoid generic advice like "flatten nesting" without saying how.
- Prefer 1-3 high-value recommendations over a long list of minor ones.
- If the function is complex because the domain is genuinely complex (state machine, parser,
  codegen), say so and suggest only tractable changes.
- Your output MUST be valid JSON matching the schema — no prose, no markdown fences.
"""


_LLM_OUTPUT_SCHEMA: Dict = {
    "type": "object",
    "properties": {
        "summary": {
            "type": "string",
            "description": "One sentence describing the core problem with this function.",
        },
        "recommendations": {
            "type": "array",
            "description": "1-3 concrete refactor steps, ordered by impact.",
            "items": {
                "type": "object",
                "properties": {
                    "approach": {
                        "type": "string",
                        "description": "Specific refactor technique (e.g. 'extract method', 'guard clauses').",
                    },
                    "target_lines": {
                        "type": "string",
                        "description": "Line range in the source (e.g. '142-178') or 'whole function'.",
                    },
                    "expected_benefit": {
                        "type": "string",
                        "description": "What gets easier to read, test, or change.",
                    },
                },
                "required": ["approach", "target_lines", "expected_benefit"],
                "additionalProperties": False,
            },
        },
    },
    "required": ["summary", "recommendations"],
    "additionalProperties": False,
}


def _read_function_source(root_path: Path, func: FunctionMetrics) -> Optional[str]:
    """
    Read the function's source text from the file. Returns lines [start_line, end_line]
    joined, or None if the file can't be read or the range is invalid.
    """
    try:
        file_path = Path(func.filepath)
        if not file_path.is_absolute():
            file_path = root_path / file_path
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except OSError:
        return None
    start = max(0, func.start_line - 1)
    end = min(len(lines), func.end_line)
    if end <= start:
        return None
    return ''.join(lines[start:end])


def generate_llm_suggestions(
    analyzer: "CppAnalyzer",
    root_path: Path,
    max_functions: int,
    model: str,
    cache_path: Optional[Path],
) -> List[Dict]:
    """
    Call the Anthropic API once per top-N function hotspot to get a concrete refactor
    recommendation. Requires `ANTHROPIC_API_KEY` in the environment and the `anthropic`
    package. Returns an empty list and prints a warning if either is missing.

    Design choices:
      - Sync calls (small responses, no benefit from streaming at max_tokens=2048).
      - Prompt caching on the system block: identical across all N calls. On Sonnet 4.6
        the minimum cacheable prefix is ~2K tokens, so a short prompt silently won't
        cache — no error, just no savings. The hook is in place if the prompt grows.
      - Adaptive thinking enabled: helps on complex-nesting cases, costs little on
        simple ones.
      - Disk-backed response cache keyed by SHA-256 of (filepath + source text). If the
        function's source hasn't changed, we skip the API call. Delete the cache file
        to force a re-run.
      - Structured outputs via `output_config.format.json_schema` — response is
        guaranteed to match the schema, no fragile regex parsing.
    """
    if not os.environ.get('ANTHROPIC_API_KEY'):
        print("Warning: ANTHROPIC_API_KEY not set. Skipping --llm-suggestions.")
        return []
    try:
        import anthropic
    except ImportError:
        print("Warning: anthropic SDK not installed. Skipping --llm-suggestions. "
              "Install with: pip install anthropic")
        return []

    import hashlib
    import json as _json

    cache: Dict[str, Dict] = {}
    if cache_path and cache_path.exists():
        try:
            cache = _json.loads(cache_path.read_text(encoding='utf-8'))
        except (OSError, _json.JSONDecodeError):
            cache = {}

    client = anthropic.Anthropic()

    hotspots = sorted(
        analyzer.function_metrics,
        key=lambda f: -f.priority_score,
    )[:max_functions]

    if not hotspots:
        return []

    print(f"  Requesting LLM refactor suggestions for {len(hotspots)} function(s)...")

    results: List[Dict] = []
    for i, fm in enumerate(hotspots, 1):
        source = _read_function_source(root_path, fm)
        if source is None:
            print(f"  [{i}/{len(hotspots)}] skip {fm.name}: unable to read source")
            continue

        source_hash = hashlib.sha256(
            (fm.filepath + '\n' + source).encode('utf-8', errors='ignore')
        ).hexdigest()
        cache_key = f"{model}:{source_hash}"

        if cache_key in cache:
            print(f"  [{i}/{len(hotspots)}] cache hit: {fm.name}")
            results.append(cache[cache_key])
            continue

        tag_str = ', '.join(fm.tags) if fm.tags else 'none'
        user_prompt = (
            f"File: {fm.filepath}\n"
            f"Function: {fm.name}  (lines {fm.start_line}-{fm.end_line})\n"
            f"Metrics: CC={fm.cyclomatic_complexity}, cognitive={fm.cognitive_complexity}, "
            f"nesting={fm.max_nesting_depth}, LOC={fm.code_lines}, params={fm.param_count}\n"
            f"Anti-pattern tags: {tag_str}\n\n"
            f"Source:\n```cpp\n{source}\n```\n\n"
            f"Return a JSON refactor recommendation matching the schema."
        )

        try:
            response = client.messages.create(
                model=model,
                max_tokens=2048,
                system=[
                    {
                        "type": "text",
                        "text": _LLM_SYSTEM_PROMPT,
                        "cache_control": {"type": "ephemeral"},
                    }
                ],
                messages=[{"role": "user", "content": user_prompt}],
                output_config={
                    "format": {
                        "type": "json_schema",
                        "schema": _LLM_OUTPUT_SCHEMA,
                    }
                },
                thinking={"type": "adaptive"},
            )
        except anthropic.RateLimitError:
            print(f"  [{i}/{len(hotspots)}] rate limited, stopping early.")
            break
        except anthropic.APIError as e:
            print(f"  [{i}/{len(hotspots)}] API error on {fm.name}: {e}")
            continue

        text_block = next((b for b in response.content if getattr(b, 'type', '') == 'text'), None)
        if not text_block:
            print(f"  [{i}/{len(hotspots)}] no text block returned for {fm.name}")
            continue

        try:
            parsed = _json.loads(text_block.text)
        except _json.JSONDecodeError as e:
            print(f"  [{i}/{len(hotspots)}] JSON parse failed for {fm.name}: {e}")
            continue

        entry = {
            'file': fm.filepath,
            'line': fm.start_line,
            'name': fm.name,
            'priority_score': fm.priority_score,
            'summary': parsed.get('summary', ''),
            'recommendations': parsed.get('recommendations', []),
        }
        results.append(entry)
        cache[cache_key] = entry
        print(f"  [{i}/{len(hotspots)}] got suggestion: {fm.name}")

    if cache_path:
        try:
            cache_path.parent.mkdir(parents=True, exist_ok=True)
            cache_path.write_text(_json.dumps(cache, indent=2), encoding='utf-8')
        except OSError as e:
            print(f"  Warning: couldn't write LLM cache to {cache_path}: {e}")

    return results


def check_fail_on(analyzer: CppAnalyzer, thresholds: Dict[str, float]) -> List[str]:
    """
    Return a list of violation messages for functions exceeding the given thresholds.
    Empty list means no violations. Used to gate CI on quality regressions.
    """
    violations: List[str] = []
    max_cc = thresholds.get('max-cc')
    max_cog = thresholds.get('max-cognitive')
    max_nesting = thresholds.get('max-nesting')
    max_loc = thresholds.get('max-loc')
    for fm in analyzer.function_metrics:
        ref = f'{fm.filepath}:{fm.start_line} {fm.name}'
        if max_cc is not None and fm.cyclomatic_complexity > max_cc:
            violations.append(f'{ref}: CC={fm.cyclomatic_complexity} exceeds max-cc={int(max_cc)}')
        if max_cog is not None and fm.cognitive_complexity > max_cog:
            violations.append(f'{ref}: cognitive={fm.cognitive_complexity} exceeds max-cognitive={int(max_cog)}')
        if max_nesting is not None and fm.max_nesting_depth > max_nesting:
            violations.append(f'{ref}: nesting={fm.max_nesting_depth} exceeds max-nesting={int(max_nesting)}')
        if max_loc is not None and fm.code_lines > max_loc:
            violations.append(f'{ref}: LOC={fm.code_lines} exceeds max-loc={int(max_loc)}')
    return violations


def _parse_fail_on(raw: str) -> Dict[str, float]:
    """Parse `--fail-on max-cc=30,max-nesting=8` into a dict."""
    out: Dict[str, float] = {}
    if not raw:
        return out
    for item in raw.split(','):
        item = item.strip()
        if not item:
            continue
        if '=' not in item:
            raise ValueError(f"Expected KEY=VALUE in --fail-on, got: {item!r}")
        k, v = item.split('=', 1)
        out[k.strip()] = float(v.strip())
    return out


def main():
    parser = argparse.ArgumentParser(
        description='Analyze C/C++ codebase for complexity and maintainability metrics'
    )
    parser.add_argument(
        'path',
        nargs='?',
        default='.',
        help='Root path of the codebase to analyze (default: current directory)'
    )
    parser.add_argument(
        '-e', '--exclude',
        nargs='+',
        default=['Middleware', 'ThirdParty', 'External', 'vendor', 'build', 'Build', '.git', '.claude'],
        help='Directories to exclude from analysis'
    )
    parser.add_argument(
        '-o', '--output',
        default='complexity_report',
        help='Output directory for visualizations and CSV files'
    )
    parser.add_argument(
        '--no-viz',
        action='store_true',
        help='Skip generating visualizations'
    )
    parser.add_argument(
        '--csv',
        action='store_true',
        help='Export metrics to CSV files'
    )
    parser.add_argument(
        '--no-json',
        action='store_true',
        help='Skip generating JSON report (generated by default)'
    )
    parser.add_argument(
        '--no-md',
        action='store_true',
        help='Skip generating markdown report (generated by default)'
    )
    parser.add_argument(
        '--html',
        action='store_true',
        help='Emit a self-contained interactive HTML dashboard (analysis_report.html). '
             'Zero external dependencies — opens in any browser.'
    )
    parser.add_argument(
        '--sarif',
        action='store_true',
        help='Emit a SARIF 2.1.0 findings report (analysis_report.sarif) for IDE / '
             'GitHub Code Scanning integration.'
    )
    parser.add_argument(
        '--thresholds',
        metavar='PATH',
        help='Path to a JSON file overriding tag thresholds (see DEFAULT_THRESHOLDS)'
    )
    parser.add_argument(
        '--ignore-macros',
        nargs='+',
        metavar='NAME',
        default=[],
        help='Macro names whose balanced MACRO(...) invocations are stripped before '
             'CC/cognitive scanning. Useful for assertion macros (e.g. --ignore-macros '
             'ZENITH_ASSERT assert).'
    )
    parser.add_argument(
        '--fail-on',
        metavar='RULES',
        help='Comma-separated thresholds that cause a non-zero exit when exceeded by any '
             'function. Keys: max-cc, max-cognitive, max-nesting, max-loc. '
             'Example: --fail-on max-cc=30,max-nesting=8'
    )
    parser.add_argument(
        '--exclude-generated',
        action='store_true',
        help='Skip files that look auto-generated (.pb.cc, .generated.*, banner markers).'
    )
    parser.add_argument(
        '--absolute-paths',
        action='store_true',
        help='Emit absolute filepaths in JSON (useful for consumers like Claude Code).'
    )
    parser.add_argument(
        '--workers',
        type=int,
        default=0,
        help='Number of worker processes for file analysis. 0 (default) = auto (parallel '
             'for >=200 files). 1 = force sequential.'
    )
    parser.add_argument(
        '--parser',
        choices=['regex', 'tree-sitter'],
        default='regex',
        help='Parser backend for function / class detection. "regex" (default) is '
             'stdlib-only and approximate. "tree-sitter" is more accurate on nested '
             'templates, C++20 `requires`, function-try-blocks, and macro-generated '
             'signatures; requires `pip install tree-sitter tree-sitter-cpp`.'
    )
    parser.add_argument(
        '--llm-suggestions',
        action='store_true',
        help='Call the Anthropic API to generate concrete refactor recommendations '
             'for the top-N function hotspots. Requires `pip install anthropic` and '
             'ANTHROPIC_API_KEY in the environment. Results are cached on disk so '
             'repeat runs on unchanged code cost nothing.'
    )
    parser.add_argument(
        '--llm-max',
        type=int,
        default=5,
        metavar='N',
        help='Max number of functions to request LLM suggestions for (default 5). '
             'Ignored unless --llm-suggestions is set.'
    )
    parser.add_argument(
        '--llm-model',
        default=DEFAULT_LLM_MODEL,
        metavar='MODEL',
        help=f'Anthropic model ID to use for LLM suggestions (default {DEFAULT_LLM_MODEL}). '
             'Ignored unless --llm-suggestions is set.'
    )

    args = parser.parse_args()

    root_path = Path(args.path).resolve()
    output_dir = Path(args.output).resolve()

    # Load optional threshold overrides.
    threshold_overrides: Optional[Dict[str, float]] = None
    if args.thresholds:
        import json
        with open(args.thresholds, 'r', encoding='utf-8') as f:
            threshold_overrides = json.load(f)
        print(f"Loaded threshold overrides from: {args.thresholds}")

    fail_on_rules = _parse_fail_on(args.fail_on) if args.fail_on else {}

    print(f"Analyzing codebase at: {root_path}")
    print(f"Excluding directories: {', '.join(args.exclude)}")
    if args.ignore_macros:
        print(f"Ignoring macros: {', '.join(args.ignore_macros)}")
    if args.exclude_generated:
        print("Skipping generated files.")

    analyzer = CppAnalyzer(
        str(root_path),
        exclude_dirs=args.exclude,
        thresholds=threshold_overrides,
        ignore_macros=args.ignore_macros,
        exclude_generated=args.exclude_generated,
        parser_backend=args.parser,
    )
    if analyzer.parser_backend == 'tree-sitter':
        print("Using tree-sitter parser.")
    analyzer.analyze(workers=args.workers)

    if args.absolute_paths:
        # Rewrite relative paths to absolute; downstream JSON / markdown will pick this up.
        for fm in analyzer.file_metrics:
            fm.filepath = str((root_path / fm.filepath).resolve())
        for fm in analyzer.function_metrics:
            fm.filepath = str((root_path / fm.filepath).resolve())

    if args.llm_suggestions:
        output_dir.mkdir(parents=True, exist_ok=True)
        cache_path = output_dir / 'llm_cache.json'
        analyzer.llm_suggestions = generate_llm_suggestions(
            analyzer,
            root_path,
            max_functions=args.llm_max,
            model=args.llm_model,
            cache_path=cache_path,
        )

    analyzer.print_report()

    if not args.no_json:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting JSON report...")
        export_json(analyzer, output_dir)

    if not args.no_md:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting markdown report...")
        export_markdown(analyzer, output_dir)

    if args.html:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting HTML dashboard...")
        export_html(analyzer, output_dir)

    if args.sarif:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting SARIF report...")
        export_sarif(analyzer, output_dir)

    if args.csv:
        output_dir.mkdir(parents=True, exist_ok=True)
        print("\nExporting CSV files...")
        export_csv(analyzer, output_dir)

    if not args.no_viz and HAS_MATPLOTLIB:
        output_dir.mkdir(parents=True, exist_ok=True)
        visualizer = MetricsVisualizer(analyzer, str(output_dir))
        visualizer.create_all_visualizations()

    # --fail-on check runs last so we still emit reports before failing.
    exit_code = 0
    if fail_on_rules:
        violations = check_fail_on(analyzer, fail_on_rules)
        if violations:
            print(f"\nFAIL: {len(violations)} function(s) exceed --fail-on thresholds:")
            for v in violations[:20]:
                print(f"  {v}")
            if len(violations) > 20:
                print(f"  ... and {len(violations) - 20} more")
            exit_code = 1

    print("\n--- Analysis complete! ---")

    if exit_code:
        sys.exit(exit_code)
    return analyzer


if __name__ == '__main__':
    main()
