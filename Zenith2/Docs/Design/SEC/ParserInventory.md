# SEC design: parser and fuzz inventory

This file is part of the SEC design family; the entry document is `Zenith2/Docs/Design/SEC.md`. It is the one programme-wide inventory of every parser of external bytes ([Z2-N-ARCH-951]) and every fuzz target ([Z2-N-ARCH-952]): one row per parser, naming its library, its trust boundary and tier, where its limits are specified, its fuzz target and the tickets that own it. The limits themselves stay in the owners' designs and in `Zenith2/Docs/Architecture.md` → "Security architecture" (table "Parser limits"); this file never restates a number. Its machine-readable form is `Zenith2/Tools/Security/parser_inventory.json` (`Zenith2/Docs/Design/SEC.md` → "Parser inventory registry"), which the SecurityConformance gate step checks (`Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Checks").

## Scope of this file

| Topic | Heading |
|---|---|
| what must be in the inventory and how a row changes | "Inventory rules" |
| the class and tier vocabulary | "Boundary classes and tiers used here" |
| the meaning of each column | "Entry columns" |
| the rows, grouped by area | "Core and platform parsers" to "Content and authoring parsers" |
| the gaps found while compiling the inventory, each with its owner | "Conformance findings" |
| the count per tier and the campaign arithmetic | "Tier totals and campaign estimate" |

## Inventory rules

1. Every function, reader or decoder that interprets bytes from a source of `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "The nine boundaries" is a row here and an entry of `parser_inventory.json`, with the same `id`.
2. Every `Tests/Fuzz/<Stem>.cpp` under `Zenith2/Engine/**`, `Zenith2/Tools/**` and `Zenith2/Player/**` is the fuzz target of exactly one row (or of one row per parser it drives, when one source fuzzes several parsers, each row naming the same stem).
3. A row's limits are specified in one or more rows of the Architecture table "Parser limits" (each by its first cell, in quotes) or, when no table row holds them, in exactly one owner heading; such a row carries `pending-row` when core-doc row X413 asks Architecture to add a table row for it, and `limits-in-design` when the owner heading (a design heading or an Architecture sub-document heading) is the limit's permanent home and no table row is requested.
4. A ticket that adds, removes or renames a parser edits its row here and its entry in `parser_inventory.json` in the same diff, together with the Architecture table row ([Z2-N-ARCH-953]) or the core-doc row that requests it.
5. Every row names at least one owner ticket by Draft-ID, or `pending:<AREA>` while that area's tickets are not drafted.
6. A row without a fuzz target carries one of the statuses of "Entry columns" that explains why, and every such status other than `outside-scope` is listed in "Conformance findings" with the owner who closes it.

## Boundary classes and tiers used here

| Class | Tier | Boundary |
|---|---|---|
| `shipped-content` | 2 | B1 |
| `downloaded-content` | 1 | B2 |
| `user-files` | 1 | B3 |
| `scripts` | 2 | B4 |
| `remote-tools` | 3 | B5 |
| `network` | 1 | B6 |
| `online` | 1 | B7 |
| `tool-inputs` | 3 | B8 |
| `process-ipc` | 1 | B9 |

A row lists every class it is reached through; its tier is the lowest tier number among them (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Boundary classes and tiers").

## Entry columns

| Column | Content |
|---|---|
| ID | `PI-<AREA>-NNN`, stable, never reused |
| Parser and input | the entry function or reader, and the bytes it interprets |
| Library | the target that owns the parser's source (`ZENITH_SUPPORTED` matters: a fuzz target is built only where the library exists in `win-asan`) |
| Classes | the `boundary` values |
| Tier | 1, 2 or 3 |
| Limits | the Architecture "Parser limits" rows (each by its first cell, in quotes), the owner heading, or both; the registry carries the rows as the array `limitsRows` and the heading as `limitsDesign` |
| Fuzz target | `Zenith_Fuzz_<Stem>` as the generic rule names it from `Tests/Fuzz/<Stem>.cpp` |
| Owner | the owning tickets' Draft-IDs |
| Status | `active` (conformant); `property` (a property fuzzer of an encoder and decoder pair, no external input of its own); `naming` (the stem or the file name breaks "Stem naming rule" of `Zenith2/Docs/Design/SEC/Fuzzing.md`); `dead-target` (the source sits in a `ZENITH_TOOLS_HOST` target, so no `win-asan` executable is ever built); `gap` (no fuzz target specified); `covered` (tier 3 only: the bytes reach no parser of their own beyond rows named in the cell); `outside-scope` (tier 3 only: local operator input outside [Z2-N-ARCH-951]'s list, reviewed instead of fuzzed); `pending-row` (no table row holds the limits yet and core-doc row X413 requests one); `limits-in-design` (the limits' permanent home is the owner heading in "Limits", and no table row is requested) |

## Core and platform parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-CORE-001 | `Zenith_JsonDocument::Parse`: every JSON byte sequence (configuration, documents, manifests, schemas) | ZenithCore | shipped-content, user-files, tool-inputs | 1 | "JSON (authoring documents, configuration, game.json, schemas)" | `Zenith_Fuzz_JsonDocument` | Z2T-CORE-074 | active |
| PI-CORE-002 | UTF-8 validation of every text field read from outside | ZenithCore | user-files, shipped-content | 1 | `Zenith2/Docs/Design/CORE/Text.md` → "JSON reader" | `Zenith_Fuzz_Utf8` | Z2T-CORE-016 | active, pending-row |
| PI-CORE-003 | configuration layer files (`user:/config/`, the game and platform layers) | ZenithCore | user-files, shipped-content | 1 | "JSON (authoring documents, configuration, game.json, schemas)"; `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Configuration file format" | `Zenith_Fuzz_ConfigLayer` | Z2T-CORE-076 | active |
| PI-CORE-004 | persistent-ID text and binary forms in saves and documents | ZenithCore | user-files, tool-inputs | 1 | `Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_PersistentID functions" | `Zenith_Fuzz_PersistentID` | Z2T-CORE-072 | active, pending-row |
| PI-COMPRESS-001 | the 24-byte compressed-section descriptor | ZenithCompression | shipped-content, downloaded-content, user-files | 1 | "cooked asset file"; `Zenith2/Docs/Design/COMPRESS.md` → "Untrusted input rules" | `Zenith_Fuzz_CompressedSectionDesc` | Z2T-COMPRESS-023 | active |
| PI-COMPRESS-002 | zlib stream decode | ZenithCompression_Zlib | shipped-content, downloaded-content, user-files | 1 | "cooked asset file" | `Zenith_Fuzz_ZlibDecode` | Z2T-COMPRESS-023 | active |
| PI-COMPRESS-003 | zstd frame decode | ZenithCompression_Zstd | shipped-content, downloaded-content, user-files | 1 | "cooked asset file" | `Zenith_Fuzz_ZstdDecode` | Z2T-COMPRESS-023 | active |
| PI-COMPRESS-004 | zstd dictionary load | ZenithCompression_Zstd | shipped-content | 2 | `Zenith2/Docs/Design/COMPRESS.md` → "Untrusted input rules" | `Zenith_Fuzz_ZstdDictionaryLoad` | Z2T-COMPRESS-023 | active, limits-in-design |
| PI-COMPRESS-005 | zlib encode then decode round trip | ZenithCompression_Zlib | shipped-content | 2 | none (property) | `Zenith_Fuzz_ZlibRoundTrip` | Z2T-COMPRESS-023 | property |
| PI-COMPRESS-006 | zstd encode then decode round trip | ZenithCompression_Zstd | shipped-content | 2 | none (property) | `Zenith_Fuzz_ZstdRoundTrip` | Z2T-COMPRESS-023 | property |
| PI-PLAT-001 | `Zenith_ParseVirtualPath` and `Zenith_VirtualPath::Append`: every path from content, saves, configuration, scripts or the network | ZenithPlatform | shipped-content, user-files, scripts, network | 1 | `Zenith2/Docs/Architecture/PlatformLayer.md` → "Path validation"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Validation rules and order" | `Zenith_Fuzz_VirtualPath` | Z2T-PLAT-007 | active, limits-in-design |
| PI-PLAT-002 | the mount manifest `mount-manifest.json` and its schema rules | ZenithPlatform | shipped-content | 2 | "JSON (authoring documents, configuration, game.json, schemas)"; `Zenith2/Docs/Design/PLAT/Filesystem.md` → "Mount manifest schema" | `Zenith_Fuzz_MountManifest` | Z2T-PLAT-016 | active |
| PI-PLAT-003 | `Zenith_DecodeUpdateManifest`, `Zenith_CheckUpdateManifestTarget`: `update.zman` and `dlc.zman` bytes | ZenithPlatform | downloaded-content | 1 | "update and DLC manifests (update.zman, dlc.zman)" | `Zenith_Fuzz_UpdateManifest` | Z2T-PLAT-092 | active |
| PI-PLAT-004 | input bindings documents of every layer, the user layer included | ZenithPlatform | user-files, shipped-content | 1 | "JSON (authoring documents, configuration, game.json, schemas)"; `Zenith2/Docs/Design/PLAT/Input.md` → "Validation rules" | `Zenith_Fuzz_BindingsJson` | Z2T-PLAT-055 | active |
| PI-PLAT-005 | input command decode (recorded commands in replay logs) | ZenithPlatform | user-files | 1 | "replay logs" | `Zenith_Fuzz_InputCommandDecode` | Z2T-PLAT-065 | active |
| PI-PLAT-006 | the `zenith2.input-script` document of `--input-script` | ZenithPlatform | tool-inputs | 3 | "JSON (authoring documents, configuration, game.json, schemas)" | `Zenith_Fuzz_InputScript` | Z2T-PLAT-066 | active |
| PI-SEC-001 | `Zenith_Ed25519Verify` over arbitrary 64-byte signatures and messages against a fixed key | ZenithPlatform (SEC's files) | downloaded-content | 1 | `Zenith2/Docs/Design/SEC/UpdateAuthentication.md` → "Primitive algorithm" | `Zenith_Fuzz_Ed25519Verify` | pending:SEC | active, limits-in-design |
| PI-MOD-001 | the C++ `game.json` validator | ZenithNewGameScaffold (`Zenith2/Docs/Design/MOD/ProjectCreation.md` → "Validation in C++") | tool-inputs | 3 | "JSON (authoring documents, configuration, game.json, schemas)" | none (stem `GameJson` requested) | pending:MOD | gap |
| PI-MOD-002 | `Zenith_CommandLine::Parse` of the process arguments | ZenithRuntime | tool-inputs | 3 | `Zenith2/Docs/Architecture/CompositionRoot.md` → "Command-line options" | none | pending:MOD | outside-scope, limits-in-design |

## Script reflection and scene parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-SCRIPT-001 | the cooked bytecode container header and its load-time verification fields | ZenithScript | scripts, shipped-content | 2 | `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" | `Zenith_Fuzz_Script_BytecodeHeader` | Z2T-SCRIPT-027 | active, pending-row |
| PI-SCRIPT-002 | `Zenith_ScriptModuleManifest::Load` | ZenithScript | scripts, shipped-content | 2 | `Zenith2/Docs/Scripting.md` → "Shipping bytecode compatibility" | `Zenith_Fuzz_Script_Manifest` | Z2T-SCRIPT-026 | active, pending-row |
| PI-SCRIPT-003 | the behaviour checkpoint decoder (checkpoints in saves and replays) | ZenithScript | user-files | 1 | `Zenith2/Docs/Scripting.md` → "Sandboxing and security" ([Z2-N-SCRIPT-485]) | `Zenith_Fuzz_Script_CheckpointDecode` | Z2T-SCRIPT-070 | active, pending-row |
| PI-SCRIPT-004 | the Luau source compiler under the source limits | ZenithScriptCompiler | tool-inputs | 3 | `Zenith2/Docs/Scripting.md` → "Sandboxing and security" ([Z2-N-SCRIPT-487]) | `Zenith_Fuzz_Script_Compile` | Z2T-SCRIPT-043 | active, pending-row |
| PI-SCRIPT-005 | the DAP transport framing and request decoder | ZenithScript | remote-tools | 3 | `Zenith2/Docs/Design/SCRIPT/Debugger.md` → "Transport and framing" | `Zenith_Fuzz_Script_DapMessage` | Z2T-SCRIPT-019 | active, pending-row |
| PI-ECS-001 | `RegisterSchemaBlob` over `schemas.zreg` | ZenithReflect | shipped-content | 2 | "schema blobs" | `Zenith_Fuzz_SchemaBlob` | Z2T-ECS-028, Z2T-ECS-024 | active |
| PI-ECS-002 | `*.schema.json` documents | ZenithReflect | tool-inputs | 3 | "JSON (authoring documents, configuration, game.json, schemas)" | `Zenith_Fuzz_SchemaJson` | Z2T-ECS-028 | active |
| PI-ECS-003 | field-set decode (binary and JSON) of saved component data | ZenithReflect | user-files | 1 | "saves" | `Zenith_Fuzz_FieldSetDecode` | Z2T-ECS-028, Z2T-ECS-018 | active |
| PI-SCENE-001 | the document write journal and `RecoverPendingJournals` | ZenithDocument | tool-inputs | 3 | `Zenith2/Docs/Design/SCENE/Format.md` → "Document limits" | `Zenith_Fuzz_DocumentJournal` | Z2T-SCENE-018 | active, limits-in-design |
| PI-SCENE-002 | scene and prefab documents and their validator | ZenithScene | tool-inputs | 3 | `Zenith2/Docs/Design/SCENE/Format.md` → "Document limits" | `Zenith_Fuzz_SceneDocument` | Z2T-SCENE-042 | active, pending-row |
| PI-SCENE-003 | the cooked-scene decoder | ZenithScene | shipped-content | 2 | `Zenith2/Docs/Design/SCENE/Format.md` → "Document validation" | `Zenith_Fuzz_SceneCooked` | Z2T-SCENE-044 | active, pending-row |

## Asset and geometry parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-ASSET-001 | the cooked header and section table | ZenithAssets | shipped-content | 2 | "cooked asset file" | `Zenith_Fuzz_CookedHeader` | Z2T-ASSET-034, Z2T-ASSET-014 | active |
| PI-ASSET-002 | the asset catalogue and catalogue fragments | ZenithAssets | shipped-content | 2 | "asset catalogue" | `Zenith_Fuzz_Catalogue` | Z2T-ASSET-034, Z2T-ASSET-017 | active |
| PI-ASSET-003 | section decode through the codec set | ZenithAssets | shipped-content | 2 | "cooked asset file" | `Zenith_Fuzz_SectionDecode` | Z2T-ASSET-034 | active |
| PI-ASSET-004 | the 24-byte asset load result payload (recorded publications) | ZenithAssets | user-files | 1 | "asset load result payload" | `Zenith_Fuzz_LoadResult` | Z2T-ASSET-034 | active |
| PI-ASSET-005 | `Zenith_CreateArchiveMountSource` over a `.zpak` | ZenithAssets | shipped-content | 2 | "cooked archive (.zpak)"; `Zenith2/Docs/Design/ASSET/Formats.md` → "Archive checks" | `Zenith_Fuzz_Archive` | Z2T-ASSET-064 | active |
| PI-ASSET-006 | the asset database `assetdb.zdb` | ZenithCook | tool-inputs | 3 | "asset database file" | `Zenith_Fuzz_AssetDatabase` (through `zenith_cook_fuzz_target`) | Z2T-ASSET-039 | active |
| PI-ASSET-007 | `.meta` sidecars | ZenithCook | tool-inputs | 3 | ".meta sidecar" | `Zenith_Fuzz_MetaDocument` (through `zenith_cook_fuzz_target`) | Z2T-ASSET-035 | active |
| PI-ASSET-008 | DDC entries, local and shared | ZenithCook | tool-inputs | 3 | "DDC entry" | `Zenith_Fuzz_DdcEntry` (through `zenith_cook_fuzz_target`) | Z2T-ASSET-075 | active |
| PI-GEOM-001 | the `.zmesh` reader | ZenithFlux | shipped-content | 2 | "meshes" | `Zenith_Fuzz_MeshFile` | Z2T-GEOM-034 | active |
| PI-GEOM-002 | the `.ztex` reader | ZenithFlux | shipped-content | 2 | "textures" | `Zenith_Fuzz_TextureFile` | Z2T-GEOM-040 | active |
| PI-GEOM-003 | the `.zmat` reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/GEOM/Materials.md` → "Material reader validation" | `Zenith_Fuzz_MaterialFile` | Z2T-GEOM-048 | active, pending-row |
| PI-GEOM-004 | stream-set codes and canonical names | ZenithVertexFormat | shipped-content | 2 | `Zenith2/Docs/Design/GEOM/VertexFormat.md` → "Layout validation and hashing" | `Zenith_Fuzz_StreamSetName` | Z2T-GEOM-011 | active, pending-row |

A cooked file in a committed patch, a Windows DLC or a delivered asset pack is read only after its bytes were authenticated (the signed manifest's SHA-256, or the base `package.zpkm`'s hash for a pack), and a stamp-trusted file is trusted exactly as far as the executable is ([Z2-N-ARCH-966]), so every cooked-format reader is class `shipped-content`. Class `downloaded-content` belongs to the parsers that run before that authentication: the manifest decoder, the signature primitive, the bundle listing of U0 and PACK's install-state files.

## Rendering parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-LIGHT-001 | the `.zenv` static environment reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" | `Zenith_Fuzz_EnvironmentFile` | Z2T-LIGHT-049 | active, pending-row |
| PI-LIGHT-002 | the `.zlut` grading LUT reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" | `Zenith_Fuzz_GradingLUTFile` | Z2T-LIGHT-064 | active, pending-row |
| PI-LIGHT-003 | the `.zgiv` irradiance-volume reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/LIGHT/GI.md` → "Irradiance volume cooked format" | `Zenith_Fuzz_IrradianceVolumeFile` | Z2T-LIGHT-085 | active, pending-row |
| PI-LIGHT-004 | the `.zlmp` lightmap-set reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/LIGHT/GI.md` → "Lightmap set cooked format" | `Zenith_Fuzz_LightmapSetFile` | Z2T-LIGHT-085 | active, pending-row |
| PI-LIGHT-005 | the `.zprb` reflection-probe reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/LIGHT/Reflections.md` → "Reflection probe cooked format" | `Zenith_Fuzz_ProbeFile` | Z2T-LIGHT-084 | active, pending-row |
| PI-LIGHT-006 | the `.cube` LUT source reader of `Cook_GradingLUT` | ZenithCook | tool-inputs | 3 | `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" | `Zenith_Fuzz_CubeReader` | Z2T-LIGHT-065 | dead-target, pending-row |
| PI-LIGHT-007 | the RGBE (`.hdr`) source reader of `Cook_Environment` | ZenithCook | tool-inputs | 3 | `Zenith2/Docs/Design/LIGHT.md` → "Data structures and formats" | `Zenith_Fuzz_RGBEReader` | Z2T-LIGHT-050 | dead-target, pending-row |
| PI-RHI-001 | the Zenith2 header of a persistent pipeline-cache file (`ZPCV`, `ZPCD`) and its payload hash | ZenithFlux | user-files | 1 | `Zenith2/Docs/Rendering/ShaderSystem.md` → "Persistent pipeline caches" | none (stem `PipelineCacheHeader` requested) | pending:RHI | gap, pending-row |

## Animation physics and AI parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-ANIM-001 | the `.zskel` reader | ZenithAnimation | shipped-content | 2 | "animation assets"; `Zenith2/Docs/Design/ANIM/Formats.md` → "Parser limits" | `Zenith_Fuzz_SkeletonFile` | Z2T-ANIM-018 | active |
| PI-ANIM-002 | the `.zanim` reader | ZenithAnimation | shipped-content | 2 | "animation assets" | `Zenith_Fuzz_ClipFile` | Z2T-ANIM-018 | active |
| PI-ANIM-003 | the `.zagraph` reader | ZenithAnimation | shipped-content | 2 | "animation assets" | `Zenith_Fuzz_GraphFile` | Z2T-ANIM-018, Z2T-ANIM-036 | active |
| PI-ANIM-004 | the `ZISK` and `ZICL` cook intermediates | ZenithCook | tool-inputs | 3 | "animation assets" | `Zenith_Fuzz_AnimIntermediates` (through `zenith_cook_fuzz_target`) | Z2T-ANIM-029 | dead-target |
| PI-ANIM-005 | the five animation document types and the graph compiler | ZenithAuthoring | tool-inputs | 3 | "animation assets" | `Zenith_Fuzz_AnimDocuments` | Z2T-ANIM-029 | dead-target |
| PI-PHYS-001 | the `.zcol`, `.zpmat` and `.zragdoll` readers | ZenithPhysics | shipped-content | 2 | "collision shapes (.zcol)", "physics materials (.zpmat)", "ragdolls (.zragdoll)" | `Zenith_Fuzz_PhysicsCooked` | Z2T-PHYS-050 | active |
| PI-PHYS-002 | the physics save section | ZenithPhysics | user-files | 1 | "physics save section" | none (stem `PhysicsSaveSection` requested) | pending:PHYS | gap |
| PI-PHYS-003 | the `ZPHI` and `ZPCI` cook intermediates | ZenithCook | tool-inputs | 3 | "physics cook intermediates (ZPHI, ZPCI)" | none | pending:PHYS | gap |
| PI-PHYS-004 | the physics layer configuration strings | ZenithPhysics | shipped-content | 2 | "physics layer configuration strings" | none (stem `PhysicsLayerConfig` requested) | pending:PHYS | gap |
| PI-AI-001 | the `.znav` tile, volume and layers sections | ZenithAI | shipped-content | 2 | "navigation files (.znav)" | `Zenith_Fuzz_Zenith_NavFuzz` (one source, five readers) | Z2T-AI-078 | naming |
| PI-AI-002 | the `ZNSG` source intermediate | ZenithAI | tool-inputs | 3 | "navigation source intermediate (ZNSG)" | `Zenith_Fuzz_Zenith_NavFuzz` | Z2T-AI-078 | naming |
| PI-AI-003 | the AI save section | ZenithAI | user-files | 1 | "AI save section" | `Zenith_Fuzz_Zenith_NavFuzz` | Z2T-AI-078 | naming |
| PI-AI-004 | navigation and navigation-settings documents | ZenithAI | tool-inputs | 3 | "navigation and navigation-settings documents" | covered by PI-CORE-001 and the AI document validator's mutated-corpus cases | Z2T-AI-078 | covered |
| PI-AI-005 | script path requests (64 bytes) and results (416 bytes) in replays | ZenithAI | user-files | 1 | "script path requests and results" | none (stem `PathRequestPayload` requested) | pending:AI | gap |
| PI-AI-006 | the `ZNTJ` tile bake intermediate (blob `ai.navTileJob`) | ZenithAI | tool-inputs | 3 | "navigation tile bake intermediate (`ZNTJ`)"; `Zenith2/Docs/Design/AI/Formats.md` → "Tile bake intermediate" | none (stem `NavTileBakeJob` requested) | Z2T-AI-078 | gap |

## Streaming save and audio parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-STREAM-001 | the streaming-world kind reader (the eight `.zstream` sections) | ZenithStreaming | shipped-content | 2 | "streaming index (.zstream)" | `Zenith_Fuzz_StreamingWorld` | Z2T-STREAM-014 | active |
| PI-STREAM-002 | `Zenith_DecodeStreamingCellDelta` (cell deltas in saves) | ZenithStreaming | user-files | 1 | "streaming cell delta" | `Zenith_Fuzz_StreamingWorld` | Z2T-STREAM-044 | active |
| PI-SAVE-001 | the save image reader | ZenithSave | user-files, online | 1 | "saves" | `Zenith_Fuzz_SaveImage` | Z2T-SAVE-067 | active |
| PI-SAVE-002 | the save journal | ZenithSave | user-files | 1 | "saves" | `Zenith_Fuzz_SaveJournal` | Z2T-SAVE-034 | active |
| PI-SAVE-003 | save metadata and the listing prefix | ZenithSave | user-files | 1 | "saves" | `Zenith_Fuzz_SaveMetadata` | Z2T-SAVE-034 | active |
| PI-SAVE-004 | the replay log reader | ZenithSave | user-files | 1 | "replay logs" | `Zenith_Fuzz_ReplayLog` | Z2T-SAVE-020 | active |
| PI-AUDIO-001 | the `.zsnd` parser | ZenithAudio | shipped-content | 2 | "audio" | `Zenith_Fuzz_SoundFile` | Z2T-AUDIO-019 | active |
| PI-AUDIO-002 | the `.zbank` parser | ZenithAudio | shipped-content | 2 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks" | `Zenith_Fuzz_BankFile` | Z2T-AUDIO-020 | active, pending-row |
| PI-AUDIO-003 | the mixer snapshot apply step | ZenithAudio | shipped-content | 2 | none (property) | `Zenith_Fuzz_MixerSnapshot` | Z2T-AUDIO-026 | property |
| PI-AUDIO-004 | trigger command payloads (recorded commands) | ZenithAudio | user-files | 1 | `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks" | `Zenith_Fuzz_TriggerPayload` | Z2T-AUDIO-036 | active, pending-row |

## UI and 2D parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-UI-001 | UI, theme, font family, localization project and string table documents | ZenithUI | tool-inputs | 3 | `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" | `Zenith_Fuzz_Fuzz_UIDocumentJson` | Z2T-UI-025, Z2T-UI-038 | naming, pending-row |
| PI-UI-002 | the `.zui` loader | ZenithUI | shipped-content | 2 | "cooked UI document (.zui)" | `Zenith_Fuzz_Fuzz_UICookedDocument` | Z2T-UI-024 | naming |
| PI-UI-003 | the `.zfont` loader and its `ZMSD` page payload | ZenithUI | shipped-content | 2 | "cooked font (.zfont) and its ZMSD page payload" | `Zenith_Fuzz_Fuzz_UICookedFont` | Z2T-UI-039 | naming |
| PI-UI-004 | the `.zloc` loader and `Format` | ZenithUI | shipped-content | 2 | "cooked localization table (.zloc)" | `Zenith_Fuzz_Fuzz_UICookedLocalization` | Z2T-UI-104 | naming |
| PI-UI-005 | `Zenith_CompileMessage` over message sources | ZenithUI | tool-inputs | 3 | `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" | `Zenith_Fuzz_Fuzz_UIMessageSyntax` | Z2T-UI-103 | naming, pending-row |
| PI-UI-006 | `Zenith_CompilePluralRule` over CLDR rule strings | ZenithUI | tool-inputs | 3 | `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" | `Zenith_Fuzz_Fuzz_UIPluralRule` | Z2T-UI-103 | naming, pending-row |
| PI-UI-007 | `Zenith_DecodeUIPublication` (recorded publications) | ZenithUI | user-files | 1 | "UI publication payload" | `Zenith_Fuzz_Fuzz_UIPublication` | Z2T-UI-083 | naming |
| PI-UI-008 | the paragraph pipeline over arbitrary UTF-8 | ZenithUI | shipped-content, scripts | 2 | `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" | `Zenith_Fuzz_Fuzz_UITextLayout` | Z2T-UI-042 | naming, limits-in-design |
| PI-UI-009 | the UI binding manifest | ZenithUI | shipped-content | 2 | "UI binding manifest (UI/BindingManifest.zbin)" | `Zenith_Fuzz_Fuzz_UIBindingManifest` | Z2T-UI-055 | naming |
| PI-UI-010 | `Zenith_ReadXliffImport` | ZenithCook | tool-inputs | 3 | `Zenith2/Docs/Design/UI/Documents.md` → "Parser limits" | `Zenith_Fuzz_Fuzz_UIXliff` | Z2T-UI-108 | dead-target, naming, pending-row |
| PI-TWOD-001 | every cooked TWOD kind reader | Zenith2D | shipped-content | 2 | "cooked TWOD files" | `Zenith_Fuzz_TwoDAsset` | Z2T-TWOD-032 | active |
| PI-TWOD-002 | `Zenith_ValidateTwoDDocument` and the four compilers | ZenithAuthoring | tool-inputs | 3 | "2D atlas document", "2D animation document", "2D tileset document", "2D tilemap document", "2D rig document" | `Zenith_Fuzz_TwoDDocument` | Z2T-TWOD-033 | dead-target |
| PI-TWOD-003 | the TWOD save participant's section reader | Zenith2D | user-files | 1 | "TWOD save section" | `Zenith_Fuzz_TwoDSaveSection` | Z2T-TWOD-056 | active |

## Environment VFX and cinematics parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-ENV-001 | terrain and tile documents through `Load` | ZenithEnvironment | tool-inputs | 3 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_TerrainDocument` | Z2T-ENV-017 | naming, pending-row |
| PI-ENV-002 | foliage, placement, atmosphere, weather, water, terrain layer and cloud-noise documents | ZenithEnvironment | tool-inputs | 3 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_EnvironmentDocuments` | Z2T-ENV-032, Z2T-ENV-084 | naming, pending-row |
| PI-ENV-003 | cooked terrain tile files | ZenithEnvironment | shipped-content | 2 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_TerrainTileFile` | Z2T-ENV-043 | naming, pending-row |
| PI-ENV-004 | cooked foliage cell files | ZenithEnvironment | shipped-content | 2 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_FoliageCellFile` | Z2T-ENV-043 | naming, pending-row |
| PI-ENV-005 | the three cooked environment parameter files | ZenithEnvironment | shipped-content | 2 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_EnvironmentParameterFiles` | Z2T-ENV-043 | naming, pending-row |
| PI-ENV-006 | the cooked cloud-noise file | ZenithEnvironment | shipped-content | 2 | `Zenith2/Docs/Design/ENV/Bake.md` → "Reader validation" | `Zenith_Fuzz_Fuzz_CloudNoiseFile` | Z2T-ENV-097 | naming, pending-row |
| PI-VFX-001 | the cooked effect reader | ZenithFlux | shipped-content | 2 | `Zenith2/Docs/Design/VFX/EffectFormat.md` → "Format limits" | `Zenith_Fuzz_ParticleEffect` | Z2T-VFX-019 | active, pending-row |
| PI-VFX-002 | the effect document validator | ZenithCook | tool-inputs | 3 | `Zenith2/Docs/Design/VFX/EffectFormat.md` → "Document limits" | `Zenith_Fuzz_VFXDocument` (through `zenith_cook_fuzz_target`) | Z2T-VFX-012 | active, pending-row |
| PI-CINE-001 | the cinematic sequence loader | ZenithCinematics | shipped-content | 2 | `Zenith2/Docs/Design/CINE/Formats.md` → "Load validation" | `Zenith_Fuzz_CinematicLoader` | Z2T-CINE-024 | active, pending-row |
| PI-CINE-002 | the cooked video container loader | ZenithCinematics | shipped-content | 2 | `Zenith2/Docs/Design/CINE/Formats.md` → "Video load validation" | `Zenith_Fuzz_VideoLoader` | Z2T-CINE-024 | active, pending-row |

## Packaging and update parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-PACK-001 | the `package.zpkm` reader | ZenithPackage | shipped-content | 2 | "package manifest (package.zpkm)" | `Zenith_Fuzz_PackageManifest` | Z2T-PACK-012 | active |
| PI-PACK-002 | the install journal `install-journal.zinj` | ZenithPackage | user-files | 1 | "PACK install and delivery state" | `Zenith_Fuzz_InstallJournal` | Z2T-PACK-028 | active |
| PI-PACK-003 | the install state file `install-state.zins` | ZenithPackage | user-files | 1 | "PACK install and delivery state" | `Zenith_Fuzz_InstallState` | Z2T-PACK-028 | active |
| PI-PACK-004 | the install record `install-record.zinr` | ZenithPackage | user-files | 1 | "PACK install and delivery state" | `Zenith_Fuzz_InstallRecord` | Z2T-PACK-029 | active |
| PI-PACK-005 | the delivery state file | ZenithPackage | user-files | 1 | "PACK install and delivery state" | `Zenith_Fuzz_DeliveryState` | Z2T-PACK-037 | active |
| PI-PACK-006 | the verified stamp file `verified.zinv` | ZenithPackage | user-files | 1 | "PACK install and delivery state" | `Zenith_Fuzz_VerifiedStamps` | Z2T-PACK-034 | active |
| PI-PACK-007 | the `Package.*` event payload (recorded publications) | ZenithPackage | user-files | 1 | "Package.* event payload" | `Zenith_Fuzz_PackageEvent` | pending:PACK | active |

## Developer tool parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-DEVT-001 | the `.zprof` capture reader | ZenithProfiling | remote-tools, tool-inputs | 3 | `Zenith2/Docs/Design/DEVT/Profiler.md` → "Profiler public headers" | `Zenith_Fuzz_ProfilerCapture` | Z2T-DEVT-026 | active, pending-row |
| PI-DEVT-002 | the console tokeniser and parser | ZenithProfiling | remote-tools | 3 | `Zenith2/Docs/Design/DEVT/Console.md` → "Console grammar" | `Zenith_Fuzz_DevConsoleLine` | Z2T-DEVT-031 | active, pending-row |
| PI-DEVT-003 | the budget-run `rows.json` and `fingerprint.json` readers | ZenithProfiling | tool-inputs | 3 | `Zenith2/Docs/Design/DEVT/Evidence.md` → "Fingerprint invalidation tests" | `Zenith_Fuzz_BudgetRunRows` | Z2T-DEVT-096 | active, pending-row |
| PI-DEVT-004 | the remote protocol frame decoder | ZenithRuntime | remote-tools | 3 | `Zenith2/Docs/Design/DEVT/Console.md` → "Remote protocol" | `Zenith_Fuzz_RemoteFrame` | Z2T-DEVT-084 | active, pending-row |
| PI-DEVT-005 | the next-launch crash report finalization parser | ZenithPlatform | user-files, process-ipc | 1 | `Zenith2/Docs/Design/DEVT/Crash.md` → "Crash report schema" | `Zenith_Fuzz_CrashReport` | Z2T-DEVT-073 | active, pending-row |
| PI-GRAPH-001 | the Flux graph dump reader of the `graph` panel | ZenithRuntime | tool-inputs | 3 | "JSON (authoring documents, configuration, game.json, schemas)" | covered by PI-CORE-001 | pending:GRAPH | covered |

## Online and network parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-ONLINE-001 | the achievement and entitlement catalogue | ZenithOnline | shipped-content | 2 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineCatalogue` | pending:ONLINE | active, pending-row |
| PI-ONLINE-002 | script request decoding | ZenithOnline | scripts, user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineScriptRequest` | pending:ONLINE | active, pending-row |
| PI-ONLINE-003 | the script result decoder | ZenithOnline | user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineResult` | pending:ONLINE | active, pending-row |
| PI-ONLINE-004 | notification payloads | ZenithOnline | online | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineNotification` | pending:ONLINE | active, pending-row |
| PI-ONLINE-005 | cloud object envelopes | ZenithOnline | online, user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineEnvelope` | pending:ONLINE | active, pending-row |
| PI-ONLINE-006 | the cloud index parser and view classifier | ZenithOnline | online, user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineCloudIndex` | pending:ONLINE | active, pending-row |
| PI-ONLINE-007 | cloud object names | ZenithOnline | online | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Validation rules" | `Zenith_Fuzz_OnlineCloudObjectName` | pending:ONLINE | active, pending-row |
| PI-ONLINE-008 | the sync manifest | ZenithOnline | user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineSyncManifest` | pending:ONLINE | active, pending-row |
| PI-ONLINE-009 | the device record | ZenithOnline | user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineDeviceRecord` | pending:ONLINE | active, pending-row |
| PI-ONLINE-010 | every completion kind's payload validator | ZenithOnline | online | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineBackendPayload` | pending:ONLINE | active, pending-row |
| PI-ONLINE-011 | the offline achievement store | ZenithOnline_Offline | user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineOfflineAchievements` | pending:ONLINE | active, pending-row |
| PI-ONLINE-012 | the offline cloud object store | ZenithOnline_Offline | user-files | 1 | `Zenith2/Docs/Design/ONLINE/Formats.md` → "Format limits" | `Zenith_Fuzz_OnlineOfflineCloudObject` | pending:ONLINE | active, pending-row |
| PI-NET-001 | `Zenith_NetReadPacket` and every message decoder over a received datagram | ZenithNet | network | 1 | "network messages (Stage 3)"; `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" | `Zenith_Fuzz_NetPacket` | pending:NET | naming |
| PI-NET-002 | the SNAPSHOT fragment decoder | ZenithNet | network | 1 | "network messages (Stage 3)"; `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" | `Zenith_Fuzz_NetSnapshotFragment` | pending:NET | naming |
| PI-NET-003 | the INPUT message decoder | ZenithNet | network | 1 | "network messages (Stage 3)"; `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" | `Zenith_Fuzz_NetInput` | pending:NET | naming |
| PI-NET-004 | the network publication payload decoder (recorded publications) | ZenithNet | network, user-files | 1 | "network messages (Stage 3)"; `Zenith2/Docs/Design/NET/Loopback.md` → "Wire limits" | `Zenith_Fuzz_NetPublication` | pending:NET | naming |
| PI-NET-005 | the `Zenith.Net.Identity` save participant's `RestoreSection` (24-byte section, version 1) | ZenithNet | user-files | 1 | `Zenith2/Docs/Design/NET/Foundations.md` → "Identity across save and restore" | `Zenith_Fuzz_NetIdentitySection` | pending:NET | naming, limits-in-design |

NET's five parsers are specified by `Zenith2/Docs/Design/NET.md` → "Security assumptions" (S1 to S6, which this design confirms: `Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "The nine boundaries", B6), and NET's "Wire limits" replace the Architecture row "network messages (Stage 3)" through cross-doc item X394, so PI-NET-001 to PI-NET-004 cite that row beside NET's heading until the core-doc diff that applies X394 re-points them to NET's rows in the same change ("Inventory rules" rule 4); Stage 7's "Serialization limits" add rows when Stage 7's messages are drafted. Every NET fuzz source is named `Tests/Fuzz/Zenith_Fuzz_<Stem>.cpp` in `Zenith2/Docs/Design/NET/Tests.md`, which the generic rule would turn into `Zenith_Fuzz_Zenith_Fuzz_<Stem>`; finding F8 asks NET to name the files `Tests/Fuzz/<Stem>.cpp` (row SEC17).

## Content and authoring parsers

| ID | Parser and input | Library | Classes | Tier | Limits | Fuzz target | Owner | Status |
|---|---|---|---|---|---|---|---|---|
| PI-CONTENT-001 | the glTF source parser | ZenithContentCore | tool-inputs | 3 | "content source importers (tool-side, out of process)" | `Zenith_Fuzz_GltfSource` | Z2T-CONTENT-013 | active |
| PI-CONTENT-002 | the image source decoders | ZenithContentCore | tool-inputs | 3 | "content source importers (tool-side, out of process)" | `Zenith_Fuzz_ImageSource` | Z2T-CONTENT-013 | active |
| PI-CONTENT-003 | the audio source decoders | ZenithContentCore | tool-inputs | 3 | "content source importers (tool-side, out of process)" | `Zenith_Fuzz_AudioSource` | Z2T-CONTENT-013 | active |
| PI-CONTENT-004 | three-way merge inputs | ZenithContentCore | tool-inputs | 3 | `Zenith2/Docs/Design/CONTENT/Merge.md` → "Merge determinism and limits" | `Zenith_Fuzz_MergeInput` | Z2T-CONTENT-065 | active, pending-row |
| PI-CONTENT-005 | batch journals | ZenithContentCore | tool-inputs | 3 | `Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits" | `Zenith_Fuzz_BatchJournal` | Z2T-CONTENT-065, Z2T-CONTENT-025 | active, pending-row |
| PI-CONTENT-006 | the content validation report reader | ZenithContentCore | tool-inputs | 3 | `Zenith2/Docs/Design/CONTENT/Importers.md` → "Parser limits" | `Zenith_Fuzz_ContentValidation` | Z2T-CONTENT-065 | active, pending-row |
| PI-CHAR-001 | every CHAR document reader | ZenithAuthoring | tool-inputs | 3 | "CHAR documents (tool-side inputs, never read by a Player)" | none (mutated-corpus cases; stem `CharDocuments` requested) | pending:CHAR | gap |
| PI-WORLD-001 | every WORLD document reader | ZenithAuthoring | tool-inputs | 3 | "WORLD documents (tool-side inputs, never read by a Player)" | none (`Test_WorldDocument_MalformedCorpusNeverAsserts`; stem `WorldDocuments` requested) | pending:WORLD | gap |
| PI-EDIT-001 | clipboard documents, live-link lines, the endpoint file, per-user editor files | ZenithEditor, ZenithEditorCore | tool-inputs, remote-tools | 3 | "JSON (authoring documents, configuration, game.json, schemas)" | covered by PI-CORE-001 and PI-SCENE-002 | pending:EDIT | covered |

## Conformance findings

Each finding is a request in `C:/dev/Zenith2-drafts/findings/A3_cross_area_notes.md` (rows SEC1 to SEC14); until the owner's revision answers it, the row keeps its status and SecurityConformance reports it as a known finding (`Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Checks", SC09 to SC11, which pass a row whose status matches the finding and fail any new one).

| # | Finding | Rows | Owner | Requested change |
|---|---|---|---|---|
| F1 | fuzz sources in `ZENITH_TOOLS_HOST` targets are built only when registered through `zenith_tool_fuzz_target` or its ZenithCook wrapper `zenith_cook_fuzz_target` (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Tool-side fuzz targets"); PI-ASSET-006 to PI-ASSET-008 and PI-VFX-002 are registered through the wrapper as their tickets are written and are `active`; the rows listed here either have a source in a tool target and no registration call, or (PI-ANIM-004) a call that is not compatible as written: `Z2T-ANIM-029` lists `ZenithAuthoring` in `DEPENDS`, a `win`-only static tool library, which fails rule 6 in `win-asan` until row SEC20 and core-doc item X444 make the static tool libraries `ZENITH_WINDOWS` | PI-LIGHT-006, PI-LIGHT-007, PI-UI-010 (ZenithCook), PI-ANIM-004 (ZenithCook, depending on ZenithAuthoring), PI-ANIM-005, PI-TWOD-002 (ZenithAuthoring) | LIGHT, UI, ANIM, TWOD (row SEC4); SHIP and Architecture for the preset change (row SEC20, X444) | LIGHT and UI call `zenith_cook_fuzz_target`; ANIM and TWOD call `zenith_tool_fuzz_target(<Stem> TOOL ZenithAuthoring TOOL_DIR Tools/Authoring DEPENDS ZenithAuthoring …)` once SEC20 lands, and ANIM-029's call becomes valid unchanged; if SEC20 is refused, ANIM's and TWOD's targets move to `ZenithAnimation` and `Zenith2D`, and ANIM-029 drops `ZenithAuthoring` from `DEPENDS` |
| F2 | stems carrying a `Fuzz_` or `Zenith_` prefix produce target names `Zenith_Fuzz_Fuzz_<X>` and `Zenith_Fuzz_Zenith_NavFuzz` | PI-UI-001 to PI-UI-010, PI-ENV-001 to PI-ENV-006, PI-AI-001 to PI-AI-003 | UI, ENV, AI (row SEC4) | rename to the stem rule (UI: `UIDocumentJson`, `UICookedDocument`, `UICookedFont`, `UICookedLocalization`, `UIMessageSyntax`, `UIPluralRule`, `UIPublication`, `UITextLayout`, `UIBindingManifest`, `UIXliff`; ENV: `TerrainDocument`, `EnvironmentDocuments`, `TerrainTileFile`, `FoliageCellFile`, `EnvironmentParameterFiles`, `CloudNoiseFile`; AI: one source per reader, `NavTileSection`, `NavVolumeSection`, `NavLayersSection`, `NavSourceIntermediate`, `NavSaveSection`) |
| F3 | parsers of external bytes with no fuzz target | PI-RHI-001, PI-PHYS-002, PI-PHYS-004, PI-AI-005, PI-AI-006, PI-MOD-001 | RHI, PHYS, AI, MOD (rows SEC4, SEC5, SEC11) | add the requested stem in the owner's next revision |
| F4 | tool-only document readers exercised by mutated-corpus cases only (no ASan) | PI-CHAR-001, PI-WORLD-001, PI-PHYS-003 | CHAR, WORLD, PHYS (row SEC9) | adopt `zenith_tool_fuzz_target` with `TOOL ZenithAuthoring TOOL_DIR Tools/Authoring DEPENDS ZenithAuthoring` once row SEC20 lands (PHYS's intermediates through `zenith_cook_fuzz_target`); until then residual R5 (`Zenith2/Docs/Design/SEC/TrustBoundaries.md` → "Accepted residual risks") |
| F5 | formats missing from the Architecture table "Parser limits" | every row marked `pending-row` | Architecture (core-doc rows X413 and, for NET, X394) | one row per format citing the owner's heading, in the form of the "schema blobs" row; a `limits-in-design` row is not a finding |
| F6 | DEVT, CINE and ADVGFX write and read `user:/devtools/` (DEVT's `asset.dump`, telemetry, captures and evidence; CINE's captures; ADVGFX's `r.vt.dump`) and DEVT reads `user:/crash/`, which rule R10 rejects | PI-DEVT-005 and the development file writer | PLAT, DEVT, CINE, ADVGFX (row SEC1) | R10's allowed first components gain `crash` (every build) and `devtools` (non-Shipping only) |
| F7 | ANIM states one hour for all five of its targets at Stage 6; the tier table of `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns" governs every fuzz duration, and gives PI-ANIM-001 to PI-ANIM-003 tier 2 and PI-ANIM-004 and PI-ANIM-005 tier 3 | PI-ANIM-001 to PI-ANIM-005 | ANIM (row SEC4) | ANIM's "Fuzz targets" cites its rows' tiers instead of a number |
| F8 | NET's fuzz sources are named `Tests/Fuzz/Zenith_Fuzz_<Stem>.cpp`, which produces target names `Zenith_Fuzz_Zenith_Fuzz_<Stem>` | PI-NET-001 to PI-NET-005 | NET (row SEC17) | name the files `Tests/Fuzz/NetPacket.cpp`, `NetSnapshotFragment.cpp`, `NetInput.cpp`, `NetPublication.cpp`, `NetIdentitySection.cpp`; the target names NET states are unchanged |

## Tier totals and campaign estimate

Counted per fuzz executable, not per row: an executable that drives several rows takes the lowest tier number among them (`Zenith_Fuzz_StreamingWorld` drives PI-STREAM-001 and PI-STREAM-002 and is tier 1), finding F2's split gives the AI rows five executables, and rows with status `covered` or `outside-scope`, and PI-PHYS-003 (no requested stem), run none. Statuses `active`, `property`, `naming`, `dead-target` and `gap` with a requested stem are counted, because each is a target once its finding closes:

| Tier | Targets | Stage 2 to 5 seconds each | Stage 2 to 5 total | Stage 6 seconds each | Stage 6 total |
|---|---|---|---|---|---|
| 1 | 50 | 600 | 8.3 h | 3,600 | 50.0 h |
| 2 | 47 | 180 | 2.4 h | 1,800 | 23.5 h |
| 3 | 36 | 60 | 0.6 h | 600 | 6.0 h |
| total | 133 | | 11.3 h | | 79.5 h |

The totals are wall time with each target run by `-jobs=8 -workers=8` and the targets one after another, on the reference desktop. They are upper bounds over the full inventory: a stage acceptance runs only the targets present in the tree at that stage, so the Stage 2 campaign is far shorter; the campaign is a `needs-human` acceptance step, not a gate (`Zenith2/Docs/Design/SEC/Fuzzing.md` → "Campaign wall-time estimate", and the pending budget rows of `Zenith2/Docs/Design/SEC.md` → "Pending new rows").
