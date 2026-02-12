# Endless Runner Example Game

An infinite runner game demonstrating Animation State Machine and Terrain features.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Animation State Machine** | `Flux_AnimationStateMachine` | State machine with Idle, Run, Jump, Slide states |
| **BlendSpace1D** | `Flux_BlendTreeNode_BlendSpace1D` | Speed-based animation blending (walk -> jog -> sprint) |
| **Terrain System** | `Zenith_TerrainComponent` | GPU-driven terrain concepts (procedural chunks in demo) |
| **Particle System** | `Flux_Particles` | Dust trails and collection effects (entity-based in demo) |
| **Lane-Based Movement** | Custom controller | Mobile-style lane switching mechanics |
| **Prefab Instantiation** | `Zenith_Prefab`, `Zenith_Scene::Instantiate` | Runtime entity creation |
| **UI Text** | `Zenith_UIComponent`, `Zenith_UIText` | Distance, score, speed HUD |
| **DataAsset System** | `Runner_Config` | Configurable gameplay parameters |
| **Multi-Scene** | `Zenith_SceneManager` | `DontDestroyOnLoad()`, `CreateEmptyScene()`, `UnloadScene()`, `SetScenePaused()` |
| **Menu Buttons** | `Zenith_UIButton` | Clickable/tappable menu buttons with `SetOnClick()` callback |

## File Structure

```
Games/Runner/
  CLAUDE.md                      # This documentation
  Runner.cpp                     # Project entry points, resource initialization
  Components/
    Runner_Config.h              # DataAsset for game configuration
    Runner_Behaviour.h           # Main game coordinator
    Runner_CharacterController.h # Lane-based movement, jump, slide
    Runner_AnimationDriver.h     # Animation state machine control
    Runner_TerrainManager.h      # Terrain chunk management
    Runner_CollectibleSpawner.h  # Obstacles and collectibles
    Runner_ParticleManager.h     # Visual particle effects
    Runner_UIManager.h           # HUD management
  Assets/
    Scenes/Runner.zscen          # Serialized scene
```

## Module Breakdown

### Runner.cpp - Entry Points
**Engine APIs:** `Project_GetName`, `Project_RegisterScriptBehaviours`, `Project_LoadInitialScene`

Demonstrates:
- Procedural capsule geometry generation (for character)
- Procedural UV sphere generation (for collectibles)
- Multiple material creation
- Prefab creation for runtime instantiation

### Runner_Config.h - Game Configuration
**Engine APIs:** `Zenith_DataAsset`

Configurable parameters:
- Character movement (speed, jump, slide, lanes)
- Terrain generation (chunk size, active chunks)
- Obstacles and collectibles (spawn rates, sizes)
- Animation (BlendSpace parameters)
- Camera follow settings
- Particle emission rates

### Runner_CharacterController.h - Movement
**Engine APIs:** `Zenith_Input`, `Zenith_TransformComponent`

Demonstrates:
- Lane-based lateral movement (mobile-style)
- Smooth lane switching with easing
- Jump mechanics with gravity
- Slide mechanics with duration timer
- Terrain height following
- Character state machine (Running, Jumping, Sliding, Dead)

### Runner_AnimationDriver.h - Animation Control
**Engine APIs:** `Flux_AnimationStateMachine` concepts, `Flux_BlendTreeNode_BlendSpace1D` concepts

Demonstrates:
- Animation state machine with states: Idle, Run, Jump, Slide
- BlendSpace1D for speed-based run animation blending
- State transitions based on gameplay parameters
- Procedural animation simulation (for this capsule demo)

**Real implementation example:**
Create `BlendSpace1D`, add blend points at different speed thresholds (walk=0, jog=15, sprint=35), sort, then set parameter each frame.

### Runner_TerrainManager.h - Terrain System
**Engine APIs:** `Zenith_TerrainComponent` concepts

Demonstrates:
- Infinite scrolling terrain via procedural chunks
- Chunk spawning ahead of player
- Chunk despawning behind player
- Terrain height queries

**Real Zenith_TerrainComponent features:**
- GPU-driven LOD streaming (4 LOD levels)
- 4096 chunks (64x64 grid) with frustum culling
- Compute shader culling dispatch
- Indirect draw for visible chunks

### Runner_CollectibleSpawner.h - Spawning System
**Engine APIs:** `Zenith_Scene::Instantiate`, `Zenith_Scene::Destroy`

Demonstrates:
- Procedural obstacle and collectible spawning
- Lane-based placement
- Distance-based pickup detection
- AABB collision checking for obstacles
- Entity cleanup for passed objects

### Runner_ParticleManager.h - Particle Effects
**Engine APIs:** `Flux_Particles` concepts

Demonstrates:
- Dust trail spawning while running
- Collection burst effects
- Particle lifetime and velocity
- Fade out over lifetime

**Real Flux_Particles usage:**
Particles use GPU structs with position/radius/color, uploaded each frame via `Flux_MemoryManager::UploadBufferData()` to instance buffer.

### Runner_UIManager.h - HUD
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Dynamic text updates (distance, score, speed)
- Color changes based on game state and progress
- Milestone-based color (gold at 1000m)
- Game over and pause overlays
- Fading controls hint

## Multi-Scene Architecture

### Entity Layout
- **Persistent scene** (DontDestroyOnLoad): `GameManager` entity with Camera + UIComponent + ScriptComponent (Runner_Behaviour). Survives scene transitions.
- **Game scene** (`m_xGameScene`, named "Run"): Contains all level entities (player, terrain chunks, obstacles, collectibles, particles). Created on play, destroyed on return to menu.

### Game State Machine
```
MAIN_MENU --> PLAYING --> PAUSED --> PLAYING
                |            |
                v            v
            GAME_OVER    MAIN_MENU
                |
                v
            MAIN_MENU
```

### Scene Transition Pattern

Uses `CreateEmptyScene("Run")` + `SetActiveScene()` to start, `SetScenePaused()` for pause/resume, and `UnloadScene()` to return to menu.

## Learning Path

1. **Start here:** `Runner.cpp` - See capsule geometry generation
2. **Understand controls:** `Runner_CharacterController.h` - Lane-based movement
3. **Study animation:** `Runner_AnimationDriver.h` - State machine patterns
4. **See spawning:** `Runner_CollectibleSpawner.h` - Procedural placement
5. **Explore terrain:** `Runner_TerrainManager.h` - Infinite scrolling
6. **Effects:** `Runner_ParticleManager.h` - Visual feedback

## Controls

| Key | Action |
|-----|--------|
| A / Left Arrow | Move to left lane |
| D / Right Arrow | Move to right lane |
| Space / W / Up | Jump over obstacles |
| S / Down | Slide under obstacles |
| Click / Touch | Select menu button |
| W/S or Up/Down | Navigate menu |
| Enter | Activate focused button |
| Escape | Return to menu / Pause |
| P | Pause/Resume |
| R | Reset game |

## Key Patterns

### Animation State Machine Setup
Create `Flux_AnimationStateMachine`, add parameters (Float/Bool/Trigger), create states with blend trees, and configure transitions with conditions and durations.

### BlendSpace1D for Speed-Based Animation
Add clips at different parameter positions, sort blend points, then call `SetParameter()` and `Evaluate()` each frame for smooth blending.

### Lane-Based Movement
Calculate lane offset from lane index and width, use smoothstep interpolation for smooth lane switching.

### Terrain Height Query
For real terrain: `SampleHeightAt(fX, fZ)`. For this demo: simple chunk-based height lookup.

## Gameplay Mechanics

1. **Endless Forward Movement:** Character automatically runs forward at increasing speed
2. **Lane System:** 3 lanes (left, center, right) with smooth lane switching
3. **Jump:** Clear low obstacles, cancel slide
4. **Slide:** Pass under high obstacles, temporary speed boost
5. **Collectibles:** Coins spawn in lanes, collect for score
6. **Obstacles:** Jump or slide based on obstacle type
7. **Game Over:** Hit obstacle or fall off terrain

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **GameManager** - Persistent entity (Camera + UIComponent + ScriptComponent/Runner_Behaviour) - DontDestroyOnLoad
- *(Game scene "Run", created at runtime)*
  - **Player** - Character entity (capsule) in the center lane
  - **TerrainChunk_X** - Procedural terrain chunk entities ahead of player
  - **Obstacle_X** - Spawned obstacle entities in lanes
  - **Collectible_X** - Coin/gem pickup entities in lanes

### Viewport
- **Third-person perspective** view behind and above the character
- **Character** (capsule) running forward on the center lane
- **Gray terrain chunks** forming the ground ahead and behind
- **Red obstacles** (cubes of various heights) in lanes
- **Yellow/gold spheres** as collectible coins in lanes
- **Three visible lanes** with lane markers on terrain

### Properties Panel (when RunnerGame selected)
- **Movement section** - Base speed, max speed, acceleration, lane width
- **Jump section** - Jump force, gravity, max jump duration
- **Slide section** - Slide duration, speed boost multiplier
- **Terrain section** - Chunk size, visible chunk count
- **Spawning section** - Obstacle frequency, collectible frequency
- **Debug section** - FPS, entity count, current speed

### Console Output on Boot
```
[Runner] Initializing endless runner
[Runner] Created X terrain chunks
[Runner] Player spawned in center lane
[Runner] Animation driver initialized
```

## Gameplay View (What You See When Playing)

### Initial State
- Character in center lane, already running forward
- Camera following behind at fixed distance
- First few terrain chunks visible ahead
- Some obstacles and collectibles spawned in lanes
- HUD showing Distance: 0, Score: 0, Speed: 15

### HUD Elements (Top-Right)
- **"Distance: 0m"** - Distance traveled in meters
- **"Score: 0"** - Points from collected items
- **"Speed: 15"** - Current running speed

### HUD Elements (Center, Fading)
- **"A/D or ←/→: Switch Lanes"** - Control hint (fades after 3 seconds)
- **"Space: Jump, S: Slide"** - Control hint (fades after 3 seconds)

### Gameplay Actions
1. **Running**: Character automatically moves forward at increasing speed
2. **Lane Switch**: A/D or arrows smoothly move to adjacent lane
3. **Jumping**: Space/W/Up makes character jump over low obstacles
4. **Sliding**: S/Down makes character slide under high obstacles
5. **Collecting**: Running through collectibles adds to score
6. **Game Over**: Hitting obstacle shows "Game Over" with final stats

### Visual Feedback
- Character smoothly animates between lanes
- Jump has arc trajectory with gravity
- Slide lowers character temporarily
- Dust particles trail behind character while running
- Burst particles on collectible pickup
- Speed increases visible in HUD
- Terrain chunks spawn ahead, despawn behind
- Obstacles and collectibles appear procedurally

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch runner.exe | Window opens with third-person view of character on track |
| T1.2 | Check character state | Character running forward automatically |
| T1.3 | Check terrain | Terrain chunks visible ahead of character |
| T1.4 | Verify HUD | Distance, Score, Speed counters visible |
| T1.5 | Check control hints | Lane switch and jump/slide hints visible, fade after 3s |

### T2: Lane Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Press D or Right Arrow | Character smoothly moves to right lane |
| T2.2 | Press D again (from right lane) | Character stays in right lane (can't go further) |
| T2.3 | Press A or Left Arrow | Character moves to center lane |
| T2.4 | Press A twice quickly | Character moves to left lane |
| T2.5 | Press A (from left lane) | Character stays in left lane (can't go further) |
| T2.6 | Observe lane switch animation | Smooth easing transition, no teleporting |

### T3: Jump Mechanics
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Press Space | Character jumps with arc trajectory |
| T3.2 | Observe landing | Character lands smoothly on terrain |
| T3.3 | Press Space while in air | No double jump (only one jump per ground touch) |
| T3.4 | Jump over low obstacle | Character clears obstacle, continues running |
| T3.5 | Press W or Up Arrow | Same jump behavior as Space |

### T4: Slide Mechanics
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Press S or Down Arrow | Character slides (lowers height) |
| T4.2 | Check slide duration | Slide ends after configured duration |
| T4.3 | Slide under high obstacle | Character passes under, continues running |
| T4.4 | Press S while sliding | No effect (already sliding) |
| T4.5 | Press Space while sliding | Jump cancels slide |

### T5: Collectible System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Run through a collectible | Collectible disappears |
| T5.2 | Check score after collect | Score counter increments |
| T5.3 | Observe particle effect | Burst particles on collection |
| T5.4 | Collect multiple items | Score continues to increase |

### T6: Obstacle Collisions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Run into low obstacle (no jump) | "Game Over" message appears |
| T6.2 | Run into high obstacle (no slide) | "Game Over" message appears |
| T6.3 | Jump into low obstacle | Character clears obstacle |
| T6.4 | Slide under high obstacle | Character passes through |
| T6.5 | Check game over state | Distance and score displayed in final stats |

### T7: Speed Progression
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Note initial speed | Speed starts at configured base (e.g., 15) |
| T7.2 | Play for 30 seconds | Speed gradually increases |
| T7.3 | Check distance relation | Distance accumulates based on speed |
| T7.4 | Observe obstacle timing | Obstacles still avoidable at higher speeds |

### T8: Terrain System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Observe terrain ahead | New chunks spawn as character approaches |
| T8.2 | Observe terrain behind | Old chunks despawn after character passes |
| T8.3 | Run for extended time | No gaps in terrain, seamless infinite scroll |
| T8.4 | Check terrain height | Character follows terrain height correctly |

### T9: Pause System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T9.1 | Press P or Escape | Game pauses, "PAUSED" message displayed |
| T9.2 | Move while paused | No input processed |
| T9.3 | Check speed/distance while paused | Values frozen |
| T9.4 | Press P again | Game resumes from paused state |

### T10: Game Reset
| Step | Action | Expected Result |
|------|--------|-----------------|
| T10.1 | Hit obstacle (game over) | Game over screen displayed |
| T10.2 | Press R | Game resets to initial state |
| T10.3 | Check counters | Distance 0, Score 0, Speed reset |
| T10.4 | Check character position | Character back in center lane |
| T10.5 | Press R during gameplay | Level resets immediately |

### T11: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T11.1 | Rapidly switch lanes | Smooth interpolation, no stuck state |
| T11.2 | Jump and switch lane | Both actions combine correctly |
| T11.3 | Slide and switch lane | Both actions combine correctly |
| T11.4 | Hold lane key | Single lane switch per press |
| T11.5 | Minimize/restore window | Game resumes correctly |

### T12: HUD and Distance Milestone
| Step | Action | Expected Result |
|------|--------|-----------------|
| T12.1 | Run to 100m | Distance counter updates correctly |
| T12.2 | Run to 1000m | Distance text color changes to gold |
| T12.3 | Collect items during run | Score updates in real-time |
| T12.4 | Check speed display | Speed shows current value, updates as accelerates |

### T13: Menu Navigation
| Step | Action | Expected Result |
|------|--------|-----------------|
| T13.1 | Launch runner.exe | Main menu displayed with Play button |
| T13.2 | Click Play button | Game starts, menu hidden, HUD visible |
| T13.3 | Press Escape (while playing) | Returns to main menu, game scene unloaded |
| T13.4 | Use W/S or Up/Down arrows | Menu button focus navigates |
| T13.5 | Press Enter on focused button | Activates the focused button |

### T14: Scene Transitions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T14.1 | Click Play from menu | Game scene "Run" created, level entities spawned |
| T14.2 | Press Escape to return to menu | Game scene unloaded, menu reappears |
| T14.3 | Click Play again | Fresh game scene created, no stale state |
| T14.4 | Press R during gameplay | Game scene destroyed and recreated (restart) |
| T14.5 | Trigger game over, press R | New game scene, score reset to 0 |

### T15: Pause / Resume
| Step | Action | Expected Result |
|------|--------|-----------------|
| T15.1 | Press P during gameplay | Game pauses, "PAUSED" overlay shown |
| T15.2 | Press P again | Game resumes, overlay hidden |
| T15.3 | Press Escape while paused | Returns to main menu |
| T15.4 | Verify scene paused | Character, terrain, obstacles frozen while paused |

### T16: Editor Features (Tools Build Only)
| Step | Action | Expected Result |
|------|--------|-----------------|
| T16.1 | Select GameManager entity | Properties panel appears |
| T16.2 | Modify base speed | Speed changes affect next run |
| T16.3 | Modify lane width | Lane spacing changes |
| T16.4 | Modify obstacle frequency | Obstacle density changes |
| T16.5 | Check debug stats | FPS, entity count visible |

## Building

```batch
cd Build
Sharpmake_Build.bat
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Runner\Build\output\win64\vs2022_debug_win64_true
runner.exe
```

## Extension Ideas

1. **Real Animation:** Load skeletal meshes and animation clips
2. **Real Terrain:** Use Zenith_TerrainComponent with heightmaps
3. **Power-ups:** Shield, magnet, double points
4. **Difficulty Curve:** Increase obstacle density over time
5. **Multiple Characters:** Different meshes and animations
6. **Sound Effects:** Collection and obstacle hit sounds
