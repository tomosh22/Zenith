"""Probe the unreal.GLTFExporter API surface in UE 5.6 to find the right export method."""
import sys
from pathlib import Path

LOG = Path("C:/tmp/dp_gltf_probe.log").open("w", encoding="utf-8")
def log(s):
    LOG.write(str(s) + "\n"); LOG.flush()
    try: unreal.log(str(s))
    except Exception: pass

import unreal

log("=== unreal.GLTFExporter attributes ===")
for name in sorted(dir(unreal.GLTFExporter)):
    if name.startswith("__"): continue
    log(f"  {name}")

log("=== unreal.GLTFExportOptions attributes ===")
try:
    for name in sorted(dir(unreal.GLTFExportOptions)):
        if name.startswith("__"): continue
        log(f"  {name}")
except Exception as e:
    log(f"  GLTFExportOptions not accessible: {e}")

log("=== unreal.Exporter attributes (for run_asset_export_task etc.) ===")
for name in sorted(dir(unreal.Exporter)):
    if name.startswith("__"): continue
    log(f"  {name}")

# Try a single mesh with multiple invocation styles to find one that works.
log("=== Trying StaticMeshExporterFBX path ===")
try:
    sm = unreal.EditorAssetLibrary.load_asset("/Game/DevilsPlayground/Assets/Blockout/SM_Plane.SM_Plane")
    if sm is None:
        log("  load failed")
    else:
        log(f"  loaded {sm.get_path_name()}, type {type(sm).__name__}")

        # Style A: AssetExportTask without setting exporter (let UE infer from extension)
        task = unreal.AssetExportTask()
        task.set_editor_property("object", sm)
        task.set_editor_property("filename", "C:/tmp/_probe_export_A.fbx")
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        result_a = unreal.Exporter.run_asset_export_task(task)
        log(f"  Style A (auto-infer .fbx): result={result_a}, file_exists={Path('C:/tmp/_probe_export_A.fbx').exists()}")

        # Style B: AssetExportTask without setting exporter, .gltf extension
        task = unreal.AssetExportTask()
        task.set_editor_property("object", sm)
        task.set_editor_property("filename", "C:/tmp/_probe_export_B.gltf")
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        result_b = unreal.Exporter.run_asset_export_task(task)
        log(f"  Style B (auto-infer .gltf): result={result_b}, file_exists={Path('C:/tmp/_probe_export_B.gltf').exists()}")

        # Style C: explicitly named glb
        task = unreal.AssetExportTask()
        task.set_editor_property("object", sm)
        task.set_editor_property("filename", "C:/tmp/_probe_export_C.glb")
        task.set_editor_property("automated", True)
        task.set_editor_property("prompt", False)
        task.set_editor_property("replace_identical", True)
        result_c = unreal.Exporter.run_asset_export_task(task)
        log(f"  Style C (auto-infer .glb): result={result_c}, file_exists={Path('C:/tmp/_probe_export_C.glb').exists()}")
except Exception as e:
    log(f"  Probe export failed: {e}")

LOG.close()
