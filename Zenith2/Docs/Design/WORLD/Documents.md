# WORLD design: authoring documents

Section file of `Zenith2/Docs/Design/WORLD.md`. It specifies every authoring document WORLD owns or derives: the world document, the region documents (SCENE scene documents under WORLD's placement rules, read only as inputs of the world's importer), the layer and grid objects inside the world document, the generator document, the baked lighting documents, and the per-region navigation documents WORLD derives in AI's schema inside the import. Each document type's canonical schema, validation messages, limits, migration chain and example are here. The algorithms that read these documents are `Zenith2/Docs/Design/WORLD/Partition.md`, `Zenith2/Docs/Design/WORLD/Generation.md` and `Zenith2/Docs/Design/WORLD/Bakes.md`; the declarations are `Zenith2/Docs/Design/WORLD/PublicAPI.md`.

## Scope of this file

| Need | Heading |
|---|---|
| where each file lives, its type and who writes it | "Document set" |
| rules every WORLD document follows | "Common document rules" |
| the world document | "World document" and its subsections |
| layers inside the world document | "Layer objects" |
| runtime grids inside the world document | "Streaming object" |
| region documents | "Region documents" |
| generator documents | "Generator document" |
| baked lighting documents | "Baked lighting documents" |
| derived navigation documents | "Derived navigation documents" |
| validation messages of every type | "World validation messages" |
| limits | "Document limits" |
| registration, canonical schemas, migration chains | "Document type registration" |

## Document set

All paths are relative to `Zenith2/Games/<Name>/` (the game storage root, `Zenith2/Docs/Design/EDIT/Foundation.md` → "Storage roots and the migration registry"); `<W>` is the world's directory name, 1 to 32 characters of `[A-Za-z0-9_-]`. A game may hold any number of worlds; a world is the directory `Worlds/<W>/` and everything under it. `<LayerKey>` is a layer's **layer key** and `<GeneratorKey>` a generator's **generator key**: the first 8 characters (lowercase hexadecimal) of the 36-character form of the layer's `id`, respectively of the generator document's `id` ("Layer objects", "Generator references"), so no path contains a name an author can rename.

| File | Document type (stable name) | `$schema` | Written by | Tracked | Asset source |
|---|---|---|---|---|---|
| `Worlds/<W>/<W>.world.json` | `Zenith.World.WorldDocument` | `zenith2.world` | authors through the editor's World panel and commands | yes | yes: importer `world.world` |
| `Worlds/<W>/Regions/<LayerKey>/<x>_<z>.scene.json` | SCENE's scene document (`Zenith2/Docs/Design/SCENE/Format.md` → "Scene document schema") | `zenith2.scene` | authors through the editor (SCENE edits) | yes | no: an input document of `world.world`, no `.meta` (row WD21) |
| `Worlds/<W>/Generators/<GeneratorKey>.generator.json` | `Zenith.World.GeneratorDocument` | `zenith2.world-generator` | authors | yes | no: an input document of `world.world` (row WD3) |
| `Worlds/<W>/Generated/<GeneratorKey>/<x>_<z>.scene.json` | SCENE's scene document | `zenith2.scene` | the command `world.generate` alone | yes | no: an input document of `world.world`, no `.meta` (row WD21) |
| `Worlds/<W>/Baked/Lighting/<Grid>/<x>_<z>.lightbake.json` | `Zenith.World.LightBakeDocument` | `zenith2.world-lightbake` | the lighting bake alone | yes, always LFS (its suffix joins CONTENT's `binaryExtensions`, row CT31 (b) option 1) | no: an input document of `world.world` (row WD3) |
| `Worlds/<W>/Baked/Lighting/<Grid>/<x>_<z>.p<NNN>.lightpage.json` | `Zenith.World.LightPageDocument` | `zenith2.world-lightpage` | the lighting bake alone | yes, always LFS (its suffix joins CONTENT's `binaryExtensions`, row CT31 (b) option 1) | no: an input document of `world.world` (row WD3) |

<!-- elaboration -->
The world document is the only file whose edit can touch the whole world; every content edit lands in exactly one region document, one generator document or one generated or baked document, which is the concurrency unit of "Concurrent editing granularity" in `Zenith2/Docs/Design/WORLD/Editing.md`. Cooked runtime data (cell scenes, layer scenes, the streaming world index, HLOD meshes, lighting assets, navigation input scenes and AI's navigation products) is never a source file: it is produced by the importer `world.world` and the owners' cook steps into the cooked tree (`Zenith2/Docs/Design/WORLD/Partition.md` → "Products and keys"). Region and generated region documents are inputs of the world's importer only: they are not assets, carry no `.meta`, never cook as standalone scenes and can therefore never reach a package on their own.

<!-- assumption: pending ASSET (row WD21, extending WD3) -->
ASSET's classification ("What is an asset source" in `Zenith2/Docs/Design/ASSET/Database.md`) gains, before the row "an extension an importer claims", the row "a `*.scene.json` under `Worlds/<W>/Regions/` or `Worlds/<W>/Generated/` of a game's storage root, where `Worlds/<W>/<W>.world.json` exists: an input-only document of that world's importer `world.world`; no `.meta`; never `COOK_WARNING_UNRECOGNISED_FILE`", together with WD3's suffix rows for `*.generator.json`, `*.lightbake.json` and `*.lightpage.json`. Until ASSET lands the row, the WORLD tickets that create region documents are `draft` and blocked by ASSET's patch.
<!-- /assumption -->

## Common document rules

- LFS tracking is per file-name suffix, never per size (CONTENT's `binaryExtensions` written into `.gitattributes`, `Zenith2/Docs/Design/CONTENT/SharedCache.md` → "Storage policy check"): `.lightbake.json` and `.lightpage.json` are always LFS-tracked and never merged, because a re-bake rewrites them whole and LFS locks are WORLD's claim mechanism for them (row CT31 (b), option 1, which WORLD's integrating ticket adds to `binaryExtensions`); the world, generator and region documents are never LFS-tracked. Their sizes still count toward Z2-B-CONTENT-001 and Z2-B-CONTENT-002.
- Every WORLD document is canonical JSON written by `Zenith_WriteCanonicalJson` with its type's canonical schema (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Canonical rules"): LF line endings, one trailing LF, UTF-8 without BOM, and every collection of identified children an ID map or a keyed map, so two independent additions are two disjoint keyed children. Disjoint keyed children are not disjoint lines (a member appended after the last one adds a `,` to the line before it), so WORLD documents merge only through CONTENT's structural merge driver (`Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge API"), never by a line merge ("Merge conflicts in WORLD documents" in `Zenith2/Docs/Design/WORLD/Editing.md`).
- Every write goes through a `Zenith_DocumentMigrator`'s `SaveDocuments` or `ApplyPlan` (journaled, backed up, under the storage root's writer lock, `Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Journaled write", "Storage-root writer lock"); no WORLD code writes a tracked file any other way.
- Unknown members are preserved in position with a warning (Z2-D-048).
- Persistent IDs are the 36-character lowercase form; `uint64` values that can exceed 2^53 (seeds, salts) are decimal strings of 1 to 20 digits.
- Numbers: metres are `double` in documents (`Zenith_FormatDouble`, shortest round trip); angles in degrees; colours linear RGB in [0, 1].
- Document readers never assert on document content ([Z2-N-CONTRACT-472]); every finding is a `Zenith_WorldDiagnostic` with the document path and a JSON pointer ("World validation messages").

## World document

`Worlds/<W>/<W>.world.json`, document type `Zenith.World.WorldDocument`, schema version 1. Its `id` is the world's identity: the source GUID of the `.world.json` asset (equal to the `.meta` GUID), the streaming world's `worldId` (`Zenith2/Docs/Design/STREAM/Formats.md` → "Root members") and the scope from which every product GUID of the world is derived (`Zenith2/Docs/Design/WORLD/Partition.md` → "Product identity").

### World root members

| Member | Type | Required | Default | Rule |
|---|---|---|---|---|
| `$schema` | string | yes | — | `"zenith2.world"` |
| `schemaVersion` | integer | yes | — | 1 |
| `id` | persistent ID | yes | — | non-null; equals the `.meta` GUID (W2) |
| `name` | string | yes | — | 1 … 64 UTF-8 bytes |
| `seed` | decimal string | yes | — | `uint64`; the world seed every generator stream derives from ("Generator document") |
| `persistentScene` | persistent ID | yes | — | a scene asset of the game: the always-loaded layer that holds the `Zenith.Streaming.WorldRef`, the terrain root, global lighting and gameplay singletons (`Zenith2/Docs/Design/ENV/Bake.md` → "Cell content records") |
| `regionSize` | integer metres | no | 256 | one of 128, 256, 512, 1,024, 2,048 |
| `origin` | `[x, z]` numbers | no | `[0, 0]` | each a multiple of 128 within ±2^40 |
| `baseY`, `height` | numbers | no | −1,024, 4,096 | the vertical extent every region and cell shares; `height` in [1, 2^20] |
| `streaming` | object | yes | — | "Streaming object" |
| `layers` | keyed map name → layer object | yes | — | 1 … `uWORLD_MAX_LAYERS` (32) entries, "Layer objects" |
| `regions` | keyed map region key → region object | yes (may be `{}`) | — | "Region objects" |
| `generators` | keyed map generator key → generator reference | no | `{}` | at most `uWORLD_MAX_GENERATORS` (64), "Generator references" |
| `terrains` | ordered array of persistent IDs | no | `[]` | ENV terrain documents (`*.terrain.json`) whose tiles this world contains, at most 16, sorted ascending |
| `bakes` | object | no | every default | "Bake settings object" |

### Region objects

An authored region's key is `<LayerKey>/<x>_<z>`: the layer key, a slash, the region coordinate x, an underscore, z, as decimal integers with a leading `-` for negatives and no leading zeros. A generated region's key is `<LayerKey>/<x>_<z>/<GeneratorKey>`, with the layer key of its generator's layer (`Zenith_WorldRegionKey`, `Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Identity and keys"). Keys sort by byte order. A layer key or generator key never changes while its layer or generator exists, so renaming a layer or a generator changes no region key, no region path and no product key.

| Member | Type | Required | Rule |
|---|---|---|---|
| `scene` | persistent ID | yes | the region document's `id` (a persistent ID that names no asset: the document is input-only); the document at `Regions/<LayerKey>/<x>_<z>.scene.json`, or, for a generated region, `Generated/<GeneratorKey>/<x>_<z>.scene.json` |
| `generator` | persistent ID or `null` | no (default `null`) | the generator document's `id` of a generated region (its first 8 characters are the key's third part, W16); `null` for an authored region |

A region coordinate (x, z) covers `[origin + (x, z) × regionSize, origin + (x + 1, z + 1) × regionSize)` in world XZ, half open, at the world's vertical extent. |x|, |z| ≤ 2^20. One layer has at most one authored region per coordinate and one generated region per (generator, coordinate); the key grammar makes a second one impossible.

### Generator references

The map's key is the **generator key**: the first 8 characters of the generator document's `id`.

| Member | Type | Required | Rule |
|---|---|---|---|
| `document` | persistent ID | yes | the generator document's `id`; its first 8 characters equal the map key, so the key is unique among the world's generators (W16, W30) |
| `path` | string | yes | its storage path, exactly `Generators/<GeneratorKey>.generator.json` (W30) |

<!-- elaboration -->
Generator documents are input documents, not assets, so the world document names both the ID (the identity every generated entity derives from) and the path (how the importer opens it, row WD3). Nothing in the world document, in a path, in a region key, in a product key or in a fingerprint holds a generator's display `name` (it lives in the generator document alone), and every reference to a layer or a generator from a generator document or a region object holds that layer's or generator's `id` ("Generator document"), so renaming a generator edits one member of one file and renaming a layer one keyed child of one file (`world.generator.rename`, `world.layer.rename` in `Zenith2/Docs/Design/WORLD/Editing.md` → "World commands").

### Streaming object

| Member | Type | Required | Default | Rule |
|---|---|---|---|---|
| `mode` | `"streamed"`, `"unstreamed"` | no | `"streamed"` | `"unstreamed"`: the importer writes one content scene and no streaming world (`Zenith2/Docs/Design/WORLD/Partition.md` → "Unstreamed worlds") |
| `grids` | ordered array of grid objects | yes when streamed | — | 1 … 8 entries in grid index order, each exactly STREAM's grid object (`Zenith2/Docs/Design/STREAM/Formats.md` → "Grid object") plus the WORLD member `defaultFor` |
| `splitStaticColliders` | boolean | no | true | "Static collider split" of `Zenith2/Docs/Design/WORLD/Partition.md` |
| `largeExtentPromotion` | boolean | no | true | entities too large for their cell move to the first `large` grid ("Grid assignment" of `Zenith2/Docs/Design/WORLD/Partition.md`) |
| `layerGridTemplate` | grid object without `name` | no | the first GAMEPLAY grid's members with `hlod` `{ "levels": 0 }` | the template every grid-realised layer's grid copies; its `hlod.levels` is 0 (W38): a layer's content is never in an HLOD proxy ("Lighting and HLOD" of `Zenith2/Docs/Design/WORLD/Partition.md` → "Layer realisation"), so a layer grid with HLOD levels would make every layer cell a counted HLOD gap |
| `layerGridBase` | integer | no | the number of `grids` | the grid index of the first layer grid; equal to the number of base grids (W28) |
| `layerGridOrder` | ordered array of persistent IDs | no | `[]` | the layer `id`s of grid-realised layers in order of first appearance; a layer grid's index is `layerGridBase` + its position here. The array only grows: adding a grid-realised layer appends its `id`; deleting or re-realising one leaves its entry, which is then **retired** (it names no current grid-realised layer) and still occupies its index as an empty grid; at most 8 − `layerGridBase` entries (W10, W28) |

`defaultFor` (WORLD's member of a grid object, removed before the grid reaches the streaming world document): an ordered array of role names among `"gameplay"`, `"collision"`, `"navigation"`, `"audio"`, `"large"`; the grid that lists a role receives content assigned to that role by "Grid assignment". Each role is listed by at most one grid; a role no grid lists falls back to the first grid of the role's STREAM role, else the first `gameplay` grid. Grid names beginning `layer.` are reserved for layer grids (W12).

<!-- elaboration -->
The world document carries STREAM's grid objects verbatim instead of a WORLD-specific shape, so the radii, HLOD distances and dependency rules an author tunes are exactly the runtime ones; STREAM's validation (V3 to V7) runs again in the `stream.world` step on the derived document, and WORLD's validation W12 only adds the rules that come from authoring (grid alignment with regions, reserved names, role coverage). `layerGridOrder` exists because a source's saved `GridMask` names grids by index (`Zenith.Streaming.Source` property 3, bit g = grid g): an index assigned to a layer grid never changes for that layer, whatever layers are added, renamed, deleted or re-ordered by name, and a base grid added or removed is refused (W28) until the author runs `world.streaming.renumberLayerGrids` ("World commands" in `Zenith2/Docs/Design/WORLD/Editing.md`), whose confirmation states the save consequence: a source whose `GridMask` a game wrote keeps its bits, which then name other grids.

### Bake settings object

| Member | Type | Default | Rule |
|---|---|---|---|
| `lighting` | object | defaults below | "Lighting settings" |
| `hlod` | object | defaults below | "HLOD settings" |
| `navigation` | object | defaults below | "Navigation settings" |

#### Lighting settings

| Member | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `enabled` | boolean | true when streamed, false when unstreamed | true only when streamed (W29) | false: no lighting bake unit exists and cells carry no baked lighting |
| `grid` | grid name | the first GAMEPLAY grid | an existing base grid whose STREAM `role` is `gameplay` (W18) | the grid whose cells are lighting bake units; STREAM's role table puts "the cell's lightmap set" in gameplay cells (`Zenith2/Docs/Design/STREAM/Formats.md` → "Content of each grid role") |
| `texelsPerMetre` | number | 4 | 0.25 … 64 | lightmap texel density |
| `pageSize` | integer | 1,024 | 256, 512, 1,024, 2,048, 4,096 | lightmap page edge |
| `paddingTexels` | integer | 2 | 1 … 8 | gutter around each instance rectangle |
| `minRectTexels` | integer | 4 | 2 … 64 | smallest instance rectangle edge |
| `samplesPerTexel` | integer | 256 | 16 … 4,096 | hemisphere rays per lightmap texel |
| `bounces` | integer | 2 | 1 … 8 | path depth of indirect light |
| `influenceRadius` | number metres | 64 | 0 … 1,024 | occluders and bouncers gathered beyond the cell rectangle |
| `volumeSpacing` | number metres | 4 | 0.25 … 16 | irradiance volume probe spacing |
| `volumeSamples` | integer | 256 | 64 … 4,096 | sphere rays per volume probe |
| `probeSamplesPerTexel` | integer | 16 | 1 … 256 | rays per probe face texel; the face edge is `uWORLD_PROBE_FACE_SIZE` (128, LIGHT's desktop size; LIGHT's step derives the mobile size, row WD25) |
| `sky` | object | `{ "upper": [0.6, 0.7, 1.0], "lower": [0.15, 0.13, 0.1], "radianceNits": 5000 }` | colours in [0, 1], nits in [0, 10^6] | the two-hemisphere sky radiance escaping rays receive ("Escaped rays" in `Zenith2/Docs/Design/WORLD/Bakes.md`) |
| `sun` | object or `null` | `null` | direction unit, illuminance 0 … 200,000 lux | `{ "direction": [x, y, z], "illuminanceLux": n, "colour": [r, g, b] }`: a directional light added to the bake; refused while a baked lighting layer or the persistent scene holds a `Zenith.Lighting.DirectionalLight` (W37) |
| `defaultAlbedo` | `[r, g, b]` | `[0.5, 0.5, 0.5]` | each [0, 0.95] | albedo of a surface whose material has no entry below |
| `materialAlbedo` | ID map material GUID → `[r, g, b]` | `{}` | each [0, 0.95] | per-material bake albedo |
| `terrainAlbedo` | `[r, g, b]` | `[0.3, 0.3, 0.25]` | each [0, 0.95] | albedo of terrain triangles in the bake |

Which layers the bake reads is decided by each layer object's `bakes.lighting` alone; the settings object holds no list of layers, so adding, removing or changing a layer edits no member of `bakes.lighting` and changes no lighting fingerprint of a unit whose inputs it does not touch (`Zenith2/Docs/Design/WORLD/Bakes.md` → "Lighting fingerprint").

#### HLOD settings

| Member | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `enabled` | boolean | true | — | false: no HLOD node is declared, whatever the grids' `hlod.levels` |
| `trianglesPerNode` | array of integers, one per level | `[20000, 10000, 5000]` | each 64 … 262,144, non-increasing | the simplification target of one node's meshes together |
| `maxError` | array of numbers, one per level | `[0.02, 0.05, 0.1]` | each (0, 1] | relative simplification error bound passed with the recipe |
| `materialMode` | array of `"keep"`, `"flatten"`, one per level | `["keep", "flatten", "flatten"]` | — | "Material modes" of `Zenith2/Docs/Design/WORLD/Bakes.md` |
| `minInstanceRadius` | array of numbers metres, one per level | `[0.5, 2, 8]` | ≥ 0 | instances whose bounding radius is smaller are left out of the level |

#### Navigation settings

| Member | Type | Default | Rule |
|---|---|---|---|
| `enabled` | boolean | false | true: the importer derives one navigation document per navigation region ("Derived navigation documents") |
| `settings` | persistent ID | — (required when enabled) | AI's navigation settings document (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation settings document") |
| `classes` | array of class names | `[]` (every class) | copied into each derived document |
| `blockerLayers`, `ignoredLayers` | arrays of physics layer names | `[]` | copied |
| `areaByMaterial` | object | `{}` | copied |
| `carveRegions`, `waterVolumes` | arrays | `[]` | each element copied into every derived document whose bounds it intersects |
| `volumes` | array | `[]` | each element copied into exactly one derived document, the one whose region holds the element's minimum corner, with its index in this array as AI's volume index (row WD23) |

## Layer objects

The key is the layer name: 1 to 24 characters of `[A-Za-z0-9_-]`, shown in the editor and used in diagnostics only. Layers are kept in a keyed map, so their order is name order; the layer **index** used in sort keys and bit masks of one import is a layer's position in that order (it is never persisted). Every persisted identity of a layer derives from its `id`: the layer key (its first 8 characters) names its region directory, its region keys and its layer grid, and the full `id` its layer scene product; every reference to a layer from another WORLD document (a generator document's `layer`, `world.scatter`'s `exclusionsFromLayers`, `streaming.layerGridOrder`) holds its `id`; so a rename edits exactly the layer's key in the `layers` map and no other member of any document.

| Member | Type | Required | Default | Rule |
|---|---|---|---|---|
| `id` | persistent ID | yes | — | stable identity of the layer: renaming a layer keeps it, and with it every region key, region path, grid name, product key and saved grid index; unique among the world's layers, and so is its layer key (W7) |
| `kind` | `"data"`, `"gameplay"` | yes | — | "Layer kinds and states" |
| `state` | `"editorOnly"`, `"baked"`, `"runtime"` | yes | — | "Layer kinds and states" |
| `grid` | grid name or `null` | no | `null` | BAKED: the base grid its content goes to before role rules (null: "Grid assignment" decides per hierarchy); RUNTIME: ignored |
| `realisation` | `"grid"`, `"additive"` | when RUNTIME | — | RUNTIME only; `"additive"` only in an unstreamed world (W25) |
| `initiallyActive` | boolean | no | false | RUNTIME `grid` only: written as the layer grid's `defaultActive` in the streaming world document, which STREAM folds into every source's effective mask, so a source whose override masks gameplay never wrote leaves the layer off (`Zenith2/Docs/Design/WORLD/Partition.md` → "Layer realisation"; rows WD19, WD32); true on an additive layer is W25 |
| `bakes` | object `{ "lighting": bool, "navigation": bool, "hlod": bool }` | no | BAKED: all true; RUNTIME and EDITOR_ONLY: all false | which bakes read the layer's content; a true member on a RUNTIME or EDITOR_ONLY layer is W19 |
| `editor` | object `{ "colour": [r, g, b], "visible": bool, "loaded": bool, "locked": bool }` | no | `{ "colour": [0.5, 0.5, 0.5], "visible": true, "loaded": true, "locked": false }` | the team-shared defaults of the editor's layer view; each user's overrides live in the untracked session file (`Zenith2/Docs/Design/EDIT/Pipeline.md` → "Per-user state stays out of documents") |

### Layer kinds and states

| Kind | Allowed states | Meaning |
|---|---|---|
| `data` (a data layer) | `editorOnly`, `baked` | groups content by purpose (terrain dressing, buildings, audio, lighting rigs, reference geometry) for authoring: visibility, loading, locking, bake participation, the default grid. A `baked` data layer is merged into the runtime cells; an `editorOnly` one never reaches a cook output |
| `gameplay` (a gameplay layer) | `runtime` | a set of content whose presence in the running world is a gameplay decision (a destroyed village, a quest stage's camp, seasonal dressing): cooked apart from the baked content and switched at run time by simulation through STREAM or SCENE, never by WORLD code |

A `gameplay` layer with another state, or a `data` layer with `runtime`, is W8. Combinations and their cook output:

| Kind, state, realisation | Region content goes to | Run-time switch |
|---|---|---|
| data, editorOnly | nothing | none |
| data, baked | the cells of the base grids | none: always streamed with its cells |
| gameplay, runtime, grid (streamed worlds) | the cells of the layer's own grid `layer.<LayerKey>`, its static colliders included | the layer grid's bit in a source's `GridMask` (`Zenith.Streaming.Source` property 3), its initial value the grid's `defaultActive` (row WD19); written by gameplay scripts or GAME's rules |
| gameplay, runtime, additive (unstreamed worlds) | one generated layer scene | SCENE's additive load and unload of that scene (`Scene.LoadAdditive`, `Zenith2/Docs/Design/SCENE/Loading.md` → "Script requests") |

## Region documents

A region document is an ordinary SCENE scene document (`$schema` `zenith2.scene`), edited with SCENE's edits through EDIT's SCENE document kind and read by the importer `world.world` as an input document; it is not an asset, has no `.meta` and is never cooked on its own. WORLD adds placement rules that SCENE does not check:

| # | Rule | Checked by |
|---|---|---|
| R1 | the file is `Regions/<LayerKey>/<x>_<z>.scene.json` for the region object `<LayerKey>/<x>_<z>` of its world document (a generated one `Generated/<GeneratorKey>/<x>_<z>.scene.json` for `<LayerKey>/<x>_<z>/<GeneratorKey>`), and that object's `scene` equals the document's `id` | the importer (W20), the editor on open |
| R2 | the transforms of root entities and root instances are relative to the region anchor: the point `(origin.x + x × regionSize, baseY, origin.z + z × regionSize)` | by construction: the editor's projection adds the anchor ("Region projection" in `Zenith2/Docs/Design/WORLD/Editing.md`) |
| R3 | every root's position lies in `[0, regionSize)` on X and Z and in `[0, height)` on Y, relative to the anchor; a root of a generated region document may lie outside by at most its generator document's `jitter` parameter on X and Z (the displacement of an output from the point that owns it, `Zenith2/Docs/Design/WORLD/Generation.md` → "Running a generator") | the importer (warning W21), the editor's move commands (they re-home a root that leaves, "Moving content between regions") |
| R4 | no root entity or root instance carries a `Zenith.Streaming.CellAnchor`, `Zenith.Streaming.WorldRef`, `Zenith.Streaming.CellDelta` or `Zenith.Streaming.CellStatus` component | the importer (W22) |
| R5 | an entity ID is unique across every region document of the world (and the persistent scene) | the importer (W23) |
| R6 | a generated region document is written only by `world.generate`; a hand edit makes it EDITED ("Generator outputs and staleness" in `Zenith2/Docs/Design/WORLD/Generation.md`) | the importer (warning W24), the editor |

<!-- elaboration -->
A region document is a scene, not a WORLD-specific type, for two reasons: the whole SCENE and EDIT machinery (prefab instances, overrides, sort keys, reference repair, undo, the semantic merge) applies unchanged, and the authoring unit stays independent of the runtime cell size, which is the separation Z2-I-WORLD-09 asks for. It is an input-only document rather than a scene asset because nothing needs it cooked alone: its roots are anchor-relative (R2) and the document carries no anchor, so a standalone cook of it would place its content near the world origin, and every runtime form of its content (cells, layer scenes, the unstreamed content scene, navigation input scenes) is produced by the partition. Standalone cooks would also cost a second resolution of every region and one SCENE cook warning per cross-region reference, and would need a packaging exclusion; input-only documents need none of that.

<!-- assumption: pending SCENE (row WD22) -->
SCENE's `Zenith_SceneDocument::Load` and `Zenith_ResolveDocument` accept a scene document whose `id` names no asset (an input-only scene): they treat the `id` as the document's identity and never resolve it through the asset registry, so `Zenith2/Docs/Design/SCENE/Format.md`'s rule "The document's `id` is its asset GUID" holds for scene assets and the region documents of WORLD are its stated exception.
<!-- /assumption -->

### Region document example

```json
{
	"$schema": "zenith2.scene",
	"schemaVersion": 1,
	"id": "2f6c1a90-7d3e-4b51-8a2c-0e9d4f7b3c11",
	"name": "9e1b4c27/0_-1",
	"entities": {
		"8c41e2a7-5b09-4d3f-9e16-7a2b0c5d8e44": {
			"name": "Well",
			"parent": null,
			"sortKey": "V",
			"components": {
				"Zenith.LocalTransform": {
					"version": 1,
					"fields": {
						"1": [118.5, 12.25, 201.75],
						"2": [0, 0, 0, 1],
						"3": [1, 1, 1]
					}
				},
				"Zenith.Streaming.Persistent": {
					"version": 1,
					"fields": {
						"1": 2
					}
				}
			}
		}
	},
	"instances": {}
}
```

The file is `Regions/9e1b4c27/0_-1.scene.json`, the region `9e1b4c27/0_-1` of layer `Buildings` (whose `id` begins `9e1b4c27`) in the world example below; the document's `name` is the region key. The well stands at world `(origin.x + 0 × 256 + 118.5, baseY + 12.25, origin.z − 256 + 201.75)` for `regionSize` 256. Its `Zenith.Streaming.Persistent` component carries only the mode (property 1); the partition writes properties 2 and 3 (`Zenith2/Docs/Design/WORLD/Partition.md` → "Persistent markers").

## Generator document

`Worlds/<W>/Generators/<GeneratorKey>.generator.json`, type `Zenith.World.GeneratorDocument`, schema version 1. The generator algorithms are `Zenith2/Docs/Design/WORLD/Generation.md`.

| Member | Type | Required | Default | Rule |
|---|---|---|---|---|
| `$schema` | string | yes | — | `"zenith2.world-generator"` |
| `schemaVersion` | integer | yes | — | 1 |
| `id` | persistent ID | yes | — | equals the world document's reference to it |
| `name` | string | yes | — | 1 … 64 UTF-8 bytes, display only: shown in the editor and in diagnostics, never part of a path, key, reference or fingerprint; renamed by `world.generator.rename` |
| `generator` | string | yes | — | a registered generator ID (`world.lattice`, `world.spline`, `world.scatter`, `world.probeGrid`) |
| `generatorVersion` | integer | yes | — | a version the registry implements for that ID (W31) |
| `mode` | `"cook"`, `"baked"` | no | `"cook"` | "Execution modes" of `Zenith2/Docs/Design/WORLD/Generation.md` |
| `layer` | persistent ID | yes | — | the `id` of an existing layer of the world whose state is not `editorOnly` (W17) |
| `scope` | object | no | `{ "regions": "all" }` | `{ "regions": "all" }`, or `{ "regions": [<region coordinates "<x>_<z>">] }`, or `{ "bounds": { "min": [x, z], "max": [x, z] } }`; the regions whose rectangle intersects the scope are generated |
| `seedSalt` | decimal string | no | `"0"` | mixed into every stream of this generator ("Seed derivation" of `Zenith2/Docs/Design/WORLD/Generation.md`) |
| `dependsOn` | ordered array of generator document `id`s, ascending | no | `[]` | generators whose outputs this one reads as obstacles; each the `document` of a reference of the world, acyclic (W33); a `baked` generator names only `baked` generators (W36) |
| `parameters` | object | yes | — | the generator's parameter object, validated by the generator ("Built-in generators") |
| `outputs` | keyed map region coordinate `<x>_<z>` → output record | no | `{}` | baked mode only, written by `world.generate`: `{ "scene": GUID or null, "fingerprint": <64 hex>, "digest": <64 hex> or null, "sceneSchemaVersion": n, "entities": n }`; `scene` and `digest` are null exactly when the region job produced no output (`entities` 0), which writes no generated document (`Zenith2/Docs/Design/WORLD/Generation.md` → "Generator outputs and staleness") |

`outputs` records the fingerprint of the inputs the output was generated from, the SHA-256 of the output document's bytes as written and the SCENE schema version those bytes were written at, so the next run tells a stale output (fingerprint differs) from a hand-edited one (digest differs at the same schema version) ("Generator outputs and staleness" in `Zenith2/Docs/Design/WORLD/Generation.md`).

### Generator document example

```json
{
	"$schema": "zenith2.world-generator",
	"schemaVersion": 1,
	"id": "51c0d8e2-3a7f-4b96-8d15-2e0a9c7f4b63",
	"name": "FencePosts",
	"generator": "world.spline",
	"generatorVersion": 1,
	"mode": "cook",
	"layer": "5d8a3f61-2c4e-4a97-b0d3-8e1f6c2a9b45",
	"scope": {
		"regions": "all"
	},
	"seedSalt": "17",
	"dependsOn": [],
	"parameters": {
		"prefab": "0d9e4c21-6b8a-4f37-a1e5-9c2d7b0f6a18",
		"points": [
			[10, 0, 10],
			[90, 0, 10],
			[90, 0, 70]
		],
		"spacing": 2.5,
		"lateralOffset": 0,
		"jitter": 0,
		"yawJitterDegrees": 0,
		"alignToPath": true,
		"obstacleRadius": 0.5
	},
	"outputs": {}
}
```

The file is `Generators/51c0d8e2.generator.json` (its generator key is the first 8 characters of its `id`), referenced from the world example below by the key `51c0d8e2`; its `layer` is the `id` of that world's layer `Dressing`.

## Baked lighting documents

One lighting bake unit (a cell of the lighting grid, `Zenith2/Docs/Design/WORLD/Bakes.md` → "Bake units") writes one header document and one page document per lightmap page. Both are written only by the lighting bake, in one journaled batch per unit, so a reader never sees a header without its pages.

### Light bake header

`Baked/Lighting/<Grid>/<x>_<z>.lightbake.json`, type `Zenith.World.LightBakeDocument`, schema version 1.

| Member | Type | Rule |
|---|---|---|
| `$schema` | string | `"zenith2.world-lightbake"` |
| `schemaVersion` | integer | 1 |
| `id` | persistent ID | `Zenith_WorldDeriveProductGUID(worldId, "lightbake:<Grid>/<x>_<z>")` |
| `world` | persistent ID | the world document's `id` |
| `grid`, `x`, `z` | string, integers | the unit's cell |
| `fingerprint` | 64 lowercase hex | the unit's input fingerprint when the bake ran ("Lighting fingerprint" in `Zenith2/Docs/Design/WORLD/Bakes.md`) |
| `tracerVersion` | integer | `uWORLD_LIGHT_TRACER_VERSION` of the tool that baked |
| `pageCount` | integer | 0 … 256 |
| `pageSize` | integer | the settings' `pageSize` |
| `rgbmRange` | number | 16 (the LIGHT format's default range, `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap set cooked format") |
| `bindings` | ID map instance entity ID → `{ "page": n, "scale": [u, v], "bias": [u, v] }` | one per receiver instance, `Zenith2/Docs/Design/WORLD/Bakes.md` → "Atlas packing" |
| `volume` | object or `null` | `{ "dims": [nx, ny, nz], "minCorner": [x, y, z], "spacing": [sx, sy, sz], "sh": <payload f32, 12 per probe>, "validity": <payload u8, 1 per probe> }`; `minCorner` relative to the cell anchor |
| `probes` | ID map probe entity ID → `{ "captureOffset": [x, y, z], "captureEV100": n, "interior": bool, "radiance": <payload f16x3, 6 × 128² texels> }` | baked reflection probes of the cell; `captureOffset` relative to the probe entity's position (zero for a probe captured at its entity); faces of `uWORLD_PROBE_FACE_SIZE` (128) texels, radiance in absolute nits |

### Light page document

`Baked/Lighting/<Grid>/<x>_<z>.p<NNN>.lightpage.json` (`NNN` the page index, three decimal digits), type `Zenith.World.LightPageDocument`, schema version 1: `$schema` `"zenith2.world-lightpage"`, `schemaVersion` 1, `id` = `Zenith_WorldDeriveProductGUID(worldId, "lightpage:<Grid>/<x>_<z>/<NNN>")`, `bake` (the header's `id`), `page` (index), `size` (edge), `irradiance` (payload f16x3: lux per texel), `direction` (payload u8x4: octahedral direction in bytes 0–1, directionality in byte 2, byte 3 zero), `coverage` (payload u8: 1 inside a chart, 0 outside).

### Payload objects

A payload is `{ "format": <name>, "count": <elements>, "codec": "zstd", "data": <base64> }`: `count` elements of `format` written little-endian, compressed as one zstd frame at level 9 through `Zenith_Compression` (deterministic per codec version, Z2-D-004), then base64 (RFC 4648 §4, padded, one line).

| Format | Element bytes | Element |
|---|---|---|
| `f32` | 4 | IEEE binary32 |
| `f16x3` | 6 | three IEEE binary16, round to nearest even, R then G then B |
| `u8` | 1 | unsigned byte |
| `u8x4` | 4 | four unsigned bytes |

A reader decodes and checks `count × element bytes` equals the decompressed size (W42), the decompressed size ≤ 268,435,456 bytes, and every float finite (W43). The canonical schema writes a payload object's members in the order `format`, `count`, `codec`, `data`.

<!-- informative -->
Base64 inside canonical JSON follows ENV's terrain tile payloads (`Zenith2/Docs/Design/ENV/Bake.md` → "Terrain tile document schema"): bake outputs stay documents, so they ride the document framework's journaled write, backups, migrations and staleness checks, instead of a second binary write path. They are regenerated rather than merged: a conflict in a baked document is resolved by taking either side and re-baking, which the editor offers ("Merge conflicts in WORLD documents" in `Zenith2/Docs/Design/WORLD/Editing.md`).
<!-- /informative -->

## Derived navigation documents

With `bakes.navigation.enabled`, the importer derives, in memory, one AI navigation document per **navigation region**. A region coordinate (x, z) is a navigation region when either holds:

1. it **owns** at least one hierarchy of a BAKED layer with `bakes.navigation` true: an authored hierarchy is owned by the coordinate of its region document; a cook-mode or baked generator output by the coordinate whose rectangle holds its owner point (`Zenith2/Docs/Design/WORLD/Generation.md` → "Generator terms"), wherever its root stands (membership is never by root position, so a root outside its own region, which W21 allows with a warning, is never lost);
2. its rectangle intersects a 128 m tile of a terrain of the world's `terrains` whose terrain document has `navigation.enabled` true (`Zenith2/Docs/Design/ENV/Bake.md` → "Terrain document schema"): open ground with no authored content is navigable.

A coordinate of the first kind has a navigation input scene (`navInput:<x>_<z>`, `Zenith2/Docs/Design/WORLD/Partition.md` → "Navigation input scenes"); a coordinate of the second kind only (a **terrain-only** navigation region) has none, and its document lists the input scenes of its neighbours and the terrains. The document is never a file: it exists only inside the import, where AI's declaration function turns it into AI's products ("Navigation workflow" in `Zenith2/Docs/Design/WORLD/Bakes.md`). Its members, in AI's schema (`Zenith2/Docs/Design/AI/Formats.md` → "Navigation document schema") with the scene and terrain entries of rows WD23 and WD34, are a pure function of the world document, the terrain documents and the partition:

| AI member | Value WORLD derives |
|---|---|
| `schemaVersion` | 1 |
| `settings` | the GUID of `bakes.navigation.settings`'s document |
| `classes` | `bakes.navigation.classes` when non-empty, else absent |
| `bounds` | the region rectangle and the world's vertical extent: `min = [ox + x·S, baseY, oz + z·S]`, `max = [ox + (x+1)·S, baseY + height, oz + (z+1)·S]`, absolute metres in `double` |
| `scenes` | one entry `{ "scene": <GUID of product navInput:<x'>_<z'>>, "offset": [ox + x'·S, baseY, oz + z'·S] }` per coordinate (x', z') within Chebyshev distance 1 of (x, z) (the 3 × 3 neighbourhood) that has a navigation input scene, in (z', x') order, possibly none (AI's `scenes` "may be empty"); each navigation input scene is anchor-relative to its own region anchor, so `offset` plus a local position, added in `double`, is the world-absolute position (`Zenith2/Docs/Design/WORLD/Partition.md` → "Navigation input scenes") |
| `terrains` | one product-form entry `{ "terrain": <GUID> }` per terrain of the world's `terrains` whose `navigation.enabled` is true and whose extent intersects the bounds, ascending GUID (row WD34: AI's `terrains` accepts a GUID entry beside a path, since a derived document has no directory a path could be relative to) |
| `blockerLayers`, `ignoredLayers`, `areaByMaterial` | copied from `bakes.navigation` |
| `carveRegions`, `waterVolumes` | the elements of `bakes.navigation` intersecting the bounds |
| `volumes` | the elements of `bakes.navigation.volumes` whose minimum corner lies in the region rectangle, each with its array index as `index` |

A hierarchy whose colliders reach beyond the 3 × 3 neighbourhood of its owning coordinate is missed by the tiles of the farther documents, which list no scene of it; W47 warns when a navigation-baking hierarchy's collider bounds extend more than `regionSize` beyond its owning region's rectangle. Because each document's bounds are its region and regions do not overlap, two derived documents never declare the same tile key (AI's `AI_ERROR_TILE_DUPLICATE` cannot arise); this needs the region size and the world origin to be multiples of AI's tile size, which W13 checks against the settings document's tile sizes. A volume element is placed in exactly one document, so its product key is unique too. The derivation is `Zenith_DeriveWorldNavigationDocument` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Navigation document functions"). No derived navigation document is a source file, so none is tracked, merged, given a `.meta` or checked for staleness: a change of any input re-derives it in the next import, exactly as cells are re-derived.

## World validation messages

`Zenith_ReadWorldDocument` and the importer report findings in this order and every finding of a pass before failing (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "World document functions"); `ERROR` findings fail the import (`COOK_ERROR_IMPORT_FAILED`, `IMPORT_ERROR_SOURCE_INVALID`) and the editor's validation, `WARNING` findings do neither. Each text begins with the `Zenith_WorldErrorCode` name (the ENV-F7 shape, `Zenith2/Docs/Design/ENV/Bake.md` → "Cook determinism"). "Every finding" is bounded by the collector: beyond 4,096 records a `Zenith_WorldDiagnostics` keeps the first 4,096 in report order and counts the rest (`GetDroppedCount`), and the importer then reports one more ERROR or WARNING (the highest level dropped) `<code>: <n> further findings not shown`, so an import never fails or passes silently past the cap.

Each rule is checked by exactly one of two passes, because a rule can be checked only where the documents it reads are open:

| Pass | Rules | Reads |
|---|---|---|
| the **reader** (`Zenith_ReadWorldDocument`, `Zenith_ValidateWorldDocument`) | W1 to W4, W6 to W12, W15, W16, W18, W19, W25, W26, W28, W29 | the world document alone |
| the **importer** pass (`Zenith_PlanWorldPartition`, after the reader; the editor's plan operation runs the same function) | W5, W13, W14, W17, W20 to W24, W27, W30 to W47 | the persistent scene and the asset registry (W5), AI's navigation settings document (W13, W46), the terrain documents (W14), the generator documents (W17, W30 to W33, W36, W39), the region content (W20 to W24, W27, W47), the bake documents (W34, W35, W40 to W45) |

| # | Level | Condition | Code | Text |
|---|---|---|---|---|
| W1 | ERROR | not JSON, wrong `$schema`, schema version newer than the tool | `WORLD_ERROR_DOCUMENT_SCHEMA` | `<code>: '<path>' is not a world document (<reason>)` |
| W2 | ERROR | `id` null, or not the `.meta` GUID | `WORLD_ERROR_DOCUMENT_SCHEMA` | `<code>: <pointer>: id <id> differs from the asset GUID <guid>` |
| W3 | ERROR | a required member missing or of the wrong type; a number out of its range | `WORLD_ERROR_DOCUMENT_SCHEMA` | `<code>: <pointer>: expected <rule>` |
| W4 | ERROR | `regionSize` not one of the five values; `origin` not a multiple of 128 | `WORLD_ERROR_INVALID_DESC` | `<code>: regionSize <n> / origin <x, z> violates <rule>` |
| W5 | ERROR | `persistentScene` not a scene asset of the game (the importer opens it with `OpenDocumentByGUID`; a missing asset or another document type is this finding) | `WORLD_ERROR_REFERENCE` | `<code>: persistentScene <id> is not a scene` |
| W6 | ERROR | a layer name outside the grammar or more than 32 layers | `WORLD_ERROR_LAYER_INVALID` | `<code>: layer '<name>': <reason>` |
| W7 | ERROR | two layers with one `id`, or with one layer key (the first 8 characters of their `id`s) | `WORLD_ERROR_LAYER_INVALID` | `<code>: layers '<a>' and '<b>' share <id / layer key> <value>` |
| W8 | ERROR | an invalid (kind, state, realisation) combination | `WORLD_ERROR_LAYER_INVALID` | `<code>: layer '<name>': <kind> cannot be <state>` |
| W9 | ERROR | a BAKED layer's `grid` names no base grid | `WORLD_ERROR_GRID_UNKNOWN` | `<code>: layer '<name>': grid '<grid>' does not exist` |
| W10 | ERROR | base grids plus `layerGridOrder` entries (retired ones included) exceed 8 | `WORLD_ERROR_GRID_LIMIT` | `<code>: <n> grids needed (<base> base + <layers> layer grids) exceed 8` |
| W11 | ERROR | streamed mode with no grid, or a grid object STREAM's rules refuse (V3 to V7 of `Zenith2/Docs/Design/STREAM/Formats.md` → "Validation messages", run through `Zenith_ValidateStreamingGridObject`) | `WORLD_ERROR_GRID_INVALID` | `<code>: grid '<name>': <STREAM's message>` |
| W12 | ERROR | a base grid name starting `layer.`; a role listed by two grids' `defaultFor` | `WORLD_ERROR_GRID_INVALID` | `<code>: grid '<name>': <reason>` |
| W13 | ERROR | navigation enabled and `regionSize` or `origin` not a multiple of every tile size the settings document's classes use (read from the opened settings document) | `WORLD_ERROR_GRID_ALIGNMENT` | `<code>: regionSize <n> is not a multiple of the navigation tile size <t>` |
| W14 | ERROR | a terrain listed and either a grid holding terrain content (the GAMEPLAY grids, the COLLISION and NAVIGATION grids) whose cell size or origin is not a multiple of 128 m from the terrain origin, or a world `origin` not a multiple of 128 m from it (so every 128 m terrain tile lies in exactly one region, `Zenith2/Docs/Design/WORLD/Editing.md` → "Terrain editing granularity") | `WORLD_ERROR_GRID_ALIGNMENT` | `<code>: <grid '<name>' / origin> is not aligned to terrain <id> (ENV's 128 m rule)` |
| W15 | ERROR | a region key outside the grammar, its layer key unknown, a coordinate beyond ±2^20 | `WORLD_ERROR_REGION_INVALID` | `<code>: region '<key>': <reason>` |
| W16 | ERROR | two region objects naming one `scene`; a generated region whose `generator` is not the `document` of the `generators` entry whose key is the region key's third part; a `generators` key that is not the first 8 characters of its `document` | `WORLD_ERROR_REGION_INVALID` | `<code>: region '<key>': <reason>` |
| W17 | ERROR | a generator document's `layer` names no layer `id` of the world, or a layer of state `editorOnly` | `WORLD_ERROR_GENERATOR_INVALID` | `<code>: generator '<name>' (<generator key>): layer <id> <is unknown / is editor-only>` |
| W18 | ERROR | `bakes.lighting.grid` not a base grid or a base grid whose STREAM `role` is not `gameplay`; an HLOD array whose length differs from the level count; navigation enabled with no `settings` | `WORLD_ERROR_INVALID_DESC` | `<code>: <pointer>: <reason>` |
| W19 | ERROR | a `bakes` member true on a RUNTIME or EDITOR_ONLY layer | `WORLD_ERROR_LAYER_INVALID` | `<code>: layer '<name>': a <state> layer is never baked (<member>)` |
| W20 | ERROR | a region document missing, of another type, at another path, or whose `id` differs from its region object (R1) | `WORLD_ERROR_REGION_MISSING` | `<code>: region '<key>': <reason>` |
| W21 | WARNING | a root outside its region beyond R3's allowance | `WORLD_WARNING_ENTITY_OUTSIDE_REGION` | `<code>: region '<key>': <entity> at (<x>, <y>, <z>) lies outside the region by <d> m` |
| W22 | ERROR | a forbidden STREAM component in a region (R4) | `WORLD_ERROR_REGION_INVALID` | `<code>: region '<key>': entity <id> carries <component>, which the partition writes` |
| W23 | ERROR | an entity ID in two region documents, or in a region and the persistent scene (R5) | `WORLD_ERROR_DUPLICATE_ENTITY` | `<code>: entity <id> is in '<a>' and '<b>'` |
| W24 | WARNING | a generated region whose digest differs from its generator's `outputs` record at the recorded schema version (R6) | `WORLD_WARNING_GENERATED_EDITED` | `<code>: '<path>' was edited by hand; world.generate will refuse to overwrite it` |
| W25 | ERROR | an `"additive"` layer in a streamed world, or an additive layer with `initiallyActive` true | `WORLD_ERROR_LAYER_INVALID` | `<code>: layer '<name>': <additive layers need an unstreamed world / an additive layer has no initial state>` |
| W26 | ERROR | a `"grid"` layer in an unstreamed world | `WORLD_ERROR_LAYER_INVALID` | `<code>: layer '<name>': an unstreamed world has no grids` |
| W27 | ERROR | a grid receiving a static collider that is not split (with `splitStaticColliders` false, in a layer grid, on a hierarchy "Static collider split" never splits, or belonging to an explicit `PHYSICS_MOTION_TYPE_STATIC` rigid body) whose `activationRadius` is below the collision grid's `activationRadius + unloadHysteresis` | `WORLD_ERROR_GRID_INVALID` | `<code>: grid '<name>' holds unsplit static colliders (first: entity <id>, layer '<layer>'): activationRadius <r> must be at least <need>` |
| W28 | ERROR | `layerGridBase` differs from the number of base grids while `layerGridOrder` is non-empty; a grid-realised layer whose `id` is not in `layerGridOrder`; a duplicate entry | `WORLD_ERROR_GRID_INVALID` | `<code>: streaming: <reason>; run world.streaming.renumberLayerGrids to renumber the layer grids` |
| W29 | ERROR | `bakes.lighting.enabled` true in an unstreamed world | `WORLD_ERROR_INVALID_DESC` | `<code>: bakes.lighting: an unstreamed world has no lighting grid` |
| W30 | ERROR | a generator document missing or unreadable at its reference's `path`, a `path` other than `Generators/<GeneratorKey>.generator.json`, or a document whose `id` differs from the reference's `document` | `WORLD_ERROR_GENERATOR_INVALID` | `<code>: generator <generator key>: <reason>` |
| W31 | ERROR | an unknown generator ID or an unimplemented `generatorVersion` | `WORLD_ERROR_GENERATOR_VERSION` | `<code>: generator '<name>': '<id>' version <v> is not implemented (this tool implements 1 … <n>)` |
| W32 | ERROR | the generator's own parameter validation failed | `WORLD_ERROR_GENERATOR_INVALID` | `<code>: generator '<name>': <pointer>: <reason>` |
| W33 | ERROR | a `dependsOn` cycle, or an `id` that is no referenced generator document | `WORLD_ERROR_GENERATOR_INVALID` | `<code>: generator cycle <a> -> <b> -> … -> <a>` (names, each with its generator key) |
| W34 | WARNING | a baked generator whose recorded fingerprint differs from the current one | `WORLD_WARNING_BAKE_STALE` | `<code>: generator '<name>' region <x>_<z> is stale` |
| W35 | WARNING | a baked generator region with no output record | `WORLD_WARNING_BAKE_MISSING` | `<code>: generator '<name>' region <x>_<z> has never been generated` |
| W36 | ERROR | a `baked` generator whose `dependsOn` names a `cook` generator | `WORLD_ERROR_GENERATOR_INVALID` | `<code>: generator '<name>': a baked generator cannot depend on cook-mode '<other>' (its outputs move with the hash library)` |
| W37 | ERROR | `bakes.lighting.sun` non-null while a baked lighting layer or the persistent scene holds a `Zenith.Lighting.DirectionalLight` | `WORLD_ERROR_INVALID_DESC` | `<code>: bakes.lighting.sun duplicates the directional light <entity> of '<path>'` |
| W38 | ERROR | `layerGridTemplate` with `hlod.levels` other than 0 | `WORLD_ERROR_GRID_INVALID` | `<code>: streaming.layerGridTemplate: layer grids have no HLOD levels` |
| W39 | ERROR | a generated region whose generator document is not in `baked` mode, or whose key's layer key is not the layer key of the layer the generator document's `layer` names | `WORLD_ERROR_REGION_INVALID` | `<code>: region '<key>': <reason>` |
| W40 | WARNING | lighting enabled and a lighting unit with content has no header document | `WORLD_WARNING_BAKE_MISSING` | `<code>: lighting <grid>/<x>_<z> has not been baked` |
| W41 | WARNING | a header whose fingerprint differs from the current one, or whose `tracerVersion` differs from the tool's | `WORLD_WARNING_BAKE_STALE` | `<code>: lighting <grid>/<x>_<z> is stale (<what changed>)` |
| W42 | ERROR | a payload whose size, base64, codec frame or format is invalid; a page document missing or naming another header | `WORLD_ERROR_BAKE_CORRUPT` | `<code>: '<path>': <pointer>: <reason>` |
| W43 | ERROR | a non-finite float in a payload | `WORLD_ERROR_BAKE_CORRUPT` | `<code>: '<path>': <pointer>: element <i> is not finite` |
| W44 | ERROR | a header or page `id` that is not its derived ID | `WORLD_ERROR_BAKE_IDENTITY` | `<code>: '<path>': id <id> should be <derived>` |
| W45 | WARNING | a lighting header for a cell with no content, or for a grid the settings do not name; a page document whose index is at least its header's `pageCount`, or which has no header | `WORLD_WARNING_BAKE_ORPHANED` | `<code>: '<path>' bakes no current unit` |
| W46 | ERROR | navigation enabled and `bakes.navigation.settings` naming no AI navigation settings document (missing, or another document type) | `WORLD_ERROR_REFERENCE` | `<code>: bakes.navigation.settings <id> is not a navigation settings document` |
| W47 | WARNING | a hierarchy of a navigation-baking layer whose collider bounds (the union of its `Zenith.Physics.Collider` entities' world bounding spheres) extend more than `regionSize` beyond its owning region's rectangle on X or Z, so tiles beyond the 3 × 3 neighbourhood miss it ("Derived navigation documents") | `WORLD_WARNING_NAVIGATION_REACH` | `<code>: region '<key>': <entity> reaches <d> m beyond its region; navigation tiles farther than one region away ignore it` |

W50 (a stale tracked navigation document) is retired with the tracked navigation documents and is never reused. A world whose import fails declares no product, so no stale or half-written cell ever reaches the cooked tree (ASSET's atomic output publication, `Zenith2/Docs/Design/ASSET/Cook.md` → "Atomic output publication").

## Document limits

| Quantity | Limit | Constant | Error |
|---|---|---|---|
| world document size | 64 MiB (the framework's open limit) | — | `WORLD_ERROR_LIMIT` |
| layers per world | 32 | `uWORLD_MAX_LAYERS` | W6 |
| regions per world | 65,536 | `uWORLD_MAX_REGIONS` | `WORLD_ERROR_LIMIT` |
| generators per world | 64 | `uWORLD_MAX_GENERATORS` | `WORLD_ERROR_LIMIT` |
| entities per region document | 1,048,576 (SCENE's document limit) | — | SCENE's `SCENE_ERROR_LIMIT`, reported as `WORLD_ERROR_LIMIT` |
| entities of a world's resolved content | 16,777,216 | `uWORLD_MAX_WORLD_ENTITIES` | `WORLD_ERROR_LIMIT` |
| terrains per world | 16 | `uWORLD_MAX_TERRAINS` | `WORLD_ERROR_LIMIT` |
| `layerGridOrder` entries | 8 − the base grid count | — | W10 |
| light pages per unit | 256 (LIGHT's format) | `uWORLD_MAX_LIGHT_PAGES` | `WORLD_ERROR_ATLAS_FULL` |
| a payload's decompressed size | 268,435,456 bytes | `ulWORLD_MAX_PAYLOAD_BYTES` | W42 |
| generator outputs per (generator, region) | 65,536 entities | `uWORLD_MAX_GENERATED_PER_REGION` | `WORLD_ERROR_GENERATOR_LIMIT` |

These limits are rows of `Zenith2/Docs/Architecture.md` → "Parser limits" (core-doc item X303 (2), applied), and the four WORLD document types are in [Z2-N-CONTRACT-552]'s list (X303 (3)); the readers, the malformed-input corpus `Test_WorldDocument_MalformedCorpusNeverAsserts` and the at-limit tests assert them ([Z2-N-ARCH-953]). No fuzz target exists: fuzz targets are built in `win-asan` alone ([Z2-N-ARCH-171] matrices), where ZenithAuthoring (`ZENITH_TOOLS_HOST`) is absent, and WORLD's documents are read only by tools, never by a Player.

## Document type registration

`Zenith_RegisterWorldDocumentTypes(Zenith_MigrationRegistry&)` (`Zenith2/Docs/Design/WORLD/PublicAPI.md` → "Document type registration functions") registers four types, in this order, from ZenithAuthoring, so the editor's and the cook's registries hold the same set:

| Stable name | `m_szSchemaTag` | Suffixes | Version | Canonical schema |
|---|---|---|---|---|
| `Zenith.World.WorldDocument` | `zenith2.world` | `.world.json` | 1 | "World canonical schema" |
| `Zenith.World.GeneratorDocument` | `zenith2.world-generator` | `.generator.json` | 1 | "Generator canonical schema" |
| `Zenith.World.LightBakeDocument` | `zenith2.world-lightbake` | `.lightbake.json` | 1 | "Light bake canonical schema" |
| `Zenith.World.LightPageDocument` | `zenith2.world-lightpage` | `.lightpage.json` | 1 | "Light bake canonical schema" (its page rule as the root) |

Each type's `m_pfnVisitFieldSets` is null (no component field sets are embedded). Region and generated region documents are SCENE's type (`Zenith.SceneDocument`), migrated by SCENE's chain; derived navigation documents are AI's type and exist only in memory.

<!-- assumption: pending EDIT (row WD15) -->
`Zenith_RegisterEditorDocumentTypes` (`Zenith2/Docs/Design/EDIT/Foundation.md` → "Storage roots and the migration registry") calls `Zenith_RegisterWorldDocumentTypes` after ENV's registration, and the cook services' item 5 calls it in the same position, so `Zenith2/Docs/Design/ASSET/Migration.md`'s registered-types order gains the four rows.
<!-- /assumption -->

### World canonical schema

| Rule | Node | Members in canonical order, or element rule |
|---|---|---|
| 0 root | OBJECT | `$schema`, `schemaVersion`, `id`, `name`, `seed`, `persistentScene`, `regionSize`, `origin` (INLINE_ARRAY), `baseY`, `height`, `streaming` (rule 1), `layers` (rule 3), `regions` (rule 5), `generators` (rule 7), `terrains` (INLINE_ARRAY when ≤ 4 IDs, else ORDERED_ARRAY), `bakes` (rule 9) |
| 1 streaming | OBJECT | `mode`, `grids` (ORDERED_ARRAY of rule 2), `splitStaticColliders`, `largeExtentPromotion`, `layerGridTemplate` (rule 2), `layerGridBase`, `layerGridOrder` (ORDERED_ARRAY of VALUE) |
| 2 grid | OBJECT | STREAM's grid object members in its table order (`Zenith2/Docs/Design/STREAM/Formats.md` → "Grid object"), then `defaultFor` (INLINE_ARRAY) |
| 3 layers | KEYED_MAP | element rule 4 |
| 4 layer | OBJECT | `id`, `kind`, `state`, `grid`, `realisation`, `initiallyActive`, `bakes` (OBJECT `lighting`, `navigation`, `hlod`), `editor` (OBJECT `colour` INLINE_ARRAY, `visible`, `loaded`, `locked`) |
| 5 regions | KEYED_MAP | element rule 6 |
| 6 region | OBJECT | `scene`, `generator` |
| 7 generators | KEYED_MAP | element rule 8 |
| 8 generator reference | OBJECT | `document`, `path` |
| 9 bakes | OBJECT | `lighting` (OBJECT in the table order of "Lighting settings"; `materialAlbedo` an ID_MAP of INLINE_ARRAY), `hlod` (OBJECT, every array INLINE_ARRAY), `navigation` (OBJECT, arrays of objects ORDERED_ARRAY) |

### Generator canonical schema

Root OBJECT: `$schema`, `schemaVersion`, `id`, `name`, `generator`, `generatorVersion`, `mode`, `layer`, `scope` (OBJECT `regions` VALUE or INLINE_ARRAY, `bounds`), `seedSalt`, `dependsOn` (INLINE_ARRAY), `parameters` (VALUE: the generator's object is written verbatim with raw numbers, `CANONICAL_NODE_VALUE`), `outputs` (KEYED_MAP of OBJECT `scene`, `fingerprint`, `digest`, `sceneSchemaVersion`, `entities`).

### Light bake canonical schema

Header root OBJECT: `$schema`, `schemaVersion`, `id`, `world`, `grid`, `x`, `z`, `fingerprint`, `tracerVersion`, `pageCount`, `pageSize`, `rgbmRange`, `bindings` (ID_MAP of OBJECT `page`, `scale` INLINE_ARRAY, `bias` INLINE_ARRAY), `volume` (OBJECT `dims`, `minCorner`, `spacing` INLINE_ARRAY each, `sh`, `validity` payload OBJECT), `probes` (ID_MAP of OBJECT `captureOffset` INLINE_ARRAY, `captureEV100`, `interior`, `radiance` payload OBJECT). Page root OBJECT: `$schema`, `schemaVersion`, `id`, `bake`, `page`, `size`, `irradiance`, `direction`, `coverage` (payload OBJECTs). A payload OBJECT: `format`, `count`, `codec`, `data`.

### Migration chains

Every type starts at schema version 1 with an empty chain. A schema change adds one `Zenith_PfnMigrationStep` registered by `Zenith_RegisterWorldDocumentTypes` with a fixture pair `v<N>.json` and `v<N>.expected.json` (old document, expected migrated document, JSON text) under `Zenith2/Tools/Authoring/Tests/Fixtures/Migration/WORLD/<Type>/`, the ZenithAuthoring fixture root of `Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules" rule 2 (`Zenith2/Docs/Design/SCENE/DocumentFramework.md` → "Migration fixtures"); the editor migrates a file on disk through EDIT's migration flow (preview, backup, journaled write, resume, `Zenith2/Docs/Design/EDIT/Pipeline.md` → "Migration flow"), and the importer migrates in memory only. A change to the light bake payload layout or to the tracer's output for unchanged inputs bumps `uWORLD_LIGHT_TRACER_VERSION`, which makes every lighting unit stale (W41) rather than migrating it, because baked data is regenerated, never translated.

### World document example

```json
{
	"$schema": "zenith2.world",
	"schemaVersion": 1,
	"id": "3c2a7e10-5d4b-4b8e-9f21-0a6b3d7c9e01",
	"name": "Valley",
	"seed": "6364136223846793005",
	"persistentScene": "6a0f3b21-9c7e-4d58-b1a4-2e8d0c6f9b37",
	"regionSize": 256,
	"origin": [-2048, -2048],
	"baseY": -512,
	"height": 2048,
	"streaming": {
		"mode": "streamed",
		"grids": [
			{
				"name": "collision",
				"role": "collision",
				"cellSize": 256,
				"origin": [-2048, -2048],
				"baseY": -512,
				"height": 2048,
				"activationRadius": 512,
				"requiredRadius": 256,
				"unloadHysteresis": 64,
				"prefetchRadius": 768,
				"dependsOn": [],
				"hlod": {
					"levels": 0
				},
				"defaultFor": ["collision"]
			},
			{
				"name": "content",
				"role": "gameplay",
				"cellSize": 128,
				"origin": [-2048, -2048],
				"baseY": -512,
				"height": 2048,
				"dependsOn": ["collision"],
				"hlod": {
					"levels": 2,
					"maxDistance": [2048, 8192]
				},
				"defaultFor": ["gameplay", "navigation", "audio"]
			}
		],
		"splitStaticColliders": true,
		"layerGridTemplate": {
			"role": "gameplay",
			"cellSize": 128,
			"origin": [-2048, -2048],
			"baseY": -512,
			"height": 2048,
			"activationRadius": 576,
			"requiredRadius": 128,
			"unloadHysteresis": 64,
			"prefetchRadius": 768,
			"dependsOn": [],
			"hlod": {
				"levels": 0
			}
		},
		"layerGridBase": 2,
		"layerGridOrder": ["c0e5f2a8-7d41-4b39-8e06-1f9a3c5d2b78"]
	},
	"layers": {
		"Buildings": {
			"id": "9e1b4c27-0a6d-4f83-b5e2-7c3d1a9f0e64",
			"kind": "data",
			"state": "baked"
		},
		"Dressing": {
			"id": "5d8a3f61-2c4e-4a97-b0d3-8e1f6c2a9b45",
			"kind": "data",
			"state": "baked"
		},
		"Reference": {
			"id": "14d7a0e9-3b2c-4e65-9f18-6a0c2d7e5b91",
			"kind": "data",
			"state": "editorOnly"
		},
		"VillageRuined": {
			"id": "c0e5f2a8-7d41-4b39-8e06-1f9a3c5d2b78",
			"kind": "gameplay",
			"state": "runtime",
			"realisation": "grid",
			"initiallyActive": false
		}
	},
	"regions": {
		"9e1b4c27/0_-1": {
			"scene": "2f6c1a90-7d3e-4b51-8a2c-0e9d4f7b3c11"
		},
		"c0e5f2a8/0_-1": {
			"scene": "7b3e9d0c-1a5f-4c82-9e47-0d6b2f8a1c35"
		}
	},
	"generators": {
		"51c0d8e2": {
			"document": "51c0d8e2-3a7f-4b96-8d15-2e0a9c7f4b63",
			"path": "Generators/51c0d8e2.generator.json"
		}
	},
	"terrains": [],
	"bakes": {
		"lighting": {
			"enabled": true,
			"grid": "content"
		}
	}
}
```

The template's `activationRadius` 576 equals the collision grid's 512 + 64, so the layer grid `layer.c0e5f2a8` (index 2) satisfies W27 for the ruined village's walls, which stay in it ("Static collider split" of `Zenith2/Docs/Design/WORLD/Partition.md`). The generator referenced as `51c0d8e2` (display name `FencePosts`, "Generator document example") names layer `Dressing` by its `id`; the reader reports no finding for this document, and the importer pass, given that generator document and a persistent scene, reports none for the generator either.
