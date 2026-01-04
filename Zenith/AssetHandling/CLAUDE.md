# Asset Handling System

## Overview

The asset handling system manages the import, export, and runtime loading of 3D models and animations. It uses Assimp for importing from standard formats (FBX, glTF, OBJ) and exports to Zenith's binary formats for optimized runtime loading.

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

## File Structure

```
AssetHandling/
  Zenith_MeshAsset.h/cpp      - Mesh geometry container
  Zenith_SkeletonAsset.h/cpp  - Skeleton hierarchy and bind pose
  Zenith_ModelAsset.h/cpp     - Model container (meshes + skeleton + materials)
  Zenith_AssetHandler.h/cpp   - Asset loading and caching
```
