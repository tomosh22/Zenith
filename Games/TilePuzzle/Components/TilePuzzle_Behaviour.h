#pragma once
/**
 * TilePuzzle_Behaviour.h - Main game coordinator
 *
 * A sliding tile puzzle where players drag colored shapes onto matching colored cats.
 * Shapes can be multi-cube polyominos. Win by eliminating all cats.
 *
 * Architecture:
 * - GameManager entity (persistent): camera + UI + script
 * - Puzzle scene (created/destroyed per level): floor, shapes, cats
 *
 * State machine: MAIN_MENU -> PLAYING -> LEVEL_COMPLETE -> (next level / menu)
 */

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Flux/Flux_ModelInstance.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIImage.h"
#include "UI/Zenith_UILayoutGroup.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIToggle.h"
#include "UI/Zenith_UIOverlay.h"
#include "Flux/Text/Flux_TextImpl.h"

#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Rules.h"
#include "TilePuzzle/Components/TilePuzzleLevelData_Serialize.h"
#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "TilePuzzle/Components/TilePuzzle_Solver.h"
#include "SaveData/Zenith_SaveData.h"
#include "Input/Zenith_TouchInputImpl.h"
#include "UI/Zenith_UICanvas.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "Collections/Zenith_Vector.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"

#include "TaskSystem/Zenith_TaskSystem.h"

#include <unordered_map>
#include <utility>
#include <cmath>
#include <ctime>

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// TilePuzzle Resources - Phase 8 per-game ProjectResources struct.
// ============================================================================
class Flux_ParticleEmitterConfig;
class Zenith_MaterialAsset;
namespace TilePuzzle
{
	struct TilePuzzleResources
	{
		// Shared geometry assets (registry-managed via handles).
		MeshGeometryHandle  m_xCubeAsset;
		MeshGeometryHandle  m_xSphereAsset;
		Flux_MeshGeometry*  m_pxCubeGeometry    = nullptr;
		Flux_MeshGeometry*  m_pxSphereGeometry  = nullptr;
		Flux_MeshGeometry*  m_pxCatMeshGeometry = nullptr;

		// Floor + blocker materials.
		MaterialHandle      m_xFloorMaterial;
		MaterialHandle      m_xBlockerMaterial;

		// Colored shape / cat materials + cat-cafe display materials/textures.
		MaterialHandle      m_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
		MaterialHandle      m_axCatMaterials[TILEPUZZLE_COLOR_COUNT];
		MaterialHandle      m_axCatCafeDisplayMaterials[TILEPUZZLE_COLOR_COUNT];
		TextureHandle       m_axCatCafeFaceTextures[TILEPUZZLE_COLOR_COUNT];

		// Prefabs.
		PrefabHandle        m_xCellPrefab;
		PrefabHandle        m_xShapeCubePrefab;
		PrefabHandle        m_xCatPrefab;

		// Pre-generated merged shape meshes.
		Flux_MeshGeometry*  m_apxShapeMeshes[TILEPUZZLE_SHAPE_COUNT] = {};

		// Highlight emissive intensity.
		float               m_fHighlightEmissiveIntensity = 0.5f;

		// UI Icon textures.
		TextureHandle       m_xIconStarFilled;
		TextureHandle       m_xIconStarEmpty;
		TextureHandle       m_xIconCoin;
		TextureHandle       m_xIconHeart;
		TextureHandle       m_xIconUndo;
		TextureHandle       m_xIconSkip;
		TextureHandle       m_xIconLock;
		TextureHandle       m_xIconMenu;
		TextureHandle       m_xIconBack;
		TextureHandle       m_xIconSoundOn;
		TextureHandle       m_xIconSoundOff;
		TextureHandle       m_xIconReset;
		TextureHandle       m_xIconGear;
		TextureHandle       m_xIconCatSilhouette;
		TextureHandle       m_xIconHint;
		TextureHandle       m_xIconHintToken;

		// Cat face textures (one per color).
		TextureHandle       m_axCatFaceTextures[TILEPUZZLE_COLOR_COUNT];

		// Gameplay textures.
		TextureHandle       m_xFloorTileTexture;
		TextureHandle       m_xBlockerTexture;

		// Pinball materials (loaded from .zmtrl files).
		Zenith_MaterialAsset* m_pxPinballBallMaterial   = nullptr;
		Zenith_MaterialAsset* m_pxPinballPegMaterial    = nullptr;
		Zenith_MaterialAsset* m_pxPinballPegHitMaterial = nullptr;

		// Pinball PBR textures.
		TextureHandle       m_xPinballBumperDiffuseTex;
		TextureHandle       m_xPinballBumperRMTex;
		TextureHandle       m_xPinballWallDiffuseTex;
		TextureHandle       m_xPinballWallRMTex;
		TextureHandle       m_xPinballFloorDiffuseTex;
		TextureHandle       m_xPinballFloorRMTex;
		TextureHandle       m_xPinballPlungerRMTex;
		TextureHandle       m_xPinballTargetDiffuseTex;

		// Pinball custom meshes.
		Flux_MeshGeometry*  m_pxBumperGeometry      = nullptr;
		Flux_MeshGeometry*  m_pxBeveledCubeGeometry = nullptr;
		Flux_MeshGeometry*  m_pxPlungerGeometry     = nullptr;
		Flux_MeshGeometry*  m_pxTargetRampGeometry  = nullptr;

		// Particle configs.
		Flux_ParticleEmitterConfig* m_pxEliminationParticleConfig = nullptr;
		Flux_ParticleEmitterConfig* m_pxVictoryConfettiConfig     = nullptr;
	};

	TilePuzzleResources& Resources();

	void GenerateShapeMeshFromDefinition(const TilePuzzleShapeDefinition& xDef, Flux_MeshGeometry& xGeometryOut);
}

namespace TilePuzzleUI
{
	// Tutorial overlay
	static constexpr float fTUTORIAL_FONT = 48.f;
	static constexpr float fTUTORIAL_HINT_FONT = 36.f;
	static constexpr float fTUTORIAL_OVERLAY_W = 810.f;
	static constexpr float fTUTORIAL_OVERLAY_H = 180.f;

	// Achievement toast
	static constexpr float fTOAST_FONT = 40.f;
	static constexpr float fTOAST_BANNER_H = 90.f;

	// Achievements screen
	static constexpr float fACHIEV_TITLE_FONT = 56.f;
	static constexpr float fACHIEV_NAME_FONT = 36.f;
	static constexpr float fACHIEV_DESC_FONT = 34.f;
	static constexpr float fACHIEV_ICON_FONT = 42.f;
	static constexpr float fACHIEV_ITEM_H = 100.f;
	static constexpr float fACHIEV_RETURN_FONT = 36.f;

	// "Need a hint?" prompt
	static constexpr float fHINT_PROMPT_FONT = 44.f;

	// MetaGame: Cat cards (runtime overrides)
	static constexpr float fCAT_CARD_COLLECTED_FONT = 32.f;
	static constexpr float fCAT_CARD_LOCKED_FONT = 42.f;

	// MetaGame: Victory title (runtime override by star rating)
	static constexpr float fVICTORY_TITLE_3STAR = 72.f;
	static constexpr float fVICTORY_TITLE_2STAR = 64.f;
	static constexpr float fVICTORY_TITLE_1STAR = 56.f;
	static constexpr float fVICTORY_CONTENT_W = 600.f;
	static constexpr float fVICTORY_CONTENT_H = 340.f;
	static constexpr float fVICTORY_CONTENT_SPACING = 26.f;
	static constexpr float fNEXT_LEVEL_BTN_Y = 195.f;

	// MetaGame: Weekly challenge banner
	static constexpr float fWEEKLY_BANNER_H = 120.f;
	static constexpr float fWEEKLY_BANNER_BOTTOM_OFFSET = 135.f;
	static constexpr float fWEEKLY_TITLE_FONT = 36.f;
	static constexpr float fWEEKLY_DESC_FONT = 34.f;
	static constexpr float fWEEKLY_PROGRESS_FONT = 32.f;
	static constexpr float fWEEKLY_BAR_H = 22.f;
}

// Forward declaration for level select button user data
class TilePuzzle_Behaviour;

// User data for level select buttons
struct TilePuzzleLevelButtonData
{
	TilePuzzle_Behaviour* pxBehaviour;
	uint32_t uLevelNumber;
};
static TilePuzzleLevelButtonData s_axLevelButtonData[20];

// Configuration constants
static constexpr uint32_t s_uMaxGridSize = 12;
static constexpr float s_fSlideAnimationDuration = 0.15f;
static constexpr float s_fEliminationDuration = 0.3f;
static constexpr float s_fCellSize = 1.0f;
static constexpr float s_fFloorHeight = 0.05f;
static constexpr float s_fShapeHeight = 0.25f;
static constexpr float s_fCatHeight = 0.35f;
static constexpr float s_fCatRadius = 0.35f;

// Puzzle gameplay costs
static constexpr uint32_t s_uUndoCoinCost = 20;
static constexpr uint32_t s_uHintCoinCost = 30;
static constexpr uint32_t s_uSkipCoinCost = 100;
static constexpr uint32_t s_uResetsBeforeSkipOffer = 3;

// ============================================================================
// Cat Data Pools
// ============================================================================
static constexpr uint32_t s_uCatNameCount = 100;
static const char* s_aszCatNames[s_uCatNameCount] = {
	"Whiskers", "Mittens", "Shadow", "Luna", "Ginger",
	"Patches", "Smokey", "Tiger", "Cleo", "Mochi",
	"Noodle", "Biscuit", "Pepper", "Marble", "Cinnamon",
	"Oreo", "Pumpkin", "Willow", "Ziggy", "Felix",
	"Clover", "Jasper", "Hazel", "Cosmo", "Maple",
	"Pickles", "Mango", "Basil", "Nutmeg", "Waffles",
	"Sprout", "Olive", "Truffle", "Toffee", "Butterscotch",
	"Mocha", "Latte", "Espresso", "Chai", "Caramel",
	"Socks", "Boots", "Domino", "Checkers", "Phantom",
	"Ember", "Ash", "Slate", "Flint", "Cobalt",
	"Saffron", "Clementine", "Tangerine", "Peach", "Apricot",
	"Velvet", "Silk", "Satin", "Cashmere", "Chenille",
	"Nimbus", "Cirrus", "Stratus", "Misty", "Foggy",
	"Cricket", "Sparrow", "Finch", "Wren", "Robin",
	"Pudding", "Crumble", "Scone", "Brioche", "Croissant",
	"Pixel", "Widget", "Gadget", "Rascal", "Bandit",
	"Sage", "Thyme", "Rosemary", "Dill", "Fennel",
	"Copper", "Bronze", "Sterling", "Pewter", "Onyx",
	"Breezy", "Sunny", "Stormy", "Frosty", "Dusty",
	"Pip", "Kit", "Dot", "Dash", "Jinx"
};

static constexpr uint32_t s_uCatBreedCount = 20;
static const char* s_aszCatBreeds[s_uCatBreedCount] = {
	"Tabby", "Calico", "Siamese", "Persian", "Bengal",
	"Ragdoll", "Sphynx", "Maine Coon", "British Shorthair", "Abyssinian",
	"Scottish Fold", "Russian Blue", "Norwegian Forest", "Birman", "Burmese",
	"Tonkinese", "Chartreux", "Turkish Van", "Somali", "Manx"
};

// ============================================================================
// Coin Award Constants
// ============================================================================
static constexpr uint32_t s_uCoinsPerLevelComplete = 10;
static constexpr uint32_t s_uCoinsPerThreeStar = 5;
static constexpr uint32_t s_uCoinsPerDailyPuzzle = 50;

// ============================================================================
// Timestamp Utility
// ============================================================================
static uint32_t GetCurrentDateYYYYMMDD()
{
	time_t xNow = time(nullptr);
	struct tm xTm;
#ifdef ZENITH_WINDOWS
	localtime_s(&xTm, &xNow);
#else
	localtime_r(&xNow, &xTm);
#endif
	return static_cast<uint32_t>((xTm.tm_year + 1900) * 10000 + (xTm.tm_mon + 1) * 100 + xTm.tm_mday);
}

static uint32_t GetCurrentTimestamp()
{
	return static_cast<uint32_t>(time(nullptr));
}

// Snapshot of game state for undo stack
struct TilePuzzleUndoState
{
	Zenith_Vector<std::pair<int32_t, int32_t>> axShapePositions;  // (iOriginX, iOriginY) per shape
	Zenith_Vector<bool> abCatEliminated;                          // per cat
	Zenith_Vector<bool> abShapeRemoved;                           // per shape
	uint32_t uMoveCount;
	int32_t iLastMovedShapeIndex;
};

// Data passed to/from the background hint solver task
struct HintSolverData
{
	TilePuzzleLevelData xLevelState;                    // Input: copy of current state
	std::vector<TilePuzzleSolutionMove> axSolution;     // Output: solution path
	int32_t iResult = -1;                               // Output: solver return value
	std::atomic<bool> bComplete{false};                 // Completion flag
};

// ============================================================================
// Main Behavior Class
// ============================================================================
class TilePuzzle_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
#ifdef ZENITH_INPUT_SIMULATOR
	friend class TilePuzzle_AutoTest;
#endif
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(TilePuzzle_Behaviour)

	TilePuzzle_Behaviour() = delete;
	TilePuzzle_Behaviour(Zenith_Entity& /*xParentEntity*/)
		: m_eState(TILEPUZZLE_STATE_MAIN_MENU)
		, m_uCurrentLevelNumber(1)
		, m_uMoveCount(0)
		, m_iSelectedShapeIndex(-1)
		, m_fSlideProgress(0.0f)
		, m_eSlideDirection(TILEPUZZLE_DIR_NONE)
		, m_fLevelTimer(0.f)
		, m_uLevelSelectPage(0)
		, m_uAvailableLevelCount(0)
		, m_bFreeUndoAvailable(true)
		, m_bHintActive(false)
		, m_iHintShapeIndex(-1)
		, m_fHintFlashTimer(0.f)
		, m_bHintSolving(false)
		, m_pxHintTask(nullptr)
		, m_uResetCount(0)
		, m_bSkipOffered(false)
		, m_fVictoryTimer(0.f)
		, m_uVictoryStarsShown(0)
		, m_uVictoryCoinsEarned(0)
		, m_uVictoryStarRating(0)
		, m_bVictoryOverlayActive(false)
		, m_uCatCafeCurrentIndex(0)
		, m_bCatCafeMouseWasDown(false)
		, m_fCatCafeSwipeStartX(0.f)
		, m_bCatCafeSwipeActive(false)
		, m_bDailyPuzzleMode(false)
	{
		m_xSaveData.Reset();
	}

	~TilePuzzle_Behaviour()
	{
		ClearHint();
	}

	// ========================================================================
	// Lifecycle Hooks
	// ========================================================================

	void OnAwake() ZENITH_FINAL override
	{
		// Load save data
		if (!Zenith_SaveData::Load("autosave", TilePuzzle_ReadSaveData, &m_xSaveData))
		{
			m_xSaveData.Reset();
		}
		m_uCurrentLevelNumber = m_xSaveData.uCurrentLevel;

		// Load all levels into RAM and validate against tier constraints
		m_uAvailableLevelCount = 0;
		for (uint32_t u = 1; u <= TilePuzzleSaveData::uMAX_LEVELS; ++u)
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/level_%04u.tlvl", u);
			if (!Zenith_FileAccess::FileExists(szPath))
				break;

			Zenith_DataStream xValidateStream;
			xValidateStream.ReadFromFile(szPath);
			Zenith_Assert(xValidateStream.IsValid(), "Failed to load level file for validation: %s", szPath);

			TilePuzzleLevelData xValidateLevel = {};
			Zenith_Vector<TilePuzzleShapeDefinition> axValidateDefs;
			bool bParsed = TilePuzzleLevelSerialize::Read(xValidateStream, xValidateLevel, axValidateDefs);
			Zenith_Assert(bParsed, "Failed to parse level file for validation: %s", szPath);

			ValidateLevelTier(u, xValidateLevel);

			m_axPreloadedLevels.PushBack(std::move(xValidateLevel));
			m_axPreloadedShapeDefs.PushBack(std::move(axValidateDefs));
			m_uAvailableLevelCount++;
		}

		// Clamp saved level to available range
		if (m_uCurrentLevelNumber > m_uAvailableLevelCount)
			m_uCurrentLevelNumber = (m_uAvailableLevelCount > 0) ? m_uAvailableLevelCount : 1;

		// Cache global resources (lightweight)
		m_pxCubeGeometry = TilePuzzle::Resources().m_pxCubeGeometry;
		m_pxSphereGeometry = TilePuzzle::Resources().m_pxSphereGeometry;
		m_pxCatGeometry = TilePuzzle::Resources().m_pxCatMeshGeometry;
		m_xFloorMaterial = TilePuzzle::Resources().m_xFloorMaterial;
		m_xBlockerMaterial = TilePuzzle::Resources().m_xBlockerMaterial;

		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			m_axShapeMaterials[i] = TilePuzzle::Resources().m_axShapeMaterials[i];
			m_axCatMaterials[i] = TilePuzzle::Resources().m_axCatMaterials[i];
		}

		// Create highlighted versions of shape materials with emissive glow
		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			Zenith_MaterialAsset* pxOriginal = m_axShapeMaterials[i].GetDirect();
			Zenith_MaterialAsset* pxHighlighted = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

			pxHighlighted->SetName(pxOriginal->GetName() + "_Highlighted");
			pxHighlighted->SetBaseColor(pxOriginal->GetBaseColor());
			pxHighlighted->SetDiffuseTexture(pxOriginal->GetDiffuseTextureHandle());

			Zenith_Maths::Vector4 xBaseColor = pxOriginal->GetBaseColor();
			pxHighlighted->SetEmissiveColor(Zenith_Maths::Vector3(xBaseColor.x, xBaseColor.y, xBaseColor.z));
			pxHighlighted->SetEmissiveIntensity(TilePuzzle::Resources().m_fHighlightEmissiveIntensity);

			m_axShapeMaterialsHighlighted[i].Set(pxHighlighted);
		}

		// Wire up button callbacks
		bool bHasMenu = false;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

			// New menu buttons
			Zenith_UI::Zenith_UIButton* pxContinueBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ContinueButton");
			if (pxContinueBtn)
			{
				pxContinueBtn->SetOnClick(&OnContinueClicked, this);
				pxContinueBtn->SetFocused(true);
				bHasMenu = true;
			}

			Zenith_UI::Zenith_UIButton* pxLevelSelectBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("LevelSelectButton");
			if (pxLevelSelectBtn)
			{
				pxLevelSelectBtn->SetOnClick(&OnLevelSelectClicked, this);
			}

			// Hide legacy Pinball button (pinball is now accessed through level select)
			Zenith_UI::Zenith_UIButton* pxPinballBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PinballButton");
			if (pxPinballBtn)
			{
				pxPinballBtn->SetVisible(false);
			}

			Zenith_UI::Zenith_UIButton* pxResetSaveBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ResetSaveButton");
			if (pxResetSaveBtn)
			{
				pxResetSaveBtn->SetOnClick(&OnResetSaveClicked, this);
			}

			// Legacy: support old single Play button as fallback
			if (!bHasMenu)
			{
				Zenith_UI::Zenith_UIButton* pxPlayBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
				if (pxPlayBtn)
				{
					pxPlayBtn->SetOnClick(&OnContinueClicked, this);
					pxPlayBtn->SetFocused(true);
					bHasMenu = true;
				}
			}

			// Gameplay action buttons
			Zenith_UI::Zenith_UIButton* pxResetBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ResetBtn");
			if (pxResetBtn)
			{
				pxResetBtn->SetOnClick(&OnResetClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxMenuBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuBtn");
			if (pxMenuBtn)
			{
				pxMenuBtn->SetOnClick(&OnMenuClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxUndoBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("UndoBtn");
			if (pxUndoBtn)
			{
				pxUndoBtn->SetOnClick(&OnUndoClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxHintBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("HintBtn");
			if (pxHintBtn)
			{
				pxHintBtn->SetOnClick(&OnHintClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
			if (pxSkipBtn)
			{
				pxSkipBtn->SetOnClick(&OnSkipClicked, this);
				pxSkipBtn->SetVisible(false);
			}

			Zenith_UI::Zenith_UIButton* pxNextLevelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			if (pxNextLevelBtn)
			{
				pxNextLevelBtn->SetOnClick(&OnNextLevelClicked, this);
				pxNextLevelBtn->SetVisible(false);
			}

			// Level select buttons
			for (uint32_t i = 0; i < 20; ++i)
			{
				char szName[32];
				snprintf(szName, sizeof(szName), "LevelBtn_%u", i);
				Zenith_UI::Zenith_UIButton* pxLevelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
				if (pxLevelBtn)
				{
					s_axLevelButtonData[i].pxBehaviour = this;
					s_axLevelButtonData[i].uLevelNumber = 0;
					pxLevelBtn->SetOnClick(&OnLevelButtonClicked, &s_axLevelButtonData[i]);
				}
			}

			Zenith_UI::Zenith_UIButton* pxPrevPageBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PrevPageButton");
			if (pxPrevPageBtn)
			{
				pxPrevPageBtn->SetOnClick(&OnPrevPageClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxNextPageBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextPageButton");
			if (pxNextPageBtn)
			{
				pxNextPageBtn->SetOnClick(&OnNextPageClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("BackButton");
			if (pxBackBtn)
			{
				pxBackBtn->SetOnClick(&OnBackClicked, this);
			}

			// Cat Cafe button
			Zenith_UI::Zenith_UIButton* pxCatCafeBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafeButton");
			if (pxCatCafeBtn)
				pxCatCafeBtn->SetOnClick(&OnCatCafeClicked, this);

			Zenith_UI::Zenith_UIButton* pxCatCafeBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafeBackButton");
			if (pxCatCafeBackBtn)
				pxCatCafeBackBtn->SetOnClick(&OnCatCafeBackClicked, this);

			Zenith_UI::Zenith_UIButton* pxCatCafePrevBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafePrevPage");
			if (pxCatCafePrevBtn)
				pxCatCafePrevBtn->SetOnClick(&OnCatCafePrevPageClicked, this);

			Zenith_UI::Zenith_UIButton* pxCatCafeNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafeNextPage");
			if (pxCatCafeNextBtn)
				pxCatCafeNextBtn->SetOnClick(&OnCatCafeNextPageClicked, this);

			// Daily Puzzle button
			Zenith_UI::Zenith_UIButton* pxDailyBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("DailyPuzzleButton");
			if (pxDailyBtn)
				pxDailyBtn->SetOnClick(&OnDailyPuzzleClicked, this);

			// Lives Refill button
			Zenith_UI::Zenith_UIButton* pxRefillBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
			if (pxRefillBtn)
				pxRefillBtn->SetOnClick(&OnRefillLivesClicked, this);

			// Settings button
			Zenith_UI::Zenith_UIButton* pxSettingsBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsButton");
			if (pxSettingsBtn)
				pxSettingsBtn->SetOnClick(&OnSettingsClicked, this);

			// Achievements button
			Zenith_UI::Zenith_UIButton* pxAchievementsBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("AchievementsButton");
			if (pxAchievementsBtn)
				pxAchievementsBtn->SetOnClick(&OnAchievementsClicked, this);

			// Settings screen buttons
			Zenith_UI::Zenith_UIButton* pxSettingsBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsBackBtn");
			if (pxSettingsBackBtn)
				pxSettingsBackBtn->SetOnClick(&OnSettingsBackClicked, this);

			Zenith_UI::Zenith_UIToggle* pxSoundToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsSoundBtn");
			if (pxSoundToggle)
			{
				pxSoundToggle->SetOnValueChanged(&OnSettingSoundChanged, this);
				pxSoundToggle->SetIsOn(m_xSaveData.bSoundEnabled);
			}

			Zenith_UI::Zenith_UIToggle* pxMusicToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsMusicBtn");
			if (pxMusicToggle)
			{
				pxMusicToggle->SetOnValueChanged(&OnSettingMusicChanged, this);
				pxMusicToggle->SetIsOn(m_xSaveData.bMusicEnabled);
			}

			Zenith_UI::Zenith_UIToggle* pxHapticsToggle = xUI.FindElement<Zenith_UI::Zenith_UIToggle>("SettingsHapticsBtn");
			if (pxHapticsToggle)
			{
				pxHapticsToggle->SetOnValueChanged(&OnSettingHapticsChanged, this);
				pxHapticsToggle->SetIsOn(m_xSaveData.bHapticsEnabled);
			}

			Zenith_UI::Zenith_UIButton* pxCreditsBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsCreditsBtn");
			if (pxCreditsBtn)
				pxCreditsBtn->SetOnClick(&OnCreditsClicked, this);

			// Cache UI element pointers for runtime access (avoids repeated FindElement)
			// Menu
			m_pxMenuTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
			m_pxMenuBtnGroup = xUI.FindElement("MenuButtonGroup");
			m_pxMenuBg = xUI.FindElement("MenuBackground");
			m_pxTopRightCounters = xUI.FindElement("TopRightCounters");
			m_pxMenuCoinText = xUI.FindElement<Zenith_UI::Zenith_UIText>("CoinText");
			m_pxTotalStarsText = xUI.FindElement<Zenith_UI::Zenith_UIText>("TotalStarsText");
			m_pxStreakGroup = xUI.FindElement("StreakGroup");
			m_pxLivesArea = xUI.FindElement("LivesArea");
			m_pxMenuSubtitle = xUI.FindElement("MenuSubtitle");
			m_pxVersionText = xUI.FindElement("VersionText");
			m_pxMenuPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
			m_pxLevelSelectBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("LevelSelectButton");
			m_pxCatCafeBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafeButton");
			m_pxDailyBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("DailyPuzzleButton");
			// Pinball button removed — pinball accessed through level select
			// HUD
			m_pxHUDInfoGroup = xUI.FindElement("HUDInfoGroup");
			m_pxHUDCoinGroup = xUI.FindElement("HUDCoinGroup");
			m_pxHUDButtonGroup = xUI.FindElement("HUDButtonGroup");
			m_pxLevelText = xUI.FindElement<Zenith_UI::Zenith_UIText>("LevelText");
			m_pxMovesText = xUI.FindElement<Zenith_UI::Zenith_UIText>("MovesText");
			m_pxCatsText = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatsText");
			m_pxHUDCoinsText = xUI.FindElement<Zenith_UI::Zenith_UIText>("HUDCoinsText");
			m_pxUndoBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("UndoBtn");
			m_pxHintBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("HintBtn");
			m_pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
			m_pxNextLevelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			// Level select
			m_pxLevelSelectTitle = xUI.FindElement("LevelSelectTitle");
			m_pxLevelSelectNavGroup = xUI.FindElement("LevelSelectNavGroup");
			m_pxLevelSelectBg = xUI.FindElement("LevelSelectBg");
			m_pxPageText = xUI.FindElement<Zenith_UI::Zenith_UIText>("PageText");
			m_pxStarProgress = xUI.FindElement<Zenith_UI::Zenith_UIText>("LevelSelectStarProgress");

			// Fix level select text positions so they don't overlap
			if (m_pxLevelSelectTitle)
			{
				m_pxLevelSelectTitle->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
				m_pxLevelSelectTitle->SetPosition(0.f, 20.f);
			}
			if (m_pxPageText)
			{
				m_pxPageText->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
				m_pxPageText->SetPosition(0.f, 90.f);
			}
			if (m_pxStarProgress)
			{
				m_pxStarProgress->SetAnchorAndPivot(Zenith_UI::AnchorPreset::TopCenter);
				m_pxStarProgress->SetPosition(0.f, 130.f);
			}

			for (uint32_t i = 0; i < 20; ++i)
			{
				char szName[32];
				snprintf(szName, sizeof(szName), "LevelBtn_%u", i);
				m_apxLevelBtns[i] = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
			}

			// Create star images for each level button (3 per button)
			for (uint32_t i = 0; i < 20; ++i)
			{
				if (!m_apxLevelBtns[i])
					continue;
				for (uint32_t s = 0; s < 3; ++s)
				{
					char szStarName[32];
					snprintf(szStarName, sizeof(szStarName), "LevelStar_%u_%u", i, s);
					Zenith_UI::Zenith_UIImage* pxStar = xUI.CreateImage(szStarName);
					pxStar->SetSize(20.f, 20.f);
					pxStar->SetAnchor(0.5f, 1.f);
					pxStar->SetPivot(0.5f, 1.f);
					float fStarX = (static_cast<float>(s) - 1.f) * 22.f;
					pxStar->SetPosition(fStarX, -4.f);
					pxStar->SetVisible(false);
					m_apxLevelBtns[i]->AddChild(pxStar);
					m_apxLevelStars[i][s] = pxStar;
				}
			}
			// Meta-game
			m_pxLivesText = xUI.FindElement<Zenith_UI::Zenith_UIText>("LivesText");
			m_pxLivesTimerText = xUI.FindElement<Zenith_UI::Zenith_UIText>("LivesTimerText");
			m_pxStreakText = xUI.FindElement<Zenith_UI::Zenith_UIText>("DailyStreakText");
			m_pxRefillBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
			m_pxHintTokenText = xUI.FindElement<Zenith_UI::Zenith_UIText>("HintTokenText");
			m_pxHintTokenGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("HintTokenGroup");
			m_pxCatProgressBg = xUI.FindElement<Zenith_UI::Zenith_UIRect>("CatProgressBg");
			m_pxCatProgressFill = xUI.FindElement<Zenith_UI::Zenith_UIRect>("CatProgressFill");
			// Confirm dialog overlay
			m_pxConfirmOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>("ConfirmOverlay");
			m_pxConfirmText = xUI.FindElement<Zenith_UI::Zenith_UIText>("ConfirmText");
			m_pxConfirmCancelBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ConfirmCancelBtn");
			m_pxConfirmAcceptBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("ConfirmAcceptBtn");
			if (m_pxConfirmCancelBtn)
				m_pxConfirmCancelBtn->SetOnClick(&OnConfirmCancelClicked, this);
			if (m_pxConfirmAcceptBtn)
				m_pxConfirmAcceptBtn->SetOnClick(&OnConfirmAcceptClicked, this);
			// Credits overlay
			m_pxCreditsOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>("CreditsOverlay");
			// Tutorial overlay
			m_pxTutorialOverlay = xUI.FindElement<Zenith_UI::Zenith_UIOverlay>("TutorialOverlay");
			m_pxTutorialText = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialText");
			m_pxTutorialHintText = xUI.FindElement<Zenith_UI::Zenith_UIText>("TutorialHintText");
			Zenith_Log(LOG_CATEGORY_GENERAL, "Tutorial UI: overlay=%p text=%p hint=%p",
				static_cast<void*>(m_pxTutorialOverlay), static_cast<void*>(m_pxTutorialText), static_cast<void*>(m_pxTutorialHintText));
		}

		// Regenerate lives on startup
		m_xSaveData.RegenerateLives(GetCurrentTimestamp());

		if (bHasMenu)
		{
			// Start in main menu state
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			ShowScreen(SCREEN_MENU);
		}
		else
		{
			// No menu UI (gameplay scene) - start game directly
			StartGame();
		}
	}

	void OnStart() ZENITH_FINAL override
	{
		// Find particle emitter entities in the parent scene
		Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
		if (pxSceneData)
		{
			Zenith_Entity xElimEmitter = pxSceneData->FindEntityByName("EliminationEmitter");
			if (xElimEmitter.IsValid())
				m_uEliminationEmitterID = xElimEmitter.GetEntityID();
			Zenith_Entity xConfettiEmitter = pxSceneData->FindEntityByName("VictoryConfettiEmitter");
			if (xConfettiEmitter.IsValid())
				m_uVictoryConfettiEmitterID = xConfettiEmitter.GetEntityID();
		}

		if (m_eState == TILEPUZZLE_STATE_MAIN_MENU)
		{
			ShowScreen(SCREEN_MENU);
		}
	}

	void OnUpdate(const float fDeltaTime) ZENITH_FINAL override
	{
#ifdef ZENITH_TOOLS
		if (m_bPendingReloadLevel)
		{
			m_bPendingReloadLevel = false;
			StartNewLevel();
			return;
		}
		if (m_bPendingResetLevel)
		{
			m_bPendingResetLevel = false;
			ResetLevel();
			return;
		}
#endif
		switch (m_eState)
		{
		case TILEPUZZLE_STATE_MAIN_MENU:
			m_fMenuTimer += fDeltaTime;
			break;

		case TILEPUZZLE_STATE_LEVEL_SELECT:
			m_fMenuTimer += fDeltaTime;
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
				return;
			}
#endif
			break;

		case TILEPUZZLE_STATE_PLAYING:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				if (m_bConfirmDialogActive)
				{
					OnConfirmDialogCancel();
				}
				else
				{
					ReturnToMenu();
				}
				return;
			}
#endif
			if (m_bTutorialActive)
			{
				UpdateTutorialOverlay(fDeltaTime);
			}
			else
			{
				m_fLevelTimer += fDeltaTime;
				if (m_bHintActive || m_bHintSolving)
				{
					m_fHintFlashTimer += fDeltaTime;
				}
				if (m_bConfirmDialogActive)
				{
					// Defensive: if overlay should be showing but isn't, retry Show()
					if (m_pxConfirmOverlay && !m_pxConfirmOverlay->IsShowing())
					{
						Zenith_Log(LOG_CATEGORY_GENERAL, "ConfirmDialog: overlay not showing despite active flag, retrying Show()");
						m_pxConfirmOverlay->Show();
					}
				}
				else
				{
					HandleDragInput();
				}
				UpdateTargetCatHighlight(fDeltaTime);
				UpdateStuckDetection(fDeltaTime);
			}
			break;

		case TILEPUZZLE_STATE_SHAPE_SLIDING:
			UpdateSlideAnimation(fDeltaTime);
			break;

		case TILEPUZZLE_STATE_CHECK_ELIMINATION:
			CheckCatElimination();
			if (IsLevelComplete())
			{
				OnLevelCompleted();
			}
			else
			{
				m_eState = TILEPUZZLE_STATE_PLAYING;
			}
			break;

		case TILEPUZZLE_STATE_LEVEL_COMPLETE:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				ReturnToMenu();
				return;
			}
#endif
			if (m_bVictoryOverlayActive)
			{
				UpdateVictoryOverlay(fDeltaTime);
			}
			break;

		case TILEPUZZLE_STATE_CAT_CAFE:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
				return;
			}
#endif
			HandleCatCafeInput(fDeltaTime);
			break;

		case TILEPUZZLE_STATE_SETTINGS:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				OnSettingsBackClicked(this);
				return;
			}
#endif
			m_fMenuTimer += fDeltaTime;
			UpdateCreditsOverlay(fDeltaTime);
			break;

		case TILEPUZZLE_STATE_VICTORY_OVERLAY:
			break;

		case TILEPUZZLE_STATE_ACHIEVEMENTS:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
				return;
			}
#endif
			m_fMenuTimer += fDeltaTime;
			break;

		}

		// Update main menu meta-game displays
		if (m_eState == TILEPUZZLE_STATE_MAIN_MENU)
		{
			UpdateMainMenuUI();
		}

		// Only update visuals/UI while playing
		if (m_eState != TILEPUZZLE_STATE_MAIN_MENU && m_eState != TILEPUZZLE_STATE_LEVEL_SELECT
			&& m_eState != TILEPUZZLE_STATE_CAT_CAFE)
		{
			UpdateVisuals(fDeltaTime);
			UpdateUI();
		}

		// Process hint solver results after rendering so the "..." indicator shows for at least one frame
		if (m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			UpdateHintSolver();
		}

		// Achievement toast
		UpdateAchievementToast(fDeltaTime);

		// Achievements screen
		UpdateAchievementsScreen(fDeltaTime);

		// Confirmation dialog overlay
		UpdateConfirmDialog(fDeltaTime);

		// Screen transition overlay (rendered on top of everything)
		UpdateTransition(fDeltaTime);
	}

	void RenderPropertiesPanel() override
	{
#ifdef ZENITH_TOOLS
		ImGui::Text("TilePuzzle Game");
		ImGui::Separator();
		ImGui::Text("Level: %u", m_uCurrentLevelNumber);
		ImGui::Text("Moves: %u", m_uMoveCount);
		ImGui::Text("Cats remaining: %zu", CountRemainingCats());

		const char* aszStateNames[] = { "Menu", "Playing", "Sliding", "Checking", "Complete", "LevelSelect", "CatCafe", "Victory", "Settings" };
		ImGui::Text("State: %s", aszStateNames[m_eState]);
		ImGui::Text("Coins: %u  Lives: %u  Stars: %u", m_xSaveData.uCoins, m_xSaveData.uLives, m_xSaveData.uTotalStars);

		if (ImGui::Button("Reload Level"))
		{
			m_bPendingReloadLevel = true;
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			m_bPendingResetLevel = true;
		}
#endif
	}

	// ========================================================================
	// Serialization
	// ========================================================================

	void WriteParametersToDataStream(Zenith_DataStream& xStream) const override
	{
		const uint32_t uVersion = 1;
		xStream << uVersion;
		xStream << m_uCurrentLevelNumber;
	}

	void ReadParametersFromDataStream(Zenith_DataStream& xStream) override
	{
		uint32_t uVersion;
		xStream >> uVersion;
		if (uVersion >= 1)
		{
			xStream >> m_uCurrentLevelNumber;
		}
	}

private:
	// ========================================================================
	// Button Callbacks (static function pointers, NOT std::function)
	// ========================================================================

	static void OnContinueClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		// Check if player has lives
		pxSelf->m_xSaveData.RegenerateLives(GetCurrentTimestamp());
		if (!pxSelf->m_xSaveData.HasLives())
			return;
		pxSelf->m_uCurrentLevelNumber = pxSelf->m_xSaveData.uHighestLevelReached;
		pxSelf->m_xSaveData.uCurrentLevel = pxSelf->m_uCurrentLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);

		// Route pinball gate levels to pinball scene
		uint32_t uGateIndex = 0;
		if (TilePuzzle_IsGateLevel(pxSelf->m_uCurrentLevelNumber, &uGateIndex))
		{
			TilePuzzle::g_uPinballRequestedGate = uGateIndex;
			Zenith_SceneManager::LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
		}
		else
		{
			Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		}
	}

	static void OnLevelSelectClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_uLevelSelectPage = 0;
		pxSelf->StartTransition(TILEPUZZLE_STATE_LEVEL_SELECT);
	}

	static void OnResetSaveClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->ShowConfirmDialog(CONFIRM_RESET_SAVE);
	}

	static void OnLevelButtonClicked(void* pxUserData)
	{
		TilePuzzleLevelButtonData* pxData = static_cast<TilePuzzleLevelButtonData*>(pxUserData);
		if (pxData->uLevelNumber == 0 || pxData->uLevelNumber > pxData->pxBehaviour->m_uAvailableLevelCount)
			return;

		// Only allow unlocked levels
		if (pxData->uLevelNumber > pxData->pxBehaviour->m_xSaveData.uHighestLevelReached)
			return;

		// Check if player has lives
		pxData->pxBehaviour->m_xSaveData.RegenerateLives(GetCurrentTimestamp());
		if (!pxData->pxBehaviour->m_xSaveData.HasLives())
			return;

		pxData->pxBehaviour->m_uCurrentLevelNumber = pxData->uLevelNumber;
		pxData->pxBehaviour->m_xSaveData.uCurrentLevel = pxData->uLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxData->pxBehaviour->m_xSaveData);

		// Route pinball gate levels to pinball scene
		uint32_t uGateIndex = 0;
		if (TilePuzzle_IsGateLevel(pxData->uLevelNumber, &uGateIndex))
		{
			TilePuzzle::g_uPinballRequestedGate = uGateIndex;
			Zenith_SceneManager::LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
		}
		else
		{
			Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
		}
	}

	static void OnPrevPageClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_uLevelSelectPage > 0)
		{
			pxSelf->m_uLevelSelectPage--;
			pxSelf->UpdateLevelSelectUI();
		}
	}

	static void OnNextPageClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		uint32_t uTotalPages = (pxSelf->m_uAvailableLevelCount + 19) / 20;
		if (uTotalPages == 0) uTotalPages = 1;
		if (pxSelf->m_uLevelSelectPage < uTotalPages - 1)
		{
			pxSelf->m_uLevelSelectPage++;
			pxSelf->UpdateLevelSelectUI();
		}
	}

	static void OnBackClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
	}

	static void OnResetClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			pxSelf->ResetLevel();
		}
	}

	static void OnUndoClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			pxSelf->PerformUndo();
		}
	}

	static void OnHintClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			pxSelf->PerformHint();
		}
	}

	static void OnSkipClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING)
		{
			pxSelf->ShowConfirmDialog(CONFIRM_SKIP_LEVEL);
		}
	}

	static void OnMenuClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		// Only show confirmation if moves were made (otherwise no life lost)
		if (pxSelf->m_eState == TILEPUZZLE_STATE_PLAYING && pxSelf->m_uMoveCount > 0)
		{
			pxSelf->ShowConfirmDialog(CONFIRM_EXIT_LEVEL);
		}
		else
		{
			pxSelf->ReturnToMenu();
		}
	}

	static void OnNextLevelClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		if (pxSelf->m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE)
		{
			pxSelf->NextLevel();
		}
	}

	// ========================================================================
	// State Transitions
	// ========================================================================

	void StartGame()
	{
		ShowScreen(SCREEN_HUD);

		// Create puzzle scene for level entities
		m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
		Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);

		LoadLevelFromFile();
	}

	void StartNewLevel()
	{
		// Hide victory overlay and next level button
		m_bVictoryOverlayActive = false;
		SetVictoryOverlayVisible(false);

		// Unload current puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			ClearEntityReferences();
			Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
		}

		// Create fresh puzzle scene
		m_xPuzzleScene = Zenith_SceneManager::CreateEmptyScene("Puzzle");
		Zenith_SceneManager::SetActiveScene(m_xPuzzleScene);

		LoadLevelFromFile();
	}

	void ReturnToMenu()
	{
		// Lose a life if exiting puzzle without completing (only if moves were made)
		if (m_eState == TILEPUZZLE_STATE_PLAYING && m_uMoveCount > 0)
		{
			m_xSaveData.LoseLife();
		}

		// Reset victory overlay state
		m_bVictoryOverlayActive = false;
		m_bDailyPuzzleMode = false;

		// Save current progress before returning
		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Unload puzzle scene (destroys all level entities automatically)
		if (m_xPuzzleScene.IsValid())
		{
			ClearEntityReferences();
			Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
			m_xPuzzleScene = Zenith_Scene();
		}

		Zenith_SceneManager::LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
	}

	void OnLevelCompleted()
	{
		m_eState = TILEPUZZLE_STATE_LEVEL_COMPLETE;

		// Calculate star rating based on par (minimum moves from solver)
		uint32_t uPar = m_xCurrentLevel.uMinimumMoves;
		if (uPar == 0) uPar = 1;  // Safeguard against zero par
		if (m_uMoveCount <= uPar)
			m_uStarsEarned = 3;
		else if (m_uMoveCount <= uPar + 2)
			m_uStarsEarned = 2;
		else
			m_uStarsEarned = 1;

		// Update save data
		uint32_t uLevelIndex = m_uCurrentLevelNumber - 1;
		if (uLevelIndex < TilePuzzleSaveData::uMAX_LEVELS)
		{
			TilePuzzleLevelRecord& xRecord = m_xSaveData.axLevelRecords[uLevelIndex];
			m_bVictoryFirstCompletion = !xRecord.bCompleted;
			m_bVictoryNewBest = xRecord.bCompleted && m_uStarsEarned > xRecord.uBestStars;
			xRecord.bCompleted = true;
			if (xRecord.uBestMoves == 0 || m_uMoveCount < xRecord.uBestMoves)
			{
				xRecord.uBestMoves = m_uMoveCount;
			}
			if (xRecord.fBestTime == 0.f || m_fLevelTimer < xRecord.fBestTime)
			{
				xRecord.fBestTime = m_fLevelTimer;
			}
			if (m_uStarsEarned > xRecord.uBestStars)
			{
				xRecord.uBestStars = m_uStarsEarned;
			}
		}

		// Always unlock next level (pinball gates are separate entries, not blockers)
		if (m_uCurrentLevelNumber >= m_xSaveData.uHighestLevelReached
			&& m_uCurrentLevelNumber < TilePuzzleSaveData::uMAX_LEVELS)
		{
			m_xSaveData.uHighestLevelReached = m_uCurrentLevelNumber + 1;
		}
		m_bPinballGateRequired = false;

		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;

		// Collect cat for this level
		uint32_t uCatID = m_uCurrentLevelNumber - 1;
		m_xSaveData.CollectCat(uCatID);

		// Award coins and update star rating
		m_uVictoryCoinsEarned = s_uCoinsPerLevelComplete;
		if (m_uStarsEarned >= 3)
		{
			m_uVictoryCoinsEarned += s_uCoinsPerThreeStar;
		}

		// Milestone coin bonuses on first completion (10/25/50/75/100 cats)
		if (m_bVictoryFirstCompletion)
		{
			uint32_t uCatCount = m_xSaveData.uCatsCollectedCount;
			if (uCatCount == 10) m_uVictoryCoinsEarned += 50;
			else if (uCatCount == 25) m_uVictoryCoinsEarned += 100;
			else if (uCatCount == 50) m_uVictoryCoinsEarned += 200;
			else if (uCatCount == 75) m_uVictoryCoinsEarned += 300;
			else if (uCatCount == 100) m_uVictoryCoinsEarned += 500;
		}

		m_xSaveData.AddCoins(static_cast<int32_t>(m_uVictoryCoinsEarned));
		m_xSaveData.SetStarRating(m_uCurrentLevelNumber, static_cast<uint8_t>(m_uStarsEarned));

		// Handle daily puzzle completion
		if (m_bDailyPuzzleMode)
		{
			OnDailyPuzzleCompleted();
		}

		// Check achievements
		CheckAchievements();

		// Update weekly challenge progress
		{
			// Type 0: levels completed
			m_xSaveData.UpdateWeeklyChallengeProgress(0, 1);
			// Type 1: stars earned
			m_xSaveData.UpdateWeeklyChallengeProgress(1, m_uStarsEarned);
			// Type 2: cats rescued (1 per level)
			m_xSaveData.UpdateWeeklyChallengeProgress(2, 1);
			// Type 3: 3-star completions
			if (m_uStarsEarned >= 3)
			{
				m_xSaveData.UpdateWeeklyChallengeProgress(3, 1);
			}
			// Check if challenge is now complete
			if (!m_xSaveData.bWeeklyChallengeCompleted &&
				m_xSaveData.uWeeklyChallengeProgress >= m_xSaveData.uWeeklyChallengeTarget)
			{
				m_xSaveData.bWeeklyChallengeCompleted = true;
				m_xSaveData.AddCoins(static_cast<int32_t>(m_xSaveData.uWeeklyChallengeReward));
			}
		}

		// Auto-save (after all save data modifications)
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Show victory overlay
		m_bVictoryOverlayActive = true;
		m_fVictoryTimer = 0.f;
		m_uVictoryStarsShown = 0;
		m_uVictoryStarRating = static_cast<uint8_t>(m_uStarsEarned);
		ShowScreenAdditive(SCREEN_VICTORY);

		// Camera zoom pulse for emphasis (extended for 3-star)
		m_fZoomPulseDuration = (m_uStarsEarned >= 3) ? 0.8f : 0.6f;
		TriggerZoomPulse();

		// Burst victory confetti (scaled by star count: 1-star=0, 2-star=40, 3-star=80)
		Zenith_SceneData* pxParentSceneData = m_xParentEntity.GetSceneData();
		if (m_uStarsEarned >= 2 && m_uVictoryConfettiEmitterID.IsValid() && pxParentSceneData && pxParentSceneData->EntityExists(m_uVictoryConfettiEmitterID))
		{
			Zenith_Entity xEmitter = pxParentSceneData->GetEntity(m_uVictoryConfettiEmitterID);
			uint32_t uConfettiCount = (m_uStarsEarned >= 3) ? 80 : 40;
			xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(uConfettiCount);

			// Extra gold sparkle burst for 3-star
			if (m_uStarsEarned >= 3 && m_uEliminationEmitterID.IsValid() && pxParentSceneData->EntityExists(m_uEliminationEmitterID))
			{
				Zenith_Entity xSparkleEmitter = pxParentSceneData->GetEntity(m_uEliminationEmitterID);
				Flux_ParticleEmitterConfig* pxConfig = xSparkleEmitter.GetComponent<Zenith_ParticleEmitterComponent>().GetConfig();
				if (pxConfig)
				{
					pxConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.85f, 0.1f, 1.0f);
				}
				xSparkleEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(10);
			}
		}

		// NextLevel button (or Pinball Gate button) is shown by the victory
		// overlay animation in UpdateVictoryOverlay based on m_bPinballGateRequired
	}

	// ========================================================================
	// Touch/Swipe Input
	// ========================================================================

	void HandleDragInput()
	{
		if (m_eState != TILEPUZZLE_STATE_PLAYING && !m_bDragging)
			return;

		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		Zenith_Maths::Vector2_64 xMousePos64;
		Zenith_Input::GetMousePosition(xMousePos64);
		float fScreenX = static_cast<float>(xMousePos64.x);
		float fScreenY = static_cast<float>(xMousePos64.y);

		if (bMouseDown && !m_bMouseWasDown)
		{
			// Mouse just pressed - try to start drag
			int32_t iGridX, iGridY;
			if (ScreenToGrid(fScreenX, fScreenY, iGridX, iGridY))
			{
				int32_t iShape = GetShapeAtPosition(iGridX, iGridY);
				if (iShape >= 0)
				{
					if (IsShapeLocked(iShape))
					{
						// Shape is locked - wiggle it and don't start drag
						TriggerShapeWiggle(iShape);
					}
					else
					{
						m_bDragging = true;
						m_bDragUndoPushed = false;
						m_iDragShapeIndex = iShape;
						m_iSelectedShapeIndex = iShape;

						// Target cat highlighting on levels 1-5
						if (m_uCurrentLevelNumber <= 5)
						{
							ActivateTargetCatHighlight(iShape);
						}

						const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShape];
						m_iDragGrabOffsetX = iGridX - xShape.iOriginX;
						m_iDragGrabOffsetY = iGridY - xShape.iOriginY;
					}
				}
			}
		}
		else if (bMouseDown && m_bDragging)
		{
			// Mouse held - move shape toward cursor
			int32_t iCursorGridX, iCursorGridY;
			if (ScreenToGrid(fScreenX, fScreenY, iCursorGridX, iCursorGridY))
			{
				int32_t iTargetX = iCursorGridX - m_iDragGrabOffsetX;
				int32_t iTargetY = iCursorGridY - m_iDragGrabOffsetY;

				for (int32_t i = 0; i < 4; ++i)
				{
					if (m_eState == TILEPUZZLE_STATE_LEVEL_COMPLETE || m_bPendingLevelComplete)
						break;
					if (!m_bDragging)
						break;

					const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
					int32_t iDX = iTargetX - xShape.iOriginX;
					int32_t iDY = iTargetY - xShape.iOriginY;

					if (iDX == 0 && iDY == 0)
						break;

					TilePuzzleDirection eDir = TILEPUZZLE_DIR_NONE;
					if (abs(iDX) >= abs(iDY))
						eDir = (iDX > 0) ? TILEPUZZLE_DIR_RIGHT : TILEPUZZLE_DIR_LEFT;
					else
						eDir = (iDY > 0) ? TILEPUZZLE_DIR_DOWN : TILEPUZZLE_DIR_UP;

					if (!MoveShapeImmediate(m_iDragShapeIndex, eDir))
					{
						// Micro screen shake on blocked move (only if no shake already active)
						if (m_fScreenShakeTimer <= 0.0f)
							TriggerScreenShake(0.05f, 0.15f);
						break;
					}

					// Set overshoot offset in drag direction (decays in UpdateVisuals)
					{
						int32_t iOvDX, iOvDY;
						TilePuzzleDirections::GetDelta(eDir, iOvDX, iOvDY);
						m_xDragOvershootOffset = Zenith_Maths::Vector3(
							static_cast<float>(iOvDX), 0.0f, static_cast<float>(iOvDY)) * s_fOvershootDistance;
					}
				}
			}
		}
		else if (!bMouseDown && m_bDragging)
		{
			// Mouse released - snap to grid position and end drag
			m_xDragOvershootOffset = Zenith_Maths::Vector3(0.0f);
			SnapShapeVisuals(m_iDragShapeIndex);
			m_bDragging = false;
			m_iDragShapeIndex = -1;
			m_iSelectedShapeIndex = -1;

			// Clear target cat highlighting
			DeactivateTargetCatHighlight();

			if (m_bPendingLevelComplete)
			{
				m_bPendingLevelComplete = false;
				OnLevelCompleted();
			}
		}

		m_bMouseWasDown = bMouseDown;
	}

	// ========================================================================
	// Menu UI
	// ========================================================================

	void SetMenuVisible(bool bVisible)
	{
		if (m_pxMenuTitle) m_pxMenuTitle->SetVisible(bVisible);
		if (m_pxMenuBtnGroup) m_pxMenuBtnGroup->SetVisible(bVisible);
		if (m_pxMenuBg) m_pxMenuBg->SetVisible(bVisible);
		if (m_pxTopRightCounters) m_pxTopRightCounters->SetVisible(bVisible);
		if (m_pxStreakGroup) m_pxStreakGroup->SetVisible(bVisible);
		if (m_pxLivesArea) m_pxLivesArea->SetVisible(bVisible);
		if (m_pxHintTokenGroup) m_pxHintTokenGroup->SetVisible(bVisible);

		// FTUE progressive disclosure: hide buttons until player reaches milestone levels
		uint32_t uProgress = m_xSaveData.uHighestLevelReached;
		if (m_pxLevelSelectBtn) m_pxLevelSelectBtn->SetVisible(bVisible && uProgress >= 5);
		if (m_pxCatCafeBtn) m_pxCatCafeBtn->SetVisible(bVisible && uProgress >= 3);
		if (m_pxDailyBtn) m_pxDailyBtn->SetVisible(bVisible && uProgress >= 10);

		if (m_pxMenuSubtitle) m_pxMenuSubtitle->SetVisible(bVisible);
		if (m_pxVersionText) m_pxVersionText->SetVisible(bVisible);
		if (m_pxMenuPlay) m_pxMenuPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		if (m_pxHUDInfoGroup) m_pxHUDInfoGroup->SetVisible(bVisible);
		if (m_pxHUDCoinGroup) m_pxHUDCoinGroup->SetVisible(bVisible);
		if (m_pxHUDButtonGroup) m_pxHUDButtonGroup->SetVisible(bVisible);
		if (m_pxSkipBtn) m_pxSkipBtn->SetVisible(bVisible && m_bSkipOffered);
	}

	void SetLevelSelectVisible(bool bVisible)
	{
		if (m_pxLevelSelectTitle) m_pxLevelSelectTitle->SetVisible(bVisible);
		if (m_pxPageText) m_pxPageText->SetVisible(bVisible);
		if (m_pxStarProgress) m_pxStarProgress->SetVisible(bVisible);
		if (m_pxLevelSelectNavGroup) m_pxLevelSelectNavGroup->SetVisible(bVisible);
		if (m_pxLevelSelectBg) m_pxLevelSelectBg->SetVisible(bVisible);

		for (uint32_t i = 0; i < 20; ++i)
		{
			if (m_apxLevelBtns[i]) m_apxLevelBtns[i]->SetVisible(bVisible);
			for (uint32_t s = 0; s < 3; ++s)
			{
				if (m_apxLevelStars[i][s]) m_apxLevelStars[i][s]->SetVisible(false);
			}
		}
	}

	// ========================================================================
	// Screen Management
	// ========================================================================

	enum ScreenID : uint8_t
	{
		SCREEN_MENU,
		SCREEN_HUD,
		SCREEN_LEVEL_SELECT,
		SCREEN_CAT_CAFE,
		SCREEN_SETTINGS,
		SCREEN_VICTORY,
		SCREEN_COUNT
	};

	void SetScreenVisible(ScreenID eScreen, bool bVisible)
	{
		switch (eScreen)
		{
		case SCREEN_MENU:			SetMenuVisible(bVisible); break;
		case SCREEN_HUD:			SetHUDVisible(bVisible); break;
		case SCREEN_LEVEL_SELECT:	SetLevelSelectVisible(bVisible); break;
		case SCREEN_CAT_CAFE:		SetCatCafeVisible(bVisible); break;
		case SCREEN_SETTINGS:		SetSettingsVisible(bVisible); break;
		case SCREEN_VICTORY:		SetVictoryOverlayVisible(bVisible); break;
		default: break;
		}
	}

	void HideAllScreens()
	{
		for (uint8_t u = 0; u < SCREEN_COUNT; u++)
		{
			SetScreenVisible(static_cast<ScreenID>(u), false);
		}
	}

	void ShowScreen(ScreenID eScreen)
	{
		HideAllScreens();
		SetScreenVisible(eScreen, true);
	}

	void ShowScreenAdditive(ScreenID eScreen)
	{
		SetScreenVisible(eScreen, true);
	}

	void UpdateLevelSelectUI()
	{
		// Update page text
		uint32_t uTotalPages = (m_uAvailableLevelCount + 19) / 20;
		if (uTotalPages == 0) uTotalPages = 1;

		if (m_pxPageText)
		{
			char szPage[32];
			snprintf(szPage, sizeof(szPage), "Page %u / %u", m_uLevelSelectPage + 1, uTotalPages);
			m_pxPageText->SetText(szPage);
		}

		// Star progress display
		if (m_pxStarProgress)
		{
			char szStars[32];
			snprintf(szStars, sizeof(szStars), "Stars: %u / %u", m_xSaveData.uTotalStars, m_uAvailableLevelCount * 3);
			m_pxStarProgress->SetText(szStars);
		}

		// Update level buttons
		uint32_t uStartLevel = m_uLevelSelectPage * 20 + 1;
		for (uint32_t i = 0; i < 20; ++i)
		{
			uint32_t uLevel = uStartLevel + i;
			Zenith_UI::Zenith_UIButton* pxBtn = m_apxLevelBtns[i];
			if (!pxBtn) continue;

			// Update user data for callback
			s_axLevelButtonData[i].pxBehaviour = this;
			s_axLevelButtonData[i].uLevelNumber = uLevel;

			// Update label
			char szLabel[16];
			if (uLevel > m_uAvailableLevelCount)
			{
				pxBtn->SetVisible(false);
				continue;
			}

			pxBtn->SetVisible(true);

			uint32_t uIndex = uLevel - 1;
			bool bUnlocked = uLevel <= m_xSaveData.uHighestLevelReached;
			bool bIsPinballGate = (uLevel % 10 == 0) && (uLevel / 10 <= TilePuzzleSaveData::uMAX_PINBALL_GATES);

			// Star images: hide by default, show for completed levels below
			for (uint32_t s = 0; s < 3; ++s)
			{
				if (m_apxLevelStars[i][s])
					m_apxLevelStars[i][s]->SetVisible(false);
			}

			if (!bUnlocked)
			{
				// Locked level: show lock symbol
				snprintf(szLabel, sizeof(szLabel), "[%u]", uLevel);
				pxBtn->SetText(szLabel);
				pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.12f, 0.12f, 0.12f, 1.f));
			}
			else if (m_xSaveData.axLevelRecords[uIndex].bCompleted)
			{
				// Completed: show level number and star images
				snprintf(szLabel, sizeof(szLabel), "%u", uLevel);
				pxBtn->SetText(szLabel);

				if (bIsPinballGate)
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.4f, 0.2f, 0.5f, 1.f));
				else
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.2f, 0.3f, 0.5f, 1.f));

				// Show star images
				uint8_t uStarRating = m_xSaveData.GetStarRating(uLevel);
				for (uint32_t s = 0; s < 3; ++s)
				{
					if (m_apxLevelStars[i][s])
					{
						m_apxLevelStars[i][s]->SetTexturePath(s < uStarRating
							? GAME_ASSETS_DIR "Textures/Icons/star_filled" ZENITH_TEXTURE_EXT
							: GAME_ASSETS_DIR "Textures/Icons/star_empty" ZENITH_TEXTURE_EXT);
						m_apxLevelStars[i][s]->SetVisible(true);
					}
				}
			}
			else
			{
				// Unlocked but not completed
				if (bIsPinballGate)
					snprintf(szLabel, sizeof(szLabel), "%u PB", uLevel);
				else
					snprintf(szLabel, sizeof(szLabel), "%u", uLevel);
				pxBtn->SetText(szLabel);

				if (uLevel == m_xSaveData.uHighestLevelReached)
				{
					// Current frontier level: pulsing green highlight (time-based)
					float fPulse = 0.5f + 0.15f * sinf(m_fMenuTimer * 3.0f);
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.15f, fPulse, 0.2f, 1.f));
				}
				else if (bIsPinballGate)
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.4f, 0.2f, 0.5f, 1.f));
				else
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.2f, 0.35f, 0.45f, 1.f));
			}
		}
	}

	// ========================================================================
	// Entity Reference Management
	// ========================================================================

	void ClearEntityReferences()
	{
		m_axFloorEntityIDs.clear();
		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			xShape.xEntityID = Zenith_EntityID();
		}
		for (auto& xCat : m_xCurrentLevel.axCats)
		{
			xCat.uEntityID = Zenith_EntityID();
		}
	}

	// ========================================================================
	// Game State
	// ========================================================================
	TilePuzzleGameState m_eState;
	TilePuzzleLevelData m_xCurrentLevel;
	uint32_t m_uCurrentLevelNumber;
	uint32_t m_uMoveCount;

	int32_t m_iLastMovedShapeIndex = -1;  // Track which shape was last moved for move counting

	// Selection
	int32_t m_iSelectedShapeIndex;

	// Animation
	float m_fSlideProgress;
	TilePuzzleDirection m_eSlideDirection;
	int32_t m_iSlidingShapeIndex = -1;
	Zenith_Maths::Vector3 m_xSlideStartPos;
	Zenith_Maths::Vector3 m_xSlideEndPos;
	TilePuzzleDirection m_eLastDragDirection = TILEPUZZLE_DIR_NONE;

	// Drag overshoot offset (decays over time, added on top of drag lerp)
	Zenith_Maths::Vector3 m_xDragOvershootOffset = Zenith_Maths::Vector3(0.0f);
	static constexpr float s_fOvershootDecaySpeed = 10.0f;
	static constexpr float s_fOvershootDistance = 0.35f;

	// Level file loading
	uint32_t m_uAvailableLevelCount;
	Zenith_Vector<TilePuzzleShapeDefinition> m_axLoadedShapeDefs;  // Owns shape defs for loaded level (must outlive m_xCurrentLevel)
	Zenith_Vector<Flux_MeshGeometry*> m_apxGeneratedShapeMeshes;  // Per-shape meshes generated from loaded cell offsets

	// Preloaded level data (all levels loaded into RAM at boot for instant access + tier validation)
	Zenith_Vector<TilePuzzleLevelData> m_axPreloadedLevels;
	Zenith_Vector<Zenith_Vector<TilePuzzleShapeDefinition>> m_axPreloadedShapeDefs;

	// Entity IDs - floor entities indexed by grid position (y * 1000 + x)
	std::unordered_map<uint32_t, Zenith_EntityID> m_axFloorEntityIDs; // #TODO: Replace with engine hash map

	// Cached resources
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	Flux_MeshGeometry* m_pxCatGeometry = nullptr;
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xBlockerMaterial;
	MaterialHandle m_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axShapeMaterialsHighlighted[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Cached UI element pointers (assigned in OnStart, avoid runtime FindElement)
	// Menu
	Zenith_UI::Zenith_UIText* m_pxMenuTitle = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxMenuBtnGroup = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxMenuBg = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxTopRightCounters = nullptr;
	Zenith_UI::Zenith_UIText* m_pxMenuCoinText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxTotalStarsText = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxStreakGroup = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxLivesArea = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxMenuSubtitle = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxVersionText = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxMenuPlay = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxLevelSelectBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxCatCafeBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxDailyBtn = nullptr;
	// HUD
	Zenith_UI::Zenith_UIElement* m_pxHUDInfoGroup = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxHUDCoinGroup = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxHUDButtonGroup = nullptr;
	Zenith_UI::Zenith_UIText* m_pxLevelText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxMovesText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxCatsText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxHUDCoinsText = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxUndoBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxHintBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxSkipBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxNextLevelBtn = nullptr;
	// Level select
	Zenith_UI::Zenith_UIElement* m_pxLevelSelectTitle = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxLevelSelectNavGroup = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxLevelSelectBg = nullptr;
	Zenith_UI::Zenith_UIText* m_pxPageText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxStarProgress = nullptr;
	Zenith_UI::Zenith_UIButton* m_apxLevelBtns[20] = {};
	Zenith_UI::Zenith_UIImage* m_apxLevelStars[20][3] = {};
	// Meta-game
	Zenith_UI::Zenith_UIText* m_pxLivesText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxLivesTimerText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxStreakText = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxRefillBtn = nullptr;
	// Hint tokens
	Zenith_UI::Zenith_UIText* m_pxHintTokenText = nullptr;
	Zenith_UI::Zenith_UIElement* m_pxHintTokenGroup = nullptr;
	bool m_bPinballGateRequired = false;
	// Cat cafe progress bar
	Zenith_UI::Zenith_UIRect* m_pxCatProgressBg = nullptr;
	Zenith_UI::Zenith_UIRect* m_pxCatProgressFill = nullptr;
	// Confirm dialog overlay
	Zenith_UI::Zenith_UIOverlay* m_pxConfirmOverlay = nullptr;
	Zenith_UI::Zenith_UIText* m_pxConfirmText = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxConfirmCancelBtn = nullptr;
	Zenith_UI::Zenith_UIButton* m_pxConfirmAcceptBtn = nullptr;
	// Credits overlay
	Zenith_UI::Zenith_UIOverlay* m_pxCreditsOverlay = nullptr;
	// Tutorial overlay
	Zenith_UI::Zenith_UIOverlay* m_pxTutorialOverlay = nullptr;
	Zenith_UI::Zenith_UIText* m_pxTutorialText = nullptr;
	Zenith_UI::Zenith_UIText* m_pxTutorialHintText = nullptr;

	// Selection tracking
	int32_t m_iPreviousSelectedShapeIndex = -1;

	// Drag state
	bool m_bDragging = false;
	bool m_bMouseWasDown = false;
	bool m_bPendingLevelComplete = false;
	bool m_bPendingReloadLevel = false;
	bool m_bPendingResetLevel = false;
	int32_t m_iDragShapeIndex = -1;
	int32_t m_iDragGrabOffsetX = 0;
	int32_t m_iDragGrabOffsetY = 0;

	// Scene handle for the puzzle scene (created/destroyed on transitions)
	Zenith_Scene m_xPuzzleScene;

	// Save data
	TilePuzzleSaveData m_xSaveData;
	float m_fLevelTimer;
	uint32_t m_uLevelSelectPage;

	// Undo system
	Zenith_Vector<TilePuzzleUndoState> m_axUndoStack;
	bool m_bFreeUndoAvailable = true;
	bool m_bDragUndoPushed = false;

	// Hint system
	bool m_bHintActive;
	int32_t m_iHintShapeIndex;
	float m_fHintFlashTimer;
	bool m_bHintSolving;
	HintSolverData m_xHintSolverData;
	Zenith_Task* m_pxHintTask;

	// Level skip
	uint32_t m_uResetCount;
	bool m_bSkipOffered;

	// Star rating for current level completion
	uint32_t m_uStarsEarned = 0;

	// Victory overlay state
	float m_fVictoryTimer;
	uint32_t m_uVictoryStarsShown;
	uint32_t m_uVictoryCoinsEarned;
	uint32_t m_uVictoryDisplayedCoins = 0; // Animated coin counter
	uint8_t m_uVictoryStarRating;
	bool m_bVictoryOverlayActive;
	bool m_bVictoryFirstCompletion = false; // True if this is first time completing the level
	bool m_bVictoryNewBest = false;         // True if player improved their star rating

	// Screen shake
	float m_fScreenShakeTimer = 0.0f;
	float m_fScreenShakeDuration = 0.0f;
	float m_fScreenShakeIntensity = 0.0f;
	Zenith_Maths::Vector3 m_xCameraBasePosition = Zenith_Maths::Vector3(0.0f);

	// Camera zoom pulse (level complete)
	float m_fZoomPulseTimer = 0.0f;
	float m_fZoomPulseDuration = 0.6f;
	float m_fZoomPulseInDuration = 0.2f; // Time to zoom in (remainder is ease back)

	// Particle emitter entity IDs (in parent scene, persist across levels)
	Zenith_EntityID m_uEliminationEmitterID;
	Zenith_EntityID m_uVictoryConfettiEmitterID;

	// Cat cafe state
	uint32_t m_uCatCafeCurrentIndex;
	Zenith_Vector<uint32_t> m_axCatCafeCats;
	Zenith_Entity m_xCatCafeDisplayEntity;
	bool m_bCatCafeMouseWasDown;
	float m_fCatCafeSwipeStartX;
	bool m_bCatCafeSwipeActive;
	Zenith_Maths::Vector3 m_xCatCafeSavedCameraPos;
	double m_fCatCafeSavedPitch = 0.0;
	double m_fCatCafeSavedYaw = 0.0;

	// Daily puzzle mode
	bool m_bDailyPuzzleMode;

	// Cat idle bob
	float m_fCatBobTimer = 0.0f;

	// Screen transitions
	bool m_bTransitionActive = false;
	float m_fTransitionTimer = 0.0f;
	bool m_bTransitionHalfDone = false;   // True once we've performed the state switch at the midpoint
	static constexpr float s_fTransitionHalfDuration = 0.15f; // Fade to black duration
	static constexpr float s_fTransitionFullDuration = 0.30f; // Total duration

	// Deferred transition target
	TilePuzzleGameState m_eTransitionTargetState = TILEPUZZLE_STATE_MAIN_MENU;
	uint32_t m_uTransitionTargetLevel = 0;  // For level select -> playing

	// Tutorial overlay
	bool m_bTutorialActive = false;
	uint32_t m_uTutorialIndex = 0;      // Which tutorial (0-5)
	uint32_t m_uTutorialStep = 0;       // Current step within tutorial
	float m_fTutorialFadeProgress = 0.0f;
	bool m_bTutorialMouseWasDown = false;
	static constexpr float s_fTutorialFadeDuration = 0.3f;

	// Menu timer (ticks in non-playing states for UI animations)
	float m_fMenuTimer = 0.0f;

	// Confirmation dialog state
	bool m_bConfirmDialogActive = false;
	TilePuzzleConfirmDialogType m_eConfirmDialogType = CONFIRM_RESET_SAVE;
	bool m_bConfirmDialogMouseWasDown = false;

	// Target cat highlighting (levels 1-5)
	bool m_bTargetHighlightActive = false;
	std::vector<uint32_t> m_auHighlightedCatIndices; // #TODO: Replace with engine container

	// Stuck detection (levels 1-10)
	float m_fTimeSinceLastMove = 0.0f;
	bool m_bStuckHintPromptShown = false;

	// Achievement toast
	float m_fAchievementToastTimer = 0.0f;
	uint32_t m_uLastAchievementUnlocked = ACHIEVEMENT_COUNT;

	// Credits overlay
	bool m_bCreditsOverlayActive = false;

	// ========================================================================
	// Tutorial Overlay System
	// ========================================================================

	// Tutorial index mapping:
	// 0 = Level 1 (basic drag)
	// 1 = Level 6 (first domino/multi-cell shape)
	// 2 = Level 10 (first pinball gate level)
	// 3 = Level 11 (first static blocker)
	// 4 = Level 26 (first blocker-cat)
	// 5 = Level 46 (first conditional/locked shape)

	int32_t GetTutorialIndexForLevel(uint32_t uLevel) const
	{
		switch (uLevel)
		{
		case 1:  return 0;
		case 6:  return 1;
		case 10: return 2;
		case 11: return 3;
		case 26: return 4;
		case 46: return 5;
		default: return -1;
		}
	}

	void TryShowTutorial()
	{
		if (m_bDailyPuzzleMode)
			return;

		int32_t iTutIdx = GetTutorialIndexForLevel(m_uCurrentLevelNumber);
		if (iTutIdx < 0)
			return;

		if (m_xSaveData.IsTutorialShown(static_cast<uint32_t>(iTutIdx)))
			return;

		// Show tutorial overlay
		m_bTutorialActive = true;
		m_uTutorialIndex = static_cast<uint32_t>(iTutIdx);
		m_uTutorialStep = 0;
		m_fTutorialFadeProgress = 0.0f;
		m_bTutorialMouseWasDown = false;

		if (m_pxTutorialOverlay)
		{
			// Size overlay to fit screen with padding
			int32_t iWinWidth, iWinHeight;
			Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
			float fScreenW = static_cast<float>(iWinWidth);
			float fOverlayW = glm::min(fScreenW - 40.f, TilePuzzleUI::fTUTORIAL_OVERLAY_W);
			m_pxTutorialOverlay->SetContentSize(fOverlayW, TilePuzzleUI::fTUTORIAL_OVERLAY_H);

			if (m_pxTutorialText)
			{
				float fTextW = fOverlayW - 60.f;
				m_pxTutorialText->SetSize(fTextW, 100.f);
			}

			SetTutorialTextWithAutoSize(GetTutorialText());
			m_pxTutorialOverlay->Show();
		}
	}

	const char* GetTutorialText() const
	{
		switch (m_uTutorialIndex)
		{
		case 0:
			switch (m_uTutorialStep)
			{
			case 0: return "Drag a colored shape to slide it";
			case 1: return "Slide it onto the matching cat";
			case 2: return "Match all cats to complete the level!";
			default: return nullptr;
			}
		case 1: return "Some shapes cover more than one tile.\nThey slide as one piece!";
		case 2: return "Every 10th level is a pinball gate.\nClear it to continue!";
		case 3: return "Dark shapes are blockers.\nThey can't be moved!";
		case 4: return "This cat sits on a blocker.\nGet a matching shape next to it!";
		case 5: return "Locked shapes need cats\neliminated first to unlock!";
		default: return nullptr;
		}
	}

	void SetTutorialTextWithAutoSize(const char* szText)
	{
		if (!m_pxTutorialText || !szText)
			return;

		float fTextW = m_pxTutorialText->GetSize().x;
		if (fTextW <= 0.f)
			fTextW = 700.f;

		// Find longest line length
		uint32_t uMaxLineLen = 0;
		uint32_t uCurrentLen = 0;
		for (const char* p = szText; *p; ++p)
		{
			if (*p == '\n')
			{
				if (uCurrentLen > uMaxLineLen) uMaxLineLen = uCurrentLen;
				uCurrentLen = 0;
			}
			else
			{
				uCurrentLen++;
			}
		}
		if (uCurrentLen > uMaxLineLen) uMaxLineLen = uCurrentLen;

		float fDesiredFont = TilePuzzleUI::fTUTORIAL_FONT;
		if (uMaxLineLen > 0)
		{
			float fLineWidth = static_cast<float>(uMaxLineLen) * fDesiredFont * fCHAR_SPACING;
			if (fLineWidth > fTextW)
				fDesiredFont = fTextW / (static_cast<float>(uMaxLineLen) * fCHAR_SPACING);
		}
		m_pxTutorialText->SetFontSize(fDesiredFont);
		m_pxTutorialText->SetText(szText);
	}

	uint32_t GetTutorialStepCount() const
	{
		if (m_uTutorialIndex == 0)
			return 3;
		return 1;
	}

	void UpdateTutorialOverlay(float fDeltaTime)
	{
		if (!m_bTutorialActive)
			return;

		m_fTutorialFadeProgress += fDeltaTime / s_fTutorialFadeDuration;
		if (m_fTutorialFadeProgress > 1.0f)
			m_fTutorialFadeProgress = 1.0f;

		// One-shot diagnostic: log UIText state on first frame
		if (m_fTutorialFadeProgress < 0.1f && m_pxTutorialText)
		{
			Zenith_Maths::Vector4 xBounds = m_pxTutorialText->GetScreenBounds();
			Zenith_Log(LOG_CATEGORY_GENERAL, "TutorialText: visible=%d text='%s' bounds=(%.0f,%.0f,%.0f,%.0f) parent=%p",
				m_pxTutorialText->IsVisible(), m_pxTutorialText->GetText().c_str(),
				xBounds.x, xBounds.y, xBounds.z, xBounds.w, static_cast<void*>(m_pxTutorialText->GetParent()));
		}

		// Pulse the "Tap to continue" hint alpha
		if (m_pxTutorialHintText)
		{
			float fHintAlpha = 0.5f + 0.5f * sinf(m_fTutorialFadeProgress * 6.0f);
			m_pxTutorialHintText->SetColor(Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, fHintAlpha));
		}

		// Check for tap to advance/dismiss (detect mouse-down transition)
		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		if (bMouseDown && !m_bTutorialMouseWasDown && m_fTutorialFadeProgress >= 0.5f)
		{
			m_uTutorialStep++;

			if (m_uTutorialStep >= GetTutorialStepCount())
			{
				DismissTutorial();
			}
			else
			{
				// Multi-step tutorial: update text for next step
				SetTutorialTextWithAutoSize(GetTutorialText());
				m_fTutorialFadeProgress = 0.0f;
			}
		}
		m_bTutorialMouseWasDown = bMouseDown;
	}

	void DismissTutorial()
	{
		m_bTutorialActive = false;
		if (m_pxTutorialOverlay)
			m_pxTutorialOverlay->Hide();
		m_xSaveData.SetTutorialShown(m_uTutorialIndex);
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);
	}

	// ========================================================================
	// Confirmation Dialog System
	// ========================================================================

	void ShowConfirmDialog(TilePuzzleConfirmDialogType eType)
	{
		if (!m_pxConfirmOverlay)
		{
			Zenith_Log(LOG_CATEGORY_GENERAL, "ShowConfirmDialog: overlay is null, performing action directly");
			m_eConfirmDialogType = eType;
			OnConfirmDialogAccept();
			return;
		}

		Zenith_Log(LOG_CATEGORY_GENERAL, "ShowConfirmDialog: overlay=%p isShowing=%d isVisible=%d canvas=%p type=%d",
			m_pxConfirmOverlay, m_pxConfirmOverlay->IsShowing(), m_pxConfirmOverlay->IsVisible(),
			static_cast<void*>(&m_xParentEntity.GetComponent<Zenith_UIComponent>()), static_cast<int>(eType));

		m_bConfirmDialogActive = true;
		m_eConfirmDialogType = eType;

		if (m_pxConfirmText)
		{
			switch (eType)
			{
			case CONFIRM_RESET_SAVE: m_pxConfirmText->SetText("Reset all progress?\nThis cannot be undone."); break;
			case CONFIRM_EXIT_LEVEL: m_pxConfirmText->SetText("Exit level?\nYou will lose 1 life."); break;
			case CONFIRM_SKIP_LEVEL: m_pxConfirmText->SetText("Skip level for 100 coins?"); break;
			default: break;
			}
		}
		if (m_pxConfirmAcceptBtn)
		{
			switch (eType)
			{
			case CONFIRM_RESET_SAVE: m_pxConfirmAcceptBtn->SetText("Reset"); break;
			case CONFIRM_EXIT_LEVEL: m_pxConfirmAcceptBtn->SetText("Exit"); break;
			case CONFIRM_SKIP_LEVEL: m_pxConfirmAcceptBtn->SetText("Skip"); break;
			default: m_pxConfirmAcceptBtn->SetText("OK"); break;
			}
		}
		m_pxConfirmOverlay->Show();
	}

	void UpdateConfirmDialog(float /*fDeltaTime*/)
	{
		// Overlay handles its own rendering and fade — nothing needed here
	}

	static void OnConfirmAcceptClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->OnConfirmDialogAccept();
	}

	static void OnConfirmCancelClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->OnConfirmDialogCancel();
	}

	void OnConfirmDialogAccept()
	{
		m_bConfirmDialogActive = false;
		if (m_pxConfirmOverlay)
			m_pxConfirmOverlay->Hide();
		switch (m_eConfirmDialogType)
		{
		case CONFIRM_RESET_SAVE:
			PerformResetSave();
			break;
		case CONFIRM_EXIT_LEVEL:
			ReturnToMenu();
			break;
		case CONFIRM_SKIP_LEVEL:
			PerformSkip();
			break;
		}
	}

	void OnConfirmDialogCancel()
	{
		m_bConfirmDialogActive = false;
		if (m_pxConfirmOverlay)
			m_pxConfirmOverlay->Hide();
	}

	void PerformResetSave()
	{
		m_xSaveData.Reset();
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);
		m_uCurrentLevelNumber = 1;

		{
			char szBuffer[64];

			if (m_pxMenuCoinText)
			{
				snprintf(szBuffer, sizeof(szBuffer), "%u", m_xSaveData.uCoins);
				m_pxMenuCoinText->SetText(szBuffer);
			}

			if (m_pxLivesText)
			{
				snprintf(szBuffer, sizeof(szBuffer), "%u/%u", m_xSaveData.uLives, TilePuzzleSaveData::uMAX_LIVES);
				m_pxLivesText->SetText(szBuffer);
			}

			if (m_pxStreakText) m_pxStreakText->SetText("0 days");
			if (m_pxRefillBtn) m_pxRefillBtn->SetVisible(false);
			if (m_pxHintTokenText)
			{
				snprintf(szBuffer, sizeof(szBuffer), "%u", m_xSaveData.uFreeHintTokens);
				m_pxHintTokenText->SetText(szBuffer);
			}
		}
	}

	// ========================================================================
	// Target Cat Highlighting (levels 1-5)
	// ========================================================================

	void ActivateTargetCatHighlight(int32_t iShapeIndex)
	{
		DeactivateTargetCatHighlight();

		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return;

		TilePuzzleColor eColor = m_xCurrentLevel.axShapes[iShapeIndex].eColor;
		Zenith_SceneData* pxScene = m_xPuzzleScene.IsValid() ? Zenith_SceneManager::GetSceneData(m_xPuzzleScene) : nullptr;
		if (!pxScene) return;

		for (uint32_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			const TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[i];
			if (xCat.bEliminated || xCat.eColor != eColor)
				continue;
			if (!xCat.uEntityID.IsValid() || !pxScene->EntityExists(xCat.uEntityID))
				continue;

			m_auHighlightedCatIndices.push_back(i);
		}
		m_bTargetHighlightActive = !m_auHighlightedCatIndices.empty();
	}

	void DeactivateTargetCatHighlight()
	{
		if (!m_bTargetHighlightActive)
			return;

		// Reset scale on highlighted cats to their original size
		float fBaseScale = s_fCatRadius * 2.0f;
		Zenith_SceneData* pxScene = m_xPuzzleScene.IsValid() ? Zenith_SceneManager::GetSceneData(m_xPuzzleScene) : nullptr;
		if (pxScene)
		{
			for (uint32_t uIdx : m_auHighlightedCatIndices)
			{
				if (uIdx >= m_xCurrentLevel.axCats.size()) continue;
				const TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[uIdx];
				if (!xCat.uEntityID.IsValid() || !pxScene->EntityExists(xCat.uEntityID))
					continue;
				Zenith_Entity xEntity = pxScene->GetEntity(xCat.uEntityID);
				xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
					Zenith_Maths::Vector3(fBaseScale, fBaseScale, fBaseScale));
			}
		}
		m_auHighlightedCatIndices.clear();
		m_bTargetHighlightActive = false;
	}

	void UpdateTargetCatHighlight(float /*fDeltaTime*/)
	{
		if (!m_bTargetHighlightActive)
			return;

		Zenith_SceneData* pxScene = m_xPuzzleScene.IsValid() ? Zenith_SceneManager::GetSceneData(m_xPuzzleScene) : nullptr;
		if (!pxScene) return;

		// Pulsing scale: oscillate between 1.0x and 1.5x of cat base scale over 0.6s period
		float fBaseScale = s_fCatRadius * 2.0f;
		float fPulse = fBaseScale * (1.0f + 0.25f * sinf(m_fLevelTimer * 10.47f)); // ±25% around base
		for (uint32_t uIdx : m_auHighlightedCatIndices)
		{
			if (uIdx >= m_xCurrentLevel.axCats.size()) continue;
			const TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[uIdx];
			if (xCat.bEliminated) continue;
			if (!xCat.uEntityID.IsValid() || !pxScene->EntityExists(xCat.uEntityID))
				continue;
			Zenith_Entity xEntity = pxScene->GetEntity(xCat.uEntityID);
			xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
				Zenith_Maths::Vector3(fPulse, fPulse, fPulse));
		}
	}

	// ========================================================================
	// Stuck Detection (levels 1-10)
	// ========================================================================

	void UpdateStuckDetection(float fDeltaTime)
	{
		if (m_eState != TILEPUZZLE_STATE_PLAYING || m_uCurrentLevelNumber > 10)
			return;

		m_fTimeSinceLastMove += fDeltaTime;

		// Auto-trigger hint at 90s
		if (m_fTimeSinceLastMove >= 90.0f && !m_bHintActive)
		{
			PerformHint();
			m_fTimeSinceLastMove = 0.0f;
			m_bStuckHintPromptShown = false;
			return;
		}

		// Show "Need a hint?" prompt at 45s
		if (m_fTimeSinceLastMove >= 45.0f && !m_bHintActive)
		{
			m_bStuckHintPromptShown = true;
			Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
			if (pxCanvas)
			{
				int32_t iWinWidth, iWinHeight;
				Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);

				float fPulseAlpha = 0.5f + 0.5f * sinf(m_fTimeSinceLastMove * 4.0f);
				pxCanvas->SubmitText(
					"Need a hint?",
					Zenith_Maths::Vector2(static_cast<float>(iWinWidth) * 0.5f - 100.0f,
						static_cast<float>(iWinHeight) * 0.7f),
					TilePuzzleUI::fHINT_PROMPT_FONT,
					Zenith_Maths::Vector4(1.0f, 0.85f, 0.2f, fPulseAlpha));
			}
		}
	}

	void ResetStuckTimer()
	{
		m_fTimeSinceLastMove = 0.0f;
		m_bStuckHintPromptShown = false;
	}

	// ========================================================================
	// Achievement System
	// ========================================================================

	static const char* GetAchievementName(uint32_t uID)
	{
		switch (uID)
		{
		case ACHIEVEMENT_FIRST_STEPS:     return "First Steps";
		case ACHIEVEMENT_GETTING_STARTED: return "Getting Started";
		case ACHIEVEMENT_HALFWAY:         return "Halfway There";
		case ACHIEVEMENT_CAT_MASTER:      return "Cat Master";
		case ACHIEVEMENT_PERFECT_PUZZLE:  return "Perfect Puzzle";
		case ACHIEVEMENT_SPEED_SOLVER:    return "Speed Solver";
		case ACHIEVEMENT_CAT_LOVER:       return "Cat Lover";
		case ACHIEVEMENT_CAT_COLLECTOR:   return "Cat Collector";
		case ACHIEVEMENT_DAILY_REGULAR:   return "Daily Regular";
		case ACHIEVEMENT_PINBALL_PRO:     return "Pinball Pro";
		default: return "";
		}
	}

	static const char* GetAchievementDescription(uint32_t uID)
	{
		switch (uID)
		{
		case ACHIEVEMENT_FIRST_STEPS:     return "Complete level 1";
		case ACHIEVEMENT_GETTING_STARTED: return "Complete 10 levels";
		case ACHIEVEMENT_HALFWAY:         return "Complete 50 levels";
		case ACHIEVEMENT_CAT_MASTER:      return "Complete all 100 levels";
		case ACHIEVEMENT_PERFECT_PUZZLE:  return "Get 3 stars on any level";
		case ACHIEVEMENT_SPEED_SOLVER:    return "Get 3 stars on 10 levels";
		case ACHIEVEMENT_CAT_LOVER:       return "Collect 10 cats";
		case ACHIEVEMENT_CAT_COLLECTOR:   return "Collect 50 cats";
		case ACHIEVEMENT_DAILY_REGULAR:   return "Reach a 7-day streak";
		case ACHIEVEMENT_PINBALL_PRO:     return "Clear 3 pinball gates";
		default: return "";
		}
	}

	void CheckAchievements()
	{
		// Count stats
		uint32_t uCompletedLevels = 0;
		uint32_t uThreeStarLevels = 0;
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_LEVELS; ++i)
		{
			if (m_xSaveData.axLevelRecords[i].bCompleted) uCompletedLevels++;
			if (m_xSaveData.GetStarRating(i + 1) >= 3) uThreeStarLevels++;
		}

		uint32_t uCats = m_xSaveData.uCatsCollectedCount;
		uint32_t uStreak = m_xSaveData.uDailyStreak;

		// Count cleared pinball gates
		uint32_t uGatesCleared = 0;
		for (uint32_t i = 0; i < TilePuzzleSaveData::uMAX_PINBALL_GATES; ++i)
		{
			if (m_xSaveData.uPinballGateFlags & (1u << i)) uGatesCleared++;
		}

		struct AchievementCheck
		{
			uint32_t uID;
			bool bCondition;
		};

		AchievementCheck axChecks[] = {
			{ ACHIEVEMENT_FIRST_STEPS,     uCompletedLevels >= 1 },
			{ ACHIEVEMENT_GETTING_STARTED, uCompletedLevels >= 10 },
			{ ACHIEVEMENT_HALFWAY,         uCompletedLevels >= 50 },
			{ ACHIEVEMENT_CAT_MASTER,      uCompletedLevels >= 100 },
			{ ACHIEVEMENT_PERFECT_PUZZLE,  uThreeStarLevels >= 1 },
			{ ACHIEVEMENT_SPEED_SOLVER,    uThreeStarLevels >= 10 },
			{ ACHIEVEMENT_CAT_LOVER,       uCats >= 10 },
			{ ACHIEVEMENT_CAT_COLLECTOR,   uCats >= 50 },
			{ ACHIEVEMENT_DAILY_REGULAR,   uStreak >= 7 },
			{ ACHIEVEMENT_PINBALL_PRO,     uGatesCleared >= 3 },
		};

		for (const auto& xCheck : axChecks)
		{
			if (xCheck.bCondition && !m_xSaveData.IsAchievementUnlocked(xCheck.uID))
			{
				m_xSaveData.UnlockAchievement(xCheck.uID);
				m_uLastAchievementUnlocked = xCheck.uID;
				m_fAchievementToastTimer = 2.0f;
			}
		}
	}

	void UpdateAchievementToast(float fDeltaTime)
	{
		if (m_fAchievementToastTimer <= 0.0f)
			return;

		m_fAchievementToastTimer -= fDeltaTime;

		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas || m_uLastAchievementUnlocked >= ACHIEVEMENT_COUNT)
			return;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);

		float fW = static_cast<float>(iWinWidth);
		float fAlpha = (m_fAchievementToastTimer > 0.3f) ? 1.0f : (m_fAchievementToastTimer / 0.3f);

		// Gold banner at top of screen
		float fBannerH = TilePuzzleUI::fTOAST_BANNER_H;
		pxCanvas->SubmitQuad(
			Zenith_Maths::Vector4(0.0f, 0.0f, fW, fBannerH),
			Zenith_Maths::Vector4(0.4f, 0.35f, 0.1f, fAlpha * 0.9f));

		char szText[128];
		snprintf(szText, sizeof(szText), "Achievement Unlocked: %s",
			GetAchievementName(m_uLastAchievementUnlocked));
		pxCanvas->SubmitText(
			szText,
			Zenith_Maths::Vector2(fW * 0.5f - 200.0f, 15.0f),
			TilePuzzleUI::fTOAST_FONT,
			Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, fAlpha));
	}

	void UpdateAchievementsScreen(float /*fDeltaTime*/)
	{
		if (m_eState != TILEPUZZLE_STATE_ACHIEVEMENTS)
			return;

		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas) return;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		float fW = static_cast<float>(iWinWidth);
		float fH = static_cast<float>(iWinHeight);

		// Background
		pxCanvas->SubmitQuad(
			Zenith_Maths::Vector4(0.0f, 0.0f, fW, fH),
			Zenith_Maths::Vector4(0.06f, 0.06f, 0.12f, 1.0f));

		// Title
		pxCanvas->SubmitText(
			"Achievements",
			Zenith_Maths::Vector2(fW * 0.5f - 120.0f, 30.0f),
			TilePuzzleUI::fACHIEV_TITLE_FONT,
			Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, 1.0f));

		// List achievements
		float fStartY = 90.0f;
		float fItemH = TilePuzzleUI::fACHIEV_ITEM_H;
		for (uint32_t i = 0; i < ACHIEVEMENT_COUNT; ++i)
		{
			float fY = fStartY + static_cast<float>(i) * fItemH;
			bool bUnlocked = m_xSaveData.IsAchievementUnlocked(i);

			// Background bar
			Zenith_Maths::Vector4 xBgColor = bUnlocked
				? Zenith_Maths::Vector4(0.2f, 0.18f, 0.08f, 0.8f)
				: Zenith_Maths::Vector4(0.1f, 0.1f, 0.15f, 0.6f);
			pxCanvas->SubmitQuad(
				Zenith_Maths::Vector4(30.0f, fY, fW - 30.0f, fY + fItemH - 5.0f),
				xBgColor);

			// Achievement name
			Zenith_Maths::Vector4 xNameColor = bUnlocked
				? Zenith_Maths::Vector4(1.0f, 0.9f, 0.3f, 1.0f)
				: Zenith_Maths::Vector4(0.5f, 0.5f, 0.5f, 1.0f);
			pxCanvas->SubmitText(
				GetAchievementName(i),
				Zenith_Maths::Vector2(50.0f, fY + 8.0f),
				TilePuzzleUI::fACHIEV_NAME_FONT,
				xNameColor);

			// Description
			pxCanvas->SubmitText(
				GetAchievementDescription(i),
				Zenith_Maths::Vector2(50.0f, fY + 35.0f),
				TilePuzzleUI::fACHIEV_DESC_FONT,
				Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 1.0f));

			// Status icon
			const char* szStatus = bUnlocked ? "\xE2\x98\x85" : "\xE2\x98\x86"; // ★ or ☆
			pxCanvas->SubmitText(
				szStatus,
				Zenith_Maths::Vector2(fW - 70.0f, fY + 15.0f),
				TilePuzzleUI::fACHIEV_ICON_FONT,
				xNameColor);
		}

		// Back button area (tap bottom of screen)
		pxCanvas->SubmitText(
			"Tap to return",
			Zenith_Maths::Vector2(fW * 0.5f - 80.0f, fH - 50.0f),
			TilePuzzleUI::fACHIEV_RETURN_FONT,
			Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, 0.5f + 0.5f * sinf(m_fMenuTimer * 3.0f)));

		// Handle back tap
		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		if (bMouseDown && !m_bConfirmDialogMouseWasDown)
		{
			Zenith_Maths::Vector2_64 xMousePos64;
			Zenith_Input::GetMousePosition(xMousePos64);
			float fMY = static_cast<float>(xMousePos64.y);
			if (fMY > fH - 80.0f)
			{
				StartTransition(TILEPUZZLE_STATE_MAIN_MENU);
			}
		}
		m_bConfirmDialogMouseWasDown = bMouseDown;
	}

	// ========================================================================
	// Meta-game systems (Cat Cafe, Victory, Coins, Lives, Daily Puzzle)
	// ========================================================================
#include "TilePuzzle/Components/TilePuzzle_MetaGame.h"

	// ========================================================================
	// Level Loading
	// ========================================================================
	void ValidateLevelTier(uint32_t uLevelNumber, const TilePuzzleLevelData& xLevel)
	{
		struct TierSpec
		{
			uint32_t uMinGrid, uMaxGrid;
			uint32_t uMinColors, uMaxColors;
			uint32_t uMinBlockers, uMaxBlockers;
		};

		TierSpec xSpec;
		if (uLevelNumber <= 10)        xSpec = { 5,  6, 1, 2, 0, 0 }; // Tutorial
		else if (uLevelNumber <= 25)   xSpec = { 6,  7, 2, 3, 0, 1 }; // Easy
		else if (uLevelNumber <= 45)   xSpec = { 7,  8, 3, 3, 1, 2 }; // Medium
		else if (uLevelNumber <= 65)   xSpec = { 8,  9, 3, 4, 1, 2 }; // Hard
		else if (uLevelNumber <= 80)   xSpec = { 9, 10, 4, 5, 2, 3 }; // Expert
		else                           xSpec = { 9, 10, 4, 5, 2, 3 }; // Master

		// Validate grid size
		Zenith_Assert(xLevel.uGridWidth >= xSpec.uMinGrid && xLevel.uGridWidth <= xSpec.uMaxGrid,
			"Level %u: grid width %u outside tier range [%u, %u]", uLevelNumber, xLevel.uGridWidth, xSpec.uMinGrid, xSpec.uMaxGrid);
		Zenith_Assert(xLevel.uGridHeight >= xSpec.uMinGrid && xLevel.uGridHeight <= xSpec.uMaxGrid,
			"Level %u: grid height %u outside tier range [%u, %u]", uLevelNumber, xLevel.uGridHeight, xSpec.uMinGrid, xSpec.uMaxGrid);

		// Count unique colors used by cats
		bool abColorUsed[TILEPUZZLE_COLOR_COUNT] = {};
		for (const auto& xCat : xLevel.axCats)
		{
			if (xCat.eColor < TILEPUZZLE_COLOR_COUNT)
				abColorUsed[xCat.eColor] = true;
		}
		uint32_t uColorCount = 0;
		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
			if (abColorUsed[i]) uColorCount++;

		Zenith_Assert(uColorCount >= xSpec.uMinColors && uColorCount <= xSpec.uMaxColors,
			"Level %u: %u colors, expected [%u, %u]", uLevelNumber, uColorCount, xSpec.uMinColors, xSpec.uMaxColors);

		// Count blocker shapes (non-draggable)
		uint32_t uBlockerCount = 0;
		for (const auto& xShape : xLevel.axShapes)
		{
			if (xShape.pxDefinition && !xShape.pxDefinition->bDraggable)
				uBlockerCount++;
		}
		Zenith_Assert(uBlockerCount >= xSpec.uMinBlockers && uBlockerCount <= xSpec.uMaxBlockers,
			"Level %u: %u blockers, expected [%u, %u]", uLevelNumber, uBlockerCount, xSpec.uMinBlockers, xSpec.uMaxBlockers);
	}

	void LoadLevelFromFile()
	{
		// Clean up previously generated per-shape meshes
		for (uint32_t i = 0; i < m_apxGeneratedShapeMeshes.GetSize(); ++i)
			delete m_apxGeneratedShapeMeshes.Get(i);
		m_apxGeneratedShapeMeshes.Clear();

		// Use preloaded level data (loaded at boot in OnAwake)
		uint32_t uIndex = m_uCurrentLevelNumber - 1;
		Zenith_Assert(uIndex < m_axPreloadedLevels.GetSize(), "Level %u not preloaded", m_uCurrentLevelNumber);

		m_xCurrentLevel = m_axPreloadedLevels.Get(uIndex);

		// Copy shape definitions and remap pointers
		m_axLoadedShapeDefs.Clear();
		const Zenith_Vector<TilePuzzleShapeDefinition>& axSrcDefs = m_axPreloadedShapeDefs.Get(uIndex);
		for (uint32_t i = 0; i < axSrcDefs.GetSize(); ++i)
			m_axLoadedShapeDefs.PushBack(axSrcDefs.Get(i));

		uint32_t uDefIdx = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			if (m_xCurrentLevel.axShapes[i].pxDefinition)
			{
				m_xCurrentLevel.axShapes[i].pxDefinition = &m_axLoadedShapeDefs.Get(uDefIdx);
				uDefIdx++;
			}
		}

		// Compute solution if the level file didn't contain one (v1 file)
		if (m_xCurrentLevel.axSolution.empty())
		{
			TilePuzzle_Solver::SolveLevelWithPath(m_xCurrentLevel, m_xCurrentLevel.axSolution);
			if (!m_xCurrentLevel.axSolution.empty())
			{
				char szPath[ZENITH_MAX_PATH_LENGTH];
				snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/level_%04u.tlvl", m_uCurrentLevelNumber);
				Zenith_DataStream xWriteStream;
				TilePuzzleLevelSerialize::Write(xWriteStream, m_xCurrentLevel);
				xWriteStream.WriteToFile(szPath);
			}
		}

		// Flip board vertically so gridY=0 appears at screen top (matching PNG layout)
		{
			uint32_t uW = m_xCurrentLevel.uGridWidth;
			uint32_t uH = m_xCurrentLevel.uGridHeight;

			// Reverse cell array rows
			std::vector<TilePuzzleCellType> aFlipped(m_xCurrentLevel.aeCells.size());
			for (uint32_t y = 0; y < uH; ++y)
				for (uint32_t x = 0; x < uW; ++x)
					aFlipped[y * uW + x] = m_xCurrentLevel.aeCells[(uH - 1 - y) * uW + x];
			m_xCurrentLevel.aeCells = std::move(aFlipped);

			// Flip shape origins
			for (auto& xShape : m_xCurrentLevel.axShapes)
				xShape.iOriginY = static_cast<int32_t>(uH - 1) - xShape.iOriginY;

			// Negate cell offset Y in owned shape definitions
			for (uint32_t d = 0; d < m_axLoadedShapeDefs.GetSize(); ++d)
				for (size_t c = 0; c < m_axLoadedShapeDefs.Get(d).axCells.size(); ++c)
					m_axLoadedShapeDefs.Get(d).axCells[c].iY = -m_axLoadedShapeDefs.Get(d).axCells[c].iY;

			// Flip cat positions
			for (auto& xCat : m_xCurrentLevel.axCats)
				xCat.iGridY = static_cast<int32_t>(uH - 1) - xCat.iGridY;

			// Flip solution end positions
			for (auto& xMove : m_xCurrentLevel.axSolution)
				xMove.iEndY = static_cast<int32_t>(uH - 1) - xMove.iEndY;
		}

		CreateLevelVisuals();
		UpdateCameraForGridSize();

		m_uMoveCount = 0;
		m_iLastMovedShapeIndex = -1;
		m_iSelectedShapeIndex = -1;
		m_iPreviousSelectedShapeIndex = -1;
		m_bDragging = false;
		m_bMouseWasDown = false;
		m_bPendingLevelComplete = false;
		m_iDragShapeIndex = -1;
		m_iDragGrabOffsetX = 0;
		m_iDragGrabOffsetY = 0;
		m_fLevelTimer = 0.f;
		m_bDragUndoPushed = false;
		m_eState = TILEPUZZLE_STATE_PLAYING;

		// Reset undo stack, hint, and highlighting for new level
		m_axUndoStack.Clear();
		m_bFreeUndoAvailable = true;
		ClearHint();
		m_uStarsEarned = 0;
		m_bTargetHighlightActive = false;
		m_auHighlightedCatIndices.clear();

		UpdateSelectionHighlight();

		// Check if this level has a tutorial to show
		TryShowTutorial();

		// Set background color based on difficulty tier
		Zenith_GraphicsOptions::Get().m_xSkyboxColour = GetBackgroundColorForLevel(m_uCurrentLevelNumber);
	}

	void ResetLevel()
	{
		m_uResetCount++;

		// After enough resets, offer the skip option
		if (m_uResetCount >= s_uResetsBeforeSkipOffer && !m_bSkipOffered)
		{
			m_bSkipOffered = true;
			if (m_pxSkipBtn)
			{
				m_pxSkipBtn->SetVisible(true);
			}
		}

		StartNewLevel();
	}

	void NextLevel()
	{
		if (m_uCurrentLevelNumber >= m_uAvailableLevelCount)
			return;

		m_uCurrentLevelNumber++;
		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// If next level is a pinball gate, transition to pinball scene
		uint32_t uGateIndex = 0;
		if (TilePuzzle_IsGateLevel(m_uCurrentLevelNumber, &uGateIndex)
			&& !m_xSaveData.IsPinballGateCleared(uGateIndex))
		{
			m_bVictoryOverlayActive = false;
			SetVictoryOverlayVisible(false);
			if (m_xPuzzleScene.IsValid())
			{
				ClearEntityReferences();
				Zenith_SceneManager::UnloadScene(m_xPuzzleScene);
				m_xPuzzleScene = Zenith_Scene();
			}
			TilePuzzle::g_uPinballRequestedGate = uGateIndex;
			Zenith_SceneManager::LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
			return;
		}

		// Reset skip state for new level
		m_uResetCount = 0;
		m_bSkipOffered = false;
		if (m_pxSkipBtn) m_pxSkipBtn->SetVisible(false);

		StartNewLevel();
	}

	// ========================================================================
	// Input Handling
	// ========================================================================
	int32_t GetShapeAtPosition(int32_t iX, int32_t iY) const
	{
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition->bDraggable)
				continue;
			if (xShape.bRemoved)
				continue;

			for (const auto& xOffset : xShape.pxDefinition->axCells)
			{
				int32_t iCellX = xShape.iOriginX + xOffset.iX;
				int32_t iCellY = xShape.iOriginY + xOffset.iY;
				if (iCellX == iX && iCellY == iY)
				{
					return static_cast<int32_t>(i);
				}
			}
		}
		return -1;
	}

	// ========================================================================
	// Movement Logic
	// ========================================================================
	bool TryMoveShape(int32_t iShapeIndex, TilePuzzleDirection eDir)
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return false;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!xShape.pxDefinition->bDraggable)
			return false;
		if (xShape.bRemoved)
			return false;

		int32_t iDeltaX, iDeltaY;
		TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);

		if (!CanMoveShape(iShapeIndex, iDeltaX, iDeltaY))
			return false;

		// Save undo state before move
		PushUndoState();

		// Clear hint when player makes a move
		ClearHint();

		// Start slide animation
		m_iSlidingShapeIndex = iShapeIndex;
		m_eSlideDirection = eDir;
		m_fSlideProgress = 0.0f;
		m_xSlideStartPos = GridToWorld(static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY), s_fShapeHeight);
		m_xSlideEndPos = GridToWorld(static_cast<float>(xShape.iOriginX + iDeltaX), static_cast<float>(xShape.iOriginY + iDeltaY), s_fShapeHeight);

		xShape.iOriginX += iDeltaX;
		xShape.iOriginY += iDeltaY;

		if (m_iLastMovedShapeIndex != iShapeIndex)
		{
			m_uMoveCount++;
			m_iLastMovedShapeIndex = iShapeIndex;
		}
		m_eState = TILEPUZZLE_STATE_SHAPE_SLIDING;
		return true;
	}

	bool MoveShapeImmediate(int32_t iShapeIndex, TilePuzzleDirection eDir)
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return false;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!xShape.pxDefinition->bDraggable)
			return false;
		if (xShape.bRemoved)
			return false;

		int32_t iDeltaX, iDeltaY;
		TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);

		if (!CanMoveShape(iShapeIndex, iDeltaX, iDeltaY))
			return false;

		// Save undo state before move (only on first drag step to avoid flooding the stack)
		if (!m_bDragUndoPushed)
		{
			PushUndoState();
			ClearHint();
			m_bDragUndoPushed = true;
		}

		xShape.iOriginX += iDeltaX;
		xShape.iOriginY += iDeltaY;
		m_eLastDragDirection = eDir;

		if (m_iLastMovedShapeIndex != iShapeIndex)
		{
			m_uMoveCount++;
			m_iLastMovedShapeIndex = iShapeIndex;
		}

		// Reset stuck detection timer on successful move
		ResetStuckTimer();

		CheckCatElimination();
		if (IsLevelComplete())
		{
			m_bPendingLevelComplete = true;
		}

		return true;
	}

	bool CanMoveShape(int32_t iShapeIndex, int32_t iDeltaX, int32_t iDeltaY) const
	{
		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];

		// Build ShapeState array for all draggable shapes
		Zenith_Vector<TilePuzzle_Rules::ShapeState> axDraggableStates;
		size_t uMovingDraggableIdx = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xOther = m_xCurrentLevel.axShapes[i];
			if (!xOther.pxDefinition || !xOther.pxDefinition->bDraggable)
				continue;
			if (xOther.bRemoved)
				continue;

			if (static_cast<int32_t>(i) == iShapeIndex)
				uMovingDraggableIdx = axDraggableStates.GetSize();

			TilePuzzle_Rules::ShapeState xState;
			xState.pxDefinition = xOther.pxDefinition;
			xState.iOriginX = xOther.iOriginX;
			xState.iOriginY = xOther.iOriginY;
			xState.eColor = xOther.eColor;
			xState.uUnlockThreshold = xOther.uUnlockThreshold;
			axDraggableStates.PushBack(xState);
		}

		// Build CatState array and elimination mask
		Zenith_Vector<TilePuzzle_Rules::CatState> axCatStates;
		uint32_t uEliminatedMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			TilePuzzle_Rules::CatState xCatState;
			xCatState.iGridX = m_xCurrentLevel.axCats[i].iGridX;
			xCatState.iGridY = m_xCurrentLevel.axCats[i].iGridY;
			xCatState.eColor = m_xCurrentLevel.axCats[i].eColor;
			xCatState.bOnBlocker = m_xCurrentLevel.axCats[i].bOnBlocker;
			axCatStates.PushBack(xCatState);

			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedMask |= (1u << i);
		}

		int32_t iNewOriginX = xShape.iOriginX + iDeltaX;
		int32_t iNewOriginY = xShape.iOriginY + iDeltaY;

		return TilePuzzle_Rules::CanMoveShape(
			m_xCurrentLevel,
			axDraggableStates.GetDataPointer(), axDraggableStates.GetSize(),
			uMovingDraggableIdx,
			iNewOriginX, iNewOriginY,
			axCatStates.GetDataPointer(), axCatStates.GetSize(),
			uEliminatedMask);
	}

	// ========================================================================
	// Shape Removal
	// ========================================================================
	void RemoveShape(size_t uLevelShapeIndex)
	{
		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[uLevelShapeIndex];
		xShape.bRemoved = true;

		if (m_xPuzzleScene.IsValid())
		{
			Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
			if (pxSceneData && xShape.xEntityID.IsValid() && pxSceneData->EntityExists(xShape.xEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
				if (xEntity.IsValid())
				{
					Zenith_TweenComponent::ScaleTo(xEntity, Zenith_Maths::Vector3(0.0f), s_fEliminationDuration, EASING_BACK_IN);
					Zenith_SceneManager::Destroy(xEntity, s_fEliminationDuration + 0.01f);
				}
			}
		}
		xShape.xEntityID = Zenith_EntityID();

		if (m_iSelectedShapeIndex == static_cast<int32_t>(uLevelShapeIndex))
		{
			m_iSelectedShapeIndex = -1;
		}

		if (m_bDragging && m_iDragShapeIndex == static_cast<int32_t>(uLevelShapeIndex))
		{
			m_bDragging = false;
			m_iDragShapeIndex = -1;
			m_iSelectedShapeIndex = -1;
		}

		if (m_iSlidingShapeIndex == static_cast<int32_t>(uLevelShapeIndex))
		{
			m_iSlidingShapeIndex = -1;
		}
	}

	// ========================================================================
	// Cat Elimination
	// ========================================================================
	void CheckCatElimination()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		// Build ShapeState array for all draggable shapes (skip removed)
		Zenith_Vector<TilePuzzle_Rules::ShapeState> axDraggableStates;
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
				continue;
			if (xShape.bRemoved)
				continue;

			TilePuzzle_Rules::ShapeState xState;
			xState.pxDefinition = xShape.pxDefinition;
			xState.iOriginX = xShape.iOriginX;
			xState.iOriginY = xShape.iOriginY;
			xState.eColor = xShape.eColor;
			xState.uUnlockThreshold = xShape.uUnlockThreshold;
			axDraggableStates.PushBack(xState);
		}

		// Build CatState array and current elimination mask
		Zenith_Vector<TilePuzzle_Rules::CatState> axCatStates;
		uint32_t uOldMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			TilePuzzle_Rules::CatState xCatState;
			xCatState.iGridX = m_xCurrentLevel.axCats[i].iGridX;
			xCatState.iGridY = m_xCurrentLevel.axCats[i].iGridY;
			xCatState.eColor = m_xCurrentLevel.axCats[i].eColor;
			xCatState.bOnBlocker = m_xCurrentLevel.axCats[i].bOnBlocker;
			axCatStates.PushBack(xCatState);

			if (m_xCurrentLevel.axCats[i].bEliminated)
				uOldMask |= (1u << i);
		}

		uint32_t uNewlyEliminated = TilePuzzle_Rules::ComputeNewlyEliminatedCats(
			axDraggableStates.GetDataPointer(), axDraggableStates.GetSize(),
			axCatStates.GetDataPointer(), axCatStates.GetSize(),
			uOldMask);

		// Apply elimination: set bEliminated and destroy entities for newly eliminated cats
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (!(uNewlyEliminated & (1u << i)))
				continue;

			TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[i];
			xCat.bEliminated = true;

			if (xCat.uEntityID.IsValid() && pxSceneData->EntityExists(xCat.uEntityID))
			{
				Zenith_Entity xCatEntity = pxSceneData->GetEntity(xCat.uEntityID);
				if (xCatEntity.IsValid())
				{
					// Pop: scale up 1.3x briefly, then shrink to 0
					Zenith_Maths::Vector3 xCatScale(s_fCatRadius * 2.0f);
					Zenith_Maths::Vector3 xPopScale = xCatScale * 1.3f;

					Zenith_TweenComponent& xTween = xCatEntity.HasComponent<Zenith_TweenComponent>()
						? xCatEntity.GetComponent<Zenith_TweenComponent>()
						: xCatEntity.AddComponent<Zenith_TweenComponent>();
					xTween.TweenScaleFromTo(xCatScale, xPopScale, 0.08f, EASING_QUAD_OUT);

					// After pop, shrink to 0
					xTween.TweenScaleFromTo(xPopScale, Zenith_Maths::Vector3(0.0f), 0.2f, EASING_BACK_IN);
					xTween.SetDelay(0.08f);

					Zenith_SceneManager::Destroy(xCatEntity, s_fEliminationDuration + 0.01f);
				}
			}

			// Burst color-matched elimination particles at cat position
			Zenith_SceneData* pxParentSceneData = m_xParentEntity.GetSceneData();
			if (m_uEliminationEmitterID.IsValid() && pxParentSceneData && pxParentSceneData->EntityExists(m_uEliminationEmitterID))
			{
				Zenith_Entity xEmitter = pxParentSceneData->GetEntity(m_uEliminationEmitterID);
				xEmitter.GetComponent<Zenith_TransformComponent>().SetPosition(
					GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));

				// Tint particles to match the eliminated cat's color
				Flux_ParticleEmitterConfig* pxConfig = xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().GetConfig();
				if (pxConfig)
				{
					pxConfig->m_xColorStart = GetParticleColorForTile(xCat.eColor);
				}

				xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(25);
			}

			xCat.uEntityID = Zenith_EntityID();
		}

		// Check if any conditional shapes just became unlocked
		if (uNewlyEliminated != 0)
		{
			uint32_t uNewEliminatedCount = 0;
			for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
			{
				if (m_xCurrentLevel.axCats[i].bEliminated)
					uNewEliminatedCount++;
			}

			uint32_t uOldEliminatedCount = 0;
			for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
			{
				if (uOldMask & (1u << i))
					uOldEliminatedCount++;
			}

			for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
			{
				const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
				if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable || xShape.bRemoved)
					continue;
				if (xShape.uUnlockThreshold == 0)
					continue;

				// Was locked before, now unlocked
				if (uOldEliminatedCount < xShape.uUnlockThreshold && uNewEliminatedCount >= xShape.uUnlockThreshold)
				{
					// Burst golden particles at the shape's position
					Zenith_SceneData* pxParentSceneData2 = m_xParentEntity.GetSceneData();
					if (m_uEliminationEmitterID.IsValid() && pxParentSceneData2 && pxParentSceneData2->EntityExists(m_uEliminationEmitterID))
					{
						Zenith_Entity xEmitter = pxParentSceneData2->GetEntity(m_uEliminationEmitterID);
						xEmitter.GetComponent<Zenith_TransformComponent>().SetPosition(
							GridToWorld(static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY), s_fShapeHeight + 0.3f));

						Flux_ParticleEmitterConfig* pxConfig = xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().GetConfig();
						if (pxConfig)
						{
							pxConfig->m_xColorStart = Zenith_Maths::Vector4(1.0f, 0.85f, 0.2f, 1.0f); // Golden
						}
						xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(15);
					}

					// Scale bounce to celebrate unlock
					if (m_xPuzzleScene.IsValid())
					{
						Zenith_SceneData* pxSD = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
						if (pxSD && pxSD->EntityExists(xShape.xEntityID))
						{
							Zenith_Entity xShapeEntity = pxSD->GetEntity(xShape.xEntityID);
							if (xShapeEntity.IsValid())
							{
								Zenith_Maths::Vector3 xBaseScale(s_fCellSize, s_fShapeHeight * 2.0f, s_fCellSize);
								Zenith_TweenComponent& xTween = xShapeEntity.HasComponent<Zenith_TweenComponent>()
									? xShapeEntity.GetComponent<Zenith_TweenComponent>()
									: xShapeEntity.AddComponent<Zenith_TweenComponent>();
								xTween.CancelByProperty(TWEEN_PROPERTY_SCALE);
								xTween.TweenScaleFromTo(xBaseScale * 1.15f, xBaseScale, 0.2f, EASING_ELASTIC_OUT);
							}
						}
					}
				}
			}
		}

		// Per-color removal: if all cats of a color are eliminated, remove all shapes of that color
		if (uNewlyEliminated != 0)
		{
			uint32_t uFullMask = uOldMask | uNewlyEliminated;

			for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
			{
				TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
				if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
					continue;
				if (xShape.bRemoved)
					continue;

				// Check if all cats of this shape's color are eliminated
				bool bAllColorCatsEliminated = true;
				for (size_t j = 0; j < m_xCurrentLevel.axCats.size(); ++j)
				{
					if (m_xCurrentLevel.axCats[j].eColor == xShape.eColor &&
						!(uFullMask & (1u << j)))
					{
						bAllColorCatsEliminated = false;
						break;
					}
				}

				if (bAllColorCatsEliminated)
				{
					RemoveShape(i);
				}
			}
		}
	}

	bool IsLevelComplete() const
	{
		// All cats must be eliminated (shapes auto-remove via per-color pool)
		uint32_t uEliminatedMask = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedMask |= (1u << i);
		}
		return TilePuzzle_Rules::AreAllCatsEliminated(
			uEliminatedMask, static_cast<uint32_t>(m_xCurrentLevel.axCats.size()));
	}

	size_t CountRemainingCats() const
	{
		size_t uCount = 0;
		for (const auto& xCat : m_xCurrentLevel.axCats)
		{
			if (!xCat.bEliminated)
				uCount++;
		}
		return uCount;
	}

	// ========================================================================
	// Animation
	// ========================================================================
	void UpdateSlideAnimation(float fDeltaTime)
	{
		m_fSlideProgress += fDeltaTime / s_fSlideAnimationDuration;

		if (m_fSlideProgress >= 1.0f)
		{
			m_fSlideProgress = 1.0f;

			// Bounce on arrival: quick scale overshoot then settle
			BounceShapeOnArrival(m_iSlidingShapeIndex);

			m_iSlidingShapeIndex = -1;
			m_eState = TILEPUZZLE_STATE_CHECK_ELIMINATION;
		}
	}

	void BounceShapeOnArrival(int32_t iShapeIndex)
	{
		SnapShapeVisuals(iShapeIndex);
	}

	void SnapShapeVisuals(int32_t iShapeIndex)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!pxSceneData->EntityExists(xShape.xEntityID))
			return;

		Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
		if (!xEntity.IsValid())
			return;

		Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(GridToWorld(
			static_cast<float>(xShape.iOriginX),
			static_cast<float>(xShape.iOriginY),
			s_fShapeHeight));
	}

	// ========================================================================
	// Rendering
	// ========================================================================
	void CreateLevelVisuals()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData || !TilePuzzle::Resources().m_xCellPrefab.GetDirect() || !TilePuzzle::Resources().m_xCellPrefab.GetDirect()->IsValid())
		{
			return;
		}

		// Create floor cells
		for (uint32_t y = 0; y < m_xCurrentLevel.uGridHeight; ++y)
		{
			for (uint32_t x = 0; x < m_xCurrentLevel.uGridWidth; ++x)
			{
				uint32_t uIdx = y * m_xCurrentLevel.uGridWidth + x;
				if (m_xCurrentLevel.aeCells[uIdx] == TILEPUZZLE_CELL_FLOOR)
				{
					Zenith_Entity xFloorEntity = TilePuzzle::Resources().m_xCellPrefab.GetDirect()->Instantiate(pxSceneData, "Floor");
					if (!xFloorEntity.IsValid())
						continue;

					Zenith_TransformComponent& xTransform = xFloorEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(GridToWorld(static_cast<float>(x), static_cast<float>(y), 0.0f));
					xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize * 0.95f, s_fFloorHeight, s_fCellSize * 0.95f));

					Zenith_ModelComponent& xModel = xFloorEntity.AddComponent<Zenith_ModelComponent>();
					xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xFloorMaterial.GetDirect());

					uint32_t uKey = y * 1000 + x;
					m_axFloorEntityIDs[uKey] = xFloorEntity.GetEntityID();
				}
			}
		}

		// Create shape visuals (one entity per shape with merged mesh)
		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			Zenith_MaterialAsset* pxMaterial = m_xBlockerMaterial.GetDirect();
			if (xShape.pxDefinition->bDraggable && xShape.eColor < TILEPUZZLE_COLOR_COUNT)
			{
				pxMaterial = m_axShapeMaterials[xShape.eColor].GetDirect();
			}

			// Generate mesh from actual loaded cell offsets (which may be rotated)
			Flux_MeshGeometry* pxShapeMesh = new Flux_MeshGeometry();
			TilePuzzle::GenerateShapeMeshFromDefinition(*xShape.pxDefinition, *pxShapeMesh);
			m_apxGeneratedShapeMeshes.PushBack(pxShapeMesh);

			Zenith_Entity xShapeEntity = TilePuzzle::Resources().m_xShapeCubePrefab.GetDirect()->Instantiate(pxSceneData, "Shape");
			Zenith_TransformComponent& xTransform = xShapeEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(
				static_cast<float>(xShape.iOriginX),
				static_cast<float>(xShape.iOriginY),
				s_fShapeHeight));
			xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize, s_fShapeHeight * 2.0f, s_fCellSize));

			Zenith_ModelComponent& xModel = xShapeEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*pxShapeMesh, *pxMaterial);

			xShape.xEntityID = xShapeEntity.GetEntityID();
		}

		// Create cat visuals
		for (auto& xCat : m_xCurrentLevel.axCats)
		{
			Zenith_Entity xCatEntity = TilePuzzle::Resources().m_xCatPrefab.GetDirect()->Instantiate(pxSceneData, "Cat");
			Zenith_TransformComponent& xTransform = xCatEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));
			xTransform.SetScale(Zenith_Maths::Vector3(s_fCatRadius * 2.0f));

			Zenith_ModelComponent& xModel = xCatEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxCatGeometry, *m_axCatMaterials[xCat.eColor].GetDirect());

			xCat.uEntityID = xCatEntity.GetEntityID();
		}

		// Overhead light for specular highlights on shapes and cats
		{
			Zenith_Entity xLight(pxSceneData, "PuzzleLight");
			Zenith_TransformComponent& xT = xLight.GetComponent<Zenith_TransformComponent>();
			float fGridExtent = static_cast<float>(m_xCurrentLevel.uGridHeight > m_xCurrentLevel.uGridWidth ? m_xCurrentLevel.uGridHeight : m_xCurrentLevel.uGridWidth);
			xT.SetPosition(Zenith_Maths::Vector3(0.f, fGridExtent * 0.8f, 0.f));
			Zenith_LightComponent& xLC = xLight.AddComponent<Zenith_LightComponent>();
			xLC.SetLightType(LIGHT_TYPE_POINT);
			xLC.SetColor(Zenith_Maths::Vector3(1.0f, 0.95f, 0.9f));
			xLC.SetIntensity(150.f);
			xLC.SetRange(30.f);
		}
	}

	void UpdateVisuals(float fDeltaTime)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		// Update shape position (slide animation with ease-out cubic)
		if (m_eState == TILEPUZZLE_STATE_SHAPE_SLIDING && m_iSlidingShapeIndex >= 0)
		{
			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iSlidingShapeIndex];
			float fEased = Zenith_ApplyEasing(EASING_CUBIC_OUT, m_fSlideProgress);
			Zenith_Maths::Vector3 xCurrentPos = m_xSlideStartPos + (m_xSlideEndPos - m_xSlideStartPos) * fEased;

			if (pxSceneData->EntityExists(xShape.xEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
				if (xEntity.IsValid())
				{
					xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xCurrentPos);
				}
			}
		}

		// Decay drag overshoot offset
		if (glm::length(m_xDragOvershootOffset) > 0.001f)
		{
			float fDecay = s_fOvershootDecaySpeed * fDeltaTime;
			m_xDragOvershootOffset *= glm::max(0.0f, 1.0f - fDecay);
		}
		else
		{
			m_xDragOvershootOffset = Zenith_Maths::Vector3(0.0f);
		}

		// Lerp dragged shape toward its logical grid position + overshoot offset
		if (m_bDragging && m_iDragShapeIndex >= 0)
		{
			static constexpr float s_fDragLerpSpeed = 20.0f;
			float fLerpFactor = fDeltaTime * s_fDragLerpSpeed;
			if (fLerpFactor > 1.0f)
				fLerpFactor = 1.0f;

			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
			if (pxSceneData->EntityExists(xShape.xEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
				if (xEntity.IsValid())
				{
					Zenith_Maths::Vector3 xTargetPos = GridToWorld(
						static_cast<float>(xShape.iOriginX),
						static_cast<float>(xShape.iOriginY),
						s_fShapeHeight);

					Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
					Zenith_Maths::Vector3 xCurrentPos;
					xTransform.GetPosition(xCurrentPos);

					Zenith_Maths::Vector3 xLerpTarget = xTargetPos + m_xDragOvershootOffset;
					xTransform.SetPosition(glm::mix(xCurrentPos, xLerpTarget, fLerpFactor));
				}
			}
		}

		// If level completion is pending and the drag has already ended (shape removed), complete now
		if (m_bPendingLevelComplete && !m_bDragging)
		{
			m_bPendingLevelComplete = false;
			OnLevelCompleted();
			return;
		}

		// Check if drag lerp has reached the target while level completion is pending
		if (m_bPendingLevelComplete && m_bDragging && m_iDragShapeIndex >= 0)
		{
			TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iDragShapeIndex];
			bool bReachedTarget = true;

			if (pxSceneData->EntityExists(xShape.xEntityID))
			{
				Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
				if (xEntity.IsValid())
				{
					Zenith_Maths::Vector3 xTargetPos = GridToWorld(
						static_cast<float>(xShape.iOriginX),
						static_cast<float>(xShape.iOriginY), s_fShapeHeight);
					Zenith_Maths::Vector3 xCurPos;
					xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xCurPos);

					if (glm::length(xTargetPos - xCurPos) > 0.01f)
					{
						bReachedTarget = false;
					}
				}
			}

			if (bReachedTarget)
			{
				SnapShapeVisuals(m_iDragShapeIndex);
				m_bDragging = false;
				m_iDragShapeIndex = -1;
				m_iSelectedShapeIndex = -1;
				m_bPendingLevelComplete = false;
				OnLevelCompleted();
			}
		}

		// Cat idle bob animation
		m_fCatBobTimer += fDeltaTime;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[i];
			if (xCat.bEliminated || !xCat.uEntityID.IsValid())
				continue;
			if (!pxSceneData->EntityExists(xCat.uEntityID))
				continue;

			Zenith_Entity xCatEntity = pxSceneData->GetEntity(xCat.uEntityID);
			if (!xCatEntity.IsValid())
				continue;

			// Each cat has a phase offset based on index so they don't all bob in sync
			float fPhase = static_cast<float>(i) * 1.2f;
			float fBob = sinf(m_fCatBobTimer * 3.14159f + fPhase) * 0.02f;

			Zenith_Maths::Vector3 xBasePos = GridToWorld(
				static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight);
			xBasePos.y += fBob;
			xCatEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xBasePos);
		}

		// Camera effects (shake / zoom pulse)
		UpdateScreenShake(fDeltaTime);
		UpdateZoomPulse(fDeltaTime);
		if (m_fScreenShakeTimer <= 0.0f && m_fZoomPulseTimer <= 0.0f && m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			m_xParentEntity.GetComponent<Zenith_CameraComponent>().SetPosition(m_xCameraBasePosition);
		}

		// Render text indicators above conditional (locked) shapes
		RenderConditionalShapeIndicators();
		RenderHintIndicator();
		RenderHintSolvingIndicator();

		UpdateSelectionHighlight();
	}

	void RenderConditionalShapeIndicators()
	{
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		// Count currently eliminated cats
		uint32_t uEliminatedCount = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedCount++;
		}

		// Build VP matrix from camera (consistent with ScreenSpaceToWorldSpace)
		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
		Zenith_Maths::Matrix4 xViewMat, xProjMat;
		xCam.BuildViewMatrix(xViewMat);
		xCam.BuildProjectionMatrix(xProjMat);
		Zenith_Maths::Matrix4 xVPMat = xProjMat * xViewMat;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		if (iWinWidth <= 0 || iWinHeight <= 0)
			return;

		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
				continue;
			if (xShape.uUnlockThreshold == 0)
				continue;

			// Calculate remaining cats needed
			uint32_t uRemaining = 0;
			if (uEliminatedCount < xShape.uUnlockThreshold)
				uRemaining = xShape.uUnlockThreshold - uEliminatedCount;

			if (uRemaining == 0)
				continue; // Already unlocked, no indicator needed

			// Get world position above the shape's origin
			Zenith_Maths::Vector3 xWorldPos = GridToWorld(
				static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY),
				s_fShapeHeight + 0.8f);

			// Project to screen space (same coordinate system as ScreenToGrid)
			Zenith_Maths::Vector4 xClipPos = xVPMat * Zenith_Maths::Vector4(xWorldPos, 1.0f);
			if (xClipPos.w <= 0.0f)
				continue;

			xClipPos.x /= xClipPos.w;
			xClipPos.y /= xClipPos.w;

			float fScreenX = (xClipPos.x + 1.0f) * 0.5f * static_cast<float>(iWinWidth);
			float fScreenY = (xClipPos.y + 1.0f) * 0.5f * static_cast<float>(iWinHeight);

			// Render the lock indicator text
			char szText[8];
			snprintf(szText, sizeof(szText), "%u", uRemaining);

			pxCanvas->SubmitText(
				szText,
				Zenith_Maths::Vector2(fScreenX - 32.0f, fScreenY - 64.0f),
				128.0f,
				Zenith_Maths::Vector4(1.0f, 0.8f, 0.2f, 1.0f));
		}
	}

	void UpdateSelectionHighlight()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		bool bShapeSelectionChanged = (m_iPreviousSelectedShapeIndex != m_iSelectedShapeIndex);
		// Update shape highlighting if selection changed
		if (bShapeSelectionChanged)
		{
			Zenith_Maths::Vector3 xBaseScale(s_fCellSize, s_fShapeHeight * 2.0f, s_fCellSize);

			// Remove highlight from previously selected shape
			if (m_iPreviousSelectedShapeIndex >= 0 &&
				m_iPreviousSelectedShapeIndex < static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			{
				TilePuzzleShapeInstance& xPrevShape = m_xCurrentLevel.axShapes[m_iPreviousSelectedShapeIndex];
				if (xPrevShape.pxDefinition->bDraggable)
				{
					Zenith_MaterialAsset* pxNormalMaterial = m_axShapeMaterials[xPrevShape.eColor].GetDirect();
					if (pxSceneData->EntityExists(xPrevShape.xEntityID))
					{
						Zenith_Entity xEntity = pxSceneData->GetEntity(xPrevShape.xEntityID);
						if (xEntity.IsValid() && xEntity.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xEntity.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshes() > 0 && xModel.GetModelInstance())
							{
								xModel.GetModelInstance()->SetMaterial(0, pxNormalMaterial);
							}
						}

						// Reset scale: cancel breathing pulse, return to base
						if (xEntity.HasComponent<Zenith_TweenComponent>())
						{
							Zenith_TweenComponent& xTween = xEntity.GetComponent<Zenith_TweenComponent>();
							xTween.CancelByProperty(TWEEN_PROPERTY_SCALE);
							xTween.TweenScale(xBaseScale, 0.1f, EASING_CUBIC_OUT);
						}
					}
				}
			}

			// Apply highlight to newly selected shape
			if (m_iSelectedShapeIndex >= 0 &&
				m_iSelectedShapeIndex < static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			{
				TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iSelectedShapeIndex];
				if (xShape.pxDefinition->bDraggable)
				{
					Zenith_MaterialAsset* pxHighlightMaterial = m_axShapeMaterialsHighlighted[xShape.eColor].GetDirect();
					if (pxSceneData->EntityExists(xShape.xEntityID))
					{
						Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
						if (xEntity.IsValid() && xEntity.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xEntity.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshes() > 0 && xModel.GetModelInstance())
							{
								xModel.GetModelInstance()->SetMaterial(0, pxHighlightMaterial);
							}
						}

						// Pickup bump + breathing pulse
						Zenith_TweenComponent& xTween = xEntity.HasComponent<Zenith_TweenComponent>()
							? xEntity.GetComponent<Zenith_TweenComponent>()
							: xEntity.AddComponent<Zenith_TweenComponent>();
						xTween.CancelByProperty(TWEEN_PROPERTY_SCALE);

						// Breathing pulse: oscillate +-5% around base scale
						Zenith_Maths::Vector3 xPulseMin = xBaseScale * 0.97f;
						Zenith_Maths::Vector3 xPulseMax = xBaseScale * 1.05f;
						xTween.TweenScaleFromTo(xPulseMin, xPulseMax, 0.8f, EASING_SINE_IN_OUT);
						xTween.SetLoop(true, true);
					}
				}
			}

			m_iPreviousSelectedShapeIndex = m_iSelectedShapeIndex;
		}

	}

	void UpdateUI()
	{
		char szBuffer[128];

		// Update level text
		if (m_pxLevelText)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Level %u", m_uCurrentLevelNumber);
			m_pxLevelText->SetText(szBuffer);
		}

		// Update move counter
		if (m_pxMovesText)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Moves: %u / Par: %u", m_uMoveCount, m_xCurrentLevel.uMinimumMoves);
			m_pxMovesText->SetText(szBuffer);
		}

		// Update cats remaining
		size_t uRemaining = CountRemainingCats();
		size_t uTotal = m_xCurrentLevel.axCats.size();
		if (m_pxCatsText)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Cats: %zu / %zu", uTotal - uRemaining, uTotal);
			m_pxCatsText->SetText(szBuffer);
		}

		// Update coin display
		if (m_pxHUDCoinsText)
		{
			snprintf(szBuffer, sizeof(szBuffer), "%u", m_xSaveData.uCoins);
			m_pxHUDCoinsText->SetText(szBuffer);
		}

		// Update undo button label with cost info
		if (m_pxUndoBtn)
		{
			if (m_bFreeUndoAvailable && m_axUndoStack.GetSize() > 0)
			{
				m_pxUndoBtn->SetText("Undo (Free)");
			}
			else if (m_axUndoStack.GetSize() > 0)
			{
				char szUndoLabel[32];
				snprintf(szUndoLabel, sizeof(szUndoLabel), "Undo (%u)", s_uUndoCoinCost);
				m_pxUndoBtn->SetText(szUndoLabel);
			}
			else
			{
				m_pxUndoBtn->SetText("Undo");
			}
		}

		// Update hint button label with cost (tokens take priority over coins)
		if (m_pxHintBtn)
		{
			char szHintLabel[32];
			if (m_xSaveData.HasHintTokens())
				snprintf(szHintLabel, sizeof(szHintLabel), "Free x%u", m_xSaveData.GetFreeHintTokens());
			else
				snprintf(szHintLabel, sizeof(szHintLabel), "(%u)", s_uHintCoinCost);
			m_pxHintBtn->SetText(szHintLabel);
		}

		// Update skip button label with cost
		if (m_pxSkipBtn && m_bSkipOffered)
		{
			char szSkipLabel[32];
			snprintf(szSkipLabel, sizeof(szSkipLabel), "Skip (%u)", s_uSkipCoinCost);
			m_pxSkipBtn->SetText(szSkipLabel);
		}
	}

	// ========================================================================
	// Undo System
	// ========================================================================

	void PushUndoState()
	{
		TilePuzzleUndoState xState;
		xState.uMoveCount = m_uMoveCount;
		xState.iLastMovedShapeIndex = m_iLastMovedShapeIndex;

		xState.axShapePositions.Reserve(static_cast<uint32_t>(m_xCurrentLevel.axShapes.size()));
		xState.abShapeRemoved.Reserve(static_cast<uint32_t>(m_xCurrentLevel.axShapes.size()));
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			xState.axShapePositions.PushBack({ m_xCurrentLevel.axShapes[i].iOriginX, m_xCurrentLevel.axShapes[i].iOriginY });
			xState.abShapeRemoved.PushBack(m_xCurrentLevel.axShapes[i].bRemoved);
		}

		xState.abCatEliminated.Reserve(static_cast<uint32_t>(m_xCurrentLevel.axCats.size()));
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			xState.abCatEliminated.PushBack(m_xCurrentLevel.axCats[i].bEliminated);
		}

		m_axUndoStack.PushBack(std::move(xState));
	}

	void PerformUndo()
	{
		if (m_axUndoStack.GetSize() == 0)
			return;

		// Check if undo costs coins
		if (!m_bFreeUndoAvailable)
		{
			if (!m_xSaveData.SpendCoins(s_uUndoCoinCost))
				return;  // Not enough coins
		}
		else
		{
			m_bFreeUndoAvailable = false;
		}

		const TilePuzzleUndoState& xState = m_axUndoStack.GetBack();

		// Restore move count
		m_uMoveCount = xState.uMoveCount;
		m_iLastMovedShapeIndex = xState.iLastMovedShapeIndex;

		// Restore shape positions and removed state
		for (uint32_t i = 0; i < m_xCurrentLevel.axShapes.size() && i < xState.axShapePositions.GetSize(); ++i)
		{
			m_xCurrentLevel.axShapes[i].iOriginX = xState.axShapePositions.Get(i).first;
			m_xCurrentLevel.axShapes[i].iOriginY = xState.axShapePositions.Get(i).second;

			// If shape was removed but shouldn't be, we need to recreate its visual
			if (m_xCurrentLevel.axShapes[i].bRemoved && !xState.abShapeRemoved.Get(i))
			{
				m_xCurrentLevel.axShapes[i].bRemoved = false;
				RecreateShapeVisual(i);
			}
			m_xCurrentLevel.axShapes[i].bRemoved = xState.abShapeRemoved.Get(i);
		}

		// Restore cat eliminated state
		for (uint32_t i = 0; i < m_xCurrentLevel.axCats.size() && i < xState.abCatEliminated.GetSize(); ++i)
		{
			// If cat was eliminated but shouldn't be, recreate its visual
			if (m_xCurrentLevel.axCats[i].bEliminated && !xState.abCatEliminated.Get(i))
			{
				m_xCurrentLevel.axCats[i].bEliminated = false;
				RecreateCatVisual(i);
			}
			m_xCurrentLevel.axCats[i].bEliminated = xState.abCatEliminated.Get(i);
		}

		m_axUndoStack.PopBack();

		// Update all shape positions visually
		RefreshAllShapeVisuals();

		// Clear selection state
		m_iSelectedShapeIndex = -1;
		m_iPreviousSelectedShapeIndex = -1;
		ClearHint();
	}

	void RecreateShapeVisual(size_t uShapeIndex)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[uShapeIndex];
		if (!xShape.pxDefinition)
			return;

		Zenith_MaterialAsset* pxMaterial = m_xBlockerMaterial.GetDirect();
		if (xShape.pxDefinition->bDraggable && xShape.eColor < TILEPUZZLE_COLOR_COUNT)
		{
			pxMaterial = m_axShapeMaterials[xShape.eColor].GetDirect();
		}

		// Find or create mesh for this shape
		Flux_MeshGeometry* pxShapeMesh = new Flux_MeshGeometry();
		TilePuzzle::GenerateShapeMeshFromDefinition(*xShape.pxDefinition, *pxShapeMesh);
		m_apxGeneratedShapeMeshes.PushBack(pxShapeMesh);

		Zenith_Entity xShapeEntity = TilePuzzle::Resources().m_xShapeCubePrefab.GetDirect()->Instantiate(pxSceneData, "Shape");
		Zenith_TransformComponent& xTransform = xShapeEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(GridToWorld(
			static_cast<float>(xShape.iOriginX),
			static_cast<float>(xShape.iOriginY),
			s_fShapeHeight));
		xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize, s_fShapeHeight * 2.0f, s_fCellSize));

		Zenith_ModelComponent& xModel = xShapeEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*pxShapeMesh, *pxMaterial);

		xShape.xEntityID = xShapeEntity.GetEntityID();
	}

	void RecreateCatVisual(size_t uCatIndex)
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		TilePuzzleCatData& xCat = m_xCurrentLevel.axCats[uCatIndex];

		Zenith_Entity xCatEntity = TilePuzzle::Resources().m_xCatPrefab.GetDirect()->Instantiate(pxSceneData, "Cat");
		Zenith_TransformComponent& xTransform = xCatEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));
		xTransform.SetScale(Zenith_Maths::Vector3(s_fCatRadius * 2.0f));

		Zenith_ModelComponent& xModel = xCatEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxCatGeometry, *m_axCatMaterials[xCat.eColor].GetDirect());

		xCat.uEntityID = xCatEntity.GetEntityID();
	}

	void RefreshAllShapeVisuals()
	{
		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			if (xShape.bRemoved || !xShape.xEntityID.IsValid())
				continue;

			if (!pxSceneData->EntityExists(xShape.xEntityID))
				continue;

			Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
			if (!xEntity.IsValid())
				continue;

			Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(
				static_cast<float>(xShape.iOriginX),
				static_cast<float>(xShape.iOriginY),
				s_fShapeHeight));
		}
	}

	// ========================================================================
	// Hint System
	// ========================================================================

	static void HintSolveTask(void* pData)
	{
		HintSolverData* pxData = static_cast<HintSolverData*>(pData);
		pxData->iResult = TilePuzzle_Solver::SolveLevelWithPath(
			pxData->xLevelState, pxData->axSolution);
		pxData->bComplete.store(true, std::memory_order_release);
	}

	void PerformHint()
	{
		if (m_bHintActive || m_bHintSolving)
			return;

		// Check if player can afford a hint (tokens take priority)
		bool bUseToken = m_xSaveData.HasHintTokens();
		if (!bUseToken && m_xSaveData.uCoins < s_uHintCoinCost)
			return;

		// Build a TilePuzzleLevelData from the current game state for the solver
		m_xHintSolverData.xLevelState = BuildCurrentLevelState();
		m_xHintSolverData.axSolution.clear();
		m_xHintSolverData.iResult = -1;
		m_xHintSolverData.bComplete.store(false, std::memory_order_release);

		// Dispatch solver on background thread
		m_pxHintTask = new Zenith_Task(ZENITH_PROFILE_INDEX__TILEPUZZLE_SOLVER_WITH_PATH, HintSolveTask, &m_xHintSolverData);
		Zenith_TaskSystem::SubmitTask(m_pxHintTask);
		m_bHintSolving = true;
		m_fHintFlashTimer = 0.f;
	}

	void UpdateHintSolver()
	{
		if (!m_bHintSolving)
			return;

		if (!m_xHintSolverData.bComplete.load(std::memory_order_acquire))
			return;

		// Solver complete — clean up task
		m_pxHintTask->WaitUntilComplete();
		delete m_pxHintTask;
		m_pxHintTask = nullptr;
		m_bHintSolving = false;

		if (m_xHintSolverData.iResult < 0 || m_xHintSolverData.axSolution.empty())
			return;

		// Extract which shape should be moved next
		const TilePuzzleSolutionMove& xFirstMove = m_xHintSolverData.axSolution[0];
		int32_t iShapeIndex = static_cast<int32_t>(xFirstMove.uShapeIndex);

		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return;

		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (xShape.bRemoved)
			return;

		// Deduct cost now that we have a valid hint
		bool bUseToken = m_xSaveData.HasHintTokens();
		if (bUseToken)
			m_xSaveData.SpendHintToken();
		else
			m_xSaveData.SpendCoins(s_uHintCoinCost);

		m_bHintActive = true;
		m_iHintShapeIndex = iShapeIndex;
		m_fHintFlashTimer = 0.f;
	}

	void RenderHintSolvingIndicator()
	{
		if (!m_bHintSolving)
			return;

		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		// Animated dots: "." -> ".." -> "..."
		int32_t iDots = (static_cast<int32_t>(m_fHintFlashTimer * 3.0f) % 3) + 1;
		char szText[16];
		snprintf(szText, sizeof(szText), "%.*s", iDots, "...");

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);

		pxCanvas->SubmitText(
			szText,
			Zenith_Maths::Vector2(static_cast<float>(iWinWidth) * 0.5f - 32.0f,
				static_cast<float>(iWinHeight) * 0.5f),
			128.0f,
			Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));
	}

	void ClearHint()
	{
		m_bHintActive = false;
		m_iHintShapeIndex = -1;
		m_fHintFlashTimer = 0.f;
		if (m_bHintSolving)
		{
			m_pxHintTask->WaitUntilComplete();
			delete m_pxHintTask;
			m_pxHintTask = nullptr;
			m_bHintSolving = false;
		}
	}

	TilePuzzleLevelData BuildCurrentLevelState() const
	{
		TilePuzzleLevelData xState;
		xState.uGridWidth = m_xCurrentLevel.uGridWidth;
		xState.uGridHeight = m_xCurrentLevel.uGridHeight;
		xState.aeCells = m_xCurrentLevel.aeCells;
		xState.uMinimumMoves = m_xCurrentLevel.uMinimumMoves;

		// Copy shapes with current positions (null out removed shapes to avoid false collisions)
		xState.axShapes.resize(m_xCurrentLevel.axShapes.size());
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			xState.axShapes[i] = m_xCurrentLevel.axShapes[i];
			if (m_xCurrentLevel.axShapes[i].bRemoved)
				xState.axShapes[i].pxDefinition = nullptr;
		}

		// Only include non-eliminated cats (solver starts with elimMask=0)
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (!m_xCurrentLevel.axCats[i].bEliminated)
				xState.axCats.push_back(m_xCurrentLevel.axCats[i]);
		}

		return xState;
	}

	void RenderHintIndicator()
	{
		if (!m_bHintActive || m_iHintShapeIndex < 0)
			return;

		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		// Flashing effect: visible for half the cycle
		float fFlashCycle = fmodf(m_fHintFlashTimer, 1.0f);
		if (fFlashCycle > 0.5f)
			return;

		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[m_iHintShapeIndex];
		if (xShape.bRemoved)
		{
			ClearHint();
			return;
		}

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
		Zenith_Maths::Matrix4 xViewMat, xProjMat;
		xCam.BuildViewMatrix(xViewMat);
		xCam.BuildProjectionMatrix(xProjMat);
		Zenith_Maths::Matrix4 xVPMat = xProjMat * xViewMat;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		if (iWinWidth <= 0 || iWinHeight <= 0)
			return;

		// Render highlight indicator above the hint shape
		Zenith_Maths::Vector3 xWorldPos = GridToWorld(
			static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY),
			s_fShapeHeight + 1.0f);

		Zenith_Maths::Vector4 xClipPos = xVPMat * Zenith_Maths::Vector4(xWorldPos, 1.0f);
		if (xClipPos.w <= 0.0f)
			return;

		xClipPos.x /= xClipPos.w;
		xClipPos.y /= xClipPos.w;

		float fScreenX = (xClipPos.x + 1.0f) * 0.5f * static_cast<float>(iWinWidth);
		float fScreenY = (xClipPos.y + 1.0f) * 0.5f * static_cast<float>(iWinHeight);

		pxCanvas->SubmitText(
			"!",
			Zenith_Maths::Vector2(fScreenX - 32.0f, fScreenY - 64.0f),
			192.0f,
			Zenith_Maths::Vector4(0.2f, 1.0f, 0.2f, 1.0f));
	}

	// ========================================================================
	// Level Skip
	// ========================================================================

	void PerformSkip()
	{
		if (!m_bSkipOffered)
			return;

		if (!m_xSaveData.SpendCoins(s_uSkipCoinCost))
			return;

		// Mark level as completed with 0 stars (skipped)
		uint32_t uLevelIndex = m_uCurrentLevelNumber - 1;
		if (uLevelIndex < TilePuzzleSaveData::uMAX_LEVELS)
		{
			TilePuzzleLevelRecord& xRecord = m_xSaveData.axLevelRecords[uLevelIndex];
			if (!xRecord.bCompleted)
			{
				xRecord.bCompleted = true;
			}
		}

		// Unlock next level
		if (m_uCurrentLevelNumber >= m_xSaveData.uHighestLevelReached &&
			m_uCurrentLevelNumber < TilePuzzleSaveData::uMAX_LEVELS)
		{
			m_xSaveData.uHighestLevelReached = m_uCurrentLevelNumber + 1;
		}

		m_xSaveData.uCurrentLevel = m_uCurrentLevelNumber;

		// Auto-save
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Advance to next level
		NextLevel();
	}

	// ========================================================================
	// Star Rating Utility
	// ========================================================================

	static const char* GetStarString(uint32_t uStars)
	{
		switch (uStars)
		{
		case 3: return "\xE2\x98\x85\xE2\x98\x85\xE2\x98\x85";       // ★★★
		case 2: return "\xE2\x98\x85\xE2\x98\x85\xE2\x98\x86";       // ★★☆
		case 1: return "\xE2\x98\x85\xE2\x98\x86\xE2\x98\x86";       // ★☆☆
		default: return "\xE2\x98\x86\xE2\x98\x86\xE2\x98\x86";      // ☆☆☆
		}
	}

	// ========================================================================
	// Coordinate Conversion
	// ========================================================================
	bool ScreenToGrid(float fScreenX, float fScreenY, int32_t& iGridX, int32_t& iGridY)
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return false;

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

		Zenith_Maths::Vector3 xNear = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 0.0f));
		Zenith_Maths::Vector3 xFar = xCam.ScreenSpaceToWorldSpace(Zenith_Maths::Vector3(fScreenX, fScreenY, 1.0f));

		Zenith_Maths::Vector3 xDir = xFar - xNear;
		if (fabsf(xDir.y) < 1e-6f)
			return false;

		float fT = (s_fShapeHeight - xNear.y) / xDir.y;
		if (fT < 0.0f)
			return false;

		float fWorldX = xNear.x + fT * xDir.x;
		float fWorldZ = xNear.z + fT * xDir.z;

		float fOffsetX = -static_cast<float>(m_xCurrentLevel.uGridWidth) * 0.5f + 0.5f;
		float fOffsetY = -static_cast<float>(m_xCurrentLevel.uGridHeight) * 0.5f + 0.5f;

		iGridX = static_cast<int32_t>(roundf(fWorldX / s_fCellSize - fOffsetX));
		iGridY = static_cast<int32_t>(roundf(fWorldZ / s_fCellSize - fOffsetY));
		return true;
	}

	Zenith_Maths::Vector3 GridToWorld(float fGridX, float fGridY, float fHeight) const
	{
		float fOffsetX = -static_cast<float>(m_xCurrentLevel.uGridWidth) * 0.5f + 0.5f;
		float fOffsetY = -static_cast<float>(m_xCurrentLevel.uGridHeight) * 0.5f + 0.5f;

		return Zenith_Maths::Vector3(
			(fGridX + fOffsetX) * s_fCellSize,
			fHeight,
			(fGridY + fOffsetY) * s_fCellSize
		);
	}

	void UpdateCameraForGridSize()
	{
		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

		// Half-extent of the grid in world units
		float fHalfExtentX = static_cast<float>(m_xCurrentLevel.uGridWidth) * s_fCellSize * 0.5f;
		float fHalfExtentZ = static_cast<float>(m_xCurrentLevel.uGridHeight) * s_fCellSize * 0.5f;

		float fHalfFOV = xCam.GetFOV() * 0.5f;
		float fTanHalfFOV = tanf(fHalfFOV);
		float fAspect = xCam.GetAspectRatio();

		// Camera looks straight down: vertical viewport maps to Z, horizontal to X
		float fHeightForZ = fHalfExtentZ / fTanHalfFOV;
		float fHeightForX = fHalfExtentX / (fTanHalfFOV * fAspect);

		float fRequiredHeight = fHeightForZ > fHeightForX ? fHeightForZ : fHeightForX;
		fRequiredHeight *= 0.9f;

		m_xCameraBasePosition = Zenith_Maths::Vector3(0.f, fRequiredHeight, 0.f);
		xCam.SetPosition(m_xCameraBasePosition);
		xCam.SetPitch(-1.5);
		xCam.SetYaw(0.0);
	}

	// ========================================================================
	// Screen Shake
	// ========================================================================
	void TriggerScreenShake(float fIntensity, float fDuration)
	{
		m_fScreenShakeIntensity = fIntensity;
		m_fScreenShakeDuration = fDuration;
		m_fScreenShakeTimer = fDuration;
	}

	void UpdateScreenShake(float fDeltaTime)
	{
		if (m_fScreenShakeTimer <= 0.0f)
			return;

		m_fScreenShakeTimer -= fDeltaTime;

		if (m_fScreenShakeTimer <= 0.0f)
		{
			m_fScreenShakeTimer = 0.0f;
			return;
		}

		// Dampened intensity decreases linearly
		float fProgress = m_fScreenShakeTimer / m_fScreenShakeDuration;
		float fCurrentIntensity = m_fScreenShakeIntensity * fProgress;

		// Random offset in XZ plane (camera looks down Y axis)
		float fOffsetX = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * fCurrentIntensity;
		float fOffsetZ = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 2.0f - 1.0f) * fCurrentIntensity;

		if (m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			xCam.SetPosition(m_xCameraBasePosition + Zenith_Maths::Vector3(fOffsetX, 0.0f, fOffsetZ));
		}
	}

	// ========================================================================
	// Camera Zoom Pulse (level complete)
	// ========================================================================
	void TriggerZoomPulse()
	{
		m_fZoomPulseTimer = m_fZoomPulseDuration;
	}

	void UpdateZoomPulse(float fDeltaTime)
	{
		if (m_fZoomPulseTimer <= 0.0f)
			return;

		m_fZoomPulseTimer -= fDeltaTime;
		if (m_fZoomPulseTimer <= 0.0f)
			m_fZoomPulseTimer = 0.0f;

		float fElapsed = m_fZoomPulseDuration - m_fZoomPulseTimer;
		float fZoomOffset = 0.0f;
		float fMaxZoom = m_xCameraBasePosition.y * 0.05f; // 5% of camera height

		if (fElapsed < m_fZoomPulseInDuration)
		{
			// Zoom in phase: ease-out quad
			float fT = fElapsed / m_fZoomPulseInDuration;
			fZoomOffset = -fMaxZoom * Zenith_ApplyEasing(EASING_QUAD_OUT, fT);
		}
		else
		{
			// Ease back phase
			float fEaseBackDuration = m_fZoomPulseDuration - m_fZoomPulseInDuration;
			float fT = (fElapsed - m_fZoomPulseInDuration) / fEaseBackDuration;
			fZoomOffset = -fMaxZoom * (1.0f - Zenith_ApplyEasing(EASING_QUAD_IN_OUT, fT));
		}

		if (m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			Zenith_Maths::Vector3 xPos = m_xCameraBasePosition;
			xPos.y += fZoomOffset;
			xCam.SetPosition(xPos);
		}
	}

	// ========================================================================
	// Shape Wiggle (locked/conditional shapes)
	// ========================================================================
	void TriggerShapeWiggle(int32_t iShapeIndex)
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return;

		if (!m_xPuzzleScene.IsValid())
			return;

		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(m_xPuzzleScene);
		if (!pxSceneData)
			return;

		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (!pxSceneData->EntityExists(xShape.xEntityID))
			return;

		Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
		if (!xEntity.IsValid())
			return;

		// Rapid ±2 degree rotation wiggle via tween: rotate to +2deg, then -2deg, then back to 0
		Zenith_TweenComponent& xTween = xEntity.HasComponent<Zenith_TweenComponent>()
			? xEntity.GetComponent<Zenith_TweenComponent>()
			: xEntity.AddComponent<Zenith_TweenComponent>();

		xTween.CancelByProperty(TWEEN_PROPERTY_ROTATION);

		// Wiggle: 0 -> +2deg -> -2deg -> 0 via chained rotation tweens around Y axis
		Zenith_Maths::Vector3 xZeroRot(0.0f, 0.0f, 0.0f);
		Zenith_Maths::Vector3 xPosRot(0.0f, 2.0f, 0.0f);
		Zenith_Maths::Vector3 xNegRot(0.0f, -2.0f, 0.0f);

		xTween.TweenRotation(xPosRot, 0.075f, EASING_SINE_OUT);
		xTween.TweenRotation(xNegRot, 0.075f, EASING_SINE_IN_OUT);
		xTween.SetDelay(0.075f);
		xTween.TweenRotation(xZeroRot, 0.075f, EASING_SINE_IN_OUT);
		xTween.SetDelay(0.15f);
		xTween.TweenRotation(xPosRot, 0.05f, EASING_SINE_IN_OUT);
		xTween.SetDelay(0.225f);
		xTween.TweenRotation(xZeroRot, 0.05f, EASING_SINE_IN);
		xTween.SetDelay(0.275f);
	}

	// ========================================================================
	// Screen Transitions
	// ========================================================================
	void StartTransition(TilePuzzleGameState eTarget)
	{
		if (m_bTransitionActive)
			return;
		m_bTransitionActive = true;
		m_fTransitionTimer = 0.0f;
		m_bTransitionHalfDone = false;
		m_eTransitionTargetState = eTarget;
	}

	void UpdateTransition(float fDeltaTime)
	{
		if (!m_bTransitionActive)
			return;

		m_fTransitionTimer += fDeltaTime;

		// At halfway point, perform the actual state switch
		if (!m_bTransitionHalfDone && m_fTransitionTimer >= s_fTransitionHalfDuration)
		{
			m_bTransitionHalfDone = true;
			PerformTransitionSwitch();
		}

		// Render fade overlay
		float fAlpha = 0.0f;
		if (m_fTransitionTimer < s_fTransitionHalfDuration)
		{
			// Fade in
			fAlpha = m_fTransitionTimer / s_fTransitionHalfDuration;
		}
		else if (m_fTransitionTimer < s_fTransitionFullDuration)
		{
			// Fade out
			fAlpha = 1.0f - (m_fTransitionTimer - s_fTransitionHalfDuration) / s_fTransitionHalfDuration;
		}
		else
		{
			m_bTransitionActive = false;
			return;
		}

		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (pxCanvas)
		{
			int32_t iW, iH;
			Zenith_Window::GetInstance()->GetSize(iW, iH);
			pxCanvas->SubmitQuad(
				Zenith_Maths::Vector4(0.0f, 0.0f, static_cast<float>(iW), static_cast<float>(iH)),
				Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, fAlpha));
		}
	}

	void PerformTransitionSwitch()
	{
		switch (m_eTransitionTargetState)
		{
		case TILEPUZZLE_STATE_MAIN_MENU:
			ShowScreen(SCREEN_MENU);
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			break;
		case TILEPUZZLE_STATE_LEVEL_SELECT:
			ShowScreen(SCREEN_LEVEL_SELECT);
			UpdateLevelSelectUI();
			m_eState = TILEPUZZLE_STATE_LEVEL_SELECT;
			break;
		case TILEPUZZLE_STATE_CAT_CAFE:
			ShowScreen(SCREEN_CAT_CAFE);
			m_eState = TILEPUZZLE_STATE_CAT_CAFE;
			break;
		case TILEPUZZLE_STATE_SETTINGS:
			ShowScreen(SCREEN_SETTINGS);
			SyncSettingsToggles();
			m_eState = TILEPUZZLE_STATE_SETTINGS;
			break;
		case TILEPUZZLE_STATE_ACHIEVEMENTS:
			HideAllScreens();
			m_eState = TILEPUZZLE_STATE_ACHIEVEMENTS;
			break;
		default:
			break;
		}
	}

	static Zenith_Maths::Vector3 GetBackgroundColorForLevel(uint32_t uLevel)
	{
		if (uLevel <= 10)       return Zenith_Maths::Vector3(0.18f, 0.15f, 0.12f);  // Warm cream/beige
		else if (uLevel <= 25)  return Zenith_Maths::Vector3(0.10f, 0.13f, 0.20f);  // Soft sky blue
		else if (uLevel <= 45)  return Zenith_Maths::Vector3(0.13f, 0.10f, 0.18f);  // Gentle lavender
		else if (uLevel <= 65)  return Zenith_Maths::Vector3(0.08f, 0.14f, 0.15f);  // Muted teal
		else if (uLevel <= 80)  return Zenith_Maths::Vector3(0.08f, 0.06f, 0.16f);  // Deep indigo
		else                    return Zenith_Maths::Vector3(0.10f, 0.04f, 0.12f);  // Rich purple-black
	}

	static Zenith_Maths::Vector4 GetParticleColorForTile(TilePuzzleColor eColor)
	{
		switch (eColor)
		{
		case TILEPUZZLE_COLOR_RED:    return Zenith_Maths::Vector4(1.0f, 0.3f, 0.3f, 1.0f);
		case TILEPUZZLE_COLOR_GREEN:  return Zenith_Maths::Vector4(0.3f, 1.0f, 0.3f, 1.0f);
		case TILEPUZZLE_COLOR_BLUE:   return Zenith_Maths::Vector4(0.3f, 0.5f, 1.0f, 1.0f);
		case TILEPUZZLE_COLOR_YELLOW: return Zenith_Maths::Vector4(1.0f, 1.0f, 0.3f, 1.0f);
		case TILEPUZZLE_COLOR_PURPLE: return Zenith_Maths::Vector4(0.8f, 0.3f, 1.0f, 1.0f);
		default:                      return Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	bool IsShapeLocked(int32_t iShapeIndex) const
	{
		if (iShapeIndex < 0 || iShapeIndex >= static_cast<int32_t>(m_xCurrentLevel.axShapes.size()))
			return false;

		const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[iShapeIndex];
		if (xShape.uUnlockThreshold == 0)
			return false;

		uint32_t uEliminatedCount = 0;
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (m_xCurrentLevel.axCats[i].bEliminated)
				uEliminatedCount++;
		}

		return uEliminatedCount < xShape.uUnlockThreshold;
	}
};
