---
id: "867306f3-4484-4fb4-8d87-cf387a708267"
key: "ZEN-46"
type: "STORY"
title: "Shipping graphics tiers: TAA toggle, cascade count, shadow resolution, dynamic-light cap and a device-tier heuristic"
category: "Engine"
parent: "ZEN-26"
sprint: "zen-zen-mobile-readiness-p0-before-a-vertical-slice"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 46
labels: ["engine-gap"]
fixVersions: []
blocks: [{"key":"ZEN-54","reason":"Render scale is the second option the tiers set; the options plumbing lands with the tiers.","raw":{"key":"ZEN-54","reason":"Render scale is the second option the tiers set; the options plumbing lands with the tiers.","raw":{"key":"ZEN-54","reason":"Render scale is the second option the tiers set; the options plumbing lands with the tiers."},"invalid":false},"invalid":false},{"key":"ZEN-68","reason":"Local light shadows are off on the LOW tier, which needs the tier heuristic to exist.","raw":{"key":"ZEN-68","reason":"Local light shadows are off on the LOW tier, which needs the tier heuristic to exist."},"invalid":false},{"key":"ZEN-87","reason":"P0 acceptance runs at a device tier."}]
updatedAt: "2026-09-19T17:42:44.054Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-05 (P0) and Undervault #19 (P2); every mobile game's shipping quality tiers. The TDD row: *"Zenith_GraphicsOptions gains m_bTAAEnabled, m_uShadowCascadeCount, m_uShadowMapResolution, m_uMaxDynamicLights; a small Zenith_DeviceTier boot heuristic games may consult; TAA-off path validated."*

Verified against HEAD: `Zenith/Core/Zenith_GraphicsOptions.h` carries feature toggles (fog, grass, bloom, HiZ, IBL, particles, gizmos) but no TAA toggle, cascade count, shadow resolution or dynamic-light cap, so a low-end device has nothing to turn down except whole features.

**Scope.** Add the four options with the current values as defaults, thread each to its consumer (`Flux/TAA`, `Flux/Shadows` cascade set and map size, `Flux/DynamicLights` cap), add `Zenith_DeviceTier` (LOW/MID/HIGH from a boot heuristic over device limits and memory, overridable from the command line and from ZEN-50's Android config), and validate the TAA-off path (jitter zero, history disabled, the velocity pass skipped) on every game. Units over the option round-trip, the tier heuristic's inputs, and TAA-off producing a stable frame in the TAA unit suite; FD-111 (thermal) and FD-60 (pacing) consume the tier later. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] The four options exist, default to today's values, and reach their consumers; a `Zenith_DeviceTier` heuristic is queryable and overridable
- [ ] TAA-off renders a stable frame with the velocity pass skipped, pinned by the TAA units and a windowed A/B on RenderTest
- [ ] Every game renders identically at the default tier (photo-tour comparison attached)
- [ ] Every game's pinned baseline bumped from an OBSERVED run
