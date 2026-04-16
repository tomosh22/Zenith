# Shader Binding Convention

Documents the **current** descriptor set / binding layout across subsystems.
This is descriptive (documenting what exists), not prescriptive (enforcing a standard).
Known divergences are flagged below for future retrofit.

## General Pattern

| Set | Purpose | Frequency |
|-----|---------|-----------|
| 0 | Frame-level constants (camera, sun, screen dims) | Once per frame |
| 1 | Per-draw constants + material textures | Once per draw call |
| 2+ | Subsystem-specific (rare) | Varies |

## Set 0 (Frame Constants)

All subsystems bind `FrameConstants` at `set=0, binding=0` (declared in `Common.fxh`).
Some subsystems add extra frame-level data in set 0:

| Subsystem | Binding 0 | Additional bindings |
|-----------|-----------|---------------------|
| All meshes | FrameConstants | -- |
| Terrain | FrameConstants | binding 1: TerrainConstants |
| Vegetation (grass) | FrameConstants | binding 1: GrassConstants, binding 2: InstanceBuffer |
| DynamicLights | FrameConstants | binding 1: LightData, binding 2: ShadowMap |
| SSR / SSGI / SSAO | FrameConstants | Various SRVs (depth, normals, HiZ) |

## Set 1 (Per-Draw / Material)

### Mesh subsystems (Static, Animated, Instanced)

| Binding | Resource | Notes |
|---------|----------|-------|
| 0 | DrawConstants (128-byte UBO via scratch buffer) | Shared header: `DrawConstants.fxh` |
| 1 | Bones UBO (animated only) / DiffuseTex (static/instanced) | **DIVERGENCE**: animated shifts textures to 2-6 |
| 1-5 | Material textures (static, instanced) | Diffuse, Normal, RoughMetal, Occlusion, Emissive |
| 2-6 | Material textures (animated) | Same 5 textures, offset by 1 due to Bones at binding 1 |
| 6-8 | Instance SSBOs (instanced only) | TransformBuffer, AnimDataBuffer, VisibleIndexBuffer |
| 9 | Animation texture / VAT (instanced only) | sampler2D |

**Known divergence**: Static/Instanced use texture bindings 1-5; Animated uses 2-6.
This means a future unified mesh shader must pick one convention. Retrofit target for Phase 5.

### Terrain

| Binding | Resource |
|---------|----------|
| 0 | TerrainMaterialConstants (288-byte UBO) |
| 1 | LOD buffer |
| 2 | Splatmap texture |
| 3-22 | Material textures (up to 4 materials x 5 slots each) |

**Known divergence**: 20 texture bindings (3-22) is out of line with all other subsystems.
Retrofit target: migrate to a texture2DArray at a single binding.

### Shadows (set 1)

| Binding | Resource |
|---------|----------|
| 0 | DrawConstants (same 128-byte block) |
| 1 | ShadowMatrix UBO (static/instanced) or binding 2 (animated) |

Shadow passes use the same `DrawConstants.fxh` plus a `ShadowMatrix` UBO.
The `#define SHADOWS` variant strips varyings and outputs depth-only.

## Non-Mesh Subsystems

These subsystems use "PushConstants" as a name for subsystem-specific uniform data
at **different** set/binding locations (not the per-draw material block):

| Subsystem | Set | Binding | Size | Content |
|-----------|-----|---------|------|---------|
| ComputeTest | 0 | 1 | varies | Test params |
| DynamicLights | 0 | 8 | varies | Light volume transform |
| HiZ_Generate | 0 | 2 | varies | Mip generation params |
| Particles | 0 | 1 | varies | Particle system params |
| SSGI_Denoise | 0 | 1 | varies | Denoise kernel params |

These share only the "PushConstants" name, not the layout. Phase 2 rename
(`PushConstants` -> `DrawConstants`) applies only to the mesh material block;
subsystem-specific blocks will be renamed to subsystem-specific names.
