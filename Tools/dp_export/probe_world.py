"""Probe how to enumerate actors from a saved /Game/.../*.umap World in UE 5.6."""
import sys
from pathlib import Path
LOG = Path("C:/tmp/dp_world_probe.log").open("w", encoding="utf-8")
def log(s):
    LOG.write(str(s) + "\n"); LOG.flush()
    try: unreal.log(str(s))
    except Exception: pass
import unreal

log("=== UnrealEditorSubsystem ===")
try:
    es = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    log(f"  got: {es}")
    log(f"  methods: {[m for m in dir(es) if not m.startswith('_')]}")
except Exception as e:
    log(f"  err: {e}")

log("=== EditorActorSubsystem ===")
try:
    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    log(f"  methods: {[m for m in dir(es) if not m.startswith('_') and 'actor' in m.lower()][:20]}")
except Exception as e:
    log(f"  err: {e}")

log("=== Loading L_GameLevel and probing it ===")
try:
    # Two ways to "load" a world: load_asset (gets the World UObject) vs.
    # EditorLoadingAndSavingUtils.load_map (opens it as the editor's main world).
    world = unreal.EditorAssetLibrary.load_asset("/Game/DevilsPlayground/L_GameLevel")
    log(f"  load_asset: {world} type={type(world).__name__}")
    if world is not None:
        log(f"  world attrs: {[a for a in dir(world) if not a.startswith('_') and ('level' in a.lower() or 'actor' in a.lower())]}")

    # Open the map as the editor's main world (lets get_all_level_actors fire).
    log("  trying EditorLoadingAndSavingUtils.load_map...")
    ok = unreal.EditorLoadingAndSavingUtils.load_map("/Game/DevilsPlayground/L_GameLevel")
    log(f"  load_map result: {ok}")

    es = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = es.get_all_level_actors()
    log(f"  get_all_level_actors: {len(actors)}")
    if len(actors) > 0:
        log("  first 5 actor classes:")
        for a in actors[:5]:
            try:
                loc = a.get_actor_location()
                log(f"    {type(a).__name__} '{a.get_name()}' at ({loc.x:.1f}, {loc.y:.1f}, {loc.z:.1f})")
            except Exception as e:
                log(f"    {type(a).__name__}: {e}")
except Exception as e:
    log(f"  err: {e}")

LOG.close()
