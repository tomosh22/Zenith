# Undervault in-engine visual studies

Three original, presentation-only 3D cutaways follow the supplied references:

| Selection | Reference | Contents |
|---|---|---|
| Colony (default) | 01-colony-gameplay.png | Bedroom, mess room, algae oxygen vessel, crank generator, cistern, three posed Vaulters |
| Planning | 02-dig-build-planning.png | Two-level colony, ice/forge boundaries, utility network, dig designations, wireframe pump blueprint, oxygen labels |
| Flooding | 03-flooding-emergency.png | Upper living quarters, sealed airlock, generator, broken water main, submerged lower chambers, escape route and posed evacuation |

The scene geometry is rendered by Zenith/Flux. Reference images are never used
as scene textures. Original transparent HUD textures contain only decorative
interface artwork. All statuses, water levels, characters, tools and networks
are staged; GPU water spray and gas haze are presentation effects; there is no gameplay simulation, input, inventory or saving.

## Reproduction

Run these from the existing `foundry-visual-mockups` worktree root:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --python Games/Undervault/SourceArt/generate_mockups.py
& Games/Undervault/SourceArt/generate_huds.ps1
& Build/regen.ps1 -AllowLinkedWorktree
& .\zenith.bat build Undervault
& .\zenith.bat build Undervault --config Vulkan_vs2022_Debug_Win64_False
```

The generator also invokes `generate_mist.py` to author the local GPU billboard.
The generator also accepts `-- Colony`, `-- Planning` or `-- Flooding` to rebuild
one view. It uses fixed seeds and original geometry, bevelled silhouettes,
packed procedural material grain, metres and glTF +Y-up export. Source depth
is reflected at export to match Zenith's +Z-facing camera. Each editable
`SourceArt/Undervault<View>.blend` retains individual objects before joining
one export mesh with separate material sections. No third-party art is needed.
The HUD script uses the repository's Roboto font and System.Drawing.

Bake and launch a selected view:

```powershell
$env:UNDERVAULT_MOCKUP='Colony' # Planning or Flooding
& .\Games\Undervault\Build\output\win64\vulkan_vs2022_debug_win64_true\undervault.exe --skip-unit-tests --automated-test Undervault_MockupBoot_Test --test-results Build/artifacts/undervault/Colony-tools.json
& .\Games\Undervault\Build\output\win64\vulkan_vs2022_debug_win64_false\undervault.exe --skip-unit-tests
```

The first tools boot performs the normal GLB/material/texture import and authors
`Assets/Scenes/<View>.zscen`. Bake each selection once before launching it in a
runtime build. Runtime boots directly into that saved composition.

For a capture, launch the runtime with:

```powershell
--automated-test Undervault_MockupBoot_Test --test-results Build/artifacts/undervault/Colony-runtime.json --window-size 1920x1080 --screenshot Build/artifacts/undervault/Colony.tga --screenshot-frame 240 --fixed-dt 0.016666667
```

Then convert the raw framebuffer without image retouching:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\5.2\python\bin\python.exe' Games/Undervault/SourceArt/capture_to_png.py Build/artifacts/undervault/Colony.tga Games/Undervault/Mockups/InEngine/Colony-1920x1080.png
```

## Generated assets and constraints

Generated GLBs, engine `.zmesh`/`.zmodel`/`.zmtrl`/`.ztxtr` bundles, scene files,
solutions and build products follow the repository's ignored-output policy.
Retain the scripts, editable blends, texture declarations and final captures.
The renderer also needs the existing engine boot assets and compatible Slang
DLLs supplied by this worktree's established Foundry setup. No engine assets
or engine sources are overwritten by Undervault's generators.

The public camera authoring API does not expose orthographic parameters, so a
distant 8-degree perspective lens approximates the fixed orthographic reference.
Underground lighting uses a midnight environment plus authored warm lamps and
cool machine/water lights, fixed exposure and no directional shadows.
Water surfaces and falling-water strands are staged meshes, not a fluid solver.
Flux GPU compute supplies the cistern splash, breach spray and planning-view gas
haze using the game's generated soft billboard. The shared particle texture
binding is set only for Undervault; CPU particles remain disabled. The figures
are posed, not animated. The render test checks the selected model, nonempty
material sections, exact emitter counts (1/1/2), empty CPU particle arrays, and
nonzero GPU-generated indirect draw instances after 301 frames. A graphics
backend is required; these are not headless unit tests. This is a Win64 visual prototype,
not an Android performance or touch-interaction validation.

The six tools/runtime checks passed. Final framebuffer PNGs are under
`Mockups/InEngine/{Colony,Planning,Flooding}-1920x1080.png`; build/test logs and
JSON reports are under `Build/artifacts/undervault/`.

## Intended file set

All new work is under `Games/Undervault`: the `.zproj`, bootstrap, boot/render
smoke test, local instructions, ignore file, this README, reproducible source
art and editable blends, HUD import declarations, and `Mockups/InEngine` PNGs.
The three original reference PNGs and both design documents are preserved.
Previously authored Foundry files, the existing worktree build-system changes,
and the unrelated complexity-report LFS files are not part of this change.
No commits or staging were performed.
