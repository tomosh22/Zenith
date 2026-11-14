# GEOM design: meshes, pools, LOD, instancing, meshlets and terrain geometry

Section file of `Zenith2/Docs/Design/GEOM.md`. It specifies the mesh cook (quantisation, precision checks, splitting, the meshoptimizer pipeline, LOD generation, meshlets), the cooked mesh format `.zmesh`, the runtime mesh resource and the unified geometry pools of ZenithFlux, LOD selection, instancing, and the geometry side of terrain. The vertex codec is `Zenith2/Docs/Design/GEOM/VertexFormat.md`; skinned and morphed meshes add the data of `Zenith2/Docs/Design/GEOM/Skinning.md`. The normative source is `Zenith2/Docs/Rendering/VertexData.md` ("Precision checks and fallbacks", "Index buffers and mesh splitting", "Unified stream pools", "Buffer usage unions") and `Zenith2/Docs/Rendering/GpuResources.md` ("Primitive record (128 bytes)", "Persistent resources", "Pools").

## Scope of this file

| Topic | Heading |
|---|---|
| the words this design uses for the parts of a mesh | "Mesh terms" |
| the mesh cook, step by step | "Mesh cook pipeline" and its subsections |
| the per-mesh fallback report | "Precision fallback report format" |
| the cooked file | "The cooked mesh format" |
| properties other cook steps read, and HLOD merge recipes | "Published mesh properties", "Merge recipes" |
| CPU readers of a cooked mesh | "CPU geometry of LOD 0", "CPU geometry of one LOD", "Cooked mesh geometry in tools" |
| runtime types | "Flux_MeshResource header" |
| where vertices and indices live on the GPU | "Geometry pools" |
| load, residency and unload | "Mesh residency state machine" |
| choosing a LOD per view | "LOD selection" |
| drawing many copies | "Instancing" |
| terrain | "Terrain geometry"; the cook contract with ENV (input, validation, the four `.zterrain` sections) in "Terrain cook contract" |
| tests | "Mesh tests" |

## Mesh terms

| Term | Meaning |
|---|---|
| mesh | one mesh asset (`Zenith_AssetHandle<Flux_MeshResource>`): every LOD of one imported mesh, sharing one quantisation box, one UV box per channel and one stream-set code |
| material slot | one of the mesh's `materialCount` material assignments, numbered from 0 in import order |
| LOD | one level of detail, 0 the finest; at most `uGEOM_MAX_LODS` = 8 |
| part | one drawable range of one LOD: one material slot, at most 65,535 unique vertices, one contiguous 16-bit index range, one primitive record |
| section | a part index within a LOD, `0 … S − 1`; parts of the same section index in different LODs have the same material slot, so the instance's material-slot records apply to every LOD (`m_uMaterialSlotBase` + section, `Zenith2/Docs/Rendering/GpuResources.md → "Instance record (128 bytes)"`) |
| S | the section count of the mesh: the sum over material slots m of `P_m`, the largest part count material m has in any LOD; at most `uGEOM_MAX_SECTIONS` = 255, because the instance record's `m_uPrimitiveLayout` gives S 8 bits (bits 0–7, `Zenith2/Docs/Rendering/GpuResources.md → "Instance record (128 bytes)"`) |
| empty part | a (LOD, section) pair whose LOD needs fewer parts for that material than `P_m`; its primitive record has `m_uIndexCount` = 0, which culling skips |
| primitive | the GPU scene record of one (LOD, section): `primitive = m_uPrimitiveBase + LOD × S + section` |

## Mesh cook pipeline

The mesh cook step runs in a ZenithCook worker process (`Zenith2/Docs/Design/GEOM.md → "Authoring and cooking"`) for one mesh and one platform. Its input is the importer's intermediate mesh (CONTENT's importers produce it: welded-or-not triangles with positions, normals, MikkTSpace tangents with sign, up to two UV sets, colour, up to eight skin influences, morph targets, material slot per triangle, one entry per material slot holding the slot's material GUID and source name, optional authored LOD chain), the mesh's import preset (CONTENT), and the texture resolutions of the materials assigned to it. Its output is one `.zmesh` and the mesh's entries in the cook report.

| Step | Name | Heading |
|---|---|---|
| 1 | validate the intermediate mesh against the parser limits | "Input validation" |
| 2 | weld | "Weld" |
| 3 | tangents: generate where missing | "Tangent generation" |
| 4 | choose the stream-set | "Stream-set choice" |
| 5 | quantisation box and precision checks | "Quantisation box and precision checks" |
| 6 | LOD chain: authored or generated | "LOD generation" |
| 7 | per LOD: optimise | "Optimisation order" |
| 8 | per LOD: split into parts and lay out sections | "Splitting and part table" |
| 9 | per part: position-only indices | "Position-only indices" |
| 10 | per part: meshlets | "Meshlets" |
| 11 | per part: bounds and UV density | "Bounds and UV density" |
| 12 | pack streams through `Zenith_PackVertexStreams`, driven by the generated `axFLUX_STREAMSET_LAYOUTS` entry of the mesh's code (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Stream-set declarations in Slang"`), record that entry's layout hash, write the file and the report | "The cooked mesh format", "Precision fallback report format" |
| 13 | publish the mesh's properties: `geom.meshBounds`, `geom.lod0Summary`, `geom.meshSections` and `geom.sourceMesh` | "Bounds and UV density", "Published mesh properties" |

Every step is a deterministic function of its inputs: no clock, no thread count, no hash-map iteration order and no address reaches the output, so a clean cook and an incremental cook of the same inputs give byte-identical `.zmesh` files (`Test_CookMesh_CleanEqualsIncremental`).

Two inputs change where the pipeline starts. An item whose intermediate is `geom.meshMergeRecipe` instead of `mesh` builds its source from other meshes by "Merge recipes" and continues from step 3. A mesh whose import settings carry `skin.rigidAttach` becomes a one-joint skinned mesh after step 1 (`Zenith2/Docs/Design/GEOM/Skinning.md → "Rigid attachment setting"`).

### Input validation

| Check | Limit | On violation |
|---|---|---|
| vertex count | at most 16,777,216 per mesh (`Zenith2/Docs/Architecture.md → "Security architecture"`, table "Parser limits") | `GEOM_COOK_ERROR_MESH_LIMIT` |
| section count S after splitting | at most 255 (`uGEOM_MAX_SECTIONS`) | `GEOM_COOK_ERROR_MESH_LIMIT`, naming the mesh, S, and the material slot whose `P_m` is largest |
| part count after splitting | at most 255 × 8 = 2,040 (S sections in each of at most 8 LODs), inside the parser limit of 65,536 sub-meshes of `Zenith2/Docs/Architecture.md → "Security architecture"`, which is a per-file parser bound and not the cook's | `GEOM_COOK_ERROR_MESH_LIMIT` |
| every position, normal, tangent, UV, colour and weight finite | — | `GEOM_COOK_ERROR_MESH_NON_FINITE` naming the attribute and the first vertex |
| every index below the vertex count, no degenerate triangle (two equal indices) | degenerate triangles are removed with a warning | `GEOM_COOK_ERROR_MESH_INDEX`, `GEOM_COOK_WARNING_MESH_DEGENERATE_REMOVED` |
| a zero-length normal | replaced by the triangle-area-weighted face normal of the vertex's triangles | `GEOM_COOK_WARNING_MESH_NORMAL_REPAIRED` |
| material slot count | at most 255 | `GEOM_COOK_ERROR_MESH_LIMIT` |
| winding | counter-clockwise front faces; the importer has already reversed nodes with a negative-determinant transform ([Z2-N-REND-885]); the cook verifies, for every triangle, that its face normal agrees with the average vertex normal in sign for at least 95% of the area, and warns otherwise | `GEOM_COOK_WARNING_MESH_WINDING_SUSPECT` |

### Weld

`meshopt_generateVertexRemapMulti` over every source attribute stream (exact bit equality of position, normal, tangent and sign, both UVs, colour, joints and weights), then `meshopt_remapIndexBuffer` and `meshopt_remapVertexBuffer` per stream. Welding happens before quantisation, so two vertices that differ only below the quantisation step stay separate and a UV seam keeps its duplicated vertices.

### Tangent generation

A mesh any of whose materials samples a normal map (the material reflection marks a texture field of usage `FLUX_TEXTURE_USAGE_NORMAL`, `Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`) and whose source lacks tangents gets MikkTSpace tangents over UV0, computed on the welded mesh, followed by a second weld. A skinned mesh always carries a QTangent (the skinned pool stores one), so a skinned mesh with no normal-mapped material gets tangents from `Zenith_BuildOrthonormalBasis` of each normal (`Zenith2/Docs/Design/CORE/Maths.md → "Vector types"`) with sign +1.

<!-- assumption: pending SHIP -->
MikkTSpace (`mikktspace.c`, zlib licence) joins `Zenith2/ThirdParty/manifest.json` with owner ZenithCook, usage tools, platform windows (`Zenith2/Docs/Architecture.md → "Third-party dependencies"`); the ticket that implements tangent generation adds the entry and the licence row in its diff.
<!-- /assumption -->

### Stream-set choice

| Field | Chosen by |
|---|---|
| POS | SNORM16X4, or FLOAT32X3 after a failed position check |
| SURF | QTANGENT when a material samples a normal map or the mesh is skinned; OCT otherwise, including a mesh whose materials are all unlit (`FLUX_SHADING_MODEL_UNLIT`): the MESH family has no surface-less stream-set, by cross-doc ruling X140 (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Canonical stream-set layouts"`) |
| UV0, UV1 | present when the source has the set and a material reads it (the material reflection records the UV channel of each texture, `Zenith2/Docs/Design/GEOM/Materials.md → "Material module conventions"`); UV1 also when the preset marks the mesh lightmapped |
| UV0F, UV1F | the UV precision checks |
| COL | the source has colours and a material reads `m_xVertexColour` (reflection flag `readsVertexColour`) |
| SKIN | the source is skinned: J8 or J16 by joint count, W8 or W16 by the weight check; for a skinned mesh POS is also `FLOAT32X3` when the posed-extent check fails (the tie of "Fallback derivation rule") |
| FAMILY | MESH. The terrain grid's code is fixed (`terrain:pos-snorm16x4`, "Terrain data"), and procedural codes are chosen by the generator's declaration (`Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets"`) |
| declaration | every code this table can produce is reachable and therefore declared (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "Reachable stream-set rule"`), so a failed precision check never fails the cook. The step is two functions: `Zenith_CookChooseStreamSet` (pure, returns the code) and `Zenith_CookResolveStreamSet(code, axTable)` (the `Zenith_FindStreamSetLayout` lookup). The lookup returns `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED`, naming the mesh and the canonical name, only for a table that lacks the code; that is a defence against a broken build, never a precision outcome, and the cook never substitutes another layout |

### Quantisation box and precision checks

The position box is the tight AABB of every vertex of every LOD (a simplified LOD's vertices are a subset of LOD 0's welded vertices, so the LOD 0 box already contains them); the UV boxes span every vertex of every LOD. All parts of all LODs share them ([Z2-N-REND-882]). The checks are those of `Zenith2/Docs/Rendering/VertexData.md → "Precision checks and fallbacks"` ([Z2-N-REND-879]), computed as follows:

<!-- elaboration -->
| Check | Computation (double precision, over every vertex of LOD 0) | Default tolerance | Preset keys (CONTENT) |
|---|---|---|---|
| position | encode with `Zenith_QuantisePositions`, decode with `Zenith_DequantisePosition`; error = max over vertices and axes of `|decoded − source|` | fails when error > max(2⁻¹⁶ × AABB diagonal, 0.001 m) | `precision.positionRelative`, `precision.positionAbsoluteMetres` |
| UV0, UV1 | error in texels = max over vertices of `|decoded − source| × R`, R the largest texture resolution (the larger of width and height) among the textures sampled with that UV channel by the mesh's materials, or `precision.uvReferenceResolution` (default 4,096) when no material is known yet | fails when error > 1/8 texel; FLOAT16x2 is chosen when its error is lower; if both fail, the lower-error format is kept with `GEOM_COOK_WARNING_UV_PRECISION` | `precision.uvTexels`, `precision.uvReferenceResolution` |
| joints | the mesh's joint count | UINT16x4 when above 256 | — |
| weights | max over vertices of `Σ_i |w_i,q − w_i| × L_i`, L_i the bind-pose distance from the vertex to joint i | fails when error > 0.001 m | `precision.weightMetres` |
| skinned pool | `Zenith2/Docs/Design/GEOM/Skinning.md → "Posed-extent check"` | 0.001 m | `precision.posedMetres` |

- Every failed check applies exactly the listed fallback and adds one record to the report and to the file's `PRECISION` section, and the one forced change of a skinned mesh adds one more, under the record rule of "Precision fallback report format". A mesh whose checks all pass writes an empty section and no report record.

### LOD generation

| Source | Behaviour |
|---|---|
| the preset sets `lod.mode` = `authored` and the intermediate mesh carries an authored chain (glTF `MSFT_lod`, or the importer's naming convention) | each authored level becomes a LOD; its screen-size threshold comes from the authored `MSFT_screencoverage` value when present, otherwise from the error formula below with the error measured by `meshopt_simplifyScale`-normalised Hausdorff sampling (1,024 points per triangle area unit, fixed seed) against LOD 0 |
| `lod.mode` = `generate` (the default from Stage 4) | LOD l ≥ 1 is `meshopt_simplifyWithAttributes` of LOD l−1's index buffer, with target index count `ratio_l × indexCount_0` (default ratios 0.5, 0.25, 0.125, 0.0625, 0.03125, `lod.ratios`), target error `lod.maxError` (default 0.05, relative), options `meshopt_SimplifyLockBorder` off and `meshopt_SimplifySparse` off, attribute weights normal 0.5 and UV0 1.0 (`lod.normalWeight`, `lod.uvWeight`); the returned `result_error` × `meshopt_simplifyScale` is the LOD's world error e_l |
| `lod.mode` = `none` | one LOD |

Generation stops at the first level whose index count exceeds 0.9 × the previous level's, whose error exceeds `lod.maxError × meshopt_simplifyScale`, or at `uGEOM_MAX_LODS`; a stopped level is discarded. LOD generation is deterministic because meshoptimizer is single-threaded and its output depends only on its inputs.

The screen-size threshold of each level follows from a pixel-error budget: with r the mesh's bounding-sphere radius, `H_ref` = 1,080 pixels and `p` = `lod.pixelError` (default 1.0 pixel), LOD l+1 may be used when the projected size fraction s (defined in "LOD selection") satisfies `s ≤ 2 r p / (H_ref × e_{l+1})`; the cook stores `m_fLODScreenSize[l] = min(m_fLODScreenSize[l−1], 2 r p / (H_ref × e_{l+1}))` for l < L−1 and 0 for the last level, so thresholds never increase with l.

<!-- informative -->
Derivation: a world-space error e at distance d projects to `e × g × (H / 2) / d` pixels, with g = `P[1][1]`. The selection metric is `s = r × g / d`, so `g / d = s / r` and the projected error is `e × s × H / (2 r)`; setting it to p pixels at the reference height gives the threshold above. Using the reference height, not the current one, keeps LOD choice independent of dynamic-resolution scale; the per-tier LOD scale adjusts it.
<!-- /informative -->

### Optimisation order

Per LOD, in exactly this order ([Z2-N-REND-881] and `Zenith2/Docs/Rendering/VertexData.md → "Index buffers and mesh splitting"`):

1. `meshopt_optimizeVertexCache` over the LOD's whole index buffer, per material slot range (triangles never change material slot).
2. `meshopt_optimizeOverdraw` with threshold 1.05, per material slot range, over the source positions.
3. `meshopt_optimizeVertexFetchRemap`, then remap every attribute stream and the index buffer.
4. Split into parts ("Splitting and part table").
5. Per part, the position-only index buffer.
6. Per part, meshlets.

### Splitting and part table

```
SplitLOD(indices sorted by material slot, then in optimised order):
	for each material slot m, in ascending order:
		part = new part(m); unique = empty map vertex → local index
		for each triangle (a, b, c) of m, in order:
			need = count of {a, b, c} not in unique
			if |unique| + need > 65535:
				close part; part = new part(m); unique = empty
			append the triangle to part, adding new vertices to unique in first-use order
		close part
	return parts                                          // per material: P_m(LOD) parts
```

- A closed part's vertices are the LOD's vertices in first-use order, copied into the part's own contiguous vertex range, so its 16-bit indices are part-local and its primitive record's `m_iVertexOffset` addresses its first vertex; vertices shared between two parts are duplicated with identical encoded bytes (they are quantised with the shared box), so shared boundary vertices decode identically and never crack (`Test_Cook_MeshSplitSharesQuantisation`).
- Section numbering: for each material slot m, `P_m` = the largest part count m has in any LOD; sections `offset_m … offset_m + P_m − 1` with `offset_m = Σ_{k<m} P_k`; part j of material m in LOD l occupies section `offset_m + j`, and sections `offset_m + P_m(l) … offset_m + P_m − 1` of LOD l are empty parts.
- Primitive restart is disabled on every PSO, and no index buffer is 32-bit anywhere ([Z2-N-REND-881]).

### Position-only indices

For each part: `meshopt_generateShadowIndexBuffer(out, partIndices, indexCount, positionStreamBytes, vertexCount, vertexSize = stream-0 stride, vertexStride = stream-0 stride)` over the part's packed stream 0, so vertices with identical quantised positions collapse to one canonical vertex. The result is stored when its unique-vertex count is below 90% of the part's vertex count (`lod.positionOnlyThreshold`), otherwise `m_uPositionOnlyFirstIndex` is `0xFFFFFFFF` and position-only draws use the ordinary indices. It is used by shadow views and the opaque bucket of pure depth-only passes, never by the Forward+ prepass or alpha-tested buckets ([Z2-N-REND-884], [Z2-N-REND-651]).

### Meshlets

For each part of each LOD, whenever the platform's shading paths include the visibility buffer (every desktop cook; never an Android cook):

| Step | Call | Parameters |
|---|---|---|
| 1 | `meshopt_buildMeshletsBound` | max vertices 64, max triangles 128 ([Z2-N-REND-883]) |
| 2 | `meshopt_buildMeshlets` | the part's indices and stream-0 positions decoded to float, 64, 128, cone weight 0.25 |
| 3 | `meshopt_optimizeMeshlet` | per meshlet |
| 4 | `meshopt_computeMeshletBounds` | per meshlet: sphere, cone axis, cone cutoff, and their 8-bit forms |

<!-- elaboration -->
The meshlet record is a `[FluxShared]` struct `Flux_MeshletRecord` (32 bytes), declared in `Zenith2/Shaders/Common/Meshlets.slang`:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 12 | `m_xBoundsCentre` | object-space sphere centre (float3) |
| 12 | 4 | `m_fBoundsRadius` | object-space radius |
| 16 | 4 | `m_uCone` | `cone_axis_s8[0..2]` in bytes 0–2 and `cone_cutoff_s8` in byte 3 |
| 20 | 4 | `m_uFirstVertex` | index of the meshlet's first entry in the meshlet-vertex pool |
| 24 | 4 | `m_uFirstTriangle` | index of the meshlet's first entry in the meshlet-triangle pool |
| 28 | 1 | `m_uVertexCount` | at most 64 |
| 29 | 1 | `m_uTriangleCount` | at most 128, so the 7-bit triangle field of the visibility ID suffices ([Z2-N-REND-851]) |
| 30 | 2 | `m_uFlags` | bit 0 alpha-tested (the part's material is alpha-tested) |

- Meshlet vertices are `u16` part-local vertex indices (read as u32 words with a halfword select); meshlet triangles are one `u32` per triangle holding three 8-bit local indices in bytes 0–2 and 0 in byte 3.
- The cone test used by meshlet culling is meshoptimizer's `dot(centre − camera, axis) ≥ cutoff × |centre − camera| + radius` → cull, evaluated in object space with the camera transformed by the instance's inverse; a meshlet of an instance whose local-to-world matrix has a negative determinant skips the cone test.

<!-- assumption: pending RPROD -->
`Flux_MeshletRecord` joins the declaring-module table of `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"` with the new module `Common/Meshlets.slang` in the diff that implements it ([Z2-N-REND-136]).
<!-- /assumption -->

### Bounds and UV density

<!-- elaboration -->
The step publishes the property `geom.meshBounds` (`Zenith_CookContext::PublishProperty`; A3 cross-area row VX8 (b)): 28 bytes, little-endian: LOD 0's local AABB as six `f32` (min x, y, z, then max x, y, z), then the radius of its bounding sphere about the origin as one `f32`. The values are the ones the `.zmesh` header stores for LOD 0. A dependent step reads them through `ReadDependencyOutput`, and the bytes join that step's derived-data key, so a mesh whose bounds change re-cooks its readers. `Test_MeshCook_PublishesMeshBounds` cooks the two-material split mesh and asserts the published bytes equal the header's LOD 0 bounds and sphere radius bit for bit.


Per part: the object-space AABB of its vertices, the bounding sphere (Ritter's algorithm seeded with the AABB centre, then grown; deterministic), and per UV channel the UV density `ρ = sqrt(Σ world triangle area / Σ UV triangle area)` in metres per UV unit (triangles with zero UV area are skipped; a part with no UV area stores 0). Per mesh: the union of every part's bounds.

### Published mesh properties

<!-- elaboration: answers A3 rows WD8 (the source property), WD9 (a), WD30 and A5 rows WdB6, WdB7 (ruling WORLD-WDB9-WDB6) -->
After the `.zmesh` is written, the step publishes three properties beside `geom.meshBounds` (`Zenith_CookContext::PublishProperty`). Each is little-endian and written field by field, and each is published for every platform the mesh cooks for.

| Property | Bytes | Read by |
|---|---|---|
| `geom.lod0Summary` | 48: the 32-byte LOD 0 geometry digest, then the bounding sphere's centre as three `f32` and its radius as one `f32` | WORLD's importer through `ReadPublishedProperty` and its editor through the cook server's `property` request, on the Windows platform (A3 rows WD30, WD31) |
| `geom.meshSections` | `u32` LOD count L, `u32` section count S, `u32` material slot count M, `u32` zero; S × `u32` LOD 0 triangle count per section, in section order (0 for an empty part); L × `f32` stored world error per LOD (the LOD record's world error; LOD 0's is 0); S × `u8` material slot per section, then zero bytes to a multiple of 4; M × 16 bytes, the slot material GUIDs (the `GEOM_SECTION_MATERIAL_SLOTS` bytes) | WORLD's HLOD planner: keep mode deals material groups by triangle count, and a recipe chooses a source LOD by stored error (A5 row WdB6) |
| `geom.sourceMesh` | the intermediate-mesh blob of "Merge recipes" | the recipe mode of another mesh's step, through `ReadDependencyOutput` |

- **Digest.** The step calls `Zenith_ReadCookedMeshGeometry` ("Cooked mesh geometry in tools") over the exact bytes it passed to `WriteAssetFile`, for LOD 0, with the context's codec set and scratch arena, and then `Zenith_ComputeMeshGeometryDigest` over the result. WORLD's editor calls the same two functions over the same file, so both produce the same digest. The digest is a function of the decoded LOD 0 alone. Compression, the codec and its library version, `uZENITH_ENGINE_COOK_VERSION` and every `lod.*` setting leave it unchanged unless they change a decoded value.
- **Sphere.** `Zenith_ComputeMeshLod0Sphere` over the same decoded positions. The centre is the centre of their axis-aligned bounding box, computed in `double` and rounded to nearest `f32`. The radius is the largest distance from that `f32` centre to a position, computed in `double` and rounded up to the next `f32`. This sphere is WORLD's, and it is not the header's Ritter sphere.
- **Two properties, not one.** `geom.lod0Summary` names exactly the geometry WORLD's lighting tracer reads, so a LOD-chain setting must not change it. `geom.meshSections` holds the stored LOD errors, which such a setting does change, and only HLOD reads it. WORLD's `Zenith_WorldMeshSummary` keeps its 48 bytes.
- **Failure.** A step that fails publishes none of the four properties, so a dependent never reads values of a file that was not written.
- `Test_MeshCook_PublishesLod0Summary` cooks the two-material split mesh and requires the property's digest to equal `Zenith_ComputeMeshGeometryDigest` of `Zenith_ReadCookedMeshGeometry` over the written file, and its sphere to equal the rounding rule above. The test also re-cooks the mesh with a different compression level and `lod.ratios`, and requires identical bytes. `Test_MeshCook_PublishesMeshSections` requires, for an authored two-level fixture, the counts, errors, slots and GUIDs above, equal to the file's part and LOD records.

### Merge recipes

<!-- elaboration: answers A3 row WD8 and A5 row WdB5 -->
WORLD's importer declares one `ASSET_KIND_MESH` product per HLOD node slot and writes its intermediate `geom.meshMergeRecipe` instead of `mesh` (`Zenith2/Docs/Design/WORLD/Bakes.md`). GEOM owns the recipe's layout, its writer and its reader, which `Zenith_AuthoringGeom.h` declares so that WORLD's authoring code in ZenithAuthoring can call them ("Cooked mesh geometry in tools"):

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringGeom.h additions
enum Zenith_MeshMergeRecipeError : uint8 { MESH_MERGE_RECIPE_ERROR_INVALID_DESC = 1, MESH_MERGE_RECIPE_ERROR_CORRUPT, MESH_MERGE_RECIPE_ERROR_OUT_OF_MEMORY };

struct Zenith_MeshMergeRecipeInstance
{
	Zenith_AssetGUID m_xMesh;                        // a static ASSET_KIND_MESH
	uint8 m_uSourceLod = 0;                         // the source mesh's LOD whose triangles are merged
	float m_afNodeFromInstance[12] = {};             // row-major 3 × 4, the instance relative to the node anchor
	Zenith_AssetGUID m_axMaterialOverrides[8];       // the zero GUID: the source mesh's slot material
};

struct Zenith_MeshMergeRecipeDesc
{
	bool m_bFlatten = false;                         // false: keep mode
	double m_adAnchor[3] = {};                       // absolute world position of the node anchor
	uint32 m_uTriangleTarget = 64;                  // at least 64
	float m_fMaxError = 0.0f;                        // metres, finite and above 0
	Zenith_Span<const Zenith_AssetGUID> m_axMaterials;              // keep: 1 … 255, the output slots in order; flatten: empty
	Zenith_Span<const Zenith_MeshMergeRecipeInstance> m_axInstances;    // 1 … 65,536, in the writer's order (WORLD's ascending entity ID)
};

constexpr uint16 uGEOM_MESH_MERGE_RECIPE_VERSION = 1;

Zenith_Result<void, Zenith_MeshMergeRecipeError> Zenith_WriteMeshMergeRecipe(const Zenith_MeshMergeRecipeDesc& xDesc,
	Zenith_Array<uint8>& auOut);
Zenith_Result<Zenith_MeshMergeRecipeDesc, Zenith_MeshMergeRecipeError> Zenith_ReadMeshMergeRecipe(Zenith_ByteSpan xBlob,
	Zenith_Arena& xArena);   // the spans live in xArena
```

Layout: a 64-byte header, then M × 16-byte material GUIDs, then N × 196-byte instance records.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `'ZMMR'` (0x524D4D5A) |
| 4 | 2 | version, `uGEOM_MESH_MERGE_RECIPE_VERSION` |
| 6 | 1 | mode: 0 keep, 1 flatten |
| 7 | 1 | reserved, zero |
| 8 | 24 | anchor, 3 × `f64` |
| 32 | 4 | triangle target, `u32` |
| 36 | 4 | maximum error, `f32` |
| 40 | 4 | material count M, `u32` |
| 44 | 4 | instance count N, `u32` |
| 48 | 16 | reserved, zero |

An instance record holds the mesh GUID (16 bytes), the source LOD (`u8`), three zero bytes, the matrix (12 × `f32`) and the eight override GUIDs (8 × 16 bytes). The writer refuses a desc outside the ranges in the declaration comments, a non-finite value or a source LOD of 8 or more, with `MESH_MERGE_RECIPE_ERROR_INVALID_DESC`, and writes nothing. The reader applies the same rules to the bytes, and also refuses a wrong magic or version, a non-zero reserved byte or a size other than 64 + 16 M + 196 N, with `MESH_MERGE_RECIPE_ERROR_CORRUPT`. The writer is a pure function of the desc, so an unchanged node writes unchanged bytes.

`geom.sourceMesh` is the intermediate-mesh blob (`'ZIMS'`, written by `Zenith_WriteCookMeshSource`) of the mesh after step 6. It holds the welded vertices with their tangents and source-precision positions, normals, UV0, UV1 and colours, one level per LOD of the chain (that LOD's indices over the welded vertices and its triangle slots, with `m_fScreenCoverage` the stored threshold), the slot table and no morph targets. A skinned mesh publishes none, so a recipe that names one fails.

The recipe mode of `geom.mesh`:

| Step | Action | Failure |
|---|---|---|
| R1 | `ReadIntermediate("geom.meshMergeRecipe")` and `Zenith_ReadMeshMergeRecipe` | `GEOM_COOK_ERROR_MESH_RECIPE`, naming the field |
| R2 | per instance in recipe order: `ReadDependencyOutput(mesh, "geom.sourceMesh")` and `Zenith_ReadCookMeshSource`; take level `m_uSourceLod` | `GEOM_COOK_ERROR_MESH_RECIPE` naming the instance index and the mesh, for an absent property or a level the source lacks |
| R3 | resolve each triangle's material: the override of its slot when the slot is below 8 and the override is not the zero GUID, else the source slot's material GUID. Keep mode keeps a triangle whose material is in the recipe's list, with output slot = its index there, and drops the rest; another slot of the same node takes those. Flatten mode keeps every triangle with output slot 0 | — |
| R4 | transform the kept vertices: position `p' = M [p, 1]` in `float`; normal by the inverse transpose of M's 3 × 3 part, then normalised; tangent direction by the 3 × 3 part, then normalised, with its sign multiplied by the sign of the determinant. An instance whose determinant is negative swaps the second and third index of each of its triangles | `GEOM_COOK_ERROR_MESH_RECIPE` for a singular 3 × 3 part |
| R5 | concatenate the instances in recipe order. The merged mesh has UV0, UV1 or colours when any instance has them, and an instance without one contributes (0, 0) or (1, 1, 1, 1). In flatten mode every vertex colour is its triangle's material's proxy colour ("Flatten mode") | — |
| R6 | merge vertices: two vertices merge when their positions round to the same point of the 1e-4 m lattice (`round(x / 1e-4)` per axis, in `double`) and their other attributes are bit-equal. The first in order is kept | — |
| R7 | `meshopt_simplifyWithAttributes` with the weights and options of "LOD generation", target index count 3 × the triangle target, and target error = maximum error / `meshopt_simplifyScale` | — |
| R8 | continue the pipeline at step 3 with `lod.mode` = `none`: one LOD. The output's slot table is the recipe's material list in keep mode, or the engine material `content:/Engine/Materials/HLODProxy.material.json` (found with `FindAssetByLogicalPath`) in flatten mode | — |

- Key inputs: the recipe bytes; each source's `geom.sourceMesh`, whose key hash joins this key through `ReadDependencyOutput`; in flatten mode each material's `geom.proxyColour`; then the ordinary key inputs of "Cook steps and derived-data keys" (`Zenith2/Docs/Design/GEOM.md`).
- The output is an ordinary `.zmesh`, and it publishes the ordinary properties.

#### Flatten mode

A material's proxy colour is its property `geom.proxyColour`, published by the material step (`Zenith2/Docs/Design/GEOM/Materials.md → "Material cook"`, step 9): four linear `f32`. It is the material's `float4` parameter `m_xBaseColourFactor`, or (1, 1, 1, 1) for a type without one, multiplied per channel by the average colour of the material's first texture binding of usage `COLOUR`. That average is the texture's property `geom.textureAverageColour`, published by the texture step (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`, step 9), or (1, 1, 1, 1) when the material has no such binding or the binding is virtual. A vertex colour stores the proxy colour clamped to [0, 1] as `UNORM8x4`. `Material_HLODProxy` (`Zenith2/Shaders/Materials/HLODProxy.slang`, opaque, reflection flag `readsVertexColour`) reads it as linear base colour, with roughness 1 and metallic 0. Its document is the engine content `Zenith2/Engine/ZenithFlux/Internal/Content/Materials/HLODProxy.material.json`.

`Test_MeshMergeRecipe_WriteReadRoundTrip` (ZenithAuthoring) round-trips a two-instance recipe and refuses one malformed desc and one malformed blob per rule. `Test_MeshCook_RecipeKeepMergesInstances` merges two instances of the two-material split mesh, one of them mirrored, and checks the output slots, the winding and the triangle target. `Test_MeshCook_RecipeRejected` fails a recipe naming a skinned mesh, a missing LOD and a singular matrix. `Test_MeshCook_RecipeFlattenProxyColours` checks one slot and the vertex colours equal to each material's proxy colour.

## Precision fallback report format

Every precision fallback is reported, never silent ([Z2-N-REND-879]). The cook report is ASSET's per-game, per-platform JSON file; GEOM owns its `vertexFallbacks`, `meshes` and `streamSets` members.

<!-- elaboration -->
```json
{
	"vertexFallbacks": [
		{ "mesh": "3c1f0d2e-8a1b-4e4f-9d4a-3b7f2a9c11e0", "source": "Games/OpenWorldSlice/Assets/Props/Crane.glb", "attribute": "position",
		  "metric": 0.00184, "unit": "m", "tolerance": 0.00100, "chosenFormat": "float32x3", "streamSet": "pos-float32x3|surface-qt16-uv16",
		  "rasterStreamSet": "pos-float32x3|surface-qt16-uv16" },
		{ "mesh": "7a02c4b1-5e3d-4c61-8f0e-2d9b6a41c7f3", "source": "Games/CinematicSlice/Assets/Creatures/Drake.glb", "attribute": "position",
		  "metric": 0.00041, "unit": "m", "tolerance": 0.00100, "chosenFormat": "float32x3", "tiedBy": "bPERM_POSITION32",
		  "streamSet": "pos-float32x3|surface-qt16-uv16|skin-j8w8", "rasterStreamSet": "pos-pool32x4|surface-qt16-uv16" },
		{ "mesh": "7a02c4b1-5e3d-4c61-8f0e-2d9b6a41c7f3", "source": "Games/CinematicSlice/Assets/Creatures/Drake.glb", "attribute": "skinnedPool",
		  "metric": 0.00195, "unit": "m", "tolerance": 0.00100, "chosenFormat": "float32x4",
		  "streamSet": "pos-float32x3|surface-qt16-uv16|skin-j8w8", "rasterStreamSet": "pos-pool32x4|surface-qt16-uv16" }
	],
	"meshes": [
		{ "mesh": "3c1f…", "streamSet": "pos-float32x3|surface-qt16-uv16", "lods": 4, "sections": 3, "parts": 9, "vertices": 212004,
		  "indices": 1048576, "splits": 2, "meshlets": 16384, "positionOnlyRatio": 0.61, "bytes": 5921536 }
	],
	"streamSets": [ { "streamSet": "pos-float32x3|surface-qt16-uv16", "meshes": 1 } ]
}
```

| Member | Rule |
|---|---|
| `vertexFallbacks[].attribute` | `position`, `uv0`, `uv1`, `joints`, `weights`, `skinnedPool` |
| `vertexFallbacks[].unit` | `m` for position, weights and skinned pool; `texels` for UVs; `joints` for joint counts |
| ordering | every array sorted by mesh GUID, then attribute name, so the report is deterministic |
| `streamSets` | the input of the shader compiler's `--stream-set-list` ("Program stream-set specialisation" of `Zenith2/Docs/Design/GEOM/VertexFormat.md`) is derived from this array's raster forms |
| `vertexFallbacks[].streamSet`, `.rasterStreamSet` | the mesh's cooked code and its raster form (`Zenith_GetRasterStreamSet`), as canonical names; equal for an unskinned mesh |
| `vertexFallbacks[].tiedBy` | present, with the value `bPERM_POSITION32`, only on a forced `position` record (the record rule below), never on any other record |

Record rule, one statement for every mesh:

| Checks that failed | Records written (in attribute order) |
|---|---|
| each failed check of an unskinned mesh | one record for that check: its own attribute, metric, tolerance and chosen format |
| a skinned mesh: the position check alone | one `position` record (failed; no `tiedBy`). The FLOAT32 pool it forces is visible in `rasterStreamSet` (`pos-pool32x4|…`) and is not a separate record, because the pool check itself passed |
| a skinned mesh: the posed-extent check alone | one `skinnedPool` record (failed: metric `halfError(E)`, tolerance `precision.posedMetres`, chosen format `float32x4`), and one forced `position` record: metric and tolerance are the position check's own passing values, chosen format `float32x3`, and `tiedBy` is `bPERM_POSITION32` |
| a skinned mesh: both | one `position` record and one `skinnedPool` record, both failed, neither with `tiedBy` |
| the joint, weight and UV checks of a skinned mesh | one record each, as for an unskinned mesh |

A record whose metric does not exceed its tolerance is therefore always a forced `position` record with `tiedBy`. The `PRECISION` section of the file holds the same records in the same order.

## The cooked mesh format

A `.zmesh` file is ASSET's cooked container (`Zenith2/Docs/Design/GEOM.md → "GEOM cooked file header"`, which cites `Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"` and "Section table") with magic `'ZMSH'` (0x48534D5A), kind `ASSET_KIND_MESH` and format version 1. ASSET orders the section table by (kind, index) and places the sections in that order, so the file holds the kinds below in ascending value. Every record below is little-endian, written field by field, never by struct copy (Z2-D-038). Byte arrays whose image is a GPU format by design (vertex streams, indices, meshlet arrays) are written as that image.

<!-- elaboration -->
| Section kind | Value | Count | Content |
|---|---|---|---|
| `GEOM_SECTION_MESH_INFO` | 1 | 1 | the mesh info record |
| `GEOM_SECTION_LOD_TABLE` | 2 | 1 | L LOD records |
| `GEOM_SECTION_PART_TABLE` | 3 | 1 | L × S part records |
| `GEOM_SECTION_STREAM` | 4 | one per (LOD, stream) present; section index = LOD × 8 + stream | the packed stream bytes of every part of that LOD, parts in section order |
| `GEOM_SECTION_INDICES` | 5 | one per LOD | the LOD's 16-bit indices, parts in section order |
| `GEOM_SECTION_POSITION_ONLY_INDICES` | 6 | zero or one per LOD | position-only indices, parts in section order |
| `GEOM_SECTION_MESHLETS` | 7 | zero or one per LOD | `Flux_MeshletRecord` byte images |
| `GEOM_SECTION_MESHLET_VERTICES` | 8 | zero or one per LOD | u16 arrays |
| `GEOM_SECTION_MESHLET_TRIANGLES` | 9 | zero or one per LOD | u32 arrays |
| `GEOM_SECTION_SKIN` | 10 | zero or one | `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinned mesh data"` |
| `GEOM_SECTION_MORPH` | 11 | zero or one per LOD | `Zenith2/Docs/Design/GEOM/Skinning.md → "Morph targets"` |
| `GEOM_SECTION_PRECISION` | 12 | 1 | fallback records |
| `GEOM_SECTION_MATERIAL_SLOTS` | 13 | 1 | 16 bytes per material slot, in slot order: the slot's material GUID (cooked binary form), the intermediate's slot table (A3 rows WD9, WD30) |

Mesh info record (112 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | stream-set code (u16) |
| 2 | 1 | LOD count L (1 … 8) |
| 3 | 1 | material slot count (1 … 255) |
| 4 | 2 | section count S (1 … 255, `uGEOM_MAX_SECTIONS`; the field stays 16-bit so widening the instance record field needs no format change) |
| 6 | 2 | flags: bit 0 skinned, bit 1 morph targets, bit 2 has position-only indices, bit 3 has meshlets, bit 4 skinned-pool FLOAT32 layout, bit 5 two-sided hint from the source, bit 6 custom cull distance (the import preset's `cull.customDistance` was true; cross-area rows RP21, RP36, RPB18); bits 7–15 zero, and the reader rejects a set one (`FLUX_GEOMETRY_ERROR_CORRUPT_SECTION`) |
| 8 | 8 | stream-set layout hash (u64): the `axFLUX_STREAMSET_LAYOUTS` entry's hash the streams were packed for |
| 16 | 12 | position scale (3 × f32) |
| 28 | 12 | position bias |
| 40 | 8 | UV0 scale (2 × f32) |
| 48 | 8 | UV0 bias |
| 56 | 8 | UV1 scale |
| 64 | 8 | UV1 bias |
| 72 | 12 | mesh AABB minimum |
| 84 | 12 | mesh AABB maximum |
| 96 | 12 | bounding-sphere centre |
| 108 | 4 | bounding-sphere radius |

LOD record (24 bytes): screen-size threshold f32; world error f32; vertex count u32; index count u32; position-only index count u32; meshlet count u32.

Part record (84 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | material slot (u8) |
| 1 | 1 | flags: bit 0 empty part |
| 2 | 2 | reserved, zero |
| 4 | 4 | first vertex (u32, within the LOD's stream sections) |
| 8 | 4 | vertex count |
| 12 | 4 | first index |
| 16 | 4 | index count |
| 20 | 4 | first position-only index (`0xFFFFFFFF` when none) |
| 24 | 4 | position-only index count |
| 28 | 4 | first meshlet |
| 32 | 4 | meshlet count |
| 36 | 12 | AABB minimum (3 × f32) |
| 48 | 12 | AABB maximum |
| 60 | 12 | bounding-sphere centre (3 × f32, Ritter's sphere of "Bounds and UV density"; the primitive record's `m_xBoundsCentre`) |
| 72 | 4 | bounding-sphere radius (f32; the primitive record's `m_fBoundsRadius`) |
| 76 | 4 | UV0 density (f32) |
| 80 | 4 | UV1 density (f32) |

Precision section: record count u32, then per record (12 bytes), in the order of the report's records for the mesh: attribute u8 (0 position, 1 UV0, 2 UV1, 3 joints, 4 weights, 5 skinned pool), chosen format u8 (`Zenith_VertexFormatType`), flags u8 (bit 0: the record is forced and the report carries `"tiedBy": "bPERM_POSITION32"`; bits 1–7 zero), reserved u8 (zero), metric f32, tolerance f32.

The material-slot section names each slot's default material, the material a mesh instance draws when it overrides nothing. It holds the GUIDs exactly as the importer wrote them, so the CPU readers and the digest of "Published mesh properties" read them from the file.

### Reader validation

`Flux_ReadMeshView(const Zenith_CookedFileView&)` and `Flux_ReadMeshSections(const Zenith_CookedFileView&, Zenith_Span<const Zenith_AssetLoadedSection>)` (in `ZenithFlux/Internal/Geometry/Flux_MeshReader.cpp`, both `…`, returning a `Zenith_Result` whose error is a `Flux_GeometryError`) treat the file as untrusted ([Z2-N-ARCH-951]). The header and the section table are ASSET's: the registry applies checks C1–C12 of `Zenith2/Docs/Design/ASSET/Formats.md → "Checks"` (magic, section-table bounds, header check, reserved fields, format and engine cook versions, platform, kind and GUID, content hash, file size, table order, descriptors, section placement) before the mesh loader's `BeginLoad` runs, and checks each section's stored bytes and decodes it before `OnSectionsLoaded` delivers it (`Zenith_VerifySection`, `DECODE_FAILED`). The mesh reader runs the kind-specific rows alone:

<!-- elaboration -->
| Check | Run by | Error |
|---|---|---|
| the view's magic is `'ZMSH'`, its kind `ASSET_KIND_MESH` and its format version 1: a defence for views not produced by the registry (the fuzz target, tests), which ASSET's C1, C5 and C7 already refuse | `Flux_ReadMeshView` | `FLUX_GEOMETRY_ERROR_BAD_HEADER`, `FLUX_GEOMETRY_ERROR_VERSION_MISMATCH` |
| the section kinds present are among kinds 1–13, each kind with the count and index range of the section table above, and match the mesh info flags; the material-slot section holds 16 bytes × the mesh info's material slot count | `Flux_ReadMeshView` (kinds and indices), `Flux_ReadMeshSections` (flags) | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| counts within the parser limits and S ≤ 255 | `Flux_ReadMeshSections` | `FLUX_GEOMETRY_ERROR_LIMIT` |
| every part's vertex and index ranges inside its LOD's sections; every index below the part's vertex count (one pass over the decoded index section in the load's staging); every meshlet range inside its arrays; decoded section sizes equal the sizes the counts imply | `Flux_ReadMeshSections` | `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| stream-set code valid, declared (an entry of the generated `axFLUX_STREAMSET_LAYOUTS`, found with `Zenith_FindStreamSetLayout`), and its recorded layout hash equal to that entry's | `Flux_ReadMeshSections` | `FLUX_GEOMETRY_ERROR_LAYOUT_MISMATCH` ([Z2-N-REND-872]) |

A failed row makes the loader call `Fail` with the error's `Flux_GeometryErrorCode` as the loader code (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`). The fuzz target `Zenith_Fuzz_MeshFile` (`ZenithFlux/Tests/Fuzz/MeshFile.cpp`) parses arbitrary bytes with `Zenith_ParseCookedHeader` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Parsing and validation functions"`), decodes each section with its descriptor's codec, and drives both reader functions with a seed corpus of the mesh fixtures.

## Flux_MeshResource header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_MeshResource.h
#pragma once
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithVertexFormat/Zenith_VertexStreamSet.h"
#include "ZenithFlux/Flux_GeometryErrors.h"

constexpr uint32 uGEOM_MAX_LODS = 8;
constexpr uint32 uGEOM_MAX_MATERIAL_SLOTS = 255;
constexpr uint32 uGEOM_MAX_SECTIONS = 255;            // bits 0–7 of the instance record's m_uPrimitiveLayout

/// @contract value Flux_MeshID
/// Ownership: Plain value naming a loaded mesh in Flux_MeshManager; the asset handle that loaded it keeps it alive.
/// Thread affinity: IMMUTABLE.
///   Members non-const: it is copy-assigned into descriptors, records and tables.
/// Invalidation: Stale once the mesh's last asset reference is released and its retirement completes; resolution then
///   fails with FLUX_GEOMETRY_ERROR_STALE_ID (generation compare) in every configuration.
/// Units: None.
/// Errors: None; resolution reports staleness.
/// Version: since Z2 Stage 2. Never serialized; the render world carries it as a value.
/// @endcontract
struct Flux_MeshID
{
	uint32 m_uSlot = 0;
	uint32 m_uGeneration = 0;      // 0: the null ID
	constexpr bool IsNull() const { return m_uGeneration == 0; }
	friend constexpr bool operator==(Flux_MeshID, Flux_MeshID) = default;
};

enum Flux_MeshResidency : uint8
{
	FLUX_MESH_RESIDENCY_LOADING = 1,     // file read or upload in flight; instances referencing it are not drawn
	FLUX_MESH_RESIDENCY_RESIDENT,        // primitive records published; drawable
	FLUX_MESH_RESIDENCY_FAILED,          // a load error; instances draw nothing and Flux.Geometry.MeshFailures counted it
	FLUX_MESH_RESIDENCY_RETIRING,        // released; waiting for retirement
};

/// @contract value Flux_MeshPartInfo
/// Ownership: Plain value copied out of the mesh resource.
/// Thread affinity: IMMUTABLE.
///   Members non-const: the producer default-constructs it and fills its fields before returning it.
/// Invalidation: None.
/// Units: Metres (bounds); metres per UV unit (densities).
/// Errors: None.
/// Version: since Z2 Stage 2.
/// @endcontract
struct Flux_MeshPartInfo
{
	uint8 m_uMaterialSlot = 0;
	bool m_bEmpty = false;
	uint32 m_uVertexCount = 0;
	uint32 m_uIndexCount = 0;
	Zenith_AABB m_xBounds;
	float m_fUV0Density = 0.0f;
	float m_fUV1Density = 0.0f;
};

/// @contract type Flux_MeshResource
/// Ownership: Created by Flux_MeshManager when the asset system loads a .zmesh (Flux_MeshKindLoader implements
///   Zenith_AssetKindLoader, Zenith2/Docs/Design/GEOM.md "Asset kind loaders"); shared through
///   Zenith_AssetHandle<Flux_MeshResource>; its pool ranges and primitive records are released through deferred
///   deletion when the last handle drops.
/// Thread affinity: ANY_THREAD for every const member; the resource is immutable once RESIDENT.
/// Invalidation: None while a handle is held; GetPrimitiveBase is meaningful only while GetResidency() == RESIDENT.
/// Units: Metres for bounds; LOD thresholds are fractions of the view height.
/// Errors: Load failures are reported through GetResidency() == FAILED and GetLoadError(); nothing asserts on file data.
/// Version: since Z2 Stage 2. Cooked data re-cooks on any format change (Z2-D-048).
/// @endcontract
class Flux_MeshResource
{
public:
	Flux_MeshID GetID() const;
	Flux_MeshResidency GetResidency() const;           // acquire load
	Flux_GeometryError GetLoadError() const;           // m_eCode 0 unless FAILED
	Zenith_VertexStreamSetCode GetStreamSet() const;   // the cooked code (SKIN included); keys a skinned mesh's skin-source pool
	Zenith_VertexStreamSetCode GetRasterStreamSet() const;   // Zenith_GetRasterStreamSet(GetStreamSet()); keys the raster pool, the buckets and the PSO permutation
	uint32 GetLODCount() const;
	uint32 GetSectionCount() const;                   // S
	uint32 GetMaterialSlotCount() const;
	Zenith_AABB GetBounds() const;
	Zenith_Sphere GetBoundingSphere() const;
	float GetLODScreenSize(uint32 uLOD) const;         // ZENITH_ASSERT uLOD < GetLODCount()
	Flux_MeshPartInfo GetPart(uint32 uLOD, uint32 uSection) const;
	uint32 GetPrimitiveBase() const;                   // first primitive record; 0xFFFFFFFF until the GPU scene has allocated the range
	bool IsSkinned() const;
	uint32 GetMorphTargetCount() const;
	const Zenith_PersistentID& GetSkeletonID() const;   // the Animation.Skeleton asset the skin section names; the null ID when unskinned (A3 row AN4)
	uint64 GetMorphTargetNameHash(uint32 uTarget) const;   // Zenith_Hash64 of target uTarget's name (LOD 0 morph table); ZENITH_ASSERT uTarget < GetMorphTargetCount()
	bool HasCustomCullDistance() const;               // mesh-info flag bit 6: every primitive record carries FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE
	uint64 GetGpuBytes() const;                        // pool bytes this mesh occupies
};

/// @contract type Flux_MeshManager
/// Ownership: Owned by Flux_Renderer, one per device; owns every Flux_MeshResource, the geometry pools and the mesh
///   slot table; destroyed with Flux_Renderer, and a mesh still referenced then is reported with its asset ID, like the
///   device's live-handle report ([Z2-N-CONTRACT-089]).
/// Thread affinity: ANY_THREAD. Resolve and the statistics read published state; loads arrive from the kind loader's
///   OnSectionsLoaded through the bounded load queue and are finalised on the Flux.BuildGraph node, which alone mutates the pools.
/// Invalidation: A Flux_MeshResource* from Resolve is valid until the end of the current render frame.
/// Units: Bytes.
/// Errors: Flux_GeometryError records (Zenith2/Docs/Design/GEOM.md "Threading lifetime and error contract").
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Version: since Z2 Stage 2.
/// @endcontract
class Flux_MeshManager
{
public:
	const Flux_MeshResource* Resolve(Flux_MeshID xID) const;   // null when stale or null
	uint32 GetResidentMeshCount() const;
	uint64 GetPoolBytesUsed() const;
	uint64 GetPoolBytesCapacity() const;
};
```

`Flux_MeshManager` is internal state of the renderer; its load entry points are the members of `Flux_MeshKindLoader`, declared in `ZenithFlux/Internal/Geometry/Flux_MeshLoader.h` (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`). The same header declares `void Flux_MeshWritePrimitiveRecords(const Flux_MeshResource& xMesh, Zenith_Span<Flux_PrimitiveRecord> axOut)`, which fills the L × S record images (`primitive = base + LOD × S + section`, the fields of `Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`) that the GPU scene uploads. The stream-set ID in bits 0–15 of `m_uStreamSetAndLOD` is the mesh's raster code, the code of the pool the part's `m_iVertexOffset` addresses. For a mesh whose mesh-info flag bit 6 is set, every one of its L × S records carries `FLUX_PRIMITIVE_FLAG_CUSTOM_CULL_DISTANCE` (primitive flag bit 5, bit 29 of `m_uStreamSetAndLOD`, `Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`), and `HasCustomCullDistance()` is true; RPROD's extraction resolver reads that accessor to pack the instance's cull distance into its custom data (cross-area rows RP36, RPB18). `GetSkeletonID()` and `GetMorphTargetNameHash()` are the members ANIM's extractor reads for its skeleton check and its morph remap (A3 row AN4 (1)).

### Draw ranges and pool streams for other passes

<!-- elaboration -->
Passes outside GEOM draw a resident mesh's section themselves: the editor's selection mask and VFX's mesh particles (A3 cross-area rows ED12 (d), VX8 (a)). Two public members give them what a primitive record holds, plus the buffers to bind:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_MeshResource.h additions
struct Flux_MeshDrawRange
{
	sint32 m_iVertexOffset = 0;                 // the raster pool element of the section's first vertex
	uint32 m_uFirstIndex = 0;                   // in the index pool, 16-bit indices
	uint32 m_uIndexCount = 0;
	Zenith_VertexStreamSetCode m_eRasterStreamSet{};   // keys the pool GetRasterPoolStreams returns
};

struct Flux_RasterPoolStreams
{
	Flux_VertexStreamRef m_axStreams[3];         // fixed-function streams 0–2 of the raster pool; a stream the code lacks is null
	Flux_IndexBufferRef m_xIndices;              // the index pool
};

// Flux_MeshResource:  Flux_MeshDrawRange GetDrawRange(uint32 uLOD, uint32 uSection) const;
// Flux_MeshManager:   Flux_RasterPoolStreams GetRasterPoolStreams(Zenith_VertexStreamSetCode eRaster) const;
```

- `GetDrawRange` is valid on a `RESIDENT` mesh. It asserts `uLOD < GetLODCount()` and `uSection < GetSectionCount()`, and returns an empty range (index count 0) for an empty part. The values equal the section's primitive record (`m_iVertexOffset`, `m_uFirstIndex`, `m_uIndexCount`).
- `GetRasterPoolStreams` returns the graph references of the raster pool keyed by `eRaster` and of the index pool, for a pass to declare as reads. A code with no resident pool returns null references.
- Both are read on the `Flux.BuildGraph` node, where pools are finalised, and the references are valid for the frame being built. A skinned mesh's positions are in the skinned-vertex pool, so these members serve its unskinned streams alone.
- `Test_Mesh_DrawRangeMatchesPrimitiveRecord` asserts, for the two-material split mesh, that every section's range equals its primitive record and that a test pass drawing each range through the returned streams writes the same pixels as the engine's draw of the instance (WARP and lavapipe).

### CPU geometry of LOD 0

<!-- elaboration -->
Tools read a cooked mesh's LOD 0 positions and indices on the CPU: the editor's precise mesh picking (A3 cross-area row ED12 (d)). `Flux_ReadMeshView` and `Flux_ReadMeshSections` are `Internal/`, so one public reader wraps them:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_MeshGeometryReader.h
struct Flux_MeshCpuSection
{
	uint32 m_uMaterialSlot = 0;
	Zenith_Span<const Zenith_Vector3> m_axPositions;   // dequantised to mesh-local metres
	Zenith_Span<const uint32> m_auIndices;            // three per triangle, section-local vertex indices
};

Zenith_Result<Zenith_Span<const Flux_MeshCpuSection>, Flux_GeometryError> Flux_ReadMeshLOD0Geometry(
	const Zenith_CookedFileView& xFile, Zenith_Span<const Zenith_AssetLoadedSection> axSections, Zenith_Arena& xArena);
```

The reader runs "Reader validation" first and returns its errors unchanged. It then decodes each LOD 0 section's position stream with the section's quantisation box and widens its 16-bit indices. Every span lives in `xArena`, and the function keeps nothing. `Test_MeshGeometryReader_LOD0MatchesSource` checks the dequantised positions of the split mesh against its source within the quantisation step, and `Test_MeshGeometryReader_CorruptRefused` checks that a truncated stream section returns the validation error.

### CPU geometry of one LOD

<!-- elaboration: answers A3 row WD9 (a) with "Cooked mesh geometry in tools" -->
A second function of the same header decodes everything the tools' lighting and merge paths read from one LOD:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_MeshGeometryReader.h additions
struct Flux_MeshCpuGeometry
{
	Zenith_Span<const Zenith_Vector3> m_axPositions;          // mesh-local metres: the LOD's vertices, parts in section order
	Zenith_Span<const Zenith_Vector3> m_axNormals;            // unit normals decoded from stream 1
	Zenith_Span<const Zenith_Vector2> m_axUV1;                // empty when the stream-set has no UV1
	Zenith_Span<const uint32> m_auIndices;                   // three per triangle: the part's first vertex + its 16-bit index
	Zenith_Span<const uint16> m_auTriangleMaterialSlot;      // one per triangle: its part's material slot
	Zenith_Span<const Zenith_AssetGUID> m_axSlotMaterials;    // the GEOM_SECTION_MATERIAL_SLOTS GUIDs, slot order
};

Zenith_Result<Flux_MeshCpuGeometry, Flux_GeometryError> Flux_ReadMeshGeometry(const Zenith_CookedFileView& xFile,
	Zenith_Span<const Zenith_AssetLoadedSection> axSections, uint32 uLod, Zenith_Arena& xArena);
```

- The function runs "Reader validation" first and returns its errors unchanged. A `uLod` not below the LOD count is `FLUX_GEOMETRY_ERROR_INVALID_DESC`, and an exhausted arena is `FLUX_GEOMETRY_ERROR_OUT_OF_MEMORY`.
- Positions decode with `Zenith_DequantisePosition` and the mesh box, or are copied for `FLOAT32X3`. Normals are the normal of `Zenith_DecodeQTangent`, or of `Zenith_DecodeOctahedral` for an `OCT` surface. UV1 decodes through the UV1 box (`UNORM16x2`) or as half floats (`FLOAT16x2`). A skinned mesh returns its bind pose.
- Vertices follow the stream sections' order, so a vertex shared by two parts appears twice, with identical values. Empty parts contribute nothing. The result is a pure function of the file's bytes.
- `Test_MeshGeometryReader_AnyLodMatchesSource` checks LOD 0 of the two-material and split cooked fixtures against their sources within half a quantisation step, the indices and slots against the part table, the duplicated boundary vertices of the split fixture, and the GUIDs against the slot table. `Test_MeshGeometryReader_LodOutOfRangeRefused` checks that `uLod` 1 of a one-LOD fixture and a 64-byte arena fail with their errors.

### Cooked mesh geometry in tools

<!-- elaboration: answers A3 rows WD9 (a) and WD30 (b) -->
WORLD's editor and the mesh step read a cooked file's bytes directly, so GEOM's owner header in ZenithAuthoring (`Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`) wraps the reader:

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringGeom.h additions
using Zenith_CookedMeshGeometry = Flux_MeshCpuGeometry;

struct Zenith_MeshLod0Sphere { float m_afCentre[3] = {}; float m_fRadius = 0.0f; };

Zenith_Result<Zenith_CookedMeshGeometry, Flux_GeometryError> Zenith_ReadCookedMeshGeometry(Zenith_ByteSpan xCookedFile,
	uint32 uLod, const Zenith_CompressionCodecSet& xCodecs, Zenith_Arena& xArena);
Zenith_Sha256Value Zenith_ComputeMeshGeometryDigest(const Zenith_CookedMeshGeometry& xGeometry);
Zenith_MeshLod0Sphere Zenith_ComputeMeshLod0Sphere(Zenith_Span<const Zenith_Vector3> axPositions);
```

- **Reader.** `Zenith_ReadCookedHeaderBytesNeeded` and `Zenith_ParseCookedHeader`, with an expectation of `'ZMSH'`, `ASSET_KIND_MESH` and format version 1 for the file's own platform. Then, per section, `Zenith_VerifySection` and `Zenith_DecompressSection` with `xCodecs` into `xArena`, and finally `Flux_ReadMeshGeometry`. An ASSET header or table error is `FLUX_GEOMETRY_ERROR_BAD_HEADER` (`FLUX_GEOMETRY_ERROR_VERSION_MISMATCH` for a version), and a section check or decode failure is `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION`. The codec set is the fourth parameter that row WD9's three-parameter form lacked: the sections of a cooked file are compressed.
- **Digest.** SHA-256 (`Zenith_Sha256Stream`) over this byte image of the geometry, little-endian: `uint32` vertex count V, `uint32` triangle count T, `uint32` slot count, `uint32` flags (bit 0: UV1 present); then V × 3 `f32` positions, V × 3 `f32` normals, V × 2 `f32` UV1 when present, T × 3 `uint32` indices, T `uint16` triangle material slots, zero bytes to a multiple of 4, and 16 bytes per slot material GUID (A3 row WD30 (a)).
- **Sphere.** As "Published mesh properties" states.
- **Library edges.** These declarations make GEOM's ZenithAuthoring files include ZenithFlux, ZenithAssets and ZenithCompression. The ticket that adds them appends the three to ZenithAuthoring's `DEPENDS` in its own diff, by the dependency growth rule of `Zenith2/Docs/Design/EDIT.md → "ZenithAuthoring contract rules"`; ZenithAuthoring stays outside every Shipping link closure.
- `Test_CookedMeshGeometry_ReadsFileBytes` (ZenithCook's tests, which link ZenithAuthoring) cooks the two-material split fixture in a cook test context and requires `Zenith_ReadCookedMeshGeometry` over the written bytes to equal the source's LOD 0 within half a quantisation step, with its slot GUIDs. `Test_CookedMeshGeometry_CorruptRefused` (the same file) checks that a truncated file and a flipped stored byte return their errors. `Test_CookedMeshGeometry_DigestLayout` (ZenithAuthoring) requires the digest of a three-vertex geometry to equal SHA-256 of a byte image the test writes by hand, with and without UV1. `Test_CookedMeshGeometry_SphereRounding` (ZenithAuthoring) checks the round-to-nearest centre and round-up radius on positions whose `double` distance lies between two `f32` values.

## Geometry pools

Every mesh's vertices and indices are suballocated into unified pools and drawn through `vertexOffset`/`BaseVertexLocation` and `m_uFirstIndex` ([Z2-N-REND-886]). The pools are `Flux_PersistentReadOnlyBuffer` objects (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`), except the procedural-geometry pool, which its producer pass writes and which is therefore a persistent-imported graph resource ([Z2-N-REND-551]).

<!-- elaboration -->
| Pool | One per | Element | Buffer usage union ([Z2-N-REND-889]) |
|---|---|---|---|
| raster stream-set pool | **raster** stream-set code in use by a resident mesh (`Flux_MeshResource::GetRasterStreamSet()`; for an unskinned mesh the cooked code). It owns one buffer per fixed-function stream the raster code has (0–2 for an unskinned code, 1–2 for a pool code), all sharing one element allocator, because one `vertexOffset` addresses every bound stream of a draw | one vertex (the stream's stride per buffer) | `VERTEX` \| `STORAGE_READ` \| `TRANSFER_DST` \| `TRANSFER_SRC` |
| skin-source pool | **cooked** code in use by a resident skinned mesh (SKIN included). It owns two buffers, stream 0 (bind positions) and stream 3 (joints and weights), sharing one element allocator. It is compute-only: skinning and morph dispatches read it, and no raster draw binds it | one vertex | `STORAGE_READ` \| `TRANSFER_DST` \| `TRANSFER_SRC` |
| procedural-geometry pool | device: one buffer, two stream regions sharing one vertex allocator | one vertex | `Zenith2/Docs/Design/GEOM/Procedural.md → "Procedural stream-sets and pool layout"` |
| index pool | device | one 16-bit index | `INDEX` \| `STORAGE_READ` \| `TRANSFER_DST` \| `TRANSFER_SRC` |
| meshlet pools | device: records, vertices, triangles | 32 bytes, 2 bytes, 4 bytes | `STORAGE_READ` \| `TRANSFER_DST` \| `TRANSFER_SRC` |
| morph-delta pool | device | 24 bytes | `STORAGE_READ` \| `TRANSFER_DST` \| `TRANSFER_SRC` |
| skin-data pool | device: inverse-bind matrices are not here (they stay on the CPU, `Zenith2/Docs/Design/GEOM/Skinning.md → "Skinning palette computation"`) | — | — |

A mesh allocates one range in each pool it uses, covering every LOD: LODs in order and, within a LOD, parts in section order, which is the order of the `.zmesh` stream sections. So vertex i of the mesh is at the raster range's base + i and, for a skinned mesh, at the skin-source range's base + i. A part's primitive record carries `m_iVertexOffset` = raster base + the LOD's first vertex + the part's first vertex.

Pool keying and binding, stated once:

| Consumer | What it binds | Keyed by |
|---|---|---|
| raster draw of an unskinned mesh | streams 0–2 of its raster pool at the primitive's `m_iVertexOffset` | the raster code, which equals the cooked code |
| raster draw of a skinned mesh | streams 1–2 of its raster pool (fixed-function, at the primitive's `m_iVertexOffset`), and the skinned-vertex pool as a parameter field (position and QTangent through `Flux_FetchPoolVertex`) | the raster code (`POOL16`/`POOL32`); two skinned meshes whose cooked codes differ only in SKIN share one raster pool and one bucket |
| compute skinning and morph dispatch | streams 0 and 3 from the skin-source pool at the record's `m_iSourceVertexOffset`, and stream 1 (the bind QTangent) from the raster pool at the `m_iVertexOffset` of the record's primitive (`Zenith2/Docs/Design/GEOM/Skinning.md → "Compute skinning dispatch"`) | the cooked code (one dispatch per cooked code) |
| raster draw of a procedural instance | the procedural pool as vertex streams 0 and 1 at its two region offsets | the procedural code (`Zenith2/Docs/Design/GEOM/Procedural.md → "Drawing procedural instances"`) |
| indirect bucket | every draw of the bucket binds the same raster pool | the bucket key of `Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`, whose `Zenith_RHIPermutationKey` stream-set is the raster code |

<!-- assumption: pending core-doc batch 4 (X94) -->
[Z2-N-REND-886]'s "one pool per stream per stream layout" and [Z2-N-REND-887]'s "(pipeline, stream-set)" are read as follows. The buckets, and the pools of every stream a fixed-function draw binds, are keyed by the raster stream-set. A skinned mesh's bind positions and skin streams live in a compute-only skin-source pool keyed by the cooked code, which the skinning dispatch record addresses and no raster draw binds. So one bucket binds one set of stream pools under RPROD's landed key. Cross-doc item X94 applies this wording to the two bullets in core-doc batch 4, and cross-area row GM19 records the reconciliation. RPROD's key derivation states the shadow-bucket reading on its side: "the stream-set component is always the raster code", which is also the key of the fixed-function vertex pool every draw of the bucket binds, while a skinned mesh's bind-position and skin streams live in the compute-only skin-source pool keyed by the cooked code (`Zenith2/Docs/Design/RPROD/Culling.md → "Bucket registry"`, row "key derivation"). Cross-area row GM21 is resolved by that row.
<!-- /assumption -->

### Pool allocator

Each pool's allocator is a two-level segregated-fit free list over element indices (TLSF: first level log2 of the size, second level 8 linear subdivisions), in CPU memory, one per pool:

| Operation | Behaviour | Complexity |
|---|---|---|
| `Allocate(uElements)` | smallest free block of at least `uElements` from the TLSF bins; split; the remainder returns to its bin | O(1) |
| `Free(uFirst, uElements, xLastUse)` | the block enters the device's deferred-deletion queue as a pool-slot object (`Zenith2/Docs/Rendering/GpuResources.md → "Retirement"`); at retirement it merges with free neighbours and returns to its bin | O(1) |
| exhaustion | the pool grows ("Pool growth") | — |

- Ranges are allocated, and freed ranges retired, on the `Flux.BuildGraph` node alone, so the allocator needs no lock; the kind loaders' `OnSectionsLoaded` calls push their objects onto it through the load queue `flux.geometry.loadQueue`, whose capacity bound and full policy are `Zenith2/Docs/Design/GEOM.md → "Lock-order entries"`: the push precedes the load's `Activate` and must not fail (`Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"`), so the queue is sized from the registry's slot count, not from the loaders' concurrency caps.
- Index-pool allocations are rounded up to an even count, so every part's first index is word-aligned for the halfword select of compute reads.

### Pool growth

| Step | Action |
|---|---|
| 1 | an allocation finds no block: the new capacity is max(2 × capacity, capacity + request), clamped so the sum of every geometry pool stays within `Z2-B-GEOM-001` Unified vertex and index pools |
| 2 | create the new buffer(s); add a resize-and-copy graph pass (`GeometryPoolGrow`, a buffer copy of the old contents, [Z2-N-REND-553]) before the frame's `StreamingUpload` pass |
| 3 | every subsequent import and every bucket's vertex-stream binding uses the new buffer; primitive records and skinning dispatch records keep their element offsets, so no record changes. Raster, skin-source, index, meshlet and morph pools grow by this procedure, and the procedural pool by its two-region variant (`Zenith2/Docs/Design/GEOM/Procedural.md → "Pool layout"`) |
| 4 | the old buffer retires through deferred deletion with the last-use values of the last frame that bound it |
| 5 | at the budget clamp, the allocation fails with `FLUX_GEOMETRY_ERROR_POOL_FULL`; the mesh load reports `FAILED`, `Flux.Geometry.PoolFull` increments, and no resident mesh is evicted |

- Initial capacities: 32 MiB per raster or skin-source pool stream buffer, 32 MiB index pool, 8 MiB per meshlet pool on `FLUX_CAPABILITY_TIER_DESKTOP`; a quarter of each on `FLUX_CAPABILITY_TIER_MOBILE`; these are proposed values that the Stage 1 per-system budget ticket for GEOM replaces with the allocation of `Z2-B-GEOM-001`.

## Mesh residency state machine

The rows are driven by the members of `Zenith_AssetKindLoader` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetKindLoader header"`) as `Flux_MeshKindLoader` implements them (`Zenith2/Docs/Design/GEOM.md → "Asset kind loaders"`), and by the GPU residency sub-state of `Zenith2/Docs/Design/ASSET/Loading.md → "GPU residency sub-state"`.

<!-- elaboration -->
| State | Event | Next state | Action |
|---|---|---|---|
| — | `BeginLoad(xContext)`: the registry delivers the validated `Zenith_CookedFileView` in `GetInfo().m_pxFile` | `LOADING` (sections) | run `Flux_ReadMeshView` ("Reader validation"); request every section of the file in one round (`RequestSections`), each with a destination of its decoded size in a staging allocation of the manager's heap, so ZenithAssets decodes in place and no copy follows; return within 1 ms |
| `LOADING` (sections) | `OnSectionsLoaded` with the decoded sections | `LOADING` (queued) | run `Flux_ReadMeshSections` over the decoded bytes; push the mesh onto the manager's load queue (the infallible push of "Geometry pools"; the mesh object holds the context, the staging and the parsed records); then `Activate(pMesh)`: the asset becomes ACTIVE with GPU residency PENDING before any drain runs, so a mesh can be ACTIVE before the first frame renders (`Zenith2/Docs/Design/ASSET/Loading.md → "Delivery and activation"` step 3, "GPU residency sub-state") |
| `LOADING` (sections) | a reader row fails | — | `Fail(code)` with the `Flux_GeometryErrorCode` (`CORRUPT_SECTION`, `LAYOUT_MISMATCH`, `LIMIT`) as the loader code; the staging is released; the load reports `FAILED` and `Flux.Geometry.MeshFailures` increments. `Fail` is used only before `Activate` |
| `LOADING` (queued) | `Flux.BuildGraph` drains the request | `LOADING` (uploading) | allocate the mesh's ranges (the raster pool; for a skinned mesh also the skin-source pool; the index, meshlet and morph pools); `RequestUpload` one splittable request per section from the staging into its range (a skinned mesh's stream sections 0 and 3 into the skin-source range, 1 and 2 into the raster range); a request refused with `FLUX_UPLOAD_ERROR_QUEUE_FULL` stays in the mesh's pending work and is retried at the next drain |
| `LOADING` (queued) | the drain finds a pool at its budget clamp | `FAILED` | free every range allocated so far through deferred deletion and release the staging; `Zenith_AssetRegistry::ReportGpuFailed(GetInfo().m_xResidency, FLUX_GEOMETRY_ERROR_POOL_FULL)`: the asset stays ACTIVE with residency FAILED, the mesh never draws, and only a reload or a device recovery clears it; `Flux.Geometry.MeshFailures` increments |
| `LOADING` (uploading) | every upload operation reports `COMPLETE` | `RESIDENT` | release the staging; `Zenith_AssetRegistry::ReportGpuResident(GetInfo().m_xResidency)` (residency RESIDENT); the mesh is drawable. The GPU scene allocates, reference counts and writes its L × S primitive records when the first instance delta references it, from `Flux_MeshWritePrimitiveRecords`, and frees them after the last (`Zenith2/Docs/Design/RHI/Memory.md → "Slot allocators"`); `GetPrimitiveBase` is published with release ordering when the range exists |
| `LOADING` (sections) | `Cancel(xContext)` (the load is cancelled before activation) | `RETIRING` | release the staging; then `AcknowledgeCancel()`. The push and `Activate` run in one `OnSectionsLoaded` call, so a queued mesh has activated, and a loader that has called `Activate` is never sent `Cancel` |
| `LOADING` (uploading) | an upload reports `FAILED` | `FAILED` | free ranges and records through deferred deletion and release the staging; `ReportGpuFailed(GetInfo().m_xResidency, FLUX_GEOMETRY_ERROR_UPLOAD_FAILED)`: residency FAILED, so the mesh never draws; `Flux.Geometry.MeshFailures` increments |
| `LOADING` (queued) | `Unload(xContext)` | `RETIRING` | mark the mesh unloaded; it stays in the load queue, and the drain that pops it allocates nothing, releases the staging and calls `AcknowledgeUnload()`, so a queued mesh is never destroyed under the queue and its slot is not reused while it is queued |
| `LOADING` (uploading), `RESIDENT` | `Unload(xContext)` (last reference dropped, keep-alive eviction, reload retirement, shutdown) | `RETIRING` | `Cancel()` every queued upload (one already `STAGED` completes and is discarded, [Z2-N-REND-495]); free ranges and records with the current frame's last-use values; release the staging if still held; `AcknowledgeUnload()` once every free is queued |
| `RETIRING` | retirement completes | — | the slot's generation increments; the ID is stale |
| `FAILED` | `Unload(xContext)` | — | free the slot; `AcknowledgeUnload()` |
| `RESIDENT`, `LOADING` (queued), `LOADING` (uploading), `FAILED` | a device-recovery `BeginLoad` (`GetInfo().m_bDeviceRecovery`, `m_pExistingObject` this mesh; `Zenith2/Docs/Design/ASSET/Loading.md → "Device-loss reload"`) | `LOADING` (sections) | the rows above run for the existing object: `OnSectionsLoaded` pushes it and activates with `Activate(m_pExistingObject)`, and the drain re-allocates its ranges in the recreated pools and re-uploads them; a mesh still in the load queue from its earlier attempt is not pushed a second time: its queued request takes the recovery attempt's ticket and staging, and the earlier staging is released; the object's address, ID and asset handle are unchanged |

- An instance whose mesh is not `RESIDENT` is kept by the GPU scene with its primitive base unset and is not drawn; it becomes drawable in the first frame after `RESIDENT` (`Test_Mesh_InstanceDrawsAfterResident`).
- Cancellation at any point leaves no pool range, primitive record, upload staging or allocation behind: `Test_Mesh_LoadCancel1000Cycles` issues and cancels 1,000 loads at randomised stages (fixed seed, printed) and asserts every pool's free bytes, the primitive table's free count and the test heap's live bytes return to their start values ([Z2-N-CONTRACT-339]).
- Device loss: the recover path of `Zenith2/Docs/Rendering/GpuResources.md → "Device loss"` step 4 calls ZenithAssets' `ReloadGpuResidencySet`, which sends every active mesh a device-recovery load (the last row); the manager keeps no bytes of a resident mesh, only the object ZenithAssets names in `m_pExistingObject`.
- `DropCpuCopies` returns 0 for meshes: the staging, the only CPU copy of a mesh's GPU data, is released at `RESIDENT` or on failure, so no RESIDENT mesh holds one.

## LOD selection

The selection runs on the GPU in the culling dispatch ([Z2-N-REND-648], `Zenith2/Docs/Rendering/Culling.md → "Indirect draw compaction"`) and on the CPU for transparent collection (`Zenith2/Docs/Rendering/Culling.md → "Transparent commands"`); both call one algorithm, `Flux_SelectLOD` in `Zenith2/Shaders/Common/LOD.slang` and `Flux_SelectLODCPU` in `ZenithFlux/Flux_LOD.h`, and they agree bit for bit because both are written in kernel operations (`Zenith2/Docs/Design/GEOM/VertexFormat.md → "The exact arithmetic kernel"`).

<!-- elaboration -->
```
ProjectedSize(xSphereCentreWS, fRadiusWS, xLODView):          // s: projected diameter as a fraction of the view height
	if (xLODView.m_uFlags & FLUX_LOD_VIEW_FLAG_ORTHOGRAPHIC) != 0:
		return VfMul(fRadiusWS, xLODView.m_fProjScaleY)          // r × P[1][1] (P[1][1] = 2 / (top − bottom))
	d = VfSqrt(VfDot3(c − xLODView.m_xPositionWS, c − xLODView.m_xPositionWS))
	if d <= fRadiusWS: return +infinity                          // the camera is inside the sphere: LOD 0
	return VfMul(VfMul(fRadiusWS, xLODView.m_fProjScaleY), VfRcp(d))   // r × g / d

SelectLOD(s, afThreshold[L], L, xLODView):
	s' = VfMul(s, xLODView.m_fLODScale)                           // the quality tier's LOD scale (default 1.0)
	l = 0
	while l < L − 1 and s' < afThreshold[l]: l = l + 1
	l = min(l + xLODView.m_uLODBias, L − 1)                       // r.lodBias, the pressure bias (none today)
	if xLODView.m_uForcedLOD != 0xFFFFFFFF:                       // debug: r.debug.forceLOD; 0xFFFFFFFF when unset
		l = min(xLODView.m_uForcedLOD, L − 1)                     // forces any level, finer or coarser
	return l
```

| Input | Source |
|---|---|
| `fRadiusWS`, centre | the primitive's `m_xBoundsCentre`/`m_fBoundsRadius` of LOD 0 section 0's record, transformed by the instance matrix (`m_fMaxScale` × radius) |
| `m_fLODScreenSize` of LOD l | the primitive record of (LOD l, section 0) (`Zenith2/Docs/Rendering/GpuResources.md → "Primitive record (128 bytes)"`); every section of a LOD carries the same value |
| `xLODView` | `Flux_LODView` in the culling pass's parameters (the layout below) |

`Flux_LODView` is a `[FluxShared]` struct of `Zenith2/Shaders/Common/LOD.slang` in the scalar layout (32 bytes, no `bool`, [Z2-N-REND-134]):

| Offset | Size | Type | Field | Meaning |
|---|---|---|---|---|
| 0 | 12 | `float3` | `m_xPositionWS` | the LOD reference view's position, world space |
| 12 | 4 | `float` | `m_fProjScaleY` | `P[1][1]` of the reference view's projection |
| 16 | 4 | `float` | `m_fLODScale` | the quality tier's LOD scale, default 1.0 |
| 20 | 4 | `uint` | `m_uLODBias` | `r.lodBias` plus the pressure bias |
| 24 | 4 | `uint` | `m_uForcedLOD` | `r.debug.forceLOD`, `0xFFFFFFFF` when unset |
| 28 | 4 | `uint` | `m_uFlags` | bit 0 `FLUX_LOD_VIEW_FLAG_ORTHOGRAPHIC`; bits 1–31 zero |

`FLUX_LOD_VIEW_FLAG_ORTHOGRAPHIC` = 1 is a Slang `static const uint` of `LOD.slang`, emitted into the generated header with the struct.

- One LOD is selected per instance, and every section of the instance uses it, so a multi-material object never mixes LODs.
- Shadow views select LOD from their LOD reference view, which is the main view of their world (or the first editor view when no main view exists), not from the light, so a shadow always shows the geometry the camera sees and a cascade never switches LOD independently; capture, texture and preview views use their own camera.
- There is no hysteresis: the GPU selection is stateless per frame, and temporal anti-aliasing absorbs the one-frame pop.

<!-- assumption: pending RPROD -->
`Flux_InstanceCullParams` (`Zenith2/Docs/Rendering/PerPass/Common.md → "InstanceCullPhase1"`) gains the plain-data field `Flux_LODView m_xLODView` in the diff that implements LOD selection; `Flux_LODView` is a `[FluxShared]` struct of `Common/LOD.slang`.
<!-- /assumption -->

## Instancing

Instancing in Zenith2 is GPU-driven: every visible instance of one draw key (material slot, LOD) in one bucket becomes one instance of one indirect command ([Z2-N-REND-648], refined by `Zenith2/Docs/Design/RPROD/Culling.md → "Draw keys and command regions"`); there is no CPU instance batching and no per-instance vertex stream for GPU-scene draws.

<!-- elaboration -->
| Element | Rule |
|---|---|
| primitive records | one set of L × S records per mesh, shared by every instance of it |
| material-slot records | one range of S records per (mesh, material assignment), allocated and reference counted by the GPU scene (`Zenith2/Docs/Design/RHI/Memory.md → "Slot allocators"`), so a thousand trees with the same materials share one range. `Flux_MaterialSlotCache` is GEOM's view of each range: keyed by (mesh ID, the S material IDs after overrides), it supplies the record images and acquires and releases the range's buckets (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`). A range is freed through deferred deletion when its count reaches zero ([Z2-N-REND-538]) |
| per-instance data | `m_uCustomData` (32 bits, `Zenith2/Docs/Rendering/GpuResources.md → "Instance record (128 bytes)"`) and `m_uLayerMask`; a material reads `m_uCustomData` through `Flux_MaterialInput::m_uInstanceIndex` and `Flux_GetInstanceCustomData(uInstanceIndex)` of `Common/GPUScene.slang` |
| render-world input | an instance delta names a `Flux_MeshID`, up to S `Flux_MaterialID` overrides (null = the mesh's default material of that slot) and the custom data; the delta record is RPROD's `Flux_RenderInstanceDelta` |
| unit of compaction | the draw key (material slot, LOD) of `Zenith2/Docs/Design/RPROD/Culling.md → "Draw keys and command regions"`: one indirect command per (material slot, LOD) per bucket, whose instance count is the number of visible instances of that draw key |
| ordering | visible instances are ordered by instance index within each draw key ([Z2-N-REND-650]), so instancing is deterministic |
| capacity | the instance table's capacity ([Z2-N-REND-537]); 100k foliage instances of W-Outdoor are GPU-scene instances like any other |

## Terrain geometry

The geometry side of terrain is GEOM's; terrain sculpting, painting, materials, collision and navigation generation are ENV's (Z2-I-ENV-01, -02, -11, -12). The method is continuous distance-dependent LOD (CDLOD): a quadtree of nodes, each drawn as one instance of one shared grid mesh whose vertices are displaced by a height field in the vertex stage and morphed toward the next coarser grid near a LOD boundary, so adjacent nodes of different levels meet without cracks and without stitching strips.

<!-- elaboration -->
### Terrain data

| Datum | Format | Owner |
|---|---|---|
| height tiles | `R16_UNORM` textures of 257 × 257 texels (256 quads plus a shared border: the last row and column of a tile equal the first of its neighbour), height = `heightMin + v / 65535 × (heightMax − heightMin)`; one tile per `256 × sampleSpacing` metres square, 128 m × 128 m at the default 0.5 m spacing (`terrain.sampleSpacing`); 9 mips by **point decimation**, never averaging (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`, step 3) | quantised by GEOM's terrain cook function from `Zenith_TerrainGeometryCookInput` ("Terrain cook contract"), which the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain` builds from its terrain document (`Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`); written by that importer as each height product's `geom.textureSource` intermediate (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`), cooked by GEOM's texture step and streamed as textures of usage `FLUX_TEXTURE_USAGE_HEIGHT` (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture usage classes"`) |
| min/max pyramid | per quadtree node, the minimum and maximum of every level-0 sample inside the node's closed rectangle, its border samples included (f32 pair), computed at cook for every node of every level | GEOM (`.zterrain` section 0x0102, "Terrain cook contract") |
| hole mask | optional `R8_UNORM` tile per height tile, 257 × 257, one texel per level-0 sample (the same sample grid and shared border as the height tile), 0 = hole, 255 = solid, with the same point-decimated mip rule | ENV authors its 256 × 256 quad mask and converts it to this sample mask before the hand-off, by the rule of `Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"`; GEOM validates, cooks and samples the sample mask ("Terrain cook contract") |
| grid mesh | 33 × 33 vertices (32 × 32 quads, 2,048 triangles), stream-set `terrain:pos-snorm16x4` (code `0x1008`), positions (a, 0, b) at the integer grid indices (a, b) ∈ [0, 32]², one engine-content mesh, each quad split along its (a, b)–(a + 1, b + 1) diagonal, the split ENV's CPU height query and PHYS's heightfield use (`Zenith2/Docs/Design/ENV/Terrain.md → "Triangle interpolation"`, A3 row EN2 (e)). The vertex stage recovers a and b by rounding the dequantised x and z to the nearest integer, which is exact because the SNORM16 quantisation error over the [0, 32] box is at most 16 / 65,534 < 0.5 | GEOM, generated by the cook for engine content |
| terrain record | `Flux_TerrainRecord` below, one per terrain in a persistent buffer of `uGEOM_MAX_TERRAINS` records written through the upload ring, at the terrain's record index R ("Terrain record constant") | GEOM, `[FluxShared]` in `Common/Terrain.slang` |

GEOM's cooked terrain data inside ENV's `.zterrain` is the four sections 0x0101 (geometry header), 0x0102 (the pyramid), 0x0103 (height-tile table) and 0x0104 (hole-tile table), laid out in "Terrain cook contract"; the tiles themselves are texture sub-assets.

<!-- elaboration -->
Extent rule: the terrain's extent along each axis is a whole number of tiles, `m_fExtentX = m_uTilesX × 256 × m_fSampleSpacing` (1,024 m = 8 tiles at 0.5 m). The terrain cook step rejects any other source extent with `GEOM_COOK_ERROR_TERRAIN_EXTENT`, naming the terrain, the axis, the extent and the two nearest multiples (for 1,000 m at 0.5 m: 896 m and 1,024 m). It never pads or crops, because a padded height field would render ground ENV did not author. `Flux_TerrainRenderer::AddTerrain` rejects a descriptor whose extents, spacing and tile counts disagree with `FLUX_GEOMETRY_ERROR_INVALID_DESC`. So `m_uSamplesX = 256 × m_uTilesX + 1` samples span exactly `m_fExtentX`, and the last sample index along x is `m_uSamplesX − 1`.

The height mip rule, which the crack-free argument below rests on: mip l of a tile has `(256 >> l) + 1` texels per axis (257, 129, 65, 33, 17, 9, 5, 3, 2 for l = 0 … 8), and its texel (i, j) is, bit for bit, texel (i·2^l, j·2^l) of mip 0. So mip l's spacing is `sampleSpacing × 2^l`, every mip keeps the tile's shared border, and every texel of every mip is a level-0 sample. A height is therefore a function of its global level-0 sample index alone, whatever mip it is read from.

`Flux_TerrainRecord` (scalar layout, 80 bytes):

| Offset | Size | Type | Field | Meaning |
|---|---|---|---|---|
| 0 | 12 | `float3` | `m_xOriginWS` | the terrain's minimum corner; global sample (0, 0) |
| 12 | 4 | `float` | `m_fSampleSpacing` | Δ, metres between level-0 samples |
| 16 | 4 | `float` | `m_fExtentX` | metres |
| 20 | 4 | `float` | `m_fExtentZ` | metres |
| 24 | 4 | `float` | `m_fHeightMin` | metres |
| 28 | 4 | `float` | `m_fHeightMax` | metres |
| 32 | 4 | `uint` | `m_uLevelCount` | quadtree levels, at most 12 |
| 36 | 4 | `uint` | `m_uTilesX` | tiles along x |
| 40 | 4 | `uint` | `m_uTilesZ` | tiles along z |
| 44 | 4 | `float` | `m_fLeafNodeSize` | L0, metres |
| 48 | 8 | `DescriptorHandle<ByteAddressBuffer>` | `m_xHeightTileTable` | `m_uTilesX × m_uTilesZ` row-major 8-byte `DescriptorHandle<Texture2D<float>>` values, one per height tile, rewritten by `Flux_TerrainRenderer` on `Flux.BuildGraph` whenever a streaming swap or a whole-chain replacement publishes a new texture for a tile ("Reallocate-and-swap realisation" and "Texture replace from memory" of `Zenith2/Docs/Design/GEOM/Textures.md`) |
| 56 | 8 | `DescriptorHandle<ByteAddressBuffer>` | `m_xHoleTileTable` | the same for hole masks; an entry of the null handle means no holes in that tile |
| 64 | 4 | `float` | `m_fLODDistanceRatio` | r, at least 8 |
| 68 | 4 | `uint` | `m_uFlags` | bit 0 the terrain has any hole tile; bits 1–31 zero |
| 72 | 4 | `uint` | `m_uSamplesX` | global level-0 samples along x, `256 × m_uTilesX + 1` (the extent rule) |
| 76 | 4 | `uint` | `m_uSamplesZ` | along z |

A node instance's `m_uCustomData` (32 bits, which Rendering documents as per-instance data a material may read) is decoded by the terrain adapter `Zenith2/Shaders/Common/TerrainGrid.slang`, which both the vertex stage and the terrain material call: bits 0–3 the node's level l, bits 4–17 the node's x index and bits 18–31 its z index, both in units of the level's node size (so the node's first global sample index is `(x, z) × 32 × 2^l`). Which terrain record a node reads is the `m_uTerrainRecord` constant of the terrain's material: the record index R that `AddTerrain` assigns and writes into it before the terrain's first node instance exists ("Terrain record constant"); each terrain owns one material-slot range. `Test_Terrain_RecordLayout` asserts the table above and the custom-data packing. The adapter also fills the terrain material's `Flux_MaterialInput` UVs (A3 row EN2 (d)): `m_xUV0` is the fragment's world position XZ in metres, `m_xUV0Grad` its explicit screen-space gradients (the hardware derivatives of world XZ in raster, the analytic derivatives in the visibility-buffer resolve), and `m_xUV1`, `m_xUV1Grad` equal them, so a terrain material scales its layer UVs from world metres (`Zenith2/Docs/Design/ENV/Terrain.md → "Layer blend in the terrain material"`); `Test_Terrain_AdapterUVIsWorldXZ` asserts it.

<!-- assumption: pending RPROD -->
`R16_UNORM` (sampled; the terrain reads it with texel `Load` alone, so no linear-filter feature is needed) is needed by terrain; `R8_UNORM` is in `Zenith2/Docs/Rendering.md → "Required format features"`, `R16_UNORM` is not, and joins the table, `FormatFeatures.table` and the profile in the diff that implements terrain geometry ([Z2-N-REND-038]).
<!-- /assumption -->

### Terrain cook contract

GEOM's terrain geometry cook is a function, not a registered cook step: `Flux.Terrain` is ENV's kind (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`: owner ENV, geometry sections GEOM), and the `Environment.TerrainGeometry` stage of ENV's importer `env.terrain` (`Zenith2/Docs/Design/ENV/Bake.md → "Step graph"`) builds the input from its validated document and calls the function (`Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"`, which cites this heading). The function's version `uGEOM_TERRAIN_COOK_VERSION` enters that importer's version, and so its import key.

```cpp
// Zenith2/Tools/ZenithCook/Internal/Terrain/Zenith_CookTerrainGeometry.h
constexpr uint32 uGEOM_TERRAIN_COOK_VERSION = 1;       // bumped by any change to the function's outputs
constexpr uint32 uGEOM_TERRAIN_TILE_SAMPLES = 257;     // per axis: 256 quads plus the shared border
constexpr uint32 uGEOM_TERRAIN_MAX_TILES_PER_AXIS = 256;

/// @contract value Zenith_TerrainGeometryCookTile
/// Ownership: Plain value; the spans are borrowed from the importer stage that builds the input, for the duration of the call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Metres for heights.
/// Errors: Zenith_CookTerrainGeometry validates it (GEOM_COOK_ERROR_TERRAIN_TILE).
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TerrainGeometryCookTile
{
	uint16 m_uTileX = 0;                                // grid cell, 0 … m_uTilesX − 1
	uint16 m_uTileZ = 0;                                // 0 … m_uTilesZ − 1
	Zenith_AssetGUID m_xHeightTexture;                   // the tile's height .ztex product (key "texture:tile/<x>_<z>/height")
	Zenith_AssetGUID m_xHoleTexture;                     // the tile's hole .ztex product (key "texture:tile/<x>_<z>/holes"); null exactly when m_auHoleMask is empty
	Zenith_Span<const float> m_afHeights;                // 257 × 257 binary32 metres, sample (s, t) at index t × 257 + s
	Zenith_Span<const uint8> m_auHoleMask;              // empty, or 257 × 257 in the same order: 0 = hole, 255 = solid
};

/// @contract value Zenith_TerrainGeometryCookInput
/// Ownership: Plain value built by the Environment.TerrainGeometry stage of ENV's importer env.terrain; the tile span is
///   borrowed for the call.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Zenith_CookTerrainGeometry validates it (GEOM_COOK_ERROR_TERRAIN_EXTENT, GEOM_COOK_ERROR_TERRAIN_TILE).
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TerrainGeometryCookInput
{
	Zenith_Vector3 m_xOriginWS;                          // the minimum corner: global sample (0, 0)
	float m_fExtentX = 0.0f;                             // the document's extents, checked against the tile counts
	float m_fExtentZ = 0.0f;
	uint16 m_uTilesX = 0;                               // 1 … 256
	uint16 m_uTilesZ = 0;
	float m_fSampleSpacing = 0.5f;                       // Δ, metres between level-0 samples; finite, > 0
	float m_fHeightMin = 0.0f;                           // the tight range over every sample, widened by ENV to at least 0.01 m
	float m_fHeightMax = 0.0f;
	float m_fLODDistanceRatio = 8.0f;                    // r, [8, 64] ("Node selection")
	Zenith_Span<const Zenith_TerrainGeometryCookTile> m_axTiles;   // exactly one per grid cell, sorted by (m_uTileZ, m_uTileX)
};

/// @contract value Zenith_TerrainGeometryCookOutput
/// Ownership: Owned by the caller; the arrays are constructed on the calling step's heap and filled by the call.
/// Thread affinity: OWNER_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: None; the call's result reports failures, after which the contents are unspecified.
/// Version: since Z2 Stage 3. Tool-side.
/// @endcontract
struct Zenith_TerrainGeometryCookOutput
{
	Zenith_Array<uint8> m_auGeometrySection;            // section 0x0101, 56 bytes
	Zenith_Array<uint8> m_auPyramidSection;             // section 0x0102
	Zenith_Array<uint8> m_auHeightTileSection;          // section 0x0103
	Zenith_Array<uint8> m_auHoleTileSection;            // section 0x0104
	Zenith_Array<uint16> m_auHeightSamples;             // per input tile, in input order: 257 × 257 quantised samples, the height sub-asset's source image
	Zenith_Array<uint8> m_auHoleSamples;                // per input tile with a hole mask, in input order: its 257 × 257 bytes, the hole sub-asset's source image
};

/// @contract functions Zenith_CookTerrainGeometryFunctions
/// Ownership: No state.
/// Thread affinity: ANY_THREAD; pure: the outputs are a function of the input bytes alone.
/// Invalidation: None.
/// Units: Metres; bytes.
/// Errors: a Zenith_TerrainGeometryCookError with GEOM_COOK_ERROR_TERRAIN_EXTENT or GEOM_COOK_ERROR_TERRAIN_TILE and the axis, tile or
///   rule as detail; Zenith_DescribeTerrainGeometryCookError names the terrain, the tile and the rule; no input asserts.
/// Version: since Z2 Stage 3. Tool-side; uGEOM_TERRAIN_COOK_VERSION.
/// @endcontract
struct Zenith_TerrainGeometryCookError
{
	Zenith_GeomCookCode m_eCode;   // GEOM_COOK_ERROR_TERRAIN_EXTENT or GEOM_COOK_ERROR_TERRAIN_TILE
	uint32 m_uDetail = 0;         // EXTENT: the axis (0 x, 1 z); TILE: the tile index (z × m_uTilesX + x), or the rule number for rule 2
};
Zenith_Result<void, Zenith_TerrainGeometryCookError> Zenith_CookTerrainGeometry(const Zenith_TerrainGeometryCookInput& xInput,
	Zenith_TerrainGeometryCookOutput& xOutput);
Zenith_String Zenith_DescribeTerrainGeometryCookError(const Zenith_TerrainGeometryCookInput& xInput,
	const Zenith_TerrainGeometryCookError& xError);   // the diagnostic text: the axis, the extent and the two nearest multiples, or the tile(s) and the rule
```

The function returns its own error record, not a `Zenith_CookError`: ASSET's `Zenith_CookErrorCode` is closed (ruling ENV-F7, cross-area row EnB26), and ENV's importer stage carries the error as `IMPORT_ERROR_SOURCE_INVALID` (so `COOK_ERROR_IMPORT_FAILED`) with one ERROR diagnostic starting with `Zenith_ToString(m_eCode)` followed by `Zenith_DescribeTerrainGeometryCookError` (`Zenith2/Docs/Design/GEOM.md → "Cook diagnostics"`).

Validation, in this order, each failure returning the error and writing nothing further:

| # | Rule | Error |
|---|---|---|
| 1 | `m_uTilesX`, `m_uTilesZ` in 1 … 256; `m_fSampleSpacing` finite and > 0; `m_fExtentX = m_uTilesX × 256 × m_fSampleSpacing` and likewise z (the extent rule of "Terrain data"); the level count `ceil(log2(max extent / (32 × Δ))) + 1` at most 12 | `GEOM_COOK_ERROR_TERRAIN_EXTENT`, naming the axis, the extent and the two nearest multiples |
| 2 | origin, range and ratio finite; `m_fHeightMax − m_fHeightMin ≥ 0.01`; `m_fLODDistanceRatio` in [8, 64] | `GEOM_COOK_ERROR_TERRAIN_TILE` |
| 3 | `m_axTiles` holds exactly `m_uTilesX × m_uTilesZ` tiles, one per grid cell, sorted by (z, x): the terrain is a dense grid. A cell ENV treats as absent is passed as a tile whose border samples are the shared border samples of its neighbours, whose other samples are `m_fHeightMin`, and whose hole mask is all 0; ENV's quad-to-sample conversion counts every quad of an absent cell as a hole quad, so the shared border samples are 0 on both sides of the border and the cell draws nothing | `GEOM_COOK_ERROR_TERRAIN_TILE` |
| 4 | per tile: `m_afHeights` has 66,049 finite values within `[m_fHeightMin, m_fHeightMax]`; `m_auHoleMask` is empty or has 66,049 bytes each 0 or 255; `m_xHeightTexture` is non-null; `m_xHoleTexture` is null exactly when the mask is empty | `GEOM_COOK_ERROR_TERRAIN_TILE` |
| 5 | per pair of neighbouring tiles: the shared border columns (x) and rows (z) of heights and of hole masks are bit-identical (a tile without a mask counts as all 255) | `GEOM_COOK_ERROR_TERRAIN_TILE`, naming both tiles |

Outputs, all little-endian and written field by field (Z2-D-038):

1. Height quantisation per sample: `v = floor((h − hMin) / (hMax − hMin) × 65535 + 0.5)` evaluated in `double`, clamped to `[0, 65535]`; the decode is the formula of "Terrain data". This is the formula of `Zenith2/Docs/Design/ENV/Terrain.md → "Height representation"`, so the R16 tile and ENV's tile CPU data hold identical values. The 257 × 257 samples of each tile are appended to `m_auHeightSamples`; the importer writes them, and each tile's hole bytes (`m_auHoleSamples`), as the `geom.textureSource` intermediates of the products `texture:tile/<x>_<z>/height` and `texture:tile/<x>_<z>/holes`, with usage `HEIGHT` and `HOLE_MASK` (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture source image intermediate"`), and GEOM's texture step cooks each, its step 3 building the 9-mip point-decimated chain (`Zenith2/Docs/Design/GEOM/Textures.md → "Texture cook pipeline"`).
2. The pyramid: at level l (0 … levels − 1, node size `32 × Δ × 2^l`), `n_l = ceil(8 × tiles / 2^l)` nodes per axis, so no node lies wholly outside the terrain; node (x, z) has index `o_l + z × n_lx + x` with `o_l` the node count of every finer level, and its entry is the (min, max) of the decoded heights of every level-0 sample of its closed rectangle clipped to `[0, samples − 1]` per axis (border samples included), as two `f32`. `Flux_TerrainDesc::m_afNodeMinMax` and `Flux_TerrainRenderer::UpdateNodeBounds` index nodes the same way.
3. The four sections, each index 0 and each present in every terrain, in GEOM's range 0x0100 … 0x01FF of the `.zterrain` section kinds:

| Section kind | Value | Content |
|---|---|---|
| `GEOM_SECTION_TERRAIN_GEOMETRY` | 0x0101 | the geometry header below (56 bytes) |
| `GEOM_SECTION_TERRAIN_PYRAMID` | 0x0102 | the node min/max pyramid |
| `GEOM_SECTION_TERRAIN_HEIGHT_TILES` | 0x0103 | the height-tile table |
| `GEOM_SECTION_TERRAIN_HOLE_TILES` | 0x0104 | the hole-tile table |

Section 0x0101 `GEOM_SECTION_TERRAIN_GEOMETRY` (56 bytes):

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | origin (3 × f32): `Flux_TerrainDesc::m_xOriginWS` |
| 12 | 4 | sample spacing Δ (f32) |
| 16 | 4 | extent X (f32) |
| 20 | 4 | extent Z (f32) |
| 24 | 4 | height min (f32) |
| 28 | 4 | height max (f32) |
| 32 | 4 | LOD distance ratio r (f32) |
| 36 | 2 | tiles X (u16) |
| 38 | 2 | tiles Z (u16) |
| 40 | 4 | level count (u32, 1 … 12) |
| 44 | 4 | node count N (u32, Σ `n_lx × n_lz`) |
| 48 | 4 | flags (u32): bit 0 the terrain has a hole tile; bits 1–31 zero |
| 52 | 4 | reserved, zero |

Section 0x0102 `GEOM_SECTION_TERRAIN_PYRAMID`: N entries of 8 bytes (min f32, max f32), level-major from level 0, row-major within a level, in node-index order.

Section 0x0103 `GEOM_SECTION_TERRAIN_HEIGHT_TILES`: tile count u32 (`tiles X × tiles Z`), then per grid cell in row-major order (`tz × tiles X + tx`) the height sub-asset's 16-byte `Zenith_AssetGUID` in its binary form; no entry is null.

Section 0x0104 `GEOM_SECTION_TERRAIN_HOLE_TILES`: the same shape; an entry is the hole sub-asset's GUID, or 16 zero bytes for a tile without a hole mask.

The importer writes the four sections as the terrain product's intermediate `geomSections`, and ENV's ASSET-phase step `env.terrain` of the kind `Flux.Terrain` writes them into the `.zterrain` unchanged through `Zenith_CookedFileWriter::AddSection` (`Zenith2/Docs/Design/ENV/Bake.md → "Section kind registry"` lists them in GEOM's range 0x0100–0x01FF). ENV's `Flux.Terrain` kind loader (`ZenithFlux/Internal/Environment/Flux_TerrainKindLoader.cpp`) reads them, resolves the tile GUIDs to `Flux_TextureID`s and builds `Flux_TerrainDesc` for `AddTerrain`. It reads them through GEOM's section reader, declared in `ZenithFlux/Internal/Terrain/Flux_TerrainSectionReader.h`:

```cpp
// Fills every scalar field of xDescOut (the tile spans and m_xMaterial are the caller's), the pyramid and the two GUID
// tables, reading the decoded sections field by field; the caller resolves the GUIDs to Flux_TextureIDs.
Zenith_Result<void, Flux_GeometryError> Flux_ReadTerrainGeometrySections(
	Zenith_Span<const Zenith_AssetLoadedSection> axSections, Flux_TerrainDesc& xDescOut, Zenith_Array<float>& afNodeMinMaxOut,
	Zenith_Array<Zenith_AssetGUID>& axHeightTilesOut, Zenith_Array<Zenith_AssetGUID>& axHoleTilesOut);
```

It returns `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` for a missing or duplicated section of the four, a section whose size disagrees with its counts, a level or node count that disagrees with the extents and spacing, a reserved field or flag bit that is not zero, or a null height GUID, and `FLUX_GEOMETRY_ERROR_LIMIT` for tile counts above 256; it allocates only into the caller's arrays and never asserts on the bytes.

### Node selection

| Constant | Value |
|---|---|
| leaf node size L0 | 16 m (32 quads at 0.5 m) |
| levels | enough that the root covers the terrain: `ceil(log2(max(extentX, extentZ) / L0)) + 1`, at most 12 (7 for the 1,024 m fixture: levels 0 … 6, root 1,024 m) |
| edge | a node whose rectangle lies wholly outside `[0, extentX] × [0, extentZ]` is never emitted and never subdivided; a node that straddles the far edge (possible at levels whose node size exceeds a tile when the tile count is not a power of two) is emitted, its selection distance, frustum test and min/max bounds use its rectangle clipped to the terrain, and its vertices beyond the edge are clamped onto it ("Morphing and displacement") |
| LOD distance of level l | `D_l = L0 × 2^l × r`, with `r = terrain.lodDistanceRatio` (default 8.0; `AddTerrain` rejects `r < 8` with `FLUX_GEOMETRY_ERROR_INVALID_DESC`, because the crack-free argument needs it) |
| morph range | the last 30% of each level's distance: morph start `0.7 × D_l`, end `D_l` |
| distance | every distance in selection and morphing is the **horizontal** (XZ) distance from the LOD reference position, computed with kernel operations; altitude never coarsens terrain LOD |

```
SelectNodes(node, level, camera):                       // CPU, once per frame per terrain, on Flux.BuildGraph
	if node's XZ rectangle lies wholly outside the terrain: return
	rect   = node's XZ rectangle clipped to the terrain; bounds = rect × [minHeight, maxHeight]
	if not intersects(bounds, union of the LOD reference views' frusta ∪ the shadow cascades' bounds): return
	if level == 0 or distanceXZ(camera, rect) > D_{level−1}:
		emit(node, level); return
	for each child (in the order −x−z, +x−z, −x+z, +x+z): SelectNodes(child, level − 1, camera)
```

- The camera is the LOD reference view's position ("LOD selection"), for every view, so every view of a frame draws the same node set and shadows match the visible terrain.
- Each emitted node is one GPU-scene instance: local-to-world `T(node.minX, minHeight, node.minZ) × S(nodeSize, max(maxHeight − minHeight, 0.01), nodeSize)`, `m_uCustomData` as "Terrain data" packs it, flags `FLUX_INSTANCE_FLAG_CASTS_SHADOW`, primitive = the grid mesh's, so culling (frustum, HiZ, two phases) treats nodes like any instance.
- `Flux_TerrainRenderer` keeps a node-to-instance map; a node that stays selected keeps its instance slot, so the per-frame GPU-scene delta is only the nodes that entered or left.
- Residency, from Stage 4 when texture streaming lands; in Stage 3 every tile is resident with its full chain and no clamp occurs: `Flux_TerrainRenderer` states each tile's required mip to the texture streamer every frame through `Flux_TextureStreamer::SetExternalRequirement` (mip 0 for a tile any level-0 node within `2 × D_0` of the camera overlaps; its tail otherwise). Every mip ≥ 1 of a height or hole tile lies in the tail and is always resident, so only level-0 nodes can find a mip missing. A level-0 node whose tile has mip 0 not yet resident is not emitted at level 0; its parent is emitted at level 1 instead, and `Flux.Terrain.ResidencyClamped` counts it. The crack-free guarantee below holds on frames where that counter does not increase.

### Morphing and displacement

Every position is computed from integer global sample indices; nothing is derived from a float grid coordinate.

```
TerrainVertex(a, b in [0, 32], instance, terrain, lodCameraWS):     // Common/Terrain.slang, vertex stage, precise
	(l, nx, nz) = TerrainGrid_Unpack(instance.m_uCustomData)
	s     = 1 << l                                             // level-0 samples per level-l quad
	G     = (nx × 32 + a) × s, (nz × 32 + b) × s               // global level-0 index of the grid vertex (uint2)
	T     = G − ((a & 1) × s, (b & 1) × s)                      // odd grid indices morph toward index − 1 on the coarser grid
	G, T  = min(G, samples − 1), min(T, samples − 1)            // per axis: the edge clamp; a no-op inside the terrain
	h0    = Height(G, l)
	d     = distanceXZ(lodCameraWS, WorldXZ(G))
	k     = d ≥ D_l ? 1 : (d ≤ 0.7 D_l ? 0 : (d − 0.7 D_l) / (0.3 D_l))   // kernel operations; exactly 0 or 1 at the ends
	x, z  = VfAdd(VfMul(WorldXZ(G), 1 − k), VfMul(WorldXZ(T), k))    // exact: equals WorldXZ(G) at k = 0 and WorldXZ(T) at k = 1
	h     = VfAdd(VfMul(h0, 1 − k), VfMul(Height(T, l), k))
	normal = Normal(T, l)
	return (x, h, z), normal

WorldXZ(G)   = m_xOriginWS.xz + float2(G) × m_fSampleSpacing               // kernel VfAdd, VfMul; float2(G) exact below 2^24
Height(G, l) = m = min(l, 8)
               tx = min(G.x >> 8, m_uTilesX − 1); tz = min(G.y >> 8, m_uTilesZ − 1)
               ix = (G.x − 256 tx) >> m;          iz = (G.y − 256 tz) >> m
               uTile = tz × m_uTilesX + tx                                   // the row-major tile table index
               v = m_xHeightTileTable[uTile].Load(int3(ix, iz, m)); return heightMin + v × (heightMax − heightMin)
Normal(T, l) = s = 1 << l; per axis c ∈ {x, z}, in signed 32-bit arithmetic:
               lo_c = max(T.c − s, 0); hi_c = min(T.c + s, samples_c − 1)
               dh/dc = (Height(hi_c) − Height(lo_c)) / ((hi_c − lo_c) × m_fSampleSpacing)   // the other axis at T.c
               normal = VfNormalize3((−dh/dx, 1, −dh/dz))
```

`Height` reads a texel with `Load` (no sampler and no filter). A level l ≥ 8 vertex sits on a tile corner, which mip 8 holds, and every G a level-l node uses is a multiple of 2^min(l, 8), so `ix` and `iz` are always integer texels of mip m. `G >> 8` maps an index on a tile border, G = 256(t + 1), to the upper tile t + 1 at its local index 0, except at the terrain's far edge, where the clamp selects the last tile at its local index 256; the shared border makes both choices read the same level-0 sample. The edge clamp keeps every index inside `[0, samples − 1]`: a straddling node's vertices beyond the far edge collapse onto it and form zero-area triangles, so no geometry extends past the terrain, and an interior vertex is never clamped. `Normal` uses central differences at mip `min(l, 8)` inside the terrain. At the first and the last sample it uses the one-sided difference that the clamp produces, whose denominator is the clamped index span: at the last sample, `hi = samples − 1 = T`, so the span is s. Normals are computed in signed arithmetic, so `T − s` never wraps.

Why adjacent nodes meet without cracks, on frames without a residency clamp (`Test_Terrain_NoCracksAcrossLevels` tests the three facts analytically and by selection, and `Test_Terrain_NoCracksFlyThrough` by rendering):

| Fact | Argument |
|---|---|
| a height is a function of G alone | the point-decimated mip rule of "Terrain data": `Height(G, l)` equals mip 0 at G for every l for which G is a multiple of 2^min(l, 8) |
| adjacent nodes differ by at most one level | if node A at level l is emitted, its parent P was subdivided, so `distanceXZ(camera, P) ≤ D_l`; every point of P's rectangle is then within `D_l + √2 × L0 × 2^(l+1) = D_l (1 + 2√2 / r) ≤ 1.354 D_l` for r ≥ 8; a neighbour C at level ≥ l + 2 needs `distanceXZ(camera, C) > D_{l+1} = 2 D_l`, but C touches A's edge, which lies inside P |
| on a boundary between A (level l) and B (level l + 1), A is fully morphed and B not at all | every vertex of A on the shared edge lies in B's rectangle, and `distanceXZ(camera, B) > D_l` because B was emitted at level l + 1, so A's k is exactly 1 there and A's odd edge vertices land on T, B's grid vertices; every vertex of B on that edge lies in P's rectangle, so its distance is at most `1.354 D_l < 0.7 D_{l+1} = 1.4 D_l`, and B's k is exactly 0 there |

With k exactly 0 or 1 the morph arithmetic returns `WorldXZ` and `Height` of an integer index unchanged, both sides compute them with the same kernel operations from the same integers, and so every shared vertex has bit-identical position on both sides. Two nodes of the same level share edge vertices with the same G, the same k and the same T. Normals are not part of the guarantee: they are central differences at mip `min(l, 8)`, so a level boundary can show a shading change but never a gap.

- A node over a hole tile is drawn in the alpha-tested bucket with the `bPERM_TERRAIN_HOLES` variant of the terrain programs. Its fragment stage reads the hole mask at the same mip `m = min(l, 8)` as the node's heights, with `Load` at the texel nearest the fragment's global sample coordinate (`G = min(round((p.xz − origin) / (Δ × 2^m)) × 2^m, samples − 1)` per axis, the level-0 index of that texel, then the tile and local index exactly as `Height` computes them from G), and discards where the value is 0. Holes are therefore aligned to the grid of the level that draws them. Nodes over tiles with no hole mask stay in the opaque bucket.
- Terrain draws in every path through the family permutation (FAMILY TERRAIN): the prepass, the opaque pass, shadow depth, the G-buffer (Stage 5) and, in the visibility-buffer path (Stage 5), the visibility raster and material resolve through the manual route with the same `TerrainVertex` function.

### Terrain public API

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Terrain.h
constexpr uint32 uGEOM_MAX_TERRAINS = 1024;                  // live terrains per Flux_Renderer: the record buffer's capacity
constexpr uint32 uGEOM_TERRAIN_RECORD_INVALID = 0xFFFFFFFF;  // GetRecordIndex of a null or stale ID

/// @contract value Flux_TerrainDesc
/// Ownership: Plain value passed by const reference; the tile handles are asset handles the caller holds.
/// Thread affinity: IMMUTABLE.
///   Members non-const: callers default-construct it and fill its fields.
/// Invalidation: None.
/// Units: Metres.
/// Errors: Flux_TerrainRenderer::AddTerrain validates it (FLUX_GEOMETRY_ERROR_INVALID_DESC), including the extent rule of
///   "Terrain data": each extent equals its tile count × 256 × m_fSampleSpacing.
/// Version: since Z2 Stage 3 (ruling Q45; pending core-doc batch 4 (Q45)).
/// @endcontract
struct Flux_TerrainDesc
{
	Zenith_Vector3 m_xOriginWS;                  // the minimum corner
	float m_fExtentX = 0.0f;
	float m_fExtentZ = 0.0f;
	float m_fSampleSpacing = 0.5f;
	float m_fHeightMin = 0.0f;
	float m_fHeightMax = 0.0f;
	float m_fLODDistanceRatio = 8.0f;             // at least 8 ("Node selection")
	Zenith_Span<const Flux_TextureID> m_axHeightTiles;   // row-major, extent / (256 × m_fSampleSpacing) per axis, a whole number
	Zenith_Span<const Flux_TextureID> m_axHoleTiles;     // empty, or one per height tile (null ID: no holes there)
	Zenith_Span<const float> m_afNodeMinMax;             // the cooked min/max pyramid, two floats per node, level-major
	Flux_MaterialID m_xMaterial;                 // the terrain's own material; its type declares uint m_uTerrainRecord
};

/// @contract type Flux_TerrainRenderer
/// Ownership: Owned by Flux_Renderer.
/// Thread affinity: RENDER_SUBMIT_THREAD for every member; ENV's extractor passes terrain changes through the render
///   world (Flux_RenderEnvironment's terrain list) and the renderer applies them in Flux.BuildGraph.
/// Invalidation: A Flux_TerrainID is stale after RemoveTerrain. Its record index stays reserved until the last frame
///   that could read its Flux_TerrainRecord retires, and is reused only after that.
/// Units: Metres.
/// Errors: FLUX_GEOMETRY_ERROR_INVALID_DESC (the descriptor; m_uDetail 10 for an m_xMaterial whose type declares no
///   m_uTerrainRecord or that already backs a live terrain), FLUX_GEOMETRY_ERROR_MISSING_DEPENDENCY (m_xMaterial null,
///   stale or FAILED), FLUX_GEOMETRY_ERROR_LIMIT (uGEOM_MAX_TERRAINS live terrains), FLUX_GEOMETRY_ERROR_POOL_FULL (the
///   material-constants pool, for the record-constant block), FLUX_GEOMETRY_ERROR_TABLE_FULL (instance table).
///   GetRecordIndex never fails: a null or stale ID returns uGEOM_TERRAIN_RECORD_INVALID; it neither allocates nor
///   asserts ("Terrain record constant").
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3 (ruling Q45; pending core-doc batch 4 (Q45)).
/// @endcontract
class Flux_TerrainRenderer
{
public:
	Zenith_Result<Flux_TerrainID, Flux_GeometryError> AddTerrain(const Flux_TerrainDesc& xDesc);
	void RemoveTerrain(Flux_TerrainID xTerrain);
	// Editor sculpting: ENV re-cooks the edited tiles in memory and swaps them in through texture streaming's swap;
	// the renderer recomputes the node min/max of the edited rectangle from the new pyramid values.
	Zenith_Result<void, Flux_GeometryError> UpdateNodeBounds(Flux_TerrainID xTerrain, uint32 uFirstNode,
		Zenith_Span<const float> afMinMax);
	uint32 GetSelectedNodeCount(Flux_TerrainID xTerrain) const;   // 0 while PENDING_CONSTANTS
	// The terrain's record index R in 0 … uGEOM_MAX_TERRAINS − 1: the slot of its Flux_TerrainRecord and the value
	// AddTerrain wrote into its material's m_uTerrainRecord. Constant from AddTerrain's return to RemoveTerrain.
	uint32 GetRecordIndex(Flux_TerrainID xTerrain) const;
};
```

`Flux_TerrainID` is an 8-byte slot-and-generation value with the contract of `Flux_MeshID`.

### Terrain record constant

A terrain's record index R names its `Flux_TerrainRecord` in GEOM's record buffer, and ENV indexes its own `Env_TerrainGpuRecord` table by the same R (`Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"`), so the terrain material reads both records through one constant, `m_uTerrainRecord`. R depends on which terrains are live, so it is never cooked: a generated terrain material document carries 0, a placeholder that no draw reads, and `AddTerrain` writes the real value. The material's cooked info record names the constant's offset (`Zenith2/Docs/Design/GEOM/Materials.md → "The cooked material format"`, field "terrain record offset").

`AddTerrain` runs on `Flux.BuildGraph` (ENV's `Flux_EnvironmentRenderer::ApplyRecords` calls it there), in this order; a failing step returns its error, undoes the steps before it and leaves no terrain, no record and no reserved index:

| # | Step | Failure |
|---|---|---|
| 1 | validate the descriptor: the extent rule of "Terrain data", r in [8, 64], one height tile per grid cell, an empty or full hole span, two floats per node in `m_afNodeMinMax` | `INVALID_DESC`, `m_uDetail` the field index |
| 2 | resolve `m_xMaterial` through `Flux_MaterialManager::Resolve` | `MISSING_DEPENDENCY` for a null or stale ID or a material whose load or GPU residency is FAILED |
| 3 | read the material's terrain record offset | `INVALID_DESC`, `m_uDetail` 10, when it is 0xFFFFFFFF (the type declares no `uint m_uTerrainRecord`) |
| 4 | check that the material backs no live terrain of this renderer (ENV refuses such an ADD first, `Flux.Environment.TerrainMaterialShared`; this is the defence) | `INVALID_DESC`, `m_uDetail` 10 |
| 5 | take R: the lowest index in 0 … `uGEOM_MAX_TERRAINS` − 1 that no live terrain holds and that no frame in flight can read (a removed terrain's index returns to the free set when the last frame that could reference its record retires, deferred deletion's last-use values) | `LIMIT`, `m_uDetail` 1,024 |
| 6 | write the terrain's 80-byte `Flux_TerrainRecord` at index R through the upload ring | — |
| 7 | bind R to the material: the material manager records the pair and creates a new constants block, the material's current parameter bytes with the four bytes at the terrain record offset set to R (little-endian), uploaded and swapped exactly as a parameter replacement (`Zenith2/Docs/Design/GEOM/Materials.md → "Material slots and constants"`, rows "parameter replacement" and "terrain record binding") | `POOL_FULL` when the constants pool is at its clamp |
| 8 | enter `PENDING_CONSTANTS` and return the ID; `GetRecordIndex` returns R from this return on | — |

The node instances are inserted on the build-node step that publishes the block, never before. A terrain in `PENDING_CONSTANTS` emits no node and holds no instance or material-slot range. Step (2) of `Zenith2/Docs/Design/GEOM.md → "Frame-level flow of GEOM work"`, in the first frame whose start finds the block's upload complete, publishes the block and moves the terrain to `ACTIVE`; step (5) of the same frame runs its first `SelectNodes`, and the GPU scene creates its slot range with the new block's offset. So no node of the terrain draws with the cooked placeholder 0 or with another terrain's index, and the first draw comes at the earliest one frame after `AddTerrain`. `GetSelectedNodeCount` is 0 in `PENDING_CONSTANTS`; `Flux.Terrain.PendingConstants` counts the terrains in it.

| Event while the binding exists | Effect |
|---|---|
| a parameter replacement of the material (`Flux_MaterialResource::ReplaceParameters`, EDIT's live tuning) | the manager overwrites the caller's four bytes at the offset with R before packing, so a tuning edit never clears the index |
| a texture swap that rewrites the material's block | the new block copies the current bytes, R included |
| device loss | the terrain returns to `PENDING_CONSTANTS` and drops its node instances; the material's recovery load writes R into the block it re-creates, and the terrain re-selects on the frame that block is published |
| `RemoveTerrain` | the node instances are removed, the binding is cleared (a block the manager creates for the material afterwards keeps the bytes it has), and R is released with the current frame's last-use values |

`Test_Terrain_RecordConstantBeforeFirstDraw` asserts every row of both tables.

## Mesh tests

Cook tests live in `Zenith2/Tools/ZenithCook/Tests/Geometry/`, runtime tests in `Zenith2/Engine/ZenithFlux/Tests/Geometry/`.

<!-- elaboration -->
| Test | File | Asserts |
|---|---|---|
| `Test_Cook_MeshSplitSharesQuantisation` | `Zenith_CookMesh_Tests.cpp` | a 200,000-vertex fixture splits into parts whose shared boundary vertices decode bit-identically ([Z2-N-REND-895]) |
| `Test_Cook_MeshSplitNoCracksRendered` | `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_MeshRender_Tests.cpp` | the split fixture rendered at 4K against its unsplit FLOAT32 reference shows no background pixel inside the silhouette (zero pixels of the clear colour where the reference has geometry) on WARP and lavapipe |
| `Test_Cook_PrecisionFallbacksReported` | `Zenith_CookMesh_Tests.cpp` | fixture meshes that fail each check get the listed fallback and one report record; a 200 m mesh stays SNORM16x4 by default and falls back to FLOAT32x3 under a preset tightening both position tolerances ([Z2-N-REND-895]) |
| `Test_Cook_NoFallbackIsSilent` | same | for every fixture, the report's `vertexFallbacks` records for the mesh and the `.zmesh` precision section have equal counts, and record k of each matches field by field: `attribute` ↔ the attribute code (position 0 … skinnedPool 5), `chosenFormat` ↔ the chosen format, `metric` and `tolerance` ↔ the f32 values (the JSON numbers parse to the same binary32 values), and `tiedBy` present ↔ flags bit 0 set; the records follow the record rule of "Precision fallback report format" |
| `Test_Cook_OptimisationOrder` | same | a spy build of the meshoptimizer wrapper records the call sequence weld, vertex cache, overdraw, fetch remap, split, shadow indices, meshlets |
| `Test_Cook_SplitAt65535` | same | a fixture of exactly 65,535 unique vertices stays one part; 65,536 makes two; no part exceeds 65,535 |
| `Test_Cook_SectionsStableAcrossLODs` | same | a two-material mesh whose LOD 0 splits material 0 into three parts has S = 4, and every LOD's section 3 is material 1 |
| `Test_Cook_SectionLimit` | same | a fixture of 255 material slots with one part each cooks with S = 255; 256 sections (a slot split into two parts) fails with `GEOM_COOK_ERROR_MESH_LIMIT` naming the mesh, S = 256 and the slot; the reader rejects a file whose S field is 256 with `FLUX_GEOMETRY_ERROR_LIMIT` |
| `Test_Cook_PartBoundsAndUVDensity` | `Zenith_CookMesh_Tests.cpp` | analytic fixtures: a 2 m × 2 m quad with UV [0, 1]² gives ρ = 2 m per UV unit; a part whose UV area is zero stores 0; each part's AABB and Ritter sphere, and the mesh's union bounds, equal the closed-form values of the fixture |
| `Test_Cook_StreamSetUndeclared` | same | `Zenith_CookResolveStreamSet` given the unreachable code `pos-snorm16x4|surface-oct16-uv16|skin-j8w8` and the generated table, and given the reachable default code and a fixture copy of the table with that entry removed, both fail with `GEOM_COOK_ERROR_STREAM_SET_UNDECLARED` naming the canonical name; the mesh step writes no file |
| `Test_Cook_ChoiceReachesOnlyDeclaredCodes` | `Zenith_CookStreamSet_Tests.cpp` | `Zenith_CookChooseStreamSet` over the full product of its inputs (source UV0, UV1 and colour present or not; each read by a material or not; lightmapped or not; a normal-mapped material or not; skinned or not; joints ≤ or > 256; each of the position, UV0, UV1, weight and posed-extent checks passing or failing, and each UV check's FLOAT16 metric lower or not) returns only reachable codes, and between them every one of the 216 cooked MESH codes; their raster forms are exactly the 72 + 36 raster codes; a skinned fixture with a FLOAT16 UV0 and a lightmapped fixture with a FLOAT16 UV1 cook successfully |
| `Test_Cook_SkinnedPositionTie` | same | a skinned fixture failing only the posed-extent check cooks with POS `FLOAT32X3`, raster `POOL32` and two records (`skinnedPool` failed; `position` forced with its own passing metric and tolerance, `tiedBy` `bPERM_POSITION32`, precision-section flags bit 0); one failing only the position check cooks with the same codes and one `position` record without `tiedBy`; one failing both writes both records, neither tied |
| `Test_Cook_PositionOnlyIndicesCanonical` | same | position-only indices reference one vertex per distinct quantised position; a mesh above the threshold stores none |
| `Test_Cook_MeshletLimits` | `Zenith_CookMeshlets_Tests.cpp` | every meshlet has at most 64 vertices and 128 triangles; triangle bytes are below the meshlet's vertex count |
| `Test_Cook_MeshletCoverage` | same | the union of meshlet triangles equals the part's triangles |
| `Test_Cook_LODThresholdsMonotonic` | `Zenith_CookLOD_Tests.cpp` | generated thresholds never increase with the level, the last is 0, and each equals the pixel-error formula |
| `Test_Cook_LODGenerationStops` | same | a fixture that cannot simplify below 0.9 of its index count produces one LOD |
| `Test_Cook_LODDeterministic` | same | two cooks of the same inputs give byte-identical LOD sections |
| `Test_CookMesh_CleanEqualsIncremental` | `Zenith_CookMesh_Tests.cpp` | a clean cook and an incremental cook of the fixture set give byte-identical files |
| `Test_Cook_WindingSuspectWarns` | same | an inside-out fixture raises `GEOM_COOK_WARNING_MESH_WINDING_SUSPECT` |
| `Test_Mesh_ReaderRejectsCorruption` | `Flux_MeshReader_Tests.cpp` | one mutated fixture per row of "Reader validation" returns its error and allocates nothing |
| `Test_Mesh_VersionMismatchRefused` | same | a `.zmesh` with format version 2, and one with another engine cook version, is refused by the registry with `ASSET_ERROR_VERSION_MISMATCH` (check C5 of `Zenith2/Docs/Design/ASSET/Formats.md → "Checks"`) before `Flux_MeshKindLoader::BeginLoad` runs (a spy records no call), and its instances draw nothing; `Flux_ReadMeshView` given a parsed view with format version 2 returns `FLUX_GEOMETRY_ERROR_VERSION_MISMATCH` |
| `Test_Mesh_InstanceDrawsAfterResident` | `Flux_MeshManager_Tests.cpp` | an instance referencing a loading mesh records zero draws until the frame after `RESIDENT`, then draws |
| `Test_Mesh_LoadCancel1000Cycles` | same | 1,000 randomised cancels leave pools, tables and the heap at their start values |
| `Test_Mesh_PoolGrowPreservesContents` | `Flux_GeometryPools_Tests.cpp` | growth preserves every resident vertex and index bit for bit and redraws identically |
| `Test_Mesh_PoolFullFailsLoad` | same | an allocation beyond the budget clamp reports `ReportGpuFailed` with `POOL_FULL` (residency FAILED, the load itself ACTIVE, never `Fail`) and evicts nothing |
| `Test_Mesh_FreedRangeReusedAfterRetirement` | same | a freed range is not reallocated before every queue passed its last use |
| `Test_Mesh_BufferUsageUnion` | same | every pool buffer is created with its usage union, and the union appears in the physical-resource cache key |
| `Test_LOD_SelectionAnalytic` | `Flux_LOD_Tests.cpp` | analytic spheres at known distances select the expected LOD for perspective and orthographic views, the tier scale and the bias; `m_uForcedLOD` = 0 forces LOD 0 where selection alone gives 2, a forced value above L − 1 gives L − 1, and `0xFFFFFFFF` leaves selection unchanged |
| `Test_LOD_CpuGpuAgree` | same | `Flux_SelectLOD` on the GPU and `Flux_SelectLODCPU` agree for 10^5 fixed cases; the generated `Flux_LODView` is 32 bytes with the offsets of "LOD selection" |
| `Test_Mesh_PoolsKeyedByRasterAndCookedCode` | `Flux_GeometryPools_Tests.cpp` | two skinned meshes whose cooked codes differ only in SKIN (`…|skin-j8w8`, `…|skin-j16w16`) share one raster pool and one bucket per kind (their raster code, the `m_xPermutation` stream-set of `Flux_BucketKey`) and get two skin-source pools; each mesh's vertex i sits at its raster base + i and its skin-source base + i; a bucket's draws all bind the same stream buffers; an unskinned mesh's pool, raster code and bucket stream-set are one code and it has no skin-source range |
| `Test_LOD_ShadowUsesReferenceView` | same | a shadow cascade selects the LOD the main view selects for the same instance |
| `Test_Instancing_MaterialSlotsShared` | `Flux_Instancing_Tests.cpp` | 1,000 instances with one material assignment share one slot range; an override creates a second |
| `Test_Instancing_CustomDataReachesMaterial` | same | a test material writing `m_uCustomData` to colour renders each instance's value |
| `Test_CookTerrain_ExtentRejected` | `Zenith2/Tools/ZenithCook/Tests/Terrain/Zenith_CookTerrain_Tests.cpp` | a 1,000 m source at 0.5 m fails with `GEOM_COOK_ERROR_TERRAIN_EXTENT` naming 896 m and 1,024 m, and writes no file; 1,024 m and 384 m sources cook with 8 and 3 tiles per axis |
| `Test_Terrain_AddTerrainExtentRejected` | `Zenith2/Engine/ZenithFlux/Tests/Terrain/Flux_Terrain_Tests.cpp` | `AddTerrain` rejects a descriptor whose 1,000 m extent disagrees with its 8 tiles with `FLUX_GEOMETRY_ERROR_INVALID_DESC` and emits no node |
| `Test_Terrain_EdgeClamped` | `Flux_Terrain_Tests.cpp` | on a 384 m (3 × 3 tile) fixture: no node lying wholly beyond the edge is emitted; every vertex of a straddling level-4 node has G ≤ 768 on each axis; no rendered pixel lies outside the terrain's XZ footprint; `Normal` at the last sample equals the one-sided difference over span s, and at G = 0 never reads a negative index |
| `Test_Terrain_NoCracksAcrossLevels` | `Flux_Terrain_Tests.cpp` | (a) analytic: for a node at level l and its neighbour at level l + 1 (l = 0 and 3 on the 1,024 m fixture; l = 7, 8 and 9 on a synthetic 32,768 m terrain of 256 × 256 tiles and 12 levels, whose record and tile table are generated in the test and whose tiles hold only mips 1–8, filled from a hash of the global level-0 index, which is all those levels read), with the camera at the distances that make each fact of "Morphing and displacement" tight, every shared-edge vertex computed by `TerrainVertex` on the GPU has bit-identical position from both nodes, the fine side's k is exactly 1 and the coarse side's exactly 0; (b) selection: for 10^4 fixed camera positions over the 1,024 m fixture, no two adjacent emitted nodes differ by more than one level |
| `Test_Terrain_NoCracksFlyThrough` | `Zenith2/Engine/ZenithFlux/Tests/Terrain/Flux_TerrainScenario_Tests.cpp` | a 600-frame fly-through of the 1,024 m fixture renders no background pixel through the surface at any frame on which `Flux.Terrain.ResidencyClamped` does not increase, failing with the frame number; Forward+ (Z2T-GEOM-075), then the Deferred and visibility-buffer paths (Z2T-GEOM-106) |
| `Test_Terrain_RecordLayout` | same | the generated `Flux_TerrainRecord` is 80 bytes with the offsets of "Terrain data"; `m_uCustomData` round-trips level, x and z at their bit limits through the C++ packer and `TerrainGrid.slang`'s decode |
| `Test_Terrain_SectionReaderRejectsCorruption` | same | `Flux_ReadTerrainGeometrySections` over the sections of the 1,024 m fixture fills every scalar of `Flux_TerrainDesc`, the 5,461 pyramid entries and the 64 tile GUIDs of each table field by field; one mutated section set per rule of "Terrain cook contract" (a missing 0x0102, a duplicated 0x0101, a pyramid one entry short, a level count of 8 for 1,024 m, a reserved field of 1, a null height GUID, tile counts of 257) returns its error and asserts nothing |
| `Test_Terrain_RecordConstantBeforeFirstDraw` | same | two terrains added in either order get record indices 0 and 1, and `GetRecordIndex` returns them; a GPU readback of each material's constants block reads its R at the cooked offset; no node instance of a terrain exists, `GetSelectedNodeCount` is 0 and a recording spy sees no draw of it until the frame that publishes its block, and the first frame with a node draw of a terrain is the frame after `AddTerrain` or after it; a `ReplaceParameters` with 0 at the offset keeps R; a removed terrain's index is not reused by an `AddTerrain` of the same frame and is reused once its frame retires; a null and a stale ID return `uGEOM_TERRAIN_RECORD_INVALID`; a material without the field and a material already backing a live terrain fail with `INVALID_DESC` (`m_uDetail` 10), a FAILED material with `MISSING_DEPENDENCY`, the 1,025th live terrain with `LIMIT`, an injected constants-pool clamp with `POOL_FULL`, each leaving no terrain and no reserved index; after a simulated device loss each terrain draws again only once its recovered block holds R |
| `Test_Terrain_LODRatioBelowEightRejected` | same | `AddTerrain` with `m_fLODDistanceRatio` = 7.9 fails with `FLUX_GEOMETRY_ERROR_INVALID_DESC` |
| `Test_Terrain_HolesDiscardAtLevelMip` | same | a hole fixture discards exactly the texels of the mip of each node's level, and a node over a tile without holes stays in the opaque bucket |
| `Test_Terrain_NodeSelectionDeterministic` | same | the selected node set for a fixed camera path is identical across runs and views |
| `Test_Terrain_ShadowMatchesMain` | same | the shadow views draw the node set of the LOD reference view |
| `Test_Terrain_AdapterUVIsWorldXZ` | same | through a compute harness of `TerrainGrid.slang`, the adapter's `m_xUV0` at a node's vertices equals world XZ in metres bit for bit and `m_xUV0Grad` equals the analytic derivative of world XZ for a fixed view; `m_xUV1` equals `m_xUV0` |
| `Test_Terrain_EnvFixtureScenario` | `Flux_TerrainScenario_Tests.cpp` (suite `z2-env-cooked`) | ENV's fixture terrain, cooked by ENV's importer stages, loaded by ENV's `Flux_TerrainKindLoader` and added by `Flux_EnvironmentRenderer::ApplyRecords`, flies through 300 frames uncracked; a preview sculpt applied by ENV's tile-update step publishes without a cracked frame; a refused height chain keeps the old chain drawn |
| `Test_Terrain_EnvFixtureResidencyClamp` | `Flux_Terrain_Tests.cpp` (suite `z2-env-cooked`) | ENV's fixture terrain with every height tile at its tail draws clamped (`Flux.Terrain.ClampedNodes` above 0) without a background pixel through the surface, unclamps within 120 frames of the budget's restoration, and a preview sculpt of a clamped tile publishes its node bounds in the replacing frame |
| `Test_Mesh_SkeletonIDFromSkinSection` | `Flux_GeometryPools_Tests.cpp` | `GetSkeletonID` returns the skin section's skeleton GUID for a skinned seed and the null ID for an unskinned one; a short skin section is refused by the reader |
| `Test_Mesh_CustomCullDistanceFlag` | `Flux_MeshManager_Tests.cpp` | a seed with mesh-info flag bit 6 gives `HasCustomCullDistance()` true and bit 29 of `m_uStreamSetAndLOD` set in every primitive record image; without the bit, false and clear; a seed with bit 7 set is refused with `FLUX_GEOMETRY_ERROR_CORRUPT_SECTION` |
| `Test_Cook_CustomCullDistancePreset` | `Zenith_CookMesh_Tests.cpp` | a mesh cooked with `cull.customDistance` true sets mesh-info flag bit 6 and a different derived-data key from the same mesh with false; the default is false |
| `Test_MeshGeometryReader_AnyLodMatchesSource`, `Test_MeshGeometryReader_LodOutOfRangeRefused` | `Zenith2/Engine/ZenithFlux/Tests/Geometry/Flux_MeshReader_Tests.cpp` | "CPU geometry of one LOD" |
| `Test_CookedMeshGeometry_DigestLayout`, `Test_CookedMeshGeometry_SphereRounding` | `Zenith2/Tools/Authoring/Tests/Geom/Zenith_CookedMeshGeometry_Tests.cpp` | "Cooked mesh geometry in tools" |
| `Test_CookedMeshGeometry_ReadsFileBytes`, `Test_CookedMeshGeometry_CorruptRefused` | `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookMeshReadBack_Tests.cpp` | "Cooked mesh geometry in tools" |
| `Test_MeshCook_PublishesLod0Summary`, `Test_MeshCook_PublishesMeshSections` | `Zenith_CookMesh_Tests.cpp` | "Published mesh properties" |
| `Test_MeshMergeRecipe_WriteReadRoundTrip` | `Zenith2/Tools/Authoring/Tests/Geom/Zenith_MeshMergeRecipe_Tests.cpp` | the recipe layout, writer and reader of "Merge recipes" |
| `Test_MeshCook_PublishesSourceMesh` | `Zenith_CookMesh_Tests.cpp` | `geom.sourceMesh` of a two-level static mesh round-trips through `Zenith_ReadCookMeshSource` with both levels and the slot table; a skinned mesh publishes none |
| `Test_MeshCook_RecipeKeepMergesInstances`, `Test_MeshCook_RecipeRejected`, `Test_MeshCook_RecipeFlattenProxyColours` | `Zenith2/Tools/ZenithCook/Tests/Geometry/Zenith_CookMeshRecipe_Tests.cpp` | the recipe mode of "Merge recipes" |
| fuzz `Zenith_Fuzz_MeshFile` | `ZenithFlux/Tests/Fuzz/MeshFile.cpp` | arbitrary bytes never crash the reader and never reach an assert |
