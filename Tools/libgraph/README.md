# libgraph

A generator for a self-contained interactive HTML report of the boundaries
between Zenith's engine libraries: dependencies, public header/type/link
surfaces, leakage, and a per-lib swap contract. Use it to shrink a lib's
public surface, and to prove a lib is actually swappable (e.g. "could
ZenithAI be replaced entirely?").

## What it measures, and from where

Every fact comes from the build's own output, not from hand-maintained
maps:

- **Ownership, declared dependencies, per-config build flags** — the
  generated `*_win64.vcxproj` files (file → lib ownership, include dirs,
  the PCH header, `FunctionLevelLinking`) and the Sharpmake `.cs` files
  (`AddPublicDependency<X>` calls, parsed after stripping comments).
- **The include graph** — `#include "..."` lines, resolved the way MSVC's
  quoted-include search resolves them (the includer's own directory, then
  the parent-include stack, then the project's `AdditionalIncludeDirectories`).
- **The type surface** — a regex scan of each lib's headers for
  namespace-scope `class`/`struct`/`enum` declarations and `using`/`typedef`
  aliases, then a scan of every other file for cross-lib uses of those
  names.
- **The link surface** — `dumpbin /symbols` on each lib's `.lib` (and each
  game's `.obj` files) from a **built Debug config**, using COMDAT-section
  detection to tell an inlined/template instantiation apart from a genuine
  out-of-line definition.

Nothing here is invented or scored — every number in the report is a count
with the actual list behind it (a header, a type, a symbol, an
`file:line` include).

## Quick start

```
Build\regen.ps1
zenith build <Game>                       REM e.g. zenith build Zenithmon -- a Debug config
python Tools\libgraph\libgraph.py --open
```

`zenith build <Game>` with no `--config` already builds
`Vulkan_vs2022_Debug_Win64_True` (see `Build/zenith_config.psd1`), which is
exactly libgraph's own default `--config`, so an ordinary build already
produces what libgraph needs.

Don't want to build first? Run header-only:

```
python Tools\libgraph\libgraph.py --no-symbols --open
```

This skips the `dumpbin` stage entirely (`meta.symbols` is `false` in the
output) — every header, include-edge and type-use fact is still measured;
only the link-symbol surface and `link-only` edges are unavailable.

### Exit codes

| Code | Meaning |
|---|---|
| `0` | ok |
| `1` | model or input error — a stale/missing `.vcxproj`, an unknown Sharpmake class, a bad `--repo-root` |
| `2` | symbols refusal (`SymbolsError`) — `FunctionLevelLinking` is on for the chosen config, or a lib's `.lib`/a game's `.obj`s are missing. Re-run with `--no-symbols` for a header-only report |
| `3` | `--baseline` is unreadable, not valid JSON, or not a `libgraph.json`-shaped document |

Also printed in `--help`. Every error is one clear, actionable line on
stderr with no traceback; pass `--debug` to also see the full Python
traceback.

The report is written to `Build/artifacts/libgraph/` (gitignored):
`libgraph.json` (the whole model) and `index.html` (the same data,
self-contained and offline — vendored `cytoscape`/`dagre` are inlined, so
it opens with no network access).

## The views

- **Graph** — a layered DAG of libs, node size = inbound swap-contract
  size, colour = layer. Edge colour = class, width = include-line count.
  Toggles for games, middleware, test/pch/allowlisted edges,
  undeclared-only, and the Sentinel lower-bound overlay.
- **Matrix (DSM)** — libs in layer order, one cell per (row, column)
  include count. Up-edges and cycles are highlighted directly in the grid.
- **Libraries** — pick a lib to open its **lib panel**: the inbound/outbound
  contract summary plus sortable tables for entry headers, the exposed
  closure, unused public headers, leakage, types (embed/ref), link symbols,
  and unreferenced externals.
- **Edge panel** — click an edge (in the Graph or the Matrix) to see every
  `file:line` include across it, the types it uses, and the symbols it
  crosses. This is the literal to-do list for cutting that edge.

### Header roles

Every header a lib owns gets exactly one `role`, independent of whether the
header is under an `Internal/` directory (`declared_internal`):

- **`entry`** — included directly from at least one file outside the lib.
- **`exposed`** — reachable only transitively, through an entry header's
  own closure; nothing outside the lib `#include`s it directly.
- **`unused-public`** — not under `Internal/`, and never `#include`d from
  outside at all. The move-to-`Internal/` candidate — unless its own
  out-of-line symbols are still linked from outside (see "trim the public
  surface" below).
- **`internal`** — under an `Internal/` directory, and never `#include`d
  from outside. The ECS convention, working as intended.

`role` and `declared_internal` are independent, and the one combination
worth watching for is **`role: "entry"` with `declared_internal: true`** —
something outside the lib reaches straight into its `Internal/` directory.
That's an encapsulation break by definition, and the header's `hint` says
so directly (`"encapsulation break: Internal/ header included from
outside"`).

### Reading the views for "trim the public surface"

Work from the **lib panel**:

- **Unused public headers** — listed by the lib's project, `role ==
  "unused-public"`, never `#include`d from outside. Each row's `hint` says
  `"no outside users: move to Internal/"` — unless the header's own
  class(es) are still linked from outside despite no `#include` reaching it
  (test code compiled elsewhere, or an `extern`-declared entry point); that
  row's hint says **check before moving**, not "move it", because deleting
  the header would still break a real out-of-line caller.
- **Single-consumer headers** — `role == "entry"` with exactly one
  consumer node; the hint names it. Candidates for moving the header behind
  that one caller's own interface, or inlining it there.
- **Types used only by reference** — a lib type whose external `ref` count
  is nonzero and `embed` count is zero: every outside use is a pointer or
  reference, so the consumer only ever needed a forward declaration. The
  hint says so directly.
- **Unreferenced externals** — `libs[L].unreferenced[]`: symbols the lib
  defines out-of-line that nothing outside it calls. Candidates for
  `static`, an anonymous namespace, or `private`.
- **Exposed headers** — reachable only transitively (through an entry
  header), never `#include`d directly. Hint: forward-declare in the entry
  header to cut the transitive pull.

### Reading the views for "is this lib swappable"

Work from the lib panel's **swap contract**:

- **Inbound** (what a replacement must provide): entry headers, exposed
  headers, externally-used types split embed vs. ref, link symbols,
  consumer libs/games, and leaking foreign headers. This is the lib's real
  boundary — everything a drop-in replacement has to reproduce. Two of
  these fields are easy to misread:
  - `types_embed`/`types_ref` are **sums of per-type consumer-file
    counts**, not counts of distinct types — a single type embedded by
    value in 3 different consumer files adds 3 to `types_embed`, not 1.
  - `link_symbols` is the **count of distinct owned symbols referenced by
    at least one other node** (not a sum of per-symbol reference counts —
    a symbol called from five different places anywhere still adds 1).
- **Outbound** (what the lib may consume): its own outgoing edges and
  outgoing symbols by target lib, set against the Sentinel-proved lower
  bound where one exists (`outbound.lower_bound`) — the scripted form of
  the `dumpbin` FORBIDDEN_EXTERNALS proof that today only exists as prose
  in a few `CLAUDE.md` files.
- **Leakage headers** (`libs[L].leakage[]`) — another lib's header that
  re-`#include`s one of L's headers, so anything reaching L through it
  never declared L at all. `downstream_via_header` vs.
  `downstream_via_cpp_only` tells you whether the leak reaches other
  *headers* (worse — it propagates further) or only `.cpp` files. Leakage
  treats a **game as a leaf**: nothing else can `#include` one of a game's
  own headers, so a game-owned header is never listed as leaking L
  onward — it's just that game's own direct reach into L, already fully
  represented by the game's own edge. `proof` nodes (Sentinel\*) are
  excluded from leakage entirely, on both sides.
- **Embed-by-value types** block a swap outright: a consumer holding one of
  L's types by value (a member, a base class, a by-value template
  argument) needs L's exact layout, not just a pointer to something that
  implements an interface. `types[].hint` says
  `"embedded by value in N consumer files: blocks swapping"`.
- **Cross-cutting link symbols** — a symbol whose `referenced_by[]` spans
  many libs (e.g. a logging entry point every lib calls,
  `Zenith_EditorAddLogMessage`-style) is a seam a swap has to keep, not
  drop, even though it looks like unrelated noise in any one lib's table.

## The edge classes and the layering rule

Every edge between two non-game nodes carries **exactly one base class**
(`declared+used`, `declared-unused`, `used-undeclared`, `test-only`, `pch`,
`link-only` — checked in that order; see `tests/CONTRACT.md` for the exact
rule), plus zero or more additive tags (`allowlisted`, `in-cycle`). A game
edge always carries `classes: []` — a `GameProject` declares a dependency
on the whole `Zenith` aggregate, never on an individual lib, so there is no
per-lib declaration to compare its use against.

**`layer` follows declared intent, not real use.** A node's `layer` in the
Graph/Matrix is computed only from `declared+used`/`declared-unused` edges
— what Sharpmake says the architecture should be. `in-cycle`, by contrast,
is computed from `declared+used`/`used-undeclared` edges — what the code
actually does. These are two different, disjoint SCC computations over two
different edge sets, and **they routinely disagree**: on the real engine,
22 of 30 libs form one strongly-connected component over real use, while
their *declared* dependencies still form a sensible, mostly-acyclic
hierarchy. Layering by real use would collapse the Graph view onto one
unreadable layer and hide the very thing worth seeing. So two libs can be
`in_cycle: true` together while sitting on *different* layers — that's not
a bug, it's what makes an undeclared or upward *use* edge visible at all:
it shows up in the **Matrix as an up-edge** (`layer[src] <= layer[dst]`,
excluding game edges) precisely because layering ignored it while real use
didn't.

**The `via_aggregate` rule.** The `Zenith` aggregate "declares" a
dependency on every Entry-table lib by construction
(`AddAllAsDependenciesOfAggregate`), which would make its whole outbound
edge set look `declared+used` even where nothing is actually `#include`d —
hiding real link-only and undeclared structure behind a wall of trivially
"declared" edges. So an aggregate-only declaration counts as `Declared? =
yes` for classification **only when the edge also carries at least one
normal (non-pch, non-test) include line**. Reached only via a link symbol
with no include at all, it falls through to `link-only` instead. An
aggregate edge with **no** include line and **no** link symbol either isn't
emitted at all — there's no evidence to report.

**The two `pch` notions are not the same thing.** Every `.cpp` in this
codebase starts `#include "Zenith.h"`, so almost every file's include
closure runs through the PCH. `includes.py` tags an edge `pch` broadly —
any line reachable only via the PCH's own closure — because that's what
`used-undeclared` needs to *exclude* (nobody should get credit, or blame,
for what the PCH drags in incidentally). The edge-class `pch` **base
class** is narrower: it only applies to a literal `#include "<pch_root>"`
line — the direct include of the PCH header itself. A file can be tagged
`pch` on plenty of lines without ever producing a `pch`-*class* edge,
because its actual relationship to that other lib is `declared+used` (or
`used-undeclared`, etc.) through some other, non-PCH include entirely; the
PCH tag on a given line just says "this specific line's reach runs through
the PCH", not "this pair's whole relationship does".

## Workflow

1. Run it, read the Libraries tab for the lib you're trimming.
2. Refactor (move a header to `Internal/`, forward-declare a ref-only type,
   `static`-ify an unreferenced symbol, break a leakage header).
3. Re-run with `--baseline <previous libgraph.json>` and read the deltas —
   the Libraries tab and every `inbound.*` count in the JSON show a signed
   `(N)` next to the current value. A negative delta on `entry_headers`,
   `types_embed`, `link_symbols`, etc. is the surface actually shrinking;
   `--baseline` diffs `inbound` counts only (`INBOUND_FIELDS`), because
   that's the boundary a replacement has to match.

## Limitations and heuristics

This is regex-based source scanning, not a real C++ parser or a linker's
own view — documented blind spots, from the module docstrings:

- **Type classification is regex-based.** Declarations, base classes and
  uses (embed/ref/mention) are recognised by pattern, not by resolving
  actual C++ semantics — see `cxxtypes.py`'s docstring for the exact
  list (macro-wrapped declarations before the keyword, nested/local
  declarations, `X**`/`X*&`, template arguments past the first level, and
  so on).
- **Namespaces are not tracked for types.** `TypeDecl.name` is a bare
  identifier; two same-named types in different `namespace` blocks (or one
  namespaced and one global) collide as a single name. Observed on the
  real repo: `Zenith_Maths::Quat` vs. DevilsPlayground's own
  `using Quat = Zenith_Maths::Quat;` re-export.
- **Inline/template API is visible only at header level.** A symbol
  defined entirely in a header (a template, or anything the compiler
  inlines into a single-lib COMDAT) never shows up in the link surface —
  only its header/type-use footprint does. The link surface only ever
  measures genuinely out-of-line definitions.
- **One config per run; render backends are swapped per config.** A
  `vulkan_*` run links no `D3D12`/`Null` objects at all, so a backend's
  swappability has to be checked per config (`--config null_...` /
  `--config d3d12_...`, each with its own `--baseline`).
- **Sentinels exist only in `_False` configs**, so on a `_True` config
  every lib's Sentinel is absent for that run and the lower-bound overlay
  is empty (`outbound.lower_bound == []` for every lib). Build/run a
  `_False` config to see it populated.
- **Release configs are refused.** `/Gy` (`FunctionLevelLinking`) puts
  every function into its own COMDAT section, which defeats the
  owned-vs-inline distinction the symbols stage relies on entirely — the
  tool exits 2 rather than silently mis-measuring the link surface.
- **Games whose objects aren't built are skipped**, not treated as an
  error: that game's node, include edges and type uses all still appear
  normally, it just loses its link-symbol contribution and is listed in
  `meta.health.missing_link_inputs` with the exact `zenith build <Game>`
  command that would fill it in.
- **A symbol owned by two or more nodes is a "conflict"**, not an error: a
  non-COMDAT (genuinely out-of-line) symbol defined in more than one node's
  object/lib gets no unambiguous owner at all, so it's invisible to every
  link surface — it never appears in any lib's `symbols[]`/`unreferenced[]`,
  and no edge crosses it. On the real repo this only affects games' own
  `Project_*` entry points, which sit outside `libs{}` anyway (games have
  no lib surface of their own to report).
- **`cls` can name a namespace, not a class.** A free-function group
  declared inside a plain `namespace` block (e.g. `Zenith_AI`) can surface
  as a `cls` value the same way a real class does; such a group's `header`
  is `""`, since a namespace isn't declared anywhere a single header can be
  attributed to. Type names throughout are bare identifiers — never
  namespace-qualified (see "namespaces are not tracked for types" above).

## Running the tests

```
python -m unittest discover Tools/libgraph/tests
```

`tests/test_e2e.py` additionally runs the real CLI as a subprocess against
`tests/fixtures/repo` (with canned `dumpbin` text from
`tests/fixtures/dumpbin`, via `--dumpbin-dir`), so it also exercises
argument parsing and every exit code.

## Module map

| File | Role |
|---|---|
| `libgraph.py` | the CLI — runs the whole pipeline and writes the report |
| `model.py` | ownership, declared edges, orphans/duplicates, allowlists (wraps `vcxproj.py` + `sharpmake.py`) |
| `vcxproj.py` | parses generated `.vcxproj` files for one config |
| `sharpmake.py` | scans `Build/*.cs` for project classes and `AddPublicDependency` edges |
| `includes.py` | the include graph: parsing, MSVC-order resolution, closures, PCH tagging |
| `cxxtypes.py` | the type index: declarations and embed/ref/mention uses (named to avoid colliding with stdlib `types`) |
| `symbols.py` | `dumpbin /symbols` parsing and the COMDAT owned/inline distinction |
| `metrics.py` | turns the above into the `libgraph.json` document: edge classes, layering, surfaces, leakage, swap contracts, delta |
| `report.py` | writes `libgraph.json` and renders `index.html` (inlines `template.html` + `vendor/`) |
| `template.html` | the report's UI (Graph/Matrix/Libraries views, lib and edge panels) |
| `vendor/` | pinned `cytoscape`/`dagre`/`cytoscape-dagre`, inlined into the offline report |
| `tests/CONTRACT.md` | the `libgraph.json` schema, in prose |
| `tests/contract.py` | the schema's validator (`validate(doc) -> list[str]`) |
| `tests/fixtures/repo/` | a miniature Zenith checkout the real parsers run against unmodified |
| `tests/fixtures/dumpbin/` | canned `dumpbin /symbols` text for the fixture libs/games |
