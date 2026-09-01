#include "Zenith.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"                        // ZM_ResolvePropFit measures a baked prop mesh
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"   // the human locomotion animator
#include "EntityComponent/Components/Zenith_ColliderComponent.h"   // the explicit human body contract
#include "EntityComponent/Components/Zenith_LightComponent.h"    // lamp posts own their own bulb
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
#include "Zenithmon/Components/ZM_GroundItemProp.h"       // the ground-item prop (ZM-27 follow-up (a))
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Components/ZM_SpawnPoint.h"
#include "Zenithmon/Components/ZM_TallGrassSystem.h"
#include "Zenithmon/Components/ZM_TerrainGrassComponent.h"
#include "Zenithmon/Components/ZM_TouchLayoutController.h"        // the B9 on-screen HUD context machine (WP3b)
#include "Zenithmon/Components/ZM_UI_MenuStack.h"
#include "Zenithmon/Components/ZM_WarpTrigger.h"
#include "Zenithmon/Source/ZM_Bindings.h"                         // the C2 action table (profiles + actions + bindings)
#include "Zenithmon/Source/Battle/ZM_BattleDirectorCore.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"                     // ZM_GetNpcData -- the greybox's appearance row (W4)
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"              // ZM_GetHumanPaletteColour (W4)
// ★ UNCONDITIONAL, like ZM_HumanAppearance.h beside it and NOT in the ZENITH_TOOLS
// block below. ZM_GreyboxVisual's PROP branch compiles in EVERY configuration and
// reads the asset-path scheme, the palette colours and the (tools-only, no-op
// elsewhere) single-prop bake from here; the bake guard is inside the header, not
// at this include.
#include "Zenithmon/Source/Gen/ZM_PropGen.h"                      // prop asset refs + palette + ZM_EnsurePropBaked (ZM-67)
#include "Zenithmon/Source/Gen/ZM_BuildingGen.h"                  // building asset refs + ZM_EnsureBuildingBaked -- the Dawnmere facades
#include "Zenithmon/Source/World/ZM_DawnmereFacades.h"            // facade entity names + the total name -> building mapping
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"                  // room-shell asset refs + ZM_EnsureInteriorBaked
#include "Zenithmon/Source/World/ZM_InteriorDressing.h"           // shell names, prop + light tables
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
#include "Zenithmon/Source/UI/ZM_UI_StarterChoice.h" // sz*_NAME + CellElementName + geometry contract (S8 starter authoring)
#include "Zenithmon/Source/UI/ZM_UI_TitleMenu.h"     // title panel / Continue / New Game authoring contract (S7 SC5)
// ★ UNCONDITIONAL, and NOT in the ZENITH_TOOLS block below that carries
// ZM_ProfLabPlacement.h. ZM_GreyboxVisual compiles in EVERY configuration and
// reads ZM_IsPlayerHomeBlockName / ZM_GetPlayerHomeInteriorTintColour from here
// (ZM-D-176); the tools-only authoring loop reads the block table from the same
// file, so both sides share one spelling.
#include "Zenithmon/Source/World/ZM_HumanAssetPolicy.h"           // is the human bake loadable right now?
#include "Zenithmon/Source/World/ZM_HumanBody.h"               // THE human body contract (size, capsule, visual scale)
#include "Zenithmon/Source/World/ZM_PlayerHomePlacement.h"      // the PlayerHome shell + its ZM-D-176 warm tint
#include "Zenithmon/Source/World/ZM_PropFit.h"                  // roster-size fit for a prop model of ANY authored scale
// ★ UNCONDITIONAL, DELIBERATELY. Project_LoadInitialScene walks this table and is
// compiled in NON-TOOLS builds too (the ZENITH_TOOLS #endif sits immediately
// above it), so the one enumerable scene inventory cannot live in the tools block.
#include "Zenithmon/Source/World/ZM_SceneRegistry.h"            // the ONE enumerable scene-registration table (S8 item 2, R1-1)
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"

#include <string>

#ifdef ZENITH_INPUT_SIMULATOR
#include "Core/Zenith_AutomatedTest.h"
#endif

#ifdef ZENITH_TOOLS
#include "Core/Zenith_CommandLine.h"
#include "Editor/TerrainEditor/Zenith_TerrainEditor.h"         // Zenith_TerrainBrushTool::TreePaint (ZM-D-217 woodland)
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"   // the SC8 no-graph authoring pin
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "UI/Zenith_UIVirtualButton.h"                         // the B9 on-screen controls (WP3b authoring)
#include "UI/Zenith_UIVirtualStick.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"                 // build indices + spawn tags READ, never spelled (SC-E)
#include "Zenithmon/Source/World/ZM_DawnmereDressing.h"         // the scenery layer + the town keep-out (ZM-D-217)
#include "Zenithmon/Source/World/ZM_DawnmerePlacement.h"        // the shared authored coordinates (S7 item 3 SC8)
#include "Zenithmon/Source/World/ZM_ProfLabPlacement.h"         // the shared ProfLab interior coordinates (S8 SC1)
#include "Zenithmon/Source/World/ZM_Route1Placement.h"          // the shared Route 1 anchors (S8 item 2, R1-1/R1-2)
#include "Zenithmon/Source/World/ZM_TerrainAuthoring.h"
#include "Zenithmon/Source/World/ZM_ThornacrePlacement.h"       // the shared Thornacre stub anchors (S8 item 2, R1-1/R1-2)

#include <cstring>
#include <filesystem>
#include <string>
#endif

// ZM_GreyboxVisual -- the ONE visual component every authored Zenithmon entity
// wears, and the thing that decides what it looks like.
//
// It serves THREE POPULATIONS, and keeping them apart is the whole design:
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
//   PROP      -- an entity carrying a ZM_GroundItemProp (ZM-67). A generated
//                STATIC prop MODEL chosen per frame from the item the prop yields
//                and whether this save has already taken it. No skeleton, no
//                animator, and -- unlike HUMAN -- NO BODY: a ground-item prop
//                deliberately has no collider (ZM-D-207), so this branch installs
//                nothing physical and must never grow a call that does.
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
// ★ WHY THE PROP BRANCH IS THE ONLY ONE WITH AN OnUpdate, AND WHY IT IS AN UPDATE
// AND NOT A NOTIFICATION. A prop stops being takeable MID-SCENE, the moment the
// player picks it up, and ZM_GroundItemProp.h is explicit that the answer "is READ
// from the save rather than latched here". A hook fired from the pickup would latch
// it: any other route into the collected set -- a save loaded over a live scene, a
// debug grant -- would leave the picture claiming a pickup that is gone. So the
// visual asks the same predicate the picker asks, every frame, and rebuilds only
// when the ANSWER changes. OnUpdate returns on its first compare for every blockout
// and every human in the game; only the three Route 1 props go further, and only
// once each per playthrough do they do any work.
//
// NOTHING NEW IS SERIALIZED. WriteToDataStream still emits a single version u_int,
// so the committed .zscen bytes cannot move; everything below is re-derived on
// every load from bytes that were already there.
// ============================================================================
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
		// row and carries no ZM_PlayerController is a ground-item prop, and never
		// reaches a line of human code.
		const ZM_HUMAN_ID eHumanId = ResolveHumanId();
		if (eHumanId >= ZM_HUMAN_COUNT)
		{
			// ★★ THERE USED TO BE A THIRD POPULATION HERE -- BLOCKOUTS -- AND IT IS
			// GONE. Every wall, floor, door and lintel in the game wore a greybox
			// cube built by ApplyBlockout. Dawnmere's eight exterior blockouts became
			// collider-only under real building models, the two interiors' fourteen
			// became collider-only under real room models, and ZM_QueueGreyboxBlock
			// was deleted with its last caller. Nothing authored reaches this arm any
			// more, so the arm is gone rather than left as unreachable code that
			// still reads like a supported case.
			//
			// ★ AND THE REMAINING CASE IS AN ERROR, NOT A FALLBACK. This component is
			// now for humans and ground-item props ONLY. An entity carrying it that
			// is neither is a wiring mistake -- most likely a ZM_GroundItemProp that
			// was never added, or an NPC whose row stopped resolving -- and it used
			// to be silently absorbed as "it must be a wall", which is exactly how a
			// prop with a missing component became an anonymous grey cube nobody
			// questioned.
			if (m_xParentEntity.TryGetComponent<ZM_GroundItemProp>() != nullptr)
			{
				ApplyProp();
				return;
			}
			Zenith_Error(LOG_CATEGORY_GAMEPLAY,
				"[ZM_GreyboxVisual] entity '%s' carries this component but resolves to "
				"no human row and carries no ZM_GroundItemProp. Since the blockout arm "
				"was removed this component serves those two populations only -- the "
				"entity will render nothing",
				m_xParentEntity.GetName().c_str());
			return;
		}
		ApplyHuman(eHumanId);
	}

	// The ONLY per-frame work this component does, and it is gated to one population
	// on its first line. See the OnUpdate note in the class comment for why a prop's
	// look is polled rather than pushed.
	//
	// ★★ RECORDED HAZARD -- THIS GATE READS WHAT **OnStart** MANAGED TO DO, NOT WHAT
	// THE ENTITY IS. A prop that never reached ZM_VISUAL_PROP or _PROP_FALLBACK is
	// stuck at ZM_VISUAL_NONE, and NONE fails this compare on every frame for the
	// rest of the session: the poll never runs again and nothing retries. There are
	// exactly two ways in --
	//   * OnStart did not see a ZM_GroundItemProp on the entity (it takes the
	//     BLOCKOUT arm instead, and a blockout never polls);
	//   * ApplyProp ran, the model did not load, and the cold fallback's
	//     BuildMeshEntry ALSO returned false -- a null geometry asset, a null
	//     material, or a Flux_MeshGeometry that never built -- which leaves
	//     m_eLoadedKind untouched at NONE.
	//
	// ★ THE ONE OBSERVABLE SYMPTOM: the prop's picture stops tracking the save. Most
	// visibly, the player picks the item up -- the bag gains it and the collected set
	// records it -- and the prop on the ground still shows its PICKUP, forever,
	// offering something that is gone. (Or shows nothing at all, if the second case
	// left the entity with no mesh entry.)
	//
	// ★ AND THE ONLY TEST THAT COULD SEE IT IS BLIND WHERE CI LOOKS. This is a
	// pixels-on-screen failure; the tests that photograph these props carry
	// m_bRequiresGraphics, and a requiresGraphics test is SKIPPED-AS-PASSED on the
	// Null backend every gate runs on. Nothing headless can distinguish "the prop
	// re-evaluated and chose the same model" from "the prop stopped re-evaluating".
	//
	// ★ NOT REDESIGNED HERE, DELIBERATELY. The component-ORDER premise this rests on
	// -- that ZM_GroundItemProp is readable at order 107 -- predates ZM-67 and is
	// argued in full in the class comment above ApplyProp; re-entering the gate on an
	// entity that carries the component but is stuck at NONE is a behaviour change
	// with its own failure modes (a per-frame TryGetComponent on every blockout and
	// every human in the game, and a retry loop against a bake that is absent by
	// design on a cold clone). Recorded so the next reader is not surprised.
	void OnUpdate(float /*fDt*/)
	{
		if (m_eLoadedKind != ZM_VISUAL_PROP && m_eLoadedKind != ZM_VISUAL_PROP_FALLBACK)
		{
			return;
		}
		ApplyProp();   // a strict no-op unless the prop's desired MODEL has changed
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
		// m_eLoadedKind / m_eLoadedHumanId / m_eLoadedPropId / m_bAnimatorAuthored are
		// deliberately NOT cleared: they record what THIS instance actually did to the
		// model and the animator, which a stream read does not undo. A genuine scene
		// load builds a FRESH component, so it starts at NONE and takes the normal path.
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
		if (const ZM_GroundItemProp* pxProp =
			m_xParentEntity.TryGetComponent<ZM_GroundItemProp>())
		{
			// DERIVED live, like the blockout colour below and for the same reason: the
			// panel must show what the NEXT frame would present, not what the last one
			// happened to build.
			const ZM_PROP_ID eDesired = ZM_GroundItemPropModel(
				pxProp->GetGroundItemId(), pxProp->IsInteractable());
			ImGui::Text("Ground-item prop: %s (%s)",
				ZM_GetPropName(eDesired), KindName(m_eLoadedKind));
			ImGui::Text("Interactable: %s", pxProp->IsInteractable() ? "yes" : "no");
			const Zenith_Maths::Vector4 xPropColour = PropFallbackColour(eDesired);
			ImGui::Text("Cold-start fallback colour: %.3f, %.3f, %.3f",
				xPropColour.x, xPropColour.y, xPropColour.z);
			return;
		}

		// Neither a human nor a ground-item prop. That was the BLOCKOUT case until
		// the arm was removed; it is a wiring error now, and the panel says so
		// rather than describing a cube this component will not build.
		ImGui::TextUnformatted(
			"UNSERVED: no human row and no ZM_GroundItemProp -- nothing will render");
	}
#endif

private:
	// What this component last put on the entity. HUMAN_FALLBACK is a first-class
	// state, not an error: it is what a cold tree ships, and it must be able to
	// transition to HUMAN (and back) without stacking meshes.
	enum ZM_VISUAL_KIND : u_int
	{
		ZM_VISUAL_NONE,
		ZM_VISUAL_HUMAN_FALLBACK,
		ZM_VISUAL_HUMAN,
		ZM_VISUAL_PROP,
		ZM_VISUAL_PROP_FALLBACK,
	};

	static const char* KindName(ZM_VISUAL_KIND eKind)
	{
		switch (eKind)
		{
		case ZM_VISUAL_HUMAN_FALLBACK: return "cold fallback block";
		case ZM_VISUAL_HUMAN:          return "model";
		case ZM_VISUAL_PROP:           return "prop model";
		case ZM_VISUAL_PROP_FALLBACK:  return "cold fallback prop shape";
		default:                       return "nothing yet";
		}
	}

	// The material every PROP wears. NOT "ZM_Greybox": ZM_RivalVesperAuthored_Test
	// finds both the blockout and the human-fallback populations by that exact name
	// (this class is file-local and cannot be named from Tests/), so a third
	// population borrowing it would silently change what that test counts.
	static constexpr const char* szPROP_MATERIAL_NAME = "ZM_GroundItemProp";

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

	// ---- PROP: a generated model whose choice is re-derived every frame ------
	//
	// ★ IT MOVES NOTHING AND ADDS NO BODY. The authored transform -- the measured
	// ground column, the interact origin, the uniform fZM_ROUTE1_PROP_CUBE_EDGE
	// scale -- is read by nobody here and written by nobody here, and the generated
	// meshes are anchored at fZM_PROP_ITEM_BASE_Y precisely so they land on that
	// frozen transform without it having to move. No collider, no rigid body, no
	// InstallHumanBody sibling: ZM-D-207 rules a prop has no physics at all, and
	// ZM_Route1GroundTruth_Test treats a second SOLID body over a prop's own ground
	// column as a failure rather than something it filters out.
	//
	// ★ WHY READING ZM_GroundItemProp AT ORDER 107 IS SAFE, GIVEN IT IS 115. The
	// identical argument the class comment makes for ZM_Interactable at 113, and it
	// is stronger here: ZM_GroundItemProp::OnStart establishes NOTHING AT ALL (its
	// body is a comment saying so). The authored id arrives from ReadFromDataStream,
	// which provably runs for every component of an entity before any pending start
	// is dispatched, or from the AddStep_Custom authoring step, which runs with the
	// editor Stopped and no OnStart fired.
	//
	// ★ AND WHY A MISSING GAME STATE COSTS NOTHING. IsCollected() answers "not
	// collected" when no ZM_GameState is reachable -- the fresh-save ruling in
	// ZM_GroundItemProp.h -- so a prop started before the manager exists shows its
	// PICKUP. That is the right guess, and because this is a poll rather than a
	// latch the next frame corrects it for free if the save says otherwise. A
	// notification fired from the pickup would have had no way to.
	void ApplyProp()
	{
		// Re-resolved every call, never cached: component pools RELOCATE their
		// elements, so a pointer held across a frame is a dangling pointer waiting.
		const ZM_GroundItemProp* pxProp =
			m_xParentEntity.TryGetComponent<ZM_GroundItemProp>();
		if (pxProp == nullptr)
		{
			return;   // the component was removed under us; leave whatever is drawn
		}

		// ★ ONE PREDICATE, ASKED -- NOT A SECOND OPINION. IsInteractable() is the
		// same answer ZM_InteractionRuntime gates the interact press on, so the thing
		// the player sees and the thing the player can press E at cannot disagree.
		const ZM_PROP_ID eDesired = ZM_GroundItemPropModel(
			pxProp->GetGroundItemId(), pxProp->IsInteractable());

		const bool bAlreadyPresenting =
			(m_eLoadedKind == ZM_VISUAL_PROP || m_eLoadedKind == ZM_VISUAL_PROP_FALLBACK);
		if (bAlreadyPresenting && m_eLoadedPropId == eDesired)
		{
			return;   // the common frame: no LoadModel, no AddMeshEntry, no stat
		}

		if (m_eLoadedKind == ZM_VISUAL_NONE)
		{
			// ★ WARM THE **OTHER** STATE AT LOAD, NOT AT PICKUP. The spent model is
			// wanted the instant the player presses E, and generating it on that frame
			// would put a hitch exactly where the game is meant to feel responsive.
			// Tools-only and a four-stat no-op once the bundle is on disk; the result is
			// ignored for the same reason it is ignored in ApplyPropModel.
			(void)ZM_EnsurePropBaked(
				ZM_GroundItemPropModel(pxProp->GetGroundItemId(), false));
		}

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}

		if (ApplyPropModel(*pxModel, eDesired))
		{
			m_eLoadedKind   = ZM_VISUAL_PROP;
			m_eLoadedPropId = eDesired;
			return;
		}

		// Cold, or the model refused to load. ★ CLEAR FIRST, for the reason the human
		// path states AND one more: LoadModel REFUSES a missing file WITHOUT clearing,
		// so on a live -> spent swap whose spent bundle is absent the OLD pickup model
		// is still on the entity, and AddMeshEntry would stack the fallback on top of
		// it -- a prop that reads as taken AND as takeable at the same time.
		if (m_eLoadedKind != ZM_VISUAL_NONE)
		{
			pxModel->ClearModel();
		}
		m_xGeometry = PropFallbackGeometry(eDesired);
		if (!BuildMeshEntry(szPROP_MATERIAL_NAME, PropFallbackColour(eDesired),
			fPROP_ROUGHNESS, fPROP_METALLIC))
		{
			return;
		}
		m_eLoadedKind   = ZM_VISUAL_PROP_FALLBACK;
		m_eLoadedPropId = eDesired;
	}

	// Load one generated prop bundle. False if it did not land, in which case the
	// caller falls back to the cold shape.
	bool ApplyPropModel(Zenith_ModelComponent& xModel, ZM_PROP_ID eProp)
	{
		char acModelRef[256];
		if (!ZM_PropAssetPath(eProp, ZM_PROP_ASSET_MODEL, acModelRef,
			static_cast<u_int>(sizeof(acModelRef))))
		{
			return false;
		}

		// Tools-only and a four-stat no-op once the bundle is on disk; an inline
		// `return false` everywhere else. The RESULT IS DELIBERATELY IGNORED: on
		// Android whatever shipped in the APK IS the bake (the ruling
		// ZM_HumanAssetPolicy.cpp records), so treating "this build cannot bake" as
		// "the model is absent" would refuse assets that are sitting right there.
		(void)ZM_EnsurePropBaked(eProp);

		xModel.LoadModel(std::string(acModelRef));

		// ★ THE PATH IS THE ONLY HONEST SIGNAL, NOT HasModel(). LoadModel refuses a
		// missing file before it clears anything, so HasModel() can still be true --
		// describing the model this call was meant to REPLACE. Zenith_ModelComponent
		// assigns m_strModelPath on its success path and nowhere else.
		if (xModel.GetModelPath() != acModelRef || xModel.GetNumMeshes() == 0u)
		{
			Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
				"[ZM_GreyboxVisual] prop model '%s' did not load; using the cold-start "
				"shape", acModelRef);
			return false;
		}

		// The material handle belongs to the fallback path; a model carries its own.
		m_xMaterial = MaterialHandle();
		m_xGeometry = MeshGeometryHandle();
		return true;
	}

	// The cold-start shape, for a tree that has never run a tools build.
	//
	// ★ THREE DISTINCT SHAPES, NOT THREE COLOURS. The point of the ticket is that the
	// presentations are tellable apart at walking distance, and a cold tree is still
	// a tree somebody plays; three identically-shaped blocks in three colours would
	// fail that the moment the sky is re-tuned.
	//
	// ★ IT PRESERVES THE **PICKUP** SILHOUETTES AND DELIBERATELY NOT THE SPENT ONE,
	// AND THE ASYMMETRY IS FORCED. A capsule for the phial and a sphere for the orb
	// are fair stand-ins for those two baked compositions -- tall-and-thin against
	// squat-and-round, the same read. The SPENT tray is not: its roster row is 0.22
	// tall (a flat, open tray) and Zenith_MeshGeometryAsset::CreateUnitCylinder is
	// radius 0.5 by height 1.0, so the cold spent prop is a full-height drum where
	// the baked one is a saucer. It is a THIRD distinct shape in the STONE grey, so
	// "taken" still reads as neither pickup, but its FLATNESS is a bake-only
	// property. Do not describe a cold capture as showing the spent silhouette.
	//
	// ★ AND THAT CANNOT BE FIXED BY SHRINKING THE CYLINDER, WHICH IS WHY IT WAS NOT.
	// Every shape here has to be centred on the origin spanning [-0.5, +0.5] (see
	// below); a 0.22-tall tray centred on the origin spans [-0.11, +0.11] and would
	// HOVER 0.39 of a cube above the measured surface, because there is nothing in
	// this path that can offset a geometry downward -- the entity transform is frozen
	// and shared with the baked model. A hovering prop is a worse cold start than a
	// too-tall one.
	//
	// ★ EVERY ONE OF THESE IS CENTRED ON THE ORIGIN SPANNING [-0.5, +0.5]. That is
	// the same anchor fZM_PROP_ITEM_BASE_Y gives the baked meshes and the same one
	// the blockout unit cube had, so the fallback stands exactly where the cube stood
	// -- on the measured surface, not hovering over it. CreateUnitCone is
	// deliberately NOT used: it spans [0, 1] and would float by half a cube.
	static MeshGeometryHandle PropFallbackGeometry(ZM_PROP_ID eProp)
	{
		switch (eProp)
		{
		case ZM_PROP_ITEM_ORB:   return Zenith_MeshGeometryAsset::CreateUnitSphere(16u);
		case ZM_PROP_ITEM_PHIAL: return Zenith_MeshGeometryAsset::CreateUnitCapsule(16u);
		default:                 return Zenith_MeshGeometryAsset::CreateUnitCylinder(16u);
		}
	}

	// ★ DERIVED FROM THE ROSTER ROW, NEVER CHOSEN HERE. The fallback wears the same
	// palette family the bake would have painted, so the two presentations cannot
	// drift into a prop that changes colour the moment a bake appears -- and there
	// is no colour constant in this file for a test to accidentally pin.
	static Zenith_Maths::Vector4 PropFallbackColour(ZM_PROP_ID eProp)
	{
		if (static_cast<u_int>(eProp) >= static_cast<u_int>(ZM_PROP_COUNT))
		{
			return Zenith_Maths::Vector4(ZM_PropPaletteColour(ZM_PROP_PALETTE_STONE), 1.0f);
		}
		return Zenith_Maths::Vector4(
			ZM_PropPaletteColour(ZM_GetPropData(eProp).m_ePalette), 1.0f);
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
	//
	// The surface constants live here, spelled once. The BLOCKOUT pair are the
	// values that branch has always carried and must not move (ZM_AutoTests_
	// InteriorTint measures its base colour to 1.0e-4); the PROP pair are separate
	// numbers for a separate population, so tuning one can never disturb the other.
	static constexpr float fBLOCK_ROUGHNESS = 0.90f;
	static constexpr float fBLOCK_METALLIC  = 0.0f;
	static constexpr float fPROP_ROUGHNESS  = 0.80f;   // matches the baked prop .zmtrl
	static constexpr float fPROP_METALLIC   = 0.0f;

	bool BuildBlockMesh(const Zenith_Maths::Vector4& xBaseColour)
	{
		return BuildMeshEntry("ZM_Greybox", xBaseColour, fBLOCK_ROUGHNESS, fBLOCK_METALLIC);
	}

	// The ONE place a procedurally-built mesh entry is put on the entity. Extracted
	// from BuildBlockMesh so the PROP fallback can wear its own material NAME and
	// surface without a second copy of the create/fetch-or-add/AddMeshEntry dance --
	// a second copy is how the "AddMeshEntry APPENDS" trap gets re-introduced. The
	// blockout call above passes exactly the literals it always passed.
	bool BuildMeshEntry(const char* szMaterialName,
		const Zenith_Maths::Vector4& xBaseColour, float fRoughness, float fMetallic)
	{
		m_xMaterial = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>();
		Zenith_MeshGeometryAsset* pxGeometryAsset = m_xGeometry.GetDirect();
		Zenith_MaterialAsset* pxMaterial = m_xMaterial.GetDirect();
		if (pxGeometryAsset == nullptr || pxMaterial == nullptr)
		{
			return false;
		}

		pxMaterial->SetName(szMaterialName);
		ApplyAppearance(*pxMaterial, xBaseColour);
		pxMaterial->SetRoughness(fRoughness);
		pxMaterial->SetMetallic(fMetallic);

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

	Zenith_Entity m_xParentEntity;
	MeshGeometryHandle m_xGeometry;
	MaterialHandle m_xMaterial;
	// Runtime-only and NOT reset by ReadFromDataStream: they record what THIS
	// instance did to the model and the animator, which a stream read does not undo.
	ZM_VISUAL_KIND m_eLoadedKind = ZM_VISUAL_NONE;
	ZM_HUMAN_ID m_eLoadedHumanId = ZM_HUMAN_NONE;
	// Which generated prop model is on the entity RIGHT NOW. The sentinel is the
	// default so the first ApplyProp can never mistake "nothing loaded" for "already
	// showing prop 0" -- ZM_PROP_ITEM_PHIAL would otherwise be indistinguishable
	// from an untouched instance and the first frame would draw nothing.
	ZM_PROP_ID m_eLoadedPropId = ZM_PROP_NONE;
	bool m_bAnimatorAuthored = false;
};


// ============================================================================
// ZM_BuildingFacade -- the PICTURE a Dawnmere blockout is wearing.
//
// A visual-only presenter: it owns nothing but a Zenith_ModelComponent carrying
// the generated multi-surface building model (wall / roof / trim / glass, each
// with its own full PBR material). The physics belongs to the sibling blockout
// entity, which is why nothing here touches a collider.
//
// ★★ WHY THIS COMPONENT EXISTS AT ALL, RATHER THAN AN AUTHORED MODEL. The
// authoring could simply AddStep_AddModel + AddStep_LoadModel and be done. That
// was written first, and it broke the scene-byte invariant:
// Zenith_ModelComponent::WriteToDataStream writes the model GUID and then EVERY
// MATERIAL INLINE, so how many bytes a facade occupies depends on how much of the
// bundle had resolved when the scene was saved. Two consecutive authoring boots
// produced 79,058 and then 82,152 bytes for the SAME 40 entities. A committed
// scene that grows by three kilobytes on its second boot is the exact failure
// ZM-D-179 and ZM-D-183 were about, and it would additionally freeze a copy of
// every material into the scene where a later material edit could never reach it.
//
// The scene therefore stores a component name and a version u_int, and the model
// is resolved and loaded HERE, at OnStart. That is not a special case -- it is
// what every model-bearing entity in this game already does, which is why a
// Zenithmon .zscen contains no game:Props/... or game:Humans/... reference.
//
// ★ THE BUILDING IS RESOLVED FROM THE ENTITY NAME (ZM_DawnmereFacades.h), the
// same way ZM_GreyboxVisual resolves a human and the PlayerHome tint. Nothing is
// serialized but the version, so this component cannot make the scene bytes move.
// ============================================================================
class ZM_BuildingFacade
{
public:
	ZM_BuildingFacade() = delete;
	explicit ZM_BuildingFacade(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	ZM_BuildingFacade(const ZM_BuildingFacade&) = delete;
	ZM_BuildingFacade& operator=(const ZM_BuildingFacade&) = delete;
	ZM_BuildingFacade(ZM_BuildingFacade&&) noexcept = default;
	ZM_BuildingFacade& operator=(ZM_BuildingFacade&&) noexcept = default;

	void OnStart()
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}

		const ZM_BUILDING_ID eBuilding =
			ZM_BuildingForFacadeEntity(m_xParentEntity.GetName().c_str());
		if (eBuilding >= ZM_BUILDING_COUNT)
		{
			// TOTAL, and LOUD. A facade entity whose name no longer maps is a
			// building that silently does not render -- the one failure mode a
			// headless test cannot see, because the entity, its transform and its
			// sibling collider are all still perfectly correct.
			Zenith_Error(LOG_CATEGORY_MESH,
				"[ZM_BuildingFacade] entity '%s' carries a facade component but maps "
				"to no building (see Source/World/ZM_DawnmereFacades.h) -- its "
				"blockout will collide and show nothing",
				m_xParentEntity.GetName().c_str());
			return;
		}

		char acModelRef[512];
		if (!ZM_BuildingAssetPath(eBuilding, ZM_BUILDING_ASSET_MODEL, acModelRef,
			static_cast<u_int>(sizeof(acModelRef))))
		{
			return;
		}

		// Tools-only and a stat-per-artifact no-op once the bundle is on disk; an
		// inline `return false` everywhere else. The RESULT IS DELIBERATELY IGNORED,
		// for the reason ZM_GreyboxVisual::ApplyPropModel records: on Android
		// whatever shipped in the APK IS the bake, so treating "this build cannot
		// bake" as "the model is absent" would refuse assets that are present.
		(void)ZM_EnsureBuildingBaked(eBuilding);

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}
		pxModel->LoadModel(std::string(acModelRef));

		// ★ THE PATH IS THE ONLY HONEST SIGNAL, NOT HasModel(). LoadModel refuses a
		// missing file before it clears anything, so HasModel() can still be true
		// while describing whatever the component held before. Zenith_ModelComponent
		// assigns m_strModelPath on its success path and nowhere else.
		if (pxModel->GetModelPath() != acModelRef || pxModel->GetNumMeshes() == 0u)
		{
			Zenith_Warning(LOG_CATEGORY_MESH,
				"[ZM_BuildingFacade] '%s' did not load for entity '%s'; the blockout "
				"will collide and show nothing. On a clone with no bake this is "
				"expected -- every generated asset in this game is gitignored",
				acModelRef, m_xParentEntity.GetName().c_str());
			return;
		}
		m_bLoaded = true;
		// Logged on SUCCESS as well as failure, deliberately. This is a purely
		// visual component with no gameplay effect: if it silently does nothing the
		// only symptom is a building that is not there, and every headless test in
		// the game still passes. One INFO line per facade per scene load is what
		// makes "did the house load?" answerable from a log rather than a screenshot.
		Zenith_Log(LOG_CATEGORY_MESH,
			"[ZM_BuildingFacade] '%s' loaded '%s' (%u submeshes)",
			m_xParentEntity.GetName().c_str(), acModelRef, pxModel->GetNumMeshes());
	}

	// Read by ZM_Tests/automated coverage: did the picture actually arrive?
	bool IsLoaded() const { return m_bLoaded; }

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// ★ A VERSION AND NOTHING ELSE, DELIBERATELY. Every byte this writes lands
		// in a COMMITTED scene, and the building id is already a pure function of
		// the entity name. Serializing it too would create a second inventory
		// nothing reconciles -- and one that could disagree with the name.
		xStream << 1u;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0u;
		xStream >> uVersion;
		(void)uVersion;
		// m_bLoaded is NOT cleared: it records what THIS instance did to the model,
		// which a stream read does not undo. A genuine scene load builds a FRESH
		// component, so it starts false and takes the normal path.
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		const ZM_BUILDING_ID eBuilding =
			ZM_BuildingForFacadeEntity(m_xParentEntity.GetName().c_str());
		ImGui::Text("Building: %s",
			eBuilding < ZM_BUILDING_COUNT ? ZM_GetBuildingName(eBuilding) : "<unmapped>");
		ImGui::Text("Model loaded: %s", m_bLoaded ? "yes" : "no");
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	bool m_bLoaded = false;
};

// ============================================================================
// ZM_InteriorShell -- the ROOM a set of interior blockouts is wearing.
//
// The interior twin of ZM_BuildingFacade, and it exists for exactly the same two
// reasons, which are worth not re-deriving:
//
//   * The blockouts must stay MODEL-FREE so their AABB colliders keep being
//     sized from transform scale alone. Those seven boxes per room are the walls
//     the player stops against, and Zenith_ColliderComponent switches to
//     mesh-bounds sizing the moment a Zenith_ModelComponent appears -- falling
//     back to a UNIT CUBE if the mesh has not streamed yet, which would put a
//     1 m collider where a 16 m wall should be, non-deterministically.
//
//   * The model must NOT be authored into the scene. Zenith_ModelComponent
//     serializes every material INLINE, so the committed byte count would track
//     asset load state; the exterior slice measured that at 79,058 bytes on one
//     boot and 82,152 on the next for the same entity set.
//
// So the scene carries a component name and a version u_int, and the room model
// is resolved from the entity name here at OnStart.
// ============================================================================
class ZM_InteriorShell
{
public:
	ZM_InteriorShell() = delete;
	explicit ZM_InteriorShell(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	ZM_InteriorShell(const ZM_InteriorShell&) = delete;
	ZM_InteriorShell& operator=(const ZM_InteriorShell&) = delete;
	ZM_InteriorShell(ZM_InteriorShell&&) noexcept = default;
	ZM_InteriorShell& operator=(ZM_InteriorShell&&) noexcept = default;

	void OnStart()
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}

		const ZM_INTERIOR_ROOM eRoom =
			ZM_RoomForShellEntity(m_xParentEntity.GetName().c_str());
		if (eRoom >= ZM_INTERIOR_ROOM_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_MESH,
				"[ZM_InteriorShell] entity '%s' carries a shell component but maps to "
				"no room (see Source/World/ZM_InteriorDressing.h) -- its blockouts will "
				"collide and show nothing", m_xParentEntity.GetName().c_str());
			return;
		}

		char acModelRef[512];
		if (!ZM_InteriorAssetPath(eRoom, ZM_INTERIOR_ASSET_MODEL, acModelRef,
			static_cast<u_int>(sizeof(acModelRef))))
		{
			return;
		}

		// Tools-only, a stat-per-artifact no-op once warm, and the RESULT IS
		// DELIBERATELY IGNORED for the reason ZM_GreyboxVisual::ApplyPropModel
		// records: on Android whatever shipped in the APK IS the bake.
		(void)ZM_EnsureInteriorBaked(eRoom);

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}
		pxModel->LoadModel(std::string(acModelRef));

		// The PATH is the only honest signal, not HasModel(): LoadModel refuses a
		// missing file before it clears anything.
		if (pxModel->GetModelPath() != acModelRef || pxModel->GetNumMeshes() == 0u)
		{
			Zenith_Warning(LOG_CATEGORY_MESH,
				"[ZM_InteriorShell] '%s' did not load for entity '%s'; the room will be "
				"an empty collider box. On a clone with no bake this is expected -- "
				"every generated asset in this game is gitignored",
				acModelRef, m_xParentEntity.GetName().c_str());
			return;
		}
		m_bLoaded = true;
		// Logged on SUCCESS too: this is a purely visual component, so if it
		// silently does nothing the only symptom is a room that is not there and
		// every headless test still passes.
		Zenith_Log(LOG_CATEGORY_MESH,
			"[ZM_InteriorShell] '%s' loaded '%s' (%u submeshes)",
			m_xParentEntity.GetName().c_str(), acModelRef, pxModel->GetNumMeshes());
	}

	bool IsLoaded() const { return m_bLoaded; }

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// A version and nothing else. The room is already a pure function of the
		// entity name; serializing it too would be a second inventory nothing
		// reconciles, and one that could disagree with the name.
		xStream << 1u;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0u;
		xStream >> uVersion;
		(void)uVersion;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		const ZM_INTERIOR_ROOM eRoom =
			ZM_RoomForShellEntity(m_xParentEntity.GetName().c_str());
		ImGui::Text("Room: %s",
			eRoom < ZM_INTERIOR_ROOM_COUNT ? ZM_InteriorRoomName(eRoom) : "<unmapped>");
		ImGui::Text("Model loaded: %s", m_bLoaded ? "yes" : "no");
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	bool m_bLoaded = false;
};

// ============================================================================
// ZM_InteriorFurniture -- one piece of furniture standing in an interior room.
//
// The third member of the same family as ZM_BuildingFacade and ZM_InteriorShell,
// and it resolves and loads the same way: the scene carries a component name and
// a version u_int, and the prop id comes from the entity's name
// (ZM_InteriorDressing.h). See ZM_BuildingFacade for why a model is never
// authored into a committed scene.
//
// ★★ FURNITURE IS SOLID, AND THE ORDER OF OPERATIONS IS THE WHOLE TRICK.
//
// An earlier draft made these visual-only, on the reasoning that
// Zenith_ColliderComponent serializes only the volume and body TYPE -- so
// explicit half-extents cannot survive a save, and a loaded scene re-derives the
// box from mesh bounds or, when no mesh has arrived yet, from a UNIT CUBE. That
// is true, and it turned out not to be the obstacle it looked like.
//
// The sequence on a scene load is: the collider deserializes and calls
// AddCollider (no ModelComponent exists yet, so it sizes a 1 m cube), and THEN
// this component's OnStart runs and loads the model. The sizing is therefore
// wrong only in the window between those two, and RebuildCollider() closes it --
// it re-runs ComputeBoxDimensionsAndOffset with the mesh present, taking the
// mesh-aware branch: half-extents = meshExtents * scale, plus a local offset that
// recentres the box on the mesh. The prop meshes are authored in real metres at
// scale 1, so that is exactly the furniture's own footprint.
//
// ★ IT IS CALLED ONLY ON THE SUCCESS PATH, and that matters. Rebuilding after a
// FAILED load would re-derive from whatever stale or absent mesh was there and
// leave a 1 m cube standing in the middle of a bed nobody can see. On a
// bake-less clone the furniture keeps its authored sizing and stays invisible,
// which is the same degradation every other generated asset in this game has.
//
// ★ AND THE CORRIDOR RULE IS A SAFETY PROPERTY NOW, not a tidiness one. The
// shared walk driver has NO OBSTACLE AVOIDANCE (map playbook 3.4): a collider on
// the line wedges a traversal test into its frame cap with a failure naming a
// DISTANCE rather than the blocker. Every prop clears
// fZM_INTERIOR_CORRIDOR_HALF_WIDTH and
// ZM_Interaction/InteriorPropsClearTheEntranceCorridor enforces it -- that clause
// existed before these colliders did, which is what made adding them safe.
// ============================================================================
class ZM_InteriorFurniture
{
public:
	ZM_InteriorFurniture() = delete;
	explicit ZM_InteriorFurniture(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	ZM_InteriorFurniture(const ZM_InteriorFurniture&) = delete;
	ZM_InteriorFurniture& operator=(const ZM_InteriorFurniture&) = delete;
	ZM_InteriorFurniture(ZM_InteriorFurniture&&) noexcept = default;
	ZM_InteriorFurniture& operator=(ZM_InteriorFurniture&&) noexcept = default;

	void OnStart()
	{
		if (!m_xParentEntity.IsValid())
		{
			return;
		}

		// ★★ TWO TABLES, COMPOSED HERE. This component's job is "wear the prop your
		// NAME resolves to, then re-size the collider to it", and that is as true of
		// a barrel against a house in Dawnmere as of a bed in a bedroom. Each
		// dressing header owns the resolver for its OWN table -- making the interior
		// one walk the Dawnmere table would make an interior room include a town --
		// so the composition happens at the one place that sees both.
		//
		// ★ THE NAME NO LONGER DESCRIBES EVERY USER, AND THAT IS A KNOWN DEBT
		// rather than an oversight. Renaming the class would change the component
		// NAME string that PlayerHome.zscen and ProfLab.zscen serialize, so it costs
		// a re-author of both plus matched edits to the meta AND editor registries
		// -- and a component present in one registry but not the other authors a
		// scene WITHOUT it, byte-stably, with every gate green. Worth doing; not
		// worth doing in the same change that first places a prop outdoors.
		const char* szName = m_xParentEntity.GetName().c_str();
		ZM_PROP_ID eProp = ZM_PropForInteriorPropEntity(szName);
		if (eProp >= ZM_PROP_COUNT)
		{
			eProp = ZM_PropForDawnmerePropEntity(szName);
		}
		if (eProp >= ZM_PROP_COUNT)
		{
			Zenith_Error(LOG_CATEGORY_MESH,
				"[ZM_InteriorFurniture] entity '%s' carries a furniture component but "
				"maps to no prop (see ZM_InteriorDressing.h for the two rooms, "
				"ZM_DawnmereDressing.h for the outdoor table) -- it will stand "
				"showing nothing", szName);
			return;
		}

		char acModelRef[512];
		if (!ZM_PropAssetPath(eProp, ZM_PROP_ASSET_MODEL, acModelRef,
			static_cast<u_int>(sizeof(acModelRef))))
		{
			return;
		}
		(void)ZM_EnsurePropBaked(eProp);

		Zenith_ModelComponent* pxModel =
			m_xParentEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr)
		{
			pxModel = &m_xParentEntity.AddComponent<Zenith_ModelComponent>();
		}
		pxModel->LoadModel(std::string(acModelRef));

		if (pxModel->GetModelPath() != acModelRef || pxModel->GetNumMeshes() == 0u)
		{
			Zenith_Warning(LOG_CATEGORY_MESH,
				"[ZM_InteriorFurniture] '%s' did not load for entity '%s'. On a clone "
				"with no bake this is expected -- every generated asset is gitignored",
				acModelRef, m_xParentEntity.GetName().c_str());
			return;
		}
		m_bLoaded = true;

		// ★ NOW that the mesh is present, re-size the collider to it. Until this
		// line the box is the 1 m cube AddCollider built during deserialization,
		// when no ModelComponent existed. See the class comment for why this is on
		// the success path only.
		if (Zenith_ColliderComponent* pxCollider =
			m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>())
		{
			pxCollider->RebuildCollider();
		}
	}

	bool IsLoaded() const { return m_bLoaded; }

	void WriteToDataStream(Zenith_DataStream& xStream) const { xStream << 1u; }
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0u;
		xStream >> uVersion;
		(void)uVersion;
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		const ZM_PROP_ID eProp =
			ZM_PropForInteriorPropEntity(m_xParentEntity.GetName().c_str());
		ImGui::Text("Prop: %s",
			eProp < ZM_PROP_COUNT ? ZM_GetPropName(eProp) : "<unmapped>");
		ImGui::Text("Model loaded: %s", m_bLoaded ? "yes" : "no");
	}
#endif

private:
	Zenith_Entity m_xParentEntity;
	bool m_bLoaded = false;
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
ZENITH_REGISTER_COMPONENT(ZM_TouchLayoutController, "ZM_TouchLayoutController", 114u)
ZENITH_REGISTER_COMPONENT(ZM_GroundItemProp, "ZM_GroundItemProp", 115u)
ZENITH_REGISTER_COMPONENT(ZM_BuildingFacade, "ZM_BuildingFacade", 116u)
ZENITH_REGISTER_COMPONENT(ZM_InteriorShell, "ZM_InteriorShell", 117u)
ZENITH_REGISTER_COMPONENT(ZM_InteriorFurniture, "ZM_InteriorFurniture", 118u)

#ifdef ZENITH_TOOLS
namespace
{
	// Every terrain recipe gets its own four live material assets, indexed
	// [recipe index][slot]. The recipe index is ZM_GetTerrainAuthoringRecipe()'s
	// registry index -- 0 = Dawnmere, 1 = Thornacre, 2 = Route1 -- and
	// Project_InitializeResources walks it in ASCENDING order.
	//
	// ★ THAT ORDER, AND DAWNMERE'S CREATION ARGUMENTS, ARE LOAD-BEARING.
	// Dawnmere.zscen is a COMMITTED file that has drifted twice already (ZM-D-179
	// and ZM-D-183, both with every existing guard green), and
	// Zenith_TerrainComponent::WriteToDataStream INLINES each slot's whole material
	// payload -- name, params, texture refs -- into the scene bytes. So:
	//   * Dawnmere is recipe 0, so its four Create<Zenith_MaterialAsset>() calls
	//     stay FIRST and keep the exact arguments they had when the file was last
	//     authored. Never create a terrain material ahead of them, never reorder
	//     this loop, and never "tidy" Dawnmere's specs in ZM_TerrainAuthoring.cpp.
	//   * A material carries no creation-ordered identity into the scene TODAY
	//     (the registry's counter-derived "procedural://asset_N" id lives on the
	//     asset's own m_strPath, which WriteToDataStream never writes, and the
	//     Flux material-table index is not serialized either). If that ever
	//     changes, this fixed ascending order is what keeps Dawnmere's bytes put.
	// Route1 and Thornacre rows exist so a later slice can author their scenes;
	// creating them costs four handles each and touches no committed file.
	constexpr u_int uZM_TERRAIN_MATERIAL_SLOT_COUNT = 4u;
	MaterialHandle g_aaxTerrainMaterials[uZM_TERRAIN_RECIPE_COUNT][uZM_TERRAIN_MATERIAL_SLOT_COUNT];

	// Addressed by the RECIPE itself rather than a second 0/1/2 mapping kept in
	// sync by hand: ZM_GetTerrainAuthoringRecipe()'s registry order is the single
	// source of truth for a recipe's index, and the registry is a function-local
	// static array, so its rows have stable addresses for the process lifetime.
	const MaterialHandle* ZM_GetTerrainMaterialsForRecipe(
		const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		const u_int uRecipeCount = ZM_GetTerrainAuthoringRecipeCount();
		for (u_int uRecipe = 0; uRecipe < uRecipeCount; ++uRecipe)
		{
			if (&ZM_GetTerrainAuthoringRecipe(uRecipe) == &xRecipe)
			{
				return g_aaxTerrainMaterials[uRecipe];
			}
		}
		Zenith_Assert(false,
			"Recipe is not a row of the terrain authoring registry");
		return g_aaxTerrainMaterials[0];
	}

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

	// The S8 starter-choice screen, authored WHOLE like the bag / shop / save screens: a
	// centred panel, the prompt header and ONE CELL PER STARTER in a single VERTICAL
	// column (ZM-D-188 -- vertical is what lets the picker ride the engine's existing
	// focus navigation with ZERO new input actions). Every cell stays VISIBLE + FOCUSABLE
	// at runtime (CellIsAlwaysShown): all three are always confirmable, so nothing is ever
	// disarmed by hiding. There is NO Back element -- cancel is a deliberate no-op on this
	// screen. All geometry is read off the ZM_UI_StarterChoice f*_ constants so this site
	// and the presenter cannot drift apart. Same 9000/9001 sort band, ALL authored HIDDEN.
	void ZM_ConfigureMenuRootStarterScreen(Zenith_UIComponent& xUI)
	{
		Zenith_UI::Zenith_UIRect* pxPanel =
			xUI.FindElement<Zenith_UI::Zenith_UIRect>(ZM_UI_StarterChoice::szPANEL_NAME);
		if (pxPanel != nullptr)
		{
			pxPanel->SetSortOrder(ZM_UI_MenuStack::iMENU_PANEL_SORT_ORDER);
			pxPanel->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxPanel->SetPosition(0.0f, 0.0f);
			// Fully COVERS the header band and the whole cell column (ZM-D-112).
			pxPanel->SetSize(ZM_UI_StarterChoice::fPANEL_WIDTH, ZM_UI_StarterChoice::fPANEL_HEIGHT);
			pxPanel->SetColor({ 0.05f, 0.06f, 0.10f, 0.85f });
			pxPanel->SetVisible(false);
		}

		Zenith_UI::Zenith_UIText* pxHeader =
			xUI.FindElement<Zenith_UI::Zenith_UIText>(ZM_UI_StarterChoice::szHEADER_NAME);
		if (pxHeader != nullptr)
		{
			pxHeader->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxHeader->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxHeader->SetPosition(0.0f, ZM_UI_StarterChoice::fHEADER_CENTRE_Y);
			// Size == wrap width == SetMaxWidth with a matching alignment (the SC2 lesson).
			pxHeader->SetSize(ZM_UI_StarterChoice::fHEADER_WIDTH, ZM_UI_StarterChoice::fHEADER_HEIGHT);
			pxHeader->SetFontSize(24.0f);
			pxHeader->SetAlignment(Zenith_UI::TextAlignment::Center);
			pxHeader->SetMaxWidth(ZM_UI_StarterChoice::fHEADER_WIDTH);
			pxHeader->SetVisible(false);
		}

		// The cells form ONE vertical column at x == 0. The explicit up/down links mirror
		// that geometry for this live authoring session; they are not serialized, so runtime
		// navigation uses the spatial fallback and walks the same order. Unlike the shop /
		// bag lists there is no liveness problem to worry about: no cell is ever hidden.
		Zenith_UI::Zenith_UIButton* apxCells[ZM_UI_StarterChoice::uCELL_COUNT] = {};
		for (u_int uCell = 0u; uCell < ZM_UI_StarterChoice::uCELL_COUNT; ++uCell)
		{
			Zenith_UI::Zenith_UIButton* pxCell =
				xUI.FindElement<Zenith_UI::Zenith_UIButton>(ZM_UI_StarterChoice::CellElementName(uCell));
			apxCells[uCell] = pxCell;
			if (pxCell == nullptr)
			{
				continue;
			}
			pxCell->SetSortOrder(ZM_UI_MenuStack::iMENU_BUTTON_SORT_ORDER);
			pxCell->SetAnchor(Zenith_UI::AnchorPreset::Center);
			pxCell->SetPivot(Zenith_UI::AnchorPreset::Center);
			pxCell->SetPosition(0.0f,
				ZM_UI_StarterChoice::fCELL_FIRST_CENTRE_Y
					+ ZM_UI_StarterChoice::fCELL_PITCH_Y * static_cast<float>(uCell));
			pxCell->SetSize(ZM_UI_StarterChoice::fCELL_WIDTH, ZM_UI_StarterChoice::fCELL_HEIGHT);
			pxCell->SetFontSize(22.0f);
			pxCell->SetFocusable(true);
			// The species name is written at RUNTIME by ZM_UI_StarterChoice::Present, so the
			// committed scene bytes carry the element NAME and an EMPTY label.
			pxCell->SetVisible(false);
		}

		for (u_int uCell = 0u; uCell < ZM_UI_StarterChoice::uCELL_COUNT; ++uCell)
		{
			if (apxCells[uCell] == nullptr)
			{
				continue;
			}
			Zenith_UI::Zenith_UIElement* pxUp = (uCell > 0u) ? apxCells[uCell - 1u] : nullptr;
			Zenith_UI::Zenith_UIElement* pxDown =
				(uCell + 1u < ZM_UI_StarterChoice::uCELL_COUNT) ? apxCells[uCell + 1u] : nullptr;
			apxCells[uCell]->SetNavigation(pxUp, pxDown, nullptr, nullptr);
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

	// WP3b (B11): the four on-screen controls on the persistent ZM_TouchRoot entity's
	// UI component. Stick bottom-LEFT, A/B bottom-RIGHT, MENU top-right, ALL AUTHORED
	// HIDDEN (ZM_TouchLayoutController shows the ones the current context wants on its
	// first OnUpdate). Sort band 9500: ABOVE the menu's 9000/9001 so a control is never
	// buried under the dialogue box it exists to advance, BELOW the fades' 10000/10001.
	//
	// ★ EVERY NUMBER HERE IS AN INTEGER-VALUED CONSTANT, AND THAT IS A HARD RULE, NOT A
	// STYLE. These values land in the COMMITTED FrontEnd.zscen. ZM-D-183 cost a cycle
	// because an authored value was computed at runtime (atan2 -> angleAxis) and MSVC
	// Debug and Release codegen disagreed by 1-2 ULP, so the file ping-ponged in git
	// forever while every tolerance guard stayed green. An integer is exact in every FP
	// model and every configuration; a computed float is not. Do not introduce a
	// screen-size-derived, scale-derived or otherwise arithmetic authored value here --
	// the DISPLAY SCALE is applied at USE time by the widgets themselves (they take
	// LOGICAL pixels), which is precisely why authoring never needs to know it.
	void ZM_ConfigureTouchControls()
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		Zenith_UIComponent* pxUI = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<Zenith_UIComponent>()
			: nullptr;
		Zenith_Assert(pxUI != nullptr,
			"TouchRoot authoring requires the selected root UI component");
		if (pxUI == nullptr)
		{
			return;
		}

		// The thumbstick: a 160-square well inside the bottom-left corner, 48 px clear
		// of both edges. Anchor AND pivot are BottomLeft, so +x reaches right and -y
		// reaches UP (canvas +y is down).
		Zenith_UI::Zenith_UIVirtualStick* pxStick =
			pxUI->FindElement<Zenith_UI::Zenith_UIVirtualStick>(
				ZM_TouchLayoutController::szSTICK_NAME);
		if (pxStick != nullptr)
		{
			pxStick->SetSortOrder(ZM_TouchLayoutController::iTOUCH_CONTROL_SORT_ORDER);
			pxStick->SetAnchorAndPivot(Zenith_UI::AnchorPreset::BottomLeft);
			// BottomLeft pivot puts the element's BOTTOM edge at (H + y), so -48
			// leaves a 48 px margin and the 160 tall body reaches up to H - 208.
			pxStick->SetPosition(48.0f, -48.0f);
			pxStick->SetSize(160.0f, 160.0f);
			pxStick->SetVisible(false);
		}

		// The two action buttons, bottom-RIGHT, A outermost (the thumb's resting
		// position) and B inboard of it with a 16 px gap. Anchor AND pivot BottomRight,
		// so -x reaches left and -y reaches up.
		struct TouchButton
		{
			const char* m_szName;
			float m_fX;
			float m_fY;
			float m_fSize;
			Zenith_UI::AnchorPreset m_ePreset;
		};
		const TouchButton axButtons[3] =
		{
			// A: right edge at W - 48, bottom edge at H - 48, 112 square.
			{ ZM_TouchLayoutController::szBUTTON_A_NAME,    -48.0f,   -48.0f, 112.0f,
				Zenith_UI::AnchorPreset::BottomRight },
			// B: right edge at W - 176 (a 16 px gap left of A), bottom at H - 80, 96 square.
			{ ZM_TouchLayoutController::szBUTTON_B_NAME,   -176.0f,   -80.0f,  96.0f,
				Zenith_UI::AnchorPreset::BottomRight },
			// MENU is top-right per B11, so its pivot flips to the TOP edge and +y
			// reaches DOWN from it.
			{ ZM_TouchLayoutController::szBUTTON_MENU_NAME, -32.0f,    32.0f,  96.0f,
				Zenith_UI::AnchorPreset::TopRight },
		};
		for (const TouchButton& xButton : axButtons)
		{
			Zenith_UI::Zenith_UIVirtualButton* pxButton =
				pxUI->FindElement<Zenith_UI::Zenith_UIVirtualButton>(xButton.m_szName);
			if (pxButton == nullptr)
			{
				continue;
			}
			pxButton->SetSortOrder(ZM_TouchLayoutController::iTOUCH_CONTROL_SORT_ORDER);
			pxButton->SetAnchorAndPivot(xButton.m_ePreset);
			pxButton->SetPosition(xButton.m_fX, xButton.m_fY);
			pxButton->SetSize(xButton.m_fSize, xButton.m_fSize);
			pxButton->SetVisible(false);
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
		ZM_ConfigureMenuRootStarterScreen(*pxUI);
		// LAST, because it is the only one that REPARENTS elements.
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

	// ============================================================================
	// SC-E -- THE THREE CONFIGURE STEPS THAT CLOSE THE LAB SEAM.
	//
	// ★★ NOT ONE OF THEM SPELLS A BUILD INDEX OR A TAG, AND THAT IS THE POINT.
	// The shipped Home pair above still writes `40u` and `2u` as literals -- the
	// older and weaker pattern, left alone rather than churned. These three read the
	// COMPILED world table instead, so the mutation "the configure function writes
	// the PlayerHome build index" is not a thing a reviewer has to notice: there is
	// no index here to get wrong. ZM_GetProfLabExitTargetBuildIndex /
	// ZM_GetProfLabExitSpawnTag walk the ZM_SCENE_PROFLAB row for the edge that
	// targets Dawnmere (Source/World/ZM_ProfLabPlacement.h), and the Dawnmere-side
	// sensor reads ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex directly.
	//
	// ★ AND THE TWO SIDES SHARE ONE SPELLING OF THE TAG. ZM_GetProfLabExitSpawnTag()
	// is what the ProfLab exit ASKS Dawnmere for AND what the Dawnmere marker
	// OFFERS. A tag that matched the compiled table on one side only would still
	// pass ZM_GameStateManager::IsWarpDestinationValid -- it consults that table and
	// never the scene -- and then park the warp machine in
	// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN. That barrier now carries a frame budget
	// (ZM-D-200), so the mismatch stalls behind an opaque fade, then names itself in
	// a Zenith_Error and gives the screen back -- loud and recoverable instead of a
	// frozen player forever, but still a lab door that leads nowhere.
	// ============================================================================

	// Dawnmere's return marker: where a player who leaves the lab comes out.
	void ZM_ConfigureFromLabSpawnPoint()
	{
		Zenith_Assert(ZM_SetSelectedSpawnPointTag(ZM_GetProfLabExitSpawnTag()),
			"the ProfLab->Dawnmere connection's spawn tag is not a valid spawn tag");
	}

	// Dawnmere's lab doorway sensor: into ProfLab, at ProfLab's own arrival tag.
	void ZM_ConfigureLabDoorTrigger()
	{
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex,
				szZM_PROFLAB_SPAWN_TAG),
			"Dawnmere Lab doorway warp configuration is invalid");
	}

	// ...and the other half of that door: ProfLab's exit sensor, back to Dawnmere.
	// The resolution is checked BEFORE the configure call rather than left to
	// Configure()'s return, so a compiled table with no ProfLab->Dawnmere edge names
	// itself instead of surfacing as a generic "warp configuration is invalid".
	void ZM_ConfigureProfLabExitTrigger()
	{
		const u_int uTargetBuildIndex = ZM_GetProfLabExitTargetBuildIndex();
		Zenith_Assert(uTargetBuildIndex != uZM_PROFLAB_EXIT_TARGET_UNRESOLVED,
			"the compiled ZM_SCENE_PROFLAB row carries no connection targeting "
			"ZM_SCENE_DAWNMERE, so the lab has no way out to resolve");
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				uTargetBuildIndex, ZM_GetProfLabExitSpawnTag()),
			"ProfLab exit warp configuration is invalid");
	}

	// ============================================================================
	// R1-2 -- THE THREE ARRIVAL TAGS, AND THE ONE CONFUSION THAT BREAKS THE GAME.
	//
	// ★★ AN ARRIVAL MARKER CARRIES AN **INBOUND** TAG: the tag asked for by the
	// scene the player is ARRIVING FROM, not by any edge leaving the scene the
	// marker sits in. Read the compiled table (Source/Data/ZM_WorldSpec.cpp) and
	// the trap is obvious in one direction and invisible in the other:
	//
	//     Dawnmere  offers { ROUTE1, "FromDawnmere" }
	//     Thornacre offers { ROUTE1, "FromThornacre" }, { GYM1, "Door" }
	//     Route1    offers { DAWNMERE, "FromRoute1" }, { THORNACRE, "FromRoute1" }
	//
	// So Route 1's SOUTH arrival must carry "FromDawnmere", its NORTH arrival
	// "FromThornacre", and Thornacre's arrival "FromRoute1".
	//
	// ★ THE INVISIBLE HALF: ZM_GetRoute1SouthGateSpawnTag() and
	// ZM_GetRoute1NorthGateSpawnTag() BOTH answer "FromRoute1" -- they are what
	// Route 1's own gates ASK their destinations for (OUTBOUND; the four gate
	// configure steps below). Reaching for the "south gate" accessor while authoring the south
	// ARRIVAL marker is the natural mistake, and it does not even change the text
	// on screen: it produces a Route 1 whose two markers both offer a tag Route 1
	// does not itself offer. ZM_GameStateManager::IsWarpDestinationValid would
	// still return TRUE -- it consults ONLY the compiled table, never the
	// destination scene -- and the warp machine would then stall in
	// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN behind an opaque fade until that
	// barrier's frame budget expires (ZM-D-200), then escape with a Zenith_Error
	// naming the tag it never found. Still not a crash and still not a red test --
	// but you find out, instead of watching a frozen player forever.
	//
	// The resolver below removes the choice. It takes the SOURCE region and this
	// region, walks the SOURCE row's connection list for the edge whose target is
	// this region, and hands back what the TABLE says -- so a table edit that
	// re-tagged a seam moves the authoring with it, and no tag is ever spelled at
	// a call site. Each of the three steps beneath it names its own source, which
	// is the one fact a reviewer has to check.
	//
	// TOTAL, and it NEVER asserts: ZM_GetWorldSpec asserts FATALLY out of range and
	// Zenith_Assert breaks in every configuration, so both ids are range-guarded
	// BEFORE the call. A miss yields "" -- never nullptr -- which ZM_SpawnPoint::
	// SetTag rejects, so the per-marker assertion below fires and names the seam
	// instead of authoring a silently untagged marker.
	// ============================================================================
	const char* ZM_ResolveInboundSpawnTag(
		ZM_SCENE_ID eSource, ZM_SCENE_ID eDestination)
	{
		if (eSource >= ZM_SCENE_COUNT || eDestination >= ZM_SCENE_COUNT)
		{
			return "";
		}

		const ZM_WorldSpec& xRow = ZM_GetWorldSpec(eSource);
		if (xRow.m_pxConnections == nullptr)
		{
			return "";
		}
		for (u_int uEdge = 0u; uEdge < xRow.m_uConnectionCount; ++uEdge)
		{
			if (xRow.m_pxConnections[uEdge].m_eTarget == eDestination
				&& xRow.m_pxConnections[uEdge].m_szSpawnTag != nullptr)
			{
				return xRow.m_pxConnections[uEdge].m_szSpawnTag;
			}
		}
		return "";
	}

	// Route 1's SOUTH arrival: where a player walking north out of Dawnmere lands.
	// The source is DAWNMERE, so the tag is Dawnmere's ROUTE1 edge ("FromDawnmere").
	void ZM_ConfigureRoute1SouthArrivalSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag(
			ZM_ResolveInboundSpawnTag(ZM_SCENE_DAWNMERE, ZM_SCENE_ROUTE1));
		Zenith_Assert(bTagSet,
			"the compiled Dawnmere->Route1 edge carries no usable spawn tag, so "
			"Route 1's south arrival marker has nothing valid to offer");
	}

	// Route 1's NORTH arrival: where a player walking south out of Thornacre lands.
	// The source is THORNACRE, so the tag is Thornacre's ROUTE1 edge
	// ("FromThornacre") -- NOT Route 1's own north gate tag.
	void ZM_ConfigureRoute1NorthArrivalSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag(
			ZM_ResolveInboundSpawnTag(ZM_SCENE_THORNACRE, ZM_SCENE_ROUTE1));
		Zenith_Assert(bTagSet,
			"the compiled Thornacre->Route1 edge carries no usable spawn tag, so "
			"Route 1's north arrival marker has nothing valid to offer");
	}

	// Thornacre's arrival: where a player walking north off Route 1 lands. The
	// source is ROUTE1, so the tag is Route 1's THORNACRE edge ("FromRoute1").
	void ZM_ConfigureThornacreSouthArrivalSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag(
			ZM_ResolveInboundSpawnTag(ZM_SCENE_ROUTE1, ZM_SCENE_THORNACRE));
		Zenith_Assert(bTagSet,
			"the compiled Route1->Thornacre edge carries no usable spawn tag, so "
			"Thornacre's arrival marker has nothing valid to offer");
	}

	// Dawnmere's arrival (R1-2 step 3): where a player walking SOUTH off Route 1
	// lands. The source is ROUTE1, so the tag is Route 1's DAWNMERE edge
	// ("FromRoute1") -- the fourth and last of the R1-2 seam markers.
	//
	// ★ THE SOURCE IS ROUTE1 EVEN THOUGH THE MARKER STANDS IN DAWNMERE, and the
	// natural mistake is the other order. ZM_ResolveInboundSpawnTag(DAWNMERE,
	// ROUTE1) resolves the edge LEAVING Dawnmere and answers "FromDawnmere" --
	// which is the tag Route 1's OWN south arrival marker carries, already
	// authored a few hundred lines below. Getting it that way round would put two
	// markers in the world offering "FromDawnmere" and none offering "FromRoute1",
	// and IsWarpDestinationValid would STILL return true because it consults only
	// the compiled table and never the destination scene. Read the resolver's
	// banner above before touching this.
	//
	// ★ AND IT IS THE SAME EDGE, SO THE SAME ONE SPELLING, THAT ROUTE 1'S SOUTH
	// GATE ASKS DAWNMERE FOR (ZM_GetRoute1SouthGateSpawnTag walks the identical
	// row; R1-3 authored that sensor). One table entry feeds both halves of the
	// seam, which is the property that makes the marker and the sensor impossible
	// to mis-pair.
	void ZM_ConfigureDawnmereFromRoute1ArrivalSpawnPoint()
	{
		const bool bTagSet = ZM_SetSelectedSpawnPointTag(
			ZM_ResolveInboundSpawnTag(ZM_SCENE_ROUTE1, ZM_SCENE_DAWNMERE));
		Zenith_Assert(bTagSet,
			"the compiled Route1->Dawnmere edge carries no usable spawn tag, so "
			"Dawnmere's Route 1 arrival marker has nothing valid to offer");
	}

	// ============================================================================
	// R1-3 -- THE FOUR SEAM GATE CONFIGURE STEPS, AND WHY THEY ARE OUTBOUND
	//
	// ★★ A GATE CARRIES AN **OUTBOUND** TAG: the tag ITS OWN region's connection
	// list asks the DESTINATION for. That is the exact mirror of the four ARRIVAL
	// markers above, which carry INBOUND tags resolved from the SOURCE region's
	// row, and the two are trivially confusable because three of the eight strings
	// involved are the literal text "FromRoute1". Read the compiled table
	// (Source/Data/ZM_WorldSpec.cpp) once and the pairing is forced:
	//
	//     Dawnmere  -> Route1     asks "FromDawnmere"   (Route 1 OFFERS it)
	//     Route1    -> Dawnmere   asks "FromRoute1"     (Dawnmere OFFERS it)
	//     Route1    -> Thornacre  asks "FromRoute1"     (Thornacre OFFERS it)
	//     Thornacre -> Route1     asks "FromThornacre"  (Route 1 OFFERS it)
	//
	// ★★ NOT ONE OF THESE FOUR SPELLS A BUILD INDEX OR A TAG, and on this seam that
	// is load-bearing rather than stylistic. Route 1's two gates ask for the SAME
	// TAG as each other and differ ONLY in their target build index (2 vs 3), so a
	// south/north SWAP changes not one byte of any name-based needle -- it produces
	// a Route 1 whose south gate leads to Thornacre and whose north gate leads to
	// Dawnmere, reading perfectly at every call site. The resolvers remove the
	// choice: each walks its own region's row FOR A NAMED TARGET SCENE, so the one
	// fact a reviewer has to check is the target named in each function below.
	// Tests/ZM_Tests_CommittedSceneBytes.cpp needles the whole serialized payload
	// -- [version][targetBuildIndex][32-byte tag] -- for exactly that reason.
	//
	// ★ EACH CHECKS ITS RESOLUTION **BEFORE** THE CONFIGURE CALL, in the shape
	// ZM_ConfigureProfLabExitTrigger established, so a compiled table with the edge
	// missing names ITSELF instead of surfacing as a generic "warp configuration is
	// invalid" from ZM_WarpTrigger::Configure's return.
	// ============================================================================

	// Route 1's SOUTH gate: back down to Dawnmere, at Dawnmere's "FromRoute1".
	void ZM_ConfigureRoute1SouthGateTrigger()
	{
		const u_int uTargetBuildIndex = ZM_GetRoute1SouthGateTargetBuildIndex();
		Zenith_Assert(uTargetBuildIndex != uZM_ROUTE1_GATE_TARGET_UNRESOLVED,
			"the compiled ZM_SCENE_ROUTE1 row carries no connection targeting "
			"ZM_SCENE_DAWNMERE, so Route 1's south gate has no destination to "
			"resolve");
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				uTargetBuildIndex, ZM_GetRoute1SouthGateSpawnTag()),
			"Route 1 south gate warp configuration is invalid");
	}

	// Route 1's NORTH gate: on to Thornacre, at Thornacre's "FromRoute1". Same
	// tag as the south gate, DIFFERENT build index -- see the banner.
	void ZM_ConfigureRoute1NorthGateTrigger()
	{
		const u_int uTargetBuildIndex = ZM_GetRoute1NorthGateTargetBuildIndex();
		Zenith_Assert(uTargetBuildIndex != uZM_ROUTE1_GATE_TARGET_UNRESOLVED,
			"the compiled ZM_SCENE_ROUTE1 row carries no connection targeting "
			"ZM_SCENE_THORNACRE, so Route 1's north gate has no destination to "
			"resolve");
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				uTargetBuildIndex, ZM_GetRoute1NorthGateSpawnTag()),
			"Route 1 north gate warp configuration is invalid");
	}

	// ---- The three Route 1 ground-item props (ZM-27 follow-up (a)) -------
	//
	// ★ THE ID IS WHAT MAKES A PROP A PROP, and it is the only thing these steps
	// author. Position, scale and the blockout visual are ordinary transform and
	// component steps; WHICH prop this entity is comes from the save-stable
	// ZM_GROUND_ITEM_ID written onto the component here and serialized with it.
	// A prop authored without this step is an unconfigured component that reports
	// itself non-interactable, which is exactly what SetGroundItemId's fail-closed
	// contract is for -- but it would still be a prop nobody can pick up, so the
	// assert below is a hard one.
	bool ZM_ConfigureSelectedGroundItemProp(ZM_GROUND_ITEM_ID eId)
	{
		Zenith_Entity* pxSelectedEntity = g_xEngine.Editor().GetSelectedEntity();
		ZM_GroundItemProp* pxProp = pxSelectedEntity != nullptr
			? pxSelectedEntity->TryGetComponent<ZM_GroundItemProp>()
			: nullptr;
		Zenith_Assert(pxProp != nullptr,
			"Ground-item prop authoring requires the selected ZM_GroundItemProp");
		return pxProp != nullptr && pxProp->SetGroundItemId(eId);
	}

	void ZM_ConfigureRoute1SouthSalveProp()
	{
		Zenith_Assert(
			ZM_ConfigureSelectedGroundItemProp(ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE),
			"Route 1 south salve prop configuration is invalid");
	}

	void ZM_ConfigureRoute1LaneCatchorbProp()
	{
		Zenith_Assert(
			ZM_ConfigureSelectedGroundItemProp(ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB),
			"Route 1 lane catchorb prop configuration is invalid");
	}

	void ZM_ConfigureRoute1NorthSalveProp()
	{
		Zenith_Assert(
			ZM_ConfigureSelectedGroundItemProp(ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE),
			"Route 1 north salve prop configuration is invalid");
	}

	// Thornacre's return gate: back down to Route 1, at Route 1's "FromThornacre".
	// The ONE entity that makes the stub traversable rather than a dead end.
	void ZM_ConfigureThornacreSouthGateTrigger()
	{
		const u_int uTargetBuildIndex = ZM_GetThornacreReturnTargetBuildIndex();
		Zenith_Assert(uTargetBuildIndex != uZM_THORNACRE_RETURN_TARGET_UNRESOLVED,
			"the compiled ZM_SCENE_THORNACRE row carries no connection targeting "
			"ZM_SCENE_ROUTE1, so the town has no way back out to resolve");
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				uTargetBuildIndex, ZM_GetThornacreReturnSpawnTag()),
			"Thornacre return gate warp configuration is invalid");
	}

	// Dawnmere's north gate: out onto Route 1, at Route 1's "FromDawnmere". The
	// fourth seam, and the only one whose sensor lands in a scene that was already
	// committed before this slice.
	void ZM_ConfigureDawnmereNorthGateTrigger()
	{
		const u_int uTargetBuildIndex = ZM_GetDawnmereNorthGateTargetBuildIndex();
		Zenith_Assert(
			uTargetBuildIndex != uZM_DAWNMERE_NORTH_GATE_TARGET_UNRESOLVED,
			"the compiled ZM_SCENE_DAWNMERE row carries no connection targeting "
			"ZM_SCENE_ROUTE1, so Dawnmere's north gate has no destination to "
			"resolve");
		Zenith_Assert(
			ZM_ConfigureSelectedWarpTrigger(
				uTargetBuildIndex, ZM_GetDawnmereNorthGateSpawnTag()),
			"Dawnmere north gate warp configuration is invalid");
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
	// neighbour's press: the closest NPC PAIR in this town is 13.4 m apart, 4.6x the
	// effective reach (ZM-D-217 compacted the town; the RATIO is the property, and
	// it moved from 5.5x on a core 1.8x wider).
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

	// S8: ProfLab's Professor Aster. The FIRST authored NPC that does not live in
	// Dawnmere, and authoring-wise the plainest of the lot: a stationary talker with
	// no story gate, no patrol, no sight cone and no trainer, so the whole of his
	// behaviour is the compiled ZM_NPC_PROF_ASTER row and none of it is an extra
	// authoring step. A per-NPC function is unavoidable regardless --
	// AddStep_Custom takes a captureless void (*)() and cannot be handed the row it
	// should install.
	void ZM_ConfigureProfAsterNpc()
	{
		const bool bConfigured = ZM_ConfigureSelectedNpc(ZM_NPC_PROF_ASTER);
		Zenith_Assert(bConfigured, "ProfLab Professor Aster NPC authoring is invalid");

		// The row really is a TALKER and really does name the professor's appearance
		// -- authoring fails loudly here rather than standing up a mute or
		// wrong-looking figure whose only symptom is a silent interact press.
		Zenith_Assert(ZM_GetNpcData(ZM_NPC_PROF_ASTER).m_eRole == ZM_NPC_ROLE_TALKER,
			"Aster's ZM_NpcData row is no longer a TALKER, so the authored NPC in "
			"ProfLab would not open dialogue when the player interacts with him");
		Zenith_Assert(ZM_GetNpcData(ZM_NPC_PROF_ASTER).m_eHuman == ZM_HUMAN_PROF_ASTER,
			"Aster's ZM_NpcData row no longer names ZM_HUMAN_PROF_ASTER, so "
			"ZM_GreyboxVisual would dress the authored professor as somebody else");
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

	// The ONE authored TRANSFORM SCALE every human in this game wears -- Dawnmere's
	// player and six NPCs, and ProfLab's Professor Aster. Named here, rather than
	// re-spelled, because it is written from a DIFFERENT function than the placement
	// blocks below.
	//
	// ★ IT IS A DRAWING SCALE, NOT A BODY. It exists to land the generated human
	// MODEL on the body contract, and it is UNIFORM -- which is precisely why the
	// bodies can no longer be derived from it (a uniform scale degenerates a
	// scale-derived capsule into a sphere). Anything that needs to know how big a
	// person is reads Source/World/ZM_HumanBody.h; the bodies themselves are
	// installed explicitly from that same contract at runtime.
	const Zenith_Maths::Vector3 g_xZMHumanVisualScale(fZM_HUMAN_VISUAL_SCALE);

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

	// One authored STATIONARY TALKER: a greybox body the player can SEE, a STATIC
	// AABB it can physically bump into (so walking up to one ends in contact rather
	// than in walking through it), the ZM_Interactable that makes it talkable, and
	// the captureless step that installs its row. Step order mirrors HomeDoorTrigger
	// -- transform, collider, components, then the configure custom step.
	//
	// ★ SCENE-AGNOSTIC BY NAME AS WELL AS BY CODE. It was ZM_QueueDawnmereNpc while
	// the town was the only place with NPCs in it, and the name no longer claims a
	// scene. Its callers today are the FOUR shipped Dawnmere townsfolk, and nothing
	// about the nine steps has ever changed, which is what keeps their committed
	// bytes where they are.
	//
	// ★ IT EMITS NO ROTATION STEP, AND EVERY CALLER DEPENDS ON THAT. The AABB on
	// the line below forces its Jolt body to identity and the physics->transform
	// sync writes that identity back into the SAVED BYTES (ZM-D-156), so a caller
	// who needs a facing must not use this helper -- use
	// ZM_QueueFacingStationaryTalkerNpc (OBB + a frozen quaternion) for a talker who
	// stands still and faces somewhere, or ZM_QueueDawnmereTrainerNpc for one who
	// also walks. ProfLab's Professor Aster USED to be authored here; SC-E moved him
	// to the facing helper when it turned him round to greet the arriving player,
	// and that move is the whole reason the facing helper exists.
	void ZM_QueueStationaryTalkerNpc(
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

	// ---- SC-E: the stationary talker who DOES face somewhere -----------------
	//
	// ★ A THIRD HELPER, AND NOT A PARAMETER ON THE ONE ABOVE. Adding an optional
	// facing to ZM_QueueStationaryTalkerNpc would mean the four shipped Dawnmere
	// townsfolk's step lists changed shape in the one file whose output is a
	// COMMITTED asset -- and their AABB is CORRECT for them, so the change would be
	// churn on bytes that must not move. Their nine steps stay untouchable BY
	// CONSTRUCTION rather than by an argument about angleAxis(0).
	//
	// The differences from the stationary helper are exactly two, and both are
	// forced by the facing:
	//
	// (1) COLLISION_VOLUME_TYPE_OBB, NOT AABB. Zenith_ColliderComponent's body
	//     creation reads
	//         const JPH::Quat xJoltRot = (eVolumeType == COLLISION_VOLUME_TYPE_AABB)
	//             ? JPH::Quat::sIdentity() : JPH::Quat(...);
	//     -- an AABB is axis-aligned BY DEFINITION, so it forces the body to
	//     identity and the physics->transform sync writes that identity straight
	//     back over the rotation this function just authored, INTO THE SAVED BYTES,
	//     with every pure unit still green (they read the compiled constants; the
	//     damage lives in the file). That is ZM-D-156, already paid for once on
	//     rival Vesper. OBB is the same box shape and differs ONLY in applying the
	//     rotation, which is precisely the case for a talker who must hold a facing
	//     and must never move -- so this helper does NOT reach for the rival's
	//     DYNAMIC CAPSULE either: that shape exists because the rival WALKS, and a
	//     dynamic body under a professor could be shoved off the anchor every
	//     clearance figure in ZM_ProfLabPlacement.h is derived at.
	//
	// (2) AddStep_SetTransformRotationQuat WITH A FROZEN QUATERNION (ZM-D-183). The
	//     yaw and euler steps run glm::angleAxis engine-side and MSVC Debug and
	//     Release disagree on that sin/cos by 1-2 ULP, so a COMMITTED scene authored
	//     through them ping-pongs in git forever -- invisibly to the same-binary
	//     pre-save guard, which compares the serialized bytes against a value that
	//     moved with them. This step performs no math at all.
	//
	// The rotation step sits between scale and collider, exactly where
	// ZM_QueueDawnmereTrainerNpc puts it and for the same reason: the transform is
	// fully authored before the body exists, so SetRotation lands the frozen bits in
	// the cache while there is still no body to normalize them.
	void ZM_QueueFacingStationaryTalkerNpc(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const Zenith_Maths::Vector3& xCenter,
		const Zenith_Maths::Vector3& xScale,
		const Zenith_Maths::Quat& xFacing,
		void (*pfnConfigure)())
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		xAuto.AddStep_SetTransformRotationQuat(
			xFacing.x, xFacing.y, xFacing.z, xFacing.w);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
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
	// Adding a yaw parameter to ZM_QueueStationaryTalkerNpc instead was considered and
	// REJECTED. AddStep_SetTransformYaw(0.0f) does build an exact identity
	// quaternion, so the four shipped NPCs' bytes probably would not move -- but SC8
	// is the one sub-commit that rewrites a committed scene file, and their step
	// lists must be untouchable BY CONSTRUCTION rather than by an argument about
	// angleAxis(0). The rotation step sits between scale and collider so the
	// transform is fully authored before the body is created -- which is also what
	// keeps ZM-D-183's frozen bits intact: SetRotation lands them in the cache while
	// there is still no body to normalize them.
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
		const Zenith_Maths::Quat& xFacing,
		void (*pfnConfigure)())
	{
		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(xCenter.x, xCenter.y, xCenter.z);
		xAuto.AddStep_SetTransformScale(xScale.x, xScale.y, xScale.z);
		// ★ ZM-D-183: A QUATERNION, VERBATIM -- NOT AddStep_SetTransformYaw. The
		// yaw step runs glm::angleAxis engine-side, and that sin/cos differs by
		// 1-2 ULP between Debug and Release codegen, so this committed scene used
		// to author different bytes depending on which tools build ran it. This
		// step performs no math. See Source/World/ZM_DawnmerePlacement.h.
		xAuto.AddStep_SetTransformRotationQuat(
			xFacing.x, xFacing.y, xFacing.z, xFacing.w);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_Interactable");
		// NO AddStep_AttachGraph. See ZM_ConfigureRivalVesperNpc.
		xAuto.AddStep_Custom(pfnConfigure);
	}

	// ---- A blockout that COLLIDES and is never seen --------------------------
	//
	// ★★ WHY THE VISUAL AND THE COLLIDER ARE TWO ENTITIES, AND WHY THIS ONE MUST
	// CARRY NO MODEL. Zenith_ColliderComponent::ComputeBoxDimensionsAndOffset is
	// MESH-AWARE: when the entity has a Zenith_ModelComponent whose mesh exposes
	// bounds, the box is meshExtents * transformScale; only when there is NO model
	// does it fall back to the unit-cube assumption that makes half-extents exactly
	// scale/2. Every Dawnmere clearance constant, the camera clamp and the keep-out
	// were measured against that fallback, so hanging a real building model on the
	// shell entity would silently redefine all of them.
	//
	// ★ AND THE FAILURE WOULD BE INTERMITTENT. Explicit half-extents
	// (SetExplicitBoxHalfExtents) are NOT serialized -- ZM_ColliderComponent's
	// stream carries only the volume/body pair -- so a LOADED scene rebuilds the
	// box from whatever the model reports at that instant. If the mesh has not
	// streamed in yet the code takes the unit-cube branch, and a 17 m building
	// gets a 1 m collider on some boots and not others.
	//
	// So: this entity is the volume the player may not enter, exactly as it always
	// was, byte for byte. The picture is a sibling (ZM_QueueBuildingFacade) with a
	// model, scale 1 and no collider at all.
	void ZM_QueueColliderBlock(
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
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	}

	// ---- The transform that makes a prop model its roster size ---------------
	//
	// Reads the prop's BAKED .zmesh off disk and measures it, then asks
	// ZM_ComputePropFit for the uniform scale and ground lift that turn that
	// measurement into the roster row's size, standing on y = 0.
	//
	// ★ HERMETIC: a Zenith_DataStream + ParseStream, never
	// Zenith_AssetRegistry::Acquire. This runs while an authoring step list is
	// being BUILT, and warming the registry with a mesh at that moment is how a
	// scene's byte count starts depending on which assets happened to be resident
	// when it was saved -- the failure ZM_BuildingFacade's header records at
	// length (79,058 bytes on one boot, 82,152 on the next, same 40 entities).
	// ZM_Tests_PropBake.cpp reads a baked .zmodel the same way and for the same
	// reason.
	//
	// ★ TOTAL: no bake, an unreadable .zmesh or a garbage id all answer the
	// IDENTITY transform -- which is precisely what the authoring emitted before
	// this function existed. A clone with no asset bake must still author a
	// complete, loadable scene; the prop simply renders nothing, which is the same
	// degradation every other generated asset in this game has.
	ZM_PropFit ZM_ResolvePropFit(ZM_PROP_ID eProp)
	{
		ZM_PropFit xFit;   // identity

		if (static_cast<u_int>(eProp) >= static_cast<u_int>(ZM_PROP_COUNT))
		{
			return xFit;
		}
		(void)ZM_EnsurePropBaked(eProp);

		char acMeshRef[512];
		if (!ZM_PropAssetPath(eProp, ZM_PROP_ASSET_MESH, acMeshRef, sizeof(acMeshRef)))
		{
			return xFit;
		}

		const std::string strMeshFs =
			Zenith_AssetRegistry::ResolvePath(std::string(acMeshRef));

		Zenith_DataStream xStream;
		xStream.ReadFromFile(strMeshFs.c_str());
		if (!xStream.IsValid())
		{
			Zenith_Warning(LOG_CATEGORY_MESH,
				"[Zenithmon] ZM_ResolvePropFit: '%s' is not readable, so '%s' is "
				"authored at identity scale. On a clone with no bake this is expected",
				strMeshFs.c_str(), ZM_GetPropName(eProp));
			return xFit;
		}

		Zenith_MeshAsset xMesh;
		if (!xMesh.ParseStream(xStream).IsOk() || xMesh.GetNumVerts() == 0u)
		{
			Zenith_Warning(LOG_CATEGORY_MESH,
				"[Zenithmon] ZM_ResolvePropFit: '%s' did not parse as a mesh, so '%s' "
				"is authored at identity scale", strMeshFs.c_str(), ZM_GetPropName(eProp));
			return xFit;
		}

		const ZM_PropData& xData = ZM_GetPropData(eProp);
		xFit = ZM_ComputePropFit(
			xMesh.GetBoundsMin(), xMesh.GetBoundsMax(),
			xData.m_fWidth, xData.m_fDepth, xData.m_fHeight);

		// OBSERVED, not asserted. A hand-made model's fit is the one number a
		// reviewer needs to see to tell "the asset is the wrong size" from "the
		// authoring is wrong", and it costs one line per prop at author time.
		const Zenith_Maths::Vector3 xFitted =
			ZM_FittedPropSize(xMesh.GetBoundsMin(), xMesh.GetBoundsMax(), xFit);
		Zenith_Log(LOG_CATEGORY_MESH,
			"[Zenithmon] PROP FIT '%s': model %.4f x %.4f x %.4f m -> scale %.4f, "
			"ground y %.4f -> %.4f x %.4f x %.4f m (roster %.2f x %.2f x %.2f)",
			ZM_GetPropName(eProp),
			(double)(xMesh.GetBoundsMax().x - xMesh.GetBoundsMin().x),
			(double)(xMesh.GetBoundsMax().y - xMesh.GetBoundsMin().y),
			(double)(xMesh.GetBoundsMax().z - xMesh.GetBoundsMin().z),
			(double)xFit.m_fScale, (double)xFit.m_fGroundY,
			(double)xFitted.x, (double)xFitted.y, (double)xFitted.z,
			(double)xData.m_fWidth, (double)xData.m_fDepth, (double)xData.m_fHeight);

		return xFit;
	}

	// ---- Everything that turns an interior blockout box into a room ----------
	//
	// One shell entity (the generated floor/wall/ceiling/trim model), the room's
	// furniture, and its lights -- queued in that fixed order, which is part of
	// the scene-byte contract (ZM-D-148 dense authoring-order file indices):
	// appending is fine, REORDERING rewrites the committed bytes.
	//
	// ★ THE LIGHTS ARE THE HALF THAT CANNOT BE SKIPPED. Zenithmon authored no
	// Zenith_LightComponent anywhere before this, so both interiors were lit by
	// the global ambient term alone -- a constant with no direction, which no
	// amount of PBR material can look like anything but flat under. SSAO has no
	// shading to darken and CSM has no caster in a scene with no lights.
	void ZM_QueueInteriorDressing(Zenith_EditorAutomation& xAuto, ZM_INTERIOR_ROOM eRoom)
	{
		// The shell. Visual-only, at the room origin, scale 1 -- the mesh is
		// authored in real metres and the seven blockouts keep the collision.
		if (!ZM_EnsureInteriorBaked(eRoom))
		{
			Zenith_Error(LOG_CATEGORY_MESH,
				"[Zenithmon] ZM_QueueInteriorDressing: room %u could not be baked, so "
				"its shell would be authored against a missing model",
				(u_int)eRoom);
		}
		const char* szShell = (eRoom == ZM_INTERIOR_ROOM_PROF_LAB)
			? szZM_PROFLAB_SHELL_ENTITY_NAME
			: szZM_PLAYERHOME_SHELL_ENTITY_NAME;
		xAuto.AddStep_CreateEntity(szShell);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(0.0f, 0.0f, 0.0f);
		xAuto.AddStep_SetTransformScale(1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AddComponent("ZM_InteriorShell");

		// The furniture. SOLID: an OBB static body, sized to the prop's own mesh by
		// ZM_InteriorFurniture::OnStart once the model has loaded (the collider
		// authored here is a placeholder cube until then -- see that component).
		//
		// ★★ OBB, AND IT USED TO BE AABB "because these are axis-aligned box
		// compositions and an AABB body is forced to identity rotation anyway".
		// Both halves of that were true and together they were circular: the props
		// were axis-aligned BECAUSE the AABB had been silently discarding their
		// rotation the whole time. Zenith_ColliderComponent forces an AABB body to
		// JPH::Quat::sIdentity(), and the physics->transform sync writes that
		// identity back over the authored rotation, INTO THE SAVED SCENE BYTES --
		// ZM-D-156, already paid for once on rival Vesper. So HomeBed, HomeShelf,
		// LabShelf and both lab counters were authored with a quarter turn and
		// stood square to the room; every pure unit stayed green because they read
		// the compiled constants and the damage lived in the file.
		//
		// It was invisible while every prop was a symmetric greybox box, and
		// AB-PROP-03 -- a chair, which has a FRONT -- is what made it visible.
		// OBB is the same box shape and differs ONLY in applying the rotation.
		const u_int uProps = ZM_GetInteriorPropCount(eRoom);
		for (u_int p = 0u; p < uProps; ++p)
		{
			const ZM_InteriorProp& xProp = ZM_GetInteriorProp(eRoom, p);

			// ★★ THE SIZE IS READ OFF THE MODEL, NEVER ASSUMED. A generated prop is
			// emitted at its roster size and grounded at y = 0, so this resolves to
			// scale 1 / y 0 and authors exactly what the loop authored before. A
			// HAND-MADE model has neither property: Bed.glb arrives at 1.00 x 0.38 x
			// 0.72 m centred on its own origin, and authored at identity it is a
			// half-size bed sunk to its mattress in the floor. ZM_PropFit.h carries
			// the argument for why the fix is here and not in the asset.
			const ZM_PropFit xFit = ZM_ResolvePropFit(xProp.m_eProp);

			xAuto.AddStep_CreateEntity(xProp.m_szEntityName);
			xAuto.AddStep_SetEntityTransient(false);
			xAuto.AddStep_SetTransformPosition(xProp.m_fX, xFit.m_fGroundY, xProp.m_fZ);
			// Explicit, and NOT skipped when it is 1: an unstated scale is a scale
			// nobody can see is deliberate, and the collider ZM_InteriorFurniture
			// rebuilds is mesh extents TIMES this number.
			xAuto.AddStep_SetTransformScale(xFit.m_fScale, xFit.m_fScale, xFit.m_fScale);
			// ★ A QUATERNION, VERBATIM -- NOT AddStep_SetTransformYaw. The yaw step
			// runs glm::angleAxis, whose sin/cos differ by 1-2 ULP between MSVC
			// Debug and Release codegen, so a committed scene authored through it
			// ping-pongs in git forever (ZM-D-183). These four values are frozen
			// literals in the dressing table.
			xAuto.AddStep_SetTransformRotationQuat(
				0.0f, xProp.m_fQuatY, 0.0f, xProp.m_fQuatW);
			xAuto.AddStep_AddCollider();
			xAuto.AddStep_AddColliderShape(
				COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			xAuto.AddStep_AddComponent("ZM_InteriorFurniture");
		}

		// The lights.
		const u_int uLights = ZM_GetInteriorLightCount(eRoom);
		for (u_int l = 0u; l < uLights; ++l)
		{
			const ZM_InteriorLight& xLight = ZM_GetInteriorLight(eRoom, l);
			xAuto.AddStep_CreateEntity(xLight.m_szEntityName);
			xAuto.AddStep_SetEntityTransient(false);
			xAuto.AddStep_SetTransformPosition(xLight.m_fX, xLight.m_fY, xLight.m_fZ);
			xAuto.AddStep_AddComponent("Light");
			xAuto.AddStep_SetLightIntensity(xLight.m_fLumens);
			xAuto.AddStep_SetLightRange(xLight.m_fRange);
			xAuto.AddStep_SetLightColor(xLight.m_fR, xLight.m_fG, xLight.m_fB);
		}
	}

	// ---- The authored outdoor props ---------------------------------------
	//
	// ★★ A CUSTOM STEP RATHER THAN AddStep_* CALLS, AND THE REASON IS THE GROUND.
	// The interior dressing can queue plain AddStep_SetTransformPosition calls
	// because an interior floor is the y = 0 plane. Outdoors the position depends
	// on the TERRAIN, which no AddStep_ knows how to sample -- so this reads
	// heights the same way ZM_ScatterDawnmerePropsStep does, from a standalone
	// terrain-editor session, and writes the transforms itself.
	//
	// ★ IT RUNS IN THE SAME LATE BLOCK AS THE SCATTER, after every pad has been
	// flattened and every path graded. Sampling earlier reads the raw heightmap
	// and stands four barrels in the air over a levelled building pad.
	void ZM_AuthorDawnmerePropsStep()
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_MESH,
				"[Zenithmon] Dawnmere props: no active scene");
			return;
		}

		Zenith_TerrainEditor& xTerrainEditor = g_xEngine.TerrainEditor();
		if (!xTerrainEditor.IsActive())
		{
			xTerrainEditor.OpenStandalone();
		}

		for (u_int u = 0u; u < ZM_GetDawnmerePropCount(); ++u)
		{
			const ZM_DawnmereProp& xProp = ZM_GetDawnmereProp(u);

			// The SAME fit the interiors resolve, from the same measured mesh --
			// there is no "is this indoors?" branch anywhere in ZM_PropFit.h and
			// this is the second caller that proves it.
			const ZM_PropFit xFit = ZM_ResolvePropFit(xProp.m_eProp);

			// ★ THE GROUND, MEASURED. The lift stands the model on the plane its
			// own min.y sits at; the terrain height is where that plane IS.
			const float fGroundY = xTerrainEditor.SampleHeightWorld(xProp.m_fX, xProp.m_fZ);

			Zenith_Entity xEntity = pxSceneData->FindEntityByName(xProp.m_szEntityName);
			if (!xEntity.IsValid())
			{
				xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, xProp.m_szEntityName);
				xEntity.SetTransient(false);
			}

			Zenith_TransformComponent& xTransform =
				xEntity.GetComponent<Zenith_TransformComponent>();
			xTransform.SetPosition(Zenith_Maths::Vector3(
				xProp.m_fX, fGroundY + xFit.m_fGroundY, xProp.m_fZ));
			xTransform.SetScale(Zenith_Maths::Vector3(
				xFit.m_fScale, xFit.m_fScale, xFit.m_fScale));
			// The frozen pair, verbatim -- never glm::angleAxis (ZM-D-183).
			xTransform.SetRotation(Zenith_Maths::Quat(
				xProp.m_fQuatW, 0.0f, xProp.m_fQuatY, 0.0f));

			// SOLID, and OBB for the same reason the interior furniture is: an AABB
			// body is forced to identity and the physics->transform sync writes that
			// identity back over the authored rotation, into the SAVED bytes
			// (ZM-D-156). Every row here is the identity today, so an AABB would
			// have been invisible -- which is exactly how that defect survived four
			// interior props.
			if (xEntity.TryGetComponent<Zenith_ColliderComponent>() == nullptr)
			{
				Zenith_ColliderComponent& xCollider =
					xEntity.AddComponent<Zenith_ColliderComponent>();
				xCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			}
			if (xEntity.TryGetComponent<ZM_InteriorFurniture>() == nullptr)
			{
				xEntity.AddComponent<ZM_InteriorFurniture>();
			}

			// ★★ A LIGHT ON THE SAME ENTITY AS THE MODEL, at the bulb. Every other
			// light in this game is its own entity at an authored world position
			// (ZM_InteriorDressing.h's lamps are all like that), which works while
			// a light is a glow in a room that nothing owns. A lamp post OWNS its
			// light: the two move, turn and scale together, and a second entity
			// would have to be kept in step with the first by hand forever.
			//
			// ★ THE OFFSET IS THE PROP'S, NOT THE PLACEMENT'S. ZM_GetPropBulb
			// carries where the bulb sits on the MODEL, measured off the mesh; the
			// component scales and rotates it by this entity's transform
			// (Zenith_LightComponent::GetWorldPosition), so one number is correct
			// for every post at every yaw and survives the asset being re-exported
			// at a different size.
			const ZM_PropBulb& xBulb = ZM_GetPropBulb(xProp.m_eProp);
			if (xBulb.m_bHasBulb)
			{
				Zenith_LightComponent* pxLight =
					xEntity.TryGetComponent<Zenith_LightComponent>();
				if (pxLight == nullptr)
				{
					pxLight = &xEntity.AddComponent<Zenith_LightComponent>();
				}
				pxLight->SetLightType(LIGHT_TYPE_POINT);
				pxLight->SetIntensity(xBulb.m_fLumens);
				pxLight->SetRange(xBulb.m_fRange);
				pxLight->SetColor(
					Zenith_Maths::Vector3(xBulb.m_fR, xBulb.m_fG, xBulb.m_fB));
				pxLight->SetLocalPositionOffset(
					Zenith_Maths::Vector3(xBulb.m_fX, xBulb.m_fY, xBulb.m_fZ));
				// Enabling is not optional: an offset authored and left switched
				// off is a lamp glowing from the pavement at its own entity origin.
				pxLight->SetUsePositionOffset(true);

				const Zenith_Maths::Vector3 xBulbWorld = pxLight->GetWorldPosition();
				Zenith_Log(LOG_CATEGORY_MESH,
					"[Zenithmon] DAWNMERE BULB '%s': model offset (%.4f, %.4f, %.4f) "
					"x scale %.4f -> world (%.3f, %.3f, %.3f), %.0f lm over %.1f m",
					xProp.m_szEntityName, (double)xBulb.m_fX, (double)xBulb.m_fY,
					(double)xBulb.m_fZ, (double)xFit.m_fScale,
					(double)xBulbWorld.x, (double)xBulbWorld.y, (double)xBulbWorld.z,
					(double)xBulb.m_fLumens, (double)xBulb.m_fRange);
			}

			Zenith_Log(LOG_CATEGORY_MESH,
				"[Zenithmon] DAWNMERE PROP '%s': %s at (%.2f, %.4f, %.2f), scale "
				"%.4f, terrain y %.4f, body-anchor clearance %.3f m",
				xProp.m_szEntityName, ZM_GetPropName(xProp.m_eProp),
				(double)xProp.m_fX, (double)(fGroundY + xFit.m_fGroundY),
				(double)xProp.m_fZ, (double)xFit.m_fScale, (double)fGroundY,
				(double)ZM_DawnmereBodyAnchorClearance(xProp.m_fX, xProp.m_fZ, true));
		}
	}

	// ---- The building a blockout is wearing ----------------------------------
	//
	// A visual-only entity: a ModelComponent loading the generated multi-surface
	// .zmodel (wall / roof / trim / glass, each with its own PBR material), at
	// SCALE 1 because the mesh is authored in real metres, and with NO collider
	// because the sibling blockout owns the physics.
	//
	// ★ THE POSITION IS THE BLOCKOUT'S FLOOR, NOT ITS CENTRE. Every generated
	// building is ground-anchored at y=0 (the same feet-on-floor convention the
	// humans and props use), so the entity sits at the bottom face of the box --
	// which is already embedded 0.05 m below the measured terrain so no gap can
	// open under the plinth.
	void ZM_QueueBuildingFacade(
		Zenith_EditorAutomation& xAuto,
		const char* szName,
		const ZM_DawnmereBlockout& xShell)
	{
		// ★ NO MODEL IS AUTHORED HERE, AND THAT IS THE WHOLE DESIGN. The scene gets
		// an entity, a transform and a COMPONENT NAME; ZM_BuildingFacade::OnStart
		// resolves the building from the entity name and loads the model at runtime.
		// Authoring the model directly makes the scene's byte count depend on how
		// much of the asset bundle had resolved at save time -- measured at 79,058
		// bytes on one boot and 82,152 on the next, same 40 entities. The full
		// argument is on the component and in Source/World/ZM_DawnmereFacades.h.
		Zenith_Assert(ZM_IsDawnmereFacadeEntity(szName),
			"ZM_QueueBuildingFacade: '%s' maps to no building, so the component "
			"would author an entity that renders nothing", szName);

		xAuto.AddStep_CreateEntity(szName);
		xAuto.AddStep_SetEntityTransient(false);
		// THE BLOCKOUT'S FLOOR, NOT ITS CENTRE: every generated building is
		// ground-anchored at y=0, and the blockout's bottom face is already embedded
		// 0.05 m below the measured terrain so no gap can open under the plinth.
		xAuto.AddStep_SetTransformPosition(
			xShell.m_xCenter.x, xShell.Min().y, xShell.m_xCenter.z);
		// Explicit unit scale. The default is already 1, but a facade whose scale
		// silently became anything else would stretch the masonry tile off its
		// world-metre pitch -- the exact "correct material at the wrong scale reads
		// as plastic" failure -- so it is stated rather than assumed.
		xAuto.AddStep_SetTransformScale(1.0f, 1.0f, 1.0f);
		xAuto.AddStep_AddComponent("ZM_BuildingFacade");
	}

	// ---- R1-2: the terrain host entity a NEW outdoor scene is built on -------
	//
	// The step list Dawnmere's own terrain host performs, in the SAME order, as
	// ONE definition -- because R1-2 authors two more of them and a nine-step shape
	// spelled by hand three times is a shape that drifts on the third. Every value
	// comes from the recipe the caller names: the asset set, the four material
	// slots (ZM_GetTerrainMaterialsForRecipe, addressed BY RECIPE rather than by a
	// second 0/1/2 mapping kept in step by hand) and the splatmap path built from
	// that same set name.
	//
	// ★ THE SHIPPED DAWNMERE BLOCK IS DELIBERATELY **NOT** MIGRATED ONTO THIS
	// HELPER. Dawnmere.zscen is a COMMITTED file that has drifted twice already
	// (ZM-D-179, ZM-D-183, both with every existing guard green); rewriting its
	// authoring in the same change that introduces two brand-new scenes would put a
	// re-author of it inside this slice's blast radius for zero behavioural gain.
	// Fold it in later, on its own, or leave it be.
	//
	// ★ THE COLLIDER IS TERRAIN + STATIC AND ZM_TerrainGrass GOES LAST -- exactly
	// the sequence Dawnmere ships, so a scene authored here streams, collides and
	// grows grass identically to the one the game has been loading since S1.
	void ZM_QueueTerrainHostEntity(
		Zenith_EditorAutomation& xAuto,
		const char* szEntityName,
		const ZM_TerrainAuthoringRecipe& xRecipe)
	{
		// Built here and passed by pointer: AddStep_* takes OWNED COPIES of every
		// string (Zenith_EditorAction), so this local may die long before the
		// action queue is drained at boot.
		const std::string strSplatmapPath = std::string("game:Terrain/") +
			xRecipe.m_pxWorldSpec->m_szTerrainSet +
			"/Splatmap_RGBA" ZENITH_TEXTURE_EXT;

		xAuto.AddStep_CreateEntity(szEntityName);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_AddComponent("Terrain");
		xAuto.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
		// The component's own copy of the shape, stamped before it initialises.
		// The BAKE stages the same spec on a standalone session (the recipe's
		// SET_DIMENSIONS op); this is the RUNTIME half -- it is what gets
		// serialized into the .zscen v5 tail and what the loader checks the baked
		// TerrainDims.zdata against. Without it a shrunken set would be loaded by
		// a component still describing the 64x64 default and refused as stale.
		xAuto.AddStep_TerrainSetDimensions(
			xRecipe.m_xDims.m_fChunkWorldSize,
			xRecipe.m_xDims.VertexSpacing(),
			(int)xRecipe.m_xDims.m_uGridChunksX,
			(int)xRecipe.m_xDims.m_uGridChunksZ);
		const MaterialHandle* paxTerrainMaterials =
			ZM_GetTerrainMaterialsForRecipe(xRecipe);
		for (u_int uSlot = 0u; uSlot < uZM_TERRAIN_MATERIAL_SLOT_COUNT; ++uSlot)
		{
			xAuto.AddStep_SetTerrainMaterial(
				(int)uSlot, paxTerrainMaterials[uSlot].GetDirect());
		}
		xAuto.AddStep_SetTerrainSplatmapPath(strSplatmapPath.c_str());
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_TERRAIN, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_TerrainGrass");
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

	// One recipe's four material slots. Recipe 0 (Dawnmere) MUST be initialised
	// first and with unchanged arguments -- see the storage declaration above for
	// why the committed scene depends on it.
	void ZM_InitializeTerrainMaterialsForRecipe(u_int uRecipeIndex)
	{
		const ZM_TerrainAuthoringRecipe& xRecipe =
			ZM_GetTerrainAuthoringRecipe(uRecipeIndex);
		Zenith_Assert(xRecipe.m_uMaterialCount == uZM_TERRAIN_MATERIAL_SLOT_COUNT,
			"Terrain recipe %u declares %u materials, expected %u",
			uRecipeIndex, xRecipe.m_uMaterialCount,
			uZM_TERRAIN_MATERIAL_SLOT_COUNT);
		for (u_int uSlot = 0; uSlot < uZM_TERRAIN_MATERIAL_SLOT_COUNT; ++uSlot)
		{
			const ZM_TerrainMaterialSpec& xSpec = xRecipe.m_pxMaterials[uSlot];
			MaterialHandle& xHandle = g_aaxTerrainMaterials[uRecipeIndex][uSlot];
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

			// A slot that names a texture set samples the ENGINE's shared ground
			// maps (the same set RenderTest's terrain uses) rather than painting a
			// flat colour. The four maps the terrain shader reads are diffuse /
			// normal / rm_packed (G = roughness, B = metallic) / ao; emissive stays
			// unset. The refs keep their "engine:" prefix so what lands in the
			// serialized scene is portable, not this machine's absolute path.
			if (xSpec.m_szTextureSetDir)
			{
				const std::string strSetDir = xSpec.m_szTextureSetDir;
				pxMaterial->SetDiffuseTexture          (TextureHandle(strSetDir + "diffuse"   ZENITH_TEXTURE_EXT));
				pxMaterial->SetNormalTexture           (TextureHandle(strSetDir + "normal"    ZENITH_TEXTURE_EXT));
				pxMaterial->SetRoughnessMetallicTexture(TextureHandle(strSetDir + "rm_packed" ZENITH_TEXTURE_EXT));
				pxMaterial->SetOcclusionTexture        (TextureHandle(strSetDir + "ao"        ZENITH_TEXTURE_EXT));
				pxMaterial->SetUVTiling(Zenith_Maths::Vector2(
					xSpec.m_fUVTiling, xSpec.m_fUVTiling));
			}
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
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_TouchLayoutController>("ZM_TouchLayoutController");
	// ★★ BOTH REGISTRATIONS OR NEITHER, AND THE COST IS WORSE THAN THIS COMMENT
	// USED TO SAY. It read "the component just silently vanishes from the editor's
	// Add Component menu, which is a defect no unit can see". That is the SMALL
	// half. `AddStep_AddComponent` resolves through THIS registry too
	// (Zenith_Editor::AddComponentToSelected walks Zenith_ComponentEditorRegistry
	// by display name), so a component registered only with the META registry is
	// one the SCENE AUTHORING cannot add:
	//
	//   * the authoring logs one `[EditorOp] Component 'X' not found in registry`
	//     at Error and CARRIES ON,
	//   * the entity is still created, still transient-false, still transformed,
	//   * the scene is published, and both boots agree it is byte-identical --
	//     because the missing component is missing CONSISTENTLY.
	//
	// ZM_BuildingFacade was added with only the meta registration and Dawnmere
	// published twice, byte-stable, with two facade entities carrying nothing but a
	// Transform. Every gate was green; the houses were simply not there.
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_GroundItemProp>("ZM_GroundItemProp");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_BuildingFacade>("ZM_BuildingFacade");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_InteriorShell>("ZM_InteriorShell");
	Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_InteriorFurniture>("ZM_InteriorFurniture");

	// Runtime toggle for the battle presenter's instant-battle mode (collapses all
	// presentation timing). Bound by reference to the ZM_BattleDirectorCore backing
	// store (ZM-D-101); flip it in the Debug Variables panel under Zenithmon/Battle.
	g_xEngine.DebugVariables().AddBoolean({ "Zenithmon", "Battle", "zm_instant_battles" }, ZM_InstantBattlesRef());
#endif

	// The C2 action table. CONFIG-INDEPENDENT and unconditional: every reader in
	// this game asks the action layer, so a build without it would have no input
	// at all. It runs BEFORE the boot unit batch (Zenith_Engine::InitialiseProject
	// calls this hook first), which is what lets ZM_Tests_Bindings assert against
	// the LIVE registration as well as against its own local instance.
	//
	// ★ THE FIRST RegisterProfile CALL CLEARS THE ENGINE DEFAULTS, so this must
	// install all three profiles or none: a game that registered one profile would
	// be left with exactly one, and every scheme outside it would go dead.
	ZM_Bindings::Register(g_xEngine.Actions());

	// Behaviour Graph node registration is CONFIG-INDEPENDENT: only .bgraph
	// AUTHORING is tools-only. A _False build still has to resolve node types
	// against a .bgraph left on disk, and the boot units build the definition
	// in-process in every config.
	ZM_RegisterGraphNodes();

	// (The save-persistence root — %APPDATA%/Zenith/Zenithmon/ — is no longer set
	// up here. Zenith_SaveData::Initialise is the ENGINE's, called with
	// Project_GetName() BEFORE this hook runs, so it happens exactly once per
	// process; see the B12 boot order in Zenith_Engine::InitialiseProject. Every
	// save call below is unaffected: the root is already live when this returns.)

#ifdef ZENITH_INPUT_SIMULATOR
	// Between-tests reset for batched automated tests. The harness force-loads
	// scene 0 before firing this hook, so entity-owned state is already cleared
	// via OnDestroy; only ownerless game globals need explicit reset here. Keep
	// this hook current as systems land (the DP hook is the reference).
	Zenith_AutomatedTestRunner::RegisterBetweenTestsHook([]()
	{
		ZM_BattleTransition::ResetRuntimeStateForTests();
		// ★ THIS ONE LINE IS ALSO THE S8 STARTER SCREEN'S FREEZE RELEASE AND ITS
		// GRANT-LATCH RESET, and neither needed a second call site. The starter picker
		// takes the freeze through ZM_UI_MenuStack::FreezePlayer (not a new owner --
		// m_bMovementEnabled is a bare bool with NO refcount, so a second claim would be
		// a bug), so a test that died with the picker up leaves the menu OPEN and the
		// CloseMenu() inside this reset releases it through the one arbitrated
		// UnfreezePlayer. What DID have to be added is inside that function: the
		// by-value m_uStarterGrantCount latch survives an ordinary close and would
		// otherwise let "exactly one starter was granted" pass in a later test with no
		// press at all.
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
		// The on-screen HUD is DontDestroyOnLoad, so its four widgets and the
		// controller's applied-context latch outlive the scene-0 force reload the
		// harness performs. Without this a test that ended in DIALOGUE would hand the
		// next one an A button still targeting Confirm. (The input PROFILE is restored
		// separately by the engine harness -- Zenith_InputActions::ResetTransientForTest.)
		ZM_TouchLayoutController::ResetRuntimeStateForTests();
		ZM_GameStateManager::ResetRuntimeStateForTests();
		// The persistent manager's GameState survives DontDestroyOnLoad across tests;
		// re-seed the starter so a caught/levelled party cannot leak into the next test.
		// ★ AND SO STORY FLAGS CANNOT EITHER (S8 item 1): the intro beat is the first
		// thing in production that writes ZM_StoryFlagSet bits, and they live on this
		// same DontDestroyOnLoad state. The reseed is a whole-state replacement, so it
		// already clears them -- but that is now load-bearing rather than incidental,
		// because a leaked ZM_STORY_FLAG_STARTER_RECEIVED would silently make Aster
		// refuse to offer a starter for the rest of the batch.
		// ★ IT IS ALSO THE ONE REMAINING SITE THAT GRANTS A STARTER -- deliberately, and
		// asymmetrically with the two production sites. The reasoning is spelled at the
		// function itself in ZM_GameStateManager.cpp; do not "restore symmetry" here.
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
	for (u_int uRecipe = 0; uRecipe < uZM_TERRAIN_RECIPE_COUNT; ++uRecipe)
	{
		for (MaterialHandle& xMaterial : g_aaxTerrainMaterials[uRecipe])
		{
			xMaterial = MaterialHandle{};
		}
	}
#endif
}

void Project_LoadInitialScene();	// forward decl for the automation step below

#ifdef ZENITH_TOOLS
void Project_InitializeResources()
{
	// Automation borrows these handles while serializing a terrain scene. The
	// saved terrain owns its material data; these temporary handles live until
	// shutdown.
	//
	// ASCENDING recipe order, deliberately: Dawnmere is recipe 0, so its four
	// material creations happen first and in the same sequence, with the same
	// arguments, as they did before the other recipes joined them. The committed
	// Dawnmere.zscen depends on that (see g_aaxTerrainMaterials above).
	const u_int uRecipeCount = ZM_GetTerrainAuthoringRecipeCount();
	Zenith_Assert(uRecipeCount == uZM_TERRAIN_RECIPE_COUNT,
		"Terrain recipe registry reports %u recipes, storage holds %u",
		uRecipeCount, uZM_TERRAIN_RECIPE_COUNT);
	for (u_int uRecipe = 0; uRecipe < uRecipeCount; ++uRecipe)
	{
		ZM_InitializeTerrainMaterialsForRecipe(uRecipe);
	}
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
	xAuto.AddStep_Custom(&ZM_BakeDawnmereNavmeshStep, "ZM NavMesh Bake (Dawnmere)");

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
	// ...and the S8 starter-choice screen (panel + header + one cell per starter), likewise
	// authored hidden. APPENDED after the save screen's widgets and BEFORE the configure
	// step, exactly like the six presenters above -- scene file indices are dense and
	// authoring-order-derived, so a block goes at the END of the widget list, never mid-list.
	// CellElementName returns string literals, so calling it at authoring time is safe.
	//
	// ★ EVERY LABEL IS AUTHORED EMPTY. The header prompt and the three species names are
	// written at RUNTIME by ZM_UI_StarterChoice::Present (from ZM_GetSpeciesName), which
	// keeps the content out of the committed scene bytes -- what those bytes carry is the
	// element NAMES, which Tests/ZM_Tests_CommittedSceneBytes.cpp needles from the very
	// same constants used here.
	xAuto.AddStep_CreateUIRect(ZM_UI_StarterChoice::szPANEL_NAME);
	xAuto.AddStep_CreateUIText(ZM_UI_StarterChoice::szHEADER_NAME, "");
	for (u_int uCell = 0u; uCell < ZM_UI_StarterChoice::uCELL_COUNT; ++uCell)
	{
		xAuto.AddStep_CreateUIButton(ZM_UI_StarterChoice::CellElementName(uCell), "");
	}
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

	// ---- WP3b: the on-screen controls, on their OWN persistent root ----------
	//
	// APPENDED after every existing entity ON PURPOSE. Scene bytes are keyed by
	// DENSE AUTHORING-ORDER file indices (ZM-D-148), so appending a block is a
	// contained change while inserting one rewrites everything after it.
	//
	// Its own root rather than ZM_MenuRoot's, matching the three roots above: the
	// HUD must survive every scene load (the additive battle included) and must
	// not share a canvas with the screens ZM_UI_MenuStack shows and hides by name.
	//
	// The AUTHORED action targets are the OVERWORLD layout -- the resting state --
	// while every control is authored HIDDEN; ZM_TouchLayoutController retargets
	// and reveals them from its first OnUpdate. The widgets take LOGICAL pixels and
	// apply the display scale at USE time, so nothing here is density-dependent.
	xAuto.AddStep_CreateEntity("ZM_TouchRoot");
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_AddUI();
	xAuto.AddStep_CreateUIVirtualStick(ZM_TouchLayoutController::szSTICK_NAME);
	xAuto.AddStep_SetUIVirtualStickAction(
		ZM_TouchLayoutController::szSTICK_NAME, ZM_Bindings::szACTION_MOVE);
	// FIXED, not FLOATING: the base stays at the authored rect's centre, so the
	// visible ring is where the control actually is and the very first frame of a
	// gesture already carries a direction.
	xAuto.AddStep_SetUIVirtualStickMode(ZM_TouchLayoutController::szSTICK_NAME, 0);
	xAuto.AddStep_SetUIVirtualStickRadius(ZM_TouchLayoutController::szSTICK_NAME, 80.f);
	xAuto.AddStep_SetUIVirtualStickActivationSlop(ZM_TouchLayoutController::szSTICK_NAME, 32.f);

	xAuto.AddStep_CreateUIVirtualButton(ZM_TouchLayoutController::szBUTTON_A_NAME);
	xAuto.AddStep_SetUIVirtualButtonAction(
		ZM_TouchLayoutController::szBUTTON_A_NAME, ZM_Bindings::szACTION_INTERACT);
	xAuto.AddStep_SetUIVirtualButtonHitSlop(ZM_TouchLayoutController::szBUTTON_A_NAME, 8.f);

	xAuto.AddStep_CreateUIVirtualButton(ZM_TouchLayoutController::szBUTTON_B_NAME);
	xAuto.AddStep_SetUIVirtualButtonAction(
		ZM_TouchLayoutController::szBUTTON_B_NAME, ZM_Bindings::szACTION_RUN);
	xAuto.AddStep_SetUIVirtualButtonHitSlop(ZM_TouchLayoutController::szBUTTON_B_NAME, 8.f);

	xAuto.AddStep_CreateUIVirtualButton(ZM_TouchLayoutController::szBUTTON_MENU_NAME);
	xAuto.AddStep_SetUIVirtualButtonAction(
		ZM_TouchLayoutController::szBUTTON_MENU_NAME, ZM_Bindings::szACTION_MENU);
	xAuto.AddStep_SetUIVirtualButtonHitSlop(ZM_TouchLayoutController::szBUTTON_MENU_NAME, 8.f);

	xAuto.AddStep_Custom(&ZM_ConfigureTouchControls);
	xAuto.AddStep_AddComponent("ZM_TouchLayoutController");

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
		ZM_QueueColliderBlock(xAuto, ZM_GetPlayerHomeBlockName(eBlock),
			xBlock.m_xCenter, xBlock.m_xScale);
	}

	// The room itself: shell model, furniture and lights. Queued straight after
	// the blockouts so the collision and the picture stay adjacent in the file.
	ZM_QueueInteriorDressing(xAuto, ZM_INTERIOR_ROOM_PLAYER_HOME);

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
	// ★ THIS ROOM NOW HAS A WAY OUT, AND IT ARRIVED WITH ITS DESTINATION (SC-E).
	// The block that stood here said the ProfLab exit was DELIBERATELY absent, and
	// it was right to: ZM_WorldSpec has declared ProfLab -> Dawnmere via spawn tag
	// "FromLab" since S1, but Dawnmere.zscen authored only the "TownCenter" and
	// "FromHome" markers -- and IsWarpDestinationValid reads ONLY the compiled tag
	// list, never the scene, so an exit shipped on its own would have passed
	// validation and then stalled the warp machine in
	// ZM_WARP_TRANSITION_WAITING_FOR_SPAWN -- an opaque fade over a frozen player
	// until that barrier's frame budget expires (ZM-D-200), then a Zenith_Error
	// naming the tag and a screen that comes back on a door into nowhere.
	//
	// That is no longer the state of the world. The Dawnmere block at the bottom of
	// this function now authors the lab blockout, the "FromLab" arrival marker and
	// the LabDoorTrigger, and this block authors the exit that returns to them. The
	// two halves are ONE change and must stay one: deleting either side re-opens
	// exactly the break described above, which is why the live-scene clause I4 of
	// ZM_ProfLabWarp_Test (a CI gate -- it needs no terrain and never skips) checks
	// this sensor's target and tag in the LOADED scene, and why
	// ZM_CommittedSceneBytes/DawnmereCarriesTheLabSeamMarkerAndTag checks the other
	// side's bytes at boot.
	xAuto.AddStep_CreateScene(szZM_PROFLAB_SCENE_NAME);
	for (u_int uBlock = 0u; uBlock < (u_int)ZM_PROFLAB_BLOCK_COUNT; ++uBlock)
	{
		const ZM_PROFLAB_BLOCK eBlock = (ZM_PROFLAB_BLOCK)uBlock;
		const ZM_ProfLabBlockout xBlock = ZM_GetProfLabBlock(eBlock);
		ZM_QueueColliderBlock(xAuto, ZM_GetProfLabBlockName(eBlock),
			xBlock.m_xCenter, xBlock.m_xScale);
	}

	// The room itself: shell model, furniture and lights. Queued straight after
	// the blockouts so the collision and the picture stay adjacent in the file.
	ZM_QueueInteriorDressing(xAuto, ZM_INTERIOR_ROOM_PROF_LAB);

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

	// Professor Aster, the interior's ONE inhabitant. APPENDED here -- after the
	// Player, before the camera -- and never inserted earlier: ZM-D-148's scene
	// files carry DENSE, AUTHORING-ORDER file indices, so appending an entity is
	// free while inserting one rewrites every index after it.
	//
	// ★ SC-E TURNED HIM ROUND, AND THAT CHANGED HIS HELPER AND HIS BODY SHAPE.
	// He shipped at identity rotation, identity forward is +Z, and he stands DEEPER
	// into the hall than the arrival point -- so the professor greeted every
	// arriving player with his back turned, on screen and facing the wall behind
	// him. (ProfLab_AsterStandsInsideTheArrivalFrustum asserted he was VISIBLE and
	// passed throughout; nothing asserted he was facing anything.) He now takes the
	// frozen half-turn ZM_ProfLabAsterFacing() through
	// ZM_QueueFacingStationaryTalkerNpc, which differs from the stationary helper in
	// exactly two forced ways -- OBB instead of AABB, and a verbatim quaternion step
	// -- both argued at that function.
	//
	// ★ THE QUATERNION IS FROZEN, NOT COMPUTED (ZM-D-183). Its four components are
	// the IEEE-754 spellings of 0 and 1, because a half turn about +Y is exactly
	// (0, 1, 0, 0) -- there is no libm result here to disagree about Debug vs
	// Release, which is what keeps this COMMITTED file's bytes stable. Do NOT
	// "simplify" this to AddStep_SetTransformYaw(pi): that step runs glm::angleAxis
	// engine-side and reopens the ping-pong.
	//
	// ★ AND THE STEPS ARE STILL NOT RE-SPELLED HERE. One helper, one definition of
	// "an authored stationary talker who faces somewhere", at the same human visual
	// scale as every other person in this game.
	const Zenith_Maths::Vector3 xProfLabAsterCenter = ZM_GetProfLabAsterCenter();
	ZM_QueueFacingStationaryTalkerNpc(xAuto, szZM_PROFLAB_ASTER_ENTITY_NAME,
		xProfLabAsterCenter, g_xZMHumanVisualScale, ZM_ProfLabAsterFacing(),
		&ZM_ConfigureProfAsterNpc);

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

	// The exit sensor (SC-E). APPENDED at the very END of this block and never
	// inserted earlier: ZM-D-148's scene files carry DENSE, AUTHORING-ORDER file
	// indices, so appending is free while inserting rewrites every index after it.
	//
	// Same nine-step shape as the shipped PlayerHomeExitTrigger -- transform,
	// scale, static AABB body, ZM_WarpTrigger, configure -- with two differences,
	// both deliberate: every number comes from ZM_GetProfLabExitTrigger() rather
	// than being typed inline (a literal spelled at two sites cannot red a drift),
	// and the configure step spells NO build index and NO tag (it resolves both
	// from the compiled world table -- see ZM_ConfigureProfLabExitTrigger).
	//
	// AABB is correct here and is NOT the ZM-D-156 hazard: this box carries no
	// rotation to lose. Its faces are the aperture's faces, which are axis-aligned
	// by construction.
	const ZM_ProfLabBlockout xProfLabExitTrigger = ZM_GetProfLabExitTrigger();
	xAuto.AddStep_CreateEntity(szZM_PROFLAB_EXIT_TRIGGER_ENTITY_NAME);
	xAuto.AddStep_SetEntityTransient(false);
	xAuto.AddStep_SetTransformPosition(
		xProfLabExitTrigger.m_xCenter.x, xProfLabExitTrigger.m_xCenter.y,
		xProfLabExitTrigger.m_xCenter.z);
	xAuto.AddStep_SetTransformScale(
		xProfLabExitTrigger.m_xScale.x, xProfLabExitTrigger.m_xScale.y,
		xProfLabExitTrigger.m_xScale.z);
	xAuto.AddStep_AddCollider();
	xAuto.AddStep_AddColliderShape(
		COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
	xAuto.AddStep_AddComponent("ZM_WarpTrigger");
	xAuto.AddStep_Custom(&ZM_ConfigureProfLabExitTrigger);

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
		const Zenith_Maths::Vector3 xPlayerScale = g_xZMHumanVisualScale;
		const float fPlayerCapsuleHalfExtent = fZM_HUMAN_BODY_HALF_HEIGHT;
		const Zenith_Maths::Vector3 xPlayerCenter =
			xTownCenterFeet + Zenith_Maths::Vector3(
				0.0f, fPlayerCapsuleHalfExtent, 0.0f);

		xAuto.AddStep_CreateScene("Dawnmere");
		xAuto.AddStep_CreateEntity("DawnmereTerrain");
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_AddComponent("Terrain");
		xAuto.AddStep_TerrainSetAssetSet(xRecipe.m_pxWorldSpec->m_szTerrainSet);
		// The component's own copy of the shape, stamped before it initialises.
		// The BAKE stages the same spec on a standalone session (the recipe's
		// SET_DIMENSIONS op); this is the RUNTIME half -- it is what gets
		// serialized into the .zscen v5 tail and what the loader checks the baked
		// TerrainDims.zdata against. Without it a shrunken set would be loaded by
		// a component still describing the 64x64 default and refused as stale.
		xAuto.AddStep_TerrainSetDimensions(
			xRecipe.m_xDims.m_fChunkWorldSize,
			xRecipe.m_xDims.VertexSpacing(),
			(int)xRecipe.m_xDims.m_uGridChunksX,
			(int)xRecipe.m_xDims.m_uGridChunksZ);
		const MaterialHandle* paxTerrainMaterials =
			ZM_GetTerrainMaterialsForRecipe(xRecipe);
		for (int iSlot = 0; iSlot < 4; ++iSlot)
		{
			xAuto.AddStep_SetTerrainMaterial(iSlot, paxTerrainMaterials[iSlot].GetDirect());
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
		// ★ THE FOUR BLOCKOUTS ARE UNCHANGED GEOMETRY AND ARE NOW INVISIBLE. Their
		// centres, scales and colliders are byte-for-byte what they have always
		// been -- every measured ground row, the camera-clearance clause, the door
		// trigger and the keep-out still describe exactly these boxes -- but the
		// greybox cube they used to wear is gone. The building below is the picture.
		//
		// The jambs and lintel keep their colliders because the doorway must stay
		// solid (entry is the trigger in front of it, not a walk-through), and they
		// lose their visual because the generated facade emits its own door surround
		// at the aperture these three describe.
		ZM_QueueColliderBlock(xAuto, "DawnmereHomeShell",
			xHomeShell.m_xCenter, xHomeShell.m_xScale);
		ZM_QueueColliderBlock(xAuto, "DawnmereHomeDoorLeft",
			xHomeDoorLeft.m_xCenter, xHomeDoorLeft.m_xScale);
		ZM_QueueColliderBlock(xAuto, "DawnmereHomeDoorRight",
			xHomeDoorRight.m_xCenter, xHomeDoorRight.m_xScale);
		ZM_QueueColliderBlock(xAuto, "DawnmereHomeDoorLintel",
			xHomeLintel.m_xCenter, xHomeLintel.m_xScale);
		ZM_QueueBuildingFacade(xAuto, szZM_DAWNMERE_HOME_FACADE_ENTITY_NAME, xHomeShell);

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
		// the ONE feet height sampled at the town centre, which made the
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
		// SEPARATION is deliberately generous -- the closest PAIR is 13.4 m, against
		// a 2.9 m effective reach (2.5 global + 0.4 per-NPC). The picker resolves the
		// NEAREST FACED candidate, so two NPCs within reach of each other would make
		// "which NPC answered?" a function of sub-metre walk error; at 13 m the
		// answer cannot be ambiguous, and the walk-up test can assert the winner BY
		// ENTITY ID. Exact distances are derived at the coordinates below.
		//
		// ★ ZM-D-217 SHRANK EVERY OFFSET WITH THE TOWN, and the figure that matters
		// is the RATIO, not the metres: 13.4 m is 4.6x the reach where v6's 16.1 m
		// was 5.5x, on a town core 1.8x wider. Nothing here is scaled by eye -- every
		// separation below is re-derived against the v7 drive corridors.
		//
		// The VILLAGER is the walk-up target and sits straight +Z of the spawn on
		// purpose: +Z is the one movement axis with existing evidence
		// (ZM_DawnmerePlayerCamera_Test already proves held-W moves the yaw-zero
		// player +Z), so the walk needs no unproven basis assumption.
		//
		// ★★ THE OTHER TWO MUST STAY OFF THE HOME DRIVE CORRIDOR. A solid STATIC
		// AABB on it WEDGES A DIFFERENT, ALREADY-GREEN TEST:
		// ZM_PlayerHomeRoundTrip_Test drives the player from the TownCenter spawn
		// (192, 128) to the door staging waypoint with DriveTowardXZ, which has NO
		// obstacle avoidance. An NPC box on that line stops the capsule head-on
		// (the 1.8 m body is far above the 0.40 m step assist), the staging
		// tolerance is never met, and that test dies at its frame cap with a
		// timeout that names distance, not the NPC.
		// ★ THE CORRIDOR IS A SHALLOW DIAGONAL, NOT A PURE -X RUN. It goes from
		// (192, 128) to ZM_GetDawnmereHomeDoorStagingXZ() -- currently (128, 122) --
		// so it drops 6 m in Z across 64 m in X. Both flank NPCs sit at z + 16 and
		// clear it by 17.1 m (caretaker) and 17.1 m (clerk), measured as PERPENDICULAR
		// distance to the leg rather than as a Z gap, which is what the diagonal
		// makes necessary.
		// ★★ AND SINCE ZM-D-217 THIS IS NO LONGER THE ONLY THING STANDING BETWEEN A
		// STATIC BODY AND THAT CORRIDOR. Source/World/ZM_DawnmereDressing.h computes
		// the same corridor as a KEEP-OUT primitive and
		// Tests/ZM_Tests_DawnmereDressing.cpp walks every leg of it end to end. This
		// paragraph is still the reasoning for the four NPCs (which the dressing does
		// not place); the scenery is checked rather than reasoned about.
		// A scene-placement change can regress a suite it never mentions -- check the
		// existing traversal routes before moving anything in this block.
		//
		const Zenith_Maths::Vector3 xNpcScale = xPlayerScale;
		const Zenith_Maths::Vector3 xVillagerCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_VILLAGER, fPlayerCapsuleHalfExtent);
		// z + 16 keeps both off the Home-traversal corridor, which runs from
		// (192, 128) down to the door staging waypoint at (128, 122).
		// Separations against the 2.9 m effective reach (2.5 global + 0.4 authored):
		//   villager <-> clerk      = sqrt(12^2 + 6^2) = 13.4 m
		//   villager <-> caretaker  = sqrt(12^2 + 6^2) = 13.4 m
		//   clerk    <-> caretaker  = 24.0 m
		//   spawn    <-> either     = sqrt(12^2 + 16^2) = 20.0 m
		// The closest pair is 4.6x reach, so the nearest-faced-candidate picker can
		// never confuse two of them and the walk-up test can assert the winner BY
		// ENTITY ID; and neither flank NPC is reachable from spawn, which keeps the
		// test's out-of-range negative unambiguous.
		const Zenith_Maths::Vector3 xClerkCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_TRADE_POST_CLERK, fPlayerCapsuleHalfExtent);
		const Zenith_Maths::Vector3 xCaretakerCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_CARETAKER, fPlayerCapsuleHalfExtent);
		ZM_QueueStationaryTalkerNpc(xAuto, "Npc_Villager",
			xVillagerCenter, xNpcScale, &ZM_ConfigureVillagerNpc);
		ZM_QueueStationaryTalkerNpc(xAuto, "Npc_TradePostClerk",
			xClerkCenter, xNpcScale, &ZM_ConfigureTradePostClerkNpc);
		ZM_QueueStationaryTalkerNpc(xAuto, "Npc_Caretaker",
			xCaretakerCenter, xNpcScale, &ZM_ConfigureCaretakerNpc);
		// S7 item 2 SC1: the story-gated warden. He stands on the authored HOME
		// WALKWAY, not on the north road: (166, 142) is ~3.7 m off the Home path
		// centreline -- inside its 10 m flatten corridor, which is what makes him
		// read as standing ON the lane -- and ~24.8 m from the nearest point of the
		// Route polyline, so his lines are written as a lane warden rather than a
		// road-blocker. The position itself is derived under exactly the constraints
		// stated above, NOT eyeballed:
		//   * PERPENDICULAR distance to the Home traversal corridor (192, 128) ->
		//     (128, 122) is 16.4 m, comparable to the two flank NPCs' 17.1 m.
		//     Anything nearer would re-open the wedging hazard the block above is
		//     written to prevent -- and note this is a perpendicular measure, not a
		//     Z gap: the v7 corridor is a diagonal.
		//   * x - 26 keeps him 26 m off the x = 192 spawn-to-villager corridor.
		//   * Separations from the existing roster, against the same 2.9 m effective
		//     reach: caretaker (180, 144) = 14.1 m (still 4.9x reach, and wider than
		//     the 13.4 m minimum quoted above, so the "closest pair" figure at
		//     fZM_NPC_AUTHORED_RADIUS is unchanged); villager (192, 138) = 27.9 m;
		//     clerk (204, 144) = 38.1 m; wanderer patrol (214, 124..132) = 50.0 m at
		//     its nearest endpoint; TownCenter spawn = sqrt(26^2 + 14^2) = 29.5 m, so
		//     the warden is not reachable from spawn and the existing out-of-range
		//     negative stays clean.
		//   * The Home shell (x 119.5..136.5, z 128..141) lies SOUTH-WEST of the
		//     warden, who stands at (166, 142): its east face is 29.5 m west and its
		//     north face is 1 m south. That is ample horizontal clearance; re-check
		//     both axes if either the facade footprint or the warden placement moves.
		// Height is his OWN measured feet plus the shared capsule half-extent, like
		// every other NPC since known-limit W5 -- see the block above.
		// ★ When a later stage authors a real Route 1, a warden who is meant to BLOCK
		// the road belongs on the Route polyline itself. Re-place him there and
		// re-derive every separation above from scratch -- none of these figures carry
		// over, INCLUDING his feet height -- and rewrite his lines in ZM_NpcData.cpp
		// to match the new ground.
		const Zenith_Maths::Vector3 xRouteWardenCenter = ZM_DawnmereNpcAuthoredCenter(
			ZM_DAWNMERE_NPC_WARDEN, fPlayerCapsuleHalfExtent);
		ZM_QueueStationaryTalkerNpc(xAuto, "Npc_Warden",
			xRouteWardenCenter, xNpcScale, &ZM_ConfigureRouteWardenNpc);
		// SC8: the fourth row is a deterministic two-point patrol. Both endpoints are
		// 22 m east of the TownCenter spawn and clear of the Home corridor, which
		// runs WEST from it; the nearest stationary NPC (the clerk) remains >20 m
		// away. ONE EXTRA capsule half-extent of clearance
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
		// ★ ZM-D-184: SPAWNED WITH CLEARANCE, not at his resting centre. He used to
		// be authored exactly on the surface and fell through the world
		// intermittently -- the full measurement is in the ZM_DawnmereTrainerSpawnY
		// block in Source/World/ZM_DawnmerePlacement.h. Gravity settles him back onto
		// this exact XZ within a second (HoldTrainerStation pins XZ every tick), so
		// no geometric claim in that header changes.
		const Zenith_Maths::Vector3 xRivalVesperCenter(
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER).m_fX,
			ZM_DawnmereTrainerSpawnY(fPlayerCapsuleHalfExtent),
			ZM_GetDawnmereNpcAnchor(ZM_DAWNMERE_NPC_RIVAL_VESPER).m_fZ);
		ZM_QueueDawnmereTrainerNpc(xAuto, "Npc_RivalVesper",
			xRivalVesperCenter, xNpcScale, ZM_DawnmereVesperFacing(),
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

		// ---- S8 SC-E: THE LAB SITE, AND THE OTHER END OF PROFLAB'S EXIT ----
		//
		// ★★ THIS IS THE HALF THAT MAKES THE PROFLAB EXIT SAFE, AND THE TWO CANNOT
		// BE SPLIT ACROSS COMMITS. ZM_GameStateManager::IsWarpDestinationValid reads
		// ONLY the compiled ZM_WorldSpec tag list, never the destination scene, and
		// "FromLab" has been a compiled Dawnmere tag since S1 -- so
		// RequestWarp(<Dawnmere>, "FromLab") returns TRUE whether or not any marker
		// in this scene carries that tag. Ship the ProfLab exit without the marker
		// below and the warp is ACCEPTED, the fade goes fully opaque, and the
		// machine stalls in ZM_WARP_TRANSITION_WAITING_FOR_SPAWN with the player
		// frozen until that barrier's frame budget expires (ZM-D-200) and a
		// Zenith_Error names the tag. Still not a crash and still not a red test --
		// but a black screen that ENDS and says why, rather than one that lasts
		// forever.
		//
		// ★ EVERY COORDINATE COMES FROM THE FROZEN SC-D BLOCK in
		// Source/World/ZM_DawnmerePlacement.h, whose ten ground heights are MEASURED
		// raycasts against the baked Dawnmere terrain, and whose derived Y formulas
		// are spelled once in that file's .cpp. Nothing here re-derives or re-spells
		// a height: a constant spelled at two sites cannot red a drift, and these
		// particular constants are measurements that a terrain re-bake invalidates.
		// Read the SC-D header block before moving any of it -- the reserved terrain
		// pad, the camera-derived entrance plane and the arrival marker are one
		// interlocking placement.
		//
		// ★ THE ENTITY NAMES COME FROM Source/World/ZM_ProfLabPlacement.h, which is
		// the deeper of the two placement headers (this file's own already includes
		// it) and therefore the one both sides of the seam can read. In particular
		// szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME is looked up BY NAME by SC-D's
		// ground-truth oracle so its post-SC-E re-measure can ignore this shell --
		// rename it and that oracle silently measures the roof instead of the ground.
		//
		// ★ APPENDED AT THE END OF THE DAWNMERE BLOCK (ZM-D-148 dense
		// authoring-order file indices): appending is free, inserting rewrites every
		// index after it. The rival-facing pre-save guard stays immediately before
		// the save, below.
		//
		// ★ AND NOT ONE OF THESE SEVEN CARRIES A ROTATION STEP. All seven are
		// axis-aligned boxes or point markers with nothing to face, exactly like the
		// four Home blocks and HomeDoorTrigger -- and identity is the one rotation
		// that is bit-exact in every build configuration (ZM-D-183).
		const ZM_DawnmereBlockout xLabShell = ZM_GetDawnmereLabShell();
		const ZM_DawnmereBlockout xLabDoorLeft = ZM_GetDawnmereLabDoorLeft();
		const ZM_DawnmereBlockout xLabDoorRight = ZM_GetDawnmereLabDoorRight();
		const ZM_DawnmereBlockout xLabLintel = ZM_GetDawnmereLabDoorLintel();
		const ZM_DawnmereBlockout xLabTrigger = ZM_GetDawnmereLabDoorTrigger();
		const Zenith_Maths::Vector3 xFromLabSpawnFeet =
			ZM_GetDawnmereFromLabSpawnFeet();
		// Same split as the Home block above: the four blockouts keep their exact
		// authored geometry and their colliders, lose their greybox cube, and the
		// generated Lab model is queued behind them as the picture.
		ZM_QueueColliderBlock(xAuto, szZM_DAWNMERE_LAB_SHELL_ENTITY_NAME,
			xLabShell.m_xCenter, xLabShell.m_xScale);
		ZM_QueueColliderBlock(xAuto, szZM_DAWNMERE_LAB_DOOR_LEFT_ENTITY_NAME,
			xLabDoorLeft.m_xCenter, xLabDoorLeft.m_xScale);
		ZM_QueueColliderBlock(xAuto, szZM_DAWNMERE_LAB_DOOR_RIGHT_ENTITY_NAME,
			xLabDoorRight.m_xCenter, xLabDoorRight.m_xScale);
		ZM_QueueColliderBlock(xAuto, szZM_DAWNMERE_LAB_DOOR_LINTEL_ENTITY_NAME,
			xLabLintel.m_xCenter, xLabLintel.m_xScale);
		ZM_QueueBuildingFacade(xAuto, szZM_DAWNMERE_LAB_FACADE_ENTITY_NAME, xLabShell);

		// The arrival marker. Its transform is the marker's FEET -- the MEASURED
		// terrain surface at (fZM_DAWNMERE_LAB_X, fZM_DAWNMERE_FROM_LAB_SPAWN_Z) --
		// because ZM_GameStateManager::CalculateSpawnCenter adds the capsule
		// half-extent at warp time. Authoring a body centre here would warp an
		// arriving player in half a body above the ground. Step list mirrors the
		// shipped FromHomeSpawn verbatim.
		xAuto.AddStep_CreateEntity(szZM_DAWNMERE_FROM_LAB_SPAWN_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xFromLabSpawnFeet.x, xFromLabSpawnFeet.y, xFromLabSpawnFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureFromLabSpawnPoint);

		// ...and the doorway sensor, 2 m in front of the solid entrance face so an
		// approaching player overlaps it before physical contact. Step list mirrors
		// the shipped HomeDoorTrigger verbatim; only the configure step differs, in
		// that it reads its destination from the compiled world table instead of
		// spelling it.
		xAuto.AddStep_CreateEntity(szZM_DAWNMERE_LAB_DOOR_TRIGGER_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xLabTrigger.m_xCenter.x, xLabTrigger.m_xCenter.y,
			xLabTrigger.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xLabTrigger.m_xScale.x, xLabTrigger.m_xScale.y,
			xLabTrigger.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureLabDoorTrigger);

		// ---- R1-2 STEP 3: DAWNMERE'S "FromRoute1" ARRIVAL MARKER -----------
		//
		// The last of the four R1-2 seam markers, and the only one that lands in
		// a scene that was ALREADY COMMITTED. Route 1's two and Thornacre's one
		// went into new files; this one re-authors an asset that has drifted
		// twice (ZM-D-179, ZM-D-183), which is why the whole slice was sequenced
		// to measure this column first and touch these bytes last.
		//
		// ★★ APPENDED AT THE END OF THE DAWNMERE BLOCK, AFTER THE LAB SEAM AND
		// STRICTLY BEFORE THE PRE-SAVE GUARD. Scene files carry DENSE
		// AUTHORING-ORDER file indices (ZM-D-148), so appending costs one new
		// entity record while inserting ANYWHERE EARLIER renumbers every entity
		// after it and turns a one-entity change into a whole-file diff -- on
		// exactly the file whose byte stability is this game's most expensive
		// invariant. The rival-facing guard below is not something this may be
		// placed after: it must stay the last thing before the save, for the
		// reason its own comment gives.
		//
		// ★★ THE TAG IS **INBOUND** -- the tag asked for by the scene the player
		// is ARRIVING FROM, which is ROUTE1, never by any edge LEAVING Dawnmere.
		// The full trap, and why getting it backwards is a warp that validates,
		// goes fully opaque and then stalls in WAITING_FOR_SPAWN until that
		// barrier's frame budget expires (ZM-D-200), is written out at
		// ZM_ResolveInboundSpawnTag. Nothing here spells the tag.
		//
		// ★ THE TRANSFORM IS THE MARKER'S FEET, NEVER A BODY CENTRE.
		// ZM_GameStateManager::CalculateSpawnCenter adds the capsule half-extent
		// at warp time, so authoring a centre would drop every arriving player
		// in from half a body up. The height is the MEASURED (512, 864) column
		// frozen by R1-2 step 1, read through ZM_GetDawnmereFromRoute1SpawnFeet
		// -- the literal is never re-spelled here.
		//
		// ★ THE OUTBOUND SENSOR THAT AIMS AT ROUTE 1 IS THE NEXT BLOCK, NOT THIS
		// ONE. R1-2 landed this marker with zero triggers anywhere in the game;
		// R1-3 lands all four sensors together, once every marker exists. Step
		// list mirrors the shipped FromLabSpawn verbatim; like it and like the
		// four Home blocks, this entity carries NO rotation step at all, so the
		// ZM-D-183 frozen-quaternion rule does not apply (identity is the one
		// rotation that is bit-exact in every configuration).
		const Zenith_Maths::Vector3 xFromRoute1SpawnFeet =
			ZM_GetDawnmereFromRoute1SpawnFeet();
		xAuto.AddStep_CreateEntity(szZM_DAWNMERE_FROM_ROUTE1_SPAWN_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xFromRoute1SpawnFeet.x, xFromRoute1SpawnFeet.y,
			xFromRoute1SpawnFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureDawnmereFromRoute1ArrivalSpawnPoint);

		// ---- R1-3: DAWNMERE'S NORTH SEAM GATE ------------------------------
		//
		// The fourth of the four seam sensors, and Dawnmere's third
		// ZM_WarpTrigger (after HomeDoorTrigger and LabDoorTrigger). It stands
		// 12 m NORTH of the marker just authored, so a player who arrives off
		// Route 1 is 9 m clear of its near face and a player walking north out
		// of town crosses it last -- the clearance derivation, and why an
		// overlap on arrival would be an infinite two-scene ping-pong, are in
		// Source/World/ZM_DawnmerePlacement.h beside the constants.
		//
		// ★★ APPENDED, AGAIN, AND STRICTLY BEFORE THE PRE-SAVE GUARD. Scene
		// files carry DENSE AUTHORING-ORDER file indices (ZM-D-148), so this
		// costs one new entity record while inserting it anywhere earlier would
		// renumber every entity after it and turn a one-entity change into a
		// whole-file diff. The rival-facing guard below must stay the last thing
		// before the save, for the reason its own comment gives.
		//
		// ★ EVERY VALUE IS READ, NONE IS SPELLED: the centre and scale come from
		// ZM_GetDawnmereNorthGate() and the destination from the compiled world
		// table via ZM_ConfigureDawnmereNorthGateTrigger. Step list mirrors the
		// shipped LabDoorTrigger verbatim -- create, transform, scale, static
		// AABB body, ZM_WarpTrigger, configure -- and, like it, carries no
		// rotation step: an AABB body forces JPH::Quat::sIdentity() anyway
		// (ZM-D-193) and identity is bit-exact in every configuration.
		const ZM_DawnmereBlockout xNorthGate = ZM_GetDawnmereNorthGate();
		xAuto.AddStep_CreateEntity(szZM_DAWNMERE_NORTH_GATE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xNorthGate.m_xCenter.x, xNorthGate.m_xCenter.y,
			xNorthGate.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xNorthGate.m_xScale.x, xNorthGate.m_xScale.y, xNorthGate.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureDawnmereNorthGateTrigger);

		// ---- ZM-D-217: THE SCENERY LAYER -----------------------------------
		//
		// ★★ WHY DAWNMERE HAD NONE UNTIL NOW. Every entity above is a BLOCKOUT,
		// a marker or a person; the map itself was terrain and grass and nothing
		// else, over 368,640 m^2. The v7 recipe cuts that to 147,456 and pulls the
		// buildings in, but a smaller empty field is still an empty field. This
		// block is the other half of the fix: instanced woodland, rock, deadwood
		// and bushes from the SHARED engine sets -- Zenith/Assets/Meshes/
		// {ProceduralTree,Rocks,FallenTrees,Bushes}, regenerated by their
		// generators in Tools/ on every tools boot, and the same sets RenderTest
		// dresses its campus with. No Zenithmon-owned art is introduced.
		//
		// ★★ EVERY PIECE OF IT IS KEPT OFF THE TOWN BY GEOMETRY, NOT BY EYE.
		// Boulders, shards, stumps, logs and tree TRUNKS all carry per-instance
		// capsule colliders, and every automated traversal in this game drives
		// the player with DriveTowardXZ, which has NO obstacle avoidance -- so a
		// prop on a drive leg stops the capsule dead and the suite dies at its
		// frame cap naming a DISTANCE, never the boulder. The Home block further
		// up this file has carried that hazard as PROSE since S6; ZM_DawnmereDressing
		// turns it into a computed keep-out (pads and path corridors read from the
		// terrain recipe, anchors and markers from ZM_DawnmerePlacement.h, plus the
		// seven blind drive legs, which exist only in test code and are the one
		// thing that file spells) and Tests/ZM_Tests_DawnmereDressing.cpp asserts
		// it headless.
		//
		// ★ THE TWO HALVES ARE AUTHORED BY DIFFERENT MECHANISMS ON PURPOSE.
		// Trees go through the ENGINE's terrain-editor tree brush, because the
		// trunk/leaves lockstep, the shared sway VAT phase and the trunk collider
		// are already implemented and tested there -- Dawnmere's only genuine
		// difference from RenderTest's tree rings is WHERE the discs are, and a
		// second implementation would be a second thing to keep correct. The
		// rocks/deadwood/bushes go through ZM_ScatterDawnmerePropsStep, because
		// the brush has no keep-out and no notion of a laid-down log.
		//
		// ★ APPENDED HERE, AFTER EVERY MARKER AND SENSOR AND STRICTLY BEFORE THE
		// PRE-SAVE GUARD (ZM-D-148 dense authoring-order file indices). The tree
		// brush creates TerrainTrees_Trunk/_Leaves on its first dab, so those two
		// entities land after the north gate rather than in the middle of the
		// seam entities.
		{
			const int iTreeTool = static_cast<int>(Zenith_TerrainBrushTool::TreePaint);
			xAuto.AddStep_TerrainSetTreeBrush(
				iZM_DAWNMERE_TREES_PER_CLUMP,
				fZM_DAWNMERE_TREE_SCALE_MIN, fZM_DAWNMERE_TREE_SCALE_MAX,
				fZM_DAWNMERE_TREE_SPACING, fZM_DAWNMERE_TREE_MAX_SLOPE_DEG,
				iZM_DAWNMERE_TREE_SEED);
			for (u_int uClump = 0u; uClump < ZM_GetDawnmereTreeClumpCount(); ++uClump)
			{
				const ZM_DawnmereTreeClump& xClump = ZM_GetDawnmereTreeClump(uClump);
				// fStrength 1.0 = full density for this dab; fToolValue 0 = paint
				// (> 0.5 would ERASE, which would silently undo the previous dabs).
				xAuto.AddStep_TerrainBrushStroke(iTreeTool,
					xClump.m_fX, xClump.m_fZ, xClump.m_fRadius, 1.0f, 0.0f);
			}
			xAuto.AddStep_Custom(&ZM_ScatterDawnmerePropsStep);
			// ★ AFTER the scatter, so the authored barrels take file indices at the
			// end of the block rather than displacing ~500 instanced transforms.
			xAuto.AddStep_Custom(&ZM_AuthorDawnmerePropsStep);
		}

		// ★ IMMEDIATELY BEFORE THE SAVE, NOT ANYWHERE EARLIER. The guard serializes the
		// rival's transform for real and compares the resulting bytes with
		// ZM_DawnmereVesperFacing(); run it any earlier and a later step could still
		// move the value it just cleared. See ZM_VerifyAuthoredRivalFacingStep.
		xAuto.AddStep_SelectEntity("Npc_RivalVesper");
		xAuto.AddStep_Custom(&ZM_VerifyAuthoredRivalFacingStep);

		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Dawnmere" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	// ========================================================================
	// ---- S8 ITEM 2, R1-2: ROUTE 1 AND THORNACRE ----------------------------
	//
	// The two scenes that turn "Dawnmere -> Route 1 -> Thornacre" from three rows
	// of a compiled table into a walk. R1-1 already landed their compiled anchors
	// (Source/World/ZM_Route1Placement.h, ZM_ThornacrePlacement.h), their
	// registrations (Source/World/ZM_SceneRegistry.h, walked by
	// Project_LoadInitialScene) and 17 boot units; what was missing was the
	// .zscen files themselves. This block authors them.
	//
	// ★ THE GATE IS xTerrainBatch.m_bAuthorDawnmereScene, AND THAT NAME IS A
	// MISNOMER THIS SLICE DELIBERATELY DOES NOT FIX. What the flag MEANS is
	// "every registered terrain recipe probed WARM and this boot queued no bake"
	// (ZM_BuildTerrainBakeBatchPlan: m_bAllWarm && m_uQueueRecipeMask == 0u), and
	// that is exactly the precondition these two terrain-derived scenes need --
	// their heightmaps, splatmaps and physics chunks must already be on disk, and
	// a Null/headless boot must author neither. Renaming the flag, or the
	// AUTHOR_DAWNMERE token the batch-result log line prints, would break three
	// docs that quote that token as a boot-log check plus every recorded proof
	// procedure that greps for it. So the flag keeps its name and this comment
	// carries its meaning.
	//
	// ★ A SEPARATE if() RATHER THAN AN EXTENSION OF THE DAWNMERE BLOCK, and NOT
	// ONE EXISTING STEP IS TOUCHED OR REORDERED. Scene files carry DENSE
	// AUTHORING-ORDER file indices (ZM-D-148): appending two whole scenes after
	// Dawnmere's AddStep_UnloadScene cannot move one byte of Dawnmere.zscen,
	// whereas authoring them from inside that block would splice new steps into a
	// committed scene's step list.
	//
	// ★★ THE MARKERS LANDED FIRST AND ALONE (R1-2), AND THE SENSORS FOLLOWED AS A
	// SET (R1-3). ZM_GameStateManager::IsWarpDestinationValid consults ONLY the
	// compiled world table -- never the destination scene -- so a warp trigger
	// shipped before its destination MARKER exists is ACCEPTED, and the machine
	// then stalls in ZM_WARP_TRANSITION_WAITING_FOR_SCENE / _WAITING_FOR_SPAWN
	// until that barrier's frame budget expires (ZM-D-200): an opaque fade over a
	// frozen player, then a Zenith_Error naming the state and the tag, with no
	// crash and no red test either way. That ordering is why R1-2 authored every
	// marker and zero triggers, and why R1-3 authors all FOUR gates in one commit
	// -- two here, one in Thornacre below, one appended to the Dawnmere block
	// above. Splitting them would leave a one-way seam nobody notices.
	//
	// ★ STILL NO GYM DOOR (ZM-D-196) AND STILL NO TRAINERS (R1-5/R1-6) in either
	// block below. The gates are the only entities R1-3 adds.
	//
	// ★ AND NOTHING IS AUTHORED INTO DAWNMERE HERE. Dawnmere's own "FromRoute1"
	// arrival marker and its north gate are both steps of the Dawnmere block
	// above, deliberately, so this block only ever touches its own two scenes.
	//
	// ★ THE HEIGHTS BELOW ARE PROVISIONAL, AND KNOWINGLY SO. Both placement
	// headers' measured-ground tables are SEEDED WITH THEIR RECIPE TARGET HEIGHT
	// rather than frozen raycasts -- there is no baked Route1/Thornacre scene to
	// raycast against until this block has run once. R1-2 step 1 measured the cost
	// of that seed: Dawnmere's route-seam column, the first Dawnmere column inside
	// a FLATTEN corridor, froze at target + 0.366 (a flatten dab drives ground TO
	// the target), while unflattened Dawnmere columns read ~+2 m. Every anchor
	// here sits on a flattened pad or inside the lane's flatten corridor, so each
	// seed is expected within a fraction of a metre. The freeze slice re-measures
	// them and re-authors both scenes.
	// ========================================================================
	if (xTerrainBatch.m_bAuthorDawnmereScene)
	{
		// ---- ROUTE 1 -------------------------------------------------------
		//
		// ★ EVERY COORDINATE, SCALE, NAME AND TAG COMES FROM
		// Source/World/ZM_Route1Placement.h or from the compiled world table.
		// Nothing here re-spells a literal: a constant spelled at two sites cannot
		// red a drift, and these particular constants are due to be RE-PASTED as a
		// set when the ground oracle freezes them -- a second copy would silently
		// not be.
		const ZM_TerrainAuthoringRecipe& xRoute1Recipe =
			ZM_GetRoute1TerrainRecipe();
		xAuto.AddStep_CreateScene(szZM_ROUTE1_SCENE_NAME);
		ZM_QueueTerrainHostEntity(
			xAuto, szZM_ROUTE1_TERRAIN_ENTITY_NAME, xRoute1Recipe);

		// ---- R1-4 scene-attach (ZM-66/ZM-D-205): the wild-encounter producer ---
		//
		// ★★ ROUTE 1 ONLY -- APPENDED HERE, NEVER INSIDE ZM_QueueTerrainHostEntity.
		// That helper also authors Thornacre's terrain host (see the Thornacre call
		// below), and ZM-D-196 rules Thornacre a TRAVERSAL STUB for this milestone --
		// terrain, one arrival marker, a player, a camera and a return trigger,
		// deliberately nothing else. Folding this step into the shared helper would
		// give Thornacre a wild-encounter surface the design does not want, and no
		// existing test would have caught it (nothing asserted Thornacre carried
		// ZERO ZM_TallGrassSystem, because until this ticket nothing had reason to).
		// Dawnmere authors its own terrain host INLINE, a third path untouched here.
		//
		// ★ LANDS ON THE TERRAIN HOST ENTITY because ZM_QueueTerrainHostEntity's own
		// AddStep_CreateEntity is still the most recently SELECTED entity at this
		// point -- nothing between its return and this line issues another
		// CreateEntity or SelectEntityByName step, and AddComponentToSelected always
		// targets the current selection (Zenith_Editor::CreateEntity calls
		// SelectEntity internally).
		//
		// ★ APPENDED AFTER ZM_TerrainGrass, PER ZM-D-148 (dense authoring-order file
		// indices: this is a NEW component record on an already-authored entity, not
		// a reorder of an existing one) and per ZM_QueueTerrainHostEntity's own
		// "goes LAST" rule for the grass component immediately above it.
		//
		// ★ NO RUNTIME ATTACH HOOK. ZM-D-204 Decision 4 already weighed and declined
		// one; this ticket does not revisit that ruling. The component is SCENE
		// CONTENT, exactly like its ZM_TerrainGrass sibling.
		xAuto.AddStep_AddComponent("ZM_TallGrassSystem");

		// ---- The two ARRIVAL markers ---------------------------------------
		//
		// ★★ EACH CARRIES ITS **INBOUND** TAG, RESOLVED FROM THE SOURCE REGION'S
		// CONNECTION LIST -- south from DAWNMERE ("FromDawnmere"), north from
		// THORNACRE ("FromThornacre"). Route 1's own gate accessors both answer
		// "FromRoute1" and are OUTBOUND; the full trap, and why using one here
		// would break the game with every test green, is written out at
		// ZM_ResolveInboundSpawnTag above. Read it before touching either step.
		//
		// ★ THE TRANSFORM IS THE MARKER'S FEET, NEVER A BODY CENTRE.
		// ZM_GameStateManager::CalculateSpawnCenter adds the capsule half-extent at
		// warp time, so authoring a centre here would drop every arriving player in
		// from half a body up. Same vocabulary as the shipped FromLabSpawn.
		//
		// ★ AND THE TWO MARKERS READ TWO DIFFERENT MEASURED COLUMNS. They are
		// 1312 m apart on eroded terrain; the placement header refuses to give them
		// one shared height for exactly that reason, and neither accessor may be
		// substituted for the other.
		const Zenith_Maths::Vector3 xRoute1SouthArrivalFeet =
			ZM_GetRoute1SouthArrivalFeet();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_SOUTH_ARRIVAL_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1SouthArrivalFeet.x, xRoute1SouthArrivalFeet.y,
			xRoute1SouthArrivalFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1SouthArrivalSpawnPoint);

		const Zenith_Maths::Vector3 xRoute1NorthArrivalFeet =
			ZM_GetRoute1NorthArrivalFeet();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_NORTH_ARRIVAL_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1NorthArrivalFeet.x, xRoute1NorthArrivalFeet.y,
			xRoute1NorthArrivalFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1NorthArrivalSpawnPoint);

		// ---- The player ----------------------------------------------------
		//
		// ★ NAMED "Player" BY CONVENTION -- no longer a contract
		// (Q-2026-08-15-002, fixed 2026-08-15). ZM_FollowCamera::ResolveTarget used
		// to resolve its subject with FindEntityByName("Player"), the only
		// FindEntityByName call in the whole game layer, so a rename cleared the
		// camera's target and ZM_GameStateManager::PollForCameraAndBeginFadeIn
		// bare-returned on a barrier that, back then, had no timeout at all -- a
		// permanent black screen. It now acquires the unique ZM_PlayerController in
		// the camera's own scene.
		//
		// ★ SO WHAT MATTERS HERE IS THE COMPONENT BELOW, NOT THE NAME: every scene
		// that authors a ZM_FollowCamera must author a ZM_PlayerController on
		// exactly one entity. Delete that AddComponent and the camera acquires
		// nothing -- the same broken arrival by a different route, now bounded by
		// that barrier's frame budget and a named Zenith_Error (ZM-D-200) rather
		// than permanent.
		//
		// ★ CAPSULE + DYNAMIC (it is the one body here that moves), authored ONE
		// half-extent ABOVE its resting centre (ZM-D-184): a dynamic body authored
		// at exact ground contact bursts physics substeps on its first frame and
		// falls THROUGH the terrain -- that is how Vesper vanished. The clearance
		// is baked into ZM_GetRoute1AuthoredPlayerCentre(), so it exists in one
		// place rather than at every author site.
		const Zenith_Maths::Vector3 xRoute1PlayerCentre =
			ZM_GetRoute1AuthoredPlayerCentre();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_PLAYER_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1PlayerCentre.x, xRoute1PlayerCentre.y, xRoute1PlayerCentre.z);
		xAuto.AddStep_SetTransformScale(
			fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_PlayerController");

		// ---- The follow camera ---------------------------------------------
		//
		// Yaw 0 is +Z (ZM_ForwardFromRotation rotates the +Z basis) and the spring
		// places the camera BEHIND its subject, so yaw 0 looks straight up the
		// route with the camera trailing south over the (future) gate, where there
		// is nothing to clip into. The far plane is a ROUTE-LENGTH decision, not a
		// default: the region is 1536 m deep on Z, so the interiors' 100 m plane
		// would clip the world away a few strides ahead of the player.
		//
		// ★ THE AUTHORED POSE IS NOT THE SETTLED POSE, AND IS NOT MEANT TO BE.
		// ZM_FollowCamera::OnStart clears the spring, so the first OnLateUpdate
		// after the scene loads SNAPS to ComputeDesiredPosition rather than easing
		// toward it; the authored value is only ever the pose of a camera that has
		// not ticked yet. The shipped PlayerHome and ProfLab cameras are authored
		// the same way, and ProfLab's header spells out why an assertion that the
		// two match in Y would red.
		//
		// ★★ AND IT IS PLAIN CONSTANT ARITHMETIC, NEVER
		// ZM_GetRoute1SettledCameraPosition(). That accessor calls std::cos /
		// std::sin, and MSVC Debug and Release codegen disagree on libm results by
		// 1-2 ULP -- which is precisely how a committed .zscen came to ping-pong in
		// git forever (ZM-D-183). It is a CHECK a boot unit runs; it is not a value
		// AddStep_* may ever take. What is authored below is the arm swung straight
		// back along -Z at yaw 0, at the follow camera's pivot height above the
		// arriving body's centre.
		const Zenith_Maths::Vector3 xRoute1ArrivalBodyCentre =
			ZM_GetRoute1SouthArrivalBodyCentre();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_CAMERA_ENTITY_NAME);
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(
			xRoute1ArrivalBodyCentre.x,
			xRoute1ArrivalBodyCentre.y + fZM_ROUTE1_CAMERA_PIVOT_HEIGHT,
			xRoute1ArrivalBodyCentre.z - fZM_ROUTE1_CAMERA_ARM);
		xAuto.AddStep_SetCameraYaw(fZM_ROUTE1_CAMERA_YAW);
		xAuto.AddStep_SetCameraPitch(fZM_ROUTE1_CAMERA_PITCH);
		xAuto.AddStep_SetCameraFOV(glm::radians(fZM_ROUTE1_CAMERA_FOV_DEGREES));
		xAuto.AddStep_SetCameraNear(fZM_ROUTE1_CAMERA_NEAR);
		xAuto.AddStep_SetCameraFar(fZM_ROUTE1_CAMERA_FAR);
		xAuto.AddStep_AddComponent("ZM_FollowCamera");
		xAuto.AddStep_SetAsMainCamera();

		// ---- R1-3: THE TWO SEAM GATES --------------------------------------
		//
		// ★★ THEY ASK FOR THE SAME TAG AND DIFFER ONLY IN THEIR TARGET BUILD
		// INDEX, WHICH IS WHY THE TWO STEPS BELOW MUST NOT BE READ AS
		// INTERCHANGEABLE. Both ZM_GetRoute1SouthGateSpawnTag() and
		// ZM_GetRoute1NorthGateSpawnTag() answer "FromRoute1"; the discriminating
		// value is 2 (Dawnmere) versus 3 (Thornacre), emitted as a raw u_int
		// immediately before the 32-byte tag buffer. So a SOUTH/NORTH swap
		// changes not one byte any name-based needle searches for, and produces a
		// route whose south end leads north. Tests/ZM_Tests_CommittedSceneBytes.cpp
		// needles the WHOLE serialized payload per gate for exactly that reason,
		// and each accessor above resolves by NAMED TARGET SCENE rather than by
		// connection index so the two cannot be transposed at this call site.
		//
		// ★ APPENDED AFTER THE CAMERA rather than slotted in beside the arrival
		// markers they stand beyond. Route1.zscen is a COMMITTED asset with DENSE
		// AUTHORING-ORDER file indices (ZM-D-148): appending costs two new entity
		// records, while inserting after the markers would renumber the player and
		// the camera and turn a two-entity change into a whole-file diff.
		//
		// ★ EACH SENSOR SITS **ON** ITS OWN MEASURED GROUND. The centres come
		// from ZM_GetRoute1SouthGate() / ZM_GetRoute1NorthGate(), which read the
		// two gate columns of the frozen Route 1 ground table -- the boxes are
		// 1336 m apart on eroded terrain and a shared height would part-bury one
		// and float the other with every scale constant still correct. Nothing
		// here re-spells a coordinate.
		//
		// Step list mirrors the shipped LabDoorTrigger verbatim: create,
		// transform, scale, static AABB body, ZM_WarpTrigger, configure. No
		// rotation step -- an AABB body forces JPH::Quat::sIdentity() anyway
		// (ZM-D-193), and identity is bit-exact in every configuration.
		const ZM_Route1Volume xRoute1SouthGate = ZM_GetRoute1SouthGate();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_SOUTH_GATE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1SouthGate.m_xCenter.x, xRoute1SouthGate.m_xCenter.y,
			xRoute1SouthGate.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xRoute1SouthGate.m_xScale.x, xRoute1SouthGate.m_xScale.y,
			xRoute1SouthGate.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1SouthGateTrigger);

		const ZM_Route1Volume xRoute1NorthGate = ZM_GetRoute1NorthGate();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_NORTH_GATE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1NorthGate.m_xCenter.x, xRoute1NorthGate.m_xCenter.y,
			xRoute1NorthGate.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xRoute1NorthGate.m_xScale.x, xRoute1NorthGate.m_xScale.y,
			xRoute1NorthGate.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1NorthGateTrigger);

		// ---- ZM-27: THE THREE GROUND-ITEM PROPS ----------------------------
		//
		// ★ APPENDED AFTER THE GATES, for the identical reason the gates were
		// appended after the camera: Route1.zscen is a COMMITTED asset with DENSE
		// AUTHORING-ORDER file indices (ZM-D-148). Appending costs three new
		// entity records; inserting anywhere earlier would renumber every entity
		// after the insertion point and turn a three-entity change into a
		// whole-file diff.
		//
		// ★★ NO COLLIDER, AND THAT IS THE DESIGN, NOT AN OMISSION. A prop is
		// taken by INTERACT REACH (ZM_InteractionRuntime picks it out of the same
		// probe set as an NPC), never by touching it, so a body would buy nothing
		// and cost two things: it would stand in the walked lane as an obstacle,
		// and it would become a SOLID body over its own ground column -- which
		// ZM_Route1GroundTruth_Test treats as a failure rather than a filter,
		// because its per-row ignore holds exactly one entity. A sensor would dodge
		// the second problem (both Raycast overloads skip sensors, ZM-D-173) and
		// not the first.
		//
		// ★ EACH PROP SITS **ON** ITS OWN MEASURED COLUMN. The centres come from
		// the three accessors, which read the frozen ground table through
		// ZM_Route1GroundFeetY and add half the cube edge in one place. The three
		// columns span 26.189 .. 26.661 over a kilometre of route, so a shared
		// height would part-bury one and float another.
		//
		// ★ THE POSITION AND SCALE STEPS BELOW ARE UNCHANGED BY ZM-67 AND MUST
		// STAY THAT WAY. The props stopped being grey cubes -- ZM_GreyboxVisual's
		// PROP branch gives each one a generated model chosen from the item it
		// yields and whether this save has taken it -- but not one authored number
		// moved to make that happen. The new art was anchored to the transform
		// (fZM_PROP_ITEM_BASE_Y) rather than the transform re-authored to the art,
		// because these coordinates are what hold each prop inside
		// fZM_INTERACT_MAX_DISTANCE of the walked lane (ZM-D-207) and a prop nudged
		// off it can never be picked up in any playthrough with every other check
		// still green. The visual is a runtime decision and reaches no .zscen byte.
		const ZM_Route1Volume xRoute1SouthSalve = ZM_GetRoute1SouthSalveProp();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_PROP_SOUTH_SALVE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1SouthSalve.m_xCenter.x, xRoute1SouthSalve.m_xCenter.y,
			xRoute1SouthSalve.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xRoute1SouthSalve.m_xScale.x, xRoute1SouthSalve.m_xScale.y,
			xRoute1SouthSalve.m_xScale.z);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_GroundItemProp");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1SouthSalveProp);

		const ZM_Route1Volume xRoute1LaneCatchorb = ZM_GetRoute1LaneCatchorbProp();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_PROP_LANE_CATCHORB_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1LaneCatchorb.m_xCenter.x, xRoute1LaneCatchorb.m_xCenter.y,
			xRoute1LaneCatchorb.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xRoute1LaneCatchorb.m_xScale.x, xRoute1LaneCatchorb.m_xScale.y,
			xRoute1LaneCatchorb.m_xScale.z);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_GroundItemProp");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1LaneCatchorbProp);

		const ZM_Route1Volume xRoute1NorthSalve = ZM_GetRoute1NorthSalveProp();
		xAuto.AddStep_CreateEntity(szZM_ROUTE1_PROP_NORTH_SALVE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xRoute1NorthSalve.m_xCenter.x, xRoute1NorthSalve.m_xCenter.y,
			xRoute1NorthSalve.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xRoute1NorthSalve.m_xScale.x, xRoute1NorthSalve.m_xScale.y,
			xRoute1NorthSalve.m_xScale.z);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_GroundItemProp");
		xAuto.AddStep_Custom(&ZM_ConfigureRoute1NorthSalveProp);

		xAuto.AddStep_SaveScene(GAME_ASSETS_DIR "Scenes/Route1" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();

		// ---- THORNACRE -----------------------------------------------------
		//
		// ★★ A TRAVERSAL STUB BY RULING (ZM-D-196), AND IT MUST STAY ONE IN THIS
		// MILESTONE: terrain, ONE arrival marker, a player, a camera and -- as of
		// R1-3 -- the return gate that makes it traversable rather than a dead
		// end. STILL NO GYM DOOR. The compiled world table already carries the
		// Thornacre -> Gym1 ("Door") edge and that edge is deliberately UNBACKED
		// -- authoring a door into a room nobody has built is the
		// WAITING_FOR_SCENE stall described at the top of this block. No
		// trainers and no shops either.
		//
		// ★ uZM_THORNACRE_PLACEMENT_ENTITY_COUNT IS 5u AND ITS INDEX 4 IS THE
		// TRIGGER "ThornacreSouthGate". It is a NAME INVENTORY for the boot units,
		// NOT an authoring loop bound -- walking [0, 5) here would author the
		// entities in the inventory's order rather than in the order this scene's
		// committed bytes were written in, which is itself contract (ZM-D-148).
		// The five entities below are spelled by name, one at a time, on purpose.
		const ZM_TerrainAuthoringRecipe& xThornacreRecipe =
			ZM_GetThornacreTerrainRecipe();
		xAuto.AddStep_CreateScene(szZM_THORNACRE_SCENE_NAME);
		ZM_QueueTerrainHostEntity(
			xAuto, szZM_THORNACRE_TERRAIN_ENTITY_NAME, xThornacreRecipe);

		// The single arrival marker, carrying its INBOUND tag: the source is
		// ROUTE1, so the tag is Route 1's THORNACRE edge ("FromRoute1"), resolved
		// by walking that row -- never spelled, and never taken from Thornacre's
		// own outbound return accessor. Feet, not a body centre; see the Route 1
		// markers above.
		const Zenith_Maths::Vector3 xThornacreArrivalFeet =
			ZM_GetThornacreSouthArrivalFeet();
		xAuto.AddStep_CreateEntity(szZM_THORNACRE_SOUTH_ARRIVAL_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xThornacreArrivalFeet.x, xThornacreArrivalFeet.y,
			xThornacreArrivalFeet.z);
		xAuto.AddStep_AddComponent("ZM_SpawnPoint");
		xAuto.AddStep_Custom(&ZM_ConfigureThornacreSouthArrivalSpawnPoint);

		// The player -- "Player" here too, for the reason argued at Route 1's, and
		// likewise a DYNAMIC capsule authored one half-extent clear of the ground
		// (ZM-D-184), with the clearance living inside the accessor.
		const Zenith_Maths::Vector3 xThornacrePlayerCentre =
			ZM_GetThornacreAuthoredPlayerCentre();
		xAuto.AddStep_CreateEntity(szZM_THORNACRE_PLAYER_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xThornacrePlayerCentre.x, xThornacrePlayerCentre.y,
			xThornacrePlayerCentre.z);
		xAuto.AddStep_SetTransformScale(
			fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE, fZM_HUMAN_VISUAL_SCALE);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		xAuto.AddStep_AddComponent("ZM_GreyboxVisual");
		xAuto.AddStep_AddComponent("ZM_PlayerController");

		// The follow camera. The player arrives at the town's SOUTH edge and walks
		// NORTH into it, so yaw 0 looks into town with the camera trailing south
		// out over the route gate. Same authored-pose-is-not-settled-pose and same
		// no-libm-in-committed-bytes rules as Route 1's camera above -- the built
		// value is arithmetic on compiled constants, never
		// ZM_GetThornacreSettledCameraPosition().
		const Zenith_Maths::Vector3 xThornacreArrivalBodyCentre =
			ZM_GetThornacreSouthArrivalBodyCentre();
		xAuto.AddStep_CreateEntity(szZM_THORNACRE_CAMERA_ENTITY_NAME);
		xAuto.AddStep_AddCamera();
		xAuto.AddStep_SetCameraPosition(
			xThornacreArrivalBodyCentre.x,
			xThornacreArrivalBodyCentre.y + fZM_THORNACRE_CAMERA_PIVOT_HEIGHT,
			xThornacreArrivalBodyCentre.z - fZM_THORNACRE_CAMERA_ARM);
		xAuto.AddStep_SetCameraYaw(fZM_THORNACRE_CAMERA_YAW);
		xAuto.AddStep_SetCameraPitch(fZM_THORNACRE_CAMERA_PITCH);
		xAuto.AddStep_SetCameraFOV(glm::radians(fZM_THORNACRE_CAMERA_FOV_DEGREES));
		xAuto.AddStep_SetCameraNear(fZM_THORNACRE_CAMERA_NEAR);
		xAuto.AddStep_SetCameraFar(fZM_THORNACRE_CAMERA_FAR);
		xAuto.AddStep_AddComponent("ZM_FollowCamera");
		xAuto.AddStep_SetAsMainCamera();

		// ---- R1-3: THE RETURN GATE -----------------------------------------
		//
		// The third of the four seam sensors, and the ONE entity that takes a
		// player back out of this town. It stands 12 m SOUTH of the arrival
		// marker, inside the same flattened "RouteGate" pad, so an arriving body
		// clears its near face by 9 m -- an overlap on arrival would fire on the
		// first contact tick and ping-pong the player between two scenes forever
		// (the derivation is in Source/World/ZM_ThornacrePlacement.h).
		//
		// ★ ITS TARGET AND TAG ARE RESOLVED FROM THE COMPILED TABLE by
		// ZM_ConfigureThornacreSouthGateTrigger -- Route 1's build index and
		// "FromThornacre", never Thornacre's own inbound "FromRoute1", which is
		// what the ARRIVAL marker above carries. Nothing here spells either.
		//
		// ★ APPENDED AFTER THE CAMERA, for the ZM-D-148 dense-index reason the
		// Route 1 gates above give. Step list mirrors the shipped LabDoorTrigger
		// verbatim, and carries no rotation step: this stub authors nothing that
		// has to face a direction, and an AABB body forces identity anyway.
		const ZM_ThornacreVolume xThornacreSouthGate = ZM_GetThornacreSouthGate();
		xAuto.AddStep_CreateEntity(szZM_THORNACRE_SOUTH_GATE_ENTITY_NAME);
		xAuto.AddStep_SetEntityTransient(false);
		xAuto.AddStep_SetTransformPosition(
			xThornacreSouthGate.m_xCenter.x, xThornacreSouthGate.m_xCenter.y,
			xThornacreSouthGate.m_xCenter.z);
		xAuto.AddStep_SetTransformScale(
			xThornacreSouthGate.m_xScale.x, xThornacreSouthGate.m_xScale.y,
			xThornacreSouthGate.m_xScale.z);
		xAuto.AddStep_AddCollider();
		xAuto.AddStep_AddColliderShape(
			COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);
		xAuto.AddStep_AddComponent("ZM_WarpTrigger");
		xAuto.AddStep_Custom(&ZM_ConfigureThornacreSouthGateTrigger);

		xAuto.AddStep_SaveScene(
			GAME_ASSETS_DIR "Scenes/Thornacre" ZENITH_SCENE_EXT);
		xAuto.AddStep_UnloadScene();
	}

	xAuto.AddStep_LoadInitialScene(&Project_LoadInitialScene);
}
#endif

void Project_LoadInitialScene()
{
	// ★ ONE ENUMERABLE TABLE, WALKED. This replaced five hand-written
	// RegisterSceneBuildIndex calls that no boot unit could see. The table lives
	// in Source/World/ZM_SceneRegistry.h and the ZM_SceneRegistry boot units walk
	// the SAME rows -- so "the gate trigger shipped, the registration did not"
	// (an ACCEPTED warp that stalls in WAITING_FOR_SCENE / WAITING_FOR_SPAWN until
	// each barrier's frame budget expires, ZM-D-200) reds at boot instead of
	// shipping as a black screen that ends in a runtime error.
	//
	// The path is built here rather than in the header so the header stays free
	// of <string> and of GAME_ASSETS_DIR; the concatenation is byte-identical to
	// the compile-time form it replaced, and matches the shape
	// ZM_BattleTransition.cpp already uses for the additive battle scene.
	for (u_int uIndex = 0u; uIndex < ZM_GetSceneRegistrationCount(); ++uIndex)
	{
		const ZM_SceneRegistration& xRow = ZM_GetSceneRegistration(uIndex);
		const u_int uBuildIndex = ZM_GetSceneRegistrationBuildIndex(xRow);

		// Unreachable for the seven shipped rows, but never pass the sentinel
		// into an asserting API: static_cast<int>(0xFFFFFFFFu) is -1 and
		// RegisterSceneBuildIndex asserts the index is non-negative.
		if (uBuildIndex == uZM_SCENE_REGISTRATION_BUILD_INDEX_UNRESOLVED)
		{
			continue;
		}

		const std::string strScenePath = std::string(GAME_ASSETS_DIR)
			+ "Scenes/" + xRow.m_szFileStem + ZENITH_SCENE_EXT;
		g_xEngine.Scenes().RegisterSceneBuildIndex(
			static_cast<int>(uBuildIndex), strScenePath);
	}

	g_xEngine.Scenes().LoadSceneByIndex(
		static_cast<int>(ZM_GetWorldSpec(ZM_SCENE_FRONTEND).m_uBuildIndex),
		SCENE_LOAD_SINGLE);
}
