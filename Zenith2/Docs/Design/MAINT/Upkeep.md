# MAINT design — ownership, upgrade rehearsals, sample games and headroom

This file is part of the MAINT design family (entry: `Zenith2/Docs/Design/MAINT.md`). It specifies subsystem ownership and its machine-checked table, the dependency-upgrade rehearsal policy with its registry, procedure and record, the representative sample games and their build matrix, the gate-headroom triage ticket, and the MAINT steps of every stage acceptance and mid-sprint integration check.

## Scope of this file

| Concern | Section |
|---|---|
| who owns each library, tool, directory and dependency | "Subsystem ownership", "Ownership file", "Ownership resolution", "Ownership checks" |
| rehearsing an upgrade before it is made | "Dependency-upgrade rehearsals", "Rehearsal registry", "Rehearsal plan", "Rehearsal procedure", "Rehearsal record" |
| the games kept building, and on which presets | "Sample games", "Sample games file", "Build matrix", "Sample game checks" |
| the ticket that restores gate headroom | "Gate headroom triage" |
| what MAINT contributes to each acceptance and mid-sprint check | "Stage acceptance and mid-sprint steps" |

## Subsystem ownership

Every subsystem has one owner, its area epic `Z2 <CODE> — <Area name>` ([Z2-N-CONTRACT-506]); the directory-level assignment is `Zenith2/Docs/Architecture.md` → "Repository layout" (table "Ownership per directory"). MAINT turns that assignment into a committed, checked file and states what an owner is responsible for.

<!-- elaboration -->
| Responsibility of the owning area | How it reaches the owner |
|---|---|
| maintenance tickets of its libraries, tools and directories | filed under its epic; `Get-Zenith2Owner` resolves the epic from any path |
| refactors owed for shortcuts its tickets leave ([Z2-N-CONTRACT-500]) | the refactor ticket's epic is the owner of the shortcut's path |
| removals of deprecated declarations of its headers | the roll's plan names the owner ("Release and roll procedures" of `Zenith2/Docs/Design/MAINT/Versioning.md`) |
| graduations or removals of its experimental blocks | the same plan |
| rehearsals and upgrades of the third-party dependencies its libraries own | the rehearsal registry's `owner` ("Rehearsal registry") |
| regressions the mid-sprint integration check measures in its rows ([Z2-N-CONTRACT-509]) | the regressing row's area code |
| complexity exceptions and surface growth in its libraries | the surface growth report lists the owner per library; an exception's entity resolves to its owner through `Get-Zenith2Owner` on its file |
| its repository games | "Sample games file" |

An owner is never a person: the board's actor model has no per-area assignee, so the area epic carries ownership and the ticket routing carries the work.

## Ownership file

`Zenith2/Tools/Maintenance/subsystem_owners.json` (format conventions of `Zenith2/Docs/Design/PROC.md` → "Format conventions").

<!-- elaboration -->
```json
{
    "schema": "zenith2.subsystem-owners",
    "schemaVersion": 1,
    "comment": "Ownership of every Zenith2 path; mirrors Zenith2/Docs/Architecture.md \"Repository layout\" and refines it per file. See Zenith2/Docs/Design/MAINT/Upkeep.md \"Ownership file\".",
    "areas": [
        { "code": "CORE", "name": "Core & mathematics", "epic": "Z2 CORE — Core & mathematics" }
    ],
    "paths": [
        { "path": "Zenith2/Engine/ZenithCore/", "owner": "CORE", "kind": "engine-library" },
        { "path": "Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h", "owner": "MAINT", "kind": "file" }
    ],
    "fallbacks": [
        { "path": "Zenith2/", "owner": "MAINT" }
    ],
    "tokens": { "tests": "CORE", "benchmark": "RESEARCH", "test-runs": "RHI", "host": "SHIP" }
}
```

| Member | Rule |
|---|---|
| `areas` | exactly the 41 area codes of `Zenith2/Docs/Requirements/Inventory.md` → "Area codes", in that table's order, each with its area name and its epic title `Z2 <CODE> — <Area name>` |
| `paths[].path` | repo-relative; a directory ends with `/`, a file does not; unique; byte-ordered |
| `paths[].owner` | an area code of `areas` |
| `paths[].kind` | `engine-library`, `module`, `backend`, `generated`, `player`, `tool`, `tool-library`, `scripts`, `harnesses`, `integration-suite`, `game`, `docs`, `build`, `third-party`, `shaders`, `directory`, `file` |
| `fallbacks` | prefixes that resolve a path no `paths` row covers; MAINT is the steward of last resort, and an acceptance check refuses any path that resolves only through a fallback ("Ownership checks") |
| `tokens` | the owning area of each manifest `owner` token (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Owner values"): doctest under CORE (ZenithTestSupport's owner), nanobench under RESEARCH, WARP and lavapipe under RHI, host tools under SHIP |

The rows the first MAINT ownership ticket writes are those of the Architecture table, one row per directory of each table row (`Engine/ZenithCompression/`, `_Zlib/`, `_Zstd/` become three rows), with the parenthesised sub-owners as their own rows where the table names a path for them (`Zenith2/Engine/ZenithFlux/Flux_Environment.h` and `Zenith2/Engine/ZenithFlux/Internal/Environment/` → ENV; `Zenith2/Engine/ZenithFlux/Flux_Sprite2D.h` and `Zenith2/Engine/ZenithFlux/Internal/Sprite2D/` → TWOD; `Zenith2/Tools/ZenithCook/Internal/Importers/` and `Internal/Content/` → CONTENT; `Zenith2/Tools/ZenithCook/Internal/Package/` → PACK), plus `Zenith2/Engine/ZenithCore/Zenith_EngineVersion.h`, `Zenith2/Engine/ZenithCore/Internal/Zenith_EngineVersion.cpp` and `Zenith2/Engine/ZenithCore/Tests/Zenith_EngineVersion_Tests.cpp` → MAINT, and `Zenith2/Tools/Maintenance/` → MAINT (cross-document row X387). A sub-owner the Architecture table names without a path (the lighting and geometry parts of ZenithFlux) gets its row in the diff of the owner's ticket that creates its directory, and until then resolves to the library's owner. The ticket also writes one `kind: file` row per tracked file directly under `Zenith2/`: `CMakeLists.txt` and `CMakePresets.json` → MOD (the scaffold's owner), `CLAUDE.md`, `.clang-format` and `.gitattributes` → PROC, `.clang-tidy` → DEVT (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "Static analysis"), `.gitignore` → SHIP.

## Ownership resolution

`Get-Zenith2Owner -Path <repo-relative path> [-OwnersFile <path>]` returns `{ Owner, Epic, Row, ViaFallback }`:

1. Normalise `\` to `/`, strip a leading `./`, and require the `Zenith2/` prefix (else exit 2).
2. Among `paths[]`, pick the longest `path` that equals the input (a file row) or is a prefix of it ending in `/` (a directory row). Complexity O(rows).
3. With no match, pick the longest matching `fallbacks[]` prefix and set `ViaFallback`.
4. Return the area, its epic from `areas[]`, and the matched row.

The CLI form `pwsh -NoProfile -File Zenith2/Tools/Maintenance/Get-Zenith2Owner.ps1 -Path <p> [-Json]` prints `<CODE>\t<epic>\t<row path>` (exit 0), or exit 2 for a path outside `Zenith2/`.

## Ownership checks

<!-- elaboration -->
| Rule ID | Rule | Run in |
|---|---|---|
| OWN-SCHEMA | the file obeys the schema and the member rules | `Test` |
| OWN-AREAS | `areas[]` equals the Inventory's area table (code, name, order) and each epic equals the Roadmap's epic title (`Zenith2/Docs/Roadmap.md` → "Board conventions", the epics table) | `Test` |
| OWN-ARCH | for every row of the Architecture ownership table, each directory it names (parsed as below) resolves through a `paths` row (never a fallback) to the owner the owner rules below give it | `Test` |
| OWN-TARGETS | every target of `declared_deps.json` (`libraries`, `tools`, `benchmarks`, `integrationSuites`, `games`) resolves through `paths` (not a fallback); a target that resolves only through a fallback is a report finding in `Test` and a failure in acceptance | `Test` (report), acceptance (fail) |
| OWN-DIRECTORIES | every directory directly under `Zenith2/Engine/`, `Zenith2/Tools/`, `Zenith2/Tests/`, `Zenith2/Benchmarks/`, `Zenith2/Benchmarks/Budgets/` and `Zenith2/Games/`, and each of `Zenith2/Player/`, `Zenith2/Shaders/`, `Zenith2/CMake/`, `Zenith2/ThirdParty/`, `Zenith2/Docs/`, resolves; through a fallback is a report in `Test`, a failure in acceptance | `Test` (report), acceptance (fail) |
| OWN-DEPENDENCIES | every `dependencies` entry of `Zenith2/ThirdParty/manifest.json` has an owner library (or token) that resolves to an area | `Test` |
| OWN-STALE | a `paths` row whose path does not exist: report in `Test`, failure in acceptance | `Test` (report), acceptance (fail) |
| OWN-ROOT-FILES | every tracked file directly under `Zenith2/` (`git ls-files Zenith2/` entries with no further `/`) has a `kind: file` row, never a fallback | `Test` |

The Architecture table is read from `Zenith2/Docs/Architecture.md` by locating the heading line `### Ownership per directory` and parsing the pipe table directly below it:

Paths (the first cell):

1. The cell is split on `, ` into backticked paths, read left to right.
2. A path beginning `_` is joined to the library directory of the row's FIRST path, never to an already joined path (`Engine/ZenithCompression/` + `_Zlib/` → `Engine/ZenithCompression_Zlib/`, and + `_Zstd/` → `Engine/ZenithCompression_Zstd/`).
3. A path whose first segment is a top-level directory of `Zenith2/` (`Engine`, `Player`, `Shaders`, `Tools`, `Benchmarks`, `Tests`, `Games`, `Docs`, `ThirdParty`, `CMake`) resolves against the repository root as written (`Tools/Setup/`, `ThirdParty/`, `CMake/` → three top-level paths). Any other path is a sibling and is joined to the parent directory of the path before it (`Games/OpenWorldSlice/`, `CinematicSlice/` → `Games/CinematicSlice/`).
4. A path containing `*` expands over the immediate directories of the tree that match it (`Games/Workload*/` → the five workload games).
5. Every path is prefixed `Zenith2/`.

Owners (the third cell), one rule per row shape:

| Row shape | How it is recognised | Owner each path is checked against |
|---|---|---|
| single-owner | the cell begins with exactly one of the 41 area codes, and the row is none of the shapes below | that code, for every path of the row; a parenthesised sub-owner with a path is its own `paths` row and does not change the row's check |
| positional | the cell ends `respectively` | the i-th code of the cell for the i-th path |
| repository game | a path under `Games/` that `Zenith2/Docs/Design/MOD.md` → "Workload games" or "Flagship slices" lists | the owner MOD's table gives that game, whatever the Architecture cell names (the `Games/Workload*/` cell omits ANIM for `WorkloadCharacters`; cross-document row X387 adds it) |
| aggregate | the cell says "per harness", "the area of each suite" or "each area" (`Benchmarks/`, `Tests/`, `Docs/`) | the directory itself: the code that begins the cell, when there is one (`Benchmarks/` RESEARCH, `Docs/` PROC), else no owner check; its child directories are never checked against the codes the cell gives as examples: each is owned by its own `paths` row, written by the ticket that creates it, and `OWN-DIRECTORIES` refuses one that resolves through a fallback |

Over the real table: `Tools/Setup/`, `ThirdParty/`, `CMake/` gives three top-level SHIP rows; `Games/OpenWorldSlice/`, `CinematicSlice/`, `Game2D/` gives GAME, CINE and TWOD by position and by MOD's table alike; `Games/Workload*/` gives RPROD (WorkloadIndoor, WorkloadOutdoor), ANIM (WorkloadCharacters), ECS (WorkloadDense) and UI (WorkloadUI2D) from MOD's table; `Benchmarks/`, `Tests/` and `Docs/` are aggregate rows; every other row is a single-owner row.

## Dependency-upgrade rehearsals

A rehearsal is a dry run of a dependency or toolchain upgrade made on a scratch worktree, recorded as evidence, before the real upgrade is decided. SHIP owns the upgrade itself: the update tool, the manifest diff and the evidence invalidation (`Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency updates"); MAINT owns which entries are rehearsed, how often, how, and what the record holds (Z2-I-MAINT-03).

<!-- informative -->
A rehearsal answers three questions before the programme commits to an upgrade: does the tree still build and pass with the new version; does performance stay within its budget rows where the dependency is performance-relevant; and what code must adapt. It never lands a manifest change on `master`: an adopted rehearsal is followed by SHIP's upgrade ticket pair (the `needs-human` manifest half and the loop ticket that adapts code), which cites the rehearsal record.
<!-- /informative -->

### Tiers and cadence

<!-- elaboration -->
| Tier | Entries | Cadence | Measured |
|---|---|---|---|
| `runtime` | every `dependencies` entry with `usage` `runtime` (linked into a Player) | once per stage from the stage after its pin; within the stage of an upstream security advisory that affects the pinned version | when `measure` is true and the rehearsal changes the pinned version |
| `tools` | every `dependencies` entry with `usage` `tools` or `tests` that has no `fromToolchain` | once every two stages | when `measure` is true and the rehearsal changes the pinned version |
| `toolchain` | every non-null key of the manifest's `toolchain` section; a `fromToolchain` dependency entry (`slang`, `dxc`, `warp`, `lavapipe`) is registered once, under its toolchain key, with the dependency entry's owner and `measure` | once every two stages | when `measure` is true and the rehearsal changes the pinned version |

The cadence and the measurement rule are ruling MAINT-R1-RULINGS R6 (orchestrator ruling; the human may override). A rehearsal whose upstream has no release newer than the pin records the verdict `no-newer` ("Rehearsal procedure"), needs no build or measurement run, and satisfies the cadence and `ACC-REHEARSALS`.

Additionally, before the Stage 6 shipping qualification every entry's last rehearsal is from Stage 5 or Stage 6.

`measure: true` marks the entries whose upgrade can move a budget row:

| Entry | Owner | Tier | `measure` | Rows the rehearsal measures |
|---|---|---|---|---|
| `jolt`, `physx` | PHYS | runtime | true | the PHYS per-system rows of W-Dense and W-Characters |
| `luau` | SCRIPT | runtime | true | the SCRIPT rows (`Zenith2/Docs/Scripting.md` → "Budgets safepoints and the watchdog") and the re-qualification list of [Z2-N-SCRIPT-502] |
| `flecs`, `entt` | ECS | runtime | true | the ECS research harness scenarios (`Zenith2/Docs/Design/ECS/Backends.md` → "Adapter cost model") |
| `zlib`, `zstd` | COMPRESS | runtime | true | the COMPRESS benchmark rows; cooked bytes change, so every game re-cooks |
| `vma`, `d3d12ma`, `d3d12-agility-sdk`, `volk` | RHI | runtime | true | the RHI memory and frame rows on the reference desktop and the S24 |
| `miniaudio` | AUDIO | runtime | false | — (the audio device layer; its conformance tests suffice) |
| `sdl3` | PLAT | runtime | false | — |
| `tracy` | DEVT | runtime | false | — (non-Shipping) |
| `slang`, `dxc` | RPROD | toolchain (one row each, under the toolchain key) | true | the GPU per-pass rows of the workload games (generated code changes) |
| `warp`, `lavapipe` | RHI | toolchain (one row each, under the toolchain key) | false | — (headless test drivers; the render and parity tests suffice) |
| `meshoptimizer`, `fastgltf` | CONTENT (owner library ZenithContentCore) | tools | false | — (cooked output changes: the rehearsal re-cooks and runs the render tests) |
| `ktx-software`, `mikktspace` | ASSET (owner library ZenithCook) | tools | false | — (as above) |
| every other `dependencies` entry | the area `Get-Zenith2Owner` returns for its manifest owner library (or the token owner of "Ownership file") | as its `usage` | false | — |
| `msvc`, `ndk` | SHIP | toolchain | true | the frame-rate rows of every workload on the reference desktop (`msvc`) and the S24 (`ndk`) |
| every other non-null `toolchain` key | SHIP | toolchain | false | — |

<!-- elaboration -->
Effort per rehearsal, for the human's planning: `no-newer`, a release-page check and a one-line record (minutes); a version change without measurement, one cold scratch worktree plus the builds of R5 and the suites of R6 (about two to four hours on the gate host, most of it unattended); with measurement, the before and after runs of each row on the reference desktop and the S24 (about one day). Entries of one owner whose builds overlap may share one rehearsal TASK (a bundle, "Rehearsal plan").

A selection candidate admitted by its ruling (D-SEL-HEAP, D-SEL-HASH, D-SEL-JSON (open, Stage 1); D-SEL-NAV, D-SEL-TEXT, D-SEL-UIRT (open, Stage 1/2); D-SEL-CODEC, D-SEL-VIDEO, D-SEL-CRASH, D-SEL-2DPHYS (open, Stage 2); D-SEL-FBX (open, Stage 4); D-SEL-UPSCALER (open, Stage 5)) joins the registry at the roll after its admission, with `measure` true for a runtime entry.

## Rehearsal registry

`Zenith2/Tools/Maintenance/rehearsals.json`:

<!-- elaboration -->
```json
{
    "schema": "zenith2.rehearsals",
    "schemaVersion": 1,
    "comment": "One row per manifest entry and toolchain key; see Zenith2/Docs/Design/MAINT/Upkeep.md \"Rehearsal registry\".",
    "cadence": { "runtime": 1, "tools": 2, "toolchain": 2 },
    "entries": [
        {
            "name": "jolt",
            "section": "dependencies",
            "tier": "runtime",
            "measure": true,
            "owner": "PHYS",
            "pinnedStage": 3,
            "last": {
                "stage": 4,
                "fromVersion": "5.2.0",
                "toVersion": "5.3.0",
                "verdict": "adopt",
                "revisitStage": null,
                "record": "Zenith2/Docs/Research/Upgrades/jolt/5.2.0-to-5.3.0/Rehearsal.json"
            }
        }
    ]
}
```

| Member | Rule |
|---|---|
| `entries[]` | one per `dependencies` entry without `fromToolchain` (`section` `dependencies`) and per non-null `toolchain` key (`section` `toolchain`) of the manifest, byte-ordered by `section`, `name`; a `fromToolchain` entry has no row of its own |
| `owner` | the area the tier table gives, derived through `Get-Zenith2Owner` on the manifest owner library |
| `pinnedStage` | the engine MAJOR when the row was added (the roll adds missing rows with the new MAJOR) |
| `last` | `null` before the first rehearsal; otherwise the latest record |
| `last.verdict` | `adopt`, `adopt-with-changes`, `defer`, `reject`, `no-newer` (then `toVersion` equals `fromVersion`) |
| `last.revisitStage` | the record's `revisitStage`: a stage number for `defer`, else `null` |

The due stage of an entry is `dueBy = (last == null ? pinnedStage : last.stage) + cadence[tier]`, lowered to `last.revisitStage` when that is set and smaller (a deferral may bring the next rehearsal forward, never postpone it past the cadence); an entry is overdue at the acceptance of stage S when `dueBy ≤ S` and it was not rehearsed in stage S. The Stage 6 rule adds: at the acceptance of Stage 6 every entry has `last.stage ≥ 5`. A `no-newer` record counts as a rehearsal in its stage.

### Rehearsal rules

| Rule ID | Rule | Run in |
|---|---|---|
| REH-SCHEMA | the registry obeys its schema, member rules and ordering; `tier`, `owner` and `last.verdict` take their listed values | `Test` |
| REH-ROWS | the registry has no duplicate row, no row for a `fromToolchain` entry and no row whose entry or key the manifest no longer has | acceptance |
| REH-ROWS-MISSING | a manifest entry or non-null toolchain key the `entries[]` rule names has no row: a `report` finding, never a failure, because the roll that follows adds the row with the new MAJOR (roll step 3; `Pwsh_Maint_Roll_AddsMissingRehearsalRows`) | acceptance (report) |
| REH-RECORDS | every `last.record` path exists, parses as `zenith2.upgrade-rehearsal`, and its `dependency`, `toVersion`, `verdict` and `revisitStage` equal the row's | `Test` |
| REH-NONEWER | a record with verdict `no-newer` has `newestUpstream` = `fromVersion` = `toVersion`, empty `builds`, `suites`, `measurements` and `adaptations`, and a `fromVersion` equal to the entry's pin in `Zenith2/ThirdParty/manifest.json` at `baseCommit` (`git show <baseCommit>:Zenith2/ThirdParty/manifest.json`) | `Test` |
| REH-OVERDUE | no row is overdue at the accepted stage; at Stage 6 every row has `last.stage ≥ 5` | acceptance |

## Rehearsal plan

The roll writes the plan of the next stage (`rehearsals` array of the plan file of `Zenith2/Docs/Design/MAINT/Versioning.md` → "Roll step"): one item `{ "name", "section", "tier", "measure", "owner", "dueBy", "reason" }` per entry with `dueBy ≤ <next stage>`, `reason` one of `cadence`, `first-after-pin`, `pre-qualification`. The human files one `needs-human` TASK per item, or one per bundle (items of one owner whose R5 builds overlap): title `Z2 MAINT rehearse <name> upgrade` (or `… <owner> dependency bundle`), epic the entry's owner epic `Z2 <owner> — <Area name>` ([Z2-N-CONTRACT-506]; MAINT's epic keeps the registry and the plan alone), fix version `Z2 Stage <next stage>`, kind `maintenance`. The mid-sprint integration check adds an item, and its ticket, for any runtime entry an upstream security advisory reaches (`reason` `advisory`).

## Rehearsal procedure

Run by the human (only a human downloads and verifies an archive, `Zenith2/Docs/Design/SHIP/Manifest.md` → "Dependency updates"), in a scratch worktree so that `master` and every lane are untouched:

| Step | Action | Evidence written into the record |
|---|---|---|
| R1 | choose the candidate version (the newest upstream release for `cadence`; the fixed release for `advisory`); read its changelog and list breaking changes. When the newest release equals the pin, record the verdict `no-newer` with `toVersion` = `fromVersion` and go to R10: R2 to R8 do not run | `toVersion`, `upstreamNotes`, and for every verdict `upstreamSource`, `newestUpstream`, `checkedOn` |
| R2 | `git worktree add --detach <scratch dir> HEAD` from the main checkout; `Enter-Zenith2Env.ps1` in it | `baseCommit` |
| R3 | `Update-Zenith2Dependency.ps1 -Name <name> -Version <v> -Url <url>` (or `Update-Zenith2Toolchain.ps1`) in the scratch worktree; review licence changes and pins (exit codes 5 and 6 of SHIP's table) | `licenceChanged`, `pinsChanged` |
| R4 | `Setup-Zenith2.ps1 -Only <name>` | — |
| R5 | `Configure`, then the `Build` presets that reach the entry (its manifest `platforms` and `usage`: `win-debug` and `win-release` for every entry with `windows`; `android-arm64` for an entry with `android`; `Sanitizers -Preset win-asan` and, for an entry with `linux`, `-Preset linux-tsan` when its `usage` is `runtime`), each through its gate step script | `builds[]` |
| R6 | `Test` filtered to the owning libraries' tests and conformance suites (`Test.ps1 -Label <owner label>` where one exists, else the whole `Test` step); for `luau`, SCRIPT's re-qualification list; for a cooking tool, the re-cook of every repository game and the render tests | `suites[]` |
| R7 | for `measure: true` and a changed version alone, the owning area's budget harness on the reference hardware (`Zenith2/Docs/Budgets.md` → "Reference hardware") before (on the base tree) and after, with the procedure of each row | `measurements[]` |
| R8 | list every adaptation the code needed (from the build and test failures of R5 and R6, fixed in the scratch worktree) | `adaptations[]` |
| R9 | decide the verdict | `verdict`, `verdictReason`, `revisitStage` |
| R10 | copy the record into the main checkout at `Zenith2/Docs/Research/Upgrades/<name>/<from>-to-<to>/`, update the registry row's `last`, commit both in the rehearsal ticket's diff, and `git worktree remove <scratch dir>` | — |

Verdict rules:

| Verdict | When | Follow-up the ticket files |
|---|---|---|
| `adopt` | builds, suites and measurements pass with no adaptation | SHIP's upgrade pair, citing the record |
| `adopt-with-changes` | they pass after the listed adaptations | SHIP's upgrade pair; the loop half's Implementation Contract lists the adaptations |
| `defer` | a failure the programme accepts for now (an upstream regression, a budget breach), or an imminent upstream fix | none; `revisitStage` is set and the registry's due stage follows the cadence |
| `reject` | the version is unusable (a licence the policy refuses, a breaking change the design cannot absorb) | a flagged proposal or a `needs-human` ruling ticket when the rejection blocks a security fix |
| `no-newer` | upstream has no release newer than the pin, as the human read it at `upstreamSource` on `checkedOn` | none; the row counts as rehearsed in this stage (`REH-NONEWER` checks the record's shape; the upstream check itself is human evidence, never computed) |

## Rehearsal record

`Zenith2/Docs/Research/Upgrades/<name>/<from>-to-<to>/Rehearsal.json`, written by `New-Zenith2RehearsalRecord.ps1 -Name <name> -From <v> -To <v>` (which fills `dependency`, `section`, `tier`, `owner`, `fromVersion`, `toVersion`, `engineVersion`, `baseCommit` and leaves the rest for the human), and `Rehearsal.md`, the same content as prose with the four sections `## Summary`, `## Results`, `## Adaptations`, `## Verdict`:

<!-- elaboration -->
```json
{
    "schema": "zenith2.upgrade-rehearsal",
    "schemaVersion": 1,
    "dependency": "jolt",
    "section": "dependencies",
    "tier": "runtime",
    "owner": "PHYS",
    "fromVersion": "5.2.0",
    "toVersion": "5.3.0",
    "engineVersion": "4.0.0-dev.12",
    "baseCommit": "<40 hex>",
    "rehearsedOn": "2026-12-01",
    "upstreamSource": "<URL of the upstream release or tag listing the human read>",
    "newestUpstream": "5.3.0",
    "checkedOn": "2026-12-01T09:30:00Z",
    "upstreamNotes": "<one paragraph>",
    "licenceChanged": false,
    "pinsChanged": [],
    "builds": [ { "preset": "win-debug", "result": "pass" } ],
    "suites": [ { "selection": "Test.ps1 -Label physics", "result": "pass", "report": "<path of the kept JUnit summary under the record directory>" } ],
    "measurements": [ { "row": "<Z2-B- row ID>", "hardware": "reference-desktop", "backend": "vulkan", "before": 1.92, "after": 1.88, "unit": "ms", "withinTolerance": true } ],
    "adaptations": [ { "file": "Zenith2/Engine/ZenithPhysics_Jolt/Internal/Zenith_JoltWorld.cpp", "change": "<one sentence>" } ],
    "verdict": "adopt",
    "verdictReason": "<one sentence>",
    "revisitStage": null
}
```

`result` is `pass`, `fail` or `skipped` (a preset the entry's platforms exclude). `hardware` is `reference-desktop` or `s24`; `backend` is `vulkan`, `d3d12` or `null` (a row whose procedure is backend-independent), because the Windows rows are judged per backend ([Z2-N-BUDGET-012]). `upstreamSource` (the URL or name of the upstream release index read), `newestUpstream` (the newest release version text as seen there) and `checkedOn` (UTC, ISO 8601) are required for every verdict. A `no-newer` record has empty `builds`, `suites`, `measurements` and `adaptations`, `newestUpstream` = `fromVersion` = `toVersion`, and `baseCommit` is the commit at which upstream was checked (rule `REH-NONEWER`). No test count is recorded ([Z2-N-CONTRACT-408]). The record is evidence written by a human; no worker computes any of its values.

## Sample games

The repository games are the representative sample games: Minimal (the proving game), the five workload games and the three flagship slices (`Zenith2/Docs/Design/MOD.md` → "Repository games"). Every game builds in every preset its platforms allow on every change ([Z2-N-ARCH-029]); MAINT keeps the set representative and checks that each game exists, keeps its platforms and boots from the stage it is due.

### Representativeness

<!-- elaboration -->
The set is representative when, from the stage each element lands:

1. every `KIND MODULE` library is selected by at least one repository game;
2. every ECS backend (`BACKEND_OF ZenithECS`) is the `backends.ecs` of at least one game;
3. every physics backend is the `backends.physics` of at least one game on at least one platform;
4. both platforms are covered by every game whose `platforms` the matrix lists as both;
5. every game has at least one `Tests/*.gametest.json`, so the `Test` step boots it headless (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Game test registration").

MOD's selection tables already satisfy rules 1 to 3 at their Stage 4 state; the check makes a drift visible in the diff that causes it.

## Sample games file

`Zenith2/Tools/Maintenance/sample_games.json`:

<!-- elaboration -->
```json
{
    "schema": "zenith2.sample-games",
    "schemaVersion": 1,
    "comment": "The representative sample games; mirrors Zenith2/Docs/Design/MOD.md \"Repository games\". See Zenith2/Docs/Design/MAINT/Upkeep.md \"Sample games file\".",
    "games": [
        { "name": "Minimal", "owner": "MOD", "role": "proving", "firstBuildStage": 2, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "WorkloadIndoor", "owner": "RPROD", "role": "workload", "firstBuildStage": 3, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "WorkloadOutdoor", "owner": "RPROD", "role": "workload", "firstBuildStage": 3, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "WorkloadCharacters", "owner": "ANIM", "role": "workload", "firstBuildStage": 3, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "WorkloadDense", "owner": "ECS", "role": "workload", "firstBuildStage": 3, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "WorkloadUI2D", "owner": "UI", "role": "workload", "firstBuildStage": 3, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "OpenWorldSlice", "owner": "GAME", "role": "flagship", "firstBuildStage": 4, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "CinematicSlice", "owner": "CINE", "role": "flagship", "firstBuildStage": 4, "platforms": [ "android", "windows" ], "minGametests": 1 },
        { "name": "Game2D", "owner": "TWOD", "role": "flagship", "firstBuildStage": 4, "platforms": [ "android", "windows" ], "minGametests": 1 }
    ],
    "pending": [
        { "kind": "module", "name": "Streaming", "selectedByStage": 4 }
    ]
}
```

| Member | Rule |
|---|---|
| `games[].firstBuildStage` | the stage by whose acceptance the game builds and boots; a game is "due" while `firstBuildStage < MAJOR` |
| `games[].owner` | the owning area of `Zenith2/Games/<name>/` in `subsystem_owners.json` |
| `games[].platforms` | equals the game's `game.json` `platforms`, byte-ordered |
| `pending[]` | a module (`kind` `module`, `name` its `MODULE_NAME`) or a backend (`kind` `ecs-backend` or `physics-backend`, `name` its `BACKEND_NAME`) that exists and that no game selects yet; legal while `selectedByStage ≥ MAJOR` |

<!-- assumption: pending GAME -->
`firstBuildStage` 4 for the three flagship slices and 3 for the workload games follows MOD's "Stage 3 selection" and "full selection from Stage 4" (`Zenith2/Docs/Design/MOD.md` → "Workload games", "Flagship slices"); GAME, CINE and TWOD create their slice's `game.json` and `CMakeLists.txt` by the Stage 4 acceptance at the latest (cross-area note MT6). A slice that lands earlier builds from then on; the stage here is the latest stage at which the check starts requiring it.
<!-- /assumption -->

## Build matrix

Which games build on which build preset, and through which mechanism, by stage. "Gate" means a gate step builds it on every diff whose paths select that step; "acceptance" means `Run-Committed.ps1 -Acceptance -Stage N`; "device" means a `needs-human` device run.

<!-- elaboration -->
| Build preset | Configuration | Minimal (from S2) | Workload games (from S3) | Flagship slices (from S4) | Mechanism |
|---|---|---|---|---|---|
| `win-debug` | Debug | built, booted (`Game_<Name>_<Stem>` in `Test`) | built, booted | built, booted | gate, every Z2 diff (`Build -Preset win-debug`, `Test`) |
| `win-release` | Release | built, booted | built, booted | built, booted | gate, every Z2 diff |
| `win-asan` | Sanitize | built, booted | built, booted | built, booted | gate when `Zenith2Memory`'s `Sanitizers -Preset win-asan` line runs (from Stage 2); acceptance |
| `win-shipping` | Shipping | built | built | built | `TestSeparationCheck` and `Libgraph`'s LG9 (Shipping symbols) when their inputs change; acceptance; packaging from S6 |
| `android-arm64` | Release | built (Gradle APK) | built | built | gate from Stage 2 (`Build -Preset android-arm64`, `ApkCheck`) |
| `android-arm64-debug` | Debug | built | built | built | device debugging alone |
| `android-arm64-shipping` | Shipping | built | built | built | acceptance from S6; packaging |
| `android-arm64-hwasan` | Hwasan | built | built | built | device (S24 HWASan runs) |
| `android-x86_64` | Debug | built | built | built | emulator smoke alone; never evidence ([Z2-N-ARCH-216]) |
| `linux-tsan` | Sanitize | — | — | — | GPU libraries and games are absent from the preset ([Z2-N-ARCH-173]) |

In addition, the CI integration smoke boots every workload and flagship game headless for 300 frames when a pushed change reaches an engine path ([Z2-N-BUDGET-354], Z2-B-PROC-030), and each stage's mid-sprint integration check runs the flagship playthrough scripts on the reference desktop and the S24.

A change that breaks any repository game's build or boot fails the gate of the diff that makes it, and the diff fixes the game (the game's content, never an engine workaround that hides the defect, and never by removing the game from the build); there is no "known broken" sample game.

## Sample game checks

<!-- elaboration -->
| Rule ID | Rule | Run in |
|---|---|---|
| SG-SCHEMA | the file obeys the schema; `games[]` and `pending[]` are byte-ordered by `name` | `Test` |
| SG-OWNER | each game's `owner` equals the owner `Get-Zenith2Owner` returns for `Zenith2/Games/<name>/` | `Test` |
| SG-DUE | a due game has `Zenith2/Games/<name>/game.json` and `CMakeLists.txt` | `Test` |
| SG-PLATFORMS | a due game's `game.json` `platforms` equals `platforms` | `Test` |
| SG-GAMETESTS | a due game has at least `minGametests` files `Tests/*.gametest.json` | `Test` |
| SG-MODULES | every `KIND MODULE` library under `Zenith2/Engine/` (read from its `CMakeLists.txt`'s `MODULE_NAME`) is in some game's `modules`, or is `pending` with `selectedByStage ≥ MAJOR` | `Test` |
| SG-BACKENDS | every ECS and physics backend library (its `BACKEND_OF` and `BACKEND_NAME`) is selected by some game, or is `pending` likewise | `Test` |
| SG-UNLISTED | a `Zenith2/Games/<name>/game.json` absent from `games[]`: report in `Test`, failure in acceptance | `Test` (report), acceptance (fail) |
| SG-PENDING-STALE | a `pending` item some game now selects: report | `Test` |

The checks read `game.json` and `CMakeLists.txt` text directly (CMake's configure output is not needed), so they run before the first configure of a fresh tree. When MOD's selection tables change, the same diff edits `sample_games.json` (cross-area note MT5).

## Gate headroom triage

A warm phase whose measured time exceeds 80 % of its cap on three consecutive tickets triggers a MAINT ticket to restore headroom, filed by the tick orchestrator ([Z2-N-BUDGET-353]).

<!-- elaboration -->
The ticket the orchestrator files:

| Field | Value |
|---|---|
| title | `Z2 MAINT gate headroom: <measure\|verify> phase at <p> % of its cap` (p is the highest of the three, rounded down) |
| type, kind | TASK, `maintenance` |
| epic, category | `Z2 MAINT — Engine maintenance`, `Zenith2` |
| labels | `needs-human` |
| sprint, fix version | the current sprint and its fix version |
| complexity, risk, points | SIMPLE, LOW, 1 |
| body `## Goal` | restore the phase below 80 % of Z2-B-PROC-002 (measure) or Z2-B-PROC-003 (verify) |
| body `## Validation` | the three tickets' keys and, for each, the `steps[]` table of its `tick_gates.json` (step, seconds), copied verbatim |

Triage procedure, by the human:

1. Rank the steps by their median seconds over the three runs; mark each step whose median grew by more than 20 % over the median of the three runs before them.
2. For each marked step, classify the cause: a cold start counted as warm (a `cold` report missed by the Build step), a test above 10 seconds, a test executable newly added to the `Test` step, a build-time regression (a header-only library pulled into many TUs), a slow gate script.
3. File one follow-up per cause: an ordinary ticket on the owning area's epic for a code cause (split or shorten a test, cut an include fan-out), or a PROC `needs-human` ticket for a gate cause (move a suite behind a change-triggered category, fix a gate script, re-allocate a step timeout row through its producer's procedure).
4. Commit the triage note `Zenith2/Docs/Research/GateHeadroom/<UTC date>-<phase>.md` (the three tables, the ranking, the causes and the follow-up Draft-IDs) in the ticket's diff.

## Stage acceptance and mid-sprint steps

MAINT's steps in the PROC-owned acceptance and mid-sprint tickets (cross-area note MT3; PROC adopts them in its revision, ruling MAINT-R1-RULINGS R5). Stages 0 and 1 run only the ownership audit, because the engine version header and the MAINT data files land in Stage 2.

<!-- elaboration -->
| # | Step of `Z2 S<N> stage acceptance` (N ≥ 2) | Command | Order |
|---|---|---|---|
| A1 | release step | `Set-Zenith2EngineRelease.ps1 -Stage <N> -Ticket <id>` (at N = 7 it also lists the Stage 7 declarations) | first |
| A2 | surface growth review and baseline re-pin | `Run-Committed.ps1 -Acceptance -Stage <N>` (acceptance suites, `Libgraph` without ticket context), `New-Zenith2SurfaceGrowthReport.ps1 -Stage <N>`, the disposition, the commit of the report, then the re-pin `Run-Committed.ps1 -Acceptance -Stage <N> -WriteBaseline` on the clean tree and the commit of the baseline (`Zenith2/Docs/Design/MAINT/GateData.md` → "Baseline re-pinning") | after A1 |
| A3 | complexity calibration (Stage 4 to Stage 7) | the MAINT ruling ticket and its PROC apply ticket of `Zenith2/Docs/Design/MAINT/GateData.md` → "Complexity calibration", both Done | after A2 |
| A4 | MAINT acceptance checks | `Test-Zenith2Maintenance.ps1 -Acceptance -Stage <N>`, the full set of `Zenith2/Docs/Design/MAINT/Tests.md` → "Acceptance checks", which reads the products of A2 and A3 | after A3 |
| A5 | roll (N ≤ 6) | `Invoke-Zenith2VersionRoll.ps1 -Stage <N> -Ticket <id>` (its precondition re-runs A4), then filing the plan's tickets | last |

| # | Step of `Z2 S<N> mid-sprint integration check` | Command |
|---|---|---|
| M1 | maintenance status | `Test-Zenith2Maintenance.ps1 -Report`; attach `status.md` to the ticket's evidence |
| M2 | advisory review | read the upstream security advisories of every `runtime` entry of `rehearsals.json`; add an `advisory` plan item and file its rehearsal ticket for each affected pinned version |
| M3 | due-work review | from `status.md`: removals due this stage not yet Done, experimental blocks expiring this stage, rehearsals due this stage not yet Done; file a BUG on the owning epic for each that has no ticket |
| M4 | Stages 0 and 1: ownership audit | `Test-Zenith2Maintenance.ps1 -Check Ownership` once the file exists; before Stage 2, the epic list of `zagent report epic --project Z2` against the Inventory's area table |
