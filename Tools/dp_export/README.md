# DevilsPlayground UE5 → Zenith asset sweep

Run-once-per-source-update pipeline that exports every UE5 asset in
`C:\dev\GameJam0` to glTF/PNG under
`Games/DevilsPlayground/Assets/`, plus a manifest.json the Zenith importer
consumes.

## Prerequisites

- Unreal Engine 5.6 (matches the GameJam0 project version).
- The GameJam0 project open in the editor.
- Editor Python scripting plugin enabled (Edit → Plugins → "Python Editor Script Plugin").
- `glTF Exporter` plugin enabled (Edit → Plugins → "glTF Exporter") — ships with UE 5.2+.

## Running

1. Open `C:\dev\GameJam0\GameJam0.uproject` in UE 5.6.
2. Wait for asset registry to finish scanning (status bar bottom-right idle).
3. Window → **Output Log**, click the **Cmd** dropdown, switch to **Python**.
4. Paste:
   ```
   py "C:/dev/Zenith/Tools/dp_export/sweep.py"
   ```
5. Watch for `[dp_export] Wrote .../manifest.json (NNN entries)`.

The first run takes 5–15 minutes depending on how many assets are in the
source. Subsequent runs over an unchanged source produce a git-diff-clean
output (the exporter task uses `replace_identical = true`).

## Output

- `Games/DevilsPlayground/Assets/Meshes/*.gltf` + matching `.bin` blobs
- `Games/DevilsPlayground/Assets/Skeletons/*.gltf`
- `Games/DevilsPlayground/Assets/Animations/*.gltf`
- `Games/DevilsPlayground/Assets/Textures/*.png`
- `Games/DevilsPlayground/Assets/Materials/*.json` — parameter snapshots
  (UE materials don't round-trip; the Zenith importer constructs a
  `Zenith_MaterialAsset` from the JSON)
- `Tools/dp_export/manifest.json` — every exported asset with status,
  source path, target Zenith path

## Importing into Zenith

The Zenith side runs as a tools-build code path: ZenithTools project (or a
new dp_import command) walks the manifest, calls
`Zenith_AssetRegistry::Create<...>` for each row, hooks the glTF/PNG via
the existing AssetHandling/ Assimp pipeline, and writes `.zmesh`/`.ztxtr`/
`.zskel`/`.zanim` outputs alongside.

The importer is not yet written — see Wave-4 polish in the port plan.
Until then, the Zenith side has no real meshes and the GameLevel scene
runs with one-of-each placeholder entities authored by
`Project_RegisterEditorAutomationSteps`.

## Re-export discipline

- Always re-run after pulling source changes from `C:\dev\GameJam0`.
- Commit the regenerated manifest.json so the Zenith side stays in sync.
- Asset GUIDs / UE paths are the source of truth; renames in UE require
  a corresponding rename in any Zenith code that references the old path.

## Troubleshooting

- **`ImportError: unreal`**: the script is not running inside the editor.
  Use the Python console, not a system Python.
- **`GLTFExporter` not found**: enable the plugin and restart the editor.
- **Empty material JSON**: only `MaterialInstanceConstant` exposes the
  parameter set cleanly. Master `Material` assets dump as a stub; the
  Zenith importer should map them to a default lit shader.
- **Slow sweep**: the bottleneck is per-asset `get_asset()` deserialization.
  This is unavoidable with the current API. Run it overnight if needed.
