# Exploration Game

A first-person terrain exploration experience demonstrating atmospheric rendering features in the Zenith engine.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Terrain System** | `Zenith_TerrainComponent` | Large-scale terrain rendering with LOD streaming |
| **Skybox Rendering** | `Flux_Skybox` | Dynamic sky with day/night cycle |
| **Fog Effects** | `Flux_Fog` | Atmospheric depth fog |
| **Cascaded Shadow Maps** | `Flux_Shadows` | 4-cascade shadow system |
| **SSAO** | `Flux_SSAO` | Screen-space ambient occlusion |
| **Async Asset Loading** | `Zenith_AsyncAssetLoader` | Background asset streaming |
| **First-Person Camera** | `Zenith_CameraComponent` | Mouse-look + WASD movement |
| **Script Behaviours** | `Zenith_ScriptBehaviour` | Game logic via lifecycle hooks |
| **DataAsset System** | `Zenith_DataAsset` | Configuration serialization |
| **UI System** | `Zenith_UIComponent` | Minimal HUD overlay |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()` |
| **UI Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Exploration/
  CLAUDE.md                          # This documentation
  Exploration.cpp                    # Project entry points, resource initialization
  Components/
    Exploration_Config.h             # DataAsset for game configuration
    Exploration_Behaviour.h          # Main coordinator (uses modules below)
    Exploration_PlayerController.h   # First-person movement and mouse-look
    Exploration_TerrainExplorer.h    # Terrain streaming observer
    Exploration_AtmosphereController.h # Day/night cycle + weather
    Exploration_AsyncLoader.h        # Asset streaming manager
    Exploration_UIManager.h          # Minimal HUD
  Assets/
    Scenes/Exploration.zscn          # Serialized scene
```

## Module Breakdown

### Exploration.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterScriptBehaviours`, `Project_LoadInitialScene`

Demonstrates:
- Project lifecycle hooks
- Scene setup with camera, terrain, and UI entities
- Terrain component creation with materials
- Initial camera positioning for terrain exploration

### Exploration_Behaviour.h - Main Coordinator
**Engine APIs:** `Zenith_ScriptBehaviour`, lifecycle hooks

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
**Engine APIs:** `Flux_Graphics`, `Flux_Skybox`, `Flux_Fog`

Demonstrates:
- Day/night cycle with sun position animation
- Sun color temperature changes (warm sunrise/sunset, blue midday)
- Fog density and color tied to time of day
- Ambient light intensity variation
- Weather state machine (clear, cloudy, foggy)
- Smooth transitions between weather states

### Exploration_AsyncLoader.h - Asset Streaming
**Engine APIs:** `Zenith_AsyncAssetLoader`

Demonstrates:
- Background texture loading
- Load state tracking (pending, loading, loaded, failed)
- Progress reporting for UI
- Priority-based loading queue
- Cancellation support for scene transitions

### Exploration_UIManager.h - HUD Management
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Minimal HUD overlay (coordinates, time of day, FPS)
- Debug information toggle
- UI anchoring (top-left positioning)
- Dynamic text updates

## Multi-Scene Architecture

### Entity Layout

The game uses two scenes:

- **Persistent Scene** (default scene): Contains the `GameManager` entity with `Zenith_CameraComponent`, `Zenith_UIComponent`, and `Zenith_ScriptComponent` (Exploration_Behaviour). This entity calls `DontDestroyOnLoad()` so it survives scene transitions.
- **World Scene** (`m_xWorldScene`, named "World"): Contains terrain, atmosphere objects, and all world entities. Created when entering gameplay, unloaded when returning to menu.

### Game State Machine

```
MAIN_MENU  ──(Play)──>  PLAYING  ──(Escape)──>  MAIN_MENU
```

There is no pause state. Pressing Escape from gameplay returns directly to the main menu.

### Scene Transition Pattern

**Menu to Gameplay:**
```cpp
m_xWorldScene = Zenith_SceneManager::CreateEmptyScene("World");
Zenith_SceneManager::SetActiveScene(m_xWorldScene);
// Populate world: terrain, resources, etc.
```

**Gameplay to Menu:**
```cpp
Zenith_SceneManager::UnloadScene(m_xWorldScene);
// GameManager persists (DontDestroyOnLoad), UI switches to menu
```

## Learning Path

1. **Start here:** `Exploration.cpp` - See how exploration scene is initialized
2. **Understand camera:** `Exploration_PlayerController.h` - First-person controls
3. **Study terrain:** `Exploration_TerrainExplorer.h` - Terrain interaction
4. **See atmosphere:** `Exploration_AtmosphereController.h` - Day/night cycle
5. **Advanced:** `Exploration_AsyncLoader.h` - Async loading patterns

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
```cpp
// Camera with pitch/yaw rotation
Zenith_CameraComponent& xCamera = xCameraEntity.AddComponent<Zenith_CameraComponent>();
xCamera.InitialisePerspective(
    Zenith_Maths::Vector3(0.f, 100.f, 0.f),  // Position on terrain
    -0.3f,    // Pitch: slightly looking down
    0.f,      // Yaw: facing forward
    glm::radians(70.f),  // FOV
    0.1f,     // Near plane
    5000.f,   // Far plane (large for terrain)
    16.f / 9.f
);
```

### Terrain Height Sampling
```cpp
// Get terrain height at player position
float fTerrainHeight = Exploration_TerrainExplorer::GetTerrainHeightAt(
    xPlayerPos.x, xPlayerPos.z, xTerrainComponent);
xPlayerPos.y = fTerrainHeight + s_fPlayerEyeHeight;
```

### Day/Night Sun Position
```cpp
// Animate sun based on time of day (0.0 - 1.0)
float fSunAngle = m_fTimeOfDay * 2.0f * 3.14159f;
Zenith_Maths::Vector3 xSunDir(
    cos(fSunAngle),
    sin(fSunAngle),  // Y = height in sky
    0.3f             // Slight offset for more interesting lighting
);
xSunDir = glm::normalize(xSunDir);
```

### Fog Configuration by Weather
```cpp
// Adjust fog based on weather state
switch (m_eWeatherState)
{
case WEATHER_CLEAR:
    m_fTargetFogDensity = 0.0001f;
    break;
case WEATHER_FOGGY:
    m_fTargetFogDensity = 0.001f;
    break;
}
// Smoothly interpolate current fog to target
m_fCurrentFogDensity = glm::mix(m_fCurrentFogDensity, m_fTargetFogDensity, fDt * 0.5f);
```

## Terrain System Integration

The game demonstrates terrain features from `Flux/Terrain/`:

- **GPU-Driven Culling:** 4096 chunks culled via compute shader
- **LOD Streaming:** LOD0-2 streamed based on camera distance
- **Always-Resident LOD3:** Fallback ensures no terrain holes
- **Cascaded Shadows:** 4 cascade levels for shadow quality

Distance thresholds for LOD (from `Flux_TerrainConfig.h`):
- LOD0: 0 - 632m (highest detail)
- LOD1: 632 - 1000m
- LOD2: 1000 - 1414m
- LOD3: 1414m+ (always loaded)

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
[Exploration] Generated material map: 4096x4096
[Exploration] Saved heightmap: .../Terrain/ExplorationHeightmap.tif
[Exploration] Exporting terrain meshes (this may take a while)...
[Exploration] Terrain mesh export complete!
```
This process generates LOD0-LOD3 mesh files for all 4096 terrain chunks and may take several minutes.

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UI + Script) - `DontDestroyOnLoad`
- **Terrain** - Terrain entity with Zenith_TerrainComponent (in World scene)

### Viewport
- **First-person perspective** view at eye height on the terrain
- **Rolling hills terrain** with procedural height variation
- **Grass/rock materials** blending based on terrain height
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
- **Time**: "12:30 PM" - Current time of day
- **Weather**: "Clear" - Current weather state
- **Position**: "X: 0.0, Y: 50.0, Z: 0.0" - Player coordinates
- **Chunk**: "(32, 32)" - Current terrain chunk
- **LOD**: "LOD0" - Current chunk's LOD level
- **FPS**: "60.0" - Frame rate
- **VRAM**: "128.5 / 256.0 MB" - Vertex buffer usage
- **Loader**: "Ready" - Asset streaming status

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
| T1.4 | Verify Terrain folder | Contains .tif files and .zmesh files |
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
