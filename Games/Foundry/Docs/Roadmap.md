# Foundry Roadmap -- M0..M6 milestone plan (living checklist)

**Document purpose:** the single source of truth for "what's next" in Foundry, joined to the `FD` board by key. One section per TDD milestone (§20); every item carries its board key; the loop picks the next claimable ticket in the ACTIVE sprint. Checkbox discipline: `[x]` = the ticket is Done on the board, its category gates were green, and `Status.md` plus affected durable docs are reconciled; `[~]` = In Progress / In Review; `[ ]` = pending. Never tick on a partial implementation.

**Board:** project **`FD`**. Each milestone heading carries its EPIC key and each item its ISSUE key. The keys are the join between this file and the queue, not a replacement for it -- the design lives in [GameDesignDocument.md](GameDesignDocument.md) and [TechnicalDesignDocument.md](TechnicalDesignDocument.md), the dependencies live on the board as `BLOCKS` links (`zagent blocked --project FD`), and where a checkbox and the board disagree the board wins for status and this file wins for intent. See [Board.md](Board.md) for how the two relate.

**Milestone gate (applies to EVERY milestone):** every child ticket Done with the Foundry category gates green (`Build\regen.ps1` + `zenith build Foundry --headless` + `zenith test Foundry --headless`, plus the unit-gate / doc-lint / SimCore-lint lines once M0-16 lands them), the Foundry pin in `Tools/unit_baselines.json` bumped from an OBSERVED Null run per commit, every golden digest and op-count pin green, and the milestone's acceptance ticket (`human-gate`, designated with `zagent sprint gate`) reviewed by a person.

**Critical path:** M0-01 scaffold -> M0-06 command queue -> M0-08 belts -> M0-10 inserters -> M0-11 machines -> M1-03 Core/Reserve -> M1-07 power -> M1-17 save/load -> M1-25 slice gate -> M2-01 gestures -> M2-11 device session -> M3a fluids -> M3b rails -> M4 threat -> M5 soak -> M6 Ark -> M6-12 ship verification.

**Parallelism:** the M0 device render spike (M0-14/15) runs beside the SimCore spike; engine register items (`--category Engine`, label `engine-gap`) run beside game work in the milestone that needs them (TDD §19 "Needed by"); content tables (label `content`) run beside systems. MSBuild dispatch is SERIAL (one ticket in flight per checkout, I5) -- parallelism is authoring order, not builds.

**Markers:** `needs-human` = a person must do the work (on-device runs, protected-path edits, keystores); `needs-gpu` = build `Vulkan_*_True` and run windowed; `human-gate` = the loop does the job then stops at In Review; `deferred` = skipped by the queue, claimable by name. Every milestone-acceptance ticket is `human-gate` and is the sprint's designated gate.

**Engine-change register (TDD §19) → tickets:** #1 audio M3A-12 + M6-04 · #2 DPI M2-03 · #3 lifecycle hook M2-05 · #4 pacing M2-07 + thermal M5-04 · #5 ASTC M3A-09 · #6 instancing upload M5-03 · #7 async save M3A-10 · #8 ScrollView M2-08 · #9 dynamic texture M3A-13 · #10 safe area M2-04 · #11 haptics M6-06 · #12 big.LITTLE M5-08 (deferred) · #13 ortho camera: not needed, bug to be filed on `ZEN` · #14/#15 IME + keycodes V-05 (deferred) · #16 packaging M6-08.

---

## [FD-1] M0 — Two parallel spikes (the de-risk milestone)

*Sprint:* `FD M0 — Two parallel spikes (the de-risk milestone)` · *Version:* — · *TDD exit:* SimCore spike: a generated 10k-line/50k-item belt city ticks < 2 ms on desktop; digests match across /Od and /O2; op-counts pinned. Device render spike: 60 fps at the §15 budgets on the floor Adreno phone and one Mali device, or the fallback decision (chunk-merged statics / bespoke belt-item renderer) is taken now.

- [ ] **[FD-11]** M0-01 Scaffold: replace the static visual study with the TDD architecture skeleton *(TASK · COMPLEX/HIGH · 8 pts · CRITICAL)*
- [ ] **[FD-12]** M0-02 SimCore boundary lint: L0-only / no-float / no-std / no-g_xEngine grep gate with self-tests *(TASK · SIMPLE/LOW · 3 pts · HIGH)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-13]** M0-03 SimCore containers: FD_Ring (pow2 growable POD ring), FD_SlotMap (generation-checked), FD_Bitset *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-14]** M0-04 Deterministic foundation: FD_Fixed Q16.16, per-tick content conversion, named XorShift32 streams, integer value noise, FNV-1a digests *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-15]** M0-05 Seed Foundry living docs: Status, TestPlan, SaveFormat, Shortfalls, Questions, DecisionLog, AgentBriefing *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `docs`)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-16]** M0-06 Command queue, FD_Sim::Tick phase skeleton, and the accumulator with 3-tick debt clamp *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M0-03 (FD-13), M0-04 (FD-14)
- [ ] **[FD-17]** M0-07 World data model: FD_Chunk 32x32 layers, chunk hashmap + sorted chunk list, occupancy, packed entity handles *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M0-03 (FD-13)
- [ ] **[FD-18]** M0-08 Belts spike: transport lines with head-gap compression, 128-tile cap, wake/sleep, two lanes *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)* -- blocked by M0-06 (FD-16), M0-07 (FD-17)
- [ ] **[FD-19]** M0-09 Belts spike: splitters, lane-precise sideloads, underground belts, circular-run anchors *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M0-08 (FD-18)
- [ ] **[FD-20]** M0-10 Inserters as event-scheduled swings + the timer wheel *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL)* -- blocked by M0-08 (FD-18)
- [ ] **[FD-21]** M0-11 Machine archetype SoA tables, one crafting archetype (furnace), wake links and wake/sleep *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M0-10 (FD-20)
- [ ] **[FD-22]** M0-12 Digest + op-count harness: canned FD_TestFactory scripts, per-module sub-digests every 64 ticks, pinned goldens *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M0-11 (FD-21)
- [ ] **[FD-23]** M0-13 Belt-city generator + SimCore spike exit measurement (10k lines / 50k items, tick < 2 ms desktop, /Od vs /O2 digests equal) *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M0-12 (FD-22)
- [ ] **[FD-24]** M0-14 Device render spike harness: 20k building + 5k moving instances with tint/anim churn on Flux_InstanceGroup + UnifiedMesh *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL · `needs-gpu`)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-25]** M0-15 Device render spike: run on the floor Adreno phone and one Mali device; record numbers; take the fallback decision *(TASK · MODERATE/HIGH · 5 pts · CRITICAL · `needs-human` `needs-gpu`)* -- blocked by M0-14 (FD-24)
- [ ] **[FD-26]** M0-16 Human edits after the scaffold: Foundry unit-gate line, doc-lint game array, foundry-tests.yml workflow *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-human`)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-27]** M0-17 M0 milestone acceptance: both spike exits recorded, go/no-go on the render path *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate`)* -- blocked by M0-13 (FD-23), M0-15 (FD-25)

*Gate:* **[FD-27]** M0-17 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-9] M1 — Vertical slice (Windows, mouse-as-touch)

*Sprint:* `FD M1 — Vertical slice (Windows, mouse-as-touch)` · *Version:* Vertical Slice · *TDD exit:* The GDD first-15-minutes script is playable; autosave snapshot ≤ 100 ms at slice scale; all digests green.

- [ ] **[FD-28]** M1-01 Content tables foundation: FD_Items, FD_Recipes, FD_Archetypes, FD_Techs as constexpr with per-tick conversion + integrity units *(TASK · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M0-04 (FD-14)
- [ ] **[FD-29]** M1-02 Worldgen: seeded integer chunk generation — ground, resource patches, water, trees, oil seeps, starting-area guarantees *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M0-07 (FD-17), M0-04 (FD-14)
- [ ] **[FD-30]** M1-03 The Foundry Core, Build Reserve, Depot flag, Core Fabricator queue, and tap-harvest bootstrap *(STORY · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M1-01 (FD-28), M0-11 (FD-21)
- [ ] **[FD-31]** M1-04 Mining: burner and electric drills, belt/adjacent output, patch depletion deltas *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M1-03 (FD-30), M1-02 (FD-29)
- [ ] **[FD-32]** M1-05 Smelting chain: stone / steel / electric furnaces; ore→plate, iron→steel 5:1, stone→brick *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `content`)* -- blocked by M1-04 (FD-31)
- [ ] **[FD-33]** M1-06 Assemblers Mk1–Mk3 with per-machine recipe selection command and recipe categories *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M1-03 (FD-30)
- [ ] **[FD-34]** M1-07 Electric networks: pole union-find, coverage, deferred BFS rebuild, Q16.16 satisfaction, brownout scaling, stats rings *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M1-03 (FD-30)
- [ ] **[FD-35]** M1-08 Steam power arc: offshore pump → boiler → steam engine (direct-adjacency fluid seam), plus solar panel and accumulator *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M1-07 (FD-34)
- [ ] **[FD-36]** M1-09 Research: labs, science packs, red-tier tech tree data, one active research, unlock gating, queue unlock *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M1-06 (FD-33)
- [ ] **[FD-37]** M1-10 Presentation core: PostTickExtract snapshots, visible-set extraction per chunk, slot ledger over Flux_InstanceGroup, interpolation *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)* -- blocked by M0-15 (FD-25), M1-02 (FD-29)
- [ ] **[FD-38]** M1-11 Workshop-tier movers: belt item nuggets from head-gap + prefix gaps, inserter arm poses, tiled ground with build-grid shader overlay *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `needs-gpu` `visual`)* -- blocked by M1-10 (FD-37)
- [ ] **[FD-39]** M1-12 Camera: steep-pitch perspective controller (CB_CameraController port) with pan/zoom sources and the three tier thresholds *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M0-01 (FD-11)
- [ ] **[FD-40]** M1-13 Shell input → commands (mouse-as-touch): tap select, drag pan, tap-to-place, drag-runs with auto-orient/auto-bend, rotate *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M1-12 (FD-39), M0-06 (FD-16)
- [ ] **[FD-41]** M1-14 UI: HUD chrome, build palette drawer (category tabs × item grid, search), tool strip, ghost with 1.5-tile reticle offset *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M1-13 (FD-40)
- [ ] **[FD-42]** M1-15 UI: Inspector panel (identity, recipe picker, contents, throughput sparkline, power satisfaction, actions) + hold-to-peek card *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M1-14 (FD-41)
- [ ] **[FD-43]** M1-16 UI: Tech tree as a vertical tier list, research pill, shared recipe picker *(TASK · MODERATE/LOW · 5 pts · MEDIUM)* -- blocked by M1-09 (FD-36), M1-14 (FD-41)
- [ ] **[FD-44]** M1-17 Save/load: FDSV length-framed module codec, Zenith_SaveData slot integration, compiled-blob migration test discipline *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL)* -- blocked by M1-09 (FD-36), M1-07 (FD-34)
- [ ] **[FD-45]** M1-18 Periodic autosave (snapshot ≤ 100 ms tracked) + save-slots UI (3 manual + autosave rotation, damaged ≠ empty) *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M1-17 (FD-44)
- [ ] **[FD-46]** M1-19 Objectives: the first-15-minutes scripted objective pane (7 steps) driven from sim state, dismissible forever *(TASK · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M1-16 (FD-43), M1-08 (FD-35)
- [ ] **[FD-47]** M1-20 Audio emissions through Zenith_AudioBus::EmitSound for sim events with camera-distance loudness *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)* -- blocked by M1-13 (FD-40)
- [ ] **[FD-48]** M1-21 Undo/redo: inverse commands, 50-group stack, Reserve refunds, HUD buttons *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M1-13 (FD-40)
- [ ] **[FD-49]** M1-22 Placeholder art kit: deterministic FD_KitGen low-poly silhouettes for M1 archetypes + item icon atlas with colorblind checker *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `needs-gpu` `visual` `content`)* -- blocked by M1-10 (FD-37)
- [ ] **[FD-50]** M1-23 Alerts foundation: aggregated alert model (starved, no power, output full), alert stack, tap-to-jump, back-tap *(TASK · MODERATE/LOW · 5 pts · MEDIUM)* -- blocked by M1-15 (FD-42)
- [ ] **[FD-51]** M1-24 Perf tracking from M1: per-system tick timing report artifact and the budget table in Docs/Status.md *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `docs`)* -- blocked by M1-17 (FD-44)
- [ ] **[FD-52]** M1-25 M1 slice gate test: FD_VerticalSlice_Test drives the first-15-minutes script, autosave ≤ 100 ms, all digests green *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M1-19 (FD-46), M1-18 (FD-45), M1-21 (FD-48), M1-23 (FD-50)
- [ ] **[FD-53]** M1-26 M1 milestone acceptance: slice playthrough review on Windows *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate` `needs-gpu`)* -- blocked by M1-25 (FD-52), M1-22 (FD-49), M1-11 (FD-38)

*Gate:* **[FD-53]** M1-26 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-3] M2 — Touch & device

*Sprint:* `FD M2 — Touch & device` · *Version:* Vertical Slice · *TDD exit:* The slice is fully playable by touch on the floor phone, survives kill-after-background, holds 60 fps; the Android digest run is green on device.

- [ ] **[FD-54]** M2-01 Gesture recognizer over Zenith_Pointers: pinch, two-finger pan, long-press, double-tap, two-finger tap, hold-to-peek *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M1-13 (FD-40)
- [ ] **[FD-55]** M2-02 Touch build flow: release ≠ commit, drag-run commit on release with 3 s undo toast, radial long-press menus *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M2-01 (FD-54)
- [ ] **[FD-56]** M2-03 Engine #2: UI DPI scaling — apply the canvas scale factor to layout, hit-testing and font sizing (opt-in per canvas) *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL · `engine-gap` · category Engine)*
- [ ] **[FD-57]** M2-04 Engine #10: safe-area insets exposed on the canvas (Android WindowInsets / display cutout) *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap` · category Engine)* -- blocked by M2-03 (FD-56)
- [ ] **[FD-58]** M2-05 Engine #3: Project_OnLifecyclePause hook invoked from APP_CMD_PAUSE / APP_CMD_SAVE_STATE *(TASK · SIMPLE/MEDIUM · 3 pts · CRITICAL · `engine-gap` · category Engine)*
- [ ] **[FD-59]** M2-06 Foundry lifecycle: synchronous pause-save ≤ 300 ms, backgrounded = paused, resume discards all debt, resume-exact status strip *(STORY · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M2-05 (FD-58), M1-18 (FD-45)
- [ ] **[FD-60]** M2-07 Engine #4 basics: target-framerate control (30/60) via paced present + ANativeWindow_setFrameRate hint *(TASK · MODERATE/MEDIUM · 5 pts · HIGH · `engine-gap` `needs-gpu` · category Engine)*
- [ ] **[FD-61]** M2-08 Engine #8: ScrollView fixes — scroll offset in hit-testing, drag-vs-tap arbitration window, textured-quad clipping, units *(BUG · MODERATE/LOW · 5 pts · HIGH · `engine-gap` · category Engine)*
- [ ] **[FD-62]** M2-09 Fat-finger law audit + safe-area HUD anchoring + automated layout assertions *(TASK · SIMPLE/LOW · 3 pts · HIGH)* -- blocked by M2-03 (FD-56), M2-04 (FD-57), M2-02 (FD-55)
- [ ] **[FD-63]** M2-10 Android build & cook for Foundry: AGDE configs green, uncompressed RGBA8 stopgap textures, x86_64 emulator boot *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL · `needs-gpu`)* -- blocked by M0-14 (FD-24)
- [ ] **[FD-64]** M2-11 Device session: Android digest run green on the floor phone, kill-after-background survives, 60 fps held, touch slice playthrough *(TASK · MODERATE/HIGH · 5 pts · CRITICAL · `needs-human` `needs-gpu`)* -- blocked by M2-10 (FD-63), M2-06 (FD-59), M2-02 (FD-55), M2-07 (FD-60)
- [ ] **[FD-65]** M2-12 Settings screen (battery 30/60, camera snap, colorblind palettes, alert mutes) + new-game world-gen options + title/new/continue flow *(TASK · MODERATE/LOW · 5 pts · MEDIUM)* -- blocked by M2-07 (FD-60), M1-18 (FD-45)
- [ ] **[FD-66]** M2-13 Touch automated test suite: FD_TouchBuild, FD_TouchInspect, FD_PinchZoomTiers, FD_TwoFingerUndo on the Null backend *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M2-02 (FD-55), M2-09 (FD-62)
- [ ] **[FD-67]** M2-14 M2 milestone acceptance: touch-playable slice on the floor phone *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate`)* -- blocked by M2-11 (FD-64), M2-13 (FD-66), M2-12 (FD-65)

*Gate:* **[FD-67]** M2-14 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-4] M3a — Fluids + circuits + construction tools

*Sprint:* `FD M3a — Fluids + circuits + construction tools` · *Version:* Feature Complete · *TDD exit:* Oil processing playable; blueprint stamp/undo digest-tested; autosave hitch-free.

- [ ] **[FD-68]** M3A-01 Fluid networks: pipe/underground union, per-network (type, volume, capacity), instant equalize, one-type rule, 320-tile extent cap, deterministic split *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M1-08 (FD-35)
- [ ] **[FD-69]** M3A-02 Fluid endpoints on networks: offshore pump, pumpjack, boiler, refinery, chemical plant, pump, storage tank; migrate the M1 steam chain *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M3A-01 (FD-68)
- [ ] **[FD-70]** M3A-03 Oil arc content: petroleum/heavy/light, plastic, sulfur, sulfuric acid, lubricant, advanced circuits, processing units, blue science techs *(TASK · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M3A-02 (FD-69)
- [ ] **[FD-71]** M3A-04 Circuit networks: red/green wire union-find, sorted signal vectors, double-buffered evaluation with 1-tick delay, entity read/enable hooks *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M1-21 (FD-48)
- [ ] **[FD-72]** M3A-05 Combinators (arithmetic / decider / constant), combinator editor, wiring mode UX, live signal table *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM)* -- blocked by M3A-04 (FD-71)
- [ ] **[FD-73]** M3A-06 Copy/paste with rotation and the settings paintbrush, riding the command queue *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M1-21 (FD-48), M2-02 (FD-55)
- [ ] **[FD-74]** M3A-07 Blueprints: library with icon names, stamp with rotation/flip, ghosts auto-filled nearest-first from the Reserve, library UI *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M3A-06 (FD-73)
- [ ] **[FD-75]** M3A-08 Deconstruction planner: marquee with filters (only trees / only belts …), timed dismantle, refund, drag-confirm *(TASK · MODERATE/LOW · 5 pts · MEDIUM)* -- blocked by M3A-06 (FD-73)
- [ ] **[FD-76]** M3A-09 Engine #5: ASTC/ETC2 texture export, per-platform format selection at cook, textureCompressionBC device check with a named failure *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `engine-gap` `needs-gpu` · category Engine)*
- [ ] **[FD-77]** M3A-10 Engine #7: Zenith_SaveData::SaveAsync — owned snapshot buffer, task-thread CRC + write, main-thread completion, same-slot serialization *(TASK · MODERATE/MEDIUM · 5 pts · HIGH · `engine-gap` · category Engine)*
- [ ] **[FD-78]** M3A-11 Hitch-free autosave on SaveAsync + save-size budget tracking *(TASK · SIMPLE/LOW · 3 pts · HIGH)* -- blocked by M3A-10 (FD-77), M1-18 (FD-45)
- [ ] **[FD-79]** M3A-12 Engine #1 phase 1: audio backend behind EmitSound — WAV/ogg decode, mixer with buses and ducking, listener attenuation, WASAPI output *(STORY · COMPLEX/HIGH · 13 pts · HIGH · `engine-gap` · category Engine)*
- [ ] **[FD-80]** M3A-13 Engine #9: dynamic 2D texture region-update API for map overlays *(TASK · MODERATE/LOW · 5 pts · MEDIUM · `engine-gap` `needs-gpu` · category Engine)*
- [ ] **[FD-81]** M3A-14 Map-tier representation: per-chunk quad InstanceGroup (or dynamic texture), resource patches, overlay chip framework *(STORY · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu` `visual`)* -- blocked by M1-10 (FD-37), M3A-13 (FD-80)
- [ ] **[FD-82]** M3A-15 Logistics-tier representation: simplified building meshes, belt flow ribbons with content tint, markers; tier switch before budgets inflate *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `needs-gpu` `visual`)* -- blocked by M1-11 (FD-38)
- [ ] **[FD-83]** M3A-16 Digest scenarios for M3a: blueprint stamp/undo, copy-paste rotation, fluid split, circuit permutation *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M3A-07 (FD-74), M3A-05 (FD-72), M3A-02 (FD-69)
- [ ] **[FD-84]** M3A-17 M3a milestone acceptance: oil processing playable, blueprints digest-tested, autosave hitch-free *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate` `needs-gpu`)* -- blocked by M3A-16 (FD-83), M3A-11 (FD-78), M3A-08 (FD-75), M3A-14 (FD-81), M3A-15 (FD-82)

*Gate:* **[FD-84]** M3A-17 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-5] M3b — Trains + drones

*Sprint:* `FD M3b — Trains + drones` · *Version:* Feature Complete · *TDD exit:* A two-line rail base with intersections runs deadlock-free for a 2-hour soak; drone throughput matches content-table spec exactly (op-counts).

- [ ] **[FD-85]** M3B-01 Rail graph: discrete piece catalog, drag-router choosing pieces, junction formation, integer edge lengths *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M2-02 (FD-55)
- [ ] **[FD-86]** M3B-02 Stations (icon-named), train composition (loco + cargo/fluid wagons), schedules with wait conditions, schedule UI *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M3B-01 (FD-85)
- [ ] **[FD-87]** M3B-03 Train kinematics, A* pathfinding, block signals with reservation and braking lookahead, amortized replans *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)* -- blocked by M3B-02 (FD-86)
- [ ] **[FD-88]** M3B-04 Chain signals, deadlock detection with the "train stuck" alert, and the block-colouring overlay in signal mode *(STORY · COMPLEX/HIGH · 8 pts · HIGH)* -- blocked by M3B-03 (FD-87)
- [ ] **[FD-89]** M3B-05 Rail presentation: spline track visuals, train/wagon interpolation, station icons as screen-projected labels *(TASK · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu` `visual`)* -- blocked by M3B-02 (FD-86), M3A-15 (FD-82)
- [ ] **[FD-90]** M3B-06 Logistics networks: Drone Port coverage union, container roles (provider/requester/storage), matcher budget round-robin *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M3A-03 (FD-70)
- [ ] **[FD-91]** M3B-07 Carrier drones: event-scheduled straight-line flights, simplified charging, per-network throughput cap, logistics panel UI *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)* -- blocked by M3B-06 (FD-90)
- [ ] **[FD-92]** M3B-08 Two-line rail soak scenario (2-hour equivalent, 216,000 ticks) deadlock-free + drone throughput spec check *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M3B-04 (FD-88), M3B-07 (FD-91)
- [ ] **[FD-93]** M3B-09 Fluid wagons and tank loading through pumps *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)* -- blocked by M3B-02 (FD-86), M3A-02 (FD-69)
- [ ] **[FD-94]** M3B-10 Green science content: logistics techs (splitter priority/filter, underground tiers, fast/filter inserters, rail, stations, signals, drone port, requester) *(TASK · SIMPLE/LOW · 3 pts · HIGH · `content`)* -- blocked by M1-09 (FD-36)
- [ ] **[FD-95]** M3B-11 M3b milestone acceptance: rail base soak and drone spec check reviewed *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate` `needs-gpu`)* -- blocked by M3B-08 (FD-92), M3B-05 (FD-89), M3B-09 (FD-93), M3B-10 (FD-94)

*Gate:* **[FD-95]** M3B-11 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-6] M4 — Threat

*Sprint:* `FD M4 — Threat` · *Version:* Feature Complete · *TDD exit:* Scripted max-wave siege (5k units) inside the §1 threat budget on device; Peaceful toggle verified; evolution digest-stable.

- [ ] **[FD-96]** M4-01 Pollution cellular automaton: per-chunk u32, every 8 ticks produce/diffuse/absorb over the sorted chunk list, Map-tier overlay *(STORY · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M3A-14 (FD-81)
- [ ] **[FD-97]** M4-02 Nests, worms, colony expansion, evolution function, spawn tables, Peaceful toggle *(STORY · MODERATE/MEDIUM · 8 pts · CRITICAL · `content`)* -- blocked by M4-01 (FD-96), M2-12 (FD-65)
- [ ] **[FD-98]** M4-03 Attack groups: hierarchical chunk-graph A*, tile corridor refinement, 1–2 repaths/tick cap, the GetCorridorDirection seam *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)* -- blocked by M4-02 (FD-97)
- [ ] **[FD-99]** M4-04 Unit steering: spatial-hash separation, integer steering toward the corridor, off-screen bulk advance, 5,000 active cap, threat shard fan-out *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)* -- blocked by M4-03 (FD-98)
- [ ] **[FD-100]** M4-05 Combat and defense: melee/spitter attacks vs occupancy + HP, walls/gates, gun turrets (ammo) + laser turrets (power), event-driven target acquisition *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)* -- blocked by M4-04 (FD-99)
- [ ] **[FD-101]** M4-06 Radar (map visibility + early warning) and military content: black science, ammo lines, turret/wall techs *(TASK · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M4-05 (FD-100)
- [ ] **[FD-102]** M4-07 Assault drones: Drone Bay, tap-a-target-zone command, squadron flight and attack, consumed on use *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M4-06 (FD-101)
- [ ] **[FD-103]** M4-08 Threat alerts UX: attack in progress, edge-of-screen directional glow, turret out of ammo, train stuck, aggregation, per-category mute, haptic seam *(TASK · MODERATE/LOW · 5 pts · HIGH)* -- blocked by M4-05 (FD-100), M1-23 (FD-50)
- [ ] **[FD-104]** M4-09 Threat presentation: biter/spitter/worm instanced animation states, nests, muzzle flashes within the particle budget *(TASK · MODERATE/MEDIUM · 5 pts · MEDIUM · `needs-gpu` `visual`)* -- blocked by M4-05 (FD-100), M1-22 (FD-49)
- [ ] **[FD-105]** M4-10 Scripted max-wave siege scenario: 5,000 units inside the threat budget (op-counts + timing report), evolution digest-stable, Peaceful verified *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M4-07 (FD-102), M4-08 (FD-103)
- [ ] **[FD-106]** M4-11 Device session: siege scenario threat-phase timing on the floor phone against the ≤ 3.5 ms worst-case budget *(TASK · SIMPLE/HIGH · 3 pts · HIGH · `needs-human` `needs-gpu`)* -- blocked by M4-10 (FD-105), M2-10 (FD-63)
- [ ] **[FD-107]** M4-12 M4 milestone acceptance: siege inside budget, Peaceful verified, evolution digest-stable *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate` `needs-gpu`)* -- blocked by M4-11 (FD-106), M4-09 (FD-104)

*Gate:* **[FD-107]** M4-12 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-7] M5 — Gigafactory hardening

*Sprint:* `FD M5 — Gigafactory hardening` · *Version:* Hardened · *TDD exit:* Promise-scale factory holds 30 UPS ≤ 8 ms + 60 fps render on the floor device through a 2-hour thermal soak; save ≤ 30 MB / load ≤ 5 s.

- [ ] **[FD-108]** M5-01 Megafactory generators at promise scale (100k buildings / 250k items): belt city, train web, siege composite + perf report *(TASK · MODERATE/MEDIUM · 5 pts · CRITICAL)* -- blocked by M4-10 (FD-105)
- [ ] **[FD-109]** M5-02 Belt shard fan-out on Zenith_DataParallelTask: 2–3× shards per worker, fixed-order merge, digest identical across worker counts *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL)* -- blocked by M5-01 (FD-108)
- [ ] **[FD-110]** M5-03 Engine #6: instancing dirty-range upload, static/dynamic bucket split, Trim/SetCapacity, RemoveInstance returns the moved slot *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL · `engine-gap` `needs-gpu` · category Engine)*
- [ ] **[FD-111]** M5-04 Engine #4 (M5 half): thermal headroom polling (getThermalHeadroom) and battery state surfaced to the game *(TASK · MODERATE/MEDIUM · 5 pts · HIGH · `engine-gap` · category Engine)* -- blocked by M2-07 (FD-60)
- [ ] **[FD-112]** M5-05 Degradation ladder (resolution / effects, never sim rate) driven by thermal headroom; battery-mode defaults *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M5-04 (FD-111)
- [ ] **[FD-113]** M5-06 Flow-field fallback behind the path-provider seam: implement if the M4 device numbers demand it, else record the decision *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M4-11 (FD-106)
- [ ] **[FD-114]** M5-07 Memory and save budgets at promise scale: SimCore ≤ 150 MB, app ≤ 1.5 GB, save ≤ 30 MB compressed, load ≤ 5 s *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)* -- blocked by M5-01 (FD-108), M3A-11 (FD-78)
- [ ] **[FD-115]** M5-08 Engine #12: big.LITTLE worker affinity — only if M5 profiling shows little-core stragglers dominating tick tails *(TASK · MODERATE/MEDIUM · 5 pts · LOW · `engine-gap` `deferred` · category Engine)* -- blocked by M5-02 (FD-109)
- [ ] **[FD-116]** M5-09 Device soak: promise-scale factory holds 30 UPS ≤ 8 ms + 60 fps through a 2-hour thermal soak; battery ≤ 12% per 30 min *(TASK · MODERATE/HIGH · 5 pts · CRITICAL · `needs-human` `needs-gpu`)* -- blocked by M5-02 (FD-109), M5-03 (FD-110), M5-05 (FD-112), M5-07 (FD-114)
- [ ] **[FD-117]** M5-10 M5 milestone acceptance: promise-scale budgets met on device *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate`)* -- blocked by M5-09 (FD-116), M5-06 (FD-113)

*Gate:* **[FD-117]** M5-10 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-8] M6 — Content complete & polish

*Sprint:* `FD M6 — Content complete & polish` · *Version:* Ship 1.0 · *TDD exit:* GDD §13 promises all measured true; first-win playthrough complete on device.

- [ ] **[FD-118]** M6-01 Full item and recipe roster (~90 items / ~110 recipes) with balance tables and integrity units *(STORY · MODERATE/LOW · 8 pts · CRITICAL · `content`)* -- blocked by M4-06 (FD-101), M3B-10 (FD-94)
- [ ] **[FD-119]** M6-02 Full tech tree (~60 techs over 5 tiers) including Ark Science and capability upgrades *(TASK · MODERATE/LOW · 5 pts · HIGH · `content`)* -- blocked by M6-01 (FD-118)
- [ ] **[FD-120]** M6-03 Ark endgame: multi-stage megaproject at the Core (structure → systems → launch), win screen, stat card, megabase continue *(STORY · MODERATE/MEDIUM · 8 pts · CRITICAL)* -- blocked by M6-02 (FD-119)
- [ ] **[FD-121]** M6-04 Engine #1 phase 2: AAudio output on Android, voice cap tuning, density mix, 200-emitter soak within budget *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL · `engine-gap` · category Engine)* -- blocked by M3A-12 (FD-79)
- [ ] **[FD-122]** M6-05 Audio content and mix: machine hum layers, positional one-shots, threat stingers, ambient music with ducking, mobile-speaker targets *(STORY · MODERATE/MEDIUM · 8 pts · HIGH · `content` `human-gate`)* -- blocked by M6-04 (FD-121), M1-20 (FD-47)
- [ ] **[FD-123]** M6-06 Engine #11: haptics — Haptic(eImpulse) → Android VibrationEffect, no-op elsewhere; Foundry impulses wired *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `engine-gap` · category Engine)*
- [ ] **[FD-124]** M6-07 Final art pass: kit for every archetype, tier colourways, state tints, full item icon set, pollution haze; ASTC bake *(STORY · COMPLEX/MEDIUM · 13 pts · HIGH · `visual` `content` `needs-gpu` `human-gate`)* -- blocked by M6-01 (FD-118), M3A-09 (FD-76)
- [ ] **[FD-125]** M6-08 Engine #16: packaging polish — per-game app icon, versionCode/versionName from the .zproj, release signing config plumbing *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap` · category Engine)*
- [ ] **[FD-126]** M6-09 Release keystore, store listing assets and a signed release APK/AAB *(TASK · SIMPLE/MEDIUM · 3 pts · HIGH · `needs-human`)* -- blocked by M6-08 (FD-125)
- [ ] **[FD-127]** M6-10 Balance from soaks: playthrough bot to first win, grace-period and first-win-time pacing, telemetry-driven tuning *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)* -- blocked by M6-03 (FD-120)
- [ ] **[FD-128]** M6-11 Objectives milestone checklist (post-first-15) and the "factory sleeps when you do" messaging *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `content`)* -- blocked by M6-02 (FD-119)
- [ ] **[FD-129]** M6-12 Ship verification on device: first-win playthrough, GDD §13 promises measured true, release build smoke *(TASK · MODERATE/HIGH · 5 pts · CRITICAL · `needs-human` `needs-gpu`)* -- blocked by M6-09 (FD-126), M6-10 (FD-127), M6-07 (FD-124), M6-05 (FD-122), M6-06 (FD-123), M6-11 (FD-128)
- [ ] **[FD-130]** M6-13 M6 milestone acceptance / ship sign-off *(TASK · SIMPLE/LOW · 3 pts · CRITICAL · `human-gate`)* -- blocked by M6-12 (FD-129)

*Gate:* **[FD-130]** M6-13 is the sprint''s designated milestone-acceptance ticket (`zagent sprint gate`). Not met.

## [FD-10] v1.x — Post-ship candidates (deferred)

*Not a milestone: claimable by name only, after 1.0 ships and a human starts a post-1.0 sprint.*

- [ ] **[FD-131]** V-01 Nuclear power arc (uranium mining, centrifuge, reactor, heat exchangers, turbines) *(STORY · COMPLEX/MEDIUM · 13 pts · LOW · `deferred`)*
- [ ] **[FD-132]** V-02 Artillery and a siege-drone analog for megabase offense *(STORY · COMPLEX/MEDIUM · 8 pts · LOW · `deferred`)*
- [ ] **[FD-133]** V-03 Modules and beacons for the electric furnace and assemblers *(STORY · MODERATE/LOW · 8 pts · LOW · `deferred`)*
- [ ] **[FD-134]** V-04 Portrait and tablet layouts *(STORY · COMPLEX/MEDIUM · 13 pts · LOW · `deferred`)*
- [ ] **[FD-135]** V-05 Engine #14 + #15: IME / soft-keyboard text input and the Android keycode table *(STORY · MODERATE/MEDIUM · 8 pts · LOW · `deferred` `engine-gap` · category Engine)*
- [ ] **[FD-136]** V-06 Cloud save *(STORY · COMPLEX/HIGH · 13 pts · LOW · `deferred`)*
- [ ] **[FD-137]** V-07 Multiplayer: lockstep over the shared command stream *(STORY · COMPLEX/HIGH · 13 pts · LOW · `deferred`)*

---

## Sizing legend

`--complexity` (TRIVIAL/SIMPLE/MODERATE/COMPLEX) and `--risk` (LOW/MEDIUM/HIGH) route the model; `--points` (Fibonacci) size the sprint; priority orders the queue within a sprint. Points are estimates made before any code exists -- re-estimate with `zagent estimate <KEY> <N>` when a ticket is claimed, and record why in its work log.

