# Zenithmon Status

**Last updated:** 2026-07-13 -- **S3 automated implementation and the definitive post-overlay-hitch local authority gate are COMPLETE (ZM-D-057); human visual acceptance is the only remaining S3 item.** PlayerHome build 40, both live door triggers, deterministic 0.20 s fade, opaque generation-exact camera barrier, global cross-canvas UI ordering, procedural material ownership, complete disabled-text frame draining, and synchronous zero-duration overlay opacity are green. Do not tick S3 complete or begin S4/S5 until the user reviews the ignored captures.

**★ Grass-render rework (2026-07-13, first visual review REJECTED Dawnmere grass):** the reviewer saw no grass in the Dawnmere exterior. Root cause (confirmed with runtime instrumentation, not theory): grass generated + uploaded fine but `Flux_GrassImpl` built a single terrain-spanning chunk whose LOD was picked from the camera->AABB-centroid distance (~150 m), forcing the WHOLE map to LOD3 (~12.5%), while the town-center spawn sat in a grass-free hole (all dabs were peripheral). Fix: (1) engine -- partition blades into a per-region chunk grid (`GrassConfig::fCHUNK_SIZE`), draw one instanced call per visible chunk with `firstInstance` = the chunk offset, and add `SV_StartInstanceLocation` to `Flux_Grass.slang` so the shader recovers the global blade index (shared engine change; RenderTest `TerrainEditorSmoke` re-verified); (2) game -- two central town-lawn grass dabs so grass surrounds the spawn (paths/pads still erase walkways) + `fGRASS_DENSITY_SCALE` 0.15->0.70; (3) test gap -- `ZM_GrassRegeneration_Test` asserted generate+upload but never on-screen visibility, so a new `SpawnVisible` phase now asserts `GetVisibleBladeCount() > 0` from the spawn camera. Dawnmere now bakes/uploads **573,693** blades from **8,098** triangles. Re-gated ALL-GREEN (5-config builds, boot units 1773 ran / 1772 passed / 0 failed, 6/6 automated incl. the new visible-blade assertion, RenderTest regression). Fresh captures produced and **APPROVED by the user 2026-07-13 -- the S3 visual gate is SIGNED OFF; S3 is COMPLETE and S4/S5 are unblocked.**

**Read this first each session.** [Roadmap.md](Roadmap.md) is the task source; [Questions.md](Questions.md) holds open rulings; [Shortfalls.md](Shortfalls.md) is the gap audit. Terrain contracts are ZM-D-051..054, controller/camera ZM-D-055, traversal infrastructure ZM-D-056, and the PlayerHome/fade/global-UI milestone ZM-D-057.

## Working model -- MASTER-ONLY, no branches/PRs (ZM-D-031)

All work is committed directly to `master` and pushed. Never create a feature branch, PR, or worktree. The local build/unit/headless/windowed gate is authoritative; `zm-tests` is the post-push backstop and red is fixed forward.

## Definitive post-overlay-hitch automated gate -- all green

- Regeneration: **2.401 s**.
- Five win64 builds: Vulkan Debug Tools=true **11.225 s**, Debug Tools=false **11.755 s**, Release Tools=true **11.213 s**, Release Tools=false **11.031 s**, and D3D12 Debug Tools=false **7.656 s**.
- Boot units: **1773 ran / 1772 passed / 0 failed / 1 skipped** (the existing quarantined `RegistryWideNodeRoundTrip`), with **180.640 s** helper wall under the canonical watchdog. `ZM_WorldTraversal` remains **16** cases; current game-unit inventory is **695** and the workflow baseline is **1773**.
- Headless registry: **6/6 harness entries**, **1.590 s** process wall, semantically **2 real passes + exactly 4 graphics-required skips**. `ZM_ControllerHarness_Test` passed **142 frames / 25.100 ms**; `ZM_Boot_Test` passed **1 / 0.018 ms**.
- Windowed: `ZM_WarpInfrastructure_Test` **29 / 2008.714 ms** (**14.869 s** wall), one frame shorter because instantaneous overlay opacity no longer waits for a UI update; `ZM_GrassRegeneration_Test` **11 / 2579.674 ms** (**15.125 s** wall); `ZM_DawnmerePlayerCamera_Test` **117 / 6212.128 ms** (**18.712 s** wall); real input-driven `ZM_PlayerHomeRoundTrip_Test` **673 / 14662.601 ms** (**27.514 s** wall).
- RenderTest rebuilt in **6.192 s**; `EngineBootShutdownSmoke` passed **1 / 28.606 ms** (**40.622 s** wall) and `TerrainEditorSmoke` **151 / 5291.193 ms** (**46.025 s** wall).
- Machine-readable results are ignored under `C:\dev\Zenith\Build\artifacts\zenithmon\s3\final\post_overlay_hitch_fix\`: **12 parsed JSON / 12 passed / 0 failed**, comprising six headless, four windowed, and two RenderTest results, with exactly the four expected headless graphics skips.

## What landed in the S3 gate candidate

- FrontEnd's persistent `ZM_GameStateRoot` now owns exact full-screen black order-10000 `WarpFade`. Input freezes on accepted traversal, SINGLE cannot issue below alpha 1, placement and motion reset occur behind black, exactly one active-scene main follow camera must target the replacement Player generation, and input unlocks only at alpha 0. Missing/ambiguous scene, Player, camera, or overlay dependencies fail closed opaque and frozen. Live fade state remains absent from the v1 component stream.
- Dawnmere authors a greybox home shell, `FromHome` feet marker **(384,26.590313,482)**, and live `HomeDoorTrigger -> (40,"Door")`. PlayerHome build 40 authors a collidable greybox interior, `Door` feet marker **(0,0,3.5)**, scene-owned Player/main camera, and live `PlayerHomeExitTrigger -> (2,"FromHome")`. Order **107** is `ZM_GreyboxVisual`; next free is **108**.
- UI quad sort is now global across canvases: stable ascending shared-queue upload, raw callers at order 0, and drop-newest with one warning when the 1,024-quad capacity is exceeded. Text overlay clipping retains the highest sort order for the frame and clears even across disabled-text pass boundaries. This keeps the persistent fade above later active-scene canvases.
- `Flux_TextImpl::DiscardPendingFrame` now owns every disabled/reset frame boundary in both legacy and render-graph paths, draining the pending queue, background/foreground/total counters, and overlay clip together. A clean re-enable cannot replay stale text or inherit stale clipping.
- `Flux_ModelInstance::SetMaterial` retains overrides with its owning handle, so `Zenith_ModelComponent` deserialization no longer adds a second manual reference. Procedural empty-model greybox records release their temporary decoded materials for registry reclamation across Dawnmere/PlayerHome reloads.
- `Zenith_UIOverlay` with zero/negative duration now makes `Show()` synchronously snap its current dim alpha to the configured dim alpha, including repair when already showing. `Hide()` synchronously clears current alpha/visibility and restores sibling interaction; positive-duration animation is unchanged. A hitch-sized manager update can therefore render actual black before it requests SINGLE rather than waiting for a later UI update.
- Four new T0 cases lock fade progression/load gating, placement/camera readiness, fail-closed dependency loss, runtime-state omission, production UICanvas forwarding/global queue order/capacity, text-clip arbitration, disabled/reset Text drain + clean re-enable, material retain/release + transient registry reclamation, direct instantaneous-overlay Show/Hide, and a **0.25 s** one-tick fade crossing whose load callback renders the real manager canvas and requires an actual sort-10000 alpha-1 quad before permission. These final lifecycle assertions extend existing cases; they add no registrations. The new P1 drives both real trigger overlaps via input, proves `Door` and `FromHome` placement, three scene generations, zero motion before unlock, no interior grass, and exterior grass/camera recovery.

## Human visual evidence -- ignored, never stage

The original captures (`01`/`02`/`03`, SHA-256 `9FEF...`/`1310...`/`B0D4...`)
were **REJECTED**: the reviewer confirmed no visible grass in the two Dawnmere
exterior views (`01` initial, `03` return). `02` (PlayerHome interior) was
accepted and is unchanged by the grass fix. After the grass-render rework above
(re-gated all-green), the fresh set was captured from a clean
`ZM_PlayerHomeRoundTrip_Test` run (passed 673 frames). These supersede the
rejected captures and are the evidence for the pending sign-off (valid
**1280x720**; captured, not yet visually accepted by the user):

- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\new_01_dawnmere_exterior_terrain_grass_camera.png` -- SHA-256 `00F64EC2A7F694BBC81B502DEF3865BBAB87831022B57A1DED7B25F6FFDBBCFD` -- Dawnmere town-center exterior, now a green grass lawn flanking the dirt route path.
- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\new_02_playerhome_interior.png` -- SHA-256 `5C19AE979731FB5DD8DD9FC76A886311272F9451323A737B5D5606D258BFD4D7` -- PlayerHome greybox interior (unchanged).
- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\new_03_dawnmere_return_camera_reacquired.png` -- SHA-256 `6319D30320D00AEAF917048B65D326FDF3126B89E415AEC97A10A35D663EE38E` -- Dawnmere return (FromHome spawn), grass lawn + generation-reacquired camera.

The user reviewed the fresh three on 2026-07-13 and accepted them ("those three
screenshots look fine"). The S3 visual gate is **SIGNED OFF**; the grass-render
rework (ZM-D-058) and this milestone are committed to master. S3 is COMPLETE --
S4/S5 are unblocked.

S3 visual sign-off: **APPROVED 2026-07-13** (fresh `new_01`/`new_02`/`new_03` set). Gate cleared.
