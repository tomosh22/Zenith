# Survival Example Game

A resource gathering and crafting survival game demonstrating advanced Zenith engine features including the Task System, Event System, and Query System.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Task System** | `Zenith_Task`, `Zenith_TaskArray` | Background world updates, parallel node processing |
| **Event System** | `Zenith_EventDispatcher` | Custom game events with deferred queue |
| **Query System** | `Zenith_Query` | Multi-component entity queries |
| **Entity-Component System** | `Zenith_Entity`, `Zenith_Scene` | Entity creation, component attachment |
| **Script Behaviors** | `Zenith_ScriptBehaviour` | Game logic via lifecycle hooks |
| **Prefab System** | `Zenith_Prefab`, `Zenith_Scene::Instantiate` | Entity templates for player, resources |
| **Input Handling** | `Zenith_Input` | Continuous and discrete input polling |
| **UI System** | `Zenith_UIComponent`, `Zenith_UIText` | HUD with inventory, crafting progress |
| **Model Rendering** | `Zenith_ModelComponent`, `Flux_MeshGeometry` | Procedural geometry for all objects |
| **Materials/Textures** | `Zenith_MaterialAsset` | Color-coded materials for game objects |
| **DataAsset System** | `Zenith_DataAsset` | Configuration with serialization |
| **Camera** | `Zenith_CameraComponent` | Third-person follow camera |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Survival/
  CLAUDE.md                      # This documentation
  Survival.cpp                   # Project entry points, resource initialization
  Components/
    Survival_Behaviour.h         # Main coordinator (uses all modules below)
    Survival_Config.h            # DataAsset for game configuration
    Survival_EventBus.h          # Custom game events
    Survival_Inventory.h         # Item storage
    Survival_ResourceNode.h      # Harvestable resources
    Survival_PlayerController.h  # Movement and interaction input
    Survival_WorldQuery.h        # Entity queries
    Survival_CraftingSystem.h    # Recipe processing
    Survival_TaskProcessor.h     # Background task management
    Survival_UIManager.h         # HUD updates
  Assets/
    Scenes/Survival.zscen        # Serialized scene
```

## Key Systems

### Task System Integration (Survival_TaskProcessor.h)

Demonstrates `Zenith_Task` and `Zenith_TaskArray`:

Submit single operations via `Zenith_Task` with `Zenith_TaskSystem::SubmitTask()`, or parallel work via `Zenith_TaskArray` with invocation count and optional submitting-thread-joins flag.

Key patterns:
- Use atomic counters for thread-safe result aggregation
- Queue events via `Survival_EventBus::QueueEvent()` for thread-safe communication
- Call `ProcessDeferredEvents()` on main thread to handle queued events

### Event System Integration (Survival_EventBus.h)

Demonstrates `Zenith_EventDispatcher`:

Define custom event structs, subscribe via `SubscribeLambda<EventType>()`, dispatch immediately on main thread via `Dispatch()`, or queue from background tasks via `QueueEvent()` for deferred processing.

### Query System Integration (Survival_WorldQuery.h)

Demonstrates `Zenith_Query`:

Use `scene.Query<ComponentA, ComponentB>()` with `.Count()`, `.First()`, `.Any()`, or `.ForEach()` for multi-component entity queries.

## Module Breakdown

### Survival.cpp - Entry Points
- Project lifecycle hooks (`Project_GetName`, `Project_RegisterScriptBehaviours`, `Project_LoadInitialScene`)
- Procedural geometry generation (cube, sphere, capsule)
- Material and prefab creation
- Scene setup with camera and UI

### Survival_Behaviour.h - Main Coordinator
- Lifecycle hooks (OnAwake, OnStart, OnUpdate)
- Event subscription and handling
- System coordination (movement, crafting, resources)
- World generation

### Survival_TaskProcessor.h - Background Tasks
- `Zenith_Task` for single operations
- `Zenith_TaskArray` for parallel processing
- Thread-safe event queuing
- Atomic result counters

### Survival_EventBus.h - Game Events
- Custom event definitions
- Immediate and deferred dispatch
- Lambda subscription support
- Event handle management

### Survival_WorldQuery.h - Entity Queries
- `Zenith_Query` usage examples
- Distance-based filtering
- Multi-component queries
- Performance considerations

### Survival_ResourceNode.h - Resource System
- Resource types (Tree, Rock, Berry Bush)
- Hit mechanics and depletion
- Respawn timers
- Visual feedback

### Survival_CraftingSystem.h - Crafting
- Recipe definitions
- Material consumption
- Progress tracking
- Event-based completion

### Survival_Inventory.h - Item Storage
- Item type management
- Add/remove with validation
- Tool bonus tracking
- Event dispatch on changes

### Survival_PlayerController.h - Input
- Camera-relative movement
- Interaction key handling
- Continuous vs discrete input

### Survival_UIManager.h - HUD
- Inventory display
- Crafting progress bar
- Interaction prompts
- Status messages

## Multi-Scene Architecture

### Entity Layout

The game uses two scenes:

- **Persistent Scene** (default scene): Contains the `GameManager` entity with `Zenith_CameraComponent`, `Zenith_UIComponent`, and `Zenith_ScriptComponent` (Survival_Behaviour). This entity calls `DontDestroyOnLoad()` so it survives scene transitions.
- **World Scene** (`m_xWorldScene`, named "World"): Contains the player, resource nodes (trees, rocks, berry bushes), ground plane, and all world entities. Created when entering gameplay, unloaded when returning to menu.

### Game State Machine

```
MAIN_MENU  ──(Play)──>  PLAYING  ──(Escape)──>  MAIN_MENU
```

There is no pause state. Pressing Escape from gameplay returns directly to the main menu.

### Scene Transition Pattern

Uses `CreateEmptyScene("World")` + `SetActiveScene()` to start gameplay, `UnloadScene()` to return to menu. GameManager persists via DontDestroyOnLoad.

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move (camera-relative) |
| E | Harvest nearest resource |
| 1 | Craft Axe (3 Wood + 2 Stone) |
| 2 | Craft Pickaxe (2 Wood + 3 Stone) |
| R | Reset game |
| Escape | Return to menu (from gameplay) |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu (in menu state) |
| Enter | Activate focused button |
| Tab | (Reserved for inventory) |
| C | (Reserved for crafting menu) |

## Gameplay Loop

1. **Gather Resources**: Walk up to trees, rocks, or berry bushes and press E to harvest
2. **Check Inventory**: Resource counts shown in top-right HUD
3. **Craft Tools**: When you have enough materials, press 1 or 2 to craft
4. **Tool Bonuses**: Axe gives 2x wood, Pickaxe gives 2x stone
5. **Resource Respawn**: Depleted nodes respawn after a delay

## Key Patterns

### Task-Based World Update
Submit parallel node updates, process deferred events on main thread, then wait for task completion.

### Event-Driven Architecture
Resource nodes dispatch harvest events, main behavior handles them to update inventory and show status messages.

### Query-Based Resource Finding
Find nearest harvestable resource within interaction range, then apply hit with bonus multiplier from equipped tools.

## Building

```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Survival\Build\output\win64\vs2022_debug_win64_true
survival.exe
```

## Technical Notes

### Thread Safety
- `Survival_EventBus::QueueEvent()` is thread-safe for background tasks
- `ProcessDeferredEvents()` must be called on main thread
- Atomic counters used for task result aggregation
- Resource node data modified only from background tasks (no scene graph changes)

### Performance Considerations
- Task array distributes work across all worker threads
- Main thread can join work with `bSubmittingThreadJoins = true`
- Queries iterate only valid entities (not max entity ID range)
- Resource manager uses fixed array to avoid allocations

### Extension Points
- Add more resource types by extending `SurvivalResourceType` enum
- Add new items by extending `SurvivalItemType` enum
- Add recipes by extending `Survival_CraftingSystem`
- Add new event types for game-specific systems

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + Script) - `DontDestroyOnLoad`
- **Player** - Player character entity (capsule) at world origin (in World scene)
- **Tree_X** - Multiple tree resource entities scattered around (in World scene)
- **Rock_X** - Multiple rock resource entities scattered around (in World scene)
- **BerryBush_X** - Multiple berry bush entities scattered around (in World scene)
- **Ground** - Ground plane entity (in World scene)

### Viewport
- **Third-person perspective** view behind and above the player
- **Player character** (green capsule) at spawn point
- **Trees** (brown vertical cylinders with green sphere tops) scattered around
- **Rocks** (gray cubes) scattered around
- **Berry bushes** (small purple spheres) scattered around
- **Flat ground plane** as the play area

### Properties Panel (when SurvivalGame selected)
- **Player section** - Move speed, interaction range
- **Resources section** - Respawn time, harvest amounts
- **Crafting section** - Recipe costs, tool bonuses
- **Debug section** - Task system stats, event counts

### Console Output on Boot
```
[Survival] Initializing world
[Survival] Spawning resource nodes...
[Survival] Created 10 trees, 8 rocks, 6 berry bushes
[Survival] Player spawned at origin
[Survival] Event system initialized
```

## Gameplay View (What You See When Playing)

### Initial State
- Player character at world center
- Camera behind and above player
- Various resource nodes visible around the environment
- Inventory showing all zeros

### HUD Elements (Top-Right)
- **"Wood: 0"** - Wood resource count
- **"Stone: 0"** - Stone resource count
- **"Berries: 0"** - Berries count
- **"Axe: No"** - Tool ownership status
- **"Pickaxe: No"** - Tool ownership status
- **"[E] Harvest"** - Interaction prompt (when near resource)

### Center Screen (Contextual)
- **"+3 Wood"** - Status message when harvesting (fades out)
- **"Crafted Axe!"** - Crafting success message
- **"Not enough materials"** - Crafting failure message

### Gameplay Actions
1. **Movement**: WASD moves player relative to camera
2. **Harvesting**: Press E near a resource to gather it
3. **Crafting**: Press 1 for Axe, 2 for Pickaxe when materials available
4. **Tool Usage**: Tools automatically apply bonuses when harvesting

### Visual Feedback
- Resource nodes shake when hit
- Depleted resources fade/shrink temporarily
- Resources respawn with fade-in effect
- Player rotates to face movement direction
- Interaction prompt appears when near harvestable resource

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch survival.exe | Window opens with third-person view |
| T1.2 | Check resource spawns | Trees, rocks, berry bushes visible |
| T1.3 | Check player spawn | Player at center of world |
| T1.4 | Verify HUD | Inventory counts all show 0 |

### T2: Player Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Press W | Player moves forward (away from camera) |
| T2.2 | Press S | Player moves backward |
| T2.3 | Press A | Player moves left |
| T2.4 | Press D | Player moves right |
| T2.5 | Diagonal movement | WASD combinations work correctly |
| T2.6 | Check rotation | Player faces movement direction |

### T3: Resource Harvesting
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Walk near tree | "[E] Harvest" prompt appears |
| T3.2 | Press E near tree | Tree shakes, "+X Wood" message appears |
| T3.3 | Check inventory | Wood count increases |
| T3.4 | Harvest repeatedly | Tree depletes and fades |
| T3.5 | Wait for respawn | Tree reappears after delay |
| T3.6 | Harvest rock | Stone count increases |
| T3.7 | Harvest berry bush | Berries count increases |

### T4: Crafting System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Press 1 with no materials | "Not enough materials" message |
| T4.2 | Gather 3 wood, 2 stone | Inventory shows materials |
| T4.3 | Press 1 | "Crafted Axe!" message, materials consumed |
| T4.4 | Check inventory | "Axe: Yes" displayed |
| T4.5 | Gather 2 wood, 3 stone | Materials for pickaxe ready |
| T4.6 | Press 2 | "Crafted Pickaxe!" message |

### T5: Tool Bonuses
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Harvest tree without axe | Normal wood amount (e.g., 1-2) |
| T5.2 | Craft axe | Axe acquired |
| T5.3 | Harvest tree with axe | Double wood amount (e.g., 2-4) |
| T5.4 | Harvest rock without pickaxe | Normal stone amount |
| T5.5 | Craft pickaxe | Pickaxe acquired |
| T5.6 | Harvest rock with pickaxe | Double stone amount |

### T6: Event System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Harvest resource | Event fires, inventory updates |
| T6.2 | Craft item | Event fires, tool status updates |
| T6.3 | Resource respawns | Event fires, node becomes harvestable |
| T6.4 | Check console logs | Event dispatch messages visible |

### T7: Task System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Multiple resources respawning | No frame drops from parallel processing |
| T7.2 | Harvest many resources rapidly | All updates processed correctly |
| T7.3 | Check debug stats | Task completion counts visible |

### T8: Game Reset
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Gather resources and craft | Inventory populated |
| T8.2 | Press R | All inventory reset to 0 |
| T8.3 | Check resources | All nodes restored to full |
| T8.4 | Check tools | Tools removed from inventory |

### T9: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Press E with no resource nearby | Nothing happens |
| T9.2 | Harvest depleted resource | Nothing happens |
| T9.3 | Craft with exact materials | Works correctly |
| T9.4 | Walk away during respawn | Respawn still occurs |
| T9.5 | Minimize/restore window | Game resumes correctly |

### T10: Menu Navigation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T10.1 | Launch game | Main menu displayed with Play button |
| T10.2 | Click Play button | World scene created, gameplay begins |
| T10.3 | Press Up/Down or W/S | Menu button focus changes |
| T10.4 | Press Enter on focused button | Button activates |

### T11: Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T11.1 | Click Play from main menu | World scene created, resources spawn |
| T11.2 | Press Escape during gameplay | World scene unloaded, main menu shown |
| T11.3 | Click Play again after returning | New world scene created, game works normally |
| T11.4 | Repeat menu/game cycle multiple times | No leaks, no crashes, transitions clean |

### T12: Editor Features (Tools Build Only)
| Step | Action | Expected Result |
|------|--------|-----------------|
| T12.1 | Select GameManager entity | Properties panel appears |
| T12.2 | Modify interaction range | Range affects harvest distance |
| T12.3 | Modify respawn time | Resources respawn faster/slower |
| T12.4 | Check task debug info | Parallel task stats displayed |
