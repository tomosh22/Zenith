# Prefab System

## Overview

Reusable entity templates that can be instantiated into scenes. Supports a variant system where derived prefabs inherit from a base and only store property overrides.

## Files

- `Zenith_Prefab.h/cpp` - Prefab class and PropertyOverride struct

## Architecture

### Zenith_Prefab
Inherits from `Zenith_Asset`, managed by `Zenith_AssetRegistry` via `PrefabHandle`.

**Creation:**
- `CreateFromEntity(xEntity, strName)` - Serializes an existing entity's components into prefab data
- `CreateAsVariant(xBasePrefab, strName)` - Creates a variant that inherits from a base prefab

**Instantiation:**
- `Instantiate(pxSceneData, strName)` - Creates a new entity in the scene from prefab data
- `ApplyToEntity(xEntity)` - Applies prefab component data to an existing entity

**Persistence:**
- `SaveToFile(strPath)` - Saves to `.zprfb` binary file
- Loading via `Zenith_AssetRegistry::Get().Get<Zenith_Prefab>(path)`

### Variant System
- Variants store a `PrefabHandle` to their base prefab plus a list of `Zenith_PropertyOverride`
- Each override tracks: component name, property path, and serialized value (as `Zenith_DataStream`)
- `IsVariant()` returns true if the prefab has a base prefab set
- `ClearOverrides()` reverts variant to match base exactly

### Binary Format
- Magic number: `0x5A505242` ("ZPRB")
- Version: 3 (current)
- Component data serialized via `Zenith_ComponentMetaRegistry`

## Key Patterns

- **Non-copyable, movable** - Supports component pool swap-and-pop
- **Component serialization** uses the ComponentMetaRegistry to handle polymorphic component types
- **Asset lifecycle** managed through `PrefabHandle` reference counting
