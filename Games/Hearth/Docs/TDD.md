# Hearth — Technical Design Document

| | |
|---|---|
| Game | **Hearth** — mobile-first life sim (see [GDD.md](GDD.md)) |
| Engine | Zenith @ current master (`5605b8d4` era) |
| Primary platform | Android (AGDE, Vulkan 1.1, minSdk 26, arm64-v8a + x86_64) |
| Secondary | Windows Vulkan (`_False` ship, `_True` authoring); Null configs = CI/tests |
| Status | v0.1 — design draft, 2026-08-21 |

Every engine claim in this document was verified against the working tree on 2026-08-21 and cites `file:line` (line numbers exact where checked this pass; symbol-level otherwise). **§3 is the headline section**: everything the engine must gain or change, as a phased work-package table.

---

## 1. Targets & budgets

**Device tiers** (selected at boot by `Zenith_DeviceTier` heuristic — HE-ENG-05; user-overridable in Settings, applied on restart):

| Tier | Reference | Render | FPS | Shadows | SSAO | TAA |
|---|---|---|---|---|---|---|
| LOW | 2019 mid (SD 665 class) | 720p-class surface | 30 | 1 cascade 1024² | off | off |
| MID (target) | **2021 mid (SD 778G class)** | native ≤1080p | 30 | 2 cascades 1024² | on | off |
| HIGH | 2023+ flagship | native | 60 opt-in | 2 cascades 2048² | on | off (revisit) |
| Windows | dev + ship | native | uncapped/vsync | 4 cascades 2048² | on | on |

**Frame budget @ MID, 33 ms:** simulation ≤ 3 ms (needs decay O(sims); ≤ 2 full utility re-scores/frame round-robin; router ≤ 1 ms amortized, 1 request/frame queue) · animation ≤ 4 ms (12 skinned sims: state machines + IK, main-thread — see HE-ENG-13) · render CPU ≤ 6 ms (snapshot + graph) · GPU ≤ 16 ms (G-buffer + deferred lighting ~9, shadows ~3, UI/text ~1.5, headroom ~2.5).

**Caps:** ≤ **8 playable + 4 visitor** skinned sims on-lot (hard cap; revisit trigger for skinned batching is > 12) · ≤ 300 visible indirect draws / ≤ 120 buckets (shell meshes merged per material per storey; colorways via the tint lane, never material clones — the material table caps at 1024 and each unique material is an extra bucket) · UI ≤ 4096 quads (HE-ENG-04) · RSS ≤ 1.2 GB on 4 GB devices, textures ≤ 350 MB (ASTC) · APK ≤ 400 MB base (packs via Play Asset Delivery) · cold boot ≤ 8 s.

**Off-lot sims are records, not bodies** (needs + career tick numerically; no entity, no skeleton).

## 2. Architecture overview

**The shipped Zenith idiom, followed deliberately:** no engine system scheduler exists — a game hangs every subsystem off one orchestrator component, owned **by value**, ticked in explicit order (`CB_CityManagerComponent` precedent, `Games/CityBuilder/Components/CB_CityManagerComponent.h`). Gameplay *systems* are C++; Behaviour Graphs are the bespoke-scripting layer only (per `Zenith/Scripting/` doctrine).

```
HE_GameComponent (order 100)  ← the orchestrator, DontDestroyOnLoad
 ├─ HE_Clock            sim time, speeds, calendar, sun-angle write
 ├─ HE_LotGrid          cells + edge walls + occupancy + rooms (source of truth)
 ├─ HE_GridRouter       per-storey A* + portals + reservations
 ├─ HE_SimScheduler     needs decay + utility autonomy + action queues
 ├─ HE_InteractionRt    the interaction FSM executor
 ├─ HE_Relationships    pair scores + social resolution
 ├─ HE_Economy          funds, bills, depreciation, careers
 ├─ HE_BuildController  build/buy verbs, validation, undo, ghost
 ├─ HE_ShellGen         dirty-region wall/floor/roof mesh + collider rebuild
 ├─ HE_AudioDirector    bus params, music state, bark selection
 └─ HE_SaveController   slots, autosave triggers, schema I/O
OnUpdate order: input/gestures → Clock → SimScheduler → InteractionRt →
Relationships → Economy → BuildController → ShellGen → AudioDirector → HUD
```

**ECS components** (`ZENITH_REGISTER_COMPONENT`, game orders 100+; headers `#include`d from `Hearth.cpp` against MSVC dead-strip; `ZENITH_TOOLS` editor-registry mirror in `Project_RegisterGameComponents`):

| Order | Component | Role |
|---|---|---|
| 100 | `HE_GameComponent` | orchestrator above; static accessors for tests; hand-written moves |
| 101 | `HE_CameraComponent` | pure-math orbit/pan/zoom (evolved `CB_CameraController.h`) + gesture consumption + cutaway driver |
| 102 | `HE_SimComponent` | one sim: save identity, animator glue, path follower, world→screen plumbob/needs projection (ZM name-tag pattern) |
| 103 | `HE_SmartObjectComponent` | placed object: catalog row id, grid pose, state, slot occupancy, tints; interaction dispatch by def row (`ZM_Interactable` idiom) |
| 104 | `HE_PortalComponent` | door/stair link data for the router (composed onto door/stair objects) |
| 105 | `HE_LotComponent` | lot root: grid origin/extents, storeys, shell-chunk entity refs, roof entities |
| 106 | `HE_UIRootComponent` | HUD canvas owner on a DontDestroyOnLoad entity (`ZM_MenuRoot` pattern) |

`Games/Hearth/Source/` holds the headless, engine-light systems (every one unit-covered — repo mandate): `Core/` (HE_Clock, HE_Ids), `Grid/` (HE_LotGrid, HE_GridRouter, HE_RoomGraph), `Sim/` (HE_Needs, HE_Utility, HE_ActionQueue, HE_InteractionLogic, HE_Traits, HE_Skills, HE_Careers, HE_Relationships), `Build/` (HE_BuildOps, HE_WallMesh, HE_Catalog), `Econ/`, `Save/`, plus `HE_Events.h` (POD rows, `CB_Events.h` idiom) and `HE_Bindings.h` (the one file allowed raw key codes). Content is **compiled constexpr data tables** (`CB_BuildingDefs` idiom): `HE_NeedDefs`, `HE_TraitDefs`, `HE_InteractionDefs`, `HE_CatalogDefs`, `HE_CareerDefs`, `HE_SocialDefs`; tuning JSON in `Games/Hearth/Config/` (DevilsPlayground precedent).

**Determinism stance:** the sim core runs on sim time only, with per-sim seeded PCG32 (`ZM_GenRNG` idiom) — a fixed-seed day replays to an identical state hash (tested, §13). Rendering/presentation reads real dt and is excluded from the hash.

## 3. ★ Engine Gap Analysis & Work Packages

Everything below was **verified absent or broken in the current engine**. Sizes: S ≤ 1 wk · M 1–3 wk · L 3–6 wk · XL 6+ wk for one engineer *including* the mandated unit coverage and same-commit migration of all existing callers (no legacy surfaces — repo rule). Phase 0 = foundations, 1 = vertical slice, 2 = town, 3 = retail.

### 3.1 Evidence summary (what is missing today)

- **Audio does not exist.** `Zenith/Core/Zenith_AudioBus.h:53` — "stub until the post-MVP audio playback layer lands"; `EmitSound` is a no-op in shipping builds. No backend, decoder, mixer, or listener anywhere in the repo.
- **Touch gestures beyond tap do not exist.** `Zenith_Pointers` gives 8 pointers with a claims system (`Zenith/Input/Zenith_Pointers.h:104`, `uMAX_POINTERS = 8`) and a tap primitive — no pinch, long-press, double-tap, or two-finger recognizers (the old `Zenith_TouchInput` was deliberately deleted, commit `72f1c900`).
- **UI resolution scaling is computed and never used.** `Zenith/UI/Zenith_UICanvas.cpp:489` computes `m_fScaleFactor`; nothing consumes it — layout, sizes, and fonts are raw pixels. Windows `GetDisplayScale()` is hardcoded 1.0 (`Zenith/Windows/Zenith_Windows_Window.h`). No safe-area/notch handling anywhere.
- **UI quad budget:** `FLUX_MAX_QUADS_PER_FRAME = 1024` (`Zenith/Flux/Quads/Flux_QuadsImpl.h:12`), a stack array (`:152`); a styled rect costs up to 4 quads — a Sims-scale HUD + catalog exceeds it.
- **No mobile texture formats.** The exporter encodes BC1/3/5 only (`Tools/Zenith_Tools_TextureExport.cpp`); no ASTC/ETC2 in the format enum, Vulkan mapping, or any capability check. Android loads desktop BC files — which works **only on the x86_64 emulator** (host desktop GPU); real Adreno/Mali devices do not support BC.
- **No app lifecycle hooks.** `Zenith/Core/Zenith_ProjectHooks.h` (read in full) declares no pause/resume/low-memory hook; Android `APP_CMD_PAUSE` only raises an input barrier, and Android kills backgrounded processes without ever delivering `APP_CMD_DESTROY` → save-on-suspend is currently impossible.
- **No shipping render scalability.** All runtime feature toggles are `DEBUGVAR`s inside `#ifdef ZENITH_TOOLS` — never compiled on Android. `Zenith_GraphicsOptions` (read in full) is the boot-time lever (CityBuilder precedent trims SSR/SSGI/SSAO/HiZ/grass/shadows/fog) but has **no TAA member** (TAA is default-on), no cascade count/resolution knobs, and no tier concept. Present mode is hardcoded FIFO (`Zenith/Vulkan/Zenith_Vulkan_Swapchain.cpp:184-194`); `m_bVSync` has no readers; no frame cap, no pacing, no thermal awareness.
- **No interior cutaway of any kind.** No clip planes (no `SV_ClipDistance` use), no stencil plumbing (pipeline spec exposes none; `Zenith_Vulkan_Pipeline.cpp` carries `//#TO_TODO: stencil`), no roof/wall fade, no per-entity hide API. Zenithmon's interiors are separate roofless scenes behind fade warps. **However**, the seam already exists: `Flux_GPUScene.h:77` gives every draw item `m_uFlags` with the low 16 bits "reserved for item flags" and the high 16 bits a working **per-view-slot mask** (`Flux_GPUScene.h:124-138`, proven by the material-preview view) — cutaway is plumbing, not a new pipeline.
- **Per-instance tint is dead for characters and statics.** The GPU scene item carries `m_uColorTintPacked`, but static and skinned draws hardcode WHITE (`Zenith/Flux/Flux_GPUSceneBuilder.cpp:86` and `:483`); only instanced-foliage groups feed it. Recoloring anything today costs one material asset (cap 1024) + one extra draw bucket per color.
- **Animation events don't fire from the state machine.** `Zenith/Flux/MeshAnimation/Flux_AnimationController.cpp:180` — "only for direct clip playback – state machine uses state callbacks". Interaction sync (apply hunger at the bite, play the chop SFX) needs notifies from SM-driven playback.
- **No text entry.** No text-field widget, no caret, and zero JNI in the repo → no Android IME/soft keyboard.
- **Skinned rendering has no batching, no LODs, no occlusion, no skinning cull**: each skinned submesh-instance is its own indirect draw in the camera and every surviving cascade; every animated model in the scene is skinned every frame; 100 bone matrices × 2 palettes (TAA history) uploaded per skeleton per frame (`Zenith/Flux/UnifiedMesh/Flux_Skinning.h`).
- **Navmesh is bake-time static** — no rebuild/tiling/dirty regions; runtime verbs are only block-polygon / block-at-point / stitch-portal (`Zenith/AI/Navigation/Zenith_NavMesh.h`). Cannot express "a new wall split this room" (grounds the §5 decision).
- Also absent: localization (hardcoded `const char*` everywhere; one Latin-range monospace MSDF font) · haptics (TilePuzzle's toggle is saved and read by nothing) · point/spot shadows (`m_bCastShadows` serialized, zero renderer consumers) · morph targets (no vertex lanes, no import) · app icons / release signing / AAB · any iOS layer.

### 3.2 Work packages

| ID | P | Title | What & why (consumer in Hearth) | Modules | Size | Risk |
|---|---|---|---|---|---|---|
| HE-ENG-01 | 0 | **Project lifecycle hooks** | `Project_OnLifecycle(PAUSE/RESUME/LOW_MEMORY)` added to the hooks contract; Android `OnAppCmd` + Windows close path route into it. All 6 games + the NewGame template gain definitions in the same commit. *Consumer: autosave-on-suspend (§11).* | `Core/Zenith_ProjectHooks.h`, `Android/Zenith_Android_Main.cpp`, `Windows/` | S | Low |
| HE-ENG-02 | 0 | **Gesture interpreter** | `Zenith_Gestures` on `g_xEngine`, built over the pointer table + claims: long-press, drag-threshold, double-tap, 2-finger pinch (scale + focus point), 2-finger pan. Runs after UI claim capture so widgets always win; two-finger gestures claim both pointers. Windows mirrors (wheel = pinch, MMB = 2-finger pan) so the game is drivable on the dev box. Full `Zenith_InputSimulator` multi-pointer test suite. *Consumer: camera, pie menu, build tools (§9).* | `Input/` (new), `Core/` frame contract | M | Med — arbitration edge cases |
| HE-ENG-03 | 0 | **UI reference-resolution scaling + safe areas** | Consume the orphaned `m_fScaleFactor` (`Zenith_UICanvas.cpp:489`) in `RecalculateScreenBounds`, font sizing, and style metrics — opt-in per canvas (`SetReferenceResolution` activates; scale = 1 keeps existing games pixel-identical). Real Windows `GetDisplayScale`. Safe-area: insets API + safe-area anchor presets (manifest `shortEdges`; exact cutout values arrive with the P3 JNI seam). *Consumer: every Hearth screen (§9).* | `UI/`, `Windows/`, `Android/` | M | Med — touch-target math interacts |
| HE-ENG-04 | 0 | **Quad/text budget raise** | `FLUX_MAX_QUADS_PER_FRAME` 1024 → 4096 (`Flux_QuadsImpl.h:12`); stack arrays (`:152`) → owned allocation; high-water debugvar. *Consumer: catalog grid + HUD (§9).* | `Flux/Quads/` | S | Low |
| HE-ENG-05 | 0 | **Shipping graphics tiers** | `Zenith_GraphicsOptions` gains `m_bTAAEnabled`, `m_uShadowCascadeCount`, `m_uShadowMapResolution`, `m_uMaxDynamicLights`; a small `Zenith_DeviceTier` boot heuristic games may consult; TAA-off path validated (jitter must zero — the no-AA story, §8). Includes the boot-order fix so a persisted tier choice (`Zenith_UserSettings`) is readable before `Project_SetGraphicsOptions`. Options stay set-once; runtime switching deferred (register, §3.3). *Consumer: §1 tier table.* | `Core/Zenith_GraphicsOptions.*`, `Core/Zenith_UserSettings`, `Flux/TAA/`, `Flux/Shadows/` | M | Med — TAA-off untested today |
| HE-ENG-06 | 0 | **Frame cap + pacing** | 30 fps cap (FIFO + frame-skip pacing; `MAX_FRAMES_IN_FLIGHT` stays 4 on Android), 60 opt-in on HIGH; dt clamp. Swappy rejected for now (§3.3). *Consumer: battery/thermal (Pillar 4).* | `Flux/Flux_SwapchainPolicy.*`, `Vulkan/…Swapchain.cpp`, `Core/Zenith_Core.cpp` | S | Low |
| HE-ENG-07 | 0 | **ASTC texture pipeline** | Vendor `astcenc` (tools-only, the Assimp pattern — hand-writing an ASTC encoder is not credible); `.ztxtr` gains ASTC 6×6 (albedo/detail) + 4×4 (normals/UI) blocks; per-platform variant selection at export; `TEXTURE_FORMAT_ASTC_*` enum + Vulkan mapping + `textureCompressionASTC_LDR` capability check with loud fallback; Gradle bundles the ASTC set. **Gate for any real-device ship** — today's BC files only render on the emulator. *Consumer: everything on device.* | `Tools/…TextureExport.cpp`, `Flux/Flux_Enums.h`, `Vulkan/`, `AssetHandling/`, `Middleware/astcenc/`, Gradle | L | Med — device matrix |
| HE-ENG-08 | 0 | **Audio module v1** | New `Zenith/Audio/` over vendored **miniaudio** (device/decode layer only; mixing ours): 32-voice pool; MUSIC/SFX/UI/AMBIENT buses; 2D distance + pan attenuation vs. a listener (top-down game — no HRTF); `.wav` + `.ogg` via AssetHandling; streamed music + crossfade; `Zenith_AudioBus::EmitSound` becomes real playback while the simulator build keeps the recording contract (both pinned by tests); PlaySound/SetBusVolume graph nodes; **Null backend** for headless CI (decode-verify, no device). Replaces the stub in the same commit. *Consumer: §10, GDD §14.* | new `Zenith/Audio/`, `Middleware/miniaudio/`, `AssetHandling/`, graph-node registration | L | Med — AAudio quirks via miniaudio |
| HE-ENG-09 | 0 | **Per-instance tint for static + skinned draws** | Feed the existing `m_uColorTintPacked` lane from a component (`Zenith_ModelComponent::SetTint`) instead of hardcoded WHITE (`Flux_GPUSceneBuilder.cpp:86`, `:483`). Buckets unchanged (tint is per-item) → colorways stop costing material assets and draws. *Consumer: CAS tints, placement ghosts, selection highlight, wall paint (§7, §6).* | `Flux/Flux_GPUSceneBuilder.cpp`, `EntityComponent/Zenith_ModelComponent.*` | S | Low |
| HE-ENG-10 | 0 | **Cutaway: per-item camera-hide + height-clip** | Two mechanisms on the existing draw-item flag word (`Flux_GPUScene.h:77`, low-16 "reserved for item flags"; high-16 view mask `:124-138` already proven by material preview): **(a) camera-view hide** — clear the camera bit, keep cascade bits, so hidden roofs/upper storeys still cast shadows and interior lighting stays stable; **(b) HEIGHT_CLIPPED** — uber-shader `discard` above a per-view `g_fCutHeight` for wall cut, camera view only. Entity API `SetRenderFlags`/`SetViewMaskOverride`. Binary hide only — the opaque deferred path cannot alpha-fade; screen-door dither is post-MVP polish. *Consumer: §8 cutaway, the signature view.* | `Flux/UnifiedMesh/` shaders, `Flux_GPUSceneBuilder.cpp`, `EntityComponent/` | M | Med — TAA/motion-vector interaction on toggle (mitigated: TAA off on Android) |
| HE-ENG-11 | 1 | **Animation events from the state-machine path** | Route SM-driven clip time through the same event scan as direct playback (`Flux_AnimationController.cpp:180`), with a defined crossfade contract (events fire from the dominant state only). *Consumer: interaction effect timeline (§4), audio hooks (§10).* | `Flux/MeshAnimation/` | S | Low |
| HE-ENG-12 | 1 | **Text field + on-screen keyboard** | `Zenith_UITextField` (caret, max length, filter) + `Zenith_UIKeyboard` (engine-drawn QWERTY overlay, Latin set) — zero JNI, headless-testable via InputSimulator. Android IME rejected for MVP (§3.3; revisit when the P3 JNI seam exists anyway). *Consumer: naming sims/households (§7, GDD §8).* | `UI/` | M | Low |
| HE-ENG-13 | 1 | **Skinned-path headroom (targeted)** | (a) Skip skinning for models failing every view frustum (today none are skipped); (b) size palette uploads to actual bone count instead of `MAX_BONES = 100`; (c) fix the per-skeleton per-frame heap churn in the TAA palette-history submit. Full skinned batching ("Stage 6") deliberately **not** pulled in — 12 sims × ~4 parts × (camera + 2 cascades) ≈ within the §1 draw budget. *Consumer: §1 animation/GPU budget.* | `Flux/Flux_GPUSceneBuilder.cpp`, `Flux/UnifiedMesh/Flux_Skinning.*` | M | Low |
| HE-ENG-14 | 1 | **SaveData slot metadata** | `ReadHeader(slot)` → {gameVersion, timestamp, payload size} + `EnumerateSlots(prefix)` — the header already stores these; nothing exposes them. ZM hand-rolled its own layer; make it engine surface. *Consumer: save/load UI (§11).* | `SaveData/` | S | Low |
| HE-ENG-15 | 2 | **VAT character path hardening** | The VAT instancing path (`Flux/InstancedMeshes/Flux_AnimationTexture.h`, 8 B/instance, tint lane already live) gains animated normals + 2-clip crossfade (`Flux_InstanceAnimData` spare bytes; 8 → 12 B). *Consumer: street-ambience sims (GDD §9) without skinned cost.* | `Flux/InstancedMeshes/`, instancing shaders | M | Med |
| HE-ENG-16 | 2 | **Point/spot shadow decision** | Recorded decision, not discovery: interior lights stay shadowless on mobile (IBL + SSAO + emissive carry the read, §8); the dead `m_bCastShadows` field is removed (no-legacy mandate) or implemented desktop-only. | `EntityComponent/Components/Zenith_LightComponent.*` | S | Low |
| HE-ENG-17 | 2 | **Haptics** | `Zenith_Haptics::Pulse(ms, strength)` — Android Vibrator via the P3 JNI seam; Windows no-op; wires TilePuzzle's orphaned settings toggle. *Consumer: placement thunks, confirm ticks (GDD §11).* | `Android/`, `Core/` | S | Low |
| HE-ENG-18 | 3 | **Release engineering** | Adaptive launcher icons, splash, release keystore flow, **AAB + install-time Play Asset Delivery** (packs stay local/offline without a 400 MB base fight), R8 config, versionCode automation, data-safety docs. | per-game `Android/`, `Build/` | M | Low |
| HE-ENG-19 | 3 | **JNI seam + Play Billing** | First Java in the repo: `hasCode=true`, `ZenithActivity extends NativeActivity`, one `Zenith_Jni` bridge (thread attach, class cache), Billing v7 client (query/purchase/acknowledge/restore), CRC'd local entitlement store. Offline-first: the game reads boolean pack flags and **never blocks on Billing**. Also unlocks IME, haptics, real cutout insets. *Consumer: GDD §13.* | `Android/`, Gradle/manifest templates, new platform-billing module | L | High — store review + refund edges |
| HE-ENG-20 | 3 | **Localization v1** | String-table asset + `HE_Text(key)` lookup (Hearth routes every string through it from day 1 so this stays a data swap), MSDF font fallback chain + extended-Latin atlas; locale from `AConfiguration`. CJK out of v1 (font-pipeline memory decision). | `Core/`, `Flux/Text/`, `Tools/` font bake | L | Med |
| HE-ENG-21 | 3 | **Morph targets (CAS depth)** | Morph lanes in the skin vertex + compute accumulation pre-skin + Assimp import + CAS sliders. **Post-1.0, data-gated**: taken only if CAS-depth retention data demands it; bone-scale presets + texture synthesis cover launch. | `Flux/UnifiedMesh/Flux_Skinning.*`, `AssetHandling/`, `Tools/` | XL | High |

### 3.3 Rejected alternatives (with revisit triggers)

| Rejected | Why | Revisit trigger |
|---|---|---|
| **Forward+ renderer rewrite** | The deferred path is already GPU-driven (one indirect draw per bucket) and boot-trimmable; a second lighting architecture is a quarter of engine work before content exists | MID device cannot hold 30 fps at 720p-class after HE-ENG-05/06/13 land, with G-buffer+lighting > 8 ms measured |
| **Swappy / Android Frame Pacing** | A dependency for what a clamp does at 30 fps FIFO | Measured jank > 5% of frames on the device lane with the simple cap |
| **Android IME at MVP** | First JNI in the repo + hasCode flip + per-OEM IME chaos — for naming sims | The P3 JNI seam (HE-ENG-19) lands anyway → HE-ENG-12 grows an IME backend |
| **Skinned batching ("Stage 6")** | 12-sim cap fits the draw budget without it | Design needs > 12 concurrently skinned sims on-lot |
| **Runtime graphics-quality switching** | `Zenith_GraphicsOptions` is set-once by contract; restart-to-apply is acceptable for a tier choice | Player-visible demand after tiers ship |
| **Engine navmesh for lots** | No dynamic rebuild/tiles; patch verbs can't express topology edits (§5) | Never for lots; navmesh remains an option for a static neighborhood exterior at P2+ |
| **World-space UI module** | ZM's world→screen projection pattern suffices for plumbobs/labels | Batched 3D-anchored UI becomes a measured CPU hotspot |
| **Prefab nesting** | Engine prefabs are single-entity; Hearth composes multi-entity objects from data tables instead | A second game needs authored multi-entity prefabs |
| **Point/spot shadows** | Bandwidth on tilers; the interior look is carried by IBL + SSAO + emissive (§8) | Desktop-only differentiation pass post-1.0 |

## 4. Simulation core

**Clock** (`HE_Clock`): sim-minutes advance = real-dt × speed × 60; speeds {0, 1, 3, 10}; calendar = day index + weekday; publishes `SunAngleDegrees` once per frame to the scene's `Zenith_SunComponent` (the `CB_DayNight` pattern — the engine sun is a static authored angle that games must drive). All gameplay durations are sim-time; UI/animation read real dt. Frame-count budgets (never wall-clock) for transitions, per ZM's CI-hardened precedent.

**Needs** (`HE_Needs`): each motive is 0–100 with a per-motive decay curve (piecewise-linear over sim-hours, trait-modified). Crisis thresholds at 15 (warning) and 5 (crisis override). Mood = Σ motive weights + Comfort + Environment + moodlets, mapped to 5 bands.

**Utility autonomy** (`HE_Utility`): for an idle sim, score every advertised interaction within lot range:

```
score = Σ_needs( advert.fDelta[n] × W_n(fNeed[n]) )      // W_n: response curve, steep as need drops
        × fDistanceFalloff(1 / (1 + fCells/16))
        × fTraitBias × fMoodGate
        × fCommitment      // ×1.3 same object/category as current, ×0.5 if abandoned <30 sim-min ago
```

Worked example: Hunger 22 (W=0.83), Fun 60 (W=0.19). Fridge *Grab Leftovers* adverts Hunger+40, Fun+5, 6 cells away → (40×0.83 + 5×0.19) × 0.94 ≈ **32.1**. TV *Watch* adverts Fun+30, 3 cells → 30×0.19 × 0.97 ≈ **5.5**. The sim eats. Amortization: ≤ 2 full re-scores per frame round-robin; a crisis forces immediate re-score. Per-sim PCG32 breaks ties so replays are deterministic.

**Action queue** (`HE_ActionQueue`): ≤ 6 intents; player-issued intents carry priority PLAYER > autonomy AUTO; crisis inserts at front (cancellable). Dequeue → the **interaction FSM** (`HE_InteractionRuntime`, pure logic + component glue, `ZM_InteractionLogic`/`Runtime` split):

```
ROUTE (router request, slot RESERVED) → APPROACH (follow path)
→ ORIENT (face slot; quat*(0,0,1) heading — repo convention, never eulerAngles)
→ ENTER (animator SM state from def row) → LOOP (duration sim-min; effects at anim events
   [HE-ENG-11] or pro-rata per sim-min fallback) → EXIT → DONE
Failure edges: ROUTE_FAILED → re-score · SLOT_STOLEN → re-reserve or re-score
· CANCELLED (player) · CRISIS_PREEMPT → requeue current at AUTO priority
```

Reject reasons are enumerated most-specific-last (ZM's unit-pinned precedence idiom) so tests can assert *why* an interaction refused.

**Socials** resolve as paired interactions (initiator FSM + a lightweight responder join); success odds = f(Charisma, mood, axis values) from `HE_SocialDefs`; deltas applied both ways; group conversations deferred (GDD §6).

**Careers**: shift window in sim time; at departure, the sim despawns to a record; performance = mood band + skill level vs. track curve; return applies pay + moodlet.

## 5. Lot grid & routing

**Data model — walls on cell edges (decided).** Per storey (cap 64×64 cells, 3 storeys; MVP lots 24×20×1):

- **Cells**: `u8` floor material · `u8` derived room id · `u16` occupancy (object slot | EMPTY) · `u8` reservation (sim slot | FREE).
- **Edge lattices**: north–south walls `(W+1)×D`, east–west walls `W×(D+1)`; each segment `u16`: `{type:4, cutout:4 (none/door/window), paintA:4, paintB:4}`.
- Rationale vs. cell-occupancy walls: thin walls consume no floor area, furniture sits flush against walls, doors are naturally edge features = router portals, and rooms are exactly "cells enclosed by edges". Diagonals explicitly out of v1.
- **Rooms** (`HE_RoomGraph`): flood fill bounded by wall edges; recomputed per dirty region; room Environment score = light + decor − mess feeds §4.
- **Objects**: footprint = cell mask + 4-way rotation; **interaction slots** = data-authored (cellOffset, facing) pairs; a slot must be walkable + unreserved at ROUTE time.

**Router** (`HE_GridRouter`): per-storey A* (binary heap; cardinal + corner-cut-forbidden diagonals) over cost grid derived from occupancy + wall edges; **portals** = door edges (open/locked) and stair runs (paired cells linking storey layers); in-room string-pull smoothing. **Incremental invalidation**: build ops mark dirty rects; cached paths whose corridor intersects a dirty rect are dropped; affected sims re-enter ROUTE (visible as a natural "huh?" beat, not a teleport — no-teleportation repo mandate).

**Why not the engine navmesh:** generation is bake-time; there are no tiles, no dirty-region rebuilds, and the runtime verbs (`SetPolygonBlocked` / `SetBlockedAtPoint` / `StitchPortalAt`, `Zenith/AI/Navigation/Zenith_NavMesh.h`) toggle *existing* polygons — they cannot express "this wall now splits the room into two". The grid **is** the build-mode authoring format, so the router is exact, incremental, and fully headless-testable. The baked navmesh remains an option for a static neighborhood exterior shell at P2+.

## 6. Build mode

- **Commands, not edits**: every verb (`PlaceWallRun`, `SetFloor`, `PlaceObject`, `MoveObject`, `Demolish`, `PaintWall`, …) is a reversible `HE_BuildOps` command with `Apply`/`Revert` → free undo/redo and a single validation choke point (footprint clear, wall-edge legality, portal reachability, funds).
- **Ghost preview**: catalog drag spawns a ghost entity tinted green/red via the tint lane (HE-ENG-09); confirm puck commits (GDD §7). Ground picking = camera ray → active-storey plane (CityBuilder `PickGroundPoint` pattern, simplified — lots are flat).
- **Shell meshing** (`HE_ShellGen`): walls/floors/roofs are **runtime procedural meshes**, one chunk entity per (storey, 8×8-cell region) per material, rebuilt per dirty region via the `ZM_GenCommon` toolkit (`AppendBox`, `AppendGableRoof/HipRoof/FlatRoof`, normals/tangents generators, validators — `Games/Zenithmon/Source/Gen/ZM_GenCommon.h`). **Copy the toolkit into `HE_` first**; promote to a shared engine lib only when a second consumer exists (avoids coupling Hearth to Zenithmon's tree). Runtime path: `Zenith_MeshGeometryAsset::CreateFromGeometryData` → `Zenith_ModelComponent::AddMeshEntry` → `MODEL_MESH` collider → `RebuildCollider`.
- **Collider caveats** (documented on `Zenith_ColliderComponent`): explicit dimensions don't serialize, and `RebuildCollider` drops sensor/gravity/lock state — `HE_ShellGen` re-applies flags after every rebuild; door trigger volumes are separate sensor colliders that survive wall rebuilds.
- **Budget**: ≤ 2 chunk rebuilds/frame amortized; a full 24×20 lot rebuild (load, bulldoze) fits one frame headless and ~3 frames on device.

## 7. Sims content pipeline (CAS)

- **One shared skeleton** (51-bone StickFigure-class: jaw, eyes, fingers) + **one shared clip set**; every sim body/outfit part is a mesh on that skeleton — the shipped `ZM_HumanGen` architecture (34 humans, one skeleton, 9 shared clips) evolved from bake-time to catalog-time.
- **A sim's appearance = a genome**: {body preset, head preset, part ids (top/bottom/shoes/hair), tint indices (skin/hair/outfit-primary/outfit-secondary)} — a few bytes in the save, deterministic to rebuild.
- **Parts are offline-baked `.zmodel`s** (Assimp import is tools/Windows-only; Android consumes baked binaries). MVP wardrobe swap = model rebuild at the dresser (acceptable hitch at a UI moment); a `ReplaceMeshEntry` API is a P2 nice-to-have, not a blocker.
- **Tints ride HE-ENG-09** — one shared part asset serves every colorway; swatches are free content (GDD §8). Faces at MVP: preset heads + jaw/eye bone posing + texture synthesis (`ZM_TextureSynth` lineage); bone-scale preset variations at v1.0; morphs post-1.0 (HE-ENG-21).
- **Anim inventory** (MVP): locomotion SM (idle/walk/run + carry variants) + one interaction state per `HE_InteractionDefs` anim class (~15 shared classes cover 25 interactions — eat-standing, sit-use, lie-sleep, stand-fiddle, …). Naming convention binds def rows to SM states (`"Sit_Use_TV"`); a boot unit asserts every def row's state exists (content can't silently T-pose).
- IK garnish: hand-to-anchor on use objects (RenderTest gun/racket FABRIK precedent); look-at for conversations.
- **Ambience sims** (P2): VAT instances via HE-ENG-15 — position-only animation, tint lane, 8–12 B/instance; never simulated.

## 8. Rendering strategy

**Boot configuration** (`Project_SetGraphicsOptions`, CityBuilder precedent — set-once, shipping-safe):

| Off on Android | On (all tiers) | New knobs (HE-ENG-05) |
|---|---|---|
| SSR, SSGI (default-off already), volumetric fog, HiZ, grass, terrain, SDFs, CPU particles | shadows (tiered), SSAO (MID+), IBL diffuse+specular, HDR bloom, atmosphere + multi-scatter, translucency, GPU particles, quads, text | `m_bTAAEnabled=false` (Android), cascades 2×1024² (MID), max dynamic lights 32 |

- **AA answer**: TAA off on Android — native-DPI panels at 30 fps hide aliasing well; the FXAA stopgap that predated TAA was removed and can be resurrected as a fallback if device captures demand (revisit register). Windows keeps TAA.
- **Cutaway** (HE-ENG-10): camera drives it — active storey from the floor chevrons; roofs + storeys above → camera-view-mask hide (still in shadow views, so interior lighting doesn't pop when toggling floors); walls-down mode → HEIGHT_CLIPPED discard at 1 m above the active floor, camera view only; walls-up disables the clip. Auto roof-hide when the camera is inside/below the roof plane.
- **Interior lighting recipe** (no point/spot shadows — HE-ENG-16): atmosphere-driven IBL supplies ambient (the engine's virtual ground-albedo term is what keeps interior walls from reading black); SSAO supplies contact grounding; shadowless clustered points for lamps (≤ 32 visible of the 256 engine cap); emissive materials sell fixtures; windows are translucent quads day-lit by the sun.
- **Draw accounting** (§1 budget): shell chunks merged per material per storey (~20–40 buckets), furniture shares `.zmodel`s across colorways via tint (~60–80 buckets), 12 skinned sims × ~4 parts ≈ 48 skinned draws × (camera + 2 cascades) — comfortably ≤ 300 indirect draws.

## 9. UI & input

- **Canvas scaling**: Hearth authors at 1920×1080 reference and opts into HE-ENG-03 scaling; all screens safe-area anchored; touch targets keep the engine's 57-logical-px physical floor.
- **Input tables** (`HE_Bindings.h`, `ZM_Bindings.h` model): profiles `HE_P_TOUCH {TOUCH}` / `HE_P_DESKTOP {KEYBOARD|MOUSE}`; **MOUSE deliberately in no profile** — UI taps ride the pointer table unconditionally (ZM precedent). Actions (ids from 16): SELECT, CANCEL(+`SystemBack`), SPEED_0/1/2/3, MODE_BUILD, MODE_MAP, FLOOR_UP/DOWN, ROTATE_OBJECT, DELETE, UNDO, REDO + desktop camera rows (WASD pan, wheel zoom, MMB orbit).
- **Gestures are consumed in code, not action rows** (positions can't ride bindings): arbitration order per frame = UI widget claims (engine step 10c/d) → `Zenith_Gestures` (10e) → camera fallback. Two-finger gestures always camera; one-finger is contextual (world drag = pan in live, tool stroke in build — tools are one-finger only, so the split is never ambiguous).
- **Pie menu**: screen-space radial on the UI canvas at the tap point (clamped inside safe area), claims its pointer, 6 slices + More; slice payload = interaction def id.
- **Catalog**: GridLayout + ScrollView with **card recycling** (visible ~24 cards × ~6 quads ≈ 150 quads; full HUD + catalog + drawers budgeted < 2,500 of the 4,096 cap).
- **World-anchored UI** (plumbob, needs pips, speech bubbles): CPU world→screen projection per frame (ZM name-tag pattern) — no world-space UI module (register, §3.3).
- **Text entry**: HE-ENG-12 keyboard for naming; filtered charset; IME later.

## 10. Audio integration

- `HE_AudioDirector` owns: music state machine (menu/live-day/live-night/build, crossfade on transitions), bus volumes from settings, bark selection (per-emotion pools, anti-repeat), ambience spawning per room/time.
- **Event sources**: animation events (HE-ENG-11) fire footsteps/effect SFX by name; the interaction FSM fires enter/exit stingers and barks; build ops fire placement/paint SFX; UI fires tick/confirm.
- Positional model: 2D attenuation vs. the camera focus point (not the camera eye — top-down games read distance-to-subject), radius from def rows.
- Headless CI runs the Null audio backend; unit tests pin decode + bus math; the simulator recording contract (perception tests) stays intact.

## 11. Persistence

- **Slots** (`Zenith_SaveData` + `HE_SaveSlots` layer, ZM idiom): `hearth_household_<0..2>` (manual) + `hearth_autosave` + `hearth_settings`. Slot UI reads HE-ENG-14 headers (name/funds/date/timestamp without payload loads).
- **Schema**: engine header (magic/version/gameVersion/CRC/timestamp) + chunked payload `{fourcc, u16 version, u32 size}`: `CLK` (clock/speed), `LOT` (grid dims; walls + floors RLE; object list {catalogId, cell, rot, storey, state, tints}), `SIMS` (genome, needs, skills, traits, career, moodlets, queue as intents), `RELS` (pair map), `ECON` (funds, bills), `META` (household name). Unknown chunks are skipped (forward-tolerant); per-chunk version gates + additive-only fields; migrations hand-written in the read callback (engine contract) and covered by a committed old-save corpus test (ZM discipline).
- **Autosave triggers**: `Project_OnLifecycle(PAUSE)` (HE-ENG-01 — the checkpoint) **+ a rolling checkpoint every 5 real minutes of play + on leaving build mode** — Android SIGKILLs without callbacks, so the interval is the safety net, not a luxury. Sim-day boundaries rotate the autosave.
- Test builds redirect into the engine's automated-test save sandbox automatically (engine feature, kept).

## 12. Platform & release

- **Android**: per-game Gradle tree (`android:true` in `Hearth.zproj` from day 1); minSdk 26 / target 34; arm64-v8a + x86_64; 16 KB page compliance is already solved engine-wide; `screenOrientation=landscape`; shaders pre-baked by FluxCompiler (Windows) into `.spv + .spv.refl` and bundled — **the Gradle asset block must include `Zenith/Flux/Shaders`** (Combat/DP forgot and cannot render on Android; boot characterization test asserts a known shader loads).
- **Lifecycle**: PAUSE → checkpoint + input barrier (engine); RESUME → swapchain recreate (engine) + clock stays paused until foreground focus; LOW_MEMORY → asset-cache trim hook.
- **Billing** (P3, HE-ENG-19): JNI bridge; entitlement bits cached locally (CRC'd), restored via Play; catalog/CAS rows carry `packId` — locked rows render badged; the sim never blocks on store state.
- **Release** (P3, HE-ENG-18): AAB + install-time Play Asset Delivery for pack assets; adaptive icons; release keystore; data-safety declaration ("no data collected" — the game is offline).
- **Windows**: `_False` ship config with packs via key file (v1); `_True` is the content-authoring workstation (scene authoring, asset bakes, FluxCompiler).

## 13. Testing strategy

Per the repo's regime (headless Null configs run every render path; gates assert `ran == pinned baseline` exactly):

- **Boot units** (`HE_Tests_*.cpp`, run at every boot): every `Source/` system — clock arithmetic, need decay curves, utility scoring (worked-example pins), queue semantics, FSM transition table, grid/edge legality, router (paths, portals, invalidation), room flood-fill, build-op apply/revert symmetry, RLE round-trip, save chunk round-trip + corpus migration, econ math, bindings-table sanity, def-row ↔ SM-state existence. New row `Hearth` in `Tools/unit_baselines.json`; every suite growth bumps the pin from an **observed** run (repo law).
- **Automated tests** (`HE_AutoTests_*.cpp`, Null exe, `zenith test Hearth --headless`): boot characterization; InputSimulator **multi-pointer gesture scripts** (pinch stream → camera distance assertion; long-press → pie menu open; drag-ghost → placement); build-then-route integration (wall placed → path invalidated → sim re-routes); save → load → resume equivalence; autosave-on-PAUSE; speed-change integrity.
- **Determinism harness**: fixed seed + scripted day at speed 3 → state hash equal across runs and across Debug/Release (sim core avoids the `/fp:fast` authored-bytes trap by keeping all authored constants integral or `bit_cast`-frozen — repo precedent ZM-D-183).
- **Windowed set** (`m_bRequiresGraphics`, board label `windowed`, human-run): cutaway pixel tests (roof hidden ⇒ interior pixels present; clip plane at wall band), tint-lane pixel test (ZM interior-tint precedent), device screenshot pass.
- **Perf asserts as tests**: headless sim tick with 8 sims × 1,000 frames under budget; router worst-case (full-lot repath) under budget; shell rebuild op count.
- **Committed-scene byte stability**: the three committed `.zscen` (below) get the ZM byte-stability test; all committed rotations use `AddStep_SetTransformRotationQuat` frozen constants; **headless runs may CREATE but never CHANGE a committed scene** — the publish guard (`Zenith/Editor/Zenith_Editor.cpp:1377`) enforces it, and lot content never touches `.zscen` at all (runtime-built from save data), which sidesteps the guard for everything player-mutable **by construction**.

## 14. Build / CI

- `Games/Hearth/Hearth.zproj` `{schemaVersion:1, name:"Hearth", android:true}`; scaffold via `zenith new Hearth` (P0); layout per repo convention (`Hearth.cpp`, `HE_Bindings.h`, `Components/`, `Source/`, `Tests/`, `Assets/`, `Config/`, `Docs/`, `Android/`).
- **Scenes**: `FrontEnd.zscen`, `Lot.zscen` (ground plane, environment entity, director/camera/UI roots — nothing player-mutable), `CAS.zscen` — all boot-authored in `_True`, committed, windowed-human re-authored only. First build after clone must be a `*_True` config (repo rule).
- **Gates** (modeled on `zm-tests.yml`): build `Null_vs2022_Debug_Win64_True` → unit gate (`run_unit_gate.ps1 -Game Hearth`, baseline from `Tools/unit_baselines.json`) → `zenith test Hearth --headless`. Engine-touching diffs additionally run the engine gate per the category-`paths` mechanism (`zagent.project.json` gains a Hearth category when the game exists — out of scope for this document, noted for the board).
- **Android lanes**: x86_64 emulator lane is valid **pre-ASTC** (host desktop GPU tolerates BC textures); a physical-device lane becomes meaningful only **post-HE-ENG-07** and is the P1 exit gate.

## 15. Risk register (top 5)

| # | Risk | Mitigation | Measuring stick |
|---|---|---|---|
| 1 | **Real-device GPU perf of the MRT3 deferred pipeline** — all Android proof to date is the x86_64 emulator on a desktop GPU | Tier trim at boot (§8) + HE-ENG-06 pacing + HE-ENG-13 headroom; device smoke inside P1, not after | G-buffer+lighting ≤ 9 ms @ MID; else the §3.3 forward+ trigger fires |
| 2 | **Skinned character cost on tilers** (no batching/LOD/skinning-cull today) | Hard 12-sim cap; HE-ENG-13; VAT for ambience (HE-ENG-15) | animation ≤ 4 ms CPU, skinned draws ≤ 150 incl. cascades |
| 3 | **Gesture/UI arbitration feel** | Claims-integrated design (HE-ENG-02) + InputSimulator regression streams + weekly on-device feel passes from P0 | zero stolen-pointer bugs in the automated gesture suite; subjective pass gate at P1 exit |
| 4 | **Per-interaction content cost** (anim × data × audio × test per row) | ~15 shared anim classes cover 25 MVP interactions; data-table + def↔SM boot assert; barks are class-level not row-level | interaction added in ≤ 1 day all-in at P1 steady state |
| 5 | **UI scaling migration regressing shipped HUDs** (HE-ENG-03 touches CB/ZM in a no-legacy commit) | Opt-in scaling (scale=1 identity for non-adopters) + InputSimulator UI suites on all three games | all existing games' UI tests green with zero pixel drift at scale=1 |

## 16. Open questions (parked, with owners)

1. **Ultra-speed semantics during shifts** — skip-to-event vs. simulate-through (sim-fidelity vs. battery). Owner: design, decide in P1 with device power data.
2. **Visitor cap & spawn cadence** — 4 concurrent assumed; tune against the 12-sim budget. Owner: design + perf, P2.
3. **Community-lot simulation LOD** — do café employees simulate or animate? Leaning animate-only (records + VAT). Owner: design, P2.
4. **Thermal policy** — lock 30 fps always vs. HIGH-tier 60 opt-in; needs device thermal data. Owner: tech, P1 device lane.
5. **Pack size ceiling** — PAD install-time budget per pack (target ≤ 60 MB ASTC). Owner: tech art, P3.
6. **CJK trigger** — market data threshold that justifies the font-pipeline spend (HE-ENG-20 scope grows ~2×). Owner: product, post-1.0.

## 17. Phased roadmap

| Phase | Engine WPs | Game deliverables | Exit criteria |
|---|---|---|---|
| **P0 — Foundations** (~6–8 wk) | 01–10 | `zenith new Hearth`; bindings; clock/grid/router/needs/utility cores with full unit suites + baseline row; greybox lot; gesture camera + tap selection; one skinned sim walking to a tapped cell | Emulator **and one real device** render the test lot at 30 fps (ASTC); suspend/resume checkpoints; all gates green |
| **P1 — Vertical slice = MVP, "one sim, one house"** (~10–12 wk) | 11–14 (+07 finishing) | Interaction FSM + 25 interactions / 40 objects; build/buy (walls, floors, doors, paint, 1 roof, undo); CAS-lite + naming; save/load + autosave; full touch UX incl. pie menu; audio v1; needs/mood HUD | A stranger plays 15 min on a phone unprompted; autosave survives process death; 30 fps sustained on the reference device |
| **P2 — Households & town** (~12 wk) | 15–17 | Multi-sim scheduling + socials/relationships; careers ×3 + skills full; 6-lot neighborhood + map travel; visitors; VAT street ambience; content to 60 interactions / 120 objects; stairs + 3 storeys | Sustained 30 fps with 8 sims + visitors; relationship arc playable end-to-end |
| **P3 — Retail** (~10 wk) | 18–21 | Cosmetic packs + Billing + local entitlements; icons/signing/AAB/PAD; localization plumbing (strings routed from day 1); haptics; device-tier QA matrix | Play-store-ready build; packs purchasable offline-after-install; data-safety clean |

**Critical path:** gestures → camera/selection → interaction FSM (the P0–P1 spine) · **ASTC + first-device bring-up is the longest external-risk item — it starts in P0 and is the P1 gate, never later** · audio is the longest independent pole (start early, lands mid-P1) · JNI → Billing gates P3.
