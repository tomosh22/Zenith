# WORLD design: the partition cook

Section file of `Zenith2/Docs/Design/WORLD.md`. It specifies how a world's authoring documents become STREAM's runtime contract: the importer `world.world`, its inputs, the partition algorithm that assigns every resolved entity hierarchy to a runtime grid cell, a layer scene or nothing, the cell scene documents it writes to STREAM's cell scene rules, the streaming world document it writes to STREAM's schema, the navigation inputs it hands to AI, the products it declares and their identities, the contributions ENV, AI and LIGHT add to cells, and the determinism argument. STREAM owns the runtime side of everything here (`Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document", "Cell scene rules"); this file only writes to it.

## Scope of this file

| Need | Heading |
|---|---|
| the importer and when it runs | "The world importer" |
| what the importer reads | "Importer inputs", "Terrain loading" |
| the algorithm, step by step | "Partition algorithm" |
| how an entity picks a grid | "Grid assignment" |
| static colliders out of gameplay cells | "Static collider split" |
| layers at run time | "Layer realisation" |
| references across cells | "Cross-cell references" |
| persistent markers | "Persistent markers" |
| ENV, AI and LIGHT content in cells | "Contributed cell content" |
| what AI's navigation bake reads | "Navigation input scenes" |
| the written cell documents | "Cell scene documents" |
| the written streaming world document | "Streaming world document generation" |
| small worlds | "Unstreamed worlds" |
| products, keys and GUIDs | "Products and keys", "Product identity" |
| why the output is reproducible | "Partition determinism" |
| limits and errors | "Partition limits" |

## The world importer

`Zenith_WorldImporter` (`Zenith2/Tools/ZenithCook/Internal/World/Zenith_WorldImporter.h`) implements ASSET's `Zenith_Importer` (`Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_Importer"):

| `Zenith_ImporterDesc` member | Value |
|---|---|
| `m_szID` | `"world.world"` |
| `m_uVersion` | `uWORLD_IMPORTER_VERSION` (1); bumped by any change to an intermediate's bytes for unchanged inputs |
| `m_uSettingsVersion` | 1 |
| `m_aszExtensions` | `".world.json"` |
| `m_axSettings` | none |
| `m_aeProducedKinds` | `ASSET_KIND_STREAMING_WORLD`, `ASSET_KIND_SCENE`, `ASSET_KIND_MESH`, `ASSET_KIND_TEXTURE`, `ASSET_KIND_REFLECTION_PROBE`, `ASSET_KIND_IRRADIANCE_VOLUME`, `ASSET_KIND_LIGHTMAP_SET`, `ASSET_KIND_NAV_MESH` (AI's kind 15, through AI's declaration function, row WD23) |
| `m_bOutOfProcess` | false: it opens documents through its context (row WD1), which needs the driver's record snapshot |
| `m_aszToolVersions` | none |

The import is a pure function of the documents it opens and the product lists and keys it reads, all recorded as import inputs, so ASSET's import key reruns it exactly when one of them changes (`Zenith2/Docs/Design/ASSET/Import.md` → "Reimport triggers"). It never writes a source file. Products are cooked by their owners' steps: cell, layer, content and navigation input scenes by SCENE's `scene.scene`, the streaming world by STREAM's `stream.world`, HLOD meshes by GEOM's `geom.mesh`, lighting assets by LIGHT's steps and GEOM's `geom.texture`, navigation tiles by AI's `ai.navMesh`.

**Threading.** `Import` runs on one of ASSET's import threads (ASSET's importer contract: ANY_THREAD, concurrently with other sources' imports). The importer never calls `Zenith_ImportContext::GetJobContext`: it runs every WORLD function single-threaded on its own thread, calling `Zenith_PlanWorldPartition`, each generator region job (`Zenith_RunWorldGeneratorRegion`) and ENV's `Zenith_Place` (`pxJobs` null) inline, in the orders this file states. Nothing in the import forks, joins or waits on another job ([Z2-N-CONTRACT-315]); its cost is the pending row "World partition import time" ("Partition cost and scaling"). An import is one long cook item of a tool host; how ASSET schedules items is ASSET's, and ruling X350 makes long tool-host background work legal in the cook as in the editor (`Zenith2/Docs/Design/WORLD/Editing.md` → "World editor extension").

<!-- assumption: pending ASSET (rows WD1, WD2, WD4, WD5, WD21, WD31, WD35) -->
The importer uses members its context does not yet declare, requested from ASSET:

```cpp
// Zenith2/Tools/ZenithCook/Internal/Import/Zenith_Importer.h (Zenith_ImportContext, additions)
// The importer's own source as a migrated document (in memory; never written back); a READ_ONLY_NEWER source succeeds and the
// importer decides. Records nothing extra: the source bytes are already the import key's first input.
Zenith_Result<void, Zenith_ImportError> OpenSourceDocument(Zenith_OpenedDocument& xOut);
// Another authoring document, by path relative to the source's directory or by asset GUID, migrated in memory through a
// call-local migrator over the verified bytes (the shape of Zenith_CookContext::OpenSourceDocument); recorded as an import input
// (path and Zenith_Hash128 of its bytes), so editing it re-imports the source. An input-only document (row WD21) is opened by path.
Zenith_Result<void, Zenith_ImportError> OpenInputDocument(Zenith_StringView strRelativePath, Zenith_OpenedDocument& xOut);
Zenith_Result<void, Zenith_ImportError> OpenInputDocumentByGUID(const Zenith_AssetGUID& xAsset, Zenith_OpenedDocument& xOut);
// The products another source declares (key, GUID, kind, primary), in key byte order, importing that source first when it is NEW
// or DIRTY (an import cycle is IMPORT_ERROR_INPUT_NOT_FOUND naming both sources); recorded as an import input keyed by the hash
// of the product list, so adding or removing one of its products re-imports this source.
Zenith_Result<Zenith_Span<const Zenith_ImportProduct>, Zenith_ImportError> ReadSourceProducts(const Zenith_AssetGUID& xSource);
// Another authoring document by a path relative to the directory of another asset's source (a terrain's tile documents,
// whose tileFiles paths are relative to the terrain document; row WD31): migrated in memory as OpenInputDocument does and
// recorded as an import input (path and Zenith_Hash128 of its bytes); found = false and no error when the file is absent.
Zenith_Result<bool, Zenith_ImportError> OpenInputDocumentRelativeTo(const Zenith_AssetGUID& xAnchor,
	Zenith_StringView strRelativePath, Zenith_OpenedDocument& xOut);
// A property another asset's cook step publishes for ePlatform (Cook.md: PublishProperty), cooking that item first when its
// key misses the DDC; recorded as an import input keyed by the property's Hash128, so the import reruns exactly when the
// property's bytes change (row WD31; WORLD reads GEOM's "geom.lod0Summary", row WD30). A property the step does not publish
// is IMPORT_ERROR_INPUT_NOT_FOUND. For a platform other than the cook's, the item is cooked into the DDC only, never published
// into the cooked tree, and the property's hash is still the input (row WD31 (a)).
Zenith_Result<Zenith_ByteSpan, Zenith_ImportError> ReadPublishedProperty(const Zenith_AssetGUID& xAsset,
	Zenith_AssetPlatform ePlatform, Zenith_StringView strName);
// Declares a product whose GUID the importer derives and which is never written into the .meta's subAssets (row WD4); refused with
// IMPORT_ERROR_DUPLICATE_PRODUCT_KEY when the key or the GUID is already declared or recorded in the .meta.
Zenith_Result<void, Zenith_ImportError> DeclareDerivedProduct(Zenith_StringView strKey, Zenith_AssetKind eKind,
	const Zenith_AssetGUID& xGUID, bool bPrimary);
// A diagnostic with a code (row WD35): strCode matches [A-Z0-9_]{1,64} (empty: no code) and becomes the cook report entry's
// "code"; every diagnostic of an import is stored with its intermediates' DDC entry and its database record, and replayed
// into the cook report of every cook in which the import that produced the source's current intermediates did not run.
void Report(Zenith_ImportDiagnosticLevel eLevel, Zenith_StringView strCode, Zenith_StringView strMessage);
```

`Zenith_RegisterWorldImporters(Zenith_ImporterRegistry&)` registers the one static `Zenith_WorldImporter` in the cook services' item 4 after CONTENT's importers (row WD5). Row WD31 withdraws WD21 (b) (`ReadCookItemKey` and the editor's `cookKey` request: no cook key is a bake input any more, "Lighting fingerprint" in `Zenith2/Docs/Design/WORLD/Bakes.md`) and WD21 (c) (the derived-data blobs of a resolution memo WORLD no longer has, "Partition cost and scaling"). Until ASSET lands these members, the WORLD tickets that implement the importer are `draft` and blocked by ASSET's patch; `Zenith_PlanWorldPartition` itself reads its inputs through `Zenith_WorldSourceAccess` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Source access interface") and is built and tested against in-memory sources first.
<!-- /assumption -->

## Importer inputs

| Input | Opened through (`Zenith_WorldSourceAccess` member → `Zenith_ImportContext` member) | Recorded as |
|---|---|---|
| the world document | `OpenSourceDocument` | the source bytes |
| every region document of `regions`, in key order | `OpenDocumentByPath("Regions/<LayerKey>/<x>_<z>.scene.json")` or `OpenDocumentByPath("Generated/<GeneratorKey>/<x>_<z>.scene.json")` → `OpenInputDocument` | an input |
| every prefab a region resolution reads | `OpenInputDocumentByGUID` from `Zenith_WorldImportPrefabProvider` (a `Zenith_PrefabProvider`, `Zenith2/Docs/Design/SCENE/Prefabs.md` → "Prefab provider") | an input |
| the persistent scene | `OpenDocumentByGUID(persistentScene)` → `OpenInputDocumentByGUID` | an input (its entity IDs, for W23, its references and its lights; its type decides W5) |
| every generator document | `OpenDocumentByPath("Generators/<GeneratorKey>.generator.json")` → `OpenInputDocument` | an input |
| AI's navigation settings document, when navigation is enabled | `OpenDocumentByGUID(bakes.navigation.settings)` → `OpenInputDocumentByGUID` | an input (W13, W46) |
| every terrain document of `terrains` | `OpenDocumentByGUID(terrain)` → `OpenInputDocumentByGUID` | an input (W14; `origin`, `heightLimits`, `tileFiles` and `navigation` for the lighting fingerprint, the navigation regions and the terrain loader) |
| the tile documents of a terrain: those intersecting a lighting unit's gather rectangle whose header exists (lighting fingerprint item 7, W41), those intersecting a baked generator region job's region grown by its surface reach plus 128 m whose `surface` names the terrain (its fingerprint, W34; `Zenith2/Docs/Design/WORLD/Generation.md` → "Obstacles and dependencies": 1,280 m for `world.scatter`, 128 m for `world.probeGrid`), and all of a terrain's tiles when a cook-mode generator's surface names it (the loader of "Terrain loading") | `OpenDocumentRelativeTo(terrain, tileFiles[i].file)` → `OpenInputDocumentRelativeTo` (row WD31) | inputs, so a sculpt or paint stroke re-imports the world ("Terrain loading") |
| every placement rule document and prefab a cook-mode generator reads | `OpenDocumentByPath` / `OpenDocumentByGUID` | inputs |
| every light bake header and page document of the lighting grid | `OpenDocumentByPath("Baked/Lighting/<Grid>/<x>_<z>.lightbake.json")`, then each page | inputs; a missing header is not an error (W40) |
| the LOD 0 summary (digest and bounding sphere) of every mesh a `Zenith.Render.MeshInstance` of a BAKED layer names, when lighting is enabled and any header exists, or HLOD is enabled (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Mesh summaries") | `GetMeshLod0Summary(mesh)` → `ReadPublishedProperty(mesh, ASSET_PLATFORM_WINDOWS, "geom.lod0Summary")` (rows WD30, WD31); an Android cook reads the Windows item too, cooked into the DDC only | inputs keyed by the property's hash |
| the products of each terrain of `terrains` | `ListProducts(terrain)` → `ReadSourceProducts(terrain)` | inputs (ENV's tile, foliage, collision and navigation products) |
| the game's frozen type registry | the import driver's services (ASSET's import context holds the registry the cook services froze at item 3) | — (part of the engine cook version) |

Every opened document is SCENE-resolved or WORLD-parsed into the import's scratch arena (`Zenith_ImportContext::GetScratch`).

### Terrain loading

<!-- assumption: pending ENV (row WD33 (b), (c)) -->
ENV confirms that `Zenith_TerrainDocument::Load` over a caller's `Zenith_TerrainTileSource` that returns canonical migrated tile bytes, with `bRepairBorders` false, is a supported tool path, and that `Zenith_ExtractNavigationGeometry` accepts an empty `axWater` span ("Bake geometry" in `Zenith2/Docs/Design/WORLD/Bakes.md`); ENV's text states neither for a WORLD caller.
<!-- /assumption -->

<!-- elaboration -->
WORLD reads terrain through ENV's document API and nothing else. `Zenith_WorldTerrainTileSource final : public Zenith_TerrainTileSource` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Source access interface") answers ENV's `ReadTile(strRelativePath)` by `Zenith_WorldSourceAccess::OpenDocumentRelativeTo(terrainGUID, strRelativePath)` and returns the canonical bytes of the opened, migrated tree (`Zenith_WriteCanonicalJson` with the canonical schema the access's migration registry holds for ENV's tile type), `ENV_ERROR_TILE_MISSING` when the access answers not found, `ENV_ERROR_TILE_READ` for its error. `Zenith_WorldLoadTerrain(access, terrainGUID, arena, heap)` opens the terrain document, writes its canonical bytes likewise and calls `Zenith_TerrainDocument::Load(bytes, tileSource, access.GetCodecs(), false, heap)` (`Zenith2/Docs/Design/ENV/Terrain.md`; `bRepairBorders` false, the cook's setting), so every tile ENV's loader reads is a recorded input. A terrain is loaded at most once per partition call and only when a consumer needs its heights: a cook-mode `world.scatter` or `world.probeGrid` whose `surface` names it (in the importer and in the editor's plan operation) and a lighting unit's prepare (in the editor). The lighting fingerprint needs only the terrain document and the digests of the tiles intersecting the gather rectangle, which the fingerprint opens without loading the terrain. Consequently a stroke changes a recorded input of `world.world` whenever the world reads that tile: the world re-imports, a cook-mode scatter over the edited terrain is placed again in that import (the "scatter that follows terrain edits" of `Zenith2/Docs/Design/WORLD/Generation.md` → "Execution modes"), and W41 is recomputed for every lighting unit whose gather rectangle holds the tile (`Test_WorldImporter_ReimportOnTerrainStroke`).

## Partition algorithm

`Zenith_PlanWorldPartition(const Zenith_WorldPartitionInput&, Zenith_Arena&, Zenith_WorldDiagnostics&)` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Partition functions") computes a `Zenith_WorldPartitionPlan` on the calling thread; the importer then writes the documents and declares the products. Steps, each over sorted inputs:

1. **Read the world.** `Zenith_ReadWorldDocument` → `Zenith_WorldDesc`, the reader rules of `Zenith2/Docs/Design/WORLD/Documents.md` → "World validation messages". An ERROR stops here. Then the importer pass opens, through the source access, the persistent scene (W5), AI's navigation settings document when navigation is enabled (W13, W46), every terrain document of `terrains` (W14) and every generator document in generator-key order (W30, W31, W32, W17, W39, then W33 and W36 over the opened set); every finding of this pass is reported before an ERROR stops the partition.
2. **Grid table.** Base grids in document order (indices 0 … B − 1, B = `layerGridBase`, W28), then one layer grid per entry i of `layerGridOrder` at index B + i: a copy of `layerGridTemplate` named `layer.<LayerKey>` (the entry's first 8 characters), role `gameplay`, `hlod` `{ "levels": 0 }`, `dependsOn` the grid that "Grid assignment" step 3 resolves for the `collision` role (`defaultFor`, else the first base grid of STREAM role `collision`) when that grid's STREAM role is `collision`, plus the grid it resolves for the `navigation` role only when that grid's STREAM role is `navigation`, never a `gameplay` grid, in grid index order (so a world whose collision grid lists no `defaultFor` still gives every layer grid its collision dependency, and a navigation role that falls back to a gameplay grid adds no edge: such an edge would pin gameplay cells to a layer's reach, which STREAM's V23 bound forbids at world scale, row 11 below), and the STREAM grid member `defaultActive` set to the layer's `initiallyActive` (row WD19). A retired entry (its `id` names no current grid-realised layer) gets the same grid with `defaultActive` false, no cell and `dependsOn` `[]`: it has no cell that needs a dependency, so it adds nothing to any grid's `reach` and, having no cell, `B'` 0 of its own; every later index stays where it was. B + `layerGridOrder` length ≤ 8 (W10).
3. **Resolve regions.** For each region object in key order: open its document by path (W20); resolve it with `Zenith_ResolveDocument` against the import prefab provider and the frozen type registry (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_PrefabResolver header"); a SCENE error is `WORLD_ERROR_PREFAB_RESOLUTION` with the `Zenith_SceneErrorCode` as detail. For each resolved entity compute its **world transform** in `double`: the region anchor `A = (origin.x + x·S, baseY, origin.z + z·S)` plus the composed local transforms of the hierarchy (rotation and scale composed in `float`, the translation accumulated in `double`). Collect the **roots**: entities and instances of the region document whose `parent` is null; a root's **hierarchy** is the root and every descendant in the resolved data. Check R3 (W21), R4 (W22). Insert every entity ID into the world ID index (a `Zenith_HashMap<Zenith_PersistentID, uint32>` keyed by ID, value the hierarchy index), refusing a duplicate (W23); the persistent scene's entity IDs are inserted first with value `uWORLD_PERSISTENT_LAYER`.
4. **Run cook-mode generators** in the topological order of `dependsOn` (ties by generator key): for each, `Zenith_PrepareWorldGenerator` then `Zenith_RunWorldGeneratorRegion` for every region of its plan in plan order (`Zenith2/Docs/Design/WORLD/Generation.md` → "Running a generator"); each produced hierarchy joins step 3's list with the generator's layer and the region that owns it, and its IDs join the ID index (a collision with an authored ID is W23). Baked-mode generators contribute through their generated region documents, already resolved in step 3.
5. **Classify hierarchies.** For each hierarchy H (in the order region key, then root sort key, then root ID; generated hierarchies after authored ones of the same region, in output key order): its layer L decides the destination: EDITOR_ONLY → dropped (its IDs stay in the index, flagged dropped, for step 9's warnings); BAKED → step 6; RUNTIME `grid` → L's layer grid, skipping step 6 (step 7 places it); RUNTIME `additive` (unstreamed worlds only, W25) → the layer scene of L (step 11); in unstreamed mode every BAKED hierarchy goes to the content scene ("Unstreamed worlds").
6. **Grid assignment** of BAKED hierarchies to base grids ("Grid assignment").
7. **Cell assignment.** For a hierarchy assigned to grid g with cell size c and origin O: the cell is `(floor((P.x − O.x) / c), floor((P.z − O.z) / c))` of the root's world position P, computed in `double` with `floor` toward negative infinity. Check every entity of H against STREAM's bound (`Zenith2/Docs/Design/STREAM/Formats.md` → "Entity bounds and the overlap margin"): its world position inside the cell rectangle grown by `overlapMargin × c` and inside `[baseY, baseY + height)`. A violation of a BAKED hierarchy with `largeExtentPromotion` and a base grid listing `large` in `defaultFor` (or of role `large`) whose bound the hierarchy satisfies moves H there (counted in the report's `promoted`); otherwise, and always for a layer-grid hierarchy, `WORLD_ERROR_ENTITY_BOUNDS` naming the entity, the cell and the distance outside.
8. **Static collider split** ("Static collider split") for BAKED hierarchies assigned to a base grid whose role is not `collision`; layer-grid hierarchies are never split.
9. **References and persistence.** "Cross-cell references" (cell content, layer scene content and the persistent scene), then "Persistent markers".
10. **Navigation and contributed content.** With navigation enabled, first the navigation input scenes and AI's products ("Navigation input scenes"), whose product keys AI's cell contribution reads; then "Contributed cell content": ENV per terrain-holding cell, AI per navigation cell, LIGHT per lighting cell.
11. **Layer scenes.** For each additive gameplay layer with at least one hierarchy: a layer scene ("Layer realisation").
12. **HLOD nodes.** `Zenith_BuildWorldHlodNodes` over the final cell content of every base grid with HLOD levels (`Zenith2/Docs/Design/WORLD/Bakes.md` → "HLOD workflow"); layer grids have none (W38).
13. **Sort and number.** Cells sorted by (grid index, z, x), the cell index being the position; hierarchies inside a cell sorted by (layer index, region key, root sort key, root ID), generated after authored; contributed entities after them in (contributor ENV, AI, LIGHT; then the contributor's order). The limits of "Partition limits" are checked here, before anything is written or declared.
14. **Emit.** The importer writes each cell's scene document ("Cell scene documents"), each layer scene, each navigation input scene, the streaming world document ("Streaming world document generation"), the HLOD recipes and the lighting intermediates, declares every product ("Products and keys") and reports the summary as one INFO diagnostic per grid.

Complexity: O(E log E) in the resolved entity count E (the sorts and the ID index), plus O(R) region opens, the generators' own costs and the HLOD clustering of `Zenith2/Docs/Design/WORLD/Bakes.md`. Memory: the resolved data of every region at once, in the scratch arena; the pending row "World partition import memory" (`Zenith2/Docs/Design/WORLD.md` → "Pending row needs") bounds it.

<!-- informative -->
UE5's World Partition generates its runtime cells at cook time from actors stored one per file; Zenith2 does the same split, with region documents in place of one-file-per-actor (a region of 256 m is a coarser lock than an actor but keeps SCENE's document model, prefab instancing and canonical merge unchanged) and the partition as an import whose output is cached by ASSET's keys. The old engine had no partition at all: Zenithmon's maps were single `.zscen` files whose layout changes had to be iterated to a fixed point by hand, re-deriving the camera clamps, walk paths and terrain pads after each move (`Games/Zenithmon/Docs/MapLayoutPlaybook.md`); WORLD's partition is a single pure function of the documents, so a layout change never needs a second pass to settle.
<!-- /informative -->

### Partition cost and scaling

<!-- elaboration -->
Every input edit re-imports the whole world, because the import key covers every input: a region edit, a prefab edit, a generator edit, a terrain tile the world reads (inside a headed lighting unit's gather rectangle, within a baked generator's surface reach, or of a terrain a cook-mode surface names), and a mesh whose decoded LOD 0 changes (its `geom.lod0Summary` property hash is an input). A `uZENITH_ENGINE_COOK_VERSION` bump or a codec upgrade that leaves every LOD 0 alone leaves those property hashes, and therefore the world's import key, unchanged. An Android cook pays one extra cost: the Windows mesh item of every summary it reads, cooked into the DDC only (shared with a Windows cook of the same DDC). The full import is O(E log E) over every resolved entity of the world plus every cook-mode generator over its whole scope; play in the editor after an edit pays it once before the incremental cook of the changed cells' items. What keeps the downstream cost proportional to the edit is byte stability: every intermediate whose inputs did not change is byte-identical ("Partition determinism"), so ASSET's cook items of unaffected cells, layer scenes, navigation input scenes, HLOD recipes and navigation tiles hit the DDC and do not re-cook (`Test_WorldImporter_ReimportOnRegionEdit`).

The import itself re-resolves every region at each re-import. A per-region resolution memo is not part of this design: on a memo hit the importer would still have to open and parse every region document (the cell writer copies its instances and sort keys) and every prefab the region reads (to record it as an input and to check its digest against the memo's list), so a memo saves only SCENE's resolution pass, at the price of a second representation of resolved content whose equivalence with `Zenith_ResolveDocument` across SCENE versions would need its own proof, and of an importer derived-data interface in ASSET (row WD31 withdraws WD21 (c)). The pending row "World partition import time" is measured on OpenWorldSlice's world both as a clean import and as the re-import after a one-region edit; its ruling sets the bound both must meet, and a measured breach of the re-import half is the evidence a memo would need, which would then be a WORLD design change keyed by the region document's digest, the type registry's shape hash, `uSCENE_COOKED_FORMAT_VERSION` and `uZENITH_ENGINE_COOK_VERSION`, and would record every prefab it lists as an input on a hit.

## Grid assignment

**Static** is defined once here and used by every WORLD rule that needs it (the role rules, "Static collider split", W27, and `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake geometry", "Lighting fingerprint", "Instance eligibility"). PHYS composes a body from an entity carrying `Zenith.Physics.RigidBody` plus the colliders of its descendants that carry no rigid body of their own (`Zenith2/Docs/Design/PHYS/ComponentsAPI.md` → "Zenith_PhysicsComponents header"); an entity's **body** is therefore the rigid body on the entity itself, else on its nearest ancestor that carries one, else none (an implicit static body).

- An entity is **static** when its body is none or has property 1 `MotionType` `PHYSICS_MOTION_TYPE_STATIC` (`Zenith2/Docs/Design/PHYS/PublicAPI.md`: `PHYSICS_MOTION_TYPE_STATIC = 1`); it is **moving** when its body has `PHYSICS_MOTION_TYPE_KINEMATIC` or `PHYSICS_MOTION_TYPE_DYNAMIC`.
- A **static collider** is a `Zenith.Physics.Collider` with property 7 `IsTrigger` false on a static entity. It is **implicit** when the entity's body is none and **explicit** when its body is a STATIC rigid body, whose `Layer`, `Flags` and `CollisionGroup` (properties 7, 9, 10) filter its collisions.

A BAKED hierarchy H of layer L goes to:

1. L's `grid` when it is set;
2. otherwise the grid of its **role**, chosen from the components of H by the first matching row of `axWORLD_ROLE_RULES` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Role rules"):

| # | Rule over H's resolved components (stable names) | Role |
|---|---|---|
| 0 | at least one entity of H carries `Zenith.Physics.RigidBody` with property 1 `MotionType` `PHYSICS_MOTION_TYPE_KINEMATIC`, and no entity carries one with `PHYSICS_MOTION_TYPE_DYNAMIC` (a kinematic platform with everything it carries: meshes, scripts, triggers) | collision |
| 1 | every entity of H is static and carries only `Zenith.LocalTransform`, `Zenith.EntityName`, `Zenith.Physics.Collider` and `Zenith.Physics.RigidBody` with `MotionType` `PHYSICS_MOTION_TYPE_STATIC`: implicit and explicit static colliders and trigger volumes, whatever `IsTrigger` says | collision |
| 2 | every entity of H carries only `Zenith.LocalTransform`, `Zenith.EntityName` and AUDIO's spatial components (`Zenith.Audio.Emitter`, `Zenith.Audio.Zone`, `Zenith.Audio.Occluder`) | audio |
| 3 | every entity of H carries only `Zenith.LocalTransform`, `Zenith.EntityName` and `Zenith.AI.NavAreaVolume`, `Zenith.AI.OffMeshLink` | navigation |
| 4 | anything else | gameplay |

3. the role's grid is the base grid whose `defaultFor` lists the role; else the first base grid whose STREAM role equals it; else the first gameplay grid.

A rule names component stable names, never C++ types, so WORLD links no module library for it; the table is data in `Internal/WORLD/Partition/Zenith_WorldRoleRules.cpp` and its rows change with a WORLD design diff only. Rows 0 and 1 conform to STREAM's role table (`Zenith2/Docs/Design/STREAM/Formats.md` → "Content of each grid role": `collision` holds "static colliders, terrain colliders, trigger volumes, kinematic platforms"); the differences, trigger colliders inside a mixed gameplay hierarchy and explicit static bodies inside one, are stated in "Static collider split" and carried by row WD19 (d).

<!-- assumption: pending AUDIO, AI (row WD13 lists the names WORLD relies on) -->
The audio and navigation stable names are the ones AUDIO's and AI's designs use for spatial content (`Zenith.Audio.Emitter`, `Zenith.Audio.Zone`, `Zenith.Audio.Occluder`; `Zenith.AI.NavAreaVolume`, `Zenith.AI.OffMeshLink`); a rename in either design changes this table in the same diff.
<!-- /assumption -->

## Static collider split

STREAM requires static colliders to live in COLLISION cells (`Zenith2/Docs/Design/STREAM/Formats.md` → "Content of each grid role"), so a gameplay body never activates above absent collision. With `splitStaticColliders` true, the split applies to every entity e of a BAKED hierarchy H assigned to a base grid whose role is not `collision`, when all of these hold:

- e's collider is an implicit static collider ("Grid assignment"): `Zenith.Physics.Collider` with `IsTrigger` false and no `Zenith.Physics.RigidBody` on e or any ancestor. An explicit static collider is never split: its collider is one shape of a STATIC body composed with the colliders of that body's other entities, whose filtering properties a proxy carrying only the collider would drop, so the whole body stays with its hierarchy under the radius rule below;
- no entity of H carries `Zenith.Streaming.Persistent`, `Zenith.Behaviour` (SCRIPT's behaviour component) or `Zenith.Physics.Joint`, and no component of any entity of H has a type whose origin is `TYPE_ORIGIN_SCHEMA` (a game's data-defined component, `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeInfo header"): such a hierarchy is gameplay that may be destroyed, tombstoned, scripted or jointed, and its collision must live and die with its entity, so it keeps its collider.

For each such e:

1. A **collider proxy** entity is created with ID `Zenith_WorldColliderProxyID(e)` = `Zenith_DeriveAuthoringID(e, xWORLD_SCOPE_COLLIDER_PROXY)`, name `<e's name> (collision)`, components `Zenith.LocalTransform` (e's world transform, made relative to its collision cell's anchor in the cell document), e's `Zenith.Physics.Collider` field set unchanged, and `Zenith.Physics.ColliderProxy` with property 1 `Source` = e's ID (an `ENTITY` property, bound as a cross-cell reference by "Cross-cell references" when e's cell differs from the proxy's).
2. The proxy is assigned to the collision grid (step 7 on its own position) and joins that cell's content.
3. e loses its `Zenith.Physics.Collider` in the cell document: a plain entity by not copying the component, an entity of a prefab instance by a `removedComponents` override on its target, written with SCENE's edit (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Document edits for the editor").

A static collider that is not split keeps its collider with its hierarchy, and every grid that receives one obeys the **radius rule**: its `activationRadius` is at least the collision grid's `activationRadius + unloadHysteresis` (W27, naming the grid, the first such entity and its layer), so a body never simulates above a collider that is not loaded. The rule covers exactly four cases: `splitStaticColliders` false (every grid that receives a static collider); a hierarchy the second bullet keeps whole; an explicit static collider (its STATIC body stays whole, first bullet); and every layer grid that holds a static collider (a runtime layer's collision must follow the layer's grid bit, so it stays in the layer's own grid and never reaches the base collision grid). A hierarchy that rows 0 and 1 of "Grid assignment" send to the collision grid needs no rule: it is collision content. A trigger collider in a hierarchy that rows 0 and 1 of "Grid assignment" do not send to the collision grid stays with its hierarchy, because gameplay listens on the trigger's own entity (`Zenith.Physics.TriggerEntered`); a trigger holds no body up, so the radius rule does not apply to it. The report lists every split (`splits`) and every unsplit static collider (`unsplit`).

<!-- assumption: pending PHYS (rows WD18, WD20) -->
PHYS treats an entity with `Zenith.Physics.Collider`, no rigid body and no parent body as an implicit static body whether or not another entity carries the same shape, so a proxy collides exactly as the original collider did. PHYS registers the component `Zenith.Physics.ColliderProxy` (version 1; property 1 `Source`, `ENTITY`, `PHASE_COMMANDS_ONLY`, serialized) and gives it two behaviours: every contact record, query hit and trigger event that names the proxy's body names `Source` instead whenever `Source` is non-null and live (a proxy whose source cell is not active reports itself); and when `Source` holds a handle whose entity no longer exists (destroyed while both cells were active), ingest removes the proxy's body and creates none for it while that holds. When the source cell unloads, STREAM nulls `Source` and the proxy collides again: the destroyed entity was not persistent (a persistent one is never split), so its cell reloads it authored, and its collision is present exactly as the reloaded entity's would be.
<!-- /assumption -->

## Layer realisation

| Layer | Realisation |
|---|---|
| BAKED data layer | its hierarchies are content of the base grids' cells (steps 6 and 7) |
| RUNTIME gameplay layer, `grid` (streamed worlds) | its hierarchies, their static colliders and triggers included, are content of its layer grid `layer.<LayerKey>`; the layer grid is an ordinary STREAM grid, so its cells stream, persist and bind references exactly as base cells; the layer is active for a source when the source's effective `GridMask` has the layer grid's bit; with the bit clear, none of its content exists, collision included |
| RUNTIME gameplay layer, `additive` (unstreamed worlds, W25) | its hierarchies are the entities of one layer scene, product `layer:<layer id>`, positions relative to the world origin; loaded and unloaded whole by gameplay through SCENE's additive load; never streamed by STREAM |
| EDITOR_ONLY data layer | nothing reaches a cook output |

- **Initial state.** The partition writes no source: a `Zenith.Streaming.Source` is authored content (GAME's pawn and camera, `Zenith2/Docs/Design/STREAM.md` → "Areas that consume STREAM"). It writes the initial state as cooked data instead: each layer grid object of the streaming world document carries `defaultActive` = the layer's `initiallyActive` (base grids `true`), and STREAM computes a source's effective mask from the bound world's default mask (row WD19 (a)) and the source's own override pair (row WD32 (a); fenced as pending STREAM in `Zenith2/Docs/Design/WORLD.md` → "Script bindings"): `effective = ((defaultMask & GridMask) | GridForcedOn) & ~GridForcedOff`, with `GridMask` keeping its default `0xFF` and both override masks defaulting to 0, so an untouched source activates no cell of a layer with `initiallyActive` false (`Test_WorldCooked_LayerInitiallyInactiveByDefault`). A gameplay script obtains a layer grid's index with the STREAM binding of row WD7 (c) (`Streaming.GetGridIndex("layer.<LayerKey>")`) and switches the layer on by setting its bit in `GridForcedOn` and clearing it in `GridForcedOff`, off by the converse.
- **Activation order.** STREAM freezes bodies by the region grid (`Zenith2/Docs/Design/STREAM/Runtime.md` → "The region", "Physics coordination"), and a layer cell depends on the base collision and navigation grids (step 2) but on no other layer cell. A layer's own static colliders are therefore present whenever its cells are (W27), but a dynamic body of layer cell A resting on a collider of layer cell B, or a base gameplay body resting on a layer collider, may unfreeze while B is still loading, and its support may drop until B activates. Base content split into collision cells has no such window. A layer grid depends on a navigation grid only when the world has one of STREAM role `navigation` (step 2), so where navigation tiles live in a gameplay grid, layer content beyond that grid's activation radius may stand where no navmesh tile is loaded; AI agents of a layer should stay within the base gameplay radius, or the world gives navigation its own grid. A layer's dynamic bodies should rest on base terrain, base collision or their own cell's colliders; content that rests across layer cells carries that window.
- **Save and restore.** The layer's switch state is the sources' `GridMask`, `GridForcedOn` and `GridForcedOff` properties and SCENE's additive instance set, all saved by their owners; WORLD adds no runtime state. A layer grid's index is `layerGridBase` + its position in `layerGridOrder`, which only grows ("Streaming object" of `Zenith2/Docs/Design/WORLD/Documents.md`), so a saved override mask names the same layers after a content update that adds, renames or deletes layers. Because a saved source records only the layers gameplay forced on or off, a layer that a content update adds with `initiallyActive` true is active for every source, old saves and toggled sources included, unless gameplay forces it off; under the alternative of row WD19 (b) (an explicit mask that, once written, replaces the default mask) such a layer would instead stay off for every source that ever toggled any layer, which is why WORLD asks for the override pair (`Test_WorldCooked_ContentUpdateLayerAppearsForOldSave`). A base-grid change is refused (W28) until `world.streaming.renumberLayerGrids` renumbers the layer grids, the one edit whose consequence for saves the editor states before applying it.
- **Navigation.** A runtime layer's content is in no navigation input scene (its `bakes.navigation` is false, W19); content that must change navigation when the layer is active carries AI's dynamic obstacle component, which AI applies at run time (`Zenith2/Docs/Design/AI/Navigation.md` → "Dynamic obstacles").
- **Lighting and HLOD.** A runtime layer's content is neither lightmapped nor in HLOD proxies (W19, W38); it is lit by the irradiance volumes of the base cells and dynamic lights.
- **Extent of an additive layer.** Every entity of an additive layer scene lies within `uSTREAMING_PRECISION_RADIUS_METRES` (4,096 m) of the world origin on each axis (`WORLD_ERROR_ENTITY_BOUNDS`), the radius within which a binary32 position keeps STREAM's 0.244 mm spacing (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Precision target"). An unstreamed world binds no streaming world, so no origin rebase ever moves the origin from 0 and the layer scene's world-origin-relative positions stay correct.

<!-- elaboration -->
Additive realisation is refused in streamed worlds (W25) because SCENE's additive load from a script places a scene's roots at their stored positions relative to the current origin O, with no root translation (`Zenith2/Docs/Design/SCENE/ModuleSeams.md`: script loads carry tag 0, so no hook supplies `GetRootTranslation`), while a streamed world may rebase O by STREAM's 2,048 m quantum (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Rebase rule"); a layer scene loaded after a rebase would appear offset by O. A grid-realised layer streams through STREAM's admission, which applies the root translation `(float)(anchor − O)` to every cell, so it is correct under rebasing and is the one gameplay-layer realisation a streamed world needs.

## Cross-cell references

For every entity of every destination scene, and of the persistent scene, every `ENTITY` property and every element of a `FIXED_ARRAY` of `ENTITY` of its known components (found with `Zenith_WalkProperties` over the type info) whose value is a non-null persistent ID T:

| Where the source is | Where T is | Action |
|---|---|---|
| a cell, a layer scene or the content scene | the same scene | kept |
| a cell, a layer scene or the content scene | the persistent scene | kept: SCENE resolves it at activation (`Zenith2/Docs/Design/STREAM/Formats.md` → "Cross-cell reference nulling") |
| a cell | another cell of any grid | the value is written null and a reference object joins the streaming world document (source cell, target cell, source entity, target entity, component stable name, property ID, element or null) |
| a cell | an additive layer scene; or a layer scene, to a cell | written null; `WORLD_WARNING_REFERENCE_UNBINDABLE` |
| the persistent scene | any region content (a cell, a layer scene or the content scene) | the persistent scene is not written by the partition, so the value stays as authored and resolves to nothing at run time; `WORLD_WARNING_REFERENCE_UNBINDABLE` naming the persistent scene's path, the entity, the component and property, and the target's region document path |
| any | a dropped (EDITOR_ONLY) entity, or no entity of the world | written null in a destination scene; `WORLD_WARNING_REFERENCE_UNRESOLVED` |

A reference inside an instance's resolved entity is written as a `properties` override of that target when its value changes. References per cell are capped by STREAM's limit (4,096 each direction, `Zenith2/Docs/Design/STREAM/Formats.md` → "Limits"); exceeding it is `WORLD_ERROR_LIMIT` naming the cell. Complexity O(Σ ENTITY properties × log E).

## Persistent markers

For every entity carrying `Zenith.Streaming.Persistent` in a cell: property 2 `CellScene` is set to the cell's GUID and property 3 `EntityID` to the entity's own ID (`Zenith2/Docs/Design/STREAM/PublicAPI.md` → "Zenith_StreamingComponents header"); property 1 `Mode` is the author's. An authored value of property 2 or 3 is overwritten without a finding (authors cannot know the generated cell). A persistent entity in an additive layer scene keeps its component with property 2 null, `WORLD_WARNING_PERSISTENT_IN_LAYER_SCENE` (STREAM persists only cell content). More than 1,024 persistent entities in one cell is `WORLD_ERROR_LIMIT` (STREAM's V18).

## Contributed cell content

Three areas add entities to cells through one interface WORLD declares, `Zenith_WorldCellContentWriter` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Cell content writer"); WORLD calls each with the cell's bounds `Zenith_WorldCellBounds` and appends what it writes after the cell's authored content, in the order the contributor writes it. A contributor never sees another cell. The partition reaches ENV and AI through the contributor seam `Zenith_WorldCellContributor` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Partition functions"): the importer passes `Zenith_WorldEnvironmentContributor` (which reads ENV's published cell content) and `Zenith_WorldNavigationContributor` (which calls AI's function), the production implementations of the table below, and tests pass a recording implementation.

| Contributor | Called for | With | Writes |
|---|---|---|---|
| ENV's published cell content, placed by WORLD's `Zenith_WorldEnvironmentContributor` (ruling WORLD-WDB9-WDB6) | every cell of a base grid that holds terrain content (`Zenith2/Docs/Design/ENV/Bake.md` → "Cell content records"): GAMEPLAY cells for tiles and foliage cells, the COLLISION grid for terrain and foliage colliders, the NAVIGATION grid for ENV's navigation element when ENV-102 publishes one (`Zenith_WorldNavigationContributor` places it beside AI's derived tile content) | for each 128 m environment cell whose minimum corner lies in the cell, the property `environment.cellContent.<x>_<z>` of each listed terrain, read with `ReadPublishedProperty(terrain, ASSET_PLATFORM_WINDOWS, …)`: a JSON array of GUID strings in the order tile, foliage cell, terrain collider, foliage collider, navigation | one entity per present GUID at the environment cell's minimum corner (cell-anchor-relative, in `double`, narrowed once): `Zenith_TerrainTileComponent` (terrain ID, tile coordinate) for the tile, `Zenith_FoliageCellComponent` (cell coordinate) for the foliage cell, PHYS's static collider component for each collider |
| AI, `Zenith_AIWriteCellNavigationContent` (`Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles") | every cell of the base grid listing `navigation` | AI's manifest view built with `Zenith_AIMakeNavigationManifestView` over the navigation products the importer itself declares ("Navigation input scenes") | one `Zenith.AI.NavMeshTiles` entity per (class, up to 16 tiles) and one `Zenith.AI.NavVolumeRef` per volume region whose minimum corner lies in the cell |
| LIGHT data, written by WORLD itself | every cell of the lighting grid with a light bake header | the header and the lighting product GUIDs | `Zenith.Lighting.LightmapSet` on the anchor (property 1 the `lightmapSet:` product); the `Zenith.Lighting.ReflectionProbe` property 2 `BakedAsset` of each baked probe set to its `probe:` product (a property change of an authored entity, which is content of this lighting cell because a baked probe is one of a hierarchy assigned to the lighting grid, `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake geometry"); one irradiance-volume entity `Zenith_DeriveAuthoringID(cellGUID, xWORLD_SCOPE_IRRADIANCE_VOLUME)` with `Zenith.LocalTransform` at the volume's minimum corner (cell-anchor-relative) and `Zenith.Lighting.IrradianceVolume` (1 the `volume:` product, 2 priority 0, 3 blend distance = the volume spacing) |

<!-- assumption: pending AI (row WD11) -->
AI's function takes WORLD's `Zenith_WorldCellBounds` and `Zenith_WorldCellContentWriter`, and AI supplies `Zenith_AIMakeNavigationManifestView(Zenith_Span<const Zenith_WorldProduct>, Zenith_Arena&)`; AI's design adopts both names (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"). It lives in ZenithAuthoring (`Zenith_AuthoringAI.h`); the importer's navigation contributor calls it. ENV declares no WORLD-facing function: ruling WORLD-WDB9-WDB6 withdrew `Zenith_EnvironmentWriteCellContent`, and WORLD places ENV-102's published cell content itself (row 1 above).
<!-- /assumption -->

**Unstreamed worlds.** There are no grids, so the partition calls the ENV contributor once per role bit of its `GetRoleMask()` (gameplay, then collision) with one `Zenith_WorldCellBounds`: `m_uGridIndex` 0xFF, `m_eRole` that role, `m_iX` = `m_iZ` = 0, the rectangle the union of every region's rectangle and every listed terrain's extent, each edge rounded outward to a multiple of 128 m from the world origin, the world's `baseY` and `height`, and `m_xCellScene` the content scene's GUID (the world's `id`). Positions the contributor writes are relative to that rectangle's minimum corner at `baseY`; the content writer adds that corner back in `double` and narrows once, so the content scene keeps its world-origin-relative positions. The environment contributor writes the entities of every environment cell whose minimum corner lies in the rectangle, which is every tile of every listed terrain. The AI contributor is not called (the world has no NAVIGATION cell); "Unstreamed worlds" writes its manifest references instead.

A stale or missing light bake still contributes nothing worse than the previous result: a STALE header's products are attached (the old lighting is better than none, W41 warns), a MISSING one attaches nothing (W40).

## Navigation input scenes

With `bakes.navigation.enabled`, step 10 derives AI's bake inputs inside the import, as cells are derived, so every navigation input is world-correct, includes cook-mode generator outputs and is never a tracked file:

1. **Ownership.** Every hierarchy of a BAKED layer with `bakes.navigation` true belongs to exactly one coordinate: an authored hierarchy to the coordinate of its region document, a generated hierarchy (baked or cook-mode) to the coordinate whose rectangle holds its output's owner point (`Zenith2/Docs/Design/WORLD/Generation.md` → "Generator terms"). Membership is by owner, never by where the root stands, so a root outside its own region is never dropped from the navmesh; its distance from its owner is bounded by W21's allowance for authored roots and by the generator's `jitter` for generated ones, and a hierarchy whose colliders reach farther than `regionSize` beyond its owning rectangle is W47.
2. **Input scenes.** For each coordinate (x, z) that owns at least one such hierarchy, in (z, x) order, one scene document, product `navInput:<x>_<z>`: an anchor root as a cell's (ID `Zenith_WorldAnchorEntityID(navInputGUID)`, identity transform, no `Zenith.Streaming.CellAnchor`), then every hierarchy it owns (authored, baked-generated and cook-mode-generated, collider proxies excluded because the originals are kept here), in step 13's hierarchy order, each root's position rewritten relative to the owning region's anchor `(origin.x + x·S, baseY, origin.z + z·S)` in `double` and narrowed once; runtime and editor-only layers are absent. Its static colliders, `Zenith.AI.NavAreaVolume` and `Zenith.AI.OffMeshLink` components are AI's inputs; other components are kept (AI ignores them) so the scene is exactly the resolved content. A terrain-only navigation region (`Zenith2/Docs/Design/WORLD/Documents.md` → "Derived navigation documents", second kind) gets no input scene.
3. **Derived documents.** For each navigation region of either kind, in (z, x) order, `Zenith_DeriveWorldNavigationDocument` builds the document of "Derived navigation documents" in memory, its `scenes` entries naming the `navInput:` products the 3 × 3 neighbourhood has, with their region anchors as `offset`, and its `terrains` entries naming the navigation-enabled terrains intersecting its bounds.
4. **AI's products.** For each derived document, in (z, x) order, the partition calls AI's declaration function (row WD23) with the plan's recording sink, which collects, and the importer's step 14 then declares through `DeclareDerivedProduct`, the region's manifest product `navigation/<x>_<z>` and its `navTile:` and `navVolume:` products with GUIDs `Zenith_WorldDeriveProductGUID(worldID, key)`, and writes each product's `ai.navTileJob` and `ai.navMappings` intermediates exactly as AI's importer `ai.navigation` writes them for a document source. A terrain-only region therefore bakes the tiles of its terrain alone where no neighbour's scene reaches, so agents path across open ground (`Test_WorldNavigation_TerrainOnlyRegionsGetDocuments`).

<!-- assumption: pending AI (rows WD23, WD34; AI revision 2 conformed WD23 in its row AI23) -->
`Zenith_AIDeclareNavigationProducts(const Zenith_JsonDocument& xDocument, Zenith_JsonNode xRoot, Zenith_StringView strManifestKey, Zenith_AINavigationProductSink& xSink, Zenith_Arena& xArena)` and the sink interface `Zenith_AINavigationProductSink` (`DeclareProduct`, `AttachIntermediate`) are declared in `Zenith2/Tools/Authoring/Zenith_AuthoringAI.h`, a ZenithAuthoring root header (`Zenith2/Docs/Design/AI/Internals.md` → "Zenith_AuthoringAI header"), and implemented in ZenithAuthoring's `Internal/AI/`. Two callers share it: `Zenith_PlanWorldPartition` (ZenithAuthoring, this section's step 4, with the plan's recording sink) and AI's own importer `ai.navigation` (ZenithCook-internal), which factors its declaration out into it. ZenithAuthoring code calls no ZenithCook-internal function (ruling AUTHORING-CONTRACT). AI's schema accepts a `scenes` element `{ "scene": <GUID>, "offset": [x, y, z] }` beside a path and `volumes` elements with an explicit `index` (AI23 (b), (c)); its step `ai.navMesh` reads a scene named by GUID from that scene product's cooked output (`ReadDependencyOutput`, decoded with SCENE's `Zenith_DecodeCookedSceneFile`) and adds `offset` to every entity's composed position in `double` before translating into tile-local coordinates (AI23 (d)). Row WD34 adds a `terrains` element of the form `{ "terrain": <GUID> }` beside a path, naming a terrain whose `navigation:` products AI reads exactly as for a path entry, and confirms that a document with an empty `scenes` array and a non-empty `terrains` array bakes terrain-only tiles. AI keeps no tracked navigation document (AI23 (e)), so WORLD's former fallback is withdrawn.
<!-- /assumption -->

## Cell scene documents

For each cell k with content (sorted index k), `Zenith_WriteWorldCellDocument` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Partition functions") builds a scene document with SCENE's API and serialises it canonically:

1. `Zenith_SceneDocument::CreateEmpty(SCENE_DOCUMENT_KIND_SCENE, cellGUID, "<WorldName>/<grid>/<x>_<z>")` (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header").
2. The **anchor root**: entity `Zenith_WorldAnchorEntityID(cellGUID)` = `Zenith_DeriveAuthoringID(cellGUID, xWORLD_SCOPE_ANCHOR)`, name `Anchor`, parent null, sort key `V`, components `Zenith.LocalTransform` identity and `Zenith.Streaming.CellAnchor` `{ 1: cellGUID, 2: 0, 3: worldID }` (`Zenith2/Docs/Design/STREAM/Formats.md` → "Anchor root").
3. Each hierarchy in step 13's order, through `SCENE_EDIT_CREATE_SUBTREE` (plain entities) or `SCENE_EDIT_PLACE_INSTANCE` (instances) with parent the anchor; the root's `Zenith.LocalTransform` position is rewritten to `(float)(P − cellAnchor)` (the cell's minimum XZ corner at the grid's base Y, `Zenith2/Docs/Design/STREAM.md` → "Cell key and coordinates"), rotation and scale unchanged; an instance root's position becomes a `properties` override on the prefab root's property 1. Descendants keep their local transforms, IDs and sort keys.
4. Sort keys of the anchor's children: `Zenith_MakeEvenSortKeys(n)` in step 13's order, so the cell's activation order is a function of the order alone.
5. Collider proxies, reference nulling, persistent markers and contributed entities as the steps above.
6. `Serialize` → the canonical bytes, written as the product's intermediate blob `scene.document` (row WD6).

The documents pass STREAM's cell scene checks V12 to V18 by construction; `Test_WorldPartition_CellsPassStreamChecks` runs STREAM's `Zenith_ValidateStreamingWorld` over a fixture world's output. Layer, content and navigation input scenes are written by the same steps with their own root rule (no `Zenith.Streaming.CellAnchor` on them; layer and content scenes have no anchor root at all).

<!-- assumption: pending SCENE (row WD6) -->
SCENE's `scene.scene` step, for an item whose asset is a product declared by an importer other than `zenith-doc` and which carries the intermediate blob `scene.document`, reads the document from that blob (parsed and validated by `Zenith_SceneDocument::Load` exactly as a source document, at the current schema version) instead of `OpenSourceDocument`, keys it by the blob's SHA-256 under the row name `scene.document.intermediate`, and resolves prefab instances through its provider as for any scene; every other behaviour of "Scene and prefab cook steps" (`Zenith2/Docs/Design/SCENE/Prefabs.md`) is unchanged.
<!-- /assumption -->

## Streaming world document generation

`Zenith_WriteWorldStreamDocument` writes STREAM's document (`Zenith2/Docs/Design/STREAM/Formats.md` → "Streaming world document") into the primary product's intermediate blob `stream.worldDocument`, canonical in STREAM's member order:

| STREAM member | Value |
|---|---|
| `$type`, `schemaVersion` | `"Zenith.Streaming.WorldDocument"`, 1 |
| `worldId` | the world document's `id` |
| `grids` | the grid table of step 2, each a STREAM grid object (WORLD's `defaultFor` removed; `defaultActive` written for every grid, row WD19) |
| `cells` | every cell with content, sorted by (grid index, z, x): `{ "grid", "x", "z", "scene": cellGUID }`; `dependsOn` absent (grid-derived dependencies suffice) |
| `hlod` | every HLOD node of step 12, sorted by (grid index, level descending, z, x): `{ "grid", "level", "x", "z", "meshes": [hlod mesh GUIDs], "castsShadows" }` |
| `references` | every reference of "Cross-cell references", sorted by (target cell, source cell, source entity, component, property, element) |

<!-- assumption: pending STREAM (rows WD7, WD19) -->
STREAM's `stream.world` step reads its document from the intermediate blob `stream.worldDocument` when its item is a product declared by the importer `world.world` (the document has no source file), keyed by the blob's SHA-256; a hand-written `*.streamworld.json` keeps working as STREAM specifies. STREAM also exposes `Zenith_ValidateStreamingGridObject` (its V3 to V7 over one grid object) for WORLD's W11, and the READ_WORLD binding `Streaming.GetGridIndex(name: string): number?` for gameplay layers. Row WD19 adds the grid member `defaultActive` and its source semantics, accepts a grid with no cell (a retired layer slot), and amends STREAM's role table as "Static collider split" states.
<!-- /assumption -->

## Unstreamed worlds

With `streaming.mode` `"unstreamed"`, the importer declares no streaming world, no cell and no HLOD node. Every BAKED hierarchy (and every contributed ENV entity, whose cell is then the whole world) goes into one **content scene**, product `content`, positions relative to the world origin, sorted as step 13, with no anchor; additive gameplay layers get their layer scenes, grid-realised gameplay layers are refused (W26: no grid exists) and lighting is disabled (W29: lighting units are cells of a lighting grid, and an unstreamed world has none; its content is lit by the persistent scene's realtime probes, irradiance volumes the author places and dynamic lights). With navigation enabled, its navigation input scenes and AI's products are derived as in a streamed world, and the content scene receives, after its baked hierarchies, one entity per derived manifest `navigation/<x>_<z>` in (z, x) order: ID `Zenith_DeriveAuthoringID(manifestGUID, xWORLD_SCOPE_NAV_MESH_REF)`, name `Navigation <x>_<z>`, `Zenith.LocalTransform` identity and `Zenith.AI.NavMeshRef` with property 1 `Manifest` the manifest's GUID (`Zenith2/Docs/Design/AI/Navigation.md` → "Streamed tiles", last paragraph: in a world without streaming an entity carrying `Zenith.AI.NavMeshRef` names a manifest whose hard references make its tiles resident). The partition writes them because the manifests are derived products whose GUIDs no author sees, and whose set changes with every region added (`Test_WorldPartition_UnstreamedContentScene`). The terrain content of the content scene comes from the ENV contributor's single call ("Contributed cell content"). The game's persistent scene loads the content scene additively (GAME's world rules, or a scene author's `Scene.LoadAdditive`). Every entity lies within `uSTREAMING_PRECISION_RADIUS_METRES` (4,096 m) of the origin on each axis (`WORLD_ERROR_ENTITY_BOUNDS`, `Zenith2/Docs/Design/STREAM/Runtime.md` → "Precision target"); no streaming world is bound, so the origin never rebases. Unstreamed worlds serve tests, small games and the Stage 3 shape of W-Outdoor, which runs with every cell resident and no Streaming module (ruling Q73's form, recorded in `Zenith2/Docs/Design/STREAM.md` → "Reconciliations with other documents").

## Products and keys

The importer declares, in key byte order, with `DeclareDerivedProduct`:

| Key | Kind | Primary | Intermediate blobs | Cooked by |
|---|---|---|---|---|
| `world` | `ASSET_KIND_STREAMING_WORLD` | yes (streamed mode) | `stream.worldDocument` | STREAM `stream.world` |
| `content` | `ASSET_KIND_SCENE` | yes (unstreamed mode) | `scene.document` | SCENE `scene.scene` |
| `cell:<grid>/<x>_<z>` | `ASSET_KIND_SCENE` | no | `scene.document` | SCENE |
| `layer:<layer id>` | `ASSET_KIND_SCENE` | no | `scene.document` | SCENE |
| `navInput:<x>_<z>` | `ASSET_KIND_SCENE` | no | `scene.document` | SCENE |
| `navigation/<x>_<z>`, `navTile:<class>/<tileSize>/<tx>_<tz>_<layer>`, `navVolume:<class>/<index>/<xmm>_<ymm>_<zmm>` | `ASSET_KIND_NAV_MESH` | no | `ai.navMappings` (manifests), `ai.navTileJob` (tiles and volumes), written by AI's declaration function (row WD23) | AI `ai.navMesh` |
| `hlod:<grid>/<level>/<x>_<z>/<slot>` | `ASSET_KIND_MESH` | no | `geom.meshMergeRecipe` (row WD8) | GEOM `geom.mesh` |
| `lightmapSet:<grid>/<x>_<z>` | `ASSET_KIND_LIGHTMAP_SET` | no | `light.bakedLightmapSet` (row WD10) | LIGHT |
| `lightmapPage:<grid>/<x>_<z>/<NNN>/irradiance`, `…/direction` | `ASSET_KIND_TEXTURE` | no | `geom.textureSource` (row WD9) | GEOM `geom.texture` |
| `volume:<grid>/<x>_<z>` | `ASSET_KIND_IRRADIANCE_VOLUME` | no | `light.bakedIrradianceVolume` (row WD10) | LIGHT |
| `probe:<entity ID>` | `ASSET_KIND_REFLECTION_PROBE` | no | `light.bakedProbe` (row WD10) | LIGHT |

In streamed mode the source GUID (the world document's `id`) names the streaming world; in unstreamed mode it names the content scene (`Zenith2/Docs/Design/ASSET/Database.md` → "Sub-asset identity": the primary product takes the source's GUID). Every key is built from coordinates, grid names, layer `id`s, entity IDs and AI's keys; none contains a layer name or a generator name, so no rename changes a product key.

### Product identity

Every non-primary product's GUID is `Zenith_WorldDeriveProductGUID(worldID, key)` = `Zenith_DeriveAuthoringID(worldID, Zenith_WorldKeyID(key))`, where `Zenith_WorldKeyID(key)` is the first 16 bytes of the SHA-256 of the 6 ASCII bytes `"ZWKEY:"` followed by the key's UTF-8 bytes, read as a persistent ID in its cooked binary form (bytes 0 to 7 the high word, 8 to 15 the low word, each little-endian) with the version-4 bits forced as `Zenith_DeriveAuthoringID` step 3 forces them (`Zenith2/Docs/Design/SCENE/Prefabs.md` → "Derivation"). Consequences:

- A cell keeps its GUID across every re-cook while its grid name and coordinate stay, which is what STREAM's saved identities need (`Zenith2/Docs/Design/STREAM/Runtime.md` → "Cell identity across re-cooks"). A re-partition that changes a grid's cell size keeps the GUID of every coordinate that still holds content, but that GUID now names a different rectangle of the world, and coordinates that gain or lose content gain or lose their GUID; STREAM's re-homing (decoder check D9 and update step 14, "Cell identity across re-cooks") moves each saved record to the cell that holds its entity now, so no persistent state is lost (`Test_WorldCooked_RepartitionKeepsIdentity`).
- Renaming a layer or a generator changes no product GUID: the keys use the layer `id` and coordinates only (`Test_WorldPartition_LayerRenameKeepsProductGUIDs`).
- Two machines cooking the same documents produce the same GUIDs with no `.meta` write, so the world's `.meta` never lists cells and never conflicts in a merge (row WD4).
- SHA-256 is outside D-SEL-HASH (open, Stage 1), so the ruling changes no product GUID.

## Partition determinism

The partition is a pure function of its opened documents, the product lists and keys and the type registry: every collection is sorted by a total order before use (keys, IDs, coordinates); positions are computed in `double` and narrowed once; no hash-map iteration order, address, clock, thread count or host path reaches an output; generators are deterministic per `Zenith2/Docs/Design/WORLD/Generation.md` → "Generator determinism"; documents are canonical. The partition evaluates no transcendental function; the generators it runs evaluate CORE's deterministic ones only, so the intermediates are byte-identical on every machine running one build (ruling X314-FMA3, `Zenith2/Docs/Design/WORLD.md` → "Determinism obligations"). `Test_WorldPartition_SameInputsSameBytes` imports one fixture world twice and in two processes; `Test_WorldPartition_RegionOrderIrrelevant` permutes the region map's file order on disk and the order regions are opened, and compares every intermediate byte for byte; `Test_WorldImporter_CleanEqualsIncremental` is ASSET's clean-versus-incremental cook over the fixture game after a one-region edit.

## Partition limits

| # | Quantity | Limit | Source |
|---|---|---|---|
| 1 | grids (base + layer grids, retired slots included) | 8 | STREAM `uSTREAMING_MAX_GRIDS` |
| 2 | cells | 65,536 | STREAM `uSTREAMING_MAX_CELLS` |
| 3 | entities per cell scene, contributed included | 16,384 | STREAM `uSTREAMING_MAX_CELL_ENTITIES`; the game's smaller `ecs.maxSpawnsPerTick` is checked by STREAM's V16 in `stream.world`, which reads the cook context's configuration |
| 4 | persistent entities per cell | 1,024 | STREAM V18 |
| 5 | references per cell, each direction | 4,096 | STREAM |
| 6 | HLOD nodes | 65,536 | STREAM |
| 7 | entities of a layer, content or navigation input scene | 1,048,576 | SCENE's document limit |
| 8 | resolved entities of a world | 16,777,216 | `uWORLD_MAX_WORLD_ENTITIES` |
| 9 | a cell coordinate | ±2^20 per axis | STREAM V8 (`iSTREAMING_MAX_CELL_COORDINATE`): a region at coordinate ±2^20 of 2,048 m holds cells of a 32 m grid at coordinates up to ±2^26, so the bound is reachable |
| 10 | dependencies per cell | 32 | STREAM V19: the grid-derived dependencies of each cell (the overlapping cells, grown by the dependency grid's overlap margin, of every grid its grid's `dependsOn` names), computed as STREAM's step 4 computes them |
| 11 | worst-case live instances | 192 | STREAM V23: Σ over grids of B′(h), computed with the formulas of `Zenith2/Docs/Design/STREAM/Formats.md` → "Worst-case active cells" over the grid table of step 2 (E(h) = 0: WORLD writes no explicit dependency); every layer grid copied from a gameplay template adds its own B′, and a retired layer slot adds 0, because STREAM's `B'(h) = min(cellCount(h), …)` is 0 for a grid with no cell, whatever radii it copied from the template |

**Worked V23 bound.** The example world of `Zenith2/Docs/Design/WORLD/Documents.md` → "World document example", grown to 4,096 m × 4,096 m: collision grid c = 256 m, r = 512 + 64 = 576 m, 256 cells; `content` c = 128 m, r = 384 + 64 = 448 m (STREAM's defaults), 1,024 cells, `dependsOn` collision; the layer template c = 128 m, r = 576 + 64 = 640 m (W27's minimum), `dependsOn` collision alone (step 2: `content`, the navigation role's grid, is a gameplay grid); every `overlapMargin` 0.25. In dependency order: reach(content) = 448, reach(layer) = 640, reach(collision) = max(576, 448 + 128 + 0.25 × 256, 640 + 128 + 0.25 × 256) = max(576, 640, 832) = 832 m.

| Grid table | B′(collision) | B′(content) | B′(layer) | Σ | V23 |
|---|---|---|---|---|---|
| no grid layer | (2 · ceil(640 / 256) + 1)² = 49 | (2 · ceil(448 / 128) + 1)² = 81 | — | 130 | fits |
| one layer of 4 cells | (2 · ceil(832 / 256) + 1)² = 81 | 81 | min(4, (2 · ceil(640 / 128) + 1)²) = min(4, 121) = 4 | 166 | fits |
| one world-wide layer (1,024 cells) | 81 | 81 | min(1,024, 121) = 121 | 283 | breach, `WORLD_ERROR_LIMIT` row 11 |

A layer grid of this template fits while it has at most 192 − 81 − 81 = 30 cells; a world-wide gameplay layer needs a smaller template radius than W27 allows with this collision grid, a larger layer cell, or a collision grid of smaller reach. Had the layer grid depended on `content` (the navigation role's fallback), reach(content) would be 640 + 128 + 0.25 × 128 = 800 m and B′(content) = (2 · ceil(800 / 128) + 1)² = 225, above 192 on its own, which is why step 2 never draws that edge. `Test_WorldPartition_Limits` asserts the 166 and 283 rows.

Each is checked in step 13, before any product is declared; a breach is `WORLD_ERROR_LIMIT` (detail: the limit's row number above) naming the cell, layer, grid or world, so the author splits content, enlarges cells, shrinks radii or moves content to another grid, and `stream.world` never fails later on a world the import accepted. Each limit is tested at the limit and at limit + 1 ([Z2-N-ARCH-953]) by `Test_WorldPartition_Limits`.
