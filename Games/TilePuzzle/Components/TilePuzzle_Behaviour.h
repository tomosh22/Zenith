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
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Input/Zenith_Input.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Prefab/Zenith_Prefab.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UIImage.h"

#include "TilePuzzle/Components/TilePuzzle_Types.h"
#include "TilePuzzle/Components/TilePuzzle_Rules.h"
#include "TilePuzzle/Components/TilePuzzleLevelData_Serialize.h"
#include "TilePuzzle/Components/TilePuzzle_SaveData.h"
#include "TilePuzzle/Components/TilePuzzle_Solver.h"
#include "SaveData/Zenith_SaveData.h"
#include "Input/Zenith_TouchInput.h"
#include "UI/Zenith_UICanvas.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include "Collections/Zenith_Vector.h"
#include "Flux/Skybox/Flux_Skybox.h"

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
// TilePuzzle Resources - Global access
// Defined in TilePuzzle.cpp, initialized in Project_RegisterScriptBehaviours
// ============================================================================
namespace TilePuzzle
{
	extern Flux_MeshGeometry* g_pxCubeGeometry;
	extern Flux_MeshGeometry* g_pxSphereGeometry;
	extern MaterialHandle g_xFloorMaterial;
	extern MaterialHandle g_xBlockerMaterial;
	extern MaterialHandle g_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
	extern MaterialHandle g_axCatMaterials[TILEPUZZLE_COLOR_COUNT];
	extern Zenith_Prefab* g_pxCellPrefab;
	extern Zenith_Prefab* g_pxShapeCubePrefab;
	extern Zenith_Prefab* g_pxCatPrefab;
	extern Flux_MeshGeometry* g_apxShapeMeshes[TILEPUZZLE_SHAPE_COUNT];
	extern float g_fHighlightEmissiveIntensity;
	void GenerateShapeMeshFromDefinition(const TilePuzzleShapeDefinition& xDef, Flux_MeshGeometry& xGeometryOut);
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
static constexpr uint32_t s_uCoinsPerPinballGate = 25;
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
		, m_eHintDirection(TILEPUZZLE_DIR_NONE)
		, m_fHintFlashTimer(0.f)
		, m_uResetCount(0)
		, m_bSkipOffered(false)
		, m_fVictoryTimer(0.f)
		, m_uVictoryStarsShown(0)
		, m_uVictoryCoinsEarned(0)
		, m_uVictoryStarRating(0)
		, m_bVictoryOverlayActive(false)
		, m_uCatCafePage(0)
		, m_bDailyPuzzleMode(false)
	{
		m_xSaveData.Reset();
	}

	~TilePuzzle_Behaviour() = default;

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

		// Scan for available level files
		m_uAvailableLevelCount = 0;
		for (uint32_t u = 1; u <= TilePuzzleSaveData::uMAX_LEVELS; ++u)
		{
			char szPath[ZENITH_MAX_PATH_LENGTH];
			snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/level_%04u.tlvl", u);
			if (!Zenith_FileAccess::FileExists(szPath))
				break;
			m_uAvailableLevelCount++;
		}

		// Clamp saved level to available range
		if (m_uCurrentLevelNumber > m_uAvailableLevelCount)
			m_uCurrentLevelNumber = (m_uAvailableLevelCount > 0) ? m_uAvailableLevelCount : 1;

		// Cache global resources (lightweight)
		m_pxCubeGeometry = TilePuzzle::g_pxCubeGeometry;
		m_pxSphereGeometry = TilePuzzle::g_pxSphereGeometry;
		m_xFloorMaterial = TilePuzzle::g_xFloorMaterial;
		m_xBlockerMaterial = TilePuzzle::g_xBlockerMaterial;

		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			m_axShapeMaterials[i] = TilePuzzle::g_axShapeMaterials[i];
			m_axCatMaterials[i] = TilePuzzle::g_axCatMaterials[i];
		}

		// Create highlighted versions of shape materials with emissive glow
		auto& xRegistry = Zenith_AssetRegistry::Get();
		for (uint32_t i = 0; i < TILEPUZZLE_COLOR_COUNT; ++i)
		{
			Zenith_MaterialAsset* pxOriginal = m_axShapeMaterials[i].Get();
			Zenith_MaterialAsset* pxHighlighted = xRegistry.Create<Zenith_MaterialAsset>();

			pxHighlighted->SetName(pxOriginal->GetName() + "_Highlighted");
			pxHighlighted->SetBaseColor(pxOriginal->GetBaseColor());
			pxHighlighted->SetDiffuseTextureDirectly(pxOriginal->GetDiffuseTexture());

			Zenith_Maths::Vector4 xBaseColor = pxOriginal->GetBaseColor();
			pxHighlighted->SetEmissiveColor(Zenith_Maths::Vector3(xBaseColor.x, xBaseColor.y, xBaseColor.z));
			pxHighlighted->SetEmissiveIntensity(TilePuzzle::g_fHighlightEmissiveIntensity);

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

			Zenith_UI::Zenith_UIButton* pxNewGameBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NewGameButton");
			if (pxNewGameBtn)
			{
				pxNewGameBtn->SetOnClick(&OnNewGameClicked, this);
			}

			Zenith_UI::Zenith_UIButton* pxPinballBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PinballButton");
			if (pxPinballBtn)
			{
				pxPinballBtn->SetOnClick(&OnPinballClicked, this);
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

			// Settings screen buttons
			Zenith_UI::Zenith_UIButton* pxSettingsBackBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsBackBtn");
			if (pxSettingsBackBtn)
				pxSettingsBackBtn->SetOnClick(&OnSettingsBackClicked, this);

			Zenith_UI::Zenith_UIButton* pxSoundToggle = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsSoundBtn");
			if (pxSoundToggle)
				pxSoundToggle->SetOnClick(&OnToggleSoundClicked, this);

			Zenith_UI::Zenith_UIButton* pxMusicToggle = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsMusicBtn");
			if (pxMusicToggle)
				pxMusicToggle->SetOnClick(&OnToggleMusicClicked, this);

			Zenith_UI::Zenith_UIButton* pxHapticsToggle = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SettingsHapticsBtn");
			if (pxHapticsToggle)
				pxHapticsToggle->SetOnClick(&OnToggleHapticsClicked, this);
		}

		// Regenerate lives on startup
		m_xSaveData.RegenerateLives(GetCurrentTimestamp());

		if (bHasMenu)
		{
			// Start in main menu state
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			SetMenuVisible(true);
			SetHUDVisible(false);
			SetLevelSelectVisible(false);
			SetCatCafeVisible(false);
			SetSettingsVisible(false);
			SetVictoryOverlayVisible(false);
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
			SetMenuVisible(true);
			SetHUDVisible(false);
		}
	}

	void OnUpdate(const float fDeltaTime) ZENITH_FINAL override
	{
		switch (m_eState)
		{
		case TILEPUZZLE_STATE_MAIN_MENU:
			break;

		case TILEPUZZLE_STATE_LEVEL_SELECT:
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
				ReturnToMenu();
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
				if (m_bHintActive)
				{
					m_fHintFlashTimer += fDeltaTime;
				}
				HandleDragInput();
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
			break;

		case TILEPUZZLE_STATE_SETTINGS:
#ifdef ZENITH_TOOLS
			if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_ESCAPE))
			{
				OnSettingsBackClicked(this);
				return;
			}
#endif
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
			StartNewLevel();
		}

		ImGui::SameLine();
		if (ImGui::Button("Reset"))
		{
			ResetLevel();
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
		pxSelf->m_uCurrentLevelNumber = pxSelf->m_xSaveData.uCurrentLevel;
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnLevelSelectClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_uLevelSelectPage = 0;
		pxSelf->StartTransition(TILEPUZZLE_STATE_LEVEL_SELECT);
	}

	static void OnNewGameClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_xSaveData.Reset();
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
		pxSelf->m_uCurrentLevelNumber = 1;
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
	}

	static void OnResetSaveClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->m_xSaveData.Reset();
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &pxSelf->m_xSaveData);
		pxSelf->m_uCurrentLevelNumber = 1;

		// Update menu texts to reflect reset state
		if (pxSelf->m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = pxSelf->m_xParentEntity.GetComponent<Zenith_UIComponent>();

			char szBuffer[64];

			Zenith_UI::Zenith_UIText* pxCoinText = xUI.FindElement<Zenith_UI::Zenith_UIText>("CoinText");
			if (pxCoinText)
			{
				snprintf(szBuffer, sizeof(szBuffer), "Coins: %u", pxSelf->m_xSaveData.uCoins);
				pxCoinText->SetText(szBuffer);
			}

			Zenith_UI::Zenith_UIText* pxLivesText = xUI.FindElement<Zenith_UI::Zenith_UIText>("LivesText");
			if (pxLivesText)
			{
				snprintf(szBuffer, sizeof(szBuffer), "Lives: %u/%u", pxSelf->m_xSaveData.uLives, TilePuzzleSaveData::uMAX_LIVES);
				pxLivesText->SetText(szBuffer);
			}

			Zenith_UI::Zenith_UIText* pxStreakText = xUI.FindElement<Zenith_UI::Zenith_UIText>("DailyStreakText");
			if (pxStreakText)
			{
				pxStreakText->SetText("Streak: 0 days");
			}

			// Hide refill button since lives are full
			Zenith_UI::Zenith_UIButton* pxRefill = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
			if (pxRefill) pxRefill->SetVisible(false);
		}
	}

	static void OnPinballClicked(void* /*pxUserData*/)
	{
		Zenith_SceneManager::LoadSceneByIndex(2, SCENE_LOAD_SINGLE);
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
		Zenith_SceneManager::LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
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
			pxSelf->PerformSkip();
		}
	}

	static void OnMenuClicked(void* pxUserData)
	{
		TilePuzzle_Behaviour* pxSelf = static_cast<TilePuzzle_Behaviour*>(pxUserData);
		pxSelf->ReturnToMenu();
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
		SetMenuVisible(false);
		SetHUDVisible(true);

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
		// Lose a life if exiting puzzle without completing (only in playing state)
		if (m_eState == TILEPUZZLE_STATE_PLAYING)
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

		// Unlock next level
		if (m_uCurrentLevelNumber >= m_xSaveData.uHighestLevelReached &&
			m_uCurrentLevelNumber < TilePuzzleSaveData::uMAX_LEVELS)
		{
			m_xSaveData.uHighestLevelReached = m_uCurrentLevelNumber + 1;
		}

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

		// Auto-save (after all save data modifications)
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);

		// Show victory overlay
		m_bVictoryOverlayActive = true;
		m_fVictoryTimer = 0.f;
		m_uVictoryStarsShown = 0;
		m_uVictoryStarRating = static_cast<uint8_t>(m_uStarsEarned);
		SetVictoryOverlayVisible(true);

		// Camera zoom pulse for emphasis
		TriggerZoomPulse();

		// Burst victory confetti
		Zenith_SceneData* pxParentSceneData = m_xParentEntity.GetSceneData();
		if (m_uVictoryConfettiEmitterID.IsValid() && pxParentSceneData && pxParentSceneData->EntityExists(m_uVictoryConfettiEmitterID))
		{
			Zenith_Entity xEmitter = pxParentSceneData->GetEntity(m_uVictoryConfettiEmitterID);
			xEmitter.GetComponent<Zenith_ParticleEmitterComponent>().Emit(80);
		}

		// Show next level button (unless this is the last level)
		bool bIsLastLevel = (m_uCurrentLevelNumber >= m_uAvailableLevelCount);
		if (!bIsLastLevel && m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Zenith_UI::Zenith_UIButton* pxNextBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("NextLevelBtn");
			if (pxNextBtn)
			{
				pxNextBtn->SetVisible(true);
			}
		}
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
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		Zenith_UI::Zenith_UIText* pxTitle = xUI.FindElement<Zenith_UI::Zenith_UIText>("MenuTitle");
		if (pxTitle) pxTitle->SetVisible(bVisible);

		// Menu buttons layout group
		Zenith_UI::Zenith_UIElement* pxMenuBtnGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("MenuButtonGroup");
		if (pxMenuBtnGroup) pxMenuBtnGroup->SetVisible(bVisible);

		// Background
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("MenuBackground");
		if (pxBg) pxBg->SetVisible(bVisible);

		// Meta-game info texts
		const char* aszInfoTexts[] = { "CoinText", "LivesText", "DailyStreakText", "TotalStarsText" };
		for (const char* szName : aszInfoTexts)
		{
			Zenith_UI::Zenith_UIText* pxText = xUI.FindElement<Zenith_UI::Zenith_UIText>(szName);
			if (pxText) pxText->SetVisible(bVisible);
		}

		// Lives refill button (only when visible and lives < max)
		Zenith_UI::Zenith_UIButton* pxRefill = xUI.FindElement<Zenith_UI::Zenith_UIButton>("RefillLivesButton");
		if (pxRefill) pxRefill->SetVisible(bVisible && m_xSaveData.uLives < TilePuzzleSaveData::uMAX_LIVES);

		// FTUE progressive disclosure: hide buttons until player reaches milestone levels
		uint32_t uProgress = m_xSaveData.uHighestLevelReached;

		Zenith_UI::Zenith_UIButton* pxLevelSelectBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("LevelSelectButton");
		if (pxLevelSelectBtn) pxLevelSelectBtn->SetVisible(bVisible && uProgress >= 5);

		Zenith_UI::Zenith_UIButton* pxCatCafeBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("CatCafeButton");
		if (pxCatCafeBtn) pxCatCafeBtn->SetVisible(bVisible && uProgress >= 3);

		Zenith_UI::Zenith_UIButton* pxDailyBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("DailyPuzzleButton");
		if (pxDailyBtn) pxDailyBtn->SetVisible(bVisible && uProgress >= 10);

		Zenith_UI::Zenith_UIButton* pxPinballBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("PinballButton");
		if (pxPinballBtn) pxPinballBtn->SetVisible(bVisible && uProgress >= 10);

		// Legacy fallback
		Zenith_UI::Zenith_UIButton* pxPlay = xUI.FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay) pxPlay->SetVisible(bVisible);
	}

	void SetHUDVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// HUD layout groups (GDD section 7.4)
		const char* aszHUDGroups[] = { "HUDInfoGroup", "HUDCoinGroup", "HUDButtonGroup" };
		for (const char* szName : aszHUDGroups)
		{
			Zenith_UI::Zenith_UIElement* pxGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
			if (pxGroup) pxGroup->SetVisible(bVisible);
		}

		// Skip button only shows when skip is offered (within HUDButtonGroup)
		Zenith_UI::Zenith_UIButton* pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
		if (pxSkipBtn) pxSkipBtn->SetVisible(bVisible && m_bSkipOffered);
	}

	void SetLevelSelectVisible(bool bVisible)
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		const char* aszElements[] = {
			"LevelSelectTitle", "PageText"
		};
		for (const char* szName : aszElements)
		{
			Zenith_UI::Zenith_UIElement* pxElem = xUI.FindElement<Zenith_UI::Zenith_UIElement>(szName);
			if (pxElem) pxElem->SetVisible(bVisible);
		}

		// Level select navigation layout group
		Zenith_UI::Zenith_UIElement* pxNavGroup = xUI.FindElement<Zenith_UI::Zenith_UIElement>("LevelSelectNavGroup");
		if (pxNavGroup) pxNavGroup->SetVisible(bVisible);

		// Level select background
		Zenith_UI::Zenith_UIElement* pxBg = xUI.FindElement<Zenith_UI::Zenith_UIElement>("LevelSelectBg");
		if (pxBg) pxBg->SetVisible(bVisible);

		for (uint32_t i = 0; i < 20; ++i)
		{
			char szName[32];
			snprintf(szName, sizeof(szName), "LevelBtn_%u", i);
			Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szName);
			if (pxBtn) pxBtn->SetVisible(bVisible);
		}
	}

	void UpdateLevelSelectUI()
	{
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;

		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Update page text
		uint32_t uTotalPages = (m_uAvailableLevelCount + 19) / 20;
		if (uTotalPages == 0) uTotalPages = 1;

		Zenith_UI::Zenith_UIText* pxPageText = xUI.FindElement<Zenith_UI::Zenith_UIText>("PageText");
		if (pxPageText)
		{
			char szPage[32];
			snprintf(szPage, sizeof(szPage), "Page %u / %u", m_uLevelSelectPage + 1, uTotalPages);
			pxPageText->SetText(szPage);
		}

		// Update level buttons
		uint32_t uStartLevel = m_uLevelSelectPage * 20 + 1;
		for (uint32_t i = 0; i < 20; ++i)
		{
			uint32_t uLevel = uStartLevel + i;
			char szBtnName[32];
			snprintf(szBtnName, sizeof(szBtnName), "LevelBtn_%u", i);
			Zenith_UI::Zenith_UIButton* pxBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szBtnName);
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

			if (!bUnlocked)
			{
				// Locked level: show lock symbol
				snprintf(szLabel, sizeof(szLabel), "[%u]", uLevel);
				pxBtn->SetText(szLabel);
				pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.12f, 0.12f, 0.12f, 1.f));
			}
			else if (m_xSaveData.axLevelRecords[uIndex].bCompleted)
			{
				// Completed: show star rating
				uint8_t uStarRating = m_xSaveData.GetStarRating(uLevel);
				const char* szStars = GetStarString(uStarRating);
				snprintf(szLabel, sizeof(szLabel), "%u\n%s", uLevel, szStars);
				pxBtn->SetText(szLabel);

				if (uStarRating >= 3)
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.5f, 0.4f, 0.1f, 1.f));
				else if (bIsPinballGate)
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.4f, 0.2f, 0.5f, 1.f));
				else
					pxBtn->SetNormalColor(Zenith_Maths::Vector4(0.2f, 0.3f, 0.5f, 1.f));
			}
			else
			{
				// Unlocked but not completed
				snprintf(szLabel, sizeof(szLabel), "%u", uLevel);
				pxBtn->SetText(szLabel);

				if (uLevel == m_xSaveData.uHighestLevelReached)
				{
					// Current frontier level: pulsing green highlight
					float fPulse = 0.5f + 0.1f * sinf(static_cast<float>(uLevel) * 0.5f);
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

	// Entity IDs - floor entities indexed by grid position (y * 1000 + x)
	std::unordered_map<uint32_t, Zenith_EntityID> m_axFloorEntityIDs; // #TODO: Replace with engine hash map

	// Cached resources
	Flux_MeshGeometry* m_pxCubeGeometry = nullptr;
	Flux_MeshGeometry* m_pxSphereGeometry = nullptr;
	MaterialHandle m_xFloorMaterial;
	MaterialHandle m_xBlockerMaterial;
	MaterialHandle m_axShapeMaterials[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axShapeMaterialsHighlighted[TILEPUZZLE_COLOR_COUNT];
	MaterialHandle m_axCatMaterials[TILEPUZZLE_COLOR_COUNT];

	// Selection tracking
	int32_t m_iPreviousSelectedShapeIndex = -1;

	// Drag state
	bool m_bDragging = false;
	bool m_bMouseWasDown = false;
	bool m_bPendingLevelComplete = false;
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
	TilePuzzleDirection m_eHintDirection;
	float m_fHintFlashTimer;

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
	uint32_t m_uCatCafePage;

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

	// ========================================================================
	// Tutorial Overlay System
	// ========================================================================

	// Tutorial index mapping:
	// 0 = Level 1 (basic drag)
	// 1 = Level 6 (first domino/multi-cell shape)
	// 2 = Level 11 (first static blocker)
	// 3 = Level 26 (first blocker-cat)
	// 4 = Level 46 (first conditional/locked shape)
	// 5 = (spare)

	int32_t GetTutorialIndexForLevel(uint32_t uLevel) const
	{
		switch (uLevel)
		{
		case 1:  return 0;
		case 6:  return 1;
		case 11: return 2;
		case 26: return 3;
		case 46: return 4;
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
	}

	const char* GetTutorialText() const
	{
		switch (m_uTutorialIndex)
		{
		case 0:
			switch (m_uTutorialStep)
			{
			case 0: return "Tap a colored shape to select it";
			case 1: return "Drag to slide it onto the matching cat";
			case 2: return "Match all cats to complete the level!";
			default: return nullptr;
			}
		case 1: return "This shape covers multiple tiles.\nIt slides as one piece!";
		case 2: return "Dark shapes are blockers.\nThey can't be moved!";
		case 3: return "This cat sits on a blocker.\nGet a matching shape next to it!";
		case 4: return "Locked shapes need cats\neliminated first to unlock!";
		default: return nullptr;
		}
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

		// Render the tutorial overlay using canvas direct rendering
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		if (iWinWidth <= 0 || iWinHeight <= 0)
			return;

		float fAlpha = m_fTutorialFadeProgress * 0.7f;

		// Semi-transparent dark overlay background (bounds: left, top, right, bottom)
		float fW = static_cast<float>(iWinWidth);
		float fH = static_cast<float>(iWinHeight);
		pxCanvas->SubmitQuad(
			Zenith_Maths::Vector4(0.0f, 0.0f, fW, fH),
			Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, fAlpha));

		// Tutorial text box at bottom third of screen
		const char* szText = GetTutorialText();
		if (szText)
		{
			float fTextY = static_cast<float>(iWinHeight) * 0.65f;
			float fTextX = static_cast<float>(iWinWidth) * 0.5f - 270.0f;

			// Text background panel (bounds: left, top, right, bottom)
			pxCanvas->SubmitQuad(
				Zenith_Maths::Vector4(fTextX - 30.0f, fTextY - 20.0f, fTextX + 570.0f, fTextY + 120.0f),
				Zenith_Maths::Vector4(0.1f, 0.1f, 0.25f, m_fTutorialFadeProgress * 0.9f));

			pxCanvas->SubmitText(
				szText,
				Zenith_Maths::Vector2(fTextX, fTextY),
				32.0f,
				Zenith_Maths::Vector4(1.0f, 1.0f, 0.8f, m_fTutorialFadeProgress));

			// "Tap to continue" hint at bottom
			float fHintAlpha = m_fTutorialFadeProgress * (0.5f + 0.5f * sinf(m_fTutorialFadeProgress * 6.0f));
			pxCanvas->SubmitText(
				"Tap to continue",
				Zenith_Maths::Vector2(fTextX + 170.0f, fTextY + 90.0f),
				22.0f,
				Zenith_Maths::Vector4(0.7f, 0.7f, 0.7f, fHintAlpha));
		}

		// Check for tap to advance/dismiss (detect mouse-down transition)
		bool bMouseDown = Zenith_Input::IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
		if (bMouseDown && !m_bTutorialMouseWasDown && m_fTutorialFadeProgress >= 0.5f)
		{
			m_uTutorialStep++;
			m_fTutorialFadeProgress = 0.0f;

			if (m_uTutorialStep >= GetTutorialStepCount())
			{
				DismissTutorial();
			}
		}
		m_bTutorialMouseWasDown = bMouseDown;
	}

	void DismissTutorial()
	{
		m_bTutorialActive = false;
		m_xSaveData.SetTutorialShown(m_uTutorialIndex);
		Zenith_SaveData::Save("autosave", TilePuzzleSaveData::uGAME_SAVE_VERSION,
			TilePuzzle_WriteSaveData, &m_xSaveData);
	}

	// ========================================================================
	// Meta-game systems (Cat Cafe, Victory, Coins, Lives, Daily Puzzle)
	// ========================================================================
#include "TilePuzzle/Components/TilePuzzle_MetaGame.h"

	// ========================================================================
	// Level Loading
	// ========================================================================
	void LoadLevelFromFile()
	{
		// Build path for current level number
		char szPath[ZENITH_MAX_PATH_LENGTH];
		snprintf(szPath, sizeof(szPath), GAME_ASSETS_DIR "Levels/level_%04u.tlvl", m_uCurrentLevelNumber);

		// Load and deserialize
		Zenith_DataStream xStream;
		xStream.ReadFromFile(szPath);
		Zenith_Assert(xStream.IsValid(), "Failed to load level file: %s", szPath);

		// Clean up previously generated per-shape meshes
		for (uint32_t i = 0; i < m_apxGeneratedShapeMeshes.GetSize(); ++i)
			delete m_apxGeneratedShapeMeshes.Get(i);
		m_apxGeneratedShapeMeshes.Clear();

		m_axLoadedShapeDefs.Clear();
		bool bParsed = TilePuzzleLevelSerialize::Read(xStream, m_xCurrentLevel, m_axLoadedShapeDefs);
		Zenith_Assert(bParsed, "Failed to parse level file: %s", szPath);

		// Compute solution if the level file didn't contain one (v1 file)
		if (m_xCurrentLevel.axSolution.empty())
		{
			TilePuzzle_Solver::SolveLevelWithPath(m_xCurrentLevel, m_xCurrentLevel.axSolution);
			if (!m_xCurrentLevel.axSolution.empty())
			{
				// Write updated file back with solution data
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

		// Reset undo stack and hint for new level
		m_axUndoStack.Clear();
		m_bFreeUndoAvailable = true;
		ClearHint();
		m_uStarsEarned = 0;

		UpdateSelectionHighlight();

		// Check if this level has a tutorial to show
		TryShowTutorial();

		// Set background color based on difficulty tier
		Flux_Skybox::s_xOverrideColour = GetBackgroundColorForLevel(m_uCurrentLevelNumber);
	}

	void ResetLevel()
	{
		m_uResetCount++;

		// After enough resets, offer the skip option
		if (m_uResetCount >= s_uResetsBeforeSkipOffer && !m_bSkipOffered)
		{
			m_bSkipOffered = true;
			if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
			{
				Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
				Zenith_UI::Zenith_UIButton* pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
				if (pxSkipBtn)
				{
					pxSkipBtn->SetVisible(true);
				}
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

		// Reset skip state for new level
		m_uResetCount = 0;
		m_bSkipOffered = false;
		if (m_xParentEntity.HasComponent<Zenith_UIComponent>())
		{
			Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();
			Zenith_UI::Zenith_UIButton* pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
			if (pxSkipBtn) pxSkipBtn->SetVisible(false);
		}

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
		if (!pxSceneData || !TilePuzzle::g_pxCellPrefab || !TilePuzzle::g_pxCellPrefab->IsValid())
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
					Zenith_Entity xFloorEntity = TilePuzzle::g_pxCellPrefab->Instantiate(pxSceneData, "Floor");
					if (!xFloorEntity.IsValid())
						continue;

					Zenith_TransformComponent& xTransform = xFloorEntity.GetComponent<Zenith_TransformComponent>();
					xTransform.SetPosition(GridToWorld(static_cast<float>(x), static_cast<float>(y), 0.0f));
					xTransform.SetScale(Zenith_Maths::Vector3(s_fCellSize * 0.95f, s_fFloorHeight, s_fCellSize * 0.95f));

					Zenith_ModelComponent& xModel = xFloorEntity.AddComponent<Zenith_ModelComponent>();
					xModel.AddMeshEntry(*m_pxCubeGeometry, *m_xFloorMaterial.Get());

					uint32_t uKey = y * 1000 + x;
					m_axFloorEntityIDs[uKey] = xFloorEntity.GetEntityID();
				}
			}
		}

		// Create shape visuals (one entity per shape with merged mesh)
		for (auto& xShape : m_xCurrentLevel.axShapes)
		{
			Zenith_MaterialAsset* pxMaterial = m_xBlockerMaterial.Get();
			if (xShape.pxDefinition->bDraggable && xShape.eColor < TILEPUZZLE_COLOR_COUNT)
			{
				pxMaterial = m_axShapeMaterials[xShape.eColor].Get();
			}

			// Generate mesh from actual loaded cell offsets (which may be rotated)
			Flux_MeshGeometry* pxShapeMesh = new Flux_MeshGeometry();
			TilePuzzle::GenerateShapeMeshFromDefinition(*xShape.pxDefinition, *pxShapeMesh);
			m_apxGeneratedShapeMeshes.PushBack(pxShapeMesh);

			Zenith_Entity xShapeEntity = TilePuzzle::g_pxShapeCubePrefab->Instantiate(pxSceneData, "Shape");
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
			Zenith_Entity xCatEntity = TilePuzzle::g_pxCatPrefab->Instantiate(pxSceneData, "Cat");
			Zenith_TransformComponent& xTransform = xCatEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));
			xTransform.SetScale(Zenith_Maths::Vector3(s_fCatRadius * 2.0f));

			Zenith_ModelComponent& xModel = xCatEntity.AddComponent<Zenith_ModelComponent>();
			xModel.AddMeshEntry(*m_pxSphereGeometry, *m_axCatMaterials[xCat.eColor].Get());

			xCat.uEntityID = xCatEntity.GetEntityID();
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
		RenderUsesIndicators();
		RenderHintIndicator();

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

	void RenderUsesIndicators()
	{
		Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
		if (!pxCanvas)
			return;

		if (!m_xParentEntity.HasComponent<Zenith_CameraComponent>())
			return;

		Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
		Zenith_Maths::Matrix4 xViewMat, xProjMat;
		xCam.BuildViewMatrix(xViewMat);
		xCam.BuildProjectionMatrix(xProjMat);
		Zenith_Maths::Matrix4 xVPMat = xProjMat * xViewMat;

		int32_t iWinWidth, iWinHeight;
		Zenith_Window::GetInstance()->GetSize(iWinWidth, iWinHeight);
		if (iWinWidth <= 0 || iWinHeight <= 0)
			return;

		// Count remaining cats per color
		uint32_t auRemainingCatsPerColor[TILEPUZZLE_COLOR_COUNT] = {};
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
			if (!m_xCurrentLevel.axCats[i].bEliminated &&
				m_xCurrentLevel.axCats[i].eColor < TILEPUZZLE_COLOR_COUNT)
			{
				auRemainingCatsPerColor[m_xCurrentLevel.axCats[i].eColor]++;
			}
		}

		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
				continue;
			if (xShape.bRemoved)
				continue;
			if (xShape.eColor >= TILEPUZZLE_COLOR_COUNT)
				continue;

			uint32_t uRemaining = auRemainingCatsPerColor[xShape.eColor];
			if (uRemaining == 0)
				continue;

			Zenith_Maths::Vector3 xWorldPos = GridToWorld(
				static_cast<float>(xShape.iOriginX), static_cast<float>(xShape.iOriginY),
				s_fShapeHeight + 0.5f);

			Zenith_Maths::Vector4 xClipPos = xVPMat * Zenith_Maths::Vector4(xWorldPos, 1.0f);
			if (xClipPos.w <= 0.0f)
				continue;

			xClipPos.x /= xClipPos.w;
			xClipPos.y /= xClipPos.w;

			float fScreenX = (xClipPos.x + 1.0f) * 0.5f * static_cast<float>(iWinWidth);
			float fScreenY = (xClipPos.y + 1.0f) * 0.5f * static_cast<float>(iWinHeight);

			char szText[8];
			snprintf(szText, sizeof(szText), "%u", uRemaining);

			pxCanvas->SubmitText(
				szText,
				Zenith_Maths::Vector2(fScreenX - 32.0f, fScreenY - 64.0f),
				128.0f,
				Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
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
					Zenith_MaterialAsset* pxNormalMaterial = m_axShapeMaterials[xPrevShape.eColor].Get();
					if (pxSceneData->EntityExists(xPrevShape.xEntityID))
					{
						Zenith_Entity xEntity = pxSceneData->GetEntity(xPrevShape.xEntityID);
						if (xEntity.IsValid() && xEntity.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xEntity.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshEntries() > 0)
							{
								xModel.GetMaterialHandleAtIndex(0).Set(pxNormalMaterial);
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
					Zenith_MaterialAsset* pxHighlightMaterial = m_axShapeMaterialsHighlighted[xShape.eColor].Get();
					if (pxSceneData->EntityExists(xShape.xEntityID))
					{
						Zenith_Entity xEntity = pxSceneData->GetEntity(xShape.xEntityID);
						if (xEntity.IsValid() && xEntity.HasComponent<Zenith_ModelComponent>())
						{
							Zenith_ModelComponent& xModel = xEntity.GetComponent<Zenith_ModelComponent>();
							if (xModel.GetNumMeshEntries() > 0)
							{
								xModel.GetMaterialHandleAtIndex(0).Set(pxHighlightMaterial);
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
		if (!m_xParentEntity.HasComponent<Zenith_UIComponent>())
			return;
		Zenith_UIComponent& xUI = m_xParentEntity.GetComponent<Zenith_UIComponent>();

		// Update level text
		char szBuffer[128];
		Zenith_UI::Zenith_UIText* pxLevel = xUI.FindElement<Zenith_UI::Zenith_UIText>("LevelText");
		if (pxLevel)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Level %u", m_uCurrentLevelNumber);
			pxLevel->SetText(szBuffer);
		}

		// Update move counter
		Zenith_UI::Zenith_UIText* pxMoves = xUI.FindElement<Zenith_UI::Zenith_UIText>("MovesText");
		if (pxMoves)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Moves: %u / Par: %u", m_uMoveCount, m_xCurrentLevel.uMinimumMoves);
			pxMoves->SetText(szBuffer);
		}

		// Update cats remaining
		size_t uRemaining = CountRemainingCats();
		size_t uTotal = m_xCurrentLevel.axCats.size();
		Zenith_UI::Zenith_UIText* pxCats = xUI.FindElement<Zenith_UI::Zenith_UIText>("CatsText");
		if (pxCats)
		{
			snprintf(szBuffer, sizeof(szBuffer), "Cats: %zu / %zu", uTotal - uRemaining, uTotal);
			pxCats->SetText(szBuffer);
		}

		// Update coin display
		Zenith_UI::Zenith_UIText* pxCoins = xUI.FindElement<Zenith_UI::Zenith_UIText>("HUDCoinsText");
		if (pxCoins)
		{
			snprintf(szBuffer, sizeof(szBuffer), "%u", m_xSaveData.uCoins);
			pxCoins->SetText(szBuffer);
		}

		// Update undo button label with cost info
		Zenith_UI::Zenith_UIButton* pxUndoBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("UndoBtn");
		if (pxUndoBtn)
		{
			if (m_bFreeUndoAvailable && m_axUndoStack.GetSize() > 0)
			{
				pxUndoBtn->SetText("Undo (Free)");
			}
			else if (m_axUndoStack.GetSize() > 0)
			{
				char szUndoLabel[32];
				snprintf(szUndoLabel, sizeof(szUndoLabel), "Undo (%u)", s_uUndoCoinCost);
				pxUndoBtn->SetText(szUndoLabel);
			}
			else
			{
				pxUndoBtn->SetText("Undo");
			}
		}

		// Update hint button label with cost
		Zenith_UI::Zenith_UIButton* pxHintBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("HintBtn");
		if (pxHintBtn)
		{
			char szHintLabel[32];
			snprintf(szHintLabel, sizeof(szHintLabel), "Hint (%u)", s_uHintCoinCost);
			pxHintBtn->SetText(szHintLabel);
		}

		// Update skip button label with cost
		Zenith_UI::Zenith_UIButton* pxSkipBtn = xUI.FindElement<Zenith_UI::Zenith_UIButton>("SkipBtn");
		if (pxSkipBtn && m_bSkipOffered)
		{
			char szSkipLabel[32];
			snprintf(szSkipLabel, sizeof(szSkipLabel), "Skip (%u)", s_uSkipCoinCost);
			pxSkipBtn->SetText(szSkipLabel);
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

		Zenith_MaterialAsset* pxMaterial = m_xBlockerMaterial.Get();
		if (xShape.pxDefinition->bDraggable && xShape.eColor < TILEPUZZLE_COLOR_COUNT)
		{
			pxMaterial = m_axShapeMaterials[xShape.eColor].Get();
		}

		// Find or create mesh for this shape
		Flux_MeshGeometry* pxShapeMesh = new Flux_MeshGeometry();
		TilePuzzle::GenerateShapeMeshFromDefinition(*xShape.pxDefinition, *pxShapeMesh);
		m_apxGeneratedShapeMeshes.PushBack(pxShapeMesh);

		Zenith_Entity xShapeEntity = TilePuzzle::g_pxShapeCubePrefab->Instantiate(pxSceneData, "Shape");
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

		Zenith_Entity xCatEntity = TilePuzzle::g_pxCatPrefab->Instantiate(pxSceneData, "Cat");
		Zenith_TransformComponent& xTransform = xCatEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(GridToWorld(static_cast<float>(xCat.iGridX), static_cast<float>(xCat.iGridY), s_fCatHeight));
		xTransform.SetScale(Zenith_Maths::Vector3(s_fCatRadius * 2.0f));

		Zenith_ModelComponent& xModel = xCatEntity.AddComponent<Zenith_ModelComponent>();
		xModel.AddMeshEntry(*m_pxSphereGeometry, *m_axCatMaterials[xCat.eColor].Get());

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

	void PerformHint()
	{
		if (m_bHintActive)
			return;  // Hint already showing

		// Check coin cost (don't deduct yet - wait until we find a valid hint)
		if (m_xSaveData.uCoins < s_uHintCoinCost)
			return;

		// Build a TilePuzzleLevelData from the current game state for the solver
		TilePuzzleLevelData xCurrentState = BuildCurrentLevelState();

		// Run solver from current state
		int32_t iSolution = TilePuzzle_Solver::SolveLevel(xCurrentState, 500000);
		if (iSolution < 0)
			return;  // Unsolvable from current state (shouldn't happen)

		// Find the best first move by trying each shape in each direction
		int32_t iBestShapeIndex = -1;
		TilePuzzleDirection eBestDirection = TILEPUZZLE_DIR_NONE;
		int32_t iBestResult = iSolution;

		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			const TilePuzzleShapeInstance& xShape = m_xCurrentLevel.axShapes[i];
			if (!xShape.pxDefinition || !xShape.pxDefinition->bDraggable)
				continue;
			if (xShape.bRemoved)
				continue;

			TilePuzzleDirection aeDirections[] = {
				TILEPUZZLE_DIR_UP, TILEPUZZLE_DIR_DOWN,
				TILEPUZZLE_DIR_LEFT, TILEPUZZLE_DIR_RIGHT
			};

			for (TilePuzzleDirection eDir : aeDirections)
			{
				int32_t iDeltaX, iDeltaY;
				TilePuzzleDirections::GetDelta(eDir, iDeltaX, iDeltaY);

				if (!CanMoveShape(static_cast<int32_t>(i), iDeltaX, iDeltaY))
					continue;

				// Build state after this move
				TilePuzzleLevelData xAfterMove = xCurrentState;
				for (size_t s = 0; s < xAfterMove.axShapes.size(); ++s)
				{
					if (s == i)
					{
						xAfterMove.axShapes[s].iOriginX += iDeltaX;
						xAfterMove.axShapes[s].iOriginY += iDeltaY;
					}
				}

				int32_t iResult = TilePuzzle_Solver::SolveLevel(xAfterMove, 500000);
				if (iResult >= 0 && iResult < iBestResult)
				{
					iBestResult = iResult;
					iBestShapeIndex = static_cast<int32_t>(i);
					eBestDirection = eDir;
				}
			}
		}

		if (iBestShapeIndex < 0)
			return;  // No improving move found

		// Deduct coins and show hint
		m_xSaveData.SpendCoins(s_uHintCoinCost);
		m_bHintActive = true;
		m_iHintShapeIndex = iBestShapeIndex;
		m_eHintDirection = eBestDirection;
		m_fHintFlashTimer = 0.f;
	}

	void ClearHint()
	{
		m_bHintActive = false;
		m_iHintShapeIndex = -1;
		m_eHintDirection = TILEPUZZLE_DIR_NONE;
		m_fHintFlashTimer = 0.f;
	}

	TilePuzzleLevelData BuildCurrentLevelState() const
	{
		TilePuzzleLevelData xState;
		xState.uGridWidth = m_xCurrentLevel.uGridWidth;
		xState.uGridHeight = m_xCurrentLevel.uGridHeight;
		xState.aeCells = m_xCurrentLevel.aeCells;
		xState.uMinimumMoves = m_xCurrentLevel.uMinimumMoves;

		// Copy shapes with current positions
		xState.axShapes.resize(m_xCurrentLevel.axShapes.size());
		for (size_t i = 0; i < m_xCurrentLevel.axShapes.size(); ++i)
		{
			xState.axShapes[i] = m_xCurrentLevel.axShapes[i];
		}

		// Copy cats with current elimination state (only non-eliminated cats matter)
		for (size_t i = 0; i < m_xCurrentLevel.axCats.size(); ++i)
		{
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

		// Render direction arrow above the hint shape
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

		const char* szArrow = "?";
		switch (m_eHintDirection)
		{
		case TILEPUZZLE_DIR_UP:    szArrow = "v"; break;   // Grid up = visual down
		case TILEPUZZLE_DIR_DOWN:  szArrow = "^"; break;   // Grid down = visual up
		case TILEPUZZLE_DIR_LEFT:  szArrow = "<"; break;
		case TILEPUZZLE_DIR_RIGHT: szArrow = ">"; break;
		default: break;
		}

		pxCanvas->SubmitText(
			szArrow,
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
		case 3: return "***";
		case 2: return "**";
		case 1: return "*";
		default: return "";
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
		float fOffsetX = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * fCurrentIntensity;
		float fOffsetZ = (static_cast<float>(rand()) / RAND_MAX * 2.0f - 1.0f) * fCurrentIntensity;

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
			SetLevelSelectVisible(false);
			SetCatCafeVisible(false);
			SetSettingsVisible(false);
			SetMenuVisible(true);
			m_eState = TILEPUZZLE_STATE_MAIN_MENU;
			break;
		case TILEPUZZLE_STATE_LEVEL_SELECT:
			SetMenuVisible(false);
			SetLevelSelectVisible(true);
			UpdateLevelSelectUI();
			m_eState = TILEPUZZLE_STATE_LEVEL_SELECT;
			break;
		case TILEPUZZLE_STATE_CAT_CAFE:
			SetMenuVisible(false);
			SetCatCafeVisible(true);
			UpdateCatCafeUI();
			m_eState = TILEPUZZLE_STATE_CAT_CAFE;
			break;
		case TILEPUZZLE_STATE_SETTINGS:
			SetMenuVisible(false);
			SetSettingsVisible(true);
			UpdateSettingsUI();
			m_eState = TILEPUZZLE_STATE_SETTINGS;
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
