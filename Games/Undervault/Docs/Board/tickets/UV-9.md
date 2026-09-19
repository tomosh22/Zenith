---
id: "9959977d-3534-4a95-b76e-2bb6c875f358"
key: "UV-9"
type: "TASK"
title: "Device-reality spike harness: UV_DeviceSpike_Test with 24,576 instanced boxes, a stub cell SSBO pass and per-frame telemetry"
category: "Undervault"
parent: "UV-1"
sprint: "uv-wv005sc30zutg2g8hpg9tfd8"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 9
labels: ["needs-gpu","gpu-reason:visual-measure"]
fixVersions: []
blocks: [{"key":"UV-10","reason":"the device run executes the harness"}]
updatedAt: "2026-09-19T19:12:08.268Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
TDD §20 Phase 0 ①: *"Device-reality spike: scaffold, kill-list applied, 24.6k instanced boxes + a stub cell SSBO pass on Tier A + Tier B phones -> measure frame/memory (replaces §15 estimates)."* Build the harness as an automated test, `UV_DeviceSpike_Test` (`requiresGraphics`): the §12.2 kill-list active, one `Flux_InstanceGroup` per tile material holding 24,576 unit boxes on a 128x192 grid (§12.3), a stub `UV_CellRenderFeature` registered after HDR uploading a 12-byte-per-cell SSBO for all chunks every frame (§12.4 record layout), 16 skinned StickFigure placeholders, and a `UV_Telemetry` ring that dumps per-frame CPU (record, upload), GPU (G-buffer, lighting, cells) and RSS to `Build/artifacts/undervault/spike/*.json`. Desktop numbers recorded in `Docs/Status.md` beside the §15 budget table; the device run is UV-10. `needs-gpu`.

## Definition of Done
- [ ] `UV_DeviceSpike_Test` renders 24,576 instanced boxes plus the stub cell pass with the kill-list applied and dumps per-frame timings over 600 frames, windowed
- [ ] The AGDE build of the same test produces an APK the emulator runs (timings diagnostic only, §15.1)
- [ ] Desktop numbers are in `Docs/Status.md` beside the §15 table
- [ ] Undervault pinned baseline bumped from an OBSERVED Null run (the row is added by UV-6; until then this line is inert)
