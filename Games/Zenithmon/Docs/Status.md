# Zenithmon Status

**Last updated:** 2026-07-13 -- **S3 automated implementation and the definitive post-overlay-hitch local authority gate are COMPLETE (ZM-D-057); human visual acceptance is the only remaining S3 item.** PlayerHome build 40, both live door triggers, deterministic 0.20 s fade, opaque generation-exact camera barrier, global cross-canvas UI ordering, procedural material ownership, complete disabled-text frame draining, and synchronous zero-duration overlay opacity are green. Do not tick S3 complete or begin S4/S5 until the user reviews the ignored captures.

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

Capture `capture_final_posthitch_20260713_183717` used the definitive binary;
its `ZM_PlayerHomeRoundTrip_Test` passed **673 frames / 14619.2 ms** with exit
0. All three ignored PNGs are valid **1280x720** files and
were inspected for capture integrity; that inspection is not the user's visual
acceptance:

- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\01_dawnmere_exterior_terrain_grass_camera.png` -- SHA-256 `9FEFA6E1B20CB9F1647F19A0416FCD6A80ACA653EB6EEEFE6A86DD722790A1DF`
- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\02_playerhome_interior.png` -- SHA-256 `13104E86246748BF58AF200DFAC213C2A6B6595A81086E30346B75857280B90E`
- `C:\dev\Zenith\Build\artifacts\zenithmon\s3\visual\03_dawnmere_return_camera_reacquired.png` -- SHA-256 `B0D49B1CE41ACB98AA184E55ECB1531D34DC76009C3BED0CBD67CCD61C3B4B41`

Review those three captures for Dawnmere terrain/grass/camera, the deliberately replaceable PlayerHome interior blockout, and the generation-reacquired return view. If approved, use StartPrompts.md prompt 4 to append the user verdict, tick the S3 visual-gate box, clear this marker, commit/push the docs, and only then resume the lifecycle loop. If rejected, record the requested rework and keep S3 active.

GATE-WAIT: S3 visual sign-off
