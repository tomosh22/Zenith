#!/usr/bin/env python3
"""
DevilsPlayground → Zenith level-data generator.

Reads Games/DevilsPlayground/Assets/Maps/DevilsPlayground_L_GameLevel.json
(produced by Tools/dp_export/sweep.py) and emits
Games/DevilsPlayground/Source/DP_LevelData.h with structured arrays of
{position, rotation, mesh, light} for each behaviour category.
Project_RegisterEditorAutomationSteps consumes these arrays to author the
real GameLevel scene with the right number of villagers / item spawners /
doors / etc. at the correct UE positions, with mesh and light wiring.

Coordinate conversion:
  UE     X-forward, Y-right, Z-up,   units = cm   (left-handed)
  Zenith Z-forward, X-right, Y-up,   units = m    (right-handed)

We keep BOTH positions and scales on the mesh-local axis mapping. The
glTF mesh export preserves UE.X as glTF.X (a wall's UE-X length axis
stays a glTF-X length axis), so any positional or scaling axis that
"means UE.X" in source data must land on Zenith.X to match what the
imported mesh expects:

  Zenith.x = UE.x / 100   (UE.X → Zenith.X, not Zenith.Z)
  Zenith.y = UE.z / 100   (height stays height)
  Zenith.z = UE.y / 100   (UE.Y → Zenith.Z)

  sx = scale[0]           (UE.X scale → Zenith.X — same axis as positions)
  sy = scale[2]           (UE.Z height scale → Zenith.Y)
  sz = scale[1]           (UE.Y scale → Zenith.Z)

This makes Zenith.X the world axis carrying everything that was UE.X in
source data: positions, scale-along-length, and the wall mesh's natural
long axis (so a corner-wall-corner triplet at the same UE.Y but
varying UE.X lays out as a contiguous wall along Zenith.X with the
intended scale[0] stretching the length).

The visual layout in Zenith is therefore *rotated* 90° from a UE editor
top-down view (UE.X is "north on screen" in UE; Zenith.X is "right on
screen" with the default top-down camera orientation). The DPOrbitCamera
default yaw is set to +π/2 so the camera spawns west of centre looking
east — that rotates the rendered top-down view back to match the UE
editor's "UE.X up on screen" orientation. Game-side concern, not a
coordinate-conversion concern.

UE rotations are (roll, pitch, yaw) in degrees. We emit just the yaw
(Y-axis in Zenith) — pitch/roll on placed actors is uncommon for top-down
games and not load-bearing for the skeleton-grade port. Full quaternion
conversion is a follow-up.

Yaw sign is negated downstream in C++ (AuthorPlacementBatch) to convert
left-handed UE yaw to right-handed Zenith Y-rotation. We emit raw degrees
here; the C++ side owns the sign flip so it stays paired with the actual
SetTransformYaw call.

Mesh path conversion:
  UE   /Game/DevilsPlayground/Assets/Blockout/Chest/ChestMain.ChestMain
=> stem DevilsPlayground_Assets_Blockout_Chest_ChestMain
=> file Games/DevilsPlayground/Assets/Meshes/<stem>.zmodel
"""

import json
import sys
from pathlib import Path

ZENITH_ROOT = Path(__file__).resolve().parents[2]
MAP_JSON    = ZENITH_ROOT / "Games" / "DevilsPlayground" / "Assets" / "Maps" / "DevilsPlayground_L_GameLevel.json"
OUT_HEADER  = ZENITH_ROOT / "Games" / "DevilsPlayground" / "Source" / "DP_LevelData.h"

# ---------------------------------------------------------------------------
# BP class path → behaviour-type-name mapping
# ---------------------------------------------------------------------------
BP_TO_BEHAVIOUR = {
    "/Game/DevilsPlayground/AI/BP_PriestPawn.BP_PriestPawn_C":                "Priest",
    "/Game/DevilsPlayground/Items/BP_ItemSpawn.BP_ItemSpawn_C":               "ItemSpawn",
    "/Game/DevilsPlayground/BPs/WorldGameObjects/BP_Door.BP_Door_C":          "Door",
    "/Game/DevilsPlayground/BPs/WorldGameObjects/BP_ChestInteractable.BP_ChestInteractable_C": "Chest",
    "/Game/DevilsPlayground/Assets/Blockout/BP_Light.BP_Light_C":             "Light",
    "/Game/DevilsPlayground/BPs/Characters/BP_Blacksmith.BP_Blacksmith_C":    "Villager",
    "/Game/DevilsPlayground/BPs/Characters/BP_Pleb.BP_Pleb_C":                "Villager",
    "/Game/DevilsPlayground/BPs/Characters/BP_Seer.BP_Seer_C":                "Villager",
    "/Game/DevilsPlayground/BPs/WorldGameObjects/DecoGroups/BP_MushroomGroup.BP_MushroomGroup_C": "MushroomGroup",
    "/Game/DevilsPlayground/BPs/BP_FogManager.BP_FogManager_C":               "FogManager",
}

# Generic class names (no bp_class field) handled here.
NATIVE_CLASS_TO_CATEGORY = {
    "DPVillager":       "Villager",
    "ItemSpawn":        "ItemSpawn",
    "DirectionalLight": "DirectionalLight",
    "PointLight":       "PointLight",
    "SkyLight":         "SkyLight",
    "PlayerStart":      "PlayerStart",
    "NiagaraActor":     "Niagara",
    "CameraActor":      "CameraStart",
    "StaticMeshActor":  "StaticDeco",
}

# Categories we generate arrays for. Anything not in here is logged and dropped.
EMITTED_CATEGORIES = {
    "Villager", "ItemSpawn", "Door", "Chest", "Light", "Priest",
    "MushroomGroup", "PlayerStart", "Niagara",
    "DirectionalLight", "PointLight",
    "StaticDeco",
}

def categorise(actor: dict) -> str | None:
    bp = actor.get("bp_class", "")
    if bp and bp in BP_TO_BEHAVIOUR:
        return BP_TO_BEHAVIOUR[bp]
    cls = actor.get("class", "")
    return NATIVE_CLASS_TO_CATEGORY.get(cls)

def ue_to_zenith(loc: list[float], rot: list[float]) -> tuple[tuple[float, float, float], float]:
    """UE (X-forward cm, Z-up) → Zenith (Z-forward m, Y-up). Returns (pos, yaw_radians).

    Position swap matches the mesh-import axis convention (UE.X → Zenith.X)
    so source-actor scale[0] (= UE.X) lands on the same Zenith axis the
    wall mesh's long dimension lives on. See module-header comment.
    """
    import math
    x, y, z = loc
    zx = x / 100.0   # UE.X → Zenith.X
    zy = z / 100.0   # UE.Z → Zenith.Y (up stays up)
    zz = y / 100.0   # UE.Y → Zenith.Z
    # UE rotation tuple is [roll, pitch, yaw] in degrees. Yaw rotates around Z in
    # UE → around Y in Zenith. Sign flip happens C++-side, see module header.
    yaw_deg = rot[2] if len(rot) > 2 else 0.0
    yaw_rad = math.radians(yaw_deg)
    return (zx, zy, zz), yaw_rad

def ue_mesh_to_zmodel_stem(ue_path: str) -> str:
    """Convert UE path '/Game/Foo/Bar.Bar' → 'Foo_Bar' file stem.

    Strips the /Game/ prefix, strips the trailing '.X' duplicated name suffix,
    and replaces '/' with '_'. So:
      /Game/DevilsPlayground/Assets/Blockout/Chest/ChestMain.ChestMain
    becomes
      DevilsPlayground_Assets_Blockout_Chest_ChestMain
    """
    if not ue_path:
        return ""
    p = ue_path
    # Strip /Game/ prefix.
    if p.startswith("/Game/"):
        p = p[len("/Game/"):]
    elif p.startswith("/Engine/"):
        # Engine assets are not exported — we'll fall back to the cube proxy.
        return ""
    # Strip the duplicated '.AssetName' suffix.
    if "." in p:
        p = p.rsplit(".", 1)[0]
    # Replace path separators with underscores.
    p = p.replace("/", "_")
    return p

def emit_placement_struct(lines: list) -> None:
    lines.append("\tstruct LightInfo")
    lines.append("\t{")
    lines.append("\t\tfloat r, g, b;        // Linear-RGB 0..1 (UE 0..255 / 255)")
    lines.append("\t\tfloat intensity;     // UE intensity (we map to lumens 1:1 for skeleton)")
    lines.append("\t};")
    lines.append("")
    lines.append("\tstruct Placement")
    lines.append("\t{")
    lines.append("\t\tfloat       x, y, z;     // Zenith (Z-forward, Y-up, metres)")
    lines.append("\t\tfloat       yaw;         // Y-axis rotation, radians")
    lines.append("\t\tconst char* label;       // UE actor label, for debug")
    lines.append("\t\tconst char* mesh;        // UE asset path; \"\" if no mesh in source")
    lines.append("\t\tLightInfo   light;       // {0,0,0,0} when actor has no light")
    lines.append("\t\tfloat       sx, sy, sz;  // UE scale (1,1,1 when omitted)")
    lines.append("\t};")
    lines.append("")

def actor_to_placement(actor: dict) -> tuple[tuple[float, float, float], float, str, str, dict, tuple[float, float, float]]:
    pos, yaw = ue_to_zenith(actor.get("loc", [0, 0, 0]), actor.get("rot", [0, 0, 0]))
    lbl = actor.get("label") or actor.get("name") or ""
    mesh = actor.get("mesh", "") or ""
    light = actor.get("light", {}) or {}
    scale = actor.get("scale", [1.0, 1.0, 1.0]) or [1.0, 1.0, 1.0]
    # UE scale → Zenith scale uses same swap as position: UE.X → Zenith.X
    # (so scale[0], the UE_X scale, drives Zenith.X — the axis the wall mesh's
    # long dimension lives on, so wall-length scaling actually stretches walls).
    sx = scale[0] if len(scale) > 0 else 1.0   # UE.X → Zenith.X
    sy = scale[2] if len(scale) > 2 else 1.0   # UE.Z → Zenith.Y
    sz = scale[1] if len(scale) > 1 else 1.0   # UE.Y → Zenith.Z
    return pos, yaw, lbl, mesh, light, (sx, sy, sz)

def fmt_placement(actor: dict) -> str:
    pos, yaw, lbl, mesh, light, (sx, sy, sz) = actor_to_placement(actor)
    lbl_esc = lbl.replace("\\", "\\\\").replace("\"", "\\\"")
    mesh_esc = mesh.replace("\\", "\\\\").replace("\"", "\\\"")
    if light:
        # UE colors are 0..255 ints; Zenith expects linear-RGB 0..1.
        r = float(light.get("r", 255)) / 255.0
        g = float(light.get("g", 255)) / 255.0
        b = float(light.get("b", 255)) / 255.0
        intensity = float(light.get("intensity", 0.0))
    else:
        r = g = b = 0.0
        intensity = 0.0
    return (
        f"\t\t{{ {pos[0]:.4f}f, {pos[1]:.4f}f, {pos[2]:.4f}f, {yaw:.4f}f, "
        f"\"{lbl_esc}\", \"{mesh_esc}\", "
        f"{{ {r:.4f}f, {g:.4f}f, {b:.4f}f, {intensity:.4f}f }}, "
        f"{sx:.4f}f, {sy:.4f}f, {sz:.4f}f }},"
    )

def main() -> int:
    if not MAP_JSON.exists():
        sys.stderr.write(f"ERROR: {MAP_JSON} not found. Run Tools/dp_export/sweep.py first.\n")
        return 1

    data = json.loads(MAP_JSON.read_text(encoding="utf-8"))
    actors = data.get("actors", [])

    # Bucket by category.
    buckets: dict[str, list[dict]] = {c: [] for c in EMITTED_CATEGORIES}
    skipped: dict[str, int] = {}
    for actor in actors:
        cat = categorise(actor)
        if cat is None or cat not in EMITTED_CATEGORIES:
            key = actor.get("class", "Unknown")
            skipped[key] = skipped.get(key, 0) + 1
            continue
        buckets[cat].append(actor)

    # ---------------------------------------------------------------------------
    # Emit C++ header
    # ---------------------------------------------------------------------------
    lines = []
    lines.append("#pragma once")
    lines.append("// =====================================================================")
    lines.append("// AUTO-GENERATED by Tools/dp_import/generate_level_data.py")
    lines.append("// from Games/DevilsPlayground/Assets/Maps/DevilsPlayground_L_GameLevel.json")
    lines.append("// DO NOT EDIT — regenerate after every asset re-export.")
    lines.append("// =====================================================================")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace DP_LevelData")
    lines.append("{")
    emit_placement_struct(lines)

    for cat in sorted(buckets.keys()):
        rows = buckets[cat]
        ident = cat
        const_ident = "k" + ident
        lines.append(f"\t// {len(rows)} placements ({cat})")
        if not rows:
            lines.append(
                f"\tstatic constexpr Placement {const_ident}[1] = {{ {{ "
                "0.f, 0.f, 0.f, 0.f, \"<none>\", \"\", { 0.f, 0.f, 0.f, 0.f }, 1.f, 1.f, 1.f } };"
            )
            lines.append(f"\tstatic constexpr uint32_t {const_ident}Count = 0;")
        else:
            lines.append(f"\tstatic constexpr Placement {const_ident}[] = {{")
            for actor in rows:
                lines.append(fmt_placement(actor))
            lines.append("\t};")
            lines.append(
                f"\tstatic constexpr uint32_t {const_ident}Count = sizeof({const_ident}) / sizeof({const_ident}[0]);"
            )
        lines.append("")

    lines.append("} // namespace DP_LevelData")
    lines.append("")

    OUT_HEADER.parent.mkdir(parents=True, exist_ok=True)
    OUT_HEADER.write_text("\n".join(lines), encoding="utf-8")

    print(f"Wrote {OUT_HEADER}")
    print(f"Bucket counts: { {k: len(v) for k, v in buckets.items()} }")
    if skipped:
        print(f"Skipped (not in EMITTED_CATEGORIES): {skipped}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
