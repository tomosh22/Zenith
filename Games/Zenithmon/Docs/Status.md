# Zenithmon Status

**Last updated:** 2026-07-16
**Stage:** S4 (Asset generators) -- **GATE-WAIT: S4 gallery visual sign-off.** ALL S4 code is complete (every generator family + `ZM_BakeManifest`); the automated gate items are all green; the full-family visual gallery is captured. **The loop is PARKED on the user's visual sign-off (StartPrompts prompt 4).** S1/S2/S3 COMPLETE.
**Build:** GREEN -- `ZM_BakeManifest` re-ran the full 5-config matrix; the new gallery test builds Vulkan Debug True + Debug False (non-tools link proof).
**Tests:** boot unit gate **1908 ran / 1907 passed / 0 failed / 1 skipped**; `zenith test Zenithmon --headless` **7 passed / 0 failed** (the new `ZM_AssetGallery_Test` skips headless -- graphics-required). `.github/workflows/zm-tests.yml` baseline = **1908**.

## GATE-WAIT: S4 gallery visual sign-off

**Every S4 automated gate item is GREEN:**
- 4-config Vulkan matrix (Debug/Release x True/False) + `D3D12_vs2022_Debug_Win64_False` null-backend link proof -- all build (re-run at `ZM_BakeManifest`; gallery re-confirmed Debug True + False).
- Boot unit gate **1908 / 0 failed** (creature/creature-anim/human/building/prop generator `ZM_Gen` units + the 6 tools bake smokes + the 3 `ZM_BakeManifest` units).
- `zenith test --headless` **7/0** (graphics tests + the gallery skip headless/CI).
- Per-family determinism / structural-validity / static-or-skeletal-contract units + tools bake smokes (files land on disk; static `.zmodel` has no rig; creature `.zmodel` self-lists its clips) + the `ZM_BakeManifest` byte-identical re-bake invariant.

**Visual evidence (windowed `ZM_AssetGallery_Test`, git-ignored, NOT committed):**
- `Build/artifacts/zenithmon/s4/gallery/gallery_01.tga` (front) + `.png`
- `Build/artifacts/zenithmon/s4/gallery/gallery_02.tga` (left) + `.png`
- `Build/artifacts/zenithmon/s4/gallery/gallery_03.tga` (right) + `.png`

The gallery is a single additive windowed scene (reflective floor, key+fill lights, dark skybox) showing **26 representatives across all four families**: 8 creatures (one per archetype incl. a shiny), 6 humans (PlayerM/Aster/Vesper/Fenna/Elara/Caretaker), 6 buildings (PlayerHome/Lab/GymGrass/GymFire/CareCenter/TownHall), 6 props (LampPost/SignPost/FenceWood/RockLarge/Barrel/DressingMeadow), each baked via the now-guarded `ZM_BakeAllAssets()` and instanced from disk `.zmodel`s. Test PASSED (26 models loaded + all 3 TGAs dumped + asserted on disk). The first capture was REJECTED by the user for **buildings intersecting each other**; root-caused to a height-only building scale (a fixed-pitch grid + `targetHeight/naturalHeight` blew wide 1-storey footprints past the column pitch) and FIXED via a width-budget `AGFitScale` + widened pitch (ZM-D-087, documented in-code + a reference memory). Re-captured: buildings now cleanly separated, all four families legible.

**Verdict: PENDING (re-review after the ZM-D-087 building-overlap fix).** The user reviews the captures and approves/rejects via StartPrompts prompt 4. If APPROVED: tick the S4 gate + `ZM_AssetGallery` gallery item, clear this marker, S4 COMPLETE -> S5. If REJECTED: rework the flagged assets/framing on master.

## Last completed

**S4 gate building-overlap fix (ZM-D-087).** First gallery capture was REJECTED (buildings intersecting). Root cause: `BuildAGBuilding` scaled by HEIGHT ONLY, so wide 1-storey buildings (CareCenter 10x8x1 -> scale 2.17 -> ~21u wide) exceeded the 12u pitch. Fix: reusable `AGFitScale(w,h,widthBudget,heightCap)=min` -> fit both a per-column width budget AND the height cap; building pitch 12->14, width budgets 11/6.5, applied to buildings+props (creatures/humans stay height-normalized, being ~isotropic). Documented in-code + a persistent reference memory. Rebuilt Debug True; re-captured (buildings cleanly separated, verified front+right). Prior: **S4 full-family gallery captured (S4 gate prep, ZM-D-086).** New `Tests/ZM_AutoTests_AssetGallery.cpp` -- `ZM_AssetGallery_Test` (windowed, `#ifdef ZENITH_INPUT_SIMULATOR`, `m_bRequiresGraphics`, tools-gated `ZM_BakeAllAssets()` bake). Copies the proven creature-gallery scaffolding; adds human/building/prop family builders + the 26-entry roster across 4 Z-rows. `ZM_BakeAllAssets()` cold-baked all four families (all 4 `game:<Family>/.manifest` stamps now written -> warm re-runs are fast). No version bumps, no generator change, no boot wiring (the bake is called ONLY from the gallery Setup). CI baseline unchanged (automated + graphics-required -> skips headless/CI, not in the boot unit gate).

## Notes for next agent

- **THIS IS A HARD-STOP VISUAL GATE.** Do NOT tick the S4 gate, start S5, or bake-wire the boot until the user signs off (prompt 4). If REJECTED, the rework is in `ZM_AutoTests_AssetGallery.cpp` (framing/roster) and/or the generators; re-capture + refresh this marker.
- **Regenerating the evidence:** the bake is warm (4 `.manifest` stamps present), so `pwsh -NoProfile -File Tools\zenith.ps1 test Zenithmon --filter ZM_AssetGallery_Test` (WINDOWED, not `--headless`) re-renders + re-dumps the 3 TGAs in seconds. Convert TGA->PNG via PIL (`python -c "from PIL import Image; Image.open('gallery_01.tga').convert('RGB').save('gallery_01.png')"`). To force a full cold re-bake, delete `Games/Zenithmon/Assets/{Creatures,Humans,Buildings,Props}/.manifest` (or bump a generator version). NEVER commit `Assets/` or `Build/artifacts/`.
- **After sign-off -> S4 COMPLETE.** S5 (Battle integration slice) is next on the critical path: battle scene (index 1, world offset (0,-2000,0), dome+platforms) + `ZM_EncounterZone`/`ZM_TallGrassSystem` + engine E5 grass reset + `ZM_BattleDirector` + `ZM_UI_BattleHUD` + engine E3 typewriter + catch/exp/faint/whiteout to GameState. See Roadmap S5 + MasterPlan.
- **All S4 generators shipped (versions):** creature v3 (152, skinned+animated), human v1 (34, shared rig+9 clips), building v1 (30, static), prop v1 (25, static). Four `ZM_GenCommon` bake bridges; `ZM_BakeManifest` gates re-bakes per family. `ZM_BakeAllAssets()` (tools) bakes all four, guarded -- wire it into any future asset-referencing scene's warm path.
- **Gate ORDER + baseline (unchanged):** `zenith test Zenithmon --headless` FIRST (heals DLLs, else `0xC0000135`); boot unit gate hangs after the units line -> `run_unit_gate.ps1 -Baseline N -TimeoutSec 300` times out+kills but captures `Unit tests complete: N ran`. New files need `pwsh -NoProfile -File Build\regen.ps1`. Non-default configs via msbuild ABSOLUTE sln path (`C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe`), serial only. `git -C C:/dev/Zenith ...`. Run WINDOWED tests from the repo root (Bash cwd persists between calls).
- **Working model:** MASTER-ONLY (ZM-D-031); local gate is the authority; `zm-tests` is the post-push backstop. Sweep stray `zenithmon.exe` before ending. Orchestrator+subagents this session (Plan -> Implement -> Reviewer per box; Doc Maintainer; the orchestrator does interactive visual/camera tuning directly). A stray harness worktree exists at `.claude/worktrees/` -- NEVER regen/build/commit from it (Invariant 2). Never commit baked assets or captures.
