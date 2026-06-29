# Asset Handling System

## Overview

The asset handling system manages the import, export, and runtime loading of game assets. It uses Assimp for importing from standard formats (FBX, glTF, OBJ) and exports to Zenith's binary formats for optimized runtime loading.

## Asset Registry (Zenith_AssetRegistry)

The **unified asset management system** for all asset types. This singleton provides:

- **Single unified cache** for all asset types (textures, materials, meshes, models, etc.)
- **Path-based identification** - assets identified by file path strings
- **Reference counting** with automatic cleanup
- **Support for procedural assets** (code-created assets with generated paths)
- **Thread-safe operations**

### Usage

```cpp
// Load asset from file (returns cached if already loaded)
Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get<Zenith_TextureAsset>("game:Textures/tex.ztxtr");

// Create procedural asset (generates unique path like "procedural://texture_0")
Zenith_TextureAsset* pProc = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();

// Cleanup unused assets
Zenith_AssetRegistry::UnloadUnused();  // Free assets with ref count 0
```

### Initialization Order

The asset registry has two-phase initialization to handle GPU-dependent assets:

```cpp
// In main():
Zenith_AssetRegistry::Initialize();        // Call early, before Flux

// ... Flux::EarlyInitialise() ...

{
    Flux_PerFrame::BeginFrame();
    Zenith_AssetRegistry::InitializeGPUDependentAssets();  // After VMA is ready
    Flux_PerFrame::EndFrame();
}
```

### Type Aliases

```cpp
using TextureHandle = Zenith_AssetHandle<Zenith_TextureAsset>;
using MaterialHandle = Zenith_AssetHandle<Zenith_MaterialAsset>;
using MeshHandle = Zenith_AssetHandle<Zenith_MeshAsset>;
using SkeletonHandle = Zenith_AssetHandle<Zenith_SkeletonAsset>;
using ModelHandle = Zenith_AssetHandle<Zenith_ModelAsset>;
using AnimationHandle = Zenith_AssetHandle<Zenith_AnimationAsset>;
using MeshGeometryHandle = Zenith_AssetHandle<Zenith_MeshGeometryAsset>;
using FontHandle = Zenith_AssetHandle<Zenith_FontAsset>;
using PrefabHandle = Zenith_AssetHandle<Zenith_Prefab>;
```

## Texture Assets (Zenith_TextureAsset)

Texture assets contain GPU texture data and metadata:

```cpp
class Zenith_TextureAsset : public Zenith_Asset
{
    Flux_SurfaceInfo m_xSurfaceInfo;  // Format, dimensions, mip count
    Flux_VRAMHandle m_xVRAMHandle;     // GPU memory handle
    Flux_ShaderResourceView m_xSRV;    // For shader binding
};
```

### Loading Textures

```cpp
// Preferred: resolve the handle directly. Lazy-loads on first call and caches.
Zenith_TextureAsset* pTex = handle.Resolve();

// Equivalent explicit form (still supported, but more verbose):
Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(handle.GetPath());
```

Asset handles (`TextureHandle`, `MaterialHandle`, etc.) store paths and manage ref-counting.

- **`handle.Resolve()`** — default accessor for file-based handles. Returns the cached pointer if loaded; otherwise calls into `Zenith_AssetRegistry`, caches, and returns. Returns `nullptr` if path is empty and no procedural pointer was set, or if the load fails. Use this in game / component code unless you specifically need one of the alternatives below.
- **`handle.GetDirect()`** — for procedural assets created via `Get().Create<T>()` and stored with `handle.Set(ptr)`. Returns the stored pointer without going through the registry. Returns `nullptr` for file-based handles that have not been loaded yet.
- **`Zenith_AssetRegistry::Get<T>(handle.GetPath())`** — explicit static form. Equivalent to `Resolve()` but verbose; use when you want the registry call to be obvious at the call site.

## Material Assets (Zenith_MaterialAsset)

Materials store textures and rendering properties.

### Creating Materials

```cpp
// Create via registry
Zenith_MaterialAsset* pMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

// Set textures via TextureHandle. The handle stores either a path
// (file-backed, lazy-loaded via the registry) or a direct pointer
// (procedural, set via TextureHandle::Set or the T* constructor).
pMat->SetDiffuseTexture(TextureHandle("Assets/diffuse.ztxtr"));
pMat->SetNormalTexture(TextureHandle("Assets/normal.ztxtr"));

// Procedural textures use the T* constructor:
Zenith_TextureAsset* pProcTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
// ...populate pProcTex via CreateFromData...
pMat->SetDiffuseTexture(TextureHandle(pProcTex));
```

### Texture Slots

Materials expose nine texture slots via the `MaterialTextureSlot` enum (`Zenith_MaterialParamTable.h`): `MATERIAL_TEXTURE_BASE_COLOR`, `MATERIAL_TEXTURE_NORMAL`, `MATERIAL_TEXTURE_ROUGHNESS_METALLIC`, `MATERIAL_TEXTURE_OCCLUSION`, `MATERIAL_TEXTURE_EMISSIVE`, `MATERIAL_TEXTURE_HEIGHT`, `MATERIAL_TEXTURE_DETAIL_ALBEDO`, `MATERIAL_TEXTURE_DETAIL_NORMAL`, `MATERIAL_TEXTURE_DETAIL_MASK`. The `SetDiffuseTexture`/`SetNormalTexture`-style accessors are legacy wrappers over the base-colour and normal slots.

### Default Textures

Materials use default textures when slots are unset:
- `s_xDefaultWhite` - White 1x1 texture
- `s_xDefaultNormal` - Flat normal (128, 128, 255)

These are initialized by `Zenith_AssetRegistry::InitializeGPUDependentAssets()`.

## Asset Types

| Asset | Extension | Description |
|-------|-----------|-------------|
| Texture | `.ztxtr` | GPU texture data and metadata |
| Material | `.zmtrl` | Texture references and rendering properties |
| Model | `.zmodel` | Container referencing meshes, skeleton, and materials |
| Mesh | `.zmesh` | Geometry data with optional skinning weights |
| Skeleton | `.zskel` | Bone hierarchy and bind pose data |
| Animation | `.zanim` | Keyframe animation clips |
| Behaviour Graph | `.bgraph` | Designer-authored visual-scripting graph (see below) |

## Behaviour Graph Assets (Zenith_BehaviourGraphAsset)

`Zenith_BehaviourGraphAsset.{h,cpp}` wraps a serialized `Zenith_GraphDefinition`
(the Behaviour Graph blueprint — variables, nodes, edges, editor layout; the
runtime lives in `Zenith/Scripting/`, see its CLAUDE.md). Surface:

- `GetDefinition()` — the owned `Zenith_GraphDefinition`.
- `LoadedOk()` — false when deserialization failed (the consumer keeps the
  slot unresolved rather than crashing; see `Zenith_GraphComponent`).
- Registered via `ZENITH_REGISTER_ASSET_TYPE(Zenith_BehaviourGraphAsset)` at
  file scope. Because nothing else references the TU, dead-strip safety is
  anchored by `Zenith_BehaviourGraphAsset_ForceLink()` called from
  `Zenith_GraphComponent::InstantiateSlotGraph()`.
- Path convention: `"game:Graphs/Foo.bgraph"` (`game:` resolves to
  `GAME_ASSETS_DIR`; extension constant `ZENITH_BGRAPH_EXT`). Games regenerate
  their graphs every tools boot through `Zenith_EditorAutomation` graph steps,
  exactly like `.zscen` scenes.

## Export Pipeline

The export pipeline is in `Tools/Zenith_Tools_MeshExport.cpp`. It processes source files through these stages:

### 1. Scene Loading
Assimp loads the source file with these post-processing flags:
- `aiProcess_CalcTangentSpace` - Generate tangent vectors
- `aiProcess_LimitBoneWeights` - Limit to 4 bones per vertex
- `aiProcess_Triangulate` - Convert all faces to triangles
- `aiProcess_FlipUVs` - Flip V coordinate for Vulkan

### 2. Mesh Export
Each mesh in the scene graph is processed individually:

**Vertex Transform Baking:** All mesh vertices are transformed by their scene graph node's world transform. This "bakes" the mesh node's position/rotation/scale into the vertex positions, putting vertices in world space at bind pose.

**Inverse Bind Pose Adjustment:** For skinned meshes, the inverse bind pose (Assimp's `mOffsetMatrix`) is adjusted to compensate for the baked vertex transforms:
```
adjustedInvBindPose = originalInvBindPose * inverse(meshNodeWorldTransform)
```
This ensures the skinning equation still produces correct results with world-space vertices.

**Normal Matrix:** Normals, tangents, and bitangents are transformed using the inverse-transpose of the mesh node's 3x3 rotation/scale matrix to handle non-uniform scaling correctly.

**Skinning Data:** Up to 4 bone influences per vertex, with weights normalized to sum to 1.0.

### 3. Skeleton Export
The skeleton is extracted from the Assimp scene graph:

**Two-Pass Approach:** First pass collects bone data from the scene graph, second pass adds bones in mesh bone index order. This ensures skeleton bone indices match the indices stored in mesh vertex skinning data.

**Bone Data:** Each bone stores:
- Local TRS (position, rotation, scale) relative to scene graph parent
- Inverse bind pose matrix (adjusted for baked mesh transforms)
- Parent bone index (-1 for roots)

**Non-Bone Ancestors:** Nodes like "Armature" that exist in the scene graph but aren't actual bones are skipped. Only nodes referenced by mesh skinning data become skeleton bones.

**Bind Pose Computation:** After all bones are added, `ComputeBindPoseMatrices()` walks the bone hierarchy to compute world-space bind pose matrices from the local TRS values.

### 4. Animation Export
Animations are extracted from `aiAnimation` structures:

**Bone Channels:** Each animated bone has separate keyframe arrays for position, rotation, and scale. Keyframes store time and value.

**Animation Duration:** Stored in seconds, computed from the maximum keyframe time.

**Node Hierarchy Preservation:** Animation keyframes are relative to scene graph parents, matching how bones store their local TRS values.

## Runtime Loading

### Skeleton Instance (`Flux_SkeletonInstance`)
Runtime representation of an animated skeleton:

**Initialization:** Copies bind pose TRS values from the skeleton asset. Sets local transforms to bind pose, then computes skinning matrices.

**Skinning Matrix Computation:** For each bone:
```
modelSpaceTransform = parentModelSpace * localTransformMatrix
skinningMatrix = modelSpaceTransform * inverseBindPose
```

**GPU skinning:** Runtime skinning matrices (computed by `Flux_SkeletonInstance`) are read CPU-side by the unified compute-skinning pre-pass, which skins vertices into a shared GPU arena consumed by the GPU-driven mesh path — there is no per-instance bone constant buffer.

### Mesh Instance (`Flux_MeshInstance`)
Three creation paths exist:

**Static Meshes:** `CreateFromAsset(mesh)` - Creates GPU buffers with vertex data as-is.

**Bind-Pose Static Meshes:** `CreateFromAsset(mesh, skeleton)` - Creates GPU buffers with the skeleton's bind pose applied to the mesh vertices.

**Skinned Meshes:** `CreateSkinnedFromAsset(mesh)` - Creates GPU buffers with bone indices and weights in vertex data for GPU skinning.

## Coordinate Space Summary

| Space | Description |
|-------|-------------|
| Mesh-Local | Original vertex positions from source file |
| World/Baked | Vertex positions after baking mesh node transform |
| Bone-Local | Position relative to bone's origin at bind pose |

## Key Relationships

**Vertex Skinning Equation:**
```
worldPos = skinningMatrix * meshLocalPos
        = modelSpaceTransform * inverseBindPose * meshLocalPos
```

**At Bind Pose:** If skeleton instance uses bind pose TRS values, `modelSpaceTransform * inverseBindPose` should produce the correct world positions for vertices.

## Known Limitations

- Maximum 100 bones per skeleton (`Zenith_SkeletonAsset::MAX_BONES`, matching shader's `g_xBones[100]` array size)
- Maximum 4 bone influences per vertex
- Blender exports with Armature nodes may have a ~90 degree rotation offset due to Z-up to Y-up conversion that isn't fully compensated in the current pipeline

## Creating Serializable Asset Types

The asset system supports serializable "data assets" (game configs, particle emitter configs, etc.) that can be saved/loaded from `.zdata` files. These inherit directly from `Zenith_Asset`.

### 1. Define the Class

Inherit from `Zenith_Asset`, add `ZENITH_ASSET_TYPE_NAME(ClassName)` macro, implement `WriteToDataStream()`/`ReadFromDataStream()` for serialization (with versioning), optionally `RenderPropertiesPanel()` under `#ifdef ZENITH_TOOLS`. Register with `ZENITH_REGISTER_ASSET_TYPE(ClassName)` at file scope for static initialization.

### 2. Use the Asset

Load via `Zenith_AssetRegistry::Get<MyConfig>("game:path.zdata")` (returns cached if loaded), create programmatically via `Create<MyConfig>()`, save with `Save(pxAsset, "game:path.zdata")`.

### Path Prefixes

Assets use prefixed paths for cross-machine portability:

| Prefix | Resolves To | Example |
|--------|-------------|---------|
| `game:` | `GAME_ASSETS_DIR` | `game:Textures/diffuse.ztxtr` |
| `engine:` | `ENGINE_ASSETS_DIR` | `engine:Materials/default.zmtrl` |
| `procedural://` | Runtime-created asset | `procedural://unit_cube` |

### .zdata File Format

Binary format for serializable assets:

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic number: `0x5441445A` ("ZDAT") |
| 4 | 4 | Version: `1` |
| 8 | N+1 | Type name (null-terminated string) |
| 8+N+1 | ... | Asset data (from `WriteToDataStream`) |

## File Structure

```
AssetHandling/
  Zenith_Asset.h/cpp          - Base asset class with ref counting and optional serialization
  Zenith_AssetRegistry.h/cpp  - Unified asset cache, loading, and saving
  Zenith_AssetHandle.h        - Smart handle template with automatic ref counting
  Zenith_TextureAsset.h/cpp   - Texture asset (GPU texture + metadata)
  Zenith_MaterialAsset.h/cpp  - Material properties + texture references
  Zenith_MeshAsset.h/cpp      - Mesh geometry container
  Zenith_SkeletonAsset.h/cpp  - Skeleton hierarchy and bind pose
  Zenith_ModelAsset.h/cpp     - Model container (meshes + skeleton + materials)
  Zenith_AnimationAsset.h/cpp - Animation clips
  Zenith_BehaviourGraphAsset.h/cpp - Behaviour Graph asset (wraps a serialized Zenith_GraphDefinition; .bgraph)
  Zenith_MeshGeometryAsset.h/cpp  - Wrapper for Flux_MeshGeometry
  Zenith_FontAsset.h/cpp          - Font asset (.zfont glyph metrics + atlas)
  Zenith_MaterialParamTable.h/cpp - Material parameter reflection table (names/types/ranges/groups)
  Zenith_PropertyTuning.h/cpp     - Live .ztune file bindings for reflected properties
  Zenith_Image.h/cpp              - Single-channel 32-bit float image container (tools/terrain)
  Zenith_AssetHandle.cpp          - Explicit handle template instantiations
  Zenith_FileWatcher.h/cpp        - File system change watching
```
