# Marble Roll Example Game

A physics-based ball rolling game demonstrating Jolt Physics integration and dynamic gameplay.

## Engine Features Demonstrated

| Feature | Engine Class | Usage |
|---------|--------------|-------|
| **Physics Integration** | `Zenith_Physics`, `Zenith_ColliderComponent` | Jolt Physics with dynamic/static bodies |
| **Collision Shapes** | `COLLISION_VOLUME_TYPE_SPHERE/AABB` | Sphere for ball, AABB for platforms |
| **Impulse-Based Movement** | `Zenith_Physics::AddImpulse` | Physics-based input response |
| **Camera Following** | `Zenith_CameraComponent` | Smooth follow with look-at |
| **Procedural Geometry** | `Flux_MeshGeometry::GenerateUVSphere` | Runtime sphere mesh generation |
| **Game State Machine** | `MarbleGameState` enum | Playing/Paused/Won/Lost states |
| **Distance-Based Pickups** | Manual distance check | Simple collectible system |
| **Entity Lifetime** | `Zenith_Scene::Destroy` | Dynamic entity creation/destruction |

## File Structure

```
Games/Marble/
  CLAUDE.md                      # This documentation
  Marble.cpp                     # Project entry points, resource initialization
  Components/
    Marble_Behaviour.h           # Main coordinator (uses modules below)
    Marble_Config.h              # DataAsset for game configuration
    Marble_Input.h               # Camera-relative physics input
    Marble_PhysicsController.h   # Ball movement via Jolt Physics
    Marble_CameraFollow.h        # Smooth camera tracking
    Marble_LevelGenerator.h      # Platform and collectible placement
    Marble_CollectibleSystem.h   # Pickup detection and scoring
    Marble_UIManager.h           # HUD management
  Assets/
    Scenes/Marble.zscn           # Serialized scene
```

## Module Breakdown

### Marble.cpp - Entry Points
**Engine APIs:** `Project_Initialise`, `Project_RegisterScriptBehaviours`, `Project_CreateScene`

Demonstrates:
- Procedural UV sphere generation with tangent calculation
- Multiple material creation for game objects
- Prefab creation for ball, platforms, collectibles, goal

### Marble_Behaviour.h - Main Coordinator
**Engine APIs:** `Zenith_ScriptBehaviour`, lifecycle hooks

Demonstrates:
- Game state machine (enum-based states)
- Coordinator pattern (delegates to modules)
- Time-based gameplay (countdown timer)

### Marble_Input.h - Camera-Relative Input
**Engine APIs:** `Zenith_Input::IsKeyHeld`, `Zenith_CameraComponent`

Demonstrates:
- Continuous input with IsKeyHeld (vs WasKeyPressedThisFrame)
- Camera-relative movement direction calculation
- Projecting camera forward onto XZ plane

### Marble_PhysicsController.h - Physics Movement
**Engine APIs:** `Zenith_Physics`, `Zenith_ColliderComponent`, `JPH::BodyID`

Demonstrates:
- Getting/setting physics body velocity
- Applying impulses for movement
- Jump mechanics with velocity check (prevents double-jump)
- Fall detection via position check

### Marble_CameraFollow.h - Camera System
**Engine APIs:** `Zenith_CameraComponent::SetPosition/SetPitch/SetYaw`

Demonstrates:
- Smooth follow with linear interpolation (glm::mix)
- Look-at calculation from pitch/yaw angles
- Fixed offset behind target

### Marble_LevelGenerator.h - Level Creation
**Engine APIs:** `Zenith_Scene::Instantiate`, `Zenith_TransformComponent`, `Zenith_ColliderComponent`

Demonstrates:
- Prefab-based entity creation
- Component order matters: Transform -> Model -> Collider
- Procedural platform placement in circular pattern
- Random distribution with std::uniform_real_distribution

### Marble_CollectibleSystem.h - Pickup System
**Engine APIs:** `Zenith_Scene::Destroy`

Demonstrates:
- Distance-based collision detection (no physics callbacks needed)
- Entity destruction on collection
- Score and win condition tracking

### Marble_UIManager.h - HUD Updates
**Engine APIs:** `Zenith_UIComponent`, `Zenith_UIText`

Demonstrates:
- Dynamic text with formatting
- Color changes based on game state
- Multiple UI elements (Score, Time, Collected, Status)

## Learning Path

1. **Start here:** `Marble.cpp` - See UV sphere generation
2. **Understand physics:** `Marble_PhysicsController.h` - Jolt integration
3. **See camera:** `Marble_CameraFollow.h` - Smooth follow pattern
4. **Level creation:** `Marble_LevelGenerator.h` - Prefab instantiation + physics
5. **Gameplay:** `Marble_CollectibleSystem.h` - Simple distance-based pickups

## Controls

| Key | Action |
|-----|--------|
| W / Up Arrow | Roll forward (camera-relative) |
| S / Down Arrow | Roll backward |
| A / Left Arrow | Roll left |
| D / Right Arrow | Roll right |
| Space | Jump |
| P / Escape | Pause/Unpause |
| R | Reset level |

## Key Patterns

### Physics-Based Movement
```cpp
// Get physics body from collider component
Zenith_ColliderComponent& xCollider = xBall.GetComponent<Zenith_ColliderComponent>();
const JPH::BodyID& xBodyID = xCollider.GetBodyID();

// Apply impulse for movement
Zenith_Maths::Vector3 xForce = glm::normalize(xDirection) * s_fMoveSpeed;
Zenith_Physics::AddImpulse(xBodyID, xForce);

// Check velocity for jump gating
Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xBodyID);
if (xVel.y < 1.0f) { /* Allow jump */ }
```

### Component Order for Physics
```cpp
// 1. Create entity from prefab (gets TransformComponent)
Zenith_Entity xEntity = Zenith_Scene::Instantiate(*pxPrefab, "Name");

// 2. Set position/scale on transform FIRST
Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
xTransform.SetPosition(xPos);
xTransform.SetScale(xScale);

// 3. Add model component (rendering)
Zenith_ModelComponent& xModel = xEntity.AddComponent<Zenith_ModelComponent>();
xModel.AddMeshEntry(*pxMesh, *pxMaterial);

// 4. Add collider LAST (physics body reads from transform)
xEntity.AddComponent<Zenith_ColliderComponent>()
    .AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
```

### Camera-Relative Input Direction
```cpp
// Get direction from camera to target
Zenith_Maths::Vector3 xToBall = xBallPos - xCamPos;
xToBall.y = 0.0f;  // Project onto XZ plane
xToBall = glm::normalize(xToBall);

// Forward is toward ball, right is perpendicular
Zenith_Maths::Vector3 xForward = xToBall;
Zenith_Maths::Vector3 xRight = glm::cross(Zenith_Maths::Vector3(0,1,0), xForward);
```

## Editor View (What You See on Boot)

When launching in a tools build (`vs2022_Debug_Win64_True`):

### Scene Hierarchy
- **MainCamera** - Perspective camera following the player ball
- **MarbleGame** - Main game entity with UIComponent and ScriptComponent (Marble_Behaviour)
- **PlayerBall** - The player-controlled marble with physics
- **StartPlatform** - Initial platform where player spawns
- **GoalPlatform** - Final destination platform
- **Platform_X** - Multiple floating platform entities
- **Collectible_X** - Coin/gem pickup entities scattered on platforms

### Viewport
- **Third-person perspective** view behind and above the marble
- **Blue sphere** (player marble) on the starting platform
- **Gray rectangular platforms** floating in space
- **Yellow/gold spheres** as collectible items on platforms
- **Green platform** marking the goal destination
- **Dark background** representing the void below

### Properties Panel (when MarbleGame selected)
- **Player section** - Move speed, jump force, physics settings
- **Camera section** - Follow distance, smoothing speed
- **Timer section** - Countdown duration
- **Debug section** - FPS, physics body count

### Console Output on Boot
```
[Marble] Initializing physics world
[Marble] Created X platforms, Y collectibles
[Marble] Player ball spawned at start position
```

## Gameplay View (What You See When Playing)

### Initial State
- Player marble on the starting platform
- Camera positioned behind and above the ball
- Timer counting down from 60 seconds (or configured time)
- Collectibles visible on nearby platforms

### HUD Elements (Top-Right)
- **"Score: 0"** - Current score from collectibles
- **"Time: 60"** - Countdown timer in seconds
- **"Collected: 0/10"** - Collectibles gathered vs total

### Gameplay Actions
1. **Rolling**: Ball responds to physics impulses from input
2. **Jumping**: Ball launches upward when Space pressed (if grounded)
3. **Collecting**: Approaching a collectible within range destroys it and adds score
4. **Winning**: Reaching the goal platform shows "You Win!" message
5. **Losing**: Falling off platforms or timer reaching 0 shows "Game Over"

### Visual Feedback
- Ball rolls realistically with physics
- Camera smoothly follows ball movement
- Collectibles spin/rotate to attract attention
- Platforms have distinct visual style from void

## Test Plan

### T1: Boot and Initialization
| Step | Action | Expected Result |
|------|--------|-----------------|
| T1.1 | Launch marble.exe | Window opens with third-person view of ball on platform |
| T1.2 | Check physics initialization | Ball rests on platform without falling through |
| T1.3 | Verify camera position | Camera positioned behind and above the ball |
| T1.4 | Check HUD | Score, Time, and Collected counters visible |

### T2: Physics Movement
| Step | Action | Expected Result |
|------|--------|-----------------|
| T2.1 | Press W | Ball rolls forward (away from camera) |
| T2.2 | Press S | Ball rolls backward (toward camera) |
| T2.3 | Press A | Ball rolls left relative to camera |
| T2.4 | Press D | Ball rolls right relative to camera |
| T2.5 | Release all keys | Ball gradually slows due to friction |
| T2.6 | Roll to platform edge | Ball can fall off into void |

### T3: Jump Mechanics
| Step | Action | Expected Result |
|------|--------|-----------------|
| T3.1 | Press Space on platform | Ball jumps upward |
| T3.2 | Press Space while in air | No additional jump (prevents double-jump) |
| T3.3 | Land on another platform | Ball can jump again |
| T3.4 | Jump across gap | Ball can cross to adjacent platform |

### T4: Collectible System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T4.1 | Roll near collectible | Collectible disappears when within range |
| T4.2 | Check score after collect | Score increments appropriately |
| T4.3 | Check collected counter | "Collected: X/Y" updates |
| T4.4 | Collect all items | Counter shows all collected |

### T5: Win/Lose Conditions
| Step | Action | Expected Result |
|------|--------|-----------------|
| T5.1 | Roll onto goal platform | "You Win!" message appears |
| T5.2 | Fall off platforms | "Game Over" after falling far enough |
| T5.3 | Let timer reach 0 | "Game Over - Time's Up!" message |
| T5.4 | Press R after game end | Level resets to initial state |

### T6: Camera System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T6.1 | Move ball around | Camera follows smoothly behind |
| T6.2 | Jump high | Camera adjusts to keep ball in view |
| T6.3 | Move quickly | Camera catches up without jarring motion |

### T7: Pause System
| Step | Action | Expected Result |
|------|--------|-----------------|
| T7.1 | Press P or Escape | Game pauses, "PAUSED" displayed |
| T7.2 | Move while paused | Ball does not respond to input |
| T7.3 | Timer while paused | Timer does not decrement |
| T7.4 | Press P again | Game resumes from paused state |

### T8: Edge Cases
| Step | Action | Expected Result |
|------|--------|-----------------|
| T8.1 | Roll very fast | Physics remains stable |
| T8.2 | Collide with platform edge | Ball bounces or stops correctly |
| T8.3 | Jump at platform edge | Ball can land back on same platform |
| T8.4 | Minimize/restore window | Game resumes correctly |

## Building

```batch
cd Build
msbuild zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64
cd ..\Games\Marble\Build\output\win64\vs2022_debug_win64_true
marble.exe
```
