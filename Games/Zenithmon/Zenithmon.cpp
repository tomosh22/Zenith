#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"   // the human locomotion animator
#include "EntityComponent/Components/Zenith_ColliderComponent.h"   // the explicit human body contract
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_NavMeshComponent.h"
#include "Flux/MeshAnimation/Flux_AnimationController.h"           // clips + layer + state machine
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_BlendTree.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"        // the instance LoadModel replaces on every load
#include "EntityComponent/Components/Zenith_TransformComponent.h"   // the authored-rotation save guard (ZM-D-179)
#include "SaveData/Zenith_SaveData.h"
#include "Zenithmon/Components/ZM_BattleArena.h"
#include "Zenithmon/Components/ZM_BattleDirector.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"
#include "Zenithmon/Components/ZM_GameComponent.h"
#include "Zenithmon/Components/ZM_FollowCamera.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_GraphNodes.h"                   // ZM_RegisterGraphNodes + the SC7 node counters
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"                     // ZM_GetNpcData -- the greybox's appearance row (W4)
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"              // ZM_GetHumanPaletteColour (W4)
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"             // the challenge graph's asset path + builder (S7 SC7)
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"   // ResetRuntimeStateForTests (between-tests hook)
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"      // ZM_TrainerEngagementLatch + ZM_TrainerCinematicLatch (between-tests hook)
#include "Zenithmon/Source/Nav/ZM_NavBake.h"                      // Dawnmere navmesh bake step + asset ref (S7 SC1b)
#include "Zenithmon/Source/Save/ZM_SaveSlots.h"                   // DeleteAllSlotsForTests (between-tests hook)
#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"   // sz*_NAME element contract (dialogue authoring)
#include "Zenithmon/Source/UI/ZM_UI_Bag.h"           // sz*_NAME + RowElementName + geometry contract (bag authoring)
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"     // fZM_BATTLE_MENU_ROOT_* row constants (battle menu authoring)
#include "Zenithmon/Source/UI/ZM_UI_Dex.h"           // sz*_NAME + geometry contract (dex authoring)
#include "Zenithmon/Source/UI/ZM_UI_Party.h"         // sz*_NAME + SlotElementName contract (party authoring)
#include "Zenithmon/Source/UI/ZM_UI_SaveSlots.h"     // sz*_NAME + RowElementName contract (S7 SC4 save-screen authoring)
#include "Zenithmon/Source/UI/ZM_UI_Shop.h"          // sz*_NAME + RowElementName + geometry contract (shop authoring)
#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"     // title panel / Continue / New Game authoring contract (S7 SC5)
// ★ UNCONDITIONAL, and NOT in the ZENITH_TOOLS block below that carries
// ZM_ProfLabPlacement.h. ZM_GreyboxVisual compiles in EVERY configuration and
// reads ZM_IsPlayerHomeBlockName / ZM_GetPlayerHomeInteriorTintColour from here
// (ZM-D-176); the tools-only authoring loop reads the block table from the same
// file, so both sides share one spelling.
#include "Zenithmon/Source/World/ZM_HumanAssetPolicy.h"           // is the human bake loadable right now?
#include "Zenithmon/Source/World/ZM_HumanBody.h"               // THE human body contract (size, capsule, visual scale)
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"      // the PlayerHome shell + its ZM-D-176 warm tint
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include <string>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_CommandLine.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"   // the SC8 no-graph authoring pin
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"        // the shared authored coordinates (S7 item 3 SC8)
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"         // the shared ProfLab interior coordinates (S8 SC1)
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"

#include <cstring>
#include <filesystem>
#include <string>
#endif

// ZM_GreyboxVisual -- the ONE visual component every authored Zenithmon entity
// wears, and the thing that decides what it looks like.
//
// It serves TWO POPULATIONS, and keeping them apart is the whole design:
//
//   BLOCKOUT  -- every wall, floor, door, lintel and interior shell. A unit cube
//                in the shipped blockout grey (or PlayerHome's warm interior tint,
//                ZM-D-176). This branch is BYTE-FOR-BYTE what it has always been
//                and must stay that way: ZM_AutoTests_InteriorTint measures the
//                tint to 1.0e-4.
//   HUMAN     -- the six authored Dawnmere NPCs and the player. These get the
//                generated humanoid MODEL (ZM_HumanGen's centre-anchored bind
//                space) plus an Idle/Walk animator, and their physics body is
//                installed from the COMPILED body contract.
//
// ★ THE HUMAN BRANCH IS THE ONE THAT MOVED. Before this, an NPC and a doorframe
// were the same object on screen: both a grey unit cube, the NPC merely tinted
// from its ZM_HUMAN_ID palette entry. The palette has not gone -- it is now the
// COLD-START FALLBACK (see below) rather than the shipped appearance.
//
// ★ COLD STARTS ARE A PICTURE PROBLEM, NEVER A GAMEPLAY ONE. If the human bake is
// absent and cannot be made (a non-tools build on a fresh clone), a resolved human
// gets a proportioned 0.8 x 1.8 x 0.8 block in its palette colour instead of a
// model -- the exact block the game used to ship, in the exact same place. Its
// COLLIDER comes from the same compiled contract either way, so the capsule, the
// ground probe, the camera pivot, the head anchors and every spawn point measure
// the same body whether or not a single asset exists on disk.
//
// ★ WHY THIS IS A STATE MACHINE AND NOT A BOOL. Zenith_ModelComponent::LoadModel
// always calls ClearModel, replacing the skeleton INSTANCE, while
// Zenith_AnimatorComponent::TryDiscoverSkeleton returns immediately once the
// controller is initialised. A second LoadModel therefore leaves the controller
// bound to a DESTROYED instance, which a flag that merely suppresses duplicate
// clips does not prevent. So this component records what it actually loaded and
// re-binds the controller explicitly after any model replacement. That is safe
// because ALL 35 humans share ONE rig: bone names and indices are identical across
// any model swap, so clips, layers and the state machine survive the rebind.
//
// ★ WHY THIS IS SAFE AT ORDER 107, GIVEN ZM_Interactable IS 113.
// OnStart hooks run in ASCENDING serialization order WITHIN one entity
// (Zenith_ComponentMetaRegistry::DispatchOnStart -> DispatchLifecycleHook over
// m_xMetasSorted), so this component starts BEFORE ZM_Interactable does. That
// would be fatal if the thing we read were established in ZM_Interactable::
// OnStart -- which is exactly how the trainer id works. It is NOT how the NPC ROW
// works: m_eNpcId arrives either from ZM_Interactable::ReadFromDataStream (which
// provably runs for every component of an entity before any pending start is
// dispatched) or from the AddStep_Custom authoring step (which runs with the
// editor Stopped, so no OnStart has fired at all). This component deliberately
// reads ONLY the row, never GetTrainerId().
//
// The one thing 113-runs-later does cost us is ZM_Interactable::OnStart's
// stale-row CLAMP: an out-of-range serialized id has not been reset to
// ZM_NPC_NONE yet when we look. Hence the explicit bounds check below.
//
// ★ REBUILDING THE BODY AT 107 IS SAFE, AND CHECKED. Installing explicit
// dimensions destroys and recreates the Jolt body, which drops sensor state,
// gravity and rotation locks. The two DYNAMIC NPCs get all of that re-applied by
// ZM_Interactable::ApplyDrivenBodySetup at 113, which is keyed on body-ID
// identity -- so a rebuild here makes it re-apply exactly once. The four STATIC
// townsfolk have no such configuration to lose, and the PLAYER's body is not
// touched here at all (ZM_PlayerController::EnsureAndConfigureBody owns it and
// installs the same dimensions immediately before its own configuration block).
//
// NOTHING NEW IS SERIALIZED. WriteToDataStream still emits a single version u_int,
// so the committed .zscen bytes cannot move; everything below is re-derived on
// every load from bytes that were already there.
class ZM_GreyboxVisual
{
public:
	ZM_GreyboxVisual() = delete;
	explicit ZM_GreyboxVisual(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	ZM_GreyboxVisual(const ZM_GreyboxVisual&) = delete;
	ZM_GreyboxVisual& operator=(const ZM_GreyboxVisual&) = delete;
	ZM_GreyboxVisual(ZM_GreyboxVisual&&) noexcept = default;
	ZM_GreyboxVisual& operator=(ZM_GreyboxVisual&&) noexcept = default;

	void OnStart()
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}

		// THE EARLY BRANCH, and deliberately so: an entity that resolves to no NPC
		// row and carries no ZM_PlayerController is a blockout, and never reaches a
		// line of human code.
		const ZM_HUMAN_ID eHumanId = ResolveHumanId();
		if (eHumanId >= ZM_HUMAN_COUNT)
		{
			ApplyBlockout();
			return;
		}
		ApplyHuman(eHumanId);
	}

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		xStream << 1u;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0u;
		xStream >> uVersion;
		(void)uVersion;
		// m_eLoadedKind / m_eLoadedHumanId / m_bAnimatorAuthored are deliberately NOT
		// cleared: they record what THIS instance actually did to the model and the
		// animator, which a stream read does not undo. A genuine scene load builds a
		// FRESH component, so it starts at NONE and takes the normal path.
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		const ZM_HUMAN_ID eHumanId = ResolveHumanId();
		if (eHumanId < ZM_HUMAN_COUNT)
		{
			ImGui::Text("Human: %s (%s)", ZM_GetHumanName(eHumanId), KindName(m_eLoadedKind));
			const Zenith_Maths::Vector4 xColour = ZM_GetHumanPaletteColour(eHumanId);
			ImGui::Text("Cold-start fallback colour: %.3f, %.3f, %.3f",
				xColour.x, xColour.y, xColour.z);
			return;
		}
		ImGui::TextUnformatted("Replaceable S3 greybox unit cube");
		// DERIVED live rather than read back off the material, so the panel shows
		// what the NEXT start would paint.
		const Zenith_Maths::Vector4 xColour = ResolveBlockoutColour();
		ImGui::Text("Appearance (derived): %.3f, %.3f, %.3f",
			xColour.x, xColour.y, xColour.z);
	}
#endif

private:
	// What this component last put on the entity. HUMAN_FALLBACK is a first-class
	// state, not an error: it is what a cold tree ships, and it must be able to
	// transition to HUMAN (and back) without stacking meshes.
	enum ZM_VISUAL_KIND : u_int
	{
		ZM_VISUAL_NONE,
		ZM_VISUAL_BLOCKOUT,
		ZM_VISUAL_HUMAN_FALLBACK,
		ZM_VISUAL_HUMAN,
	};

	static const char* KindName(ZM_VISUAL_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_VISUAL_BLOCKOUT:       return "blockout";
		case ZM_VISUAL_HUMAN_FALLBACK: return "cold fallback block";
		case ZM_VISUAL_HUMAN:          return "model";
		default:                       return "nothing yet";
		}
	}

	// ---- Who is this? -------------------------------------------------------
	// NAME-INDEPENDENT on purpose, so all three scenes work: an NPC is whoever its
	// sibling ZM_Interactable's authored row says, and the player is whoever
	// carries the controller.
	ZM_HUMAN_ID ResolveHumanId() const
	{
		if (const ZM_Interactable* pxInteractable =
			m_xParentEntity.TryGetComponent<ZM_Interactable>())
		{
			// ZM_NPC_NONE aliases ZM_NPC_COUNT, so one comparison rejects the sentinel
			// and every garbage value together. It must come FIRST: ZM_GetNpcData
			// asserts on an out-of-range id, and ZM_Interactable's own clamp has not
			// run yet at order 107 (see the class comment).
			const ZM_NPC_ID eNpcId = pxInteractable->GetNpcId();
			if (eNpcId >= ZM_NPC_COUNT)
			{
				return ZM_HUMAN_NONE;
			}
			return ZM_GetNpcData(eNpcId).m_eHuman;
		}
		if (m_xParentEntity.TryGetComponent<ZM_PlayerController>() != nullptr)
		{
			// ZM_HUMAN_PLAYER_F's row and bake both exist, but nothing selects a
			// gender and the save schema has no field for one -- see Docs/Questions.md.
			return ZM_HUMAN_PLAYER_M;
		}
		return ZM_HUMAN_NONE;
	}

	// ---- BLOCKOUT: unchanged, and it must stay unchanged ---------------------
	void ApplyBlockout()
	{
		const Zenith_Maths::Vector4 xBaseColour = ResolveBlockoutColour();

		// A RE-RUN must REFRESH, never restack. ReadFromDataStream can hand this
		// instance a different sibling row, and a second AddMeshEntry would leave the
		// entity drawing two overlapping cubes with the OLD material on the first.
		if (m_eLoadedKind == ZM_VISUAL_BLOCKOUT)
		{
			if (Zenith_MaterialAsset* pxOwnedMaterial = m_xMaterial.GetDirect())
			{
				ApplyAppearance(*pxOwnedMaterial, xBaseColour);
			}
			return;
		}

		m_xGeometry = Zenith_MeshGeometryAsset::CreateUnitCube();
		if (!BuildBlockMesh(xBaseColour))
		{
			return;
		}
		m_eLoadedKind = ZM_VISUAL_BLOCKOUT;
	}

	// ---- HUMAN: model when warm, proportioned palette block when cold --------
	void ApplyHuman(ZM_HUMAN_ID eHumanId)
	{
		// The body first and unconditionally: gameplay dimensions must not depend on
		// whether the picture loaded.
		InstallHumanBody();

		const ZM_VISUAL_KIND eDesired = ZM_AreHumanAssetsReady()
			? ZM_VISUAL_HUMAN
			: ZM_VISUAL_HUMAN_FALLBACK;
		if (m_eLoadedKind == eDesired && m_eLoadedHumanId == eHumanId)
		{
			return;   // nothing to do -- no LoadModel, no AddMeshEntry, no rebuild
		}

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}

		if (eDesired == ZM_VISUAL_HUMAN && ApplyHumanModel(*pxModel, eHumanId))
		{
			m_eLoadedKind    = ZM_VISUAL_HUMAN;
			m_eLoadedHumanId = eHumanId;
			return;
		}

		// Cold, or the model refused to load. Either way the block is what ships.
		// ★ CLEAR FIRST. AddMeshEntry APPENDS to an existing instance, so without
		// this a HUMAN -> HUMAN_FALLBACK transition would draw the block ON TOP of
		// the human still in the model.
		if (m_eLoadedKind != ZM_VISUAL_NONE)
		{
			pxModel->ClearModel();
		}
		m_xGeometry = Zenith_MeshGeometryAsset::CreateBox(Zenith_Maths::Vector3(
			fZM_HUMAN_BODY_FOOTPRINT * 0.5f / fZM_HUMAN_VISUAL_SCALE,
			fZM_HUMAN_BODY_HALF_HEIGHT / fZM_HUMAN_VISUAL_SCALE,
			fZM_HUMAN_BODY_FOOTPRINT * 0.5f / fZM_HUMAN_VISUAL_SCALE));
		if (!BuildBlockMesh(ZM_GetHumanPaletteColour(eHumanId)))
		{
			return;
		}
		m_eLoadedKind    = ZM_VISUAL_HUMAN_FALLBACK;
		m_eLoadedHumanId = eHumanId;
	}

	// Load the baked model and (re)bind the animator. False if the model did not
	// resolve, in which case the caller falls back to the block.
	bool ApplyHumanModel(Zenith_ModelComponent& xModel, ZM_HUMAN_ID eHumanId)
	{
		char acModelRef[256];
		if (!ZM_HumanAssetPath(eHumanId, ZM_HUMAN_ASSET_MODEL, acModelRef,
			static_cast<u_int>(sizeof(acModelRef))))
		{
			return false;
		}

		xModel.LoadModel(std::string(acModelRef));
		Flux_SkeletonInstance* pxSkeleton = xModel.GetSkeletonInstance();
		if (pxSkeleton == nullptr)
		{
			// LoadModel REFUSES a missing file without clearing, so the model may still
			// hold whatever was there. Say so and let the caller clear + fall back.
			Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
				"[ZM_GreyboxVisual] '%s' reported warm but did not load; using the "
				"cold-start block", acModelRef);
			return false;
		}

		// The material handle belongs to the block paths; a model carries its own.
		m_xMaterial = MaterialHandle();
		m_xGeometry = MeshGeometryHandle();

		EnsureHumanAnimator(*pxSkeleton);
		return true;
	}

	// ---- The body: a COMPILED contract, never the transform scale ------------
	void InstallHumanBody()
	{
		// The PLAYER's body belongs to ZM_PlayerController::EnsureAndConfigureBody,
		// which installs these same dimensions immediately BEFORE its sensor/gravity/
		// lock/friction block. Installing them here as well would rebuild the body out
		// from under that configuration, for nothing.
		if (m_xParentEntity.TryGetComponent<ZM_PlayerController>() != nullptr)
		{
			return;
		}

		Zenith_ColliderComponent* pxCollider =
			m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
		if (pxCollider == nullptr || !pxCollider->HasValidBody())
		{
			return;   // nothing configured to re-shape; the setters would only warn
		}

		if (pxCollider->GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_CAPSULE)
		{
			pxCollider->SetExplicitCapsuleDimensions(
				fZM_HUMAN_BODY_CAPSULE_RADIUS, fZM_HUMAN_BODY_CAPSULE_HALF_CYLINDER);
			return;
		}
		pxCollider->SetExplicitBoxHalfExtents(Zenith_Maths::Vector3(
			fZM_HUMAN_BODY_FOOTPRINT * 0.5f,
			fZM_HUMAN_BODY_HALF_HEIGHT,
			fZM_HUMAN_BODY_FOOTPRINT * 0.5f));
	}

	// ---- The animator: Idle + Walk, driven off speed ------------------------
	void EnsureHumanAnimator(Flux_SkeletonInstance& xSkeleton)
	{
		Zenith_AnimatorComponent* pxAnimator =
			m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>();
		if (pxAnimator == nullptr)
		{
			pxAnimator = &m_xParentEntity.AddComponent<Zenith_AnimatorComponent>();
		}
		Flux_AnimationController& xController = pxAnimator->GetController();

		// ★ REBIND, EVERY TIME, BEFORE ANYTHING ELSE. LoadModel replaced the skeleton
		// INSTANCE and Zenith_AnimatorComponent::TryDiscoverSkeleton returns
		// immediately once the controller is initialised, so without this an already-
		// initialised controller would keep pointing at the destroyed one. Initialize
		// re-takes the skeleton asset handle and re-initialises every existing layer's
		// pose, so the clips, the layer and the state machine below all survive it.
		xController.Initialize(&xSkeleton);

		if (m_bAnimatorAuthored)
		{
			return;   // clips/layers/parameters are added exactly once per instance
		}

		// Clips are NOT auto-loaded from the .zmodel's animation list; they have to be
		// added by path. Both are the SHARED set every human binds.
		if (!AddSharedClip(xController, ZM_HUMAN_SHARED_ASSET_ANIM_IDLE)
			|| !AddSharedClip(xController, ZM_HUMAN_SHARED_ASSET_ANIM_WALK))
		{
			return;   // leave m_bAnimatorAuthored false so a later start can retry
		}

		Flux_AnimationLayer* pxLayer = xController.AddLayer("ZM_HumanBase");
		if (pxLayer == nullptr)
		{
			return;
		}
		pxLayer->SetWeight(1.0f);
		pxLayer->SetBlendMode(LAYER_BLEND_OVERRIDE);

		Flux_AnimationStateMachine* pxMachine =
			pxLayer->CreateStateMachine("ZM_HumanLocomotion");
		if (pxMachine == nullptr)
		{
			return;
		}
		Flux_AnimationClipCollection& xClips = xController.GetClipCollection();
		pxMachine->GetParameters().AddFloat("Speed", 0.0f);

		AddClipState(*pxMachine, xClips, "Idle", ZM_HumanClipName(ZM_HUMAN_CLIP_IDLE));
		AddClipState(*pxMachine, xClips, "Walk", ZM_HumanClipName(ZM_HUMAN_CLIP_WALK));

		AddSpeedTransition(*pxMachine, "Idle", "Walk",
			Flux_TransitionCondition::CompareOp::Greater, fZM_HUMAN_WALK_SPEED_THRESHOLD);
		AddSpeedTransition(*pxMachine, "Walk", "Idle",
			Flux_TransitionCondition::CompareOp::LessEqual, fZM_HUMAN_WALK_SPEED_THRESHOLD);

		m_bAnimatorAuthored = true;
	}

	static bool AddSharedClip(Flux_AnimationController& xController,
		ZM_HUMAN_SHARED_ASSET_KIND eKind)
	{
		char acRef[256];
		if (!ZM_HumanSharedAssetPath(eKind, acRef, static_cast<u_int>(sizeof(acRef))))
		{
			return false;
		}
		return xController.AddClipFromFile(
			Zenith_AssetRegistry::ResolvePath(std::string(acRef))) != nullptr;
	}

	static void AddClipState(Flux_AnimationStateMachine& xMachine,
		Flux_AnimationClipCollection& xClips, const char* szState, const char* szClip)
	{
		Flux_AnimationState* pxState = xMachine.AddState(szState);
		Flux_AnimationClip* pxClip = xClips.GetClip(szClip);
		if (pxState != nullptr && pxClip != nullptr)
		{
			pxState->SetBlendTree(new Flux_BlendTreeNode_Clip(pxClip, 1.0f));
		}
	}

	static void AddSpeedTransition(Flux_AnimationStateMachine& xMachine,
		const char* szFrom, const char* szTo,
		Flux_TransitionCondition::CompareOp eOp, float fThreshold)
	{
		Flux_AnimationState* pxFrom = xMachine.GetState(szFrom);
		if (pxFrom == nullptr)
		{
			return;
		}
		Flux_StateTransition xTransition;
		xTransition.m_strTargetStateName = szTo;
		xTransition.m_fTransitionDuration = fZM_HUMAN_LOCOMOTION_BLEND_SECONDS;

		Flux_TransitionCondition xCondition;
		xCondition.m_strParameterName = "Speed";
		xCondition.m_eParamType = Flux_AnimationParameters::ParamType::Float;
		xCondition.m_eCompareOp = eOp;
		xCondition.m_fThreshold = fThreshold;
		xTransition.m_xConditions.PushBack(xCondition);

		pxFrom->AddTransition(xTransition);
	}

	// ---- Shared block-mesh construction (BLOCKOUT and HUMAN_FALLBACK) --------
	// Both wear the name "ZM_Greybox" on purpose: it is the only handle a test TU
	// has on these materials (this class is file-local and cannot be named from
	// Tests/), and ZM_RivalVesperAuthored_Test uses it to find both populations.
	bool BuildBlockMesh(const Zenith_Maths::Vector4& xBaseColour)
	{
		m_xMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MeshGeometryAsset* pxGeometryAsset = m_xGeometry.GetDirect();
		Zenith_MaterialAsset* pxMaterial = m_xMaterial.GetDirect();
		if (pxGeometryAsset == nullptr || pxMaterial == nullptr)
		{
			return false;
		}

		pxMaterial->SetName("ZM_Greybox");
		ApplyAppearance(*pxMaterial, xBaseColour);
		pxMaterial->SetRoughness(0.90f);
		pxMaterial->SetMetallic(0.0f);

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}
		Flux_MeshGeometry* pxGeometry = pxGeometryAsset->GetGeometry();
		if (pxGeometry == nullptr)
		{
			return false;
		}
		pxModel->AddMeshEntry(*pxGeometry, *pxMaterial);
		return true;
	}

	// ZM-D-171: the ZM-D-169 NPC emissive floor is DELETED. With the engine's
	// energy-consistent sun + ground-bounce IBL the LIT palette carries readability
	// on its own.
	void ApplyAppearance(
		Zenith_MaterialAsset& xMaterial,
		const Zenith_Maths::Vector4& xBaseColour) const
	{
		xMaterial.SetBaseColor(xBaseColour);
		// Explicit zeros, not "leave alone": the re-run refresh path reuses a LIVE
		// material that may still carry the old floor's emission.
		xMaterial.SetEmissiveColor(Zenith_Maths::Vector3(0.0f));
		xMaterial.SetEmissiveIntensity(0.0f);
	}

	// ZM-D-176. The non-human answer is not a single colour: the seven PlayerHome
	// shell blocks wear a warm interior tint so the player's bedroom stops reading
	// as the same greybox room as ProfLab. Keyed on the ENTITY NAME, matched EXACTLY
	// against the same inventory the authoring loop walks
	// (Source/World/ZM_PlayerHomePlacement.h) -- one spelling, so a rename moves
	// both sides together. Nothing here is serialized.
	//
	// ★ WHY THE NAME IS SAFE TO READ AT ORDER 107. It is established by
	// AddStep_CreateEntity at authoring time (editor Stopped, no OnStart has fired)
	// and by scene deserialization before any pending start is drained -- the
	// identical argument the class comment makes for the NPC row.
	Zenith_Maths::Vector4 ResolveBlockoutColour() const
	{
		return ZM_IsPlayerHomeBlockName(m_xParentEntity.GetName().c_str())
			? ZM_GetPlayerHomeInteriorTintColour()
			: ZM_GetHumanPaletteFallbackColour();
	}

	Zenith_Entity m_xParentEntity;
	MeshGeometryHandle m_xGeometry;
	MaterialHandle m_xMaterial;
	// Runtime-only and NOT reset by ReadFromDataStream: they record what THIS
	// instance did to the model and the animator, which a stream read does not undo.
	ZM_VISUAL_KIND m_eLoadedKind = ZM_VISUAL_NONE;
	ZM_HUMAN_ID m_eLoadedHumanId = ZM_HUMAN_NONE;
	bool m_bAnimatorAuthored = false;
};

// ============================================================================
// Zenithmon -- Pokemon-style monster-collecting RPG (see Docs/ for the GDD,
// roadmap, and stage plan).
//
// The game component registers with the component-meta registry via the
// static-init macro (NOT a direct call from Project_RegisterGameComponents: the
// registry is sealed before that hook runs). Dead-strip safe: this TU defines
// the Project_* entry points the engine references, so its static initializers
// always run. Serialization orders: ZM components claim 100+.
// ============================================================================
ZENITH_REGISTER_COMPONENT(ZM_GameComponent, "ZM_Game", 100u)
ZENITH_REGISTER_COMPONENT(ZM_TerrainGrass, "ZM_TerrainGrass", 101u)
ZENITH_REGISTER_COMPONENT(ZM_PlayerController, "ZM_PlayerController", 102u)
ZENITH_REGISTER_COMPONENT(ZM_FollowCamera, "ZM_FollowCamera", 103u)
ZENITH_REGISTER_COMPONENT(ZM_GameStateManager, "ZM_GameStateManager", 104u)
ZENITH_REGISTER_COMPONENT(ZM_SpawnPoint, "ZM_SpawnPoint", 105u)
ZENITH_REGISTER_COMPONENT(ZM_WarpTrigger, "ZM_WarpTrigger", 106u)
ZENITH_REGISTER_COMPONENT(ZM_GreyboxVisual, "ZM_GreyboxVisual", 107u)
ZENITH_REGISTER_COMPONENT(ZM_BattleArena, "ZM_BattleArena", 108u)
ZENITH_REGISTER_COMPONENT(ZM_TallGrassSystem, "ZM_TallGrassSystem", 109u)
ZENITH_REGISTER_COMPONENT(ZM_BattleTransition, "ZM_BattleTransition", 110u)
ZENITH_REGISTER_COMPONENT(ZM_BattleDirector, "ZM_BattleDirector", 111u)
ZENITH_REGISTER_COMPONENT(ZM_UI_MenuStack, "ZM_UI_MenuStack", 112u)
ZENITH_REGISTER_COMPONENT(ZM_Interactable, "ZM_Interactable", 113u)

#ifdef ZENITH_TOOLS
namespace
{
	MaterialHandle g_axDawnmereTerrainMaterials[4];

	void ZM_ConfigureWarpFade()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"WarpFade authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		Zenith_UI::Zenith_UIElement* pxFade = pxUI->FindElement("WarpFade");
		Zenith_Assert(pxFade != nullptr
			&& pxFade->GetType() == Zenith_UI::UIElementType::Overlay,
			"WarpFade must be an Overlay on ZM_GameStateRoot");
		if (pxFade == nullptr
			|| pxFade->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return;
		}

		Zenith_UI::Zenith_UIOverlay* pxFadeOverlay =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxFade);
		pxFadeOverlay->SetContentSize(0.0f, 0.0f);
		pxFadeOverlay->SetAnchorAndPivot(
			Zenith_UI::AnchorPreset::StretchAll);
		pxFadeOverlay->SetSortOrder(10000);
		pxFadeOverlay->SetDimColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		pxFadeOverlay->SetFadeDuration(0.0f);
		pxFadeOverlay->SetGroupAlpha(0.0f);
		pxFadeOverlay->SetVisible(false);
	}

	// ZM_BattleTransition's OWN full-canvas fade, on its OWN persistent root. Sort
	// order 10001 puts it one above WarpFade's 10000, so the two overlays never
	// fight for the top of the canvas (ZM-D-097).
	void ZM_ConfigureBattleFade()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"BattleFade authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		Zenith_UI::Zenith_UIElement* pxFade = pxUI->FindElement("BattleFade");
		Zenith_Assert(pxFade != nullptr
			&& pxFade->GetType() == Zenith_UI::UIElementType::Overlay,
			"BattleFade must be an Overlay on ZM_BattleTransitionRoot");
		if (pxFade == nullptr
			|| pxFade->GetType() != Zenith_UI::UIElementType::Overlay)
		{
			return;
		}

		Zenith_UI::Zenith_UIOverlay* pxFadeOverlay =
			static_cast<Zenith_UI::Zenith_UIOverlay*>(pxFade);
		pxFadeOverlay->SetContentSize(0.0f, 0.0f);
		pxFadeOverlay->SetAnchorAndPivot(
			Zenith_UI::AnchorPreset::StretchAll);
		pxFadeOverlay->SetSortOrder(10001);
		pxFadeOverlay->SetDimColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		pxFadeOverlay->SetFadeDuration(0.0f);
		pxFadeOverlay->SetGroupAlpha(0.0f);
		pxFadeOverlay->SetVisible(false);
	}

	// The director-owned battle HUD (S5 item 4 SC4). Authors the five HUD elements on
	// the selected BattleDirector entity's UI component: sort order 10002 (one above
	// BattleFade's 10001, so the end-fade never clips them), sensible anchor / position
	// / size / font, and hidden (SetVisible(false)) -- ZM_BattleDirector shows them at
	// Setup and hides them at Hide. Element names are the ZM_UI_BattleHUD contract.
	void ZM_ConfigureBattleHUD()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"BattleHUD authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		// Shared placement: every HUD element sits above BattleFade (10001) and is
		// authored hidden. Anchor == pivot keeps the offset intuitive per corner.
		auto fnPlace = [](Zenith_UI::Zenith_UIElement* pxElement,
			Zenith_UI::AnchorPreset ePreset, float fX, float fY, float fW, float fH)
		{
			if (pxElement == nullptr)
			{
				return;
			}
			pxElement->SetSortOrder(10002);
			pxElement->SetAnchor(ePreset);
			pxElement->SetPivot(ePreset);
			pxElement->SetPosition(fX, fY);
			pxElement->SetSize(fW, fH);
			pxElement->SetVisible(false);
		};

		const Zenith_Maths::Vector4 xWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
		const Zenith_Maths::Vector4 xHpGreen = { 0.20f, 0.85f, 0.30f, 1.0f };

		// Battle text log -- bottom-centre, wide, centred, word-wrapped.
		Zenith_UI::Zenith_UIText* pxLog =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_Log");
		fnPlace(pxLog, Zenith_UI::AnchorPreset::BottomCenter, 0.0f, -48.0f, 900.0f, 72.0f);
		if (pxLog != nullptr)
		{
			pxLog->SetFontSize(30.0f);
			pxLog->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxLog->SetMaxWidth(900.0f);
			pxLog->SetColor(xWhite);
		}

		// Enemy active panel -- top-left, left-aligned; its HP bar just below.
		Zenith_UI::Zenith_UIText* pxEnemyPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_EnemyPanel");
		fnPlace(pxEnemyPanel, Zenith_UI::AnchorPreset::TopLeft, 40.0f, 36.0f, 320.0f, 32.0f);
		if (pxEnemyPanel != nullptr)
		{
			pxEnemyPanel->SetFontSize(24.0f);
			pxEnemyPanel->SetAlignment(Zenith_UI::TextAlignment::Left);
			pxEnemyPanel->SetColor(xWhite);
		}

		Zenith_UI::Zenith_UIRect* pxEnemyHpBar =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_EnemyHPBar");
		fnPlace(pxEnemyHpBar, Zenith_UI::AnchorPreset::TopLeft, 40.0f, 76.0f, 240.0f, 16.0f);
		if (pxEnemyHpBar != nullptr)
		{
			pxEnemyHpBar->SetFillDirection(Zenith_UI::FillDirection::LeftToRight);
			pxEnemyHpBar->SetColor(xHpGreen);
		}

		// Player active panel -- bottom-right, right-aligned; its HP bar just above.
		Zenith_UI::Zenith_UIText* pxPlayerPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>("BattleHUD_PlayerPanel");
		fnPlace(pxPlayerPanel, Zenith_UI::AnchorPreset::BottomRight, -40.0f, -120.0f, 320.0f, 32.0f);
		if (pxPlayerPanel != nullptr)
		{
			pxPlayerPanel->SetFontSize(24.0f);
			pxPlayerPanel->SetAlignment(Zenith_UI::TextAlignment::Right);
			pxPlayerPanel->SetColor(xWhite);
		}

		Zenith_UI::Zenith_UIRect* pxPlayerHpBar =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_PlayerHPBar");
		fnPlace(pxPlayerHpBar, Zenith_UI::AnchorPreset::BottomRight, -40.0f, -100.0f, 240.0f, 16.0f);
		if (pxPlayerHpBar != nullptr)
		{
			pxPlayerHpBar->SetFillDirection(Zenith_UI::FillDirection::LeftToRight);
			pxPlayerHpBar->SetColor(xHpGreen);
		}

		// --- Interactive battle menu (8 elements). Sort order 10003 sits above the SC4
		//     HUD (10002) so the menu reads on top; all authored hidden -- ZM_BattleDirector
		//     reveals/highlights/hides them via UpdateMenu/HideMenu. A bottom-right box:
		//     Fight/Catch/Run as a vertical stack (SC4 adds Catch), or a 2x2 move grid in
		//     its place (mutually exclusive screens, so they share the box). ---
		auto fnPlaceMenu = [](Zenith_UI::Zenith_UIElement* pxElement,
			Zenith_UI::AnchorPreset ePreset, float fX, float fY, float fW, float fH)
		{
			if (pxElement == nullptr)
			{
				return;
			}
			pxElement->SetSortOrder(10003);
			pxElement->SetAnchor(ePreset);
			pxElement->SetPivot(ePreset);
			pxElement->SetPosition(fX, fY);
			pxElement->SetSize(fW, fH);
			pxElement->SetVisible(false);
		};

		const Zenith_Maths::Vector4 xMenuPanelColour = { 0.05f, 0.06f, 0.10f, 0.85f };

		// Backing panel -- the box behind the buttons, bottom-right. Tall enough for the
		// 3-row root stack AND wide enough for the 2x2 move grid (they share the box).
		Zenith_UI::Zenith_UIRect* pxMenuPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>("BattleHUD_MenuPanel");
		fnPlaceMenu(pxMenuPanel, Zenith_UI::AnchorPreset::BottomRight, -24.0f, -24.0f, 388.0f, 184.0f);
		if (pxMenuPanel != nullptr)
		{
			pxMenuPanel->SetColor(xMenuPanelColour);
		}

		// Root actions -- a single vertical stack inside the panel, authored top to bottom
		// in Fight / Catch / Run ENTRY order. That is entry IDENTITY order, NOT cursor
		// order: ZM_BATTLE_MENU_FIGHT/CATCH/RUN are entry ids, and the Catch entry is
		// HIDDEN outright when the battle disallows catching (a trainer battle, or the
		// Battle Tower), which makes Run cursor index 1. A cursor is therefore resolved to
		// an entry through ZM_UI_BattleHUD::MenuRootItemAtIndex, never by reading this
		// layout -- and the presenter re-anchors each visible button to the row of its
		// resolved index, so hiding Catch closes the gap instead of leaving a hole. The
		// row Y values come from the SHARED constants for exactly that reason.
		auto fnRootRowY = [](int iRow)
		{
			return fZM_BATTLE_MENU_ROOT_FIRST_ROW_Y + fZM_BATTLE_MENU_ROOT_ROW_PITCH_Y * (float)iRow;
		};

		Zenith_UI::Zenith_UIButton* pxFight =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionFight");
		fnPlaceMenu(pxFight, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(0), 170.0f, 44.0f);
		if (pxFight != nullptr)
		{
			pxFight->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxCatch =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionCatch");
		fnPlaceMenu(pxCatch, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(1), 170.0f, 44.0f);
		if (pxCatch != nullptr)
		{
			pxCatch->SetFontSize(26.0f);
		}

		Zenith_UI::Zenith_UIButton* pxRun =
			pxUI->FindElement<Zenith_UI::Zenith_UIButton>("BattleHUD_ActionRun");
		fnPlaceMenu(pxRun, Zenith_UI::AnchorPreset::BottomRight, -133.0f, fnRootRowY(2), 170.0f, 44.0f);
		if (pxRun != nullptr)
		{
			pxRun->SetFontSize(26.0f);
		}

		// Move slots -- a 2x2 grid in the same box (shown only in MOVE_SELECT). The
		// director rewrites the labels to the active's move names each frame.
		struct MenuButtonPlace { const char* m_szName; float m_fX; float m_fY; };
		const MenuButtonPlace axMoves[4] =
		{
			{ "BattleHUD_Move0", -206.0f, -80.0f },
			{ "BattleHUD_Move1",  -30.0f, -80.0f },
			{ "BattleHUD_Move2", -206.0f, -32.0f },
			{ "BattleHUD_Move3",  -30.0f, -32.0f },
		};
		for (const MenuButtonPlace& xMove : axMoves)
		{
			Zenith_UI::Zenith_UIButton* pxMove =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xMove.m_szName);
			fnPlaceMenu(pxMove, Zenith_UI::AnchorPreset::BottomRight, xMove.m_fX, xMove.m_fY, 170.0f, 44.0f);
			if (pxMove != nullptr)
			{
				pxMove->SetFontSize(22.0f);
			}
		}
	}

	// The SC7 shop screen, authored WHOLE like the SC6 bag: a centred panel, the
	// mode/money/quantity header, six list rows and the eight controls in two bands
	// below them. Split out of ZM_ConfigureMenuRoot (which is already five screens long)
	// rather than inlined; it owns exactly the ZM_UI_Shop::sz*_NAME / RowElementName
	// contract and reads all of its geometry off the ZM_UI_Shop f*_ constants, so this
	// site and the presenter can never drift apart. ALL authored HIDDEN.
	void ZM_ConfigureMenuRootShopScreen(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Shop::szPANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the header band, the row stack and BOTH control bands, so nothing
			// the screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxPanel->SetSize(ZM_UI_Shop::fPANEL_WIDTH, ZM_UI_Shop::fPANEL_HEIGHT);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxHeader =
			xUI.FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Shop::szHEADER_NAME);
		if (pxHeader != nullptr)
		{
			pxHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPosition(0.0f, ZM_UI_Shop::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen). The header carries the transaction
			// report too, so it is authored two lines tall.
			pxHeader->SetSize(ZM_UI_Shop::fHEADER_WIDTH, ZM_UI_Shop::fHEADER_HEIGHT);
			pxHeader->SetFontSize(22.0f);
			pxHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxHeader->SetMaxWidth(ZM_UI_Shop::fHEADER_WIDTH);
			pxHeader->SetVisible(false);
		}

		// The rows carry NO explicit navigation links -- the party / bag idiom, and here it
		// is a CORRECTNESS requirement, not a preference: ZM_UI_Shop::Present hides every
		// row past the live entry count, and Zenith_UICanvas::NavigateDown only falls back
		// to the spatial search when the link is NULL. A bake-time link from the last LIVE
		// row into a row Present has just hidden would be fetched, fail the
		// visible+focusable test, and swallow the press -- dead navigation on every partial
		// page. Liveness is per-page runtime state, so it cannot be wired at bake time.
		for (u_int uRow = 0u; uRow < ZM_UI_Shop::uROWS_PER_PAGE; ++uRow)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Shop::RowElementName(uRow));
			if (pxRow == nullptr)
			{
				continue;
			}
			pxRow->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxRow->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPosition(0.0f,
				ZM_UI_Shop::fROW_FIRST_CENTRE_Y + ZM_UI_Shop::fROW_PITCH_Y * (float)uRow);
			pxRow->SetSize(ZM_UI_Shop::fROW_WIDTH, ZM_UI_Shop::fROW_HEIGHT);
			pxRow->SetFontSize(22.0f);
			pxRow->SetFocusable(true);
			pxRow->SetVisible(false);
		}

		// The eight controls, in two bands below the list and likewise unlinked. CONFIRM
		// sits ALONE at x == 0 in the first band, directly under the row column: the engine
		// scores spatial candidates on raw squared distance, so from any live row (they all
		// share x == 0) it is always the nearest element below -- ONE Down press reaches the
		// primary action even when the page holds a single row.
		struct ShopControl { const char* m_szName; float m_fX; float m_fY; };
		const ShopControl axShopControls[ZM_UI_Shop::uCONTROL_COUNT] =
		{
			{ ZM_UI_Shop::szBUY_TAB_NAME,   -ZM_UI_Shop::fCONTROL_OUTER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szSELL_TAB_NAME,  -ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szCONFIRM_NAME,                            0.0f, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szPREV_PAGE_NAME,  ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szNEXT_PAGE_NAME,  ZM_UI_Shop::fCONTROL_OUTER_X, ZM_UI_Shop::fCONTROL_BAND1_Y },
			{ ZM_UI_Shop::szQTY_DOWN_NAME,  -ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND2_Y },
			{ ZM_UI_Shop::szQTY_UP_NAME,                             0.0f, ZM_UI_Shop::fCONTROL_BAND2_Y },
			{ ZM_UI_Shop::szEXIT_NAME,       ZM_UI_Shop::fCONTROL_INNER_X, ZM_UI_Shop::fCONTROL_BAND2_Y },
		};
		for (const ShopControl& xControl : axShopControls)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(xControl.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xControl.m_fX, xControl.m_fY);
			pxButton->SetSize(ZM_UI_Shop::fCONTROL_WIDTH, ZM_UI_Shop::fCONTROL_HEIGHT);
			pxButton->SetFontSize(20.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}
	}

	// The S7 SC4 save/load slot screen, authored WHOLE like the bag/shop: a centred panel,
	// the mode header, four slot rows and a Back button. Every row stays VISIBLE + FOCUSABLE
	// regardless of status (RowIsAlwaysShown), so a row is disarmed by ResolveRowAction
	// returning NONE, never by hiding it. The rows and Back share one vertical column:
	// SetNavigation below helps only in the live authoring session because navigation links
	// are not serialized; loaded runtime scenes use the spatial fallback and reach the same
	// row0..row3..Back order. Same 9000/9001 sort band, ALL authored HIDDEN. The 420x300
	// panel spans y [-150,+150]; header, rows and Back all sit inside it (ZM-D-112).
	void ZM_ConfigureMenuRootSaveScreen(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_SaveSlots::szPANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			pxPanel->SetSize(420.0f, 300.0f);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxHeader =
			xUI.FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_SaveSlots::szHEADER_NAME);
		if (pxHeader != nullptr)
		{
			pxHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPosition(0.0f, -124.0f);
			// Size == wrap width == SetMaxWidth with a matching alignment (the SC2 lesson).
			pxHeader->SetSize(380.0f, 32.0f);
			pxHeader->SetFontSize(24.0f);
			pxHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxHeader->SetMaxWidth(380.0f);
			pxHeader->SetVisible(false);
		}

		// The four rows + Back form one vertical column. The explicit links mirror that geometry
		// for this live authoring session; they are not serialized, so runtime navigation uses
		// spatial fallback. The rows sit at a 52 px pitch; Back is the fifth element.
		const float afRowY[ZM_UI_SaveSlots::uROW_COUNT] = { -68.0f, -16.0f, 36.0f, 88.0f };
		Zenith_UI::Zenith_UIElement* apxNav[ZM_UI_SaveSlots::uROW_COUNT + 1u] = {};
		for (u_int uRow = 0u; uRow < ZM_UI_SaveSlots::uROW_COUNT; ++uRow)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_SaveSlots::RowElementName(uRow));
			apxNav[uRow] = pxRow;
			if (pxRow == nullptr)
			{
				continue;
			}
			pxRow->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxRow->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPosition(0.0f, afRowY[uRow]);
			pxRow->SetSize(380.0f, 44.0f);
			pxRow->SetFontSize(22.0f);
			pxRow->SetFocusable(true);
			pxRow->SetVisible(false);
		}

		Zenith_UI::Zenith_UIButton* pxCancel =
			xUI.FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_SaveSlots::szCANCEL_NAME);
		apxNav[ZM_UI_SaveSlots::uROW_COUNT] = pxCancel;
		if (pxCancel != nullptr)
		{
			pxCancel->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxCancel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxCancel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxCancel->SetPosition(0.0f, 126.0f);
			pxCancel->SetSize(200.0f, 32.0f);
			pxCancel->SetFontSize(20.0f);
			pxCancel->SetFocusable(true);
			pxCancel->SetVisible(false);
		}

		const u_int uNavCount = ZM_UI_SaveSlots::uROW_COUNT + 1u;
		for (u_int i = 0u; i < uNavCount; ++i)
		{
			if (apxNav[i] == nullptr)
			{
				continue;
			}
			Zenith_UI::Zenith_UIElement* pxUp   = (i > 0u) ? apxNav[i - 1u] : nullptr;
			Zenith_UI::Zenith_UIElement* pxDown = (i + 1u < uNavCount) ? apxNav[i + 1u] : nullptr;
			apxNav[i]->SetNavigation(pxUp, pxDown, nullptr, nullptr);
		}
	}

	// S7 SC5 FrontEnd title controls live on the persistent MenuRoot, not the
	// scene-owned GameManager that carries the large "Zenithmon" title text. The
	// automation steps create all three as canvas-owned root elements (AddElement);
	// ReparentElement below then moves the buttons under the panel without dropping
	// canvas ownership. AddChild-only would leak them from m_xAllElements.
	void ZM_ConfigureMenuRootTitleScreen(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_TitleMenu::szPANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 32.0f);
			pxPanel->SetSize(360.0f, 168.0f);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		struct TitleButton
		{
			const char* m_szName;
			float m_fY;
		};
		const TitleButton axButtons[2] =
		{
			{ ZM_UI_TitleMenu::szCONTINUE_NAME, -32.0f },
			{ ZM_UI_TitleMenu::szNEW_GAME_NAME,  32.0f },
		};
		for (const TitleButton& xButton : axButtons)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(xButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			if (pxPanel != nullptr)
			{
				xUI.GetCanvas().ReparentElement(pxButton, pxPanel);
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(0.0f, xButton.m_fY);
			pxButton->SetSize(280.0f, 48.0f);
			pxButton->SetFontSize(28.0f);
			pxButton->SetFocusable(false);
			pxButton->SetVisible(false);
			// Availability is runtime disk state. The presenter wires only live targets.
			pxButton->SetNavigation(nullptr, nullptr, nullptr, nullptr);
		}
	}

	// The overworld pause menu (S6 item 2 SC1). Authors the ROOT screen's backing panel
	// + Party/Bag/Dex/Save/Quit/Exit entries (Save/Quit added by S7 item 2 SC4, INSERTED
	// before Exit so the authored visual order matches ZM_MENU_ROOT_ITEM's enum order) on
	// the selected ZM_MenuRoot entity's UI component:
	// centred vertical stack, sort band 9000/9001 (BELOW WarpFade 10000 / BattleFade
	// 10001 so a fade always covers the menu), each entry focusable + navigation-wired
	// (up/down) for deterministic engine focus-nav, and ALL authored hidden --
	// ZM_UI_MenuStack shows/hides + focuses them at runtime. Element names are the
	// ZM_UI_MenuStack::sz*_NAME contract. SC2 adds the dialogue box (a bottom-centre
	// panel + wrapped text, also authored hidden) under the ZM_UI_DialogueBox::sz*_NAME
	// contract -- to which SC8 adds the two yes/no prompt buttons inside that same panel
	// -- SC4 the party screen (list panel + six slot rows + summary panel and
	// body, all hidden) under the ZM_UI_Party::sz*_NAME / SlotElementName contract, and
	// SC5 the dex screen's STATIC half (panel + completion header + two page buttons)
	// under the ZM_UI_Dex::sz*_NAME contract -- its grid is built at runtime, not here --
	// and SC6 the bag screen (panel + header + eight list rows + four nav buttons),
	// authored WHOLE under the ZM_UI_Bag::sz*_NAME / RowElementName contract. SC7's
	// shop screen (panel + header + six list rows + eight controls) is authored the same
	// way, in ZM_ConfigureMenuRootShopScreen above.
	void ZM_ConfigureMenuRoot()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"MenuRoot authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		// Backing panel -- centred box behind the entries, authored hidden.
		Zenith_UI::Zenith_UIRect* pxPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_MenuStack::szROOT_PANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			// 328 tall (was 232): SC4 grew the ROOT stack from four entries to six.
			pxPanel->SetSize(260.0f, 328.0f);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		// The six entries, top to bottom (== ZM_MENU_ROOT_PARTY..EXIT). 48 px pitch, centred,
		// focusable, authored hidden. THE VISUAL ORDER HERE MUST MATCH THE ENUM ORDER: the
		// focus walk in ZM_AutoTests_UI compares enum ordinals to decide UP vs DOWN, so an
		// entry placed out of enum order would make it oscillate to its deadline. The six span
		// [-120, +120] inside the 328-tall panel's [-164, +164].
		struct MenuEntry { ZM_MENU_ROOT_ITEM m_eItem; float m_fY; };
		const MenuEntry axEntries[ZM_MENU_ROOT_ITEM_COUNT] =
		{
			{ ZM_MENU_ROOT_PARTY, -120.0f },
			{ ZM_MENU_ROOT_BAG,    -72.0f },
			{ ZM_MENU_ROOT_DEX,    -24.0f },
			{ ZM_MENU_ROOT_SAVE,    24.0f },
			{ ZM_MENU_ROOT_QUIT,    72.0f },
			{ ZM_MENU_ROOT_EXIT,   120.0f },
		};
		Zenith_UI::Zenith_UIButton* apxButtons[ZM_MENU_ROOT_ITEM_COUNT] = {};
		for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(
					ZM_UI_MenuStack::RootItemElementName(axEntries[i].m_eItem));
			apxButtons[i] = pxButton;
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(0.0f, axEntries[i].m_fY);
			pxButton->SetSize(220.0f, 44.0f);
			pxButton->SetFontSize(26.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		// Explicit up/down navigation links (no wrap); left/right null. The engine's
		// focus-nav follows these first, falling back to the spatial search otherwise.
		for (u_int i = 0u; i < ZM_MENU_ROOT_ITEM_COUNT; ++i)
		{
			if (apxButtons[i] == nullptr)
			{
				continue;
			}
			Zenith_UI::Zenith_UIElement* pxUp   = (i > 0u) ? apxButtons[i - 1u] : nullptr;
			Zenith_UI::Zenith_UIElement* pxDown = (i + 1u < ZM_MENU_ROOT_ITEM_COUNT) ? apxButtons[i + 1u] : nullptr;
			apxButtons[i]->SetNavigation(pxUp, pxDown, nullptr, nullptr);
		}

		// The SC2 dialogue box: a wide bottom-centre panel + its wrapped text line, in
		// the same 9000/9001 sort band and likewise authored HIDDEN -- ZM_UI_DialogueBox
		// shows them while the DIALOGUE screen is on top. NOT focusable: the box advances
		// on confirm, never by focus-nav.
		Zenith_UI::Zenith_UIRect* pxDialoguePanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_DialogueBox::szPANEL_NAME);
		if (pxDialoguePanel != nullptr)
		{
			pxDialoguePanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxDialoguePanel->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialoguePanel->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialoguePanel->SetPosition(0.0f, -32.0f);
			pxDialoguePanel->SetSize(880.0f, 160.0f);
			pxDialoguePanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.90f });
			pxDialoguePanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxDialogueText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_DialogueBox::szTEXT_NAME);
		if (pxDialogueText != nullptr)
		{
			pxDialogueText->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxDialogueText->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialogueText->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxDialogueText->SetPosition(0.0f, -56.0f);
			// Size == the wrap width (the BattleHUD_Log idiom above): text is drawn inside
			// the element's bounds, so leaving the default 100x100 would start a Left-
			// aligned line at centre-50 and flow it clean off the right of the screen. 120
			// tall keeps the box inside the 160-tall panel at the -56 offset.
			pxDialogueText->SetSize(820.0f, 120.0f);
			pxDialogueText->SetFontSize(24.0f);
			pxDialogueText->SetAlignment(Zenith_UI::TextAlignment::Center);   // matches the BottomCenter anchor
			pxDialogueText->SetMaxWidth(820.0f);   // > 0 enables word wrap inside the panel
			pxDialogueText->SetVisible(false);
		}

		// The SC8 yes/no prompt buttons, side by side in the dialogue panel's lower-right
		// band (the geometry constants are ZM_UI_DialogueBox's, so this site and the
		// presenter can never drift). They sit INSIDE the 880x160 panel -- a question the
		// player answers must never float over the world (ZM-D-112) -- and BELOW where a
		// one/two-line prompt actually renders. Authored HIDDEN and NOT focusable:
		// ZM_UI_DialogueBox::Present raises + labels them only while a choice is awaiting an
		// answer, and turns both off again afterwards.
		//
		// NO SetNavigation between them, deliberately: the engine consults the explicit
		// link FIRST and only falls back to the spatial search when it is null, so a
		// bake-time link into a button Present has just hidden would swallow the press
		// outright (the SC6/SC7 rule). Side by side at the same Y, the spatial search walks
		// Yes <-> No on Left/Right with no links at all.
		struct DialogueChoiceButton { const char* m_szName; const char* m_szLabel; float m_fX; };
		const DialogueChoiceButton axChoiceButtons[2] =
		{
			{ ZM_UI_DialogueBox::szYES_NAME, "Yes", ZM_UI_DialogueBox::fCHOICE_YES_X },
			{ ZM_UI_DialogueBox::szNO_NAME,  "No",  ZM_UI_DialogueBox::fCHOICE_NO_X  },
		};
		for (const DialogueChoiceButton& xChoice : axChoiceButtons)
		{
			Zenith_UI::Zenith_UIButton* pxChoice =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xChoice.m_szName);
			if (pxChoice == nullptr)
			{
				continue;
			}
			pxChoice->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxChoice->SetAnchor(Zenith_UI::AnchorPreset::BottomCenter);
			pxChoice->SetPivot(Zenith_UI::AnchorPreset::BottomCenter);
			pxChoice->SetPosition(xChoice.m_fX, ZM_UI_DialogueBox::fCHOICE_Y);
			pxChoice->SetSize(ZM_UI_DialogueBox::fCHOICE_WIDTH, ZM_UI_DialogueBox::fCHOICE_HEIGHT);
			pxChoice->SetFontSize(20.0f);
			pxChoice->SetText(xChoice.m_szLabel);   // the arming caller overwrites this at runtime
			pxChoice->SetFocusable(false);
			pxChoice->SetVisible(false);
		}

		// The SC4 party screen: a centred list panel, six slot buttons stacked inside it,
		// and a summary panel + body text on top -- same 9000/9001 sort band, ALL authored
		// HIDDEN (ZM_UI_Party shows what the live party fills). The slots are focusable and
		// navigation-wired exactly like the ROOT entries; ZM_UI_Party turns the unfilled
		// ones back off at runtime so nav can never reach an empty slot.
		Zenith_UI::Zenith_UIRect* pxPartyPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szPANEL_NAME);
		if (pxPartyPanel != nullptr)
		{
			pxPartyPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPartyPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPartyPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPartyPanel->SetPosition(0.0f, 0.0f);
			pxPartyPanel->SetSize(560.0f, 360.0f);
			pxPartyPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPartyPanel->SetVisible(false);
		}

		// Six rows at a 52 px pitch, centred on the panel (5 gaps -> a 260 px span, so
		// +/-130 keeps the stack inside the 360-tall panel).
		Zenith_UI::Zenith_UIButton* apxPartySlots[ZM_UI_Party::uMAX_SLOTS] = {};
		for (u_int u = 0u; u < ZM_UI_Party::uMAX_SLOTS; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxSlot =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Party::SlotElementName(u));
			apxPartySlots[u] = pxSlot;
			if (pxSlot == nullptr)
			{
				continue;
			}
			pxSlot->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxSlot->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSlot->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSlot->SetPosition(0.0f, -130.0f + 52.0f * static_cast<float>(u));
			pxSlot->SetSize(500.0f, 44.0f);
			pxSlot->SetFontSize(22.0f);
			pxSlot->SetFocusable(true);
			pxSlot->SetVisible(false);
		}

		// DELIBERATELY NO SetNavigation links on the slot pool (unlike the ROOT entries,
		// whose four items are always visible). Slot LIVENESS is per-page runtime state:
		// ZM_UI_Party::Present hides + SetFocusable(false)s every slot past the party
		// count, and Zenith_UICanvas::NavigateDown consults the explicit link FIRST,
		// falling back to the spatial FindNearestFocusable only when that link is NULL --
		// a link into a slot that Present has just hidden therefore SWALLOWS the press
		// instead of degrading. The slots share x, so the spatial search walks the live
		// column correctly and re-reads liveness every frame. (SC6 hit exactly this on
		// the bag list, where it also blocked the walk down onto the nav band.) Bake-time
		// links are additionally NOT serialized by Zenith_UIElement::WriteToDataStream,
		// so they would only exist in tools builds -- another reason not to rely on them.

		Zenith_UI::Zenith_UIRect* pxSummaryPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Party::szSUMMARY_PANEL_NAME);
		if (pxSummaryPanel != nullptr)
		{
			// +2/+3 inside the SAME menu band (still far below the WarpFade 10000 /
			// BattleFade 10001 overlays): the summary OVERLAYS the list, and the slot
			// buttons sit at 9001, so a flat 9000/9001 pair would draw the rows straight
			// through it.
			pxSummaryPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER + 2);
			pxSummaryPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSummaryPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSummaryPanel->SetPosition(0.0f, 0.0f);
			// 340 tall, NOT 300: the slot stack spans y [-152,+152] (slot 0 sits at -130
			// with a 44-tall Center pivot, slot 5 at +130), so a 300-tall overlay would
			// leave the top 2 px of slot 0 and the bottom 2 px of slot 5 rendering OUTSIDE
			// it -- the exact bleed-through class the S5 visual gate (ZM-D-112) caught.
			// 340 still fits inside the 360-tall list panel.
			pxSummaryPanel->SetSize(520.0f, 340.0f);
			pxSummaryPanel->SetColor({ 0.08f, 0.09f, 0.14f, 0.95f });
			pxSummaryPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxSummaryText =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Party::szSUMMARY_TEXT_NAME);
		if (pxSummaryText != nullptr)
		{
			pxSummaryText->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER + 2);
			pxSummaryText->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxSummaryText->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxSummaryText->SetPosition(0.0f, 0.0f);
			// Size == the wrap width == SetMaxWidth, with a matching alignment: the SC2
			// lesson is that leaving the default 100x100 Left-aligned bounds flows the body
			// clean off the right of the screen. All three are set together, always.
			pxSummaryText->SetSize(470.0f, 260.0f);
			pxSummaryText->SetFontSize(20.0f);
			pxSummaryText->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxSummaryText->SetMaxWidth(470.0f);
			pxSummaryText->SetVisible(false);
		}

		// The SC5 dex screen: only the STATIC widgets are authored here (a centred panel,
		// the completion header and the two page buttons) -- the 5x6
		// Zenith_UIGridLayoutGroup and its 30 cells are built ONCE AT RUNTIME by
		// ZM_UI_Dex::Present, because the engine exposes no CreateGridLayoutGroup /
		// AddStep_* for a grid and adding one is out of SC5's scope. Same 9000/9001 sort
		// band, ALL authored HIDDEN. Geometry comes from the ZM_UI_Dex f*_ constants so
		// this site and the runtime grid build can never drift apart.
		Zenith_UI::Zenith_UIRect* pxDexPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Dex::szPANEL_NAME);
		if (pxDexPanel != nullptr)
		{
			pxDexPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxDexPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxDexPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxDexPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the grid (912x270 at +10) plus the header and page-button bands,
			// so nothing the screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxDexPanel->SetSize(ZM_UI_Dex::fPANEL_WIDTH, ZM_UI_Dex::fPANEL_HEIGHT);
			pxDexPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxDexPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxDexHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Dex::szHEADER_NAME);
		if (pxDexHeader != nullptr)
		{
			pxDexHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxDexHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxDexHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxDexHeader->SetPosition(0.0f, ZM_UI_Dex::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen).
			pxDexHeader->SetSize(ZM_UI_Dex::fHEADER_WIDTH, ZM_UI_Dex::fHEADER_HEIGHT);
			pxDexHeader->SetFontSize(26.0f);
			pxDexHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxDexHeader->SetMaxWidth(ZM_UI_Dex::fHEADER_WIDTH);
			pxDexHeader->SetVisible(false);
		}

		// The two page buttons sit BELOW the grid and carry NO explicit navigation links:
		// the engine's spatial focus-nav walks down off the last grid row onto them and
		// back up again, which is the whole point of using a grid.
		struct DexPageButton { const char* m_szName; float m_fX; };
		const DexPageButton axDexPageButtons[2] =
		{
			{ ZM_UI_Dex::szPREV_NAME, -ZM_UI_Dex::fPAGE_BUTTON_CENTRE_X },
			{ ZM_UI_Dex::szNEXT_NAME,  ZM_UI_Dex::fPAGE_BUTTON_CENTRE_X },
		};
		for (const DexPageButton& xPageButton : axDexPageButtons)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xPageButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xPageButton.m_fX, ZM_UI_Dex::fPAGE_BUTTON_CENTRE_Y);
			pxButton->SetSize(ZM_UI_Dex::fPAGE_BUTTON_WIDTH, ZM_UI_Dex::fPAGE_BUTTON_HEIGHT);
			pxButton->SetFontSize(22.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		// The SC6 bag screen, authored WHOLE (unlike the dex): a centred panel, the
		// pocket/money header, eight list rows stacked inside it and four nav buttons
		// below them. It is a 1-D LIST, so there is nothing a Zenith_UIGridLayoutGroup
		// would buy -- the rows are a plain authored pool like the party slots, with no
		// runtime construction and none of SC5's reparenting ownership hazard. Same
		// 9000/9001 sort band, ALL authored HIDDEN; geometry comes from the ZM_UI_Bag
		// f*_ constants so this site and the presenter can never drift apart.
		Zenith_UI::Zenith_UIRect* pxBagPanel =
			pxUI->FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_Bag::szPANEL_NAME);
		if (pxBagPanel != nullptr)
		{
			pxBagPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxBagPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxBagPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxBagPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the header band, the row stack and the nav band, so nothing the
			// screen draws bleeds outside the panel it sits on (ZM-D-112).
			pxBagPanel->SetSize(ZM_UI_Bag::fPANEL_WIDTH, ZM_UI_Bag::fPANEL_HEIGHT);
			pxBagPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxBagPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxBagHeader =
			pxUI->FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_Bag::szHEADER_NAME);
		if (pxBagHeader != nullptr)
		{
			pxBagHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxBagHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxBagHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxBagHeader->SetPosition(0.0f, ZM_UI_Bag::fHEADER_CENTRE_Y);
			// Size == the wrap width == SetMaxWidth, with a matching alignment -- all three
			// together, always (the SC2 lesson: the default 100x100 Left-aligned bounds flow
			// the line clean off the right of the screen).
			pxBagHeader->SetSize(ZM_UI_Bag::fHEADER_WIDTH, ZM_UI_Bag::fHEADER_HEIGHT);
			pxBagHeader->SetFontSize(26.0f);
			pxBagHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxBagHeader->SetMaxWidth(ZM_UI_Bag::fHEADER_WIDTH);
			pxBagHeader->SetVisible(false);
		}

		// The rows carry NO explicit navigation links -- the dex idiom, and here it is a
		// CORRECTNESS requirement, not a preference: ZM_UI_Bag::Present hides every row past
		// the live stack count, and Zenith_UICanvas::NavigateDown only falls back to the
		// spatial search when the link is NULL. A bake-time link from the last LIVE row to a
		// row Present has just hidden would be fetched, fail the visible+focusable test, and
		// swallow the press -- so on every partial page (the starter BALL pocket holds ONE
		// stack) Down would be dead. Liveness is per-page runtime state, so it cannot be
		// wired at bake time; the spatial search reads it correctly every frame because it
		// collects only visible + focusable elements.
		for (u_int u = 0u; u < ZM_UI_Bag::uROWS_PER_PAGE; ++u)
		{
			Zenith_UI::Zenith_UIButton* pxRow =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_Bag::RowElementName(u));
			if (pxRow == nullptr)
			{
				continue;
			}
			pxRow->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxRow->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxRow->SetPosition(0.0f,
				ZM_UI_Bag::fROW_FIRST_CENTRE_Y + ZM_UI_Bag::fROW_PITCH_Y * static_cast<float>(u));
			pxRow->SetSize(ZM_UI_Bag::fROW_WIDTH, ZM_UI_Bag::fROW_HEIGHT);
			pxRow->SetFontSize(22.0f);
			pxRow->SetFocusable(true);
			pxRow->SetVisible(false);
		}

		// The four nav buttons sit BELOW the rows and carry no explicit links either: the
		// rows all share x == 0, so the spatial search walks the live column vertically and,
		// off the LAST LIVE row, onto this band (and back up again). Pocket pair on the
		// left, page pair on the right.
		struct BagNavButton { const char* m_szName; float m_fX; };
		const BagNavButton axBagNavButtons[4] =
		{
			{ ZM_UI_Bag::szPREV_POCKET_NAME, -ZM_UI_Bag::fNAV_BUTTON_OUTER_X },
			{ ZM_UI_Bag::szNEXT_POCKET_NAME, -ZM_UI_Bag::fNAV_BUTTON_INNER_X },
			{ ZM_UI_Bag::szPREV_PAGE_NAME,    ZM_UI_Bag::fNAV_BUTTON_INNER_X },
			{ ZM_UI_Bag::szNEXT_PAGE_NAME,    ZM_UI_Bag::fNAV_BUTTON_OUTER_X },
		};
		for (const BagNavButton& xNavButton : axBagNavButtons)
		{
			Zenith_UI::Zenith_UIButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIButton>(xNavButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxButton->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxButton->SetPosition(xNavButton.m_fX, ZM_UI_Bag::fNAV_BUTTON_CENTRE_Y);
			pxButton->SetSize(ZM_UI_Bag::fNAV_BUTTON_WIDTH, ZM_UI_Bag::fNAV_BUTTON_HEIGHT);
			pxButton->SetFontSize(20.0f);
			pxButton->SetFocusable(true);
			pxButton->SetVisible(false);
		}

		ZM_ConfigureMenuRootShopScreen(*pxUI);
		ZM_ConfigureMenuRootSaveScreen(*pxUI);
		ZM_ConfigureMenuRootTitleScreen(*pxUI);
	}

	bool ZM_SetSelectedSpawnPointTag(const char* szTag)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_SpawnPoint* pxSpawnPoint = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_SpawnPoint>()
			: nullptr;
		Zenith_Assert(pxSpawnPoint != nullptr,
			"Spawn authoring requires the selected ZM_SpawnPoint");
		return pxSpawnPoint != nullptr && pxSpawnPoint->SetTag(szTag);
	}

	bool ZM_ConfigureSelectedWarpTrigger(
		u_int uTargetBuildIndex, const char* szSpawnTag)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_WarpTrigger* pxWarpTrigger = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_WarpTrigger>()
			: nullptr;
		Zenith_Assert(pxWarpTrigger != nullptr,
			"Warp authoring requires the selected ZM_WarpTrigger");
		return pxWarpTrigger != nullptr
			&& pxWarpTrigger->Configure(uTargetBuildIndex, szSpawnTag);
	}

	void ZM_ConfigureTownCenterSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag("TownCenter");
		Zenith_Assert(bTagSet, "TownCenter is not a valid spawn tag");
	}

	// Points Dawnmere's holder entity at the COMMITTED .znavmesh. The ref is the
	// only thing that serializes, so this one call is what makes every future
	// load of Dawnmere.zscen -- windowed, headless or packaged -- come up with a
	// live navmesh.
	void ZM_ConfigureDawnmereNavMesh()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_NavMeshComponent* pxNavMesh = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_NavMeshComponent>()
			: nullptr;
		Zenith_Assert(pxNavMesh != nullptr,
			"Dawnmere navmesh authoring requires the selected Zenith_NavMeshComponent "
			"(is \"NavMesh\" mirrored into the editor component registry?)");
		if (pxNavMesh == nullptr)
		{
			return;
		}

		// SetAssetRef loads immediately, so authoring proves the committed asset
		// is loadable rather than serializing a ref nobody has tried.
		const bool bLoaded = pxNavMesh->SetAssetRef(szZM_DAWNMERE_NAVMESH_ASSET_REF);
		Zenith_Assert(bLoaded, "Dawnmere navmesh ref did not load: %s", szZM_DAWNMERE_NAVMESH_ASSET_REF);
	}

	void ZM_ConfigureDoorSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag("Door"),
			"Door is not a valid spawn tag");
	}

	void ZM_ConfigureFromHomeSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag("FromHome"),
			"FromHome is not a valid spawn tag");
	}

	void ZM_ConfigurePlayerHomeExitTrigger()
	{
		Zenith_Assert(ZM_ConfigureSelectedWarpTrigger(2u, "FromHome"),
			"PlayerHome exit warp configuration is invalid");
	}

	void ZM_ConfigureHomeDoorTrigger()
	{
		Zenith_Assert(ZM_ConfigureSelectedWarpTrigger(40u, "Door"),
			"Dawnmere Home doorway warp configuration is invalid");
	}

	// ProfLab's arrival marker. A SEPARATE function from ZM_ConfigureDoorSpawnPoint
	// above even though both install "Door": that one is named for PlayerHome and
	// spells its tag as a literal, whereas this one reads the tag ProfLab's shared
	// placement header mirrors from the compiled world table, so a table rename
	// cannot leave this authoring behind. AddStep_Custom takes a captureless
	// void (*)(), so one function per configured entity is unavoidable regardless.
	void ZM_ConfigureProfLabDoorSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag(szZM_PROFLAB_SPAWN_TAG),
			"ProfLab arrival spawn tag is not a valid spawn tag");
	}

	// ---- S6 item 3 SC5: the authored Dawnmere NPCs ---------------------------
	//
	// Reach BONUS authored onto every Dawnmere NPC. 0.4 is this NPC's OWN AABB
	// half-width (the greybox body is 0.8 m wide), so the global 2.5 m reach is not
	// silently spent crossing the NPC's own body. Note the player capsule adds a
	// further 0.4 m of its own radius, so contact actually happens at ~0.8 m from
	// the NPC's transform centre against a 2.9 m effective reach -- do NOT read 0.4
	// as "the distance the capsule stops at". Well inside
	// ZM_Interactable::fMAX_RADIUS (8.0), and far too small to let one NPC swallow a
	// neighbour's press: the closest NPC PAIR in this town is 16.1 m apart, 5.5x the
	// effective reach.
	constexpr float fZM_NPC_AUTHORED_RADIUS = 0.4f;

	// The shared body of the stationary configure functions below. A PER-NPC function is
	// unavoidable: AddStep_Custom takes a captureless `void (*)()`, so there is no
	// way to hand one parameterised step the row it should install.
	bool ZM_ConfigureSelectedNpc(ZM_NPC_ID eId)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_Interactable* pxInteractable = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_Interactable>()
			: nullptr;
		Zenith_Assert(pxInteractable != nullptr,
			"NPC authoring requires the selected ZM_Interactable");
		if (pxInteractable == nullptr)
		{
			return false;
		}

		// SetInteractable LAST, deliberately: SetNpcId fails CLOSED by clearing the
		// candidacy flag, so arming before the row is installed would be silently
		// undone by a bad id and the NPC would author itself mute.
		const bool bIdSet = pxInteractable->SetNpcId(eId);
		const bool bRadiusSet = pxInteractable->SetRadius(fZM_NPC_AUTHORED_RADIUS);
		pxInteractable->SetInteractable(true);
		// IsInteractable() is the LIVE candidacy answer the picker reads, so assert on
		// it rather than on the setters alone -- that is the property authoring owes.
		return bIdSet && bRadiusSet && pxInteractable->IsInteractable();
	}

	void ZM_ConfigureVillagerNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_VILLAGER),
			"Dawnmere Villager NPC authoring is invalid");
	}

	void ZM_ConfigureTradePostClerkNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_TRADE_POST_CLERK),
			"Dawnmere Trade Post clerk NPC authoring is invalid");
	}

	void ZM_ConfigureCaretakerNpc()
	{
		Zenith_Assert(ZM_ConfigureSelectedNpc(ZM_NPC_CARETAKER),
			"Dawnmere Caretaker NPC authoring is invalid");
	}

	// S7 item 2 SC1: the story-gated NPC. Nothing about the AUTHORING differs from
	// the other stationary talkers -- the gate lives entirely in the compiled row
	// (ZM_NpcData.cpp), so a gated NPC costs no extra authoring step and no extra
	// component state.
	void ZM_ConfigureRouteWardenNpc()
	{
		const bool bConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_ROUTE_WARDEN);
		Zenith_Assert(bConfigured, "Dawnmere Route Warden NPC authoring is invalid");
	}

	// S7 item 3 SC8: the authored rival. The trainer identity is NOT installed here
	// -- it is DERIVED at load from his ZM_NpcData row by
	// ZM_Interactable::DeriveTrainerFromNpcRow, which is the whole point of the
	// zero-byte persistence route. Authoring only has to stand him up and point him.
	void ZM_ConfigureRivalVesperNpc()
	{
		const bool bConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_RIVAL_VESPER);
		Zenith_Assert(bConfigured, "Dawnmere rival Vesper NPC authoring is invalid");
		// The row really is the one that names a trainer -- authoring fails loudly
		// rather than standing up a rival who can never battle.
		Zenith_Assert(
			ZM_GetNpcData(ZM_NPC_RIVAL_VESPER).m_eTrainer == ZM_TRAINER_RIVAL_VESPER,
			"Vesper's ZM_NpcData row no longer names ZM_TRAINER_RIVAL_VESPER");

		// ★ THE SC7 BINDING, PINNED AT THE ONLY DECIDABLE MOMENT. The challenge
		// .bgraph is attached at RUNTIME by EnsureTrainerChallengeGraph, an
		// OnUpdate-only path, and that call is idempotent BY PATH -- so a RUNTIME
		// observation can never tell an authored slot apart from the runtime attach
		// (both yield GetGraphCount() == 1). This assert runs with the editor
		// STOPPED, in the very boot that calls AddStep_SaveScene, which is the one
		// instant where "no graph component exists on this entity" is observable.
		// If it ever fires, someone added AddStep_AttachGraph and an order-60 graph
		// payload is about to land in the committed scene bytes.
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxSelectedEntity != nullptr
			&& pxSelectedEntity->TryGetComponent<Zenith_GraphComponent>() == nullptr,
			"the authored rival must carry NO graph slot -- SC7 attaches the "
			"challenge graph at runtime (ZM_Interactable::EnsureTrainerChallengeGraph, "
			"declared in ZM_Interactable.h)");
	}

	// The scene-byte guard below is about ULPs, so it compares BIT PATTERNS. A
	// tolerance comparison -- even a zero one -- cannot express "these are the same
	// bits", and a 1-ULP yaw drift is invisible to every dot-product assertion in the
	// suite (1 - |dot| lands around 1e-14).
	u_int ZM_FloatBits(float fValue)
	{
		u_int uBits = 0u;
		std::memcpy(&uBits, &fValue, sizeof(uBits));
		return uBits;
	}

	bool ZM_FloatBitsEqual(float fA, float fB)
	{
		return ZM_FloatBits(fA) == ZM_FloatBits(fB);
	}

	// ★ THE LAST STEP BEFORE Dawnmere IS SAVED, AND IT EXISTS BECAUSE THIS EXACT
	// PROPERTY BROKE ONCE (Q-2026-08-01-002 / ZM-D-179).
	//
	// The rival is the only authored entity in this game carrying a NON-IDENTITY
	// rotation, and ZM_DawnmerePlacement.h's contract is that the committed bytes are
	// reproducible from COMPILED constants. They stopped being: the commit at
	// a6c66b68 shipped a quaternion 1 ULP off in y and ~10 in w from
	// ZM_DawnmereVesperFacing(), a value no other boot of the same source reproduces,
	// so every windowed boot since has left Dawnmere.zscen dirty in git status -- and
	// a permanently dirty tracked scene is a permanently disabled tripwire.
	//
	// The cause was that Zenith_TransformComponent serialized the LIVE JOLT BODY's
	// rotation rather than the authored one (ZM-D-179 fixed that engine-side). This
	// step is the game-side proof that the fix holds, and it is deliberately NOT a
	// GetRotation() comparison: it runs the REAL serializer into a scratch stream and
	// reads back the exact bytes AddStep_SaveScene is about to write. A boot unit
	// cannot do this -- units run before any scene is authored, and the damage lives
	// in the saved bytes, which is the ZM-D-156 lesson in its original form.
	void ZM_VerifyAuthoredRivalFacingStep()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_Assert(pxSelectedEntity != nullptr,
			"the rival facing guard needs Npc_RivalVesper selected");
		if (pxSelectedEntity == nullptr)
		{
			return;
		}

		Zenith_DataStream xScratch;
		pxSelectedEntity->GetComponent<Zenith_TransformComponent>()
			.WriteToDataStream(xScratch);
		xScratch.SetCursor(0u);
		Zenith_Maths::Vector3 xSerialisedPosition;
		Zenith_Maths::Quat xSerialisedRotation;
		xScratch >> xSerialisedPosition;
		xScratch >> xSerialisedRotation;

		const Zenith_Maths::Quat xAuthored = ZM_DawnmereVesperFacing();

		// The LIVE BODY's rotation is logged beside the serialized one because the two
		// were the same value before ZM-D-179 and are now allowed to differ sub-epsilon.
		// Anyone re-opening Q-2026-08-01-002 wants to know whether the divergence this
		// commit made harmless is dormant or live on their machine, and the answer is
		// one grep away rather than another day of archaeology.
		Zenith_Maths::Quat xLiveBodyRotation;
		pxSelectedEntity->GetComponent<Zenith_TransformComponent>()
			.GetRotation(xLiveBodyRotation);
		Zenith_Log(LOG_CATEGORY_EDITOR,
			"[ZM Authoring] Npc_RivalVesper rotation: authored=(%08X %08X %08X %08X) "
			"serialised=(%08X %08X %08X %08X) liveBody=(%08X %08X %08X %08X)",
			ZM_FloatBits(xAuthored.x), ZM_FloatBits(xAuthored.y),
			ZM_FloatBits(xAuthored.z), ZM_FloatBits(xAuthored.w),
			ZM_FloatBits(xSerialisedRotation.x), ZM_FloatBits(xSerialisedRotation.y),
			ZM_FloatBits(xSerialisedRotation.z), ZM_FloatBits(xSerialisedRotation.w),
			ZM_FloatBits(xLiveBodyRotation.x), ZM_FloatBits(xLiveBodyRotation.y),
			ZM_FloatBits(xLiveBodyRotation.z), ZM_FloatBits(xLiveBodyRotation.w));

		const bool bBitExact =
			ZM_FloatBitsEqual(xSerialisedRotation.x, xAuthored.x)
			&& ZM_FloatBitsEqual(xSerialisedRotation.y, xAuthored.y)
			&& ZM_FloatBitsEqual(xSerialisedRotation.z, xAuthored.z)
			&& ZM_FloatBitsEqual(xSerialisedRotation.w, xAuthored.w);
		Zenith_Assert(bBitExact,
			"Dawnmere is about to be saved with a rival rotation that is NOT "
			"ZM_DawnmereVesperFacing(): serialised (%08X %08X %08X %08X) vs authored "
			"(%08X %08X %08X %08X). See Q-2026-08-01-002 -- do NOT re-commit the "
			"scene until this is understood.",
			ZM_FloatBits(xSerialisedRotation.x), ZM_FloatBits(xSerialisedRotation.y),
			ZM_FloatBits(xSerialisedRotation.z), ZM_FloatBits(xSerialisedRotation.w),
			ZM_FloatBits(xAuthored.x), ZM_FloatBits(xAuthored.y),
			ZM_FloatBits(xAuthored.z), ZM_FloatBits(xAuthored.w));
	}

	// The ONE authored TRANSFORM SCALE every Dawnmere human wears -- the player and
	// all six NPCs. Named here, rather than re-spelled, because it is written from a
	// DIFFERENT function than the placement block below.
	//
	// ★ IT IS A DRAWING SCALE, NOT A BODY. It exists to land the generated human
	// MODEL on the body contract, and it is UNIFORM -- which is precisely why the
	// bodies can no longer be derived from it (a uniform scale degenerates a
	// scale-derived capsule into a sphere). Anything that needs to know how big a
	// person is reads Source/World/ZM_HumanBody.h; the bodies themselves are
	// installed explicitly from that same contract at runtime.
	const Zenith_Maths::Vector3 g_xDawnmereHumanScale(fZM_HUMAN_VISUAL_SCALE);

	// KNOWN-LIMIT W5. The authored CENTRE of one Dawnmere NPC: the shared anchor's
	// XZ plus that NPC's OWN measured feet height, lifted by the capsule half-extent.
	//
	// ★ IT IS CALLED AT PLAN-BUILD TIME, INSIDE THE AddStep_SetTransformPosition
	// ARGUMENT, AND THAT IS LOAD-BEARING. Sampling a height and then writing it back
	// with Zenith_TransformComponent::SetPosition AFTER AddStep_AddCollider would be
	// write-through to the body and would fire the pose-changed hook, whose
	// SyncPhysicsPoseAndInvalidate copies position AND ROTATION back -- the exact
	// mechanism (ZM-D-156) by which an AABB collider wiped rival Vesper's authored
	// yaw out of the SAVED BYTES while every boot unit stayed green. Keeping the
	// change to ONE FLOAT PER EXISTING CALL SITE makes that whole class of defect
	// unreachable by construction. Never grow a post-collider transform write here.
	Zenith_Maths::Vector3 ZM_DawnmereNpcAuthoredCenter(
		u_int uNpc, float fCapsuleHalfExtent)
	{
		const ZM_DawnmereNpcAnchor& xAnchor = ZM_GetDawnmereNpcAnchor(uNpc);
		return Zenith_Maths::Vector3(
			xAnchor.m_fX,
			ZM_DawnmereNpcCentreY(uNpc, fCapsuleHalfExtent),
			xAnchor.m_fZ);
	}

	void ZM_ConfigureWandererNpc()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_Interactable* pxInteractable = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_Interactable>()
			: nullptr;
		Zenith_Assert(pxInteractable != nullptr,
			"Wanderer authoring requires the selected ZM_Interactable");
		if (pxInteractable == nullptr)
		{
			return;
		}

		// A north/south loop at x=540 stays 28 m east of TownCenter spawn, beyond
		// every existing straight-line traversal corridor and all stationary NPCs.
		// Waypoint Y is serialized only as an authored reference: ZM_StepWalker is
		// explicitly XZ-only, while the dynamic capsule owns Y and follows terrain.
		//
		// KNOWN-LIMIT W5: both endpoints now come from the shared wander-waypoint
		// table (Source/World/ZM_DawnmerePlacement.h), whose feet heights are the
		// MEASURED terrain surface at each endpoint rather than the town centre's.
		// Endpoint 0 shares the wanderer's own anchor row by construction.
		Zenith_Assert(ZM_GetDawnmereWanderWaypointCount() == 2u,
			"the authored patrol is written as exactly two endpoints here; the shared "
			"waypoint table has grown or shrunk");
		const ZM_DawnmereNpcAnchor& xWaypoint0 = ZM_GetDawnmereWanderWaypoint(0u);
		const ZM_DawnmereNpcAnchor& xWaypoint1 = ZM_GetDawnmereWanderWaypoint(1u);

		ZM_WalkerWaypoints xWaypoints{};
		xWaypoints.m_uCount = 2u;
		xWaypoints.m_axPoints[0] = {
			xWaypoint0.m_fX, xWaypoint0.m_fFeetY + fZM_HUMAN_BODY_HALF_HEIGHT, xWaypoint0.m_fZ };
		xWaypoints.m_axPoints[1] = {
			xWaypoint1.m_fX, xWaypoint1.m_fFeetY + fZM_HUMAN_BODY_HALF_HEIGHT, xWaypoint1.m_fZ };

		const bool bNpcConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_WANDERER);
		const bool bPatrolConfigured =
			pxInteractable->ConfigureWander(xWaypoints, ZM_WalkerTuning{});
		Zenith_Assert(bNpcConfigured && bPatrolConfigured
			&& pxInteractable->IsWanderEnabled()
			&& pxInteractable->GetWaypointCount() == 2u,
			"Dawnmere Wanderer NPC authoring is invalid");
	}

	// One authored NPC: a greybox body the player can SEE, a STATIC AABB it can
	// physically bump into (so walking up to one ends in contact rather than in
	// walking through it), the ZM_Interactable that makes it talkable, and the
	// captureless step that installs its row. Step order mirrors HomeDoorTrigger --
	// transform, collider, components, then the configure custom step.
	void ZM_QueueDawnmereNpc(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale,
		void (*pfnConfigure)())
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		xAuto.AddStep_Custom(pfnConfigure);
	}

	// The PATROLLING NPC intentionally does NOT reuse the stationary helper: the
	// authored body contract is a solid dynamic capsule, driven only by XZ velocity.
	//
	// It was SC8's only moving NPC; S7 item 1 SC3 gave rival Vesper the same body
	// class for the walk-up (see ZM_QueueDawnmereTrainerNpc below), so "the one
	// dynamic body in Dawnmere" is no longer true. The two are still authored by
	// separate functions on purpose -- the wanderer needs an extra half-extent of
	// spawn clearance and no yaw, the rival needs an authored yaw and no waypoints --
	// and merging them would trade two readable step lists for one parameterised one
	// in the single file whose output is a COMMITTED asset.
	void ZM_QueueDawnmereWanderer(
		Zenith_EditorAutomation& xAuto,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale)
	{
		xAuto.AddStep_CreateEntity("Npc_Wanderer");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		xAuto.AddStep_Custom(&ZM_ConfigureWandererNpc);
	}

	// SC8's authored TRAINER deliberately does NOT reuse the stationary helper, for
	// the same reason ZM_QueueDawnmereWanderer does not: he needs a step the other
	// four do not have. The sight cone is a 60-degree FORWARD cone and an unrotated
	// entity faces +Z, so a trainer without an authored yaw stares north and is
	// functionally blind.
	//
	// Adding a yaw parameter to ZM_QueueDawnmereNpc instead was considered and
	// REJECTED. AddStep_SetTransformYaw(0.0f) does build an exact identity
	// quaternion, so the four shipped NPCs' bytes probably would not move -- but SC8
	// is the one sub-commit that rewrites a committed scene file, and their step
	// lists must be untouchable BY CONSTRUCTION rather than by an argument about
	// angleAxis(0). The yaw step sits between scale and collider so the transform is
	// fully authored before the body is created.
	//
	// ★ THE COLLIDER IS A DYNAMIC CAPSULE, AND **AABB REMAINS FORBIDDEN HERE
	// FOREVER**. Two separate reasons now stack on this one line; neither replaces
	// the other, and both have already been paid for once.
	//
	// (1) AABB DESTROYS THE AUTHORED YAW -- the ZM-D-156 lesson, UNCHANGED and still
	// true. Zenith_ColliderComponent's body creation reads
	//     const JPH::Quat xJoltRot = (eVolumeType == COLLISION_VOLUME_TYPE_AABB)
	//         ? JPH::Quat::sIdentity() : JPH::Quat(...);
	// -- an AABB is axis-aligned BY DEFINITION, so it forces the body to identity and
	// the physics-to-transform sync then writes that identity straight back over the
	// yaw this function just authored. The scene saves with an unrotated rival who
	// stares north and is functionally blind, WITH EVERY BOOT UNIT STILL GREEN,
	// because the units reason about the COMPILED placement constants while the
	// defect lives in the SAVED BYTES. Only the windowed round trip catches it, and
	// it did: facingAbsDot=0.22975 against a required 0.999, authoredRot=identity.
	// The four shipped townsfolk all use AABB and never surfaced this, because none
	// of them has to FACE anywhere. ANY future authored entity that must face a
	// direction is subject to the same rule: NOT AABB. Ever.
	//
	// (2) S7 item 1 SC3 NEEDS HIM TO **WALK**, so the body is now DYNAMIC and the
	// shape is a CAPSULE -- exactly what ZM_QueueDawnmereWanderer authors, and
	// exactly the contract ZM_Interactable::IsDrivableBodyContractMet demands before
	// ZM_TrainerSightFsm will ever enter APPROACHING. A STATIC body cannot be given a
	// velocity, so the walk-up would silently never happen; an OBB **would** keep the
	// yaw (it shares AABB's box shape and differs only in applying the rotation) but
	// is not a shape this game drives, and two body shapes for "an NPC who moves"
	// is a distinction with no owner. The capsule is therefore the SANCTIONED shape
	// for an authored trainer, and OBB is left to bodies that must keep a rotation
	// without ever moving.
	//
	// ★ WHAT THE DYNAMIC BODY COSTS, AND WHERE IT IS PAID. RIGIDBODY_TYPE offers only
	// DYNAMIC and STATIC -- there is NO KINEMATIC -- so this rival can be physically
	// SHOVED and, with a free Y axis, YAWED by the player, which would silently
	// reintroduce exactly the blindness (1) is about. That is mitigated in
	// ZM_Interactable, not here: the sight tick locks ALL THREE rotation axes on a
	// stationary trainer's body, holds his XZ station on every tick he is not
	// walking, and repairs the authored yaw while WATCHING. Read
	// ZM_Interactable::ApplyDrivenBodySetup / HoldTrainerStation before changing
	// anything on this line.
	//
	// ★ THE OCCLUDER FOOTPRINT CHANGES SHAPE (box -> capsule) but not scale, and the
	// SC6 sight ray only asks whether SOMETHING blocked the line; the wanderer has
	// carried a capsule in this same scene since SC8 with no ray fallout.
	void ZM_QueueDawnmereTrainerNpc(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale,
		float fYawRadians,
		void (*pfnConfigure)())
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_SetTransformYaw(fYawRadians);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		// NO AddStep_AttachGraph. See ZM_ConfigureRivalVesperNpc.
		xAuto.AddStep_Custom(pfnConfigure);
	}

	void ZM_QueueGreyboxBlock(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xScale)
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xPosition.x, xPosition.y, xPosition.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	}

	const char* ZM_TerrainBakeQueueResultToString(
		ZM_TERRAIN_BAKE_QUEUE_RESULT eResult)
	{
		switch (eResult)
		{
		case ZM_TERRAIN_BAKE_HEADLESS: return "HEADLESS";
		case ZM_TERRAIN_BAKE_WARM: return "WARM";
		case ZM_TERRAIN_BAKE_QUEUED: return "QUEUED";
		case ZM_TERRAIN_BAKE_PREPARE_FAILED: return "PREPARE_FAILED";
		default: return "INVALID";
		}
	}

	void ZM_InitializeDawnmereTerrainMaterials()
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		for (u_int uSlot = 0; uSlot < 4u; ++uSlot)
		{
			const ZM_TerrainMaterialSpec& xSpec = xRecipe.m_pxMaterials[uSlot];
			MaterialHandle& xHandle = g_axDawnmereTerrainMaterials[uSlot];
			xHandle = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();

			Zenith_MaterialAsset* pxMaterial = xHandle.GetDirect();
			pxMaterial->SetName(xSpec.m_szName);
			pxMaterial->SetBaseColor({
				xSpec.m_afBaseColour[0],
				xSpec.m_afBaseColour[1],
				xSpec.m_afBaseColour[2],
				xSpec.m_afBaseColour[3] });
			pxMaterial->SetRoughness(xSpec.m_fRoughness);
			pxMaterial->SetMetallic(xSpec.m_fMetallic);
		}
	}
}
#endif

const char* Project_GetName()
{
	return "Zenithmon";
}

const char* Project_GetGameAssetsDirectory()
{
	return GAME_ASSETS_DIR;
}

void Project_SetGraphicsOptions(Zenith_GraphicsOptions&)
{
}

void Project_RegisterGameComponents()
{
	// Meta-registry registration is the ZENITH_REGISTER_COMPONENT macro above.
	// Tools builds additionally register with the editor "Add Component" menu
	// (append-anytime registry, not sealed).
#ifdef ZENITH_TOOLS
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameComponent>("ZM_Game");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TerrainGrass>("ZM_TerrainGrass");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_PlayerController>("ZM_PlayerController");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_FollowCamera>("ZM_FollowCamera");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GameStateManager>("ZM_GameStateManager");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_SpawnPoint>("ZM_SpawnPoint");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_WarpTrigger>("ZM_WarpTrigger");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GreyboxVisual>("ZM_GreyboxVisual");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleArena>("ZM_BattleArena");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TallGrassSystem>("ZM_TallGrassSystem");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleTransition>("ZM_BattleTransition");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BattleDirector>("ZM_BattleDirector");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_UI_MenuStack>("ZM_UI_MenuStack");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_Interactable>("ZM_Interactable");

	// Runtime toggle for the battle presenter's instant-battle mode (collapses all
	// presentation timing). Bound by reference to the ZM_BattleDirectorCore backing
	// store (ZM-D-101); flip it in the Debug Variables panel under Zenithmon/Battle.
	g_xEngine.DebugVariables().AddBoolean({ "Zenithmon", "Battle", "zm_instant_battles" }, ZM_InstantBattlesRef());
#endif

	// Behaviour Graph node registration is CONFIG-INDEPENDENT: only .bgraph
	// AUTHORING is tools-only. A _False build still has to resolve node types
	// against a .bgraph left on disk, and the boot units build the definition
	// in-process in every config.
	ZM_RegisterGraphNodes();

	// Save/load persistence root: %APPDATA%/Zenith/Zenithmon/. The versioned
	// per-module save schema lands at S7 (Docs/SaveFormat.md); initialising from
	// S0 keeps the test-hook plumbing live from the first commit.
	Zenith_SaveData::Initialise("Zenithmon");

#ifdef ZENITH_INPUT_SIMULATOR
	// Between-tests reset for batched automated tests. The harness force-loads
	// scene 0 before firing this hook, so entity-owned state is already cleared
	// via OnDestroy; only ownerless game globals need explicit reset here. Keep
	// this hook current as systems land (the DP hook is the reference).
	Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
	{
		ZM_BattleTransition::ResetRuntimeStateForTests();
		ZM_UI_MenuStack::ResetRuntimeStateForTests();
		// The interaction latches are process-global (the runtime rides on whichever
		// player exists), so a batched test must not inherit the previous test's
		// interaction outcome or raise count.
		ZM_InteractionRuntime::ResetRuntimeStateForTests();
		// SC6's session latch is ownerless process-global state, so the harness's
		// scene-0 force-reload cannot clear it: without this, one test's engaged
		// flagless trainer would silence him for every later batched test.
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
		// S7 item 1 SC2's cinematic freeze latch is ownerless process-global state for
		// the SAME reason and MUST be cleared for a sharper one: it is a FREEZE OWNER
		// (ZM_UI_MenuStack::UnfreezePlayer consults it), and m_bMovementEnabled is a
		// bare bool with no refcount. A test that died mid-cinematic would otherwise
		// leave this armed and every later batched test would inherit a player who can
		// never be unfrozen again.
		ZM_TrainerCinematicLatch::ResetRuntimeStateForTests();
		// SC7's node counters are ownerless process-global observation state
		// (convention C3): without this, one test's bark count leaks into every
		// later batched test.
		ZM_GraphNodeTestCounters::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		// The persistent manager's GameState survives DontDestroyOnLoad across tests;
		// re-seed the starter so a caught/levelled party cannot leak into the next test.
		ZM_GameStateManager::ResetGameStateForTests();
		ZM_SetInstantBattlesForTests(false);
		// Disk hygiene FIRST: Zenith_SaveData::ClearForTest wipes only the in-memory
		// write log and readback stash and explicitly does NOT delete files
		// (Zenith_SaveData.h:119), so a .zsave written by one test would otherwise
		// survive into the next test AND into the next process.
		ZM_SaveSlots::DeleteAllSlotsForTests();
		Zenith_SaveData::ClearForTest();
		// A forced-cold human-asset override is scoped, but a test that leaked one
		// (an early return past its guard, a throw) would hand its "there is no bake"
		// answer to every test that ran afterwards -- and those would silently start
		// asserting against fallback blocks instead of models. Restore the production
		// policy, with a fresh bake latch, between every pair of tests.
		ZM_ResetHumanAssetPolicy();
	});
#endif
}

void Project_Shutdown()
{
#ifdef ZENITH_TOOLS
	for (MaterialHandle& xMaterial : g_axDawnmereTerrainMaterials)
	{
		xMaterial = MaterialHandle{};
	}
#endif
}

void Project_LoadInitialScene();	// forward decl for the automation step below

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Automation borrows these handles while serializing Dawnmere. The saved
	// terrain owns its material data; these temporary handles live until shutdown.
	ZM_InitializeDawnmereTerrainMaterials();
}

// Boot-authored scene: a camera, a title, and the game component, saved to
// Assets/Scenes/FrontEnd.zscen (build index 0 -- the plan's scene table lives
// in Docs/GameDesignDocument.md). A _False / Android build LOADS that baked
// scene instead of authoring it (this function is tools-only), so the FIRST
// build+run must be a *_True config to bake FrontEnd.zscen.
void Project_RegisterEditorAutomationSteps()
{
	ZM_TerrainBakeSelection xTerrainSelection;
	if (!ZM_ParseTerrainBakeSelection(__argc, __argv, xTerrainSelection))
	{
		const bool bHasErrorArgument = xTerrainSelection.m_iErrorArgument >= 0 &&
			xTerrainSelection.m_iErrorArgument < __argc && __argv != nullptr &&
			__argv[xTerrainSelection.m_iErrorArgument] != nullptr;
		Zenith_Error(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Selector rejected: result=%s, argvIndex=%d, argument='%s'; no automation queued",
			ZM_TerrainBakeSelectionParseResultToString(
				xTerrainSelection.m_eParseResult),
			xTerrainSelection.m_iErrorArgument,
			bHasErrorArgument ? __argv[xTerrainSelection.m_iErrorArgument] : "<null>");
		Zenith_Assert(false,
			"Invalid Zenithmon terrain-bake selector at argv index %d",
			xTerrainSelection.m_iErrorArgument);
		return;
	}

	Zenith_EditorAutomation& xAuto = g_xEngine.EditorAutomation();

	// Bake the Dawnmere navmesh FIRST, and unconditionally on a windowed tools
	// boot. It belongs here rather than inside the Dawnmere authoring block
	// below because that block is gated on every terrain recipe already being
	// warm -- false on a fresh clone's first boot -- while this bake needs no
	// terrain at all (it harvests a pure coverage grid from the const recipe
	// table). Running it first also guarantees the committed asset exists by the
	// time the Dawnmere block authors a component that loads it.
	xAuto.AddStep_Custom(&ZM_BakeDawnmereNavmeshStep);

	// Graphs are authored before any scene, so a scene that later references one
	// cannot race the asset. The executor is GPU-free (Zenith_EditorAutomation.cpp:
	// 2000-2023), so this runs on the Null headless config too -- and the harness
	// blocks in HarnessPhase::WaitForAutomationComplete until
	// EditorAutomation().IsComplete() (Zenith_AutomatedTest.cpp:920-923), so the
	// .bgraph is guaranteed on disk before the first automated test steps.
	xAuto.AddStep_GraphBuild(szZM_GRAPH_TRAINER_CHALLENGE_ASSET, &BuildGraph_ZM_TrainerChallenge);

	xAuto.AddStep_CreateScene("FrontEnd");
	xAuto.AddStep_CreateEntity("ZM_GameStateRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIOverlay("WarpFade");
	xAuto.AddStep_Custom(&ZM_ConfigureWarpFade);
	xAuto.AddStep_AddComponent("ZM_GameStateManager");

	// Its OWN persistent root: ZM_GameStateManager drives WarpFade every frame and
	// its DontDestroyOnLoad relocates every component on ZM_GameStateRoot, so the
	// battle machine gets a separate entity + a separate overlay (ZM-D-097).
	xAuto.AddStep_CreateEntity("ZM_BattleTransitionRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIOverlay("BattleFade");
	xAuto.AddStep_Custom(&ZM_ConfigureBattleFade);
	xAuto.AddStep_AddComponent("ZM_BattleTransition");

	// The overworld pause menu (S6 item 2 SC1) on its OWN persistent root, mirroring
	// the two roots above: a non-transient DontDestroyOnLoad entity carrying a UI
	// component (the ROOT panel + Party/Bag/Dex/Exit entries plus the SC2 dialogue
	// box panel + text, the SC4 party screen, the SC5 dex screen's static widgets,
	// the SC6 bag screen and the SC7 shop screen, all authored hidden by
	// ZM_ConfigureMenuRoot) + the
	// ZM_UI_MenuStack machine. Persistent so the menu and the dialogue box are
	// reachable from every overworld scene (Dawnmere / PlayerHome / future towns)
	// without re-authoring, and separate so its 9000/9001 sort band never collides
	// with the two fade overlays' 10000/10001.
	xAuto.AddStep_CreateEntity("ZM_MenuRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	// S7 SC5 title controls share the persistent MenuRoot so the surviving menu machine
	// can auto-raise them on every return to FrontEnd. CreateUI* first adds each element
	// to the canvas ownership lists; ZM_ConfigureMenuRootTitleScreen then reparents the
	// two buttons under the panel through ReparentElement (never AddChild-only).
	xAuto.AddStep_CreateUIRect(ZM_UI_TitleMenu::szPANEL_NAME);
	xAuto.AddStep_CreateUIButton(ZM_UI_TitleMenu::szCONTINUE_NAME, "Continue");
	xAuto.AddStep_CreateUIButton(ZM_UI_TitleMenu::szNEW_GAME_NAME, "New Game");
	xAuto.AddStep_CreateUIRect("Menu_RootPanel");
	xAuto.AddStep_CreateUIButton("Menu_RootParty", "Party");
	xAuto.AddStep_CreateUIButton("Menu_RootBag", "Bag");
	xAuto.AddStep_CreateUIButton("Menu_RootDex", "Dex");
	// The S7 SC4 Save / Quit entries, INSERTED before Exit so the created + authored visual
	// order matches ZM_MENU_ROOT_ITEM's enum order (Party/Bag/Dex/Save/Quit/Exit).
	xAuto.AddStep_CreateUIButton("Menu_RootSave", "Save");
	xAuto.AddStep_CreateUIButton("Menu_RootQuit", "Quit");
	xAuto.AddStep_CreateUIButton("Menu_RootExit", "Exit");
	// The SC2 dialogue box lives on the same root (bottom-centre band, authored
	// hidden): names are the ZM_UI_DialogueBox::szPANEL_NAME / szTEXT_NAME contract.
	xAuto.AddStep_CreateUIRect(ZM_UI_DialogueBox::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_DialogueBox::szTEXT_NAME, "");
	// ...plus the SC8 yes/no prompt buttons on the same panel, likewise hidden. The
	// labels here are the defaults; the caller that ARMS a choice supplies the real ones
	// and ZM_UI_DialogueBox::Present writes them.
	xAuto.AddStep_CreateUIButton(ZM_UI_DialogueBox::szYES_NAME, "Yes");
	xAuto.AddStep_CreateUIButton(ZM_UI_DialogueBox::szNO_NAME, "No");
	// ...and the SC4 party screen (list panel + six slot rows + summary panel/body),
	// likewise authored hidden. SlotElementName returns string literals, so calling it
	// at authoring time is safe.
	xAuto.AddStep_CreateUIRect(ZM_UI_Party::szPANEL_NAME);
	for (u_int uSlot = 0u; uSlot < ZM_UI_Party::uMAX_SLOTS; ++uSlot)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Party::SlotElementName(uSlot), "");
	}
	xAuto.AddStep_CreateUIRect(ZM_UI_Party::szSUMMARY_PANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Party::szSUMMARY_TEXT_NAME, "");
	// ...and the SC5 dex screen's STATIC widgets (panel + completion header + the two
	// page buttons), likewise authored hidden. The 5x6 grid and its 30 cells are NOT
	// authored -- ZM_UI_Dex::Present builds them once at runtime (there is no engine
	// surface for creating a Zenith_UIGridLayoutGroup from an automation step).
	xAuto.AddStep_CreateUIRect(ZM_UI_Dex::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Dex::szHEADER_NAME, "");
	xAuto.AddStep_CreateUIButton(ZM_UI_Dex::szPREV_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Dex::szNEXT_NAME, "Next >");
	// ...and the SC6 bag screen, authored WHOLE (panel + header + eight list rows + the
	// four pocket/page nav buttons), likewise hidden. Unlike the dex there is NO runtime
	// construction: a 1-D list needs no grid. RowElementName returns string literals, so
	// calling it at authoring time is safe.
	xAuto.AddStep_CreateUIRect(ZM_UI_Bag::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Bag::szHEADER_NAME, "");
	for (u_int uRow = 0u; uRow < ZM_UI_Bag::uROWS_PER_PAGE; ++uRow)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Bag::RowElementName(uRow), "");
	}
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szPREV_POCKET_NAME, "< Pocket");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szNEXT_POCKET_NAME, "Pocket >");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szPREV_PAGE_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Bag::szNEXT_PAGE_NAME, "Next >");
	// ...and the SC7 shop screen, authored WHOLE too (panel + header + six list rows +
	// the eight controls), likewise hidden. RowElementName / ControlElementName return
	// string literals, so calling them at authoring time is safe. The row labels are
	// written at runtime; the control labels are static and set here.
	xAuto.AddStep_CreateUIRect(ZM_UI_Shop::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_Shop::szHEADER_NAME, "");
	for (u_int uRow = 0u; uRow < ZM_UI_Shop::uROWS_PER_PAGE; ++uRow)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_Shop::RowElementName(uRow), "");
	}
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szBUY_TAB_NAME, "Buy");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szSELL_TAB_NAME, "Sell");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szCONFIRM_NAME, "Confirm");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szPREV_PAGE_NAME, "< Prev");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szNEXT_PAGE_NAME, "Next >");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szQTY_DOWN_NAME, "Qty -");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szQTY_UP_NAME, "Qty +");
	xAuto.AddStep_CreateUIButton(ZM_UI_Shop::szEXIT_NAME, "Leave");
	// ...and the S7 SC4 save/load screen (panel + header + four ALWAYS-VISIBLE slot rows +
	// Back), likewise authored hidden. ONE presenter serves both modes. RowElementName
	// returns string literals, so calling it at authoring time is safe. Row labels are
	// written at runtime from the live slot statuses.
	xAuto.AddStep_CreateUIRect(ZM_UI_SaveSlots::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_SaveSlots::szHEADER_NAME, "");
	for (u_int uRow = 0u; uRow < ZM_UI_SaveSlots::uROW_COUNT; ++uRow)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_SaveSlots::RowElementName(uRow), "");
	}
	xAuto.AddStep_CreateUIButton(ZM_UI_SaveSlots::szCANCEL_NAME, "Back");
	xAuto.AddStep_Custom(&ZM_ConfigureMenuRoot);
	xAuto.AddStep_AddComponent("ZM_UI_MenuStack");

	xAuto.AddStep_CreateEntity("GameManager");
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(0.f, 3.f, 6.f);
	xAuto.AddStep_SetCameraPitch(-0.4f);
	xAuto.AddStep_SetCameraFOV(glm::radians(60.f));
	xAuto.AddStep_SetAsMainCamera();
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIText("Title", "Zenithmon");
	xAuto.AddStep_SetUIAnchor("Title", static_cast<int>(Zenith_UI::AnchorPreset::Center));
	xAuto.AddStep_SetUIPosition("Title", 0.f, -220.f);
	xAuto.AddStep_SetUIFontSize("Title", 54.f);
	xAuto.AddStep_SetUIColor("Title", 1.f, 1.f, 1.f, 1.f);
	xAuto.AddStep_AddComponent("ZM_Game");
	xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// PlayerHome is a terrain-independent interior and is authored on every
	// tools boot, including headless/cold terrain runs. All shell pieces carry
	// a replaceable procedural greybox visual and their own static collider.
	//
	// ★ EVERY COORDINATE, SCALE AND NAME BELOW COMES FROM
	// Source/World/ZM_PlayerHomePlacement.h -- the SAME data the ZM-D-176 tint
	// resolver, the boot units and the automated tint test read. Nothing here
	// re-spells a literal, because a constant spelled at both sites cannot red a
	// drift. The derived values are dyadic rationals and are bit-identical to the
	// seven literal calls this loop replaced, so the committed PlayerHome.zscen
	// bytes do not move.
	//
	// ★ THE ORDER IS PART OF THE CONTRACT (ZM-D-148 dense authoring-order file
	// indices): appending a block is fine, reordering rewrites the scene bytes.
	xAuto.AddStep_CreateScene("PlayerHome");
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_PLAYERHOME_BLOCK_COUNT; ++uBlock)
	{
		const ZM_PLAYERHOME_BLOCK eBlock = (ZM_PLAYERHOME_BLOCK)uBlock;
		const ZM_PlayerHomeBlockout xBlock = ZM_GetPlayerHomeBlock(eBlock);
		ZM_QueueGreyboxBlock(xAuto, ZM_GetPlayerHomeBlockName(eBlock),
			xBlock.m_xCenter, xBlock.m_xScale);
	}

	xAuto.AddStep_CreateEntity("DoorSpawn");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, 0.0f, 3.5f);
	xAuto.AddStep_AddComponent("ZM_SpawnPoint");
	xAuto.AddStep_Custom(&ZM_ConfigureDoorSpawnPoint);

	xAuto.AddStep_CreateEntity("Player");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, fZM_HUMAN_BODY_HALF_HEIGHT, 3.5f);
	xAuto.AddStep_SetTransformScale(
		fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
	xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
	xAuto.AddStep_AddComponent("ZM_PlayerController");

	xAuto.AddStep_CreateEntity("PlayerHomeCamera");
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(0.0f, 3.0f, -2.0f);
	xAuto.AddStep_SetCameraYaw(0.0f);
	xAuto.AddStep_SetCameraPitch(0.0f);
	xAuto.AddStep_SetCameraFOV(glm::radians(65.0f));
	xAuto.AddStep_SetCameraNear(0.1f);
	xAuto.AddStep_SetCameraFar(100.0f);
	xAuto.AddStep_AddComponent("ZM_FollowCamera");
	xAuto.AddStep_SetAsMainCamera();

	xAuto.AddStep_CreateEntity("PlayerHomeExitTrigger");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, 1.0f, 5.2f);
	xAuto.AddStep_SetTransformScale(3.0f, 2.0f, 1.2f);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	xAuto.AddStep_AddComponent("ZM_WarpTrigger");
	xAuto.AddStep_Custom(&ZM_ConfigurePlayerHomeExitTrigger);

	xAuto.AddStep_SaveScene(
		GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// Battle arena (build index 1) is a terrain-independent, self-contained scene
	// authored 2000 m below the overworld (ZM_BattleArena::fARENA_WORLD_Y) so the
	// S5 additive load never bleeds through the overworld. The ZM_BattleArena
	// component spawns the dome + platforms + six dressing sets in OnStart.
	xAuto.AddStep_CreateScene("Battle");

	xAuto.AddStep_CreateEntity("BattleArena");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(0.0f, -2000.0f, 0.0f);   // ZM_BattleArena::fARENA_WORLD_Y
	xAuto.AddStep_AddComponent("ZM_BattleArena");

	// The battle presenter-driver (S5 item 4 SC3, order 111): a sibling entity in the
	// Battle scene that watches the persistent transition and, once IN_BATTLE, runs a
	// deterministic AI-vs-AI wild battle and ends it via RequestBattleEnd().
	xAuto.AddStep_CreateEntity("BattleDirector");
	xAuto.AddStep_SetEntityTransient(false);
	// The battle HUD (S5 item 4 SC4): a UI component + five elements authored on the
	// director entity, configured hidden by ZM_ConfigureBattleHUD. ZM_BattleDirector
	// owns a ZM_UI_BattleHUD by value and drives them (reveal / typewriter / hide).
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIText("BattleHUD_Log", "");
	xAuto.AddStep_CreateUIText("BattleHUD_PlayerPanel", "");
	xAuto.AddStep_CreateUIText("BattleHUD_EnemyPanel", "");
	xAuto.AddStep_CreateUIRect("BattleHUD_PlayerHPBar");
	xAuto.AddStep_CreateUIRect("BattleHUD_EnemyHPBar");
	// The interactive battle menu: a backing panel + three root buttons (Fight/Catch/Run,
	// SC4 adds Catch) + four move buttons, all authored hidden by ZM_ConfigureBattleHUD.
	// ZM_UI_BattleHUD (owned by ZM_BattleDirector) shows/highlights/hides them via
	// UpdateMenu/HideMenu.
	xAuto.AddStep_CreateUIRect("BattleHUD_MenuPanel");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionFight", "Fight");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionCatch", "Catch");
	xAuto.AddStep_CreateUIButton("BattleHUD_ActionRun",   "Run");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move0", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move1", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move2", "");
	xAuto.AddStep_CreateUIButton("BattleHUD_Move3", "");
	xAuto.AddStep_Custom(&ZM_ConfigureBattleHUD);
	xAuto.AddStep_AddComponent("ZM_BattleDirector");

	xAuto.AddStep_CreateEntity("BattleCamera");
	xAuto.AddStep_AddCamera();
	// Camera forward at yaw 0 is +Z, so the camera sits on the -Z side looking
	// toward the arena/platforms at Z~=0 (pitched slightly down onto the -2000 m
	// platform plane). Mirrors the PlayerHome camera, which sits behind its subject.
	xAuto.AddStep_SetCameraPosition(0.0f, -1997.5f, -8.0f);
	xAuto.AddStep_SetCameraYaw(0.0f);
	xAuto.AddStep_SetCameraPitch(-0.25f);
	xAuto.AddStep_SetCameraFOV(glm::radians(55.0f));
	xAuto.AddStep_SetCameraNear(0.1f);
	xAuto.AddStep_SetCameraFar(200.0f);
	xAuto.AddStep_SetAsMainCamera();

	xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// ProfLab ("Aster's Lab", the ZM_SCENE_PROFLAB row of the compiled world
	// table) is a terrain-independent interior, so like PlayerHome it is authored
	// on EVERY tools boot including headless/cold-terrain ones -- it is
	// deliberately NOT inside the AUTHOR_DAWNMERE gate below, which exists only to
	// protect terrain-derived content. An interior has no terrain to protect.
	//
	// ★ EVERY COORDINATE, SCALE, NAME AND TAG BELOW COMES FROM
	// Source/World/ZM_ProfLabPlacement.h -- the SAME data the boot units and the
	// automated arrival test read. Nothing here re-spells a literal, because a
	// constant spelled at both sites cannot red a drift.
	//
	// ★ NO ZM_WarpTrigger, AND THAT IS DELIBERATE. ZM_WorldSpec declares
	// ProfLab -> Dawnmere via spawn tag "FromLab", but Dawnmere.zscen authors only
	// the "TownCenter" and "FromHome" markers. An exit configured against
	// "FromLab" would pass IsWarpDestinationValid -- which reads only the compiled
	// tag list, never the actual scene -- and then park the warp machine in
	// WAITING_FOR_SPAWN, which has no timeout: an opaque fade and a frozen player,
	// forever. The exit, the "FromLab" marker and the Dawnmere-side Lab door all
	// land together in the sub-commit that owns re-writing Dawnmere.zscen.
	xAuto.AddStep_CreateScene(szZM_PROFLAB_SCENE_NAME);
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++uBlock)
	{
		const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)uBlock;
		const ZM_ProfLabBlockout xBlock = ZM_GetProfLabBlock(eBlock);
		ZM_QueueGreyboxBlock(xAuto, ZM_GetProfLabBlockName(eBlock),
			xBlock.m_xCenter, xBlock.m_xScale);
	}

	// The single arrival marker. Its transform is the marker's FEET: the warp adds
	// the capsule half-extent at spawn time (ZM_GameStateManager::CalculateSpawnCenter),
	// so authoring a body centre here would put an arriving player half a body
	// into the ceiling.
	const Zenith_Maths::Vector3 xProfLabSpawnFeet = ZM_GetProfLabSpawnFeet();
	xAuto.AddStep_CreateEntity(szZM_PROFLAB_SPAWN_ENTITY_NAME);
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(
		xProfLabSpawnFeet.x, xProfLabSpawnFeet.y, xProfLabSpawnFeet.z);
	xAuto.AddStep_AddComponent("ZM_SpawnPoint");
	xAuto.AddStep_Custom(&ZM_ConfigureProfLabDoorSpawnPoint);

	// The authored player stands ON that marker. CAPSULE, not AABB or OBB: it is
	// the one body here that moves.
	const Zenith_Maths::Vector3 xProfLabPlayerCenter = ZM_GetProfLabPlayerCenter();
	xAuto.AddStep_CreateEntity(szZM_PROFLAB_PLAYER_ENTITY_NAME);
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(
		xProfLabPlayerCenter.x, xProfLabPlayerCenter.y, xProfLabPlayerCenter.z);
	xAuto.AddStep_SetTransformScale(
		fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
	xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
	xAuto.AddStep_AddComponent("ZM_PlayerController");

	// Camera forward at yaw 0 is +Z, so the camera sits on the player's -Z side
	// looking back toward the +Z entrance -- which is why the hall extends into -Z
	// and the aperture is the +Z face. The X and Z below are the follow camera's own
	// arm length straight back from the arrival point, i.e. exactly where the spring
	// settles in plan view.
	//
	// The Y is NOT the settled height, and is not meant to be: it is
	// fZM_PROFLAB_CAMERA_HEIGHT above the FLOOR, while
	// ZM_FollowCamera::ComputeDesiredPosition adds that same height to the player's
	// CENTRE and therefore settles one capsule half-extent higher. Nothing snaps
	// visibly because ZM_FollowCamera::OnStart clears the spring state, so the first
	// OnLateUpdate after the scene loads jumps the spring straight to the desired
	// pose rather than easing toward it -- the authored Y is only ever the pose of a
	// camera that has not ticked yet. The shipped PlayerHome camera above is authored
	// at the same 3.0 in the same way. (The relationship between the two points is
	// asserted, not assumed: clause (5) of the boot unit
	// ProfLab_FollowCameraTrailsIntoTheRoomAtTheAuthoredYaw.)
	const Zenith_Maths::Vector3 xProfLabCameraPos = ZM_GetProfLabCameraPosition();
	xAuto.AddStep_CreateEntity(szZM_PROFLAB_CAMERA_ENTITY_NAME);
	xAuto.AddStep_AddCamera();
	xAuto.AddStep_SetCameraPosition(
		xProfLabCameraPos.x, xProfLabCameraPos.y, xProfLabCameraPos.z);
	xAuto.AddStep_SetCameraYaw(fZM_PROFLAB_CAMERA_YAW);
	xAuto.AddStep_SetCameraPitch(fZM_PROFLAB_CAMERA_PITCH);
	xAuto.AddStep_SetCameraFOV(glm::radians(fZM_PROFLAB_CAMERA_FOV_DEGREES));
	xAuto.AddStep_SetCameraNear(fZM_PROFLAB_CAMERA_NEAR);
	xAuto.AddStep_SetCameraFar(fZM_PROFLAB_CAMERA_FAR);
	xAuto.AddStep_AddComponent("ZM_FollowCamera");
	xAuto.AddStep_SetAsMainCamera();

	xAuto.AddStep_SaveScene(
		GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT);
	xAuto.AddStep_UnloadScene();

	// Null (headless/CI) builds still author the FrontEnd, Battle and the
	// terrain-independent PlayerHome and ProfLab interiors above -- preserve that
	// split exactly.
	// What they must NOT do is mutate terrain assets or author the Dawnmere
	// Terrain/Flux scene: a null backend would bake render content from nothing
	// and overwrite good baked terrain. Those two stay DEFERRED.
	const bool bHeadless = Zenith_IsNullRenderer();
	ZM_TerrainBakeBatchPlan xTerrainBatch;
	if (bHeadless)
	{
		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, true, 0u);
		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=HEADLESS, probes=0, warmMask=0x0, queueMask=0x0, queued=0, sceneAuthoring=DEFERRED",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode));
	}
	else
	{
		u_int uWarmRecipeMask = 0u;
		const u_int uRecipeCount = ZM_GetTerrainAuthoringRecipeCount();
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const bool bWarm = ZM_IsTerrainBakeWarm(
				xRecipe, std::filesystem::path(GAME_ASSETS_DIR));
			if (bWarm)
			{
				uWarmRecipeMask |= 1u << i;
			}
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch probe: index=%u, set='%s', warm=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				bWarm ? "true" : "false");
		}

		xTerrainBatch = ZM_BuildTerrainBakeBatchPlan(
			xTerrainSelection, false, uWarmRecipeMask);
		u_int uQueuedRecipeCount = 0u;
		for (u_int i = 0; i < uRecipeCount; ++i)
		{
			const ZM_TerrainAuthoringRecipe& xRecipe =
				ZM_GetTerrainAuthoringRecipe(i);
			const u_int uRecipeBit = 1u << i;
			const bool bQueue =
				(xTerrainBatch.m_uQueueRecipeMask & uRecipeBit) != 0u;
			const bool bWarm = (uWarmRecipeMask & uRecipeBit) != 0u;
			const char* szDecision = bQueue ? "QUEUE" :
				(bWarm ? "SKIP_WARM" : "SKIP_FILTERED");
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch decision: index=%u, set='%s', action=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet, szDecision);
			if (!bQueue)
			{
				continue;
			}

			const bool bForce = xTerrainSelection.m_eMode !=
				ZM_TERRAIN_BAKE_SELECTION_AUTO_MISSING;
			const ZM_TERRAIN_BAKE_QUEUE_RESULT eQueueResult =
				ZM_QueueTerrainBake(xAuto, xRecipe, false, bForce);
			Zenith_Log(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch queue: index=%u, set='%s', result=%s",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			if (eQueueResult == ZM_TERRAIN_BAKE_QUEUED)
			{
				++uQueuedRecipeCount;
				continue;
			}
			if (eQueueResult == ZM_TERRAIN_BAKE_WARM && !bForce)
			{
				// A cold-to-warm race is harmless, but the immutable pre-scan
				// plan still defers scene authoring until the next boot.
				continue;
			}

			// Preparation may fail after earlier recipes appended actions.
			// Reset makes this boot all-or-nothing: no partial batch executes.
			xAuto.Reset();
			Zenith_Error(LOG_CATEGORY_TERRAIN,
				"[ZM Terrain] Batch aborted: index=%u, set='%s', result=%s; automation reset",
				i, xRecipe.m_pxWorldSpec->m_szTerrainSet,
				ZM_TerrainBakeQueueResultToString(eQueueResult));
			Zenith_Assert(false,
				"Terrain bake batch preparation failed for %s",
				xRecipe.m_pxWorldSpec->m_szTerrainSet);
			return;
		}

		Zenith_Log(LOG_CATEGORY_TERRAIN,
			"[ZM Terrain] Batch result: mode=%s, result=%s, probes=%u, warmMask=0x%X, queueMask=0x%X, queued=%u, sceneAuthoring=%s",
			ZM_TerrainBakeSelectionModeToString(xTerrainSelection.m_eMode),
			xTerrainBatch.m_uQueueRecipeMask == 0u ? "NO_QUEUE" : "QUEUED",
			uRecipeCount, xTerrainBatch.m_uWarmRecipeMask,
			xTerrainBatch.m_uQueueRecipeMask, uQueuedRecipeCount,
			xTerrainBatch.m_bAuthorDawnmereScene ?
				"AUTHOR_DAWNMERE" : "DEFERRED");
	}

	// A queued cold/forced batch completes this boot. Author Dawnmere only when
	// the windowed pre-scan found every registered terrain warm and queued none.
	// Thornacre and Route1 remain measurement-only recipes in this milestone.
	if (xTerrainBatch.m_bAuthorDawnmereScene)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe = ZM_GetDawnmereTerrainRecipe();
		const ZM_TerrainPreviewCameraSpec& xCamera = xRecipe.m_xPreviewCamera;
		const std::string strSplatmapPath = std::string("game:Terrain/") +
			xRecipe.m_pxWorldSpec->m_szTerrainSet + "/Splatmap_RGBA" ZENITH_TEXTURE_EXT;
		// S7 item 3 SC8: the anchor now comes from the shared pure header, with
		// IDENTICAL literals, so no already-authored entity moves by one bit. The
		// migration is what lets the boot units reason about these coordinates --
		// they used to be a local inside this ZENITH_TOOLS block, invisible to
		// anything that runs in CI.
		const Zenith_Maths::Vector3 xTownCenterFeet(
			fZM_DAWNMERE_TOWN_CENTER_X,
			fZM_DAWNMERE_TOWN_CENTER_FEET_Y,
			fZM_DAWNMERE_TOWN_CENTER_Z);
		const Zenith_Maths::Vector3 xPlayerScale = g_xDawnmereHumanScale;
		const float fPlayerCapsuleHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
		const Zenith_Maths::Vector3 xPlayerCenter =
			xTownCenterFeet + Zenith_Maths::Vector3(
				0.0f, fPlayerCapsuleHalfExtent, 0.0f);

		xAuto.AddStep_CreateScene("Dawnmere");
		xAuto.AddStep_CreateEntity("DawnmereTerrain");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_AddComponent("Terrain");
		xAuto.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
		for (int iSlot = 0; iSlot < 4; ++iSlot)
		{
			xAuto.AddStep_SetTerrainMaterial(iSlot, g_axDawnmereTerrainMaterials[iSlot].GetDirect());
		}
		xAuto.AddStep_SetTerrainSplatmapPath(strSplatmapPath.c_str());
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_TerrainGrass");

		// Spawn markers are feet/surface anchors. Runtime warps and this authored
		// preview placement share ONE compiled capsule half-extent.
		xAuto.AddStep_CreateEntity("TownCenterSpawn");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xTownCenterFeet.x, xTownCenterFeet.y, xTownCenterFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureTownCenterSpawnPoint);

		// The player and camera are Dawnmere-owned. SINGLE scene loads therefore
		// replace both entities instead of carrying movement/camera state between
		// scenes. TownCenter is the exact sampled terrain surface; adding the
		// contract's 0.9 m capsule half-extent produces the authored centre.
		xAuto.AddStep_CreateEntity("Player");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xPlayerCenter.x, xPlayerCenter.y, xPlayerCenter.z);
		xAuto.AddStep_SetTransformScale(
			xPlayerScale.x, xPlayerScale.y, xPlayerScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_PlayerController");

		// Replaceable outdoor Home blockout. Every coordinate below comes from the
		// shared ZM-D-173 block in Source/World/ZM_DawnmerePlacement.h -- the
		// SAME data the boot units and the real-scene clearance guard read -- so
		// "the camera fits behind the player at this doorway" is arithmetic a test
		// runs rather than a claim this comment makes. Read that header before
		// moving any of it: the entrance face, the sensor, the spawn marker and
		// the traversal route are one interlocking placement, and the terrain pad
		// in ZM_TerrainAuthoring.cpp moves with them.
		const ZM_DawnmereBlockout xHomeShell = ZM_GetDawnmereHomeShell();
		const ZM_DawnmereBlockout xHomeDoorLeft = ZM_GetDawnmereHomeDoorLeft();
		const ZM_DawnmereBlockout xHomeDoorRight = ZM_GetDawnmereHomeDoorRight();
		const ZM_DawnmereBlockout xHomeLintel = ZM_GetDawnmereHomeDoorLintel();
		const ZM_DawnmereBlockout xHomeTrigger = ZM_GetDawnmereHomeDoorTrigger();
		const Zenith_Maths::Vector3 xFromHomeSpawnFeet =
			ZM_GetDawnmereFromHomeSpawnFeet();
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeShell",
			xHomeShell.m_xCenter, xHomeShell.m_xScale);
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorLeft",
			xHomeDoorLeft.m_xCenter, xHomeDoorLeft.m_xScale);
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorRight",
			xHomeDoorRight.m_xCenter, xHomeDoorRight.m_xScale);
		ZM_QueueGreyboxBlock(xAuto, "DawnmereHomeDoorLintel",
			xHomeLintel.m_xCenter, xHomeLintel.m_xScale);

		xAuto.AddStep_CreateEntity("FromHomeSpawn");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xFromHomeSpawnFeet.x, xFromHomeSpawnFeet.y, xFromHomeSpawnFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureFromHomeSpawnPoint);

		xAuto.AddStep_CreateEntity("HomeDoorTrigger");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xHomeTrigger.m_xCenter.x, xHomeTrigger.m_xCenter.y,
			xHomeTrigger.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xHomeTrigger.m_xScale.x, xHomeTrigger.m_xScale.y,
			xHomeTrigger.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureHomeDoorTrigger);

		// ---- The authored Dawnmere NPCs (S6 item 3; SC8 added the patrol, and ----
		// ---- S7 item 2 SC1 added the story-gated warden)                    ----
		//
		// Bodies share the PLAYER'S scale, so an NPC's AABB half-height IS
		// fPlayerCapsuleHalfExtent and every NPC centre sits at exactly the player's
		// authored centre height.
		//
		// ★ HEIGHT IS MEASURED, NOT ASSUMED (known-limit W5). Every NPC used to reuse
		// the ONE feet height sampled at the town centre (512, 480), which made the
		// picker's +/-2 m band -- and the trainer cone's fZM_SIGHT_MAX_VERTICAL -- an
		// inference from a single out-of-band measurement. Each of the six now carries
		// its OWN feet height in Source/World/ZM_DawnmerePlacement.h's W5 block, and
		// every centre below is ZM_DawnmereNpcCentreY(id, halfExtent): that NPC's
		// measured surface plus the shared capsule half-extent.
		//
		// THE ORACLE is ZM_DawnmereNpcGroundTruth_Test (Tests/ZM_AutoTests_NpcTalk.cpp).
		// It loads the COMMITTED Dawnmere, casts a real downward ray at each anchor's
		// XZ against the baked terrain body, and reds if any compiled row has drifted
		// from the surface the capsule actually rests on -- so the numbers cannot go
		// stale silently. It also LOGS every measured height at INFO on every run.
		//
		// ★ WHAT RE-MEASURES THEM: regenerating the Dawnmere heightmap (a terrain
		// recipe, a seed, or the flatten radii in ZM_TerrainAuthoring.cpp). Run that
		// test, paste its six `measured=` figures into the W5 block, rebuild, and
		// re-author this scene from a windowed tools boot.
		//
		// STILL NOT SAMPLED HERE, and deliberately: this authoring does NOT raycast
		// the terrain. The committed .zscen bytes must be reproducible from COMPILED
		// constants rather than from a gitignored terrain bake -- and there is no
		// terrain physics body during authoring anyway (the editor add path uses the
		// deserialization ctor, which never calls LoadCombinedPhysicsGeometry), so an
		// authoring-time raycast would simply MISS.
		//
		// SEPARATION is deliberately enormous -- the closest PAIR is 16.1 m, against
		// a 2.9 m effective reach (2.5 global + 0.4 per-NPC). The picker resolves the
		// NEAREST FACED candidate, so two NPCs within reach of each other would make
		// "which NPC answered?" a function of sub-metre walk error; at 16 m the
		// answer cannot be ambiguous, and the walk-up test can assert the winner BY
		// ENTITY ID. Exact distances are derived at the coordinates below.
		//
		// The VILLAGER is the walk-up target and sits straight +Z of the spawn on
		// purpose: +Z is the one movement axis with existing evidence
		// (ZM_DawnmerePlayerCamera_Test already proves held-W moves the yaw-zero
		// player +Z), so the walk needs no unproven basis assumption.
		//
		// ★★ THE OTHER TWO MUST STAY OFF THE HOME DRIVE CORRIDOR. A solid STATIC
		// AABB on it WEDGES A DIFFERENT, ALREADY-GREEN TEST:
		// ZM_PlayerHomeRoundTrip_Test drives the player from the TownCenter spawn
		// (512, 480) to the door staging waypoint with DriveTowardXZ, which has NO
		// obstacle avoidance. An NPC box on that line stops the capsule head-on
		// (the 1.8 m body is far above the 0.40 m step assist), the staging
		// tolerance is never met, and that test dies at its frame cap with a
		// timeout that names distance, not the NPC.
		// ★ ZM-D-173 CHANGED THAT CORRIDOR'S SHAPE. It used to be pure -X along
		// z = 480 (|dz| inside DriveTowardXZ's 0.08 dead zone, so only 'A' was
		// held); with the Home relocated the staging waypoint is
		// ZM_GetDawnmereHomeDoorStagingXZ() -- currently (384, 470) -- so the leg is
		// a shallow diagonal that drops 10 m in Z across 128 m in X. Anything
		// between z = 470 and z = 480 on that run is now in the way, which the two
		// flank NPCs at z + 18 still clear by 18 m.
		// So both flank NPCs are pushed to z + 18, keeping 18 m of clearance from
		// the Home corridor while staying 14 m off the x = 512 spawn-to-villager
		// corridor and well clear of the Home shell (x 375.5..392.5, z 476..489).
		// A scene-placement change can regress a suite it never mentions -- check the
		// existing traversal routes before moving anything in this block.
		//
		const Zenith_Maths::Vector3 xNpcScale = xPlayerScale;
		const Zenith_Maths::Vector3 xVillagerCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_VILLAGER, fPlayerCapsuleHalfExtent);
		// z + 18 keeps both off the Home-traversal corridor, which since ZM-D-173
		// runs from (512, 480) down to the door staging waypoint at (384, 470).
		// Separations against the 2.9 m effective reach (2.5 global + 0.4 authored):
		//   villager <-> clerk      = sqrt(14^2 + 8^2) = 16.1 m
		//   villager <-> caretaker  = sqrt(14^2 + 8^2) = 16.1 m
		//   clerk    <-> caretaker  = 28.0 m
		//   spawn    <-> either     = sqrt(14^2 + 18^2) = 22.8 m
		// The closest pair is 5.5x reach, so the nearest-faced-candidate picker can
		// never confuse two of them and the walk-up test can assert the winner BY
		// ENTITY ID; and neither flank NPC is reachable from spawn, which keeps the
		// test's out-of-range negative unambiguous.
		const Zenith_Maths::Vector3 xClerkCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_TRADE_POST_CLERK, fPlayerCapsuleHalfExtent);
		const Zenith_Maths::Vector3 xCaretakerCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_CARETAKER, fPlayerCapsuleHalfExtent);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Villager",
			xVillagerCenter, xNpcScale, &ZM_ConfigureVillagerNpc);
		ZM_QueueDawnmereNpc(xAuto, "Npc_TradePostClerk",
			xClerkCenter, xNpcScale, &ZM_ConfigureTradePostClerkNpc);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Caretaker",
			xCaretakerCenter, xNpcScale, &ZM_ConfigureCaretakerNpc);
		// S7 item 2 SC1: the story-gated warden. He stands on the authored HOME
		// WALKWAY, not on the north road: (478, 498) is ~1.1 m off the Home path
		// centreline and ~36.8 m from the nearest point of the Route polyline
		// (ZM_TerrainAuthoring.cpp:36-49), so his lines are written as a lane warden
		// rather than a road-blocker. The position itself is derived under exactly the
		// constraints stated above, NOT eyeballed:
		//   * z + 18 is the SAME clearance the two flank NPCs use, so the warden is
		//     18 m off the Home traversal corridor (z 470..480 since ZM-D-173) that
		//     ZM_PlayerHomeRoundTrip_Test drives blind along. Anything nearer would
		//     re-open the wedging hazard the block above is written to prevent.
		//   * x - 34 keeps it 34 m off the x = 512 spawn-to-villager corridor.
		//   * Separations from the existing roster, against the same 2.9 m effective
		//     reach: caretaker (498, 498) = 20.0 m (the new closest pair, still 6.9x
		//     reach and wider than the existing 16.1 m minimum, so the "closest pair"
		//     figure quoted at fZM_NPC_AUTHORED_RADIUS is unchanged); villager
		//     (512, 490) = sqrt(34^2 + 8^2) = 34.9 m; clerk (526, 498) = 48.0 m;
		//     wanderer patrol (540, 476..484) = 63.6 m at its nearest endpoint;
		//     TownCenter spawn = sqrt(34^2 + 18^2) = 38.5 m, so the warden is not
		//     reachable from spawn and the existing out-of-range negative stays clean.
		//   * The Home shell (x 375.5..392.5, z 476..489) lies SOUTH-WEST of the
		//     warden, who stands at (478, 498): its east face is 85.5 m west and its
		//     north face is 9 m south. That is ample clearance; re-check both axes
		//     if either the facade footprint or the warden placement moves.
		// Height is his OWN measured feet plus the shared capsule half-extent, like
		// every other NPC since known-limit W5 -- see the block above.
		// ★ When a later stage authors a real Route 1, a warden who is meant to BLOCK
		// the road belongs on the Route polyline itself. Re-place him there and
		// re-derive every separation above from scratch -- none of these figures carry
		// over, INCLUDING his feet height -- and rewrite his lines in ZM_NpcData.cpp
		// to match the new ground.
		const Zenith_Maths::Vector3 xRouteWardenCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_WARDEN, fPlayerCapsuleHalfExtent);
		ZM_QueueDawnmereNpc(xAuto, "Npc_Warden",
			xRouteWardenCenter, xNpcScale, &ZM_ConfigureRouteWardenNpc);
		// SC8: the fourth row is a deterministic two-point patrol. Both endpoints are
		// 28 m east of the TownCenter spawn and outside the Home corridor's
		// x<=512 run; the nearest stationary NPC (the clerk) remains >19 m away.
		// Its resting centre would put this capsule ON the local surface, which is
		// higher than the town centre; ONE EXTRA capsule half-extent of clearance
		// authors it safely above that surface so gravity settles it from the front
		// side. That special case is NAMED (ZM_DawnmereWandererSpawnY) rather than
		// open-coded, because it is the one NPC whose authored Y is not its centre.
		const Zenith_Maths::Vector3 xWandererCenter(
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WANDERER).m_fX,
			ZM_DawnmereWandererSpawnY(fPlayerCapsuleHalfExtent),
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_WANDERER).m_fZ);
		ZM_QueueDawnmereWanderer(xAuto, xWandererCenter, xNpcScale);

		// ---- S7 item 3 SC8 / item 4: rival Vesper, the FIRST authored trainer ----
		//
		// WHAT HE DOES. He is a stationary TALKER row like the warden, plus one
		// thing no other NPC has: his ZM_NpcData row names ZM_TRAINER_RIVAL_VESPER,
		// so ZM_Interactable::DeriveTrainerFromNpcRow arms his sight cone the moment
		// this scene loads -- off the COMMITTED bytes, with nobody calling
		// ConfigureTrainerSight. Walk into his cone and he barks, then battles.
		//
		// ★ HE IS A LIVE ARMED TRAINER IN A SCENE MANY TESTS LOAD. The between-tests
		// hook re-seeds the game state and clears ZM_TrainerEngagementLatch, so he is
		// RE-ARMED at the start of every batched Dawnmere test. DISTANCE is the
		// guard, not facing. Any future walk that comes within 8 m of his forward
		// cone will take a forced battle it never mentions, and it will surface as a
		// timeout naming a distance rather than naming him.
		//
		// ★ S7 item 1 SC3 MADE THAT WORSE IN TWO WAYS, AND BOTH ARE DELIBERATE.
		// He now carries a DYNAMIC CAPSULE instead of a static box, so he is a body
		// the player can lean on rather than a wall; and once he spots anyone he
		// WALKS AT THEM and FREEZES THEM for up to
		// ZM_TrainerSightFsmTuning::m_fApproachTimeoutSeconds while he closes. A test
		// that strays into his cone therefore loses control of its player for a
		// couple of seconds BEFORE the battle it was not expecting. The mitigations
		// (all-axis rotation lock, per-tick XZ station hold, WATCHING yaw repair)
		// live in ZM_Interactable and keep him ON this authored spot; the geometry
		// that keeps every shipped route out of his cone is unchanged and is derived
		// in Source/World/ZM_DawnmerePlacement.h.
		//
		// EVERY COORDINATE AND CLEARANCE IS DERIVED IN
		// Source/World/ZM_DawnmerePlacement.h -- read that header before moving
		// anything here. It is a shared header rather than a local so that the boot
		// units (Vesper_PlacementCannotSpawnCampOnTheWhiteoutTarget,
		// Vesper_FacingIsDerivedFromTheTownCentreBearing) can assert the geometry
		// with no scene and no assets, in CI.
		//
		// ★ GDD DEVIATION, RECORDED: canon puts rival battle 1 on "Route 1 (L5)".
		// Route 1 does not exist in S7. When it is authored, move him and re-derive
		// every separation from scratch (ZM-D-156, Shortfalls).
		const Zenith_Maths::Vector3 xRivalVesperCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_RIVAL_VESPER, fPlayerCapsuleHalfExtent);
		ZM_QueueDawnmereTrainerNpc(xAuto, "Npc_RivalVesper",
			xRivalVesperCenter, xNpcScale, ZM_DawnmereVesperYaw(),
			&ZM_ConfigureRivalVesperNpc);

		xAuto.AddStep_CreateEntity("DawnmerePreviewCamera");
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(xCamera.m_xPosition.m_fX, xCamera.m_xPosition.m_fY, xCamera.m_xPosition.m_fZ);
		xAuto.AddStep_SetCameraYaw(0.0f);
		xAuto.AddStep_SetCameraPitch(xCamera.m_fPitch);
		xAuto.AddStep_SetCameraFOV(glm::radians(xCamera.m_fFovDegrees));
		xAuto.AddStep_SetCameraNear(xCamera.m_fNearPlane);
		xAuto.AddStep_SetCameraFar(xCamera.m_fFarPlane);
		xAuto.AddStep_AddComponent("ZM_FollowCamera");
		xAuto.AddStep_SetAsMainCamera();

		// The scene's baked-navmesh holder. Its OWN entity so the component's
		// lifetime is exactly the scene's -- load Dawnmere, the navmesh comes
		// with it; unload, it goes. No cache, no hook, nothing to invalidate.
		xAuto.AddStep_CreateEntity("DawnmereNavMesh");
		xAuto.AddStep_AddComponent("NavMesh");
		xAuto.AddStep_Custom(&ZM_ConfigureDawnmereNavMesh);

		// ★ IMMEDIATELY BEFORE THE SAVE, NOT ANYWHERE EARLIER. The guard serializes the
		// rival's transform for real and compares the resulting bytes with
		// ZM_DawnmereVesperFacing(); run it any earlier and a later step could still
		// move the value it just cleared. See ZM_VerifyAuthoredRivalFacingStep.
		xAuto.AddStep_SelectEntity("Npc_RivalVesper");
		xAuto.AddStep_Custom(&ZM_VerifyAuthoredRivalFacingStep);

		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	xAuto.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	g_xEngine.Scenes().RegisterSceneBuildIndex(0, GAME_ASSETS_DIR "Scenes/FrontEnd" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(1, GAME_ASSETS_DIR "Scenes/Battle" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(2, GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(40, GAME_ASSETS_DIR "Scenes/PlayerHome" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().RegisterSceneBuildIndex(41, GAME_ASSETS_DIR "Scenes/ProfLab" ZENITH_SCENE_EXT);
	g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
}
