---
id: "d26ad212-c550-42a3-ac39-2b98962a9e1a"
key: "ZEN-26"
type: "EPIC"
title: "Mobile platform readiness: the shared engine register for Hearth, Undervault and Foundry"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "COMPLEX"
risk: "HIGH"
storyPoints: null
position: 26
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:32:33.409Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
The engine work three designed games need before they can ship on Android, filed ONCE. `Games/Hearth/Docs/TDD.md` §3.2 registers 21 work packages (HE-ENG-01..21), `Games/Undervault/Docs/TechnicalDesignDocument.md` §2 registers 20 gaps, and Foundry's TDD §19 registers 16, and the three lists overlap heavily. Foundry's engine items are ALREADY tickets on the FD board under `--category Engine`: FD-56 (UI DPI scaling), FD-57 (safe-area insets), FD-58 (lifecycle pause hook), FD-60 (frame pacing), FD-61 (ScrollView), FD-76 (ASTC export), FD-77 (SaveAsync), FD-79 and FD-121 (audio, two phases), FD-80 (dynamic texture), FD-110 (instancing upload), FD-111 (thermal), FD-115 (big.LITTLE), FD-123 (haptics), FD-125 (packaging polish), FD-135 (IME). **Those are not duplicated here**; this epic holds everything the Hearth and Undervault registers add that Foundry's does not, so one implementation serves all three games.

Phases follow Hearth's: P0 (before a vertical slice can exist: ortho camera, gestures, priority queue, quad budget, graphics tiers, surface recreation, tint, cutaway, config plumbing, device gate), P1 (before an Android ship: 9-slice, canvas aspect mode, render scale, animation events, skinned headroom, save metadata, text field, AAB packaging, Android CI, slim instances), P2 and post-1.0 (VAT hardening, Billing, localization, morph targets), each its own PLANNED sprint. Every item is engine work under `Zenith/**`, so the Engine gates build every game and every game's pin moves together.

## Definition of Done
- [ ] Every P0 and P1 child Done; P2 and post-1.0 children Done or carried with a recorded reason
- [ ] Each landed item is struck from the Hearth and Undervault TDD registers with its ZEN key, in the same commit
