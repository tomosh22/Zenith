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
// Owning handle — AddRef'd under the registry lock (race-free), survives UnloadUnused:
TextureHandle xTex = Zenith_AssetRegistry::Acquire<Zenith_TextureAsset>("game:Textures/tex.ztxtr");
// Raw, NON-owning transient view — must NOT outlive an UnloadUnused()/scene transition:
Zenith_TextureAsset* pView = Zenith_AssetRegistry::GetView<Zenith_TextureAsset>("game:Textures/tex.ztxtr");

// Create procedural asset (generates unique path like "procedural://texture_0").
// Create<T>() returns an OWNING Zenith_AssetHandle<T> (AddRef'd — there is no 0-refcount window):
TextureHandle xProc = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();

// Cleanup unused assets
Zenith_AssetRegistry::UnloadUnused();  // Free assets no handle references (ref count 0)
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

## Loader Contract (unified)

Every file-backed asset type is loaded through **one of two templates** in
`Zenith_AssetRegistry.cpp` — there is no longer a per-type `LoadXxxAsset` free
function:

- **`LoadAssetGeneric<T>`** — member-contract types (Texture, Material, Animation,
  MeshGeometry, Font, Prefab). Empty path → `new T()` (the `Create<T>()` path); else
  `new T()` + `T::LoadFromFile(...)` + delete-on-failure.
- **`LoadAssetViaStaticFactory<T>`** — the static-factory trio (Mesh, Skeleton, Model),
  whose `static LoadFromFile → Zenith_Result<T*>` owns the new+parse internally.

Per-type knobs live in one place — `Zenith_AssetLoadTraits<T>`: `kGuardProcedural`
(reject `procedural://` load paths — Animation + MeshGeometry only) and `DoLoad`
(bakes each type's default args, e.g. Texture's `bCreateMips=true`). The trait is
friended by each member-contract asset so `DoLoad` can call the private `LoadFromFile`.
The generic `.zdata` serializable path (`RegisterAssetType<T>` / `LoadSerializableAsset`)
is separate and untouched.

`Zenith_AssetRegistry::ForceUnload(path)` copies the key **before** deleting the asset
(callers pass `pxAsset->GetPath()`, a reference into the asset's own `m_strPath`; using
it after `delete` would read freed memory).

## Typed-Asset Serialization (Stream Envelope)

Texture, Material, Mesh, Skeleton and Model prefix their DataStream payload with the
shared `Zenith_StreamEnvelope` header (`magic + envelope version + asset-type-id +
schema version`). The **single source of truth** for every asset-type-id and current
schema version is `AssetHandling/Zenith_AssetTypeIds.h` — no more per-asset
`#define ZENITH_*_VERSION`.

- **Write**: `WriteToDataStream` calls `Zenith_WriteStreamHeader(stream, <typeId>,
  <schemaCurrent>)` first, then the payload. Tools exporters that call `Export()`
  inherit the header for free (texture is the exception — its exporter writes the
  header directly).
- **Read**: a status-returning `ParseStream(stream)` does the envelope read
  (`Zenith_ReadStreamHeader`): wrong type-id → `INVALID_ARGUMENT`, newer envelope →
  `VERSION_MISMATCH`, and a **legacy pre-envelope file** trips `BAD_MAGIC` — the read
  is non-destructive, so the cursor rewinds to 0 and the old bare-version-word layout
  is read exactly as before. `LoadFromFile` = `ReadFromFile` + `ParseStream`; the void
  `ReadFromDataStream` virtual delegates to `ParseStream` (the file-load error contract
  lives in `ParseStream`).
- **Back-compat note**: a *new-format* payload has NO bare version word (the envelope's
  schema field replaces it); a *legacy* file does, and `ParseStream` reads it after the
  `BAD_MAGIC` rewind. Do not bump a `*_SCHEMA_CURRENT` without adding a legacy branch.

`Zenith_MaterialParams` (in `Zenith_MaterialParamTable.h`) owns the ~22-field v5
parameter order **once** via its own `WriteToDataStream`/`ReadFromDataStream` (called
explicitly, never via `<<`, so struct padding never leaks into the file); the material
asset defers to it in both directions.

## GPU/CPU Resource Lifetime

`Zenith_Asset` exposes a uniform vocabulary — `EnsureGPUResources()`, `IsGPUReady()`,
`ReleaseGPUResources()` (default no-ops for pure-CPU assets). The **timing still differs
by type** by design:

- **Texture — eager**: VRAM + SRV are created during `LoadFromFile`/`CreateFromData`,
  so `EnsureGPUResources()` is a no-op (nothing to lazily upload; source bytes aren't
  retained). `IsGPUReady()` reflects the eager upload; `ReleaseGPUResources()` routes to
  the deferred-free `ReleaseGPU()`.
- **Mesh — lazy**: `EnsureGPUResources()` builds the buffers on demand. There is no
  skinned variant — `EnsureGPUBuffers()` takes no arguments and always uploads the
  STATIC stream. The skin-INPUT stream (packed static vertex + the two
  compute-skinning bone lanes) is not asset-owned at all: `Flux_MeshInstance` /
  `Flux_SkinnedPoseProvider` build it into the skinning arena at the use site, where
  skinned-ness is known. The destructor already auto-releases via `Reset()`.

**Material GPU-table index reclamation**: a material's `Flux_MaterialTable` slot is
freed when the material asset is destroyed. `~Zenith_MaterialAsset` calls the guarded
`Flux_ReleaseMaterialIndex(GetMaterialTableIndex())`, which **enqueues** the index
(any thread); `Flux_MaterialTable::AdvanceFrame()` (main thread, once per frame in
`Flux_RendererImpl::ProcessFrameEnd`) drains the queue into the index allocator's
deferred `Free` (recycled after the frames-in-flight grace) and resets the slot's
build mirrors. The forwarder is a no-op when the renderer is unavailable
(`g_xEngine.HasFluxGraphics()` false, or `Flux_MaterialTable::IsInitialised()` false),
so a late destructor during shutdown teardown is safe. Net effect: the table's live
index count is **bounded across scene reloads** instead of growing forever.

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

## Export Pipeline (meshes)

The MESH export pipeline is in `Tools/Zenith_Tools_MeshExport.cpp`. It processes source files through these stages. Textures have their own pipeline — see **Texture Export Pipeline** below.

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

## Texture Export Pipeline

Source images (`.png` / `.jpg` / `.jpeg`) become `.ztxtr` in **two stages, and only
the first is automatic.** This is the thing people forget, because stage 1 needs no
action at all and stage 2 needs a code edit.

### Stage 1 -- image files to `.ztxtr` (automatic)

`ExportAllTextures()` (`Tools/Zenith_Tools_TextureExport.cpp`) walks
`GAME_ASSETS_DIR/Textures` and **all of `ENGINE_ASSETS_DIR`**, recursively, and writes
a sibling `.ztxtr` for every `.png` / `.jpg` / `.jpeg` it finds -- **BC1**, auto-upgraded
to **BC3** when the source has alpha, with an offline-baked mip chain (a v2 `.ztxtr`).

It runs at boot on **any** `ZENITH_TOOLS` build, from `Zenith/Core/Zenith_Engine.cpp`,
before Flux comes up. There is no opt-in and **no freshness check** -- every image is
re-exported on every boot; `--skip-tool-exports` turns the whole export phase off.
So dropping new images into an asset directory and booting any tools exe is the whole
of stage 1. There is also a debug-variable button, **Export -> Textures -> Export All
Textures**.

The Content Browser's right-click **"Export to .ztxtr"** is a per-file alternative, but
note it exports jpgs **Uncompressed** rather than BC1, so a set built that way will not
match one built by the boot path. Prefer the boot path.

`ExportFromHeightmapImageFile` is the PNG-specific variant that preserves bit depth
(16-bit heightmaps stay `R16_UNORM`, single-channel stays single-channel) -- this is what
terrain heightmaps go through, and what the Content Browser uses for `.png`.

### Stage 2 -- `rm_packed.ztxtr` for PBR ground sets (NOT automatic)

The terrain shader reads roughness and metallic from **one packed RGBA8 texture**
(`xRM.gb` in `Flux/Shaders/Terrain/Flux_Terrain_ToGBuffer.slang`), not from the separate
`roughness` / `metallic` maps a downloaded PBR set ships with. That packed file is
written by `RenderTest_PackRoughnessMetallic` in `Games/RenderTest/RenderTest.cpp`,
driven from a **hand-maintained list of set directories** in
`RenderTest_PackTerrainRoughnessMetallic`.

* **That list is the only producer of `rm_packed.ztxtr` for any game.** RenderTest owns
  the packer; a Zenithmon or CityBuilder boot will not create these files however many
  times you run it. A shared ground set that is not named in the list ends up with three
  of its four maps, and a terrain slot sampling it **falls back silently to the default
  RM texture** -- no error, no red test.
* It is idempotent on **file presence**, not mtime: once `rm_packed.ztxtr` exists it is
  never re-packed until you pass `--rendertest-force-regenerate` or delete it.
* It expects the roughness and metallic `.ztxtr` to be **BC1** and the same resolution,
  and logs an error rather than guessing if either is not.

### Adding a new shared ground set, end to end

1. Put the source jpgs in `Zenith/Assets/Textures/Terrain/<Name>/`. The maps that matter
   downstream are `diffuse`, `normal`, `roughness`, `metallic` and `ao`; `gloss`,
   `height` and `reflection` are converted too but nothing samples them today.
2. Add
   `RenderTest_PackRoughnessMetallic(std::string(ENGINE_ASSETS_DIR) + "Textures/Terrain/<Name>/");`
   to `RenderTest_PackTerrainRoughnessMetallic`.
3. Build and boot RenderTest tools:
   `rendertest.exe --automated-test EngineBootShutdownSmoke --skip-unit-tests`.
   Stage 1 runs at engine boot, stage 2 runs in the editor-automation batch after it, so
   one boot does both. Use an `--automated-test` -- a bare windowed tools boot idles in
   the editor forever and never exits.
4. Confirm all four consumed maps exist: `diffuse` / `normal` / `rm_packed` / `ao`. That
   is exactly the set a terrain material's four slots expect (see
   `ZM_TerrainMaterialSpec` in Zenithmon, or `SetupPBRTerrainMaterial` in RenderTest).

## Generated shared assets (`Zenith/Assets/Meshes/`)

Five asset sets are not imported from anywhere -- they are GENERATED, in full, on
every tools boot, by `GenerateTestAssets()` (`Tools/Zenith_Tools_TestAssetExport.cpp`,
called from `Zenith_Engine` before Flux comes up). They live under `ENGINE_ASSETS_DIR`
rather than a game's assets because more than one game consumes them.

| Set | Generator | Output |
|---|---|---|
| StickFigure | `Zenith_Tools_TestAssetExport.cpp` | 16-bone rig, lofted body, painted atlas, 13 clips |
| ProceduralTree | `Zenith_Tools_TreeAssetExport.cpp` | branching trunk + leaf cards, bark/leaf textures, sway VATs |
| **Rocks** | `Zenith_Tools_RockAssetExport.cpp` | 4 stone meshes + granite/sandstone PBR sets |
| **FallenTrees** | `Zenith_Tools_FallenTreeAssetExport.cpp` | 4 deadwood meshes + bark/mossy-bark PBR sets |
| **Bushes** | `Zenith_Tools_BushAssetExport.cpp` | 3 wind-animated foliage bushes (skeleton + sway VAT each) + masked foliage material |

Every one of them is SEEDED: a re-boot rewrites the same bytes, so a generator that
drifts shows up as churn rather than as a silent visual change. `--skip-tool-exports`
turns the whole phase off (and then needs one prior full tools run to have produced
the files).

### The rock set

`Zenith/Assets/Meshes/Rocks/` -- `Rock_Boulder`, `Rock_Slab`, `Rock_Shard` and
`Rock_Pebbles` (a six-stone cluster in one mesh), each as `.zasset` (what
`Zenith_InstancedMeshComponent::LoadMesh` takes), `.zmesh` (static geometry) and
`.zmodel` (what `AddStep_LoadModel` / a `ModelComponent` takes) -- plus two full PBR
texture sets, `Rock_Granite_*` and `Rock_Sandstone_*` (`Albedo` / `Normal` / `RM` /
`AO`), and the two materials that wire them into the four slots
`EvaluateMaterialSurface` samples.

Four things about it are worth knowing before changing it:

* **The textures TILE, and the meshes depend on that.** Rock UVs are box-projected
  along each triangle's dominant axis at a fixed metres-per-tile, which is what avoids
  the pole pinch and wrap seam a sphere unwrap puts on a 2 m boulder -- but it means
  the maps meet their own edges constantly. The generator therefore samples a WRAPPED
  integer lattice (its local `TileableFBM`), not `Zenith_TerrainNoise::FBM`, which
  cannot tile. `RockAssets.RockTexturesTileExactlyAcrossTheWrap` pins this.
* **`RM` is the glTF layout the engine samples: G = roughness, B = metallic.** This is
  the same convention as the terrain's `rm_packed` (`Common/Material.slang`
  `SampleRoughnessMetallic`), NOT a separate roughness map. Stone is a dielectric, so
  B is 0 and the material's metallic multiplier is 0 as well.
* **The icosphere's winding is normalised to the ENGINE convention before anything
  reads it** (`cross(C-A, B-A)` outward). The published icosahedron face list is the
  other way round, and the difference is not cosmetic: the smooth-normal pass
  accumulates `cross(C-A, B-A)`, so an unnormalised shell hands every vertex an
  INWARD smooth normal, the emitter blends its (correctly wound) face normal toward
  it, and a stone with `m_fNormalSmooth` above ~0.5 shades with a normal pointing
  into itself. It still silhouettes correctly, so it reads as "the albedo is too
  dark" — which cost one round of albedo tuning before
  `RockAssets.NormalsAndTangentsAreOrthonormalAndOutward` named it. That test
  compares each shading normal against ITS OWN face; a radial check alone passes on
  the shard, which blends only 0.22.
* **A rock's origin sits on its own flattened underside.** That is a contract a scatter
  relies on -- it places an instance AT the sampled terrain height with no per-mesh
  offset table -- and `RockAssets.RockSitsOnItsBaseAtTheRequestedWidth` pins it
  alongside `m_fWidthMetres` meaning the actual horizontal extent in metres.

Adding a stone type is a new row in `RockVariantAt` -- the ONE table the exporter and
the unit tests both read, so a tuned knob cannot drift out of the tests. Nothing
engine-side knows what a rock is; RenderTest's `RenderTest_ScatterRocks` is the
reference consumer (see `Games/RenderTest/CLAUDE.md`).

### The deadwood set

`Zenith/Assets/Meshes/FallenTrees/` -- `FallenTree_Log`, `FallenTree_LogMossy`,
`FallenTree_Stump` and `FallenTree_Branches` (three loose pieces in one mesh), with the
same `.zasset` / `.zmesh` / `.zmodel` trio and the same four-map PBR sets as the rocks
(`FallenTree_Bark_*`, `FallenTree_MossyBark_*`). It shares the rock set's tileable
noise, its RM layout and its origin-on-the-base contract, so only the differences are
worth writing down:

* *** EVERY PIECE IS MODELLED STANDING UP**, along +Y with its origin on the butt end,
  and the SCATTER lays it down. That is not stylistic. `Zenith_InstanceColliderConfig`
  can only describe a **Y-aligned** capsule -- but `CreateInstanceBody` rotates the
  capsule, and its local Y offset, by the instance's own rotation. So a log authored
  along +Y and tipped 90 degrees by the scatter gets a correctly aligned HORIZONTAL
  capsule for free. Modelling the log lying down would need a per-instance shape axis
  on the component, i.e. a serialization bump, to express the same thing. The stump
  follows the same convention and is simply never tipped.
* **The bark noise is ANISOTROPIC** -- a high lattice period across U against a low one
  along V -- because bark ridges run ALONG a trunk. That is why the shared tileable
  noise takes per-axis periods, and
  `FallenTreeAssets.BarkHeightFieldIsAnisotropicAlongTheTrunk` measures it rather than
  trusting the parameters.
* **A WRAPPED LATTICE OF PERIOD 1 IS CONSTANT.** It has exactly one distinct sample, so
  both interpolants are the same value and the field does not vary at all. The first
  version of the trunk's surface noise passed period 1 along the length and the bark
  relief simply vanished -- which reads as a tuning problem, not a bug. `SurfaceRadius`
  clamps to >= 2 and `FallenTreeAssets.ALatticePeriodOfOneWouldBeConstant` pins both
  halves.
* **UVs are cylindrical at world scale**, not box-projected: a tube has an exact
  unwrap, and the rocks' dominant-axis projection would smear across a curved flank.
  The broken end caps map V **radially** instead of axially, so the same along-trunk
  ridges come out as concentric rings -- end grain from the bark texture, with no
  second material and no second instance group.
* **The settle is a post-pass**, not part of the tube builder: a broken end's splinter
  offsets are what push geometry below the axis origin, and a cluster only knows its
  own floor once every piece is in.

### The bush set

`Zenith/Assets/Meshes/Bushes/` -- `Bush_Broad` (waist-high dome), `Bush_Mound`
(knee-high hemisphere) and `Bush_Spindly` (tall sparse upright), each as `.zasset`
+ `.zmesh` + its own `.zskel` and `_Sway.zanmt` VAT, sharing one
`Bush_Foliage_Albedo.ztxtr` + `Bush_Foliage.zmtrl`. It is the first ANIMATED set
after the ProceduralTree, and the differences from its static siblings are the
whole story:

* **ONE instance group per bush, not the tree's lockstep trunk+leaves pair.** An
  instance group is single-material; the tree splits because its trunk is OPAQUE
  while its leaves are MASKED. A bush is alpha-tested foliage all the way
  through -- the card shell is dense enough that interior stems would never be
  seen -- so a stem group would double the entity count and serialized instance
  data for invisible geometry. A game wanting leggy see-through bushes authors a
  second lockstep group the tree's way; it is not a change to this set.
* **A VAT needs a SKINNED mesh**, so the chain is the tree's: branch graph ->
  `Zenith_SkeletonAsset` (one bone per branch + a root anchor; `AddBone` takes
  PARENT-LOCAL positions) -> cards skinned wholly to their branch bone (weight
  exactly 1 -- an unskinned vertex bakes frozen while its neighbours sway, which
  is very visible and has a dedicated unit) -> per-bone rotation clip with
  keyframe times in **TICKS** (0..120, not 0..4 seconds) ->
  `Zenith_Tools_CreateFluxMeshGeometry` (the SKINNED converter --
  `CreateStaticFluxMeshGeometry` drops the bone lanes and the bake would have
  nothing to deform) -> `Flux_AnimationTexture::BakeFromAnimations`.
* **Per-instance sway phase is transient, never serialized.**
  `Zenith_InstancedMeshComponent::ReadFromDataStream` re-derives it on load
  (`fmodf(instanceID * 0.618034f, 1.0f)`), so a reloaded scene comes back
  de-synchronised for free; RenderTest's scatter seeds the SAME derivation at
  authoring time so the authoring session matches every reload. In the editor's
  Stopped mode nothing services these components' VAT time (the terrain editor
  hand-ticks only its own tree entities), so bushes sway in Play and stand still
  in Stopped -- accepted.
* **The foliage material is `MATERIAL_BLEND_MASKED` with cutoff 0.45**, and the
  albedo's alpha is a REAL leaf mask -- `BuildMaterialDrawConstants` writes
  cutoff 0 for OPAQUE, so an opaque material (or an all-255 alpha) renders every
  card as the leaf texture on an opaque black square. Both halves are pinned by
  units (`BushAssets.FoliageAlbedoAlphaIsARealMask`).
* **Cards are double-sided by emission**: both windings per card, in a fixed
  pair order the unit tests check, because the instanced pipeline backface-culls
  and foliage is seen from both sides. The rock set's
  every-triangle-faces-outward test is therefore the WRONG shape here.
* **`m_fHeightMetres` means metres EXACTLY, not a band.** After card placement
  the whole cluster (branch graph included -- the skeleton is built after) is
  uniformly rescaled so the highest card corner sits at the authored height, so
  a consumer's bounds sphere and sink depth derive from a true number. This is
  the lesson the deadwood's stump capsule learned as a band the hard way.
* **No `.zmodel`** -- the instanced component is the consumer; a ModelComponent
  would render the bush frozen at bind pose.

Adding a bush shape is a new row in `BushVariantAt` -- the ONE table the exporter
and the unit tests both read. RenderTest's `RenderTest_ScatterInstancedProps` is
the reference consumer (three `TerrainBushes_*` rows, the table's VAT columns).

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
  Zenith_GrassTypeTableAsset.h/cpp - Authored grass type table (wraps a Flux_GrassTypeTable; .zdata)
  Zenith_MeshGeometryAsset.h/cpp  - Wrapper for Flux_MeshGeometry
  Zenith_FontAsset.h/cpp          - Font asset (.zfont glyph metrics + atlas)
  Zenith_MaterialParamTable.h/cpp - Material parameter reflection table (names/types/ranges/groups)
  Zenith_PropertyTuning.h/cpp     - Live .ztune file bindings for reflected properties
  Zenith_Image.h/cpp              - Single-channel 32-bit float image container (tools/terrain)
  Zenith_AssetHandle.cpp          - Explicit handle template instantiations
  Zenith_FileWatcher.h/cpp        - File system change watching
```
