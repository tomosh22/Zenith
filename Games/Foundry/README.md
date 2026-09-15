# Foundry static visual mockups

A static Zenith 3D workshop, built in `codex/foundry-visual-mockups`. No game
component, simulation, inventory, recipes, research logic, input handlers or
gameplay animation loop exists. Machine exhaust uses Flux GPU particles; all displayed status values are decorative.

## Reproduce in this worktree

Run these PowerShell commands from
`C:\dev\Zenith\.claude\worktrees\foundry-visual-mockups`:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Foundry/SourceArt/generate_workshop.py
powershell -NoProfile -ExecutionPolicy Bypass -File Build/regen.ps1 -AllowLinkedWorktree
.\zenith.bat build Foundry
.\Games\Foundry\Build\output\win64\vulkan_vs2022_debug_win64_true\foundry.exe --skip-unit-tests --automated-test Foundry_WorkshopBoot_Test --test-results Build/artifacts/foundry/smoke.json
.\zenith.bat build Foundry --config Vulkan_vs2022_Debug_Win64_False
.\Games\Foundry\Build\output\win64\vulkan_vs2022_debug_win64_false\foundry.exe --skip-unit-tests
```

The first `_True` boot runs the normal Zenith GLB importer and scene authoring.
It displays the tools editor while authoring; the `_False` executable boots
straight into the full-screen static study. `--skip-unit-tests` skips the unrelated
engine unit suite; it does not skip asset exports or the selected Foundry test.
The linked-worktree opt-in preserves the default refusal, verifies the Git/script
root and checks every generated game `ZENITH_ROOT` before reporting success.

This fresh worktree lacked Slang runtime DLLs. Provision the repository-pinned
version before launching either executable if it has not already been installed:

```powershell
Import-Module ./Build/zenith_buildsystem.psm1
$foundryVersion = (Get-ZenithBuildConfigData).SlangVersion
New-Item -ItemType Directory -Force Build/artifacts/foundry | Out-Null
Invoke-WebRequest "https://github.com/shader-slang/slang/releases/download/v$foundryVersion/slang-$foundryVersion-windows-x86_64.zip" -OutFile Build/artifacts/foundry/slang.zip
Expand-Archive Build/artifacts/foundry/slang.zip Build/artifacts/foundry/slang -Force
# Supply the DLLs to the supported build's normal copy/heal location.
Get-ChildItem Build/artifacts/foundry/slang -Recurse -Filter '*.dll' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination Middleware/slang/bin
}
```

Build again after provisioning DLLs. No models or textures are downloaded.

## Editable art and generated outputs

`SourceArt/FoundryWorkshop.blend` is the editable Blender scene. Hidden originals
form the reusable kit; the visible joined composition is the engine export.
`SourceArt/generate_workshop.py` rebuilds it using a fixed random seed, metres,
triangulated bevelled meshes, PBR factors, glTF +Y up and no animations.
It exports 20 reusable models plus the complete `Workshop.glb` under
`Assets/Workshop/`: terrain, shore, ore, conveyors, three belt loads, drill,
inserter, furnace, two filled bins, pole, domed research building, three rock
variants, pine, bush and grass. The composition includes three drills, curved
conveyors, detailed furnace doors, a depot, sagging wires and a rocky shoreline.
`SourceArt/MeadowAlbedo.png` is generated terrain colour variation.
`SourceArt/generate_ui.py` generates nine original transparent HUD glyphs; the
workshop generator invokes it automatically. `Assets/Textures/TextureUsage.ztexdecl`
imports these as uncompressed UI textures through the normal tools pipeline.

The engine imports these into `.zmesh`, `.zmodel`, `.zmtrl` and `.ztxtr` files.
Source GLBs and generated bundles are ignored under the repository asset policy;
the script and editable blend are the retained source. `Assets/Scenes/Main.zscen`
is boot-authored and ignored locally, matching the template's fresh-bake policy.

On a fresh worktree the script also supplies absent ignored engine boot defaults:
six neutral cubemap PNGs, an unused particle fallback, and the repository's
Cousine font at the default font-source path. Existing files are never overwritten.
Zenith's standard tools bake creates their textures/font atlas. The font filename
is a compatibility path; its bytes are Cousine, not Liberation Mono. No engine
source or tracked engine assets were changed.

## Capture and validation

```powershell
.\Games\Foundry\Build\output\win64\vulkan_vs2022_debug_win64_false\foundry.exe --skip-unit-tests --automated-test Foundry_WorkshopBoot_Test --test-results Build/artifacts/foundry/runtime-smoke.json --window-size 1920x1080 --screenshot Build/artifacts/foundry/workshop.tga --screenshot-frame 90
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Foundry/SourceArt/capture_to_png.py
```

Zenith writes uncompressed BGRA TGA. The converter only changes the container to
PNG, preserving pixels and orientation; it adds no imagery or retouching.
Final image: `Mockups/InEngine/Workshop-1920x1080.png`.
Build, import, smoke-test and capture logs live in ignored `Build/artifacts/foundry`.
Both Win64 Vulkan debug configurations build. The Foundry smoke test verifies a
static model, nonempty draw sections and resolved materials over 121 frames.
Build-system verification: 54 passed, 0 failed. The final import/render logs have
no missing Foundry assets or Vulkan validation errors. Driver discovery can emit
an informational AMD layer message. Engine shutdown reports pre-existing registry
allocations; no engine memory-management changes were made for this mockup.

## Visual decisions and limits

The workshop reference guided mustard machinery, an earthy grid, ore-to-smelter
composition, warm furnace lighting, water, tree framing and the large palette.
The capture was reviewed and iterated for camera distance, drill orientation,
belt continuity, panel contrast, icon count and labels. All three Foundry references now have separate static scenes.

The refinement adds denser vegetation, organic ore seams, bevels, vents, hydraulic
braces, furnace fittings, rounded conveyor paths, a faceted glass dome and eight
illustrated construction cards. Belt joins use three explicit transfer decks: incoming
strips are clipped at each opening, guard rails stop at the ports, and cargo is
kept clear of the junction throat. The left copper approach shares the same axis
as its junction, avoiding overlapping curved rails. The kit remains a single static engine model
with 31 material sections. Water and vegetation remain static; machine exhaust is simulated by Flux GPU particles.
The HUD uses image glyphs to remain below the existing 128-root-element limit.
Construction cards are 190 x 140 authored pixels. Alerts and research values are
decorative. Layout is fixed at 16:9; Android deployment and adaptive DPI behavior
were not validated. No interactive controls are implied.

## Intended files and preserved state

New Foundry files: `.gitignore`, `CLAUDE.md`, `Foundry.zproj`, `Foundry.cpp`,
`Tests/Foundry_Boot.cpp`, `SourceArt/generate_workshop.py`,
`SourceArt/capture_to_png.py`, `SourceArt/generate_ui.py`, `SourceArt/MeadowAlbedo.png`,
`Assets/Textures/TextureUsage.ztexdecl`, `SourceArt/FoundryWorkshop.blend`, this README,
and `Mockups/InEngine/Workshop-1920x1080.png`.
Build changes: `Build/regen.ps1`, `Build/zenith_buildsystem.psm1`,
`Build/Tests/run_buildsystem_tests.ps1`, plus the linked-worktree notes in
`Docs/BuildSystem.md` and `Docs/GameProjects.md`.

All nine pre-existing untracked reference PNGs under Foundry, Hearth and Undervault
remain separate and unchanged. Initial normal status failed on LFS cache permissions;
filter-disabled status showed only those three reference directories. With LFS cache
access, final normal status shows the expected 12 pre-existing modified files under
`Zenith/complexity_report/`: comment_ratio, complexity_distribution,
complexity_per_function, dashboard_summary, directory_comparison,
file_size_distribution, files_requiring_attention, halstead_metrics, loc_breakdown,
maintainability_heatmap, nesting_depth and risk_quadrant (all `.png`).
None was edited, restored, staged or normalized. Nothing was committed.





## Additional reference views

The approved Workshop remains the default. `FOUNDRY_MOCKUP` selects a view once
at process startup: `Logistics` or `MapDefense`. Unknown or absent values select
Workshop. Each scene has its own saved `.zscen`, model, editable Blender file and
HUD. Selection introduces no gameplay, scene switching controls or simulation.

Generate both extra scenes from the reusable kit in the approved workshop blend:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Foundry/SourceArt/generate_expanded_mockups.py
.\zenith.bat build Foundry
.\zenith.bat build Foundry --config Vulkan_vs2022_Debug_Win64_False
```

The generator also invokes `generate_expanded_ui.py` for the original circuit,
throughput chart and minimap textures. It does not overwrite the workshop blend,
workshop GLB, or any reference image. Retained editable source files are
`SourceArt/FoundryLogistics.blend` and `SourceArt/FoundryMapDefense.blend`.

Bake, test and capture either view by replacing `Logistics` below with `MapDefense`:

```powershell
$env:FOUNDRY_MOCKUP='Logistics'
.\Games\Foundry\Build\output\win64\vulkan_vs2022_debug_win64_true\foundry.exe --skip-unit-tests --automated-test Foundry_WorkshopBoot_Test --test-results Build/artifacts/foundry/selected-tools.json
.\Games\Foundry\Build\output\win64\vulkan_vs2022_debug_win64_false\foundry.exe --skip-unit-tests --automated-test Foundry_WorkshopBoot_Test --test-results Build/artifacts/foundry/selected-runtime.json --window-size 1920x1080 --screenshot Build/artifacts/foundry/selected.tga --screenshot-frame 90
& 'C:\Program Files\Blender Foundation\Blender 5.2\5.2\python\bin\python.exe' Games/Foundry/SourceArt/capture_to_png.py Build/artifacts/foundry/selected.tga Games/Foundry/Mockups/InEngine/Logistics-1920x1080.png
Remove-Item Env:FOUNDRY_MOCKUP
```

Use the corresponding output PNG name when capturing MapDefense. Without the
smoke-test and screenshot flags, the runtime stays open for inspection. The same
boot smoke test validates the specifically selected model, its static mesh and
resolved material sections. The tools boot must run once per view after export.

`Mockups/InEngine/Logistics-1920x1080.png` focuses on assembly, highlighted belts,
pipework, rail freight, station storage and an electronic-circuit recipe panel.
`Mockups/InEngine/MapDefense-1920x1080.png` shows a factory perimeter, solar arrays,
storage and tanks, turrets, static attackers, radar markings and a strategic HUD.
The phone casing and hands in reference 02 are presentation framing and are not
part of the engine scene. All recipes, charts, minimap, alerts, radar sweep and
muzzle flashes are decorative static artwork; no device deployment is implied.



## Reference fidelity pass

Rebuild all three models, then their shared HUD layers, in this order:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Foundry/SourceArt/generate_workshop.py
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Foundry/SourceArt/generate_expanded_mockups.py
.\Games\Foundry\SourceArt\generate_huds.ps1
```

The PowerShell art source uses System.Drawing and the repository's Roboto-Medium
font to generate original transparent 1920 x 1080 HUD PNGs. They contain labels,
panels, icons and decorative status diagrams only. Every terrain feature, machine,
rail, belt, tree and enemy in the captures is rendered as 3D geometry by Zenith.
The HUD textures are imported normally as uncompressed colour textures and
presented by a single native UIImage; no engine-wide font changes are required.

The fidelity pass strengthens drill stabilizers and furnace casing details,
warms the meadow palette, clusters the forests, adds process equipment and
pipework, and replaces the defense map's large rectangular slab with textured
industrial soil. The defense camera is wider, showing the southern river and
surrounding resource fields. The corrected workshop transfer decks remain intact;
the removed gantry has not been reintroduced. All three environments remain static apart from the requested GPU exhaust.


## GPU machine exhaust

Machine exhaust is the user-requested exception to the static presentation:
three workshop furnace stacks, the logistics smelter and three process-column
vents, and nine defense-map smelters. Emitters are anchored at the generated
stack/vent openings and serialized in their respective scenes.

`Foundry_GPU_Smoke` and `Foundry_GPU_ProcessSteam` set `m_bUseGPUCompute=true`.
CPU particle rendering remains disabled. Flux performs integration and builds
the particle instances in compute, then renders its alpha-blended indirect draw.
The normal emitter component only schedules spawns; it holds no CPU particles.
The maximum reservation is 1,152 of Flux's 4,096 GPU particle slots.

`SourceArt/generate_smoke.py` authors a seeded soft alpha billboard. The workshop
generator invokes it, or it can be run independently with Blender before baking.
Flux currently has one shared particle billboard binding, so Foundry binds this
local texture at resource/scene initialization; no engine default is overwritten.

Use `--automated-test Foundry_GPUSmoke_Test --fixed-dt 0.016666667` for verification.
The test checks the selected model, exact emitter count, live GPU registrations,
empty CPU arrays and CPU instance buffers, disabled CPU particles, and a nonzero
GPU-computed indirect instance count (one readback at verification only).
It runs for 301 frames and requires a graphics backend. Capture smoke with
`--screenshot-frame 240`, allowing four seconds of warmup. All six tools/runtime
runs passed; the three PNG captures were refreshed from this GPU particle output.
