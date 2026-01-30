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
// Get singleton
auto& reg = Zenith_AssetRegistry::Get();

// Load asset from file (returns cached if already loaded)
Zenith_TextureAsset* pTex = reg.Get<Zenith_TextureAsset>("Assets/tex.ztex");

// Create procedural asset (generates unique path like "procedural://texture_0")
Zenith_TextureAsset* pProc = reg.Create<Zenith_TextureAsset>();

// Cleanup unused assets
reg.UnloadUnused();  // Free assets with ref count 0
```

### Initialization Order

The asset registry has two-phase initialization to handle GPU-dependent assets:

```cpp
// In main():
Zenith_AssetRegistry::Initialize();        // Call early, before Flux

// ... Flux::EarlyInitialise() ...

{
    Flux_MemoryManager::BeginFrame();
    Zenith_AssetRegistry::InitializeGPUDependentAssets();  // After VMA is ready
    Flux_MemoryManager::EndFrame(false);
}
```

### Type Aliases

```cpp
using TextureRef = Zenith_AssetRef<Zenith_TextureAsset>;
using MaterialRef = Zenith_AssetRef<Zenith_MaterialAsset>;
using MeshRef = Zenith_AssetRef<Flux_MeshGeometry>;
using ModelRef = Zenith_AssetRef<Zenith_ModelAsset>;
using PrefabRef = Zenith_AssetRef<Zenith_Prefab>;
```

## Texture Assets (Zenith_TextureAsset)

Texture assets contain GPU texture data and metadata:

```cpp
class Zenith_TextureAsset : public Zenith_Asset
{
    Flux_SurfaceInfo m_xSurfaceInfo;  // Format, dimensions, mip count
    Flux_VRAMHandle m_xVRAMHandle;     // GPU memory handle
    Flux_ShaderResourceView m_xSRV;    // For shader binding
    std::string m_strSourcePath;       // For serialization
};
```

### Loading Textures

```cpp
// Via registry (preferred)
Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(path);

// Via asset reference
TextureRef xTexRef;
xTexRef.SetFromPath("Assets/tex.ztex");
Zenith_TextureAsset* pTex = xTexRef.Get();
```

## Material Assets (Zenith_MaterialAsset)

Materials store textures and rendering properties.

### Creating Materials

```cpp
// Create via registry
Zenith_MaterialAsset* pMat = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();

// Set textures (stores path for serialization)
pMat->SetDiffuseWithPath("Assets/diffuse.ztex");
pMat->SetNormalWithPath("Assets/normal.ztex");
```

### Default Textures

Materials use default textures when slots are unset:
- `s_pxDefaultDiffuse` - White 1x1 texture
- `s_pxDefaultNormal` - Flat normal (128, 128, 255)

These are initialized by `Zenith_AssetRegistry::InitializeGPUDependentAssets()`.

## Asset Types

| Asset | Extension | Description |
|-------|-----------|-------------|
| Model | `.zmodel` | Container referencing meshes, skeleton, and materials |
| Mesh | `.zmesh` | Geometry data with optional skinning weights |
| Skeleton | `.zskel` | Bone hierarchy and bind pose data |
| Animation | `.zanim` | Keyframe animation clips |

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

**GPU Upload:** Skinning matrices are uploaded to a constant buffer for GPU skinning. Triple-buffered to match frame-in-flight count.

### Mesh Instance (`Flux_MeshInstance`)
Two creation paths exist:

**Static Meshes:** `CreateFromAsset(mesh)` - Creates GPU buffers with vertex data as-is.

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

- Maximum 100 bones per skeleton at runtime (Flux_SkeletonInstance limit, matches shader's bone array size)
- Maximum 4 bone influences per vertex
- Blender exports with Armature nodes may have a ~90 degree rotation offset due to Z-up to Y-up conversion that isn't fully compensated in the current pipeline

## Creating Serializable Asset Types

The asset system supports serializable "data assets" (game configs, particle emitter configs, etc.) that can be saved/loaded from `.zdata` files. These inherit directly from `Zenith_Asset`.

### 1. Define the Class

```cpp
// In MyConfig.h
#include "AssetHandling/Zenith_Asset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_DataStream.h"

class MyConfig : public Zenith_Asset
{
public:
    ZENITH_ASSET_TYPE_NAME(MyConfig)  // Required for .zdata loading

    // Data members
    float m_fValue = 1.0f;
    std::string m_strName = "Default";

    // Serialization (required for .zdata support)
    void WriteToDataStream(Zenith_DataStream& xStream) const override
    {
        uint32_t uVersion = 1;
        xStream << uVersion;
        xStream << m_fValue;
        xStream << m_strName;
    }

    void ReadFromDataStream(Zenith_DataStream& xStream) override
    {
        uint32_t uVersion;
        xStream >> uVersion;
        if (uVersion >= 1)
        {
            xStream >> m_fValue;
            xStream >> m_strName;
        }
    }

#ifdef ZENITH_TOOLS
    void RenderPropertiesPanel() override
    {
        ImGui::DragFloat("Value", &m_fValue);
    }
#endif
};

// Auto-register type (static initialization)
ZENITH_REGISTER_ASSET_TYPE(MyConfig)
```

### 2. Use the Asset

```cpp
// Load from file
MyConfig* pxConfig = Zenith_AssetRegistry::Get().Get<MyConfig>("game:Config/my.zdata");

// Create programmatically
MyConfig* pxNew = Zenith_AssetRegistry::Get().Create<MyConfig>();
pxNew->m_fValue = 2.0f;
Zenith_AssetRegistry::Get().Save(pxNew, "game:Config/new.zdata");
```

### Path Prefixes

Assets use prefixed paths for cross-machine portability:

| Prefix | Resolves To | Example |
|--------|-------------|---------|
| `game:` | `GAME_ASSETS_DIR` | `game:Textures/diffuse.ztex` |
| `engine:` | `ENGINE_ASSETS_DIR` | `engine:Materials/default.zmat` |
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
  Zenith_AssetHandle.h/cpp    - Smart handle template with automatic ref counting
  Zenith_TextureAsset.h/cpp   - Texture asset (GPU texture + metadata)
  Zenith_MaterialAsset.h/cpp  - Material properties + texture references
  Zenith_MeshAsset.h/cpp      - Mesh geometry container
  Zenith_SkeletonAsset.h/cpp  - Skeleton hierarchy and bind pose
  Zenith_ModelAsset.h/cpp     - Model container (meshes + skeleton + materials)
  Zenith_AnimationAsset.h/cpp - Animation clips
  Zenith_MeshGeometryAsset.h  - Wrapper for Flux_MeshGeometry
  Zenith_AsyncAssetLoader.h   - Background asset loading
```
