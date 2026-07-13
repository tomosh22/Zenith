# Zenithmon Status

**Last updated:** 2026-07-13 -- **S3 input/controller/camera milestone COMPLETE and fully gated locally (ZM-D-055).** Dawnmere now owns a traversable Player and follow camera. **CURRENT TASK: persistent `ZM_GameStateManager` (`DontDestroyOnLoad`) + `ZM_SpawnPoint`/`ZM_WarpTrigger` spawn-tag respawn.** PlayerHome and its door round trip follow; the hard human visual gate occurs only after both tasks and the full S3 automated gate are complete.

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open rulings; [Shortfalls.md](Shortfalls.md) is the current gap audit. Terrain contracts are ZM-D-051..054; the controller/camera/input contract and corrected spawn-centre rationale are ZM-D-055.

## Working model -- MASTER-ONLY, no branches/PRs (ZM-D-031)

All work is committed directly to `master` and pushed. Never create a feature branch, PR, or worktree. Before every push, the orchestrator runs the local authority gate: build, exact-baseline boot units, headless automated suite, and task-scoped windowed tests. `zm-tests` is the post-push backstop; fix forward on red.

## Build / tests -- all green

- Five win64 configurations: all four Vulkan Debug/Release x Tools true/false builds plus `D3D12_vs2022_Debug_Win64_False` are green.
- Boot unit gate: **1757 ran / 1756 passed / 0 failed / 1 skipped** (the existing quarantined `RegistryWideNodeRoundTrip`). This milestone adds exactly **20** `ZM_Overworld*` tests: input 5, controller policy 4, live physics 5, camera 4, and ECS/serialization 2.
- Automated registry: **4 tests**. Headless passes `ZM_Boot_Test` and the asset-free `ZM_ControllerHarness_Test`; graphics-required `ZM_GrassRegeneration_Test` and `ZM_DawnmerePlayerCamera_Test` skip as designed.
- Windowed `ZM_DawnmerePlayerCamera_Test`: **117 frames / 4990.3 ms**, covering real Dawnmere movement, fixed follow, SINGLE reload with generation-safe reacquisition, grass readiness, and FrontEnd teardown.
- Windowed `ZM_GrassRegeneration_Test`: **11 frames / 1924.3 ms**, retaining the exact **200,159 blades / 5,133 terrain triangles** load-reload contract.

## Last completed -- input, controller, camera

- `ZM_InputActions` maps WASD/arrows, pressed-edge confirm/cancel/menu, and either Shift for run; opposing axes cancel and the controller normalizes diagonal movement.
- `ZM_PlayerController` is component order **102**. It owns a dynamic, upright Jolt capsule derived from transform scale and drives camera-relative **horizontal-world speed** at **4 m/s walk / 7 m/s run**. On grounded walkable downslopes it adds only the tangent-required downward velocity for adhesion, preserving a stronger fall or positive step-assist rise. Slopes through **45 degrees** are accepted; qualified steps through **0.40 m** receive bounded velocity-only assistance. Invalid or nonpositive dt is a true no-op for controller state, animation, physics and facing; gameplay motion never teleports the body.
- `ZM_FollowCamera` is component order **103**. It preserves authored yaw, follows through a critically damped spring, clamps its arm against occluders, and caches only a generation-bearing same-scene `Player` ID: a still-live target moved to another scene is rejected, and SINGLE reload reacquires the replacement.
- Dawnmere authors the Player capsule centre at **(512, 26.9, 480)**. The baked surface sample at that exact XZ is **Y=25.98577**; with a **0.9 m** half-extent, the corrected centre starts above the real surface and grounds within the **1.05 m** probe. This is generator-authored placement, not the future spawn-tag system.

## Current task and remaining S3 order

1. Persistent `ZM_GameStateManager` + `ZM_SpawnPoint`/`ZM_WarpTrigger` spawn-tag respawn.
2. PlayerHome interior + door warp round trip using SINGLE loads + fade.
3. Complete the S3 automated walk/door round-trip gate and capture visual evidence. Only then write `GATE-WAIT: S3 visual sign-off`, commit/push the evidence docs, and stop for human review; do not pause at the intermediate persistence task.

## Notes for the next agent

- Baked terrain/scenes remain ignored; new terrain work keeps the named-set, bounded-export, atomic-marker and fixed registry contracts.
- `m_eGender` must enter the future S7 party/daycare serializer.
- For S5 AI integration, a fainted active still requires the caller to submit the forced switch.
