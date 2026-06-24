# Prefab System

## Overview

Reusable entity templates that can be instantiated into scenes. Supports a variant system where derived prefabs inherit from a base and only store property overrides.

## Files

- `Zenith_Prefab.h/cpp` - Prefab class and PropertyOverride struct

## Architecture

### Zenith_Prefab
Inherits from `Zenith_Asset`, managed by `Zenith_AssetRegistry` via `PrefabHandle`.

**Creation:**
- `CreateFromEntity(xEntity, strPrefabName)` - Serializes an existing entity's components into prefab data
- `CreateAsVariant(xBasePrefab, strVariantName)` - Creates a variant that inherits from a base prefab (rejects bases that would form a variant cycle)

**Instantiation:**
- `Instantiate(pxSceneData, strEntityName, xPosition, xRotation, xScale)` - Creates a new entity in the scene with the given transform (position/rotation/scale default to origin/identity/(1,1,1)). Unity-style: the transform is applied to the entity BEFORE its lifecycle (`OnAwake`/`OnEnable`) runs, so a baked collider's physics body is built at the instance transform. For variants, per-property overrides apply on top of this transform.
- `ApplyToEntity(xEntity)` - Applies prefab component data to an existing entity

**Persistence:**
- `SaveToFile(strPath)` - Saves to `.zprfb` binary file
- Loading via `Zenith_AssetRegistry::Get<Zenith_Prefab>(path)`

### Variant System
- Variants store a `PrefabHandle` to their base prefab plus a list of `Zenith_PropertyOverride`
- Each override tracks: component name, property path, and serialized value (as `Zenith_DataStream`)
- `IsVariant()` returns true if the prefab has a base prefab set
- `ClearOverrides()` reverts variant to match base exactly
- `AddOverride(xOverride)` / `GetOverrides()` add and read the override list; `GetBasePrefab()` returns the base handle
- `GetName()` / `IsValid()` expose prefab identity and load state
- `WouldFormVariantCycle(xProposedBase)` guards `CreateAsVariant`: it walks the proposed base's ancestor chain (bounded at depth 64, treating overflow as a cycle) and uses the side-effect-free `TryGetLoadedPrefab` so cycle checks never trigger asset loads
- Override property paths must be flat: nested paths (containing `.`) are not yet supported and are skipped with a warning during instantiation/apply

### Instantiation Internals
- `InstantiateInternal` - private recursive variant of `Instantiate` that walks the base prefab chain (with a per-call visited-set cycle guard) and applies each level's overrides on top; the non-variant leaf creates the entity, deserializes components, and applies the spawn transform
- `Zenith_PrefabInstantiationGuard` - RAII guard `Instantiate` holds across the recursive chain to suppress per-entity lifecycle dispatch in the entity constructor; `OnAwake`/`OnEnable` are dispatched once at the top level after recursion completes

### Binary Format
- Magic number: `0x5A505242` ("ZPRB")
- Version: 3 (current)
- Layout: header (magic, version, name), variant flag; if variant, the base prefab handle; override count followed by each `Zenith_PropertyOverride`; then component data (size + bytes) for non-variants only
- Component data serialized via `Zenith_ComponentMetaRegistry`

## Key Patterns

- **Non-copyable, movable** - Supports component pool swap-and-pop
- **Component serialization** uses the ComponentMetaRegistry to handle polymorphic component types
- **Asset lifecycle** managed through `PrefabHandle` reference counting
