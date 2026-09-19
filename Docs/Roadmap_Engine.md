# Engine Roadmap -- the ZEN board's epics and tickets (living checklist)

**Document purpose:** the join between the `ZEN` board and the engine's plan documents, in the shape `Games/Foundry/Docs/Roadmap.md` and `Games/Zenithmon/Docs/Roadmap.md` use. Every epic and every ticket carries its key; `zagent board status --project ZEN --roadmap Roadmap_Engine.md` reports drift between this file and the board. The design lives in the documents each epic cites (`Docs/design/EngineRefactor_Phase-1_Inventory.md`, `Docs/design/Photorealism.md`, `Games/Hearth/Docs/TDD.md` §3, `Games/Undervault/Docs/TechnicalDesignDocument.md` §2, `Docs/Onboarding/JuniorExplorationFollowup.md`), the dependencies live on the board as BLOCKS links, and where a checkbox and the board disagree the board wins for status and this file wins for intent.

**Generated 2026-09-19 from the board by the audit that filed ZEN-24 to ZEN-88.** Checkbox discipline: `[x]` = Done on the board with the Engine gates green and every game's pin bumped from an observed run.

**Sprints.** Milestone-shaped work sits in PLANNED sprints a human starts (`zagent sprint start "<name>"`): `ZEN De-globalisation program` (gate ZEN-86), `ZEN Mobile readiness P0 — before a vertical slice` (gate ZEN-87), `ZEN Mobile readiness P1 — before an Android ship` (gate ZEN-88). Everything else carries no sprint and is claimable from the queue as ordinary engine hygiene.

**Foundry's engine register is on the FD board, not here.** FD-56 (UI DPI scaling), FD-57 (safe-area insets), FD-58 (lifecycle pause hook), FD-60 (frame pacing), FD-61 (ScrollView), FD-76 (ASTC export), FD-77 (SaveAsync), FD-79 and FD-121 (audio), FD-80 (dynamic texture), FD-110 (instancing upload), FD-111 (thermal), FD-115 (big.LITTLE), FD-123 (haptics), FD-125 (packaging polish), FD-135 (IME) are filed `--category Engine` under Foundry's milestones and are deliberately not duplicated in ZEN-26. Zenithmon's engine gaps E6/E7/E8 (ZM-47, ZM-48, ZM-49, ZM-113) stay on the ZM board under ZM-14.

**Markers:** `needs-human` = a person must do the work (protected paths, device sessions, rulings); `human-gate` = the loop stops at In Review; `deferred` = skipped by the queue, claimable by name; `question` = a ruling is part of the deliverable.

---

## [ZEN-1] Engine gates & pinned baselines

- [ ] **[ZEN-3]** The RenderTest pin is gated by no workflow, so it can drift silently *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `needs-human`)*
- [x] **[ZEN-7]** ZENITH_ASSERT_EQ_FLOAT passes on NaN at every epsilon, so a degenerate float is indistinguishable from a correct one *(BUG · MODERATE/HIGH · 5 pts · HIGH · `tech-debt`)*
- [ ] **[ZEN-84]** RenderTest tennis integration units must not depend on an asset a prior tools boot wrote *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM · `flaky`)*
- [ ] **[ZEN-85]** Make the aux tools (FluxCompiler, font libraries) build green so a whole solution can be built *(TASK · MODERATE/MEDIUM · 5 pts · MEDIUM · `tech-debt`)*

## [ZEN-24] Engine architecture: de-globalisation, leaf libraries and module layout

*Sprints:* `ZEN De-globalisation program`

- [x] **[ZEN-4]** Make the physics-chunk-span lifetime invariant simple instead of subtle *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM)*
- [x] **[ZEN-5]** Zenith_TerrainComponent's //private: is commented out, so the lockstep invariant has no access control behind it *(TASK · MODERATE/MEDIUM · 5 pts · MEDIUM)*
- [ ] **[ZEN-14]** Zenith library source files must be in their own dedicated directories *(TASK · MODERATE/MEDIUM · 8 pts · MEDIUM · `tech-debt`)*
- [ ] **[ZEN-16]** Move Vulkan, D3D12 and Null render backends into dedicated RenderBackends directory *(TASK · MODERATE/LOW · 5 pts · MEDIUM · `tech-debt`)*
- [ ] **[ZEN-18]** De-globalisation program spine: refreshed inventory, the injection shape, the ratchet's target and the phase split *(TASK · MODERATE/HIGH · 5 pts · MEDIUM)*
- [ ] **[ZEN-29]** De-globalisation phase 3b: TaskSystem and Profiling off g_xEngine *(STORY · COMPLEX/MEDIUM · 8 pts · MEDIUM)*
- [ ] **[ZEN-30]** De-globalisation phase 4: AssetRegistry and Physics *(STORY · COMPLEX/HIGH · 13 pts · MEDIUM)*
- [ ] **[ZEN-31]** De-globalisation phase 5: SceneManager callback registries and the entity store *(STORY · COMPLEX/HIGH · 13 pts · MEDIUM)*
- [ ] **[ZEN-32]** De-globalisation phase 5.5: Input and Editor statics *(STORY · COMPLEX/MEDIUM · 8 pts · MEDIUM)*
- [ ] **[ZEN-33]** De-globalisation phase 6: Flux_Graphics and the Vulkan backend statics *(STORY · COMPLEX/HIGH · 13 pts · MEDIUM)*
- [ ] **[ZEN-34]** De-globalisation phase 7: the Flux subsystems *(STORY · COMPLEX/MEDIUM · 13 pts · MEDIUM)*
- [ ] **[ZEN-35]** De-globalisation phase 8: per-game extern globals *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM)*
- [ ] **[ZEN-36]** De-globalisation phase 9: static-API removal sweep and the allowlist to zero *(STORY · MODERATE/HIGH · 8 pts · MEDIUM)*
- [ ] **[ZEN-37]** Break the SceneManager/SceneData include cycle: extract the component-pool types (T2.4) *(TASK · COMPLEX/MEDIUM · 8 pts · LOW · `tech-debt` · `deferred`)*
- [ ] **[ZEN-38]** Async asset loading: implement the documented NOT IMPLEMENTED loader or delete its scaffolding *(TASK · MODERATE/MEDIUM · 5 pts · LOW · `tech-debt`)*
- [ ] **[ZEN-86]** De-globalisation acceptance: allowlist at zero outside the composition root, every game green *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `human-gate`)*

## [ZEN-25] Flux: view neutrality and renderer cleanup

- [x] **[ZEN-6]** Headless runs must be able to CHANGE a committed .zscen, not only create a missing one *(STORY · COMPLEX/HIGH · 13 pts · HIGH · `engine-gap`)*
- [x] **[ZEN-10]** Move mip load bias out of Vulkan and into Flux *(TASK · SIMPLE/MEDIUM · 1 pts · LOW)*
- [ ] **[ZEN-12]** Remove FLUX_RENDER_VIEW_PREVIEW *(TASK · COMPLEX/HIGH · 13 pts · HIGH)*
- [ ] **[ZEN-19]** Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[ZEN-20]** Remove bSubmitRenderWork from SubmitRenderWork *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM)*
- [ ] **[ZEN-22]** Remove const char* szName overloads of Flux_ShaderBinder functions *(TASK · MODERATE/LOW · 5 pts · MEDIUM)*
- [ ] **[ZEN-39]** Unified mesh Stage 6: crowd batching via DrawIndexedIndirectCount per bucket *(STORY · COMPLEX/HIGH · 13 pts · LOW · `deferred`)*
- [ ] **[ZEN-40]** Teardown scratch overflow makes every windowed run exit non-zero *(BUG · MODERATE/MEDIUM · 5 pts · HIGH · `tech-debt`)*
- [ ] **[ZEN-41]** FluxCompiler lint: reject a raw normal-map decode outside Common/Material.slang *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*

## [ZEN-26] Mobile platform readiness: the shared engine register for Hearth, Undervault and Foundry

*Sprints:* `ZEN Mobile readiness P0 — before a vertical slice`, `ZEN Mobile readiness P1 — before an Android ship`

- [ ] **[ZEN-42]** Ortho camera: public InitialiseOrthographic, plane setters and the missing Vulkan Y-flip *(TASK · SIMPLE/MEDIUM · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-43]** Zenith_Gestures: tap, long-press, drag, flick, pinch and two-finger pan over the pointer table and claims *(STORY · COMPLEX/MEDIUM · 8 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-44]** Zenith_PriorityQueue: a binary heap in Zenith/Collections, first consumer Zenith_Pathfinding *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-45]** Quad budget: per-game configurable FLUX_MAX_QUADS_PER_FRAME, owned arrays, high-water debug variable *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-46]** Shipping graphics tiers: TAA toggle, cascade count, shadow resolution, dynamic-light cap and a device-tier heuristic *(STORY · MODERATE/MEDIUM · 8 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-47]** Vulkan surface recreation across TERM_WINDOW and INIT_WINDOW: verify on the emulator, then fix *(TASK · MODERATE/HIGH · 8 pts · HIGH · `engine-gap` · `needs-human`)*
- [ ] **[ZEN-48]** Per-instance tint from Zenith_ModelComponent for static and skinned draws *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-49]** Cutaway rendering: per-item camera-hide and height-clip flags on the draw item *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-50]** Android config plumbing: an APK args file and intent extras reach Zenith_CommandLine *(TASK · SIMPLE/LOW · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-51]** Device-gate relaxation: core plus per-enabled-feature requirements declared per project *(TASK · SIMPLE/MEDIUM · 3 pts · HIGH · `engine-gap`)*
- [ ] **[ZEN-52]** 9-slice UI widget for panel and frame art *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-53]** UI canvas scaling mode: match width, shorter or longer side, not height only *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-54]** Render scale decoupled from TAA and reachable on Android *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-55]** Animation events from the state-machine path with a crossfade contract *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-56]** Skinned-path headroom: skip culled instances, size palettes to the rig, no per-frame heap churn *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-57]** SaveData slot metadata: ReadHeader and EnumerateSlots as engine surface *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-58]** Text field and engine-drawn on-screen keyboard, headless-testable through the input simulator *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-59]** AAB and install-time Play Asset Delivery packaging: zenith package grows an Android target *(STORY · MODERATE/HIGH · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-60]** Android CI leg: an AGDE compile-and-link gate plus APK assembly on engine changes *(TASK · SIMPLE/MEDIUM · 3 pts · MEDIUM · `engine-gap` · `needs-human`)*
- [ ] **[ZEN-61]** Slim instance path for static tiles: position-plus-tint records in their own bucket *(STORY · MODERATE/MEDIUM · 8 pts · MEDIUM · `engine-gap`)*
- [ ] **[ZEN-62]** VAT character path hardening: animated normals and a two-clip crossfade *(STORY · MODERATE/MEDIUM · 8 pts · LOW · `engine-gap` · `deferred`)*
- [ ] **[ZEN-63]** JNI seam and Play Billing with CRC'd local entitlements: the first Java in the repo *(STORY · COMPLEX/HIGH · 13 pts · LOW · `engine-gap` · `deferred` · `question`)*
- [ ] **[ZEN-64]** Localization v1: string-table asset, keyed lookup, MSDF font fallback chain, locale detection *(STORY · MODERATE/MEDIUM · 8 pts · LOW · `engine-gap`)*
- [ ] **[ZEN-65]** Morph targets: vertex lanes, pre-skin compute accumulation, glb import and sliders *(STORY · COMPLEX/MEDIUM · 13 pts · LOW · `engine-gap` · `deferred`)*
- [ ] **[ZEN-66]** Input program hand-off: run the Android on-device and gamepad checklists *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `needs-human`)*
- [ ] **[ZEN-87]** Mobile P0 acceptance: the emulator renders a test lot at 30 fps with suspend/resume *(TASK · SIMPLE/LOW · 2 pts · HIGH · `human-gate`)*
- [ ] **[ZEN-88]** Mobile P1 acceptance: an AAB installs and runs on the emulator; Android CI green *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `human-gate`)*

## [ZEN-27] Rendering quality: the photorealism known gaps

- [ ] **[ZEN-67]** Rule on local light shadows: implement point and spot shadows on desktop, or delete the dead cast-shadows field *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `needs-human` · `question`)*
- [ ] **[ZEN-68]** Local light shadows: budgeted spot and point shadow maps through the render-graph shadow passes *(STORY · COMPLEX/HIGH · 13 pts · LOW)*
- [ ] **[ZEN-69]** SSGI as occlusion plus colour bleed on top of IBL irradiance, energy-conserving, on by default *(STORY · COMPLEX/HIGH · 13 pts · LOW)*
- [ ] **[ZEN-70]** Ozone absorption layer in the atmosphere so twilight carries the Chappuis band *(STORY · MODERATE/MEDIUM · 8 pts · LOW)*
- [ ] **[ZEN-71]** Motion blur and depth of field over the existing velocity MRT as optional HDR-chain passes *(STORY · MODERATE/MEDIUM · 8 pts · LOW)*
- [ ] **[ZEN-72]** Local IBL occlusion for interiors: baked sky-visibility volumes or per-room probes *(STORY · COMPLEX/MEDIUM · 13 pts · LOW)*

## [ZEN-28] Agent loop, Vault and documentation tooling

- [ ] **[ZEN-8]** doc_lint: verify bare ticket-key citations, not just markdown links *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*
- [x] **[ZEN-23]** Verify Vault production cutover *(TASK · SIMPLE/LOW · 2 pts · MEDIUM)*
- [ ] **[ZEN-73]** doc_lint check C9: reconcile AssetManifest roster counts and generator version stamps against the headers *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `docs`)*
- [ ] **[ZEN-74]** zagent claim archives an existing run directory to KEY.prev before writing fresh scratch *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[ZEN-75]** zagent writes .zagent/last.exit on every exit path *(TASK · SIMPLE/LOW · 2 pts · MEDIUM)*
- [ ] **[ZEN-76]** zagent check: the contract verdict (exit 4) wins over repo-busy (exit 5); claim and next unchanged *(TASK · SIMPLE/LOW · 2 pts · MEDIUM)*
- [ ] **[ZEN-77]** Warn at file time when a Definition of Done's identifiers have hits outside the declared file list *(TASK · MODERATE/MEDIUM · 5 pts · MEDIUM)*
- [ ] **[ZEN-78]** tick_gates measure phase reports deferred gate lines and the automated-test registry count *(TASK · SIMPLE/LOW · 3 pts · MEDIUM)*
- [ ] **[ZEN-79]** A refused zagent claim must change nothing: reproduce the mutate-before-validate defect against Vault and fix it *(BUG · MODERATE/HIGH · 5 pts · HIGH)*
- [ ] **[ZEN-80]** Vault web app: the status menu offers exactly the workflow's transitions *(TASK · SIMPLE/LOW · 2 pts · LOW)*
- [ ] **[ZEN-81]** Add a non-tools (_False) build line to every category's gate list *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `needs-human`)*
- [ ] **[ZEN-82]** Give the Engine category a decision log (Docs/EngineDecisionLog.md, ZEN-D- prefix) and make zagent decide warn without one *(TASK · SIMPLE/LOW · 2 pts · MEDIUM · `needs-human` · `docs`)*
- [ ] **[ZEN-83]** Apply the six recorded tick.md changes (paste the WHY, grep the subject, durable findings to the log, retire docs-sync prose, two allowed-tools gaps) *(TASK · SIMPLE/LOW · 3 pts · MEDIUM · `needs-human`)*

## Unparented

- [x] **[ZEN-9]** Nothing checks that the twinned needsDocsTree rule agrees across client and board *(TASK · SIMPLE/MEDIUM · 1 pts · MEDIUM)*
