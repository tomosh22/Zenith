# Hearth Roadmap -- milestone plan (living checklist)

**Document purpose:** the single source of truth for "what's next" in Hearth, joined to the `HE` board by key. One section per TDD §17 phase; every item carries its board key; the loop picks the next claimable ticket in the ACTIVE sprint. Checkbox discipline: `[x]` = the ticket is Done on the board, its category gates were green, and `Status.md` plus affected durable docs are reconciled; `[ ]` = anything less. The design lives in [GameDesignDocument.md](GameDesignDocument.md) and [TDD.md](TDD.md); the dependencies live on the board as `BLOCKS` links (`zagent blocked --project HE`); where a checkbox and the board disagree the board wins for status and this file wins for intent. `zagent board status --project HE --roadmap Hearth/Docs/Roadmap.md` reports drift.

**Generated 2026-09-19 from the board** by the audit that filed every ticket below; regenerate rather than hand-edit titles.

**Milestone gate (applies to EVERY phase):** every child ticket Done with the Hearth category gates green (`Build\regen.ps1` + `zenith build Hearth --headless` + `zenith test Hearth --headless`, plus the unit-gate / doc-lint lines once HE-6 lands them), the `Hearth` pin in `Tools/unit_baselines.json` bumped from an OBSERVED Null run per commit, `Status.md` reconciled, and the phase's `human-gate` acceptance ticket (the sprint's designated gate) moved through In Review by a person.

**Critical path:** HE-5 scaffold -> HE-9 lot grid -> HE-10 router -> HE-11 sim cores -> HE-13 camera -> HE-14 first sim -> HE-17 interaction FSM -> HE-20 build ops -> HE-24 save -> HE-31 ASTC device (THE P1 gate) -> HE-33 stranger -> HE-34 MVP gate -> HE-35 scheduler -> HE-36 socials -> HE-44 P2 gate -> HE-46 Billing -> HE-48 submission -> HE-53 ship gate.

**Engine dependencies (cross-board):** FD-76 ASTC blocks HE-15 and HE-31 · ZEN-58 text field blocks HE-23 · FD-58 lifecycle hook blocks HE-24 · ZEN-49 cutaway blocks HE-26 · FD-79 audio blocks HE-27 · ZEN-46 tiers and FD-60 pacing block HE-30 · ZEN-62 VAT blocks HE-39 · ZEN-56 headroom blocks HE-42 · ZEN-63 Billing blocks HE-46 · ZEN-59 / FD-125 block HE-47 · ZEN-64 localization blocks HE-49 · FD-123 haptics blocks HE-50 · ZEN-43 gestures, ZEN-44 heap, ZEN-45 quads, ZEN-48 tint and ZEN-55 animation events are consumed when present and RELATE rather than block.

**Markers:** `needs-human` = a person must do the work (on-device runs, protected-path edits, keystores, rulings); `needs-gpu` = build `Vulkan_*_True` and run windowed (each carries a `gpu-reason:*` companion); `human-gate` = the loop does the job then stops at In Review; `deferred` = skipped by the queue, claimable by name; `question` = a ruling is part of the deliverable; `content` / `docs` = what kind of deliverable it is.

---

## [HE-1] HE P0 — Foundations

*Sprint:* `HE P0 — Foundations` · *Version:* — · *TDD exit:* emulator and one real device render the test lot at 30 fps (ASTC); suspend/resume checkpoints; all gates green

- [ ] **[HE-5]** Scaffold Hearth: zenith new Hearth shaped to TDD §2, android:true, three committed scenes, boot tests, observed baseline row *(TASK · COMPLEX/HIGH · 8 pts · CRITICAL)*
- [ ] **[HE-6]** Human edits after the scaffold: Hearth unit-gate line, he-tests.yml, doc-lint, baseline row leaves advisory *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[HE-7]** Seed the Hearth living docs: Status, TestPlan, SaveFormat, Shortfalls, Questions, DecisionLog (HE-D-), AgentBriefing, Board *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `docs`)*
- [ ] **[HE-8]** HE_Bindings (touch and desktop profiles, MOUSE in none) and HE_Clock (speeds 0/1/3/10, calendar, sun angle) *(STORY · MODERATE/LOW · 5 pts · HIGH)*
- [ ] **[HE-9]** HE_LotGrid with walls on cell edges and HE_RoomGraph flood fill *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-10]** HE_GridRouter: per-storey A* with portals, string-pull smoothing and dirty-rect invalidation *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-11]** HE_Needs, HE_Utility (worked example pinned) and HE_ActionQueue *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)*
- [ ] **[HE-12]** Greybox lot: HE_GenCommon copy, HE_ShellGen v0 walls and floors with colliders, Lot.zscen holds nothing player-mutable *(STORY · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu`)*
- [ ] **[HE-13]** HE_CameraComponent: gesture camera with desktop mirrors, UI-then-gesture-then-camera arbitration, tap selection and Go Here *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-14]** One skinned sim on the shared skeleton walks a routed path to a tapped cell *(STORY · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu`)*
- [ ] **[HE-15]** Emulator boot and first real device: the greybox lot at 30 fps with ASTC on the MID reference *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-human` · `needs-gpu`)*
- [ ] **[HE-16]** P0 acceptance: emulator and device render the lot at 30 fps, suspend/resume checkpoints, gates green *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [HE-2] HE P1 — Vertical slice: MVP, "one sim, one house"

*Sprint:* `HE P1 — Vertical slice: MVP, "one sim, one house"` · *Version:* MVP · *TDD exit:* a stranger plays 15 min on a phone unprompted; autosave survives process death; 30 fps sustained on the reference device

- [ ] **[HE-17]** Interaction FSM: HE_InteractionLogic and HE_InteractionRuntime with every failure edge and reject-reason precedence *(STORY · COMPLEX/MEDIUM · 8 pts · CRITICAL)*
- [ ] **[HE-18]** HE_InteractionDefs: 25 MVP interactions over ~15 shared anim classes with the def-to-state boot assert *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)*
- [ ] **[HE-19]** HE_CatalogDefs and HE_SmartObjectComponent: 40 MVP objects as shared .zmodels tinted per colourway *(STORY · MODERATE/LOW · 5 pts · HIGH · `content`)*
- [ ] **[HE-20]** HE_BuildOps: reversible build/buy commands through one validation choke point, undo/redo, refunds *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-21]** Build/Buy UI: recycled catalog grid, drag-ghost with confirm puck, wall and floor tools, toolbar *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-22]** HE_ShellGen full: doors, windows, per-side paint via tint, three roof kinds, door sensors surviving rebuilds *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `needs-gpu`)*
- [ ] **[HE-23]** CAS-lite: genome, MVP wardrobe parts on the shared skeleton, tints (ZEN-48), naming through the engine keyboard (ZEN-58) *(STORY · MODERATE/MEDIUM · 8 pts · HIGH · `content`)*
- [ ] **[HE-24]** HE_SaveSchema and HE_SaveSlots: chunked forward-tolerant payload, corpus migration test, autosave triggers (FD-58, ZEN-57) *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-25]** Live touch UX: pie menu, info card, portraits, queue chips, needs drawer, toasts, coach marks on the scaled safe-area canvas *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-26]** Cutaway driven by the camera over ZEN-49: view-mask roof hide and height clip *(STORY · MODERATE/MEDIUM · 5 pts · HIGH · `needs-gpu`)*
- [ ] **[HE-27]** HE_AudioDirector and audio v1 content over FD-79: music states, barks, ambience, event sources *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM · `content`)*
- [ ] **[HE-28]** Moodlets and bands, 8 traits, 5 skills, 1 career, 4 aspirations, HE_Economy bills and depreciation *(STORY · COMPLEX/LOW · 8 pts · HIGH)*
- [ ] **[HE-29]** Determinism harness (scripted day hash across Debug/Release) and the three perf asserts *(TASK · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-30]** Device tiers wired: ZEN-46 options, FD-60 pacing, ZEN-54 render scale, Android kill-list, TAA off *(STORY · SIMPLE/MEDIUM · 3 pts · HIGH)*
- [ ] **[HE-31]** ASTC on the MID reference device over FD-76: declared usages, cooked set, RSS and frame time (the P1 gate) *(TASK · SIMPLE/LOW · 3 pts · CRITICAL · `needs-gpu`)*
- [ ] **[HE-32]** Windowed set: cutaway and tint-lane pixel tests plus the device screenshot pass *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `needs-gpu`)*
- [ ] **[HE-33]** Stranger playtest: 15 minutes on the reference device unprompted, process death mid-session *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[HE-34]** P1 acceptance (MVP): stranger session, autosave through process death, 30 fps on the reference device *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [HE-3] HE P2 — Households & town

*Sprint:* `HE P2 — Households & town` · *Version:* v1.0 · *TDD exit:* sustained 30 fps with 8 sims + visitors; relationship arc playable end-to-end

- [ ] **[HE-35]** HE_SimScheduler: 8 playable plus 4 visitor sims, off-lot records, household switcher, sleep-until-morning *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-36]** Socials and HE_Relationships: 16 socials, two axes, gates, milestones to Move-in, visitors *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-37]** Three careers, the complete skill set, bills and depreciation at v1.0 scope *(STORY · MODERATE/LOW · 5 pts · MEDIUM)*
- [ ] **[HE-38]** Ember Lane: six lots, the map sheet, off-lot records, persistent visitor pool *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `needs-gpu` · `content`)*
- [ ] **[HE-39]** VAT street ambience over ZEN-62 *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM · `needs-gpu`)*
- [ ] **[HE-40]** Content to the GDD §16 v1.0 column: 60 interactions, 120 objects, 12 traits, CAS 16/14/20/16, 10 aspirations, audio 5/120/80 *(STORY · COMPLEX/LOW · 13 pts · HIGH · `content`)*
- [ ] **[HE-41]** Stairs and three storeys: HE_PortalComponent stair runs, per-storey shells and roofs, chevron-driven cutaway *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH)*
- [ ] **[HE-42]** Skinned-path headroom adoption (ZEN-56) measured with 12 sims on the MID device *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM)*
- [ ] **[HE-43]** Record the point/spot shadow decision (HE-ENG-16) with a supporting capture *(TASK · SIMPLE/LOW · 1 pts · LOW · `question`)*
- [ ] **[HE-44]** P2 acceptance: 30 fps with 8 sims plus visitors, relationship arc end to end *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [HE-4] HE P3 — Retail

*Sprint:* `HE P3 — Retail` · *Version:* Retail 1.0 · *TDD exit:* Play-store-ready build; packs purchasable offline-after-install; data-safety clean

- [ ] **[HE-45]** Cosmetic packs: packId rows, pack shelf tab, CRC-checked local entitlements, never blocking on Billing *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-46]** Play Billing over the JNI seam (ZEN-63): query, purchase, acknowledge, restore against the sandbox *(STORY · MODERATE/HIGH · 5 pts · HIGH)*
- [ ] **[HE-47]** Release engineering over ZEN-59 and FD-125: AAB with PAD packs, icons, R8, version fields, data-safety text *(STORY · MODERATE/MEDIUM · 5 pts · HIGH)*
- [ ] **[HE-48]** Keystore, store listing and submission of the Retail build *(TASK · SIMPLE/LOW · 2 pts · HIGH · `needs-human`)*
- [ ] **[HE-49]** Localization v1 over ZEN-64: every string through HE_Text, one extended-Latin locale rendered *(STORY · MODERATE/MEDIUM · 5 pts · MEDIUM)*
- [ ] **[HE-50]** Haptics over FD-123: placement thunks and confirm ticks with a Settings toggle *(TASK · SIMPLE/LOW · 2 pts · LOW)*
- [ ] **[HE-51]** Device-tier QA matrix on LOW, MID and HIGH devices *(TASK · SIMPLE/LOW · 3 pts · HIGH · `needs-human`)*
- [ ] **[HE-52]** Windows _False ship configuration with packs via key file and a packaged dist/ build *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[HE-53]** P3 acceptance and ship gate: store approval, QA matrix, offline pack purchase, data-safety clean *(TASK · SIMPLE/LOW · 1 pts · HIGH · `human-gate`)*

## [HE-54] HE Post-1.0 candidates (deferred)

*Sprint:* (none -- deferred) · *Version:* — · *TDD exit:* (deferred; opened by a human)

- [ ] **[HE-55]** Morph targets for CAS depth over ZEN-65 (data-gated) *(STORY · COMPLEX/HIGH · 13 pts · LOW · `deferred`)*
- [ ] **[HE-56]** CJK localization *(STORY · COMPLEX/MEDIUM · 8 pts · LOW · `deferred`)*
- [ ] **[HE-57]** Android IME for naming (FD-135) *(STORY · SIMPLE/LOW · 3 pts · LOW · `deferred`)*
- [ ] **[HE-58]** v2.0 content column: 120 interactions, 300 objects, 20 traits, 5 careers, 10 lots *(STORY · COMPLEX/LOW · 13 pts · LOW · `deferred` · `content`)*
- [ ] **[HE-59]** Group conversations (3+ sims sharing one social) *(STORY · MODERATE/MEDIUM · 5 pts · LOW · `deferred`)*
- [ ] **[HE-60]** Revisit question: forward+ and runtime graphics-quality switching *(TASK · SIMPLE/LOW · 1 pts · LOW · `deferred` · `question`)*
- [ ] **[HE-61]** Revisit question: the GDD §16 cut list (pets, aging, kids, weather, pools) against player data *(TASK · SIMPLE/LOW · 1 pts · LOW · `deferred` · `question`)*
