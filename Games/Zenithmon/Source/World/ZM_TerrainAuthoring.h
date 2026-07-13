#pragma once

#include "Collections/Zenith_Vector.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"

#include <filesystem>
#include <string>

// ============================================================================
// ZM_TerrainAuthoring -- deterministic, WorldSpec-driven outdoor terrain
// recipes. The recipe is immutable; both editor automation and the tests walk
// the same ordered plan, so content cannot silently drift away from its gate.
// Generated terrain assets remain workspace-local and gitignored.
// ============================================================================

struct ZM_TerrainPoint2
{
	float m_fX;
	float m_fZ;
};

struct ZM_TerrainPoint3
{
	float m_fX;
	float m_fY;
	float m_fZ;
};

struct ZM_TerrainExportRect
{
	int m_iMinX;
	int m_iMinY;
	int m_iMaxX;
	int m_iMaxY;
};

struct ZM_TerrainProceduralSpec
{
	float m_fBaseHeight;
	float m_fAmplitude;
	float m_fFrequency;
	u_int m_uOctaves;
	float m_fLacunarity;
	float m_fGain;
	float m_fRidgedBlend;
};

struct ZM_TerrainLandformSpec
{
	ZM_TerrainPoint2 m_xCentre;
	float m_fRadius;
	float m_fStrength;
	float m_fHeight;
};

struct ZM_TerrainPathSpec
{
	const char* m_szName;
	const ZM_TerrainPoint2* m_pxPoints;
	u_int m_uPointCount;
	float m_fFlattenRadius;
	float m_fFlattenSpacing;
	u_int m_uFlattenSampleCount;
	float m_fDirtRadius;
	float m_fDirtSpacing;
	u_int m_uDirtSampleCount;
};

struct ZM_TerrainPadSpec
{
	const char* m_szName;
	ZM_TerrainPoint2 m_xCentre;
	float m_fFlattenRadius;
	float m_fDirtRadius;
	u_int m_uDirtPassCount;
};

struct ZM_TerrainErosionSpec
{
	u_int m_uHydraulicDroplets;
	u_int m_uThermalIterations;
	bool m_bRegionOnly;
	ZM_TerrainPoint2 m_xCentre;
	float m_fRadius;
};

struct ZM_TerrainAutoSplatSpec
{
	const char* m_szName;
	float m_fHeightMin;
	float m_fHeightMax;
	float m_fSlopeMin;
	float m_fSlopeMax;
	float m_fWeight;
	float m_fJitter;
};

struct ZM_TerrainGrassDabSpec
{
	ZM_TerrainPoint2 m_xCentre;
	float m_fRadius;
	float m_fTargetDensity;
};

struct ZM_TerrainLandmarkSpec
{
	const char* m_szName;
	ZM_TerrainPoint3 m_xPosition;
};

struct ZM_TerrainMaterialSpec
{
	const char* m_szName;
	float m_afBaseColour[4];
	float m_fRoughness;
	float m_fMetallic;
};

struct ZM_TerrainPreviewCameraSpec
{
	ZM_TerrainPoint3 m_xPosition;
	float m_fYaw;
	float m_fPitch;
	float m_fFovDegrees;
	float m_fNearPlane;
	float m_fFarPlane;
};

struct ZM_TerrainAuthoringRecipe
{
	const ZM_WorldSpec* m_pxWorldSpec;
	u_int m_uSeed;
	// Exported authoring bounds are rectangular. The anonymous aliases retain
	// source compatibility with Dawnmere's original square-only contract while
	// making X/Z containment explicit for route recipes.
	union
	{
		float m_fWorldMinX;
		float m_fWorldMin;
	};
	union
	{
		float m_fWorldMaxX;
		float m_fWorldMax;
	};
	float m_fWorldMinZ;
	float m_fWorldMaxZ;
	ZM_TerrainExportRect m_xExportRect;
	float m_fTargetHeight;
	ZM_TerrainProceduralSpec m_xProcedural;
	const ZM_TerrainLandformSpec* m_pxLandforms;
	u_int m_uLandformCount;
	const ZM_TerrainPathSpec* m_pxPaths;
	u_int m_uPathCount;
	const ZM_TerrainPadSpec* m_pxPads;
	u_int m_uPadCount;
	ZM_TerrainErosionSpec m_xErosion;
	const ZM_TerrainAutoSplatSpec* m_pxAutoSplat;
	u_int m_uAutoSplatCount;
	const ZM_TerrainGrassDabSpec* m_pxGrassDabs;
	u_int m_uGrassDabCount;
	const ZM_TerrainLandmarkSpec* m_pxLandmarks;
	u_int m_uLandmarkCount;
	const ZM_TerrainMaterialSpec* m_pxMaterials;
	u_int m_uMaterialCount;
	ZM_TerrainPreviewCameraSpec m_xPreviewCamera;
};

enum ZM_TERRAIN_PLAN_OP_TYPE : u_int
{
	ZM_TERRAIN_PLAN_SET_ASSET_SET,
	ZM_TERRAIN_PLAN_RESET,
	ZM_TERRAIN_PLAN_GENERATE_PROCEDURAL,
	ZM_TERRAIN_PLAN_BRUSH_DAB,
	ZM_TERRAIN_PLAN_EROSION,
	ZM_TERRAIN_PLAN_AUTO_SPLAT_RULE,
	ZM_TERRAIN_PLAN_RUN_AUTO_SPLAT,
	// One checked terminal action: textures -> bounded meshes -> manifest.
	// Scene authoring is intentionally deferred to the next warm boot.
	ZM_TERRAIN_PLAN_TERMINAL_BAKE,
};

enum ZM_TERRAIN_DAB_KIND : u_int
{
	ZM_TERRAIN_DAB_SET_HEIGHT,
	ZM_TERRAIN_DAB_FLATTEN,
	ZM_TERRAIN_DAB_SPLAT,
	ZM_TERRAIN_DAB_GRASS_DENSITY,
};

enum ZM_TERRAIN_PLAN_PHASE : u_int
{
	ZM_TERRAIN_PHASE_NONE,
	ZM_TERRAIN_PHASE_LANDFORM,
	ZM_TERRAIN_PHASE_FLATTEN_PRE_EROSION,
	ZM_TERRAIN_PHASE_FLATTEN_POST_EROSION,
	ZM_TERRAIN_PHASE_DIRT,
	ZM_TERRAIN_PHASE_GRASS_FILL,
	ZM_TERRAIN_PHASE_GRASS_ERASE,
};

struct ZM_TerrainPlanOp
{
	ZM_TERRAIN_PLAN_OP_TYPE m_eType = ZM_TERRAIN_PLAN_RESET;
	ZM_TERRAIN_DAB_KIND m_eDabKind = ZM_TERRAIN_DAB_SET_HEIGHT;
	ZM_TERRAIN_PLAN_PHASE m_ePhase = ZM_TERRAIN_PHASE_NONE;
	u_int m_uIndex = 0;
	float m_fWorldX = 0.0f;
	float m_fWorldZ = 0.0f;
	float m_fRadius = 0.0f;
	float m_fStrength = 0.0f;
	float m_fValue = 0.0f;
};

enum ZM_TERRAIN_BAKE_QUEUE_RESULT : u_int
{
	ZM_TERRAIN_BAKE_HEADLESS,
	ZM_TERRAIN_BAKE_WARM,
	ZM_TERRAIN_BAKE_QUEUED,
	ZM_TERRAIN_BAKE_PREPARE_FAILED,
};

enum ZM_TERRAIN_BAKE_SELECTION_MODE : u_int
{
	ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING,
	ZM_TERRAIN_BAKE_SELECTION_FORCE_ALL,
	ZM_TERRAIN_BAKE_SELECTION_FORCE_SELECTED,
};

enum ZM_TERRAIN_BAKE_SELECTION_PARSE_RESULT : u_int
{
	ZM_TERRAIN_BAKE_SELECTION_PARSE_OK,
	ZM_TERRAIN_BAKE_SELECTION_PARSE_MALFORMED,
	ZM_TERRAIN_BAKE_SELECTION_PARSE_UNKNOWN_SET,
	ZM_TERRAIN_BAKE_SELECTION_PARSE_DUPLICATE,
	ZM_TERRAIN_BAKE_SELECTION_PARSE_CONFLICT,
};

struct ZM_TerrainBakeSelection
{
	ZM_TERRAIN_BAKE_SELECTION_MODE m_eMode =
		ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING;
	u_int m_uSelectedRecipeMask = 0u;
	int m_iErrorArgument = -1;
	ZM_TERRAIN_BAKE_SELECTION_PARSE_RESULT m_eParseResult =
		ZM_TERRAIN_BAKE_SELECTION_PARSE_OK;
};

struct ZM_TerrainBakeBatchPlan
{
	u_int m_uWarmRecipeMask = 0u;
	u_int m_uQueueRecipeMask = 0u;
	bool m_bAllWarm = false;
	bool m_bAuthorDawnmereScene = false;
};

constexpr u_int uZM_TERRAIN_MANIFEST_VERSION = 1u;
constexpr u_int uZM_TERRAIN_RECIPE_COUNT = 3u;
constexpr u_int uZM_DAWNMERE_REQUIRED_OUTPUT_COUNT = 771u;
constexpr u_int uZM_THORNACRE_REQUIRED_OUTPUT_COUNT = 771u;
constexpr u_int uZM_ROUTE1_REQUIRED_OUTPUT_COUNT = 1155u;
constexpr u_int uZM_TERRAIN_MANIFEST_SIZE = 12u;
constexpr const char* szZM_FORCE_TERRAIN_BAKE_FLAG = "--zm-force-terrain-bake";

u_int ZM_Fnv1a32(const char* szText);
u_int ZM_GetTerrainAuthoringRecipeCount();
const ZM_TerrainAuthoringRecipe& ZM_GetTerrainAuthoringRecipe(u_int uIndex);
const ZM_TerrainAuthoringRecipe* ZM_FindTerrainAuthoringRecipe(ZM_SCENE_ID eSceneId);
const ZM_TerrainAuthoringRecipe& ZM_GetDawnmereTerrainRecipe();
const ZM_TerrainAuthoringRecipe& ZM_GetThornacreTerrainRecipe();
const ZM_TerrainAuthoringRecipe& ZM_GetRoute1TerrainRecipe();

// Parse the repeatable terrain-bake selector without inspecting or mutating
// terrain state. An absent flag selects missing recipes; the bare flag forces
// all recipes; '=Set' arguments select exactly those case-sensitive sets.
// Invalid input stops at and records the first offending argv index.
bool ZM_ParseTerrainBakeSelection(int iArgumentCount,
	const char* const* pszArguments, ZM_TerrainBakeSelection& xSelectionOut);
const char* ZM_TerrainBakeSelectionModeToString(
	ZM_TERRAIN_BAKE_SELECTION_MODE eMode);
const char* ZM_TerrainBakeSelectionParseResultToString(
	ZM_TERRAIN_BAKE_SELECTION_PARSE_RESULT eResult);

// Pure batch policy. The caller supplies the complete pre-scan warm mask;
// headless callers receive an empty plan and must not perform that scan.
ZM_TerrainBakeBatchPlan ZM_BuildTerrainBakeBatchPlan(
	const ZM_TerrainBakeSelection& xSelection, bool bHeadless,
	u_int uWarmRecipeMask);

// Build the complete ordered recipe plan used by editor automation. Repeated
// calls are byte-for-byte field deterministic; no values depend on clocks or
// container iteration order.
void ZM_BuildTerrainAuthoringPlan(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<ZM_TerrainPlanOp>& xPlanOut);

// Relative to the game Assets root, in stable export order. Every entry must
// exist and be non-empty before a bake can be marked warm.
void ZM_EnumerateRequiredTerrainOutputs(const ZM_TerrainAuthoringRecipe& xRecipe,
	Zenith_Vector<std::string>& xOutputsOut);
u_int ZM_GetTerrainRequiredOutputCount(const ZM_TerrainAuthoringRecipe& xRecipe);
std::string ZM_GetTerrainManifestRelativePath(const ZM_TerrainAuthoringRecipe& xRecipe);
std::string ZM_GetTerrainGrassAssetPath(const ZM_TerrainAuthoringRecipe& xRecipe);

// Pure four-state policy used by the queue and its headless unit gate.
// bPrepareSucceeded is consulted only for a cold/forced regeneration.
ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_DetermineTerrainBakeQueueResult(bool bHeadless,
	bool bForce, bool bWarm, bool bPrepareSucceeded);
bool ZM_IsTerrainBakeWarm(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot);

#ifdef ZENITH_TOOLS
// Prepare removes stale marker/temp files and all three old textures before
// any forced or incomplete regeneration. A failure queues no work. Finalize
// verifies the recipe-specific output family, writes its required count into a
// 12-byte temp marker, then atomically renames it as the final bake write.
bool ZM_PrepareTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot);
bool ZM_FinalizeTerrainBake(const ZM_TerrainAuthoringRecipe& xRecipe,
	const std::filesystem::path& xGameAssetsRoot);

class Zenith_EditorAutomation;

// Queue the complete terrain-only recipe. QUEUED means this boot must stop
// after the bake; the next WARM boot may author its scene. Only immutable
// registry recipes are accepted; callbacks are fixed per recipe and retain no
// mutable queued recipe pointer.
ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_QueueTerrainBake(Zenith_EditorAutomation& xAutomation,
	const ZM_TerrainAuthoringRecipe& xRecipe, bool bHeadless, bool bForce);

// Compatibility wrapper for the first shipped terrain consumer.
ZM_TERRAIN_BAKE_QUEUE_RESULT ZM_QueueDawnmereTerrainBake(Zenith_EditorAutomation& xAutomation,
	bool bHeadless, bool bForce);
#endif
