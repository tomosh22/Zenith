# PHYS design — formats

This file is part of the PHYS design family; the entry document is `Zenith2/Docs/Design/PHYS.md`. It specifies every byte layout and document schema PHYS owns: the sections of the three cooked asset kinds (collision shape, physical material, ragdoll) inside ASSET's cooked container, the authoring documents and importer settings they are cooked from, the cook intermediates ENV writes for terrain and foliage collision, the deferred-query payloads, the save section, and the parser limits.

## Scope of this file

| Format | Heading |
|---|---|
| rules shared by every format | "Common rules", "Section kinds" |
| `.zcol` | "Collision shape file" and its subsections |
| `.zpmat` | "Material file" |
| `.zragdoll` | "Ragdoll file" |
| authoring JSON | "Material document", "Ragdoll document", "Collision shape document", "Collision importer settings" |
| ENV's inputs | "Heightfield intermediate", "Compound intermediate", "Physical-material names" |
| script requests | "Deferred query payloads" |
| saves | "Save section layout" |
| limits | "Parser limits", "Buoyancy samples" |

## Common rules

- Every cooked file is ASSET's cooked asset file: the 64-byte header, the 40-byte section table and 16-byte-aligned sections of `Zenith2/Docs/Design/ASSET/Formats.md` → "Cooked asset file", with the kind's magic, PHYS's format version (1 for every kind in Stage 3) and the codec of each section recorded in its descriptor (Z2-D-004). The codec of each section is the one ASSET's content rule table assigns to the kind, recorded in the section descriptor so a reader selects it from the file.
- Every integer and float is little-endian; floats are IEEE-754 binary32; a persistent ID is its 16-byte binary form, high word first (its JSON form, in authoring documents, is [Z2-N-CONTRACT-067]'s).
- Every count and offset read from a file is untrusted: each is checked against the section's decoded size with overflow-checked arithmetic before use, and a violation is `PHYSICS_ERROR_COOKED_CORRUPT` from the loader, never an assert ([Z2-N-CONTRACT-472]).
- Strings (surface types, interaction names, joint names in documents) are stored as UTF-8 in a strings section and interned at load into the process `Zenith_StringTable`; no interned ID is ever written to disk.
- A changed layout bumps the kind's format version; with `uZENITH_ENGINE_COOK_VERSION` this re-cooks every file ([Z2-N-CONTRACT-529]), so no reader migrates cooked data.

## Section kinds

| Kind | Name | File | Content |
|---|---|---|---|
| 0x0001 | `PHYSICS_SECTION_SHAPE_HEADER` | `.zcol` | "Shape header section" |
| 0x0002 | `PHYSICS_SECTION_NEUTRAL_GEOMETRY` | `.zcol` | "Neutral geometry section" |
| 0x0003 | `PHYSICS_SECTION_MATERIAL_PALETTE` | `.zcol` | "Material palette section" |
| 0x0004 | `PHYSICS_SECTION_BUOYANCY_SAMPLES` | `.zcol` | "Buoyancy samples section" |
| 0x0010 | `PHYSICS_SECTION_BACKEND_JOLT` | `.zcol` | "Backend sections" |
| 0x0011 | `PHYSICS_SECTION_BACKEND_PHYSX` | `.zcol` | "Backend sections" |
| 0x0101 | `PHYSICS_SECTION_RAGDOLL_HEADER` | `.zragdoll` | "Ragdoll file" |
| 0x0102 | `PHYSICS_SECTION_RAGDOLL_PARTS` | `.zragdoll` | "Ragdoll file" |
| 0x0103 | `PHYSICS_SECTION_RAGDOLL_SHAPES` | `.zragdoll` | "Ragdoll file" |
| 0x0201 | `PHYSICS_SECTION_MATERIAL` | `.zpmat` | "Material file" |
| 0x0202 | `PHYSICS_SECTION_INTERACTIONS` | `.zpmat` | "Material file" |
| 0x0203 | `PHYSICS_SECTION_STRINGS` | `.zpmat`, `.zragdoll` | UTF-8 strings, each `uint32` length then bytes, no terminator |

An unknown section kind is ignored by the loader (forward-compatible within one engine cook version is never needed, because cooked data re-cooks; ignoring keeps a newer tool's section harmless).

## Collision shape file

Asset kind 16 `Physics.CollisionShape`, extension `.zcol`, magic `ZCOL` (`Zenith2/Docs/Design/ASSET.md` → "Asset kinds"). Sections: exactly one shape header, one neutral geometry, one material palette (possibly empty), at most one buoyancy samples, at most one backend section per backend kind.

### Shape header section

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 1 | kind | `Zenith_PhysicsShapeKind` 5 (convex hull), 6 (triangle mesh), 7 (heightfield) or 8 (compound) |
| 1 | 1 | flags | bit 0 dynamic-capable (convex hulls and compounds of convex children); others 0 |
| 2 | 2 | reserved | 0 |
| 4 | 4 | sub-shape count | convex 1; mesh triangles; heightfield 2 × quads; compound children |
| 8 | 24 | local bounds | min xyz, max xyz |
| 32 | 4 | volume | m³ (0 for mesh and heightfield) |
| 36 | 12 | centre of mass | local, uniform density |
| 48 | 36 | inertia | the unit-density inertia tensor about the centre of mass, row-major 3×3 |
| 84 | 4 | palette count | 0 … 256 |
| 88 | 8 | source hash | the cook step's input hash (diagnostics) |

### Neutral geometry section

The neutral section is authoritative: every backend can build the runtime shape from it alone.

**Convex hull (kind 5).** `uint32 vertexCount` (4 … 256), `uint32 faceCount`, `uint32 indexCount`, `f32 convexRadius` (0 … 0.05 m), then `vertexCount × f32[3]` vertices, then `faceCount × { uint32 firstIndex; uint16 count; uint8 paletteIndex; uint8 reserved }`, then `indexCount × uint16` vertex indices, faces counter-clockwise seen from outside. The cook computes the hull (quickhull, `Tools/ZenithCook/Internal/Physics/Zenith_PhysicsConvexHull.cpp`) with a weld tolerance of 1e-4 m and rejects a degenerate hull (volume < 1e-9 m³).

**Triangle mesh (kind 6).** `uint32 vertexCount` (3 … 2^21 × 3), `uint32 triangleCount` (1 … `uPHYSICS_MAX_MESH_TRIANGLES`), then vertices (`f32[3]`), then triangles (`uint32[3]` indices, counter-clockwise front faces), then one `uint8` palette index per triangle, padded to 4 bytes, then one `uint8` edge-flag byte per triangle (bit e set: edge e is a convex internal edge that must not generate a contact normal, the active-edge data both backends use), padded to 4 bytes. Triangles are in the cook's spatial order (sorted by Morton code of their centroid at 16 bits per axis over the mesh bounds), which is the sub-shape order hits report.

**Heightfield (kind 7).** `uint32 samplesX`, `uint32 samplesZ` (2 … 4,097, and equal: heightfields are square, because Jolt's `HeightFieldShapeSettings` takes one `mSampleCount` for both axes; a cook input with different counts is a cook error), `f32 spacing`, `f32 heightMin`, `f32 heightMax`, `uint8 diagonal` (0: every quad split (s, t)–(s+1, t+1)), 3 reserved bytes, then `samplesX × samplesZ × uint16` heights row-major (+X fastest) decoding to `heightMin + h / 65535 × (heightMax − heightMin)`, then `(samplesX − 1) × (samplesZ − 1)` palette indices (`uint8`, each below the palette count, which is at most `uPHYSICS_MAX_HEIGHTFIELD_PALETTE` = 127 for a heightfield: PhysX's per-triangle material index has 7 bits and its value 127 is the hole), then the hole bits (`ceil(quads / 8)` bytes, bit set = hole, quad index row-major). The origin is the shape's local (0, 0, 0) at the minimum corner. The triangles of a quad follow the fixed diagonal, so the neutral collision surface equals ENV's height interpolation (cross-area row EN4). What each backend's runtime shape adds to that surface (none on PhysX, at most `e_J,max` on Jolt) is specified and tested in PHYS/Backends.md "Heightfield builds".

**Compound (kind 8).** `uint32 childCount` (1 … 65,535), then children `{ uint8 childKind (1 … 5); uint8 paletteIndex; uint16 reserved; f32 position[3]; f32 rotation[4]; f32 params[4]; uint32 hullOffset }` (48 bytes): sphere params[0] radius; box params[0..2] half extents; capsule and cylinder params[0] radius, params[1] half height; convex hull `hullOffset` the byte offset, within the section, of an inline convex-hull block in the layout above. A cooked compound is static (a cell's foliage, a building's pieces); a dynamic-capable compound has no more than 64 children.

### Public neutral-geometry reader

<!-- elaboration -->
Tools outside ZenithPhysics read a cooked shape's collision geometry as triangles (AI's navigation bake gathers static colliders, A3 cross-area row AI6). ZenithPhysics's own decoders are `Internal/`, so the public reader is one function over them:

```cpp
// Zenith2/Engine/ZenithPhysics/Zenith_CollisionNeutralGeometry.h
struct Zenith_CollisionNeutralPrimitive          // a sphere, box, capsule or cylinder child of a compound, unexpanded
{
	Zenith_PhysicsShapeKind m_eKind = PHYSICS_SHAPE_KIND_SPHERE;
	Zenith_Vector3 m_xPosition;                  // in the shape's local space
	Zenith_Quaternion m_xRotation;
	float m_afParams[4] = {};                    // the compound child's params, as "Neutral geometry section" defines them
};

struct Zenith_CollisionNeutralTriangles
{
	Zenith_Span<const Zenith_Vector3> m_axPositions;         // in the shape's local space, metres
	Zenith_Span<const uint32> m_auIndices;                  // three per triangle, counter-clockwise front faces
	Zenith_Span<const uint8> m_auPaletteIndices;            // one per triangle
	Zenith_Span<const Zenith_CollisionNeutralPrimitive> m_axPrimitives;
};

Zenith_Result<Zenith_CollisionNeutralTriangles, Zenith_PhysicsError> Zenith_DecodeCollisionNeutralTriangles(
	Zenith_ByteSpan xShapeHeaderSection, Zenith_ByteSpan xNeutralSection, Zenith_Arena& xArena);
```

The two inputs are the decoded bytes of the shape header section and the neutral geometry section of one `.zcol`, as the caller read them (a cook step through its dependency outputs). The kind comes from the shape header's byte 0. Every span of the result lives in `xArena`, and the function keeps nothing:

| Kind | Triangles |
|---|---|
| convex hull (5) | each face fan-triangulated from its first index, in face order |
| triangle mesh (6) | the triangles in their stored order |
| heightfield (7) | two triangles per quad that is not a hole, split by the `diagonal` rule, in row-major quad order |
| compound (8) | each convex-hull child's triangles transformed into the compound's space, in child order; each primitive child appended to `m_axPrimitives` |

Validation is ZenithPhysics's own decoders' (`PHYSICS_ERROR_COOKED_CORRUPT` for every violation, with no input assert), and an arena that cannot hold the result returns `PHYSICS_ERROR_OUT_OF_MEMORY`. `Test_CollisionNeutralTriangles_EachKind` decodes one fixture of each kind and checks the counts, winding and a hole quad's absence; `Test_CollisionNeutralTriangles_CorruptRefused` truncates each fixture and expects `PHYSICS_ERROR_COOKED_CORRUPT`.

### Material palette section

`palette count × 16` bytes: each entry the persistent ID of a `Physics.Material` asset, the null ID meaning the default material. Every non-null entry is a hard dependency of the shape asset in the catalogue, so materials are resident before the shape activates.

### Buoyancy samples section

`uint32 count` (1 … 32), then `count × f32[4]` (local position xyz, volume m³), the volumes summing to the shape's volume. See "Buoyancy samples" for how they are chosen.

### Backend sections

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | backend kind (`Zenith_PhysicsBackendKind`) |
| 1 | 3 | reserved |
| 4 | 4 | payload bytes |
| 8 | 8 | data tag (`Zenith_PhysicsBackendInfo::m_ulDataTag` of the baking backend) |
| 16 | 16 | neutral hash: `Zenith_Hash128` of the neutral geometry section's decoded bytes |
| 32 | … | payload: the backend's binary shape (PHYS/Backends.md "Backend shape payloads") |

The loader uses a backend section only when its kind is the running backend's, its data tag equals the running backend's and its neutral hash equals the hash of the file's neutral section; otherwise it builds from the neutral section (`physics.shapes.built_from_neutral`, cumulative). The cook writes the section for the game's selected backend of each platform (`backends.physics`, `Zenith2/Docs/Design/MOD/GameBuild.md` → "Keys") when `physics.cook.backendSections` is true.

## Material file

Asset kind `Physics.Material`, extension `.zpmat`, magic `ZPMT`. Section 0x0201 (24 bytes): `f32 friction; f32 restitution; f32 density; uint8 frictionCombine; uint8 restitutionCombine; uint16 reserved; uint32 surfaceTypeString; uint32 interactionCount` (the string index refers to section 0x0203). Section 0x0202: `interactionCount × { uint32 otherSurfaceString (0xFFFFFFFF = "*"); uint32 interactionString; f32 impactSpeed; f32 slideSpeed; uint16 cooldownTicks; uint16 reserved }` (20 bytes), at most 64 rules.

## Ragdoll file

Asset kind `Physics.Ragdoll`, extension `.zragdoll`, magic `ZRGD`.

| Section | Layout |
|---|---|
| 0x0101 header (32 bytes) | `uint32 partCount` (1 … 64); `uint32 reserved`; `uint64 skeletonHierarchyHash` (ANIM's hierarchy hash of the skeleton the document names, `Zenith2/Docs/Design/ANIM/Formats.md` → "Skeleton file"); `f32 rootHeightOffset`; `f32 maxMotorTorque`; `f32 motorFrequency`; `f32 motorDamping` |
| 0x0102 parts (96 bytes each, parent before child) | `uint64 jointNameHash`; `uint32 parentPart` (0xFFFFFFFF root); `uint8 constraintKind` (7 swing-twist or 3 hinge); `uint8 shapeKind` (1 … 5); `uint16 reserved`; `f32 mass`; `f32 linearDamping`; `f32 angularDamping`; `f32 offsetPosition[3]`; `f32 offsetRotation[4]`; `f32 shapeParams[4]`; `f32 twistMin`; `f32 twistMax`; `f32 normalCone`; `f32 planeCone`; `uint32 hullOffset` (into section 0x0103, for convex parts) |
| 0x0103 shapes | inline convex-hull blocks ("Neutral geometry section") for convex parts |
| 0x0203 strings | the joint names (diagnostics) |

A part whose joint name hash the target skeleton lacks makes the asset refuse enters for that skeleton (the check uses `Zenith_RagdollPoseSource::FindJoint` at enter time).

## Material document

`Games/<Name>/Assets/**/<Name>.physmat.json` (document type `Zenith.PhysicsMaterialDocument`, schema version 1), cooked by `physics.material` into one `.zpmat`:

```json
{
	"schemaVersion": 1,
	"friction": 0.6,
	"restitution": 0.1,
	"density": 700.0,
	"frictionCombine": "Average",
	"restitutionCombine": "Max",
	"surfaceType": "Wood",
	"interactions": [
		{ "other": "Stone", "interaction": "Impact.WoodOnStone", "impactSpeed": 1.5, "slideSpeed": 0.6, "cooldownTicks": 6 },
		{ "other": "*", "interaction": "Impact.Wood", "impactSpeed": 1.0 }
	]
}
```

| Member | Type | Rule |
|---|---|---|
| `schemaVersion` | integer | 1 |
| `friction` | number | ≥ 0, ≤ 10 |
| `restitution` | number | 0 … 1 |
| `density` | number | > 0, ≤ 100,000 kg/m³ |
| `frictionCombine`, `restitutionCombine` | string | `Average`, `Min`, `Multiply`, `Max`; defaults `Average`, `Max` |
| `surfaceType` | string | 1 … 63 bytes, `^[A-Za-z][A-Za-z0-9._]*$`; default `Default` |
| `interactions` | array | at most 64; `other` a surface type or `*`; `interaction` 1 … 63 bytes; thresholds ≥ 0; `cooldownTicks` 0 … 65,535, default 6; `slideSpeed` default 0.5 |

Unknown members are preserved and warned ([Z2-N-CONTRACT-543]).

## Ragdoll document

`Games/<Name>/Assets/**/<Name>.ragdoll.json` (document type `Zenith.RagdollDocument`, schema version 1), cooked by `physics.ragdoll`:

```json
{
	"schemaVersion": 1,
	"skeleton": "Assets/Characters/Humanoid60.skeleton",
	"rootHeightOffset": 0.95,
	"maxMotorTorque": 400.0,
	"motorFrequency": 8.0,
	"motorDamping": 1.0,
	"parts": [
		{ "joint": "Hips", "shape": { "kind": "Capsule", "radius": 0.15, "halfHeight": 0.1 }, "mass": 12.0,
		  "offset": { "position": [0, 0.05, 0], "rotationDegrees": [0, 0, 90] } },
		{ "joint": "Spine1", "parent": "Hips", "constraint": { "kind": "SwingTwist", "twistDegrees": [-20, 20],
		  "normalConeDegrees": 30, "planeConeDegrees": 20 }, "shape": { "kind": "Box", "halfExtents": [0.15, 0.12, 0.1] }, "mass": 10.0 },
		{ "joint": "LeftKnee", "parent": "LeftUpLeg", "constraint": { "kind": "Hinge", "minDegrees": -140, "maxDegrees": 0 },
		  "shape": { "kind": "Capsule", "radius": 0.06, "halfHeight": 0.18 }, "mass": 4.0 }
	]
}
```

Angles are degrees in the document and radians in the cooked file (`Zenith2/Docs/Design/CORE/Maths.md` → "Units"). The cook resolves `skeleton` through the asset database, reads the skeleton's published joint names and hierarchy hash (`ReadDependencyOutput`), refuses a joint the skeleton lacks or a parent that is not an ancestor in the skeleton (cook error `COOK_ERROR_STEP_FAILED` with detail `PHYSICS_COOK_DIAG_RAGDOLL_JOINT`), and orders parts parent before child. `damping` members default to 0.05; `shape.kind` is `Sphere`, `Box`, `Capsule` or `ConvexHull` (`"points": [[x, y, z], …]`, at most 256).

## Collision shape document

`Games/<Name>/Assets/**/<Name>.collision.json` (document type `Zenith.CollisionShapeDocument`, schema version 1): a hand-authored convex hull, compound or triangle mesh, cooked by `physics.shape`:

```json
{
	"schemaVersion": 1,
	"kind": "Compound",
	"children": [
		{ "kind": "Box", "halfExtents": [1, 0.1, 1], "position": [0, 0, 0], "rotationDegrees": [0, 0, 0], "material": "Assets/Physics/Wood.physmat" },
		{ "kind": "ConvexHull", "points": [[0, 0, 0], [1, 0, 0], [0, 1, 0], [0, 0, 1]] }
	],
	"buoyancySamples": "Auto"
}
```

`kind` is `ConvexHull` (`points`), `TriangleMesh` (`source`: a mesh asset path and an optional `node` name; the triangles come from GEOM's cooked mesh positions through `ReadDependencyOutput`), or `Compound` (`children`, each a primitive or convex hull). `material` names a material document's asset path; absent members take defaults; `buoyancySamples` is `Auto` (the lattice rule) or an explicit array of `[x, y, z, volume]`.

## Collision importer settings

A mesh source's `.meta` sidecar (ASSET's `Zenith.AssetMeta`, `Zenith2/Docs/Design/ASSET/Formats.md` → "Meta sidecar schema") may carry a `physics` member, read by PHYS's `physics.collision` importer stage:

```json
"physics": { "collision": "ConvexHull", "nodes": ["*"], "material": "Assets/Physics/Stone.physmat", "maxHullVertices": 64 }
```

| Member | Values | Product |
|---|---|---|
| `collision` | `None` (default), `ConvexHull` (one hull over the selected nodes), `ConvexHullPerNode` (a compound of one hull per node), `TriangleMesh` | one `Physics.CollisionShape` sub-asset `collision`, GUID derived per ASSET's sub-asset rule |
| `nodes` | glob patterns over node names | the nodes used |
| `material` | a material asset path | palette entry 0 |
| `maxHullVertices` | 4 … 256 | the hull simplification bound |

## Heightfield intermediate

ENV's importer writes, per present terrain tile, the intermediate of the product `collision:<x>_<z>` with `Zenith_WritePhysicsHeightfieldIntermediate(const Zenith_TerrainCollisionTile& xTile, Zenith_Span<const Zenith_StringView> astrPalette, Zenith_Arena& xArena) → Zenith_Result<Zenith_ByteSpan, Zenith_PhysicsError>` (`Tools/ZenithCook/Internal/Physics/Zenith_PhysicsIntermediates.h`, a ZenithCook-internal function both ENV's importer and PHYS's cook step use, so no library edge is created; cross-area rows PH9, PH25). `astrPalette` is the terrain document's `physicalMaterials` list in order, the names ENV authors ("Physical-material names"); the tile's per-quad indices refer to it. An invalid physical-material name, more than 127 names, or a quad index at or above the count is `PHYSICS_ERROR_INVALID_DESC`, logged with the entry:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `ZPHI` |
| 4 | 2 | version 1 |
| 6 | 2 | reserved |
| 8 | 4 | samples per side (257) |
| 12 | 4 | sample spacing (0.5 m) |
| 16 | 12 | tile origin (world minimum corner at height 0) |
| 28 | 4 | height min |
| 32 | 4 | height max |
| 36 | 1 | diagonal (0) |
| 37 | 1 | palette count (1 … 127, `uPHYSICS_MAX_HEIGHTFIELD_PALETTE`) |
| 38 | 2 | reserved |
| 40 | 64 × palette count | the palette the per-quad indices refer to: per entry `uint8 length` (1 … 63), then the physical-material name's UTF-8 bytes, zero-padded to 63 |
| … | 257² × 2 | heights (u16) |
| … | 256² | per-quad palette indices |
| … | 8,192 | hole bits |

`physics.heightfield` resolves each palette name to its `Physics.Material` asset ("Physical-material names"), writes those assets' persistent IDs as the `.zcol` material palette, declares each as a hard reference, and cooks the rest into a `.zcol` of kind 7 whose origin is the tile's minimum corner; the tile's cell content holds one static collider entity at the tile origin with ShapeKind 7 referencing it (`Zenith2/Docs/Design/ENV/Bake.md` → "Heightfield extraction").

## Compound intermediate

ENV writes, per foliage cell with collidable instances, `ZPCI` version 1 through `Zenith_WritePhysicsCompoundIntermediate(const Zenith_FoliageCollisionSet& xSet, Zenith_Span<const Zenith_StringView> astrMaterialNames, Zenith_Arena& xArena) → Zenith_Result<Zenith_ByteSpan, Zenith_PhysicsError>`. `astrMaterialNames` holds the physical-material names of the cell's collidable foliage types (any order, duplicates allowed); each shape's `Zenith_FoliageCollisionDesc::m_uPhysicalMaterial` is a physical-material ID ("Physical-material names"), which the writer maps to the name whose `Zenith_MakePhysicsMaterialNameID` equals it (0 maps to the default material); an ID no name produces, or two different names with one ID, is `PHYSICS_ERROR_INVALID_DESC`, logged with the shape. The intermediate is a header (magic, version, child count, palette count), the palette (names in first-use order over the shapes in index order, each entry as in the heightfield palette), then one record per instance in record order `{ uint8 kind (1 sphere … 5 convex); uint8 paletteIndex; uint16 reserved; f32 position[3]; f32 rotation[4]; f32 scale; f32 params[4]; Zenith_PersistentID convexHull }` (64 bytes). `physics.foliageCompound` resolves the palette names as `physics.heightfield` does and cooks the intermediate into one static `.zcol` compound per cell, convex-hull children resolved from the foliage type's hull asset and scaled by the instance scale.

## Physical-material names

ENV's authoring documents name physical materials by string (the terrain document's `physicalMaterials` and a layer's `physicalMaterial`, a foliage type's `collision.physicalMaterial`; `Zenith2/Docs/Design/ENV/Bake.md` → "Heightfield extraction", "Collidable foliage"), resolved "against PHYS's physical-material table at cook". PHYS conforms to those names (cross-area rows PH9 (b) as revised, PH25). The table and the mapping:

| Item | Rule |
|---|---|
| name | `^[A-Za-z][A-Za-z0-9_]{0,62}$`, case-sensitive; `Zenith_IsValidPhysicsMaterialName(Zenith_StringView) → bool` (`Zenith_PhysicsIntermediates.h`) checks it |
| the table | a game's physical-material table is the set of its `Physics.Material` assets whose logical path is `content:/Physics/Materials/<N>.physmat.json`, the documents `Games/<Name>/Assets/Physics/Materials/<N>.physmat.json` (`Zenith2/Docs/Design/ASSET.md` → "Content root mapping"); N, the file stem, is the material's physical-material name. A material document elsewhere is an ordinary material, reachable from colliders by asset reference and never by name |
| name → asset | that logical path, looked up in the cook's asset record snapshot; the record must be a live `Physics.Material` asset (state NEW, CURRENT or DIRTY). A name with no such record fails the cook item of `physics.heightfield` or `physics.foliageCompound` with `COOK_ERROR_STEP_FAILED` and detail `PHYSICS_COOK_DIAG_MATERIAL_NAME_UNRESOLVED`, the message naming the name and the tile or cell; this cook-time resolution is the check ENV's import validation defers to PHYS |
| name → ID | the physical-material ID `Zenith_MakePhysicsMaterialNameID(N)`: h = the low 32 bits of `Zenith_Hash64` over N's UTF-8 bytes, and the ID is h, or 1 when h is 0 (PHYS/PublicAPI.md "Zenith_PhysicsMaterial header"); 0 is reserved for the default material and only the empty name gives it. It is the `u32` ENV's cooked terrain header lists per `physicalMaterials` entry and `Zenith_FoliageCollisionDesc::m_uPhysicalMaterial` carries, so ENV's runtime, AUDIO and VFX compare materials by ID without reading PHYS |
| collisions | two different names of one cook item with one ID make the writers refuse (above); for two given names the probability is 2^−32 |
| dependency | each resolved material is a hard reference of the `.zcol` (`DeclareReference`), so renaming, moving or deleting `<N>.physmat.json` re-cooks the shapes that name it, and an unresolved name surfaces at that cook |

<!-- assumption: pending ASSET (row PH26) -->
A cook step looks an asset up by logical path in the plan's record snapshot: `Zenith_CookContext::FindAssetByLogicalPath(Zenith_StringView strLogicalPath) const → const Zenith_AssetRecord*` (null when absent; the snapshot's view of `Zenith_AssetDatabase::FindByLogicalPath`, `Zenith2/Docs/Design/ASSET/Database.md` → "Zenith_AssetDatabase API"). The ragdoll cook's resolution of its document's `skeleton` path ("Ragdoll document") uses the same member.
<!-- /assumption -->

`Test_PhysicsCook_MaterialNamesResolveByTable` (PHYS/Tests.md "Cook and format tests") asserts the rules: a name resolves to the material at its logical path and that asset's persistent ID enters the `.zcol` palette; an absent name and an invalid name are refused with their diagnostics; two different names with one ID (the first colliding pair among the names `C0`, `C1`, …, found by search at test time after about 2^16 candidates) make the compound writer refuse; the IDs of a fixed list of names equal the formula.

## Deferred query payloads

```cpp
// ZenithPhysics/Internal/Query/Zenith_PhysicsDeferredQuery.h (also the layout of the Zenith.Physics.QueryRequest value struct)
struct Zenith_PhysicsQueryRequest            // 64 bytes; the payload after the source-kind byte
{
	uint8 m_eKind;                          // 1 ray, 2 sphere cast, 3 overlap sphere
	uint8 m_uMaxHits;                       // 1 … 16 (counted hits; the result carries the closest)
	uint8 m_auReserved[2];
	uint32 m_uLayerMask;
	Zenith_Vector3 m_xOrigin;
	Zenith_Vector3 m_xDirection;             // unit (ray, sphere cast)
	float m_fDistance;
	float m_fRadius;                         // sphere cast and overlap
	Zenith_EntityHandle m_xIgnoreRoot;
	uint8 m_auPadding[8];
};

inline constexpr uint8 PHYSICS_QUERY_RESULT_FLAG_CANCELLED = 1u << 0;   // completed at DetachWorld without running

struct Zenith_PhysicsQueryResult             // 64 bytes; uPHYSICS_QUERY_RESULT_BYTES
{
	uint8 m_bHit;
	uint8 m_uCount;                         // hits found, ≤ m_uMaxHits
	uint8 m_uFlags;                         // PHYSICS_QUERY_RESULT_FLAG_CANCELLED; other bits 0
	uint8 m_auReserved[5];
	Zenith_EntityHandle m_xEntity;           // the closest hit's entity
	Zenith_Vector3 m_xPosition;
	Zenith_Vector3 m_xNormal;
	float m_fDistance;
	uint8 m_auPadding[20];
};
```

Both are serialized field by field, little-endian; the entity handle is world-local and never leaves the world (a save records the request, not the result: "Save section layout").

## Save section layout

The section of participant `Zenith.Physics.World`, section version 1, written through `Zenith_SaveSectionWriter` (every value little-endian, floats by bit pattern):

| Block | Content |
|---|---|
| header | `u8 backendKind`; `u8 flags` (bit 0 EXACT blob present); `u16 reserved`; `u64 backendDataTag`; `u32 bodyCount`; `u32 constraintCount`; `u32 controllerCount`; `u32 ragdollCount`; `u32 vehicleCount`; `u32 pairCount`; `u32 triggerPairCount`; `u32 endedPairIDCount`; `u32 pendingOpCount`; `u32 deferredCount`; `u32 constraintPriorityCounter`; `u32 originRebaseCount`; `f64 appliedOrigin[3]` (the `m_adAppliedOrigin` of PHYS/Simulation.md "Module world state"); `f32 gravity[3]` |
| slot tables | the bytes of `SaveSlotTables`: for the body table then the constraint table, `u32 capacity`, `u32 highWater`, then `highWater × u32 generation` (slot order; 0 = never used) |
| bodies (ascending simulation ID) | entity reference (16); `u64 handleBits`; `u8 motionType`; `u8 flags`; `u16 reserved`; `f32 position[3]`; `f32 rotation[4]`; `f32 linear[3]`; `f32 angular[3]`; `f32 frozenLinear[3]`; `f32 frozenAngular[3]`; `u8 sleeping`; `u8 frozen`; `u16 reserved`; `f32 writtenLocalPosition[3]`; `f32 writtenLocalRotation[4]`; `f32 writtenLinear[3]`; `f32 writtenAngular[3]` (13 floats, the exact bits); `u64 parentVersion` |
| constraints (ascending priority) | joint entity reference; `u64 handleBits`; `u32 priority`; `u8 state`; `u8 broken`; `u16 reserved` |
| controllers (ascending simulation ID) | entity reference (16); `u64 capsuleHandleBits`; `f32 writtenLocalPosition[3]`; `f32 writtenLocalRotation[4]` (the exact `L_w` bits that decide adoption in PHYS/CharacterRagdollVehicle.md "Character move algorithm" step 1); `u64 parentVersion`; `f32 lastTargetPosition[3]`; `f32 lastTargetRotation[4]`; `f32 carriedYawRate`; `u8 capsuleQueryOnly`; `u8 reserved[7]` (the fields of `Zenith_PhysicsControllerRecord`, PHYS/Simulation.md "Module world state"; every float by its bit pattern) |
| ragdolls (ascending simulation ID) | entity reference; `u8 state`; `u8 reserved[3]`; `f32 weight`; `f32 w0`; `u32 k`; `f32 enterSeconds`; `f32 recoverSeconds`; `f32 capturedRoot[7]`; `u32 partCount`; per part, in definition order ("Ragdoll part identity" of PHYS/Simulation.md): `u64 bodyHandleBits`; `u64 constraintHandleBits` (0 for the root part); `u32 constraintPriority`; `u8 motionType` (the motion type before a freeze when frozen); `u8 sleeping`; `u8 frozen`; `u8 reserved`; `f32 position[3]`; `f32 rotation[4]`; `f32 linear[3]`; `f32 angular[3]` (the body origin's pose and velocities, the exact bits; for a frozen part, the velocities saved at the freeze, PHYS/Simulation.md "Region freeze") |
| vehicles | entity reference; engine RPM, gear, shift timer, per wheel the previous compression and steer |
| pairs, trigger pairs (each in its sorted order) | per pair, the 56-byte `Zenith_PhysicsPairRecord` of PHYS/Simulation.md "Module world state" field by field: `u64 low`; `u64 high`; `u32 subShapeContacts`; `u16 suspendedContacts`; `u16 impactCooldown`; `u8 flags` (only bit 2 sliding is ever set at a capture; bits 0, 1 and 3 live within one emission); `u8 reserved[3]`; `f32 point[3]`; `f32 normal[3]` |
| ended list | `endedPairIDCount × u64` simulation IDs, ascending (`m_aulEndedPairIDs`: non-empty when a playback after this tick's emission removed bodies, such as `Physics.RemoveRagdollBodies` at sync point 6) |
| pending operations | kind, simulation ID, payload |
| deferred inbox | the INBOX entries alone, ascending completion ID: completion ID, the 64-byte request (parked results are SAVE's unpublished results, `Zenith2/Docs/Design/SAVE/Formats.md` → "Async section") |
| EXACT blob | length-prefixed `SaveState` bytes (Jolt: Jolt's global, body and contact state, then the constraints' states in ascending priority, PHYS/Backends.md "Jolt state save and restore") |

A section version above 1 is refused with `SAVE_ERROR_CORRUPT` by an older build; each subsequent version adds a forward migration step in `RestoreSection` ([Z2-N-CONTRACT-524]). No binary fixture is committed (the orchestrator's Q55 ruling): the diff that introduces version N + 1 adds a header-only, version-pinned writer `Zenith2/Engine/ZenithPhysics/Tests/Support/Zenith_PhysicsSaveSectionV<N>.h` (X184) that writes a version-N section field by field from this table as it stood at version N, and that diff's migration test, `Test_PhysicsSave_MigratesV<N>Section`, generates the bytes at test time with it, restores them through the current `RestoreSection` and asserts the migrated state.

## Parser limits

The PHYS rows the core-doc batch adds to `Zenith2/Docs/Architecture.md` → "Parser limits" (cross-area row PH15); each is a constant in the reader, tested at the limit and at limit + 1 ([Z2-N-ARCH-953]):

| Parser | Limits (a violation is a recoverable error) |
|---|---|
| `.zcol` | the ASSET container limits; convex vertices ≤ 256, faces ≤ 512, indices ≤ 4,096; mesh triangles ≤ 2,097,152; heightfield samples per side 2 … 4,097 with samplesX = samplesZ; heightfield palette ≤ 127; other palettes ≤ 256; compound children ≤ 65,535; buoyancy samples ≤ 32; backend payload ≤ 512 MiB |
| `.zpmat` | interactions ≤ 64; string lengths 1 … 63 |
| `.zragdoll` | parts ≤ 64; convex vertices per part ≤ 256 |
| `ZPHI`, `ZPCI` intermediates | samples 257 per side; `ZPHI` palette ≤ 127, `ZPCI` palette ≤ 256; compound records ≤ 65,535 |
| physics save section | the save limits; body, constraint and pair counts ≤ their configuration capacities; slot-table capacities equal to the world's and high-water marks ≤ them |
| layer configuration strings | ≤ 32 names of 1 … 31 bytes; ≤ 1,024 pair tokens; ≤ 8 KiB |

## Buoyancy samples

`Auto` samples are a lattice: the shape's local bounds are divided into an n × n × n grid with n = 3 (27 cells) for convex hulls and compounds, the cells whose centre lies inside the shape (convex containment test; for compounds, inside any child) become samples at their centres, each with volume = shape volume / count; when fewer than 4 cells qualify, n = 4 (64 cells, then the 32 nearest the centre of mass by distance with the lower cell index on ties). Primitives and runtime-composed compounds use the same rule over their analytic volume, the samples computed by `Zenith_PhysicsPrimitiveBuoyancy` at ingest step 4 into the body's entry of the per-world table `m_xBuoyancySamples` (PHYS/Simulation.md "Module world state", "Buoyancy algorithm"); the process's primitive cache holds no samples.
