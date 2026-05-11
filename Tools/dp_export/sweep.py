"""
DevilsPlayground UE5 → Zenith asset sweep.

Run inside the Unreal Editor (or via UnrealEditor-Cmd.exe -run=pythonscript).

Walks /Game/ and exports each kind into the right Zenith assets directory:

    Games/DevilsPlayground/Assets/Meshes/<stem>.gltf      (StaticMesh)
    Games/DevilsPlayground/Assets/Skeletons/<stem>.gltf   (SkeletalMesh)
    Games/DevilsPlayground/Assets/Animations/<stem>.gltf  (AnimSequence)
    Games/DevilsPlayground/Assets/Textures/<stem>.png     (Texture2D)
    Games/DevilsPlayground/Assets/Materials/<stem>.json   (Material/MIC parameter snapshot)
    Games/DevilsPlayground/Assets/Maps/<stem>.json        (World — placement extraction stub)

Logs to C:/tmp/dp_sweep.log + writes Tools/dp_export/manifest.json.

Re-running over an unchanged source set is git-diff-clean.
"""

import sys
import os
import json
import re
from pathlib import Path

LOG_PATH = Path("C:/tmp/dp_sweep.log")
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
    sys.stderr.write("must run inside UE Python\n")
    sys.exit(1)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
ZENITH_ROOT     = Path("C:/dev/Zenith").resolve()
ASSETS_ROOT     = ZENITH_ROOT / "Games" / "DevilsPlayground" / "Assets"
MANIFEST_PATH   = ZENITH_ROOT / "Tools" / "dp_export" / "manifest.json"
UE_CONTENT_ROOT = "/Game"

MESH_DIR  = ASSETS_ROOT / "Meshes"
TEX_DIR   = ASSETS_ROOT / "Textures"
SKEL_DIR  = ASSETS_ROOT / "Skeletons"
ANIM_DIR  = ASSETS_ROOT / "Animations"
MAT_DIR   = ASSETS_ROOT / "Materials"
MAP_DIR   = ASSETS_ROOT / "Maps"

for d in (MESH_DIR, TEX_DIR, SKEL_DIR, ANIM_DIR, MAT_DIR, MAP_DIR):
    d.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def safe_stem(asset_path: str) -> str:
    """Convert /Game/Foo/Bar/MyMesh into Foo_Bar_MyMesh (filename-safe)."""
    s = asset_path
    if s.startswith("/Game/"):
        s = s[len("/Game/"):]
    # Strip the .ObjectName suffix (`/Game/Foo/Bar.Bar` → `Foo/Bar`).
    if "." in s:
        s = s.split(".", 1)[0]
    s = s.replace("/", "_")
    return re.sub(r"[^A-Za-z0-9_\-]", "_", s)

def relpath(p: Path) -> str:
    return p.resolve().relative_to(ZENITH_ROOT).as_posix()

def asset_kind(ad) -> str:
    """Return a string class name. UE 5.1+ asset_class_path > UE 5.0 asset_class."""
    try:
        return str(ad.asset_class_path.asset_name)
    except Exception:
        try:
            return str(ad.asset_class)
        except Exception:
            return "Unknown"

def ue_path_of(ad) -> str:
    try:
        return f"{ad.package_name}.{ad.asset_name}"
    except Exception:
        try:
            return ad.object_path.string if hasattr(ad.object_path, "string") else str(ad.object_path)
        except Exception:
            return "<unknown>"

# ---------------------------------------------------------------------------
# Exporters
# ---------------------------------------------------------------------------
def run_export_task(asset, out_path: Path) -> bool:
    """Run AssetExportTask WITHOUT setting the exporter property — UE infers
    the right exporter from the file extension. Setting it explicitly broke
    glTF mesh export in our testing, so we let UE pick.
    Confirmed working extensions: .fbx, .gltf, .glb, .png."""
    task = unreal.AssetExportTask()
    task.set_editor_property("object", asset)
    task.set_editor_property("filename", str(out_path))
    task.set_editor_property("automated", True)
    task.set_editor_property("prompt", False)
    task.set_editor_property("replace_identical", True)
    try:
        result = unreal.Exporter.run_asset_export_task(task)
        if not result:
            log(f"  WARN: export task returned False for {asset.get_path_name()} -> {out_path}")
        return bool(result)
    except Exception as exc:
        log(f"  WARN: export raised: {exc}")
        return False

def export_mesh_or_anim(asset, out_path: Path) -> bool:
    return run_export_task(asset, out_path)

def export_texture(asset, out_path: Path) -> bool:
    return run_export_task(asset, out_path)

def dump_material(asset, out_path: Path):
    """Materials don't round-trip via export — snapshot their parameter set as
    JSON so Zenith can reconstruct an equivalent Zenith_MaterialAsset."""
    info = {
        "ue_path": asset.get_path_name(),
        "kind":    type(asset).__name__,
        "scalars":  {},
        "vectors":  {},
        "textures": {},
    }
    try:
        if isinstance(asset, unreal.MaterialInstanceConstant):
            for sp in asset.get_editor_property("scalar_parameter_values") or []:
                info["scalars"][str(sp.parameter_info.name)] = sp.parameter_value
            for vp in asset.get_editor_property("vector_parameter_values") or []:
                v = vp.parameter_value
                info["vectors"][str(vp.parameter_info.name)] = [v.r, v.g, v.b, v.a]
            for tp in asset.get_editor_property("texture_parameter_values") or []:
                tex = tp.parameter_value
                info["textures"][str(tp.parameter_info.name)] = (
                    tex.get_path_name() if tex else None
                )
            try:
                parent = asset.get_editor_property("parent")
                info["parent"] = parent.get_path_name() if parent else None
            except Exception:
                info["parent"] = None
    except Exception as exc:
        info["_warning"] = f"parameter snapshot incomplete: {exc}"
    out_path.write_text(json.dumps(info, indent=2, sort_keys=True), encoding="utf-8")

def _get_child_static_meshes(actor):
    """Walk every StaticMeshComponent attached to `actor` (root + SCS
    children) and return a list of world-space transforms + mesh paths.
    Blueprint actors authored via the SCS commonly carry multiple meshes
    — a chest with a base + lid, a door with a frame + leaf — so the
    actor-pivot snapshot the original sweep emits misses everything but
    the first/primary mesh. Returning the full list lets the Zenith side
    author one entity per child component with the correct world
    transform inherited from UE.

    Each entry shape:
        {
          "comp": <component name>,
          "class": "StaticMeshComponent" | "InstancedStaticMeshComponent" | ...,
          "mesh": "/Game/.../MeshName.MeshName",
          "loc":   [x, y, z],
          "rot":   [roll, pitch, yaw],
          "scale": [sx, sy, sz],
        }

    The root primitive (whose world transform matches the actor pivot)
    is included so the consumer can spot it via `comp == "root"` or by
    matching transforms. We deliberately keep it in the list rather than
    skipping it — the consumer has a stable, ordered view of every mesh
    the actor renders.
    """
    out = []

    def _emit_smc(comp, prefix=""):
        try:
            mesh = comp.static_mesh if hasattr(comp, "static_mesh") \
                else comp.get_editor_property("static_mesh")
            if mesh is None:
                return
            mesh_path = mesh.get_path_name()
            wt = comp.get_world_transform()
            t = wt.translation
            r = wt.rotation
            s = wt.scale3d
            # rotation is a Quat — convert to Euler (roll/pitch/yaw) so the
            # Zenith-side conversion can use the same UE-style euler math
            # already wired up in generate_level_data.py.
            try:
                rot_euler = wt.rotator()
                rr, rp, ry = rot_euler.roll, rot_euler.pitch, rot_euler.yaw
            except Exception:
                # Fallback: leave the quat for the consumer to convert.
                rr, rp, ry = r.x, r.y, r.z
            out.append({
                "comp":  prefix + str(comp.get_name()),
                "class": type(comp).__name__,
                "mesh":  mesh_path,
                "loc":   [t.x, t.y, t.z],
                "rot":   [rr, rp, ry],
                "scale": [s.x, s.y, s.z],
            })
        except Exception:
            return

    # Pass 1: direct StaticMeshComponents on the actor.
    try:
        comps = actor.get_components_by_class(unreal.StaticMeshComponent.static_class())
    except Exception:
        comps = []
    for comp in comps or []:
        _emit_smc(comp)

    # Pass 2: ChildActorComponents. BP_Door wraps BP_DoorFrame via a
    # ChildActorComponent, and the door's leaf mesh + frame mesh live on
    # *that* nested actor — not on BP_Door directly. Without this pass we
    # report BP_Door with zero geometry, and the Zenith side authors an
    # invisible door entity. Recursively walking each child actor's own
    # StaticMeshComponents preserves the full visible hierarchy.
    try:
        cacs = actor.get_components_by_class(unreal.ChildActorComponent.static_class())
    except Exception:
        cacs = []
    for cac in cacs or []:
        try:
            child = cac.get_child_actor() if hasattr(cac, "get_child_actor") else None
            if child is None:
                continue
            try:
                child_smcs = child.get_components_by_class(
                    unreal.StaticMeshComponent.static_class())
            except Exception:
                child_smcs = []
            child_label = str(child.get_name()) + "."
            for smc in child_smcs or []:
                _emit_smc(smc, prefix=child_label)
        except Exception:
            continue

    return out


def _get_static_mesh_path_from_actor(actor):
    """For StaticMeshActor (and BP actors carrying a StaticMeshComponent),
    return the /Game/.../MeshName.MeshName path the placement references.
    Returns None if the actor has no StaticMeshComponent or its mesh is unset."""
    try:
        comp = actor.static_mesh_component if hasattr(actor, "static_mesh_component") else None
        if comp is None:
            # Maybe it's a BP actor. Walk components.
            comps = actor.get_components_by_class(unreal.StaticMeshComponent.static_class())
            if not comps:
                return None
            comp = comps[0]
        mesh = comp.static_mesh if hasattr(comp, "static_mesh") else comp.get_editor_property("static_mesh")
        if mesh is None:
            return None
        return mesh.get_path_name()
    except Exception:
        return None

def _get_light_color_from_actor(actor):
    try:
        comps = actor.get_components_by_class(unreal.LightComponent.static_class())
        if not comps:
            return None
        c = comps[0]
        col = c.get_editor_property("light_color")
        intensity = c.get_editor_property("intensity")
        return {"r": col.r, "g": col.g, "b": col.b, "intensity": intensity}
    except Exception:
        return None

def dump_world(asset, out_path: Path):
    """Snapshot every actor's class + transform + key references (StaticMesh
    path, light color/intensity, BP class). The Zenith import side consumes
    this to drive Project_RegisterEditorAutomationSteps for parity with the
    source level."""
    info = {
        "ue_path": asset.get_path_name(),
        "actors":  []
    }
    try:
        # `EditorLoadingAndSavingUtils.load_map` opens the map as the editor's
        # main world (necessary for get_all_level_actors to enumerate it).
        # This mutates editor state — the next world we dump replaces this
        # one, so we never end up holding multiple maps open simultaneously.
        unreal.EditorLoadingAndSavingUtils.load_map(asset.get_path_name())
        actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors = actor_subsystem.get_all_level_actors()

        for actor in actors:
            if actor is None:
                continue
            try:
                loc = actor.get_actor_location()
                rot = actor.get_actor_rotation()
                scl = actor.get_actor_scale3d()
                row = {
                    "name":  str(actor.get_name()),
                    "label": str(actor.get_actor_label()) if hasattr(actor, "get_actor_label") else "",
                    "class": str(type(actor).__name__),
                    "loc":   [loc.x, loc.y, loc.z],
                    "rot":   [rot.roll, rot.pitch, rot.yaw],
                    "scale": [scl.x, scl.y, scl.z],
                }
                # Reference enrichment: mesh path, light props, BP class.
                mesh_ref = _get_static_mesh_path_from_actor(actor)
                if mesh_ref:
                    row["mesh"] = mesh_ref
                light = _get_light_color_from_actor(actor)
                if light:
                    row["light"] = light
                # SCS child meshes — emit the full list of attached
                # StaticMeshComponents (including those reached through
                # ChildActorComponents) so BP actors with multi-mesh
                # hierarchies (BP_Door = frame + leaf via BP_DoorFrame,
                # BP_ChestInteractable = base + lid, etc.) survive the
                # export with their child world transforms intact. Always
                # emit when any child meshes exist so the consumer can see
                # the full mesh hierarchy and detect actors where the
                # actor-pivot snapshot would miss visible geometry.
                children = _get_child_static_meshes(actor)
                if children:
                    row["children"] = children
                # For BP actors the class path tells us which BP it is.
                try:
                    cls = actor.get_class()
                    cls_path = cls.get_path_name() if cls else ""
                    if cls_path and "/Game/" in cls_path:
                        row["bp_class"] = cls_path
                except Exception:
                    pass
                info["actors"].append(row)
            except Exception:
                # Some actors don't have transforms (WorldSettings etc.) — skip.
                pass
    except Exception as exc:
        info["_warning"] = f"world dump incomplete: {exc}"
    out_path.write_text(json.dumps(info, indent=2, sort_keys=True), encoding="utf-8")

# ---------------------------------------------------------------------------
# Sweep
# ---------------------------------------------------------------------------
def sweep():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        registry.wait_for_completion()
    except Exception:
        pass
    asset_data_list = registry.get_assets_by_path(UE_CONTENT_ROOT, recursive=True)
    log(f"[sweep] {len(asset_data_list)} total assets in /Game/")

    rows = []
    counts = {
        "static_mesh": 0, "skeletal_mesh": 0, "animation": 0,
        "texture": 0, "material": 0, "world": 0,
        "skipped_redirector": 0, "skipped_other": 0, "errors": 0,
    }

    for ad in asset_data_list:
        kind = asset_kind(ad)
        ue_path = ue_path_of(ad)

        # Skip ObjectRedirectors entirely — they're stale rename pointers, not real assets.
        if kind == "ObjectRedirector":
            counts["skipped_redirector"] += 1
            continue

        asset = ad.get_asset()
        if asset is None:
            log(f"  WARN: get_asset() None for {ue_path}")
            counts["errors"] += 1
            continue

        stem = safe_stem(ue_path)
        out_rel = ""
        out_kind = None

        try:
            if kind == "StaticMesh":
                out = MESH_DIR / f"{stem}.gltf"
                if export_mesh_or_anim(asset, out):
                    out_rel, out_kind = relpath(out), "static_mesh"
                    counts["static_mesh"] += 1

            elif kind == "SkeletalMesh":
                out = SKEL_DIR / f"{stem}.gltf"
                if export_mesh_or_anim(asset, out):
                    out_rel, out_kind = relpath(out), "skeletal_mesh"
                    counts["skeletal_mesh"] += 1

            elif kind == "AnimSequence":
                out = ANIM_DIR / f"{stem}.gltf"
                if export_mesh_or_anim(asset, out):
                    out_rel, out_kind = relpath(out), "animation"
                    counts["animation"] += 1

            elif kind == "Texture2D":
                out = TEX_DIR / f"{stem}.png"
                if export_texture(asset, out):
                    out_rel, out_kind = relpath(out), "texture"
                    counts["texture"] += 1

            elif kind in ("Material", "MaterialInstanceConstant", "MaterialFunction", "MaterialParameterCollection"):
                out = MAT_DIR / f"{stem}.json"
                dump_material(asset, out)
                out_rel = relpath(out)
                out_kind = "material" if kind != "MaterialInstanceConstant" else "material_instance"
                counts["material"] += 1

            elif kind == "World":
                out = MAP_DIR / f"{stem}.json"
                dump_world(asset, out)
                out_rel, out_kind = relpath(out), "world"
                counts["world"] += 1

            else:
                # Blueprints, BehaviorTree, Blackboard, NiagaraSystem, InputAction,
                # WidgetBlueprint, ControlRig, etc. don't have a clean export path
                # without UE-specific decoding. We log them in the manifest as
                # "todo" so the import side has a paper trail.
                counts["skipped_other"] += 1
                rows.append({
                    "ue_path":            ue_path,
                    "kind":               kind,
                    "status":             "todo",
                    "target_zenith_path": "",
                    "owning_stream":      "W2-B1",
                })
                continue
        except Exception as exc:
            log(f"  ERROR exporting {ue_path}: {exc}")
            counts["errors"] += 1
            continue

        rows.append({
            "ue_path":            ue_path,
            "kind":               out_kind or kind.lower(),
            "status":             "done" if out_rel else "todo",
            "target_zenith_path": out_rel,
            "owning_stream":      "W2-B1",
        })

    manifest = {
        "_comment": "Generated by Tools/dp_export/sweep.py",
        "_counts":  counts,
        "rows":     rows,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8")
    log(f"[sweep] Wrote {MANIFEST_PATH}")
    log(f"[sweep] Counts: {counts}")

sweep()
log("[sweep] Done.")
_log_file.close()
