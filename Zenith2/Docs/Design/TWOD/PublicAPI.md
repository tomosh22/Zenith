# TWOD design: public API

Section file of `Zenith2/Docs/Design/TWOD.md`. It declares every public header of the `Zenith2D` module and the TWOD-owned public header of ZenithFlux, `Flux_Sprite2D.h`, with their full declarations and one contract block per type. Algorithms that the declarations name are specified in `Zenith2/Docs/Design/TWOD/Simulation.md` and `Zenith2/Docs/Design/TWOD/Rendering.md`; formats in `Zenith2/Docs/Design/TWOD/Formats.md`.

## Scope of this file

| Topic | Heading |
|---|---|
| which header declares what, and its first stage | "Header list" |
| the two module entry points | "Zenith_Module_2D header" |
| enumerations, constants, the error record | "Zenith_TwoDTypes header" |
| the sprite and sort-group components | "Zenith_Sprite2DComponents header" |
| the pixel-perfect camera component and its canvas helpers | "Zenith_Camera2D header" |
| the sprite atlas asset type | "Zenith_SpriteAtlas header" |
| sprite animation, the animator component and asset | "Zenith_Sprite2DAnimation header" |
| cutout skeletons, the rig asset and the skeleton component | "Zenith_Skeleton2D header" |
| tilesets, tilemaps, their component and edit commands | "Zenith_Tilemap header" |
| 2D physics components, the physics world resource and queries | "Zenith_Physics2D header" |
| the events TWOD emits | "Zenith_TwoDEvents header" |
| the functions Luau reaches | "Zenith_TwoDScriptApi header" |
| the ZenithFlux side: render-world records, the renderer, feature functions | "Flux_Sprite2D header" |

Every fallible function in every header below returns `Zenith_Result<T, E>`; every public type carries one contract block of `Zenith2/Docs/Contracts.md → "Contract blocks per type"`; no header includes a third-party or `Internal/` header or declares an STL type ([Z2-N-CONTRACT-026], [Z2-N-CONTRACT-043], [Z2-N-CONTRACT-272]), and the only standard headers included are `<cstddef>` and `<type_traits>` of the allowed thirteen ([Z2-N-CONTRACT-271]), for the payloads' layout assertions. Component structs are C++-registered types with fixed property IDs (the mechanism of `Zenith2/Docs/Design/RPROD/Extractors.md → "Flux_RenderComponents header"`), registered by `Zenith_Module_2D_RegisterTypes`; their stable names never change and their property IDs are never reused ([Z2-N-CONTRACT-374]).

## Header list

| Header (root of `Zenith2/Engine/Zenith2D/` unless stated) | Declares | First stage |
|---|---|---|
| `Zenith_Module_2D.h` | `Zenith_Module_2D_RegisterTypes`, `Zenith_Module_2D_Start` | S3 (API slice S2 pull-forward) |
| `Zenith_TwoDTypes.h` | `Zenith_TwoDErrorCode`, `Zenith_TwoDError`, `Zenith_Sprite2DSpace`, `Zenith_Sprite2DBlend`, `Zenith_Sprite2DDrawMode`, the limits constants, `Zenith_ToString`, `Zenith_TwoDCommandKinds`, `Zenith_TileCellRef`, `Zenith_MakeTileCellKey`, `Zenith_TileCellRefFromKey` | S3 (API slice S2) |
| `Zenith_Sprite2DComponents.h` | `Zenith_SpriteComponent`, `Zenith_SortGroupComponent` | S3 |
| `Zenith_Camera2D.h` | `Zenith_Camera2DScaleMode`, `Zenith_Camera2DSnapMode`, `Zenith_Camera2DComponent`, `uTWOD_PURE_2D_FEATURE_CLEAR_MASK`, `Zenith_Camera2D_WorldToReference`, `Zenith_Camera2D_ReferenceToWorld` | S3 |
| `Zenith_SpriteAtlas.h` | `Zenith_SpriteAtlasAsset`, `Zenith_SpriteFrameView`, `Zenith_SpriteAtlasFilter` | S3 |
| `Zenith_Sprite2DAnimation.h` | `Zenith_SpriteAnimatorComponent`, `Zenith_AnimatorParameter2D`, `Zenith_SpriteAnimationAsset`, `Zenith_Animation2DParameterKind`, `Zenith_Animation2DConditionOp`, `Zenith_AnimatorPlayPayload`, `Zenith_AnimatorParameterPayload`, `Zenith_RecordAnimatorPlay`, `Zenith_RecordAnimatorSetParameter` | S4 (API slice S3) |
| `Zenith_Skeleton2D.h` | `Zenith_Skeleton2DComponent`, `Zenith_Rig2DAsset`, `Zenith_Bone2DPose`, `Zenith_Affine2D`, `Zenith_Slot2DPose`, `Zenith_Skeleton2DPoseView`, `Zenith_Skeleton2DPoseResource` | S4 |
| `Zenith_Tilemap.h` | `Zenith_TilemapComponent`, `Zenith_TilesetAsset`, `Zenith_TilemapAsset`, `Zenith_TileCell`, `Zenith_SetTilePayload`, `Zenith_FillTilesPayload`, `Zenith_RecordSetTile`, `Zenith_RecordFillTiles`, `Zenith_TilemapEditResource` | S4 (API slice S3) |
| `Zenith_Physics2D.h` | `Zenith_Body2DType`, `Zenith_Shape2DKind`, `Zenith_Joint2DKind`, `Zenith_RigidBody2DComponent`, `Zenith_Collider2DComponent`, `Zenith_Joint2DComponent`, `Zenith_Physics2DErrorCode`, `Zenith_Physics2DError`, `Zenith_RayCast2DHit`, `Zenith_Overlap2DHit`, `Zenith_Physics2DQueryFilter`, `Zenith_Physics2DWorldResource`, `Zenith_Body2DCommandPayload`, `Zenith_RecordBody2DCommand` | S3 (API slice S2 pull-forward) |
| `Zenith_TwoDEvents.h` | `Zenith_Animation2DEvent`, `Zenith_Animation2DFinishedEvent`, `Zenith_Contact2DBeganEvent`, `Zenith_Contact2DEndedEvent`, `Zenith_Trigger2DEnteredEvent`, `Zenith_Trigger2DExitedEvent`, `Zenith_Joint2DBrokenEvent` | S3 (animation events S4) |
| `Zenith_TwoDScriptApi.h` | `Zenith_TwoDRayHit`, `Zenith_TwoDOverlapResult`, `Zenith_TwoDScriptApi` (the static functions the `TwoD` Luau table binds), `Zenith_RegisterTwoDScriptBindings` | S3 (animation and tilemap functions S4) |
| `Zenith2/Engine/ZenithFlux/Flux_Sprite2D.h` | the render-world record types, `Flux_Sprite2DCanvasGeometry`, `Flux_ComputeCanvasGeometry`, `Flux_Sprite2DRendererDesc`, `Flux_Sprite2DRenderer`, `Flux_Sprite2DFrame`, `Flux_Sprite2DError`, the feature functions | S3 (API slice S2 pull-forward, no-pass definitions) |

## Zenith_Module_2D header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Module_2D.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module2DEntryPoints
/// Ownership: Called by the generated composition of a game whose game.json selects the 2D module; Start returns the module
///   instance as a Zenith_Owned that ZenithRuntime owns until shutdown step 6.
/// Thread affinity: MAIN_THREAD. RegisterTypes at startup step 6, before Freeze; Start at startup step 15.
/// Invalidation: None.
/// Units: None.
/// Errors: RegisterTypes returns MODULE_ERROR_REGISTRATION when any type, property or configuration-key registration is
///   rejected (the registry logs which); Start returns MODULE_ERROR_OUT_OF_MEMORY or MODULE_ERROR_REGISTRATION after
///   unwinding every completed step in reverse ([Z2-N-CONTRACT-340]).
/// Version: since Z2 Stage 3. The Start signature takes no backend factory under either outcome of D-SEL-2DPHYS
///   (open, Stage 2): Box2D is Zenith2D's own, and under the Jolt outcome the 2D world is the Physics module's planar
///   world resource (Zenith2/Docs/Design/TWOD/Simulation.md "Candidate J production realisation").
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_2D_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_2D_Start(
	Zenith_ModuleStartContext& xContext);
```

`Zenith_Module_2D_RegisterTypes` registers, in this order: the nine component types of this file (stable names in "Data structures and formats" of the entry document), the seven event types of "Zenith_TwoDEvents header", the five payload structs (`Zenith.TwoD.SetTilePayload` 24 bytes, `Zenith.TwoD.FillTilesPayload` 32, `Zenith.TwoD.AnimatorPlayPayload` 24, `Zenith.TwoD.AnimatorParameterPayload` 24, `Zenith.TwoD.Body2DCommandPayload` 32, the sizes their `static_assert`s pin), the three script value structs (`Zenith.TwoD.TileCellRef`, `Zenith.TwoD.RayHit`, `Zenith.TwoD.OverlapResult`), the element struct `Zenith.TwoD.AnimatorParameter2D` (the element type of `Zenith.TwoD.SpriteAnimator` property 9, which rule 8 of `Zenith2/Docs/Design/ECS/Reflect.md → "Freeze validation"` requires registered), the three world resources (`Zenith.TwoD.Physics`, `Zenith.TwoD.Poses`, `Zenith.TwoD.TilemapEdits`) and the configuration keys of `Zenith2/Docs/Design/TWOD.md → "Configuration keys"` through `Zenith_TypeRegistry::GetConfigSchema()`. The asset types (`TYPE_KIND_ASSET`) are registered by ZenithAssets' `Zenith_RegisterAssetTypes` from the kind table once ASSET adds TWOD's rows (cross-area row TD3).

Every one of those registrations calls `ScriptName` with its explicit script name, never relying on ECS's default (the stable name's last segment, `Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`): the components take the "Script name" column of `Zenith2/Docs/Design/TWOD.md → "Component table"` and the other types the script-name table of `Zenith2/Docs/Design/TWOD.md → "World resources events and commands"`, each Bindings rule 1 applied to the C++ type name (orchestrator ruling X353-SCRIPTNAMES). For example `xTypes.BeginComponent<Zenith_Camera2DComponent>("Zenith.TwoD.Camera", 1).DefaultAuthority(PHASE_LATE_SCRIPTS).ScriptName("Camera2D")` and `xTypes.BeginType<Zenith_Contact2DBeganEvent>(TYPE_KIND_EVENT, "Zenith.TwoD.ContactBegan", 1).ScriptName("Contact2DBeganEvent")`, followed by the type's `Property` calls and `End`. A script-name clash is reported by `Freeze` (`REFLECT_ERROR_BAD_NAME`), which `Test_TwoDIntegration_RegistryFreezesWithEngineTypes` exercises beside the engine's own types.

## Zenith_TwoDTypes header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_TwoDTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_TwoDErrorCode
/// Ownership: Plain enumeration; the code of Zenith_TwoDError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator's meaning is the comment beside it; an enumerator once shipped keeps its meaning ([Z2-N-CONTRACT-467]).
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_TwoDErrorCode : uint8
{
	TWOD_ERROR_OUT_OF_MEMORY = 1,           // a Zenith_TryNew or reservation of the module failed
	TWOD_ERROR_INVALID_DESC = 2,            // a configuration value outside its domain after schema validation
	TWOD_ERROR_CORRUPT_ASSET = 3,           // a cooked TWOD file failed a kind-specific check (m_uDetail: the check number of Formats.md)
	TWOD_ERROR_VERSION_MISMATCH = 4,        // a cooked TWOD file's format version differs from the build's
	TWOD_ERROR_UNKNOWN_SPRITE = 5,          // a sprite name the atlas does not contain
	TWOD_ERROR_UNKNOWN_STATE = 6,           // an animator state name the animation set does not contain
	TWOD_ERROR_UNKNOWN_PARAMETER = 7,       // an animator parameter name the animation set does not declare
	TWOD_ERROR_UNKNOWN_BONE = 8,            // a bone name the rig does not contain
	TWOD_ERROR_CELL_OUT_OF_RANGE = 9,       // a tile coordinate outside the map's declared bounds
	TWOD_ERROR_TILE_OUT_OF_RANGE = 10,      // a tile index above the tileset's tile count
	TWOD_ERROR_LAYER_OUT_OF_RANGE = 11,     // a tilemap layer index at or above the map's layer count
	TWOD_ERROR_NOT_RESIDENT = 12,           // the asset an operation reads is not ACTIVE yet
	TWOD_ERROR_CAPACITY = 13,               // a per-world capacity of "Limits" is exhausted (m_uDetail: the limit's row number)
	TWOD_ERROR_STALE_ENTITY = 14,           // an entity handle that no longer resolves
	TWOD_ERROR_MISSING_COMPONENT = 15,      // the entity lacks the component the operation needs
	TWOD_ERROR_PHYSICS = 16,                // a 2D physics call failed; m_uDetail carries the Zenith_Physics2DErrorCode
};

/// @contract value Zenith_TwoDError
/// Ownership: Plain value; the struct form of Zenith2/Docs/Contracts.md "Errors and results" (8 bytes, no text; the text is
///   logged under category twod at the point of failure).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uDetail as the code's comment states.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TwoDError
{
	const Zenith_TwoDErrorCode m_eCode = TWOD_ERROR_OUT_OF_MEMORY;
	const uint8 m_uReserved = 0;
	const uint16 m_uDetail = 0;
	const uint32 m_uSimulationIDLow = 0;         // the low 32 bits of the entity's stable simulation ID, 0 when none
};
static_assert(sizeof(Zenith_TwoDError) == 8);

constexpr const char* Zenith_ToString(Zenith_TwoDErrorCode eCode);   // "TWOD_ERROR_OUT_OF_MEMORY", …

/// @contract enum Zenith_Sprite2DSpace
/// Ownership: Plain enumeration; Zenith_SpriteComponent property 11, Zenith_TilemapComponent property 7.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; any other stored value draws as SPRITE2D_SPACE_CANVAS and counts twod.render.bad_enum_delta.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Sprite2DSpace : uint8
{
	SPRITE2D_SPACE_CANVAS = 0,              // drawn on the pixel canvas of a 2D camera; in a view without a canvas, drawn as WORLD
	SPRITE2D_SPACE_WORLD = 1,               // drawn into the scene colour of every view at FLUX_EXTENSION_POINT_TRANSPARENT, depth-tested
};

/// @contract enum Zenith_Sprite2DBlend
/// Ownership: Plain enumeration; Zenith_SpriteComponent property 10.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value draws as SPRITE2D_BLEND_ALPHA and counts twod.render.bad_enum_delta.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Sprite2DBlend : uint8
{
	// The blend table of Rendering.md "Sprite2DCanvas pass"; c, a: the texel's straight colour times the tint and its alpha
	// times the tint's; d: the premultiplied canvas colour; d.a: the canvas coverage.
	SPRITE2D_BLEND_ALPHA = 0,               // dst = c * a + d * (1 - a); coverage a + d.a * (1 - a)
	SPRITE2D_BLEND_PREMULTIPLIED = 1,       // texel already premultiplied (c * a stored): dst = c * a + d * (1 - a); coverage as ALPHA
	SPRITE2D_BLEND_ADDITIVE = 2,            // dst = c * a + d; coverage unchanged
	SPRITE2D_BLEND_MULTIPLY = 3,            // dst = d * lerp(1, c, a); coverage unchanged
};

/// @contract enum Zenith_Sprite2DDrawMode
/// Ownership: Plain enumeration; Zenith_SpriteComponent property 8.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; SLICED on a frame without a nine-slice border, and an unknown value, draw as SIMPLE.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Sprite2DDrawMode : uint8
{
	SPRITE2D_DRAW_MODE_SIMPLE = 0,          // one quad of the frame's own size
	SPRITE2D_DRAW_MODE_SLICED = 1,          // nine quads stretched to m_xSize, corners unscaled
	SPRITE2D_DRAW_MODE_TILED = 2,           // the frame repeated to fill m_xSize, at most uTWOD_MAX_TILED_QUADS quads
};

/// @contract functions Zenith_TwoDLimits
/// Ownership: constexpr data.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: As each name states.
/// Errors: None.
/// Version: since Z2 Stage 3. A limit changes only with the "Parser limits" table of Formats.md in the same diff.
/// @endcontract
inline constexpr uint32 uTWOD_MAX_SORTING_LAYERS = 32;
inline constexpr uint32 uTWOD_MAX_TILED_QUADS = 1024;
inline constexpr uint32 uTWOD_MAX_ATLAS_PAGE_EXTENT = 4096;
inline constexpr uint32 uTWOD_MAX_REFERENCE_EXTENT = 4096;
inline constexpr uint32 uTWOD_MAX_CANVAS_EXTENT = 4096;
inline constexpr float fTWOD_MIN_PIXELS_PER_UNIT = 1.0f;
inline constexpr float fTWOD_MAX_PIXELS_PER_UNIT = 1024.0f;
inline constexpr uint32 uTWOD_TILE_CHUNK_EXTENT = 32;                 // cells per chunk side
inline constexpr uint32 uTWOD_TILE_CHUNK_CELLS = 1024;                // 32 x 32
inline constexpr uint32 uTWOD_MAX_TILEMAP_LAYERS = 16;
inline constexpr uint32 uTWOD_MAX_TILEMAP_CHUNKS = 16384;             // per tilemap, every layer together
inline constexpr uint32 uTWOD_MAX_TILES_PER_TILESET = 65535;
inline constexpr uint32 uTWOD_MAX_ANIMATOR_PARAMETERS = 16;
inline constexpr uint32 uTWOD_MAX_ANIMATOR_STATES = 64;
inline constexpr uint32 uTWOD_MAX_ANIMATOR_TRANSITIONS = 256;
inline constexpr uint32 uTWOD_MAX_CLIP_FRAMES = 1024;
inline constexpr uint32 uTWOD_MAX_RIG_BONES = 128;
inline constexpr uint32 uTWOD_MAX_RIG_SLOTS = 256;
inline constexpr uint32 uTWOD_MAX_POLYGON_VERTICES = 8;
inline constexpr uint32 uTWOD_MAX_QUERY_HITS = 64;
inline constexpr uint32 uTWOD_MAX_PHYSICS_LAYERS = 32;

/// @contract value Zenith_TwoDCommandKinds
/// Ownership: Plain value: the command-kind IDs the module resolved at its first AttachWorld through
///   Zenith_WorldAttachContext::FindCommandKind (TWOD.md "World attach and detach"; pending ARCH, row TD23).
/// Thread affinity: IMMUTABLE after the first AttachWorld; members non-const: the module fills it at AttachWorld.
/// Invalidation: None while the module is started.
/// Units: None.
/// Errors: None; a zero ID means the kind is not resolved (the module is not started): every recorder then records nothing,
///   returns success and counts twod.command.unresolved_kind_delta.
/// Version: since Z2 Stage 3. One member per command kind of TWOD.md "World resources events and commands", in that order.
/// @endcontract
struct Zenith_TwoDCommandKinds
{
	uint32 m_uSetTile = 0;                 // "TwoD.SetTile"
	uint32 m_uFillTiles = 0;               // "TwoD.FillTiles"
	uint32 m_uAnimatorPlay = 0;            // "TwoD.AnimatorPlay"
	uint32 m_uAnimatorSetParameter = 0;    // "TwoD.AnimatorSetParameter"
	uint32 m_uBody2D = 0;                  // "TwoD.Body2D"
};
static_assert(sizeof(Zenith_TwoDCommandKinds) == 20);

/// @contract value Zenith_TileCellRef
/// Ownership: Plain value; registered struct Zenith.TwoD.TileCellRef, the Cell field of query results.
/// Thread affinity: IMMUTABLE; members non-const: a registered struct its containing component, payload or result writes
///   field by field.
/// Invalidation: None.
/// Units: Cells of the map's layer.
/// Errors: None; m_bValid false means the hit is not a tile (the other members are then zero).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TileCellRef                   // 12 bytes
{
	sint32 m_iX = 0;                       // 1 X
	sint32 m_iY = 0;                       // 2 Y
	uint8 m_uLayer = 0;                    // 3 Layer, 0 … 15
	bool m_bValid = false;                  // 4 Valid
	uint16 m_uReserved = 0;
};
static_assert(sizeof(Zenith_TileCellRef) == 12);

/// @contract functions Zenith_TileCellKeys
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Cells.
/// Errors: None; ZENITH_ASSERT(uLayer < 16, iX and iY in [−2^20, 3 × 2^20)): callers pass cells of authored bounds only
///   (Formats.md "Tile cell encoding").
/// Version: since Z2 Stage 3. The 64-bit key is saved and compared; its layout never changes within a section version.
/// @endcontract
// (1 << 63) | (layer << 44) | ((x + 2^20) << 22) | (y + 2^20); 0 is never a key.
constexpr uint64 Zenith_MakeTileCellKey(uint32 uLayer, sint32 iX, sint32 iY);
constexpr Zenith_TileCellRef Zenith_TileCellRefFromKey(uint64 ulKey);   // m_bValid false for 0
```

The tile-cell value type is declared in "Zenith_Tilemap header"; the bit layout of a cell is shared with the GPU (Formats.md "Tile cell encoding").

## Zenith_Sprite2DComponents header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Sprite2DComponents.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "Zenith2D/Zenith_TwoDTypes.h"
#include "Zenith2D/Zenith_SpriteAtlas.h"         // Zenith_SpriteAtlasAsset complete: Zenith_AssetHandle checks the kind

/// @contract value Zenith_SpriteComponent
/// Ownership: Plain component value stored by the world's ECS backend as "Zenith.TwoD.Sprite"; m_xAtlas holds one reference,
///   copied and destroyed through the type's lifecycle hooks ([Z2-N-ARCH-645]).
/// Thread affinity: As every component: written by phase PHASE_LATE_SCRIPTS directly and by commands at sync points; read by
///   TwoD.Extract in phase 12.
/// Invalidation: None.
/// Units: m_xSize in metres; m_iOrder unitless; m_xTint linear Rec.709 with straight alpha (Zenith_Colour).
/// Errors: None; an unresolvable atlas or an unknown sprite name draws nothing and counts twod.render.missing_sprite_delta.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_SpriteComponent
{
	Zenith_AssetHandle<Zenith_SpriteAtlasAsset> m_xAtlas;   // 1  Atlas (asset TwoD.SpriteAtlas)
	Zenith_StringID m_xSprite;                               // 2  Sprite: the frame name in the atlas
	Zenith_Colour m_xTint = { 1.0f, 1.0f, 1.0f, 1.0f };      // 3  Tint
	bool m_bFlipX = false;                                   // 4  FlipX
	bool m_bFlipY = false;                                   // 5  FlipY
	uint8 m_uLayer = 0;                                     // 6  Layer: sorting layer index, 0 … uTWOD_MAX_SORTING_LAYERS − 1
	sint16 m_iOrder = 0;                                    // 7  Order in layer
	uint8 m_uDrawMode = SPRITE2D_DRAW_MODE_SIMPLE;          // 8  DrawMode (Zenith_Sprite2DDrawMode)
	Zenith_Vector2 m_xSize = { 1.0f, 1.0f };                 // 9  Size: SLICED and TILED extent in metres
	uint8 m_uBlend = SPRITE2D_BLEND_ALPHA;                  // 10 Blend (Zenith_Sprite2DBlend)
	uint8 m_uSpace = SPRITE2D_SPACE_CANVAS;                 // 11 Space (Zenith_Sprite2DSpace)
	bool m_bVisible = true;                                  // 12 Visible
	bool m_bPixelSnap = true;                                // 13 PixelSnap: snap to canvas texels on a snapping camera
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static constexpr Zenith_PropertyID uPROP_ATLAS{ 1 };
	static constexpr Zenith_PropertyID uPROP_SPRITE{ 2 };
	static constexpr Zenith_PropertyID uPROP_TINT{ 3 };
	static constexpr Zenith_PropertyID uPROP_FLIP_X{ 4 };
	static constexpr Zenith_PropertyID uPROP_FLIP_Y{ 5 };
	static constexpr Zenith_PropertyID uPROP_LAYER{ 6 };
	static constexpr Zenith_PropertyID uPROP_ORDER{ 7 };
	static constexpr Zenith_PropertyID uPROP_DRAW_MODE{ 8 };
	static constexpr Zenith_PropertyID uPROP_SIZE{ 9 };
	static constexpr Zenith_PropertyID uPROP_BLEND{ 10 };
	static constexpr Zenith_PropertyID uPROP_SPACE{ 11 };
	static constexpr Zenith_PropertyID uPROP_VISIBLE{ 12 };
	static constexpr Zenith_PropertyID uPROP_PIXEL_SNAP{ 13 };
};

/// @contract value Zenith_SortGroupComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.SortGroup".
/// Thread affinity: As every component: PHASE_LATE_SCRIPTS authority; read by TwoD.Extract.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a group nested inside another group is flattened into the outermost one (Rendering.md "Sort keys").
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_SortGroupComponent
{
	uint8 m_uLayer = 0;                                     // 1 Layer
	sint16 m_iOrder = 0;                                    // 2 Order
	bool m_bEnabled = true;                                  // 3 Enabled
	static constexpr Zenith_PropertyID uPROP_LAYER{ 1 };
	static constexpr Zenith_PropertyID uPROP_ORDER{ 2 };
	static constexpr Zenith_PropertyID uPROP_ENABLED{ 3 };
};
```

| Stable name | Property: ID, name, type, default, range | Authority | Serialized |
|---|---|---|---|
| `Zenith.TwoD.Sprite` | 1 Atlas ASSET `TwoD.SpriteAtlas` null; 2 Sprite STRING_ID empty; 3 Tint COLOUR (1,1,1,1); 4 FlipX BOOL false; 5 FlipY BOOL false; 6 Layer U8 0, 0 … 31; 7 Order I16 0; 8 DrawMode U8 0, 0 … 2; 9 Size VEC2 (1,1), each > 0 … 10,000; 10 Blend U8 0, 0 … 3; 11 Space U8 0, 0 … 1; 12 Visible BOOL true; 13 PixelSnap BOOL true | `PHASE_LATE_SCRIPTS` for 1–13 | yes |
| `Zenith.TwoD.SortGroup` | 1 Layer U8 0, 0 … 31; 2 Order I16 0; 3 Enabled BOOL true | `PHASE_LATE_SCRIPTS` | yes |

The sorting layer names are the project configuration key `twod.sortingLayers` (an array of at most 32 names, index = layer, default `["Default"]`); `Layer` stores the index, and the editor shows the name. A layer index at or above the array's length draws in the last configured layer and counts `twod.render.bad_layer_delta`.

## Zenith_Camera2D header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Camera2D.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"

/// @contract enum Zenith_Camera2DScaleMode
/// Ownership: Plain enumeration; Zenith_Camera2DComponent property 4; equal in value to Flux_Sprite2DScaleMode.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value behaves as CAMERA2D_SCALE_MODE_INTEGER_LETTERBOX.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Camera2DScaleMode : uint8
{
	CAMERA2D_SCALE_MODE_INTEGER_LETTERBOX = 0,   // canvas = reference extent, integer scale, centred, bars outside
	CAMERA2D_SCALE_MODE_INTEGER_EXPAND = 1,      // integer scale from the reference extent; the canvas grows to fill the output
	CAMERA2D_SCALE_MODE_NATIVE = 2,              // canvas = output extent; world-to-pixel scale = PPU × outputHeight / referenceHeight
};

/// @contract enum Zenith_Camera2DSnapMode
/// Ownership: Plain enumeration; Zenith_Camera2DComponent property 5; equal in value to Flux_Sprite2DSnapMode.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value behaves as CAMERA2D_SNAP_MODE_TEXEL.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Camera2DSnapMode : uint8
{
	CAMERA2D_SNAP_MODE_NONE = 0,                 // camera and sprites at fractional canvas positions
	CAMERA2D_SNAP_MODE_TEXEL = 1,                // camera and snapping sprites rounded to whole canvas texels
	CAMERA2D_SNAP_MODE_SUBPIXEL = 2,             // as TEXEL, plus the camera's remainder applied as a whole-output-pixel composite offset
};

/// @contract value Zenith_Camera2DComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.Camera"; read on the entity that RPROD's Flux_SelectMainCamera
///   selects as the world's main camera (Zenith2/Docs/Design/RPROD/Extractors.md "Flux_RenderComponents header").
/// Thread affinity: As every component: PHASE_LATE_SCRIPTS authority; read by TwoD.Extract in phase 12.
/// Invalidation: None.
/// Units: Reference extent in canvas pixels; m_fPixelsPerUnit in canvas pixels per metre; bounds in metres; colours linear.
/// Errors: None; out-of-range values were clamped by the property ranges; a camera entity without Zenith.Render.Camera has no
///   view, so its 2D camera produces no canvas (twod.canvas.no_render_camera_delta).
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_Camera2DComponent
{
	uint32 m_uReferenceWidth = 320;                         // 1  ReferenceWidth, 16 … 4,096
	uint32 m_uReferenceHeight = 180;                        // 2  ReferenceHeight, 16 … 4,096
	float m_fPixelsPerUnit = 16.0f;                          // 3  PixelsPerUnit, 1 … 1,024
	uint8 m_uScaleMode = CAMERA2D_SCALE_MODE_INTEGER_LETTERBOX;   // 4 ScaleMode
	uint8 m_uSnapMode = CAMERA2D_SNAP_MODE_TEXEL;           // 5  SnapMode
	Zenith_Colour m_xLetterboxColour = { 0.0f, 0.0f, 0.0f, 1.0f };   // 6 LetterboxColour
	Zenith_Colour m_xClearColour = { 0.0f, 0.0f, 0.0f, 0.0f };       // 7 ClearColour: the canvas clear (alpha 0: transparent over 3D)
	bool m_bEnabled = true;                                  // 8  Enabled: false removes the canvas; canvas sprites then draw as WORLD
	bool m_bBoundsEnabled = false;                           // 9  BoundsEnabled
	Zenith_Vector2 m_xBoundsMin = { -1000.0f, -1000.0f };    // 10 BoundsMin
	Zenith_Vector2 m_xBoundsMax = { 1000.0f, 1000.0f };      // 11 BoundsMax
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_REFERENCE_WIDTH{ 1 } … uPROP_BOUNDS_MAX{ 11 }, one constant per property in declaration order.
};

// The Flux_ViewFeature bits a pure 2D main camera clears through Zenith.Render.Camera's FeatureMaskClear (property 9): every
// 3D pass bit (HIZ_OCCLUSION, SHADOW_RECEIVE, SSAO, CONTACT_SHADOWS, VOLUMETRIC_FOG, SSR, TRANSPARENT, PARTICLES, TEMPORAL_AA,
// AUTO_EXPOSURE, DEPTH_OF_FIELD, MOTION_BLUR, BLOOM, TONEMAP, SKY), keeping UI, DEBUG_OVERLAY, EDITOR_OVERLAY and COLOUR.
inline constexpr uint32 uTWOD_PURE_2D_FEATURE_CLEAR_MASK = 0x00023FFFu;

/// @contract functions Zenith_Camera2DMapping
/// Ownership: Pure functions over plain values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: World positions in metres; reference coordinates in reference pixels, origin at the top-left of the reference
///   rectangle, +Y down.
/// Errors: None; ZENITH_ASSERT(m_fPixelsPerUnit > 0) (the property range guarantees it).
/// Version: since Z2 Stage 3. Deterministic: binary32 arithmetic in the order written, so simulation may call them.
/// @endcontract
// x_ref = (x − cx) × PPU + W_ref / 2; y_ref = (cy − y) × PPU + H_ref / 2, with (cx, cy) the camera's world position, unsnapped.
Zenith_Vector2 Zenith_Camera2D_WorldToReference(const Zenith_Camera2DComponent& xCamera,
	Zenith_Vector2 xCameraPosition, Zenith_Vector2 xWorld);
Zenith_Vector2 Zenith_Camera2D_ReferenceToWorld(const Zenith_Camera2DComponent& xCamera,
	Zenith_Vector2 xCameraPosition, Zenith_Vector2 xReference);
```

| Stable name | Properties | Authority | Serialized |
|---|---|---|---|
| `Zenith.TwoD.Camera` | 1 ReferenceWidth U32 320, 16 … 4,096; 2 ReferenceHeight U32 180, 16 … 4,096; 3 PixelsPerUnit F32 16, 1 … 1,024; 4 ScaleMode U8 0, 0 … 2; 5 SnapMode U8 1, 0 … 2; 6 LetterboxColour COLOUR (0,0,0,1); 7 ClearColour COLOUR (0,0,0,0); 8 Enabled BOOL true; 9 BoundsEnabled BOOL false; 10 BoundsMin VEC2 (−1000, −1000); 11 BoundsMax VEC2 (1000, 1000) | `PHASE_LATE_SCRIPTS` | yes |

- The two mapping functions use the reference rectangle only, never the output size, so a script that maps a recorded pointer position (normalised to the reference rectangle) to the world gets the same bits live and in replay; the canvas actually drawn depends on the output extent and is presentation alone (Rendering.md "Canvas geometry").
- The 2D camera never writes `Zenith.Render.Camera` or the entity's transform: bounds clamping and snapping are applied at extraction to the rendered canvas camera only (Rendering.md "Extraction").

## Zenith_SpriteAtlas header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_SpriteAtlas.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithFlux/Flux_TextureResource.h"     // GEOM's Flux_TextureResource: the page type

/// @contract enum Zenith_SpriteAtlasFilter
/// Ownership: Plain enumeration; the atlas document's "filter" and the cooked header's filter byte.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3. Cooked as uint8; values never move.
/// @endcontract
enum Zenith_SpriteAtlasFilter : uint8
{
	SPRITE_ATLAS_FILTER_POINT = 0,          // pixel art: pages FLUX_TEXTURE_USAGE_COLOUR_EXACT, one mip, sampler uFLUX_SAMPLER_POINT_CLAMP
	SPRITE_ATLAS_FILTER_LINEAR = 1,         // HD art: pages FLUX_TEXTURE_USAGE_COLOUR, mipped, sampler uFLUX_SAMPLER_LINEAR_CLAMP
};

/// @contract value Zenith_SpriteFrameView
/// Ownership: Plain value copied out of the atlas; borrows nothing.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None; a copy.
/// Units: Texels for the rectangle, pivot and border; m_fPixelsPerUnit in texels per metre.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_SpriteFrameView
{
	uint16 m_uPage = 0;                    // index into the atlas's page list
	uint16 m_auRect[4] = {};               // x0, y0, x1, y1 in page texels, origin top-left, exclusive max
	float m_afPivot[2] = {};                // the pivot in texels from the frame's top-left, may be fractional (k / 2)
	uint16 m_auBorder[4] = {};             // nine-slice left, top, right, bottom in texels; all 0 = no border
	uint16 m_auTrimOffset[2] = {};         // where the trimmed rectangle sits in the untrimmed frame, texels
	uint16 m_auSourceSize[2] = {};         // the untrimmed frame size, texels
	float m_fPixelsPerUnit = 16.0f;         // the atlas's value, copied
};

/// @contract type Zenith_SpriteAtlasAsset
/// Ownership: The runtime object of kind TwoD.SpriteAtlas, created by Zenith2D's kind loader and shared through
///   Zenith_AssetHandle<Zenith_SpriteAtlasAsset>; its page textures are hard dependencies it holds as counted handles.
/// Thread affinity: ANY_THREAD for every const member once the asset is ACTIVE.
/// Invalidation: A hot reload publishes a new object; a Zenith_SpriteFrameView copied earlier stays a valid value.
/// Units: Texels.
/// Errors: FindFrame returns TWOD_ERROR_UNKNOWN_SPRITE for an absent name; nothing asserts on cooked data.
/// Version: since Z2 Stage 3. The cooked format is Formats.md "Cooked sprite atlas format".
/// @endcontract
class Zenith_SpriteAtlasAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SPRITE_ATLAS;   // the code ASSET reserves (cross-area row TD3)
	uint32 GetFrameCount() const;
	uint32 GetPageCount() const;
	Zenith_SpriteAtlasFilter GetFilter() const;
	float GetPixelsPerUnit() const;
	// O(log n) over the name-hash table sorted by (hash, name); n = frame count.
	Zenith_Result<uint32, Zenith_TwoDError> FindFrame(Zenith_StringID xName) const;
	Zenith_SpriteFrameView GetFrame(uint32 uFrameIndex) const;        // ZENITH_ASSERT(index < count)
	const Zenith_AssetHandle<Flux_TextureResource>& GetPage(uint32 uPage) const;
	Zenith_StringID GetFrameName(uint32 uFrameIndex) const;
};
```

`Zenith_SpriteAtlasAsset` includes `Flux_TextureResource.h`, a ZenithFlux public header; Zenith2D declares ZenithFlux, so the edge exists ([Z2-N-ARCH-076] forbids only the reverse). The page handle type is the complete `Zenith_AssetHandle<Flux_TextureResource>` that `Zenith_AssetHandle` requires.

## Zenith_Sprite2DAnimation header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Sprite2DAnimation.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "Zenith2D/Zenith_TwoDTypes.h"
#include <cstddef>                                // offsetof (allowed standard header)
#include <type_traits>                            // has_unique_object_representations_v (allowed standard header)

/// @contract enum Zenith_Animation2DParameterKind
/// Ownership: Plain enumeration; the kind of each declared animator parameter in a cooked animation set.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Cooked as uint8; values never move.
/// @endcontract
enum Zenith_Animation2DParameterKind : uint8
{
	ANIMATION2D_PARAMETER_FLOAT = 0,
	ANIMATION2D_PARAMETER_INT = 1,          // stored as a float holding an integer in [−2^24, 2^24]
	ANIMATION2D_PARAMETER_BOOL = 2,         // 0.0 or 1.0
	ANIMATION2D_PARAMETER_TRIGGER = 3,      // a bit of TriggerBits, consumed by the transition that tests it
};

/// @contract enum Zenith_Animation2DConditionOp
/// Ownership: Plain enumeration; a transition condition's operator.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Cooked as uint8; values never move.
/// @endcontract
enum Zenith_Animation2DConditionOp : uint8
{
	ANIMATION2D_CONDITION_GREATER = 0, ANIMATION2D_CONDITION_LESS = 1, ANIMATION2D_CONDITION_EQUAL = 2,
	ANIMATION2D_CONDITION_NOT_EQUAL = 3, ANIMATION2D_CONDITION_TRUE = 4, ANIMATION2D_CONDITION_FALSE = 5,
	ANIMATION2D_CONDITION_TRIGGERED = 6,
};

/// @contract value Zenith_AnimatorParameter2D
/// Ownership: Plain value; the element type of Zenith_SpriteAnimatorComponent property 9 (a fixed array of 16).
/// Thread affinity: IMMUTABLE; members non-const: a registered struct its containing component, payload or result writes
///   field by field.
/// Invalidation: None.
/// Units: As the parameter declares.
/// Errors: None.
/// Version: since Z2 Stage 4; struct version 1.
/// @endcontract
struct Zenith_AnimatorParameter2D
{
	float m_fValue = 0.0f;                  // 1 Value
};

/// @contract type Zenith_SpriteAnimationAsset
/// Ownership: The runtime object of kind TwoD.SpriteAnimation, created by Zenith2D's kind loader; holds its atlases and rigs as
///   counted hard dependencies.
/// Thread affinity: ANY_THREAD for every const member once ACTIVE.
/// Invalidation: A hot reload publishes a new object; animators keep their state index when the state's name exists in the new
///   set and restart in the default state otherwise (Simulation.md "Animation hot reload").
/// Units: Clip frame durations in ticks of 1/1000 s (milliseconds) as authored; evaluation converts with the tick duration.
/// Errors: Find functions return TWOD_ERROR_UNKNOWN_STATE or TWOD_ERROR_UNKNOWN_PARAMETER; nothing asserts on cooked data.
/// Version: since Z2 Stage 4. Cooked format: Formats.md "Cooked sprite animation format".
/// @endcontract
class Zenith_SpriteAnimationAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SPRITE_ANIMATION;   // reserved by ASSET (row TD3)
	uint32 GetStateCount() const;
	uint32 GetDefaultState() const;
	uint32 GetParameterCount() const;
	Zenith_Result<uint32, Zenith_TwoDError> FindState(Zenith_StringID xName) const;
	Zenith_Result<uint32, Zenith_TwoDError> FindParameter(Zenith_StringID xName) const;
	Zenith_Animation2DParameterKind GetParameterKind(uint32 uParameter) const;
	Zenith_StringID GetStateName(uint32 uState) const;
};

/// @contract value Zenith_AnimatorPlayPayload
/// Ownership: Plain value; the payload of the command kind TwoD.AnimatorPlay (registered struct Zenith.TwoD.AnimatorPlayPayload).
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None.
/// Errors: Playback resolves m_xState against the entity's set at the sync point and drops an unknown name, a stale entity,
///   a missing component or an unresolved set (twod.anim.command_dropped_delta; Simulation.md "Animator state machine").
/// Version: since Z2 Stage 4; struct version 1. In-process command bytes only; never serialized (a Zenith_StringID is
///   meaningless in another process, CORE/Text.md "String interning"). Every byte is a declared field: no implicit padding
///   reaches the command buffer.
/// @endcontract
struct Zenith_AnimatorPlayPayload   // 24 bytes: 8 + 4 + 1 + 11
{
	Zenith_EntityHandle m_xEntity;          // offset 0: the animated entity
	Zenith_StringID m_xState;               // offset 8: the state's name, resolved at playback
	uint8 m_uRestart = 0;                  // offset 12: 1 re-enters the state even when it is already current
	uint8 m_auReserved[11] = {};           // offset 13: zero
};
static_assert(sizeof(Zenith_AnimatorPlayPayload) == 24);
static_assert(std::has_unique_object_representations_v<Zenith_AnimatorPlayPayload>);   // no padding bits

/// @contract value Zenith_AnimatorParameterPayload
/// Ownership: Plain value; the payload of TwoD.AnimatorSetParameter (registered struct Zenith.TwoD.AnimatorParameterPayload).
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: As the parameter declares.
/// Errors: Playback drops a name the set lacks and a kind byte that disagrees with the parameter's kind (a value for a
///   trigger, a trigger for a value), counting twod.anim.command_dropped_delta.
/// Version: since Z2 Stage 4; struct version 1. In-process command bytes only; never serialized (CORE's StringID rule).
///   Every byte is a declared field: no implicit padding reaches the command buffer.
/// @endcontract
struct Zenith_AnimatorParameterPayload   // 24 bytes: 8 + 4 + 4 + 1 + 7
{
	Zenith_EntityHandle m_xEntity;          // offset 0
	Zenith_StringID m_xParameter;           // offset 8: the parameter's name, resolved at playback
	float m_fValue = 0.0f;                  // offset 12: FLOAT, INT (an integer value), BOOL (0 or 1); ignored for a trigger
	uint8 m_uKind = 0;                     // offset 16: 0 a value (FLOAT, INT or BOOL parameter), 1 set a trigger
	uint8 m_auReserved[7] = {};            // offset 17: zero
};
static_assert(sizeof(Zenith_AnimatorParameterPayload) == 24);
// has_unique_object_representations is false for any struct holding a float, so the no-padding check is by offsets:
static_assert(offsetof(Zenith_AnimatorParameterPayload, m_xParameter) == 8 && offsetof(Zenith_AnimatorParameterPayload, m_fValue) == 12
	&& offsetof(Zenith_AnimatorParameterPayload, m_uKind) == 16 && offsetof(Zenith_AnimatorParameterPayload, m_auReserved) == 17);

/// @contract functions Zenith_AnimatorCommands
/// Ownership: Record into the caller's command buffer; the playback functions are Zenith2D's (Simulation.md "Animator state
///   machine").
/// Thread affinity: OWNER_THREAD of the buffer, from any phase 1 … 10.
/// Invalidation: None.
/// Units: None.
/// Errors: TWOD_ERROR_CAPACITY when the buffer's Reserve fails; a zero kind ID records nothing and counts
///   twod.command.unresolved_kind_delta. A recorder reads no world state: it never consults the entity, its component or its
///   asset.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_TwoDError> Zenith_RecordAnimatorPlay(Zenith_CommandBuffer& xBuffer,
	const Zenith_TwoDCommandKinds& xKinds, const Zenith_AnimatorPlayPayload& xPayload);
Zenith_Result<void, Zenith_TwoDError> Zenith_RecordAnimatorSetParameter(Zenith_CommandBuffer& xBuffer,
	const Zenith_TwoDCommandKinds& xKinds, const Zenith_AnimatorParameterPayload& xPayload);

/// @contract value Zenith_SpriteAnimatorComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.SpriteAnimator"; m_xSet holds one reference.
/// Thread affinity: As every component. State fields (2–4, 6–7, 9–10) are owned by PHASE_ANIMATION and written by
///   TwoD.Animate; m_xSet, m_fSpeed and m_bPaused by PHASE_LATE_SCRIPTS; scripts change state, parameters and triggers
///   only through the commands TwoD.AnimatorPlay and TwoD.AnimatorSetParameter.
/// Invalidation: None.
/// Units: m_uStateTicks in simulation ticks since the state was entered; m_fSpeed a unitless multiplier.
/// Errors: None; an unresolvable set leaves the state fields unchanged and counts twod.anim.unresolved_set_delta.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_SpriteAnimatorComponent
{
	Zenith_AssetHandle<Zenith_SpriteAnimationAsset> m_xSet;  // 1  AnimationSet
	uint16 m_uState = 0xFFFFu;                              // 2  State: index into the set; 0xFFFF = enter the default state
	uint16 m_uPreviousState = 0xFFFFu;                      // 3  PreviousState
	uint32 m_uStateTicks = 0;                               // 4  StateTicks
	float m_fSpeed = 1.0f;                                   // 5  Speed, 0 … 16
	Zenith_StringID m_xCurrentSprite;                        // 6  CurrentSprite: the frame drawn this tick
	uint16 m_uCurrentFrame = 0;                             // 7  CurrentFrame: index into the state's clip
	bool m_bPaused = false;                                  // 8  Paused
	Zenith_AnimatorParameter2D m_axParameters[uTWOD_MAX_ANIMATOR_PARAMETERS];   // 9 Parameters (fixed array of 16)
	uint16 m_uTriggerBits = 0;                              // 10 TriggerBits: bit p = trigger parameter p is set
	uint32 m_uLoopCount = 0;                                // 11 LoopCount: completed loops of the current state, saturating
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_ANIMATION_SET{ 1 } … uPROP_LOOP_COUNT{ 11 }, one constant per property in declaration order.
};
```

When an entity carries both `Zenith.TwoD.Sprite` and a playing `Zenith.TwoD.SpriteAnimator` whose state plays a flipbook clip, the extractor draws `CurrentSprite` from the clip's atlas instead of the sprite component's `Atlas` and `Sprite` (Simulation.md "Flipbook evaluation"); the sprite component keeps tint, layer, order, flips, blend, space and visibility.

## Zenith_Skeleton2D header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Skeleton2D.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "Zenith2D/Zenith_TwoDTypes.h"

/// @contract value Zenith_Bone2DPose
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Metres; radians, counter-clockwise about +Z; unitless scale.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_Bone2DPose
{
	Zenith_Vector2 m_xTranslation;
	float m_fRotation = 0.0f;
	Zenith_Vector2 m_xScale = { 1.0f, 1.0f };
};

/// @contract type Zenith_Rig2DAsset
/// Ownership: The runtime object of kind TwoD.Rig, created by Zenith2D's kind loader; holds its atlas as a counted hard
///   dependency.
/// Thread affinity: ANY_THREAD for every const member once ACTIVE.
/// Invalidation: A hot reload publishes a new object; skeletons re-evaluate with the new rig at the next PHASE_ANIMATION.
/// Units: Metres and radians.
/// Errors: FindBone returns TWOD_ERROR_UNKNOWN_BONE; nothing asserts on cooked data.
/// Version: since Z2 Stage 4. Cooked format: Formats.md "Cooked rig format".
/// @endcontract
class Zenith_Rig2DAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_RIG_2D;    // reserved by ASSET (row TD3)
	uint32 GetBoneCount() const;
	uint32 GetSlotCount() const;
	Zenith_Result<uint32, Zenith_TwoDError> FindBone(Zenith_StringID xName) const;
	uint32 GetParentBone(uint32 uBone) const;               // 0xFFFFFFFF for the root
	Zenith_Bone2DPose GetBindPose(uint32 uBone) const;
};

/// @contract value Zenith_Affine2D
/// Ownership: Plain value: the 2D affine x' = a·x + c·y + tx, y' = b·x + d·y + ty (column vectors).
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: Metres for tx, ty; unitless a, b, c, d.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_Affine2D
{
	float m_fA = 1.0f;
	float m_fB = 0.0f;
	float m_fC = 0.0f;
	float m_fD = 1.0f;
	float m_fTx = 0.0f;
	float m_fTy = 0.0f;
};

/// @contract value Zenith_Slot2DPose
/// Ownership: Plain value; one slot of an evaluated pose (Simulation.md "Cutout skeleton evaluation" step 3), saved verbatim.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: Indices into the rig; colour linear 8-bit per channel.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_Slot2DPose   // 12 bytes
{
	uint16 m_uSlot = 0;
	uint16 m_uAttachment = 0xFFFFu;        // index into the rig's attachment table; 0xFFFF = none
	uint16 m_uDrawOrder = 0;
	uint16 m_uReserved = 0;
	uint32 m_uColourRGBA8 = 0xFFFFFFFFu;   // R bits 0–7, G, B, A; linear, each channel ⌊clamp(c, 0, 1) × 255 + 0.5⌋
};
static_assert(sizeof(Zenith_Slot2DPose) == 12);

/// @contract type Zenith_Skeleton2DPoseView
/// Ownership: A view over the module's per-world pose buffer for one entity, returned by Zenith_Skeleton2DPoseResource::GetPose;
///   borrows the buffer.
/// Thread affinity: OWNER_THREAD; valid inside the reading system's invocation (phases 4 … 11 read this tick's pose written by
///   TwoD.AnimateSkeletons in phase 3; phases 1 … 2 read the previous tick's).
/// Invalidation: Invalid when the reading system's invocation ends.
/// Units: Rig space: metres and the entity's local axes; compose with the entity's Zenith.WorldTransform for world space.
/// Errors: None; an entity without a pose returns an empty view (GetBoneCount 0).
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_Skeleton2DPoseView
{
public:
	uint32 GetBoneCount() const;
	Zenith_Affine2D GetRigMatrix(uint32 uBone) const;   // ZENITH_ASSERT(uBone < GetBoneCount())
	// The bone of the rig this pose was evaluated with; TWOD_ERROR_UNKNOWN_BONE for an absent name, O(log bones).
	Zenith_Result<uint32, Zenith_TwoDError> FindBone(Zenith_StringID xName) const;
	Zenith_Span<const Zenith_Slot2DPose> GetSlots() const;   // draw order
};

/// @contract type Zenith_Skeleton2DPoseResource
/// Ownership: The per-world resource "Zenith.TwoD.Poses", owned by Zenith2D's per-world state, registered with
///   AddWorldResource(…, PHASE_ANIMATION, …) at AttachWorld and destroyed at DetachWorld.
/// Thread affinity: ANY_THREAD for GetPose inside a system that declared ReadResource(Zenith.TwoD.Poses) outside phase 3, whose
///   writer is TwoD.AnimateSkeletons.
/// Invalidation: A returned view is invalid when the reading invocation ends.
/// Units: Rig space (see Zenith_Skeleton2DPoseView).
/// Errors: None; an entity without a pose returns an empty view.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_Skeleton2DPoseResource
{
public:
	Zenith_Skeleton2DPoseView GetPose(Zenith_EntityHandle xEntity) const;   // O(log n), n = posed entities
};

/// @contract value Zenith_Skeleton2DComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.Skeleton"; m_xRig holds one reference. Its animation is driven by
///   the entity's Zenith.TwoD.SpriteAnimator, whose states may play skeletal clips.
/// Thread affinity: As every component: PHASE_LATE_SCRIPTS authority; read by TwoD.AnimateSkeletons and TwoD.Extract.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unresolvable rig draws nothing and counts twod.anim.unresolved_rig_delta.
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_Skeleton2DComponent
{
	Zenith_AssetHandle<Zenith_Rig2DAsset> m_xRig;            // 1 Rig
	Zenith_StringID m_xSkin;                                 // 2 Skin: empty = the rig's default skin
	bool m_bVisible = true;                                  // 3 Visible
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static constexpr Zenith_PropertyID uPROP_RIG{ 1 };
	static constexpr Zenith_PropertyID uPROP_SKIN{ 2 };
	static constexpr Zenith_PropertyID uPROP_VISIBLE{ 3 };
};
```

## Zenith_Tilemap header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Tilemap.h
#pragma once
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "Zenith2D/Zenith_TwoDTypes.h"
#include <type_traits>                            // has_unique_object_representations_v (allowed standard header)

/// @contract value Zenith_TileCell
/// Ownership: Plain 32-bit value; the encoding of Formats.md "Tile cell encoding", shared bit for bit with the GPU.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a tile index above the tileset's count draws nothing and collides nothing (twod.tilemap.bad_tile_delta).
/// Version: since Z2 Stage 4. Cooked and saved; the bit layout never changes within a major format version.
/// @endcontract
struct Zenith_TileCell
{
	uint32 m_uBits = 0;                    // bits 0–15 tile index (0 = empty), 16 flip X, 17 flip Y, 18 transpose, 19–31 zero
	constexpr uint32 GetTile() const { return m_uBits & 0xFFFFu; }
	constexpr bool IsEmpty() const { return (m_uBits & 0xFFFFu) == 0; }
};

/// @contract type Zenith_TilesetAsset
/// Ownership: The runtime object of kind TwoD.Tileset; holds its atlas as a counted hard dependency.
/// Thread affinity: ANY_THREAD for every const member once ACTIVE.
/// Invalidation: A hot reload publishes a new object; maps referencing it re-extract every chunk (Simulation.md "Tilemap hot reload").
/// Units: Tile size in texels and in metres.
/// Errors: None returned; unknown tile indices read as empty.
/// Version: since Z2 Stage 4. Cooked format: Formats.md "Cooked tileset format".
/// @endcontract
class Zenith_TilesetAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_TILESET;    // reserved by ASSET (row TD3)
	uint32 GetTileCount() const;                   // valid indices 1 … count
	uint32 GetTileWidthTexels() const;
	uint32 GetTileHeightTexels() const;
	uint32 GetCollisionKind(uint32 uTile) const;  // Formats.md "Tile collision kinds"; 0 for an index out of range
};

/// @contract type Zenith_TilemapAsset
/// Ownership: The runtime object of kind TwoD.Tilemap; holds its tileset as a counted hard dependency; its chunk cells are
///   the authored state that runtime edits overlay.
/// Thread affinity: ANY_THREAD for every const member once ACTIVE.
/// Invalidation: A hot reload publishes a new object; runtime edits of the old object are kept and re-applied over it.
/// Units: Cells; metres for cell size.
/// Errors: None returned.
/// Version: since Z2 Stage 4. Cooked format: Formats.md "Cooked tilemap format".
/// @endcontract
class Zenith_TilemapAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_TILEMAP;    // reserved by ASSET (row TD3)
	uint32 GetLayerCount() const;
	Zenith_Vector2 GetCellSize() const;             // metres
	sint32 GetMinCellX() const;                    // inclusive bounds over every layer
	sint32 GetMinCellY() const;
	sint32 GetMaxCellX() const;                    // exclusive
	sint32 GetMaxCellY() const;
	Zenith_TileCell GetAuthoredCell(uint32 uLayer, sint32 iX, sint32 iY) const;   // empty outside the bounds
	const Zenith_AssetHandle<Zenith_TilesetAsset>& GetTileset() const;
};

/// @contract value Zenith_TilemapComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.Tilemap"; m_xMap holds one reference.
/// Thread affinity: As every component: PHASE_LATE_SCRIPTS authority; read by TwoD.Physics2DStep (collision) and TwoD.Extract.
/// Invalidation: None.
/// Units: Colours linear; friction and restitution unitless.
/// Errors: None; an unresolvable map draws and collides nothing (twod.tilemap.unresolved_delta).
/// Version: since Z2 Stage 4; type version 1.
/// @endcontract
struct Zenith_TilemapComponent
{
	Zenith_AssetHandle<Zenith_TilemapAsset> m_xMap;          // 1  Map
	uint8 m_uLayer = 0;                                     // 2  Layer: sorting layer of map layer 0 (map layers add their offsets)
	sint16 m_iOrder = 0;                                    // 3  Order
	Zenith_Colour m_xTint = { 1.0f, 1.0f, 1.0f, 1.0f };      // 4  Tint
	bool m_bVisible = true;                                  // 5  Visible
	bool m_bCollision = true;                                // 6  Collision: generate static 2D colliders from tile collision kinds
	uint8 m_uSpace = SPRITE2D_SPACE_CANVAS;                 // 7  Space
	uint8 m_uPhysicsLayer = 0;                              // 8  PhysicsLayer, 0 … 31
	float m_fFriction = 0.6f;                                // 9  Friction, 0 … 1
	float m_fRestitution = 0.0f;                             // 10 Restitution, 0 … 1
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_MAP{ 1 } … uPROP_RESTITUTION{ 10 }, one constant per property in declaration order.
};

/// @contract value Zenith_SetTilePayload
/// Ownership: Plain value; the payload of the command kind TwoD.SetTile (registered struct Zenith.TwoD.SetTilePayload).
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Cells.
/// Errors: Playback drops an invalid payload with a counter (Simulation.md "Tile edit playback").
/// Version: since Z2 Stage 4; struct version 1. In-process command bytes only; never serialized (replay logs record input
///   commands, not module command buffers). Every byte is a declared field.
/// @endcontract
struct Zenith_SetTilePayload   // 24 bytes: 8 + 4 + 4 + 4 + 1 + 3
{
	Zenith_EntityHandle m_xMap;             // offset 0: the tilemap entity
	sint32 m_iX = 0;                       // offset 8
	sint32 m_iY = 0;                       // offset 12
	Zenith_TileCell m_xCell;                // offset 16
	uint8 m_uLayer = 0;                    // offset 20
	uint8 m_auReserved[3] = {};            // offset 21: zero
};
static_assert(sizeof(Zenith_SetTilePayload) == 24);
static_assert(std::has_unique_object_representations_v<Zenith_SetTilePayload>);

/// @contract value Zenith_FillTilesPayload
/// Ownership: Plain value; the payload of TwoD.FillTiles (registered struct Zenith.TwoD.FillTilesPayload).
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Cells; the rectangle [m_iX0, m_iX1) × [m_iY0, m_iY1), at most 65,536 cells.
/// Errors: Playback clips the rectangle to [−2^30, 2^30) per axis, never to the map's authored bounds (so playback does not
///   depend on residency), and drops an empty or oversized one with a counter (Simulation.md "Tile edit playback").
/// Version: since Z2 Stage 4; struct version 1. In-process command bytes only; never serialized. Every byte is a declared field.
/// @endcontract
struct Zenith_FillTilesPayload   // 32 bytes: 8 + 4 × 4 + 4 + 1 + 3
{
	Zenith_EntityHandle m_xMap;             // offset 0
	sint32 m_iX0 = 0;                      // offset 8
	sint32 m_iY0 = 0;                      // offset 12
	sint32 m_iX1 = 0;                      // offset 16
	sint32 m_iY1 = 0;                      // offset 20
	Zenith_TileCell m_xCell;                // offset 24
	uint8 m_uLayer = 0;                    // offset 28
	uint8 m_auReserved[3] = {};            // offset 29: zero
};
static_assert(sizeof(Zenith_FillTilesPayload) == 32);
static_assert(std::has_unique_object_representations_v<Zenith_FillTilesPayload>);

/// @contract functions Zenith_TilemapCommands
/// Ownership: Record into the caller's command buffer with Zenith_CommandBuffer::Record(xKinds.m_uSetTile or m_uFillTiles,
///   the payload's bytes); the playback functions are Zenith2D's (Simulation.md "Tile edit playback").
/// Thread affinity: OWNER_THREAD of the buffer, from any phase 1 … 10.
/// Invalidation: None.
/// Units: Cells.
/// Errors: TWOD_ERROR_CAPACITY when the buffer's Reserve fails; a zero kind ID records nothing, returns success and counts
///   twod.command.unresolved_kind_delta. The payload carries only cell coordinates and bits, never an index or hash derived
///   from the map's asset, so its bytes are the same whether or not the map is resident.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_TwoDError> Zenith_RecordSetTile(Zenith_CommandBuffer& xBuffer,
	const Zenith_TwoDCommandKinds& xKinds, const Zenith_SetTilePayload& xPayload);
Zenith_Result<void, Zenith_TwoDError> Zenith_RecordFillTiles(Zenith_CommandBuffer& xBuffer,
	const Zenith_TwoDCommandKinds& xKinds, const Zenith_FillTilesPayload& xPayload);

/// @contract type Zenith_TilemapEditResource
/// Ownership: The per-world resource "Zenith.TwoD.TilemapEdits": the runtime edit overlay of every tilemap of the world over the
///   resolved map assets, owned by Zenith2D's per-world state, registered with AddWorldResource(…, PHASE_PHYSICS, …) at
///   AttachWorld (written by the tile-edit playback at sync points and drained by TwoD.Physics2DStep in phase 5).
/// Thread affinity: ANY_THREAD for the const members inside a system that declared ReadResource(Zenith.TwoD.TilemapEdits)
///   outside phase 5.
/// Invalidation: Results are values.
/// Units: Cells.
/// Errors: GetCell returns TWOD_ERROR_STALE_ENTITY, TWOD_ERROR_MISSING_COMPONENT, TWOD_ERROR_NOT_RESIDENT,
///   TWOD_ERROR_LAYER_OUT_OF_RANGE or TWOD_ERROR_CELL_OUT_OF_RANGE; never asserts.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_TilemapEditResource
{
public:
	// The effective cell: the runtime edit overlay's value when the cell was edited, else the authored cell. O(1) expected.
	Zenith_Result<Zenith_TileCell, Zenith_TwoDError> GetCell(Zenith_EntityHandle xMap, uint32 uLayer,
		sint32 iX, sint32 iY) const;
	// Cell coordinates of a world point on the map's layer plane (floor of the local position divided by the cell size).
	Zenith_Result<Zenith_Vector2, Zenith_TwoDError> WorldToCell(Zenith_EntityHandle xMap,
		Zenith_Vector2 xWorld) const;
	// The module's command-kind IDs (the same object Zenith_Physics2DWorldResource::GetCommandKinds returns).
	const Zenith_TwoDCommandKinds& GetCommandKinds() const;
};
```

C++ systems of other modules read tilemap cells through the world resource `Zenith.TwoD.TilemapEdits` (`Zenith_TilemapEditResource`, declared access `ReadResource`), and scripts through `TwoD.GetTile` and `TwoD.WorldToCell` of "Zenith_TwoDScriptApi header".

## Zenith_Physics2D header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_Physics2D.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "Zenith2D/Zenith_TwoDTypes.h"
#include <cstddef>                                // offsetof (allowed standard header)

/// @contract enum Zenith_Body2DType
/// Ownership: Plain enumeration; Zenith_RigidBody2DComponent property 1.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value is treated as BODY2D_TYPE_STATIC and counted twod.physics.bad_enum_delta.
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Body2DType : uint8 { BODY2D_TYPE_STATIC = 0, BODY2D_TYPE_KINEMATIC = 1, BODY2D_TYPE_DYNAMIC = 2 };

/// @contract enum Zenith_Shape2DKind
/// Ownership: Plain enumeration; Zenith_Collider2DComponent property 1.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value creates no shape (twod.physics.bad_enum_delta).
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Shape2DKind : uint8
{
	SHAPE2D_KIND_BOX = 0,                   // m_xSize full extents, rotated by m_fAngle about m_xOffset
	SHAPE2D_KIND_CIRCLE = 1,                // m_fRadius at m_xOffset
	SHAPE2D_KIND_CAPSULE = 2,               // segment of length m_xSize.x along the local X axis, radius m_fRadius
	SHAPE2D_KIND_POLYGON = 3,               // convex, 3 … 8 vertices counter-clockwise in m_axVertices
	SHAPE2D_KIND_SEGMENT = 4,               // m_axVertices[0] to m_axVertices[1], two-sided, static and kinematic bodies only
};

/// @contract enum Zenith_Joint2DKind
/// Ownership: Plain enumeration; Zenith_Joint2DComponent property 1.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value creates no joint (twod.physics.bad_enum_delta).
/// Version: since Z2 Stage 3. Serialized as uint8; values never move.
/// @endcontract
enum Zenith_Joint2DKind : uint8
{
	JOINT2D_KIND_DISTANCE = 0,              // keeps the anchors at m_fLength, optionally a spring, limits [m_fMinLength, m_fMaxLength]
	JOINT2D_KIND_REVOLUTE = 1,              // shared anchor; optional angle limits and motor
	JOINT2D_KIND_PRISMATIC = 2,             // translation along m_xAxis; optional limits and motor
	JOINT2D_KIND_WELD = 3,                  // fixed relative pose; optional spring stiffness
	JOINT2D_KIND_WHEEL = 4,                 // prismatic suspension along m_xAxis with a spring plus a revolute motor
	JOINT2D_KIND_MOTOR = 5,                 // drives body B toward a target offset from body A with bounded force and torque
};

/// @contract value Zenith_RigidBody2DComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.RigidBody".
/// Thread affinity: As every component. Properties 1–11 PHASE_LATE_SCRIPTS authority; 20–22 PHASE_PHYSICS (written by
///   TwoD.Physics2DStep's write-back); scripts change velocities through TwoD.Body2D commands.
/// Invalidation: None.
/// Units: kg; metres per second; radians per second; damping per second; gravity scale unitless.
/// Errors: None; non-finite values are rejected at ingest with twod.physics.non_finite_delta and the body is not created.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_RigidBody2DComponent
{
	uint8 m_uType = BODY2D_TYPE_DYNAMIC;                    // 1  Type
	float m_fMass = 0.0f;                                    // 2  Mass: 0 = from the colliders' densities
	float m_fGravityScale = 1.0f;                            // 3  GravityScale, −16 … 16
	float m_fLinearDamping = 0.0f;                           // 4  LinearDamping, 0 … 100
	float m_fAngularDamping = 0.05f;                         // 5  AngularDamping, 0 … 100
	bool m_bFixedRotation = false;                           // 6  FixedRotation
	bool m_bBullet = false;                                  // 7  Bullet: continuous collision against dynamic bodies as well
	bool m_bSleepAllowed = true;                             // 8  SleepAllowed
	bool m_bEnabled = true;                                  // 9  Enabled
	Zenith_Vector2 m_xInitialLinearVelocity;                 // 10 InitialLinearVelocity
	float m_fInitialAngularVelocity = 0.0f;                  // 11 InitialAngularVelocity
	Zenith_Vector2 m_xLinearVelocity;                        // 20 LinearVelocity (runtime)
	float m_fAngularVelocity = 0.0f;                         // 21 AngularVelocity (runtime)
	bool m_bSleeping = false;                                // 22 Sleeping (runtime)
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_TYPE{ 1 } … uPROP_INITIAL_ANGULAR_VELOCITY{ 11 }; uPROP_LINEAR_VELOCITY{ 20 }, uPROP_ANGULAR_VELOCITY{ 21 }, uPROP_SLEEPING{ 22 }.
};

/// @contract value Zenith_Collider2DComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.Collider". A collider belongs to the nearest entity at or above it
///   in the hierarchy that carries Zenith.TwoD.RigidBody; without one it is a static collider of its own.
/// Thread affinity: As every component: PHASE_LATE_SCRIPTS authority; read by TwoD.Physics2DStep.
/// Invalidation: None.
/// Units: Metres; radians; kg per square metre; friction and restitution unitless.
/// Errors: None; a degenerate shape (zero extent, a non-convex or clockwise polygon, fewer than 3 polygon vertices) creates no
///   shape and counts twod.physics.bad_shape_delta with one development-build warning per entity.
/// Version: since Z2 Stage 3; type version 1.
/// @endcontract
struct Zenith_Collider2DComponent
{
	uint8 m_uShape = SHAPE2D_KIND_BOX;                      // 1  Shape
	Zenith_Vector2 m_xOffset;                                // 2  Offset in the entity's local space
	float m_fAngle = 0.0f;                                   // 3  Angle
	Zenith_Vector2 m_xSize = { 1.0f, 1.0f };                 // 4  Size
	float m_fRadius = 0.5f;                                  // 5  Radius
	Zenith_Vector2 m_axVertices[uTWOD_MAX_POLYGON_VERTICES]; // 6  Vertices (fixed array of 8)
	uint8 m_uVertexCount = 0;                               // 7  VertexCount, 0 … 8
	float m_fDensity = 1.0f;                                 // 8  Density, 0 … 100,000
	float m_fFriction = 0.6f;                                // 9  Friction, 0 … 1
	float m_fRestitution = 0.0f;                             // 10 Restitution, 0 … 1
	bool m_bSensor = false;                                  // 11 Sensor: a trigger; no contact response
	uint8 m_uLayer = 0;                                     // 12 Layer, 0 … 31
	bool m_bOneWay = false;                                  // 13 OneWay
	Zenith_Vector2 m_xOneWayNormal = { 0.0f, 1.0f };         // 14 OneWayNormal: the side contacts are kept from (local space)
	bool m_bEnabled = true;                                  // 15 Enabled
	bool m_bReportContacts = false;                          // 16 ReportContacts: emit Contact2DBegan/Ended for this collider
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_SHAPE{ 1 } … uPROP_REPORT_CONTACTS{ 16 }, one constant per property in declaration order.
};

/// @contract value Zenith_Joint2DComponent
/// Ownership: Plain component value stored as "Zenith.TwoD.Joint" on its own entity.
/// Thread affinity: As every component. Properties 1–20 PHASE_LATE_SCRIPTS authority; 21 Broken PHASE_PHYSICS.
/// Invalidation: None.
/// Units: Metres; radians; metres or radians per second for motor speed; newtons or newton-metres; stiffness in hertz;
///   damping as a ratio. AnchorA and AnchorB are body-local; WorldAnchor is world space, read only when BodyB is null.
/// Errors: None; a joint whose bodies do not resolve creates no constraint (twod.physics.joint_unresolved_delta).
/// Version: since Z2 Stage 3; type version 1 (property 20 is part of version 1: the component has not shipped).
/// @endcontract
struct Zenith_Joint2DComponent
{
	uint8 m_uKind = JOINT2D_KIND_REVOLUTE;                  // 1  Kind
	Zenith_EntityHandle m_xBodyA;                            // 2  BodyA
	Zenith_EntityHandle m_xBodyB;                            // 3  BodyB: null = the world
	Zenith_Vector2 m_xAnchorA;                               // 4  AnchorA in A's local space
	Zenith_Vector2 m_xAnchorB;                               // 5  AnchorB in B's local space; ignored when BodyB is null
	Zenith_Vector2 m_xAxis = { 1.0f, 0.0f };                 // 6  Axis in A's local space (PRISMATIC, WHEEL)
	bool m_bCollideConnected = false;                        // 7  CollideConnected
	bool m_bLimitEnabled = false;                            // 8  LimitEnabled
	float m_fLowerLimit = 0.0f;                              // 9  LowerLimit
	float m_fUpperLimit = 0.0f;                              // 10 UpperLimit
	bool m_bMotorEnabled = false;                            // 11 MotorEnabled
	float m_fMotorSpeed = 0.0f;                              // 12 MotorSpeed
	float m_fMaxMotorForce = 0.0f;                           // 13 MaxMotorForce (force or torque by kind)
	float m_fLength = 1.0f;                                  // 14 Length (DISTANCE)
	float m_fMinLength = 0.0f;                               // 15 MinLength (DISTANCE)
	float m_fMaxLength = 1.0e6f;                             // 16 MaxLength (DISTANCE; a rope when MinLength is 0)
	float m_fStiffness = 0.0f;                               // 17 Stiffness: 0 = rigid
	float m_fDamping = 0.0f;                                 // 18 Damping
	float m_fBreakForce = 0.0f;                              // 19 BreakForce: 0 = unbreakable
	Zenith_Vector2 m_xWorldAnchor;                           // 20 WorldAnchor: the world-space anchor of a joint whose BodyB is null
	bool m_bBroken = false;                                  // 21 Broken (runtime)
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	// uPROP_KIND{ 1 } … uPROP_BREAK_FORCE{ 19 }; uPROP_WORLD_ANCHOR{ 20 }; uPROP_BROKEN{ 21 }.
};

/// @contract enum Zenith_Physics2DErrorCode
/// Ownership: Plain enumeration; the code of Zenith_Physics2DError.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator's meaning is the comment beside it.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Zenith_Physics2DErrorCode : uint8
{
	PHYSICS2D_ERROR_OUT_OF_MEMORY = 1,      // the backend world or a capacity reservation failed
	PHYSICS2D_ERROR_INVALID_DESC = 2,       // a twod.physics.* value outside its domain, or a step duration above 1/15 s
	PHYSICS2D_ERROR_BODY_CAPACITY = 3,      // twod.physics.maxBodies reached
	PHYSICS2D_ERROR_SHAPE_CAPACITY = 4,     // twod.physics.maxShapes reached
	PHYSICS2D_ERROR_JOINT_CAPACITY = 5,     // twod.physics.maxJoints reached
	PHYSICS2D_ERROR_NO_BODY = 6,            // a query or command names an entity without a body
	PHYSICS2D_ERROR_BAD_QUERY = 7,          // a non-finite origin, a zero-length direction or a degenerate cast shape
	PHYSICS2D_ERROR_STATE_CORRUPT = 8,      // a saved backend state failed its check on restore
	PHYSICS2D_ERROR_STEP_FAILED = 9,        // the backend reported a step failure (a defect report; never a simulation branch)
	PHYSICS2D_ERROR_WORLD_CAPACITY = 10,    // the backend's process-wide world table is full (Box2D's B2_MAX_WORLDS)
};

/// @contract value Zenith_Physics2DError
/// Ownership: Plain value; the struct form of Zenith2/Docs/Contracts.md "Errors and results".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_Physics2DError
{
	const Zenith_Physics2DErrorCode m_eCode = PHYSICS2D_ERROR_OUT_OF_MEMORY;
	const uint8 m_auReserved[3] = {};
	const uint32 m_uSimulationIDLow = 0;         // the entity's simulation ID low bits, 0 when none
};
constexpr const char* Zenith_ToString(Zenith_Physics2DErrorCode eCode);

/// @contract value Zenith_Physics2DQueryFilter
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_Physics2DQueryFilter
{
	uint32 m_uLayerMask = 0xFFFFFFFFu;     // bit l = collider layer l is considered
	bool m_bIncludeSensors = false;
	Zenith_EntityHandle m_xIgnoreEntity;    // its body and colliders are skipped; null = none
};

/// @contract value Zenith_RayCast2DHit
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None; the entity handle follows its own rules.
/// Units: Metres; m_fFraction in [0, 1] of the cast length.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_RayCast2DHit
{
	Zenith_EntityHandle m_xCollider;        // the entity carrying the hit collider (a tilemap entity for a tile edge)
	Zenith_EntityHandle m_xBody;            // the body entity, null for a static collider without a body and for a tile hit (the
	                                        // ordering body key is then the collider's or the map's simulation ID)
	Zenith_Vector2 m_xPoint;
	Zenith_Vector2 m_xNormal;
	float m_fFraction = 1.0f;
	Zenith_TileCellRef m_xCell;             // for a tilemap hit: the cell of the unit edge or polygon hit; else m_bValid false
};

/// @contract value Zenith_Overlap2DHit
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_Overlap2DHit
{
	Zenith_EntityHandle m_xCollider;
	Zenith_EntityHandle m_xBody;
};

/// @contract type Zenith_Physics2DWorldResource
/// Ownership: The per-world resource "Zenith.TwoD.Physics", owned by Zenith2D's per-world state and registered with
///   AddWorldResource(…, PHASE_PHYSICS, …) at AttachWorld; destroyed at DetachWorld.
/// Thread affinity: ANY_THREAD for the const query members, called only inside a system that declared
///   ReadResource(Zenith.TwoD.Physics), in phases 1 … 4 and 6 … 11 (phase 5 belongs to its writer, TwoD.Physics2DStep);
///   concurrent queries from parallel sub-jobs are allowed.
/// Invalidation: Results reflect the state after the last completed phase 5; handles in results follow their own rules.
/// Units: Metres; radians.
/// Errors: Queries return Zenith_Physics2DError (PHYSICS2D_ERROR_BAD_QUERY for invalid input). The backend collects with a
///   bounded collector that keeps the axOut.Size() smallest hits by (fraction, body key, collider ID, cell key) over every hit,
///   whatever its traversal order (Simulation.md "Physics 2D queries"); the kept hits are sorted and the count's bit 31 is set
///   when more existed.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_Physics2DWorldResource
{
public:
	// Closest hit along [xOrigin, xOrigin + xDelta]; ties at equal fraction resolve to the lower body key, then the lower collider
	// simulation ID, then the lower cell key. Body key: the simulation ID of the entity whose 2D body owns the shape (the
	// collider's own entity for a bodiless collider, the map entity for tile chains and CUSTOM polygons, 0 for the ground body;
	// Simulation.md "Physics 2D model"). Cell key: Zenith_MakeTileCellKey of the hit cell, 0 for a non-tile hit.
	Zenith_Result<bool, Zenith_Physics2DError> RayCast(Zenith_Vector2 xOrigin, Zenith_Vector2 xDelta,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_RayCast2DHit& xOut) const;
	// Every hit, sorted by (fraction, body key, collider simulation ID, cell key); returns the count, with bit 31
	// set when more than axOut's capacity (at most uTWOD_MAX_QUERY_HITS) existed.
	Zenith_Result<uint32, Zenith_Physics2DError> RayCastAll(Zenith_Vector2 xOrigin, Zenith_Vector2 xDelta,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_Span<Zenith_RayCast2DHit> axOut) const;
	// A circle of radius fRadius swept along xDelta; closest hit, same tie-break as RayCast.
	Zenith_Result<bool, Zenith_Physics2DError> CircleCast(Zenith_Vector2 xOrigin, float fRadius, Zenith_Vector2 xDelta,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_RayCast2DHit& xOut) const;
	// Colliders containing the point, or overlapping the axis-aligned box [xMin, xMax], sorted by (body key, collider simulation
	// ID, cell key).
	Zenith_Result<uint32, Zenith_Physics2DError> OverlapPoint(Zenith_Vector2 xPoint,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_Span<Zenith_Overlap2DHit> axOut) const;
	Zenith_Result<uint32, Zenith_Physics2DError> OverlapBox(Zenith_Vector2 xMin, Zenith_Vector2 xMax,
		const Zenith_Physics2DQueryFilter& xFilter, Zenith_Span<Zenith_Overlap2DHit> axOut) const;
	Zenith_Vector2 GetGravity() const;
	const Zenith_TwoDCommandKinds& GetCommandKinds() const;
};

/// @contract enum Zenith_Body2DCommandKind
/// Ownership: Plain enumeration; the m_uKind byte of Zenith_Body2DCommandPayload.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown kind is dropped at playback (twod.physics.command_bad_kind_delta).
/// Version: since Z2 Stage 3. In-process command bytes only; values never move.
/// @endcontract
enum Zenith_Body2DCommandKind : uint8
{
	BODY2D_COMMAND_APPLY_IMPULSE = 1,       // m_xVector at m_xPoint (world), before the first substep
	BODY2D_COMMAND_APPLY_ANGULAR_IMPULSE = 2,   // m_fScalar
	BODY2D_COMMAND_APPLY_FORCE = 3,         // m_xVector at m_xPoint, before every substep of the tick
	BODY2D_COMMAND_APPLY_TORQUE = 4,        // m_fScalar, before every substep of the tick
	BODY2D_COMMAND_SET_VELOCITY = 5,        // linear m_xVector, angular m_fScalar
	BODY2D_COMMAND_SET_KINEMATIC_TARGET = 6,    // position m_xPoint, rotation m_fScalar, reached at the end of the tick
	BODY2D_COMMAND_WAKE = 7,
	BODY2D_COMMAND_SET_GRAVITY = 8,         // the world's gravity m_xVector; the entity is ignored
};

/// @contract value Zenith_Body2DCommandPayload
/// Ownership: Plain value; the payload of the command kind TwoD.Body2D (registered struct Zenith.TwoD.Body2DCommandPayload).
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Newton-seconds, newtons, metres, radians as the kind states.
/// Errors: Playback drops a non-finite payload (twod.physics.non_finite_delta) and an unknown kind byte
///   (twod.physics.command_bad_kind_delta).
/// Version: since Z2 Stage 3; struct version 1. In-process command bytes only; never serialized. Every byte is a declared field.
/// @endcontract
struct Zenith_Body2DCommandPayload   // 32 bytes: 8 + 8 + 8 + 4 + 1 + 3
{
	Zenith_EntityHandle m_xBody;            // offset 0
	Zenith_Vector2 m_xVector;               // offset 8
	Zenith_Vector2 m_xPoint;                // offset 16
	float m_fScalar = 0.0f;                 // offset 24
	uint8 m_uKind = BODY2D_COMMAND_APPLY_IMPULSE;   // offset 28
	uint8 m_auReserved[3] = {};            // offset 29: zero
};
static_assert(sizeof(Zenith_Body2DCommandPayload) == 32);
static_assert(offsetof(Zenith_Body2DCommandPayload, m_fScalar) == 24 && offsetof(Zenith_Body2DCommandPayload, m_uKind) == 28);

/// @contract functions Zenith_Body2DCommands
/// Ownership: Records into the caller's command buffer with Zenith_CommandBuffer::Record(xKinds.m_uBody2D, payload bytes).
/// Thread affinity: OWNER_THREAD of the buffer, phases 1 … 10.
/// Invalidation: None.
/// Units: As Zenith_Body2DCommandPayload.
/// Errors: TWOD_ERROR_CAPACITY when the buffer's Reserve fails; a zero kind ID records nothing, returns success and counts
///   twod.command.unresolved_kind_delta.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_TwoDError> Zenith_RecordBody2DCommand(Zenith_CommandBuffer& xBuffer,
	const Zenith_TwoDCommandKinds& xKinds, const Zenith_Body2DCommandPayload& xPayload);
```

| Stable name | Properties | Authority | Serialized |
|---|---|---|---|
| `Zenith.TwoD.RigidBody` | 1 Type U8 2, 0 … 2; 2 Mass F32 0, 0 … 1e6; 3 GravityScale F32 1, −16 … 16; 4 LinearDamping F32 0, 0 … 100; 5 AngularDamping F32 0.05, 0 … 100; 6 FixedRotation BOOL; 7 Bullet BOOL; 8 SleepAllowed BOOL true; 9 Enabled BOOL true; 10 InitialLinearVelocity VEC2; 11 InitialAngularVelocity F32; 20 LinearVelocity VEC2; 21 AngularVelocity F32; 22 Sleeping BOOL | 1–11 `PHASE_LATE_SCRIPTS`; 20–22 `PHASE_PHYSICS` | 1–11, 20–22 (the runtime velocities are part of EXACT saves) |
| `Zenith.TwoD.Collider` | 1–16 as the struct | `PHASE_LATE_SCRIPTS` | yes |
| `Zenith.TwoD.Joint` | 1–19 as the struct (5 AnchorB VEC2 (0, 0), B-local, ignored when BodyB is null); 20 WorldAnchor VEC2 (0, 0), metres, world space, read only when BodyB is null, registered with `PROPERTY_FLAG_WORLD_POSITION` in the diff that lands X332 (c)'s VEC2 admission (row TD24; until then with no flag); 21 Broken BOOL | 1–20 `PHASE_LATE_SCRIPTS`; 21 `PHASE_PHYSICS` | yes |
| `Zenith.TwoD.Physics` (resource) | none (a runtime object) | owning phase `PHASE_PHYSICS` | no: the save participant saves the backend state (Simulation.md "Save participation") |
| `Zenith.TwoD.Poses` (resource) | none | owning phase `PHASE_ANIMATION` | no: the save participant saves the pose buffer (Simulation.md "Save participation") |
| `Zenith.TwoD.TilemapEdits` (resource) | none | owning phase `PHASE_PHYSICS` | no: the save participant saves the overlay |

## Zenith_TwoDEvents header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_TwoDEvents.h
#pragma once
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract value Zenith_Animation2DEvent
/// Ownership: Plain event value "Zenith.TwoD.AnimationEvent", emitted by TwoD.Animate through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Animation events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: Seconds are not used; m_fValue is the authored number, unitless.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 4. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Animation2DEvent          // "Zenith.TwoD.AnimationEvent": a frame event of a clip
{
	Zenith_EntityHandle m_xEntity;      // 1 Entity
	Zenith_StringID m_xName;            // 2 Name: the event name authored on the frame
	uint16 m_uState = 0;               // 3 State
	uint16 m_uFrame = 0;               // 4 Frame
	float m_fValue = 0.0f;              // 5 Value: the event's authored number
};

/// @contract value Zenith_Animation2DFinishedEvent
/// Ownership: Plain event value "Zenith.TwoD.AnimationFinished", emitted by TwoD.Animate through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Animation events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: None.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 4. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Animation2DFinishedEvent  // "Zenith.TwoD.AnimationFinished": a non-looping clip reached its last frame
{
	Zenith_EntityHandle m_xEntity;      // 1 Entity
	uint16 m_uState = 0;               // 2 State
};

/// @contract value Zenith_Contact2DBeganEvent
/// Ownership: Plain event value "Zenith.TwoD.ContactBegan", emitted by TwoD.Physics2DStep through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Contact and trigger events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: Metres; metres per second.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 3. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Contact2DBeganEvent       // "Zenith.TwoD.ContactBegan"
{
	Zenith_EntityHandle m_xColliderA;   // 1 ColliderA: the lower simulation ID of the pair
	Zenith_EntityHandle m_xColliderB;   // 2 ColliderB
	Zenith_Vector2 m_xPoint;            // 3 Point
	Zenith_Vector2 m_xNormal;           // 4 Normal: from A toward B
	float m_fApproachSpeed = 0.0f;      // 5 ApproachSpeed: the relative normal speed at the first contact point
};

/// @contract value Zenith_Contact2DEndedEvent
/// Ownership: Plain event value "Zenith.TwoD.ContactEnded", emitted by TwoD.Physics2DStep through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Contact and trigger events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: None.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 3. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Contact2DEndedEvent       // "Zenith.TwoD.ContactEnded"
{
	Zenith_EntityHandle m_xColliderA;   // 1 ColliderA
	Zenith_EntityHandle m_xColliderB;   // 2 ColliderB
};

/// @contract value Zenith_Trigger2DEnteredEvent
/// Ownership: Plain event value "Zenith.TwoD.TriggerEntered", emitted by TwoD.Physics2DStep through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Contact and trigger events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: None.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 3. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Trigger2DEnteredEvent     // "Zenith.TwoD.TriggerEntered"
{
	Zenith_EntityHandle m_xTrigger;     // 1 Trigger: the sensor collider's entity
	Zenith_EntityHandle m_xOther;       // 2 Other
};

/// @contract value Zenith_Trigger2DExitedEvent
/// Ownership: Plain event value "Zenith.TwoD.TriggerExited", emitted by TwoD.Physics2DStep through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Contact and trigger events").
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: None.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 3. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Trigger2DExitedEvent      // "Zenith.TwoD.TriggerExited"
{
	Zenith_EntityHandle m_xTrigger;     // 1 Trigger
	Zenith_EntityHandle m_xOther;       // 2 Other
};

/// @contract value Zenith_Joint2DBrokenEvent
/// Ownership: Plain event value "Zenith.TwoD.JointBroken", emitted by TwoD.Physics2DStep through Zenith_SystemContext::Events and delivered in phase 9
///   in merge-key order (Simulation.md "Physics 2D step" step 4).
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None; handles follow their own rules.
/// Units: Newtons.
/// Errors: None; a full event queue rejects deterministically at the merge (Z2-B-GAME-003, counted).
/// Version: since Z2 Stage 3. A registered TYPE_KIND_EVENT, version 1.
/// @endcontract
struct Zenith_Joint2DBrokenEvent        // "Zenith.TwoD.JointBroken"
{
	Zenith_EntityHandle m_xJoint;       // 1 Joint
	float m_fForce = 0.0f;              // 2 Force: the reaction force that exceeded BreakForce
};
```

## Zenith_TwoDScriptApi header

```cpp
// Zenith2/Engine/Zenith2D/Zenith_TwoDScriptApi.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"
#include "Zenith2D/Zenith_TwoDTypes.h"
#include "Zenith2D/Zenith_Physics2D.h"

/// @contract value Zenith_TwoDRayHit
/// Ownership: Plain value returned by value from the cast bindings; registered struct Zenith.TwoD.RayHit, marshalled as a
///   frozen table (Scripting/Bindings.md "Value marshalling", row reflected value struct).
/// Thread affinity: IMMUTABLE; members non-const: a registered struct its containing component, payload or result writes
///   field by field.
/// Invalidation: None; the handles follow their own rules.
/// Units: Metres; m_fFraction in [0, 1] of the cast length.
/// Errors: None; m_bHit false means no hit, and every other member is then its default.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TwoDRayHit
{
	bool m_bHit = false;                    // 1 Hit
	Zenith_EntityHandle m_xCollider;        // 2 Collider: the hit collider's entity (the tilemap entity for a tile)
	Zenith_EntityHandle m_xBody;            // 3 Body: the body's entity; null (nil) for a static collider without a body
	Zenith_Vector2 m_xPoint;                // 4 Point
	Zenith_Vector2 m_xNormal;               // 5 Normal
	float m_fFraction = 1.0f;               // 6 Fraction
	Zenith_TileCellRef m_xCell;             // 7 Cell
};

/// @contract value Zenith_TwoDOverlapResult
/// Ownership: Plain value returned by value from TwoD.OverlapBox; registered struct Zenith.TwoD.OverlapResult. The two fixed
///   arrays marshal as Luau arrays of length uTWOD_MAX_QUERY_HITS whose entries at and beyond m_uCount are null handles (nil),
///   the shape PHYS's Zenith.Physics.OverlapResult has (pending SCRIPT, row TD22).
/// Thread affinity: IMMUTABLE; members non-const: a registered struct its containing component, payload or result writes
///   field by field.
/// Invalidation: None; the handles follow their own rules.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TwoDOverlapResult
{
	uint32 m_uCount = 0;                                        // 1 Count, 0 … 64
	bool m_bTruncated = false;                                   // 2 Truncated: more than 64 overlaps existed
	Zenith_EntityHandle m_axColliders[uTWOD_MAX_QUERY_HITS];     // 3 Colliders, sorted by (body key, collider ID, cell key)
	Zenith_EntityHandle m_axBodies[uTWOD_MAX_QUERY_HITS];        // 4 Bodies, element i the body of collider i
};

/// @contract type Zenith_TwoDScriptApi
/// Ownership: A non-instantiable struct of static functions (the block covers every static member) bound into the Luau table
///   TwoD (Zenith.TwoD) by Zenith_RegisterTwoDScriptBindings; every non-PURE function receives the running
///   Zenith_ScriptInvocationContext as its leading parameter, and the two PURE camera functions take none (the parameter is
///   optional, Scripting/Bindings.md binding registration step 2).
/// Thread affinity: The calling script invocation's thread; contexts SYSTEM and BEHAVIOUR unless a row says otherwise.
/// Invalidation: None.
/// Units: Metres, radians, seconds, cells, as each row of Zenith2/Docs/Design/TWOD.md "Luau surface" states.
/// Errors: Misuse raises a controlled Luau error with a code of TWOD's block (Zenith2/Docs/Design/TWOD.md "Luau errors and
///   asserts") through Zenith_ScriptFailThunk; no function asserts on a script argument ([Z2-N-CONTRACT-473]).
/// Version: since Z2 Stage 3 (camera, physics, sprite functions); Stage 4 (animation, tilemap, skeleton functions).
/// @endcontract
struct Zenith_TwoDScriptApi
{
	// Physics (READ_WORLD Zenith.TwoD.Physics); every result is a value struct with a marshalling row
	static Zenith_Result<Zenith_TwoDRayHit, Zenith_ScriptThunkError> RayCast(Zenith_ScriptInvocationContext& xContext,
		Zenith_Vector2 xOrigin, Zenith_Vector2 xDelta, uint32 uLayerMask);
	static Zenith_Result<Zenith_TwoDRayHit, Zenith_ScriptThunkError> CircleCast(Zenith_ScriptInvocationContext& xContext,
		Zenith_Vector2 xOrigin, float fRadius, Zenith_Vector2 xDelta, uint32 uLayerMask);
	static Zenith_Result<Zenith_TwoDOverlapResult, Zenith_ScriptThunkError> OverlapBox(Zenith_ScriptInvocationContext& xContext,
		Zenith_Vector2 xMin, Zenith_Vector2 xMax, uint32 uLayerMask);
	// Physics commands (BUFFERED, SIDE_EFFECT_KIND_FIELD_COMMAND)
	static Zenith_Result<void, Zenith_ScriptThunkError> ApplyImpulse(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xBody, Zenith_Vector2 xImpulse, Zenith_Vector2 xWorldPoint);
	static Zenith_Result<void, Zenith_ScriptThunkError> ApplyForce(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xBody, Zenith_Vector2 xForce, Zenith_Vector2 xWorldPoint);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetVelocity(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xBody, Zenith_Vector2 xLinear, float fAngular);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetKinematicTarget(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xBody, Zenith_Vector2 xPosition, float fRotation);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetGravity(Zenith_ScriptInvocationContext& xContext,
		Zenith_Vector2 xGravity);
	// Animation commands (BUFFERED): the name is recorded as its Zenith_StringID and resolved at playback
	// (Simulation.md "Animator state machine"); the thunk reads no world state and raises no unknown-name error
	static Zenith_Result<void, Zenith_ScriptThunkError> Play(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xEntity, Zenith_StringID xState, bool bRestart);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetParameter(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xEntity, Zenith_StringID xParameter, double fValue);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetTrigger(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xEntity, Zenith_StringID xParameter);
	// Tilemaps (READ_WORLD for GetTile and WorldToCell; BUFFERED for SetTile and FillTiles)
	static Zenith_Result<uint32, Zenith_ScriptThunkError> GetTile(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xMap, uint32 uLayer, sint32 iX, sint32 iY);
	static Zenith_Result<void, Zenith_ScriptThunkError> SetTile(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xMap, uint32 uLayer, sint32 iX, sint32 iY, uint32 uCellBits);
	static Zenith_Result<void, Zenith_ScriptThunkError> FillTiles(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xMap, uint32 uLayer, sint32 iX0, sint32 iY0, sint32 iX1, sint32 iY1, uint32 uCellBits);
	static Zenith_Result<Zenith_Vector2, Zenith_ScriptThunkError> WorldToCell(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xMap, Zenith_Vector2 xWorld);
	// Skeletons (READ_WORLD Zenith.TwoD.Poses): the bone resolves through the pose's own rig (Zenith_Skeleton2DPoseView::FindBone)
	static Zenith_Result<Zenith_Vector2, Zenith_ScriptThunkError> GetBonePosition(Zenith_ScriptInvocationContext& xContext,
		Zenith_EntityHandle xEntity, Zenith_StringID xBone);
	// Camera mapping (PURE over the arguments and the camera component the script passes in as its fields)
	static Zenith_Vector2 ReferenceToWorld(Zenith_Vector2 xCameraPosition, double fPixelsPerUnit,
		double fReferenceWidth, double fReferenceHeight, Zenith_Vector2 xReference);
	static Zenith_Vector2 WorldToReference(Zenith_Vector2 xCameraPosition, double fPixelsPerUnit,
		double fReferenceWidth, double fReferenceHeight, Zenith_Vector2 xWorld);
};

void Zenith_RegisterTwoDScriptBindings(Zenith_ScriptBindingTable& xTable);   // module start step 6
```

Luau names follow `Zenith2/Docs/Scripting/Bindings.md → "Name mapping from Zenith Hungarian"`: `TwoD.RayCast`, `TwoD.CircleCast`, `TwoD.OverlapBox`, `TwoD.ApplyImpulse`, `TwoD.ApplyForce`, `TwoD.SetVelocity`, `TwoD.SetKinematicTarget`, `TwoD.SetGravity`, `TwoD.Play`, `TwoD.SetParameter`, `TwoD.SetTrigger`, `TwoD.GetTile`, `TwoD.SetTile`, `TwoD.FillTiles`, `TwoD.WorldToCell`, `TwoD.GetBonePosition`, `TwoD.ReferenceToWorld`, `TwoD.WorldToReference`. Every parameter and result type has a row of `Zenith2/Docs/Scripting/Bindings.md → "Value marshalling"`: no reference out-parameter and no span crosses the binding. A cast returns the frozen table `Zenith.TwoD.RayHit` `{ Hit, Collider, Body, Point, Normal, Fraction, Cell = { X, Y, Layer, Valid } }`, always a table (test `Hit`), the shape of PHYS's `Zenith.Physics.RayHit`; `OverlapBox` returns `Zenith.TwoD.OverlapResult` `{ Count, Truncated, Colliders, Bodies }`. `TwoD.FillTiles` takes an inclusive rectangle and records the half-open `[x0, x1 + 1) × [y0, y1 + 1)` payload. The BUFFERED bindings append `SIDE_EFFECT_KIND_FIELD_COMMAND` records of TWOD's command kinds with the kind IDs of `Zenith_TwoDCommandKinds`, reached exactly as GAME's request bindings reach theirs (`Zenith2/Docs/Design/GAME/Scripting.md → "Binding registration"`; the binding-side route is row TD22). Sprite and camera fields are ordinary component fields scripts read and write through component views ([Z2-N-SCRIPT-562] does not apply: no world resource is read); the reflected enums `Sprite2DSpace`, `Sprite2DBlend`, `Sprite2DDrawMode`, `Camera2DScaleMode`, `Camera2DSnapMode`, `Body2DType`, `Shape2DKind` and `Joint2DKind` are exposed with `AddEnum`.

## Flux_Sprite2D header

The TWOD-owned public header of ZenithFlux. It declares the render-world element types TWOD's extractor writes, the GPU-side renderer that `Flux_Renderer::Enable2D` creates (RPROD's declaration, TWOD's body; cross-area row TD1), the frame it publishes through `Flux_FrameShared`, and the two feature functions the module's view extensions call. ZenithFlux includes no Zenith2D header ([Z2-N-ARCH-076]): every type here is self-contained.

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Sprite2D.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Types.h"
#include "ZenithFlux/Flux_TextureResource.h"     // Flux_TextureID
#include "ZenithFlux/Flux_RenderGraph.h"
#include "ZenithFlux/Flux_ViewContext.h"

class Flux_UploadRing;
struct Flux_RenderWorld;

/// @contract enum Flux_Sprite2DError
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: Each enumerator's meaning is the comment beside it.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Flux_Sprite2DError : uint8
{
	FLUX_SPRITE2D_ERROR_OUT_OF_MEMORY = 1,  // Create's allocation, or a persistent pool's creation, failed
	FLUX_SPRITE2D_ERROR_INVALID_DESC = 2,   // a desc member outside its domain
	FLUX_SPRITE2D_ERROR_DEVICE = 3,         // a device call of a creation step failed (the step is in the log)
};

/// @contract value Flux_Sprite2DRecord
/// Ownership: Plain value in the render world's slot arena, inside a Flux_RenderSprite2DList.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: an element its producer writes in place in an arena
///   or Zenith_Array.
/// Invalidation: Valid for the render world that carries it.
/// Units: Metres for m_afOrigin (x and y relative to the list's reference point, Flux_RenderSprite2DList::m_xCanvas's camera
///   position, or absolute when the list has no canvas; z absolute), m_afBasisX, m_afBasisY (world XY plane); page texels for
///   m_auTexelRect.
/// Errors: None; a null or stale m_xPage draws the white fallback for one frame and counts flux.twod.missing_page_delta.
/// Version: since Z2 Stage 3. Never serialized; Test_Sprite2DRecord_Layout pins the layout.
/// @endcontract
struct Flux_Sprite2DRecord   // 72 bytes, 8-byte aligned
{
	uint64 m_ulSortPrimary = 0;            // Rendering.md "Sort keys": layer << 56 | order << 40 | y << 16 | subOrder
	uint64 m_ulSimulationID = 0;           // the entity's stable simulation ID: the tie-break
	Flux_TextureID m_xPage;                 // GEOM's texture ID of the atlas page
	float m_afOrigin[3] = {};               // the quad corner (u, v) = (0, 0) after pivot, rotation, flip; x, y camera-relative
	uint32 m_uTintSrgb = 0xFFFFFFFFu;      // Zenith_ColourToSrgb8Packed of the tint: R bits 0–7, G, B, A (alpha linear)
	float m_afBasisX[2] = {};               // corner (1, 0) − corner (0, 0)
	float m_afBasisY[2] = {};               // corner (0, 1) − corner (0, 0)
	uint16 m_auTexelRect[4] = {};          // x0, y0, x1, y1 of the page; flips swap the pairs ("Pixel exactness rules")
	uint32 m_uFlags = 0;                   // bit 0 space (1 world), bits 1–2 blend, bit 3 snap, bit 4 point filter, bits 5–31 zero
	uint32 m_uReserved = 0;
};
static_assert(sizeof(Flux_Sprite2DRecord) == 72);

/// @contract value Flux_Sprite2DTilemapDraw
/// Ownership: Plain value in the slot arena, inside a Flux_RenderSprite2DList: one per visible tilemap layer.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: an element its producer writes in place in an arena
///   or Zenith_Array.
/// Invalidation: Valid for the render world that carries it.
/// Units: Metres; chunk coordinates.
/// Errors: None; a draw whose tilemap key or tileset key the renderer does not hold draws nothing (flux.twod.tilemap_unknown_delta).
/// Version: since Z2 Stage 4. Never serialized.
/// @endcontract
struct Flux_Sprite2DTilemapDraw   // 72 bytes
{
	uint64 m_ulSortPrimary = 0;
	uint64 m_ulSimulationID = 0;
	uint64 m_ulTilemapKey = 0;             // Flux_MakeModuleInstanceKey of the map entity's simulation ID and world tag
	uint64 m_ulTilesetKey = 0;             // the tileset asset's persistent-ID hash, bit 63 set
	float m_afOrigin[3] = {};               // bottom-left corner of cell (0, 0): x, y relative to the list's reference point, z
	                                        // absolute (the layer's z offset included)
	float m_afCellSize[2] = {};             // metres
	float m_afParallax[2] = { 1.0f, 1.0f }; // the layer's parallax factor (Rendering.md "Tile tables and the chunk pool")
	uint32 m_uTintSrgb = 0xFFFFFFFFu;      // the component tint; alpha multiplied by the layer's opacity
	uint8 m_uLayer = 0;                    // the map layer
	uint8 m_uFlags = 0;                    // bit 0 space (1 world), bits 1–2 blend, bit 3 snap, bit 4 point filter
	uint16 m_uReserved = 0;
	uint32 m_uReserved2 = 0;
};
static_assert(sizeof(Flux_Sprite2DTilemapDraw) == 72);

/// @contract enum Flux_Sprite2DScaleMode
/// Ownership: Plain enumeration; equal in value to Zenith_Camera2DScaleMode.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Flux_Sprite2DScaleMode : uint8 { FLUX_SPRITE2D_SCALE_INTEGER_LETTERBOX = 0, FLUX_SPRITE2D_SCALE_INTEGER_EXPAND = 1, FLUX_SPRITE2D_SCALE_NATIVE = 2 };

/// @contract enum Flux_Sprite2DSnapMode
/// Ownership: Plain enumeration; equal in value to Zenith_Camera2DSnapMode.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
enum Flux_Sprite2DSnapMode : uint8 { FLUX_SPRITE2D_SNAP_NONE = 0, FLUX_SPRITE2D_SNAP_TEXEL = 1, FLUX_SPRITE2D_SNAP_SUBPIXEL = 2 };

/// @contract value Flux_Sprite2DCanvasRecord
/// Ownership: Plain value inside a Flux_RenderSprite2DList.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: its producer default-constructs it and fills it
///   member by member.
/// Invalidation: Valid for the render world.
/// Units: Pixels; pixels per metre; metres; colours sRGB-encoded 8-bit.
/// Errors: None; the extractor validated the ranges.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_Sprite2DCanvasRecord   // 40 bytes
{
	double m_dCameraX = 0.0;                // the interpolated, bounds-clamped camera position: the list's reference point (binary64)
	double m_dCameraY = 0.0;
	uint32 m_uReferenceWidth = 320;
	uint32 m_uReferenceHeight = 180;
	float m_fPixelsPerUnit = 16.0f;
	uint32 m_uLetterboxSrgb = 0xFF000000u; // packed as Zenith_ColourToSrgb8Packed
	uint32 m_uClearSrgb = 0x00000000u;
	uint8 m_eScaleMode = FLUX_SPRITE2D_SCALE_INTEGER_LETTERBOX;
	uint8 m_eSnapMode = FLUX_SPRITE2D_SNAP_TEXEL;
	uint16 m_uReserved = 0;
};

/// @contract value Flux_RenderSprite2DList
/// Ownership: Plain value written once per world per frame by TwoD.Extract through Flux_RenderWorldWriter::AddSprite2DList;
///   its spans point into the writer partition's slot arena.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: its producer default-constructs it and fills it
///   member by member.
/// Invalidation: Valid for the render world.
/// Units: As the element types.
/// Errors: A list that does not fit the slot arena is refused whole (FLUX_RENDER_WORLD_ERROR_SLOT_FULL); that world draws no 2D
///   content that frame (twod.extract.slot_full_delta).
/// Version: since Z2 Stage 3. Whole-frame state, never a delta.
/// @endcontract
struct Flux_RenderSprite2DList
{
	uint16 m_uWorldTag = 0;
	uint16 m_uReserved = 0;
	uint32 m_uOutputViewKindMask = 0;      // 1 << VIEW_KIND_MAIN for a game world; the editor's mask per row TD8
	uint64 m_ulAnimationTick = 0;          // the frame's simulation tick: animated tiles' clock
	uint32 m_uTickDurationUs = 16667;      // the world's tick duration
	bool m_bHasCanvas = false;
	Flux_Sprite2DCanvasRecord m_xCanvas;
	Zenith_Span<const Flux_Sprite2DRecord> m_axSprites;           // unsorted, in extraction order
	Zenith_Span<const Flux_Sprite2DTilemapDraw> m_axTilemapDraws;
};

/// @contract enum Flux_TilemapChunkOp
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Flux_TilemapChunkOp : uint8 { FLUX_TILEMAP_CHUNK_OP_UPSERT = 1, FLUX_TILEMAP_CHUNK_OP_REMOVE_CHUNK = 2, FLUX_TILEMAP_CHUNK_OP_REMOVE_MAP = 3 };

/// @contract value Flux_RenderTilemapChunk
/// Ownership: Plain value written through Flux_RenderWorldWriter::AddTilemapChunk, a delta record; m_auCells is copied into the
///   slot arena by the writer.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: callers default-construct it and set the members
///   they need.
/// Invalidation: Valid for the render world.
/// Units: Chunk coordinates (cell / 32, floored).
/// Errors: A record whose cell span is not 1,024 cells for UPSERT is refused by the writer (FLUX_RENDER_WORLD_ERROR_SLOT_FULL is
///   not used for it: the extractor never produces one; Test_TilemapExtract_ChunkRecordShape asserts it).
/// Version: since Z2 Stage 4. Never serialized.
/// @endcontract
struct Flux_RenderTilemapChunk
{
	uint64 m_ulTilemapKey = 0;
	uint8 m_eOp = FLUX_TILEMAP_CHUNK_OP_UPSERT;
	uint8 m_uLayer = 0;
	uint16 m_uReserved = 0;
	sint32 m_iChunkX = 0;
	sint32 m_iChunkY = 0;
	uint32 m_uRevision = 0;                // incremented per edit of the chunk; diagnostics only
	uint64 m_ulContentHash = 0;            // UPSERT: Zenith_Hash64 of the 4,096 cell bytes; the mirror skips a re-upload when equal
	Zenith_Span<const uint32> m_auCells;   // UPSERT: 1,024 cells row-major from the chunk's bottom row; empty otherwise
};

/// @contract value Flux_TilesetTileEntry
/// Ownership: Plain value; one element of Flux_RenderTilesetRecord::m_axTiles, uploaded verbatim into the tile table.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: None.
/// Units: Page texels; animation frame durations in milliseconds.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_TilesetTileEntry   // 24 bytes; the GPU tile-table element (Rendering.md "Tile tables")
{
	Flux_TextureID m_xPage;
	uint16 m_auTexelRect[4] = {};
	uint16 m_uAnimFirstEntry = 0;          // index of its first animation-frame entry; 0 = not animated
	uint16 m_uAnimFrameCount = 0;
	uint16 m_uAnimFrameMs = 0;             // every frame's duration
	uint16 m_uFlags = 0;                   // bit 0 point filter
};

/// @contract value Flux_RenderTilesetRecord
/// Ownership: Plain value written through Flux_RenderWorldWriter::AddTilesetRecord, a delta record; the span is copied.
/// Thread affinity: IMMUTABLE after Render.Seal; members non-const: callers default-construct it and set the members
///   they need.
/// Invalidation: Valid for the render world.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Never serialized.
/// @endcontract
struct Flux_RenderTilesetRecord
{
	uint64 m_ulTilesetKey = 0;
	bool m_bRemove = false;
	Zenith_Span<const Flux_TilesetTileEntry> m_axTiles;   // entry 0 is the empty tile; then tiles 1 … n; then animation frames
};

/// @contract value Flux_Sprite2DCanvasGeometry
/// Ownership: Plain value returned by Flux_ComputeCanvasGeometry.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_Sprite2DCanvasGeometry
{
	uint32 m_uCanvasWidth = 0;             // the visible canvas, before the margin
	uint32 m_uCanvasHeight = 0;
	uint32 m_uMargin = 0;                  // 1 under FLUX_SPRITE2D_SNAP_SUBPIXEL, else 0
	uint32 m_uScale = 1;                   // the integer scale k (1 under NATIVE and under the downscale fallback)
	float m_fDownscale = 1.0f;              // < 1 only under the downscale fallback ("Canvas geometry" rule 5)
	sint32 m_iDestX = 0;                   // output pixel of the canvas's top-left visible texel
	sint32 m_iDestY = 0;
	uint32 m_uDestWidth = 0;               // k × canvas width (or the fallback's extent)
	uint32 m_uDestHeight = 0;
	float m_fWorldToCanvas = 16.0f;         // canvas pixels per metre
};

/// @contract functions Flux_Sprite2DCanvasFunctions
/// Ownership: Pure function.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Pixels.
/// Errors: None; a zero output extent returns a geometry with zero extents, which draws nothing.
/// Version: since Z2 Stage 3. The rules are Rendering.md "Canvas geometry"; Test_Sprite2DCanvasGeometry_Rules asserts them.
/// @endcontract
Flux_Sprite2DCanvasGeometry Flux_ComputeCanvasGeometry(const Flux_Sprite2DCanvasRecord& xCanvas,
	uint32 uOutputWidth, uint32 uOutputHeight);

/// @contract value Flux_Sprite2DRendererDesc
/// Ownership: Plain value read by Flux_Renderer::Enable2D.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Counts and bytes.
/// Errors: Enable2D returns FLUX_RENDERER_ERROR_CONFIGURATION for a member outside its range (mapped from
///   FLUX_SPRITE2D_ERROR_INVALID_DESC).
/// Version: since Z2 Stage 3; the tilemap members since Stage 4.
/// @endcontract
struct Flux_Sprite2DRendererDesc
{
	uint32 m_uMaxSpritesPerFrame = 262144;        // twod.render.maxSpritesPerFrame, 1,024 … 1,048,576
	uint32 m_uMaxCanvasesPerFrame = 4;            // twod.render.maxCanvasesPerFrame, 1 … 8
	uint32 m_uTileChunkPoolChunks = 4096;         // twod.render.tileChunkPoolChunks, 64 … 65,536 (4 KiB each, GPU)
	uint32 m_uTileChunkMirrorChunks = 8192;       // twod.render.tileChunkMirrorChunks, 64 … 65,536 (4 KiB each, CPU)
	uint32 m_uTileTableEntries = 262144;          // twod.render.tileTableEntries, 1,024 … 1,048,576 (24 B each)
	uint32 m_uMaxChunkUploadsPerFrame = 64;       // twod.render.maxChunkUploadsPerFrame, 1 … 4,096 (GPU uploads)
};

/// @contract type Flux_Sprite2DFrame
/// Ownership: Owned by Flux_Sprite2DRenderer; one per frame, valid from Flux.BuildGraph step 4 to the end of the frame's
///   recording; reached by feature functions through Flux_FrameShared::m_px2DFrame alone (cross-doc row in TWOD.md).
/// Thread affinity: IMMUTABLE for the feature functions and record jobs after step 4 (record jobs read its per-batch
///   offsets after step 11 wrote them).
/// Invalidation: Invalid after the frame's last record job.
/// Units: Instances; bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_Sprite2DFrame;              // opaque outside ZenithFlux/Internal/Sprite2D (Rendering.md "Frame data path")

/// @contract value Flux_Sprite2DStats
/// Ownership: Plain value owned by the renderer, rewritten at each PrepareFrame.
/// Thread affinity: IMMUTABLE between two PrepareFrame calls; members non-const: its producer default-constructs it and
///   fills it member by member.
/// Invalidation: Overwritten by the next PrepareFrame.
/// Units: Counts; bytes.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Flux_Sprite2DStats
{
	uint32 m_uSpritesSubmitted = 0;        // records read from the render world
	uint32 m_uSpritesCulled = 0;
	uint32 m_uSpritesDrawn = 0;            // instances written, summed over canvases and the world list
	uint32 m_uSpritesDropped = 0;          // over m_uMaxSpritesPerFrame
	uint32 m_uBatches = 0;
	uint32 m_uCanvases = 0;
	uint32 m_uTileChunksResident = 0;
	uint32 m_uTileChunkUploads = 0;
	uint32 m_uTileChunksRefused = 0;       // GPU pool full of wanted chunks
	uint32 m_uTileChunksEvicted = 0;
	uint32 m_uTileChunksMirrored = 0;
	uint64 m_ulFrameDataBytes = 0;
	uint64 m_ulResidentBytes = 0;          // the CPU mirrors plus the GPU pool and tile table (the pending row TWOD-008)
};

/// @contract type Flux_Sprite2DRenderer
/// Ownership: Created by Flux_Renderer::Enable2D (RPROD's declaration, TWOD's body) as a Zenith_Owned member of the renderer,
///   destroyed in the renderer's destructor before the GEOM managers; owns the tile chunk pool, the tile tables and the
///   per-frame Flux_Sprite2DFrame.
/// Thread affinity: RENDER_SUBMIT_THREAD: PrepareFrame at Flux.BuildGraph step 4, WriteFrame at step 11, EndFrame at step 13,
///   OnDeviceRecovered from the renderer's recovery; GetFrame and GetStats ANY_THREAD after step 4.
/// Invalidation: GetFrame's pointer is valid for the frame.
/// Units: Bytes; instances.
/// Errors: Create returns Flux_Sprite2DError after unwinding its steps; per-frame capacity overflows are counted, never returned.
/// Reentrancy: NOT_APPLICABLE.
/// Version: since Z2 Stage 3.
/// @endcontract
class Flux_Sprite2DRenderer
{
public:
	static Zenith_Result<Zenith_Owned<Flux_Sprite2DRenderer>, Flux_Sprite2DError> Create(
		const Flux_Sprite2DRendererDesc& xDesc, Zenith_Heap& xHeap);
	~Flux_Sprite2DRenderer();
	void PrepareFrame(const Flux_RenderWorld& xWorld, Flux_UploadRing& xRing);     // step 4
	void WriteFrame(Flux_UploadRing& xRing);                                       // step 11, after FITS or EXTENDED
	void EndFrame(bool bSubmitted);                                                // step 13, every path
	void OnDeviceRecovered();                                                      // drops every pool slot; the full-state world re-adds them
	const Flux_Sprite2DFrame* GetFrame() const;
	const Flux_Sprite2DStats& GetStats() const;
};

/// @contract functions Flux_Sprite2DFeatureFunctions
/// Ownership: Declare passes on the frame's graph; read Flux_FrameShared::m_px2DFrame and the view's source world tag.
/// Thread affinity: The Flux.BuildGraph node, through the module's view extensions.
/// Invalidation: None.
/// Units: None.
/// Errors: None returned; graph diagnostics only. Each adds no pass when m_px2DFrame is null or the view has no 2D content.
/// Version: since Z2 Stage 3. The S2 API slice defines both as functions that add no pass, so path and extension code links
///   before the implementation lands (the declare-first rule).
/// @endcontract
// FLUX_EXTENSION_POINT_BEFORE_UI (cross-area row TD2): the view's canvas passes Sprite2DScatter (when chunks changed),
// Sprite2DCanvas and Sprite2DComposite, writing xTargets.m_xOutput.
void Flux_AddSprite2DCanvas(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
// FLUX_EXTENSION_POINT_TRANSPARENT: Sprite2DWorld, drawing the world list (and canvas content in a view without a canvas) into
// xTargets.m_xSceneColour with a read-only depth test.
void Flux_AddSprite2DWorld(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

The writer members, the three section spans and the `Flux_FrameShared` member are Rendering's declarations, which the core-doc batch adds from the cross-doc item TWOD files (`Zenith2/Docs/Design/TWOD.md → "Cross-area reconciliation"`):

<!-- assumption: pending REND, RPROD (rows TD1, TD2; cross-doc item X295) -->
```cpp
// Flux_RenderWorldSection gains, after m_axEnvironmentViews:
	Zenith_Span<const Flux_RenderSprite2DList> m_axSprite2DLists;     // TWOD's element types (Flux_Sprite2D.h) from here on
	Zenith_Span<const Flux_RenderTilemapChunk> m_axTilemapChunks;
	Zenith_Span<const Flux_RenderTilesetRecord> m_axTilesetRecords;
// Flux_RenderWorldWriter gains:
	Zenith_Result<void, Flux_RenderWorldError> AddSprite2DList(const Flux_RenderSprite2DList& xList);
	Zenith_Result<void, Flux_RenderWorldError> AddTilemapChunk(const Flux_RenderTilemapChunk& xChunk);
	Zenith_Result<void, Flux_RenderWorldError> AddTilesetRecord(const Flux_RenderTilesetRecord& xRecord);
// Flux_FrameShared gains:
	const Flux_Sprite2DFrame* m_px2DFrame;   // TWOD's frame (Flux_Sprite2DRenderer::GetFrame), set before any feature function when Enable2D ran; null otherwise
// Flux_Renderer gains (RPROD's declaration, TWOD's body in Internal/Renderer/Flux_Renderer.cpp):
	Zenith_Result<Flux_Sprite2DRenderer*, Flux_RendererError> Enable2D(const Flux_Sprite2DRendererDesc& xDesc);
// Flux_ExtensionPoint gains:
	FLUX_EXTENSION_POINT_BEFORE_UI = 5,          // output extent, after TonemapAndGrade (or where it would run), before UIComposite
```
Each typed writer member copies the record and every span it holds into the partition's slot arena, exactly as Rendering's "Render-world record types" rule states for ENV's records; a record or span that does not fit is refused whole with `FLUX_RENDER_WORLD_ERROR_SLOT_FULL`.
<!-- /assumption -->
