# analyze_code_complexity.py

A snapshot-only code complexity analyzer for C/C++ codebases. Surfaces the
worst-offending functions, classes, and files as a prioritized refactoring
queue -- with concrete `file:line` references so a human (or Claude) can act on
the output without re-exploring the tree.

The tool treats the codebase as a point-in-time snapshot: it does not read
`git log`, does not compare across runs, and has no baseline/regression mode.
All signal comes from the current source.

## Out of scope

The following capabilities are *deliberately* absent and will not be added:

- **No baseline / churn / git history.** No `--baseline` flag, no churn
  multiplier, no time-series. If you want trend tracking, run the tool in CI
  and diff the JSONs yourself.
- **No cross-run state.** The tool reads only the current source.
- **No aspirational quality bars.** CI thresholds are checked-in *ceilings* of
  the current worst-case code, not targets the codebase already meets.

## Requirements

- Python 3.9+ (standard library only, for the core analyzer)
- Optional: `matplotlib` + `numpy` for PNG visualizations (skip with `--no-viz`)
- Optional: `tree-sitter` + `tree-sitter-cpp` for the accurate parser backend
  (auto-selected by `--parser auto` when installed)
- Optional: `anthropic` + an API key for AI refactor suggestions
  (`--llm-suggestions`)

```bash
pip install matplotlib numpy                 # PNG charts
pip install tree-sitter tree-sitter-cpp      # accurate parser
pip install anthropic                        # AI suggestions
export ANTHROPIC_API_KEY=sk-ant-...          # AI suggestions (Unix)
```

## Quickstart -- wrapper scripts

Two batch wrappers ship next to the analyzer. Use them rather than the raw
`python` command unless you need a custom configuration.

### `Tools\RunAnalysis.bat` -- full-feature snapshot

Runs the analyzer under the `full-snapshot` profile. Emits every output format
(JSON + Markdown + HTML + SARIF + CSV + PNG charts) into
`<repo-root>\complexity_report\`. Uses tree-sitter when its deps are installed,
otherwise falls back to regex.

```batch
Tools\RunAnalysis.bat
```

The `full-snapshot` profile covers the whole repo (games, tools, tests
included) and excludes only vendor/build directories. Informational only --
never blocks CI.

### `Tools\CheckComplexity.bat` -- CI gate

Runs the analyzer under the `engine-ci` profile. Production engine code only;
games, tools, test fixtures (`UnitTests`, `**/*.Tests.inl`), and vendor code
are excluded so a wide gameplay dispatch or a `RunAllTests` doesn't poison the
gate. Outputs land in `<repo-root>\complexity_report_ci\`.

```batch
Tools\CheckComplexity.bat
```

Exits non-zero when any function exceeds the profile's `--fail-on` thresholds.
**The thresholds are checked-in ceilings**, not aspirational quality targets,
so the script may fail today if the current snapshot exceeds them. As of
writing, `Zenith_EditorAutomation::ExecuteAction` (LOC=980) exceeds the
`max-loc=900` ceiling and would fail the gate. The point of the gate is to
keep things from getting *worse*, not to certify the codebase as already
clean.

## Profiles

All scope and threshold configuration for both wrappers lives in
[`complexity_profiles.json`](complexity_profiles.json). Edit that file, not
the batch scripts, to tune behavior.

A profile sets the base values for `--exclude`, `--exclude-glob`,
`--ignore-macros`, `--fail-on`, and the duplicate-filter flags. Explicit CLI
flags after `--profile` *append* to list values (`--exclude`, `--exclude-glob`,
`--ignore-macros`) and *replace* scalar values (`--fail-on`).

Two profiles ship in the box:

| Profile | Scope | Used by | Blocks CI? |
|---|---|---|---|
| `engine-ci` | `Zenith/**` minus games, tools, tests, middleware. Drops `**/*.Tests.inl`. Filters low-priority and test-only duplicates. | `CheckComplexity.bat` | Yes |
| `full-snapshot` | Whole repo minus vendor/build. Includes games, tools, tests. | `RunAnalysis.bat` | No |

Run the analyzer directly with `--profile <name>` to use a profile from any
shell. Override the profile JSON path with `--profile-file <path>`.

## CLI reference

### Positional

| Argument | Description |
|---|---|
| `path` | Root path of the codebase to analyze. Default `.` (current directory). |

### Profile

| Flag | Description |
|---|---|
| `--profile NAME` | Named profile from `complexity_profiles.json` (e.g. `engine-ci`, `full-snapshot`). Pre-populates excludes, ignored macros, fail-on thresholds, and duplicate filters. |
| `--profile-file PATH` | Override the profile JSON path. Default: `Tools/complexity_profiles.json` next to this script. |

### Input selection

| Flag | Description |
|---|---|
| `-e`, `--exclude DIR [DIR ...]` | Directory names to skip. Matches any path segment. With `--profile`, CLI values are appended to the profile list. Default (no profile): `Middleware ThirdParty External vendor build Build .git .claude`. |
| `--exclude-glob GLOB` | Glob pattern (full relative path) to exclude. Repeatable. Examples: `--exclude-glob "**/*.Tests.inl" --exclude-glob "Generated/**"`. With `--profile`, CLI values are appended. |
| `--exclude-generated` | Skip files that look auto-generated (`.pb.cc`, `.generated.*`, `DO NOT EDIT` banners in the first 5 lines). |
| `--ignore-macros NAME [NAME ...]` | Macro names whose balanced `MACRO(...)` invocations are stripped before complexity scanning. Useful for assertion macros. With `--profile`, CLI values append. |

### Output selection

By default the tool emits `analysis_report.json` and `analysis_report.md` into
`-o`. All other outputs are opt-in.

| Flag | Description |
|---|---|
| `-o`, `--output DIR` | Output directory. Default `complexity_report`. Created if it doesn't exist. |
| `--no-json` | Skip the JSON report. |
| `--no-md` | Skip the Markdown report. |
| `--no-viz` | Skip the PNG chart suite. Charts require matplotlib; if it's missing they're always skipped. |
| `--html` | Emit a self-contained HTML dashboard (`analysis_report.html`). Zero external dependencies -- opens in any browser, sortable tables, filter boxes. |
| `--sarif` | Emit a SARIF 2.1.0 findings report (`analysis_report.sarif`) for IDE integration (VS Code, GitHub Code Scanning, etc.). |
| `--csv` | Export file / directory / function metrics as CSV. |
| `--absolute-paths` | Emit absolute filepaths in JSON (useful when downstream consumers need resolved paths). |

### Tuning

| Flag | Description |
|---|---|
| `--thresholds PATH` | Path to a JSON file overriding the anti-pattern thresholds (see **Thresholds** below). |
| `--workers N` | Number of worker processes. `0` (default) = auto-parallel for >=200 files. `1` = force sequential. |
| `--parser {auto,regex,tree-sitter}` | Parser backend. `auto` (default) uses tree-sitter when its deps are installed and falls back to regex otherwise. `regex` is stdlib-only and approximate. `tree-sitter` is accurate on nested templates, C++20 `requires`, function-try-blocks, and most macro-generated signatures; requires `pip install tree-sitter tree-sitter-cpp`. The actual backend used is recorded in the report's health summary so reports are explicit about what ran. |

### AI refactor suggestions

| Flag | Description |
|---|---|
| `--llm-suggestions` | Call the Anthropic API to generate one structured refactor recommendation per top-N hotspot. Emits a new "AI-suggested refactors" section in the markdown, JSON (`llm_suggestions`), and HTML dashboard. Requires `pip install anthropic` and `ANTHROPIC_API_KEY` in the environment. |
| `--llm-max N` | Max number of functions to request suggestions for (default `5`). Each function = one API call. |
| `--llm-model MODEL` | Anthropic model ID (default `claude-sonnet-4-6`). Override with e.g. `--llm-model claude-opus-4-7` for higher-quality (and more expensive) suggestions. |

Results are disk-cached in `<output>/llm_cache.json`, keyed by
`<model>:<sha256 of (filepath + source)>`. Re-running on unchanged code is
free (cache hit -> zero API calls). Delete the cache file to force a full
re-request.

### CI gating

| Flag | Description |
|---|---|
| `--fail-on RULES` | Comma-separated thresholds that cause a non-zero exit when exceeded. **Per-function**: `max-cc`, `max-cognitive`, `max-nesting`, `max-loc`, `max-function-priority`. **Per-file**: `max-file-priority`. **Aggregate** (repository-wide): `max-include-cycles`, `max-duplicate-clusters`. Example: `--fail-on max-cc=30,max-nesting=8,max-include-cycles=0`. Replaces (does not merge with) any profile `fail_on`. |

## What the tool produces

### Default outputs (always)

- **`analysis_report.json`** -- Full machine-readable report. Key sections:
  - `health_summary` -- one-glance digest (parser backend, profile, high-priority file count, P90 file priority, include-cycle count, duplicate-cluster count, edge-confidence breakdown, worst function, worst file)
  - `summary` -- priority-score distribution (high/medium/low counts, P50/P90)
  - `function_hotspots` -- top 50 functions by composite priority score, each with a `dominant_signal` field (which dimension drove the score)
  - `low_cohesion_classes` -- classes with LCOM5 >= 0.7 (header-inline only -- see caveats)
  - `duplicate_clusters` -- near-duplicate function groups
  - `include_cycles` -- strongly-connected components in the `#include` graph
  - `tech_debt_markers` -- TODO/FIXME/HACK/XXX locations (capped at 500)
  - `top_priority_files`, `top_complex_files`, `deepest_nesting_files`, `largest_files`
  - `directories` -- per-directory metrics ranked by worst file
  - `all_files` -- full per-file metrics
- **`analysis_report.md`** -- Human-readable markdown opened by a "Health
  summary" block, then the refactoring queue, tagged files, low-cohesion
  classes (with header-inline caveat), coupling table, cycles, duplicates,
  tech-debt markers, and directory overview. Each hotspot includes its
  dominant signal and a concrete "extract lines X-Y" suggestion pointing at
  its worst nested block.

### Opt-in outputs

- **`analysis_report.html`** (`--html`) -- Self-contained dashboard with
  sortable filterable tables. Embeds the full JSON, so it works offline.
- **`analysis_report.sarif`** (`--sarif`) -- SARIF 2.1.0 findings with rules
  for `priority-high`, each anti-pattern tag, `low-cohesion`, `include-cycle`,
  and `near-duplicate`.
- **`file_metrics.csv` / `directory_metrics.csv` / `function_metrics.csv`** (`--csv`)
- **12 PNG charts** (default unless `--no-viz`): directory comparison,
  complexity distributions, maintainability heatmap, LOC breakdown, Halstead
  metrics, risk quadrant, nesting depth, comment ratio, file size
  distribution, dashboard.

## Metrics glossary

| Metric | What it means | Range |
|---|---|---|
| **Cyclomatic complexity (CC)** | McCabe: independent paths through code. Each `if`/`for`/`while`/`case`/`catch`/`&&`/`||`/`?:` adds 1. | 1+, <10 is simple, >20 is complex |
| **Cognitive complexity** | SonarSource: human effort to understand. Each control-flow keyword costs `1 + current_nesting`; logical ops each add 1. | 0+, <15 is clear, >25 is hard |
| **Max nesting depth** | Deepest control-flow block nesting inside the function/file. | 0+, >4 is painful |
| **Priority score** | Composite 0-100. 30% cognitive + 25% CC + 20% nesting + 15% length + 10% params, each soft-capped. The tool ranks by this. Each hotspot row also names the *dominant signal* (the dimension contributing most to its score). | 0-100. 0 = clean, >=70 = priority refactoring target |
| **LCOM5** | Henderson-Sellers class cohesion. `1 - (method/field touches) / (methods * fields)`. Uses the engine's `m_` convention to identify fields. **Caveat: header-inline only** -- see Accuracy caveats. | 0.0-1.0. Near 1 = methods use disjoint fields -> split by responsibility |
| **Fan-in** | Number of files that `#include` this file. High fan-in = change risk. | |
| **Fan-out** | Number of local headers this file includes. | |
| **Jaccard similarity (duplicate detection)** | `|A intersect B| / |A union B|` over 5-gram token shingles, with identifiers/numbers normalized. Threshold 85%. The `engine-ci` profile additionally drops low-priority candidates and `*.Tests.inl` files so trivial getters and `ZENITH_TEST` wrappers don't dominate. | 0.0-1.0 |
| **Halstead volume / difficulty** | Operator/operand counts. Kept as secondary metrics only -- not used as headline signal because the 1977-era "bugs delivered" and "time to program" constants don't guide refactoring decisions. | |
| **Maintainability Index (MI)** | Classic 1994 formula. Retained in the JSON for tooling that expects it but deliberately *not* the headline -- its resolution collapses at the bottom (MI=0 for every bad file). | 0-100 |

## Anti-pattern tags

Functions get tagged when they cross a threshold. Tags drive the SARIF rule IDs
and the markdown "Tagged files" table.

| Tag | Rule |
|---|---|
| `long-function` | code_lines > 80 (default) |
| `very-long-function` | code_lines > 200 |
| `deep-nesting` | max_nesting_depth >= 5 |
| `complex-branching` | cyclomatic_complexity >= 15 |
| `hard-to-follow` | cognitive_complexity >= 25 |
| `many-params` | param_count >= 6 |
| `many-returns` | return_count >= 6 |
| `big-switch` | case_count >= 10 AND max_nesting_depth <= 3 |
| `nesting-hell` | max_nesting_depth >= 5 AND case_count < 5 |
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
    "extract_candidate_min_lines": 15,
    "priority_weight_cognitive": 0.30,
    "priority_weight_cyclomatic": 0.25,
    "priority_weight_nesting": 0.20,
    "priority_weight_length": 0.15,
    "priority_weight_params": 0.10
}
```

The `priority_weight_*` keys must sum to 1.0 and control how the composite
priority score blends each dimension.

## Tests

A fixture-based unittest suite lives in [tests/test_complexity.py](tests/test_complexity.py).

```bash
cd <repo>
python -m unittest Tools.tests.test_complexity -v
```

Test classes (run from any CWD):

- `TestCyclomaticComplexity`, `TestCognitiveComplexity`, `TestPercentile` -- core metric helpers
- `TestRawStringAndMacroHandling` -- raw-string skipping, macro stripping
- `TestFunctionDetection` -- regex parser boundary cases
- `TestPriorityScore` -- composite score factor breakdown
- `TestDuplicateDetection`, `TestDuplicateFiltering` -- shingle clustering plus the engine-ci low-priority / test-only filters
- `TestIncludeGraph`, `TestIncludeResolution` -- cycle detection plus relative-path-preferred resolution
- `TestDirectoryAggregates` -- percentile aggregation
- `TestLCOM`, `TestLCOMCaveat` -- cohesion metric and the header-inline caveat field
- `TestTreeSitterParser`, `TestParserAutoMode` -- tree-sitter behavior plus the `--parser auto` selection logic
- `TestLLMCache` -- on-disk cache hit, corrupt-cache recovery, missing-API-key fallback
- `TestExports` -- HTML and SARIF emission
- `TestProfileLoading` -- profile JSON resolution and the shipped `engine-ci` / `full-snapshot` definitions
- `TestExcludeGlob` -- glob-based exclusions including the `**/<pat>` top-level convenience
- `TestExpandedFailOn` -- new `--fail-on` keys (`max-function-priority`, `max-include-cycles`, `max-duplicate-clusters`, plus unknown-key rejection)
- `TestHealthSummary`, `TestDominantSignal` -- the markdown / JSON health block and per-hotspot dominant signal

Tree-sitter tests skip automatically if `tree-sitter` / `tree-sitter-cpp`
aren't installed.

## Accuracy caveats

### Parser backend

The default is `--parser auto`: tree-sitter when its deps are installed,
otherwise regex. The actual backend chosen is recorded in the report's
`health_summary.parser_backend`, so consumers can tell which one ran without
reading the invocation.

The regex parser is stdlib-only and approximate. The following constructs may
cause functions to be missed:

- Function-try-blocks (`void f() try { ... } catch (...) { ... }`)
- C++20 `requires` clauses in complex positions
- Macro-generated signatures (`DEFINE_ENTITY_COMPONENT(Foo)`)
- Deeply nested template arg lists (`std::vector<std::pair<int, std::map<K, V>>>`)
- `throw(...)` exception specifications

Tree-sitter parses the full C++ grammar, so it handles nested templates,
operator overloads, destructors, and function-try-blocks correctly.
File-level metrics (total lines, CC across the file, cognitive complexity,
Halstead) are unaffected by parser choice -- they don't depend on function
boundary detection. A missed function just doesn't appear in the per-function
hotspot list; the file it lives in still shows its aggregate complexity.

### Include graph

Resolution is two-tier:

1. **Relative-path match (high confidence).** The raw `#include` string is
   matched against an index of file relative paths and unambiguous suffixes.
   `#include "Flux/Foo.h"` resolves uniquely to the file whose relative path
   ends with `Flux/Foo.h`.
2. **Basename fallback (low confidence).** When step 1 finds no match we fall
   back to basename matching (`Foo.h` -> any file named `Foo.h`).

Edges resolved at each tier are counted in
`health_summary.include_edges_relative` and `include_edges_basename`. A high
basename ratio means cycles should be treated with skepticism. System
`<...>` includes are skipped at parse time.

### LCOM (header-inline only)

LCOM5 is computed from inline-defined methods only. Method bodies that live
in a separate `.cpp` file are invisible at the per-class scope, so LCOM5
should be read as a *header-inline* cohesion signal, not a full-class score.
Each class record carries a `caveat: 'header-inline only'` field and the
markdown report flags this in the section header.

### Duplicate detection

The base detector flags any near-duplicate Jaccard >= 85%. The `engine-ci`
profile additionally drops:

- functions with priority score < 15 (cosmetic dupes -- one-line getters and
  ZENITH_TEST wrappers always shape-match each other)
- functions in `*.Tests.inl` files, `UnitTests/`, and `tests/` directories

Both filters are profile-driven; the `full-snapshot` profile leaves the raw
clusters in place. Use `filter_low_priority_duplicates` and
`filter_test_only_duplicates` keys in a profile to opt in.

### Nesting

The nesting detector intentionally does *not* treat function bodies, lambda
bodies, or initializer lists as control-flow nesting, even though they open a
`{` after `)`. Earlier versions of the tool had this bug -- it's documented
here so future contributors don't "fix" the working case.

## Design principle: snapshot only

The tool deliberately avoids any cross-run or git-history integration. No
`--baseline` flag, no churn multiplier, no time-series. The current source is
the only input. To track trends, run the tool in CI and diff the JSONs in
your downstream pipeline.
