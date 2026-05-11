"""
DevilsPlayground UE5 → Zenith asset PROBE.

Read-only enumeration. Writes results to C:/tmp/dp_probe.log so we can read
them from outside the UE process — bare print() doesn't make it to the
commandlet log under -nullrhi, and unreal.log() writes to a UE log channel
that's hard to filter cleanly.
"""

import sys
import os
import json
from pathlib import Path

LOG_PATH = Path("C:/tmp/dp_probe.log")
LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
_log_file = LOG_PATH.open("w", encoding="utf-8")

def log(msg):
    _log_file.write(str(msg) + "\n")
    _log_file.flush()
    try:
        unreal.log(str(msg))
    except Exception:
        pass

try:
    import unreal
except ImportError:
    log("ERROR: must run inside UE Python")
    sys.exit(1)

# ---------------------------------------------------------------------------
# 1. Asset registry access
# ---------------------------------------------------------------------------
try:
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    log("[probe] AssetRegistry: OK")
except Exception as exc:
    log(f"[probe] AssetRegistry: FAIL - {exc}")
    sys.exit(2)

# Wait for any in-progress asset scan to finish before enumerating.
try:
    registry.wait_for_completion()
    log("[probe] AssetRegistry scan complete")
except Exception:
    pass

# ---------------------------------------------------------------------------
# 2. /Game/ enumeration
# ---------------------------------------------------------------------------
try:
    asset_data_list = registry.get_assets_by_path("/Game", recursive=True)
    log(f"[probe] /Game/ asset count: {len(asset_data_list)}")
except Exception as exc:
    log(f"[probe] /Game/ enumeration: FAIL - {exc}")
    sys.exit(3)

if len(asset_data_list) == 0:
    log("[probe] WARNING: zero assets found")
    sys.exit(4)

# ---------------------------------------------------------------------------
# 3. Per-type counts (no get_asset() calls — that's slow + can fail; the
#    AssetData itself carries the class name in `asset_class_path`).
# ---------------------------------------------------------------------------
type_counts = {}
sample_paths = {}
for ad in asset_data_list:
    # UE 5.1+: asset_class_path is a TopLevelAssetPath. UE 5.0: asset_class.
    type_name = "Unknown"
    try:
        type_name = str(ad.asset_class_path.asset_name)
    except Exception:
        try:
            type_name = str(ad.asset_class)
        except Exception:
            pass

    type_counts[type_name] = type_counts.get(type_name, 0) + 1

    if type_name not in sample_paths:
        try:
            pkg = str(ad.package_name)
            name = str(ad.asset_name)
            sample_paths[type_name] = f"{pkg}.{name}"
        except Exception:
            sample_paths[type_name] = "<unknown>"

log("[probe] Asset type breakdown:")
for k in sorted(type_counts.keys()):
    log(f"    {type_counts[k]:4d}  {k:30s}  e.g. {sample_paths[k]}")

# ---------------------------------------------------------------------------
# 4. Exporter plugin probes
# ---------------------------------------------------------------------------
log("[probe] Exporter plugin availability:")
log(f"    GLTFExporter           {'OK' if hasattr(unreal, 'GLTFExporter') else 'MISSING'}")
log(f"    TextureExporterPNG     {'OK' if hasattr(unreal, 'TextureExporterPNG') else 'MISSING'}")
log(f"    StaticMeshExporterFBX  {'OK' if hasattr(unreal, 'StaticMeshExporterFBX') else 'MISSING'}")
log(f"    AssetExportTask        {'OK' if hasattr(unreal, 'AssetExportTask') else 'MISSING'}")
log(f"    EditorAssetLibrary     {'OK' if hasattr(unreal, 'EditorAssetLibrary') else 'MISSING'}")

# Save a JSON summary alongside.
summary = {
    "asset_count": len(asset_data_list),
    "type_counts": type_counts,
    "sample_paths": sample_paths,
    "exporters": {
        "GLTFExporter":          hasattr(unreal, "GLTFExporter"),
        "TextureExporterPNG":    hasattr(unreal, "TextureExporterPNG"),
        "StaticMeshExporterFBX": hasattr(unreal, "StaticMeshExporterFBX"),
        "AssetExportTask":       hasattr(unreal, "AssetExportTask"),
        "EditorAssetLibrary":    hasattr(unreal, "EditorAssetLibrary"),
    },
}
Path("C:/tmp/dp_probe.json").write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")
log("[probe] Wrote C:/tmp/dp_probe.json")

log("[probe] Done.")
_log_file.close()
