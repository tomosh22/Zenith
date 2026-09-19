# Undervault Roadmap -- milestone plan (living checklist)

**Document purpose:** the single source of truth for "what's next" in Undervault, joined to the `UV` board by key. One section per TDD §20 phase; every item carries its board key; the loop picks the next claimable ticket in the ACTIVE sprint. Checkbox discipline: `[x]` = the ticket is Done on the board, its category gates were green, and `Status.md` plus affected durable docs are reconciled; `[ ]` = anything less. The design lives in [GameDesignDocument.md](GameDesignDocument.md) and [TechnicalDesignDocument.md](TechnicalDesignDocument.md); the dependencies live on the board as `BLOCKS` links (`zagent blocked --project UV`); where a checkbox and the board disagree the board wins for status and this file wins for intent. `zagent board status --project UV --roadmap Undervault/Docs/Roadmap.md` reports drift.

**Generated 2026-09-19 from the board** by the audit that filed every ticket below; regenerate rather than hand-edit titles.

**Milestone gate (applies to EVERY phase):** every child ticket Done with the Undervault category gates green (`Build\regen.ps1` + `zenith build Undervault --headless` + `zenith test Undervault --headless`, plus the unit-gate / doc-lint lines once UV-7 lands them), the `Undervault` pin in `Tools/unit_baselines.json` bumped from an OBSERVED Null run per commit, `Status.md` reconciled, and the phase's `human-gate` acceptance ticket (the sprint's designated gate) moved through In Review by a person.

**Critical path:** UV-6 scaffold -> UV-12 fluid prototype -> UV-15 mutations -> UV-16 slice fluid -> UV-23 regions/A* -> UV-24 job board -> UV-25 Vaulters -> UV-27 UI -> UV-28 save -> UV-29 session test -> UV-31 slice gate -> UV-32/33 networks -> UV-47 Alpha gate -> UV-52 device matrix -> UV-55 Beta gate -> UV-64 submission -> UV-65 ship gate.

**Engine dependencies (cross-board):** ZEN-42 (ortho camera) blocks UV-21 · ZEN-47 (surface recreation) blocks UV-11 · ZEN-43 gestures, ZEN-44 heap and ZEN-45 quads have game-side fallbacks the TDD sanctions, so they RELATE rather than block · FD-79 audio blocks UV-45 · FD-58 lifecycle hook blocks UV-48 · FD-76 ASTC blocks UV-49 · FD-60 / ZEN-54 / ZEN-46 / ZEN-50 block UV-50 · FD-57 / ZEN-52 block UV-51 · ZEN-59 packaging blocks UV-53 and UV-64.

**Markers:** `needs-human` = a person must do the work (on-device runs, protected-path edits, keystores, rulings); `needs-gpu` = build `Vulkan_*_True` and run windowed (each carries a `gpu-reason:*` companion); `human-gate` = the loop does the job then stops at In Review; `deferred` = skipped by the queue, claimable by name; `question` = a ruling is part of the deliverable; `content` / `docs` = what kind of deliverable it is.

---

## [UV-1] UV Phase 0 — Spikes

*Sprint:* `UV Phase 0 — Spikes` · *Version:* — · *TDD exit:* measured budgets on Tier A and Tier B; go/no-go on Tier B

- [ ] **[UV-6]** Scaffold Undervault: replace the static visual study with the TDD §1 skeleton, android:true, boot tests and an observed baseline row *(TASK · COMPLEX/HIGH · 8 pts · CRITICAL)*
- [ ] **[UV-7]** Human edits after the scaffold: Undervault unit-gate line, uv-tests.yml, doc-lint, baseline row leaves advisory *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[UV-8]** Seed the Undervault living docs: Status, TestPlan, SaveFormat, Shortfalls, Questions, DecisionLog (UV-D-), AgentBriefing, Board *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `docs`)*
- [ ] **[UV-9]** Device-reality spike harness: UV_DeviceSpike_Test with 24,576 instanced boxes, a stub cell SSBO pass and per-frame telemetry *(TASK · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-10]** Device spike run: measure frame, memory and thermal on Tier A and Tier B phones; re-baseline TDD §15 *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human` · `needs-gpu`)*
- [ ] **[UV-11]** Surface-lifecycle verdict for Undervault: 20 background/foreground cycles on a device after ZEN-47 *(TASK · SIMPLE/LOW · 1 pts · HIGH · `needs-human`)*
- [ ] **[UV-12]** Fluid-feel prototype: UV_CellGrid + UV_FluidSim at 5 Hz with tools-only debug draw, conservation and worker-invariance units *(STORY · COMPLEX/HIGH · 13 pts · CRITICAL)*
- [ ] **[UV-13]** Phase 0 acceptance: measured budgets, surface verdict, fluid constants, GO/NO-GO on Tier B *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [UV-2] UV Vertical Slice

*Sprint:* `UV Vertical Slice` · *Version:* Vertical Slice · *TDD exit:* GDD §20 gesture test passes on-device; 30-min playable loop; determinism + conservation suites green; Undervault baseline row pinned

- [ ] **[UV-14]** UV_ElementTable (22 MVP elements, totality units) and UV_SimClock (5 Hz, cap-and-discard, interpolation alpha) *(STORY · MODERATE/LOW · 5 pts · HIGH)*
- [ ] **[UV-15]** Chunk metadata and UV_SolidMutation: queued, cell-index-ordered topology changes, debris records, settled chunks *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-16]** Fluid sim at slice scale: banded workers, settled-chunk skipping, pinned determinism master at three worker counts, /fp:precise unit *(STORY · COMPLEX/HIGH · 8 pts · CRITICAL)*
- [ ] **[UV-17]** UV_ThermalSim: harmonic-mean conduction, stability clamp, phase transitions with hysteresis and an energy ledger *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-18]** UV_WorldGen: per-stage PCG32 pipeline, biome bands, veins, pockets, start chamber, validation and re-roll; pinned CRC digest *(STORY · MODERATE/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-19]** UV_TileInstances over Flux_InstanceGroup with a stable cell-to-instance table, plus the procedural tile kit bake *(STORY · COMPLEX/HIGH · 8 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-20]** UV_CellRenderFeature: post-HDR cell SSBO pass drawing liquid, gas haze, fog-of-unexplored, tints and overlay ramps *(STORY · COMPLEX/HIGH · 8 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-21]** UV_CameraHostComponent: orthographic pan/zoom/pick on the engine camera (ZEN-42), frame-rate not tick-rate *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-22]** UV_Bindings and the gesture layer: pan, paint, pinch-pan, tap, long-press, flick, lifecycle cancel, simulator-driven units *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-23]** UV_Regions and UV_Pathfinder: chunk flood labels, O(1) unreachable reject, generation-stamped A* on workers, repath rules *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-24]** UV_JobBoard and UV_Designations: GDD §6.2 scoring, exclusive claims with GC, the five paint tools *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-25]** UV_VaulterSim and UV_VaulterComponent: needs, interrupt precedence, schedule blocks, grid mover, skinned figure on the shared rig *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-26]** Slice building subset: UV_BuildingDefs table, UV_BuildingComponent, Ladder/Tile/Bin/Algae Pod/Cot/Crank/Stasis Vault *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)*
- [ ] **[UV-27]** Slice UI: HUD strip, speed cluster, tool dock with tool-mode flow, Build menu with placement, inspector sheet *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-28]** UV_SaveSchema and UV_SaveSlots: module-framed transactional codec over Zenith_SaveData, frozen blob policy *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-29]** UV_ColonySession_Test end-to-end on Null plus a Vulkan smoke; observe and record the Undervault pin *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-30]** Android boot demo: x86_64 emulator APK boots the slice, pinch-pan and one dig complete *(TASK · MODERATE/MEDIUM · 3 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-31]** Vertical Slice acceptance: GDD §20 gesture test on a device, 30-minute loop, suites green, pin observed *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate` · `needs-human`)*

## [UV-3] UV Alpha — MVP feature-complete

*Sprint:* `UV Alpha — MVP feature-complete` · *Version:* Alpha · *TDD exit:* 2-hour session no soft-locks; <= 8 ms/tick on Tier A; 100k-tick soak clean

- [ ] **[UV-32]** Power networks: circuits, supply/demand/batteries, brown-out, deterministic overload repair errands, Power overlay data *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-33]** Liquid plumbing: single-packet segments in reverse-topological order, persisted round-robin junctions, pump/pipe/vent *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[UV-34]** Food chain: Gloomsprout Bed, Mush Press, Mess Table, Ration Box, Compost Heap, spoilage and kcal accounting *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)*
- [ ] **[UV-35]** Hygiene: Latrine, Wash Basin, the Bladder need and accidents, germ-vector hook as a flag *(STORY · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[UV-36]** Schedule and Priorities screens plus the full morale model with graduated floors *(STORY · MODERATE/LOW · 5 pts · HIGH)*
- [ ] **[UV-37]** UV_Rooms: bounded flood, classification table, 1 Hz cap, the four MVP room bonuses, Rooms overlay data *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM)*
- [ ] **[UV-38]** Complete the MVP catalog (Airlock Door, Research Bench) and the 5-rung linear research tree with its screen *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)*
- [ ] **[UV-39]** Six MVP overlays with dual encoding, picker and pill; simulated-CVD capture pass at min zoom *(STORY · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-40]** Remaining GDD §11.6 screens: roster, detail, cycle summary, pause/settings, new game (numeric seed), Vault Log *(STORY · MODERATE/LOW · 8 pts · MEDIUM)*
- [ ] **[UV-41]** Stasis Vault population valve, the trait pool with one hook each, seven skill families *(STORY · MODERATE/LOW · 5 pts · MEDIUM)*
- [ ] **[UV-42]** Threats with early warning and escape hatch, the alert stack, colony wipe; one scenario test per threat *(STORY · MODERATE/LOW · 5 pts · HIGH)*
- [ ] **[UV-43]** Warden Protocols onboarding: ten state-driven steps and once-ever toasts with a Log tab *(STORY · MODERATE/LOW · 5 pts · MEDIUM · `content`)*
- [ ] **[UV-44]** Autosave contract: cycle boundary and 60 s cadence, resume exactly, three manual slots *(STORY · MODERATE/LOW · 3 pts · HIGH)*
- [ ] **[UV-45]** Undervault audio v1 on the engine mixer (FD-79): sound set, event bindings, silent on Null *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM · `content`)*
- [ ] **[UV-46]** 100k-tick nightly soak script and a 2-hour Tier A device session measuring tick cost by phase *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-human`)*
- [ ] **[UV-47]** Alpha acceptance: soak, Tier A tick cost, protocol-test completion, GDD §19 MVP column present *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate` · `needs-human`)*

## [UV-4] UV Android Beta

*Sprint:* `UV Android Beta` · *Version:* Android Beta · *TDD exit:* Tier B 30 fps at 3x; battery <= 12%/hr Tier A; restore-after-kill <= 60 s loss; crash-free >= 99.5%

- [ ] **[UV-48]** Save and pause on background through the engine lifecycle hook (FD-58); kill-and-relaunch resume *(STORY · SIMPLE/MEDIUM · 3 pts · HIGH)*
- [ ] **[UV-49]** ASTC texture set for Undervault over FD-76: declared usages, cooked variant, RSS measured *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-gpu`)*
- [ ] **[UV-50]** Battery posture: 30 fps cap, tier-selected render scale, config overrides; drain and 3x frame time measured *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-51]** Safe-area insets as HUD margins (FD-57) and 9-slice panels (ZEN-52), cutout mode in the manifest *(STORY · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[UV-52]** Device matrix: run the TDD §16.5 checklist on Adreno, Mali and a Tier B phone *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-human`)*
- [ ] **[UV-53]** Play internal track: signed AAB via zenith package (ZEN-59), listing, upload *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[UV-54]** Local telemetry for GDD §20 KPIs: session, crash sentinel, cycle and protocol counters, Tools/ summariser *(STORY · MODERATE/LOW · 5 pts · MEDIUM)*
- [ ] **[UV-55]** Android Beta acceptance: Tier B 30 fps at 3x, battery, restore-after-kill, crash-free rate from the internal track *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [UV-5] UV v1.0

*Sprint:* `UV v1.0` · *Version:* v1.0 · *TDD exit:* release checklist + store approval

- [ ] **[UV-56]** v1 elements: six additions, latent heat paid for real, in-pipe freezing and boiling as break events *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[UV-57]** v1 buildings: the ~18 GDD §7.2 rows as data plus one hook each, catalog totality unit *(STORY · COMPLEX/LOW · 8 pts · HIGH · `content`)*
- [ ] **[UV-58]** Branched research: the six v1 branches gating every v1 row exactly once *(STORY · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[UV-59]** Gas plumbing loop and the Ventilation and Light overlays with a CVD pass *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM · `needs-gpu`)*
- [ ] **[UV-60]** Care skill and Med Cot, morale expectations, group schedule presets, Bathtime, three more rooms, building toggles *(STORY · MODERATE/LOW · 5 pts · MEDIUM)*
- [ ] **[UV-61]** Deep map size on Tier A: generation, validation, digest and tick cost against budget *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM)*
- [ ] **[UV-62]** Vault Log moments, onboarding polish from the protocol test, revisitable tips *(STORY · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[UV-63]** Accessibility pass: GDD §17 items as options with units, CVD pass over every overlay *(STORY · MODERATE/LOW · 5 pts · MEDIUM · `needs-gpu`)*
- [ ] **[UV-64]** Release checklist and store submission of the v1.0 AAB *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[UV-65]** v1.0 acceptance and ship gate: release checklist, KPIs on the release build, store approval *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [UV-66] UV Post-launch candidates (deferred)

*Sprint:* (none -- deferred) · *Version:* — · *TDD exit:* (deferred; opened by a human)

- [ ] **[UV-67]** Vaultlife creatures (Burrowers, Drifters) and ranching *(STORY · COMPLEX/MEDIUM · 8 pts · LOW · `deferred`)*
- [ ] **[UV-68]** Full automation logic: gates, signal ribbon, priority automation *(STORY · MODERATE/MEDIUM · 8 pts · LOW · `deferred`)*
- [ ] **[UV-69]** Conveyor rails as a third conduit network *(STORY · MODERATE/MEDIUM · 8 pts · LOW · `deferred`)*
- [ ] **[UV-70]** Large-map tablet mode over the width-aware canvas (ZEN-53) *(STORY · MODERATE/LOW · 5 pts · LOW · `deferred`)*
- [ ] **[UV-71]** Vault Archive cosmetic unlocks *(STORY · SIMPLE/LOW · 3 pts · LOW · `deferred`)*
- [ ] **[UV-72]** Custom map sizes within the memory budget *(STORY · SIMPLE/LOW · 3 pts · LOW · `deferred`)*
- [ ] **[UV-73]** Full disease sim over the v1 germ hooks *(STORY · COMPLEX/MEDIUM · 8 pts · LOW · `deferred`)*
