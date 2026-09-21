---
id: "97666154-84e7-4a08-8950-b5ab65befa01"
key: "ZEN-85"
type: "TASK"
title: "Make the aux tools (FluxCompiler, font libraries) build green so a whole solution can be built"
category: "Engine"
parent: "ZEN-1"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 85
labels: ["tech-debt"]
fixVersions: []
blocks: []
updatedAt: "2026-09-21T12:24:30.480Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
CLAUDE.md: *"Always build with `/t:<Game>`, never the whole solution: the aux tools (FluxCompiler / font libs) present in the sln are pre-existing-red in `ToolsEnabled=True`."* A solution that cannot be built whole hides every error in the projects nobody targets, and `release-build.yml` and the packaging path have to enumerate targets by hand.

Make FluxCompiler and the font dependency projects (`Sharpmake_FreeType.cs`, `Sharpmake_Msdfgen.cs`, `Sharpmake_MsdfAtlasGen.cs`) build green in every `ToolsEnabled=True` configuration, or exclude them from the configurations they cannot support with the reason in the Sharpmake file; then build `Build\zenith_engine_win64.sln` and one game solution WHOLE as the proof and record it in `Docs/BuildSystem.md`, retiring the "never the whole solution" rule.

## Definition of Done
- [ ] The engine solution and one game solution build whole in `Vulkan_vs2022_Debug_Win64_True` with zero errors
- [ ] `Docs/BuildSystem.md` and root `CLAUDE.md` drop the never-whole-solution rule or state exactly which project is excluded and why
- [ ] Every game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Done** — merged to `master` as `8440514d`. All 15 Engine gate lines green
(verify phase, 0 skipped, 0 deferred, 0 flakes). Guard clean; gate selection
current; gate verdict fresh.

**The ticket's premise was false.** Measured 2026-09-21 in
`Vulkan_vs2022_Debug_Win64_True`: `Build/zenith_engine_win64.sln` and
`Games/Combat/combat_win64.sln` both build **whole, exit 0, zero errors**, and
FluxCompiler, FreeType, msdfgen and msdf-atlas-gen all compile green in every
solution they appear in. The rule root `CLAUDE.md` carried — *"never the whole
solution: the aux tools … are pre-existing-red in `ToolsEnabled=True`"* — is
retired across `CLAUDE.md`, `Docs/BuildSystem.md`, `Docs/GameProjects.md` and
the engine-build comment in `Tools/ZenithCli/ZenithCli.psm1`. `/t:<target>`
stays the recommended default; only the stated reason changed, from a
workaround to a scoping preference.

**The one genuinely red whole-solution build is TilePuzzle, and neither failing
project is an aux tool.** `TilePuzzleLevelGen` has bit-rotted against engine API
drift (21 compile errors); `TilePuzzleRegistryViewer` fails to link without the
`Project_*` composition-root symbols (6 errors). `zenith build TilePuzzle` is
unaffected — it passes `/t:TilePuzzle`, which is exactly why the rot went
unnoticed. Filed as **ZEN-103**.

**Also documented**: the nine projects a `_True` engine solution build
legitimately skips (7 Sentinels declaring `ToolsEnabled.False` only, all green
in the `_False` sibling; plus the two non-active render backends), and which
other thing builds each — `zenith build engine` covers the Sentinels but neither
backend, which the Engine gate list covers instead.

**No pin moved.** This ticket changes no compiled code; the OBSERVED `Null_` run
reports Combat 2718 ran == 2718 baseline, so `Tools/unit_baselines.json` is
correct unedited.

Review (required: `risk MEDIUM` plus an absence-asserting DoD item) returned
**zero blocking findings**, having re-derived every count from the `.sln` files
and checked the TilePuzzle error narrative against real source. Two defects in
the draft text were caught and fixed before it landed — a misattributed error
count and a fabricated claim about what `zenith build engine` builds; the latter
is recorded as `ZEN-D-027`.

Follow-ups: **ZEN-103** (TilePuzzle tooling), **ZM-117** (Zenithmon docs repeat
the retired claim), and a Suggestions row — `zagent decide` never writes the
Engine decision log, so 23 ZEN rulings exist only on the board and are invisible
to a loop worker that reads rulings from disk.

<!-- agent:worklog:start ZEN-85 -->
## Work Log — 2026-09-21

ran on sonnet/medium via MODERATE+MEDIUM · category Engine

**Outcome**: merged to `master` as `8440514d` · ran on sonnet/medium via MODERATE+MEDIUM

**The ticket's premise was false, and measuring it first is what shaped the work.**
The Goal quotes root `CLAUDE.md`: *"never the whole solution: the aux tools
(FluxCompiler / font libs) present in the sln are pre-existing-red in
`ToolsEnabled=True`."* Measured on this tree before dispatching anything
(`MSBuild <sln> /p:Configuration=<cfg> /p:Platform=x64 -maxCpuCount`,
`MSBUILDDISABLENODEREUSE=1`):

| Solution | Configuration | Exit | Errors |
|---|---|---|---|
| `Build/zenith_engine_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 0 | 0 |
| `Build/zenith_engine_win64.sln` | `Vulkan_vs2022_Debug_Win64_False` | 0 | 0 |
| `Games/Combat/combat_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 0 | 0 |
| `Games/TilePuzzle/tilepuzzle_win64.sln` | `Vulkan_vs2022_Debug_Win64_True` | 1 | 27 |

FluxCompiler, FreeType, msdfgen and msdf-atlas-gen all build **green**. The one
genuinely red whole-solution build is TilePuzzle, and neither failing project is
an aux tool: `TilePuzzleLevelGen` has bit-rotted against engine API drift (21
compile errors — `Zenith_TaskSystem::Inititalise`, an undeclared
`Zenith_TaskArray`, a changed `Zenith_Profiling::Initialise` arity, a
no-longer-static `Zenith_Multithreading::RegisterThread`) and
`TilePuzzleRegistryViewer` fails to link without the `Project_*`
composition-root symbols (6 errors). Filed as **ZEN-103**.

So DoD item 2's SECOND branch — *state exactly which project is excluded and
why* — is the one taken, and the measurements became the worker's spec rather
than the Goal.

Also measured, because "the solution builds whole" overstates itself without it:
a solution build silently skips any project with no `.Build.0` mapping. The
engine solution maps 43 projects to `ActiveCfg` and 34 to `Build.0` (9 skipped);
Combat 36/34; TilePuzzle 38/36. The 9 are the 7 Sentinels (`ToolsEnabled.False`
only — all seven confirmed green in the `_False` sibling) plus the two
non-active render backends. None is broken; each is absent by target declaration.

**What changed**

- `CLAUDE.md` — both assertions of the retired rule (the `/t:<Game>` paragraph
  and the engine-only note) replaced with the measured result; `/t:` kept as the
  recommended default, with the reason changed from "workaround" to "scoping".
- `Docs/BuildSystem.md` — §9 troubleshooting row now names the true cause;
  §11 invariant #2 is no longer an absolute prohibition; new §9 subsection
  "Whole-solution build status (ZEN-85, 2026-09-21)" carries the full evidence,
  the TilePuzzle breakdown and the nine skipped projects with per-project
  coverage.
- `Docs/GameProjects.md` — heading and body reworded off the false premise.
- `Tools/ZenithCli/ZenithCli.psm1` — comment only, above the engine build steps.
  No executable line changed; the `/t:` steps are untouched.

**Files** (`git diff --stat`)

```
 CLAUDE.md                      | 16 ++++++++--
 Docs/BuildSystem.md            | 71 +++++++++++++++++++++++++++++++++++++++++--
 Docs/GameProjects.md           | 14 ++++++---
 Tools/ZenithCli/ZenithCli.psm1 | 12 ++++----
```

**Gates**

All 15 lines from the `Engine` category, `-Phase verify`: exit 0, "all gates
green", 0 skipped, 0 deferred, 0 flakes. `zagent gates` unioned in nothing — the
diff touches no path in any category's `paths` (`Tools/*.ps1` does not match
`Tools/ZenithCli/ZenithCli.psm1`), so the ticket's own Engine list applied.

| Command | Result |
|---|---|
| `zenith build engine` | 0 |
| `zenith build Combat --headless` | 0 |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | 0 |
| `zenith build {Zenithmon,CityBuilder,RenderTest,TilePuzzle} --headless` | 0 |
| `zenith build {DevilsPlayground,ScriptTest,Foundry,Undervault} --headless` | 0 |
| `zenith test Combat --headless` | 0 — 17 passed, 0 failed |
| `run_unit_gate.ps1 -Game Combat` | 0 — **2718 ran, 2717 passed, 0 failed, 1 skipped** |
| `doc_lint.ps1 -Game Zenithmon` | 0 |
| `doc_lint.ps1 -Game DevilsPlayground` | 0 |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/ZenithCli/ZenithCli.psm1` | Covered. Every `zenith build`/`zenith test` gate line imports this module, and gate line 1 (`zenith build engine`) executes the exact `if ($target -ieq 'engine')` branch whose comment changed. The edit is comment-only, so what the gates actually prove is that the module still parses and behaves — which is the real risk for a `.psm1` edit. Also parse-checked directly via `Parser::ParseFile` (clean) and checked against this directory's ASCII-only convention (one non-ASCII line at 465, pre-existing, not mine). |
| `CLAUDE.md` | **Nothing.** No gate reads it. |
| `Docs/BuildSystem.md` | **Nothing.** `doc_lint.ps1` is scoped to `Games/<Game>/Docs` (`$docsDir = Join-Path $gameDir 'Docs'`), so neither `doc_lint` line lints it. |
| `Docs/GameProjects.md` | **Nothing.** Same reason. |

Three of the four files are prose that no gate can read, which is why the
correctness of this ticket rests on the measured builds plus the review pass, not
on the gate colour. Stated explicitly because a green run here is close to
vacuous for those three files.

**Definition of Done**

- [x] The engine solution and one game solution build whole in
      `Vulkan_vs2022_Debug_Win64_True` with zero errors — measured:
      `zenith_engine_win64.sln` exit 0 / 0 errors, `combat_win64.sln` exit 0 /
      0 errors. Caveat recorded in the docs and in `ZEN-D-027`: the engine
      solution builds 34 of its 43 projects in that configuration, the other 9
      being absent by target declaration rather than broken.
- [x] `Docs/BuildSystem.md` and root `CLAUDE.md` drop the never-whole-solution
      rule or state exactly which project is excluded and why — both, in fact:
      the false rule is retired and the exclusions are enumerated with reasons.
      The phrase survives in `Docs/BuildSystem.md` only as explicitly-marked
      history ("The historical rule … is **stale**"), which the reviewer
      confirmed by grep.
- [x] Every game's pinned baseline bumped from an OBSERVED run — **no bump was
      required and none was invented.** This ticket changes no compiled code.
      The OBSERVED `Null_` run reports Combat **2718 ran == 2718 baseline**, so
      `Tools/unit_baselines.json` is correct as it stands and was not edited.
      Recorded this way rather than ticked silently, because "bumped" and
      "verified unchanged" are different facts.

**Durable findings**

- `ZEN-D-027` — a whole-solution build proves nothing about projects the `.sln`
  maps no `Build.0` for, so "solution X builds green" is vacuously true of every
  skipped project; and "covered by another configuration" is a per-project claim
  (`zenith build engine` covers the seven Sentinels but neither backend library).

**Deviations / follow-ups**

- **Two factual defects in the worker's text, found and fixed by the
  orchestrator before the review ran.** (1) It attributed all 27 errors to
  `TilePuzzleLevelGen`; the real split is 21 compile errors there and 6 link
  errors in `TilePuzzleRegistryViewer`. (2) It wrote that each of the nine
  skipped projects is covered by a configuration `zenith build engine` already
  builds — false for the two backend libraries, since that command runs only
  `/t:Zenith` plus `/t:Sentinel*`. The fabricated attribution was replaced with
  the measured one. This is the finding `ZEN-D-027` exists to prevent recurring.
- **Review (required: `risk MEDIUM` + an absence-asserting DoD item) returned
  zero blocking findings.** It independently re-derived all three
  `ActiveCfg`/`Build.0` counts from the `.sln` files and verified the TilePuzzle
  error narrative against real source (`Zenith_TaskSystem.h:187`,
  `Zenith_Multithreading.h:20`, `Zenith_Main.cpp:133`), so the documented
  mechanism is grounded rather than merely plausible. One non-blocking point was
  acted on: the `.psm1` comment claimed `/t:` was used "for speed", which ZEN-85
  never measured and which sat alongside genuinely measured claims. Rewritten to
  rest only on what is structurally true — the Sentinels exist solely in
  `_False` configs, so the command necessarily spans two configurations and no
  single solution build can cover it. The verify phase re-ran after that edit.
- **Corrections made to the ticket's own text before inlining it (I3).** The
  Goal's central claim was reproduced and found false, so the measurements were
  inlined as the specification and the worker was told explicitly to follow them
  over the Goal. DoD item 3 was inlined as written (it already carries the
  corrected "OBSERVED run" phrasing and names no pin site).
- **Scope deliberately not widened.** The retired claim is also asserted in
  `Games/Zenithmon/Docs/{AgentBriefing,BuildEnvironment,OrchestratorPlaybook}.md`.
  Editing `Games/**` would have unioned Zenithmon's whole gate list onto an
  engine documentation ticket, so it is filed as **ZM-117** instead. The worker
  independently found the same three files and correctly reported rather than
  edited them. `Games/Zenithmon/Docs/StartPrompts.md` says "never the whole sln"
  without the false cause and needs no change.
- **No Sharpmake file was edited.** The Goal offered "make them green **or**
  exclude them with the reason in the Sharpmake file"; they were already green,
  so there was nothing to exclude. The existing headers already say "Tools-only,
  Win64 only".
- **A board defect found on the way past, filed as a Suggestions row**:
  `zagent decide` warns that ZEN's `Engine` category declares no `decisionLog`,
  but `zagent.project.json` declares
  `"decisionLog": "Zenith/Docs/EngineDecisionLog.md"` and the claim payload read
  it correctly. Consequence: 23 ZEN rulings exist on the board
  (`ZEN-D-005`…`ZEN-D-027`) and **zero** in the on-disk log, whose own header
  claims every entry lands in both places. A loop worker has no shell and no
  board, so under I3 those rulings are invisible to it — this ticket only
  carried `ZEN-D-021` because the board was dumped and grepped by hand.
- **Incidental corroboration**: the gate log shows `zenith build engine`
  compiling `msdf-atlas-gen`, so the font libraries have been built green by the
  first Engine gate line on every engine ticket all along. The retired claim was
  not merely stale; it was contradicted by the gate list itself.
- No excursions: the changed set is exactly the four files in
  `.zagent/run/ZEN-85/filelist.txt`.
<!-- agent:worklog:end -->
