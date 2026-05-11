"""Probe a single BP_Door actor's component tree to find where the world position lives."""
import sys
from pathlib import Path
LOG = Path("C:/tmp/dp_door_probe.log").open("w", encoding="utf-8")
def log(s):
    LOG.write(str(s) + "\n"); LOG.flush()
    try: unreal.log(str(s))
    except Exception: pass
import unreal

unreal.EditorLoadingAndSavingUtils.load_map("/Game/DevilsPlayground/L_GameLevel")
es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = es.get_all_level_actors()

doors = [a for a in actors if "BP_Door" in a.get_actor_label()][:2]
log(f"Found {len(doors)} BP_Door actors to inspect")

for door in doors:
    log(f"\n=== {door.get_actor_label()} ===")
    log(f"  actor.get_actor_location: {door.get_actor_location()}")
    log(f"  actor.get_actor_rotation: {door.get_actor_rotation()}")

    # Try get_root_component world transform.
    try:
        rc = door.root_component
        log(f"  root_component: {rc.__class__.__name__}")
        log(f"    get_world_location: {rc.get_world_location()}")
        log(f"    get_relative_location: {rc.get_relative_location()}")
    except Exception as e:
        log(f"  root_component error: {e}")

    # Walk all SceneComponents.
    comps = door.get_components_by_class(unreal.SceneComponent.static_class())
    log(f"  {len(comps)} SceneComponents:")
    for i, c in enumerate(comps):
        try:
            cls = type(c).__name__
            try:
                wl = c.get_world_location()
                wl_str = f"({wl.x:.1f},{wl.y:.1f},{wl.z:.1f})"
            except Exception:
                wl_str = "<no get_world_location>"
            try:
                rl = c.get_relative_location()
                rl_str = f"({rl.x:.1f},{rl.y:.1f},{rl.z:.1f})"
            except Exception:
                rl_str = "<no get_relative_location>"
            log(f"    [{i}] {cls:35s} world={wl_str} rel={rl_str}")
        except Exception as e:
            log(f"    [{i}] err: {e}")

LOG.close()
