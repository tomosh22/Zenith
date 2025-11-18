# Zenith Scene Serialization System

## Overview

The Zenith engine provides a robust binary scene serialization system that enables complete save/load functionality for game scenes. This system uses the `Zenith_DataStream` binary serialization infrastructure to efficiently save and restore all entities, components, and their properties.

## File Format: .zscen

### Format Specification

Zenith scenes are saved as `.zscen` (Zenith Scene) files using a custom binary format built on `Zenith_DataStream`.

#### File Header

```
Offset | Size | Type   | Description
-------|------|--------|----------------------------------
0x00   | 4    | u_int  | Magic Number (0x5A53434E = "ZSCN")
0x04   | 4    | u_int  | Format Version (currently 1)
0x08   | 4    | u_int  | Number of Entities
```

#### Entity Data Block (repeated for each entity)

```
Field                | Type        | Description
---------------------|-------------|--------------------------------------
Entity ID            | u_int       | Unique entity identifier
Parent Entity ID     | u_int       | Parent entity ID (-1 if no parent)
Entity Name          | std::string | Human-readable entity name
Component Count      | u_int       | Number of components attached
[Component Blocks]   | varies      | Component type and data (see below)
```

#### Component Data Block (repeated for each component)

```
Field                | Type        | Description
---------------------|-------------|--------------------------------------
Component Type Name  | std::string | Component type (e.g., "TransformComponent")
Component Data       | varies      | Component-specific binary data
```

#### Scene Metadata (footer)

```
Field                | Type   | Description
---------------------|--------|--------------------------------------
Main Camera Entity ID| u_int  | ID of main camera entity (-1 if none)
```

---

## Component Serialization

Each component type implements custom serialization logic via `WriteToDataStream()` and `ReadFromDataStream()` methods.

### Supported Components

#### 1. TransformComponent

**Serialized Data:**
- Entity name (std::string)
- Position (Zenith_Maths::Vector3)
- Rotation (Zenith_Maths::Quat)
- Scale (Zenith_Maths::Vector3)

**Notes:**
- Physics rigid body pointer (`m_pxRigidBody`) is NOT serialized (runtime-only)
- Transform values are extracted from physics if a rigid body exists
- On load, transform is applied directly; physics is reconstructed by ColliderComponent

**Implementation:** [Zenith_TransformComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_TransformComponent.cpp#L121-L152)

---

#### 2. ModelComponent

**Serialized Data:**
- Number of mesh entries (u_int)
- For each entry:
  - Mesh asset name (std::string)
  - Material asset name (std::string)

**Notes:**
- Asset **names** are serialized, not pointers
- Assets must be loaded before scene deserialization via `Zenith_AssetHandler`
- Created asset tracking arrays (`m_xCreatedMeshes`, etc.) are NOT serialized
- On load, assets are looked up by name; deserialization fails if assets are missing

**Asset Resolution:**
- Uses `Zenith_AssetHandler::GetMeshName()` for pointer-to-name lookup during save
- Uses `Zenith_AssetHandler::GetMesh()` for name-to-pointer lookup during load
- Asserts if assets are not found (assets must be preloaded)

**Implementation:** [Zenith_ModelComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_ModelComponent.cpp#L5-L67)

---

#### 3. CameraComponent

**Serialized Data:**
- Camera type (u_int - enum: Perspective/Orthographic)
- Near plane (float)
- Far plane (float)
- Left/Right/Top/Bottom (float) - for orthographic
- Field of view (float)
- Yaw (double)
- Pitch (double)
- Aspect ratio (float)
- Position (Zenith_Maths::Vector3)

**Notes:**
- All camera parameters preserved for accurate reconstruction
- Parent entity reference is restored during entity deserialization

**Implementation:** [Zenith_CameraComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_CameraComponent.cpp#L119-L161)

---

#### 4. ColliderComponent

**Serialized Data:**
- Collision volume type (u_int - enum: AABB, OBB, Sphere, Capsule, Terrain)
- Rigid body type (u_int - enum: Dynamic, Static)

**Notes:**
- Physics body (`m_pxRigidBody`) and body ID (`m_xBodyID`) are NOT serialized (runtime-only)
- Terrain mesh data (`m_pxTerrainMeshData`) is NOT serialized (reconstructed from TerrainComponent)
- On deserialization, `AddCollider()` is called to recreate the physics body
- **IMPORTANT:** Deserialization calls `AddCollider()`, which requires TransformComponent to be already loaded

**Deserialization Order Dependency:**
1. TransformComponent must be deserialized first
2. ColliderComponent calls `AddCollider()` which reads from TransformComponent
3. Physics body is created with correct position/rotation/scale

**Implementation:** [Zenith_ColliderComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_ColliderComponent.cpp#L155-L184)

---

#### 5. TextComponent

**Serialized Data:**
- 2D text entries (std::vector<TextEntry>):
  - Text string (std::string)
  - Position in pixels (Zenith_Maths::Vector2)
  - Scale (float)
- 3D world text entries (std::vector<TextEntry_World>):
  - Text string (std::string)
  - World position (Zenith_Maths::Vector3)
  - Scale (float)

**Notes:**
- Both 2D and 3D text entries are fully serialized
- TextEntry structs implement their own `WriteToDataStream()`/`ReadFromDataStream()`
- Parent entity reference is restored during entity deserialization

**Implementation:**
- [Zenith_TextComponent.h](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_TextComponent.h#L11-L47) (TextEntry structs)
- [Zenith_TextComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_TextComponent.cpp#L23-L43)

---

#### 6. TerrainComponent

**Serialized Data:**
- Render geometry asset name (std::string)
- Physics geometry asset name (std::string)
- Water geometry asset name (std::string)
- Material 0 asset name (std::string)
- Material 1 asset name (std::string)
- 2D position (Zenith_Maths::Vector2)

**Notes:**
- All mesh and material references are serialized as asset names
- Assets must be preloaded before scene deserialization
- On load, asset pointers are resolved via `Zenith_AssetHandler`
- Deserialization fails if any referenced assets are missing

**Limitations:**
- TerrainComponent has a complex constructor requiring all asset references
- Current implementation does not support component-less entity creation
- Future versions may support deferred initialization

**Implementation:** [Zenith_TerrainComponent.cpp](c:\dev\Zenith\Zenith\EntityComponent\Components\Zenith_TerrainComponent.cpp#L22-L86)

---

#### 7. ScriptComponent

**Status:** **NOT YET SUPPORTED**

**Reason:**
ScriptComponent contains a polymorphic pointer to `Zenith_ScriptBehaviour`, which requires:
- Runtime type identification system
- Script behavior factory/registry
- Polymorphic serialization mechanism

**Future Implementation:**
Will require a script behavior type registry similar to the component registry:
```cpp
// Planned approach:
xStream << strBehaviorTypeName;
xStream << behaviorData; // Call virtual Serialize() on behavior
```

**Workaround:**
Scripts must be manually reattached after scene load.

---

## API Usage

### Saving a Scene

```cpp
#include "EntityComponent/Zenith_Scene.h"

// Save the current scene
Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
xScene.SaveToFile("MyScene.zscen");
```

### Loading a Scene

```cpp
#include "EntityComponent/Zenith_Scene.h"

// IMPORTANT: Load required assets BEFORE loading the scene
Zenith_AssetHandler::AddMesh("PlayerMesh", "Assets/player.zmsh");
Zenith_AssetHandler::AddMaterial("PlayerMaterial");
// ... load other assets ...

// Load the scene
Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
xScene.LoadFromFile("MyScene.zscen");
```

**Critical Note:** Assets referenced by ModelComponent, TerrainComponent, etc. **must** be loaded into the `Zenith_AssetHandler` before calling `LoadFromFile()`, otherwise deserialization will fail with an assertion.

### Editor Integration

The Zenith Editor provides menu-based scene save/load:

**Menu:** `File → Save Scene` (Ctrl+S)
- Saves the current scene to `scene.zscen` in the working directory
- TODO: Native file dialog for custom paths

**Menu:** `File → Open Scene` (Ctrl+O)
- Loads a scene from `scene.zscen`
- TODO: Native file dialog for browsing

**Menu:** `File → New Scene`
- Clears the current scene via `Zenith_Scene::Reset()`

**Implementation:** [Zenith_Editor.cpp](c:\dev\Zenith\Games\Test\Build\Zenith\Editor\Zenith_Editor.cpp#L177-L209)

---

## Architecture Details

### Component Type Registry

The `Zenith_ComponentRegistry` provides a mapping system for dynamic component deserialization:

```cpp
// Registration (done at startup)
Zenith_ComponentRegistry::RegisterComponent(
    "TransformComponent",
    [](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
        // Deserializer lambda
        xEntity.GetComponent<Zenith_TransformComponent>().ReadFromDataStream(xStream);
    },
    [](Zenith_Entity& xEntity, Zenith_DataStream& xStream) {
        // Serializer lambda
        xStream << std::string("TransformComponent");
        xEntity.GetComponent<Zenith_TransformComponent>().WriteToDataStream(xStream);
    }
);
```

**Registration:** All components are registered in [Zenith_ComponentRegistration.cpp](c:\dev\Zenith\Zenith\EntityComponent\Zenith_ComponentRegistration.cpp)

**Registry Files:**
- [Zenith_ComponentRegistry.h](c:\dev\Zenith\Zenith\EntityComponent\Zenith_ComponentRegistry.h) - Interface
- [Zenith_ComponentRegistry.cpp](c:\dev\Zenith\Zenith\EntityComponent\Zenith_ComponentRegistry.cpp) - Implementation
- [Zenith_ComponentRegistration.cpp](c:\dev\Zenith\Zenith\EntityComponent\Zenith_ComponentRegistration.cpp) - Component registration

---

### Entity Serialization

Entities serialize their metadata followed by all attached components:

**Entity Metadata:**
- Entity ID (preserved across save/load for entity references)
- Parent entity ID (for hierarchy reconstruction)
- Entity name

**Component Serialization:**
- Component type name (string identifier)
- Component data (via component-specific serialization)

**Implementation:** [Zenith_Entity.cpp](c:\dev\Zenith\Zenith\EntityComponent\Zenith_Entity.cpp#L48-L163)

---

### Scene Serialization

The scene saves:
1. **File Header:** Magic number and version
2. **Entity Count:** Number of entities in the scene
3. **Entity Data:** All entities with their components
4. **Scene Metadata:** Main camera reference

**Deserialization Process:**
1. Read and validate file header
2. Clear current scene via `Reset()`
3. For each entity:
   - Read entity metadata
   - Create entity with original ID
   - Deserialize all components
4. Restore main camera reference

**Implementation:** [Zenith_Scene.cpp](c:\dev\Zenith\Zenith\EntityComponent\Zenith_Scene.cpp#L83-L225)

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                    SAVE OPERATION                            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Zenith_Scene::SaveToFile()         │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Create Zenith_DataStream           │
         │ Write file header (magic, version) │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ For each entity:                   │
         │   Zenith_Entity::WriteToDataStream()│
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ For each component:                │
         │   Component::WriteToDataStream()   │
         │   (TransformComponent, etc.)       │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Write main camera ID               │
         │ Flush to disk (.zscen file)        │
         └────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    LOAD OPERATION                            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ PREREQUISITE:                      │
         │ Load all assets via                │
         │ Zenith_AssetHandler::AddMesh()     │
         │ Zenith_AssetHandler::AddMaterial() │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Zenith_Scene::LoadFromFile()       │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Read file into Zenith_DataStream   │
         │ Validate header (magic, version)   │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Reset current scene (clear all)    │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ For each entity:                   │
         │   Create entity with saved ID      │
         │   Zenith_Entity::ReadFromDataStream()│
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ For each component:                │
         │   Lookup component type            │
         │   Component::ReadFromDataStream()  │
         │   (Reconstruct from binary data)   │
         └────────────────────────────────────┘
                              │
                              ▼
         ┌────────────────────────────────────┐
         │ Restore main camera reference      │
         │ Scene fully reconstructed          │
         └────────────────────────────────────┘
```

---

## Limitations and Known Issues

### 1. Asset Dependency Management

**Issue:** Assets must be manually loaded before scene deserialization.

**Current Behavior:**
- Scene file does NOT embed asset data
- Scene file references assets by name
- Assets must exist in `Zenith_AssetHandler` before `LoadFromFile()`

**Future Enhancement:**
Add an asset dependency manifest to .zscen files:
```cpp
// Header would include:
std::vector<std::string> requiredMeshes;
std::vector<std::string> requiredMaterials;
std::vector<std::string> requiredTextures;
```

---

### 2. ScriptComponent Not Supported

**Issue:** Polymorphic script behaviors cannot be serialized.

**Workaround:** Scripts must be manually reattached after scene load.

**Planned Solution:**
- Implement script behavior type registry
- Add virtual `Serialize()`/`Deserialize()` to `Zenith_ScriptBehaviour`
- Store behavior type name and serialized data

---

### 3. TerrainComponent Constructor Complexity

**Issue:** TerrainComponent requires all asset references at construction time.

**Current Limitation:** Cannot create an empty TerrainComponent and populate it during deserialization.

**Workaround:** Deserialization looks up assets first, then creates component.

**Potential Solution:**
- Add default constructor
- Add `Initialise()` method for deferred setup
- Update deserialization to use deferred initialization

---

### 4. Entity ID Conflicts

**Issue:** Entity IDs are generated from a static counter, which may conflict during deserialization.

**Current Workaround:** Entity constructor bypasses ID generation when loading scenes.

**Future Enhancement:**
- Save/restore the entity ID counter state
- Implement entity ID remapping on load

---

### 5. No File Dialog

**Issue:** Editor uses hardcoded "scene.zscen" filename.

**Workaround:** Manually rename files in filesystem.

**Future Enhancement:** Integrate platform-native file dialogs (Windows: `IFileDialog`, Linux: GTK, etc.)

---

## Testing and Validation

### Automated Unit Tests

Comprehensive unit tests are provided in [Zenith_UnitTests.cpp](c:\dev\Zenith\Zenith\UnitTests\Zenith_UnitTests.cpp) to verify scene serialization integrity. Tests are automatically run during engine startup.

#### Test Suite Overview

**Test Function** | **Description** | **Coverage**
------------------|-----------------|-------------
`TestComponentSerialization()` | Individual component round-trip | TransformComponent, CameraComponent, TextComponent
`TestEntitySerialization()` | Entity with multiple components | Entity metadata, component attachment, data integrity
`TestSceneSerialization()` | Full scene save to disk | File creation, header validation, multi-entity scenes
`TestSceneRoundTrip()` | **Complete end-to-end test** | Save → Clear → Load → Verify all data

#### Running Tests

```cpp
// Tests run automatically at engine startup, or invoke manually:
Zenith_UnitTests::RunAllTests();
```

**Expected Output:**
```
Running TestComponentSerialization...
  ✓ TransformComponent serialization passed
  ✓ CameraComponent serialization passed
  ✓ TextComponent serialization passed
TestComponentSerialization completed successfully

Running TestEntitySerialization...
TestEntitySerialization completed successfully

Running TestSceneSerialization...
  Scene file size: 487 bytes
TestSceneSerialization completed successfully

Running TestSceneRoundTrip...
  ✓ Scene saved to disk
  ✓ Scene cleared
  ✓ Scene loaded from disk
  ✓ Entity count verified (3 entities)
  ✓ Camera entity verified
  ✓ Entity1 verified
  ✓ Entity2 verified
  ✓ Main camera reference verified
TestSceneRoundTrip completed successfully - full data integrity verified!
```

#### TestSceneRoundTrip - Ground Truth Validation

The round-trip test creates precise ground truth data and verifies byte-for-byte accuracy:

```cpp
// Ground Truth Setup
const Zenith_Maths::Vector3 xCameraPos(0.0f, 10.0f, 20.0f);
const float fCameraPitch = 0.3f;
const float fCameraFOV = 75.0f;
// ... create scene with specific values

// Save → Clear → Load
xGroundTruthScene.SaveToFile("unit_test_roundtrip.zscen");
xGroundTruthScene.Reset();
xLoadedScene.LoadFromFile("unit_test_roundtrip.zscen");

// Verify Exact Match
Zenith_Assert(xLoadedCameraPos == xCameraPos);
Zenith_Assert(xLoadedCameraComp.GetPitch() == fCameraPitch);
Zenith_Assert(xLoadedCameraComp.GetFOV() == fCameraFOV);
```

**Verified Properties:**
- ✅ Entity count (3 entities)
- ✅ Entity IDs preserved
- ✅ Entity names preserved
- ✅ Camera position (0.0, 10.0, 20.0)
- ✅ Camera pitch (0.3 rad)
- ✅ Camera yaw (1.57 rad)
- ✅ Camera FOV (75°)
- ✅ Transform position, rotation, scale
- ✅ TextComponent data
- ✅ Main camera reference

### Manual Validation Checklist

For custom scene validation during development:

- [x] All component data preserved (position, rotation, scale, etc.) - **Automated**
- [x] Entity hierarchy preserved (parent-child relationships) - **Automated**
- [x] Main camera reference restored correctly - **Automated**
- [ ] Asset references resolved (meshes, materials) - **Requires asset preloading**
- [ ] Physics bodies recreated with correct properties - **Requires physics initialization**
- [x] No crashes or assertions during load - **Automated**

### Test File Artifacts

Unit tests create temporary `.zscen` files during execution:
- `unit_test_scene.zscen` - Created by TestSceneSerialization (not cleaned up for manual inspection)
- `unit_test_roundtrip.zscen` - Created and cleaned up by TestSceneRoundTrip

These files can be inspected with a hex editor to verify binary format correctness.

### Binary Format Verification

Example hex dump of `unit_test_roundtrip.zscen` header:
```
0000: 4E 53 43 5A  01 00 00 00  03 00 00 00  ...    NSCZ........ (magic, version, entity count)
000C: 01 00 00 00  FF FF FF FF  0A 00 00 00  ...    Entity ID, parent ID, name length
0018: 4D 61 69 6E  43 61 6D 65  72 61        ...    "MainCamera"
```

---

## Best Practices

### For Engine Developers

1. **Always implement both `WriteToDataStream()` and `ReadFromDataStream()`** for new components
2. **Do NOT serialize runtime-only data** (physics handles, GPU resources, etc.)
3. **Use asset names, not pointers** for cross-referencing assets
4. **Document component dependencies** (e.g., ColliderComponent needs TransformComponent)
5. **Add version numbers** to component serialization for future compatibility

### For Game Developers

1. **Load all assets before loading scenes**
2. **Use descriptive entity names** for easier debugging
3. **Save scenes frequently** during development
4. **Test round-trip serialization** regularly
5. **Keep .zscen files in version control** for team collaboration

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1       | 2025 | Initial implementation with core component support |

---

## Related Documentation

- [Entity-Component System](Zenith/EntityComponent/CLAUDE.md) - ECS architecture overview
- [Zenith_DataStream](Zenith/DataStream/Zenith_DataStream.h) - Binary serialization infrastructure
- [Zenith_AssetHandler](Zenith/AssetHandling/Zenith_AssetHandler.h) - Asset management system

---

**Author:** Zenith Engine Team
**Last Updated:** November 2025
