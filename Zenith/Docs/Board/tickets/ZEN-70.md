---
id: "ae7eed4f-07c2-43bc-a39d-90289212c101"
key: "ZEN-70"
type: "STORY"
title: "Ozone absorption layer in the atmosphere so twilight carries the Chappuis band"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 8
position: 70
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:48.287Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"Ozone is absent from the atmosphere (`TODO(ozone)` carries a full plan), which is why twilight lacks the Chappuis band."* `Zenith/Flux/Shaders/Common/Atmosphere.slang` records that the medium is purely scattering (extinction equals scattering) and `MultiScatter.slang` notes what changes once an absorption layer exists.

Add the ozone absorption layer per the plan in those two TODOs: a Chappuis-band absorption coefficient (the standard three-wavelength fit), the ozone density profile, extinction that includes it in both the single-scatter integration and the multi-scatter LUT, so twilight skies turn the expected blue-violet. Calibrate against measured sky radiance at three sun elevations (the program's §2 method), record the fit, and pin the LUT with a golden hash unit plus a photo-tour A/B at a twilight pose. The sun disc radiance and the auto-exposure histogram range (26) must not need re-tuning; if they do, say why.

## Definition of Done
- [ ] The atmosphere carries ozone absorption in single- and multi-scatter with the coefficients and profile recorded in `Docs/design/Photorealism.md` §2
- [ ] A twilight photo-tour pose shows the Chappuis band; noon poses are within the recorded tolerance of before
- [ ] The multi-scatter LUT golden is re-pinned with its reason
- [ ] Every game's pinned baseline bumped from an OBSERVED run
