# Zenith2 decision log

This log records every settled decision of the Zenith2 programme, every open decision together with the rule that will settle it, and the session rulings that govern how the programme's documentation and board are produced. The /tick orchestrator pastes the entries that govern a ticket verbatim into the worker prompt, so every entry is written to stand on its own.

## How this log is written

### Where the log lives and how entries are added
- The log is `Zenith2/Docs/DecisionLog.md`. Every Z2 category in `zagent.project.json` declares, identically, `decisionLog: Zenith2/Docs/DecisionLog.md` and `decisionIdPrefix: Z2-D-` (Z2-D-086). That file is what the rulings scan (`Tools/Vault/cli/client.mjs:519-531`) and the review's decision term (`compat.mjs:704-713`) read.
- A ruling is recorded by appending an entry to this file **in the diff of the ticket that makes the ruling** (Z2-D-097): a `### Z2-D-NNN <Title>` heading carrying the next free ID, followed by the fields of the entry template below. The ticket cites that ID in its Goal or DoD (Z2-D-098). The entry is reviewed with the rest of the diff and reaches workers from the next claim after it lands.
- The next free ID is one above the highest `Z2-D-NNN` entry heading in this file. The seed holds the settled decisions, the open decisions and the session rulings; its highest entry closes the "Session rulings" section. An ID is never reused, skipped or renumbered.
<!-- elaboration -->
- The next free ID is read from this file at the time of the diff. A collision — for example between a human-driven ruling ticket and a lane ticket that applies an approved flagged proposal — is caught by the coverage gate step's duplicate or non-sequential heading failure (Z2-D-067), never by the lane lock, which does not bind human-driven tickets.
<!-- elaboration -->
- This file is never split. The rulings scan and the review's decision term read the single `decisionLog` path (`client.mjs:472, 519-524`; `compat.mjs:167-171, 704`), so an entry in any second file would be invisible to both. The 2,000-line cap of Z2-D-121 does not apply to this file, because a worker receives entries by paste (`build_prompt.ps1 -Rulings`) and never reads this file whole. The exemption is recorded here, not assumed; an array-valued `decisionLog` is a Stage 0 PROC candidate noted under "Open decisions".
- `zagent decide` does **not** write this file: it appends a knowledge row to `metadata.md` and mints its own IDs from ticket-body citations and its own rows, ignoring this file (`write.mjs:1734-1792, 1817-1858`). A ticket may mirror a ruling into that knowledge database with `zagent decide`, passing an explicit `"ID": "Z2-D-NNN"` equal to the appended heading's ID (`write.mjs:1787`); the mirror is optional and never the record. The seed filer never calls `decide`, because it would add `metadata.md` to the seed diff.
- A ruling that settles an open decision also edits that decision's `### D-NAME` entry under "Open decisions" in the same diff: its **Status** becomes `Ruled as Z2-D-NNN`, and the rest of the entry stays as the record of what was asked.
- Every ticket that changes a contract, API or design edits the governing design-doc section in the same diff (Z2-D-085).
- New entries are appended at the end of the file, under "Decisions ruled after the seed".

### Entry template
Every entry, settled, open or ruled, uses this shape. Fields are bold labels on bullets; a field with several clauses nests them one per line.

```
### <Z2-D ID> <Title that reuses the nouns of the tickets it governs>
- **Status:** Settled · **Date:** YYYY-MM-DD
- **Context:** the problem, in two to six sentences, including the evidence that made it a problem.
- **Decision:** the ruling, complete: every clause, number, name and exception. Never a summary.
- **Alternatives considered:** at least two, each with the reason it was rejected.
- **Consequences:** what the ruling obliges, and the doc sections that specify it, cited as
  `Zenith2/Docs/<Doc>.md → "<## heading>"`.
- **Nouns:** the exact nouns a governed ticket uses, comma-separated: at least one token that
  matches IDENTIFIER_SCAN_RE (a Prefix_Name or a PascalCase word with two or more capitals),
  reused in the heading where the title allows, plus at least three distinctive words of six
  letters or more.
- **Related:** the other Z2-D IDs this entry depends on or constrains.
- **Research:** (a ruling of a measured decision, profiles RA, RS, RJ, RB and RP) the profile, the
  study path with its version and SHA-256, every run used, and the report path.
- **Analysis:** (the same rulings) the study-level rule outcome over the ruling's runs, as the
  report's generated analysis block gives it, each primary comparison's verdict per platform, then
  `default change:` followed by none or the follow-up ticket.
- **Revalidation:** (every research ruling; a qualitative ruling carries this field alone) the
  trigger tokens.
```

The heading line is `###`, a space, the entry's own `Z2-D-` ID with its three digits, a space and the title. An open decision uses `### D-NAME` as its heading and the fields **Status** (Open) · **Stage** · **Question** · **Alternatives** · **Acceptance rule** · **Consumers blocked** · **Harness and ruling split** · **Nouns** · **Related**.

### Heading and noun rule
- How an entry reaches a worker: at claim, the client scores every block of the category's `decisionLog` against the ticket's **Goal and DoD text only** (`matchGoverningRulings`, `Tools/Vault/cli/legacy-pure/ticket.mjs:353-390`, called from `client.mjs:459-531`; the text read is `client.mjs:484-489`) and writes up to 25 candidates to `.zagent/run/<KEY>/rulings.md`, ordered by score with ties in ascending ID order (`ticket.mjs:380`). A list of 25 is saturated, not complete — a high-numbered governing entry tied at the floor is the first dropped — so the orchestrator then looks up every ID cited in the Goal or DoD directly in this file. No other section of the ticket — including the `Decisions:` line — is scanned.
- Blocks: the file is split at every heading, and each block is keyed by every `Z2-D-` ID it contains (`decisionLogEntries`, `ticket.mjs:266-284`). So `rulings.md` is a recall-tuned candidate list: every block that mentions an ID — a `Related:` line, an open-decision entry, this section — is a candidate for that ID. The orchestrator pastes only the `### Z2-D-NNN` entry itself, never a candidate block that is not an entry. The snippet printed under `## Z2-D-NNN` in `rulings.md` is the highest-scoring block that *mentions* that ID (`ticket.mjs:380-388`), often another entry's `Related:` line; the orchestrator locates the entry by its own `### Z2-D-NNN` heading in this file, never by the snippet.
- Identifiers: a token counts as an identifier only if it matches `IDENTIFIER_SCAN_RE` (`ticket.mjs:224`), which admits two shapes — a `Prefix_Name` token containing an underscore (`Zenith_Owned`, `VIEW_KIND_STEREO`, `BRANCH_MOVED`, `tick_gates`) and a PascalCase word with two or more capitals (`ForbiddenCheck`, `DecisionLog`, `HarnessParity`). `game.json`, `linux-tsan`, `D-SLANG`, `Coverage.ps1`, `File-Backlog.ps1`, `laneAdd`, `operations.mjs`, `--project Z2`, `z2-board-seed`, `Spec-Maturity` and all-capital words such as `TASK` are not identifiers.
- Scoring (`ticket.mjs:202-204, 244-255, 344-390`): each identifier the block shares with the Goal and DoD scores 2 (`IDENTIFIER_WEIGHT`); a direct citation of the block's `Z2-D-NNN` scores 2, once; nouns are case-folded words of six letters or more, minus a stopword list, each worth its rarity in (0, 1], and at most the 3 rarest count. A block becomes a `rulings.md` candidate at a score of 4 (`RULINGS_FILE_MIN_SCORE`); the review's decision term uses the same scoring at 8, computed live (`compat.mjs:704-713`).
- Consequence: a citation alone scores 2 and never selects an entry. A ticket author therefore does both: cites each governing `Z2-D-NNN` inside the Goal or DoD, **and** names in the same text at least one identifier from that entry's **Nouns** line (or, where the entry has none a ticket would use, at least two of its distinctive words that occur in no other block of this file, `Related:` lines, open-decision entries and the "How this log is written" section included, because rarity is counted over blocks (`nounRarity`, `ticket.mjs:308-313`)) (Z2-D-098). The `Decisions:` line is kept for humans and the coverage checker.
- Every entry's **Nouns** line therefore carries at least one identifier that matches `IDENTIFIER_SCAN_RE`. An entry that governs code names the type, function or constant the code uses (`Zenith_*`, `Flux_*`, a PascalCase type); where the governing type does not exist yet, the entry names the type the governed tickets will create, consistent with `Zenith2/Docs/Architecture.md → "Purpose and scope"` and the other core docs.
<!-- elaboration -->
- An entry that governs process rather than code names the real symbols the governed ticket touches — an underscored script or constant name (`tick_gates`, `build_lock`, `MAIN_NOT_ON_BASE`) — or, where none exists, the `Test_*` name of the gate test or checker fixture that proves the rule; the ticket implementing that check uses the same name. A ruling that governs only the documentation-and-filing session itself (no ticket will cite it) keeps a `Test_*` name that no ticket is expected to create, only so the template holds. Brief-mandated settled titles are never changed to carry an identifier.
- Each governing entry is written whole, heading line included, to `.zagent/run/<KEY>/rulings/Z2-D-NNN.md`, one file per entry, and those paths are passed to `build_prompt.ps1 -Rulings`, which inlines each file under `### <file name>` (`build_prompt.ps1:349-355`) beneath the worker prompt's "Rulings that govern this work" heading, so the entry's ID heads it.
- An entry never says "see above" or "as before". It names other entries by ID, because the worker sees only the entries that were pasted.
- Headings are plain text: no trailing punctuation, no backticks, no bold, no numbering prefix. The heading text is the anchor.

### Changing a settled decision
- A settled decision is changed only through a flagged proposal (Z2-D-022): a row stating the concern, the evidence, the proposal and the trade-off, listed in `Zenith2/Docs/Roadmap.md → "Flagged proposals"`. Nothing changes until the human approves it.
<!-- elaboration -->
- An approved proposal is recorded as a new entry appended in the diff of the ticket that applies it, with the next free ID; its **Decision** states what it supersedes. The superseded entry's **Status** becomes `Superseded by Z2-D-NNN`. From the docs and config commit (A8a, Z2-D-115) onward, that status line is the only edit a settled entry receives; its text stays, so a ticket filed against it can still be revalidated against what it cited. Before A8a, entries are revised by the review rounds.
- A budget change is a decision plus a workload version bump (Z2-D-012). A complexity-profile exception, a `ForbiddenCheck` allowlist entry, a sanitizer suppression and an `SV_Depth` allowlist entry each cite the decision ID that justifies it.

### Ruling an open decision
- An open decision has no ID until it is ruled. Docs and tickets cite it by name with its stage, for example "D-SLANG (open, Stage 1)".
- A decision that needs measurement is split into an ordinary harness ticket and a `needs-human` ruling ticket (Z2-D-084). The ruling ticket's diff appends the ruling to this file with the next free ID and marks the `### D-NAME` entry `Ruled as Z2-D-NNN`. Its dependents are `blocks`-linked from the ruling ticket, and a spec-revalidation ticket scoped to (area, stage, decision set) restores `Spec-Maturity: ready` on them afterwards (Z2-D-129).
- The rulings scan keys blocks by `Z2-D-` IDs only, so an open decision is never listed under its own name and yields no candidate of its own until it is ruled. A ticket governed by an open decision cites `Zenith2/Docs/DecisionLog.md → "Open decisions"` and the D-NAME in its `Cites:`, names the D-NAME with its stage in its Goal, and the orchestrator writes that `### D-NAME` entry whole to `.zagent/run/<KEY>/rulings/<D-NAME>.md` and passes it to `-Rulings` like a ruling.
- `human-gate` is never used for a Z2 decision (Z2-D-126).
- A ruling of a measured decision (profiles RA, RS, RJ, RB and RP of `Zenith2/Docs/Design/RESEARCH/Methodology.md` → "Measurement profiles") carries the **Research**, **Analysis** and **Revalidation** fields of the entry template after **Related**, and a qualitative ruling carries **Revalidation** alone (`Zenith2/Docs/Design/RESEARCH/Formats.md` → "DecisionLog research fields"). The ruling ticket generates them with `New-Zenith2ResearchDecisionFields`, and the `Test` step's `Pwsh_Research_CheckDecisionLog` checks them.

### Statuses
<!-- elaboration -->
| Status | Meaning |
|---|---|
| Settled | In force. Changed only through a flagged proposal |
| Open | Not yet ruled; its dependents are blocked and carry `Spec-Maturity: decision-pending` |
| Ruled as Z2-D-NNN | An open decision that has been ruled; the ruling is the named entry |
| Superseded by Z2-D-NNN | A settled decision replaced through an approved flagged proposal |
| Provisional | A custom-infrastructure justification record (Z2-D-041) appended by the diff that adds the primitive, whose measured evidence is pending its harness run; its **Decision** reads "adopt provisionally". The `needs-human` ruling ticket completes the same entry in place and rewrites the status to `Settled`, or, when the acceptance criteria fail, keeps the entry as the record of the failed criterion with `Decision: reject in favour of <alternative>` and files the maintenance ticket that replaces the primitive; in both outcomes the status becomes `Settled`, and a rejected record's Decision line names the alternative that replaces it; a record never reads `Settled` while its measured evidence is pending (`Zenith2/Docs/Contracts.md → "Containers and standard library policy"`) |

## Settled decisions

### Z2-D-001 Build system: CMake presets, Ninja Multi-Config, globbed sources, one library per backend
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith builds through a 12-configuration Sharpmake build whose generated solutions are gitignored and silently go stale, and it needed a dedicated membership checker to prove that every compiled source belongs to exactly one project. Explicit source lists and per-configuration source excludes were the mechanisms that drifted. Zenith2 needs ownership that holds by construction, one build tree per checkout, and link-time backend selection.
- **Decision:**
  - The build is CMake with presets, using the Ninja Multi-Config generator. Ninja's automatic CMake re-run is the only regeneration; there is no regenerate step and no `.vcxproj` file.
  - Sources are found by `file(GLOB_RECURSE … CONFIGURE_DEPENDS)`. There are no explicit source lists. Each target globs its own directory, so ownership follows directory structure by construction.
  - Platform sources live under `Internal/Windows/`, `Internal/Android/` or `Internal/Linux/`, and the shared `zenith_add_library()` helper filters those per platform. `Tests/` feeds the separate test target.
  - `zenith_add_library()` also declares each target's supported (platform × configuration) matrix, `ZENITH_SUPPORTED` (GPU libraries exclude `linux-tsan`), and writes each target's declared direct dependencies to `Zenith2/out/<checkout-hash>/declared_deps.json` for libgraph.
  - Every backend (ECS, RHI, physics, compression) is its own library target. The selected backend is chosen at link time by the consumer.
  - The presets are `win-debug`, `win-release`, `win-tools`, `win-asan`, `linux-tsan`, `android-arm64` and `android-x86_64` (emulator smoke only). Host tools reach Android builds through `ZENITH_HOST_TOOLS_DIR`. Presets and the WSL `binaryDir` are keyed per checkout path, so lanes never share a build tree.
- **Alternatives considered:** (1) Keep Sharpmake with explicit source lists — rejected: it is the failure the audit found (12 configurations, stale generated files, a membership checker needed to prove what globbing gives by construction). (2) CMake with explicit source lists — rejected: lists drift from the tree, and ownership would again have to be checked rather than constructed. (3) CMake with the Visual Studio or a single-config Ninja generator — rejected: the first reintroduces `.vcxproj` files, the second needs one build tree per configuration.
- **Consequences:** The `SourceLayout` gate step checks the directory convention (every `.cpp/.c/.h/.hpp/.inl` under `Engine/`, `Tools/`, `Player/`, `Games/` or `Benchmarks/`, tracked or untracked, sits in a globbed directory; platform code sits under `Internal/<Platform>/`; test code sits under `Tests/`), and `CompileCoverage` checks that every source produces an object in each configuration of its target's matrix. Specified in `Zenith2/Docs/Architecture.md → "Build system and presets"`, `Zenith2/Docs/Architecture.md → "Repository layout"` and `Zenith2/Docs/Contracts.md → "No layout-changing conditionals"`.
- **Nouns:** CMake, CMakePresets.json, Ninja Multi-Config, GLOB_RECURSE, CONFIGURE_DEPENDS, zenith_add_library, ZENITH_SUPPORTED, declared_deps.json, Internal/Windows, Internal/Android, Internal/Linux, Tests/, backend library target, preset, win-debug, win-release, win-tools, win-asan, linux-tsan, android-arm64, android-x86_64, ZENITH_HOST_TOOLS_DIR, binaryDir, SourceLayout, CompileCoverage
- **Related:** Z2-D-002, Z2-D-011, Z2-D-024, Z2-D-028, Z2-D-042
### Z2-D-002 Games live in-repo under Zenith2/Games, statically link the engine, contain no C++
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** An engine whose games live outside its build cannot hold those games to its gates, and an installed SDK or plugin boundary creates a binary-compatibility surface. Because Zenith2 games define components in data and gameplay in Luau, a game needs no C++ of its own.
- **Decision:**
  - Every game lives in `Zenith2/Games/<Name>/`, in this repo, and statically links the engine. Its layout is `game.json`, `CMakeLists.txt`, `Scripts/`, `Scenes/`, `Schemas/` and `Assets/`.
  - A game's `CMakeLists.txt` is one call, `zenith_add_game(<Name>)`, which reads `game.json` at configure time with CMake's `string(JSON)`: the optional modules, the ECS and physics backends, the target platforms and the content roots.
  - A game contains no C++. Its executable is the shared `Player/` main, statically linked with the engine and the modules it selected. On Android the shared Gradle template packages that executable (as a `.so`) with the game's cooked content.
  - There is no installed SDK, no external or out-of-repo project and no dynamic plugin loading. Every game is built by the same CMake tree, so the gates, libgraph and the complexity gate cover every game automatically.
  - Project creation is a CLI (`Tools/NewGame`) plus an editor wizard, both scaffolding `Games/<Name>/` with no C++.
  - The repo's games are the focused workload games `WorkloadIndoor`, `WorkloadOutdoor`, `WorkloadCharacters`, `WorkloadDense` and `WorkloadUI2D`, and the three flagship slices `OpenWorldSlice`, `CinematicSlice` and `Game2D`.
  - Amended 2026-09-25 (extension of this ruling, not a new decision): the repository's games are those eight plus `Games/Minimal`, the Stage 2 proving game that builds on Windows and Android and proves the in-repo game model (`zenith_add_game`, `game.json`, static-module selection and backend selection).
- **Alternatives considered:** (1) An installed SDK with external game projects — rejected: out-of-repo games escape the gates and create a binary-compatibility surface. (2) Games with their own C++ gameplay modules, as in Zenith — rejected: data-defined components and Luau cover gameplay, and game C++ would reopen every contract per game. (3) Games as dynamically loaded libraries — rejected: binary compatibility and unloading questions with no benefit for in-repo games.
- **Consequences:** Data-defined components (Z2-D-020) and generated `.d.luau` are required, because a game cannot register a C++ type. Specified in `Zenith2/Docs/Architecture.md → "Engine modules and game projects"`, `Zenith2/Docs/Architecture.md → "Composition root and Player"` and `Zenith2/Docs/Architecture.md → "Repository layout"`; the module-system catalogue is `Zenith2/Docs/Requirements/MOD.md`.
- **Nouns:** Zenith2/Games, game.json, zenith_add_game, string(JSON), CMakeLists.txt, Player, shared Player main, Gradle template, Tools/NewGame, new-game wizard, content roots, Scripts/, Scenes/, Schemas/, Assets/, WorkloadIndoor, WorkloadOutdoor, WorkloadCharacters, WorkloadDense, WorkloadUI2D, OpenWorldSlice, CinematicSlice, Game2D, Minimal, Games/Minimal
- **Related:** Z2-D-020, Z2-D-021, Z2-D-024, Z2-D-075, Z2-D-081
### Z2-D-003 One neutral Zenith_Physics API with Jolt and PhysX backends
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith integrates Jolt directly, so nothing proves that the physics boundary is swappable, and a single backend gives no comparison point for determinism or performance. Zenith2 treats every heavy middleware family as a replaceable backend behind a neutral API.
- **Decision:**
  - One neutral `Zenith_Physics` API in the `ZenithPhysics` target, with two backends as separate library targets, `ZenithPhysics_Jolt` and `ZenithPhysics_PhysX`, each exporting its own factory (for example `Zenith_CreatePhysics_PhysX`). The API target exports no factory.
  - A shared conformance suite runs against both backends. Determinism is promised per backend, never across backends.
  - A game links the backend it selects in `game.json`, at link time. The conformance executables link every backend.
  - Whether PhysX runs on Android is D-SEL-PHYSX-ANDROID (open, Stage 1). If it cannot, the capability matrix makes Jolt the Android backend.
  - Both backends and the conformance suite are delivered in Stage 3.
- **Alternatives considered:** (1) Jolt only, as in Zenith — rejected: no swap-contract evidence and no second backend to validate the neutral API or the conformance suite. (2) PhysX only — rejected: its Android arm64 viability is unproven, and one backend leaves the API unproven. (3) A closed commercial physics engine — rejected: no source to build under the sanitizer presets (Z2-D-011).
- **Consequences:** Third-party physics headers never reach a public header (libgraph); the libgraph swap-contract report is the evidence of swappability. Specified in `Zenith2/Docs/Architecture.md → "Library dependency graph"`, `Zenith2/Docs/Architecture.md → "Named selection decisions"` and `Zenith2/Docs/Contracts.md → "API boundary"`; the catalogue is `Zenith2/Docs/Requirements/PHYS.md`.
- **Nouns:** Zenith_Physics, ZenithPhysics, ZenithPhysics_Jolt, ZenithPhysics_PhysX, Zenith_CreatePhysics_PhysX, Zenith_CreatePhysics_Jolt, physics backend, physics conformance suite, per-backend determinism, Jolt, PhysX 5, capability matrix, D-SEL-PHYSX-ANDROID
- **Related:** Z2-D-019, Z2-D-024, Z2-D-042, Z2-D-082
### Z2-D-004 One neutral Zenith_Compression API with zlib and zstd backends
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Cooked data, archives and packages need compression whose output is reproducible, and the codec must be recorded so a reader never guesses. Treating compression as a backend family also proves the backend pattern on its smallest instance.
- **Decision:**
  - One neutral `Zenith_Compression` API in `ZenithCompression`, with `ZenithCompression_Zlib` and `ZenithCompression_Zstd` as separate library targets. Both backends are required.
  - Output is deterministic per codec. The cooked-format headers record the codec ID.
  - A game always links both zlib and zstd, and the codec is chosen per asset.
  - The API has streaming and one-shot modes, a conformance suite and benchmarks. It is delivered in Stage 2.
- **Alternatives considered:** (1) zstd only — rejected: zlib/deflate is the interchange codec for formats the pipeline reads and writes, and one backend leaves the neutral API unproven. (2) LZ4 or a proprietary codec — rejected: LZ4's ratio is lower for cooked content, and a proprietary codec cannot be pinned and built from source for the sanitizer presets.
- **Consequences:** The cook records the codec per asset, and the runtime refuses a codec it does not know. Specified in `Zenith2/Docs/Architecture.md → "Library dependency graph"` and `Zenith2/Docs/Architecture.md → "Third-party dependencies"`; the catalogue is `Zenith2/Docs/Requirements/COMPRESS.md`.
- **Nouns:** Zenith_Compression, ZenithCompression, ZenithCompression_Zlib, ZenithCompression_Zstd, zlib, zstd, codec ID, cooked header, streaming compression, one-shot compression, deterministic output, compression conformance suite, compression benchmark
- **Related:** Z2-D-038, Z2-D-042, Z2-D-082
### Z2-D-005 On-screen profiler required in game and editor, runtime toggle
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Performance problems in a multi-threaded frame graph and a GPU render graph are invisible without per-node and per-pass timing on the device itself, and a build-flag profiler is never present in the build that shows the problem. Tracy is excellent for deep capture but needs a host connection.
- **Decision:**
  - An in-engine on-screen profiler is required in the game and in the editor, on Windows and on the S24.
  - The overlay shows: the CPU frame-graph timeline per worker; GPU time per render-graph pass, from timestamp queries the graph inserts automatically; frame-time history with hitch markers; memory (heap, pools, GPU heaps, aliasing savings); counters (draws, culled instances, streaming, uploads); and Luau time and GC.
  - It is a runtime toggle, not a build flag. It is available in development builds and disabled by default in shipping builds.
  - Tracy stays for deep capture.
  - The core (CPU graph timeline, frame-time history, counters, memory) lands in Stage 2, with GPU per-pass timings as soon as the render graph lands.
- **Alternatives considered:** (1) Tracy only — rejected: it needs a connected host and is awkward on the S24 in the field. (2) An overlay behind a build flag — rejected: the flagged build is never the build that shows the problem. (3) External GPU tools only (RenderDoc, PIX, vendor tools) — rejected: no CPU graph or Luau view, and no always-available counters.
- **Consequences:** Every CPU graph node and sub-job emits a zone; every render-graph pass gets timestamps; the missing-PSO fallback counters, the HiZ per-view per-phase counts and transient memory with and without aliasing are shown. Specified in `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`, `Zenith2/Docs/Rendering.md → "Render graph"` and `Zenith2/Docs/Budgets.md → "Measured in every workload run"`; the design is `Zenith2/Docs/Design/DEVT.md`.
- **Nouns:** on-screen profiler, ZenithProfiling, profiler overlay, CPU frame-graph timeline, GPU timestamp query, per-pass GPU time, frame-time history, hitch marker, memory view, aliasing savings, counters, Luau time, Luau GC, runtime toggle, Tracy
- **Related:** Z2-D-006, Z2-D-046, Z2-D-055, Z2-D-057
### Z2-D-006 The CPU frame is a DAG built on the shared ZenithGraph core; no fibers
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's task system orders work by hand, so races and ordering bugs are found at runtime rather than when the schedule is built. Implicit ordering also makes determinism and pipelining hard to reason about, and a fiber system complicates debuggers, TSan, Android and Luau VMs.
- **Decision:**
  - Nodes are systems and engine stages: input, scripts, physics step, animation, transform propagation, AI, audio update, render extraction, GPU-graph build and record, and submit.
  - Each node declares its access: ECS component read/write sets; world resources (physics world, navmesh, audio mixer, render world); and explicit `Before`/`After` constraints. Edges are derived automatically from read-after-write, write-after-read and write-after-write hazards.
  - The graph is compiled once into a static schedule (a topological order, per-node dependency counts and critical-path priorities), cached, and recompiled only when the system set changes (a module or world added or removed, or a hot-reloaded script system). Cycle diagnostics name the conflicting accesses.
  - At runtime a node may fan out parallel-for jobs over ECS chunks (dynamic sub-jobs). They are joined before the node completes, so successors see a finished node.
  - No fibers and no blocking waits: a job never waits on another job; anything that looks like waiting is an edge or a continuation job.
  - The scheduler is a work-stealing pool on `Zenith_Multithreading`, with per-node thread affinity (the main thread for the SDL platform pump and for the world's main Luau VM; a render-submit thread; per-worker chunk-system VMs belong to their workers and need no pinning), bounded queues, and orderly shutdown (drain the frame, then join the workers in reverse dependency order).
  - Frames are pipelined through render extraction (Z2-D-050). The determinism ordering rules apply to both the node schedule and the sub-job merge. Every node and sub-job emits a profiling zone.
  - The CPU frame graph is built on the shared `ZenithGraph` core, which the GPU render graph also uses.
- **Alternatives considered:** (1) A fiber job system (Naughty Dog) — considered and rejected: it complicates debuggers, TSan, Android and the Luau VMs. (2) Fixed tick groups without access-derived edges (UE tick groups alone) — rejected: ordering stays implicit and races surface at runtime. (3) A task system with blocking waits, as in Zenith — rejected: blocked workers idle, and wait chains deadlock.
- **Consequences:** Prior art is UE Task Graph and `UE::Tasks`, UE tick groups, Frostbite's job graph and FrameGraph, Destiny's job graph with extraction, Unity DOTS `JobHandle`, and Bevy and flecs access-derived scheduling. Specified in `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"` and `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`; the concurrency rules are `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`.
- **Nouns:** CPU frame graph, ZenithGraph, ZenithJobs, frame graph node, access declaration, Before, After, hazard edge, compiled schedule, critical-path priority, cycle diagnostic, dynamic sub-job, parallel-for, continuation job, fiber, blocking wait, work-stealing scheduler, Zenith_Multithreading, thread affinity, render-submit thread, bounded queue, orderly shutdown
- **Related:** Z2-D-019, Z2-D-050, Z2-D-055, Z2-D-080
### Z2-D-007 libgraph and the complexity gate are hard CI gates
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's audit found include cycles, no API/Internal separation and a global service locator used about 4,250 times; its libgraph and complexity analyses were reports, not gates, so the structure drifted. Zenith2 makes both hard failures from the first scaffold.
- **Decision:**
  - libgraph gets a generic CMake File API front end (a Stage 0 PROC ticket, alongside its Sharpmake front end). It reads the declared direct dependencies from `declared_deps.json`, because the File API reports transitive static-library dependencies that would hide used-but-undeclared edges, and it runs from a `win-debug` build with `/Gy-` so the link surface can be measured.
  - libgraph fails on: an `Internal/` encapsulation break; a used-but-undeclared edge; any cycle; third-party or `Internal/` headers leaking into a public header; Slang or DXC symbols in a runtime target; and inbound-surface growth against the committed baseline `Zenith2/Tools/Gates/libgraph_baseline.json` unless covered by API review. Its swap-contract report, written per backend family, is the evidence that each backend is swappable.
  - The complexity gate runs `analyze_code_complexity.py --profile zenith2 --profile-file Zenith2/Tools/Gates/complexity_profile.json`. Its function complexity (`max-cc`, `max-cognitive`), nesting, function length (`max-loc`), `max-forbidden-tokens` and file length (`max-file-loc`, a generic key a Stage 0 ticket adds) thresholds are hard failures.
  - Thresholds are calibrated from measured distributions. Any exception is a profile entry carrying a decision ID. Reviewers reject artificial fragmentation (splitting a coherent function only to pass a limit) and prefer a justified exception.
  - `include_roots` is added to the analyzer, with a test, in Stage 0.
- **Alternatives considered:** (1) Report-only analysis, as in Zenith — rejected: the audit shows reports do not stop drift. (2) Checking CMake link declarations only — rejected: misses include and symbol use, generated code and header leakage. (3) A different complexity tool — rejected: the existing analyzer already supports `--profile-file`, and a new tool adds a dependency without adding a check.
- **Consequences:** The `Libgraph` and `Complexity` gate steps are in the base `Zenith2` category. Specified in `Zenith2/Docs/Contracts.md → "Layering DAG"`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` and `Zenith2/Docs/Contracts.md → "Enforceability matrix"`.
- **Nouns:** libgraph, CMake File API, declared_deps.json, libgraph_baseline.json, swap-contract report, inbound surface, Internal/ encapsulation, header leakage, used-but-undeclared edge, Libgraph step, Complexity step, analyze_code_complexity.py, complexity_profile.json, max-cc, max-cognitive, max-loc, max-file-loc, max-forbidden-tokens, include_roots, /Gy-
- **Related:** Z2-D-028, Z2-D-042, Z2-D-049
### Z2-D-008 Reproducibility manifest with pinned toolchain hashes and a setup script
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Evidence is only comparable when the toolchain that produced it is known, and a clean machine must reproduce any build. Zenith relies on whatever toolchain happens to be installed.
- **Decision:**
  - A pinned manifest with hashes covers: MSVC, LLVM (clang-tidy and clang), CMake, Ninja, Slang, DXC (offline shader tool only), the WARP NuGet package, lavapipe, NDK r27c, JDK 17, Gradle, AGP, and the Android SDK platform and build-tools.
  - A clean-machine setup script (`Zenith2/Tools/Setup`) installs and verifies them. For `linux-tsan` it also sets `vm.mmap_rnd_bits` to 28 or less in the pinned WSL2 distro.
  - The manifest, the toolchain pins, the setup script and sccache are a Stage 0 ticket, because the scaffold needs them. The pins live with the third-party pins in `ThirdParty/manifest.json`.
- **Alternatives considered:** (1) Version checks against developer-installed tools — rejected: a version string does not prove the binary, and nothing reproduces the machine. (2) Container images for every build — rejected: MSVC, GPU and Android device work do not run in a container on this host. (3) A package manager (vcpkg or Conan) for the toolchain — rejected: it pins libraries, not MSVC, the NDK, the JDK or the GPU runtimes.
- **Consequences:** The toolchain and manifest hashes are part of every evidence fingerprint (Z2-D-065). Specified in `Zenith2/Docs/Architecture.md → "Third-party dependencies"` and `Zenith2/Docs/Architecture.md → "Build system and presets"`.
- **Nouns:** reproducibility manifest, ThirdParty/manifest.json, toolchain pin, setup script, Zenith2/Tools/Setup, MSVC, LLVM, clang-tidy, CMake, Ninja, Slang, DXC, WARP NuGet, lavapipe, NDK r27c, JDK 17, Gradle, AGP, build-tools, sccache, vm.mmap_rnd_bits
- **Related:** Z2-D-011, Z2-D-028, Z2-D-065, Z2-D-082
### Z2-D-009 C++20
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The language standard fixes which standard headers the runtime may use and which toolchains qualify. It must be fully supported by the pinned MSVC and by NDK r27c clang.
- **Decision:** Zenith2 is written in C++20 on every target.
- **Alternatives considered:** (1) C++17 — rejected: no concepts, `<source_location>`, `<bit>` or `<compare>`, all of which the runtime's allowed-header list uses. (2) C++23 — rejected: library and module support is uneven across the pinned MSVC and NDK r27c clang.
- **Consequences:** The allowed standard headers include `<concepts>`, `<compare>`, `<source_location>` and `<bit>`. Specified in `Zenith2/Docs/Contracts.md → "Containers and standard library policy"` and `Zenith2/Docs/Architecture.md → "Build system and presets"`.
- **Nouns:** C++20, CMAKE_CXX_STANDARD, concepts, source_location, bit, compare, language standard
- **Related:** Z2-D-041
### Z2-D-010 Platforms: Windows (Vulkan + D3D12 runtime choice), Android arm64 validated on the S24 Ultra only, minSdk 34, 16 KB pages
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Validation claims are only honest for hardware that is actually tested. Current Android releases require 16 KB page-size compliance, and a Windows engine with two RHIs needs the choice made at runtime rather than per build.
- **Decision:**
  - Windows, with Vulkan and D3D12 both linked and chosen at runtime.
  - Android arm64, validated on the Galaxy S24 Ultra only (declared). Other Adreno/Snapdragon devices are best-effort. Mali is dropped. minSdk 34.
  - 16 KB page-size compliance: the r27 toolchain sets `ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON` and links with `-Wl,-z,max-page-size=16384`; `llvm-readelf -l` checks that every `.so`, including prebuilt middleware, has all PT_LOAD alignments at or above 0x4000; AGP 8.5.1 or newer with `jniLibs.useLegacyPackaging=false`; `ApkCheck` runs `zipalign -c -P 16 -v 4` on the release APK.
  - The HWASan APK is a separate debuggable build type (`wrap.sh`, `extractNativeLibs=true`). It is exempt from the zip-alignment rule, but ELF alignment is still checked.
  - `android-x86_64` is emulator smoke only. Linux exists only as the `linux-tsan` test host. Consoles are deferred (Z2-D-071).
- **Alternatives considered:** (1) Broad Android support including Mali — rejected: there are no devices to validate on, so any claim would be unproven. (2) Vulkan only on Windows — rejected: D3D12 is the native Windows API, and the second RHI is the neutrality proof. (3) A lower minSdk — rejected: the Vulkan 1.3 baseline and the 16 KB page rules target current devices.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Platform layer"`, `Zenith2/Docs/Architecture.md → "Build system and presets"` and `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`; the device budgets are `Zenith2/Docs/Budgets.md → "Reference hardware"`.
- **Nouns:** Windows, Vulkan, D3D12, runtime RHI choice, Android arm64, Galaxy S24 Ultra, S24, Adreno 750, minSdk 34, 16 KB page size, ANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES, max-page-size=16384, llvm-readelf, PT_LOAD, AGP, useLegacyPackaging, ApkCheck, zipalign, HWASan APK, wrap.sh, extractNativeLibs, android-x86_64, emulator smoke
- **Related:** Z2-D-011, Z2-D-013, Z2-D-027, Z2-D-071
### Z2-D-011 Sanitizer presets: win-asan, linux-tsan under WSL2, HWASan on the S24; dependencies built from source in sanitizer presets
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Ownership in Zenith2 relies on sanitizers to catch use-after-free (Z2-D-040), and TSan and HWASan need the whole link instrumented, including third-party code. Android ASan is deprecated.
- **Decision:**
  - `win-asan`: MSVC ASan plus libFuzzer.
  - `linux-tsan`: a test-only headless host under WSL2 with no GPU libraries, running TSan and UBSan. `Sanitizers.ps1` invokes it through `wsl.exe -d <pinned distro> -e bash -lc`; its `binaryDir` is on the Linux filesystem, not DrvFS; the Setup script sets `vm.mmap_rnd_bits` to 28 or less; the preset has `condition: hostSystemName`, so Windows configures skip it; CI runs it natively on Linux.
  - HWASan on the S24. Android ASan is deprecated and not used.
  - Every C/C++ dependency is built from source in the sanitizer presets (Jolt, PhysX, Luau, flecs, EnTT, zlib, zstd, SDL3 and the rest). The manifest pins the sources, and these presets build them with the sanitizer's flags, because TSan and HWASan need the whole link instrumented.
  - Suppression files are allowed only with a decision ID.
  - The sanitizer steps are change-triggered through the `Zenith2Memory` category; stage acceptance runs the full sanitizer set.
- **Alternatives considered:** (1) Sanitizers on engine code only, linking prebuilt dependencies — rejected: TSan and HWASan report false positives and miss real defects across an uninstrumented link. (2) Android ASan — rejected: deprecated. (3) Running TSan on Windows — rejected: MSVC has no TSan.
- **Consequences:** Arenas and pools poison under ASan and tag under HWASan (Z2-D-040). Specified in `Zenith2/Docs/Architecture.md → "Build system and presets"`, `Zenith2/Docs/Contracts.md → "Ownership and allocation"` and `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`.
- **Nouns:** sanitizer preset, win-asan, MSVC ASan, libFuzzer, linux-tsan, WSL2, TSan, UBSan, HWASan, Sanitizers.ps1, Sanitizers step, wsl.exe, DrvFS, vm.mmap_rnd_bits, hostSystemName, suppression file, Zenith2Memory, built from source
- **Related:** Z2-D-008, Z2-D-040, Z2-D-044, Z2-D-082
### Z2-D-012 Budgets v1 adopted; a change needs a decision plus a workload version bump
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Performance claims without a fixed definition, reference hardware and workload version are not comparable across runs, and per-system limits cannot be allocated until the totals are fixed. Budgets v1 fixes the totals now; per-system rows are allocated in Stage 1.
- **Decision:**
  - `Zenith2/Docs/Budgets.md` is versioned; v1 is adopted. A change needs a recorded decision plus a workload version bump.
  - "60 fps" is p95 frame time at most 16.7 ms and p99 at most 20 ms. Only the first 10 s after the initial load is excluded, once per run; background streaming, runtime asset uploads and shader or PSO compilation always count. Each workload run lasts at least 120 s and 7,200 frames and reports its excluded intervals.
  - "Never below 45" is the 1-second rolling average staying at or above 45 fps through a 30-minute run, with the S24 unplugged, 25 °C ambient, starting at 80% battery.
  - The dynamic-resolution floor is 75% of output (desktop 2560×1440; S24 2340×1080). A run that hits the floor and still misses its target fails.
  - The reference desktop is a Ryzen 7 5800X-class CPU, 32 GB RAM, an RTX 3070, at 1440p. Graphics settings are fixed per workload version.
  - Memory: the S24 at most 3.0 GB process RSS; Windows at most 8 GB RAM and 6 GB VRAM.
  - Package size: the AAB base module stays within Google Play's base-module download limit, with the rest shipped through Play Asset Delivery packs; the exact limit is pinned in Stage 6 from Play's current documentation. Windows install size is a budget row per flagship slice. Repository asset storage (the Git LFS policy and per-game quotas) is a budget row; large binary content is LFS-tracked, and the lane checkouts' cost is measured.
  - Workloads: W-Indoor v1 (200 lights, 32 shadowed, 1M visible triangles); W-Outdoor v1 (4 km² terrain, 100k foliage instances, 2 km view distance, 10 m/s traversal); W-Characters v1 (100 skinned characters at 60 bones, and 100 AI agents); W-Load (cold boot at most 10 s on Windows and 15 s on the S24); W-Editor (iteration times set by Stage 1 row tickets); W-Dense and W-UI2D (content scale set by Stage 1 workload tickets, planned producers that block their consumers); W-OpenWorld, W-Cinematic and W-Game2D (the flagship slices).
  - Measured in every workload run (Stage 1 sets the limit rows): cold versus warm caches (shader, PSO and DDC); teleport and rapid-traversal streaming; transient memory peaks; input-to-photon latency; individual hitches (count of frames over 33.3 ms, and the maximum frame time); editor iteration (script, shader and asset hot reload).
  - Per-system rows (CPU, GPU, memory, upload cap, script GC/CPU and so on) are allocated in Stage 1 from these totals. Until then they are pending, and the tickets citing them are blocked by the row's producer ticket.
  - A benchmark regression beyond its row's tolerance blocks the stage acceptance until it is fixed or re-budgeted by a decision. Animation budgeting and PSO preparation are implementation tickets in ANIM and RPROD, not only performance targets.
- **Alternatives considered:** (1) Average-fps targets — rejected: averages hide hitches, which p95/p99 and the hitch count expose. (2) Budgets set per feature as features land — rejected: per-system rows would not sum to a total, and nothing would bind the first tickets. (3) Excluding streaming and PSO compilation from measurement — rejected: those are exactly the stalls a player sees.
- **Consequences:** Every ticket's `## Budgets` section names rows; a row is adopted (a number exists) or pending (its producer ticket blocks the citing ticket). Specified in `Zenith2/Docs/Budgets.md → "Purpose versioning and change rule"`, `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"`, `Zenith2/Docs/Budgets.md → "Workload definitions"` and `Zenith2/Docs/Budgets.md → "Per-system rows"`.
- **Nouns:** Zenith_MemoryBudgets, Budgets_CheckAllocation_Fixtures, IsBudgetRun, Zenith_Bench_BudgetsCORE, Zenith_Bench_BudgetsJOBS, Zenith_Bench_BudgetsECS, Zenith_Bench_BudgetsLIGHT, Zenith_Bench_BudgetsGEOM, Zenith_Bench_BudgetsPLAT, Zenith_Bench_BudgetsScript, Zenith_Bench_Budgets_RPROD, Zenith_Bench_Budgets_RHI, Budgets v1, Budgets.md, budget row, workload version, p95 frame time, p99 frame time, 60 fps, never below 45, dynamic resolution floor, reference desktop, RTX 3070, S24, process RSS, VRAM, AAB base module, Play Asset Delivery, Git LFS, W-Indoor, W-Outdoor, W-Characters, W-Load, W-Editor, W-Dense, W-UI2D, W-OpenWorld, W-Cinematic, W-Game2D, hitch, input-to-photon latency, pending budget row, benchmark regression
- **Related:** Z2-D-021, Z2-D-064, Z2-D-065
### Z2-D-013 GPU baseline: Vulkan 1.3 profile with shaderFloat16, D3D12 SM 6.6, bindless, dynamic rendering
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Fully bindless binding, dynamic rendering and native half precision need a baseline that every target — the RTX 3070, the S24's Adreno 750, WARP and lavapipe — exposes. A baseline expressed as a Vulkan profile makes the check mechanical.
- **Decision:**
  - A Vulkan 1.3 profile (through Vulkan-Profiles), and D3D12 Shader Model 6.6. Bindless and dynamic rendering are required on both.
  - Half precision needs `shaderFloat16` on Vulkan and native 16-bit shader ops on D3D12 (SM 6.2 `-enable-16bit-types` with `Native16BitShaderOpsSupported`), not `min16float`.
  - `shaderFloat16` is a custom Zenith2 profile addition (Roadmap 2022 does not require it). Adreno 750 exposes it, and WARP and lavapipe must pass the Stage 1 check too, or CI cannot run half shaders.
  - `shaderDrawParameters` (core in 1.2) is in the profile, for `DrawIndex`/`gl_DrawID`. `VK_EXT_mutable_descriptor_type` is not in the baseline, which is why the `DescriptorHandle` lowering is D-SLANG (open, Stage 1).
  - The S24 profile is a Stage 1 ticket.
- **Alternatives considered:** (1) Vulkan 1.1/1.2 with render-pass objects — rejected: dynamic rendering and the 1.3 feature set are what the graph and the bindless model are built on. (2) The unmodified Roadmap 2022 profile — rejected: it lacks `shaderFloat16`, which the half-precision rule needs. (3) D3D12 SM 6.5 or lower — rejected: SM 6.6 gives direct `ResourceDescriptorHeap` indexing, which the bindless model uses.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`, `Zenith2/Docs/Rendering.md → "Binding model"` and `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`.
- **Nouns:** GPU baseline, Vulkan 1.3, Vulkan-Profiles, Zenith2 profile, S24 profile, shaderFloat16, D3D12, Shader Model 6.6, Native16BitShaderOpsSupported, enable-16bit-types, min16float, bindless, dynamic rendering, shaderDrawParameters, VK_EXT_mutable_descriptor_type, Adreno 750, WARP, lavapipe
- **Related:** Z2-D-010, Z2-D-014, Z2-D-054
### Z2-D-014 Universal GPU performance rules and their lints
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Android's optimisation guidance (compressed vertex data, early-Z, no empty passes, 16-bit formats, half precision) is good practice on every GPU, and rules applied only on one platform diverge. Rules without automation erode. Zenith2 applies them universally and enforces each where it can be enforced.
- **Decision:**
  - The rules apply on every platform: compressed vertex streams declared in Slang with position in a dedicated stream; QTangent or octahedral normals, UNORM16 UVs with per-mesh scale and bias, 16-bit indices; pass culling, clear folding and attachment-compatible merging; early-Z preserved; D24S8 by default, D32F only where precision requires it; no 32-bit render targets without justification and no unnecessary attachments; half precision; back-face culling.
  - No empty render passes; merge similar passes: the graph compiler culls raster passes with no draws and no side effects, folds clear-only passes into the next user's load op (`CLEAR` instead of a separate clear pass), and merges consecutive raster passes into one rendering scope only when the attachment set is identical, load/store is compatible, and no synthesised barrier falls between them (dynamic rendering forbids barriers inside a rendering instance). This is not tile-local-read merging, which is D-TBDR (open, Stage 5). Enforced by graph unit tests, including a barrier-blocked merge; the frame dump shows the merges.
  - Keep early-Z: opaque pipelines never `discard`/`clip` and never write `SV_Depth`; alpha-tested materials use separate pipelines drawn after opaque in depth-writing passes; every subsequent colour pass uses an equal depth test with no discard. Enforced by the `ShaderChecks` lint: `discard`/`clip` only in `[AlphaTest]` variants; `SV_Depth`/`SV_DepthGreaterEqual` forbidden unless a decision allowlists it.
  - Depth format: D24S8 by default (standard Z), including all shadow views; reversed Z only for views failing the D24 precision check, always with D32F_S8; D32F_S8 as the capability fallback where D24S8 is not exposed. Enforced at graph compile: a D32 target needs the view's precision justification or the capability fallback (Z2-D-061).
  - No full 32-bit render targets unless necessary: HDR colour R11G11B10F (bloom, SSR previous colour); TAA history RGBA16F; normals RG16_UNORM octahedral; velocity RG16F; the G-buffer at most 16 bits per channel; the HiZ pyramid RG16F. Enforced at graph compile: a 32-bit-per-channel format needs an entry on the justified list (the visibility ID R32_UINT, D32F views, GPU-scene data in buffers).
  - Do not bind attachments unnecessarily: a pass binds exactly its `Flux_RasterTargets`; depth is bound read-only when not written; unwritten attachments are checked by a declared-shader-set check (each pass lists its shader programs, whose reflected outputs are checked against its targets) and by a development-build record-time check for indirect multi-PSO passes; a conditionally written MRT output has a colour write mask of 0 in PSOs that do not write it.
  - Half precision where possible: colour, normal, BRDF-term and post-process maths use `half` with native 16-bit types; positions, depth, UV derivatives and accumulations stay `float`; the vertex codec's integer-only half conversion is kept so compute codecs never need `shaderInt16`. `ShaderChecks` reports the float/half ratio per shader as a review signal.
  - Back-face culling for opaque: the default raster state is cull back; two-sided rendering needs an explicit material flag and is reported. Enforced by a PSO lint: an opaque PSO with culling off carries the material's two-sided flag.
  - Compressed vertex data, 16-bit indices and a position stream (Z2-D-062) are enforced by cook checks and the generated layout tables.
  - The WorkerGuide carries these rules in short form, and every graphics ticket's `## Validation` cites the checks it passes.
- **Alternatives considered:** (1) Android-only rules — rejected: rules that apply to one platform diverge, and desktop gains from the same bandwidth savings. (2) Guidance without lints — rejected: unenforced rules erode. (3) Full 32-bit formats everywhere for simplicity — rejected: bandwidth and memory cost on the S24, with no image benefit for these targets.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`, `Zenith2/Docs/Rendering.md → "Vertex data format"` and `Zenith2/Docs/Rendering.md → "Render graph"`.
- **Nouns:** GPU performance rules, ShaderChecks, PSO lint, pass culling, clear folding, pass merging, early-Z, AlphaTest, discard, clip, SV_Depth, SV_DepthGreaterEqual, D24S8, D32F_S8, R11G11B10F, RGBA16F, RG16_UNORM, RG16F, R32_UINT, justified 32-bit list, Flux_RasterTargets, declared-shader-set check, colour write mask, half precision, back-face culling, two-sided flag, frame dump
- **Related:** Z2-D-013, Z2-D-036, Z2-D-056, Z2-D-061, Z2-D-062
### Z2-D-015 Slang is the shader source of truth; C++ parameter structs are generated
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Hand-mirrored C++ structs and shader declarations drift, and a render graph that derives access from parameter fields needs a machine-readable field table. Zenith2 generates the C++ side from Slang so there is one declaration.
- **Decision:**
  - Slang is the source of truth. Each pass's Slang file declares its parameter struct and entry points; C++ parameter structs are generated.
  - `ZenithShaderCompiler` emits SPIR-V and DXIL (through DXC), reflection JSON, and `<Name>.generated.h`, which holds the C++ parameter struct with a `static_assert(offsetof)` per field, typed `Flux_SRV<T>`/`Flux_UAV<T>` fields, a field table (offset, kind, access, shader stage), constexpr shader IDs and a permutation struct.
  - Resource fields are Slang `DescriptorHandle<T>` (8 bytes). `DescriptorHandle<T>` is the intended API, subject to D-SLANG (open, Stage 1).
  - Vertex-input layouts are declared in Slang too, and baked into the generated header (Z2-D-062).
- **Alternatives considered:** (1) Hand-written C++ structs mirrored from the shaders — rejected: drift is silent until a GPU reads garbage. (2) C++ as the source generating shader declarations — rejected: shader authors lose Slang's type system and modules, and the reflection would still be needed for access. (3) HLSL with DXC and SPIRV-Cross — rejected: no link-time constants or modules, and a cross-compilation step whose output is a second source of truth.
- **Consequences:** The render graph derives reads and writes from the field table (Z2-D-055). Specified in `Zenith2/Docs/Rendering.md → "Shader system and offline compilation"`, `Zenith2/Docs/Rendering.md → "Binding model"` and `Zenith2/Docs/Rendering.md → "Decision D-SLANG"`.
- **Nouns:** Slang, ZenithShaderCompiler, parameter struct, generated.h, Flux_SRV, Flux_UAV, Zenith_RHISRV, Zenith_RHIUAV, Zenith_RHIDescriptorHandle, Zenith_RHIShaderInterface, field table, shader ID, permutation struct, reflection JSON, DescriptorHandle, SPIR-V, DXIL, DXC, D-SLANG
- **Related:** Z2-D-025, Z2-D-054, Z2-D-055, Z2-D-062
### Z2-D-016 Luau scripting; every invocation is a transaction over engine-visible state
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's visual scripting grew 141 hand-written script nodes and a second reflection system. Games in Zenith2 contain no C++, so gameplay needs a sandboxed, typed, fast language whose failures cannot corrupt engine state or change results with thread layout.
- **Decision:**
  - Luau, with two script forms: behaviours and chunk systems.
  - Every invocation (a chunk-system call or a behaviour callback) is a transaction over engine-visible state; the boundary is the entity (Z2-D-051).
  - Luau-heap state (script tables, globals, upvalues) is not transactional: a failing instance is quarantined and restored from its last good state on reload.
  - `.d.luau` type declarations are generated.
- **Alternatives considered:** (1) Lua 5.4 or LuaJIT — rejected: no gradual types or built-in sandboxing, and LuaJIT's maintenance and arm64 story are weaker. (2) C# through an embedded runtime — rejected: runtime size, AOT constraints on Android, and a GC outside engine budgets. (3) Node-based visual scripting, as in Zenith — rejected: the audit's 141 hand-written nodes and duplicate reflection.
- **Consequences:** Specified in `Zenith2/Docs/Scripting.md → "Purpose and scope"`, `Zenith2/Docs/Scripting.md → "Behaviours and chunk systems"` and `Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary"`.
- **Nouns:** Luau, ZenithScript, behaviour, chunk system, script invocation, transaction, engine-visible state, Luau heap, quarantine, last good state, .d.luau
- **Related:** Z2-D-017, Z2-D-018, Z2-D-023, Z2-D-051
### Z2-D-017 Luau exception firewall: /EHs and -fexceptions only in ZenithScript/Internal; protected entry for every call into Luau
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Luau reports errors by unwinding (C++ exceptions or longjmp, depending on build), while the engine is compiled without exceptions. An error unwinding through an EH-disabled engine frame skips destructors and corrupts state.
- **Decision:**
  - Luau and `ZenithScript/Internal` alone compile with C++ exceptions: `/EHs` (not `/EHsc`) on MSVC, and `-fexceptions` on clang, per target. `/EHsc` assumes `extern "C"` functions never throw and could elide the cleanups around a `lua_error` call.
  - Stage 1 verifies whether Luau's API has C or C++ linkage in the pinned version.
  - So RAII unwinds correctly through the EH-enabled binding TUs. Amended 2026-10-23 by Z2-D-141 (the human's ruling): an engine function a binding calls carries no exception specification; it cannot throw, because it is compiled with exceptions off, and it re-enters script only through the protected entry.
  - Every entry into Luau, from anywhere, is a protected call inside `ZenithScript/Internal`. `lua_call`/`lua_callk` outside it is a `ForbiddenCheck` failure.
  - An engine callback that re-enters script goes through that same protected entry, so no Luau error ever unwinds through an EH-disabled engine frame.
  - The gate enforces exceptions-off everywhere else.
  - Amended 2026-09-24 (extension of this ruling, not a new decision): the firewall has two targets, `ZenithScript/Internal` and `ZenithScriptCompiler/Internal` (the tool-side compiler library no Player links). Each library passes `EXCEPTIONS INTERNAL` to `zenith_add_library`, which compiles its `Internal/**` partition as the object library `<Name>_EH` — `ZenithScript_EH` and `ZenithScriptCompiler_EH` — with `/EHs` or `-fexceptions`, and the rest of the library without; any other library passing it fails the configure. Luau calls stay in `ZenithScript/Internal`; Luau headers may be included in both (`Zenith2/Docs/Architecture.md → "Build system and presets"`; `Zenith2/Docs/Contracts.md → "Errors and results"`).
- **Alternatives considered:** (1) Build Luau with longjmp error handling and no exceptions anywhere — rejected: longjmp skips destructors in the binding TUs, so RAII there is unsafe. (2) Exceptions enabled engine-wide — rejected: contradicts the no-exceptions ruling (Z2-D-045) and costs code size everywhere. (3) `/EHsc` in the binding TUs — rejected: it may elide cleanups around `extern "C"` calls that do throw.
- **Consequences:** Required tests include a Luau error raised through a binding while RAII objects in the EH-enabled TU are live (destructors run), and an engine callback re-entering script then raising (caught at the protected entry, no engine frame unwound). Specified in `Zenith2/Docs/Scripting.md → "Luau integration and the exception firewall"`, `Zenith2/Docs/Contracts.md → "Errors and results"` and `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`.
- **Nouns:** exception firewall, ZenithScript_EH, ZenithScriptCompiler_EH, EXCEPTIONS INTERNAL, zenith_add_library, ZenithScript/Internal, ZenithScriptCompiler/Internal, /EHs, /EHsc, -fexceptions, protected call, protected entry, lua_pcall, lua_call, lua_callk, lua_error, ForbiddenCheck, binding TU, RAII, extern "C"
- **Related:** Z2-D-016, Z2-D-042, Z2-D-045
### Z2-D-018 One Luau VM per worker thread per world for chunk systems; behaviours on the world main VM
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A Luau VM is single-threaded, and chunk systems run in parallel on workers. A shared VM would serialise them; a VM per invocation would cost too much. Per-worker VMs are only safe if system modules hold no state.
- **Decision:**
  - One VM per worker thread, per world, for chunk systems, loaded lazily. The `Z2-B-SCRIPT-011` row (VM memory times workers per world) caps VM memory × workers; the per-VM caps are `Z2-B-SCRIPT-009` (world main VM memory cap) and `Z2-B-SCRIPT-010` (per-worker chunk-system VM memory cap).
  - System modules must be stateless. `luaL_sandbox` and frozen tables do not establish that on their own: a closure can capture and mutate a local. The enforcement mechanism is D-LUAU-STATELESS (open, Stage 1); chunk systems use only modules its check accepts.
  - The DAP server fans breakpoints out to every VM.
  - Behaviours run on the world's main VM, pinned to the main thread, in the Early or Late script phase they declare (Z2-D-080).
- **Alternatives considered:** (1) One VM with a global lock — rejected: serialises every chunk system. (2) A fresh VM or environment per invocation — rejected as the default for cost; it survives only as one D-LUAU-STATELESS (open, Stage 1) alternative. (3) One VM shared across worlds — rejected: breaks world isolation.
- **Consequences:** Specified in `Zenith2/Docs/Scripting.md → "VM model and threading"`, `Zenith2/Docs/Scripting.md → "Stateless system enforcement"` and `Zenith2/Docs/Budgets.md → "Scripting rows"`.
- **Nouns:** Luau VM, per-worker VM, world main VM, chunk-system VM, lazy VM load, VM memory budget, stateless system module, luaL_sandbox, frozen table, upvalue, DAP server, breakpoint fan-out, Early scripts, Late scripts, D-LUAU-STATELESS
- **Related:** Z2-D-016, Z2-D-023, Z2-D-080
### Z2-D-019 Determinism guarantee: same backend, same build, same content and inputs give identical results regardless of workers, chunk layout and scheduling, including failing ticks
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Replays, input recording, the multiplayer loopback and bug reproduction all need identical results from identical inputs. A parallel engine with scripts that can fail mid-tick makes that impossible unless ordering, randomness, budgets and failure are each designed to be layout-independent.
- **Decision:**
  - For the same backend and the same build, identical content and inputs give identical simulation results, regardless of worker count, chunk layout or scheduling, including ticks where scripts fail or exhaust budgets. Cross-backend and cross-platform equality is not promised.
  - The transaction boundary is the entity, never the chunk: a failure rolls back only the failing entity's writes and side effects (Z2-D-051).
  - Script budgets count instructions, measured as deterministic safepoint counts, never wall-clock time (Z2-D-052).
  - Command buffers from parallel invocations are merged in (system order, logical invocation ID, sequence) order, never completion order. Spawn IDs are assigned at playback in that merged order, and events are delivered in that same order. Parallel reductions use a fixed-order tree, independent of worker count.
  - Script randomness is counter-based, derived per invocation from (world seed, system or behaviour ID, tick, stable simulation ID of the entity, draw counter), never keyed by chunk or storage index; there is no shared mutable stream.
  - Asynchronous results become visible only at a tick boundary, in completion-ID order, and the publication tick is replay-logged (Z2-D-053).
  - Tests: the same replay run with 1, 2, 8 and all workers, and with forced chunk compaction and a different chunk capacity, on each backend, gives bit-identical results per backend. The fault matrix repeats this with injected failures.
- **Alternatives considered:** (1) Cross-platform lockstep determinism (fixed-point or strict floating point everywhere) — rejected: cost across physics, animation and middleware for a guarantee single-player replays do not need. (2) Best-effort determinism — rejected: replays, loopback proof and bug reproduction need a guarantee. (3) Determinism only in a single-threaded mode — rejected: the shipping configuration would be the one without the guarantee.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"`, `Zenith2/Docs/Scripting.md → "Determinism rules and randomness"` and `Zenith2/Docs/Scripting.md → "Fault-matrix tests"`.
- **Nouns:** Zenith_ScriptRandomKey, Zenith_MergeKey, Zenith_ReplayLog, determinism guarantee, per-backend determinism, replay, merge order, logical invocation ID, command buffer merge, spawn ID, event order, fixed-order reduction tree, counter-based randomness, stable simulation ID, world seed, draw counter, chunk compaction, chunk capacity, worker count, bit-identical, fault matrix
- **Related:** Z2-D-003, Z2-D-051, Z2-D-052, Z2-D-053, Z2-D-073
### Z2-D-020 Data-defined component schemas in *.schema.json, registered at load as reflected types
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Games contain no C++ (Z2-D-002), so a game that needs a new component must define it in data. Zenith had two reflection systems; Zenith2 needs one reflection path that treats data-defined and C++-registered types identically.
- **Decision:**
  - Game authors define new component schemas in data, in one format: `Games/<Name>/Schemas/*.schema.json`. Luau types are generated from it as `.d.luau`, so there is no second schema front end.
  - Field types: primitives, vectors, quaternions, colours; strings as interned IDs; persistent IDs, entity handles and asset handles; fixed arrays and nested structs; containers, stored out of line.
  - Each schema carries a stable type ID, stable field IDs and a schema version. A field ID can never be reused.
  - `ZenithReflect` supports runtime type registration: at game load, schemas are registered alongside the engine's C++-registered types, with no difference to consumers — serialization, the inspector, migration, `.d.luau` generation, chunk-system column access, and the phase/authority declarations.
  - The ECS backends store data-defined components natively; the `Zenith_ECS` API is type-erased by reflection ID, so no API change is needed. flecs uses native runtime components; the custom backend is designed for runtime layouts from the start; EnTT needs a dynamic-storage adapter, a flagged cost (P1).
  - A data schema change is an authoring-data migration (Z2-D-048).
  - Delivered in Stage 2, in ZenithReflect and all three ECS backends; the schema format is specified in Stage 1.
- **Alternatives considered:** (1) Components defined only in C++ — rejected: games contain no C++. (2) Schemas declared in Luau — rejected: a second schema front end, and schemas would need a VM to load. (3) Dynamic property bags with no stable IDs — rejected: no native storage, no migration and no stable serialization.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Data-defined components"`, `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"` and `Zenith2/Docs/Contracts.md → "Reflection"`.
- **Nouns:** data-defined component, schema.json, Schemas/, ZenithReflect, runtime type registration, reflected type, stable type ID, stable field ID, schema version, interned string ID, out-of-line container, Zenith_ECS, reflection ID, dynamic-storage adapter, .d.luau, inspector, authoring-document migration
- **Related:** Z2-D-002, Z2-D-039, Z2-D-048, Z2-D-080
### Z2-D-021 Three equally binding flagship slices: OpenWorldSlice, CinematicSlice, Game2D
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** "Production quality" means nothing without representative content and a fixed acceptance procedure. A single flagship would let whole capability families (2D, UI, accessibility, cinematics) go unproven.
- **Decision:**
  - Several equally binding representative slices: an open-world action/RPG slice (`OpenWorldSlice`), a cinematic action/adventure slice (`CinematicSlice`) and a 2D/UI game (`Game2D`). They define the content specifications, the quality-matrix priorities and the production acceptance.
  - Each slice has: a playthrough script (a scripted walkthrough of 20–30 minutes); a feature checklist mapped to requirement IDs; per-platform budgets from Budgets v1 and the quality matrix; a bug bar (zero crash or blocker defects, and a severity-weighted open-defect cap); and a human-review sign-off for the subjective quality items.
  - OpenWorldSlice: streaming, world authoring, AI/navigation at scale, persistent-world saves, and the Luau quest, dialogue and inventory samples. CinematicSlice: character and animation quality, cinematics, lighting and image quality, audio, and set-pieces. Game2D: 2D, UI, accessibility flows, and localization.
  - Their workloads are W-OpenWorld, W-Cinematic and W-Game2D. Stage 1 defines the acceptance definitions, `QualityMatrix.md` and `ContentSpecs.md`; Stage 4 delivers each slice's first playable milestone, accepted against its playthrough script so far.
- **Alternatives considered:** (1) One flagship game — rejected: leaves whole capability families unproven. (2) Tech demos only — rejected: demos prove features in isolation, not production integration. (3) One primary slice with secondary ones — rejected: a secondary slice's failures would not block acceptance.
- **Consequences:** Specified in `Zenith2/Docs/Budgets.md → "Production-quality acceptance"` and `Zenith2/Docs/Roadmap.md → "Acceptance evidence"`.
- **Nouns:** flagship slice, OpenWorldSlice, CinematicSlice, Game2D, playthrough script, feature checklist, bug bar, human-review sign-off, QualityMatrix.md, ContentSpecs.md, W-OpenWorld, W-Cinematic, W-Game2D, production acceptance
- **Related:** Z2-D-012, Z2-D-077, Z2-D-078
### Z2-D-022 Settled choices are challenged only as flagged proposals with evidence
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A programme run through many agent sessions drifts if any session can quietly revise a settled choice while writing a doc or a ticket. Challenges must be possible, visible and decided by the human.
- **Decision:**
  - A settled choice may be challenged only as a flagged proposal, stating the concern, the evidence, the proposal and the trade-off.
  - Nothing changes without the human's approval. Until approval the docs and tickets are written as the settled decision says.
  - Flagged proposals are listed in `Zenith2/Docs/Roadmap.md → "Flagged proposals"`; P1 and P2 are recorded there (Z2-D-130).
- **Alternatives considered:** (1) Let writers revise settled choices when they find a better option — rejected: silent drift across sessions. (2) Forbid challenges — rejected: evidence found in implementation must have a path to change a decision.
- **Consequences:** An approved proposal becomes a new entry appended to this log in the diff of the ticket that applies it, with the next free ID (Z2-D-097), and the old entry is marked superseded (see "Changing a settled decision" under How this log is written). Specified in `Zenith2/Docs/Contracts.md → "Review and change management"` and `Zenith2/Docs/Roadmap.md → "Flagged proposals"`.
- **Nouns:** flagged proposal, settled choice, challenge, concern, evidence, trade-off, human approval, FLAGGED_PROPOSALS.md, Roadmap.md, P1, P2
- **Related:** Z2-D-085, Z2-D-097, Z2-D-130
### Z2-D-023 Script recovery through explicit serialized behaviour state and OnRecover
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A failing behaviour's Luau heap cannot be rolled back, and a suspended coroutine's execution position cannot be serialized portably. Recovery, hot reload and saves therefore need a state model the engine owns.
- **Decision:**
  - Recovery uses explicit serialized behaviour state plus a restart entry point. Behaviour state is a declared, reflected, serializable table. Recovery restores it and calls `OnRecover(state)`, which restarts coroutine logic from its declared phase. A suspended execution position is never serialized.
  - Serializable state is only the declared state table: reflected fields, primitives, strings, persistent IDs, entity and asset handles, and nested tables of the same. Closures, coroutines and userdata other than handles are not serializable; declaring them is a load error.
  - The checkpoint is serialized into reserved memory inside the transaction, before commit, only when the declared state is dirty, and at most once per tick (at the behaviour's last invocation in the tick, or at a yield). Its cost is the `Z2-B-SCRIPT-014` row (checkpoint serialization CPU per tick), and its reserved memory the `Z2-B-SCRIPT-015` row (checkpoint reserved memory per behaviour instance). A serialization failure is a Luau error, so the entity's transaction rolls back; commit then swaps the checkpoint in, which cannot fail.
  - Behaviours cannot share mutable tables; cross-instance data goes through components, events or entity handles, and the D-LUAU-STATELESS (open, Stage 1) mechanism is extended to check it.
  - On restore, stale entity and asset references are resolved through persistent IDs; anything unresolvable becomes `nil`, with a diagnostic.
  - A failing instance is quarantined: its heap state is discarded, and it is restored on reload from its last successfully committed serialized state, using the same mechanism as hot-reload migration.
- **Alternatives considered:** (1) Serializing the whole Luau heap or coroutine stacks — rejected: not portable across bytecode versions and not bounded. (2) Restarting a failed behaviour from its initial state — rejected: loses gameplay state on every error. (3) Transactional Luau heap (copy-on-write VM state) — rejected: cost on every table write.
- **Consequences:** Required tests include quarantine followed by reload from last-good state, checkpoint serialization failure (the invocation rolls back) and restore with a destroyed referenced entity (it becomes `nil`, with a diagnostic). Specified in `Zenith2/Docs/Scripting.md → "Behaviour state model and recovery"` and `Zenith2/Docs/Scripting.md → "Quarantine and hot reload"`.
- **Nouns:** behaviour state, declared state table, OnRecover, checkpoint, dirty state, reserved checkpoint memory, serialization failure, quarantine, last-good restore, persistent ID resolution, hot-reload migration, coroutine restart, script recovery
- **Related:** Z2-D-016, Z2-D-018, Z2-D-051
### Z2-D-024 Optional engine modules are static libraries selected per game in game.json; no dynamic plugins
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Not every game needs 2D, physics, audio, AI, online, networking or UI, and a dynamic plugin system creates binary-compatibility and unloading problems that in-repo games never need.
- **Decision:**
  - Optional engine modules (2D, Physics, Audio, AI, Online, Net, UI and others) are static libraries, selected per game in `game.json`.
  - A selected module is linked in, and the composition root registers it when present: its reflection types, Luau bindings and systems.
  - Module dependencies are declared in CMake and checked by libgraph.
  - There are no dynamic plugins, so there is no binary compatibility question and nothing to unload.
- **Alternatives considered:** (1) Dynamic plugins (DLL/.so) — rejected: binary compatibility, unloading and a second link model. (2) Every module always linked — rejected: game executable size and startup cost, and unused systems in every schedule. (3) Registration through static initialisers — rejected: order-dependent global state (Z2-D-046).
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` and `Zenith2/Docs/Architecture.md → "Composition root and Player"`; the compatibility kind is `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`.
- **Nouns:** optional module, static module, game.json modules, composition root, module registration, reflection registration, Luau binding registration, system registration, dynamic plugin, Zenith2D, ZenithPhysics, ZenithAudio, ZenithAI, ZenithOnline, ZenithNet, ZenithUI
- **Related:** Z2-D-002, Z2-D-046, Z2-D-081
### Z2-D-025 Shader compilation is offline only; the runtime loads precompiled blobs; PSOs are precached
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A shader compiled while the game runs is a hitch, and a runtime linked against Slang or DXC is large and has a wide attack surface. First-draw PSO creation causes the same stalls.
- **Decision:**
  - Only the separate `ZenithShaderCompiler` process compiles Slang, through DXC for DXIL, which is an offline-only dependency. It runs at build time (a CMake custom command) or in a file-watch mode during editor iteration. Never while the game or editor runs.
  - The runtime only loads precompiled SPIR-V/DXIL blobs. The link-surface gate (libgraph) proves no runtime target links Slang or DXC.
  - Permutations use Slang link-time constants, all enumerated and compiled offline. A permutation missing from the cooked set is a cook-time error, never a runtime compile.
  - Driver ISA compilation happens at PSO creation, ahead of use, by PSO precaching, never on a first draw. A PSO is keyed by (shader, permutation, raster state, attachment formats); the graph supplies the formats. PSOs are created from each game's recorded and predicted PSO set at load and asynchronously during streaming, with a Vulkan pipeline cache and a D3D12 PSO library persisted per driver version.
  - A PSO missing at draw time uses a defined fallback: in shipping builds the draw is skipped for that frame (with indirect draws, the whole material bucket); in development builds a magenta fallback PSO draws it; both increment counters (buckets and instances) shown by the on-screen profiler; it never causes a synchronous compile. A skipped shadow draw leaks light for one frame, which is accepted because precaching makes a miss a defect.
  - In the editor, the external compiler recompiles on file change; the editor loads the new blobs and rebuilds the affected PSOs. A parameter-struct layout change needs a C++ rebuild, and the reload says so.
- **Alternatives considered:** (1) Runtime compilation with a cache — rejected: first-run hitches and Slang/DXC in the runtime. (2) Lazy PSO creation on first draw — rejected: the stall the precache removes. (3) Compiling permutations on demand in the editor only — rejected: the cooked set would differ from what the editor exercised.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Shader system and offline compilation"`, `Zenith2/Docs/Rendering.md → "Pipeline state objects"` and `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`.
- **Nouns:** ZenithShaderCompiler, offline shader compilation, file-watch mode, shader blob, SPIR-V, DXIL, DXC, permutation, link-time constant, cooked permutation set, PSO, PSO key, PSO precaching, recorded PSO set, predicted PSO set, Vulkan pipeline cache, D3D12 PSO library, missing-PSO fallback, magenta fallback PSO, link-surface gate
- **Related:** Z2-D-015, Z2-D-042, Z2-D-005
### Z2-D-026 volk kept for Vulkan device-level dispatch
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Calling Vulkan through the loader's trampolines costs an indirection per call, and extension entry points must be loaded uniformly on Windows and Android.
- **Decision:** volk is kept for device-level dispatch and uniform extension loading on Windows and Android. It lives, like every Vulkan header, only inside `ZenithRHI_Vulkan/Internal`.
- **Alternatives considered:** (1) Link the Vulkan loader directly — rejected: loader trampolines on every call and per-platform extension loading. (2) A hand-written dispatch table — rejected: maintenance of a table volk already provides, with no measured gain.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Third-party dependencies"` and `Zenith2/Docs/Rendering.md → "GPU baseline and profiles"`.
- **Nouns:** volk, device-level dispatch, extension loading, ZenithRHI_Vulkan, Vulkan loader
- **Related:** Z2-D-013, Z2-D-082
### Z2-D-027 Play distribution in scope for Stage 6: AAB, bundletool, Play Asset Delivery, update migration
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A game shipped on Google Play must meet the base-module size limit and deliver its content through asset packs, and an installed update must migrate the player's data. Sideloaded APKs prove none of that.
- **Decision:**
  - In scope for Stage 6: Android App Bundles, bundletool split-APK testing, Play Asset Delivery asset packs, and installed-update migration tests, all 16 KB verified.
  - Signed APKs remain for sideload and development.
  - Interrupted content installation (a PAD pack download interrupted, then resumed or rolled back, with the game in a consistent state) is part of the scope.
- **Alternatives considered:** (1) APK only — rejected: cannot ship large content through Play. (2) A custom content downloader — rejected: duplicates Play Asset Delivery and fails Play policy for base-module content.
- **Consequences:** The package budget rows pin the base-module limit in Stage 6. Specified in `Zenith2/Docs/Roadmap.md → "Stage 6 Shipping qualification"` and `Zenith2/Docs/Budgets.md → "Package and storage budgets"`; the catalogue is `Zenith2/Docs/Requirements/PACK.md`.
- **Nouns:** Zenith_AssetPackInstaller, Android App Bundle, AAB, bundletool, bundletool build-apks, Z2-B-PACK-001, split APK, Play Asset Delivery, PAD asset pack, installed-update migration, signed APK, sideload, interrupted content installation
- **Related:** Z2-D-010, Z2-D-012, Z2-D-048
### Z2-D-028 Gate budget: 30-minute warm cap per phase, cold exemption, sccache, /Gy- in win-debug
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Z2 runs one ticket at a time (Z2-D-029), so a slow gate phase is a direct cost on throughput, and a wedged step can hold the machine-wide build slot indefinitely. A cold build (a new lane, or a preset or CMake change) is legitimately slower and must not be confused with a regression.
- **Decision:**
  - Enforced, not only reported: a 30-minute wall-clock cap per warm phase run. Measure and verify are each capped at 30 minutes; the single tree-change retry is counted separately, with the same cap.
  - A compiler cache (sccache) is pinned for MSVC and NDK clang, and shared across lanes.
  - `win-debug` always builds with `/Gy-`, so libgraph's symbol stage reuses the win-debug build, with no fourth build flavour.
  - Cold exemption: a phase that starts from an empty or invalidated build tree, detected by the Build step, gets a separate, larger cold cap from its own budget row, `Z2-B-PROC-005` (cold phase cap); the warm caps are the adopted `Z2-B-PROC-002` (measure), `Z2-B-PROC-003` (verify) and `Z2-B-PROC-004` (tree-change retry) rows. The cold build is reported in the gate output, never silently slow.
  - The cap is a generic external-mode setting, `phaseTimeoutMinutes`. `tick_gates.ps1` kills the running line's process tree when it is exceeded, and fails the phase. Each step also has its own hard timeout, sized for a cold build (a new lane, or a preset or CMake change); incremental builds are expected to be far below it.
  - Android arm64 compile and the APK check are in the base `Zenith2` category from Stage 2, on every ticket. Sanitizers (`Zenith2Memory`) and parity (`Zenith2Render`) are change-triggered.
  - Stage acceptance suites are exempt (`Zenith2Acceptance`, Z2-D-083).
- **Alternatives considered:** (1) Report gate duration without a cap — rejected: a wedged step holds the build slot and blocks every lane. (2) One cap for warm and cold — rejected: either too loose to catch warm regressions or too tight for a legitimate cold build. (3) Run every suite on every ticket — rejected: sanitizer and parity suites would exceed the cap; change-triggering keeps them where they matter.
- **Consequences:** Specified in `Zenith2/Docs/Budgets.md → "Gate budget rows"` and `Zenith2/Docs/Architecture.md → "Build system and presets"`.
- **Nouns:** gate budget, phaseTimeoutMinutes, warm cap, cold cap, cold exemption, tick_gates.ps1, measure phase, verify phase, tree-change retry, sccache, /Gy-, win-debug, step timeout, Build step, ApkCheck, Zenith2Memory, Zenith2Render, Zenith2Acceptance
- **Related:** Z2-D-007, Z2-D-029, Z2-D-044, Z2-D-083
### Z2-D-029 A dedicated Z2 lane; one lane per project so Z2 runs one ticket at a time
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Z2 must run in parallel with the ZEN and ZM lanes without sharing a build tree or a claim lock. The lane tooling allows one lane per project.
- **Decision:**
  - A dedicated Z2 lane (`zagent lane add Z2`), running in parallel with the ZEN and ZM lanes. It is a Stage 0 ticket.
  - `laneAdd` allows one lane per project, so Z2 runs one ticket at a time. A wide dependency graph therefore buys pull-forward flexibility, not concurrency.
  - CMake presets and the WSL `binaryDir` are keyed per checkout path, so lanes never share a build tree. The Z2 steps sweep their own Player and test executables, scoped to the checkout path.
- **Alternatives considered:** (1) Run Z2 tickets in the main checkout — rejected: collides with human work and other projects' ticks. (2) Several Z2 lanes — not available under `laneAdd`'s one-lane-per-project rule; recorded as flagged proposal P2, not applied.
- **Consequences:** Interfaces land first as small API-only tickets, and ticket order follows the critical path, to keep the single lane busy. Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"` and `Zenith2/Docs/Roadmap.md → "Pull-forward rule"`.
- **Nouns:** tick_gates, build_lock, MAIN_DIRTY, STALE_VERDICT, Z2 lane, zagent lane add, laneAdd, one lane per project, lane worktree, checkout path, binaryDir, exe sweep, build slot
- **Related:** Z2-D-028, Z2-D-044, Z2-D-130
### Z2-D-030 One deliberate mention of Zenith2 on the Zenith side: the root delegation line
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Existing projects must stay unaware of Zenith2, yet a tick running from the repo root inlines the root CLAUDE.md as binding, which would impose Zenith's conventions on Zenith2 work.
- **Decision:**
  - The root CLAUDE.md, and the universal convention in `zagent.project.json`, say: "`Zenith2/**` is governed by `Zenith2/CLAUDE.md`." It is the one deliberate mention of Zenith2 on the Zenith side.
  - It is allowlisted in the blindness check. It is added by a Stage 0 PROC ticket; until then the universal "Root CLAUDE.md is binding" line is still inlined, and the Z2 conventions string names `Zenith2/CLAUDE.md` and `Zenith2/Docs/WorkerGuide.md` explicitly.
- **Alternatives considered:** (1) No mention at all — rejected: the root CLAUDE.md would govern Zenith2 tickets. (2) Zenith2 rules merged into the root CLAUDE.md — rejected: every existing project would read them.
- **Consequences:** The blindness check (`git grep -nE '\bZenith2/'` outside `Zenith2/`) matches only `vault.config.json` and `zagent.project.json` until the PROC tickets add the allowlisted mentions. Specified in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** HarnessParity, ZagentClient, root delegation line, root CLAUDE.md, Zenith2/CLAUDE.md, universal convention, blindness check, allowlist, git grep
- **Related:** Z2-D-044, Z2-D-119, Z2-D-128
### Z2-D-031 CI workflow zenith2-gates.yml runs the union of active Z2 gates minus acceptance
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A local tick runs only the gates its diff selects, so CI must run a superset to catch what path selection misses, and shared harness changes must reach the Z2 checks too.
- **Decision:**
  - `.github/workflows/zenith2-gates.yml`, allowlisted, runs only when the user pushes.
  - It is triggered by changes to any of: `Zenith2/**`; the workflow itself; `zagent.project.json`, `vault.config.json`; `Tools/tick_gates.ps1`, `Tools/Vault/**`, `Tools/build_lock.ps1`. So shared harness changes also reach the Z2 integration checks.
  - CI runs the union of every Z2 category's active gates, excluding `Zenith2Acceptance`; a local `/tick` runs the path-selected subset. For any diff the local set is a subset of the CI set, and a test in `Zenith2/Tools/Gates/Tests` asserts that.
  - CI calls `Zenith2/Tools/Gates/Run-Committed.ps1`. The Linux job runs only if a `linux-tsan` line is active.
- **Alternatives considered:** (1) CI runs the same path-selected subset as a tick — rejected: no superset to catch selection errors. (2) CI on every commit — rejected: nothing is pushed by the loop; the user pushes.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"` and `Zenith2/Docs/Roadmap.md → "Integration cadence"`.
- **Nouns:** zenith2-gates.yml, CI, Run-Committed.ps1, gate union, path-selected subset, Linux job, linux-tsan, Zenith2Acceptance, Zenith2/Tools/Gates/Tests
- **Related:** Z2-D-032, Z2-D-044, Z2-D-083
### Z2-D-032 Gate activation has one source of truth: the Z2 categories' gates lists in zagent.project.json
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** If CI and local ticks read gate lists from different places, a step can be active in one and not the other. The gate list grows stage by stage as tooling lands.
- **Decision:**
  - One committed source of truth: the Z2 categories' `gates` lists in `zagent.project.json`.
  - PROC tickets activate new steps by adding lines there. `/tick` runs them through `tick_gates.ps1` in its generic external-build mode.
  - CI calls `Zenith2/Tools/Gates/Run-Committed.ps1`, which reads the union of every Z2 category's gate lines straight from the committed file, with no ticket and no claim. So CI and local runs always execute the same active set.
  - Every gate line has the form `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 …`. Every step is self-sufficient and order-independent (Build configures if needed; Test builds if needed), writes only under `Zenith2/out/<checkout-hash>/`, never into `.zagent/run/`, and sweeps its own executables scoped to the checkout path.
  - Stage 0 activates the base list (the Windows build, the tests, the structural checks and the coverage gate step). Android build and APK, sanitizers, backend parity and path parity are each added by a `needs-human` PROC ticket in the stage that brings the matching tooling (Stages 2, 3 and 5).
- **Alternatives considered:** (1) A separate CI gate list — rejected: two lists drift. (2) Gate lists in each ticket body — rejected: a ticket's list replaces its category's, and bodies go stale.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"` and `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`.
- **Nouns:** gate activation, gates list, zagent.project.json, Run-Committed.ps1, tick_gates.ps1, external-build mode, gate line, pwsh -NoProfile, gate step, Zenith2/out, PROC ticket
- **Related:** Z2-D-031, Z2-D-044, Z2-D-086, Z2-D-117
### Z2-D-033 The whole backlog is filed before the scaffold ticket runs
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The coverage gate step checks that every inventory item is covered by requirements and tickets. If the scaffold ran before the backlog existed, that step would be red from its first run.
- **Decision:** The whole backlog is drafted, signed off and filed before the scaffold ticket runs, so the coverage gate step is green from its first run.
- **Alternatives considered:** (1) File tickets stage by stage as work progresses — rejected: the coverage step would be red, and blocking links to unfiled tickets could not exist. (2) Run the scaffold first and exempt coverage — rejected: an exempted gate step is not a gate.
- **Consequences:** Stage 0 closes with the scaffold as its last ticket (Z2-D-128). Specified in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** zenith_add_library, Test_CoverageFixtures, backlog, scaffold ticket, coverage gate step, Coverage step, Coverage.ps1, File-Backlog.ps1, filing
- **Related:** Z2-D-067, Z2-D-069, Z2-D-116, Z2-D-128
### Z2-D-034 Zenith Hungarian naming retained
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The naming convention is the most visible style decision and the one a worker applies on every line. Keeping Zenith's lets prior art be read without translation.
- **Decision:**
  - Zenith Hungarian is retained. Core classes are `Zenith_ClassName`, renderer classes `Flux_ClassName`.
  - Scope prefixes: `m_` member, `s_` static member, `g_` global, `ls_` local static. Type prefixes after the scope prefix: `x` struct/class instance, `p` pointer, `px` pointer to class/struct, `a` array, `u` unsigned int, `b` bool, `f` float, `str` string, `e` enum value, `pfn` function pointer, `ul` 64-bit unsigned, `i` signed int.
  - Amended 2026-10-23 by Z2-D-141 (the human's ruling): `u` covers `uint` and `uint8` to `uint32`, `ul` `uint64`, `i` covers every signed type, `sint` and `sint8` to `sint64`, and `c` is `char` (`ac` a `char` array, `sz` a pointer to a NUL-terminated C string); integers are the `Zenith_Types.h` names `uint8` … `uint64`, `sint8` … `sint64`, `uint` and `sint`; no `noexcept` or `[[nodiscard]]` unless a mechanism requires it, `final` only on a class with a base, and `const` members on `IMMUTABLE` types where possible.
  - Functions are PascalCase. Enums are `SCREAMING_SNAKE_CASE` with a category prefix (`CATEGORY_NAME_VALUE`). Constants are `SCREAMING_SNAKE_CASE` with a type prefix (`uFOO`, `fFOO`).
  - Braces on a new line for classes, functions and control flow; short inline one-liners in headers may use same-line braces. Tabs. `public:`, then `protected:`, then `private:`. `#pragma once`; no `using namespace` in headers.
  - The adopted code-style subset is the one in `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`. The root repository's rules that every `.cpp` begins with `#include "Zenith.h"` and that tools code sits under `#ifdef ZENITH_TOOLS` do not apply to Zenith2.
- **Alternatives considered:** (1) A standard style (Google, LLVM) — rejected: prior-art code and existing reviewers read Zenith Hungarian. (2) Plain camelCase without type prefixes — rejected: loses the ownership and type cue a reviewer reads at a glance.
- **Consequences:** `clang-format` (the `Format` step) enforces layout. Specified in `Zenith2/Docs/Contracts.md → "API boundary"` and summarised in `Zenith2/Docs/WorkerGuide.md`.
- **Nouns:** Zenith Hungarian, naming convention, Zenith.h, ZENITH_TOOLS, Zenith_ prefix, Flux_ prefix, m_, s_, g_, ls_, px, pfn, SCREAMING_SNAKE_CASE, PascalCase, clang-format, Format step
- **Related:** Z2-D-047
### Z2-D-035 Clustered forward+ by default; forward, deferred and visibility buffer supported behind a capability matrix
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Clustered forward+ is the path that suits both the S24 and desktop, but comparing shading architectures is a research deliverable (Z2-D-064), and a reference path is needed for parity. Paths with different capabilities must declare what they omit.
- **Decision:**
  - Clustered forward+ is the default and the only path on Android.
  - Forward (non-clustered; the parity reference, previews and captures), deferred (desktop only) and visibility buffer (desktop only) are supported, behind `Flux_ShadingPath` with a capability matrix.
  - Materials and lighting are single-source `EvaluateMaterial`/`EvaluateLighting`, with explicit gradients, shared by every path.
  - There is no MSAA in any path; antialiasing is TAA or the temporal upscaler.
  - Forward+ lands in Stage 3 on both APIs and the S24; forward, deferred and visibility buffer land in Stage 5 with path parity.
- **Alternatives considered:** (1) Deferred as the default — rejected: G-buffer bandwidth on the S24 and weak transparency handling. (2) One path only — rejected: the architecture comparison is a deliverable, and a reference path is needed for parity. (3) Per-path material code — rejected: parity would compare different materials.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Shading paths and capability matrix"`, `Zenith2/Docs/Rendering.md → "Frame anatomy Forward+"`, `Zenith2/Docs/Rendering.md → "Frame anatomy Forward"`, `Zenith2/Docs/Rendering.md → "Frame anatomy Deferred"` and `Zenith2/Docs/Rendering.md → "Frame anatomy Visibility buffer"`.
- **Nouns:** Forward+, clustered forward, Forward path, Deferred path, visibility buffer, Flux_ShadingPath, capability matrix, EvaluateMaterial, EvaluateLighting, explicit gradients, MSAA, TAA, temporal upscaler, path parity
- **Related:** Z2-D-036, Z2-D-063, Z2-D-064
### Z2-D-036 No tile-local merging until D-TBDR is measured on the S24
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Tile-local reads (keeping attachments on-chip between passes) can save bandwidth on a tile-based GPU, but they constrain the graph and are only worth it if measured. Two-phase HiZ culling also forces a mid-frame depth store on tile-based GPUs.
- **Decision:**
  - No tile-local merging until Stage 5 decision D-TBDR (open, Stage 5), measured on the S24.
  - The graph's attachment-compatible pass merging (Z2-D-014) is allowed; it is not tile-local-read merging.
  - Two-phase culling's mid-frame depth store and reload has its own budget row, `Z2-B-RPROD-026` (HiZ mid-frame depth store and reload bandwidth), and the D-TBDR (open, Stage 5) measurement covers it.
  - The D-TBDR (open, Stage 5) revalidation is a Stage 5 ticket and blocks only Stage 5 and subsequent rendering tickets.
- **Alternatives considered:** (1) Design for tile-local reads from the start — rejected: constrains the graph before any measurement shows a gain. (2) Never consider tile-local reads — rejected: forgoes a known mobile bandwidth win without measuring it.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"` and `Zenith2/Docs/Rendering.md → "Hierarchical-Z occlusion culling"`; the open decision is D-TBDR (open, Stage 5).
- **Nouns:** Flux_AddHiZBuild, Flux_RasterTargets, TBDR, tile-local merging, tile-local read, D-TBDR, attachment-compatible merging, mid-frame depth store, S24, Adreno counters
- **Related:** Z2-D-014, Z2-D-060
### Z2-D-037 A Slang compute path tracer with a software BVH, for physical-accuracy validation only
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Cross-path consistency proves the raster paths agree, not that they are right. A path-traced reference is needed, and it must run in headless CI on WARP and lavapipe, which have no ray-tracing hardware.
- **Decision:**
  - A Slang compute path tracer with a software BVH. It runs on WARP and lavapipe.
  - It serves physical-accuracy validation only, on controlled reference scenes, never as a shipping renderer.
  - It lives in the tool-side static library `ZenithPathTracer` (`Zenith2/Tools/ZenithPathTracer`), linked only by `ZenithFlux_Tests` and `ZenithPathTracer_Tests`, never by a runtime target.
  - It is required in Stage 5 with the physical-accuracy suite.
- **Alternatives considered:** (1) Hardware ray tracing — rejected: unavailable on WARP and lavapipe, and RT is deferred to Stage 7. (2) An external offline renderer as the reference — rejected: different materials and lights, so errors would be the translation's.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Validation roles"`.
- **Nouns:** ZenithPathTracer, ZenithPathTracer_Tests, Flux_PathTracer, path tracer, compute path tracer, software BVH, Z2-B-ADVGFX-004, Z2-B-ADVGFX-005, Z2-B-ADVGFX-007, path-traced reference, physical-accuracy validation, reference scene, WARP, lavapipe, convergence criterion
- **Related:** Z2-D-043, Z2-D-063
### Z2-D-038 JSON for authoring, versioned binary for cooked data, serialization never depends on C++ layout
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Authoring files must diff, merge and survive schema evolution; cooked files must load fast and fail cleanly on mismatch. Zenith-style layout-dependent serialization breaks whenever a struct changes.
- **Decision:**
  - JSON for authoring documents.
  - Versioned binary for cooked data; the runtime refuses mismatched cooked versions.
  - Serialization never depends on C++ layout; it goes through reflection with stable type and field IDs.
  - Authoring JSON is canonical (stable key order, one ID-keyed child per line), a Stage 2 SCENE requirement that the semantic merge driver depends on.
  - The JSON library is D-SEL-JSON (open, Stage 1).
- **Alternatives considered:** (1) A binary authoring format — rejected: no diff or merge. (2) Layout-dependent memcpy serialization — rejected: breaks on any struct change and across compilers. (3) YAML or a custom text format — rejected: weaker tooling and parsers for no gain over JSON.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`, `Zenith2/Docs/Contracts.md → "Reflection"` and `Zenith2/Docs/Architecture.md → "Reflection and runtime type registration"`.
- **Nouns:** Zenith_JsonWriter, Zenith_TypeRegistry, authoring JSON, cooked binary, versioned binary format, canonical serialization, stable key order, reflection-driven serialization, cooked version, D-SEL-JSON
- **Related:** Z2-D-020, Z2-D-048
### Z2-D-039 Three ECS backends — custom, flecs, EnTT through an adapter — all in Stage 2
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The ECS architecture comparison is a research deliverable, and a neutral ECS API is only proven by backends with different storage models. All three must be in place before the systems built on the ECS.
- **Decision:**
  - Three ECS backends: custom, flecs and EnTT (through an adapter), all delivered in Stage 2.
  - The API is `ZenithECS`; the backends are the separate library targets `ZenithECS_Custom`, `ZenithECS_Flecs` and `ZenithECS_EnTT`, each exporting its own factory (for example `Zenith_CreateWorld_Flecs`). The API target exports no factory. A game links the backend its `game.json` selects, at link time.
  - The `Zenith_ECS` API is type-erased by reflection ID. Data-defined components are stored natively: flecs through native runtime components; the custom backend designed for runtime layouts; EnTT through a dynamic-storage adapter (flagged cost P1).
  - The `Zenith2ECS` category builds and runs the conformance suite against all three backend targets. The backends are research subjects measured per Z2-D-064.
- **Alternatives considered:** (1) Custom ECS only — rejected: no comparison and no swap proof. (2) flecs only — rejected: the comparison is a deliverable. (3) Adding backends in a subsequent stage — rejected: systems built on one backend would bake in its assumptions.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "ECS model"` and `Zenith2/Docs/Architecture.md → "Worlds and isolation"`; the research methodology is `Zenith2/Docs/Design/RESEARCH.md`, and the catalogue is `Zenith2/Docs/Requirements/ECS.md`.
- **Nouns:** ZenithECS, Zenith_ECS, ZenithECS_Custom, ZenithECS_Flecs, ZenithECS_EnTT, Zenith_CreateWorld_Flecs, Zenith_CreateWorld_EnTT, Zenith_CreateWorld_Custom, flecs, EnTT, ECS adapter, dynamic-storage adapter, ECS conformance suite, Zenith2ECS
- **Related:** Z2-D-020, Z2-D-064, Z2-D-130
### Z2-D-040 Ownership: Zenith_Owned<T> is {T*} freed through the general heap; generational handles for shared references; no Zenith_Ptr; STL smart pointers forbidden
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith has no RAII and relies on a global service locator. Zenith2 needs one owning type whose layout never changes between configurations, use-after-free protection for shared references in every build, and allocator paths that sanitizers can see.
- **Decision:**
  - Three tiers: the general heap (a thin `Zenith_Heap` over an allocator chosen by D-SEL-HEAP (open, Stage 1), system `malloc` or mimalloc; it tracks sizes internally, so frees need no header); arenas (frame and graph lifetime); pools (fixed-size, hot paths).
  - `Zenith_Owned<T>` is `{T*}`: the same layout in every configuration, move-only, with no per-allocation header. It is created by `Zenith_New<T>(…)` and destroyed by `~Zenith_Owned` → `Zenith_Delete`, which runs the destructor then frees to the general heap, in every configuration. Nothing but the general heap backs a `Zenith_Owned`.
  - An `Owned<Derived>` → `Owned<Base>` conversion needs a virtual destructor, and asserts in every configuration that the two pointers are the same address, because the heap must be freed with the original pointer. There is no RTTI.
  - Shared references are generational handles: entities, components, assets, GPU resources, operations. Handles are the use-after-free protection that remains in every configuration.
  - Non-owning references are plain `T&`/`T*`, scoped to a call or to a documented lifetime. There is no `Zenith_Ptr`. STL smart pointers are forbidden (the human's ruling, not re-justified).
  - Allocation failure: `Zenith_TryNew` returns a `Zenith_Result`; `Zenith_New` goes to the configurable OOM handler.
  - Arenas use `Zenith_Construct`/`Zenith_Destroy`. Graph captures are trivially destructible. Bare `new`/`delete`/`malloc` exist only inside `Zenith_Heap`, the arenas and the pools.
  - Debug leak tracking is a side table in the general heap, in Debug builds only (address → size and source location), reported at shutdown. It adds no per-allocation header and changes no object's layout.
  - Sanitizers: the general heap is instrumented natively by ASan and HWASan when the system allocator is used in sanitizer builds. Arenas and pools poison and unpoison through `__asan_poison_memory_region`/`__asan_unpoison_memory_region` under ASan; under HWASan they tag through `__hwasan_tag_memory`/`__hwasan_tag_pointer`, with sizes rounded up to 16-byte granules (overflow-checked), short-granule encoding for the partial last granule, pointer tags drawn from 16–255, and a retag on free or reset.
  - Required tests: intentional defects each form of instrumentation catches — overflow past an arena block, use after arena reset, use after pool free, and 1-, 15-, 16- and 17-byte pool allocations.
- **Alternatives considered:** (1) `std::unique_ptr`/`std::shared_ptr` — rejected by the human's ruling (STL smart pointers forbidden). (2) A `Zenith_Ptr` non-owning checked pointer with a debug header — rejected: changes layout between configurations; sanitizers and generational handles already catch use-after-free. (3) A per-allocation header recording size and type — rejected: layout and memory cost in every allocation; the heap tracks sizes internally instead.
- **Consequences:** `ForbiddenCheck` rejects bare allocation and smart pointers outside the allocator TUs. Specified in `Zenith2/Docs/Contracts.md → "Ownership and allocation"`, `Zenith2/Docs/Contracts.md → "Handle kinds"` and `Zenith2/Docs/Contracts.md → "Errors and results"`.
- **Nouns:** Zenith_Owned, Zenith_New, Zenith_Delete, Zenith_TryNew, Zenith_Heap, general heap, arena, pool, Zenith_Construct, Zenith_Destroy, OOM handler, generational handle, Zenith_Ptr, smart pointer, unique_ptr, shared_ptr, debug leak table, ASan poisoning, HWASan tagging, short granule, virtual destructor, D-SEL-HEAP
- **Related:** Z2-D-011, Z2-D-041, Z2-D-045, Z2-D-046
### Z2-D-041 Custom containers only where justified by a decision record; no STL type in a runtime public header; the allowed standard headers list
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith grew custom infrastructure without recorded justification. Custom containers are sometimes worth their cost (allocator control, invalidation guarantees, no exceptions), but each is a maintenance burden an established alternative might avoid. STL types in public headers leak allocator and exception assumptions across the API boundary.
- **Decision:**
  - Custom containers where justified, each with a written contract documenting invalidation, alignment, overflow, allocation failure, complexity and thread safety. They are tested against STL oracles and with property-based tests.
  - Custom infrastructure must be justified: each custom container, allocator, heap wrapper or threading primitive has a decision record stating what it improves over the standard alternative, its maintenance cost, and measured acceptance criteria. Everything is assessed on measured benefit versus maintenance cost, and an established alternative may win.
  - The human's explicit rulings are not re-justified: STL smart pointers are forbidden, and so are exceptions outside the Luau firewall. They stand as recorded decisions (Z2-D-040, Z2-D-045).
  - The STL is allowed in tests and in private tooling. No STL type appears in a runtime public header.
  - Standard utilities that stay acceptable in runtime code: `<cstdint>`, `<cstddef>`, `<cstring>`, `<cmath>`, `<limits>`, `<type_traits>`, `<concepts>`, `<utility>`, `<new>`, `<bit>`, `<atomic>`, `<initializer_list>`, `<compare>`, `<source_location>`.
- **Alternatives considered:** (1) Use the STL everywhere — rejected: exception-based allocation failure, allocator coupling and unspecified layout across platforms in public headers. (2) Custom containers for everything, without justification, as in Zenith — rejected: unmeasured maintenance cost. (3) A third-party container library (EASTL) — rejected as a default: it is assessed per container in the decision records, where it may win.
- **Consequences:** `ForbiddenCheck` rejects STL types in runtime public headers. Specified in `Zenith2/Docs/Contracts.md → "Containers and standard library policy"` and `Zenith2/Docs/Contracts.md → "Enforceability matrix"`.
- **Nouns:** custom container, container contract, invalidation, STL oracle, property-based test, decision record, STL in public header, allowed standard headers, cstdint, cstddef, cstring, cmath, limits, type_traits, concepts, utility, new, bit, atomic, initializer_list, compare, source_location, ForbiddenCheck
- **Related:** Z2-D-009, Z2-D-040, Z2-D-045
### Z2-D-042 Hard gates: libgraph, complexity, unsafe allocation, forbidden constructs, test separation, API contracts, no Slang/DXC in runtime links
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The audit's failures were each a rule that existed in prose and was not enforced. Zenith2 turns every mechanically checkable rule into a gate step that fails the phase.
- **Decision:**
  - The hard gates are: libgraph (cycles, encapsulation, undeclared edges, leakage); the complexity gate (Zenith2 profile); unsafe allocation; forbidden constructs; shipping/test separation; type-level API contracts; and no Slang or DXC in runtime links.
  - The gate steps are `Configure`; `Build -Preset` (win-debug, win-release, android-arm64); `Test`; `SourceLayout`; `CompileCoverage`; `Libgraph`; `Complexity`; `ForbiddenCheck` (bare allocation, smart pointers, STL in public headers, exceptions outside the firewall, `lua_call` outside it); `Format` (`clang-format`, pinned version); `HeaderSelfContained` (every public header compiled alone in a generated TU); `ApiContractCheck` (type-level); `TestSeparationCheck`; `ApkCheck` (16 KB alignment); `Coverage`; `ReviewSignals` (report only); `ShaderChecks`; `BackendParity`; `PathParity`; `Sanitizers`.
  - Compiler and hygiene gates: warnings are errors (`/W4 /WX`, and `-Wall -Wextra -Werror` plus a curated `-Wconversion`/`-Wsign-conversion` subset); checked narrowing through `Zenith_NarrowCast`, which asserts, never a truncating `static_cast`. Amended 2026-10-23 by Z2-D-141 (the human's ruling): no `[[nodiscard]]` on `Zenith_Result` or any API; an intentionally ignored result is discarded explicitly with a reason, which review confirms, and ForbiddenCheck rule F25 refuses the attribute.
  - Tests are never in shipping targets: doctest and CTest, judged pass or fail only.
- **Alternatives considered:** (1) Review-only enforcement — rejected: the audit shows prose rules erode. (2) Pinned test counts, as in Zenith — rejected: a count proves nothing about what passed; pass or fail only.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"` and `Zenith2/Docs/Contracts.md → "Enforceability matrix"`.
- **Nouns:** hard gate, gate step, Configure, Build, Test, SourceLayout, CompileCoverage, Libgraph, Complexity, ForbiddenCheck, Format, HeaderSelfContained, ApiContractCheck, TestSeparationCheck, ApkCheck, Coverage, ReviewSignals, ShaderChecks, BackendParity, PathParity, Sanitizers, warnings as errors, Zenith_NarrowCast, doctest, CTest
- **Related:** Z2-D-007, Z2-D-017, Z2-D-040, Z2-D-041, Z2-D-047
### Z2-D-043 Advanced graphics classification: RT and virtualized geometry deferred to Stage 7, virtual texturing required Stage 4, hair excluded, path-traced reference required Stage 5
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Advanced rendering features are each large programmes. Without an explicit classification they either creep into early stages or silently drop out.
- **Decision:**
  - Hardware ray tracing: deferred to Stage 7.
  - Virtualized geometry: deferred to Stage 7.
  - Virtual texturing: required, Stage 4.
  - Hair/groom: excluded.
  - Path-traced reference: required, Stage 5 (Z2-D-037).
  - Deferred capabilities still get full requirement trees and detailed leaf tickets in their stage's sprint; the excluded one has a rationale and a decision ID and no tickets.
- **Alternatives considered:** (1) All advanced features required — rejected: the programme would not reach shipping qualification. (2) Leave classification to each area — rejected: implicit scope is the failure the scope rule forbids.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Purpose and scope"` and `Zenith2/Docs/Roadmap.md → "Stage 7 Later expansion"`; inventory rows are in `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`.
- **Nouns:** ray tracing, RT, virtualized geometry, virtual texturing, VT, Z2-B-ADVGFX-001, VIEW_KIND_STEREO, hair, groom, path-traced reference, ADVGFX, deferred capability, excluded capability
- **Related:** Z2-D-037, Z2-D-078
### Z2-D-044 Process: the Z2 board with categories Zenith2, Zenith2Render, Zenith2ECS, Zenith2Memory, Zenith2Acceptance; other projects unaware
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith2 needs its own board, gates and conventions without leaking any of them into the ZEN, ZM or DP projects. Gate selection is by diff paths, so categories must map paths to added gates.
- **Decision:**
  - A Z2 board, with categories on `project: Z2`; the union adds gates according to the paths a diff touches:
  - `Zenith2` — paths `Zenith2/**` — the base steps.
  - `Zenith2Render` — ZenithFlux, ZenithRHI*, Shaders, `Tools/ZenithShaderCompiler`, and the Cook mesh, texture and material importers — adds `ShaderChecks`, `BackendParity` (from Stage 3) and `PathParity` (from Stage 5).
  - `Zenith2ECS` — ZenithECS* — builds and runs the conformance suite against all three backend targets.
  - `Zenith2Memory` — ZenithCore allocators, containers and threading, the job graph, ZenithScript's VM glue — adds `Sanitizers -Preset win-asan` plus `linux-tsan` (change-triggered).
  - `Zenith2Acceptance` — no paths; selected explicitly by stage acceptance tickets only — holds the full acceptance suites (`-Stage N`) with a longer `phaseTimeoutMinutes`, exempt from the 30-minute cap and excluded from CI's union (Z2-D-083).
  - Other projects stay unaware of Zenith2 (Z2-D-030 is the one allowlisted mention).
- **Alternatives considered:** (1) File Z2 work on the ZEN board — rejected: ZEN's gates and conventions would govern it, and ZEN would see Zenith2. (2) One Z2 category with every gate — rejected: every ticket would pay for sanitizers and parity.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Z2 board, Zenith2 category, Zenith2Render, Zenith2ECS, Zenith2Memory, Zenith2Acceptance, category paths, gate union, blindness check
- **Related:** Z2-D-028, Z2-D-030, Z2-D-032, Z2-D-083, Z2-D-086
### Z2-D-045 No exceptions outside the Luau firewall; Zenith_Result for recoverable errors; asserts for programmer errors
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Mixed error models make every call site guess. Exceptions cost code size and hide control flow; asserts compiled out of release builds cannot guard against script misuse, which is user input.
- **Decision:**
  - `Zenith_Result<T,E>` for recoverable errors; hygienic asserts for programmer errors; no exceptions outside the Luau firewall (the human's ruling, not re-justified).
  - Amended 2026-10-23 by Z2-D-141 (the human's ruling): neither `Zenith_Result` nor any fallible API carries `[[nodiscard]]`; an ignored result is discarded explicitly with a reason and confirmed in review. `Zenith_Result<T, E>` is the plain `{ T m_xValue; E m_eError; }`, successful when the error code is 0, its misuse a debug-only assert.
  - Fallible construction goes through a factory returning `Zenith_Result`, and partially-built members are released by their own owners. Replaceable subsystems are created by a factory returning `Zenith_Result<Zenith_Owned<I>>`.
  - Script misuse never relies on asserts; it always raises a controlled Luau error.
- **Alternatives considered:** (1) C++ exceptions — rejected by the human's ruling. (2) Error codes with out-parameters — rejected: easy to ignore and awkward to compose. (3) Asserts for script misuse — rejected: compiled out, and script input is not a programmer invariant.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Errors and results"` and `Zenith2/Docs/Scripting.md → "Views epochs and misuse errors"`.
- **Nouns:** Zenith_Result, recoverable error, assert, programmer error, factory, fallible construction, controlled Luau error, script misuse, no exceptions
- **Related:** Z2-D-017, Z2-D-040, Z2-D-047
### Z2-D-046 No mutable global state except the root allocator and profiler hooks installed in main; no runtime service lookup
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's global service locator is used about 4,250 times, which hides every dependency and makes isolation, testing and shutdown order impossible to reason about.
- **Decision:**
  - No mutable global state, except the root-allocator and profiler hooks installed in `main`.
  - Dependencies are passed in constructors.
  - Runtime service lookup (registries queried by name or type for services) is forbidden.
  - Amended 2026-10-09 (dated extension of this decision by orchestrator ruling Q88 (a), not a new decision; FLAGGED FOR HUMAN ATTENTION because it extends a settled decision and plan §3 rule 4 — orchestrator ruling, human may override, with the fallback (c): the Crashpad realisation only): the crash handler anchor `g_pxCrashHandlerAnchor` (`std::atomic<Zenith_CrashHandler*>`, `Zenith2/Engine/ZenithPlatform/Internal/CrashHandler/`), the only state OS crash callbacks without user data can reach; everything else stays in the handler object. The anchor exists under either D-SEL-CRASH realisation; under Crashpad, whose own handlers read no Zenith state, it serves only `CRASH_HANDLER_ERROR_ALREADY_INSTALLED`. Its writers and readers are `Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-152]; the writable-data allowlist entry cites this decision (`Zenith2/Docs/Design/DEVT/Crash.md` → "Crash handler anchor"; cross-doc items X273, X282, X308).
  - Amended 2026-10-16 (dated extension of this decision by the human's ruling Q11 (a), not a new decision): the Debug thread state `g_xDebugThreadState` (`thread_local`, `Zenith2/Engine/ZenithCore/Internal/Profiling/`) is the per-thread state the plan §3.8 lock-order checker needs (its held-lock stack), with the job-worker, OOM-handler and failure flags and the no-allocation depth; the checker runs with no hook installed, so this is debug-checker state, not a profiler hook. The same ruling covers the log-record members of the hook table, which add no writable symbol (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-157]). Signposted, as the human required, in documentation and in code: the allowlist entry is `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries" [Z2-N-CONTRACT-152], whose `extensions` and `comment` members name this line ([Z2-N-CONTRACT-153]), and the variable's declaration carries the comment block of [Z2-N-CONTRACT-578] (`Zenith2/Docs/Design/CORE/Threading.md` → "Debug thread state").
  - Amended 2026-10-16 (dated extension of this decision by the human's ruling Q79 (a), not a new decision): ZenithProfiling's one allowlisted `thread_local`, `g_xProfilerThreadBuffers` (`Zenith2/Engine/ZenithProfiling/Internal/`), holds the per-thread buffers of the hook implementations and of the debug-draw recorder, which caches its per-thread ring, owner and generation in the same slot; no writable symbol is added (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-144]; `Zenith2/Docs/Design/DEVT/Profiler.md` → "Thread slots and the thread-local variable"; cross-doc item X247 (a)). Signposted as for Q11 (a): the allowlist entry is `Zenith2/Docs/Contracts.md` → "Writable-data allowlist entries" [Z2-N-CONTRACT-152], and the declaration carries the comment block of [Z2-N-CONTRACT-578].
- **Alternatives considered:** (1) A service locator, as in Zenith — rejected: the audit finding. (2) Singletons per subsystem — rejected: the same hidden coupling with more types. (3) A dependency-injection container — rejected: runtime lookup by type is itself a service locator.
- **Consequences:** The composition root wires every subsystem explicitly. Specified in `Zenith2/Docs/Contracts.md → "No mutable global state"` and `Zenith2/Docs/Architecture.md → "Composition root and Player"`.
- **Nouns:** mutable global state, Zenith_Heap, Player main, ForbiddenCheck, service locator, service lookup, root allocator hook, profiler hook, main, constructor injection, composition root, singleton, crash handler anchor, g_xDebugThreadState, g_xProfilerThreadBuffers, Debug thread state, debug-draw recorder, writable-data allowlist, ZENITH-EXEMPTION
- **Related:** Z2-D-024, Z2-D-040, Z2-D-081
### Z2-D-047 Contracts are written per type or interface, with one contract block each
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's headers are 97% comments, mostly per declaration and restating the code, while the facts a caller needs — ownership, threading, invalidation — are missing. Zenith2 documents the contract once, where the type is.
- **Decision:**
  - Contracts are written per type or interface, not per declaration. Every public type or interface carries one contract block: ownership, thread affinity, invalidation, units, error behaviour and version compatibility.
  - An individual declaration is documented only where it departs from its type's contract.
  - Implementation comments stay minimal, but they explain non-obvious algorithms, invariants and performance decisions. There is no history in source.
  - Public examples are compiled as tests.
  - `ApiContractCheck` proves a contract block exists; review confirms it is accurate. `ReviewSignals` reports the comment ratio.
- **Alternatives considered:** (1) Per-declaration doc comments — rejected: the 97% comment ratio the audit found. (2) Contracts only in external docs — rejected: they drift from the code.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Contract blocks per type"` and `Zenith2/Docs/Contracts.md → "Enforceability matrix"`.
- **Nouns:** contract block, per-type contract, ownership, thread affinity, invalidation, units, error behaviour, version compatibility, ApiContractCheck, ReviewSignals, comment ratio, compiled example
- **Related:** Z2-D-042, Z2-D-048
### Z2-D-048 Compatibility policy per kind: source API deprecation, statically linked modules, save migration, cooked re-cook, authoring-document migration with preview, backup, journaled write and unknown-field preservation
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Different kinds of data and API age differently, and one policy for all of them is either too strict for cooked data or too loose for saves. An interrupted migration must never corrupt an authoring document.
- **Decision:**
  - Source API: deprecated for one fix version, with a replacement, then removed.
  - Modules: statically linked, so there is no binary compatibility surface.
  - Saves: forward migration from every shipped save version of the current major release, with a migration fixture per version.
  - Cooked assets: re-cooked on any engine version change. The runtime refuses mismatched cooked versions, and patches are compatible only within one release line.
  - Authoring documents (scenes, prefabs, data-defined component schemas, animation, UI and cinematic documents, material definitions, project settings, importer `.meta` settings) are migrated, never merely re-cooked: a per-type schema version and migration chain (one step per version); an upgrade preview (a dry-run diff before anything is written); an automatic backup before writing; a journaled atomic write, so an interrupted migration resumes or rolls back cleanly; authored overrides preserved, keyed by stable authoring IDs; unknown fields preserved on round-trip, with a warning, never dropped; a migration fixture per version, per type.
  - The framework lands in Stage 2 (SCENE/ASSET); complete coverage in Stage 4 (EDIT); the policy is demonstrated with migration fixtures in Stage 6.
- **Alternatives considered:** (1) Re-import authoring documents from source on every version — rejected: authored overrides and hand edits are lost. (2) Binary compatibility for modules — rejected: modules are static, so the surface does not exist. (3) Load old cooked data with converters — rejected: a re-cook is simpler and exact.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"`.
- **Nouns:** Zenith_MigrationChain, Zenith_MigrationRegistry, Zenith_PfnMigrationStep, compatibility policy, source API deprecation, save migration, migration fixture, cooked version, re-cook, release line, authoring-document migration, migration chain, upgrade preview, dry-run diff, automatic backup, journaled atomic write, authored override, unknown-field preservation, schema version
- **Related:** Z2-D-020, Z2-D-024, Z2-D-038
### Z2-D-049 API review is enforced from the diff through reviewPaths and revision-bound review records
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A label-driven review depends on someone remembering to apply the label. A public-header change must be reviewed whatever the ticket's risk, and a review is void once the reviewed code changes. Category conventions do not currently reach the reviewer prompt.
- **Decision:**
  - API review is enforced from the diff, not from a label. PROC-API is a `needs-human` ticket, because it edits protected files. It contains: (1) a generic `reviewPaths` category key — `zagent gates` (run at measure, with the changed set) writes `review.requiredByDiff` into `gates.json`; (2) a `TICK.md` step 6.3 edit — the reviewer is dispatched when `review.required` from the payload, or `requiredByDiff`, is true — followed by `Build-HarnessArtifacts.ps1` regeneration for every harness, with the tier-mention allowlist re-derived; (3) a revision-bound review-completion signal, a new generic verb `zagent review record <KEY> --verdict <pass|fixes-applied> --reviewer <tier>`, written into the ticket's history with a hash over the current blobs of every changed path in the `reviewPaths` scope; (4) enforcement: `zagent finish --status Done` refuses when `requiredByDiff` is true and no review record's hash still matches the current blobs, so any change to a reviewed path after the review invalidates it; (5) a test that a post-review edit to a reviewed header is refused at `finish`; (6) Vault tests for each of these.
  - Reviewer conventions: `TICK.md` step 6.3 gives the reviewer only `diff.patch` and `review.reasons`. A definite Stage 0 PROC ticket adds a generic per-category `reviewerConventions` field, inlined into the reviewer prompt. The Z2 value instructs reviewers to challenge behaviour beyond the author's tests and to check each ticket's "Review focus" list.
  - The Z2 categories list their public root headers and generated public API outputs as `reviewPaths`.
  - Risky changes get independent review. A refactor is owed when a change leaves a known shortcut behind, and it is filed as a ticket in the same stage. Every subsystem has a named owner (an area epic), responsible for its maintenance tickets.
- **Alternatives considered:** (1) An `api-change` label that triggers review — rejected: depends on the author remembering; the label survives only as an informational marker. (2) Review every ticket — rejected: cost on the single lane. (3) A review record without a content hash — rejected: a post-review edit would ride on the old review.
- **Consequences:** Specified in `Zenith2/Docs/Contracts.md → "Review and change management"` and `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** API review, PROC-API, reviewPaths, review.requiredByDiff, gates.json, TICK.md step 6.3, zagent review record, review record hash, zagent finish, reviewerConventions, Review focus, Build-HarnessArtifacts.ps1, api-change, subsystem owner, refactor owed
- **Related:** Z2-D-007, Z2-D-022, Z2-D-068
### Z2-D-050 Frame pipelining through render extraction: one frame of CPU render lag, simulation N+1 overlaps rendering N
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A serial simulate-then-render frame leaves workers idle, while letting rendering read live game state needs locks on everything. The Destiny model extracts an immutable snapshot and renders it while the next simulation runs.
- **Decision:**
  - The extract node (phase 12, Render extraction) snapshots render-relevant state into an immutable `Flux_RenderWorld` for frame N.
  - Frame N's GPU-graph build, record and submit nodes run concurrently with frame N+1's simulation, in the same pool, with no locks on game state.
  - There is one frame of render lag on the CPU side: simulation of N+1 overlaps rendering of N and never runs further ahead. GPU frames in flight (2–3, configurable) are separate.
  - The extra frame of latency is accounted for in the `Z2-B-RPROD-027` row (input-to-photon latency); the lag itself is the adopted `Z2-B-RPROD-021` row (CPU render lag).
  - Flux never sees Scene; Runtime extracts the render world for it.
- **Alternatives considered:** (1) Serial simulate then render — rejected: idle workers and a longer frame. (2) Rendering reads live state under locks — rejected: contention and nondeterminism. (3) Two or more frames of CPU lag — rejected: latency beyond the input budget.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "CPU frame graph and scheduler"`, `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"` and `Zenith2/Docs/Budgets.md → "Measured in every workload run"`.
- **Nouns:** frame pipelining, render extraction, Flux_RenderWorld, extract node, render lag, frames in flight, input-to-photon latency, ZenithRuntime, Destiny model
- **Related:** Z2-D-006, Z2-D-055, Z2-D-080
### Z2-D-051 The transaction boundary is the entity, never the chunk (D-SCRIPT-TXN as pre-decided in the plan; its Stage 1 ticket refines and confirms)
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** If a script failure rolled back a whole chunk, the outcome would depend on which entities shared a chunk, which changes with compaction and capacity, breaking determinism (Z2-D-019). The boundary must be one entity, and cross-entity reads must not observe in-flight writes.
- **Decision:**
  - The transaction boundary is the entity. A chunk-system call iterates entities, and each entity's processing is its own logical transaction; a behaviour callback is one entity's transaction.
  - Column snapshots are taken per chunk on first write (only that chunk's declared-writable columns, with their change-tracking bits), but restore is per row: a failure restores only the failing entity's rows and discards only that entity's side-effect sub-buffer. Other entities in the same call and tick commit normally.
  - The failing system or behaviour is disabled from the next tick boundary, never mid-tick, so every other invocation in this tick runs regardless of which worker failed first.
  - Cross-entity reads: a chunk system may read a column it writes in this phase only for its own row; other entities' rows are readable only in columns not written in this phase. Per-field authority makes this checkable when the schedule compiles, so no entity observes another's in-flight write.
  - Reductions and shared structures (sums, spatial grids, counters) are never written in place across entities; each entity emits a contribution into its private buffer, folded at the merge point in merge order with the fixed-order reduction tree.
  - Chunk snapshots live until the system's merge point, because merge-time capacity rejection can still roll back an entity's rows. Change-tracking versions advance at merge, not at write. The per-tick total of snapshot memory is the `Z2-B-SCRIPT-013` row (snapshot memory per tick); the snapshot cap is per entity, the `Z2-B-SCRIPT-012` row (snapshot memory per entity).
  - "Commit cannot fail": all capacity is reserved during the invocation (a reservation failure is a Luau error inside the transaction), and acceptance and rejection are both decided at merge, before commit.
  - This is D-SCRIPT-TXN (open, Stage 1) as pre-decided. Its Stage 1 ticket refines and confirms it, including the interpreted-versus-native safepoint conformance test (Z2-D-052), merge-time capacity (Z2-D-053) and async publication.
- **Alternatives considered:** (1) The chunk as the boundary — rejected: outcomes depend on layout. (2) The whole invocation as the boundary — rejected: one entity's failure discards every entity's work, and the outcome depends on how entities were batched. (3) No rollback, report only — rejected: partial writes and queued side effects would survive a failure.
- **Consequences:** Specified in `Zenith2/Docs/Scripting.md → "Transaction scope and the entity boundary"`, `Zenith2/Docs/Scripting.md → "Commit and failure semantics"` and `Zenith2/Docs/Scripting.md → "Component writes shadows and side-effect buffers"`.
- **Nouns:** Zenith_EntityTransaction, Zenith_ColumnSnapshot, Zenith_SideEffectBuffer, D-SCRIPT-TXN, entity transaction, transaction boundary, per-row restore, column snapshot, declared-writable column, side-effect sub-buffer, merge point, contribution, reduction tree, snapshot lifetime, change-tracking version, commit cannot fail, capacity reservation, cross-entity read, disabled at next tick boundary
- **Related:** Z2-D-019, Z2-D-052, Z2-D-053, Z2-D-080
### Z2-D-052 Script budgets count safepoints, wall-clock is only a fatal watchdog
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A wall-clock script budget makes the outcome depend on machine load, which breaks replay. Luau's interrupt fires at safepoints, which gives a deterministic count for the same bytecode.
- **Decision:**
  - The script CPU budget is a safepoint count. Luau's interrupt fires at safepoints (loop back-edges, calls and returns), not on every instruction, and the count is deterministic for the same bytecode, so it is replayable.
  - Native codegen (`--!native`) may not hit the same safepoints. D-SCRIPT-TXN (open, Stage 1) includes a conformance test over the adversarial and representative corpus asserting that interpreted and native safepoint counts match; if they do not, native codegen is disabled for budget-limited simulation invocations.
  - Time spent inside C++ bindings is not counted, so the wall-clock watchdog remains the backstop there.
  - Wall-clock time is only a watchdog. Tripping it is a fatal, non-replayable diagnostic event, never a recoverable rollback.
  - Exhausting the safepoint budget is a Luau error in the entity's transaction, rolled back like any failure. Per-world GC step budgets and memory caps through the allocator complete the script budgets.
- **Alternatives considered:** (1) A wall-clock budget with rollback — rejected: nondeterministic. (2) A per-instruction count — rejected: Luau's interrupt does not fire per instruction, and a hook per instruction is too slow. (3) No budget — rejected: a runaway script stalls the frame.
- **Consequences:** Specified in `Zenith2/Docs/Scripting.md → "Budgets safepoints and the watchdog"` and `Zenith2/Docs/Budgets.md → "Scripting rows"`.
- **Nouns:** Zenith_ScriptInterrupt, Zenith_ScriptWatchdog, Zenith_ScriptNativePolicy, safepoint count, script CPU budget, Luau interrupt, native codegen, --!native, wall-clock watchdog, fatal diagnostic, GC step budget, script memory cap, budget exhaustion
- **Related:** Z2-D-019, Z2-D-051
### Z2-D-053 Asynchronous results become visible only at a tick boundary in completion-ID order
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Pathfinding, streaming, physics queries and online calls complete at times that depend on thread scheduling. If gameplay saw them as they arrived, the simulation would diverge between runs.
- **Decision:**
  - Asynchronous results (pathfinding, streaming loads, physics queries, online calls) become visible only at a tick boundary, in completion-ID order.
  - The tick at which each result was published is written to the replay log, and replay publishes it at the same tick.
  - Shared capacity (entity IDs, event queues, container storage) is checked at the deterministic merge point, in merge order, not reserved first-come during parallel execution. An entity whose side effects exceed the remaining capacity at merge is rolled back deterministically, as an entity-level failure with a diagnostic; commit still cannot fail.
- **Alternatives considered:** (1) Deliver results immediately on completion — rejected: scheduling-dependent. (2) Deliver in arrival order at the tick boundary — rejected: arrival order is scheduling-dependent too.
- **Consequences:** Specified in `Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"` and `Zenith2/Docs/Architecture.md → "Determinism guarantee and mechanisms"`.
- **Nouns:** Zenith_ReplayAsyncPublication, Zenith_OperationHandle, asynchronous result, tick boundary, completion ID, D-SCRIPT-TXN, Z2-B-AI-003, Z2-B-SCRIPT-016, replay log, publication tick, merge-time capacity, capacity rejection, operation handle
- **Related:** Z2-D-019, Z2-D-051
### Z2-D-054 Fully bindless binding model: one 8-byte push constant per draw or dispatch
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Per-pass descriptor sets and layouts must be kept in sync with shaders by hand, which is a steady source of binding bugs, and they complicate GPU-driven indirect drawing.
- **Decision:**
  - Global bindless heaps for sampled textures, storage textures and buffers, plus a static sampler table.
  - Each draw or dispatch gets one 8-byte push or root constant, `{paramsBufferIndex, paramsOffset}`, which locates the pass's parameter struct in the per-frame upload ring.
  - Indirect draws get per-command data from a draw ID: `DrawIndex`/`gl_DrawID` on Vulkan (core in 1.2 as `shaderDrawParameters`, and in the profile), and on D3D12 a root-constant argument written into each `ExecuteIndirect` command. The shader indexes the culling-produced per-draw record (instance range and material slot) with it.
  - Resource fields are Slang `DescriptorHandle<T>` (8 bytes), subject to D-SLANG (open, Stage 1).
  - There are no per-pass descriptor sets, no layouts to keep in sync and no "bind bindless" calls.
  - Vertex data is not bindless for raster: raster passes use fixed-function vertex streams whose layout is declared in Slang, so the fetch unit decodes the compressed formats. Compute passes (skinning, the visibility-buffer resolve, GPU-scene tables) reach the same buffers through bindless storage views, with the shared Slang codec.
- **Alternatives considered:** (1) Per-pass descriptor sets, as in Zenith — rejected: layouts drift from shaders. (2) Bindless vertex pulling for raster — rejected: loses the fetch unit's format decode for compressed streams. (3) Larger push constants carrying parameters directly — rejected: the 8-byte indirection fits every API's limit and keeps parameters in one upload ring.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Binding model"` and `Zenith2/Docs/Rendering.md → "Decision D-SLANG"`.
- **Nouns:** bindless, bindless heap, static sampler table, push constant, root constant, paramsBufferIndex, paramsOffset, upload ring, draw ID, DrawIndex, gl_DrawID, ExecuteIndirect, per-draw record, DescriptorHandle, descriptor set, bindless storage view
- **Related:** Z2-D-013, Z2-D-015, Z2-D-062
### Z2-D-055 The render graph is rebuilt every frame in an arena; resources declared once in two phases; features are free functions over handles
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith's render graph declares every resource twice, orders passes by registration, and needs rebuild requests and placeholders when a feature toggles. Zenith2 derives everything from one declaration per frame.
- **Decision:**
  - The graph is built on `ZenithGraph` and rebuilt every frame, in an arena. Physical resources are cached across frames by hashing their descriptions.
  - A disabled feature simply adds no passes: no rebuild requests, no placeholders, no early-return callbacks.
  - Features are free functions over handles, for example `Flux_RGTexture Flux_AddSSAO(Flux_RenderGraph&, const Flux_ViewContext&, const Flux_SSAOSettings&)`. Composition is plain data flow.
  - A resource is declared once, in two phases. Setup: the pass fills its generated parameter struct with virtual graph handles (resource, version and view), for example `pxParams->m_xDepth = xGraph.SRV(xView.m_xDepth)`. Compile: the graph walks the struct's field table to derive reads, writes, stages and access. Record: the fields are patched to real descriptor values.
  - Render targets live in a separate CPU-only `Flux_RasterTargets` (attachments, load/store ops, resolves), which also declares the vertex, index and indirect-argument buffers the pass draws from (`Flux_VertexStreamRef`, `Flux_IndexBufferRef`, `Flux_IndirectArgsRef`), so the compute-written skinned pool, the particle pool and culling-produced indirect arguments get their vertex-input and indirect-read barriers synthesized automatically. The lambda can bind only the buffers the pass declared.
  - Every write returns a new handle version. Passes flagged `Flux_SideEffect`, and resources marked with `MarkOutput`, are never culled.
  - Pass types: `AddComputePass` and `AddFullscreenPass` need no lambda; `AddRasterPass(name, params, targets, lambda)` takes a trivially destructible capture stored as an arena `Zenith_Function`. The lambda receives a `Flux_RasterContext` or `Flux_ComputeContext`, which exposes only draw, dispatch and parameter operations.
  - Compile step: cull unused passes; topological order (`ZenithGraph`); lifetimes indexed by `(queue, position)`; memory placement (aliased or dedicated); automatic barrier synthesis; queue assignment; partitioning into command lists for parallel recording. Each pass gets GPU timestamp queries automatically.
- **Alternatives considered:** (1) A persistent graph with rebuild requests, as in Zenith — rejected: toggles need placeholders, and declarations are duplicated. (2) Registration-order pass ordering — rejected: hides dependencies; ordering is topological from declared access. (3) Pass classes with virtual setup/execute — rejected: free functions over handles compose as plain data flow without inheritance.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Render graph"`, `Zenith2/Docs/Rendering.md → "Barrier synthesis"` and `Zenith2/Docs/Architecture.md → "ZenithGraph shared core"`.
- **Nouns:** render graph, Flux_RenderGraph, ZenithGraph, graph arena, physical resource cache, Flux_RGTexture, Flux_AddSSAO, Flux_ViewContext, feature function, virtual graph handle, handle version, field table, Flux_RasterTargets, Flux_VertexStreamRef, Flux_IndexBufferRef, Flux_IndirectArgsRef, Flux_SideEffect, MarkOutput, AddComputePass, AddFullscreenPass, AddRasterPass, Zenith_Function, Flux_RasterContext, Flux_ComputeContext, pass culling, queue assignment
- **Related:** Z2-D-006, Z2-D-015, Z2-D-056, Z2-D-057
### Z2-D-056 Barrier synthesis is completely automatic; pass contexts expose no barrier API
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Hand-written barriers are the largest class of silent GPU defects: a missing one corrupts intermittently, an extra one costs time, and neither is visible in a review. The graph already knows every access, so it can synthesize them all.
- **Decision:**
  - Graphics programmers never write a barrier, a layout transition, a queue-ownership transfer or an aliasing barrier, and cannot: the pass contexts expose no command list and no barrier API. Only `ZenithFlux/Internal/Graph` can reach the RHI's barrier calls.
  - Every access is derived from the parameter struct's field table (SRV, UAV or tile-local, plus the shader stage) and from `Flux_RasterTargets` (attachment and depth read/write).
  - For each subresource the compiler tracks its state through the pass order; merges compatible reads; batches each pass's barriers into one `vkCmdPipelineBarrier2` or Enhanced Barriers call; emits aliasing discards on first use; handles cross-queue ownership at queue boundaries; and places split barriers where the gap between producer and consumer allows.
  - Validation: every graph unit test and render test runs with Vulkan synchronization validation and D3D12 GPU-based validation, and is clean. A new technique cannot land with a hazard.
- **Alternatives considered:** (1) Manual barriers with validation layers as the safety net — rejected: validation finds only the hazards a test exercises. (2) Barrier hints from passes — rejected: a hint API is a barrier API. (3) Coarse full barriers between passes — rejected: serialises async compute and costs GPU time.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Barrier synthesis"` and `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`.
- **Nouns:** barrier synthesis, automatic barriers, ZenithFlux/Internal/Graph, vkCmdPipelineBarrier2, Enhanced Barriers, layout transition, queue-ownership transfer, aliasing barrier, split barrier, subresource state, synchronization validation, GPU-based validation
- **Related:** Z2-D-055, Z2-D-057, Z2-D-058
### Z2-D-057 Transient aliasing on wherever possible with a runtime toggle; the first-write rule; poison-on-first-use
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Zenith aliases only same-format slots, wasting memory. Aggressive aliasing saves memory but turns undefined-read bugs into intermittent corruption, so it needs a runtime off switch and a rule that makes the on and off modes produce identical images.
- **Decision:**
  - With aliasing on: transients are grouped per heap and memory type, sorted by size (largest first), and placed first-fit into free lifetime intervals at arbitrary offsets, across formats (VMA aliasing, or D3D12 placed resources). Each resource's first use gets a discard or UNDEFINED transition, synthesized automatically.
  - `Flux_GraphSettings::m_bAliasTransients` is a runtime toggle, available from the on-screen debug UI and the console. The next graph compile switches placement: off gives each transient its own dedicated (still cached) allocation; on resumes aliased placement. There are no restarts, and nothing else in the graph changes.
  - The first-write rule: fresh GPU memory is undefined even in a dedicated allocation, and masking does not help (`0 × NaN = NaN`). So a transient's first access is a clear or a declared full-coverage write, and a read of any region not yet written is a graph compile error.
  - A debug poison-on-first-use mode fills every transient with NaN or a sentinel pattern before its first write, so any undefined read shows up.
  - Both modes are first-class: the parity suite renders every scene with aliasing on and off, with poisoning enabled; the images are bit-identical, and sync validation is clean in both modes. The on-screen profiler shows transient memory with and without aliasing, and the savings. Toggling is the first diagnostic for any suspected aliasing bug.
- **Alternatives considered:** (1) Same-format aliasing only, as in Zenith — rejected: leaves most of the saving unrealised. (2) Aliasing with no runtime toggle — rejected: an aliasing bug needs a one-switch diagnostic. (3) Zero-fill on first use instead of the first-write rule — rejected: costs bandwidth and still hides reads of unwritten regions.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Transient aliasing"` and `Zenith2/Docs/Rendering.md → "Validation roles"`.
- **Nouns:** transient aliasing, Flux_GraphSettings, m_bAliasTransients, aliasing toggle, first-write rule, full-coverage write, poison-on-first-use, lifetime interval, first-fit placement, VMA aliasing, placed resource, dedicated allocation, aliasing savings, parity suite
- **Related:** Z2-D-005, Z2-D-055, Z2-D-056
### Z2-D-058 GPU scene tables follow the UE5 GPU Scene model; tables are read-only, dynamic content reaches consumers through pools
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** GPU-driven rendering reaches most resources indirectly, through table entries the graph cannot see per frame. If a table-reached resource could also be written in the frame, the graph would miss the hazard.
- **Decision:**
  - Indirectly reachable resources follow UE5's GPU Scene model (`FGPUScene`, RDG external buffers, scatter upload).
  - The table buffers (instance, primitive, material-slot and light data) are a small fixed set of persistent buffers, imported into the graph every frame as external buffers; passes read them through ordinary parameter-struct fields.
  - The CPU never writes table memory directly. Updates are queued as records in the frame's upload ring; a scatter-upload graph pass applies them, declaring a write on the table buffer and running at a fixed point in the frame, before any consumer. Growth is a resize-and-copy graph pass. Earlier frames' reads are ordered before a subsequent scatter write by the read-to-write barrier the graph emits, because the table is a tracked external buffer; consumers on other queues are covered by per-queue timelines and ownership transfers. No CPU-side versioning is needed.
  - Everything a table references (textures, mesh and vertex buffers, material constants) is registered as `Flux_PersistentReadOnly`: in a steady readable state, not tracked per frame, and unable to hand out a UAV or render-target view to any pass (enforced by the type system and validated by the graph). So any number of tables and direct fields may reference the same resource.
  - Lifetime: referenced resources are reference-counted by the asset system, with deferred deletion once every queue has completed the frames that referenced them. Freed table slots are recycled only after that same fence.
  - Resources written during the frame (VT physical and page-table updates, VT feedback, GPU simulation state, streaming uploads) are never reached through tables; they are ordinary graph resources.
  - Dynamic content reaches consumers through pools (skinned-vertex, particle, procedural-geometry, VT physical/page-table). A table entry references a `Flux_PersistentReadOnly` resource or a slot within a pool. A pool is an ordinary graph resource whose producer pass declares a write; every consumer binds the pool as a parameter-struct field, declaring a read; the table supplies only the slot index. The skinned-vertex pool is double-buffered (current and previous skinned positions) for velocity.
  - Texture streaming uses reallocate-and-swap: the new resource is filled on the copy queue, swapped into the table by a scatter-upload once the copy fence completes, and the old one retires through deferred deletion.
  - Required tests: skinned meshes drawn in depth, base and shadow passes the frame they are skinned; GPU particles simulated then drawn; procedural geometry regenerated mid-frame; a VT tile uploaded then sampled; two tables referencing the same texture during a streaming swap; a slot freed and reused across frames in flight; a pass that tries to write a persistent read-only resource (rejected); all with sync validation clean.
- **Alternatives considered:** (1) CPU writes into mapped table memory with CPU-side versioning — rejected: queue order alone does not order memory access, and versioning is error-prone. (2) Track every table-referenced resource in the graph per frame — rejected: cost scales with scene size. (3) Allow writable resources behind tables — rejected: the graph could not see the hazard.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "GPU scene tables and pools"` and `Zenith2/Docs/Rendering.md → "GPU lifetime and synchronisation"`.
- **Nouns:** GPU scene, GPU Scene model, table buffer, instance table, primitive table, material-slot table, light table, scatter upload, scatter-upload pass, resize-and-copy pass, external buffer, Flux_PersistentReadOnly, pool, skinned-vertex pool, particle pool, procedural-geometry pool, VT page table, deferred deletion, reallocate-and-swap, table slot recycling
- **Related:** Z2-D-055, Z2-D-056, Z2-D-062
### Z2-D-059 Render views: a stable view ID, view kinds, independent per-view histories
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Temporal techniques keep per-view history, and an editor with several viewports corrupts the main view's TAA if history is global. Shadow views, captures, previews and render-to-texture all need the same "render from here" unit with different pass sets.
- **Decision:**
  - A render view is one independent "render the scene from here" request, the unit a feature function receives and the unit per-view state belongs to. `Flux_View` holds: `m_xViewID` (a stable identity keying per-view persistent state across frames); `m_eKind` (the view kind, which selects the pass set); the camera (view and projection, previous-frame matrices, jitter, near/far); the depth mode (Z2-D-061); the extent (output resolution, dynamic-resolution scale, viewport); the outputs (swapchain, texture, cubemap face, shadow-atlas tile); the feature mask; the quality tier and per-view overrides; and persistent state (TAA/upscaler history, previous HiZ pyramid, exposure history, SSR previous colour, velocity history), independent per view.
  - Kinds: `VIEW_KIND_MAIN` (the game camera; the full frame for the active path); `VIEW_KIND_SHADOW` (one per cascade, per local shadowed light or per point-light cube face; GPU culling then depth only, no lighting or post); `VIEW_KIND_EDITOR` (each editor viewport; the main-view frame plus editor overlays; own histories); `VIEW_KIND_CAPTURE` (reflection-probe and IBL cubemap faces, the environment-lighting bake, screenshots; no TAA, no post, fixed exposure); `VIEW_KIND_PREVIEW` (asset, material and prefab thumbnails and material previews; a small forward frame against a preview scene); `VIEW_KIND_TEXTURE` (in-game render-to-texture cameras; the main frame at reduced quality into a texture the main view samples); `VIEW_KIND_STEREO` (Stage 7, XR; left and right eyes sharing culling; multi-view rendering).
  - Each frame the renderer builds a view list: main, then shadow views from the visible lights, then requested editor, capture, preview and texture views. Shared producers (GPU-scene upload, skinning, particle simulation, light list, shadow-atlas allocation) run once per frame, not per view. Per-view feature functions are called once per view with a `Flux_ViewContext` and add passes to the same frame graph, which schedules and aliases across views. Dependencies between views are explicit graph edges.
  - The view count per frame is capped per quality tier, and texture views and captures are round-robined across frames.
  - Views are never per-player: local multiplayer and split-screen are excluded (Z2-D-076).
  - Stage 2 delivers view identity, per-view resources and independent histories; Stage 5 adds advanced multi-view (many simultaneous captures and probes, stereo-ready paths).
- **Alternatives considered:** (1) One global camera with ad hoc passes for shadows and captures — rejected: histories collide and passes duplicate. (2) A separate graph per view — rejected: no cross-view scheduling or aliasing, and shared producers would run per view.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Render views"`.
- **Nouns:** render view, Flux_View, m_xViewID, view ID, m_eKind, view kind, VIEW_KIND_MAIN, VIEW_KIND_SHADOW, VIEW_KIND_EDITOR, VIEW_KIND_CAPTURE, VIEW_KIND_PREVIEW, VIEW_KIND_TEXTURE, VIEW_KIND_STEREO, view list, Flux_ViewContext, per-view history, feature mask, shared producer, round-robin capture
- **Related:** Z2-D-060, Z2-D-061, Z2-D-072, Z2-D-076
### Z2-D-060 Two-phase HiZ occlusion culling with a linear RG16F min/max pyramid, required in every path
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Single-phase occlusion culling against last frame's depth pops newly visible objects in a frame late. A pyramid of raw device depth depends on the depth mode and quantises badly in 16 bits.
- **Decision:**
  - HiZ is required in every shading path and runs for every main, editor and texture view, inside the shared GPU scene pipeline, after frustum culling.
  - The pyramid stores linear view-space depth. Mip 0 converts device depth once, per the view's depth mode, and rounds up to R16F; every coarser mip is the max of its 2×2 footprint, which is exact. There is no min/max inversion in any depth mode.
  - It is RG16F holding min and max linear depth: max (farthest, rounded up) drives occlusion culling; min (nearest, rounded down) drives SSR's hierarchical tracing. Half-float relative precision is about 0.1% (about 0.25 m steps at 500 m). Far-plane or cleared texels, and any linear depth at or above 65,504 m, store +∞, which never culls. Odd dimensions fold the extra row or column into the edge texel. It is built in one compute dispatch with a single-pass downsampler (SPD style).
  - The test: project the candidate's bounding sphere or box to a screen rectangle plus its nearest depth; choose the mip where the rectangle covers at most 2×2 texels; cull if the nearest depth is farther than the pyramid's farthest over the footprint; always keep objects crossing the near plane or with degenerate projection. In phase 1 the nearest depth is linearised in the previous camera's view space.
  - Two phases: (1) instances visible last frame are frustum-culled against the current camera, their current bounds projected with the previous view-projection and tested against last frame's pyramid, unmodified; survivors draw to depth (or depth plus visibility). (2) This frame's pyramid is built from that partial depth. (3) Every instance not drawn in phase 1 (phase 1's rejects plus those invisible last frame) is frustum-culled and tested against the new pyramid with current bounds and view-projection; survivors draw; next frame's visibility bits are the union of both phases' draws. (4) The final pyramid is kept as the view's persistent HiZ. (5) On tile-based GPUs the mid-frame depth store and reload has its own budget row, `Z2-B-RPROD-026` (HiZ mid-frame depth store and reload bandwidth), covered by D-TBDR (open, Stage 5).
  - GPU-driven output: culling is compute over the GPU-scene instance buffer, writing compacted indirect arguments (`DrawIndexedIndirectCount`/`ExecuteIndirect` with a count buffer) per material bucket and pipeline; the CPU never sees per-object visibility. The visibility-buffer path repeats the two-phase test per meshlet and adds cone culling. Shadow views use frustum and distance culling, plus HiZ against a cascade-depth pyramid where the capability tier enables it.
  - Diagnostics: tested, frustum-culled, occlusion-culled and drawn counts per view and per phase in the on-screen profiler; a debug overlay of the pyramid and culled bounds; unit tests of projection and mip-selection math; a render test in which a fully occluded object records zero draws.
  - Delivered in Stage 3 (RPROD core).
- **Alternatives considered:** (1) Single-phase culling against reprojected last-frame depth — rejected: newly visible objects pop in a frame late. (2) A raw device-depth pyramid — rejected: depends on depth mode and quantises to tens of metres at 16 bits. (3) CPU occlusion queries — rejected: latency and CPU cost.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Hierarchical-Z occlusion culling"` and `Zenith2/Docs/Rendering.md → "Frame anatomy common prologue"`.
- **Nouns:** HiZ, hierarchical Z, two-phase occlusion culling, HiZ pyramid, RG16F, linear view-space depth, min depth, max depth, single-pass downsampler, SPD, phase 1, phase 2, visibility bits, persistent HiZ, indirect draw compaction, DrawIndexedIndirectCount, ExecuteIndirect, count buffer, meshlet culling, cone culling, cascade-depth pyramid
- **Related:** Z2-D-036, Z2-D-059, Z2-D-061
### Z2-D-061 Depth mode: standard Z with D24S8 by default; reversed Z with D32F_S8 only where the precision check requires it; stencil kept
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** D32F everywhere doubles depth bandwidth for views that do not need it, while a 2 km outdoor view needs reversed Z to avoid z-fighting. Vulkan guarantees only one of D24S8 and D32F_S8, and AMD drivers do not expose D24S8.
- **Decision:**
  - Two independent axes. Z direction by precision need: standard Z by default; reversed Z when the view's near/far ratio needs more precision than D24 gives (for example a 2 km outdoor main view). Format by capability: D24S8 where exposed, else D32F_S8 (for example AMD Vulkan drivers). Reversed-Z views always use D32F_S8.
  - D24S8 is the default including all shadow views.
  - Stencil (S8) is kept, for decals, masking and outlines.
  - Every combination runs through the same code: the linear HiZ plus a depth-mode abstraction for projection and clear values. No HiZ or shader code depends on the depth bit depth. Cross-API parity tests record that AMD takes the D32 path.
  - Enforced at graph compile: a D32 target needs the view's precision justification or the capability fallback.
- **Alternatives considered:** (1) Reversed-Z D32F everywhere — rejected: bandwidth on the S24 and in shadow views that do not need it. (2) Standard-Z D24 everywhere — rejected: insufficient precision for 2 km views. (3) Dropping stencil — rejected: decals, masking and outlines need it.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Render views"` and `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`.
- **Nouns:** depth mode, standard Z, reversed Z, D24S8, D32F_S8, precision check, near/far ratio, capability fallback, stencil, depth-mode abstraction, clear value
- **Related:** Z2-D-014, Z2-D-059, Z2-D-060
### Z2-D-062 Vertex data: Slang-declared streams, dedicated position stream, QTangent SNORM16x4, UNORM16 UVs with scale and bias, 16-bit indices with mesh splitting, precision-checked fallbacks
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Vertex fetch bandwidth dominates many passes on mobile, and uncompressed 32-bit attributes waste most of it. When a layout is declared in two places (the shader and the cooker), the writer and reader drift.
- **Decision:**
  - Vertex layouts are declared in Slang and nowhere else. A vertex-input field declares its semantic and stream; `[VtxFmt("snorm16x4")]` gives the storage format where it differs from the declared type (the fetch unit widens, so a compressed field is still declared `float4`); `[Stream(n)]` places it in stream n; `[PerInstance]` puts it on the instance stream. The legal formats are a closed vocabulary; a typo is a compile error.
  - `ZenithShaderCompiler` reflects each vertex struct and bakes the layout into the program's generated header (per-attribute semantic, format, stream, offset; per-stream stride). Runtime PSO creation builds the vertex input from that table, and the cook's packers are driven by the same table.
  - One codec, in the leaf library `ZenithVertexFormat` (depends only on Core; owned by GEOM). Its public root header `Zenith_VertexFormat.h` exports the CPU codec (quantise and pack, unpack, QTangent encode/decode, octahedral, half) and the generated layout-table types. Flux, ZenithCook and ZenithEditor all link it. The GPU half is `Zenith2/Shaders/Common/VertexFormats.slang` (decoders and encoders for passes with no fetch unit); both halves match bit for bit, round-trip-tested for every format. The GPU half-float codec uses only 32-bit integer arithmetic, so it needs no `shaderInt16`.
  - Streams: 0 position (dedicated), SNORM16x4 quantised to the mesh's tight AABB and dequantised by a per-mesh scale and bias from the GPU scene, FLOAT32x3 only if the cook's precision check fails (8 or 12 bytes); 1 surface, a QTangent in SNORM16x4 (8 bytes) — `t` and unsigned `b` extracted from the quaternion, `n = cross(t, b)` never sign-flipped, the handedness sign applied to the bitangent only (stored in the sign of `w`, the cook negating `q` so `w ≥ 0` means no reflection, `abs(w)` biased to at least 1/32767 with xyz renormalised) — plus UV0 UNORM16x2 remapped into [0,1] with a per-mesh UV scale and bias (FLOAT16x2 only if the check fails; 4 bytes); 1 normal-only variant, octahedral SNORM16x2 (4 bytes); 2 optional, UV1 UNORM16x2 and vertex colour UNORM8x4 (4/4 bytes); 3 skin, joints UINT8x4 (UINT16x4 only above 256 joints) and weights UNORM8x4 (UNORM16x4 only if the error check fails).
  - 32-bit vertex types only where a documented precision check requires them; the cook reports every fallback.
  - Index buffers are 16-bit. The cook splits any mesh above 65,535 vertices into sub-meshes sharing the parent's quantisation box and UV scale and bias, so boundary vertices never crack. Meshlets use 8-bit local indices. No 32-bit index buffers.
  - Fetch order: meshoptimizer vertex-cache, overdraw and vertex-fetch optimisation. Position-only draws (shadow views, and the opaque bucket of pure depth-only passes) use an extra position-only index buffer (`meshopt_generateShadowIndexBuffer`); not the Forward+ prepass, not alpha-tested buckets.
  - Unified per-stream pools with `vertexOffset`/`BaseVertexLocation`; indirect buckets keyed by (pipeline, stream-set); every precision fallback is a distinct Slang layout, stream-set pool and PSO permutation; the cook reports the permutation count per game, capped by the RPROD shader-permutation limit. Buffers get the union of their declared uses, part of the physical-resource cache key; compute reads 16-bit indices as u32 words with a halfword select.
  - The skinned-vertex pool stores FLOAT16x4 object-space positions (FLOAT32 layout if the skeleton's maximum reach fails the FLOAT16 tolerance), re-encodes the tangent frame as a QTangent with the bias rule, and is double-buffered. The GPU codec therefore has encoders as well as decoders, bit-matched to the CPU codec by tests.
  - D-VTX-TANGENTLESS (open, Stage 3) is an experiment; QTangent is the default until its ruling.
- **Alternatives considered:** (1) Uncompressed FLOAT32 attributes — rejected: bandwidth. (2) Zenith's two SNORM10:10:10:2 tangent words — rejected: the QTangent is the same 8 bytes with better quality. (3) Layouts declared in C++ or the cooker — rejected: two declarations drift; the runtime has no Slang, so the generated table is the only runtime source.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Vertex data format"` and `Zenith2/Docs/Rendering.md → "GPU performance rules and lints"`; the codec design is `Zenith2/Docs/Design/GEOM.md`.
- **Nouns:** vertex format, ZenithVertexFormat, Zenith_VertexFormat.h, VertexFormats.slang, VtxFmt, Stream, PerInstance, position stream, SNORM16x4, QTangent, octahedral normal, UNORM16x2, UV scale and bias, FLOAT16x2, UINT8x4, UNORM8x4, precision check, precision fallback, 16-bit index, mesh splitting, meshlet local index, meshopt_generateShadowIndexBuffer, position-only index buffer, unified stream pool, stream-set, skinned-vertex pool
- **Related:** Z2-D-014, Z2-D-015, Z2-D-054, Z2-D-058
### Z2-D-063 Validation roles kept separate: cross-path consistency, physical accuracy, independent analytical material tests
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Comparing raster paths with each other proves consistency, not correctness, and a bug in shared shading code passes both sides of any comparison built on it. Each validation question needs its own oracle.
- **Decision:**
  - Cross-path consistency: the raster paths are compared with each other and between Vulkan and D3D12, first on surface-data channels (near-exact), then on final images (FLIP plus per-channel thresholds). Only features that behave identically across paths are compared; path-specific approximations are disabled or have their own per-feature tolerance in the capability matrix.
  - Physical accuracy: the compute path tracer versus the raster paths on controlled reference scenes (analytic lights, IBL furnace, area-light shadows, VT-backed terrain materials), with screen-space approximations off. The tracer runs to a convergence criterion (per-pixel variance estimate below a threshold, with a minimum spp); error tolerances are set per scene. WARP and lavapipe runs are capped in resolution and wall-clock time per scene. Screen-space and approximate GI are validated against their own documented error budgets, not against convergence.
  - Independent analytical material tests: a separately written CPU reference BRDF in test code checks energy conservation (white furnace), reciprocity and closed-form cases, catching bugs the shared Slang shading code would pass to both renderers.
  - The parity scenes exercise every pass that appears in more than one path.
- **Alternatives considered:** (1) Golden images only — rejected: a golden captures a bug as correct. (2) Path tracer as the only oracle — rejected: shares the Slang material code, so it cannot catch material bugs.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Validation roles"` and `Zenith2/Docs/Rendering.md → "Per-pass specifications"`.
- **Nouns:** validation roles, cross-path consistency, physical accuracy, analytical material test, reference BRDF, white furnace, reciprocity, FLIP, surface-data channels, parity scene, BackendParity, PathParity, convergence criterion, reference scene
- **Related:** Z2-D-035, Z2-D-037, Z2-D-064
### Z2-D-064 Research methodology: every ECS scenario measured natively and through the adapter; controlled conditions; conclusions are decision records
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The architecture comparison is a deliverable, so it must measure the architecture, not the harness or the adapter. Uncontrolled clocks and single runs produce noise mistaken for findings.
- **Decision:**
  - ECS: every scenario is measured twice — natively (each backend through its own API, idiomatically, with no Zenith adapter; for EnTT both plain views and owning groups) and through `Zenith_ECS`. The adapter's costs are reported separately (copying, extra indirection, memory overhead, chunk-synthesis cost for EnTT), and job-scheduling overhead per backend. The report states which differences are architectural (native vs native) and which are adapter artefacts.
  - ECS scenarios: spawn/destroy churn; iterating 1, 2 and 4 components; random access; adding and removing components; hierarchy walks; Luau behaviours vs chunk systems; the W-Dense workload.
  - Shading paths: each path runs the same workload versions and quality settings, reporting GPU time per pass, memory and bandwidth (Adreno counters on the S24, vendor counters on desktop), the image-difference metrics of the validation roles, and the capability-matrix exclusions stated explicitly.
  - Research measurements are separate from shipping qualification: research uses controlled conditions (locked desktop GPU and CPU clocks; Android fixed-performance mode where available) and still reports run-to-run variance; shipping qualification runs on devices in their ordinary state, including throttling.
  - Procedure: fixed hardware, drivers and clocks; warm-up per Budgets v1; at least 10 repetitions; medians with 95% confidence intervals; raw data committed under `Zenith2/Docs/Research/`.
  - Conclusions are decision records. They never silently change a default.
- **Alternatives considered:** (1) Measure only through the common API — rejected: adapter costs would be reported as architecture. (2) Single runs on ordinary devices — rejected: noise and throttling dominate. (3) Conclusions applied directly to defaults — rejected: defaults change only through decisions.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Shading path research methodology"` and `Zenith2/Docs/Budgets.md → "Frame-rate definitions and measurement procedure"`; the ECS half is `Zenith2/Docs/Design/RESEARCH.md`.
- **Nouns:** Zenith_ECS, Flux_ShadingPath, research methodology, native measurement, adapter measurement, adapter artefact, owning group, chunk synthesis, controlled conditions, locked clocks, fixed-performance mode, repetitions, median, confidence interval, Docs/Research, Adreno counters, research report
- **Related:** Z2-D-035, Z2-D-039, Z2-D-084
### Z2-D-065 Evidence validity uses an input fingerprint, not path recency
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Evidence judged fresh because no file under a path changed since the run misses a dependency, toolchain, middleware or content change that altered the result.
- **Decision:**
  - Evidence records (for implemented coverage) state the tested tree SHA and build configuration, the workload version, the platform and backend, the result, and the artifact location.
  - Validity uses an input fingerprint, not path recency. Each record stores a fingerprint of: the transitive target-dependency closure of the code under test (blob hashes of every source in those targets, from the CMake File API plus libgraph); the toolchain and `ThirdParty/manifest.json` hashes; the shader-blob set hash; the content and workload version hashes; and the test-harness version.
  - Acceptance mode recomputes the fingerprint and rejects any mismatch.
  - Tests prove that a change only in a dependency, a toolchain bump, a middleware bump and a content bump each invalidate the evidence.
- **Alternatives considered:** (1) Path recency (no file under the tested path changed) — rejected: misses dependency and toolchain changes. (2) Re-run everything at acceptance — rejected: device and visual evidence is human work that cannot be re-run on demand.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Acceptance evidence"`; the fingerprint specification is a Stage 1 ticket.
- **Nouns:** evidence record, input fingerprint, ThirdParty/manifest.json, Coverage.ps1 -Stage N, implemented.md, evidence fingerprint, dependency closure, blob hash, toolchain hash, manifest hash, shader-blob set hash, workload version hash, harness version, acceptance mode, evidence validity
- **Related:** Z2-D-008, Z2-D-067, Z2-D-083
### Z2-D-066 Normative traceability: tagged bullets catalogued in Requirements/Normative.md
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A requirement buried in prose is invisible to coverage: nothing maps it to a ticket. Every obligation in the normative docs needs an ID a ticket can cite.
- **Decision:**
  - Every normative statement in `Architecture.md`, `Rendering.md`, `Scripting.md`, `Contracts.md` and `Budgets.md` is a tagged bullet, `- [Z2-N-<DOC>-NNN] …`, with `<DOC>` one of `ARCH`, `REND`, `SCRIPT`, `CONTRACT`, `BUDGET`.
  - The IDs are catalogued in `Zenith2/Docs/Requirements/Normative.md` (a name that avoids clashing with `Contracts.md`) and mapped to tickets like requirements.
  - Grammar enforced by `Coverage.ps1`: a configurable keyword list (must, required, never, forbidden, cannot, only, always, shall) is scanned in prose only (fenced code, tables and headings excluded); a keyword outside a tagged bullet fails unless the paragraph is inside an explicit `<!-- informative -->` … `<!-- /informative -->` block.
  - The structured requirement catalogue is authoritative; the scanner is only a consistency check. It cannot see obligations phrased without keywords, so capturing normative content is a checklist item in the human area sign-off.
- **Alternatives considered:** (1) Free prose with manual review — rejected: obligations go unticketed. (2) Tag every sentence — rejected: noise that hides the real obligations.
- **Consequences:** The inventory item IDs `Z2-I-<CODE>-NN` and the requirement IDs `Z2-R-<CODE>-NNN` sit beside the normative IDs. Specified in `Zenith2/Docs/Requirements/Inventory.md → "Purpose and versioning"` and `Zenith2/Docs/Contracts.md → "Enforceability matrix"`.
- **Nouns:** Test_NormativeGrammar, normative statement, tagged bullet, Z2-N, Normative.md, Coverage.ps1, keyword scan, informative block, requirement catalogue, normative traceability
- **Related:** Z2-D-067, Z2-D-068, Z2-D-124
### Z2-D-067 Coverage: backlog coverage tracked in Coverage.md; implemented coverage generated and untracked
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A tracked coverage file that changes with ticket status would drift under every ordinary ticket and conflict between lanes. Coverage must distinguish what the backlog promises from what has been done.
- **Decision:**
  - The input is `Zenith2/Docs/Requirements/Inventory.md` v1 (the approved v1 capability scope verbatim, with one ID per item, `Z2-I-<CODE>-NN`). Each `Zenith2/Docs/Requirements/<CODE>.md` breaks those items into requirements `Z2-R-<CODE>-NNN`, each recording its statement, the inventory items it covers, its stage, classification, governing decisions, budget rows and acceptance evidence. Changing the inventory needs a recorded decision and a version bump.
  - One owner per requirement: every v1 scope row has exactly one area code and each requirement belongs to exactly one area; anything spanning areas is owned by one and names the others in `## Dependencies`.
  - Backlog coverage (a requirement mapped to leaf and integration tickets) is a pure function of the filed backlog and is the only content of the tracked `Zenith2/Docs/Coverage.md`. It changes only when the filer runs.
  - Implemented coverage (tickets Done, evidence present) and implementation readiness (`Spec-Maturity: ready`, unblocked, not stale) are generated only into `Zenith2/out/<checkout-hash>/coverage/implemented.md`, never tracked. Acceptance mode reads it.
  - Classification: required and deferred capabilities both need detailed leaf tickets (a deferred ticket sits in its stage's sprint); excluded capabilities need a rationale and a decision ID, and have no tickets.
  <!-- elaboration -->
  - Game-side capabilities (quest, dialogue, inventory and interaction, Z2-D-077) are covered by OpenWorldSlice Luau-sample requirements in `Zenith2/Docs/Requirements/GAME.md`. Their leaf tickets sit under the GAME epic with fix version `Z2 Stage 4`, touch only paths under `Zenith2/Games/OpenWorldSlice/Scripts/`, and are proven by an integration ticket in the OpenWorldSlice acceptance definition. A game-side item never produces engine tickets.
  - `Coverage.ps1` (protected, read-only) regenerates into `Zenith2/out/<checkout-hash>/coverage/` and fails on drift from the tracked file, and on: an unmapped inventory item; a required or deferred requirement without a leaf implementation ticket and an integration or acceptance ticket; epic-only coverage; an excluded requirement without rationale and decision ID; a missing template section; a DoD with no failure scenario; a referenced decision, fixture or budget row neither existing nor planned (planned means an explicit producer ticket `blocks`-linked to the referrer; in acceptance mode `-Stage N`, every reference from a stage-≤N ticket exists on disk, in `DecisionLog.md` or in `Budgets.md`); incomplete routing; a stage acceptance ticket not listing every requirement of its stage; dangling links, `blocks` cycles or duplicate Draft-IDs; a pending reference whose ticket is not blocked by its producer; and a broken protected-work rule. It ships with failing fixtures, one fixture board per failure mode, and a test asserting each is detected.
  <!-- elaboration -->
  - `Coverage.ps1` also fails on a game-side requirement without those GAME tickets, on an engine ticket covering a game-side item, and on a duplicate or non-sequential `Z2-D-NNN` heading in `Zenith2/Docs/DecisionLog.md`.
  - The coverage gate step reports stale cites (a cited section's hash changed) and does not fail on them.
- **Alternatives considered:** (1) One tracked file with both backlog and implemented coverage — rejected: drifts under every ticket and conflicts between lanes. (2) No tracked coverage — rejected: the backlog's promise would have no committed record to diff.
- **Consequences:** Specified in `Zenith2/Docs/Requirements/Inventory.md → "Change rule"` and `Zenith2/Docs/Roadmap.md → "Acceptance evidence"`.
- **Nouns:** Test_CoverageFixtures, Coverage.md, Coverage.ps1, Coverage step, backlog coverage, implemented coverage, implemented.md, Zenith2/out/<checkout-hash>/coverage, Inventory.md, Z2-I, Z2-R, requirement catalogue, classification, required, deferred, excluded, failing fixture, acceptance mode, stale cite, planned reference
- **Related:** Z2-D-033, Z2-D-065, Z2-D-066, Z2-D-068
### Z2-D-068 Ticket standard: the ten body sections, Draft-ID, Spec-Maturity, Cites with hashes; decomposition per facet
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A worker holding only Read, Edit, Write, Grep and Glob, with no build and no questions, can implement only what its prompt contains. `build_prompt.ps1` inlines the step-zero survey, the category conventions, `## Goal`, `## Definition of Done`, then the entire rest of the body in order, then the ruling files — so the ticket body is the whole specification.
- **Decision:**
  - Body sections: (1) `## Goal` (scope and exclusions); (2) `## Implementation Contract`; (3) `## Dependencies` (prerequisites and cross-system dependencies); (4) `## Compatibility` (saved data, cooked data, public API, migration); (5) `## Definition of Done` (success and failure scenarios, as a `- [ ]` checkbox list); (6) `## Validation` (automated checks, fixtures, diagnostic evidence, and a "Review focus" list for the reviewer); (7) `## Budgets` (rows); (8) `## Ownership` (area epic and subsystem owner); (9) `Requirements:` and `Decisions:` (the governing `Z2-D-*` IDs; the rulings matcher reads only the Goal and DoD, so each governing ID is also cited there together with at least one identifier from that entry's Nouns line, because a citation alone never selects the entry, Z2-D-098); (10) `Draft-ID:`, `Spec-Maturity:` (`draft` | `decision-pending` | `ready`) and `Cites:` (doc path, heading) pairs and normative IDs, each with its content hash at filing. There is no `## Gates` section.
  - The Implementation Contract names the exact files (repo-root-relative paths), the exact public declarations in Zenith Hungarian with signatures, the tests (file, names, cases including failures), and at most 3 doc sections to read, each as a repo-root path plus exact heading text. These are starting points, not a fence: a worker may inspect callers; a contradiction between ticket, docs and code is reported, and the orchestrator blocks the ticket for revalidation.
  - The DoD avoids "human sign-off" phrasing and board references (the phrase rules are Z2-D-099), names rulings by `Z2-D-*` ID, and states observable checks, avoiding phrasing that forces review ("provably", "no new allocations") unless review is wanted. Compatibility and Ownership stay terse.
  - Decomposition: every substantive capability is split into bounded tickets per applicable facet — runtime behaviour, authoring and cooking, diagnostics, failure handling, integration evidence. Tickets are fine-grained. Tests land in the same ticket as the code they test.
  - Sizing (guidance): about one library, about 8 files and a 300–500 line diff; a coherent atomic change may exceed this, stating why. Complexity `SIMPLE` (mechanical against a full contract), `MODERATE` (ordinary), `COMPLEX` (algorithmic or concurrency-heavy). Risk `HIGH` where a defect is silent (memory, sync, determinism) or the change is a destructive migration, a security boundary, a release or packaging path, or a save-format change; `HIGH` escalates the model and makes review required.
  - Routing: category, parent epic, sprint, fix version, complexity, risk, points and labels; epics never get a sprint or fix version. `needs-gpu` needs exactly one `gpu-reason:*` label from the fixed set validated by `tick-checks.mjs:482-485`, of which Z2 uses three: `gpu-reason:render-capture` (parity and capture), `gpu-reason:visual-measure` (measurement) and `gpu-reason:requires-graphics-test` (a test that needs a graphics device); `gpu-reason:legacy-scene-reauthor` is Zenith-specific. `create` does not check the pairing, so the filer and the coverage checker do. Device steps are `needs-human`. Leaf tickets are `TASK` (or `BUG`/`SUBTASK`), never `STORY` (Z2-D-109). A ticket that edits a protected path is a PROC `needs-human` ticket naming the file and change; ordinary tickets may run protected checks and name them by step, never by path. Decision tickets list alternatives, measurements and an acceptance rule, and block their dependents.
  - Revalidation: the coverage gate step reports stale cites; after any Z2 ticket finishes Done, the tick runs `pwsh -NoProfile -File Zenith2/Tools/Backlog/File-Backlog.ps1 --mark-stale` (the per-category `postFinish` command, a Stage 0 PROC ticket), which applies `needs-revalidation` together with `deferred` (Z2-D-102, because `needs-revalidation` alone blocks no claim) and drops `Spec-Maturity` to `draft`. A `needs-revalidation` or `decision-pending` ticket is blocked until reconcile mode restores `ready`. No ticket body carries a placeholder for an unresolved value; a pending item names its decision or budget-row ticket, which blocks it.
  - Worker-executability test: could a worker holding only this ticket, the WorkerGuide and the at most 3 cited sections, plus the callers of what it changes, produce a gate-passing diff without asking a question? If not, the ticket is rewritten or split.
- **Alternatives considered:** (1) Short tickets pointing at docs — rejected: the worker cannot follow a pointer it was not given. (2) Separate "add tests" tickets — rejected: every diff proves itself in its own gates. (3) A `## Gates` section per ticket — rejected: it replaces the category's list and goes stale.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"`; the short form is in `Zenith2/Docs/WorkerGuide.md`.
- **Nouns:** ticket standard, Goal, Implementation Contract, Dependencies, Compatibility, Definition of Done, Validation, Review focus, Budgets, Ownership, Requirements line, Decisions line, Draft-ID, Spec-Maturity, Cites, content hash, decomposition rule, facet, postFinish, File-Backlog.ps1 --mark-stale, needs-revalidation, worker-executability test, build_prompt.ps1, gpu-reason
- **Related:** Z2-D-049, Z2-D-067, Z2-D-084, Z2-D-098, Z2-D-099, Z2-D-102, Z2-D-109, Z2-D-129
### Z2-D-069 Filing: seed branch in the main checkout, one squash commit, lanes idle throughout
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Filing thousands of tickets through the Vault writer makes one commit per operation. Landing those on master one at a time would regate every in-flight lane ticket repeatedly and could leave master holding a half-filed board.
- **Decision:**
  - Every ticket lands on master in one commit. Filing runs on a seed branch checked out in the main checkout (a worktree does not work; Z2-D-087).
  - The lanes are idle for the whole filing run. The seed branch `z2-board-seed` is cut from master after the docs and config commit; the session filer runs through the normal zagent, committing on the seed branch; the result is verified (`zagent doctor` ok, the session coverage checker clean, `git diff master...z2-board-seed --name-only` touching only `Zenith2/Docs/Board/**`); then `git switch master`, `git merge --squash z2-board-seed`, and one commit `board: seed Z2 backlog` with a `Vault-Operation: z2-seed-<date>` trailer. The seed branch is deleted and the lanes resume.
  - The content is board-only, so lane `integrate`'s board-only classification is honest.
  - Master never holds a half-filed board.
- **Alternatives considered:** (1) File directly on master — rejected: thousands of commits on master and a half-filed board if filing breaks. (2) File in a worktree — rejected: redirected to the main checkout (Z2-D-087). (3) Write board files by hand — rejected: bypasses the writer's validation.
- **Consequences:** The procedure and recovery rules are Z2-D-087, Z2-D-095, Z2-D-096, Z2-D-101, Z2-D-110 and Z2-D-113. Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** BRANCH_MOVED, MAIN_NOT_ON_BASE, filing, seed branch, z2-board-seed, main checkout, squash merge, board: seed Z2 backlog, Vault-Operation trailer, lanes idle, board-only commit, session filer
- **Related:** Z2-D-087, Z2-D-096, Z2-D-113, Z2-D-115
### Z2-D-070 Every stage is drafted in full detail, including Stages 3 to 7; revalidation tickets absorb the rewrite cost
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Tickets for distant stages are usually left as stubs, but a stub is not claimable and hides scope. The human ruled that every stage is drafted in full, accepting that Stage 1 and 2 decisions will force rewrites.
- **Decision:**
  - Every stage is drafted in full detail — exact files, signatures and tests — including Stages 3 to 7 (the human's ruling).
  - Accepted consequence: Stage 1–2 decisions will trigger revalidation rewrites of many subsequent tickets. That cost is borne by the spec-revalidation tickets and the filer's reconcile mode.
  - Every requirement is decomposed into tickets for Stages 0 to 7: PROC infrastructure, scaffold, implementation, tests, integration, decision, revalidation and acceptance tickets. Deferred capabilities get detailed leaf tickets in their stage's sprint.
- **Alternatives considered:** (1) Stub tickets for Stages 3–7 — rejected by the human's ruling: stubs hide scope and cannot be covered. (2) Draft each stage only when it starts — rejected: the whole backlog is filed before the scaffold (Z2-D-033).
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Stage overview"`.
- **Nouns:** Test_FilerReconcile, full-detail drafting, File-Backlog.ps1, Spec-Maturity, needs-revalidation, Stages 3 to 7, revalidation rewrite, reconcile mode, spec-revalidation ticket, ticket draft
- **Related:** Z2-D-033, Z2-D-125, Z2-D-129
### Z2-D-071 Scope: consoles deferred to Stage 7, gated on SDK access
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Console SDKs are under NDA and unavailable today, but a scope left implicit is a scope that drifts. The programme records every boundary explicitly and gives deferred scope a full requirement tree now.
- **Decision:**
  - Consoles are deferred to Stage 7, gated on SDK access.
  - Deferred scopes get full requirement trees now. Console tickets are specified to the level possible without an NDA, and each console platform's tickets are blocked by a `needs-human` SDK-access ticket for that platform.
  - The architecture keeps the platform layer, RHI and packaging behind neutral APIs, so a console backend is an additional backend target rather than a restructuring.
- **Alternatives considered:** (1) Consoles in scope now — rejected: no SDK access. (2) Consoles excluded — rejected: AAA general-purpose scope keeps them as a deferred target.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Stage 7 Later expansion"` and `Zenith2/Docs/Architecture.md → "Platform layer"`; the inventory rows are under SCOPE in `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"`.
- **Nouns:** Zenith_Platform, Zenith_RHIBackendEntry, consoles, console SDK, SDK-access ticket, needs-human, NDA, deferred scope, SCOPE, Stage 7
- **Related:** Z2-D-010, Z2-D-072
### Z2-D-072 Scope: VR/XR deferred to Stage 7 via OpenXR; multi-view and input kept XR-ready
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** XR retrofitted into a renderer and input system that assumed one camera and one pointer is expensive. The design decisions that make XR cheap are small if taken now.
- **Decision:**
  - VR/XR is deferred to Stage 7, through OpenXR. XR's tickets are specified against OpenXR.
  - Multi-view and input-action design are kept XR-ready now: render views carry `VIEW_KIND_STEREO` (left and right eyes sharing culling, multi-view rendering), and the input system is action-based with input contexts rather than device-bound.
- **Alternatives considered:** (1) XR in scope now — rejected: no flagship slice needs it before shipping qualification. (2) XR excluded without XR-ready design — rejected: a retrofit would restructure views and input.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Render views"` and `Zenith2/Docs/Architecture.md → "Platform layer"`.
- **Nouns:** VR, XR, OpenXR, VIEW_KIND_STEREO, multi-view, stereo rendering, input action, input context, XR-ready, SCOPE
- **Related:** Z2-D-059, Z2-D-071
### Z2-D-073 Scope: networked multiplayer and dedicated servers deferred to Stage 7 with Stage 1 foundations and a Stage 3 loopback proof
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Networking bolted onto a single-player engine fails on identity, authority and determinism. The foundations must exist from the architecture stage even though full networking ships last.
- **Decision:**
  - Networked multiplayer and dedicated servers are deferred to Stage 7, with a full requirement tree ticketed now: transport/session, replication, prediction/reconciliation, interest management, serialization, dedicated server builds, network simulation, diagnostics.
  - Foundations, Stage 1 architecture: stable network IDs, distinct from runtime handles and persistent IDs; an authority model, server-authoritative by default, with each component declaring its authority; fixed simulation ticks; gameplay input as replayable commands, shared with input recording (SAVE); headless execution, a runtime configuration without Flux, Audio or Platform windowing.
  - Proof, Stage 3: a two-process loopback that replicates transforms and commands.
  - `ZenithNet` holds the foundations (IDs, ticks, commands).
- **Alternatives considered:** (1) Networking in scope for shipping — rejected: single-player production comes first. (2) No foundations until Stage 7 — rejected: IDs, authority and ticks retrofitted would restructure ECS and scripting.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Multiplayer foundations"`.
- **Nouns:** networked multiplayer, dedicated server, ZenithNet, network ID, authority model, server-authoritative, fixed tick, replayable command, headless execution, loopback proof, replication, prediction, interest management, NET
- **Related:** Z2-D-019, Z2-D-076, Z2-D-080
### Z2-D-074 Scope: Steam and Google Play online-service backends in Stage 6; the adapter API with an offline implementation is required in Stage 4
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Online services (identity, entitlements, achievements, cloud saves) shape save and account flows long before a store backend exists. An adapter with an offline implementation lets those flows be built and tested without a store.
- **Decision:**
  - The `Zenith_OnlineServices` adapter API plus an offline implementation is required in Stage 4: identity, entitlements, achievements, a cloud-save conflict model; account changes mid-session (sign-out, switch user: saves and entitlements rebind, with no cross-account leakage); cloud-save conflict resolution flows.
  - The Steam and Google Play backends are delivered in Stage 6.
  - `ZenithOnline` holds the adapter API and the offline implementation.
- **Alternatives considered:** (1) Integrate a store SDK directly — rejected: couples gameplay to one store and blocks testing without it. (2) No online services — rejected: account and cloud-save flows are part of shipping qualification.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` and `Zenith2/Docs/Roadmap.md → "Stage 4 Production content and worlds"`.
- **Nouns:** Zenith_OnlineServices, ZenithOnline, online-services adapter, offline implementation, identity, entitlements, achievements, cloud save, cloud-save conflict, account change, Steam backend, Google Play backend, ONLINE
- **Related:** Z2-D-024
### Z2-D-075 Scope: modding excluded
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Third-party modding needs runtime code loading, a stable binary or script API across versions, and a trust boundary for untrusted content — each contradicting settled choices (static modules, in-repo games, per-version recooks).
- **Decision:** Modding is excluded. There is no third-party mod loading. It has no tickets; its inventory items carry this ID.
- **Alternatives considered:** (1) Script-only modding through Luau — rejected: needs a stable script API across releases and an untrusted-content boundary the sandbox does not claim. (2) Modding deferred to Stage 7 — rejected: the static-module and in-repo model (Z2-D-002, Z2-D-024) would have to be reversed.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Engine modules and game projects"` and `Zenith2/Docs/Architecture.md → "Security architecture"`.
- **Nouns:** modding, mod loading, third-party mods, game.json, Tools/NewGame, excluded scope, SCOPE, MOD
- **Related:** Z2-D-002, Z2-D-024
### Z2-D-076 Scope: local multiplayer and split-screen excluded
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Split-screen multiplies per-player views, input and UI across every system. Leaving the question open would force every render-view and input design to carry per-player cost.
- **Decision:**
  - Local multiplayer and split-screen are excluded. They have no tickets.
  - Render views are never per-player views.
- **Alternatives considered:** (1) Split-screen in scope — rejected: per-player views, input contexts and UI in every system for no flagship slice. (2) Deferred — rejected: no planned slice or stage needs it.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Render views"` and `Zenith2/Docs/Architecture.md → "Multiplayer foundations"`.
- **Nouns:** local multiplayer, split-screen, per-player view, Flux_View, VIEW_KIND_MAIN, excluded scope, SCOPE, NET
- **Related:** Z2-D-059, Z2-D-073
### Z2-D-077 Scope: quest, dialogue, inventory and interaction are Luau gameplay modules inside OpenWorldSlice, never engine features
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Engine-level quest, dialogue and inventory systems hard-code one game's rules into a general-purpose engine. The engine's job is the primitives those systems are built from.
- **Decision:**
  - Quest, dialogue, inventory and interaction systems are game-side only: Luau gameplay modules inside `OpenWorldSlice`, owned by that slice, never in the engine.
  - The engine provides the reusable primitives they use (tags and events, state and ability primitives, timers, attributes and effects, script integration, data-defined components), and samples that do not hard-code one game's rules.
- **Alternatives considered:** (1) Engine quest/dialogue/inventory systems — rejected: they hard-code one game's rules. (2) No samples at all — rejected: the primitives need a real consumer to prove them.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Engine modules and game projects"`; the primitives are the GAME catalogue, `Zenith2/Docs/Requirements/GAME.md`.
- **Nouns:** quest, dialogue, inventory, interaction, Luau gameplay module, OpenWorldSlice, game-side, gameplay sample, GAME
- **Related:** Z2-D-016, Z2-D-021
### Z2-D-078 Scope: 2D is first class; hair/groom excluded
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A general-purpose engine that treats 2D as an afterthought forces 2D games through 3D machinery. Hair/groom is a large specialist programme that no flagship slice needs.
- **Decision:**
  - 2D is first class: sprites and batching through the render graph, atlases, tilemaps, 2D animation, 2D physics (D-SEL-2DPHYS, open, Stage 2), pixel-perfect cameras and rendering, 2D authoring in the editor, and the UI2D sample, proven by `Game2D`. `Zenith2D` is its module.
  - Hair/groom is excluded; hair alternatives (cards/shells) are classified under CHAR.
  - Amended 2026-10-09 (CHAR's classification of hair alternatives, Z2-I-CHAR-10, CHAR ruling CH-R8; a refinement of this decision, not a new decision): hair cards are REQUIRED as an authoring convention over existing features (skinned card meshes on the head's skeleton, GEOM's `alphaTest` domain with `twoSided`, the anisotropy surface model from Stage 5, CHAR's hair-card rules HC1–HC5); hair shells (fur shells) are EXCLUDED: each shell is a further alpha-tested layer over the same pixels, multiplying the overdraw the early-Z rules keep minimal and the S24 pays in fragment bandwidth, and no flagship slice needs fur; strand hair and groom simulation stay excluded. Simulated card motion is revisited only through D-CLASS-SIM (open, Stage 2). The classification is `Zenith2/Docs/Design/CHAR/Modular.md` → "Hair alternatives classification" and "Hair card authoring rules" (requirements Z2-R-CHAR-085, Z2-R-CHAR-086); the human's A4 sign-off of scope cuts confirms it.
- **Alternatives considered:** (1) 2D as a 3D mode — rejected: pixel-perfect rendering and 2D authoring need their own path. (2) Hair/groom in scope — rejected: a specialist programme with no consuming slice.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Stage 3 Playable authoring slice"`; the catalogues are `Zenith2/Docs/Requirements/TWOD.md` and `Zenith2/Docs/Requirements/CHAR.md`.
- **Nouns:** 2D, Zenith2D, sprite, sprite batching, atlas, tilemap, 2D animation, 2D physics, pixel-perfect camera, 2D authoring, UI2D sample, Game2D, hair, groom, hair cards, shells, TWOD, fur shells, hair-card rules, CH-R8
- **Related:** Z2-D-021, Z2-D-043
### Z2-D-079 Materials and VFX are authored in code with previews; animation, terrain, UI and cinematics get visual authoring
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Node-graph material and VFX editors are large tools whose output is harder to review and diff than code. Animation, terrain, UI and cinematics, by contrast, are spatial or temporal and need visual tools.
- **Decision:**
  - Materials are Slang surface functions plus a material `.json` of parameters. There is no node graph.
  - VFX are authored in code, with previews and inspectors for code-authored effects.
  - Animation, terrain, UI and cinematics get visual authoring in the editor.
- **Alternatives considered:** (1) Node-graph material and VFX editors — rejected: large tools, and graphs review and diff poorly. (2) Code-only authoring for everything — rejected: spatial and temporal content needs visual tools.
- **Consequences:** Specified in `Zenith2/Docs/Rendering.md → "Materials"`; the VFX and editor designs are `Zenith2/Docs/Design/VFX.md` and `Zenith2/Docs/Design/EDIT.md`.
- **Nouns:** material authoring, Slang surface function, material .json, EvaluateMaterial, Flux_MaterialType, Flux_MaterialInput, node graph, VFX authoring, code-authored effect, effect preview, inspector, visual authoring, animation editor, terrain editor, UI editor, cinematic editor
- **Related:** Z2-D-015, Z2-D-035
### Z2-D-080 Frame phases and authority: twelve fixed phases, per-field write authority, commands at sync points, default arbitration precedence (D-PHASES as pre-decided in the plan; its Stage 1 ticket refines and confirms)
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Hazard analysis orders two writers, but it does not decide which one should own an outcome — root motion versus physics, a cinematic versus gameplay. Without declared ownership, the order that happens to compile becomes the design.
- **Decision:**
  - Each tick runs twelve fixed phases, parallel within a phase by access, with ownership declared explicitly (phase — owns — publishes at its end): 1 Input — action state, per-player input contexts — input commands (replay-logged); 2 Early scripts — gameplay intent (desired velocity, requested actions) — command and event buffers, merged deterministically; 3 Animation — poses, root-motion extraction (not application) — root-motion deltas; 4 Movement arbitration — the character-controller move combining root motion, script intent and constraints under the arbitration rules — kinematic targets; 5 Physics — dynamic-body transforms, contacts and triggers — collision and trigger events; 6 Post-physics — ragdoll ↔ animation blending, physical-animation sync — final poses; 7 AI and navigation — agent decisions and path requests (async results at tick boundaries) — AI commands; 8 Transform propagation — world transforms from local transforms and hierarchy — transform change bits; 9 Events — delivery of the merged event queue; 10 Late scripts — reactions to events and final gameplay state — command buffers (applied at the next sync point); 11 Save-snapshot point — the consistent world snapshot for saves and replays; 12 Render extraction — the immutable `Flux_RenderWorld` — the hand-off to rendering (pipelined).
  - Behaviours declare Early scripts (phase 2) or Late scripts (phase 10); chunk systems declare a phase too. A sync point sits between every pair of phases, where commands are applied.
  - One granularity: read/write sets and authority are both checked per field when the schedule compiles; declaring a whole component expands to all its fields.
  - Authority: every component field declares the phase that owns writes to it. A write outside the owning phase goes through a command, applied at the next sync point. Two direct writers of one field in the same phase are a schedule-compile error unless the component declares an arbitration rule. Default precedence: cinematic > ragdoll > physics > animation > script; arbitration is itself a deterministic merge step.
  - D-PHASES (open, Stage 1) is owned by GAME, with JOBS and ECS as dependencies.
  - Lifecycle commands, each with a defined publication point and tests:
    - Spawn: created at a sync point, and visible to the next phase.
    - Destroy: deferred to the end of the tick; handles become invalid next tick.
    - Teleport: resets physics velocity, interpolation, animation root, and the render motion-vector history, so there is no velocity smear.
    - Ragdoll enter and recover: blends ownership from animation to physics and back.
    - Cinematic start, interrupt and skip: takes arbitration precedence, and on end or skip restores gameplay state from a pre-cinematic snapshot of owned fields.
    - World transition: a new world is loaded in isolation, and the swap happens at a tick boundary.
    - Shutdown: drain the tick, flush the commands, then tear down in reverse phase order.
  - Scenario tests: teleport during a ragdoll; skipping a cinematic mid-blend; a spawn and a destroy in the same tick; a world transition with in-flight async results; saving at the snapshot point during streaming, with animation and physics active.
  - This is D-PHASES (open, Stage 1) as pre-decided; its Stage 1 ticket refines and confirms it.
- **Alternatives considered:** (1) Hazard ordering alone — rejected: decides order, not ownership. (2) UE-style tick groups without per-field authority — rejected: conflicts surface at runtime, not at schedule compile. (3) Per-component rather than per-field authority — rejected: too coarse for transforms shared between animation, physics and scripts.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Frame phases and authority"` and `Zenith2/Docs/Architecture.md → "Lifecycle commands"`.
- **Nouns:** Zenith_Phase, Zenith_SyncPointToken, Zenith_CommandBuffer, Zenith_LifecycleListener, D-PHASES, frame phase, Input phase, Early scripts, Animation phase, Movement arbitration, Physics phase, Post-physics, AI and navigation phase, Transform propagation, Events phase, Late scripts, Save-snapshot point, Render extraction, sync point, per-field authority, write authority, command, arbitration rule, arbitration precedence, schedule-compile error, lifecycle command, Spawn, Destroy, Teleport, ragdoll enter, ragdoll recover, cinematic skip, world transition, Shutdown
- **Related:** Z2-D-006, Z2-D-050, Z2-D-051, Z2-D-073
### Z2-D-081 Every game is one executable: the shared Player main statically linked with the engine and the selected modules
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A per-game main function is C++ in the game, which games do not contain (Z2-D-002). One shared main also gives one composition root to audit.
- **Decision:**
  - Every game produces one executable: the shared `Player/` main, statically linked with the engine and the modules the game selected in `game.json`.
  - `Player/` holds the shared main and the Android Gradle template; on Android the executable is packaged as a `.so` with the game's cooked content.
  - The composition root in the Player wires every subsystem explicitly, installs the root-allocator and profiler hooks in `main`, and registers each selected module.
  <!-- elaboration -->
  - The only per-game translation unit is the composition table `zenith_add_game` generates from `game.json`, `Zenith_GameComposition.generated.cpp`: constant data and one-line thunks that bind module entry points to the selected backends, with no `main` and no logic beyond those thunks. It is data derived from `game.json`, not game C++.
- **Alternatives considered:** (1) A per-game `main` function, hand-written or generated — rejected: the entry point, the composition wiring and the hook installation would be duplicated per game with nothing to vary; what does vary (modules and backends) is carried as constant data by the generated composition table, which holds no `main`. (2) One executable loading games as data packages — rejected: every module would be linked into every game.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Composition root and Player"`.
- **Nouns:** Player, shared Player main, Zenith_GameComposition, Zenith_GameComposition.generated.cpp, zenith_add_game, composition root, executable per game, Gradle template, module registration, root-allocator hook, profiler hook
- **Related:** Z2-D-002, Z2-D-024, Z2-D-046
### Z2-D-082 Third-party dependencies pinned in ThirdParty/manifest.json, integrated not rebuilt, confined to Internal/
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Unpinned middleware changes evidence silently, and middleware headers in public headers couple every consumer to the vendor. Rebuilding a vendor's own build system inside the engine's multiplies maintenance.
- **Decision:**
  - All third-party dependencies are pinned in `ThirdParty/manifest.json`, integrated rather than rebuilt, and confined to `Internal/`. Third-party headers (EnTT, flecs, Jolt, PhysX, zlib, zstd, Vulkan, D3D12) never reach a public header; libgraph checks this.
  - Selected: SDL3 (window, input, Android lifecycle, filesystem); volk, Vulkan-Profiles, VMA (Vulkan loading, baseline profile, memory); the D3D12 Agility SDK and D3D12MA (D3D12 runtime and memory); Slang and DXC (shaders, SPIR-V/DXIL, offline tool only, never linked into a runtime target); Jolt and PhysX 5 (physics); zlib and zstd (compression); Luau (scripting); flecs and EnTT (ECS); miniaudio (audio device, mixing and decode base); ImGui docking (editor UI); fastgltf and meshoptimizer (glTF import, mesh processing and meshlets); KTX-Software with basisu (texture compression, BC7/ASTC); Tracy (profiling); doctest and nanobench (tests, benchmarks); WARP (NuGet) and lavapipe (headless rendering in CI).
  - Amended 2026-10-09 (narrowing of a selected role by the accepted flagged proposal AUDIO-1, not a new decision): miniaudio is the audio output device layer only (WASAPI, AAudio, null), plus format conversion when the endpoint is not 48 kHz; its decoding, node graph, engine and resource manager are compiled out, and ZenithAudio owns the mixer, DSP and decode (`Zenith2/Docs/Design/AUDIO.md` → "ZENITH_SUPPORTED matrix and miniaudio configuration"; `Zenith2/Docs/Architecture.md` → "Third-party dependencies").
  - Selections still open are D-SEL-NAV (open, Stage 1/2), D-SEL-TEXT (open, Stage 1/2), D-SEL-UIRT (open, Stage 1/2), D-SEL-HEAP (open, Stage 1), D-SEL-JSON (open, Stage 1), D-SEL-HASH (open, Stage 1), D-SEL-CODEC (open, Stage 2), D-SEL-VIDEO (open, Stage 2), D-SEL-CRASH (open, Stage 2), D-SEL-2DPHYS (open, Stage 2), D-SEL-MOTIONMATCH (open, Stage 3) and D-SEL-FBX (open, Stage 4); each lists candidates, criteria and an acceptance rule, and blocks its consumers.
  - In the sanitizer presets every C/C++ dependency is built from source from the pinned sources (Z2-D-011).
- **Alternatives considered:** (1) System-installed or package-manager dependencies — rejected: unpinned, and not buildable with sanitizer flags. (2) Vendoring with local modifications — rejected: upgrades become merges; dependencies are integrated unmodified.
- **Consequences:** Specified in `Zenith2/Docs/Architecture.md → "Third-party dependencies"` and `Zenith2/Docs/Architecture.md → "Named selection decisions"`.
- **Nouns:** ThirdParty/manifest.json, third-party dependency, pinned dependency, Internal/, SDL3, volk, Vulkan-Profiles, VMA, D3D12 Agility SDK, D3D12MA, Slang, DXC, Jolt, PhysX 5, zlib, zstd, Luau, flecs, EnTT, miniaudio, ImGui, fastgltf, meshoptimizer, KTX-Software, basisu, Tracy, doctest, nanobench, WARP, lavapipe
- **Related:** Z2-D-008, Z2-D-011, Z2-D-042
### Z2-D-083 Stage acceptance suites run outside the loop through Run-Committed.ps1 -Acceptance while every lane is idle
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Stage acceptance runs the full suites, including sanitizers and device work, far beyond the 30-minute phase cap, and needs the whole machine. `/tick` refuses `needs-human` tickets even by name.
- **Decision:**
  - The `Zenith2Acceptance` category has no paths; it is selected explicitly by stage acceptance tickets only, and exists only to hold the full acceptance-suite lines (`-Stage N`) with a longer `phaseTimeoutMinutes`, exempt from the 30-minute cap and excluded from CI's union.
  - Acceptance tickets are `needs-human`, so the human runs the lines directly, through `Run-Committed.ps1 -Acceptance -Stage N`, in the main checkout, while every lane is idle (lanes are simply not fired; there is no pause mechanism). The run holds the machine-wide slot through `build_lock.ps1 -Run`.
  - `Zenith2Acceptance`'s timeout is read by `Run-Committed.ps1 -Acceptance`, not by `tick_gates.ps1`. Stage acceptance tickets also run the full sanitizer set, and `Coverage.ps1` in acceptance mode (`-Stage N`).
- **Alternatives considered:** (1) Acceptance as ordinary tick gates — rejected: exceeds the phase cap and competes with lanes for the machine. (2) Acceptance in CI only — rejected: device, sanitizer and GPU evidence needs the local hardware.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Acceptance evidence"`.
- **Nouns:** Zenith2Acceptance, stage acceptance, acceptance suite, Run-Committed.ps1 -Acceptance, -Stage N, build_lock.ps1 -Run, machine-wide slot, lanes idle, acceptance ticket, acceptance mode
- **Related:** Z2-D-028, Z2-D-044, Z2-D-127
### Z2-D-084 Decisions needing measurement are split into an ordinary harness ticket plus a needs-human ruling ticket
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Measurement on reference hardware is human work, but building the harness is ordinary engineering the loop can do. A single `needs-human` ticket would leave the harness unbuilt by the loop; a single loop ticket could not run on the S24.
- **Decision:**
  - A decision that needs measurement is split in two. An ordinary harness ticket, which the loop runs, builds the measurement harness under `Zenith2/Benchmarks/` (never under the protected gate directory), plus a short CTest smoke run proving it works inside the normal Test step. A `needs-human` ruling ticket, blocked by the harness ticket: the human runs the full measurement on the reference hardware (the RTX 3070 desktop and the S24), commits the report under `Zenith2/Docs/Research/`, and appends the ruling to `Zenith2/Docs/DecisionLog.md` in the ruling ticket's diff with the next free ID (Z2-D-097).
  - Dependents are blocked by the ruling ticket.
  - Measurements that need reference hardware are human work anyway, so this costs nothing in loop throughput.
- **Alternatives considered:** (1) One `needs-human` ticket for harness and measurement — rejected: the loop could not build the harness. (2) `human-gate` on a loop ticket — rejected: the loop parks it at In Review, but the measurement still needs a human to run it (Z2-D-126).
- **Consequences:** Every open decision in `## Open decisions` states its split. Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** harness ticket, ruling ticket, needs-human, measurement harness, Zenith2/Benchmarks, CTest smoke run, reference hardware, RTX 3070, S24, Docs/Research, DecisionLog.md
- **Related:** Z2-D-064, Z2-D-085, Z2-D-097, Z2-D-126
### Z2-D-085 Living docs are edited in the same diff as the contract change; rulings are appended to DecisionLog.md in the ticket's own diff
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Docs edited after the code drift immediately, and workers read only the docs, so a stale doc is a wrong specification. A ruling kept anywhere but the decision log never reaches a worker, because the rulings scan reads only the category's `decisionLog` file.
- **Decision:**
  - A ticket that changes a contract, API or design edits its `Zenith2/Docs/Design/<CODE>.md` section in the same diff; its DoD says so.
  - The docs are unprotected, apart from `Zenith2/CLAUDE.md`.
  - Rulings are appended to `Zenith2/Docs/DecisionLog.md` in the diff of the ticket that makes them, each under the next free `Z2-D-NNN` (Z2-D-097). `zagent decide` writes only a `metadata.md` knowledge row; it is at most an optional mirror carrying that explicit ID, never the record.
  - The coverage gate step reports stale cites without failing, so a living-docs ticket that edits its own cited section passes; `--mark-stale` then flags dependent tickets for revalidation.
- **Alternatives considered:** (1) Docs updated by separate documentation tickets — rejected: drift between the tickets. (2) Record rulings only through `zagent decide` — rejected: it writes `metadata.md`, which the rulings scan never reads (Z2-D-097).
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"` and the heading "How this log is written" in this file.
- **Nouns:** living docs, same-diff doc update, design doc, Docs/Design, DecisionLog.md, next free ID, metadata.md, zagent decide, stale cite, mark-stale
- **Related:** Z2-D-022, Z2-D-068, Z2-D-084, Z2-D-097
### Z2-D-086 The Z2 categories carry identical settings except paths, gates and phaseTimeoutMinutes; the filer categorises everything Zenith2 except acceptance tickets
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `resolveRules` resolves conventions from the ticket's own category, while the path union adds only gates. If the categories differed in any other setting, a ticket's conventions would depend on which category it happened to be filed under.
- **Decision:**
  - Every Z2 category carries identical settings: `project: Z2`; `buildSystem: "external"` and `measureSteps`; the same `conventions`; `suppressLanguageConventions: true`; `decisionLog: Zenith2/Docs/DecisionLog.md`; `decisionIdPrefix: Z2-D-`; `branching: direct`. Only `paths`, `gates` and `phaseTimeoutMinutes` differ.
  - The filer categorises every ticket as `Zenith2`, except stage acceptance tickets, which are `Zenith2Acceptance`. The Render, ECS and Memory categories exist to add gates through the union, and are safe even if one is selected by hand.
  - `Zenith2/Tools/Gates/Tests` checks that the Z2 categories agree on every setting except `paths`/`gates`, and resolves a sample ticket's rules under each category directly.
  - The Z2 `conventions` string names `Zenith2/CLAUDE.md` and `Zenith2/Docs/WorkerGuide.md` explicitly.
- **Alternatives considered:** (1) Per-category conventions — rejected: conventions would vary with filing category. (2) Filing Render/ECS/Memory work under those categories — rejected: the path union already adds their gates to any ticket touching their paths.
- **Consequences:** Specified in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Zenith2Render, Zenith2Memory, Zenith2Acceptance, Z2 category settings, resolveRules, buildSystem external, measureSteps, conventions, suppressLanguageConventions, decisionLog, decisionIdPrefix, branching direct, paths, gates, phaseTimeoutMinutes, category parity test
- **Related:** Z2-D-032, Z2-D-044, Z2-D-117

## Open decisions

Each entry below is ruled by appending a `Z2-D-NNN` entry to this file, with the next free ID, in the diff of its ruling ticket (Z2-D-097), and marking the entry here `Ruled as Z2-D-NNN`; until then it is cited by name with its stage, for example "D-SLANG (open, Stage 1)". Every dependent ticket is `blocks`-linked from the ruling ticket and carries `Spec-Maturity: decision-pending`, and a spec-revalidation ticket scoped to (area, stage, decision set) restores `ready` after the ruling. A revalidation ticket for stage N depends only on decisions from stages ≤ N. An acceptance rule or alternative list preceded by an elaboration marker details the decision beyond its first statement; the ruling ticket confirms or amends it.

Stage 0 PROC notes for the rulings scan (generic Vault changes, not decisions; each would be filed as a `needs-human` PROC ticket):
<!-- elaboration -->
- A direct decision-ID citation is self-sufficient: weight a direct `Z2-D-NNN` citation at `RULINGS_FILE_MIN_SCORE`, so a citation alone selects its entry. The code comment at `ticket.mjs:344-350` already calls a citation "largely self-sufficient" while its weight is half the threshold. The same PROC sorts cited IDs above uncited candidates (or raises the 25-entry limit), because ties sort by ascending ID and a multi-citation ticket saturates the list.
<!-- elaboration -->
- An array-valued `decisionLog` in `resolveRules`, `renderRulingsMarkdown` and `decisionLogTextOf`, needed only if this log is ever to be split across files.


### D-SLANG
- **Status:** Open · **Stage:** 1
- **Question:** Which Slang version is pinned, and which Vulkan lowering does `DescriptorHandle<T>` use? The documented default lowering needs `VK_EXT_mutable_descriptor_type`, which is not in the Vulkan 1.3 baseline profile.
- **Alternatives:** (1) Add mutable descriptors to the Zenith2 profile. (2) A custom per-kind typed-array lowering. (3) `uint` indices plus generated accessors.
- **Acceptance rule:** Non-uniform indexing, descriptor limits (within `Z2-B-RHI-009`, bindless heap capacity per kind), generated layouts and the two-word handle read of `Test_BindlessProbe_HandleWordsRoundTrip` all pass on an RTX GPU, the S24 (Adreno 750), WARP and lavapipe. The S24 run uses a Stage 1 `android-arm64` bindless probe, which is why the Stage 0 scaffold includes the `android-arm64` preset and a Player stub.
- **Consumers blocked:** RHI (bindless heaps, descriptor allocation), RPROD (shader system, generated headers), GEOM (compute codecs through bindless storage views), LIGHT, VFX, TWOD, ADVGFX (the virtual-texture handle words read through `Flux_HandleWords`) and every Flux pass whose parameter struct carries a resource field.
- **Harness and ruling split:** An ordinary probe ticket builds the bindless probe and its CTest smoke run on Windows (RTX, WARP, lavapipe); a `needs-human` ruling ticket runs the probe on the S24 as a device step, commits the report under `Zenith2/Docs/Research/`, and appends the ruling to this file.
- **Nouns:** D-SLANG, Slang version, DescriptorHandle, VK_EXT_mutable_descriptor_type, typed-array lowering, uint index, generated accessor, bindless probe, non-uniform indexing, descriptor limit
- **Related:** Z2-D-013, Z2-D-015, Z2-D-054, Z2-D-084
### D-SEL-HEAP
- **Status:** Open · **Stage:** 1
- **Question:** What backs the general heap `Zenith_Heap`: the system allocator or mimalloc? Sanitizer builds always use the system allocator.
- **Alternatives:** (1) The system allocator (`malloc`, with size tracking through the platform's usable-size query). (2) mimalloc (with `mi_usable_size`).
<!-- elaboration -->
- **Acceptance rule:** mimalloc is adopted only if, on the RTX 3070 desktop and the S24, it improves allocation throughput or p95 frame time in the W-Dense and W-Characters workloads beyond the run-to-run variance (95% confidence intervals, at least 10 repetitions), without raising S24 process RSS against `Z2-B-CORE-001` (S24 process RSS) or the general-heap residual `Z2-B-CORE-004` (general heap unattributed residual); otherwise the system allocator wins, as the established alternative with no maintenance cost. The decision record states the measured benefit against the maintenance cost, as every custom infrastructure record does (Z2-D-041).
- **Consumers blocked:** CORE (the `Zenith_Heap` implementation, the debug leak table, OOM handling) and the CORE memory budget rows.
- **Harness and ruling split:** An ordinary harness ticket builds an allocation benchmark under `Zenith2/Benchmarks/` (mixed alloc/free sizes, cross-thread frees, a spawn/destroy churn replay) with a CTest smoke run; a `needs-human` ruling ticket measures on the reference hardware and records the ruling.
- **Nouns:** D-SEL-HEAP, Zenith_Heap, general heap, system allocator, malloc, mimalloc, usable size, allocation benchmark
- **Related:** Z2-D-040, Z2-D-041, Z2-D-084
### D-SEL-PHYSX-ANDROID
- **Status:** Open · **Stage:** 1
- **Question:** Can PhysX 5 build and pass the physics conformance suite on arm64 Android?
- **Alternatives:** (1) PhysX runs on Android, so both backends are available there. (2) It cannot, and the capability matrix makes Jolt the Android backend.
- **Acceptance rule:** PhysX is an Android backend only if it builds under `android-arm64` (including the 16 KB page rules) and passes the shared physics conformance suite, including the per-backend determinism tests, on the S24. Otherwise Jolt is the Android backend and `game.json` refuses PhysX for Android.
- **Consumers blocked:** PHYS (the PhysX backend's Android targets), MOD (`game.json` backend validation per platform), the capability matrix.
- **Harness and ruling split:** A harness ticket (an Android PhysX build plus the conformance-suite runner) and a `needs-human` ruling ticket that runs it on the S24.
- **Nouns:** D-SEL-PHYSX-ANDROID, PhysX 5, Android arm64, physics conformance suite, capability matrix, Jolt Android backend
- **Related:** Z2-D-003, Z2-D-010, Z2-D-084
### D-JOLT-PARALLEL-STEP
- **Status:** Open · **Stage:** 1
- **Question:** From flagged proposal PHYS-1, which the orchestrator's ruling Q87 decides on Stage 1 evidence: does the Jolt backend keep the single-threaded `JPH::JobSystemSingleThreaded` step, or admit the narrow help-while-wait exemption to Z2-D-006 pre-specified in `Zenith2/Docs/Design/PHYS/Backends.md` → "Conditional Jolt parallel step"? The single-threaded step is the compliant baseline and ships unless the exemption is admitted.
- **Alternatives:** (1) Rejected: the single-threaded Jolt step stays the step on every platform, and `PHYSICS_CAPABILITY_PARALLEL_STEP` stays absent from Jolt. (2) Admitted: `Zenith_JoltJobSystem`, internal to `ZenithPhysics_Jolt` only; its barrier wait holds no lock across a check and calls no blocking primitive, after the barrier wait returns it reports the measured wait once through `Zenith_DispatchBlockingWait` as `physics-barrier`, and stress and determinism tests run at worker counts 1, 2, 8 and all.
- **Acceptance rule:** Rejected when both the W-Dense and the W-OpenWorld single-threaded Jolt p95 step times on the S24 are within the Stage 1 value of `Z2-B-PHYS-002` (physics step CPU); admitted otherwise. The ruling records the single-threaded and thread-pool figures either way. When admitted, the Contracts bullet beside [Z2-N-CONTRACT-365] and the lock rows 520 `uLOCK_LEVEL_PHYSICS_JOLT_JOBS` and 530 `uLOCK_LEVEL_PHYSICS_CONTACT_SINK` (cross-doc item X264 (m)) are applied by the route the orchestrator fixes.
- **Consumers blocked:** PHYS (the conditional `Zenith_JoltJobSystem` ticket Z2T-PHYS-027), and JOBS (the `Test_JobSystem_NoBlockingWaitInJobs` exclusion, only if the exemption is admitted).
- **Harness and ruling split:** The thread-pool rows of `Zenith_Bench_PhysicsBackends` (the harness ticket Z2T-PHYS-002) are the measurement; the `needs-human` ruling ticket Z2T-PHYS-005 rules this decision and D-SEL-PHYSX-ANDROID from the same run and appends the ruling to this file.
- **Nouns:** D-JOLT-PARALLEL-STEP, PHYS-1, Zenith_JoltJobSystem, JPH::JobSystemThreadPool, JPH::JobSystemSingleThreaded, physics-barrier, single-threaded baseline
- **Related:** Z2-D-003, Z2-D-006, Z2-D-084
### D-LUAU-STATELESS
- **Status:** Open · **Stage:** 1
- **Question:** How is the rule that chunk-system modules hold no state enforced? `luaL_sandbox` and frozen tables do not establish it: a closure can capture and mutate a local. The same mechanism is extended to check that behaviours share no mutable tables.
- **Alternatives:** (1) A compile-time validation pass over the Luau AST that rejects upvalue writes, and any mutation reachable from module scope, in system modules. (2) Isolated per-invocation environments, rebuilt every call. (3) A restricted scripting subset for systems. (4) A combination.
- **Acceptance rule:** The mechanism covers captured locals, nested tables, imported modules, mutable userdata and stateful library functions (`math.random`, `os.clock` and similar are removed from the system environment). A corpus of adversarial modules is each rejected at load time, not merely caught by a test.
- **Consumers blocked:** SCRIPT (chunk systems, per-worker VMs, behaviour state sharing), ECS (chunk-system column access), GAME (script integration).
<!-- elaboration -->
- **Harness and ruling split:** An ordinary prototype ticket builds the candidate mechanism and the adversarial corpus as CTest cases under `Zenith2/Engine/ZenithScript/Tests/`; a `needs-human` ruling ticket appends the chosen mechanism to this file as its ruling. No reference-hardware measurement is needed.
- **Nouns:** Zenith_ScriptCheckStateless, Zenith_ScriptStatelessVerdict, D-LUAU-STATELESS, stateless system module, upvalue write, module-scope mutation, AST validation pass, per-invocation environment, restricted subset, adversarial corpus, load-time rejection, math.random, os.clock
- **Related:** Z2-D-018, Z2-D-023, Z2-D-084
### D-LUAUDBG
- **Status:** Open · **Stage:** 1
- **Question:** Which client does the Luau DAP server serve? The server is built over `debugbreak`/`debugstep`/interrupt callbacks and `lua_breakpoint`, and fans breakpoints out to every VM.
<!-- elaboration -->
- **Alternatives:** (1) A generic DAP client such as VS Code. (2) A debugger client built into the Zenith2 editor. (3) Both, over the same server.
<!-- elaboration -->
- **Acceptance rule:** The chosen client sets breakpoints that hit on every per-worker VM and the world main VM, steps behaviours across yields, inspects declared behaviour state, and attaches to a development build on the S24 through the remote-tools channel; the DAP server is absent from shipping builds. Staged: the Stage 1 ruling chooses the client against the DAP server skeleton's scripted CTest session, and records the runtime criteria above (breakpoints on every per-worker VM and the main VM, stepping across yields, inspecting declared state, the S24 attach) as conditions of the choice, verified by the Stage 3 DAP integration and the S24 attach run; a Stage 3 failure of a condition reopens the choice.
- **Consumers blocked:** SCRIPT (the DAP server), EDIT (any in-editor client), DEVT (remote Android diagnostics), SEC (debugger exposure in shipping).
- **Harness and ruling split:** An ordinary prototype ticket builds the DAP server skeleton with a scripted CTest client session; it is scheduled in the Stage 1 sprint with fix version `Z2 Stage 1`, the stage whose ruling it serves; the DAP debugger it grows into (breakpoints, the stop protocol, stepping) is delivered at Stage 3. A `needs-human` ruling ticket evaluates the clients and appends the choice to this file.
- **Nouns:** D-LUAUDBG, DAP server, debug adapter protocol, debugbreak, debugstep, lua_breakpoint, breakpoint fan-out, debugger client
- **Related:** Z2-D-018, Z2-D-084
### D-PHASES
- **Status:** Open (refinement; the contract is pre-decided as Z2-D-080) · **Stage:** 1
- **Question:** Refine and confirm the frame-phase and authority contract: the per-field authority table for every engine component and the arbitration rules per contested component, and confirm or amend the lifecycle-command publication points pre-decided in Z2-D-080.
- **Alternatives:** (1) Confirm Z2-D-080 as written. (2) Confirm with amendments to phase contents, arbitration precedence or lifecycle publication points, each recorded in the ruling.
- **Acceptance rule:** The ruling lists every engine component field with its owning phase; every contested field names its arbitration rule; every lifecycle-command publication point pre-decided in Z2-D-080 is confirmed or amended; and the five scenario tests (teleport during a ragdoll; skipping a cinematic mid-blend; spawn and destroy in one tick; a world transition with in-flight async results; saving at the snapshot point during streaming with animation and physics active) are specified with expected outcomes.
- **Consumers blocked:** GAME (owner), JOBS, ECS, ANIM, PHYS, AI, SCRIPT, CINE, SAVE.
- **Harness and ruling split:** No measurement; a `needs-human` ruling ticket records the refinement.
- **Nouns:** Zenith_Phase, Zenith_SyncPointToken, D-PHASES, frame phase, per-field authority, arbitration rule, lifecycle command, publication point, sync point
- **Related:** Z2-D-080
### D-SCRIPT-TXN
- **Status:** Open (refinement; the boundary is pre-decided as Z2-D-051) · **Stage:** 1
- **Question:** Refine and confirm the script transaction specification: the entity boundary, instruction (safepoint) budgets, merge-time capacity and async publication.
- **Alternatives:** (1) Confirm Z2-D-051, Z2-D-052 and Z2-D-053 as written. (2) Confirm with amendments recorded in the ruling. For native codegen: (a) interpreted and native safepoint counts match, so native codegen stays enabled; (b) they differ, so native codegen is disabled for budget-limited simulation invocations.
- **Acceptance rule:** The interpreted-versus-native safepoint conformance test over the adversarial and representative corpus has run and its result fixes (a) or (b); the required transaction tests and the fault-matrix tests are specified with expected outcomes. Staged per Scripting.md [Z2-N-SCRIPT-308]: the Stage 1 ruling runs the budget adversarial corpus at VM level on Windows x64 and the S24 and fixes (a) or (b) per platform from it; the representative corpus, per entity transaction, joins in Stage 3 as the extended test of the budget-scope ticket; no platform's native policy is recorded `all` until that Stage 3 run passes.
- **Consumers blocked:** SCRIPT, ECS, SAVE (replay), GAME.
- **Harness and ruling split:** An ordinary harness ticket builds the safepoint-count comparison over the corpus with a CTest smoke run; a `needs-human` ruling ticket records the refinement.
- **Nouns:** Zenith_EntityTransaction, Zenith_ScriptNativePolicy, D-SCRIPT-TXN, entity transaction, safepoint count, native codegen, merge-time capacity, async publication
- **Related:** Z2-D-051, Z2-D-052, Z2-D-053
### D-SEL-NAV
- **Status:** Open · **Stage:** 1 or 2
- **Question:** Recast/Detour, or a custom navigation library?
- **Alternatives:** (1) Recast/Detour (with DetourTileCache and DetourCrowd). (2) A custom navmesh baker and pathfinder.
<!-- elaboration -->
- **Acceptance rule:** The winner supports tiled baking and streaming, dynamic obstacles, off-mesh links, several agent sizes and non-ground navigation, gives identical path results for identical inputs across worker counts, and meets `Z2-B-AI-001` (navigation mesh and pathfinding memory), `Z2-B-AI-002` (AI and navigation phase CPU) and `Z2-B-AI-003` (asynchronous path requests in flight) on W-Characters (100 agents) and W-Outdoor (4 km²) on the S24. Under Z2-D-041, a custom navigation library is adopted only with a recorded measured benefit over Recast/Detour against these criteria.
- **Consumers blocked:** AI, ENV (navigation generation), WORLD (navigation baking), STREAM (navigation streaming).
- **Harness and ruling split:** A harness ticket (bake and query benchmark on the workload terrain) and a `needs-human` ruling ticket. The ruling ticket is `blocks`-linked from the Stage 1 per-system budget ticket for AI.
<!-- elaboration -->
- **What the selection decides:** The selection decides who implements the navmesh bake stages and the runtime navmesh storage the queries read (AI's decoded neutral tile, or a `dtNavMesh` tile built from the same neutral section); the search loop, straight path, raycast, surface movement and nearest and random points are ZenithAI code over either storage (ruling AI-R1), so the search contract is identical in both candidates and no Detour source is patched. DetourTileCache and DetourCrowd are used by neither outcome; carving, local avoidance and volume (non-ground) navigation are ZenithAI code measured together with each candidate. Until W-Characters gains AI at Stage 4 the harness measures the acceptance rule's workloads through the stand-ins N7 (4 km² at W-Outdoor scale) and N14 (100 agents) and reports AI's share of the phase-7 span (`Zenith2/Docs/Design/AI/Navigation.md` → "Ruling consequences").
- **Nouns:** D-SEL-NAV, Recast, Detour, DetourTileCache, DetourCrowd, navmesh, pathfinding library, dtNavMesh, neutral tile
- **Related:** Z2-D-082, Z2-D-084
### D-SEL-TEXT
- **Status:** Open · **Stage:** 1 or 2
- **Question:** Which text stack: HarfBuzz + FreeType + msdfgen, or alternatives; and bidi through SheenBidi or an ICU4X subset?
- **Alternatives:** (1) HarfBuzz + FreeType + msdfgen. (2) Alternative shaping and rasterisation stacks. For bidi: (a) SheenBidi; (b) an ICU4X subset.
<!-- elaboration -->
- **Acceptance rule:** The stack shapes the test corpus (Arabic, Hebrew, Devanagari, Thai, CJK, pseudo-localized long strings) correctly, supports a font fallback chain, fits `Z2-B-UI-001` (UI, fonts and glyph atlases), and meets `Z2-B-UI-002` (UI update and layout CPU) in W-UI2D on the S24; licences are recorded in the dependency inventory.
<!-- elaboration -->
- **Order of comparison (sharpened 2026-10-10, cross-doc item X251 (b)):** The acceptance rule's criteria stay as stated and are compared conjunctively, in this order. 1. The corpus and the fallback chain pass on Windows and the S24, else the stack is out: the test-font wiring, the Bidi group's levels, graphemes, line breaks and SA dictionary breaks against ICU4C offsets recorded before the ruling by a committed record case, fallback, the pseudo-localized long strings laid out in the test font, and outline equivalence. 2. Among the stacks passing step 1, one that both fits `Z2-B-UI-001` and meets `Z2-B-UI-002` in W-UI2D on the S24 beats every stack that misses either. 3. Only between two that meet both, the lower `Z2-B-UI-002` time beyond the recorded run-to-run spread wins. 4. Otherwise the smaller resident bytes. 5. Otherwise the smaller binary size. When no stack passes step 1, or none passing it meets every row, the ruling records the shortfall and the human decides. Shaping goldens are not a criterion, because HarfBuzz is common to every candidate (`Zenith2/Docs/Design/UI.md` → "Selection decisions UI owns"). UI schedules the harness and ruling pair in Stage 2.
- **Consumers blocked:** UI (text shaping, font fallback, bidirectional text), TWOD, CINE (subtitles).
- **Harness and ruling split:** A harness ticket (shaping corpus and throughput benchmark) and a `needs-human` ruling ticket.
- **Nouns:** D-SEL-TEXT, HarfBuzz, FreeType, msdfgen, SheenBidi, ICU4X, text shaping, bidi, font fallback
- **Related:** Z2-D-082, Z2-D-084
### D-SEL-UIRT
- **Status:** Open · **Stage:** 1 or 2
- **Question:** RmlUi, or a custom retained UI?
- **Alternatives:** (1) RmlUi. (2) A custom retained UI.
<!-- elaboration -->
- **Acceptance rule:** The winner provides retained layout, navigation and focus, a semantic accessibility tree, large-list virtualization, world-space UI and rendering through Flux's render graph, supports visual authoring, and meets `Z2-B-UI-002` (UI update and layout CPU) and `Z2-B-UI-003` (UI composite GPU) in W-UI2D on the S24.
<!-- elaboration -->
- **Sharpened question (orchestrator ruling Q75 (a), 2026-10-03, human may override; cross-doc item X251 (a)):** RmlUi's layout and style engine, or ZenithUI's own, inside a shared ZenithUI shell. The shell provides input, focus, navigation, controls, the semantic tree, virtualization, world-space panels, RTL mirroring, text, rendering and the style resolver that resolves every property into the computed style for both candidates. RmlUi's style engine cascades the layout-affecting properties from cook-generated RCSS and run-time pushes (`Zenith2/Docs/Design/UI/Layout.md` → "Runtime selection").
- **Order of comparison (X251 (a)):** The acceptance rule stays neutral and is compared conjunctively, in this order. 1. Every CSS case and every "ZenithUI semantics" case of `Zenith2/Docs/Design/UI/Layout.md` → "Layout conformance cases" passes for the candidate in the harness's selection reports on Windows and the S24, a ZenithUI-semantics case met natively or through a documented shim, else the candidate is out. Shell cases are not selection criteria: while the decision is open they gate only in an invariant form relative to each candidate's own layout, and their absolute expectations are reported as class `shell`, recorded and not judged. 2. Among the candidates passing step 1, one that meets both `Z2-B-UI-002` and `Z2-B-UI-003` in W-UI2D on the S24 beats every candidate that misses either. 3. Only between two that meet both, the lower `Z2-B-UI-002` time beyond the recorded run-to-run spread wins. 4. Otherwise the smaller binary size. When no candidate passes step 1, or none passing it meets every row, the ruling records the shortfall and the human decides; there is no default winner. The ruling ticket is blocked by the Stage 1 per-system budget ticket for UI and deletes the losing engine with its manifest entry. UI schedules the harness and ruling pair in Stage 2.
- **Consumers blocked:** UI, TWOD, EDIT (UI visual authoring).
- **Harness and ruling split:** A harness ticket (a W-UI2D-scale UI benchmark) and a `needs-human` ruling ticket.
- **Nouns:** D-SEL-UIRT, RmlUi, retained UI, UI runtime
- **Related:** Z2-D-079, Z2-D-084
### D-SEL-JSON
- **Status:** Open · **Stage:** 1
- **Question:** yyjson or simdjson?
- **Alternatives:** (1) yyjson. (2) simdjson.
<!-- elaboration -->
- **Acceptance rule:** The winner reads and writes authoring JSON with canonical key order, preserves unknown fields on round-trip, reports error positions, passes a JSON conformance corpus and a fuzzing run under `win-asan`, and meets `Z2-B-EDIT-005` (scene open) on the reference desktop. A read-only parser needs a writer alongside it, which counts against it.
- **Consumers blocked:** SCENE, ASSET, ECS (schema loading), EDIT, CORE (configuration).
- **Harness and ruling split:** A harness ticket (parse and write benchmark over representative authoring documents) and a `needs-human` ruling ticket.
- **Nouns:** Zenith_JsonWriter, D-SEL-JSON, yyjson, simdjson, JSON parser, canonical serialization, unknown-field preservation
- **Related:** Z2-D-038, Z2-D-084
### D-SEL-HASH
- **Status:** Open · **Stage:** 1
- **Question:** The named candidate is xxHash. Which variant serves which use (hash containers, the physical-resource and PSO caches, DDC keys)?
<!-- elaboration -->
- **Alternatives:** (1) XXH3-64 for in-memory keys and XXH3-128 for persisted keys. (2) XXH64 throughout. (3) A hand-rolled hash — named as the baseline the selection is measured against.
<!-- elaboration -->
- **Acceptance rule:** Throughput on the reference hardware for short keys (container lookups) and long inputs (DDC keys), collision behaviour for persisted keys, and deterministic output across platforms, which persisted keys need.
- **Consumers blocked:** CORE (hash containers), ASSET (DDC keys), RHI (physical-resource cache, PSO keys), PACK.
- **Harness and ruling split:** A harness ticket (hash benchmark) and a `needs-human` ruling ticket.
- **Nouns:** Zenith_Hash64, D-SEL-HASH, xxHash, XXH3, XXH64, hash function, DDC key, PSO key
- **Related:** Z2-D-084
### D-SEL-CODEC
- **Status:** Open · **Stage:** 2
- **Question:** Opus or Vorbis for streamed audio?
- **Alternatives:** (1) Opus. (2) Vorbis.
<!-- elaboration -->
- **Acceptance rule:** Decode CPU per voice on the S24 against `Z2-B-AUDIO-002` (audio update CPU) and `Z2-B-AUDIO-003` (audio mixer callback CPU per period), quality at the chosen bitrates, sample-accurate seeking and loop points for music transitions, and package size against `Z2-B-PACK-001` (AAB base module download size).
- **Consumers blocked:** AUDIO (streaming and decode), PACK, CINE (dialogue and video audio).
- **Harness and ruling split:** A harness ticket (decode benchmark) and a `needs-human` ruling ticket.
- **Nouns:** Zenith_AudioStreamDecoder, D-SEL-CODEC, Opus, Vorbis, streamed audio, audio codec
- **Related:** Z2-D-084
### D-SEL-VIDEO
- **Status:** Open · **Stage:** 2
- **Question:** Platform decoders (Media Foundation on Windows, MediaCodec on Android) or FFmpeg, including licensing?
- **Alternatives:** (1) Platform decoders. (2) FFmpeg.
<!-- elaboration -->
- **Acceptance rule:** Hardware-accelerated decode on both platforms within `Z2-B-CINE-001` (cinematic timelines and video decode buffers), GPU-texture output into the render graph, audio/video sync, and a licence compatible with static linking and distribution (Z2-D-024); the licence finding is recorded in the dependency inventory.
- **Consumers blocked:** CINE (video playback), PACK, SHIP (licence inventory).
- **Harness and ruling split:** A harness ticket (decode prototype on both platforms) and a `needs-human` ruling ticket, which includes the licence review.
- **Nouns:** D-SEL-VIDEO, Media Foundation, MediaCodec, FFmpeg, video playback, licensing
- **Related:** Z2-D-024, Z2-D-084
### D-SEL-CRASH
- **Status:** Open · **Stage:** 2
- **Question:** Crashpad, or a custom minidump writer?
- **Alternatives:** (1) Crashpad. (2) A custom minidump writer.
<!-- elaboration -->
- **Acceptance rule:** Out-of-process capture on Windows and Android, minidumps symbolised against the retained symbols, no exceptions in engine code, and no crash handler reachable as a remote tool in shipping builds.
- **Consumers blocked:** DEVT (crash dumps and symbols), SHIP (symbol retention), PLAT.
- **Harness and ruling split:** A prototype ticket and a `needs-human` ruling ticket.
- **Nouns:** Zenith_CrashHandler, D-SEL-CRASH, Crashpad, minidump, crash handler, symbol retention
- **Related:** Z2-D-084
### D-SEL-2DPHYS
- **Status:** Open · **Stage:** 2
- **Question:** Box2D v3, or Jolt in a 2D-constrained mode?
- **Alternatives:** (1) Box2D v3. (2) Jolt in 2D-constrained mode.
<!-- elaboration -->
- **Acceptance rule:** One neutral total rule (orchestrator ruling Q91 (a), 2026-10-09, human may override; cross-doc item X299). Gates: the 2D scenario suite of `Zenith2/Docs/Design/TWOD/Tests.md` → "D-SEL-2DPHYS scenario suite" minus `ExactSave`, plus per-build determinism on Windows and the S24. Then the rows "2D physics step CPU" (Z2-B-TWOD-004) and "2D physics world memory" (Z2-B-TWOD-005) at the Stage 1 committed Game2D and W-UI2D scales (the Stage 1 flagship acceptance ticket for Game2D and UI's Stage 1 workload scale ticket for W-UI2D; no fixed numbers): both candidates meet the rows → Jolt-2D (no new dependency); exactly one meets → it; neither meets → the lower Game2D-scale p95 step time, with the breach recorded; exactly one passes the gates → it, any row breach recorded; neither passes → the ruling ticket stays Blocked naming the failing scenarios, and Stage 2 acceptance waits on it. EXACT save capability breaks ties after the rows; a COHERENT-only winner ships COHERENT 2D captures. The rows are compared through the harness zone `Physics2DSelection.Tick` (every seam call one `TwoD.Physics2DStep` would make per tick, p95 over ticks 300 … 2,099) and the harness's peak resident bytes in the seeded scenes `CostGame2D` and `CostUI2D`, each committed row value being an upper bound for the harness value (`Zenith2/Docs/Design/TWOD/Tests.md` → "D-SEL-2DPHYS cost procedure"). The ruling ticket is blocked by the measurement run and the Stage 1 per-system budget ticket for TWOD; under the Jolt outcome the production path waits for PHYS's conditional post-signoff patch (ruling TD10-CONDITIONAL). The TWOD rows replace Z2-B-PHYS-002 and Z2-B-PHYS-001 here, because PHYS's rows measure PHYS's nodes (`Zenith2/Docs/Design/TWOD.md` → "Selection decision D-SEL-2DPHYS").
- **Consumers blocked:** TWOD (2D physics), PHYS.
- **Harness and ruling split:** A harness ticket (2D physics benchmark scene) and a `needs-human` ruling ticket.
- **Nouns:** D-SEL-2DPHYS, Box2D, Jolt 2D-constrained, 2D physics
- **Related:** Z2-D-003, Z2-D-078, Z2-D-084
### D-SEL-MOTIONMATCH
- **Status:** Open · **Stage:** 3
- **Question:** Is motion matching in scope, or blend trees only?
- **Alternatives:** (1) Motion matching in scope. (2) Blend trees and state machines only.
<!-- elaboration -->
- **Acceptance rule:** CinematicSlice character-quality needs, CPU and memory cost for W-Characters (100 characters at 60 bones) on the S24 against `Z2-B-ANIM-001` (animation clips, skeletons and pose buffers) and `Z2-B-ANIM-002` (animation phase CPU), and the authoring cost of the animation database. The ruling classifies motion matching required or excluded, with a rationale.
- **Consumers blocked:** ANIM, CHAR.
- **Harness and ruling split:** A harness ticket (motion-matching prototype over a test database) and a `needs-human` ruling ticket.
- **Nouns:** Zenith_MotionMatcher, D-SEL-MOTIONMATCH, motion matching, blend tree, animation database
- **Related:** Z2-D-084
### D-SEL-FBX
- **Status:** Open · **Stage:** 4
- **Question:** Is FBX supported by the import pipeline, and through which conversion path? The supported-DCC matrix names "FBX via a conversion path if selected".
- **Alternatives:** (1) glTF 2.0 only: FBX content reaches the pipeline only through each DCC's own glTF export. (2) An out-of-process converter built on FBX2glTF. (3) An out-of-process conversion through Blender run headless. (4) An out-of-process converter built on the Autodesk FBX SDK (proprietary licence; redistribution terms reviewed). Every converter runs out of process, like every importer (importer crash isolation).
- **Acceptance rule:** Round-trip fidelity across the supported-DCC matrix: the per-feature rows (skins, morphs, animation, materials, cameras, lights) and the per-DCC axis, handedness and unit-conversion tests pass through the chosen path on the test asset set, compared with the same assets imported through glTF 2.0. The converter's licence is recorded in the dependency and licence inventory. If no converter passes both, FBX content reaches the pipeline only through DCC glTF export.
- **Consumers blocked:** CONTENT (the supported-DCC matrix and importer tickets), SHIP (the licence inventory row).
- **Harness and ruling split:** An ordinary converter-prototype ticket builds the out-of-process conversion for each candidate and the fidelity comparison over the test asset set, with a CTest smoke run; a `needs-human` ruling ticket runs the comparison, includes the licence review, and appends the ruling to this file.
- **Nouns:** Zenith_FbxImporter, D-SEL-FBX, FBX, FBX2glTF, Blender headless, Autodesk FBX SDK, glTF 2.0, fastgltf, supported-DCC matrix, out-of-process importer, licence inventory
- **Related:** Z2-D-082, Z2-D-084
### D-CLASS-SIM
- **Status:** Open · **Stage:** 2
- **Question:** Cloth, destruction and soft bodies: each classified required, deferred or excluded, with an owner and a library candidate (Jolt soft bodies, custom).
- **Alternatives:** For each of cloth, destruction and soft bodies: (1) required, with an owner area and a library candidate; (2) deferred to Stage 7; (3) excluded.
- **Acceptance rule:** Each of the three has a recorded classification and rationale. A required one names its owner area and library candidate and is scheduled in Stage 5 ("the in-scope specialist simulations"); a deferred one gets a full requirement tree in Stage 7; an excluded one gets its own decision ID and no tickets.
- **Consumers blocked:** PHYS, CHAR (cloth), VFX (destruction debris), ENV.
- **Harness and ruling split:** A `needs-human` classification ruling ticket; any required classification whose library choice needs measurement gets its own harness ticket.
- **Nouns:** Zenith_Physics, D-CLASS-SIM, cloth, destruction, soft bodies, Jolt soft bodies, specialist simulation
- **Related:** Z2-D-003
### D-VTX-TANGENTLESS
- **Status:** Open · **Stage:** 3
- **Question:** Adopt an octahedral normal plus a screen-space-derivative tangent frame instead of QTangent — per quality tier, per material class, or not at all?
- **Alternatives:** (1) Adopt per quality tier. (2) Adopt per material class. (3) Not at all. The hypothesis stores only an octahedral normal (2 components) and derives tangent and bitangent per pixel from screen-space derivatives of position and UV (Schüler's normal mapping without precomputed tangents; Mikkelsen's surface-gradient framework).
- **Acceptance rule:** The harness measures mismatch with MikkTSpace-baked normal maps (FLIP against the QTangent reference on the test asset set); derivative artefacts at triangle and quad edges and in the visibility-buffer path (which uses analytic gradients); and ALU cost versus bandwidth saved on the S24 and the RTX 3070. QTangent is the default until the ruling.
- **Consumers blocked:** GEOM (vertex format variants), RPROD (permutations), CONTENT (cook), and only the tickets that would adopt the tangentless variant.
- **Harness and ruling split:** A Stage 3 harness ticket plus a `needs-human` ruling ticket.
- **Nouns:** D-VTX-TANGENTLESS, octahedral normal, screen-space derivative, tangentless, QTangent, MikkTSpace, surface gradient
- **Related:** Z2-D-062, Z2-D-084
### D-TBDR
- **Status:** Open · **Stage:** 5
- **Question:** Should the render graph use tile-local merging on tile-based GPUs, measured on the S24?
<!-- elaboration -->
- **Alternatives:** (1) Keep no tile-local merging. (2) Tile-local reads for specific pass pairs on Android (for example prepass to lighting). (3) Tile-local merging wherever the graph finds a compatible pair.
<!-- elaboration -->
- **Measured forms of the alternatives:** the harness `Zenith_Bench_TBDR` measures each alternative as one configuration of the Forward+ frame on the S24. (1) is the baseline: every merge of the render graph's pass-merging algorithm and no tile-local read. (2) is measured as `ForwardPlusOpaque`, `SkyAtmosphere` and `TransparentForward` in one rendering scope, the second and third passes reading colour and depth tile-locally through `VK_KHR_dynamic_rendering_local_read` where the Adreno driver exposes it. (3) is measured as (2) plus prepass phase 2 kept on tile ahead of the opaque scope, with the final HiZ build (a compute pass, which cannot join a rendering scope) moved after the merged scope: the pass-merging rule relaxed so that a same-pixel attachment read between two adjacent compatible raster passes becomes a tile-local read instead of a blocking barrier, applied to every such pair of the frame.
<!-- elaboration -->
- **Acceptance rule:** Each configuration is measured on the S24 on W-Indoor and W-Outdoor under the research procedure (controlled conditions, at least 10 repetitions, 95% confidence intervals). A configuration (2) or (3) passes only when all of these hold against (1): its median main-view GPU frame time is at least 5% lower than (1)'s; its 95% confidence interval and (1)'s do not overlap; its backend-parity and path-parity images are bit-identical to (1)'s; and sync validation is clean. Among passing configurations the one with the lowest median GPU frame time is adopted; when none passes, (1) stays. The measurement also reports the Adreno bandwidth counters of every configuration and the two-phase culling mid-frame depth store and reload row, `Z2-B-RPROD-026` (HiZ mid-frame depth store and reload bandwidth); bandwidth is reported evidence and is not itself an acceptance criterion.
- **Consumers blocked:** RPROD and RHI (render-graph merging), LIGHT and every Stage 5 and subsequent rendering ticket. The D-TBDR revalidation is a Stage 5 ticket and blocks only Stage 5 and subsequent rendering tickets.
- **Harness and ruling split:** A harness ticket (tile-local merge prototype with counter capture) and a `needs-human` ruling ticket measured on the S24.
- **Nouns:** Flux_AddHiZBuild, Flux_RasterTargets, Zenith_Bench_TBDR, VK_KHR_dynamic_rendering_local_read, ForwardPlusOpaque, TransparentForward, D-TBDR, tile-local merging, tile-based GPU, Adreno counters, mid-frame depth store, bit-identical parity
- **Related:** Z2-D-036, Z2-D-060, Z2-D-084
### D-SEL-UPSCALER
- **Status:** Open · **Stage:** 5
- **Question:** Which third-party temporal upscaler, if any, runs behind `FLUX_UPSCALER_EXTERNAL` (`Flux_UpscalerMode` value 3, refused by `Flux_ReadLightQualitySettings` with `FLUX_LIGHTING_ERROR_INVALID_SETTINGS` until this decision is ruled), and on which quality tiers? The selection is owned by LIGHT; the built-in temporal paths `FLUX_UPSCALER_TAA` and `FLUX_UPSCALER_TAAU` are unaffected.
- **Alternatives:** (1) AMD FidelityFX Super Resolution 2 or 3: MIT licence, shader source compiled offline through the pinned Slang/DXC toolchain, Vulkan and D3D12, mobile-capable. (2) NVIDIA DLSS: proprietary SDK, desktop NVIDIA GPUs only. (3) Intel XeSS: Windows only. (4) None: `FLUX_UPSCALER_EXTERNAL` stays refused and `FLUX_UPSCALER_TAAU` remains the temporal upscaler on every tier.
- **Acceptance rule:** A candidate is eligible only with a licence compatible with shipping, support on both RHIs of the tier it is adopted for, and no runtime shader compilation (Z2-D-025); S24 availability is recorded for each candidate. An eligible candidate is adopted for a tier only when it beats `FLUX_UPSCALER_TAAU` on FLIP against a 4× supersampled reference on the TAA temporal scenes at equal or lower `Z2-B-LIGHT-010` (TAA or temporal upscaler GPU) time on that tier's reference hardware, over 10 repetitions under the research procedure of `Zenith2/Docs/Rendering.md → "Shading path research methodology"`. A tier where no candidate is adopted keeps `FLUX_UPSCALER_TAAU`. An adopted candidate's licence is recorded in the dependency and licence inventory, and its package is pinned in `Zenith2/ThirdParty/manifest.json` (Z2-D-082).
- **Consumers blocked:** LIGHT (the Stage 5 external-upscaler tickets of temporal upscaling) and SHIP (the licence inventory row of an adopted candidate).
- **Harness and ruling split:** An ordinary harness ticket builds the FLIP-at-equal-GPU-time comparison of each eligible candidate against `FLUX_UPSCALER_TAAU` on the TAA temporal scenes under `Zenith2/Benchmarks/`, with a CTest smoke run; a `needs-human` ruling ticket runs it on the RTX 3070 and the S24, includes the licence review, and appends the ruling to this file (Z2-D-084).
- **Nouns:** Flux_UpscalerMode, FLUX_UPSCALER_EXTERNAL, FLUX_UPSCALER_TAAU, Flux_ReadLightQualitySettings, D-SEL-UPSCALER, temporal upscaler, FidelityFX Super Resolution, DLSS, XeSS, supersampled reference, licence inventory
- **Related:** Z2-D-025, Z2-D-082, Z2-D-084
### D-SEL-EnTT-SHIPPING
- **Status:** Open only as flagged proposal P1; not an accepted decision · **Stage:** 5
- **Question:** From flagged proposal P1: after the Stage 5 research report, does EnTT stay a supported shipping backend, or become research-only? P1 also scopes EnTT's research claim to C++-registered components.
- **Alternatives:** (1) EnTT stays a supported shipping backend. (2) EnTT becomes research-only.
- **Acceptance rule:** Exists only if the human approves P1. The ruling weighs the dynamic-storage adapter's share of measured cost in the research report against backend-maintenance burden and the original intent of three equal shipping backends.
- **Consumers blocked:** ECS, RESEARCH, MOD (`game.json` backend options) — only if P1 is approved.
- **Harness and ruling split:** The Stage 5 research report is the measurement; a `needs-human` ruling ticket, filed only if P1 is approved.
- **Nouns:** D-SEL-EnTT-SHIPPING, P1, EnTT, shipping backend, research-only backend, dynamic-storage adapter
- **Related:** Z2-D-039, Z2-D-064, Z2-D-130

### D-SEL-NETTRANSPORT
- **Status:** Open · **Stage:** 7
- **Question:** Which transport library runs under `Zenith_NetTransportBackend` for Stage 7 networked sessions (`Zenith2/Docs/Design/NET/Stage7.md` → "Transport selection")?
- **Alternatives:** (1) NET's own reliability layer over `Zenith_NetDatagramPort`, the Stage 3 transport extended with channels, reliability and fragmentation; no new dependency. (2) Valve GameNetworkingSockets (BSD-3-Clause; built-in encryption; one service thread; protobuf and OpenSSL or libsodium dependencies). (3) ENet (MIT; no encryption of its own). (4) yojimbo with netcode (BSD-3-Clause; libsodium encryption; connect tokens).
- **Acceptance rule:** A candidate is eligible when it builds with the pinned toolchains for Windows and Android arm64 (NDK r27c, 16 KB pages), stays confined to `ZenithNet/Internal` (Z2-D-082), joins any thread of its own in shutdown step 6 and never blocks a job (Z2-D-006), and has a port injection point so `Zenith_NetSimulatedPort` and the memory port work underneath it. The transport conformance suite `ZenithNet_TransportConformance` passes under all four simulation profiles; among passing candidates, the one with the lowest median S24 CPU per 1,000 messages over 10 repetitions wins, a third-party candidate only when it is at least 20% cheaper than candidate 1 or supplies the encryption the D-NET-SESSION-AUTH ruling requires and candidate 1 lacks; candidate 1 wins ties. Per-connection memory and the licence are recorded.
- **Consumers blocked:** NET (the Stage 7 transport, session and diagnostics tickets) and SHIP (the manifest entry of an adopted library).
- **Harness and ruling split:** An ordinary harness ticket under `Zenith2/Benchmarks/NetTransport/` with a CTest smoke run; a `needs-human` ruling ticket runs it on the reference hardware, commits `Zenith2/Docs/Research/NetTransport/` and appends the ruling to this file (Z2-D-084).
- **Nouns:** D-SEL-NETTRANSPORT, Zenith_NetTransportBackend, Zenith_NetDatagramPort, Zenith_NetSimulatedPort, ZenithNet_TransportConformance, transport library, GameNetworkingSockets, reliability layer, fragmentation
- **Related:** Z2-D-006, Z2-D-073, Z2-D-082, Z2-D-084
### D-NET-SESSION-AUTH
- **Status:** Open · **Stage:** 7
- **Question:** How does a Stage 7 server know who joined a session, and how are datagrams protected (`Zenith2/Docs/Design/NET/Stage7.md` → "Session security")?
- **Alternatives:** (1) Platform identity tickets: a Steam authentication session ticket or a Play Games server auth code, obtained and validated through ONLINE's backends, with AEAD keys from an X25519 exchange inside the handshake. (2) Signed join tokens: an Ed25519-signed token `{ server address, expiry, player key, session key }` issued by a matchmaking or hosting service outside the engine, the server holding the public key, with AEAD (XChaCha20-Poly1305) under the token's session key. (3) Direct connect with a session password, keys derived by a SPAKE2-style exchange.
- **Acceptance rule:** Candidate 2 by default; candidate 1 is adopted only if ONLINE's backends expose ticket validation for both stores; candidate 3 only for LAN sessions in addition to the chosen one. Criteria: works without a game-specific backend service for the flagship slices, no secret stored in the client package, and the licence and audit status of a pinned, audited cryptographic library admitted to the manifest by the ruling. Whatever is chosen, Stage 7 Shipping builds accept a non-loopback session only when it is authenticated and encrypted; Stage 3's Shipping guard is lifted in the same diff as the ruling's implementation, and the parser limits, rate limits and the 8-malformed-packets rule stay.
- **Consumers blocked:** NET (handshake version 2, Shipping sessions, the server secret) and ONLINE (ticket validation, only if candidate 1 is adopted).
- **Harness and ruling split:** An analysis ruling with no measurement harness: a `needs-human` ruling ticket records the written analysis and appends the ruling to this file; the chosen library's cost is measured afterwards against the Stage 7 NET rows.
- **Nouns:** D-NET-SESSION-AUTH, CONNECT_REJECT, CHALLENGE_RESPONSE, join token, session authentication, XChaCha20-Poly1305, authenticated encryption, handshake
- **Related:** Z2-D-073, Z2-D-074, Z2-D-082, Z2-D-084
### D-NET-PREDICTION-MODEL
- **Status:** Open · **Stage:** 7
- **Question:** Which client prediction model does Stage 7 use (`Zenith2/Docs/Design/NET/Stage7.md` → "Prediction model selection")?
- **Alternatives:** (1) Owned-entity re-simulation: the client simulates its owned entities ahead and, on a correction, restores their state to the server's at tick T and re-runs phases 2 to 8 for them alone up to the current tick, against a frozen copy of everything else. (2) Full-world rollback: a per-tick out-of-tick capture of the whole client world (SAVE's out-of-tick snapshot), restored and re-run for every phase. (3) Controller-only prediction: only `Zenith.Physics.CharacterController` movement is predicted, by PHYS's controller sweep run ahead with buffered commands.
- **Acceptance rule:** Candidate 1 by default; candidate 2 only if candidate 1's re-run subset cannot express a flagship-slice mechanic and candidate 2 fits the Stage 7 NET prediction row; candidate 3 only if neither fits. Criteria: S24 CPU at 150 ms RTT within the Stage 7 NET prediction row; the misprediction rate on the deterministic scenario `PredictionCorridor` (`Games/WorkloadCharacters/Scenes/PredictionCorridor.scene.json`, driven by `Tests/PredictionCorridor.input.json` and run by the suite `Zenith2/Tests/NetPrediction/` through the simulated port's profiles); intrusion into other areas; and a client's replay still reproducing its run (Z2-D-019).
- **Consumers blocked:** NET (every prediction ticket) and the areas whose systems must be re-runnable for a subset of entities.
- **Harness and ruling split:** An ordinary harness ticket under `Zenith2/Benchmarks/` authors the scenario and measures each candidate; a `needs-human` ruling ticket runs it on the S24, commits its report under `Zenith2/Docs/Research/` and appends the ruling to this file (Z2-D-084).
- **Nouns:** D-NET-PREDICTION-MODEL, PredictionCorridor, Zenith.Physics.CharacterController, client prediction, re-simulation, rollback, misprediction, reconciliation
- **Related:** Z2-D-019, Z2-D-073, Z2-D-084
### D-NET-SERVER-PLATFORM
- **Status:** Open · **Stage:** 7
- **Question:** On which platforms does the Stage 7 dedicated server build (`Zenith2/Docs/Design/NET/Stage7.md` → "Server platforms")?
- **Alternatives:** (1) Windows x64 only, built in the existing `win-release` configuration. (2) Windows plus Linux x86-64: a new `linux-server` configure preset on the `linux-tsan` host's toolchain without sanitizers, with its own SHIP pins and `CompileCoverage` matrix entry.
- **Acceptance rule:** (1) unless a flagship slice's hosting target requires Linux. Criteria: hosting cost (most server hosting is Linux), the maintenance of a third production platform against Z2-D-010, toolchain pins and CI time. Adopting (2) needs a decision amending Z2-D-010's platform list, recorded with the ruling.
- **Consumers blocked:** NET (the `<Name>Server` build target, its gate step and packaging) and SHIP (the pins of a Linux server preset).
- **Harness and ruling split:** An analysis ruling: a `needs-human` ruling ticket records the written analysis and appends the ruling to this file.
- **Nouns:** D-NET-SERVER-PLATFORM, linux-server, CompileCoverage, dedicated server platform, server build target, hosting cost
- **Related:** Z2-D-010, Z2-D-073
### D-XR-PLATFORMS
- **Status:** Open · **Stage:** 7
- **Question:** Does Stage 7 add a standalone Android OpenXR headset as a second validated Android device, and passthrough with a non-opaque environment blend mode (`Zenith2/Docs/Design/SCOPE.md` → "Open decisions SCOPE owns")?
- **Alternatives:** (a) Neither. (b) A standalone headset (Meta Quest 3 class through the Khronos Android loader) without passthrough. (c) (b) plus `XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND` passthrough.
- **Acceptance rule:** (b) or (c) is adopted only when a harness build of WorkloadIndoor runs on the headset at the XR frame-interval row with the S24's Android build unchanged, and a human accepts adding a second validated Android device to Z2-D-010 through a flagged proposal; otherwise (a).
- **Consumers blocked:** SCOPE (the standalone-headset and passthrough tickets of the XR tree).
- **Harness and ruling split:** An ordinary harness ticket builds WorkloadIndoor for the headset; a `needs-human` ruling ticket runs it on the device and appends the ruling to this file (Z2-D-084).
- **Nouns:** D-XR-PLATFORMS, ZenithXR, XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND, standalone headset, passthrough, Khronos Android loader, validated Android device
- **Related:** Z2-D-010, Z2-D-072, Z2-D-084
### D-XR-MULTIVIEW
- **Status:** Open · **Stage:** 7
- **Question:** Does Stage 7 XR render through single-pass stereo (`VK_KHR_multiview` and D3D12 view instancing) or through the two per-eye views of RPROD's stereo group?
- **Alternatives:** (a) Two views, the Stage 7 baseline. (b) Multiview for the opaque and prepass passes only. (c) Multiview everywhere it is legal.
- **Acceptance rule:** The harness measures W-Indoor per-eye GPU time on the RTX 3070 with each alternative at the same image; (b) or (c) is adopted when it saves at least 15% of the frame with PathParity's stereo images bit-identical to (a); otherwise (a).
- **Consumers blocked:** SCOPE (the XR render tickets) and RPROD (the stereo view group, only if (b) or (c) is adopted).
- **Harness and ruling split:** An ordinary harness ticket under `Zenith2/Benchmarks/XrMultiview/`; a `needs-human` ruling ticket runs it on the reference hardware and appends the ruling to this file (Z2-D-084).
- **Nouns:** D-XR-MULTIVIEW, VK_KHR_multiview, VIEW_KIND_STEREO, PathParity, view instancing, single-pass stereo, per-eye views
- **Related:** Z2-D-059, Z2-D-072, Z2-D-084
### D-CONSOLE-RHI
- **Status:** Open · **Stage:** 7
- **Question:** Per console platform, how is the RHI realised?
- **Alternatives:** Xbox Series: (a) `ZenithRHI_D3D12` with a platform overlay `Internal/` variant, or (b) a separate backend. PlayStation 5 and Switch 2: (c) a separate backend library of the platform.
- **Acceptance rule:** Ruled per platform after its SDK-access ticket, by a `needs-human` ruling ticket that records the baseline-equivalence check of `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Neutral seams a console implements".
- **Consumers blocked:** SCOPE (the console RHI tickets of that platform).
- **Harness and ruling split:** One `needs-human` ruling ticket per platform, blocked by that platform's SDK-access ticket; the ruling appends its entry to this file.
- **Nouns:** D-CONSOLE-RHI, ZenithRHI_D3D12, Zenith_RHIDevice, console backend, baseline equivalence, SDK-access ticket
- **Related:** Z2-D-013, Z2-D-071
### D-CONSOLE-SHADERS
- **Status:** Open · **Stage:** 7
- **Question:** Per console platform, what is the offline shader path?
- **Alternatives:** (a) Slang emits the platform's accepted intermediate form and the platform's compiler step finishes it. (b) The platform's compiler consumes HLSL that Slang emits.
- **Acceptance rule:** Ruled per platform by a harness that compiles every program of the shader set, and a ruling ticket that rejects any alternative failing a program or the generated-layout checks.
- **Consumers blocked:** SCOPE (the console shader tickets of that platform) and RPROD (the platform's shader-compiler step).
- **Harness and ruling split:** Per platform, a harness ticket and a `needs-human` ruling ticket, both blocked by that platform's SDK-access ticket; the ruling appends its entry to this file.
- **Nouns:** D-CONSOLE-SHADERS, ZenithShaderCompiler, offline shader path, intermediate form, generated layout, SDK-access ticket
- **Related:** Z2-D-015, Z2-D-025, Z2-D-071
### D-RT-SCOPE
- **Status:** Open · **Stage:** 7
- **Question:** Which ray-tracing features ship, on which quality tiers and devices (desktop alone, or the S24 too), and do ray-tracing pipelines join ray queries (`Zenith2/Docs/Design/ADVGFX/RayTracing.md` → "Decision D-RT-SCOPE")?
- **Alternatives:** (1) Shadows and AO on desktop high tiers only, ray queries only. (2) All four features (shadows, reflections, ambient occlusion, diffuse GI) on desktop high and epic tiers, ray queries only. (3) Candidate 2 plus RT shadows on the S24. (4) Candidate 2 plus ray-tracing pipelines for reflections and GI.
- **Acceptance rule:** A feature is admitted on a tier when its GPU time fits the tier's frame budget (`Z2-B-SHIP-001`) with the rest of the frame on W-OpenWorld; the acceleration structures of every admitted feature together stay within 512 MiB on desktop and 128 MiB on the S24 (an absolute bound, which the ruling then allocates as the pending acceleration-structure row, at or below it); and its physical-accuracy error is at most its fallback's on every reference scene that exercises the feature, and strictly below on at least one. Pipelines are admitted only if a measured feature is at least 15% faster with them, with non-overlapping 95% intervals.
- **Consumers blocked:** Z2-R-ADVGFX-122 to Z2-R-ADVGFX-140 and Z2-R-ADVGFX-171 (the RT tree).
- **Harness and ruling split:** An ordinary harness ticket under `Zenith2/Benchmarks/RayTracing/` with a CTest smoke run measures each feature's GPU time and memory on W-Indoor and W-OpenWorld on the RTX 3070 and the S24, and each feature's physical-accuracy gain over its fallback on the reference scenes; a `needs-human` ruling ticket blocked by it appends the ruling to this file (Z2-D-084).
- **Nouns:** D-RT-SCOPE, Flux_RayTracingScene, Flux_RayTracingFeature, Flux_RayTracingSettings, Zenith_RHIRayTracingCapabilities, Zenith_RHIAccelerationStructureHandle, acceleration structure, ray queries
- **Related:** Z2-D-043, Z2-D-084
### D-VGEO-RASTER
- **Status:** Open · **Stage:** 7
- **Question:** Does a compute software rasteriser join the hardware route for clusters of small virtualized-geometry triangles (`Zenith2/Docs/Design/ADVGFX/VirtualizedGeometry.md` → "Decision D-VGEO-RASTER")?
- **Alternatives:** (1) The hardware route alone (mesh shaders or indirect draws). (2) Hardware plus software for clusters below `render.vg.softwareEdgePixels`, on devices with 64-bit atomics.
- **Acceptance rule:** Candidate 2 is adopted when it is at least 20% faster on the densest scene with non-overlapping 95% intervals and is image-space equivalent to candidate 1: on the crack test scene and the three density scenes the final image is within FLIP mean 0.01 of candidate 1's, and no interior hole appears (zero pixels inside a mesh's silhouette that candidate 1 covers and candidate 2 leaves uncovered); the fraction of pixels whose visibility IDs agree is reported, not required. Otherwise candidate 1.
- **Consumers blocked:** Z2-R-ADVGFX-141 to Z2-R-ADVGFX-150 (the VG tree).
- **Harness and ruling split:** An ordinary harness ticket under `Zenith2/Benchmarks/VirtualGeometry/` with a CTest smoke run measures visibility-raster GPU time on a dense VG scene at three triangle densities on the RTX 3070 with each candidate; a `needs-human` ruling ticket blocked by it appends the ruling to this file (Z2-D-084).
- **Nouns:** D-VGEO-RASTER, Flux_VirtualGeometrySystem, Flux_VirtualGeometryResource, Flux_AddVGRaster, Flux_AddVGCulling, software rasteriser, visibility raster, interior hole
- **Related:** Z2-D-043, Z2-D-084

## Session rulings

These rulings govern how the Zenith2 documentation and the Z2 board are produced, and bind any session that resumes that work. Z2-D-087 to Z2-D-112 are the findings of the read-only re-verification of the /tick protocol and the Vault CLI against the code (each cites its `path:line` evidence); Z2-D-113 to Z2-D-132 are the orchestration rulings of the documentation and filing session. Z2-D-133 and Z2-D-136 to Z2-D-140 are SCOPE's design rulings (`Zenith2/Docs/Design/SCOPE.md` → "Rulings this design makes"); The two numbers between them are reserved for SCOPE's two rulings that wait on the human's decision, and those entries are appended in place when it is made.

### Z2-D-087 The seed branch lives in the main checkout; the worktree is never the filing location
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A worktree looks like the natural place to file thousands of tickets without touching master. But every Vault write resolves its authority first: `resolveAuthority` redirects any worktree whose vault sits at the same relative location as the registered authority to `C:\dev\Zenith` (`Tools/Vault/server/operations.mjs:58-71`, `authority.json`), and each write does `configured(root)` → `context(await resolveAuthority(root))` (`:147-152`). Commits then go to that checkout's `symbolic-ref HEAD` (`:270-299`). The write verbs `create`, `link`, `sprint`, `update` and `parent` are not in `UNREGISTERED_REFUSED_COMMANDS` (`Tools/Vault/cli/lanes.mjs:36-38`), so a filer running in a `.claude/worktrees/*` session is not refused: it still commits, onto whatever the main checkout's HEAD is.
- **Decision:**
  - Filing never runs in a worktree. The filer runs only from `C:\dev\Zenith`, with HEAD on the seed branch `z2-board-seed`.
  - The human's ruling: file on a seed branch checked out in the main checkout. The Vault writer commits each operation onto the branch that checkout's `HEAD` points at (`symbolic-ref HEAD`, then `commit-tree`, then `update-ref <branch> <commit> <parent>`, `server/operations.mjs:270-271, 297-299`), so every filing commit lands on the seed branch. A detached HEAD makes `symbolic-ref` fail, so the write fails.
  - `z2-board-seed` is cut from master after the docs and config commit (Z2-D-115), in the main checkout, with a clean tree.
- **Alternatives considered:** (1) File in a lane or scratch worktree — rejected: redirected to the main checkout by `resolveAuthority`, and not refused, so its commits land on the main checkout's branch. (2) File directly on master — rejected: thousands of commits on master and a half-filed master if filing breaks (Z2-D-069).
- **Consequences:** The main checkout is off master for the whole filing run, which is why every tick stays idle (Z2-D-096). Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** z2-board-seed, resolveAuthority, operations.mjs, UNREGISTERED_REFUSED_COMMANDS, lanes.mjs, symbolic-ref HEAD, commit-tree, update-ref, authority.json, .claude/worktrees, seed branch, main checkout
- **Related:** Z2-D-069, Z2-D-095, Z2-D-096, Z2-D-115
### Z2-D-088 Registering Z2 includes a metadata.md jira.projects row
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The programme first assumed board registration needs only `vault.config.json` and `zagent.project.json`. But `projectSelection` treats `metadata.md`'s `jira.projects` table as the only authority for `--project` once it is non-empty (`Tools/Vault/cli/read.js:460-477`; `reports.mjs:21, 36-37`). Measured: `zagent queue --project XYZ`, `blocked` and `report epic` all exit 1 with `{"error":"No Jira project with key XYZ."}`. `metadata.md` holds 29 `jira.projects` rows; FD's is `pwd7f8n4egx222rkl7emmzrx` in organisation `vv0505bn5nemdtqp9evrydjd` (pink-goat).
- **Decision:**
  - Registering Z2 (A1) appends one row to `metadata.md`'s `tables["jira.projects"]`: `key: "Z2"`, `name: "Zenith2"`, `organization_id: "vv0505bn5nemdtqp9evrydjd"`, `description: null`, `permission_scheme_id: null`, and a fresh 24-character `id` matching `[a-z][a-z0-9]{23}`, the same value used as `project_id` in `Zenith2/Docs/Board/workflow.md`, `labels.md` and `versions.md`.
  - The file is `-text` (`.gitattributes:191`), so every other byte of `metadata.md` is preserved exactly. Nothing else in it changes.
  - The row contains no `Zenith2/` path, so the blindness grep outside `Zenith2/` still matches only `vault.config.json` and `zagent.project.json`.
  - The row is outside the files the programme first named, so it is applied only with the human's approval (open question Q1 in `C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md`).
  - Amended 2026-10-16 (the human's ruling Q1 (a), not a new decision): the one-row edit is approved, and the human makes it as a hand edit in A8a, the docs and config commit (Z2-D-111); no session tool writes `metadata.md`.
- **Alternatives considered:** (1) Register without the row — rejected: `queue`, `blocked`, `report epic --project Z2` and `epic Z2-N` all exit 1, so the board's own verification cannot run. (2) Re-derive `--project` from board roots — rejected: that is a Vault code change, a PROC ticket, and the seed must work against the Vault as it is.
- **Consequences:** A1's file set grows by `metadata.md`. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** metadata.md, jira.projects, projectSelection, read.js, reports.mjs, project_id, organization_id, pink-goat, zagent queue --project Z2, zagent blocked, report epic, A1
- **Related:** Z2-D-094, Z2-D-119, Z2-D-108
### Z2-D-089 policy.resolved.projects.Z2 carries no categories, protectedPaths, conventions or cleanup
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Routing copies the project-file-owned keys (`baseBranch`, `branching`, `merge`, `conventions`, `cleanup`, `reviewOn`, `workerTools`, `categories`, `conventionDocs`, `protectedPaths`) from `zagent.project.json` onto every resolved project (`PROJECT_FILE_OWNED`, `withProjectFileOverlay`, `Tools/Vault/cli/compat.mjs:285-323`), while `projectPolicy` reads the raw entry (`:337-348`) for `baseBranch` in `integrate` (`integrate.mjs:172`), in finish's landed check (`write.mjs:951`) and for guardrails (`compat.mjs:677-691`). `projectPolicy` falls back to the fresh categories when the entry has none (`:342-343`); a measured in-memory simulation with no `categories` gave `contractValid: true`, the Z2 gates and the Z2 `decisionLog`.
- **Decision:**
  - `policy.resolved.projects.Z2` in `vault.config.json` carries `repo: "C:/dev/Zenith"`, `baseBranch: "master"`, `branching: "direct"`, `gates: []`, `merge: {push: false}`, `reviewOn: ["HIGH", "MEDIUM"]` and `workerTools: "Read,Edit,Write,Grep,Glob"`.
  - It carries no `categories`, `protectedPaths`, `conventions`, `cleanup` or `conventionDocs`. A categories mirror would go stale and shadow the fresh file; a `protectedPaths` key at project or category level would replace the global list in the payload (`compat.mjs:681`).
  - `repo` and `workerTools` are genuinely per-project. `baseBranch`, `branching`, `merge` and `reviewOn` equal `zagent.project.json`'s top-level values, because routing uses the overlay while `integrate` and `finish` read the raw entry.
  - No Z2 category key is named after a project-policy field (`protectedPaths`, `ticketTimeoutMinutes`, `fixForwardAttempts`, `maxConsecutiveBlocked`, `repo`, `baseBranch`), because `projectPolicy` spreads the category into the project (`compat.mjs:344`).
  - `policy.board`, `policy.client`, the guardrails and the routing are not touched.
- **Alternatives considered:** (1) Mirror the categories into the project entry, as FD and ZEN do — rejected: the mirror shadows the fresh file and already drifts (FD's lacks `Tooling`). (2) Per-Z2 overrides of `baseBranch`/`reviewOn` on the project — rejected: routing and integrate would disagree; per-Z2 differences go on the categories.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** policy.resolved.projects.Z2, vault.config.json, PROJECT_FILE_OWNED, withProjectFileOverlay, projectPolicy, compat.mjs, protectedPaths, workerTools, reviewOn, baseBranch, categories mirror
- **Related:** Z2-D-086, Z2-D-118, Z2-D-119
### Z2-D-090 Every Z2 write names --project Z2 explicitly
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The Vault never maps a category to a project: `create` never validates `--category` (`Tools/Vault/cli/write.mjs:1165-1252`), and nothing reads a category's `project` key. `create`, `sprint` and `version` fall back to `clientFile.project` (`ZEN`) and then `boardRoots[0]` (`write.mjs:1166, 1357, 1454`). A measured simulation filed a Z2 ticket in the `Zenithmon` category as contract-valid.
- **Decision:**
  - Every Z2 write names `--project Z2` wherever the flag vocabulary allows it: `create`, `sprint`, `version`, and the reads `queue`, `blocked`, `report`, `board`, `show`, `check`.
  - The filer refuses to emit any write command that lacks the flag.
  - The Z2 board root is never `boardRoots[0]`, which stays ZEN, the fallback project.
- **Alternatives considered:** (1) Rely on `--category` to select the board — rejected: nothing maps categories to projects. (2) Make Z2 the client default for the session — rejected: it would misfile every other board's writes made in the same checkout.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_FilerProjectFlag, --project Z2, write.mjs, clientFile.project, boardRoots, zagent create, zagent sprint, zagent version, session filer
- **Related:** Z2-D-113, Z2-D-093
### Z2-D-091 create validates the graph, not the contract; the session checker owns the contract
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Create-time validation is a re-scan of a staged copy of the vault plus the workflow-transition check (`Tools/Vault/server/operations.mjs:238-268`): dangling sprint, parent, blocks and fix-version references, duplicate keys, unknown statuses, blocks without a reason and bad hierarchy (`server/index.js:343-448, 491-537`). `create` never calls `evaluateContract`; a `git grep` of `cli/` and `server/` finds no create-time Gates, protected-path or Goal check. The root CLAUDE.md and `TICK.md:640-646` statements that `create` "refuses at FILE time" are therefore wrong.
- **Decision:**
  - A body with a `## Gates` section, no Goal, no DoD checkbox, a protected path in the DoD, an unknown category or a human-only phrase is filed with exit 0 (`write.mjs:1165-1252`).
  - The session coverage checker therefore runs those contract rules on the drafts before filing, and runs `zagent check` on every ticket after filing.
  - It asserts that `contractError` contains only epic, needs-human, blocked and (for PROC tickets) reachability lines — never a spec error. `contractValid` is false for every `needs-human`, blocked or EPIC ticket, so the checker never asserts `contractValid: true`.
  - The filer passes every routing field explicitly, because `create` silently defaults type TASK, priority MEDIUM, complexity MODERATE and risk LOW (`write.mjs:1170-1173`).
- **Alternatives considered:** (1) Trust create-time refusal — rejected: it does not exist. (2) Add contract validation to `create` first — rejected: a Vault PROC, and the seed must work against the Vault as it is.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_FilerContractRules, zagent create, evaluateContract, operations.mjs, contractError, contractValid, zagent check, session coverage checker, Gates heading, reachability, write.mjs
- **Related:** Z2-D-068, Z2-D-114
### Z2-D-092 Not claimable means not queue-claimable
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The programme relied on "every Z2 ticket sits in a PLANNED sprint, so nothing is claimable". But a targeted claim ignores the sprint entirely (`Tools/Vault/cli/read.js:272-276`); measured, `zagent check HE-5` (PLANNED sprint, unblocked) gives `targeted.would: true`, exit 0. A non-EPIC ticket with no sprint is queue-claimable while no sprint of its project is ACTIVE (`compat.mjs:822`).
- **Decision:**
  - The Z2 board's pre-Stage-0 guarantee is "not queue-claimable": `zagent queue --project Z2` lists every To Do row with `claimable: false`.
  - Every non-EPIC Z2 ticket carries a sprint, so none is queue-claimable while no Z2 sprint is ACTIVE.
  - The human is told that `/tick Z2-N` pulls work forward regardless of stage: an unblocked, non-`needs-human` ticket named explicitly starts work early and would then fail at its gates.
- **Alternatives considered:** (1) Treat PLANNED sprints as unclaimable by name — rejected: the code does not, and the measured `check` shows it. (2) Label every ticket `deferred` until its stage — rejected: a label to remove on thousands of tickets, and `deferred` already carries the revalidation meaning (Z2-D-102).
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_QueueNotClaimable, queue-claimable, claimable, targeted claim, zagent check, zagent queue --project Z2, read.js, compat.mjs, PLANNED sprint, /tick Z2-N
- **Related:** Z2-D-116, Z2-D-102
### Z2-D-093 Sprint slugs are derived, never looked up
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `create --sprint` needs the exact slug, a case-sensitive key; anything else is `DANGLING_REFERENCE` and the create exits 4 (`Tools/Vault/server/index.js:405-408`). `sprint create` returns `{id: <slug>}` only on first creation (`write.mjs:1360-1375`), and `sprint list` never shows the slug (`queries.mjs:410-411`).
- **Decision:**
  - The filer computes the slug as `"z2-" + NFKC(name).trim().toLowerCase()`, with each run of `[^a-z0-9]` replaced by `-` and leading/trailing `-` trimmed (`write.mjs:1319-1322, 1366-1371`), and cross-checks it against `Zenith2/Docs/Board/sprints/*.md` frontmatter.
  - Sprint names omit a leading "Z2", to avoid a `z2-z2-` slug: `Stage 0 — Tooling bring-up` … `Stage 7 — Later expansion`, slugs `z2-stage-0-tooling-bring-up` … `z2-stage-7-later-expansion`. Fix versions stay `Z2 Stage 0` … `Z2 Stage 7`.
- **Alternatives considered:** (1) Read the slug from `sprint create`'s output — rejected: absent on re-runs, which a resumable filer performs. (2) Keep the "Z2 S0 — …" names — rejected: slug `z2-z2-s0-…`.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** sprint slug, zagent sprint create, create --sprint, DANGLING_REFERENCE, sprints/*.md, z2-stage-0-tooling-bring-up, z2-stage-7-later-expansion, write.mjs, Z2 Stage 0
- **Related:** Z2-D-113, Z2-D-117
### Z2-D-094 Stage versions are authored into versions.md by A1, with project_id
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `version create` appends rows without `project_id` (`Tools/Vault/cli/write.mjs:1485-1486`), and `version list` resolves owners through `project_id` → `jira.projects` (`queries.mjs:434-436`), so CLI-created rows are invisible. Measured: `version list --project HE` returns `[]` although `Games/Hearth/Docs/Board/versions.md` holds three CLI-created rows.
- **Decision:**
  - A1 writes all eight stage versions (`Z2 Stage 0` … `Z2 Stage 7`) straight into `Zenith2/Docs/Board/versions.md`, one row each: `{id: <minted>, name, status: UNRELEASED, position: N+1, project_id: <the Z2 jira.projects id>, start_date: null, description, release_date: null}`.
  - The filer never calls `version create`. `create --fix-version "<name>"` resolves by name, case-insensitively (`server/index.js:494-506`).
- **Alternatives considered:** (1) `version create` per stage — rejected: `version list --project Z2` would be empty. (2) Fix `version create` first — rejected: a Vault PROC (A0 anchor (t)), not needed for the seed.
- **Consequences:** Keeps `version list --project Z2` truthful. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** versions.md, project_id, zagent version create, version list, --fix-version, Z2 Stage 0, write.mjs, queries.mjs
- **Related:** Z2-D-088, Z2-D-119
### Z2-D-095 Journal recovery is triggered with zagent check, not zagent doctor
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A crash can leave a Vault journal in the `prepared` or `applied` phase, and `ensureBranch` compares the journal's branch to HEAD on every subsequent operation, including lane `integrate` (`Tools/Vault/server/operations.mjs:273-276, 303-305`; `recoverLocked` walks every journal, `:406-417`). `doctor` loads the vault without the lock and never calls `recoverLocked` (`Tools/Vault/cli/client.mjs:636-669`; `local.mjs:19-35`; `lanes.mjs:569-620`). Recovery runs in `executeOperation` (`operations.mjs:437`), `withAuthoritativeIndex` (`:425-432`, used by `check` when writes are enabled, `client.mjs:671-682`) and `executeIntegrate` (`:566`).
- **Decision:**
  - Journal recovery is triggered with `zagent check <any existing key>`, run from the main checkout (it is refused in unregistered worktrees, `lanes.mjs:36-38`). `zagent doctor` is never relied on for recovery.
  - The abandon procedure for the seed branch is: switch to `z2-board-seed`; run `zagent check Z2-1` (or any existing Z2 key) and confirm exit 0 or 4, not a `BRANCH_MOVED` or `RECOVERY_CONFLICT` error (`RECOVERY_CONFLICT` at `operations.mjs:309, 329` is resolved first); only then switch to master and delete the branch.
- **Alternatives considered:** (1) `zagent doctor` — rejected: it takes no lock and runs no recovery, so the journal stays open and deleting the branch wedges every write with `BRANCH_MOVED`. (2) Delete stale journals by hand — rejected: it discards the recovery state the writer relies on.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** zagent check, zagent doctor, recoverLocked, withAuthoritativeIndex, ensureBranch, BRANCH_MOVED, RECOVERY_CONFLICT, Vault journal, operations.mjs, client.mjs, z2-board-seed
- **Related:** Z2-D-096, Z2-D-087
### Z2-D-096 Every tick must be idle during the seed, because board writes follow HEAD
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A lane's own board writes — `claim`, `comment`, `finish`, `update`, `decide` — resolve to the main authority and commit onto whatever branch the main checkout has checked out (`Tools/Vault/server/operations.mjs:492-499`). During the seed that is `z2-board-seed`, so a fired lane's write would be squashed into the board-only commit. Separately, while the main checkout is not on master, lane `integrate` returns `MAIN_NOT_ON_BASE` (`operations.mjs:573-577`; `TICK.md:413-427`) and gives up after `integrateWaitMinutes` (240, `zagent.project.json` `lanes`).
- **Decision:**
  - Every tick, in every lane and in the main checkout, is idle for the whole filing run. There is no pause mechanism, so this means no lane is fired; the concurrent /tick session is told first and is idle.
  - Procedure: (1) all ticks idle; (2) in the main checkout, with a clean tree, `git switch -c z2-board-seed`, branched from master after the docs and config commit (A8a, Z2-D-115); (3) the session filer runs through the normal zagent, every operation through the supported writer, committing on `z2-board-seed`; (4) verify on the seed branch: `zagent doctor` ok, the session coverage checker clean, and `git diff master...z2-board-seed --name-only` touching only `Zenith2/Docs/Board/**` — this path check is the detector for a stray write and is mandatory (Z2-D-101); (5) `git switch master`, `git merge --squash z2-board-seed`, and one commit `board: seed Z2 backlog` whose body carries a line-start `Vault-Operation: z2-seed-<date>` trailer; (6) delete the seed branch, make the coverage commit (A8c), then resume the lanes.
  - Filing is resumable: the filer is keyed on Draft-ID and a re-run skips existing tickets. A broken run leaves its partial state only on `z2-board-seed`; master is untouched. To resume, check out the branch and re-run.
  - To abandon, never delete the branch first: follow Z2-D-095 (`zagent check`, then switch, then delete).
  - No repo file is edited while a Vault write is running. Master never holds a half-filed board.
- **Alternatives considered:** (1) File with lanes running — rejected: their board writes land on the seed branch and `integrate` fails with `MAIN_NOT_ON_BASE`. (2) Rely on the path check alone — rejected: it detects a stray write but cannot remove it from a squashed history; idleness prevents it.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** z2-board-seed, MAIN_NOT_ON_BASE, integrateWaitMinutes, git merge --squash, board: seed Z2 backlog, Vault-Operation, operations.mjs, lanes idle, Draft-ID, resumable filing
- **Related:** Z2-D-069, Z2-D-087, Z2-D-095, Z2-D-101, Z2-D-115
### Z2-D-097 Z2 rulings are appended to Zenith2/Docs/DecisionLog.md in the ticket's own diff
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The programme assumed `zagent decide` records decisions into the decision log. It does not: it appends a row to `metadata.md`'s decisions database, a knowledge record, and commits `board: decide` (`Tools/Vault/cli/write.mjs:1817-1858, 1883-1903`). Nothing writes the `decisionLog` file; it is read by the rulings scan (`client.mjs:519-531`) and the review term (`compat.mjs:167-195, 704-713`). Its ID minting uses max(ticket-body citations, database rows) + 1 and ignores `DecisionLog.md` (`write.mjs:1734-1792`).
- **Decision:**
  - A Z2 ruling is appended to `Zenith2/Docs/DecisionLog.md` in the diff of the ticket that makes it, as a `### Z2-D-NNN <Title>` entry in the entry template, using the next free ID in that file. That file is what the rulings scan and the review term read.
  - Where `zagent decide` is used at all, it is an optional knowledge-row mirror and carries an explicit `"ID": "Z2-D-NNN"` equal to the appended entry's (`write.mjs:1787`), or its minted ID would collide with the log. It is never the record.
  - The seed filer never calls `decide`: it writes `metadata.md`, a path outside `Zenith2/Docs/Board/**`.
- **Alternatives considered:** (1) Record rulings with `zagent decide` — rejected: the rulings scan never sees them. (2) Append to the log from a separate documentation ticket — rejected: the ruling would reach workers only after that ticket lands.
- **Consequences:** "How this log is written" in this file describes the mechanism. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** DecisionLog.md, zagent decide, metadata.md, decisionLog, rulings scan, next free ID, write.mjs, client.mjs, compat.mjs, board: decide
- **Related:** Z2-D-085, Z2-D-084, Z2-D-022, Z2-D-098
### Z2-D-098 Governing decision IDs are cited inside Goal or DoD text
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** At claim the client writes candidate rulings to `.zagent/run/<KEY>/rulings.md` from the category's `decisionLog` (`Tools/Vault/cli/client.mjs:459-531`) through `matchGoverningRulings` (`legacy-pure/ticket.mjs:353-390`), which reads only the Goal and DoD (`client.mjs:484-489`; `ticket.mjs:135-142`). Identifiers are only tokens matching `IDENTIFIER_SCAN_RE` (`ticket.mjs:224`: a `Prefix_Name` token or a PascalCase word with two or more capitals); each shared identifier scores 2 (`IDENTIFIER_WEIGHT`), a direct `Z2-D-NNN` citation scores 2 once (`:367`), and at most the 3 rarest case-folded nouns of six letters or more add their rarity in (0, 1] (`:244-255`). A block is selected at 4 (`RULINGS_FILE_MIN_SCORE`, `:202-204`); the review's third term uses 8. Measured against this file, "Implement this under Z2-D-090." selects nothing, and neither do two citations with their prose nouns; a DoD naming `Zenith_Owned` next to its citation selects the entry.
- **Decision:**
  - Every Z2 ticket cites each governing `Z2-D-NNN` inside its Goal or DoD text, **and** names in the same text at least one identifier from that entry's **Nouns** line — or, where the entry has none the ticket would use, at least two of its distinctive words that occur in no other block of the file (`Related:` lines, open-decision entries and the "How this log is written" section included, because rarity is counted over blocks). A citation alone scores 2 and never selects the entry.
  - The `Decisions:` line stays for humans and the coverage checker but is not relied on for matching.
  - Every entry in `Zenith2/Docs/DecisionLog.md` carries at least one `IDENTIFIER_SCAN_RE` identifier on its **Nouns** line, because prose nouns alone (at most 3 points) and a citation alone (2 points) never reach the threshold.
  - Weighting a direct citation at the threshold is a Stage 0 PROC candidate, noted under "Open decisions"; until it lands, the two-part rule stands.
- **Alternatives considered:** (1) Rely on the citation alone — rejected: it scores 2 against a threshold of 4. (2) Rely on the `Decisions:` line — rejected: it is not scanned. (3) Rely on prose nouns — rejected: they cannot reach 4 without an identifier.
- **Consequences:** "Heading and noun rule" in this file states the scoring. The session coverage checker verifies the two-part rule on every draft. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** IDENTIFIER_SCAN_RE, IDENTIFIER_WEIGHT, RULINGS_FILE_MIN_SCORE, matchGoverningRulings, rulings.md, ticket.mjs, Decisions line, Definition of Done, identifier hit, citation
- **Related:** Z2-D-068, Z2-D-097
### Z2-D-099 Ticket-body phrase rules
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The Vault's section parser splits at any `##`/`###`, and a subsequent same-named section overwrites an earlier one (`Tools/Vault/cli/legacy-pure/ticket.mjs:68-87`), while `build_prompt.ps1` slices up to the next heading of the same or higher level (`:172-206`). A `Gates` heading becomes ticket gates that replace the category's (`ticket.mjs:31-33`; `routing.mjs:79-81`). `boardRefWarning` scans DoD items (`ticket.mjs:391-450`) and always fires on link phrases, because `linkedOf` ignores frontmatter links while `metadata.md` holds `jira.issue_links` rows (`compat.mjs:538-558`). `markerWarning` scans the whole spec for human-only phrases (`ticket.mjs:451-486`).
- **Decision:**
  - `##` is used for the ten template sections; there is no `##` or `###` inside Goal or DoD, and no heading named `Gates` at any level.
  - DoD items are `- [ ] text` with indented continuation lines; at least one checkbox.
  - No "the blocker", and no "the blocking/linked/related/parent X" in the DoD.
  - No human-only phrase from the `ticket.mjs:451-471` list (human/manual/visual sign-off or approval; "no agent may/can …"; "a human/person must …"; "human-executed/-authored/-only"; "needs a human"; "only a human can"; "must be done/run/authored by a human") outside `needs-human` tickets.
  - No protected path or glob in the DoD of a non-PROC ticket; that is a `reachabilityError`, i.e. contract-invalid.
  - No title, reason or goal begins with `--` (it parses as a flag, `legacy-pure/args.mjs:11-17`).
- **Alternatives considered:** (1) Free-form bodies checked by review — rejected: the parser and scanners act mechanically. (2) Fix the scanners first — rejected: Vault PROCs; the seed must work against the Vault as it is.
- **Consequences:** The session coverage checker enforces these on drafts. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** ticket.mjs, boardRefWarning, markerWarning, reachabilityError, Gates heading, DoD checkbox, linkedOf, build_prompt.ps1, args.mjs
- **Related:** Z2-D-068, Z2-D-091
### Z2-D-100 No tracked Z2 text may match the Vault grep guards
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The Tooling gate's `npm test` runs two grep-guard tests over every tracked file (both under `Tools/Vault/test/`). The first greps case-insensitively for the four-letter abbreviation of "software as a service", exempting only `Middleware/`, `Zenith/RenderBackends/Vulkan/`, `metadata.md` and board paths (its lines 33, 93-98, 105). The second (`legacy-backend-guard.test.mjs:29, 38, 46, 74`) greps for the word "legacy" followed by a space and "backend" or "zagent", and for the two `ZAGENT` environment-variable names ending `_URL` and `_TOKEN`, with no board exemption. A multisample-count identifier that joins the antialiasing abbreviation to "Samples" contains the first guard's substring. This entry spells the patterns indirectly so that it does not trip them itself.
- **Decision:**
  - No tracked Z2 text — docs, board tickets and all subsequent sources — matches either guard: no case-insensitive occurrence of the software-as-a-service abbreviation, no "legacy" + space + "backend"/"zagent" phrase, and neither `ZAGENT` variable name.
  - Z2 naming uses `SampleCount` or `MultisampleCount` for multisample counts.
  - Before the docs and config commit the check `grep -rniE "s[a]as|legacy[ ]backend|legacy[ ]zagent|ZAGENT[_]URL|ZAGENT[_]TOKEN" Zenith2/` is clean (the bracketed classes match the literal patterns without containing them).
  - The rule stands until a generic PROC (A0 anchor (q)) fixes the guards.
- **Alternatives considered:** (1) Exempt `Zenith2/` in the guards — rejected: a protected Vault test edit, and it names `Zenith2/` on the Zenith side. (2) Accept the red — rejected: it reds every ZEN Tooling ticket.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** legacy-backend-guard.test.mjs, Tools/Vault/test, git grep, npm test, Tooling gate, SampleCount, MultisampleCount, grep guard
- **Related:** Z2-D-121
### Z2-D-101 The seed commit's board-only status rests on its message
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `integrate` classifies a commit as board-only when its subject starts `board: ` and its body has a line starting `Vault-Operation:`; paths are never inspected (`Tools/Vault/server/operations.mjs:583-593`).
- **Decision:**
  - The seed commit `board: seed Z2 backlog` carries the `Vault-Operation: z2-seed-<date>` trailer at line start in its body, so lane `integrate` treats it as board-only.
  - Because classification is by message alone, the honesty of that label comes from the path check before the squash (`git diff master...z2-board-seed --name-only` touching only `Zenith2/Docs/Board/**`), which is mandatory, not advisory (Z2-D-096).
- **Alternatives considered:** (1) Trust the message alone — rejected: a stray non-board path would be classified board-only and skip regating. (2) A path-inspecting classifier — rejected: a Vault change, not needed if the path check is mandatory.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** MAIN_NOT_ON_BASE, Test_SeedPathCheck, board-only commit, Vault-Operation, board: seed Z2 backlog, operations.mjs, zagent integrate, git diff master...z2-board-seed
- **Related:** Z2-D-096, Z2-D-115
### Z2-D-102 needs-revalidation is paired with deferred
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Only `needs-human` (contract, `Tools/Vault/cli/contract.mjs:38`), `deferred` (queue, `compat.mjs:805, 866`), `human-gate` (queue, `:806`), EPIC, assignee and `blocks` links affect claiming (`compat.mjs:433-446, 826-869`; `read.js:247-299`). `needs-revalidation` and `Spec-Maturity` are inert.
- **Decision:**
  - Until a Vault PROC adds a claim predicate for it, `File-Backlog.ps1 --mark-stale` applies `needs-revalidation` and `deferred` together, and the revalidation pass (reconcile mode) removes both.
  - `deferred` is queue-skipped and still targetable, so a stale ticket leaves the queue but can be revalidated by name.
- **Alternatives considered:** (1) `needs-revalidation` alone — rejected: it blocks nothing. (2) Pair with `needs-human` — rejected: it would also bar the loop from the ticket after revalidation until someone removes a second meaning.
- **Consequences:** Pending the human's answer to open question Q5 in `OPEN_QUESTIONS_FOR_HUMAN.md`. Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_MarkStaleDeferred, needs-revalidation, deferred, File-Backlog.ps1 --mark-stale, Spec-Maturity, contract.mjs, compat.mjs, claim predicate, reconcile mode
- **Related:** Z2-D-068, Z2-D-129, Z2-D-092
### Z2-D-103 PROC tickets are closed by a human actor
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A `needs-human` ticket is refused by any agent claim (`Tools/Vault/cli/contract.mjs:38`; `read.js:272-280`; measured, DP-10 gives `contractValid: false`, `needsHuman: true`).
- **Decision:**
  - PROC and other `needs-human` tickets are closed by a human actor: `zagent move <KEY> "In Progress" --human` then `zagent move <KEY> Done --human`, or `zagent finish <KEY> --human` with the evidence files.
  - The global `(null)→Done` edge of the workflow copied from FD allows To Do → Done directly, so the Z2 `workflow.md` copies FD's five statuses and ten transitions exactly.
- **Alternatives considered:** (1) Let the loop close PROC tickets — rejected: the contract refuses the claim. (2) A Z2-specific workflow — rejected: `finish Done|Blocked` and `move … Blocked` rely on FD's global edges.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_HumanActorClose, zagent move --human, zagent finish --human, needs-human, contract.mjs, workflow.md, global Done edge, PROC ticket
- **Related:** Z2-D-126, Z2-D-128
### Z2-D-104 conventionDocs needs no Z2 change
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `conventionDocs` (`zagent.project.json:573-583`) holds only a title and icon, with no enumeration and no test; `docsMirrorNeeded` is disabled when writes are enabled (`Tools/Vault/cli/local.mjs:6`).
- **Decision:** `Zenith2/CLAUDE.md` is mirrored nowhere and fails nothing, so no `conventionDocs` or allowlist entry is added for it. The Z2 conventions string names `Zenith2/CLAUDE.md` and `Zenith2/Docs/WorkerGuide.md` directly (Z2-D-086).
- **Alternatives considered:** (1) Add a `conventionDocs` entry — rejected: nothing reads it. (2) Allowlist `Zenith2/CLAUDE.md` in a docs mirror — rejected: no mirror runs.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_ConventionDocsInert, conventionDocs, zagent.project.json, docsMirrorNeeded, local.mjs, Zenith2/CLAUDE.md
- **Related:** Z2-D-030, Z2-D-119
### Z2-D-105 External mode also skips the .inl proof, and never exits 10 outside run_unit_gate lines
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Besides the Sharpmake line classification, created-file regen and `.obj` proof, `tick_gates.ps1` has two more Zenith-specific measure steps: the `.inl` include proof (`Tools/tick_gates.ps1:1041-1060`) and unit-count measurement with the pin-bump exit 10 (`:1067-1127`). Its exe sweep takes names from root `Games/*` (`:310-321`).
- **Decision:**
  - The generic external-build mode skips the `.inl` include proof; for Z2 the `CompileCoverage` step owns that check.
  - External mode never exits 10 except for `run_unit_gate.ps1` lines; Z2 has no pin row in `Tools/unit_baselines.json`.
  - External mode uses step-owned exe sweeps, not `Invoke-ExeSweep`'s root-game names.
- **Alternatives considered:** (1) Keep the `.inl` proof in external mode — rejected: it resolves includes through Sharpmake projects Z2 does not have. (2) Give Z2 a unit pin — rejected: Z2 tests are judged pass or fail only (Z2-D-042).
- **Consequences:** Part of the Stage 0 external-mode PROC ticket. Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** tick_gates.ps1, external-build mode, .inl include proof, exit 10, run_unit_gate.ps1, unit_baselines.json, CompileCoverage, Invoke-ExeSweep
- **Related:** Z2-D-032, Z2-D-117
### Z2-D-106 The mixed-union refusal must allow declared cross-tooling PROC diffs
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `effectiveGates` unions across every category whose `paths` a diff touches (`Tools/Vault/cli/legacy-pure/routing.mjs:102-121`). Measured: a Z2 diff touching `Tools/tick_gates.ps1` and `Zenith/**` unions `Engine` and `Tooling`. No mixed-union check exists today.
- **Decision:**
  - A Z2 PROC diff that edits `Tools/*.ps1` or `Zenith/**` unions Sharpmake-based gates by design, and that is legitimate.
  - The mixed-union refusal (exit 5) the external-mode PROC adds applies only when the ticket's own category is external and the ticket did not opt in to the cross-tooling union. The opt-in's shape is open question Q7 in `OPEN_QUESTIONS_FOR_HUMAN.md`.
- **Alternatives considered:** (1) Refuse every mixed union — rejected: it refuses every PROC ticket that edits shared tooling. (2) No refusal — rejected: an ordinary Z2 ticket that accidentally touches Zenith paths would run a Sharpmake build inside an external phase.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** mixed-union refusal, effectiveGates, routing.mjs, exit 5, buildSystem external, cross-tooling PROC diff, tick_gates.ps1
- **Related:** Z2-D-044, Z2-D-117
### Z2-D-107 supportingFiles lists only stable Z2 files
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Only `metadata.md` is an optional supporting file (`Tools/Vault/server/scanner.js:264`); a missing listed file gives `FILE_READ_ERROR` (`:624-634`), `index.valid = false` (`server/index.js:441-443`), and then every claim in every project is refused ("Vault is invalid", `read.js:107-113, 272`).
- **Decision:**
  - `supportingFiles` in `vault.config.json` lists only Z2 files no loop ticket will rename or delete; listing none is acceptable. `board status --project Z2 --roadmap Zenith2/Docs/Roadmap.md` needs the Roadmap listed.
  - A listed Z2 file is renamed or deleted only by a PROC ticket that edits `vault.config.json` in the same commit.
- **Alternatives considered:** (1) List every Z2 doc — rejected: a routine doc rename would take down all boards. (2) List none and forgo `board status` — acceptable fallback, pending open question Q6.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** supportingFiles, vault.config.json, scanner.js, FILE_READ_ERROR, index.valid, board status --roadmap, Roadmap.md
- **Related:** Z2-D-119
### Z2-D-108 The Z2 board is byte-exact under a -text attribute
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Every other board path is `-text` (`.gitattributes:189-190`, `Zenith/Docs/Board/** -text`, `Games/*/Docs/Board/** -text`). Measured with `git check-attr`: the Z2 board would get `text=auto` under `core.autocrlf=true`, so its files would be CRLF in the working tree, not the byte-exact LF the writer produces.
- **Decision:**
  - A nested `Zenith2/.gitattributes` carries `Docs/Board/** -text`, restoring parity with the other boards without naming `Zenith2/` outside `Zenith2/`.
  - Z2-specific LFS rules, if any, go in the same file; the root LFS rules are extension-global and already cover `Zenith2/` (`.gitattributes:111-128, 171-175`).
  - It is part of A1, applied with the human's approval (open question Q2).
- **Alternatives considered:** (1) A root `.gitattributes` rule — rejected: it names `Zenith2/` on the Zenith side. (2) Leave `text=auto` — rejected: CRLF board files break byte-exact body hashes and diverge from every other board.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_BoardByteExact, Zenith2/.gitattributes, -text, Docs/Board, text=auto, core.autocrlf, git check-attr, LFS rules
- **Related:** Z2-D-119
### Z2-D-109 Leaves are TASK, not STORY, unless the Roadmap lists them
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `board status` reports every EPIC or STORY key missing from the Roadmap as drift and exits 1 (`Tools/Vault/cli/local.mjs:51-72`).
- **Decision:**
  - Leaf tickets are `TASK` (or `BUG`/`SUBTASK` where apt), never `STORY`. `EPIC` is used only for the 41 area epics, `Z2 <CODE> — <Area name>`, which `Zenith2/Docs/Roadmap.md` lists in `[Z2-N]` form.
  - This keeps `board status --project Z2` a usable check without listing thousands of keys.
- **Alternatives considered:** (1) STORY leaves listed in the Roadmap — rejected: thousands of keys in a doc and drift on every filing. (2) STORY leaves and ignore `board status` — rejected: loses the check.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_BoardStatusDrift, TASK, STORY, EPIC, board status, local.mjs, --type TASK, area epic, leaf ticket, Roadmap drift, thousands of keys
- **Related:** Z2-D-068
### Z2-D-110 Filing throughput is measured, not assumed, and journals are part of the cost
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Each write does at least three full vault scans plus a staged copy (`Tools/Vault/server/operations.mjs:238-268, 434-502`), writes all vault files to `%TEMP%` for validation, and re-reads every journal (`recoverLocked`, `:406-417`): 1,083 files and 212 MB today, 0.4 s measured. Measured reads: `loadVault` 430–580 ms, `doctor` 1.98 s, `show` 1.36–1.38 s, `check` 3.17 s, `queue` 2.05 s; inferred about 4–6 s per write, rising with ticket and journal count. The lanes must stay idle for the whole window (Z2-D-096), so the window must be known before filing starts.
- **Decision:**
  - Before filing, a dry run files 50 tickets, with links, sprint membership, versions and gates, on a scratch branch that is then deleted following Z2-D-095.
  - It records seconds per operation at N=50 and fits a line against the vault file count; the filing window is budgeted from that fit, not from the 1–3 s estimate.
  - It confirms the squash procedure end to end.
  - Whether complete journals are archived first is open question Q3 in `OPEN_QUESTIONS_FOR_HUMAN.md`.
  - Amended 2026-10-16 (the human's ruling on that question, recorded as Q2 (a) in `C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md`, not a new decision): before A6 the human archives the complete journals of `.git/vault/operations` once, by hand, following the exact procedure the orchestrator supplies (only `--operation-id` replay of the archived IDs is forfeited; recovery is unaffected), and the dry run measures the filing rate after the archival.
- **Alternatives considered:** (1) Estimate from the index size — rejected: journal re-reads dominate and grow per operation. (2) Start filing and extrapolate — rejected: the idle window must be agreed first.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_FilingDryRun, dry run, filing throughput, recoverLocked, loadVault, operations.mjs, Vault journal, seconds per operation, scratch branch
- **Related:** Z2-D-095, Z2-D-096
### Z2-D-111 The filer validates --file paths and body hashes
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `create`'s `--file` is lenient: a path that does not exist becomes the literal body text (`Tools/Vault/cli/client.mjs:280-288`), so a ticket is filed whose body is the path string.
- **Decision:**
  - The filer checks that every `create --file` path exists before the create.
  - After every create it compares the SHA-256 of `zagent show --json`'s `body` with the draft's and stops on a mismatch.
  - Draft files are written LF, because the body bytes follow the frontmatter verbatim.
- **Alternatives considered:** (1) Inline bodies with `--text` — rejected: the Windows command-line length limit. (2) Trust the create exit code — rejected: exit 0 with the path as the body.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_FilerBodyHash, create --file, client.mjs, zagent show --json, SHA-256 body hash, session filer, Test-Path
- **Related:** Z2-D-113
### Z2-D-112 No cross-project links from Z2
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `--blocked-by` and `link` patch the source ticket's file (`Tools/Vault/cli/write.mjs:1239-1245, 1271-1298`). A cross-project edge would edit another project's board file inside the Z2 seed.
- **Decision:**
  - No Z2 ticket is linked to or from a ticket of another project, with any link verb.
  - This keeps the seed diff `Zenith2/Docs/Board/**`-only and the other projects unaware of Zenith2.
- **Alternatives considered:** (1) Allow links from Z2 to ZEN tickets — rejected: it edits ZEN's board inside the seed and breaks the path check. (2) Record cross-project dependencies as links added after the seed — rejected: still names Zenith2 work on another board.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_NoCrossProjectLinks, cross-project link, --blocked-by, zagent link, write.mjs, Docs/Board, blocks
- **Related:** Z2-D-113, Z2-D-101
### Z2-D-113 The session filer files in topological order with create-time routing and per-reason links
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The production filer with reconcile mode is itself a Stage 0 ticket, so the seed backlog is filed by a session tool. `create` applies one `--reason` to every `--blocked-by`, and a Windows command line cannot carry a full ticket body.
- **Decision:**
  - The session filer's code and state live in `C:/dev/Zenith2-drafts/filer/`, durable.
  - It creates sprints and versions first, then files tickets in topological order, so `--blocked-by … --reason`, `--sprint`, `--fix-version`, `--parent` and `--label` are all passed at create time. Keys are predictable (max + 1).
  - A ticket whose blockers need distinct reasons is created with no blockers, and the filer adds each with `zagent link … blocks … --reason` right after the create.
  - A second pass does only `zagent sprint gate`, designating each stage's acceptance ticket.
  - It is keyed on Draft-ID, so a re-run skips existing tickets. Bodies are passed with `create --file <path>`, never inline.
  - It implements the production filing rules minus reconcile mode and the fixtures; the production `File-Backlog.ps1` with its two passes and reconcile mode is the Stage 0 version.
  - `Zenith2/Docs/Board/labels.md` gets rows for `needs-human`, `needs-gpu`, the three `gpu-reason:*` labels Z2 uses (`gpu-reason:render-capture`, `gpu-reason:visual-measure`, `gpu-reason:requires-graphics-test`; `gpu-reason:legacy-scene-reauthor` is Zenith-specific and not listed), `deferred`, `api-change`, `blocked-scope`, `integration-conflict`, `needs-revalidation`, `human-gate` and `approved`, each with `project_id` set to the Z2 `jira.projects` id; the CLI does not validate labels, so the rows are for the UI and for provenance.
  - Every write names `--project Z2` (Z2-D-090); sprint slugs are derived, never looked up (Z2-D-093); stage versions come from `versions.md`, never `version create` (Z2-D-094); `decide` is never called (Z2-D-097); every `--file` path and body hash is checked (Z2-D-111); no link crosses projects (Z2-D-112).
- **Alternatives considered:** (1) File in arbitrary order and link in a second pass — rejected: more operations, each reloading the vault. (2) Build the production filer first — rejected: it is a Stage 0 ticket, and the backlog must exist before Stage 0 runs (Z2-D-033).
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_FilerProjectFlag, Test_FilerTopologicalOrder, session filer, Zenith2-drafts/filer, topological order, create-time routing, blocked-by, reason, zagent link, zagent sprint gate, Draft-ID, create --file, labels.md, File-Backlog.ps1
- **Related:** Z2-D-068, Z2-D-069, Z2-D-090, Z2-D-093, Z2-D-094, Z2-D-111, Z2-D-112, Z2-D-114
### Z2-D-114 The session coverage checker implements the coverage rules and the normative grammar, and must be clean
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The production `Coverage.ps1` is a Stage 0 ticket, but the seed board must already satisfy its rules, or the coverage gate step would be red on its first run.
- **Decision:**
  - A session coverage checker, in `C:/dev/Zenith2-drafts/checker/`, implements the production coverage rules (Z2-D-067) and the normative-grammar scan (Z2-D-066) against the filed board and the docs, minus the failing fixtures.
  - It runs the contract rules `create` does not (Z2-D-091) on every draft before filing, then `zagent check` on every filed ticket, asserting that `contractError` holds only epic, needs-human, blocked and (for PROC tickets) reachability lines.
  - It is clean before filing finishes: every inventory item mapped, every required or deferred requirement with leaf and integration tickets, every cited (path, heading) pair resolving to exactly one heading, every routing field present, no dangling links or `blocks` cycles, and no normative keyword outside a tagged bullet or informative block.
  - At the end of filing it generates `Zenith2/Docs/Coverage.md` (requirement coverage), outside Vault, left uncommitted on the seed branch so the seed diff stays `Docs/Board/**`-only.
- **Alternatives considered:** (1) Trust the filer without a checker — rejected: the coverage gate step would find the defects on its first run. (2) Build the production checker first — rejected: a Stage 0 ticket.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Acceptance evidence"`.
- **Nouns:** Test_CoverageFixtures, Test_NormativeGrammar, session coverage checker, Zenith2-drafts/checker, Coverage.md, coverage rules, normative grammar, cite resolution, heading resolution
- **Related:** Z2-D-066, Z2-D-067, Z2-D-091, Z2-D-115
### Z2-D-115 Exactly three commits land on master: docs and config, board seed, coverage report
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A non-board-only commit on master regates every in-flight lane ticket once, and a board-only commit does not, so the session's commits are shaped to keep regates to a minimum and the board seed honestly board-only.
- **Decision:**
  - Exactly three commits land on master, each needing the human's approval, and all the tickets are in one of them.
  - A8a, before filing: the docs and config commit, covering `Zenith2/**` except `Docs/Board/tickets`, plus `vault.config.json`, `zagent.project.json` and the `metadata.md` `jira.projects` row (Z2-D-088), which the human approved on 2026-10-16 (ruling Q1 (a)) and makes as a human edit in this commit. It is a normal commit, not board-only, so any in-flight lane tickets regate once; it is made while the lanes are idle. The seed branch is cut from it, so filing sees the registered board and categories.
  - A8b, after filing: the board seed commit `board: seed Z2 backlog` (Z2-D-096), containing every ticket and nothing else.
  - A8c: the coverage report commit. `Zenith2/Docs/Coverage.md` needs the real ticket keys, so it is generated only after the seed; it is not a board file, so it gets its own small commit, which keeps A8b honestly board-only. Like A8a it is not board-only, so it is made before the lanes resume.
- **Alternatives considered:** (1) Commit docs and board together — rejected: the board seed would not be board-only, and filing needs the registered categories before it starts. (2) Commit Coverage.md inside the seed — rejected: the seed diff would not be `Docs/Board/**`-only.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_SeedPathCheck, A8a, A8b, A8c, docs and config commit, board seed commit, coverage report commit, board-only commit, regate, Coverage.md, commit approval
- **Related:** Z2-D-069, Z2-D-096, Z2-D-121
### Z2-D-116 The whole backlog is filed before the scaffold; every sprint stays PLANNED and a human starts Stage 0
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The session delivers exactly two things: extremely detailed documentation under `Zenith2/`, and a fully detailed Z2 board with a ticket for every step from the first PROC infrastructure change through Stage 7. Everything else is a ticket.
- **Decision:**
  - The whole backlog is filed before the scaffold ticket runs (Z2-D-033); nothing beyond the docs, the board registration and the board itself is implemented by the session.
  - Implemented by tickets, not by the session: the generic `tick_gates.ps1` external-build mode and `phaseTimeoutMinutes`; the Vault pass-through, `reviewPaths` and `zagent review record`; the `TICK.md` 6.3 edit and harness regeneration; `include_roots` and the scanner audit; the root CLAUDE.md delegation line; `zenith2-gates.yml`; every Z2 gate step script and `Run-Committed.ps1`; the production coverage checker with its fixtures and the production filer with reconcile mode; the scaffold and all engine code.
  - Board state at the end of the session: every sprint (Stages 0–7) PLANNED and none started; Stage 0 a closed DAG; every subsequent ticket filed in full detail and blocked as its decisions and budget rows require; `zagent queue --project Z2` lists every To Do row with `claimable: false` and a blocked or out-of-stage reason, so no row is queue-claimable (Z2-D-092); a human starts Stage 0.
- **Alternatives considered:** (1) Start Stage 0 inside the session — rejected: the PROC tickets are `needs-human` and edit protected paths. (2) File only Stage 0 and 1 — rejected: Z2-D-070 requires every stage in full.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** Test_QueueNotClaimable, session deliverables, backlog before scaffold, PLANNED sprint, closed DAG, zagent queue --project Z2, claimable, Stage 0 start
- **Related:** Z2-D-033, Z2-D-070, Z2-D-092, Z2-D-128
### Z2-D-117 The Z2 categories are registered before the tools that read their keys
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `create --category` requires the category to exist, so the Z2 categories must be registered before filing — but the tooling that reads their Zenith2-specific keys is itself Stage 0 work.
- **Decision:**
  - The Z2 categories are added to `zagent.project.json` in board registration, before the Stage 0 tooling exists.
  - Unknown keys (`buildSystem`, `measureSteps`, `phaseTimeoutMinutes`, `reviewPaths`) are silently ignored until the Stage 0 tooling reads them; that was verified against `resolveRules` and the overlay.
  - Every gate line starts `pwsh -NoProfile`, because `Test-HarnessParity` requires a pre-authorised prefix.
  - Gate lines name step scripts that do not exist yet. That is harmless: every Z2 ticket sits in a PLANNED sprint, so nothing is claimable until a human starts Stage 0.
- **Alternatives considered:** (1) Register the categories only after the tooling lands — rejected: tickets cannot be filed without them. (2) Register minimal categories and extend them by PROC ticket — rejected: every category would change after filing, and the gate lists are the source of truth (Z2-D-032).
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Z2 categories, zagent.project.json, unknown keys, buildSystem, measureSteps, phaseTimeoutMinutes, reviewPaths, resolveRules, pwsh -NoProfile, Test-HarnessParity, create --category
- **Related:** Z2-D-032, Z2-D-086, Z2-D-119
### Z2-D-118 No guardrail additions in this session; the first Stage 0 PROC ticket adds them
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Changing the protected-path list changes the worker charter's `{{PROTECTED_PATHS}}` substitution, which forces a three-harness regeneration, and there is nothing under `Zenith2/Tools/Gates` to protect yet. `guardrailsOf` resolves protected paths as a fallback chain, not a union.
- **Decision:**
  - The session adds no guardrails.
  - The first Stage 0 PROC ticket adds `Zenith2/Tools/Gates/**` and `Zenith2/CLAUDE.md` to the global guardrail lists, with `Build-HarnessArtifacts.ps1 -Harness claude|opencode|codex` regeneration and `-Check`. `.github/**` is already protected, which covers `zenith2-gates.yml`.
  - `policy.resolved.projects.Z2` in `vault.config.json` carries no per-project `protectedPaths`, because `guardrailsOf` is a fallback chain and a project-level list would replace the global one (Z2-D-089).
  - The guardrail PROC edits both `policy.board.guardrails.protectedPaths` and `policy.resolved.guardrails.protectedPaths` in `vault.config.json`, because enforcement reads the resolved list (`Tools/Vault/cli/local.mjs:82, 99`; `contract.mjs:39-42`) while `{{PROTECTED_PATHS}}` is filled from the same resolved list (`Build-HarnessArtifacts.ps1:218-243`).
- **Alternatives considered:** (1) Add the guardrails during board registration — rejected: forces a harness regeneration for paths that do not exist yet. (2) A Z2 project-level `protectedPaths` list — rejected: it would replace, not extend, the global list.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** guardrail, protectedPaths, PROTECTED_PATHS, guardrailsOf, fallback chain, Build-HarnessArtifacts.ps1, harness regeneration, Zenith2/Tools/Gates, Zenith2/CLAUDE.md, vault.config.json
- **Related:** Z2-D-089, Z2-D-119, Z2-D-128
### Z2-D-119 Board registration edits vault.config.json, zagent.project.json, one metadata.md row and Zenith2 board files
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Existing projects stay unaware of Zenith2, so the session's footprint outside `Zenith2/` is limited to the files without which the board cannot exist.
- **Decision:**
  - The files outside `Zenith2/` the session edits are `vault.config.json` (the board root `{Z2, Zenith2/Docs/Board}`, `policy.resolved.projects.Z2` with `repo`, `baseBranch`, `branching`, `merge`, `reviewOn` and `workerTools`, and `supportingFiles` listed after Zenithmon's) `zagent.project.json` (the Z2 categories), and one `jira.projects` row in `metadata.md` (Z2-D-088), approved by the human on 2026-10-16 (ruling Q1 (a)) and made by the human as a hand edit in A8a. The row contains no `Zenith2/` path, so the blindness grep still matches only the two config files. The Z2 board root is not `boardRoots[0]` (Z2-D-090), and the project entry carries only the keys of Z2-D-089.
  - Inside `Zenith2/`: `Zenith2/Docs/Board/workflow.md`, generated fresh, modelled on FD's structure but with Z2's own `project_id` and status IDs, verified by `zagent doctor`; `labels.md` and `versions.md`, written fresh, the stage versions authored with `project_id` (Z2-D-094); the nested `Zenith2/.gitattributes` carrying `Docs/Board/** -text` (Z2-D-108); and the nested `Zenith2/.gitignore`, which explicitly ignores `out/` so untracked step output never changes `treeShaEnd` (a fixture test runs a full verify and asserts `treeShaStart == treeShaEnd`). Docs live outside `Board/`.
  - Before registration: the tree is clean, `Dawnmere.zscen` is never touched, and the concurrent /tick session is idle. After it, the blindness checks run: `Test-ZagentClient.ps1`, `Test-HarnessParity.ps1`, the Vault tests and `zagent doctor` pass; `zagent gates` output for existing ZEN and ZM tickets is unchanged; `git grep -nE '\bZenith2/'` outside `Zenith2/` matches only `vault.config.json` and `zagent.project.json`.
- **Alternatives considered:** (1) Also land the delegation line and CI workflow in the session — rejected: those are protected Stage 0 PROC edits. (2) Copy FD's `workflow.md` — rejected: it carries FD's project and status IDs.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** HarnessParity, ZagentClient, board registration, vault.config.json, zagent.project.json, policy.resolved.projects.Z2, supportingFiles, workflow.md, labels.md, versions.md, Zenith2/.gitignore, out/, treeShaEnd, blindness check, Dawnmere.zscen
- **Related:** Z2-D-030, Z2-D-088, Z2-D-089, Z2-D-094, Z2-D-107, Z2-D-108, Z2-D-117, Z2-D-118
### Z2-D-120 The /tick protocol is re-verified read-only before board registration and its findings are recorded
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The Stage 0 PROC tickets modify `tick_gates.ps1`, the Vault writer and lane tooling, all of which change often. Tickets specified against a remembered version of that code would be wrong on arrival.
- **Decision:**
  - Before board registration, the /tick protocol is re-verified as it is now, read-only: lane mode (`TICK.md#lane-mode`, `Tools/zagent/README.md#lanes`, `build_lock.ps1`, `lane_init.ps1`, `lane_pins.ps1`), `tick_gates.ps1`, `integrate.mjs` and `write.mjs` (the `create` flags, `sprint`, `version`, `link`).
  - Findings are recorded as decisions: they are the session rulings Z2-D-087 to Z2-D-112. The Stage 0 PROC tickets are specified against this current code.
- **Alternatives considered:** (1) Specify PROC tickets from documentation alone — rejected: docs lag the code. (2) Verify during Stage 0 — rejected: the tickets are filed in full detail before Stage 0 starts.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** tick protocol verification, lane mode, build_lock.ps1, lane_init.ps1, lane_pins.ps1, tick_gates.ps1, integrate.mjs, write.mjs, PROC ticket specification
- **Related:** Z2-D-118, Z2-D-128
### Z2-D-121 Documentation is written straight into Zenith2/Docs and stays uncommitted until the docs and config commit
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Docs drafted outside the repo would have to be copied in, and copies drift. Committing docs piecemeal would regate lane tickets on every commit.
- **Decision:**
  - The core docs (`Zenith2/CLAUDE.md` at most 150 lines; `Docs/WorkerGuide.md` under 300 lines; `Architecture.md`, `Contracts.md`, `Rendering.md`, `Scripting.md`, `Budgets.md` v1, `Roadmap.md`, `DecisionLog.md` and `Requirements/Inventory.md`) and every per-area design doc and requirement catalogue are written straight into `Zenith2/Docs/`.
  - They stay uncommitted until the docs and config commit (A8a, Z2-D-115).
  - Every normative statement in the five scanned docs is a tagged bullet (Z2-D-066). Each file stays at or under about 2,000 lines, or is split per section — except `Zenith2/Docs/DecisionLog.md`, which is never split because the rulings scan reads one `decisionLog` path (see "How this log is written").
- **Alternatives considered:** (1) Draft docs in the drafts root and copy them in — rejected: two copies drift. (2) Commit each doc when reviewed — rejected: each commit regates in-flight lane tickets.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Zenith2/Docs, uncommitted docs, core docs, design doc, requirement catalogue, A8a, WorkerGuide.md, 2,000-line limit
- **Related:** Z2-D-115, Z2-D-122
### Z2-D-122 Drafts are durable under C:/dev/Zenith2-drafts with a manifest and a resume protocol
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** The session can run for weeks, and a session's scratchpad is session-specific, so any draft kept there would be lost on a restart.
- **Decision:**
  - All drafts live in a durable, gitignored-by-location root outside the repo: `C:/dev/Zenith2-drafts/`.
  - It holds a `manifest.json`: per area its state (drafted / reviewed-round-N / signed-off / filed) and the file hashes; per ticket draft its progress, so an interrupted drafting run resumes where it stopped.
  - It has a documented resume protocol (`RESUME.md`), so any session can continue from the manifest.
  - Ticket drafts live in `C:/dev/Zenith2-drafts/tickets/<AREA>/`, the session filer in `filer/`, the session coverage checker in `checker/`, reviewer rounds in `reviews/`, and flagged proposals and open questions in `findings/`.
- **Alternatives considered:** (1) Keep drafts in the session scratchpad — rejected: lost on restart. (2) Keep ticket drafts in the repo — rejected: they are not deliverables, and would dirty the tree the loop needs clean.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** OPEN_QUESTIONS_FOR_HUMAN, FLAGGED_PROPOSALS, Zenith2-drafts, durable drafts, manifest.json, RESUME.md, resume protocol, ticket drafts, drafts root
- **Related:** Z2-D-121, Z2-D-125
### Z2-D-123 Fable review is capped at three rounds, then open issues escalate to the human
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** A reviewer and a writer can loop indefinitely on a disagreement neither can settle, and the settling authority is the human.
- **Decision:**
  - A Fable reviewer checks each core doc, each area's design doc and catalogue, and each area's ticket drafts for at most 3 rounds.
  - Anything still open after round 3 goes to the human as a listed question, rather than looping (`C:/dev/Zenith2-drafts/findings/OPEN_QUESTIONS_FOR_HUMAN.md`).
  - Ticket reviews apply the ticket standard and the worker-executability test (Z2-D-068). A final Fable review checks that the docs, the catalogues, the board and the coverage report agree, followed by a spot check of sampled tickets read as a worker would (3 random tickets per area, read cold).
- **Alternatives considered:** (1) Review until writer and reviewer agree — rejected: unbounded, and some questions are the human's. (2) One review round — rejected: too few to converge on detailed specifications.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Fable review, review round, three rounds, escalation, open questions, OPEN_QUESTIONS_FOR_HUMAN, final review, spot check, worker-executability test
- **Related:** Z2-D-068, Z2-D-124, Z2-D-125
### Z2-D-124 Human sign-off per area is batched: scope, classification, normative capture, open decisions
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Scope cuts, classifications and open decisions are the human's to confirm, and the normative scanner cannot see obligations phrased without keywords. Asking per item would stall the programme.
- **Decision:**
  - The human signs off per area, in batches, on: the scope cuts and classifications; that normative content has been captured (the checklist item the keyword scanner cannot enforce); and the open decision tickets.
  - Sign-off is where the brainstorming continues; an area's tickets are drafted after its sign-off.
- **Alternatives considered:** (1) Sign-off per ticket — rejected: thousands of approvals. (2) One sign-off for the whole programme — rejected: too coarse to catch an area's scope errors.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** area sign-off, A4, manifest.json, OPEN_QUESTIONS_FOR_HUMAN.md, batched sign-off, scope cuts, classification, normative capture, open decision tickets
- **Related:** Z2-D-066, Z2-D-123
### Z2-D-125 Ticket drafts live outside the repo and every stage is drafted in full detail
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Ticket drafts are inputs to filing, not repo content, and the human ruled that every stage is drafted in full (Z2-D-070).
- **Decision:**
  - Ticket drafts go in `C:/dev/Zenith2-drafts/tickets/<AREA>/`, never in the repo.
  - Every requirement is decomposed per the ticket standard into tickets for Stages 0 to 7 — PROC infrastructure, scaffold, implementation, tests, integration, decision, revalidation and acceptance — each with exact files, signatures and tests, including Stages 3–7.
  - Areas that depend on each other are drafted in dependency order (Core → Jobs → Reflect/ECS → RHI/Render → …), so subsequent areas cite earlier APIs exactly; about six areas are drafted at a time.
- **Alternatives considered:** (1) Draft tickets inside `Zenith2/Docs/Board/` — rejected: board files are written only through the Vault writer. (2) Draft areas in arbitrary order — rejected: downstream areas would cite APIs that do not yet exist.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage overview"`.
- **Nouns:** Test_DraftCompleteness, ticket drafts, Zenith2-drafts/tickets, full-detail drafting, dependency order, area drafting
- **Related:** Z2-D-068, Z2-D-070, Z2-D-122
### Z2-D-126 human-gate is not used for Z2 decisions
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** `human-gate` makes the loop run a ticket and then park it at In Review. For a decision that needs reference-hardware measurement, the loop cannot do the part that matters; for a ruling, there is nothing for the loop to run.
- **Decision:**
  - `human-gate` is not used for Z2 decisions.
  - A decision that needs measurement is an ordinary harness ticket plus a `needs-human` ruling ticket (Z2-D-084); a decision that needs no measurement is a `needs-human` ruling ticket.
- **Alternatives considered:** (1) `human-gate` on a combined harness-and-measurement ticket — rejected: the loop parks it, but a human still has to run the measurement. (2) A ruling ticket claimable by the loop — rejected: `contract.mjs:38` refuses any agent claim of a `needs-human` ticket, and the ruling is a judgement over reference-hardware results the loop cannot produce.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Board conventions"`.
- **Nouns:** Test_RulingNotClaimable, human-gate, In Review, ruling ticket, needs-human, harness ticket
- **Related:** Z2-D-084
### Z2-D-127 Stage acceptance suites are run by the human outside the loop
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Acceptance tickets are `needs-human`, and `/tick` refuses those even by name. The full suites exceed the phase cap and need the whole machine.
- **Decision:**
  - Every stage acceptance ticket is `needs-human`, categorised `Zenith2Acceptance`, and designated with `zagent sprint gate`.
  - The human runs its lines directly through `Run-Committed.ps1 -Acceptance -Stage N` in the main checkout, while every lane is idle, holding the machine-wide slot through `build_lock.ps1 -Run` (Z2-D-083).
  - Each stage sprint also carries a `needs-human` mid-sprint integration check that runs the flagship slices' playthrough scripts on the RTX 3070 desktop and the S24, measures them against the budgets, and files regressions as tickets.
- **Alternatives considered:** (1) Acceptance tickets claimable by the loop — rejected: the loop cannot run device and sanitizer suites within the cap. (2) Only a stage-exit check — rejected: drift would be caught too late.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Acceptance evidence"` and `Zenith2/Docs/Roadmap.md → "Integration cadence"`.
- **Nouns:** stage acceptance ticket, Zenith2Acceptance, zagent sprint gate, Run-Committed.ps1 -Acceptance, build_lock.ps1 -Run, mid-sprint integration check, playthrough script
- **Related:** Z2-D-083, Z2-D-086
### Z2-D-128 Stage 0 PROC tickets are needs-human and the scaffold is the last Stage 0 ticket
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Stage 0 changes protected and Zenith-side paths, which the loop cannot edit, and the first ticket the loop can run needs the external build mode and the base gate steps to exist.
- **Decision:**
  - Stage 0 (Tooling bring-up) is the first sprint. Its PROC tickets edit protected or Zenith-side paths, so they are `needs-human`, executed by a human-driven session, and each is fully specified like any other ticket.
  - The Stage 0 PROC list: first, the global guardrail additions with harness regeneration and `-Check`; the generic `tick_gates.ps1` external-build mode, `phaseTimeoutMinutes` and flake exemptions, with tests; the Vault pass-through (`buildSystem`, `measureSteps`, `phaseTimeoutMinutes`, `reviewPaths`), with tests; `include_roots` for the analyzer and the scanner audit; the root CLAUDE.md delegation line; libgraph's generic CMake File API front end reading the declared-dependency JSON, with tests; a generic `max-file-loc` key with a test; the base Z2 gate step scripts (Configure, Build, Test, SourceLayout, CompileCoverage, Libgraph, Complexity, Forbidden, ApiContract, TestSeparation, ReviewSignals) and `Run-Committed.ps1`; the production coverage checker with its failing fixtures; the production filer with reconcile mode; `zenith2-gates.yml`; PROC-API with its revision-bound review records; the generic per-category `postFinish` command in `TICK.md` with harness regeneration; the reproducibility manifest and toolchain pins (`ThirdParty/manifest.json`, the setup script, sccache); the generic `reviewerConventions` field; the generic per-project provisioning hook of `zagent lane add` (added 2026-10-16 by the human's ruling Q3 (a)); `zagent lane add Z2`.
  <!-- elaboration -->
  - The /tick verification adds six generic PROC candidates: fixing the Vault grep guards (Z2-D-100); scoping `Test-ZenithProjectShapingPath` (`Build/zenith_buildsystem.psm1:1078-1100`) to Sharpmake source roots, so Z2 sources stop triggering Zenith regens; a claim predicate for `needs-revalidation` (Z2-D-102); a `project_id` on `version create` rows (Z2-D-094); weighting a direct decision-ID citation at `RULINGS_FILE_MIN_SCORE`, so a citation alone selects its entry (Z2-D-098); and an array-valued `decisionLog`, needed only if the decision log is ever split.
  - PROC tickets are closed by a human actor (Z2-D-103).
  - The scaffold ticket is the last Stage 0 ticket and the first one `/tick` runs, once the external mode and the base gate steps exist. It includes the `android-arm64` preset and a Player stub, which the Stage 1 D-SLANG (open, Stage 1) arm64 probe needs.
  - Stage 0 acceptance: the scaffold is ticked end to end in the Z2 lane, `integrate` is GREEN, and CI is green.
  - The gate list grows by PROC tickets: Stage 0 activates the base list; Android build and APK, sanitizers, backend parity and path parity are each added by a `needs-human` PROC ticket in Stages 2, 3 and 5.
- **Alternatives considered:** (1) Let the loop run PROC tickets — rejected: they edit protected paths. (2) Scaffold first, tooling after — rejected: the loop cannot run the scaffold without the external mode and gate steps.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Stage 0 Tooling bring-up"`.
- **Nouns:** tick_gates, build_lock, HarnessArtifacts, analyze_code_complexity, Stage 0, Tooling bring-up, PROC ticket, needs-human, scaffold ticket, human-driven session, external-build mode, base gate steps, Stage 0 acceptance, integrate GREEN, gate list growth
- **Related:** Z2-D-032, Z2-D-100, Z2-D-102, Z2-D-103, Z2-D-116, Z2-D-118
### Z2-D-129 Spec-revalidation tickets are scoped to area, stage and decision set
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Tickets are drafted in full before their governing decisions are ruled (Z2-D-070), so each needs a mechanical way back to `ready` after the rulings. Revalidation for a stage must not depend on decisions of subsequent stages, or Stage 1 would stop being a closed DAG.
- **Decision:**
  - A downstream ticket is `blocks`-linked from the decision tickets and budget-row tickets that govern it, and from a spec-revalidation ticket scoped to (area, stage, decision set).
  - A revalidation ticket for stage N depends only on decisions from stages ≤ N, so Stage 1 stays a closed DAG. For example, the D-TBDR (open, Stage 5) revalidation is a Stage 5 ticket and blocks only Stage 5 and subsequent rendering tickets.
  - A revalidation ticket is `needs-human` and is itself blocked by the governing decisions. Its DoD: every dependent body is consistent with the rulings, and every budget it cites is concrete.
  - `--mark-stale` applies `needs-revalidation` with `deferred` (Z2-D-102). The filer's reconcile mode updates the bodies and removes both; only after that does a ticket become claimable.
- **Alternatives considered:** (1) One revalidation ticket per decision — rejected: a ticket governed by several decisions would be revalidated repeatedly. (2) One per stage — rejected: too coarse for a human to review.
- **Consequences:** Recorded in `Zenith2/Docs/Roadmap.md → "Pull-forward rule"`.
- **Nouns:** Test_FilerReconcile, spec-revalidation ticket, File-Backlog.ps1, needs-revalidation, Spec-Maturity, revalidation scope, area, stage, decision set, reconcile mode, Spec-Maturity ready, closed DAG
- **Related:** Z2-D-068, Z2-D-070, Z2-D-084
### Z2-D-130 Flagged proposals P1 and P2 are recorded and not applied
- **Status:** Settled · **Date:** 2026-09-24
- **Context:** Two concerns about settled choices were raised with evidence during planning. Under Z2-D-022 they are recorded for the human and change nothing until approved.
- **Decision:**
  - P1 (settled choice: all three ECS backends, including EnTT through an adapter). Concern: data-defined components conflict with EnTT's compile-time-typed storage, so a growing share of what the research measures is the dynamic-storage adapter rather than EnTT, and the cost is paid in every conformance run. Proposal: keep EnTT but scope its research claim to C++-registered components; after the Stage 5 research report a ruling ticket decides whether EnTT stays a supported shipping backend or becomes research-only (D-SEL-EnTT-SHIPPING, flagged proposal P1, Stage 5). Trade-off: less backend-maintenance burden versus the original intent of three equal shipping backends.
  - P2 (settled choice: one lane per project, so Z2 runs one ticket at a time). Concern: the backlog is thousands of tickets, so calendar throughput is bound by serial ticks. Proposal: a generic PROC ticket makes `laneAdd` support N lanes per project, with per-lane build directories and the shared build slot, running 2–3 Z2 lanes on independent libraries. Trade-off: protected Vault and lane-tooling changes and more build-slot contention, versus much higher Z2 throughput.
  - Neither is applied. The docs and the board are written as the settled decisions say.
- **Alternatives considered:** (1) Apply P1 and P2 now — rejected: nothing changes without the human's approval (Z2-D-022). (2) Drop them — rejected: the evidence is real and belongs in front of the human.
- **Consequences:** Listed in `Zenith2/Docs/Roadmap.md → "Flagged proposals"`.
- **Nouns:** flagged proposal, P1, P2, EnTT adapter, D-SEL-EnTT-SHIPPING, laneAdd, N lanes per project, throughput
- **Related:** Z2-D-022, Z2-D-029, Z2-D-039
### Z2-D-131 Inventory v1 stage corrections before the docs and config commit move device loss, asset recovery and the WorkloadOutdoor terrain slices to Stage 3
- **Status:** Settled · **Date:** 2026-10-02
- **Context:** The area designs found three places where Inventory v1 staged an item after the stage whose acceptance already requires it. `Zenith2/Docs/Roadmap.md → "Acceptance evidence"` requires resize and device-loss recovery at Stage 3, while Z2-I-RHI-21 was S6. It requires reimport, clean versus incremental cook equivalence and missing or corrupt content at Stage 3, while Z2-I-ASSET-04, -12 and -13 were S4. Stage 3 authors the focused workload games and confirms W-Outdoor's rows at its full scale (4 km² terrain, 100k foliage instances), while terrain sculpting, foliage placement, atmosphere, day/night, procedural placement and terrain geometry were S4. The change rule of `Zenith2/Docs/Requirements/Inventory.md → "Change rule"` requires a recorded decision for each such change.
- **Decision:**
  - Z2-I-RHI-21 (recovery: device loss, with breadcrumbs and a recover-or-clean-exit policy) moves from S6 to S3. Z2-I-RHI-19 (recovery diagnostics) stays S6.
  - Z2-I-ASSET-04 (reimport), Z2-I-ASSET-12 (missing assets) and Z2-I-ASSET-13 (corrupt assets) move from S4 to S3. The Stage 4 remainder — reimport through the cook server, retry after mount changes, the full corruption fixture sweep in the integration suite — stays S4 at requirement granularity in `Zenith2/Docs/Requirements/ASSET.md`.
  - Z2-I-ENV-01 (terrain sculpting), Z2-I-ENV-03 (foliage placement), Z2-I-ENV-06 (atmosphere), Z2-I-ENV-09 (day/night), Z2-I-ENV-10 (procedural placement) and Z2-I-GEOM-19 (terrain geometry) move from S4 to S3, so that W-Outdoor is authored and measured at its full scale in Stage 3; the ENV and GEOM requirements serving them carry Stage S3 and fix version `Z2 Stage 3`, and the Stage 3 `WorkloadOutdoor` game ticket is `blocks`-linked from them. The other ENV items stay S4.
  - A requirement's Stage is its fix version, the stage whose acceptance it serves; no requirement carries Stage S3 with fix version `Z2 Stage 4`. Scheduling earlier than the fix version is a sprint property of the ticket (`Zenith2/Docs/Roadmap.md → "Pull-forward rule"`).
  - The corrections are made in place inside Inventory v1, with no version bump and no rename of the heading "Inventory v1": before the docs and config commit (Z2-D-115) no filed ticket cites an inventory stage, so there is no stale citation for a version bump to flag. Classifications, IDs and the item count (706) are unchanged. After that commit every inventory change follows the full change rule.
- **Alternatives considered:** (1) Bump the inventory to v2 and rename its heading — rejected: no filed ticket exists to revalidate, and renaming a heading the drafted catalogues cite would break their citations for no benefit. (2) Keep the items at S4 and author a placeholder W-Outdoor scene in Stage 3 — rejected: it would measure a workload that is not W-Outdoor while Stage 3's acceptance claims W-Outdoor's frame-rate rows. (3) Let requirements carry Stage S3 with fix version `Z2 Stage 4` — rejected: it contradicts the rule that a requirement's Stage is its fix version and leaves Stage 3 tickets behind Stage 4 blockers.
- **Consequences:** The rows are edited in `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"` with this ID in their Notes; `Zenith2/Docs/Roadmap.md → "Stage 3 Playable authoring slice"` names the pulled-in terrain and environment slices and the device-loss recovery, and `Zenith2/Docs/Roadmap.md → "Stage 4 Production content and worlds"` keeps the remaining environment features. The RHI, ASSET, ENV and GEOM catalogues stage their covering requirements to match.
- **Nouns:** WorkloadOutdoor, Flux_TerrainRenderer, Flux_TerrainDesc, EXIT_CODE_DEVICE_LOST, Zenith_AssetRegistry, inventory stage, device loss, reimport, missing assets, corrupt assets, terrain sculpting, foliage placement, atmosphere, procedural placement, terrain geometry, fix version
- **Related:** Z2-D-067, Z2-D-070, Z2-D-115
### Z2-D-132 Inventory v1 stage corrections move the CONTENT importer items to Stage 3
- **Status:** Settled · **Date:** 2026-10-16
- **Context:** CONTENT's design found that Inventory v1 staged seven of its items at S4 although Stage 3 needs them: the Stage 3 workloads (W-Characters, W-Outdoor) import real content, and `Zenith2/Docs/Roadmap.md → "Acceptance evidence"` requires reimport and missing or corrupt content evidence at Stage 3. CONTENT's revision chose to move the items rather than restage its Stage 3 requirements to Stage 4 (cross-doc item X305 resolved as X334, option (a)). The docs and config commit (Z2-D-115) has not happened, so no filed ticket cites an inventory stage, which is the case Z2-D-131's in-place rule and the elaboration of `Zenith2/Docs/Requirements/Inventory.md → "Change rule"` govern.
- **Decision:**
  - Z2-I-CONTENT-01 (the supported-DCC feature matrix), Z2-I-CONTENT-02 (glTF 2.0 extensions), Z2-I-CONTENT-04 (skins), Z2-I-CONTENT-06 (animation), Z2-I-CONTENT-07 (materials), Z2-I-CONTENT-15 (reimport that preserves authored overrides) and Z2-I-CONTENT-22 (importer crash isolation) move from S4 to S3. Each item's later parts stay S4 at requirement granularity in `Zenith2/Docs/Requirements/CONTENT.md`, whose 36 S3 requirements cover only these items (and Z2-I-CONTENT-28).
  - The corrections are made in place inside Inventory v1, with no version bump and no heading rename, as Z2-D-131's were; classifications, IDs and the item count are unchanged.
- **Alternatives considered:** (1) Restage CONTENT's 36 Stage 3 requirements to S4 and schedule the work as pull-forward tickets with fix version `Z2 Stage 4` (X305 option (b)) — rejected: Stage 3 acceptance would claim reimport and corrupt-content evidence that no Stage 3 requirement serves. (2) Bump the inventory to v2 — rejected for the reason Z2-D-131 gives: no filed ticket exists to revalidate.
- **Consequences:** The rows are edited in `Zenith2/Docs/Requirements/Inventory.md → "Inventory v1"` with this ID in their Notes; `Zenith2/Docs/Roadmap.md → "Stage 3 Playable authoring slice"` names the importers as Stage 3 work, and `Zenith2/Docs/Roadmap.md → "Acceptance evidence"` splits the reimport row between Stage 3 and Stage 4.
- **Nouns:** inventory stage, supported-DCC matrix, glTF 2.0, reimport, authored overrides, importer crash isolation, out-of-process importer, product key, fix version
- **Related:** Z2-D-067, Z2-D-115, Z2-D-131

### Z2-D-133 Consoles: PlayStation 5, Xbox Series and Nintendo Switch 2, each gated on its own SDK-access ticket
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** Z2-D-071 defers consoles to Stage 7 gated on SDK access but names no platform, no `game.json` value and no rule for what one platform's missing access blocks. Without a platform set the console tree cannot be decomposed, and with a single gate a refused SDK would block every Stage 7 expansion.
- **Decision:**
  - The console platform set is PlayStation 5, Xbox Series X and S, and Nintendo Switch 2; earlier generations are not targets.
  - `game.json` platform values `playstation5`, `xboxseries`, `switch2` (schema version 2, Stage 7).
  - One `needs-human` SDK-access ticket per platform, in the Stage 6 sprint with fix version Z2 Stage 7; every other ticket of that platform is `blocks`-linked from it with the reason `backlog: <platform> SDK access`; it blocks nothing else.
- **Alternatives considered:** (1) every current and previous console — rejected: previous-generation GPUs cannot be assumed to meet Z2-D-013's baseline. (2) one SDK gate for all consoles — rejected: one refused programme would block the others and Stage 7's other expansions.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/Consoles.md` → "Console platform set", "SDK-access tickets"; `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "Consoles record".
- **Nouns:** SDK-access ticket, PlayStation 5, Xbox Series, Switch 2, playstation5, xboxseries, switch2, console platform, Zenith_GameComposition
- **Related:** Z2-D-071, Z2-D-013
### Z2-D-136 XR input enters PLAT's model through a control-mirror action set
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** OpenXR exposes input only as actions with suggested bindings, while PLAT's model maps device controls to the game's actions through contexts and bindings files that change at run time.
- **Decision:** ZenithXR creates one action set `zenith_mirror` with one action per XR catalogue control kind (left and right subaction paths) and a haptic action, suggests bindings for five interaction profiles, and pushes the controls' states to PLAT's `left`, `right` and `head` XR controller devices before each platform pump; PLAT's mapper applies the game's bindings.
- **Alternatives considered:** (1) one OpenXR action per game action — rejected: action sets attach once per session, so contexts and rebinding could not be represented. (2) a vendor input API — rejected: not OpenXR.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/XrInput.md` → "The control-mirror action set", "Suggested bindings".
- **Nouns:** zenith_mirror, control-mirror action set, Zenith_XrInputSource, SetVirtualControl, INPUT_DEVICE_KIND_XR_CONTROLLER, interaction profile
- **Related:** Z2-D-072, Z2-D-019
### Z2-D-137 The OpenXR instance is created before the RHI device by an optional device-interop factory of the XR module's descriptor
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** OpenXR's Vulkan binding creates the Vulkan instance and device itself and its D3D12 binding fixes the adapter, every one of those calls taking the `XrSystemId`, so the runtime and its system must exist before startup step 12; modules start at step 15, a module has no third entry point ([Z2-N-ARCH-848], [Z2-N-ARCH-261]), and the startup steps are a fixed enumeration of 19 (X307).
- **Decision:**
  - `Zenith_ModuleDescriptor` gains the optional member `m_pfnCreateDeviceInterop`, nullptr for every module but XR, filled by MOD's generator with `Zenith_Module_XR_CreateDeviceInterop`. It is reached only through the generated descriptor, so no third module entry point exists and [Z2-N-ARCH-848] and [Z2-N-ARCH-261] stand unamended.
  - No startup step and no enumerator is added: the last action of step 11 calls the one non-null member (two are a composition defect, exit code 3); step 12 calls `FillInteropRequest` before the device factory and `OnDeviceCreated` after it; shutdown step 9 calls `OnDeviceDestroying` and `OnDeviceDestroyed` around the device's destruction and then destroys the provider, which destroys the instance after the device.
  - Failure rule: the factory and the provider's fallible members return their own `Zenith_DeviceInteropError` (`DEVICE_INTEROP_ERROR_CONFIGURATION`, `DEVICE_INTEROP_ERROR_OUT_OF_MEMORY`), never `Zenith_ModuleError`; an invalid `xr.*` key or scripted-runtime document is exit code 2 at step 11, an allocation failure of the provider exit code 4; an absent loader, runtime or headset, a missing extension or version, or a refused requirement never fails startup. `uZENITH_MODULE_INTERFACE_VERSION` stays 1.
  - A headset connected after launch, an instance loss and a device loss recreate the device through RHI's recovery path, so the device is always the one the runtime requires; a requested recreation (`Flux_Renderer::RequestDeviceRecreation`, false outside `NORMAL`) pauses every world as a loss does, is not governed by `rhi.deviceLossPolicy`, and lifts the same-adapter clause; on D3D12 it is skipped when the existing device already has the runtime's adapter LUID and feature level; no XR frame call is made while the render subgraph is empty.
  - The session is created at module start.
- **Alternatives considered:** (1) a third module entry point or a game-level composition pointer amending [Z2-N-ARCH-848] — rejected: it widens the module contract, while an optional descriptor member is a composition-time hook that leaves [Z2-N-ARCH-848] and [Z2-N-ARCH-261] as they are. (2) a new startup step with its own enumerator — rejected: the startup steps are a fixed enumeration of 19 and the same work fits steps 11 and 12. (3) restarting the game when the headset appears after launch — rejected: the recovery path already rebuilds a device in place, and its cost (reloading resident GPU resources and recreating the PSO groups) arises once per headset appearance after launch or per instance recreation, and not at all on D3D12 when the adapter already matches.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "Startup and the device-interop provider", "Loss recovery and shutdown"; `Zenith2/Docs/Design/SCOPE/XrPublicAPI.md` → "Zenith_Module_XR header", "Interfaces of lower libraries"; cross-doc item X411.
- **Nouns:** Zenith_DeviceInteropProvider, Zenith_DeviceInteropError, Zenith_Module_XR_CreateDeviceInterop, Zenith_ModuleDescriptor, m_pfnCreateDeviceInterop, Zenith_RHIInteropRequest, RequestDeviceRecreation, Flux_DeviceRecreationReason
- **Related:** Z2-D-072, Z2-D-081
### Z2-D-138 XR gate tests run on a scripted OpenXR runtime; headsets are device runs
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** Gates run headless on WARP and lavapipe; no installed OpenXR runtime or headset exists there, and test doubles of final classes are forbidden (FINAL-DOUBLES).
- **Decision:** Every OpenXR call goes through the `Zenith_OpenXrApi` seam with the loader-backed and the scripted implementation (`xr.runtime = scripted`, non-Shipping, a `zenith2.xr-script` document); `ZenithXR_Tests` run under label `z2-xr` on the scripted runtime; real runtimes run in the windowed GPU configuration with skip code 77; headsets are `needs-human` device runs on the reference matrix (Meta Quest 3 over Link and over Steam Link, Valve Index on SteamVR).
- **Alternatives considered:** (1) tests only on real runtimes — rejected: no gate could run them. (2) mocking OpenXR per test — rejected: duplicates the seam and violates FINAL-DOUBLES' intent.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/XrRuntime.md` → "The OpenXR API seam", "Scripted runtime"; `Zenith2/Docs/Design/SCOPE/XrTests.md` → "Reference device matrix".
- **Nouns:** Zenith_OpenXrApi, Zenith_OpenXrApiScripted, zenith2.xr-script, z2-xr, SKIP_RETURN_CODE, reference device matrix
- **Related:** Z2-D-072
### Z2-D-139 XR-readiness checks in the scope register bind Stage 2 to Stage 6 tickets
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** Z2-D-072 keeps multi-view and input XR-ready, but an earlier ticket of another area could break a property Stage 7 relies on without failing any of its own tests.
- **Decision:** Each readiness check is a row of `Zenith2/Docs/Scope/ScopeRegister.json` with its owner heading, stage and tests; a ticket that edits the owner heading keeps the tests (renaming only with the register in the same diff); `Pwsh_ScopeRegister_ReadinessTestsPresent` fails a stage acceptance when a required test is missing.
- **Alternatives considered:** (1) review alone — rejected: invisible to every gate. (2) SCOPE re-implements every check — rejected: duplicates owners' tests.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/XrReadiness.md` → "How a readiness check binds a ticket".
- **Nouns:** XR readiness, Pwsh_ScopeRegister_ReadinessTestsPresent, ScopeRegister.json, XRR-V1, XRR-I1, readiness register
- **Related:** Z2-D-072, Z2-D-140
### Z2-D-140 Every scope boundary is a row of the machine-checked scope register
- **Status:** Settled · **Date:** 2026-10-17
- **Context:** Scope boundaries are recorded in prose (Architecture's scope table, Z2-D-071 to Z2-D-076); nothing fails when a deferred scope loses its tree or an exclusion loses its enforcement.
- **Decision:** `Zenith2/Docs/Scope/ScopeRegister.json` (schema `zenith2.scope-register` version 1) lists every boundary with classification, stage, decisions, rulings (labels or entry IDs), owner, inventory, explicit requirement IDs or ID ranges of named catalogues, design headings and enforcement, and a `meta` list, so every SCOPE requirement is attributed once; `Pwsh_ScopeRegister_Valid` checks it on every `Test` run reading only requirement headings and their Stage and Classification lines.
- **Alternatives considered:** (1) prose only — rejected: drift is silent. (2) the coverage gate step alone — rejected: it cannot see enforcement entries.
- **Consequences:** `Zenith2/Docs/Design/SCOPE/ScopeRecords.md` → "The scope register", "Register checks".
- **Nouns:** Pwsh_ScopeRegister_Valid, ScopeRegister.json, zenith2.scope-register, scope boundary, enforcement entry
- **Related:** Z2-D-070, Z2-D-071, Z2-D-076

### Z2-D-141 Code conventions from the human's review: no noexcept or nodiscard, sint and uint integers, final only with a base, const IMMUTABLE members, Zenith_Result as T plus E, one threading class per platform, no vk in Slang
- **Status:** Settled · **Date:** 2026-10-23
- **Context:** The human reviewed an implementation of the early engine phases and found that conventions the docs mandated add noise or hide behaviour: `noexcept` and `[[nodiscard]]` on every public declaration although exceptions are already off outside the Luau firewall, `u_int*` and `intN_t` integer spellings, `final` on classes with no base, writable members on types whose contract says `IMMUTABLE`, a profiling bracket that could not be used twice in a frame, a `char` array prefixed `x`, a `Zenith_Result` built on a union and a success flag, synchronisation types holding opaque `m_auNative` byte arrays, and `vk::` attributes in Slang. This entry records the human's ruling of 2026-10-23, settled by the human and binding on every Zenith2 document and ticket.
- **Decision:**
  - `noexcept` is not written in Zenith2 code, except where a language or standard-library mechanism actually requires it, which is very rare; such a use carries a `// ZENITH-ALLOW: <reason>` comment on its own line naming the mechanism (`Zenith2/Docs/Contracts.md` [Z2-N-CONTRACT-582]). The rules that every public function is `noexcept` ([Z2-N-CONTRACT-030], [Z2-N-CONTRACT-047], [Z2-N-CONTRACT-048], [Z2-N-CONTRACT-049]), the `noexcept` requirements on job bodies, bindings and constructors, and `ApiContractCheck` step 6 are withdrawn. Exceptions stay disabled outside the Luau firewall (Z2-D-017, Z2-D-045).
  - `[[nodiscard]]` is not written anywhere, `Zenith_Result` included ([Z2-N-CONTRACT-400] and [Z2-N-CONTRACT-401] withdrawn; [Z2-N-CONTRACT-586]). `[[noreturn]]` is written only on a function that genuinely never returns (the fatal and unreachable handlers), where warnings-as-errors needs it. An intentionally ignored fallible result is written `static_cast<void>(Expr)` with its reason, and review confirms that no fallible result is dropped silently ([Z2-N-CONTRACT-402]).
  - Integers are named through `Zenith_Types.h`, whose names alias the exact types: `uint8` … `uint64` are `std::uint8_t` … `std::uint64_t`, `sint8` … `sint64` are `std::int8_t` … `std::int64_t`, `uint` is `unsigned int` and `sint` is `int`; engine code never writes `u_int*`, `intN_t` or `uintN_t`, which only a third-party API boundary inside `Internal/` may use ([Z2-N-CONTRACT-588]). Hungarian type prefixes: `u` for `uint` and `uint8` to `uint32`, `ul` for `uint64`, `i` for every signed type, `sint` and `sint8` to `sint64` (there is no `il`), `c` for `char` and `ac` for a `char` array (`Zenith_String::m_acBytes`); `sz` stays the prefix of a pointer to a NUL-terminated C string.
  - `final` appears only on a class that inherits from a base; a class or struct with no base is never `final` ([Z2-N-CONTRACT-588]). `ForbiddenCheck` rule F25 refuses `noexcept` and `[[nodiscard]]` without the signpost, `u_int*`, `intN_t` and `uintN_t` outside their boundary files, and `final` on a class with no base.
  - A `struct` or `class` whose contract block says `Thread affinity: IMMUTABLE` declares its data members `const` where possible, each `const` member with a default member initializer so the type stays default-constructible and aggregate-initialisable; a member stays non-const only where `const` would break an operation the type needs (copy or move assignment by an API or a container, default construction then filling, a builder or decoder writing after construction), and the `Thread affinity` field names that operation in one clause ([Z2-N-CONTRACT-585]).
  - A profiling bracket, one `ZENITH_PROFILE_SCOPE` site, may be entered any number of times in a frame, nested or on several threads; the profiler accumulates every closure of the site into its frame total (count, total, maximum) and resets that total once per frame at `EndFrame` (`Zenith2/Docs/Design/DEVT/Profiler.md` → "Zone statistics"), and designs and tests prove both the sum and the single reset.
  - `Zenith_Result<T, E>` stores exactly `{ T m_xValue; E m_eError; }` in every build, with no union and no success flag: success is `Zenith_GetErrorCode(m_eError) == 0` (error codes are never 0), `T` is default-constructible (`static_assert`), and `Zenith_Result<void, E>` stores `{ E m_eError; }`. Misuse of `Value()`, `TakeValue()` or `Error()` is a `ZENITH_ASSERT`, checked in Debug and sanitizer builds alone, so a non-debug build has no check and no extra state and the layout is identical in every build ([Z2-N-CONTRACT-587]; [Z2-N-CONTRACT-462] is restated from a `ZENITH_CHECK` in every configuration to this debug-only assert). `Zenith_Result` is not `[[nodiscard]]`, not `final` and not `noexcept`.
  - `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore` follow the existing Zenith pattern: one class per platform (`Zenith_WindowsMutex`, `Zenith_AndroidMutex`, `Zenith_LinuxMutex`, and likewise for the others) in ZenithCore's per-platform header `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h`, which the build selects through the platform's include directory; each holds its native object as a typed member (Android and Linux: `pthread_mutex_t m_xMutex`, `sem_t m_xSemaphore` with `uint m_uMaxValue`, `pthread_cond_t m_xCondition`; Windows: the pointer-sized `SRWLOCK`, `CONDITION_VARIABLE` and `HANDLE` as `void* m_pSRWLock`, `void* m_pConditionVariable` and `void* m_pHandle`, so `<windows.h>` stays out of public headers), and the public header aliases the platform class (`using Zenith_Mutex = Zenith_<Platform>Mutex;`). Opaque byte arrays (`m_auNative`) are withdrawn. The per-platform header is a signposted exception to the public-header rules allowing `<pthread.h>` and `<semaphore.h>` on Android and Linux, carried by a `ZENITH-EXEMPTION` comment block in the file and by the `publicPlatformHeaders` elements of `platform_headers.json` ([Z2-N-CONTRACT-583], [Z2-N-CONTRACT-584], [Z2-N-CONTRACT-432]); it is platform code in platform files, not a layout-changing `#ifdef`.
  - Amended 2026-10-23 by Z2-D-142 (the human's request): each of these classes is one class template per platform on `bool bProfiled` (`Zenith_<Platform>Mutex_T<true>` is `Zenith_Mutex`, `<false>` is `Zenith_MutexNoProfiling`, and likewise for the other two), with six aliases at the end of each platform header; the header, the typed native members and the exemption are unchanged.
  - No `vk::` attribute or intrinsic appears in any Slang source or snippet: push and root constants are entry-point `uniform` parameters, which Slang lays out as Vulkan push constants and as the D3D12 entry-point constant buffer the root signature feeds with root constants; resources use the bindless accessors, `ParameterBlock` and `ConstantBuffer` with reflection-driven layout; `ShaderChecks.NoVulkanAttributes` fails any `vk::` token in a `.slang` file (`Zenith2/Docs/Rendering.md` [Z2-N-REND-998]).
  - Amended in place by this entry, each with a dated line naming it: Z2-D-017 (its clause that every engine function a binding calls is `noexcept`), Z2-D-034 (its type-prefix list), Z2-D-042 (its clause putting `[[nodiscard]]` on `Zenith_Result` and every fallible API) and Z2-D-045 (the same clause). Their other clauses stand.
- **Alternatives considered:** (1) keep `noexcept` and `[[nodiscard]]` on every declaration, checked by `ApiContractCheck` and the compiler — rejected by the human: noise on every line, while exceptions are already disabled and an ignored result is a review item. (2) A `Zenith_Result` with a union and a success flag, or with a debug-only flag — rejected by the human: the union forces manual lifetime code, and a debug-only member changes the layout between builds ([Z2-N-CONTRACT-441]); `{ T, E }` is a pure wrapper whose only cost is a default-constructed `T` on failure. (3) Opaque native byte arrays in one cross-platform synchronisation class — rejected by the human: the native type is hidden from the debugger and its size must be guessed per platform; the old Zenith one-class-per-platform pattern is chosen. (4) A global `[vk::push_constant]` declaration — rejected: it is Vulkan-specific syntax in a source both backends compile, while the entry-point `uniform` parameter is Slang's API-neutral form.
- **Consequences:** `Zenith2/Docs/Contracts.md → "API boundary"`, `Zenith2/Docs/Contracts.md → "Contract blocks per type"`, `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`, `Zenith2/Docs/Contracts.md → "Errors and results"`, `Zenith2/Docs/Contracts.md → "Enforceability matrix"`; `Zenith2/Docs/Contracts/Memory.md → "Zenith_New Zenith_TryNew and Zenith_Delete"`; `Zenith2/Docs/Rendering.md → "Binding model"`; `Zenith2/Docs/Rendering/ShaderSystem.md → "Shader lints"`; `Zenith2/Docs/Scripting.md → "Luau integration and the exception firewall"`; `Zenith2/Docs/Design/PROC/StructuralGates.md → "ForbiddenCheck rule table"` (rule F25, `publicPlatformHeaders`). Every area design and ticket applies the same rules.
- **Nouns:** Zenith_Result, Zenith_GetErrorCode, Zenith_Types, Zenith_PlatformThreading, Zenith_WindowsMutex, Zenith_AndroidMutex, Zenith_LinuxMutex, Zenith_Semaphore, Zenith_ConditionVariable, NoVulkanAttributes, ApiContractCheck, ForbiddenCheck, ZENITH_PROFILE_SCOPE, ZENITH-EXEMPTION, ZENITH-ALLOW, m_acBytes, m_auNative, publicPlatformHeaders, sint32, uint32, noexcept, nodiscard, noreturn, IMMUTABLE, push constant, entry-point uniform parameter, accumulates
- **Related:** Z2-D-015, Z2-D-017, Z2-D-034, Z2-D-042, Z2-D-045, Z2-D-047, Z2-D-054

### Z2-D-142 Profilable locks: Zenith_Mutex and Zenith_MutexNoProfiling variants, contended-wait measurement, Zenith_Dispatch hook functions and the m_uHookDepth recursion guard
- **Status:** Settled · **Date:** 2026-10-23
- **Context:** The human required that mutex locks be easily profilable, that the profiling itself be thread-safe, and that a profiled mutex used by the profiler cannot recurse without end. Before this ruling every `Zenith_Mutex` called the installed lock hooks, there was no unprofiled variant, and the profiler stopped its own recursion only at run time (a lock at level 940 was never recorded, and `m_bInProfiler` ended any recursion before the ring). The profiler recorded only hold time, never the time a thread waited to get the lock, which is the number a contention view needs.
- **Decision:**
  - Each synchronisation primitive exists in two variants chosen at compile time, the old Zenith pattern: every platform class becomes a class template on one `bool bProfiled` (`Zenith_WindowsMutex_T<bool bProfiled>`, `Zenith_AndroidMutex_T`, `Zenith_LinuxMutex_T`, and likewise `…ConditionVariable_T` and `…Semaphore_T`), and each platform header ends with six aliases: `Zenith_Mutex` and `Zenith_MutexNoProfiling`, `Zenith_ConditionVariable` and `Zenith_ConditionVariableNoProfiling`, `Zenith_Semaphore` and `Zenith_SemaphoreNoProfiling`, the unsuffixed name being the `<true>` instantiation. The two variants hold the same native members and behave the same, except that the `<false>` variant never reads the hook table (its hook calls sit under `if constexpr (bProfiled)`, so it contains no reference to `Zenith_GetProfilerHooks`, to any `Zenith_Dispatch…` function or to the clock). Lock-level registration, lock-order checks and the held-lock stack apply to both variants unchanged. `Zenith_ScopedLock` is a class template on the mutex type and works with both. A `Zenith_ConditionVariableNoProfiling` waits only on a `Zenith_MutexNoProfiling`, and a `Zenith_ConditionVariable` only on a `Zenith_Mutex`; a `static_assert` refuses either mixed pairing. `Zenith_Thread::Join` and `Zenith_SleepUs` keep calling the blocking-wait hook.
  - A profiled `Lock` tries the native lock first. On success it reports the acquisition with a wait of 0, meaning it did not block. On contention it reads the monotonic clock, blocks, reads it again and reports the measured wait, at least 1; the clock is read only on contention and only when an acquire hook is installed, so the path with no hook installed reads no clock (the per-dispatch cost is stated in `Zenith2/Docs/Design/CORE/Threading.md` and is pending measurement). The acquire hook gains a parameter, `uint64 ulWaitTicks`, the wait in ticks of `Zenith_GetMonotonicTimeNs()` (one tick is one nanosecond). `TryLock` reports a success with wait 0 and calls no hook on failure; `Unlock` reports the release as before, from a copy of the desc taken before the native unlock. A profiled condition-variable wait reports a release before the native wait and an acquire with wait 0 after it. The blocking-wait hook gains a parameter, `uint64 ulWaitTicks`, and is dispatched after the wait returns.
  - The profiler (DEVT) records per lock site, per frame, the acquisitions, the contended acquisitions, the total and maximum wait and the total and maximum hold, accumulated within the frame and reset once per frame like zone statistics, and a contended acquire also produces a `lockwait:<name>` zone beside the `lock:<name>` hold zone. Every hook writes only the calling thread's own ring and per-thread lock-site table, which are merged into the frame totals at `EndFrame` on the main thread; no hook takes a lock, allocates or blocks.
  - Recursion is impossible by construction, in three independent layers. (a) Types: the profiler and everything ZenithCore calls from inside a hook dispatch (the clock, `Zenith_SpscRing`, the Debug thread state, the log router) use only the `…NoProfiling` variants; the run-time rule "a lock at level 940 is never recorded" is deleted as a recursion defence. (b) Gate: `ForbiddenCheck` rule F26 refuses a profiled primitive in `Zenith2/Engine/ZenithProfiling/**`, `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and the files of the `profilerReachable` list, and a direct use of the hook table outside `Zenith_ProfilerHooks.h`. (c) Run time, in every build: every invocation of the zone-begin, zone-end, counter, lock-acquire, lock-release and blocking-wait hooks goes through the inline functions `Zenith_DispatchZoneBegin`, `Zenith_DispatchZoneEnd`, `Zenith_DispatchCounter`, `Zenith_DispatchLockAcquire`, `Zenith_DispatchLockRelease` and `Zenith_DispatchBlockingWait` of `Zenith_ProfilerHooks.h`, which raise the `thread_local` `m_uHookDepth` of the Debug thread state around the call; a dispatch made while the depth is non-zero does not call the hook, increments the thread's `m_uHookReentryCount` and, wherever asserts are enabled, fails `ZENITH_ASSERT(false, "profiler hook re-entered: a hook used a profiled primitive")`. So a hook that breaks the contract (Tracy, a test hook, a future profiler) recurses at most one level in any build. The profiler's own `m_bInProfiler` stays as defence in depth, not as the guarantee.
  - The log-record hooks (`m_pfnOnLogRecord`, `m_pfnIsLogLevelEnabled`) stay outside the dispatch functions. The failure procedure emits its fatal log record through the router from inside whatever hook asserted, and a refused dispatch would drop exactly the record that explains the failure; the router has its own rules (sinks never log, the router's locks `uLOCK_LEVEL_LOG_SINKS` and `uLOCK_LEVEL_LOG_RING_SINK` are `Zenith_MutexNoProfiling`, its drain thread waits on a `Zenith_SemaphoreNoProfiling`), so a hook that logs cannot recurse through it.
  - `m_uHookDepth` and `m_uHookReentryCount` are two new members of the existing allowlisted `thread_local` `g_xDebugThreadState`, present and maintained in every build. No variable is added, so the writable-data allowlist gains no entry and no extension line; the entry's comment and the "Why" line of its declaration block are widened.
  - Refinements from review, same day: the log router carries a thread-local `m_bInLogRouter` (saved on entry and restored on exit) with a count `m_uLogNestedDropCount` in the Debug thread state, drops a nested non-fatal record, dispatches no counter from `OnRecord`, and counts failed try-locks on its locks as `m_ulContended` (its locks are unprofiled by necessity); a Shipping profiler installs no lock pair and no blocking-wait hook, and in a Shipping build the profiled lock operations skip the hook lookup under `if constexpr (bProfiled && !Zenith_IsShippingBuild())`; a hook never logs because a profiler resource ran out (the profiler defers that warning to `EndFrame`); a profiled condition-variable wait reports a release before and an acquire after the native wait, and its blocking-wait record is named by the paired mutex (`wait:<mutex name>`), a semaphore by its optional static-storage name (default `Zenith_Semaphore`); `Zenith_LockDesc::m_szName` has static storage duration; `Zenith_BoundedQueue`'s semaphores are unprofiled and the queue reports once, by its own name; tests install hook tables before any other thread exists and uninstall after joining.
  - Amended in place by this entry, with a dated line: Z2-D-141 (its clause that `Zenith_Mutex`, `Zenith_ConditionVariable` and `Zenith_Semaphore` are one class per platform: each is now one class template per platform, instantiated twice).
- **Alternatives considered:** (1) One profiled class with a run-time "unprofiled" exemption (a level, a flag or a name list) — rejected: the recursion would be stopped only after the profiled lock has already called the hook, by a rule every new profiler lock must remember, which is the arrangement that prompted the requirement. (2) A second hand-written class per primitive and platform — rejected: two bodies of native code to keep identical; the template shares one body and differs only under `if constexpr`. (3) A `thread_local` re-entry flag alone, with no types and no gate — rejected: it is a run-time defence only, found in a debug build at the first slow path, and it leaves the profiler free to take profiled locks it will then silently skip. (4) Dispatching the log-record hooks through the guard too — rejected for the reason in the decision: it would drop the fatal record of a re-entry failure. (5) Measuring the wait around every lock call — rejected: it pays two clock reads on the uncontended path to learn that the wait was nil.
- **Consequences:** `Zenith2/Docs/Contracts.md → "No mutable global state"`, `Zenith2/Docs/Contracts.md → "Library directory layout"`, `Zenith2/Docs/Contracts.md → "Threading and concurrency contract"`, `Zenith2/Docs/Contracts.md → "Tests and code hygiene gates"`, `Zenith2/Docs/Design/CORE/Threading.md → "Platform threading classes"`, `Zenith2/Docs/Design/CORE/Threading.md → "Lock profiling and hook dispatch"`, `Zenith2/Docs/Design/PROC/StructuralGates.md → "ForbiddenCheck rule table"` (rule F26, `profilerReachable`), `Zenith2/Docs/Design/DEVT/Profiler.md → "Lock and wait events"`. Every area that calls a hook directly is converted to the dispatch functions by a later pass; F26 makes a missed call a gate failure.
- **Nouns:** Zenith_MutexNoProfiling, Zenith_ConditionVariableNoProfiling, Zenith_SemaphoreNoProfiling, Zenith_WindowsMutex_T, bProfiled, ulWaitTicks, Zenith_DispatchLockAcquire, Zenith_DispatchZoneBegin, Zenith_DispatchBlockingWait, m_uHookDepth, m_uHookReentryCount, profilerReachable, lockwait, hook re-entry, contended acquire, profiled primitive, unprofiled variant, recursion guard
- **Related:** Z2-D-005, Z2-D-006, Z2-D-046, Z2-D-141

## Decisions ruled after the seed

<!-- elaboration -->
Rulings made after the seed are appended below this line, each in the diff of the ticket that makes it, with the next free ID (one above the highest entry heading in this file) and the entry template of "How this log is written". Entries are never inserted above this section and never renumbered.
