# `libgraph.json` — the contract

This is the precise, human-readable schema for `libgraph.json`, the single
JSON document `report.py` writes (as `Build/artifacts/libgraph/libgraph.json`
and embedded in `index.html`) and that both the HTML/UI and `metrics.py`
build against. It is pinned by example in
`tests/fixtures/sample_libgraph.json` and enforced by `tests/contract.py`
(`validate(doc) -> list[str]`, empty list = conforms). If this document and
`contract.py` ever disagree, `contract.py` is the one the tests run, but
treat the disagreement as a bug in whichever one is wrong — file both
together.

All paths in the document are **repo-relative, forward-slash**. All node
names (`nodes[].name`, every `libs` key, every `src`/`dst`/`node`/`via_node`
value) are drawn from the same flat namespace and must appear in `nodes[]`.

Run the validator directly:
```
python Tools/libgraph/tests/contract.py <path-to-libgraph.json>
```
Prints `OK` and exits 0 if the document conforms; otherwise prints every
error found (one per line) and exits 1.

## Enums (module constants in `contract.py`)

Import these from `contract.py` instead of re-typing the string literals:

| Constant | Values |
|---|---|
| `NODE_KINDS` | `lib`, `middleware`, `game`, `proof` |
| `EDGE_CLASSES` | the union of the two rows below |
| (base, exactly one per non-game edge) | `declared+used`, `declared-unused`, `used-undeclared`, `test-only`, `pch`, `link-only` |
| (additive, zero or more, layered on the base) | `allowlisted`, `in-cycle` |
| `HEADER_ROLES` | `entry`, `exposed`, `unused-public`, `internal` |
| `TYPE_MODES` | `embed`, `ref`, `mention` |
| `FILE_TAGS` | `test`, `pch`, `conditional` |
| `INBOUND_FIELDS` | `entry_headers`, `exposed_headers`, `types_embed`, `types_ref`, `link_symbols`, `consumer_libs`, `consumer_games`, `leaking_headers` |

`EDGE_BASE_CLASSES` and `EDGE_ADDITIVE_CLASSES` are also exported
separately, since the edge-class rule below needs the split.

## The edge-class rule

Every **non-game** edge (an edge where neither endpoint's `nodes[].kind` is
`"game"`) carries **exactly one base class**, chosen by testing these rows
**in order** and taking the first that matches. "Use" means a non-test,
non-pch include (a conditional one still counts) or a link symbol.

| Declared? | Condition | Base class |
|---|---|---|
| yes | has a use | `declared+used` |
| yes | no use | `declared-unused` |
| no | has a non-test, non-pch include | `used-undeclared` |
| no | its only includes come from `*.Tests.inl` | `test-only` |
| no | reached only through the PCH | `pch` |
| no | no include, only a link symbol | `link-only` |

On top of the one base class, an edge can also carry either or both of
these **additive** classes:

- **`allowlisted`** — at least one of the edge's include lines matches an
  allowlist key. `allowlist_kind` records that key's kind (`null` for a
  plain, un-prefixed allowlist line; otherwise the string before the `:`,
  e.g. `"encapsulation"`, `"layer-up"`).
- **`in-cycle`** — `src` and `dst` are in the same strongly-connected
  component, computed over the subgraph of edges whose **base** class is
  `declared+used` or `used-undeclared` (a `test-only`/`pch`/`link-only`-only
  relationship never forms a cycle). This is a measure of **real usage**:
  it stays true regardless of what anyone declared.

  ★ **`in-cycle` and `layer` are now computed over two DIFFERENT, disjoint
  edge sets, and that is deliberate — see "How `layer` is computed" below.**
  `layer` follows only *declared* edges (`declared+used`/`declared-unused`);
  `in-cycle` follows only *used* edges (`declared+used`/`used-undeclared`).
  The two agree on `declared+used` and diverge on everything else. **There
  is no longer a rule that two `in-cycle` nodes share a `layer`, and
  `contract.py` does not check for one.** On the measured real engine, 22
  of the 30 libs form one strongly-connected component over real use —
  almost everything is `in-cycle` with almost everything else — while their
  *declared* dependencies are still a sensible, mostly-acyclic hierarchy.
  Flattening every one of those 22 libs onto one `layer` would turn the
  Graph view into an unreadable blob and hide the one thing worth seeing:
  which of those real-use edges run the WRONG WAY against what was
  declared. `sample_libgraph.json` exercises exactly this: `LibB` and
  `LibC` are both `in_cycle: true` (via `LibB → LibC` `declared+used` and
  `LibC → LibB` `used-undeclared`) but sit on **different** layers (`2` and
  `1`), because only the `declared+used` half of that pair is a layering
  edge.

`classes[]` therefore has 1–3 entries for a non-game edge: one base class,
plus optionally `allowlisted` and/or `in-cycle`, e.g.
`["used-undeclared", "allowlisted"]` or `["declared+used", "in-cycle"]`.

**Game edges are not classified.** An edge where either endpoint's `kind` is
`"game"` always carries `classes: []`, because a `GameProject` declares a
dependency on the whole `Zenith` aggregate, not on individual libs — there
is no per-lib declaration to compare against. Such edges still exist (a
game's own files really do include a lib's headers and reference its
symbols) and still carry `files[]`/`types[]`/`symbols[]`/`include_lines`;
they just carry no `classes`. They feed `libs[].inbound.consumer_games`
only.

`pch`-class edges point at the node that **owns** the PCH header, not at
whatever else the PCH transitively pulls in. In `sample_libgraph.json` that
is `LibA` (which owns `Zenith/LibA/FxPch.h`); in the real repo it is
whichever lib owns `Zenith.h`. **Never hardcode "Core" (or any other lib
name) here or in `contract.py`** — which node owns the PCH is a property of
the model, not of the schema.

Per-edge **endpoint kinds are not carried on the edge**. A UI toggle that
needs to know whether `src`/`dst` is a game, middleware, lib or proof node
joins through `nodes[].kind` by name — there is no `src_kind`/`dst_kind`
field.

## Top level

```
{ "meta": {...}, "nodes": [...], "edges": [...], "libs": {...}, "delta": null | {...} }
```

### `meta`

| Field | Type | Meaning |
|---|---|---|
| `config` | string | the Sharpmake config the model was built from, e.g. `"fx_debug"` / `"vulkan_vs2022_debug_win64_true"` |
| `generated` | string | ISO-8601 UTC timestamp of the run, e.g. `"2026-09-23T00:00:00Z"` |
| `repo_head` | string | the git SHA the tree was at when generated |
| `baseline` | string \| null | repo-relative path to the `--baseline` JSON this run diffed against, or `null` if none was given |
| `health` | object | see below — used by the **Health banner** view |
| `delta_removed` | list[string] | lib names present in `baseline` but not in `nodes[]` now; `[]` when `baseline` is null |
| `delta_added` | list[string] | lib names present in `nodes[]` now but not in `baseline`; `[]` when `baseline` is null |
| `symbols` | bool | **always present.** `false` when this run was `--no-symbols` (header-only) — no `dumpbin` pass ran, so nothing in `link_symbols`, `outbound.symbols_by_target`, `symbols[]`, `unreferenced[]`, or any `link-only` classification was actually measured; every one of those is `0`/`[]`/`{}` by omission, not by observation. The UI reads this to say **"link data not collected"** instead of rendering those zeros as if they were real. `true` for a normal run, whether or not `missing_link_inputs` below is non-empty |

`meta.health`:

| Field | Type | Meaning |
|---|---|---|
| `orphans` | list[path] | files no `.vcxproj` lists, fell back to directory ownership |
| `duplicates` | list[path] | files listed by more than one project |
| `unresolved_conditions` | list[string] | Sharpmake `AddPublicDependency` conditions the parser couldn't evaluate, e.g. an unhandled `#if` |
| `ambiguous` | list[object] | include strings more than one candidate file could satisfy; each is `{file, line, raw, candidates}` — `file`/`raw` are strings, `line` is an int, `candidates` is a list of paths |
| `missing_link_inputs` | list[string] | **always present.** One human-readable entry per node the symbols stage had to skip because its build outputs don't exist yet (a game whose `.obj`s were never built, a lib whose `.lib` is missing), e.g. `"Foundry: no .obj files under <path> -- run \`zenith build Foundry\`..."`. `[]` when nothing was missing, and also `[]` for a `--no-symbols` run (there's no symbols stage to have skipped anything in — that case is `meta.symbols == false` instead, not a `missing_link_inputs` entry per node) |

### `nodes[]`

One entry per node in the graph (libs, middleware, games, proof/tool
projects). Used by the **Graph** and **Matrix (DSM)** views for layout, and
by every other section to resolve a name to a kind.

| Field | Type | Meaning |
|---|---|---|
| `name` | string | unique across the whole document |
| `kind` | string, one of `NODE_KINDS` | `lib` (engine lib incl. the `Zenith` aggregate), `middleware` (vendored/third-party, collapsed), `game` (consumer only), `proof` (Sentinel*/tool projects) |
| `layer` | int | DAG layer over **declared** dependencies; `0` = leaves. Nodes in the same *layering* SCC (declared+used/declared-unused only — see below) share the same `layer`; nodes in the same `in_cycle` SCC do **not** necessarily. See "How `layer` is computed" below — `contract.py` can recompute and cross-check this field from `edges[]` |
| `in_cycle` | bool | true iff this node participates in at least one **real-use** SCC (the `in-cycle` definition above — `declared+used`/`used-undeclared`, a different edge set from `layer`'s) |
| `inbound_size` | int | the node-sizing scalar for the **Graph** view ("node size is the inbound swap-contract size"). For a `kind=="lib"` node this is the sum of its `libs[name].inbound` fields; for `middleware`/`game`/`proof` nodes (which have no `libs` entry) it is `0` |

#### How `layer` is computed

This is the rule a producer (`metrics.py`) follows to assign every node's
`layer`, and the rule `contract.py`'s `_validate_layers` recomputes from
`edges[]`/`libs{}` to cross-check the field (see "What `contract.py`'s
`validate()` checks" below). Never hardcode a specific lib as "the bottom
layer" anywhere this rule is implemented — the layering is derived, not
declared.

★ **`layer` reflects the INTENDED architecture (declared Sharpmake
dependencies), not measured use.** An earlier version of this rule
layered by real use (`declared+used`/`used-undeclared`/`link-only`), which
seemed like the more honest measurement — until it was run on the real
engine: **22 of the real engine's 30 libs turn out to form ONE strongly-
connected component over real use**, so layering by use collapses almost
the entire Graph view onto a single layer and it stops being readable as
architecture at all. Layering by *declared* intent instead keeps the DAG
meaningful, and makes an undeclared or upward *use* edge show up as
exactly what it is: a violation of the declared shape, visible as a DSM
up-edge (below) rather than erased by flattening everything it touches
onto one layer.

1. **Layering edges** are the edges the layer computation walks: every
   **non-game** edge (neither endpoint's `kind` is `"game"`) whose single
   **base** class is **`declared+used` or `declared-unused`** — i.e. the
   edge was *declared* in Sharpmake, regardless of whether it's also used.
   `used-undeclared`, `link-only`, `test-only` and `pch` edges never
   affect a layer, even though `used-undeclared` and `link-only` are two
   of the three edges `in-cycle` (above) is computed over — that is the
   whole point: a real, undeclared or link-only use is exactly the kind of
   edge this rule must NOT let pull two nodes onto the same layer, because
   doing so is what would hide it.
2. **Collapse strongly-connected components over the layering edges** (in
   case the *declarations themselves* cycle — two libs each declaring a
   dependency on the other). This is a genuinely different SCC computation
   from `in-cycle` above: the two use disjoint extra edges
   (`declared-unused` here only, `used-undeclared` there only) and share
   only `declared+used`, so neither is a subset of the other. All nodes in
   one layering SCC share one `layer` value; nodes in the same `in-cycle`
   SCC do not, in general, share anything.
3. **`layer(node) = 0`** if the node (or its SCC) has no outgoing layering
   edge to a *different* SCC; otherwise **`1 + max(layer(dst))`** over its
   outgoing layering edges (an edge to a fellow SCC member never counts,
   since it doesn't leave the SCC).
4. **Games and proof nodes use their own rule, not step 3:**
   - A **game**'s `layer` is `1 + max(layer(dst))` over the destination of
     **every** edge whose `src` is that game (game edges carry
     `classes: []`, but all of them still count here, not just declared
     ones — a game has no declarations to be selective about) — `0` if
     the game has no edges at all.
   - A **proof** node's `layer` equals the `layer` of its subject lib —
     the lib `L` where `libs[L].sentinel == this proof node's name` — or
     `0` if no lib claims it as a sentinel.

`contract.py`'s cross-check is intentionally **best-effort, per node**: it
skips (reports nothing for) a node it cannot resolve unambiguously rather
than guessing, so it never produces a false mismatch. That covers, for
example: more than one lib naming the same proof node as its `sentinel`
(which subject to use is undecidable), a game whose edges point at another
game (the two would need each other's layer first), or — defensively, this
should never occur given a valid SCC collapse — an unexpected residual
cycle in the collapsed graph. Everything else is compared exactly, and any
disagreement is reported as an error naming both the stored and the
recomputed value.

#### Up-edges (the DSM view)

An edge (excluding game edges) where **`layer[src] <= layer[dst]`** is an
**up-edge** — a layering violation, in the DSM sense: `src` reaches `dst`
at the same layer or a layer *above* itself, which cannot happen for a
clean, declared-only DAG walked strictly downward. This is not a
`contract.py` check and not a `classes` value — it is a **derived**
property the UI computes by joining `edges[].src`/`.dst` through
`nodes[].layer`, the same way endpoint kinds are joined (see "Per-edge
endpoint kinds" above), and the Matrix/DSM view is expected to highlight
it.

Because `layer` now follows only *declared* edges, an up-edge is exactly
how a `used-undeclared` or `link-only` edge — the ones layering
deliberately ignores — makes itself visible: it is real (the code really
does that), it did not move any layer (declared edges did that), so it
shows up running sideways or backwards against the DAG the declarations
drew. A `declared-unused`/`declared+used` edge, by construction, is never
an up-edge except inside a layering SCC (step 2 above), where several
nodes legitimately share one layer.

`sample_libgraph.json` has one: `LibC → LibB` is `used-undeclared`, with
`layer[LibC] == 1` and `layer[LibB] == 2` — `1 <= 2`, an up-edge. It
validates cleanly (`test_used_undeclared_edge_pointing_up_validates_fine`
in `test_contract.py`); the contract's job stops at the document's shape,
and flagging the edge visually is the UI's.

### `edges[]`

One entry per **ordered** `(src, dst)` pair that has any relationship at
all (include, link symbol, or a game's direct usage). Used by the
**Graph** view (colour = class, width = `include_lines`), the **Matrix**
view (cell = `include_lines`), and the **Edge panel** (the `files[]` /
`types[]` / `symbols[]` breakdown is that panel's to-do list).

| Field | Type | Meaning |
|---|---|---|
| `src`, `dst` | string | node names, must exist in `nodes[]` |
| `classes` | list[string] | see "The edge-class rule" above; `[]` for a game edge, else exactly one base class plus optional additive classes |
| `allowlist_kind` | string \| null | set iff `"allowlisted"` is in `classes`; the allowlist line's kind prefix, or `null` for a plain line |
| `lower_bound` | bool | true iff this edge appears in the Sentinel-proved closure of `src` (i.e. `dst` is in the subject lib's `outbound.lower_bound`) |
| `include_lines` | int | number of distinct include lines across `files[]` (or link-only symbol count when there are no includes) |
| `files[]` | list[object] | every `file:line` include across this edge: `{file: path, line: int, raw: string, tags: list[string] from FILE_TAGS}` |
| `types[]` | list[object] | types of `dst` that this edge's includes/uses touch: `{name: string, mode: string from TYPE_MODES, count: int}` |
| `symbols[]` | list[object] | link symbols this edge crosses: `{demangled: string, cls: string \| null}` (`cls` is `null` for a free function) |

### `libs{}`

Keyed by lib name. **The key set is exactly the `nodes[]` entries whose
`kind == "lib"`** — middleware, games and proof nodes have no entry here.
In `sample_libgraph.json` that is `LibA`, `LibB`, `LibC`, `Zenith`.

Each value:

| Field | Type | Meaning |
|---|---|---|
| `inbound` | object | swap-contract **inbound** counts — what a replacement for this lib must provide. Exactly the keys in `INBOUND_FIELDS`, each an int (a plain count; the lists behind each count live in `headers[]`/`types[]`/`symbols[]`/`leakage[]` below) |
| `outbound` | object | swap-contract **outbound** — what this lib may consume |
| `headers[]` | list[object] | every header this lib owns, entry-role or not, including zero-consumer ones |
| `types[]` | list[object] | every type this lib declares at namespace scope, including zero-consumer ones |
| `symbols[]` | list[object] | this lib's owned out-of-line symbols that are referenced from outside it |
| `unreferenced[]` | list[object] | this lib's owned out-of-line symbols that nothing outside references (candidates for `static`/anonymous-namespace/private) |
| `leakage[]` | list[object] | **other** libs' headers that include this lib's headers, and who reaches this lib only that way |
| `sentinel` | string \| null | the `kind=="proof"` node whose subject is this lib, or `null` if none exists |

`inbound` fields (all ints; see `INBOUND_FIELDS`):

| Field | Meaning |
|---|---|
| `entry_headers` | count of this lib's headers with `role == "entry"` |
| `exposed_headers` | count of this lib's headers with `role == "exposed"` |
| `types_embed` | sum, over this lib's `types[]`, of each type's `embed` field — itself a count of distinct **consumer files** that embed that type (not distinct consuming nodes: one node embedding a type from three of its own files contributes 3, not 1) |
| `types_ref` | sum, over this lib's `types[]`, of each type's `ref` field — the same per-type consumer-**file** count, for `ref`-mode uses |
| `link_symbols` | the number of **distinct owned symbols** referenced by at least one other node, computed from the symbol table. **This is not derivable from `symbols[]`**: each `symbols[]` entry is a *class group* (one row per `cls`, i.e. per class or free function — see `symbols[]` below), and its `count` is a *reference count* across possibly many symbols in that group, not a count of distinct symbols. Neither `len(symbols[])` nor a sum of `symbols[].count` equals `link_symbols`. (Binding decision, matches the implementation at `metrics.py:932`.) |
| `consumer_libs` | count of distinct `kind=="lib"` nodes that actually use this lib (an edge into it whose base class is `declared+used` or `used-undeclared`) |
| `consumer_games` | count of distinct `kind=="game"` nodes with a direct edge into this lib |
| `leaking_headers` | count of entries in this lib's `leakage[]` |

`outbound`:

| Field | Type | Meaning |
|---|---|---|
| `edges[]` | list[node name] | the distinct `dst` nodes this lib has an outgoing edge to |
| `symbols_by_target` | object `{dst_node: [{demangled, cls}]}` | this lib's UNDEF symbols, grouped by the node that owns them |
| `lower_bound[]` | list[node name] | the nodes in this lib's Sentinel-proved closure. **Always a list, never null** — `[]` when `sentinel` is `null` |

`headers[]` entries — one per header the lib owns, always, even with zero
consumers (that emptiness is how "unused public header" is represented):

| Field | Type | Meaning |
|---|---|---|
| `path` | string | repo-relative header path |
| `role` | string, `HEADER_ROLES` | `entry` (included directly from outside), `exposed` (in the transitive closure of an entry header but not itself directly included), `unused-public` (not under `Internal/`, never included from outside — move-to-Internal/ candidate), `internal` (under an `Internal/` dir, never included from outside) |
| `declared_internal` | bool | true iff the header sits under an `Internal/` directory — **independent of `role`**. A header can be `role: "entry"` **and** `declared_internal: true` at the same time: that combination is an encapsulation break (something outside the lib reaches into its `Internal/` dir) |
| `consumers[]` | list[object] | `{node: string, files: [path]}` — which nodes include this header directly, and from which of their own files (paths, not a count) |
| `symbols_out_of_line` | int | out-of-line symbol count joined in from this header's declared types |
| `hint` | string \| null | trim hint (`"move to Internal/"`, `"1 consumer"`, `"encapsulation break — ..."`, etc.) or `null` when there's nothing to flag |

`types[]` entries — one per type the lib declares at namespace scope,
always, even with zero consumers:

| Field | Type | Meaning |
|---|---|---|
| `name` | string | bare type identifier |
| `header` | string | the header that declares it |
| `embed`, `ref`, `mention` | int | count of distinct **consumer files** using this type in that mode — a **file** count, not a node count: one node using a type by value from three of its own files contributes 3 to `embed`, not 1. `libs[].inbound.types_embed`/`types_ref` sum these per-type fields across all of a lib's `types[]` |
| `consumers[]` | list[object] | `{node: string, mode: string from TYPE_MODES}` — one entry per external file/consumer relationship (a node using the type in more than one mode may appear more than once) |
| `hint` | string \| null | trim hint (e.g. `"embedded by value in N consumers — consider forward-declare+pointer"`, `"no external consumers"`) or `null` |

`symbols[]` entries — **one row per `cls` (class-group), not one row per
symbol.** Each row rolls up every one of this lib's owned out-of-line
symbols that share the same outermost class (or, for `cls: null`, every
referenced free function considered as its own group) and that are
referenced from outside the lib:

| Field | Type | Meaning |
|---|---|---|
| `cls` | string \| null | outermost class name, namespace-qualifiers stripped; `null` for a free function |
| `header` | string | the header (via the type index) this symbol's class is declared in |
| `count` | int | number of external **references** to symbols in this group — a reference count, not a count of distinct symbols in the group (a group with 3 externally-used members referenced once each has `count: 3`, same as 1 member referenced 3 times) |
| `referenced_by[]` | list[node name] | which nodes reference something in this group |

`libs[].inbound.link_symbols` (above) is **not** `len(symbols[])` (that
would count class-groups, not symbols) and **not** a sum of `count` (that
double-counts a symbol referenced by more than one node) — it is measured
independently, from the symbol table itself, as the number of distinct
owned symbols with at least one external reference.

`unreferenced[]` entries — owned out-of-line symbols nothing outside
references:

| Field | Type | Meaning |
|---|---|---|
| `demangled` | string | full demangled symbol name |
| `cls` | string \| null | outermost class name, or `null` for a free function |

`leakage[]` entries — another lib's header that re-exposes this lib's
types/headers onward, and who reaches this lib only that way:

| Field | Type | Meaning |
|---|---|---|
| `header` | string | the **foreign** header (owned by `via_node`) that leaks this lib outward |
| `via_node` | string | the node that owns `header` |
| `downstream_via_header[]` | list[node name] | nodes that reach this lib only by including `header` (a header-level leak) |
| `downstream_via_cpp_only[]` | list[node name] | nodes that reach this lib only from a `.cpp` including `header` (never from one of their own headers) — counted separately from `downstream_via_header` per the plan's leakage rule |

## `delta`

`null` when no `--baseline` was given. Otherwise an object:

```
{ "<lib name>": { "<inbound field>": <±int>, ... }, ... }
```

- **Keys are the union of the current run's lib names and the baseline
  run's lib names** — not just `nodes[]` from this document. A lib present
  only in the baseline (i.e. named in `meta.delta_removed`) appears here
  with every field equal to `-baseline_value`; a lib present only now
  (named in `meta.delta_added`) appears with every field equal to
  `+current_value`. **`contract.py` therefore allows `delta` keys that are
  not in `nodes[]`** — do not add a referential-integrity check there.
- Each value is an object with **exactly** the keys in `INBOUND_FIELDS`
  (the same field set as `libs[].inbound`), each an int (may be negative,
  zero, or positive).
- At least two libs must have a non-trivial entry for the sample to
  exercise this (see `sample_libgraph.json`: `LibA` changed, `LibC` was
  added, `LibOld` was removed).

## Decisions made where the plan/brief left a type implicit

These are pinned here (and enforced in `contract.py`) because two other
agents build against this document without re-deriving them:

- `inbound.*` fields are **integers** (plain counts).
- `outbound.edges[]` is a **list of destination node names** (strings).
- `outbound.lower_bound[]` is a **list of node names** from the Sentinel
  closure, always a list (never `null`).
- `symbols[].referenced_by[]` is a **list of node names**.
- `edges[].include_lines` is an **int**.
- `edges[].types[].count` is an **int**.
- `hint` fields (`headers[].hint`, `types[].hint`) are **string or null**.
- `meta.generated` is an **ISO-8601 UTC string** (`YYYY-MM-DDTHH:MM:SSZ`).
- `meta.repo_head` is a **git SHA string** (not validated for length/hex —
  any string is accepted, since a shortened or symbolic ref is still
  meaningful to a human reading the report).
- `edges[].files[].tags[]` values are drawn from `FILE_TAGS` = `test`,
  `pch`, `conditional`.
- `libs[].sentinel`, when non-null, must name a `kind == "proof"` node.
- A node's `inbound_size` is `0` for every non-`lib` kind, since only lib
  nodes have a `libs{}` entry to sum.

## What `contract.py`'s `validate()` checks

- Every required key at every level listed above is present.
- Every value's type matches this document (strict: a JSON `true`/`false`
  is never accepted where an int is expected, even though Python's `bool`
  is an `int` subclass).
- Enum membership: `kind`, `classes` (both the base/additive split and the
  game-edge `[]` rule), `role`, `mode`, `tags`.
- Referential integrity: every edge's `src`/`dst`, every `libs` key, every
  `consumers[].node`, `symbols[].referenced_by[]`, `leakage[].via_node` /
  `downstream_via_header[]` / `downstream_via_cpp_only[]`,
  `outbound.edges[]` / `outbound.lower_bound[]`, and `types[].consumers[].node`
  all name a node that exists in `nodes[]`.
- `libs{}` keys are **exactly** the `kind=="lib"` nodes — neither missing
  one nor carrying an extra.
- `delta` shape: `null`, or `{lib: {field: int}}` with `field` drawn
  exactly from `INBOUND_FIELDS`; `delta` keys are **not** required to
  exist in `nodes[]`.
- **`layer` consistency**: every node's `layer` is recomputed from
  `edges[]`/`libs{}` per "How `layer` is computed" above, and compared
  against the stored value — best-effort per node, skipping (not failing)
  any node the recomputation can't resolve unambiguously. A mismatch is
  reported naming both values. **There is deliberately no "every `in-cycle`
  edge has equal `layer`" check** — that rule was correct only while
  `layer` followed real use; now that it follows declared intent, two
  `in-cycle` nodes routinely sit on different layers (see "The edge-class
  rule" above), and checking for equality would reject correct documents.
- `meta.symbols` is required and must be a bool; `meta.health.missing_link_inputs`
  is required and must be a list of strings. Both are checked unconditionally
  (never optional, never inferred from whether other fields are zero).

It does **not** attempt to re-derive whether the classification itself is
*correct* (that's `metrics.py`'s job and `test_metrics.py`'s coverage) —
only that the document's *shape* is self-consistent and matches this
schema.
