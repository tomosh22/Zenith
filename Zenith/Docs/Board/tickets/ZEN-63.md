---
id: "a23f02a9-175e-43f8-8c77-bbbc9e684d8d"
key: "ZEN-63"
type: "STORY"
title: "JNI seam and Play Billing with CRC'd local entitlements: the first Java in the repo"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: 13
position: 63
labels: ["engine-gap","deferred","question"]
fixVersions: []
blocks: [{"key":"HE-46","reason":"Billing needs the JNI seam"}]
updatedAt: "2026-09-19T19:29:43.418Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-19 (P3); cosmetic-only IAP, fully offline entitlements. The TDD row: *"JNI seam + Play Billing: first Java in the repo: hasCode=true, ZenithActivity extends NativeActivity, one Zenith_Jni bridge (thread attach, class cache), Billing v7 client (query/purchase/acknowledge/restore), CRC'd local entitlements."*

Every Android build today is a bare `NativeActivity` with no Java; Billing, and later IME (FD-135), need one JNI bridge. `deferred`: Hearth is the only consumer and its P3 is post-slice; `question`: the first Java in the repo and the store account are a human decision.

**Scope.** The JNI seam first (a `Zenith_Jni` bridge with thread attach and class cache, testable with a stub class on the emulator), then the Billing client behind an engine interface with a Windows stub, and a CRC'd local entitlement file read at boot; units over the entitlement codec and the interface's state machine; the store-side product setup is a human step recorded in `Docs/BuildSystem.md`. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A JNI bridge exists and is exercised on the emulator; the Billing interface has a Windows stub and a state-machine unit
- [ ] Local entitlements round-trip with CRC and fail closed on corruption
- [ ] A `ZEN-D-` style decision (or the Engine decisionLog once ZEN-82 lands) records the first-Java choice
- [ ] Every game's pinned baseline bumped from an OBSERVED run
