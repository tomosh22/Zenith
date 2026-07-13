# Zenithmon Status

**Last updated:** 2026-07-13 -- **S3 traversal-infrastructure milestone COMPLETE and fully gated locally (ZM-D-056).** Persistent manager, spawn-point, and warp-trigger infrastructure now resolves a direct FrontEnd -> Dawnmere `TownCenter` request deterministically. **CURRENT TASK: PlayerHome (build index 40) + Dawnmere door round trip using SINGLE loads + fade.** The hard human visual gate occurs only after that box and the full S3 automated gate are complete; do not stop now.

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open rulings; [Shortfalls.md](Shortfalls.md) is the current gap audit. Terrain contracts are ZM-D-051..054; controller/camera/input is ZM-D-055; traversal infrastructure and exact spawn semantics are ZM-D-056.

## Working model -- MASTER-ONLY, no branches/PRs (ZM-D-031)

All work is committed directly to `master` and pushed. Never create a feature branch, PR, or worktree. Before every push, the orchestrator runs the local authority gate: build, exact-baseline boot units, headless automated suite, and task-scoped windowed tests. `zm-tests` is the post-push backstop; fix forward on red.

## Build / tests -- all green

- Five win64 configurations: all four Vulkan Debug/Release x Tools true/false builds plus `D3D12_vs2022_Debug_Win64_False` are green.
- Boot unit gate: **1769 ran / 1768 passed / 0 failed / 1 skipped** (the existing quarantined `RegistryWideNodeRoundTrip`). This milestone adds exactly **12** `ZM_WorldTraversal` T0 tests for singleton/state-machine, tag/stream, marker-placement, trigger/sensor/latch, active-scene identity, and scene/entity-generation adversarial contracts.
- Automated registry: **5 tests**. Headless passes `ZM_Boot_Test` and the asset-free `ZM_ControllerHarness_Test`; graphics-required `ZM_WarpInfrastructure_Test`, `ZM_GrassRegeneration_Test`, and `ZM_DawnmerePlayerCamera_Test` skip as designed.
- Windowed `ZM_WarpInfrastructure_Test`: **4 frames / 885.7 ms**, proving direct playerless FrontEnd build-0 request, deferred SINGLE load, persistent manager identity, destination freeze, exact `TownCenter` placement, motion reset, and return to idle.
- Windowed `ZM_GrassRegeneration_Test`: **11 frames / 1927.5 ms**, retaining the exact **200,159 blades / 5,133 terrain triangles** load-reload contract.
- Windowed `ZM_DawnmerePlayerCamera_Test`: **117 frames / 5043.5 ms**, covering real Dawnmere movement, fixed follow, SINGLE reload with generation-safe reacquisition, grass readiness, and FrontEnd teardown.

## Last completed -- persistent traversal infrastructure

- Component orders are `ZM_GameStateManager` **104**, `ZM_SpawnPoint` **105**, and `ZM_WarpTrigger` **106**. FrontEnd authors the manager-only `ZM_GameStateRoot`; the authoritative manager moves that entity to the persistent scene, stores a generation-bearing singleton ID, and retires duplicates.
- Warp acceptance validates the WorldSpec build/tag and requires the unique valid active-scene dynamic-capsule Player, except for the deliberate playerless FrontEnd build-index-0 direct-request path. Acceptance freezes the source immediately; `ZM_PlayerController::OnStart` freezes a replacement while any transition is active. `QUEUED -> WAITING_FOR_SCENE -> WAITING_FOR_SPAWN` updates defer and issue exactly one SINGLE load.
- Spawn tags are 1-31 printable ASCII bytes in a fixed 32-byte buffer. A `ZM_SpawnPoint` transform denotes **feet**: authored `TownCenterSpawn` is **(512, 25.98577, 480)** and the scale-derived Player centre is **(512, 26.88577, 480)**. Resolution teleports the physics body once, zeros linear/angular velocity, resets controller runtime state, enables movement, and returns the manager to idle.
- `ZM_WarpTrigger` reasserts a sensor collider and latches only the unique generation-exact active-scene valid dynamic-capsule Player; foreign/additive-scene, duplicate, malformed, bodyless, and slot-reused identities fail closed. The latch clears only on exit by that exact full entity ID. The v1 component streams serialize scene authoring only; live transition state is deliberately absent and is **not** the S7 `ZM_SaveSchema`.

## Current task and remaining S3 order

1. Author PlayerHome at build index **40** plus live Dawnmere/home `ZM_WarpTrigger` entities.
2. Add fade and a real door round-trip P1, asserting both spawn-tag arrivals and camera/controller recovery.
3. Complete the S3 automated gate and capture visual evidence. Only then write `GATE-WAIT: S3 visual sign-off`, commit/push the evidence docs, and stop for human review.

## Notes for the next agent

- The current P1 calls the manager directly from playerless FrontEnd; no PlayerHome, fade, build-40 scene, or authored live warp trigger exists yet. Do not mistake component/serialization tests for the pending real trigger route.
- Baked terrain/scenes remain ignored; new terrain work keeps the named-set, bounded-export, atomic-marker and fixed registry contracts.
- `m_eGender` must enter the future S7 party/daycare serializer.
- For S5 AI integration, a fainted active still requires the caller to submit the forced switch.
