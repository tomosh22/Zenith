#include "Zenith.h"

// ============================================================================
// ZM_Tests_HumanVisual -- the boot-unit gate for the human BODY CONTRACT, the
// cold-start fallback, and the promise that giving NPCs models left every
// blockout wall, floor, door and shell exactly where it was.
//
// It drives ZM_GreyboxVisual through the component-meta registry rather than by
// name: the class is file-local to Zenithmon.cpp and unnameable from a test TU,
// but its registered "ZM_GreyboxVisual" meta exposes create + OnStart, which is
// the whole surface these cases need.
//
//   1. HumanVisual_BodyContractIsTheShippedBody -- the compiled contract still
//      answers exactly what the retired scale-derived formula answered.
//   2. HumanVisual_UnservedEntityIsLeftAlone -- an entity that is neither a human
//      nor a ground-item prop comes away with NO geometry, no animator and no
//      explicit body. (Was "BlockoutIsUntouched"; the blockout arm was removed
//      once every blockout in the game became collider-only under a real model.)
//   3. HumanVisual_ColdFallbackShipsTheContractBlock -- a resolved NPC on a cold
//      tree gets a proportioned palette block whose GAMEPLAY dimensions are the
//      contract, not the drawing scale.
//   4. HumanVisual_ColdStartIsIdempotent -- a second OnStart appends nothing.
//   5. HumanVisual_AssetPolicyRestoresTheProductionDefault -- a scoped override
//      leaves the default policy AND its bake latch as it found them.
//   6. HumanVisual_RestartPreservesTheAnimatorRig -- the WARM branch: a repeated
//      OnStart re-binds the controller WITHOUT wiping or duplicating the layer,
//      the state machine or the clips.
//
// HEADLESS: no GPU. Cases 1-5 read no disk either. ★ CASE 6 IS THE EXCEPTION and
// is deliberately so -- it runs under the PRODUCTION policy, so on a warm tree it
// loads a real .zmodel. That is the only way to exercise the branch at all, and
// it records which branch it took rather than passing vacuously on a cold one.
// Cases that need a physics body use the live simulation the boot gate already has.
// ============================================================================

#include "Core/Zenith_TestFramework.h"
#include "Core/Zenith_Engine.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshGeometryAsset.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
// Zenith_AnimatorComponent.h deliberately carries ZERO Flux includes (it is a
// forwarding handle), so the rig read-back below needs these directly.
#include "Flux/MeshAnimation/Flux_AnimationClip.h"          // the clip collection
#include "Flux/MeshAnimation/Flux_AnimationController.h"
#include "Flux/MeshAnimation/Flux_AnimationLayer.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Maths/Zenith_FrustumCulling.h"   // Zenith_AABB -- the instance's local bounds
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"
#include "Zenithmon/Source/Gen/ZM_HumanAppearance.h"
#include "Zenithmon/Source/Gen/ZM_HumanGen.h"
#include "Zenithmon/Source/World/ZM_HumanAssetPolicy.h"
#include "Zenithmon/Source/World/ZM_HumanBody.h"

#include <cmath>
#include <string>

namespace
{
	// The registered NAME of the visual, spelled exactly as Zenithmon.cpp's
	// ZENITH_REGISTER_COMPONENT line spells it.
	constexpr const char* szHV_VISUAL_META = "ZM_GreyboxVisual";

	// Create the visual on xEntity and run its OnStart, through the meta registry.
	// Returns false (having done nothing) if the meta is missing, which is itself
	// worth failing on -- a renamed component would silently make every case below
	// test an entity with no visual at all.
	bool HVStartVisual(Zenith_Entity& xEntity)
	{
		const Zenith_ComponentMeta* pxMeta =
			Zenith_ComponentMetaRegistry::Get().GetMetaByName(szHV_VISUAL_META);
		if (pxMeta == nullptr || pxMeta->m_pfnCreate == nullptr
			|| pxMeta->m_pfnOnStart == nullptr)
		{
			return false;
		}
		if (pxMeta->m_pfnHasComponent == nullptr || !pxMeta->m_pfnHasComponent(xEntity))
		{
			pxMeta->m_pfnCreate(xEntity);
		}
		pxMeta->m_pfnOnStart(xEntity);
		return true;
	}

	bool HVRestartVisual(Zenith_Entity& xEntity)
	{
		const Zenith_ComponentMeta* pxMeta =
			Zenith_ComponentMetaRegistry::Get().GetMetaByName(szHV_VISUAL_META);
		if (pxMeta == nullptr || pxMeta->m_pfnOnStart == nullptr)
		{
			return false;
		}
		pxMeta->m_pfnOnStart(xEntity);
		return true;
	}

	// The world-space Y extent of whatever the model component is drawing, i.e.
	// mesh bounds times the entity's uniform scale. This is what an eye sees.
	bool HVMeasureDrawnHeight(const Zenith_Entity& xEntity, float& fHeightOut,
		float& fWidthOut)
	{
		const Zenith_ModelComponent* pxModel =
			xEntity.TryGetComponent<Zenith_ModelComponent>();
		if (pxModel == nullptr || pxModel->GetNumMeshes() == 0u)
		{
			return false;
		}
		Flux_MeshInstance* pxInstance = pxModel->GetMeshInstance(0u);
		if (pxInstance == nullptr)
		{
			return false;
		}
		// The instance's own LOCAL AABB: it reads baked bounds for an asset-backed
		// mesh and computes them from the vertices for a procedural one, so the same
		// measurement serves a human .zmodel and a fallback block.
		const Zenith_AABB& xBounds = pxInstance->GetLocalBounds();
		Zenith_Maths::Vector3 xScale(1.0f);
		xEntity.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
		const float fMinY = xBounds.m_xMin.y;
		const float fMaxY = xBounds.m_xMax.y;
		const float fMinX = xBounds.m_xMin.x;
		const float fMaxX = xBounds.m_xMax.x;
		if (!(fMinY < fMaxY) || !(fMinX < fMaxX))
		{
			return false;
		}
		fHeightOut = (fMaxY - fMinY) * xScale.y;
		fWidthOut  = (fMaxX - fMinX) * xScale.x;
		return true;
	}

	const Zenith_MaterialAsset* HVFirstMaterial(const Zenith_Entity& xEntity)
	{
		const Zenith_ModelComponent* pxModel =
			xEntity.TryGetComponent<Zenith_ModelComponent>();
		return (pxModel != nullptr && pxModel->GetNumMeshes() > 0u)
			? pxModel->GetMaterial(0u)
			: nullptr;
	}
}

// ############################################################################
// 1. The body contract IS the body the game used to compute
// ############################################################################

// The five constants in Source/World/ZM_HumanBody.h replaced a body derived from
// TRANSFORM SCALE. This case pins that the replacement was exact, by re-deriving
// the RETIRED formula here -- the only place it still exists -- and requiring it
// to agree. It also pins the two conversions the whole game hangs off the answer:
// a spawn marker's FEET to a body CENTRE, and the ground probe's reach.
//
// This clause is the migration's receipt. Once it has been green for a release it
// may be deleted; while the change is fresh it is what says "nothing moved".
ZENITH_TEST(ZM_HumanVisual, HumanVisual_BodyContractIsTheShippedBody)
{
	// THE RETIRED FORMULA, spelled once: ZM_PlayerController::
	// CalculateCapsuleHalfExtent, applied to the authored 0.8 x 1.8 x 0.8 body box.
	const float fRetiredRadius = 0.8f * 0.5f;
	const float fRetiredHalfCylinder = 1.8f * 0.5f - fRetiredRadius;
	const float fRetiredHalfExtent = fRetiredRadius + fRetiredHalfCylinder;

	ZENITH_ASSERT_EQ_FLOAT(fZM_HUMAN_BODY_HALF_HEIGHT, fRetiredHalfExtent, 0.0f,
		"the contract's half height is not BIT-IDENTICAL to what the retired "
		"scale-derived formula returned -- every spawn point in the game moved");
	ZENITH_ASSERT_EQ_FLOAT(fZM_HUMAN_BODY_CAPSULE_RADIUS, fRetiredRadius, 0.0f,
		"the contract's capsule radius is not the retired one");
	ZENITH_ASSERT_EQ_FLOAT(fZM_HUMAN_BODY_CAPSULE_HALF_CYLINDER, fRetiredHalfCylinder,
		1.0e-6f, "the contract's cylinder half-height is not the retired one");

	// The feet -> centre conversion every warp, every spawn marker and the authored
	// placement all share.
	const Zenith_Maths::Vector3 xFeet(120.0f, 25.05666f, 60.0f);
	const Zenith_Maths::Vector3 xCentre = xFeet
		+ Zenith_Maths::Vector3(0.0f, fZM_HUMAN_BODY_HALF_HEIGHT, 0.0f);
	ZENITH_ASSERT_EQ_FLOAT(xCentre.y, 25.95666f, 1.0e-5f,
		"the Dawnmere town-centre body height moved");
	ZENITH_ASSERT_EQ_FLOAT(xCentre.y - fZM_HUMAN_BODY_HALF_HEIGHT, xFeet.y, 1.0e-5f,
		"the feet <-> centre conversion is not its own inverse");

	// The ground probe must reach past the SOLE, not past the drawing scale. A
	// human drawn at fZM_HUMAN_VISUAL_SCALE is well under a metre of transform, so
	// a scale-derived reach would probe from the navel and never find the floor.
	ZENITH_ASSERT_TRUE(fZM_HUMAN_BODY_HALF_HEIGHT > fZM_HUMAN_VISUAL_SCALE,
		"the body's half height (%.4f) is not clear of the drawing scale (%.4f) -- "
		"the two are close enough that a mixed-up call site would go unnoticed",
		fZM_HUMAN_BODY_HALF_HEIGHT, fZM_HUMAN_VISUAL_SCALE);
	ZENITH_ASSERT_TRUE(fZM_HUMAN_VISUAL_SCALE > 0.0f && fZM_HUMAN_VISUAL_SCALE < 1.0f,
		"the visual scale %.6f is not a plausible shrink of the ~2.6-unit loft body",
		fZM_HUMAN_VISUAL_SCALE);
}

// ############################################################################
// 2. An entity this component does NOT serve is left alone
// ############################################################################

// ★★ THIS TEST USED TO BE "BLOCKOUTS ARE UNTOUCHED", AND THE POPULATION IT
// NAMED NO LONGER EXISTS. Every wall, floor, door and lintel in the game wore a
// greybox unit cube built by ZM_GreyboxVisual::ApplyBlockout; it asserted the
// cube, the shipped grey, the unit extent and the scale-derived body. Dawnmere's
// eight exterior blockouts became collider-only under real building models, the
// two interiors' fourteen became collider-only under real room models,
// ZM_QueueGreyboxBlock went with its last caller, and the arm itself was removed.
//
// So the fixture below is no longer "a wall". It is an entity carrying this
// component that resolves to NO human row and carries NO ZM_GroundItemProp --
// which used to be silently absorbed as "it must be a wall" and is a WIRING
// MISTAKE now. That absorption is exactly how a ground-item prop with a missing
// component became an anonymous grey cube nobody questioned.
//
// ★ WHAT SURVIVED IS THE HALF THAT WAS ALWAYS THE POINT: none of the human
// machinery may reach an entity the human branch did not claim. That clause is
// unchanged; what changed is that there is now no cube either.
ZENITH_TEST(ZM_HumanVisual, HumanVisual_UnservedEntityIsLeftAlone)
{
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "no active scene to build the fixture in");
	if (pxSceneData == nullptr) { return; }

	Zenith_Entity xWall = g_xEngine.Scenes().CreateEntity(pxSceneData, "HV_BlockoutWall");
	Zenith_TransformComponent& xTransform = xWall.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(600.0f, 400.0f, 400.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(16.0f, 6.0f, 40.0f));

	Zenith_ColliderComponent& xCollider = xWall.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

	ZENITH_ASSERT_TRUE(HVStartVisual(xWall),
		"the '%s' component meta is missing -- nothing below tested a visual",
		szHV_VISUAL_META);

	// NOTHING IS DRAWN. The component serves humans and ground-item props; this
	// entity is neither, so it must come away with no geometry rather than the
	// grey cube that used to stand in for one.
	const Zenith_ModelComponent* pxModel = xWall.TryGetComponent<Zenith_ModelComponent>();
	if (pxModel != nullptr)
	{
		ZENITH_ASSERT_EQ(pxModel->GetNumMeshes(), 0u,
			"an entity this component does not serve came away drawing %u meshes -- "
			"the blockout arm is back, or something else is building geometry for a "
			"population that has none", pxModel->GetNumMeshes());
	}

	// ...and none of the human machinery reached it. This is the clause the test
	// existed for, and it is untouched.
	ZENITH_ASSERT_NULL(xWall.TryGetComponent<Zenith_AnimatorComponent>(),
		"an unserved entity acquired an animator");

	// The body is still scale-derived: 16 x 6 x 40 -> half extents 8 x 3 x 20. An
	// explicit human box would have shrunk it to 0.4 x 0.9 x 0.4.
	Zenith_Maths::Vector3 xHalf(0.0f), xOffset(0.0f);
	xCollider.ComputeBoxDimensionsAndOffset(
		Zenith_Maths::Vector3(16.0f, 6.0f, 40.0f), xHalf, xOffset, false);
	ZENITH_ASSERT_TRUE(std::abs(xHalf.y - 3.0f) < 1.0e-4f,
		"an unserved entity's body is no longer scale-derived (half height %.4f) -- "
		"the human body contract was installed on something that is not a human",
		xHalf.y);

	// Re-running must stay a no-op, never accumulate.
	ZENITH_ASSERT_TRUE(HVRestartVisual(xWall), "restart failed");
	const Zenith_ModelComponent* pxAfter = xWall.TryGetComponent<Zenith_ModelComponent>();
	if (pxAfter != nullptr)
	{
		ZENITH_ASSERT_EQ(pxAfter->GetNumMeshes(), 0u,
			"a second OnStart built geometry on an entity this component does not serve");
	}
}

// ############################################################################
// 3. The cold-start fallback ships the CONTRACT block
// ############################################################################

// With no human bake reachable, a resolved NPC gets the proportioned palette
// block the game used to ship -- and, critically, the SAME gameplay dimensions it
// would have had with a model. The picture degrades; the body does not.
ZENITH_TEST(ZM_HumanVisual, HumanVisual_ColdFallbackShipsTheContractBlock)
{
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "no active scene to build the fixture in");
	if (pxSceneData == nullptr) { return; }

	// FORCED COLD through the policy seam. Renaming or deleting the shared baked
	// files would race every other test in the batch and corrupt a developer tree.
	const ZM_ScopedHumanAssetPolicy xColdPolicy(ZM_MakeForcedColdHumanAssetPolicy());
	ZENITH_ASSERT_FALSE(ZM_AreHumanAssetsReady(),
		"the forced-cold policy did not take -- this case would silently test the "
		"WARM path instead");

	Zenith_Entity xNpc = g_xEngine.Scenes().CreateEntity(pxSceneData, "HV_ColdNpc");
	Zenith_TransformComponent& xTransform = xNpc.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(620.0f, 400.0f, 400.0f));
	// The AUTHORED scale: uniform, and nothing to do with how big a person is.
	xTransform.SetScale(Zenith_Maths::Vector3(fZM_HUMAN_VISUAL_SCALE));

	Zenith_ColliderComponent& xCollider = xNpc.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

	ZM_Interactable& xInteractable = xNpc.AddComponent<ZM_Interactable>();
	ZENITH_ASSERT_TRUE(xInteractable.SetNpcId(ZM_NPC_RIVAL_VESPER),
		"the fixture NPC row did not take");

	ZENITH_ASSERT_TRUE(HVStartVisual(xNpc), "the visual meta is missing");

	// THE PICTURE: the palette block, not a model.
	const Zenith_MaterialAsset* pxMaterial = HVFirstMaterial(xNpc);
	ZENITH_ASSERT_NOT_NULL(pxMaterial, "a cold NPC must draw exactly one mesh");
	if (pxMaterial != nullptr)
	{
		const Zenith_Maths::Vector4 xExpected =
			ZM_GetHumanPaletteColour(ZM_GetNpcData(ZM_NPC_RIVAL_VESPER).m_eHuman);
		const Zenith_Maths::Vector4 xLive = pxMaterial->GetBaseColor();
		ZENITH_ASSERT_TRUE(
			ZM_HumanPaletteSeparation(xLive, xExpected) < 1.0e-4f,
			"the cold fallback is wearing (%.4f, %.4f, %.4f) but its row resolves to "
			"(%.4f, %.4f, %.4f)", xLive.x, xLive.y, xLive.z,
			xExpected.x, xExpected.y, xExpected.z);
		ZENITH_ASSERT_TRUE(
			ZM_HumanPaletteSeparation(xLive, ZM_GetHumanPaletteFallbackColour())
				>= fZM_HUMAN_PALETTE_MIN_SEPARATION,
			"the cold fallback reads as the blockout grey -- an NPC and a doorframe "
			"would look identical again");
	}

	// THE DRAWN SIZE: after the uniform authored scale, exactly the body box the
	// game has always shipped. This is what makes the cold path a degraded PICTURE
	// rather than a different-sized person.
	float fHeight = 0.0f;
	float fWidth = 0.0f;
	ZENITH_ASSERT_TRUE(HVMeasureDrawnHeight(xNpc, fHeight, fWidth),
		"the cold fallback's geometry could not be measured");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fZM_HUMAN_BODY_HEIGHT, 1.0e-4f,
		"the cold fallback draws %.4f m tall; the body contract is %.4f m",
		fHeight, fZM_HUMAN_BODY_HEIGHT);
	ZENITH_ASSERT_EQ_FLOAT(fWidth, fZM_HUMAN_BODY_FOOTPRINT, 1.0e-4f,
		"the cold fallback draws %.4f m wide; the body contract is %.4f m",
		fWidth, fZM_HUMAN_BODY_FOOTPRINT);

	// THE BODY: the compiled contract, installed explicitly. A scale-derived box at
	// this uniform scale would have been 0.35 m on a side.
	Zenith_Maths::Vector3 xHalf(0.0f), xOffset(0.0f);
	xCollider.ComputeBoxDimensionsAndOffset(
		Zenith_Maths::Vector3(fZM_HUMAN_VISUAL_SCALE), xHalf, xOffset, false);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.y, fZM_HUMAN_BODY_HALF_HEIGHT, 1.0e-5f,
		"a cold NPC's body is %.4f half-tall, not the contract's %.4f -- the cold "
		"path changed a GAMEPLAY dimension, which it must never do",
		xHalf.y, fZM_HUMAN_BODY_HALF_HEIGHT);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.x, fZM_HUMAN_BODY_FOOTPRINT * 0.5f, 1.0e-5f,
		"a cold NPC's body footprint is not the contract's");
	ZENITH_ASSERT_TRUE(std::abs(xOffset.y) < 1.0e-6f,
		"an explicit human box must sit ON the entity origin");

	// No animator: there is no skeleton to drive.
	ZENITH_ASSERT_NULL(xNpc.TryGetComponent<Zenith_AnimatorComponent>(),
		"a cold fallback block acquired an animator");
}

// ############################################################################
// 4. A second OnStart appends nothing
// ############################################################################

// The state machine's "desired == current -> do nothing" arm. Without it a
// re-started NPC would draw its block twice, and a re-started HUMAN would leave
// its animator bound to a destroyed skeleton instance.
ZENITH_TEST(ZM_HumanVisual, HumanVisual_ColdStartIsIdempotent)
{
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "no active scene to build the fixture in");
	if (pxSceneData == nullptr) { return; }

	const ZM_ScopedHumanAssetPolicy xColdPolicy(ZM_MakeForcedColdHumanAssetPolicy());

	Zenith_Entity xNpc = g_xEngine.Scenes().CreateEntity(pxSceneData, "HV_ColdRestart");
	xNpc.GetComponent<Zenith_TransformComponent>().SetPosition(
		Zenith_Maths::Vector3(640.0f, 400.0f, 400.0f));
	xNpc.GetComponent<Zenith_TransformComponent>().SetScale(
		Zenith_Maths::Vector3(fZM_HUMAN_VISUAL_SCALE));
	ZM_Interactable& xInteractable = xNpc.AddComponent<ZM_Interactable>();
	ZENITH_ASSERT_TRUE(xInteractable.SetNpcId(ZM_NPC_VILLAGER), "row did not take");

	ZENITH_ASSERT_TRUE(HVStartVisual(xNpc), "the visual meta is missing");
	const Zenith_ModelComponent* pxModel = xNpc.TryGetComponent<Zenith_ModelComponent>();
	ZENITH_ASSERT_NOT_NULL(pxModel, "the cold fallback built no model");
	if (pxModel == nullptr) { return; }
	ZENITH_ASSERT_EQ(pxModel->GetNumMeshes(), 1u, "the cold fallback is not one mesh");

	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_TRUE(HVRestartVisual(xNpc), "restart failed");
	}
	ZENITH_ASSERT_EQ(pxModel->GetNumMeshes(), 1u,
		"three extra OnStarts left %u meshes on one NPC -- AddMeshEntry APPENDS, so "
		"the 'desired == current' arm is not holding", pxModel->GetNumMeshes());
}

// ############################################################################
// 5. The asset policy restores the production default, latch and all
// ############################################################################

// The bake-attempt latch lives IN the policy, not in a file static, precisely so
// a forced-cold override cannot consume the production policy's one attempt. If
// it could, every test after the first forced-cold one would run against a tree
// that could never bake, and would quietly assert about fallback blocks.
ZENITH_TEST(ZM_HumanVisual, HumanVisual_AssetPolicyRestoresTheProductionDefault)
{
	// Whatever the batch has done so far, start from a known default.
	ZM_ResetHumanAssetPolicy();
	const ZM_HumanAssetPolicy xDefaultBefore = ZM_GetHumanAssetPolicy();
	ZENITH_ASSERT_NOT_NULL(xDefaultBefore.m_pfnIsWarm,
		"the production policy has no warmth query");
	ZENITH_ASSERT_NOT_NULL(xDefaultBefore.m_pfnTryBake,
		"the production policy has no bake");
	ZENITH_ASSERT_FALSE(xDefaultBefore.m_bBakeAttempted,
		"a freshly reset policy must not already have spent its bake attempt");

	{
		const ZM_ScopedHumanAssetPolicy xCold(ZM_MakeForcedColdHumanAssetPolicy());
		ZENITH_ASSERT_FALSE(ZM_AreHumanAssetsReady(),
			"the forced-cold policy did not take");
		// Ask twice: the override must spend ITS OWN latch, not the default's.
		ZENITH_ASSERT_FALSE(ZM_AreHumanAssetsReady(), "forced cold is not stable");
		ZENITH_ASSERT_TRUE(ZM_GetHumanAssetPolicy().m_bBakeAttempted,
			"the override did not latch its own bake attempt");
	}

	const ZM_HumanAssetPolicy xDefaultAfter = ZM_GetHumanAssetPolicy();
	ZENITH_ASSERT_TRUE(xDefaultAfter.m_pfnIsWarm == xDefaultBefore.m_pfnIsWarm
		&& xDefaultAfter.m_pfnTryBake == xDefaultBefore.m_pfnTryBake,
		"leaving the scope did not restore the PRODUCTION policy");
	ZENITH_ASSERT_FALSE(xDefaultAfter.m_bBakeAttempted,
		"the restored production policy came back with a SPENT bake latch -- a "
		"forced-cold test would have permanently disabled the lazy bake");

	// ...and the two answers are independent: the production policy's warmth is a
	// live manifest query, never a cached "cold" left over from the override.
	const bool bWarmNow = ZM_GetHumanAssetPolicy().m_pfnIsWarm();
	ZENITH_ASSERT_EQ(bWarmNow ? 1 : 0,
		ZM_GetHumanAssetPolicy().m_pfnIsWarm() ? 1 : 0,
		"the warmth query is not stable within a single tick");
}

// ############################################################################
// 6. A repeated OnStart RE-BINDS the rig without wiping or duplicating it
// ############################################################################

// ★ THIS IS THE CLAUSE THE WHOLE STATE MACHINE EXISTS FOR, and the only one that
// reaches the WARM branch. `Zenith_ModelComponent::LoadModel` replaces the
// skeleton INSTANCE, while `Zenith_AnimatorComponent::TryDiscoverSkeleton` returns
// immediately once the controller is initialised -- so a second start would leave
// the controller pointing at a destroyed instance unless the visual re-binds
// explicitly. The visual's answer is `xController.Initialize(&xSkeleton)` on every
// start, guarded by `m_bAnimatorAuthored` so the clips/layer/machine are authored
// exactly once. That rests on a claim nothing checked until now: that Initialize
// re-initialises the EXISTING layers rather than dropping them.
//
// Both failure modes are covered, and they pull in opposite directions -- a wipe
// leaves the states null, a duplicate grows the layer count. A fix for one that
// caused the other cannot pass this.
//
// ★ IT NEVER PASSES VACUOUSLY. The branch taken is recorded from the same
// `ZM_AreHumanAssetsReady()` the runtime asked, and the cold branch asserts the
// fallback's own invariants rather than skipping.
ZENITH_TEST(ZM_HumanVisual, HumanVisual_RestartPreservesTheAnimatorRig)
{
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	ZENITH_ASSERT_NOT_NULL(pxSceneData, "no active scene to build the fixture in");
	if (pxSceneData == nullptr) { return; }

	// THE PRODUCTION POLICY, explicitly re-installed: a leaked forced-cold override
	// from an earlier case would otherwise silently send this down the cold branch
	// and the warm clauses would never run.
	ZM_ResetHumanAssetPolicy();
	const bool bWarm = ZM_AreHumanAssetsReady();

	Zenith_Entity xNpc = g_xEngine.Scenes().CreateEntity(pxSceneData, "HV_RigRestart");
	Zenith_TransformComponent& xTransform = xNpc.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(Zenith_Maths::Vector3(660.0f, 400.0f, 400.0f));
	xTransform.SetScale(Zenith_Maths::Vector3(fZM_HUMAN_VISUAL_SCALE));

	Zenith_ColliderComponent& xCollider = xNpc.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

	ZM_Interactable& xInteractable = xNpc.AddComponent<ZM_Interactable>();
	ZENITH_ASSERT_TRUE(xInteractable.SetNpcId(ZM_NPC_RIVAL_VESPER),
		"the fixture NPC row did not take");

	ZENITH_ASSERT_TRUE(HVStartVisual(xNpc), "the visual meta is missing");

	const Zenith_ModelComponent* pxModel = xNpc.TryGetComponent<Zenith_ModelComponent>();
	ZENITH_ASSERT_NOT_NULL(pxModel, "the visual built no model at all");
	if (pxModel == nullptr) { return; }
	const u_int uMeshesAfterFirstStart = pxModel->GetNumMeshes();
	ZENITH_ASSERT_GT(uMeshesAfterFirstStart, 0u, "the visual drew nothing");

	// ---- The GAMEPLAY invariant, asserted on BOTH branches -------------------
	// Whichever picture loaded, the body is the compiled contract. This is the one
	// clause that must never depend on what is on disk.
	Zenith_Maths::Vector3 xHalf(0.0f), xOffset(0.0f);
	xCollider.ComputeBoxDimensionsAndOffset(
		Zenith_Maths::Vector3(fZM_HUMAN_VISUAL_SCALE), xHalf, xOffset, false);
	ZENITH_ASSERT_EQ_FLOAT(xHalf.y, fZM_HUMAN_BODY_HALF_HEIGHT, 1.0e-5f,
		"the body is %.4f half-tall on the %s branch; the contract is %.4f -- the "
		"body must not depend on whether the bake exists",
		xHalf.y, bWarm ? "WARM" : "COLD", fZM_HUMAN_BODY_HALF_HEIGHT);

	if (!bWarm)
	{
		// The cold branch, asserted rather than skipped. There is no rig to preserve
		// because there is no skeleton to drive.
		ZENITH_ASSERT_NULL(xNpc.TryGetComponent<Zenith_AnimatorComponent>(),
			"a cold fallback block acquired an animator");
		ZENITH_ASSERT_TRUE(pxModel->GetModelPath().empty(),
			"a cold fallback loaded a model asset ('%s')", pxModel->GetModelPath().c_str());
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[HumanVisual] cold tree: the WARM rig-rebind clauses did not run. Bake the "
			"humans family (a tools boot) to exercise them.");
		return;
	}

	// ---- The WARM branch -----------------------------------------------------
	ZENITH_ASSERT_FALSE(pxModel->GetModelPath().empty(),
		"the tree reports warm but the visual loaded no model asset");

	Zenith_AnimatorComponent* pxAnimator = xNpc.TryGetComponent<Zenith_AnimatorComponent>();
	ZENITH_ASSERT_NOT_NULL(pxAnimator, "a warm human must carry an animator");
	if (pxAnimator == nullptr) { return; }

	const Flux_AnimationController& xController = pxAnimator->GetController();
	ZENITH_ASSERT_TRUE(xController.IsInitialized(),
		"the controller was never bound to a skeleton instance");
	const uint32_t uLayersAfterFirstStart = xController.GetLayerCount();
	ZENITH_ASSERT_EQ(uLayersAfterFirstStart, 1u,
		"the visual authors exactly ONE layer (got %u)", uLayersAfterFirstStart);

	// The rig is authored: a machine with both locomotion states, and both shared
	// clips actually in the collection.
	const Flux_AnimationLayer* pxLayer = xController.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxLayer, "layer 0 is missing after a successful start");
	if (pxLayer == nullptr) { return; }
	const Flux_AnimationStateMachine* pxMachine = pxLayer->GetStateMachinePtr();
	ZENITH_ASSERT_NOT_NULL(pxMachine, "the locomotion state machine was never created");
	if (pxMachine == nullptr) { return; }
	ZENITH_ASSERT_NOT_NULL(pxMachine->GetState("Idle"), "the Idle state is missing");
	ZENITH_ASSERT_NOT_NULL(pxMachine->GetState("Walk"), "the Walk state is missing");
	ZENITH_ASSERT_NOT_NULL(
		xController.GetClipCollection().GetClip(ZM_HumanClipName(ZM_HUMAN_CLIP_IDLE)),
		"the shared Idle clip is not in the collection");
	ZENITH_ASSERT_NOT_NULL(
		xController.GetClipCollection().GetClip(ZM_HumanClipName(ZM_HUMAN_CLIP_WALK)),
		"the shared Walk clip is not in the collection");

	// ---- ...and it all survives repeated starts ------------------------------
	for (u_int u = 0u; u < 3u; ++u)
	{
		ZENITH_ASSERT_TRUE(HVRestartVisual(xNpc), "restart %u failed", u);
	}

	ZENITH_ASSERT_EQ(pxModel->GetNumMeshes(), uMeshesAfterFirstStart,
		"three extra OnStarts changed the mesh count %u -> %u -- either a block was "
		"stacked onto the model or the model was reloaded and lost entries",
		uMeshesAfterFirstStart, pxModel->GetNumMeshes());
	ZENITH_ASSERT_TRUE(xController.IsInitialized(),
		"a repeated OnStart left the controller UNBOUND");
	ZENITH_ASSERT_EQ(xController.GetLayerCount(), uLayersAfterFirstStart,
		"a repeated OnStart DUPLICATED the layer (%u -> %u) -- the "
		"m_bAnimatorAuthored guard is not holding",
		uLayersAfterFirstStart, xController.GetLayerCount());

	// The re-bind must not have DROPPED the rig either -- the opposite failure to
	// the duplication clause above, and the specific claim Initialize() rests on.
	const Flux_AnimationLayer* pxLayerAfter = xController.GetLayer(0u);
	ZENITH_ASSERT_NOT_NULL(pxLayerAfter, "the re-bind dropped layer 0");
	if (pxLayerAfter == nullptr) { return; }
	const Flux_AnimationStateMachine* pxMachineAfter = pxLayerAfter->GetStateMachinePtr();
	ZENITH_ASSERT_NOT_NULL(pxMachineAfter,
		"the re-bind dropped the state machine -- Initialize() does NOT preserve "
		"existing layers, which the visual's rebind depends on");
	if (pxMachineAfter == nullptr) { return; }
	ZENITH_ASSERT_NOT_NULL(pxMachineAfter->GetState("Idle"),
		"the re-bind dropped the Idle state");
	ZENITH_ASSERT_NOT_NULL(pxMachineAfter->GetState("Walk"),
		"the re-bind dropped the Walk state");
	ZENITH_ASSERT_NOT_NULL(
		xController.GetClipCollection().GetClip(ZM_HumanClipName(ZM_HUMAN_CLIP_IDLE)),
		"the re-bind dropped the shared Idle clip");
}
