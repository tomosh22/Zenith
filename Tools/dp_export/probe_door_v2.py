"""Thorough BP_Door probe — every angle that could be hiding the world position."""
import sys
from pathlib import Path
LOG = Path("C:/tmp/dp_door_v2.log").open("w", encoding="utf-8")
def log(s):
    LOG.write(str(s) + "\n"); LOG.flush()
    try: unreal.log(str(s))
    except Exception: pass
import unreal

# Try World Partition: load every cell so all actors are visible.
log("=== Loading L_GameLevel ===")
unreal.EditorLoadingAndSavingUtils.load_map("/Game/DevilsPlayground/L_GameLevel")

# WP cell loading. Several APIs may exist depending on UE version.
for api_name in ("WorldPartitionBlueprintLibrary", "WorldPartitionLib", "EditorWorldPartitionLib"):
    if hasattr(unreal, api_name):
        log(f"  found unreal.{api_name}")
        try:
            getattr(unreal, api_name).load_all_cells()
            log("    load_all_cells OK")
        except Exception as e:
            log(f"    load_all_cells err: {e}")

es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = es.get_all_level_actors()
log(f"\n=== Total actors after WP load: {len(actors)} ===")

doors = [a for a in actors if "BP_Door" in a.get_actor_label()]
log(f"BP_Door actors: {len(doors)}")

for door in doors[:4]:
    log(f"\n--- {door.get_actor_label()} (name={door.get_name()}) ---")

    # Method 1: get_actor_location / get_actor_transform.
    try:
        loc = door.get_actor_location()
        log(f"  get_actor_location: ({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f})")
    except Exception as e:
        log(f"  get_actor_location err: {e}")

    try:
        tr = door.get_actor_transform()
        t = tr.translation
        r = tr.rotation
        s = tr.scale3d
        log(f"  get_actor_transform: T=({t.x:.1f},{t.y:.1f},{t.z:.1f}) R=({r.x:.3f},{r.y:.3f},{r.z:.3f},{r.w:.3f}) S=({s.x:.2f},{s.y:.2f},{s.z:.2f})")
    except Exception as e:
        log(f"  get_actor_transform err: {e}")

    # Method 2: bounds.
    try:
        bb = door.get_actor_bounds(False)
        if isinstance(bb, tuple) and len(bb) == 2:
            origin, ext = bb
            log(f"  bounds: origin=({origin.x:.1f},{origin.y:.1f},{origin.z:.1f}) extent=({ext.x:.1f},{ext.y:.1f},{ext.z:.1f})")
    except Exception as e:
        log(f"  bounds err: {e}")

    # Method 3: every SceneComponent's world transform via get_world_transform().
    log("  All SceneComponents (with full world transforms):")
    comps = door.get_components_by_class(unreal.SceneComponent.static_class())
    for i, c in enumerate(comps):
        try:
            wt = c.get_world_transform()
            wl = wt.translation
            cls = type(c).__name__
            log(f"    [{i}] {cls:40s} world=({wl.x:8.1f}, {wl.y:8.1f}, {wl.z:8.1f})")
        except Exception as e:
            log(f"    [{i}] {type(c).__name__}: err {e}")

    # Method 4: ISM/HISM probe.
    for cls_name in ("InstancedStaticMeshComponent", "HierarchicalInstancedStaticMeshComponent"):
        if hasattr(unreal, cls_name):
            cls = getattr(unreal, cls_name)
            ism_comps = door.get_components_by_class(cls.static_class())
            for j, ism in enumerate(ism_comps):
                try:
                    n = ism.get_instance_count()
                    log(f"  {cls_name}[{j}]: {n} instances")
                    for k in range(min(n, 5)):
                        ok, tr = ism.get_instance_transform(k, world_space=True)
                        if ok:
                            t = tr.translation
                            log(f"    instance[{k}] world=({t.x:.1f}, {t.y:.1f}, {t.z:.1f})")
                except Exception as e:
                    log(f"  ISM probe err: {e}")

    # Method 5: child actor components.
    try:
        cac = door.get_components_by_class(unreal.ChildActorComponent.static_class())
        log(f"  ChildActorComponents: {len(cac)}")
        for j, ch in enumerate(cac):
            try:
                child = ch.get_child_actor()
                if child:
                    cl = child.get_actor_location()
                    log(f"    child[{j}] {child.get_name()} at ({cl.x:.1f},{cl.y:.1f},{cl.z:.1f})")
            except Exception as e:
                log(f"    child err: {e}")
    except Exception as e:
        log(f"  child actor err: {e}")

LOG.close()
