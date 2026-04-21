# analyze_code_complexity.py

A snapshot-only code complexity analyzer for C/C++ codebases. Surfaces the
worst-offending functions, classes, and files as a prioritized refactoring
queue — with concrete `file:line` references so a human (or Claude) can act on
the output without re-exploring the tree.

The tool treats the codebase as a point-in-time snapshot: it does not read
`git log`, does not compare across runs, and has no baseline/regression mode.
All signal comes from the current source.

## Requirements

- Python 3.9+ (standard library only, for the core analyzer)
- Optional: `matplotlib` + `numpy` for PNG visualizations (skip with `--no-viz`)
- Optional: `tree-sitter` + `tree-sitter-cpp` for the accurate parser backend (`--parser tree-sitter`)
- Optional: `anthropic` + an API key for AI refactor suggestions (`--llm-suggestions`)

```bash
pip install matplotlib numpy                 # PNG charts
pip install tree-sitter tree-sitter-cpp      # accurate parser
pip install anthropic                        # AI suggestions
export ANTHROPIC_API_KEY=sk-ant-...          # AI suggestions (Unix)
```

## Quickstart

From the repo root:

```bash
# Text report + JSON + Markdown (default)
python Tools/analyze_code_complexity.py .

# Everything at once: HTML dashboard, SARIF, CSV, charts
python Tools/analyze_code_complexity.py . --html --sarif --csv -o complexity_report

# CI gate: fail if any function exceeds the given thresholds
python Tools/analyze_code_complexity.py . --fail-on max-cc=60,max-nesting=8

# Skip assertion macros so ZENITH_ASSERT(x && y) doesn't inflate complexity
python Tools/analyze_code_complexity.py . --ignore-macros ZENITH_ASSERT

# Use tree-sitter for accurate parsing (handles nested templates, macros, C++20 requires)
python Tools/analyze_code_complexity.py . --parser tree-sitter

# Get AI-generated refactor recommendations for the top 10 hotspots
python Tools/analyze_code_complexity.py . --llm-suggestions --llm-max 10
```

## CLI reference

### Positional

| Argument | Description |
|---|---|
| `path` | Root path of the codebase to analyze. Default `.` (current directory). |

### Input selection

| Flag | Description |
|---|---|
| `-e`, `--exclude DIR [DIR ...]` | Directory names to skip. Default: `Middleware ThirdParty External vendor build Build .git .claude`. Matches any path segment. |
| `--exclude-generated` | Skip files that look auto-generated (`.pb.cc`, `.generated.*`, `DO NOT EDIT` banners in the first 5 lines). |
| `--ignore-macros NAME [NAME ...]` | Macro names whose balanced `MACRO(...)` invocations are stripped before complexity scanning. Useful for assertion macros. Example: `--ignore-macros ZENITH_ASSERT assert`. |

### Output selection

By default the tool emits `analysis_report.json` and `analysis_report.md` into
`-o`. All other outputs are opt-in.

| Flag | Description |
|---|---|
| `-o`, `--output DIR` | Output directory. Default `complexity_report`. Created if it doesn't exist. |
| `--no-json` | Skip the JSON report. |
| `--no-md` | Skip the Markdown report. |
| `--no-viz` | Skip the PNG chart suite. Charts require matplotlib; if it's missing they're always skipped. |
| `--html` | Emit a self-contained HTML dashboard (`analysis_report.html`). Zero external dependencies — opens in any browser, sortable tables, filter boxes. |
| `--sarif` | Emit a SARIF 2.1.0 findings report (`analysis_report.sarif`) for IDE integration (VS Code, GitHub Code Scanning, etc.). |
| `--csv` | Export file / directory / function metrics as CSV. |
| `--absolute-paths` | Emit absolute filepaths in JSON (useful when downstream consumers need resolved paths). |

### Tuning

| Flag | Description |
|---|---|
| `--thresholds PATH` | Path to a JSON file overriding the anti-pattern thresholds (see **Thresholds** below). |
| `--workers N` | Number of worker processes. `0` (default) = auto-parallel for ≥200 files. `1` = force sequential. |
| `--parser {regex,tree-sitter}` | Parser backend. `regex` (default) is stdlib-only and approximate; `tree-sitter` is accurate on nested templates, C++20 `requires`, function-try-blocks, and most macro-generated signatures. Requires `pip install tree-sitter tree-sitter-cpp`. Falls back to regex with a warning if the deps aren't installed. |

### AI refactor suggestions

| Flag | Description |
|---|---|
| `--llm-suggestions` | Call the Anthropic API to generate one structured refactor recommendation per top-N hotspot. Emits a new "AI-suggested refactors" section in the markdown, JSON (`llm_suggestions`), and HTML dashboard. Requires `pip install anthropic` and `ANTHROPIC_API_KEY` in the environment. |
| `--llm-max N` | Max number of functions to request suggestions for (default `5`). Each function = one API call. |
| `--llm-model MODEL` | Anthropic model ID (default `claude-sonnet-4-6`). Override with e.g. `--llm-model claude-opus-4-7` for higher-quality (and more expensive) suggestions. |

Results are disk-cached in `<output>/llm_cache.json`, keyed by `<model>:<sha256 of (filepath + source)>`. Re-running on unchanged code is free (cache hit → zero API calls). Delete the cache file to force a full re-request.

### CI gating

| Flag | Description |
|---|---|
| `--fail-on RULES` | Comma-separated thresholds that cause a non-zero exit when *any* function exceeds them. Keys: `max-cc`, `max-cognitive`, `max-nesting`, `max-loc`. Example: `--fail-on max-cc=30,max-nesting=8`. |

## What the tool produces

### Default outputs (always)

- **`analysis_report.json`** — Full machine-readable report. Key sections:
  - `summary` — priority-score distribution (high/medium/low counts, P50/P90)
  - `function_hotspots` — top 50 functions by composite priority score
  - `low_cohesion_classes` — classes with LCOM5 ≥ 0.7
  - `duplicate_clusters` — near-duplicate function groups
  - `include_cycles` — strongly-connected components in the `#include` graph
  - `tech_debt_markers` — TODO/FIXME/HACK/XXX locations (capped at 500)
  - `top_priority_files`, `top_complex_files`, `deepest_nesting_files`, `largest_files`
  - `directories` — per-directory metrics ranked by worst file
  - `all_files` — full per-file metrics
- **`analysis_report.md`** — Human-readable markdown with the refactoring queue,
  tagged files, low-cohesion classes, coupling table, cycles, duplicates,
  tech-debt markers, and directory overview. Each hotspot includes a concrete
  "extract lines X–Y" suggestion pointing at its worst nested block.

### Opt-in outputs

- **`analysis_report.html`** (`--html`) — Self-contained dashboard with sortable
  filterable tables. Embeds the full JSON, so it works offline.
- **`analysis_report.sarif`** (`--sarif`) — SARIF 2.1.0 findings with rules for
  `priority-high`, each anti-pattern tag, `low-cohesion`, `include-cycle`, and
  `near-duplicate`.
- **`file_metrics.csv` / `directory_metrics.csv` / `function_metrics.csv`** (`--csv`)
- **12 PNG charts** (default unless `--no-viz`): directory comparison, complexity
  distributions, maintainability heatmap, LOC breakdown, Halstead metrics, risk
  quadrant, nesting depth, comment ratio, file size distribution, dashboard.

## Metrics glossary

| Metric | What it means | Range |
|---|---|---|
| **Cyclomatic complexity (CC)** | McCabe: independent paths through code. Each `if`/`for`/`while`/`case`/`catch`/`&&`/`||`/`?:` adds 1. | 1+, <10 is simple, >20 is complex |
| **Cognitive complexity** | SonarSource: human effort to understand. Each control-flow keyword costs `1 + current_nesting`; logical ops each add 1. | 0+, <15 is clear, >25 is hard |
| **Max nesting depth** | Deepest control-flow block nesting inside the function/file. | 0+, >4 is painful |
| **Priority score** | Composite 0–100. 30% cognitive + 25% CC + 20% nesting + 15% length + 10% params, each soft-capped. The tool ranks by this. | 0–100. 0 = clean, ≥70 = priority refactoring target |
| **LCOM5** | Henderson-Sellers class cohesion. `1 - (method/field touches) / (methods × fields)`. Uses the engine's `m_` convention to identify fields. | 0.0–1.0. Near 1 = methods use disjoint fields → split by responsibility |
| **Fan-in** | Number of files that `#include` this file. High fan-in = change risk. | |
| **Fan-out** | Number of local headers this file includes. | |
| **Jaccard similarity (duplicate detection)** | `|A ∩ B| / |A ∪ B|` over 5-gram token shingles, with identifiers/numbers normalized. Threshold 85%. | 0.0–1.0 |
| **Halstead volume / difficulty** | Operator/operand counts. Kept as secondary metrics only — not used as headline signal because the 1977-era "bugs delivered" and "time to program" constants don't guide refactoring decisions. | |
| **Maintainability Index (MI)** | Classic 1994 formula. Retained in the JSON for tooling that expects it but deliberately *not* the headline — its resolution collapses at the bottom (MI=0 for every bad file). | 0–100 |

## Anti-pattern tags

Functions get tagged when they cross a threshold. Tags drive the SARIF rule IDs
and the markdown "Tagged files" table.

| Tag | Rule |
|---|---|
| `long-function` | code_lines > 80 (default) |
| `very-long-function` | code_lines > 200 |
| `deep-nesting` | max_nesting_depth ≥ 5 |
| `complex-branching` | cyclomatic_complexity ≥ 15 |
| `hard-to-follow` | cognitive_complexity ≥ 25 |
| `many-params` | param_count ≥ 6 |
| `many-returns` | return_count ≥ 6 |
| `big-switch` | case_count ≥ 10 AND max_nesting_depth ≤ 3 |
| `nesting-hell` | max_nesting_depth ≥ 5 AND case_count < 5 |
| `low-mi` | maintainability_index < 40 (file-level) |
| `god-file` | code_lines > 1000 OR function_count > 40 (file-level) |

### Thresholds

Override any default by passing a JSON file to `--thresholds`. Full key list
(with defaults):

```json
{
    "long_function_loc": 80,
    "very_long_function_loc": 200,
    "deep_nesting": 5,
    "complex_branching_cc": 15,
    "hard_to_follow_cognitive": 25,
    "many_params": 6,
    "many_returns": 6,
    "big_switch_cases": 10,
    "big_switch_max_nesting": 3,
    "nesting_hell_depth": 5,
    "nesting_hell_max_cases": 5,
    "low_mi": 40,
    "god_file_loc": 1000,
    "god_file_funcs": 40,
    "extract_candidate_depth": 4,
    "extract_candidate_min_lines": 15
}
```

## Tests

A fixture-based unittest suite lives in [tests/test_complexity.py](tests/test_complexity.py).

```bash
cd <repo>
python -m unittest Tools.tests.test_complexity -v
```

Covers: cyclomatic, cognitive, nesting, raw-string handling, macro stripping,
function detection, priority scoring (factor breakdown), duplicate detection,
include-graph cycle detection, directory aggregates, LCOM5 cohesion, and
percentile helpers.

## Accuracy caveats

The default (`--parser regex`) is stdlib-only and approximate. The following
constructs may cause functions to be missed:

- Function-try-blocks (`void f() try { ... } catch (...) { ... }`)
- C++20 `requires` clauses in complex positions
- Macro-generated signatures (`DEFINE_ENTITY_COMPONENT(Foo)`)
- Deeply nested template arg lists (`std::vector<std::pair<int, std::map<K, V>>>`)
- `throw(...)` exception specifications

**Use `--parser tree-sitter` to close these gaps** if you have the deps installed
(`pip install tree-sitter tree-sitter-cpp`). Tree-sitter parses the full C++
grammar, so it handles nested templates, operator overloads, destructors, and
function-try-blocks correctly.

File-level metrics (total lines, CC across the file, cognitive complexity,
Halstead) are unaffected by parser choice — they don't depend on function
boundary detection. A missed function just doesn't appear in the per-function
hotspot list; the file it lives in still shows its aggregate complexity.

The nesting detector intentionally does *not* treat function bodies, lambda
bodies, or initializer lists as control-flow nesting, even though they open a
`{` after `)`. Earlier versions of the tool had this bug — it's documented
here so future contributors don't "fix" the working case.

## Design principle: snapshot only

The tool deliberately avoids any cross-run or git-history integration. No
`--baseline` flag, no churn multiplier, no time-series. If you want trend
tracking, run the tool in CI and diff the JSONs yourself — but the tool itself
treats the current source as the only input.
