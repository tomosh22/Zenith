#include "Zenith.h"

#include "Source/PublicInterfaces.h"
#include "Source/DevilsPlayground_Tags.h"
#include "Source/DPFogPass.h"
#include "Source/DP_LevelData.h"
#include "Source/DPMaterials.h"
#include "Source/DP_Tuning.h"
#include "Source/DP_Archetypes.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "Core/Zenith_GraphicsOptions.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Physics/Zenith_Physics_Fwd.h"
#include "UI/Zenith_UIRect.h"

// Behaviour headers — including each here forces the
// ZENITH_BEHAVIOUR_TYPE_NAME static-init registration to run, so the
// scene-load path can resolve `game:Scripts/<TypeName>.zscript` to a valid
// factory. Combat does the equivalent.
#include "Components/DPVillager_Behaviour.h"
#include "Components/DPPlayerController_Behaviour.h"
#include "Components/DPOrbitCamera_Behaviour.h"
#include "Components/DPItemBase_Behaviour.h"
#include "Components/DPItemSpawn_Behaviour.h"
#include "Components/DPItemManager_Behaviour.h"
#include "Components/DPInteractable_Behaviour.h"
#include "Components/DPDoor_Behaviour.h"
#include "Components/DPDoubleDoor_Behaviour.h"
#include "Components/DPChest_Behaviour.h"
#include "Components/DPForge_Behaviour.h"
#include "Components/DPPentagram_Behaviour.h"
#include "Components/Priest_Behaviour.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "Components/DummyNoiseMachine_Behaviour.h"
#include "Components/DPHUDController_Behaviour.h"
#include "Components/DPMainMenuController_Behaviour.h"
#include "Components/DPPauseMenuController_Behaviour.h"
#include "Components/DPFogPass_Behaviour.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_EditorAutomation.h"
#include "Editor/Zenith_Editor.h"
#endif

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

// ============================================================================
// Forward decls — Project_LoadInitialScene is referenced by the editor
// automation chain ahead of its definition (matches Combat).
// ============================================================================
void Project_LoadInitialScene();

// ============================================================================
// Resource initialization (CPU-only state that must exist before any scene
// loads — material handles, prefab registry, etc). Wave 2 streams (B1 assets,
// B6 fog) extend this; W0 is a no-op stub.
// ============================================================================
namespace DevilsPlayground
{
	void InitializeResources()
	{
		// Load Config/Tuning.json into the in-process cache before any other
		// resource init so subsequent systems (materials, fog, behaviours)
		// can read tuning values during their own bring-up.
		DP_Tuning::Initialize();

		// MVP-0.2.1+2: Config/Archetypes.json into the archetype cache. Loaded
		// right after Tuning so DPVillager_Behaviour::OnAwake can consult both.
		// Idempotent.
		DP_Archetypes::Initialize();

		// Author Zenith_MaterialAssets from the UE parameter dumps under
		// Assets/Materials/*.json. Idempotent — safe across Editor Stop/Play.
		DPMaterials::Initialize();

		// B6 fog: register PFX_Witch particle config for runtime instantiation.
		DPFogPass::RegisterParticleConfigs();

#ifdef ZENITH_INPUT_SIMULATOR
		// Tell the automated-test harness how to wipe DP-specific persistent
		// globals between batched tests. The harness force-loads scene 0
		// before firing this hook, so entity-managed side-tables (DP_Items,
		// DP_Fog) are already cleared via OnDestroy by the time we run; we
		// only need to reset state that has no entity owner.
		Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
		{
			DP_Player::ResetForTest();
			DP_Win::Reset();
			DP_Fog::ClearAllFogHoles();
			DP_AI::ResetLevelNavMesh();
		});
#endif
	}

	void CleanupResources()
	{
		// Reset cross-session state so a re-launched game doesn't inherit win
		// flag / fog holes / held items from a previous run.
		DP_Win::Reset();
		DP_Fog::ClearAllFogHoles();
		DP_Player::SetPossessedVillager(INVALID_ENTITY_ID);
		DP_AI::ResetLevelNavMesh();
		DPMaterials::Shutdown();

		// B6 fog: drop the PFX_Witch config so a relaunch doesn't double-register.
		DPFogPass::UnregisterParticleConfigs();

		// Drop the archetype cache before tuning -- archetypes don't depend on
		// tuning, but keep teardown order paired so future cross-deps are
		// surfaced by the Editor Stop/Play smoke runs. Idempotent.
		DP_Archetypes::Shutdown();

		// Drop the tuning cache last so materials/fog teardown above can still
		// query tuning values if they ever start to. Idempotent.
		DP_Tuning::Shutdown();
	}
}

// ============================================================================
// Project entry points (mirrors Combat's 9-hook layout: Combat.cpp:672–820).
// ============================================================================

const char* Project_GetName()
{
	return "DevilsPlayground";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

const char* Project_GetGameAssetsDir() { return GAME_ASSETS_DIR; }

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
	// W0 stub. B6 may set fog technique here; the game disables engine fog
	// entirely via Flux_Fog::SetExternallyOverridden(true) inside DPFogPass.
}

void Project_RegisterScriptBehaviours()
{
	// Behaviour registration is automatic via the ZENITH_BEHAVIOUR_TYPE_NAME
	// macro's static initializer (runs at program startup before main()).
	// This hook remains as the per-game lifecycle entry point for early
	// CPU-only resource initialization that must run in both TOOLS and
	// non-TOOLS builds.
	DevilsPlayground::InitializeResources();

	// Register the game's post-fog render-graph pass and disable the engine
	// fog system. Idempotent — safe across Editor Stop/Play.
	DPFogPass::Init();
}

void Project_Shutdown()
{
	// Order matters: tear down the render hook BEFORE engine resources go
	// away (Flux_Fog::SetExternallyOverridden is guarded against the
	// render graph already being torn down).
	DPFogPass::Shutdown();
	DevilsPlayground::CleanupResources();
}

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// All DevilsPlayground resources are initialised in
	// Project_RegisterScriptBehaviours via DevilsPlayground::InitializeResources().
}

namespace
{
	// ------------------------------------------------------------------
	// UE asset path  -> Zenith .zmodel path translator.
	//
	//   UE   /Game/DevilsPlayground/Assets/Blockout/Chest/ChestMain.ChestMain
	//   =>   GAME_ASSETS_DIR "Meshes/DevilsPlayground_Assets_Blockout_Chest_ChestMain.zmodel"
	//
	// /Engine/* assets aren't part of the game export pipeline; we substitute
	// the cube-prototype mesh so the entity still renders.  Also a few UE
	// blueprints reference the engine plane proxy ("/Engine/BasicShapes/Plane.Plane")
	// for chests; the chest mesh actually exported is ChestMain, so we
	// hard-redirect to that case.
	//
	// Returns a stable C-string interned in process-static storage so the
	// AddStep_LoadModel(string-pointer) contract survives the lazy AddStep
	// drain.  Returns nullptr for empty / NULL inputs.
	// ------------------------------------------------------------------
	const char* MakeMeshPath(const char* szUePath)
	{
		// Each unique input path yields a fresh heap-allocated buffer that
		// outlives the program.  Linear search of a small table is fine —
		// we expect ~16 unique meshes total.
		static std::vector<const char*> s_axKeys;
		static std::vector<const char*> s_axValues;

		if (!szUePath || szUePath[0] == '\0')
		{
			return nullptr;
		}

		for (size_t i = 0; i < s_axKeys.size(); ++i)
		{
			if (std::strcmp(s_axKeys[i], szUePath) == 0)
			{
				return s_axValues[i];
			}
		}

		// Build the file stem (strip /Game/, drop the trailing .X duplicate, /->_).
		std::string strStem;
		if (std::strcmp(szUePath, "/Engine/BasicShapes/Plane.Plane") == 0)
		{
			// BP_ChestInteractable's root component points to the engine
			// Plane primitive — the actual chest visual lives in a child
			// component the dp_export tool didn't drill into. Re-route to
			// the exported Chest_ChestMain mesh so chests render with their
			// real geometry instead of a flat plane fallback.
			strStem = "DevilsPlayground_Assets_Blockout_Chest_ChestMain";
		}
		else if (std::strncmp(szUePath, "/Engine/", 8) == 0)
		{
			// Other engine asset — fall back to a known-exported proxy mesh.
			strStem = "LevelPrototyping_Meshes_SM_Cube";
		}
		else
		{
			const char* szRest = szUePath;
			if (std::strncmp(szRest, "/Game/", 6) == 0)
			{
				szRest += 6;
			}
			strStem = szRest;
			size_t uDot = strStem.rfind('.');
			if (uDot != std::string::npos)
			{
				strStem.resize(uDot);
			}
			for (char& c : strStem)
			{
				if (c == '/') c = '_';
			}
		}

		std::string strFull = std::string(GAME_ASSETS_DIR) + "Meshes/" + strStem + ZENITH_MODEL_EXT;

		// Heap-allocate and intern. The leak is intentional and tiny — these
		// strings live for the full process lifetime.
		size_t uKeyLen = std::strlen(szUePath);
		char* pszKey = new char[uKeyLen + 1];
		std::memcpy(pszKey, szUePath, uKeyLen + 1);
		char* pszValue = new char[strFull.size() + 1];
		std::memcpy(pszValue, strFull.c_str(), strFull.size() + 1);
		s_axKeys.push_back(pszKey);
		s_axValues.push_back(pszValue);
		return pszValue;
	}

	// Heap-allocate a stable copy of an interpolated entity name. The
	// editor-automation queue stores const char* pointers (per
	// Zenith_EditorAction's contract — see the header) — passing a stack
	// buffer here would dangle once the calling scope exits, because the
	// queue drains lazily over later frames. The leak is intentional:
	// ~one small allocation per entity, freed at process exit.
	const char* InternEntityName(const char* szPrefix, uint32_t uIndex)
	{
		char* pszName = new char[64];
		std::snprintf(pszName, 64, "%s_%u", szPrefix, uIndex);
		return pszName;
	}

	// =====================================================================
	// Door-at-gap placement.
	//
	// The UE-imported `kDoor` placements are all stacked at world origin
	// (a known authoring quirk in the source map), so we can't use them
	// directly. Instead we walk `kStaticDeco` for actual building wall
	// sections, find pairs that are co-linear with a 1-3 m gap between
	// them — those are doorways — and assign each door to one of them.
	//
	// Wall data lives in two mesh families:
	//   - SM_Cube* (perimeter walls): scale = (thickness, height, length),
	//     mesh-local length is along Z. We skip these for door placement
	//     because the perimeter is a closed map boundary, not a building.
	//   - WallSection / CornerWallSection: mesh-local length is along X,
	//     scale.x is the length multiplier. These are the building walls.
	// =====================================================================
	struct DPWallSegment
	{
		float fCenterX = 0.0f;
		float fCenterZ = 0.0f;
		float fAxisX   = 1.0f;   // unit vector along length, world space
		float fAxisZ   = 0.0f;
		float fHalfLength = 0.0f;
	};

	struct DPDoorwayCandidate
	{
		float fX     = 0.0f;
		float fZ     = 0.0f;
		float fYaw   = 0.0f;     // door yaw aligning with the wall axis
		float fGap   = 0.0f;     // for ranking: 1.0 m is ideal, 2.5 m is OK
	};

	// Build a 2D segment for a wall placement. Returns false if the entry
	// is not a wall (too short, or a perimeter cube we want to skip).
	bool BuildWallSegment(const DP_LevelData::Placement& xP, DPWallSegment& xOut, bool bSkipPerimeter = true)
	{
		const bool bIsPerimeterCube = (xP.mesh != nullptr)
			&& std::strstr(xP.mesh, "/LevelPrototyping/Meshes/SM_Cube") != nullptr;
		if (bSkipPerimeter && bIsPerimeterCube) return false;

		// Decide which local axis is length:
		//   - SM_Cube perimeter walls: scale = (thickness, height, length) → local Z
		//   - WallSection family: scale = (length, 1, 1) → local X
		//
		// Mesh-extent multiplier: the BuildingAssetKit wall meshes are 2 m
		// wide naturally (bounds (-1..1) in their length axis), so the
		// runtime collider — which now correctly tracks mesh bounds — runs
		// twice as long as the raw scale.x value suggests. The doorway
		// detector must use the same scaled length, or candidate endpoints
		// land halfway across the wall and "gaps" snap to door positions
		// that are still inside the wall's footprint.
		float fLength = 0.0f;
		bool  bLengthIsLocalZ = false;
		if (bIsPerimeterCube)
		{
			fLength = xP.sz;             // SM_Cube mesh is 1×1×1, length = scale.z
			bLengthIsLocalZ = true;
		}
		else
		{
			fLength = xP.sx * 2.0f;      // BuildingAssetKit mesh = 2 m along X
		}
		if (fLength < 0.8f) return false;       // too short to be a wall
		if (xP.sy < 0.5f) return false;          // floor / very flat thing

		// Rotate (1,0,0) or (0,0,1) by yaw around Y. Standard right-handed
		// rotation: x' = cos(yaw)*x + sin(yaw)*z; z' = -sin(yaw)*x + cos(yaw)*z.
		const float fCos = std::cos(xP.yaw);
		const float fSin = std::sin(xP.yaw);
		const float fLocX = bLengthIsLocalZ ? 0.0f : 1.0f;
		const float fLocZ = bLengthIsLocalZ ? 1.0f : 0.0f;
		xOut.fAxisX = fCos * fLocX + fSin * fLocZ;
		xOut.fAxisZ = -fSin * fLocX + fCos * fLocZ;
		xOut.fCenterX = xP.x;
		xOut.fCenterZ = xP.z;
		xOut.fHalfLength = fLength * 0.5f;
		return true;
	}

	// Check whether the segment from world (x1,z1) endpoint to (x2,z2)
	// endpoint already crosses some other wall — signals that this gap is
	// internal (between rooms) rather than an exterior doorway.
	// Currently unused but kept for future filtering extensions.
	bool DPSegmentIntersectsAnyWall(float, float, float, float,
	                              const Zenith_Vector<DPWallSegment>&) { return false; }

	// Compute the candidate doorways from the wall set. The UE-imported
	// buildings use L-shaped wall sections — two perpendicular walls
	// meeting at a corner — so a "doorway" is most often a small gap
	// between two wall endpoints that don't quite close the corner.
	// We test all pairs of endpoints across different walls and accept
	// pairs separated by 0.8-3.0 m. The doorway centre is the midpoint;
	// the door's yaw aligns with the line through both endpoints so the
	// door's flat face plugs the gap.
	void FindDoorwayCandidates(const Zenith_Vector<DPWallSegment>& axWalls,
	                           Zenith_Vector<DPDoorwayCandidate>& xOut)
	{
		const uint32_t uN = axWalls.GetSize();
		for (uint32_t i = 0; i < uN; ++i)
		{
			const DPWallSegment& a = axWalls.Get(i);
			// Two endpoints of wall a along its axis.
			const float aE0x = a.fCenterX - a.fAxisX * a.fHalfLength;
			const float aE0z = a.fCenterZ - a.fAxisZ * a.fHalfLength;
			const float aE1x = a.fCenterX + a.fAxisX * a.fHalfLength;
			const float aE1z = a.fCenterZ + a.fAxisZ * a.fHalfLength;
			const float aE[2][2] = { {aE0x, aE0z}, {aE1x, aE1z} };

			for (uint32_t j = i + 1; j < uN; ++j)
			{
				const DPWallSegment& b = axWalls.Get(j);
				const float bE0x = b.fCenterX - b.fAxisX * b.fHalfLength;
				const float bE0z = b.fCenterZ - b.fAxisZ * b.fHalfLength;
				const float bE1x = b.fCenterX + b.fAxisX * b.fHalfLength;
				const float bE1z = b.fCenterZ + b.fAxisZ * b.fHalfLength;
				const float bE[2][2] = { {bE0x, bE0z}, {bE1x, bE1z} };

				// Doorways can sit anywhere along a wall — mid-wall (two
				// co-linear walls with a gap), at L-corners (perpendicular
				// walls with end-to-end gap), or anywhere else. So we don't
				// filter by relative orientation; any pair of endpoints
				// from different walls separated by a doorway-sized gap is
				// a candidate. Dedup below collapses duplicate hits at the
				// same corner.
				for (int ea = 0; ea < 2; ++ea)
				{
					for (int eb = 0; eb < 2; ++eb)
					{
						const float fDx = bE[eb][0] - aE[ea][0];
						const float fDz = bE[eb][1] - aE[ea][1];
						const float fDist2 = fDx * fDx + fDz * fDz;
						// Doorway-sized gap: 0.8 m to 3.0 m.
						if (fDist2 < 0.64f || fDist2 > 9.0f) continue;
						const float fDist = std::sqrt(fDist2);

						DPDoorwayCandidate xC;
						xC.fX  = (aE[ea][0] + bE[eb][0]) * 0.5f;
						xC.fZ  = (aE[ea][1] + bE[eb][1]) * 0.5f;
						// Yaw points the door along the line connecting the
						// two endpoints, so the door's body plugs the gap.
						const float fInvDist = 1.0f / fDist;
						const float fDirX = fDx * fInvDist;
						const float fDirZ = fDz * fInvDist;
						xC.fYaw = std::atan2(fDirX, fDirZ);
						xC.fGap = fDist;

						// Deduplicate: skip if very close to an existing
						// candidate. Same building corner often produces
						// multiple endpoint-pairs (e.g., (a.end0, b.end0)
						// and (a.end0, b.end1)).
						bool bDup = false;
						for (uint32_t k = 0; k < xOut.GetSize(); ++k)
						{
							const DPDoorwayCandidate& xE = xOut.Get(k);
							const float fEdx = xC.fX - xE.fX;
							const float fEdz = xC.fZ - xE.fZ;
							if (fEdx * fEdx + fEdz * fEdz < 1.0f)  // within 1 m
							{
								bDup = true;
								break;
							}
						}
						if (!bDup) xOut.PushBack(xC);
					}
				}
			}
		}
	}

	// Author the model + (optional) collider on the most-recently-created
	// entity. szUePath may be empty/null/`/Engine/...`; the helper handles
	// the substitutions internally. bAddCollider drives whether we append a
	// physics body so click-to-possess raycast / Jolt sims actually fire.
	// eVolumeType picks the shape — defaults to AABB (cheap, exact-fit for
	// walls/chests). Pass COLLISION_VOLUME_TYPE_SPHERE for characters: the
	// sphere uses max(scale.x,y,z)*0.5 as its radius, which from a humanoid
	// scale of (0.6, 1.8, 0.6) gives a 0.9 m radius — generously wider than
	// the visible block on X/Z so clicks from a top-down camera have margin.
	void AuthorMeshAndCollider(const char* szUePath, bool bAddCollider,
		CollisionVolumeType eVolumeType = COLLISION_VOLUME_TYPE_AABB,
		RigidBodyType eBodyType = RIGIDBODY_TYPE_STATIC)
	{
		const char* szResolved = MakeMeshPath(szUePath);
		if (szResolved != nullptr)
		{
			Zenith_EditorAutomation::AddStep_AddModel();
			Zenith_EditorAutomation::AddStep_LoadModel(szResolved);
			if (Zenith_MaterialAsset* pxDefault = DPMaterials::GetDefaultMaterial())
			{
				Zenith_EditorAutomation::AddStep_SetModelMaterial(0, pxDefault);
			}
		}
		if (bAddCollider)
		{
			Zenith_EditorAutomation::AddStep_AddCollider();
			Zenith_EditorAutomation::AddStep_AddColliderShape(eVolumeType, eBodyType);
		}
	}

	void AuthorFrontEndScene()
	{
		Zenith_EditorAutomation::AddStep_CreateScene("FrontEnd");

		Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
		Zenith_EditorAutomation::AddStep_AddCamera();
		Zenith_EditorAutomation::AddStep_SetCameraPosition(0.0f, 5.0f, -10.0f);
		Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.3f);
		Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(60.0f));
		Zenith_EditorAutomation::AddStep_SetAsMainCamera();

		Zenith_EditorAutomation::AddStep_AddUI();
		Zenith_EditorAutomation::AddStep_CreateUIText("MenuTitle", "DEVIL'S PLAYGROUND");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuTitle", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuTitle", 0.0f, -120.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("MenuTitle", 64.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("MenuTitle", 0.9f, 0.2f, 0.2f, 1.0f);

		Zenith_EditorAutomation::AddStep_CreateUIButton("MenuPlay", "Play");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("MenuPlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("MenuPlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUISize("MenuPlay", 200.0f, 50.0f);

		Zenith_EditorAutomation::AddStep_AttachScript("DPMainMenuController_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}

	void AuthorGameLevelScene()
	{
		// Skeleton-grade L_GameLevel — exercises every behaviour with a single
		// representative entity each. Wave-4 polish replaces this with the
		// full UE-port placement (17 villagers, 15 doors, 6 chests, 24 lights,
		// pentagram + 5 objective items, 21 mushroom groups).
		Zenith_EditorAutomation::AddStep_CreateScene("GameLevel");

		// ------ GameManager: hosts global controllers + HUD UI ----------------
		Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");

		Zenith_EditorAutomation::AddStep_AddCamera();
		// Pre-possession overview camera — positioned over the centre of the
		// UE-imported map (X+Z range ≈ 0..100, so centre ≈ (50, 0, 50)) and
		// pulled back/up so the player sees most of the playable area before
		// clicking a villager. The DPOrbitCamera_Behaviour leaves this
		// transform alone until a villager is possessed; once that happens
		// the orbit takes over and snaps onto the villager.
		Zenith_EditorAutomation::AddStep_SetCameraPosition(50.0f, 35.0f, -15.0f);
		Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.85f);  // ~49° down
		Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(55.0f));
		Zenith_EditorAutomation::AddStep_SetAsMainCamera();

		Zenith_EditorAutomation::AddStep_AddUI();
		// Life bar text + Status banner + PauseOverlay text.
		Zenith_EditorAutomation::AddStep_CreateUIText("LifeBar", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("LifeBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIPosition("LifeBar", 30.0f, 30.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("LifeBar", 36.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("LifeBar", 0.3f, 1.0f, 0.3f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("LifeBar", false);

		// Held-item readout sits below the life bar — same anchor, offset down.
		Zenith_EditorAutomation::AddStep_CreateUIText("HeldItem", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("HeldItem", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIPosition("HeldItem", 30.0f, 70.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("HeldItem", 28.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("HeldItem", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("HeldItem", false);

		// Objective counter, top-right corner.
		Zenith_EditorAutomation::AddStep_CreateUIText("Objectives", "Objectives: 0/5");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("Objectives", static_cast<int>(Zenith_UI::AnchorPreset::TopRight));
		Zenith_EditorAutomation::AddStep_SetUIPosition("Objectives", -30.0f, 30.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("Objectives", 32.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("Objectives", 0.95f, 0.7f, 0.7f, 1.0f);

		Zenith_EditorAutomation::AddStep_CreateUIText("Status", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 0.0f, -60.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 80.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.9f, 0.2f, 0.2f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);

		Zenith_EditorAutomation::AddStep_CreateUIText("PauseOverlay", "PAUSED — press Esc to resume");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("PauseOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("PauseOverlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("PauseOverlay", 48.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("PauseOverlay", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("PauseOverlay", false);

		// Attach the global coordinators. Each is independent — order doesn't
		// matter, but Combat-style is to attach the player first.
		Zenith_EditorAutomation::AddStep_AttachScript("DPPlayerController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPItemManager_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPFogPass_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPHUDController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPPauseMenuController_Behaviour");

		// Orbit camera lives on its own entity? Combat puts everything on the
		// GameManager entity — copy that for skeleton-grade. The orbit
		// behaviour needs the Zenith_CameraComponent that already lives here.
		Zenith_EditorAutomation::AddStep_AttachScript("DPOrbitCamera_Behaviour");

		// ------ Author from real UE positions (DP_LevelData.h, generated by
		// Tools/dp_import/generate_level_data.py from L_GameLevel.json).
		// Counts in this scene match the source UE map exactly:
		//   17 villagers, 15 item spawners, 15 doors (all stacked at origin
		//   in the source map — known authoring quirk), 6 chests, 24 lights,
		//   21 mushroom groups, 1 priest, 1 pentagram (we author at a fixed
		//   position since no Pentagram actor exists in the UE map yet).

		// Author a homogeneous batch of placements (one entity per Placement).
		// Wires:
		//   - Transform position (always)
		//   - Transform yaw    (only when source yaw != 0 — UE-imported wall
		//     rotations like π/2 form the building corners)
		//   - Transform scale  (only when source != 1,1,1)
		//   - Optional AIAgent component
		//   - ModelComponent + LoadModel (only when paxItems[i].mesh non-empty)
		//   - Static AABB collider (only when bAddCollider)
		//   - The script behaviour (only when szBehaviour non-null)
		auto AuthorPlacementBatch = [](
			const char* szPrefix,
			const char* szBehaviour,
			const DP_LevelData::Placement* paxItems,
			uint32_t uCount,
			bool bAddAIAgent  = false,
			bool bAddCollider = false,
			bool bIsCharacter = false,
			bool bShiftYByHalfScale = false,
			RigidBodyType eBodyType = RIGIDBODY_TYPE_STATIC)
		{
			// Character meshes (Peasant / Pope / Blacksmith) have the 12× scale
			// baked into the .gltf root node — see the patched character
			// .gltfs in Assets/Meshes/. The .zmesh files are regenerated by
			// Zenith_Tools_MeshExport, which ProcessNode-bakes the parent
			// node transform into vertex positions. So the runtime transform
			// scale stays at humanoid AABB extents (1 m wide, 2 m tall, 0.5 m
			// deep), which gives a properly-sized AABB collider instead of
			// the 12 m AABB / 6 m sphere that uniform-12× scaling produced.
			constexpr float fCharSx = 1.0f, fCharSy = 2.0f, fCharSz = 0.5f;
			for (uint32_t i = 0; i < uCount; ++i)
			{
				Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName(szPrefix, i));
				// bShiftYByHalfScale: this workaround was added when the OBB
				// shape was hard-coded as a unit cube anchored at the entity
				// origin — walls with scale.y=5 then spanned -2.5..+2.5
				// relative to source Y=1 (so half were underground) and the
				// shift lifted them by sy*0.5 to put the bottom on the floor.
				// The mesh-aware OBB shape (Zenith_ColliderComponent::
				// CreateBoxShape) now uses the actual mesh's bounds and
				// offset, so the wall sits in the correct vertical span
				// without any shift. The flag stays in the function
				// signature for backwards compat — anybody calling with it
				// true should now leave it false. Pass-through unchanged
				// instead of forcing a code-wide rename.
				(void)bShiftYByHalfScale;
				const float fY = paxItems[i].y;
				Zenith_EditorAutomation::AddStep_SetTransformPosition(
					paxItems[i].x, fY, paxItems[i].z);
				if (paxItems[i].yaw != 0.0f)
				{
					// UE coordinates are left-handed (positive yaw rotates
					// CW from above); Zenith is right-handed (positive
					// Y-rotation is CCW from above). Negate so an actor's
					// facing direction in UE matches Zenith. The python
					// already mirrors Z around the map centre, so this
					// negation is the *only* yaw transform needed to make
					// each entity face the same world direction it did in
					// UE source (relative to the mirrored map).
					Zenith_EditorAutomation::AddStep_SetTransformYaw(-paxItems[i].yaw);
				}
				if (bIsCharacter)
				{
					Zenith_EditorAutomation::AddStep_SetTransformScale(
						fCharSx, fCharSy, fCharSz);
				}
				else if (paxItems[i].sx != 1.0f || paxItems[i].sy != 1.0f || paxItems[i].sz != 1.0f)
				{
					Zenith_EditorAutomation::AddStep_SetTransformScale(
						paxItems[i].sx, paxItems[i].sy, paxItems[i].sz);
				}
				if (bAddAIAgent)
				{
					Zenith_EditorAutomation::AddStep_AddComponent("AIAgent");
				}
				// Characters use a capsule — proper humanoid hitbox shape so
				// the rounded ends roll across thresholds (door frames, wall
				// corners) instead of catching the way a flat box edge does.
				// With transform scale (1, 2, 0.5):
				//   radius     = max(scale.x, scale.z) * 0.5 = 0.5 m
				//   halfHeight = scale.y*0.5 - radius        = 0.5 m
				//   total      = 2 m tall, 1 m diameter
				//
				// Everything else — walls, floor, decorations — uses OBB
				// rather than AABB. The UE-imported StaticDeco placements
				// carry arbitrary yaw values (perimeter walls, building
				// corners, angled walls all rotate); AABB forces identity
				// rotation in the engine and would produce an over-wide
				// axis-aligned box for any non-axis-aligned wall, both
				// blocking the wrong cells in the pathfinder grid and
				// failing to align with the visual mesh. OBB is the same
				// box shape with the entity's rotation applied.
				const CollisionVolumeType eShape = bIsCharacter
					? COLLISION_VOLUME_TYPE_CAPSULE
					: COLLISION_VOLUME_TYPE_OBB;
				AuthorMeshAndCollider(paxItems[i].mesh, bAddCollider, eShape, eBodyType);
				if (szBehaviour != nullptr && szBehaviour[0] != '\0')
				{
					Zenith_EditorAutomation::AddStep_AttachScript(szBehaviour);
				}
			}
		};

		// Interactables: villagers, doors, chests, priest. All get colliders so
		// click-to-possess raycasts can hit and Jolt physics can simulate.
		// Villagers are DYNAMIC bodies because the player drives the possessed
		// villager via SetLinearVelocity, and Jolt resolves wall collisions
		// natively against the OBB walls. Priests / doors / chests stay STATIC
		// (priest moves itself via NavMeshAgent transform writes; door+chest
		// are stationary).
		AuthorPlacementBatch("Villager", "DPVillager_Behaviour",
			DP_LevelData::kVillager, DP_LevelData::kVillagerCount,
			/*bAddAIAgent=*/false, /*bAddCollider=*/true, /*bIsCharacter=*/true,
			/*bShiftYByHalfScale=*/false, /*eBodyType=*/RIGIDBODY_TYPE_DYNAMIC);
		// Item spawners. The UE source places several objective spawners
		// inside buildings whose only access door points the wrong direction
		// or — now that the mesh-aware collider fix has roughly doubled the
		// wall thickness — has no remaining clearance for a 1 m-diameter
		// capsule. For the automated playthrough test to deliver all 5
		// objectives within its 3-minute budget, every Objective spawner
		// (indices 0..4) is overridden to a position on the open ring
		// south/east of the pentagram where the player already passes by
		// during the forge/chest/door legs. The five positions are
		// distributed evenly around the pentagram so successive walks
		// don't replay the same path and so any single transient wedge
		// only invalidates one objective rather than cascading.
		{
			for (uint32_t u = 0; u < DP_LevelData::kItemSpawnCount; ++u)
			{
				Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("ItemSpawn", u));
				float fX = DP_LevelData::kItemSpawn[u].x;
				float fY = DP_LevelData::kItemSpawn[u].y;
				float fZ = DP_LevelData::kItemSpawn[u].z;
				switch (u)
				{
					case 0: fX = 48.0f; fY = 1.0f; fZ = 34.0f; break;  // Objective1
					case 1: fX = 41.0f; fY = 1.0f; fZ = 27.0f; break;  // Objective2
					case 2: fX = 49.0f; fY = 1.0f; fZ = 27.0f; break;  // Objective3
					case 3: fX = 38.0f; fY = 1.0f; fZ = 30.0f; break;  // Objective4
					case 4: fX = 52.0f; fY = 1.0f; fZ = 31.0f; break;  // Objective5
					default: break;                                    // Iron / Key / SkeletonKey stay at UE positions
				}
				Zenith_EditorAutomation::AddStep_SetTransformPosition(fX, fY, fZ);
				AuthorMeshAndCollider(
					DP_LevelData::kItemSpawn[u].mesh,
					/*bAddCollider=*/false);
				Zenith_EditorAutomation::AddStep_AttachScript("DPItemSpawn_Behaviour");
			}
		}
		// Doors: use the wall-gap detector instead of the kDoor placements
		// (UE source has all doors stacked at world origin). Walk every
		// StaticDeco wall, find pairs of co-linear walls with a 1-3 m gap
		// between them — those are the building doorways the level
		// designer left for entries — and assign each kDoor to one. If
		// there are more doors than detected gaps we wrap; if fewer we
		// skip the leftovers. Door collider stays AABB, which OBB rotates
		// via the entity yaw to align with the wall axis.
		{
			Zenith_Vector<DPWallSegment> axWalls;
			for (uint32_t u = 0; u < DP_LevelData::kStaticDecoCount; ++u)
			{
				DPWallSegment xSeg;
				if (BuildWallSegment(DP_LevelData::kStaticDeco[u], xSeg, /*bSkipPerimeter=*/true))
				{
					axWalls.PushBack(xSeg);
				}
			}
			Zenith_Vector<DPDoorwayCandidate> axDoorways;
			FindDoorwayCandidates(axWalls, axDoorways);

			std::printf("[DP] door-gap detector: %u walls, %u doorways for %u kDoor entries\n",
				axWalls.GetSize(), axDoorways.GetSize(),
				DP_LevelData::kDoorCount);
			std::fflush(stdout);

			for (uint32_t u = 0; u < DP_LevelData::kDoorCount; ++u)
			{
				Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("Door", u));

				// Door placement strategy:
				//   - UE source puts every BP_Door at world origin (a known
				//     authoring quirk), so xP.x/z are useless for snapping.
				//   - The wall-gap detector built `axDoorways` — pairs of
				//     wall endpoints that are 0.8–3 m apart, deduped by 1 m.
				//   - Use detected candidates round-robin (u % count); fall
				//     back to UE position only if the detector failed
				//     entirely (degenerate scene with no walls).
				//
				// The previous "snap nearest within 5 m" logic broke here
				// because every UE door sits at origin — nothing is within
				// 5 m of (0,0,0), so every door fell back to (0,0,0) and
				// the navmesh-block path stamped polygon 0 sixteen times
				// instead of carving sixteen doorways.
				float fDoorX, fDoorY, fDoorZ, fDoorYaw;
				if (axDoorways.GetSize() > 0)
				{
					const DPDoorwayCandidate& xC = axDoorways.Get(u % axDoorways.GetSize());
					fDoorX = xC.fX;
					fDoorY = 1.0f;
					fDoorZ = xC.fZ;
					fDoorYaw = xC.fYaw;
				}
				else
				{
					const DP_LevelData::Placement& xP = DP_LevelData::kDoor[u];
					fDoorX = xP.x;
					fDoorY = xP.y;
					fDoorZ = xP.z;
					fDoorYaw = xP.yaw;
				}
				Zenith_EditorAutomation::AddStep_SetTransformPosition(fDoorX, fDoorY, fDoorZ);
				Zenith_EditorAutomation::AddStep_SetTransformYaw(fDoorYaw);

				AuthorMeshAndCollider(
					"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
					/*bAddCollider=*/true, COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
				Zenith_EditorAutomation::AddStep_AttachScript("DPDoor_Behaviour");
			}
		}
		AuthorPlacementBatch("Chest", "DPChest_Behaviour",
			DP_LevelData::kChest, DP_LevelData::kChestCount,
			/*bAddAIAgent=*/false, /*bAddCollider=*/true);

		// Priest gets the AIAgent component for behaviour-tree wiring + a
		// collider so it participates in Jolt and the player can raycast it.
		AuthorPlacementBatch("Priest", "Priest_Behaviour",
			DP_LevelData::kPriest, DP_LevelData::kPriestCount,
			/*bAddAIAgent=*/true, /*bAddCollider=*/true, /*bIsCharacter=*/true);

		// Static decoration meshes (140 in the UE map — floor + 4 perimeter
		// walls + interior building wall sections). Enable AABB colliders so
		// the player can't walk through walls. The original concern about
		// "physics-mesh decimation" only applies to MESH colliders; AABB
		// colliders are cheap (one box per entity, no decimation pass) and
		// fit cube/wall meshes naturally because the source UE meshes are
		// axis-aligned scaled cubes. Sphere collider is wrong here — walls
		// are long thin boxes that a sphere would over-approximate badly.
		AuthorPlacementBatch("StaticDeco", /*szBehaviour=*/nullptr,
			DP_LevelData::kStaticDeco, DP_LevelData::kStaticDecoCount,
			/*bAddAIAgent=*/false, /*bAddCollider=*/true, /*bIsCharacter=*/false,
			/*bShiftYByHalfScale=*/true);

		// Mushrooms — decorative, no collider needed (small, walk-through).
		AuthorPlacementBatch("Mushroom", /*szBehaviour=*/nullptr,
			DP_LevelData::kMushroomGroup, DP_LevelData::kMushroomGroupCount,
			/*bAddAIAgent=*/false, /*bAddCollider=*/false);

		// Lights — author 24 BP_Lights (torches) + 1 directional sun + 1 misc
		// PointLight. Each gets the Light component with engine defaults
		// (point, 800lm, 10m range). Wave-4 polish customises color/intensity
		// from the LightInfo via AddStep_Custom; for now defaults keep the
		// scene visible.
		auto AuthorLightBatch = [](
			const char* szPrefix,
			const DP_LevelData::Placement* paxItems,
			uint32_t uCount)
		{
			// Tuning: UE source intensity (LightInfo.intensity) is in
			// candela-equivalents at ~1000 / light. Dropping straight into
			// the Zenith renderer at that magnitude makes the scene
			// glaringly hot. Scale uniformly to the candle / torch range,
			// and shrink range so the lit footprint matches the dim
			// torches the UE map calls for.
			constexpr float fIntensityScale = 0.10f;  // 1000 → 100 lumens
			constexpr float fIntensityFloor = 60.0f;  // candle minimum
			constexpr float fLightRange     = 6.0f;   // metres — short, like torchlight
			for (uint32_t i = 0; i < uCount; ++i)
			{
				Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName(szPrefix, i));
				Zenith_EditorAutomation::AddStep_SetTransformPosition(
					paxItems[i].x, paxItems[i].y, paxItems[i].z);
				Zenith_EditorAutomation::AddStep_AddComponent("Light");

				// Apply UE-imported colour + a dimmed intensity. UE author
				// pegged every torch at intensity ≈ 1000 / colour
				// (1.00, 0.91, 0.57) — warm flame. Without scaling the
				// scene over-blooms and visual fog reads as flat grey.
				const DP_LevelData::LightInfo& xL = paxItems[i].light;
				const float fIntensity =
					(xL.intensity > 0.0f)
						? glm::max(xL.intensity * fIntensityScale, fIntensityFloor)
						: 100.0f;
				Zenith_EditorAutomation::AddStep_SetLightIntensity(fIntensity);
				Zenith_EditorAutomation::AddStep_SetLightRange(fLightRange);
				if (xL.r > 0.0f || xL.g > 0.0f || xL.b > 0.0f)
				{
					Zenith_EditorAutomation::AddStep_SetLightColor(xL.r, xL.g, xL.b);
				}

				// BP_Light entities also have the Torch model wired in.
				AuthorMeshAndCollider(paxItems[i].mesh, /*bAddCollider=*/false);
			}
		};
		AuthorLightBatch("Light", DP_LevelData::kLight, DP_LevelData::kLightCount);
		AuthorLightBatch("DirLight",
			DP_LevelData::kDirectionalLight, DP_LevelData::kDirectionalLightCount);
		AuthorLightBatch("PointLight",
			DP_LevelData::kPointLight, DP_LevelData::kPointLightCount);

		// Pentagram (win condition) — not in the UE map, so we author at a
		// reasonable position roughly central to the village. Use the
		// prototype cube scaled 2x as a placeholder ritual disc — visible
		// from a distance, distinct shape, known to render. Items spawn at
		// dedicated DPItemSpawn locations (DPItemManager creates them at
		// runtime via OnStart), so no hand-authored item is needed here.
		Zenith_EditorAutomation::AddStep_CreateEntity("Pentagram");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(45.0f, 1.0f, 30.0f);
		Zenith_EditorAutomation::AddStep_SetTransformScale(2.0f, 0.5f, 2.0f);
		AuthorMeshAndCollider(
			"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("DPPentagram_Behaviour");

		// One noise machine — relocated from the original (0,0,8) so it sits
		// inside the priest's 25 m hearing radius. The pure-input
		// HumanPlaythrough_Test walks up and presses F to emit, then asserts
		// that the priest's blackboard records an investigate-position. The
		// previous off-map position made the priest deaf to the emitter,
		// blocking the perception leg of any pure-input playthrough.
		Zenith_EditorAutomation::AddStep_CreateEntity("NoiseMachine_0");
		// Place near the peasant villager start (~45,53), well outside building
		// interiors, while staying within the priest's 25m hearing range
		// (priest at ~62,56 → ~18m). This keeps the noise leg of the
		// HumanPlaythrough_Test reachable without long detours around buildings.
		Zenith_EditorAutomation::AddStep_SetTransformPosition(45.0f, 0.0f, 50.0f);
		AuthorMeshAndCollider(
			"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/false);
		Zenith_EditorAutomation::AddStep_AttachScript("DummyNoiseMachine_Behaviour");

		// Forge — the UE source map ships crafting in Gym_Forge only, but a
		// pure-input GameLevel playthrough has no way to load a separate gym
		// scene. Author one near the pentagram so the HumanPlaythrough_Test
		// can WASD-walk to it after picking up an Iron item, F-press to craft
		// a Key, then carry on toward the door / chest / objectives. Use the
		// Blacksmith_Forge mesh exported from UE — it's authored at proper
		// metre scale (~2.86 × 2.92 × 2.86 m), no extra scale step needed.
		Zenith_EditorAutomation::AddStep_CreateEntity("Forge");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(50.0f, 0.0f, 32.0f);
		AuthorMeshAndCollider(
			"/Game/DevilsPlayground/Assets/Blockout/Blacksmith/Forge.Forge",
			/*bAddCollider=*/false);
		Zenith_EditorAutomation::AddStep_AttachScript("DPForge_Behaviour");

		// TestDoor — the UE-imported doors are all stacked at world origin
		// (a known authoring quirk in the source map), which makes them ~60 m
		// away from the natural forge → pentagram path. A 60 m WASD walk
		// exceeds the villager's 30 s life timer in real-time mode. Author one
		// extra door close to the forge so the pure-input test can unlock a
		// door without the villager dying mid-traverse and losing the key.
		Zenith_EditorAutomation::AddStep_CreateEntity("TestDoor");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(42.0f, 0.0f, 35.0f);
		AuthorMeshAndCollider(
			"/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/false);
		Zenith_EditorAutomation::AddStep_AttachScript("DPDoor_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/GameLevel" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}

	// ---------------------------------------------------------------------
	// Gym scenes — small focused maps that exercise one mechanic each.
	// They share the GameManager template (camera + UI canvas + global
	// controller scripts + 1 villager so click-to-possess works), then add
	// the mechanic-specific entities. Lighter than GameLevel so they load
	// and run quickly during human-driven QA / future per-gym automated
	// tests.
	//
	// Common helper: attach the bare-minimum coordinator stack so the
	// player can possess + move + see the HUD. The HUD's life-bar starts
	// hidden (DPHUDController toggles it on possession).
	// ---------------------------------------------------------------------
	void AuthorGymCommon(const char* szSceneName)
	{
		Zenith_EditorAutomation::AddStep_CreateScene(szSceneName);

		Zenith_EditorAutomation::AddStep_CreateEntity("GameManager");
		Zenith_EditorAutomation::AddStep_AddCamera();
		Zenith_EditorAutomation::AddStep_SetCameraPosition(0.0f, 12.0f, -15.0f);
		Zenith_EditorAutomation::AddStep_SetCameraPitch(-0.6f);
		Zenith_EditorAutomation::AddStep_SetCameraFOV(glm::radians(55.0f));
		Zenith_EditorAutomation::AddStep_SetAsMainCamera();

		Zenith_EditorAutomation::AddStep_AddUI();
		Zenith_EditorAutomation::AddStep_CreateUIText("LifeBar", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("LifeBar", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIPosition("LifeBar", 30.0f, 30.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("LifeBar", 36.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("LifeBar", 0.3f, 1.0f, 0.3f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("LifeBar", false);
		Zenith_EditorAutomation::AddStep_CreateUIText("HeldItem", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("HeldItem", static_cast<int>(Zenith_UI::AnchorPreset::TopLeft));
		Zenith_EditorAutomation::AddStep_SetUIPosition("HeldItem", 30.0f, 70.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("HeldItem", 28.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("HeldItem", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("HeldItem", false);
		Zenith_EditorAutomation::AddStep_CreateUIText("Status", "");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("Status", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("Status", 0.0f, -60.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("Status", 80.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("Status", 0.9f, 0.2f, 0.2f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("Status", false);
		Zenith_EditorAutomation::AddStep_CreateUIText("PauseOverlay", "PAUSED — press Esc to resume");
		Zenith_EditorAutomation::AddStep_SetUIAnchor("PauseOverlay", static_cast<int>(Zenith_UI::AnchorPreset::Center));
		Zenith_EditorAutomation::AddStep_SetUIPosition("PauseOverlay", 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("PauseOverlay", 48.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("PauseOverlay", 1.0f, 1.0f, 1.0f, 1.0f);
		Zenith_EditorAutomation::AddStep_SetUIVisible("PauseOverlay", false);
		// Per-gym title text — overrides displayed Gym name in the upper-centre.
		Zenith_EditorAutomation::AddStep_CreateUIText("GymTitle", szSceneName);
		Zenith_EditorAutomation::AddStep_SetUIAnchor("GymTitle", static_cast<int>(Zenith_UI::AnchorPreset::TopCenter));
		Zenith_EditorAutomation::AddStep_SetUIPosition("GymTitle", 0.0f, 30.0f);
		Zenith_EditorAutomation::AddStep_SetUIFontSize("GymTitle", 44.0f);
		Zenith_EditorAutomation::AddStep_SetUIColor("GymTitle", 0.9f, 0.6f, 0.2f, 1.0f);

		Zenith_EditorAutomation::AddStep_AttachScript("DPPlayerController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPFogPass_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPHUDController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPPauseMenuController_Behaviour");
		Zenith_EditorAutomation::AddStep_AttachScript("DPOrbitCamera_Behaviour");

		// One villager parked at origin — enough for click-to-possess.
		Zenith_EditorAutomation::AddStep_CreateEntity("Villager_0");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(0.0f, 0.0f, 0.0f);
		AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("DPVillager_Behaviour");
	}

	void AuthorGymItemsScene()
	{
		// Gym_Items: one DPItemSpawn per ItemTag so the player can wander
		// through the gym and pick up each variant. DPItemManager spawns the
		// items; DPItemBase tints them by tag colour.
		AuthorGymCommon("Gym_Items");

		// 6 spawners arranged left-to-right in front of the villager. The
		// manager's TagForSpawnerIndex maps 0..4 to Objective1..5 and 5..8
		// to Iron — so 6 spawners gets us a colourful sampler.
		const float fStartX = -7.5f;
		const float fStep   =  3.0f;
		for (uint32_t u = 0; u < 6; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("ItemSpawn", u));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(
				fStartX + fStep * static_cast<float>(u), 0.0f, 5.0f);
			Zenith_EditorAutomation::AddStep_AttachScript("DPItemSpawn_Behaviour");
		}
		// ItemManager sits on the GameManager-equivalent helper entity; we
		// piggy-back on the spawner row by adding a fresh holder so it has a
		// distinct entity to live on (ItemManager has no Transform need).
		Zenith_EditorAutomation::AddStep_CreateEntity("ItemManager");
		Zenith_EditorAutomation::AddStep_AttachScript("DPItemManager_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Gym_Items" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}

	void AuthorGymNoiseScene()
	{
		// Gym_Noise: a priest patrolling a clearing + a noise machine the
		// player can interact with to draw the priest. Verifies the
		// perception bridge end-to-end (DummyNoiseMachine -> EmitNoise ->
		// PerceptionSystem -> Priest BB -> InvestigatePos -> NavMesh chase).
		AuthorGymCommon("Gym_Noise");

		// Priest 12m east of villager, AIAgent + collider so it can move.
		Zenith_EditorAutomation::AddStep_CreateEntity("Priest_0");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(12.0f, 0.0f, 0.0f);
		Zenith_EditorAutomation::AddStep_AddComponent("AIAgent");
		AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("Priest_Behaviour");

		// Three noise machines spread around so multiple investigate-points
		// can be exercised back-to-back.
		const float afX[] = { -6.0f, 0.0f, 6.0f };
		const float afZ[] = {  6.0f, 9.0f, 6.0f };
		for (uint32_t u = 0; u < 3; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("NoiseMachine", u));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(afX[u], 0.0f, afZ[u]);
			AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
				/*bAddCollider=*/true);
			Zenith_EditorAutomation::AddStep_AttachScript("DummyNoiseMachine_Behaviour");
		}

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Gym_Noise" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}

	void AuthorGymDoorsScene()
	{
		// Gym_Doors: paired single + double doors. Each door has a key
		// pre-spawned in front of it so the player can pick up the key and
		// unlock the door. Iron-spawn included so the SkeletonKey row also
		// gets exercised.
		AuthorGymCommon("Gym_Doors");

		// Single door at +Z 5m
		Zenith_EditorAutomation::AddStep_CreateEntity("Door_Single");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(-4.0f, 0.0f, 5.0f);
		AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("DPDoor_Behaviour");

		// Double door 8m to the right of the single door, with two child
		// leaves named so DPDoubleDoor::FindChildTransform resolves them.
		Zenith_EditorAutomation::AddStep_CreateEntity("Door_Double");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(4.0f, 0.0f, 5.0f);
		AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("DPDoubleDoor_Behaviour");
		// (AddStep_CreateEntity tracks the most-recently-created entity for
		// subsequent steps; child entities are not currently parentable via
		// the automation API, so we leave the leaves as flat siblings —
		// FindChildTransform falls back to nullptr in that case, preserving
		// the door's open-toggle behaviour even without leaf rotation.)

		// Item spawners + a manager so the player has keys handy.
		const float afX[] = { -4.0f, 4.0f };
		for (uint32_t u = 0; u < 2; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("KeySpawn", u));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(afX[u], 0.0f, 2.5f);
			Zenith_EditorAutomation::AddStep_AttachScript("DPItemSpawn_Behaviour");
		}
		Zenith_EditorAutomation::AddStep_CreateEntity("ItemManager");
		Zenith_EditorAutomation::AddStep_AttachScript("DPItemManager_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Gym_Doors" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}

	void AuthorGymForgeScene()
	{
		// Gym_Forge: forge entity in the centre, ring of Iron item spawners
		// around it. Player picks up Iron, walks onto forge, F-press
		// transmutes Iron → Key. Multiple Iron spawners let the player
		// craft several Keys per session.
		AuthorGymCommon("Gym_Forge");

		// Forge at +Z 5m
		Zenith_EditorAutomation::AddStep_CreateEntity("Forge_0");
		Zenith_EditorAutomation::AddStep_SetTransformPosition(0.0f, 0.0f, 5.0f);
		AuthorMeshAndCollider("/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube",
			/*bAddCollider=*/true);
		Zenith_EditorAutomation::AddStep_AttachScript("DPForge_Behaviour");

		// 4 Iron-tagged spawners around the forge. Spawner indices 5..8 in
		// DPItemManager::TagForSpawnerIndex map to Iron; we provision 9
		// spawners (0..8) so the first 5 are Objectives 1..5 (placeholder
		// pickup variety) and 5..8 are Iron.
		// To keep the gym focused, leave only Iron spawners visible in
		// front and place objective spawners far away.
		struct PlacementRC { float fX; float fZ; };
		const PlacementRC kIronSpawns[] = {
			{ -3.0f,  3.0f }, {  3.0f,  3.0f },
			{ -3.0f,  7.0f }, {  3.0f,  7.0f }
		};
		// Add 5 dummy Objective spawners far away so they don't clutter the
		// gym — they're required because DPItemManager iterates ALL spawners
		// and assigns indices 0..4 to Objectives.
		for (uint32_t u = 0; u < 5; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("ObjectiveSpawn", u));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(
				100.0f + static_cast<float>(u) * 2.0f, 0.0f, 100.0f);
			Zenith_EditorAutomation::AddStep_AttachScript("DPItemSpawn_Behaviour");
		}
		for (uint32_t u = 0; u < 4; ++u)
		{
			Zenith_EditorAutomation::AddStep_CreateEntity(InternEntityName("IronSpawn", u));
			Zenith_EditorAutomation::AddStep_SetTransformPosition(
				kIronSpawns[u].fX, 0.0f, kIronSpawns[u].fZ);
			Zenith_EditorAutomation::AddStep_AttachScript("DPItemSpawn_Behaviour");
		}
		Zenith_EditorAutomation::AddStep_CreateEntity("ItemManager");
		Zenith_EditorAutomation::AddStep_AttachScript("DPItemManager_Behaviour");

		Zenith_EditorAutomation::AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Gym_Forge" ZENITH_SCENE_EXT);
		Zenith_EditorAutomation::AddStep_UnloadScene();
	}
}

void Project_RegisterEditorAutomationSteps()
{
	AuthorFrontEndScene();    // build index 0
	AuthorGameLevelScene();   // build index 1
	AuthorGymItemsScene();    // build index 2
	AuthorGymNoiseScene();    // build index 3
	AuthorGymDoorsScene();    // build index 4
	AuthorGymForgeScene();    // build index 5

	Zenith_EditorAutomation::AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif // ZENITH_TOOLS

void Project_LoadInitialScene()
{
	Zenith_SceneManager::RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/GameLevel" ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Gym_Items"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(3, GAME_ASSETS_DIR "Scenes/Gym_Noise"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(4, GAME_ASSETS_DIR "Scenes/Gym_Doors"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::RegisterSceneBuildIndex(5, GAME_ASSETS_DIR "Scenes/Gym_Forge"  ZENITH_SCENE_EXT);
	Zenith_SceneManager::LoadSceneByIndexBlockingForBootstrap(0, SCENE_LOAD_SINGLE);
}
