# Entity-Component System

## Files

### Core
- `Zenith_Scene.h/cpp` - Scene management, component pools, entity-component mapping
- `Zenith_Entity.h/cpp` - Entity wrapper class
- `Zenith_ComponentRegistry.h/cpp` - Component type registration for editor

### Components (in Components/ subdirectory)
- `Zenith_TransformComponent` - Position, rotation, scale
- `Zenith_CameraComponent` - View/projection matrices
- `Zenith_ModelComponent` - Renderable mesh with material
- `Zenith_ColliderComponent` - Physics collision shapes (Jolt Physics)
- `Zenith_TerrainComponent` - Heightmap-based terrain
- `Zenith_ScriptComponent` - Custom behavior attachment
- `Zenith_TextComponent` - 2D text rendering
- `Zenith_UIComponent` - UI element support

## Architecture

### Entity
Lightweight wrapper around scene pointer and entity ID. Provides template methods for adding, getting, checking, and removing components. Entity IDs are unsigned integers.

### Scene
Static singleton storing all entities and component pools. Components of same type stored in contiguous `Zenith_Vector` for cache-friendly iteration. Scene serialization uses `Zenith_DataStream` with `.zscn` file extension.

### Component Pools
Each component type has dedicated pool (vector) indexed by TypeID. Entity-component mapping uses hash map from entity ID to component indices per type.

## Script Components

Custom behaviors use factory pattern for serialization. Behaviors must:
1. Inherit from `Zenith_ScriptBehaviour`
2. Use `ZENITH_BEHAVIOUR_TYPE_NAME(ClassName)` macro
3. Register via `RegisterBehaviour()` before scene load

Callbacks: `OnCreate()`, `OnUpdate(float)`, `OnCollisionEnter(entity)`, `OnCollisionExit(entityID)`

## Key Concepts

**Type Safety:** Template methods ensure compile-time type checking for component access.

**Serialization:** All components implement `WriteToDataStream()` and `ReadFromDataStream()` for scene save/load.

**Thread Safety:** Scene modifications during rendering handled via deferred operations (see Editor/CLAUDE.md).

**Entity Lifecycle:** Entities can have parent-child relationships via `m_uParentEntityID` member.
