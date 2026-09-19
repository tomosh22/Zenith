---
id: "aa50a690-373e-47c0-a98d-a40acc41062a"
key: "ZEN-64"
type: "STORY"
title: "Localization v1: string-table asset, keyed lookup, MSDF font fallback chain, locale detection"
category: "Engine"
parent: "ZEN-26"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 64
labels: ["engine-gap"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:25.614Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
**Register:** Hearth HE-ENG-20 (P3); also DevilsPlayground's Strings.json and Zenithmon's dialogue. The TDD row: *"Localization v1: string-table asset + text lookup by key, MSDF font fallback chain + extended-Latin atlas; locale from AConfiguration. CJK out of v1."*

No game routes text through a table today; every string is a literal. A string-table asset with a font fallback chain is engine surface all three mobile games and DevilsPlayground's deferred localisation need once.

**Scope.** A `.zstrings` table asset (key to UTF-8 per locale) baked by the tools boot from a JSON source, `Zenith_Text(key)` lookup with a recorded missing-key fallback, the MSDF atlas extended to Latin Extended-A/B with a per-font fallback chain, and locale from `AConfiguration` on Android and the user locale on Windows, overridable by command line; units over the codec, the fallback, and the atlas coverage of the extended set; one game routes one screen through it as the proof. Engine work under `Zenith/**`: the Engine gates build every `.zproj` game, the cross-game sweep applies, and a backend-neutral unit moves every game's pin. `Null_` must keep working (the headless suite is where the units run); the Android half is proven on the x86_64 emulator where a device is not available (`Vulkan_x86_64_vs2022_Debug_Agde_False`).

## Definition of Done
- [ ] A string-table asset, lookup and font fallback chain exist with units over codec, missing keys and atlas coverage
- [ ] Locale is read on both platforms and overridable; one game's screen renders from the table
- [ ] `Zenith/UI/CLAUDE.md` documents the pipeline
- [ ] Every game's pinned baseline bumped from an OBSERVED run
