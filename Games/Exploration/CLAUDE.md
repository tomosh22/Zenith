# Exploration Game

A first-person terrain exploration experience demonstrating atmospheric rendering features in the Zenith engine.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Terrain System** | `Zenith_TerrainComponent` | Large-scale terrain rendering with LOD streaming |
| **Instanced Vegetation** | `Zenith_InstancedMeshComponent` | 10,000 instanced trees scattered across the terrain (height/slope-filtered) |
| **Skybox Rendering** | `Flux_SkyboxImpl` | Dynamic sky with day/night cycle |
| **Fog Effects** | `Flux_FogImpl` | Atmospheric depth fog |
| **Cascaded Shadow Maps** | `Flux_ShadowsImpl` | 4-cascade shadow system |
| **SSAO** | `Flux_SSAOImpl` | Screen-space ambient occlusion |
| **First-Person Camera** | `Zenith_CameraComponent` | Mouse-look + WASD movement |
| **Game Components** | `Zenith_ComponentMetaRegistry` | Game logic via component lifecycle hooks |
| **DataAsset System** | `Zenith_DataAsset` | Configuration serialization |
| **UI System** | `Zenith_UIComponent` | Minimal HUD overlay |
| **Multi-Scene** | `Zenith_SceneSystem` | `LoadSceneByIndex()` with `SCENE_LOAD_SINGLE`, `RegisterSceneBuildIndex()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Exploration/
  CLAUDE.md                          # This documentation
  Exploration.cpp                    # Project entry points, resource init, graph builders
  Components/
    Exploration_Config.h             # DataAsset for game configuration
    Exploration_GameComponent.h      # Main coordinator component; holds the Graph_* shims + Test_ surface
    Exploration_GraphNodes.h         # Behaviour Graph node library (all 3 graphs) + Exploration_RegisterGraphNodes
    Exploration_PlayerController.h   # First-person movement and mouse-look (a SYSTEM - stays C++)
    Exploration_TerrainExplorer.h    # Terrain streaming observer
    Exploration_AtmosphereController.h # Day/night cycle + weather + sun/fog math (ApplyAtmosphere)
    Exploration_UIManager.h          # Minimal HUD
  Tests/
    Test_ExplorationCharacterization.cpp  # 7 automated chars (menu/escape/debug/movement/daynight+sun/weather/cross-scene)
  Assets/
    Scenes/MainMenu.zscen            # Serialized main-menu scene (build index 0)
    Scenes/Exploration.zscen         # Serialized gameplay scene (build index 1)
    Graphs/                          # Boot-authored Behaviour Graphs (.bgraph):
                                     #   Exploration_GameFlow, Exploration_PlayerActions, Exploration_Atmosphere
```

## Module Breakdown

### Exploration.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterGameComponents`, `Project_LoadInitialScene`

Demonstrates:
- Project lifecycle hooks
- Scene setup with camera, terrain, and UI entities
- Terrain component creation with materials
- Initial camera positioning for terrain exploration

### Exploration_GameComponent.h - Main Coordinator
**Engine APIs:** `Zenith_ComponentMetaRegistry` (registered as "ExplorationGame", order 100), lifecycle hooks

Demonstrates:
- `OnAwake()` - Runtime initialization
- `OnStart()` - Late initialization after all entities ready
- `OnUpdate(float fDt)` - Main game loop coordinating all systems
- `RenderPropertiesPanel()` - Editor UI (tools build only)
- Coordinator pattern delegating to specialized modules

### Exploration_PlayerController.h - First-Person Movement
**Engine APIs:** `Zenith_Input`, `Zenith_CameraComponent`

Demonstrates:
- Mouse-look with pitch/yaw camera rotation
- WASD movement relative to camera facing direction
- Smooth camera movement with acceleration/deceleration
- Collision with terrain height (gravity simulation)
- Sprint modifier (Shift key)
- Mouse capture and release handling

### Exploration_TerrainExplorer.h - Terrain Observer
**Engine APIs:** `Zenith_TerrainComponent`, `Flux_TerrainStreamingManager`

Demonstrates:
- Terrain height sampling for player placement
- Streaming state observation for debug display
- Chunk position tracking for analytics
- LOD distance visualization

### Exploration_AtmosphereController.h - Day/Night + Weather
**Engine APIs:** `Flux_Graphics`, `Flux_SkyboxImpl`, `Flux_FogImpl`

Demonstrates:
- Day/night cycle with sun position animation
- Sun color temperature changes (warm sunrise/sunset, blue midday)
- Fog density and color tied to time of day
- Ambient light intensity variation
- Weather state machine (clear, cloudy, foggy)
- Smooth transitions between weather states

### Exploration_UIManager.h - HUD Management
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Minimal HUD overlay (coordinates, time of day, FPS)
- Debug information toggle
- UI anchoring (top-left positioning)
- Dynamic text updates

## Behaviour Graphs (game DECISION logic — 3 graphs)

Exploration is a pure-atmosphere demo with very little gameplay logic. Its only genuine
DECISIONS — the menu/play flow, the Tab debug-HUD toggle, the day/night time-advance, and
the weather state machine — live in behaviour graphs; everything continuous (the FPS movement
simulation in `Exploration_PlayerController::Update`, terrain height sampling, the sun/fog math
+ `ApplyToEngine` Flux uploads, the HUD text, the FPS counter) stays a C++ SYSTEM. The graphs
are boot-authored via a fluent `Zenith_GraphBuilder` in `BuildGraph_Exploration*` functions
(`AddStep_GraphBuild` in `Project_RegisterEditorAutomationSteps`; runtime docs in
`Zenith/Scripting/CLAUDE.md`). Nodes live header-only in `Components/Exploration_GraphNodes.h`,
registered via `Exploration_RegisterGraphNodes()` from `Project_RegisterGameComponents`.

| Graph | Attached | Driven by | What moved |
|---|---|---|---|
| `Exploration_GameFlow.bgraph` | BOTH managers (MenuManager + GameManager) | order-60 input sources (`OnUIButtonClicked` / `OnKeyPressed` / `OnUpdate`) | Menu Play / Escape→menu / menu-focus DECISIONS. Play → `OnUIButtonClicked("MenuPlay")`→`LoadSceneByIndex(1)` (SINGLE = old `OnPlayClicked`); Escape → `ExplorationGetGameState`→`SwitchOnInt(gameState, 2)`→pin PLAYING→`ExplorationReturnToMenu`; focus → `OnUpdate`→`ExplorationFocusPlayButton`. Two states (MAIN_MENU=0, PLAYING=1); the MenuManager's gameState gates its Escape off. No pause, no reset. |
| `Exploration_PlayerActions.bgraph` | GameManager | `OnKeyPressed(TAB)` @60 | Tab → `ExplorationToggleDebugHUD` (flips the `Exploration_UIManager` debug-HUD bool that `UpdateUI`@100 reads the same frame). GameManager-only (always PLAYING) preserves the old only-when-PLAYING guard. |
| `Exploration_Atmosphere.bgraph` | GameManager | `"ExplorationAtmosphereTick"` (dt payload, fired inline where `AtmosphereController::Update` sat) | Day/night time-advance + weather FSM DECISIONS. Two `OnCustomEvent` chains in node-add order: `ExplorationAdvanceTime` (A: `timeOfDay += dt/duration`, wrap) THEN `ExplorationTickWeather` (B: `UpdateWeather` verbatim, mt19937 kept in the C++ shim). Then `OnUpdate` calls `ApplyAtmosphere(dt)` (C-I: sun/fog math + `ApplyToEngine`) directly — the systems shim, reading the just-advanced state (byte-identical A→B→C-I order). dt rides the payload (custom-event ctx dt is 0). |

**6 game nodes** (`Exploration_GraphNodes.h`): `ExplorationGetGameState`, `ExplorationReturnToMenu`,
`ExplorationFocusPlayButton` (GameFlow); `ExplorationToggleDebugHUD` (PlayerActions);
`ExplorationAdvanceTime`, `ExplorationTickWeather` (Atmosphere). Each resolves the self component
and calls a verbatim `Graph_*` shim on `Exploration_GameComponent`.

### Shims + source of truth

`m_eGameState` stays the per-instance source of truth (`ExplorationGetGameState` only mirrors it to
the blackboard to gate the Escape chain). The decision bodies live verbatim in the `Graph_*` shims
(`Graph_ReturnToMenu`/`FocusPlayButton`/`ToggleDebugHUD`/`AdvanceTime`/`TickWeather`).
`AtmosphereController::Update` was split into `AdvanceTimeOfDay`[A] / `UpdateWeather`[B] /
`ApplyAtmosphere`[C-I] (each a verbatim slice); the graph drives A+B, `OnUpdate` calls C-I directly.

**Escape-return frame (fixed, byte-identical):** `ReturnToMenu` sets `m_eGameState = MAIN_MENU`
(the graph runs it at order 60) so the systems-only `OnUpdate` switch (order 100) takes the no-op
MAIN_MENU branch for the rest of that frame — faithfully reproducing the original C++ `ReturnToMenu();
return;`, which skipped the remaining PLAYING systems. Without this, the deferred scene load would leave
the state PLAYING and the @100 block would run one EXTRA atmosphere tick + `PlayerController::Update` on
the doomed frame — both mutate NAMESPACE-STATIC state that persists across the SINGLE scene swap (the
weather timer/transition/RNG schedule; and `s_bMouseCaptured` via the otherwise-dead Esc-mouse-release),
a cross-scene-persistent divergence the adversarial review caught. Locked by `MouseCaptureAcrossEsc`.

**Accepted divergences** (unobservable / precedented): two DISTINCT keys pressed on the exact same
frame (e.g. Tab + Esc) both fire — the graph's independent order-60 `OnKeyPressed` sources vs the
original's sequential in-block `if` checks — a humanly-unreachable input coincidence affecting only the
cosmetic debug-HUD bool. Both `Reset()` functions and the in-controller Esc-mouse-release are DEAD CODE
(never reached / never called) — preserved structurally, not wired; state persistence across menu↔play
swaps is load-bearing for byte-identity.

**Equivalence proof:** `Tests/Test_ExplorationCharacterization.cpp` — 8 tests written against the C++
versions FIRST, all identical pre/post conversion (`_True` AND `_False`). Run (windowed; games hang at
shutdown, so tally the per-test JSON):

```
exploration.exe --all-automated-tests --exit-after-frames 30000 --fixed-dt 0.01666 --skip-unit-tests --test-results-dir <dir>
```

Coverage: `MenuPlay` (Play → PLAYING), `ReturnToMenu` (Escape → MAIN_MENU), `DebugToggle` (Tab flips the
debug-HUD bool + back), `PlayerMovement` (hold W → camera +Z + terrain-follow — the movement SYSTEM still
runs), `DayNightAndSun` (time advances/freezes/wraps AND the sun/fog math matches the pure `Calculate*` of
the set time-of-day — locks the A→B→ApplyAtmosphere read-after-write order), `Weather` (a forced transition
advances + interpolates fog toward the new weather), `CrossSceneAtmos` (play→Esc→play: time-of-day resets
via `Configure` but the weather timer PERSISTS), `MouseCaptureAcrossEsc` (the Esc-return frame skips the
@100 PLAYING block, so the mouse-capture flag is unchanged — locks the review fix). The conversion
touched ZERO engine files.

## Multi-Scene Architecture

### Entity Layout

The game uses two scenes, each registered to a build index and loaded one at a time with `SCENE_LOAD_SINGLE` (loading one fully unloads the other — there is no persistent scene):

- **Main Menu Scene** (build index 0, named "MainMenu"): Contains the `MenuManager` entity with `Zenith_CameraComponent`, `Zenith_UIComponent` (title + Play button), and the ExplorationGame component (Exploration_GameComponent).
- **Exploration Scene** (build index 1, named "Exploration"): Contains the `GameManager` entity (camera + ExplorationGame component) plus the procedurally generated terrain and all world entities. Loaded via `LoadSceneByIndex(1, SCENE_LOAD_SINGLE)` when entering gameplay, replaced by the menu scene when returning.

### Game State Machine

```
MAIN_MENU  ──(Play)──>  PLAYING  ──(Escape)──>  MAIN_MENU
```

There is no pause state. Pressing Escape from gameplay returns directly to the main menu.

### Scene Transition Pattern

Uses `LoadSceneByIndex(1, SCENE_LOAD_SINGLE)` to enter gameplay and `LoadSceneByIndex(0, SCENE_LOAD_SINGLE)` to return to the menu. Because `SCENE_LOAD_SINGLE` keeps only one scene active, each transition fully unloads the previous scene and loads the next — nothing persists across the transition.

## Learning Path

1. **Start here:** `Exploration.cpp` - See how exploration scene is initialized
2. **Understand camera:** `Exploration_PlayerController.h` - First-person controls
3. **Study terrain:** `Exploration_TerrainExplorer.h` - Terrain interaction
4. **See atmosphere:** `Exploration_AtmosphereController.h` - Day/night cycle

## Controls

| Key | Action |
|-----|--------|
| W / Up Arrow | Move forward |
| S / Down Arrow | Move backward |
| A / Left Arrow | Strafe left |
| D / Right Arrow | Strafe right |
| Mouse | Look around |
| Shift | Sprint |
| Space | Jump (if applicable) |
| Tab | Toggle debug HUD |
| Escape | Return to menu (from gameplay) |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu (in menu state) |
| Enter | Activate focused button |

## Key Patterns

### First-Person Camera Setup
Initialize perspective camera with position, pitch/yaw angles, FOV, near/far planes via `InitialisePerspective()`. Large far plane (5000) needed for terrain.

### Terrain Height Sampling
Sample terrain height at player XZ position and add eye height offset for camera placement.

### Day/Night Sun Position
Animate sun direction based on time-of-day (0.0-1.0) using cosine/sine for circular arc, then normalize.

### Fog Configuration by Weather
Set target fog density per weather state (clear=0.00015, foggy=0.0015), then smoothly interpolate current density toward target each frame.

## Terrain System Integration

The game demonstrates terrain features from `Flux/Terrain/`:

- **GPU-Driven Culling:** 4096 chunks culled via compute shader
- **LOD Streaming:** LOD_HIGH streamed based on camera distance
- **Always-Resident LOD_LOW:** Fallback ensures no terrain holes
- **Cascaded Shadows:** 4 cascade levels for shadow quality

Distance thresholds for LOD (from `Flux_TerrainConfig.h`):
- LOD_HIGH (0): 0 - 1000m (highest detail, streamed dynamically)
- LOD_LOW (1): 1000m+ (always-resident fallback, never evicted)

## Building

```batch
cd Build
Sharpmake_Build.bat
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Exploration\Build\output\win64\vs2022_debug_win64_true
exploration.exe
```

## Visual Focus

This game prioritizes visual quality and atmosphere over gameplay mechanics:

- **No combat or objectives** - Pure exploration experience
- **Day/night cycle** - Watch the sun rise and set
- **Weather variation** - Fog rolling in and clearing
- **Shadow quality** - 4-cascade CSM for sharp shadows
- **SSAO** - Subtle ambient occlusion for depth
- **Large terrain** - 4km x 4km explorable area

The focus is demonstrating how Zenith's rendering features work together to create an immersive environment.

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### First Run (Terrain Generation)
On the very first launch, the game will generate terrain mesh data:
```
[Exploration] Generating procedural terrain...
[Exploration] Generated procedural heightmap: 4096x4096
[Exploration] Exporting terrain meshes (this may take a while)...
[Exploration] Terrain mesh export complete!
```
This process generates LOD_HIGH, LOD_LOW, and physics collision mesh files for all 4096 terrain chunks (plus the four PBR material texture sets and a splatmap) and may take several minutes.

### Scene Hierarchy
- **MenuManager** - Menu-scene entity (Camera + UI + ExplorationGame component) - build index 0 ("MainMenu")
- **GameManager** - Gameplay-scene entity (Camera + ExplorationGame component) - build index 1 ("Exploration")
- **Terrain** - Terrain entity with Zenith_TerrainComponent (in the Exploration scene)

### Viewport
- **First-person perspective** view at eye height on the terrain
- **Rolling hills terrain** with procedural height variation
- **Grass/rock materials** blending based on terrain height
- **Instanced trees** (10,000 via `SpawnInstancedTrees`) scattered across the terrain, skipping water-level lows and steep highs
- **Dynamic sky** with sun position based on time of day
- **Atmospheric fog** fading distant terrain into the sky
- **Cascaded shadows** from the sun direction

### Properties Panel (when ExplorationGame selected)
- **Time & Weather section**
  - Current time display (e.g., "12:30 PM")
  - Time of Day slider (0.0 - 1.0)
  - Day Cycle Enabled checkbox
  - Weather buttons (Clear, Cloudy, Foggy)
- **Player section**
  - Move Speed, Sprint Multiplier, Mouse Sensitivity
  - Apply Player Settings button
- **Debug section**
  - FPS counter
  - Vertex Buffer usage (MB)
  - High LOD Chunks resident count
  - Streams/Frame counter
  - Show Debug HUD checkbox
- **Atmosphere Debug section**
  - Sun Direction vector
  - Sun Intensity, Ambient Intensity
  - Fog Density value

### Console Output on Boot
```
[Exploration] Terrain mesh files already exist, skipping generation
[Exploration] Creating terrain entity...
[Exploration] Terrain entity created successfully!
```

## Gameplay View (What You See When Playing)

### Initial State
- Camera positioned at terrain center, ~50m above ground
- Looking slightly downward across rolling hills
- Morning/daytime lighting with clear weather
- Mouse not captured (click to capture)

### HUD Elements (Top-Left, Toggle with Tab)
- **Time**: "12:30" - Current time of day (24-hour format)
- **Weather**: "Clear" - Current weather state
- **Position**: "Position: 0, 50, 0" - Player coordinates
- **Chunk**: "Chunk: 32, 32" - Current terrain chunk
- **LOD**: "Terrain LOD: HIGH (Resident: LOD0)" - Current chunk's LOD level (HIGH or LOW)
- **FPS**: "60.0" - Frame rate

Vertex buffer / VRAM usage is not a separate HUD element; it appears only in the **Streaming** debug line (`Streaming: %.0f/%.0f MB | HiLOD: %u | Rate: %u/frame`) shown when the debug HUD is toggled on.

### Gameplay Actions
1. **Mouse Look**: Click to capture mouse, move mouse to look around
2. **Walking**: WASD moves relative to camera facing direction
3. **Sprinting**: Hold Shift for faster movement
4. **Jumping**: Space for vertical movement (with gravity)
5. **Debug Toggle**: Tab shows/hides debug HUD
6. **Mouse Release**: Escape releases mouse capture

### Visual Feedback
- **Day/Night Cycle**: Sun moves across sky if enabled
- **Weather Transitions**: Fog density changes smoothly
- **LOD Streaming**: Terrain detail increases as you approach
- **Terrain Following**: Camera stays at consistent height above terrain
- **Atmospheric Scattering**: Sky colors change with sun position

## Test Plan

### T1: Boot and Terrain Generation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Delete Assets/Terrain folder | Terrain folder removed |
| T1.2 | Launch exploration.exe | Heightmap generation begins |
| T1.3 | Wait for terrain export | All LOD meshes generated (check console) |
| T1.4 | Verify Terrain folder | Contains .ztxtr and .zmesh files |
| T1.5 | Relaunch exploration.exe | "Terrain mesh files already exist" message |

### T2: First-Person Controls
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Click in window | Mouse captured, cursor hidden |
| T2.2 | Move mouse | Camera rotates (pitch and yaw) |
| T2.3 | Press W | Camera moves forward |
| T2.4 | Press S | Camera moves backward |
| T2.5 | Press A | Camera strafes left |
| T2.6 | Press D | Camera strafes right |
| T2.7 | Press Escape | Mouse released, cursor visible |
| T2.8 | Hold Shift + W | Camera moves faster (sprint) |

### T3: Terrain Interaction
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Walk up a hill | Camera follows terrain height |
| T3.2 | Walk down a slope | Camera follows terrain height |
| T3.3 | Walk to terrain edge | Player clamped to terrain bounds |
| T3.4 | Check HUD position | Coordinates update in real-time |
| T3.5 | Move to different chunk | Chunk coordinates update |

### T4: Day/Night Cycle
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Open Properties Panel | Time & Weather section visible |
| T4.2 | Enable Day Cycle | Checkbox checked |
| T4.3 | Wait/observe sky | Sun position moves across sky |
| T4.4 | Move Time slider to 0.0 | Night time (dark sky) |
| T4.5 | Move Time slider to 0.25 | Sunrise (warm colors) |
| T4.6 | Move Time slider to 0.5 | Midday (sun overhead) |
| T4.7 | Move Time slider to 0.75 | Sunset (warm colors) |

### T5: Weather System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Click "Clear" button | Low fog density, clear visibility |
| T5.2 | Click "Cloudy" button | Medium fog, reduced visibility |
| T5.3 | Click "Foggy" button | High fog density, limited visibility |
| T5.4 | Observe fog transition | Smooth blend between states |

### T6: LOD Streaming
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Stand still, check debug HUD | LOD level shown for current chunk |
| T6.2 | Walk toward distant terrain | LOD improves as you approach |
| T6.3 | Walk away from high-detail area | LOD decreases behind you |
| T6.4 | Check VRAM usage | Changes as LODs stream in/out |

### T7: Debug HUD
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Press Tab | Debug HUD appears |
| T7.2 | Verify all debug info visible | Position, chunk, LOD, FPS, VRAM shown |
| T7.3 | Press Tab again | Debug HUD hides |

### T8: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Look straight up | Pitch clamped to prevent over-rotation |
| T8.2 | Look straight down | Pitch clamped to prevent over-rotation |
| T8.3 | Sprint for extended time | No issues, consistent speed |
| T8.4 | Minimize/restore window | Game resumes correctly |
| T8.5 | Run at terrain boundary | Player position clamped, no crashes |

### T9: Menu Navigation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Launch game | Main menu displayed with Play button |
| T9.2 | Click Play button | World scene created, gameplay begins |
| T9.3 | Press Up/Down or W/S | Menu button focus changes |
| T9.4 | Press Enter on focused button | Button activates |

### T10: Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T10.1 | Click Play from main menu | World scene created, terrain loads |
| T10.2 | Press Escape during gameplay | World scene unloaded, main menu shown |
| T10.3 | Click Play again after returning | New world scene created, game works normally |
| T10.4 | Repeat menu/game cycle multiple times | No leaks, no crashes, transitions clean |

### T11: Editor Features (Tools Build Only)
| Step | Action | Expected Result |
|------|--------|-----------------|
| T11.1 | Select GameManager entity | Properties panel appears |
| T11.2 | Modify move speed | Player movement speed changes |
| T11.3 | Modify mouse sensitivity | Look sensitivity changes |
| T11.4 | Toggle Show Debug HUD | Debug HUD visibility changes |
