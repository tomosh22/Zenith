#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// ZM_AutoTests_GroundItemProp (ZM-27 follow-up (a) of ZM-D-201) -- the REAL-SCENE
// guard behind "a world prop can be picked up into the bag".
//
// ★★ WHY THIS FILE HAD TO EXIST AT ALL. ZM-27 shipped the pickup MECHANISM with
// 14 pure units and a full save round trip, and every one of them passed while
// there was no component, no prop and no scene -- because a compiled-constant unit
// cannot tell a working feature from an unreachable one. That is the same lesson
// ZM_IntroBeat_Test recorded: 15 boot units passed while pressing E at the
// professor did nothing. This test loads the COMMITTED Route1.zscen, finds the
// three authored props, and takes one.
//
// ★ WHAT IT PROVES:
//   * the three prop entities are IN the committed scene, at their authored
//     centres, each carrying a ZM_GroundItemProp configured with the id its
//     placement row names;
//   * a pickup through the LIVE component puts the LIVE item in the LIVE bag;
//   * the prop then goes inert -- IsCollected() true, IsInteractable() false --
//     so the picker stops offering it;
//   * a second press is a clean no-op that reports ALREADY_COLLECTED and does not
//     move the bag.
//
// ★ WHAT IT DELIBERATELY DOES NOT PROVE, STATED SO NOBODY INFERS IT. It does not
// walk the player up to the prop. Two other checks cover that seam and neither is
// duplicated here:
//   * the GEOMETRY -- that a player on the walked lane is inside interact reach of
//     each prop -- is the boot unit
//     Route1_GroundItemPropsAreReachableFromTheWalkedLane, measured against the
//     recipe's own DirtLane polyline;
//   * the PICKER -- that ZM_InteractionRuntime offers the nearest faced candidate
//     and calls Interact() on it -- is already driven end to end by
//     ZM_NpcTalk_Test, and props enter that identical probe array through the same
//     gather. Nothing about the prop path is picker-specific.
// A walk from the south arrival marker to the nearest prop is ~90 m of physics
// motion; it would be a slow, flaky re-test of ZM_NpcTalk_Test's approach loop for
// no claim this file does not already make.
//
// ★ NOTHING HERE IS TELEPORTED, CREATED OR MOVED. The test reads the committed
// scene and calls one shipped function on one live component. No SetPosition
// appears in this file (project rule: no teleportation for movement, even in
// tests), and no entity is created -- an entity-creating boot would re-author
// different .zscen bytes for every committed scene in the game.
//
// m_bRequiresGraphics = false: nothing here reads a pixel. It needs the scene, the
// component pools and the live save, all of which the Null backend has.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_GameStateManager.h"
#include "Zenithmon/Components/ZM_GroundItemProp.h"
#include "Zenithmon/Source/Data/ZM_ItemData.h"
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"
#include "Zenithmon/Source/World/ZM_GroundItem.h"
#include "Zenithmon/Source/World/ZM_Route1Placement.h"

#include <cstdio>    // snprintf -- the failure detail
#include <cstring>

namespace
{
	// How close the live transform must sit to the authored centre. Not an
	// approximation budget -- the scene stores exactly what the authoring step wrote
	// -- but a float round-trip through the .zscen codec, so it is tight.
	constexpr float fGIP_POSITION_EPSILON = 0.001f;

	// The warp barrier's frame budget. ZM_GameStateManager stalls in
	// WAITING_FOR_SCENE / WAITING_FOR_SPAWN rather than failing loudly (ZM-D-200), so
	// this deadline is what turns a warp to nowhere into a diagnostic.
	constexpr int iGIP_WARP_DEADLINE_FRAMES = 900;

	// The prop this test actually takes. The SOUTH salve, because it is the first in
	// authoring order and the one a player meets first -- so a failure here is the
	// first failure a playthrough would hit.
	constexpr ZM_GROUND_ITEM_ID eGIP_SUBJECT = ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE;

	enum class GIPPhase
	{
		RequestWarp,
		WarpToRoute1,
		Inspect,
		Pickup,
		SecondPress,
		Done
	};

	GIPPhase g_eGIPPhase        = GIPPhase::Done;
	int      g_iGIPPhaseFrames  = 0;
	bool     g_bGIPSkipped      = false;

	bool g_bGIPScenePassed      = false;
	bool g_bGIPInspectPassed    = false;
	bool g_bGIPPickupPassed     = false;
	bool g_bGIPInertPassed      = false;
	bool g_bGIPSecondPassed     = false;

	u_int g_uGIPBagBefore       = 0u;
	u_int g_uGIPBagAfter        = 0u;
	u_int g_uGIPBagAfterSecond  = 0u;

	const char* g_szGIPFailure  = "test did not reach verification";
	char        g_aszGIPDetail[512] = {};

	void FailGIP(const char* szWhy)
	{
		if (g_szGIPFailure == nullptr
			|| strcmp(g_szGIPFailure, "test did not reach verification") == 0)
		{
			g_szGIPFailure = szWhy;
		}
		g_eGIPPhase = GIPPhase::Done;
	}

	// The spawn tag Route 1's SOUTH ARRIVAL marker carries: the compiled
	// Dawnmere -> Route1 edge's tag, resolved by WALKING that row rather than
	// spelled here.
	//
	// ★ THIS IS NOT ZM_GetRoute1SouthGateSpawnTag(). That one is what Route 1's
	// south GATE asks Dawnmere for on the way OUT; arriving ON Route 1 needs the tag
	// Route 1 itself offers. Passing the gate tag makes TryQueueWarp refuse the
	// destination silently -- which is exactly how this test failed its first runs.
	// Reading the table is the opposite of mirroring it: a re-tagged edge moves the
	// authoring and this test together.
	const char* GIPInboundRoute1SpawnTag()
	{
		const ZM_WorldSpec& xDawnmere = ZM_GetWorldSpec(ZM_SCENE_DAWNMERE);
		for (u_int u = 0u; u < xDawnmere.m_uConnectionCount; ++u)
		{
			if (xDawnmere.m_pxConnections[u].m_eTarget == ZM_SCENE_ROUTE1)
			{
				return xDawnmere.m_pxConnections[u].m_szSpawnTag;
			}
		}
		return nullptr;
	}

	Zenith_Entity GIPFindEntity(const char* szName)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
		return pxData != nullptr ? pxData->FindEntityByName(szName) : Zenith_Entity();
	}

	// The three authored props, paired with the id each must carry and the accessor
	// that says where it stands. Read from the placement header rather than spelled,
	// so a moved anchor moves this test with it.
	struct GIPExpectedProp
	{
		const char*       m_szEntityName;
		ZM_GROUND_ITEM_ID m_eId;
		ZM_Route1Volume   (*m_pfnVolume)();
	};

	const GIPExpectedProp axGIP_EXPECTED[] =
	{
		{ szZM_ROUTE1_PROP_SOUTH_SALVE_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_SOUTH_SALVE,   &ZM_GetRoute1SouthSalveProp },
		{ szZM_ROUTE1_PROP_LANE_CATCHORB_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_LANE_CATCHORB, &ZM_GetRoute1LaneCatchorbProp },
		{ szZM_ROUTE1_PROP_NORTH_SALVE_ENTITY_NAME,
		  ZM_GROUND_ITEM_ROUTE1_NORTH_SALVE,   &ZM_GetRoute1NorthSalveProp },
	};

	constexpr u_int uGIP_EXPECTED_COUNT =
		sizeof(axGIP_EXPECTED) / sizeof(axGIP_EXPECTED[0]);

	// The live component for a prop id, resolved through the scene every time rather
	// than cached: ECS pools RELOCATE their elements, so a component pointer held
	// across a frame is a dangling pointer waiting to happen.
	ZM_GroundItemProp* GIPResolveProp(ZM_GROUND_ITEM_ID eId)
	{
		for (u_int u = 0u; u < uGIP_EXPECTED_COUNT; ++u)
		{
			if (axGIP_EXPECTED[u].m_eId != eId)
			{
				continue;
			}
			Zenith_Entity xEntity = GIPFindEntity(axGIP_EXPECTED[u].m_szEntityName);
			return xEntity.IsValid()
				? xEntity.TryGetComponent<ZM_GroundItemProp>()
				: nullptr;
		}
		return nullptr;
	}

	void Setup_GroundItemProp()
	{
		g_eGIPPhase       = GIPPhase::RequestWarp;
		g_iGIPPhaseFrames = 0;
		g_bGIPSkipped     = false;

		g_bGIPScenePassed   = false;
		g_bGIPInspectPassed = false;
		g_bGIPPickupPassed  = false;
		g_bGIPInertPassed   = false;
		g_bGIPSecondPassed  = false;

		g_uGIPBagBefore      = 0u;
		g_uGIPBagAfter       = 0u;
		g_uGIPBagAfterSecond = 0u;

		g_szGIPFailure     = "test did not reach verification";
		g_aszGIPDetail[0]  = '\0';

		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		// ★ THE WARP IS NOT ISSUED HERE. Setup runs BEFORE the first frame, so the
		// FrontEnd traversal authority does not exist yet and RequestWarp is refused
		// -- which is what this test did on its first run. The request belongs in a
		// Step, retried until the manager has bootstrapped and settled.
	}

	bool Step_GroundItemProp(int)
	{
		++g_iGIPPhaseFrames;

		switch (g_eGIPPhase)
		{
		case GIPPhase::RequestWarp:
		{
			// RETRIED, not asserted once. The FrontEnd manager becomes the
			// authoritative singleton some frames into the boot, and a request made
			// before then is legitimately refused -- so a refusal is "not yet" until
			// the deadline, and only then a failure.
			const u_int uRoute1 = ZM_GetWorldSpec(ZM_SCENE_ROUTE1).m_uBuildIndex;
			const char* szInboundTag = GIPInboundRoute1SpawnTag();
			if (szInboundTag == nullptr)
			{
				FailGIP("the compiled Dawnmere row carries no connection targeting "
					"Route 1, so there is no inbound spawn tag to warp to");
				return false;
			}

			if (ZM_GameStateManager::RequestWarp(uRoute1, szInboundTag))
			{
				g_eGIPPhase       = GIPPhase::WarpToRoute1;
				g_iGIPPhaseFrames = 0;
				return true;
			}

			if (g_iGIPPhaseFrames > iGIP_WARP_DEADLINE_FRAMES)
			{
				FailGIP("RequestWarp to Route 1 was still being refused after the "
					"bootstrap deadline, so the test never reached the scene that "
					"holds the props");
				return false;
			}
			return true;   // not yet -- keep asking
		}

		case GIPPhase::WarpToRoute1:
		{
			if (ZM_GameStateManager::IsWarpInProgress())
			{
				if (g_iGIPPhaseFrames > iGIP_WARP_DEADLINE_FRAMES)
				{
					FailGIP("the warp to Route 1 never completed -- the machine is "
						"still WAITING_FOR_SCENE / WAITING_FOR_SPAWN after the "
						"deadline (ZM-D-200: a warp to nowhere stalls rather than "
						"failing)");
					return false;
				}
				return true;   // still in flight -- keep waiting
			}

			// The scene is what the props live in; naming it in the failure makes a
			// warp that landed somewhere else obvious rather than mysterious.
			Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
			Zenith_SceneData* pxData = g_xEngine.Scenes().GetSceneData(xScene);
			if (pxData == nullptr)
			{
				FailGIP("the warp completed but the active scene has no data");
				return false;
			}

			g_bGIPScenePassed = true;
			g_eGIPPhase       = GIPPhase::Inspect;
			g_iGIPPhaseFrames = 0;
			return true;
		}

		case GIPPhase::Inspect:
		{
			// ★ ALL THREE, not just the subject. A scene that authored one prop and
			// dropped two would pass every later phase of this test.
			for (u_int u = 0u; u < uGIP_EXPECTED_COUNT; ++u)
			{
				const GIPExpectedProp& xExpected = axGIP_EXPECTED[u];

				Zenith_Entity xEntity = GIPFindEntity(xExpected.m_szEntityName);
				if (!xEntity.IsValid())
				{
					snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
						"prop entity '%s' is not in the committed Route1.zscen -- the "
						"scene has not been re-authored since the props were added "
						"(windowed Vulkan_*_True tools boot, sceneAuthoring=AUTHOR_DAWNMERE)",
						xExpected.m_szEntityName);
					FailGIP(g_aszGIPDetail);
					return false;
				}

				ZM_GroundItemProp* pxProp =
					xEntity.TryGetComponent<ZM_GroundItemProp>();
				if (pxProp == nullptr)
				{
					snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
						"prop entity '%s' exists but carries NO ZM_GroundItemProp",
						xExpected.m_szEntityName);
					FailGIP(g_aszGIPDetail);
					return false;
				}

				// The CONFIGURED id, which is the whole point of the authoring step.
				// An unconfigured component deserialises to the sentinel and reports
				// itself non-interactable -- a prop nobody can ever take.
				if (pxProp->GetGroundItemId() != xExpected.m_eId)
				{
					snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
						"prop entity '%s' carries ground-item id %u ('%s') but its "
						"placement row names id %u ('%s')",
						xExpected.m_szEntityName,
						(u_int)pxProp->GetGroundItemId(),
						ZM_GroundItemName(pxProp->GetGroundItemId()),
						(u_int)xExpected.m_eId,
						ZM_GroundItemName(xExpected.m_eId));
					FailGIP(g_aszGIPDetail);
					return false;
				}

				// And it stands where the placement header says. This is what catches
				// a scene re-authored from a STALE ground table: the entity is
				// present, configured and completely in the wrong place.
				Zenith_TransformComponent* pxTransform =
					xEntity.TryGetComponent<Zenith_TransformComponent>();
				if (pxTransform == nullptr)
				{
					snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
						"prop entity '%s' has no transform", xExpected.m_szEntityName);
					FailGIP(g_aszGIPDetail);
					return false;
				}

				Zenith_Maths::Vector3 xLive(0.0f);
				pxTransform->GetPosition(xLive);
				const ZM_Route1Volume xAuthored = xExpected.m_pfnVolume();
				const Zenith_Maths::Vector3 xDelta = xLive - xAuthored.m_xCenter;
				const float fError = glm::length(xDelta);
				if (!(fError <= fGIP_POSITION_EPSILON))
				{
					snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
						"prop '%s' stands at (%.4f, %.4f, %.4f) while its placement "
						"accessor says (%.4f, %.4f, %.4f) -- %.4f m apart. The scene "
						"was authored from a different ground table than the one "
						"compiled in",
						xExpected.m_szEntityName,
						(double)xLive.x, (double)xLive.y, (double)xLive.z,
						(double)xAuthored.m_xCenter.x,
						(double)xAuthored.m_xCenter.y,
						(double)xAuthored.m_xCenter.z,
						(double)fError);
					FailGIP(g_aszGIPDetail);
					return false;
				}
			}

			g_bGIPInspectPassed = true;
			g_eGIPPhase         = GIPPhase::Pickup;
			g_iGIPPhaseFrames   = 0;
			return true;
		}

		case GIPPhase::Pickup:
		{
			ZM_GameState* pxState = nullptr;
			if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
			{
				FailGIP("no live ZM_GameState on Route 1, so there is no bag to pick "
					"anything up into");
				return false;
			}

			ZM_GroundItemProp* pxProp = GIPResolveProp(eGIP_SUBJECT);
			if (pxProp == nullptr)
			{
				FailGIP("the subject prop's component vanished between Inspect and "
					"Pickup");
				return false;
			}

			// A prop nobody has taken must offer itself. If this is already false the
			// pickup below would be testing the refusal path by accident.
			if (!pxProp->IsInteractable())
			{
				FailGIP("the subject prop reports itself NON-interactable before "
					"anything touched it -- either the save already records it "
					"collected, or its id failed to configure");
				return false;
			}

			const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eGIP_SUBJECT);
			g_uGIPBagBefore = pxState->m_xBag.GetCount(xRow.m_eItem);

			// ★ THE BEAT. One call to the shipped function the interaction runtime
			// calls, on the live component, against the live save.
			pxProp->Interact();

			g_uGIPBagAfter = pxState->m_xBag.GetCount(xRow.m_eItem);

			if (pxProp->GetLastPickupResult() != ZM_GROUND_ITEM_PICKUP_OK)
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"picking up '%s' answered %s rather than OK",
					ZM_GroundItemName(eGIP_SUBJECT),
					ZM_GroundItemPickupName(pxProp->GetLastPickupResult()));
				FailGIP(g_aszGIPDetail);
				return false;
			}

			// The BAG is the claim, not the return value. A pickup that reported OK
			// while the bag did not move is exactly the "burnt prop" failure the
			// add-first ordering exists to prevent.
			if (g_uGIPBagAfter != g_uGIPBagBefore + xRow.m_uCount)
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"picking up '%s' reported OK but the bag holds %u x '%s' where it "
					"held %u and the row yields %u",
					ZM_GroundItemName(eGIP_SUBJECT), g_uGIPBagAfter,
					ZM_GetItemName(xRow.m_eItem), g_uGIPBagBefore, xRow.m_uCount);
				FailGIP(g_aszGIPDetail);
				return false;
			}
			g_bGIPPickupPassed = true;

			// ...and the prop goes INERT, which is what takes it out of the picker's
			// candidate set on the very next frame.
			if (!pxProp->IsCollected() || pxProp->IsInteractable())
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"after a successful pickup '%s' reports collected=%d "
					"interactable=%d -- it must be collected and inert, or the picker "
					"keeps offering a prop with nothing left to give",
					ZM_GroundItemName(eGIP_SUBJECT),
					(int)pxProp->IsCollected(), (int)pxProp->IsInteractable());
				FailGIP(g_aszGIPDetail);
				return false;
			}
			g_bGIPInertPassed = true;

			g_eGIPPhase       = GIPPhase::SecondPress;
			g_iGIPPhaseFrames = 0;
			return true;
		}

		case GIPPhase::SecondPress:
		{
			ZM_GameState* pxState = nullptr;
			if (!ZM_GameStateManager::TryGetGameState(pxState) || pxState == nullptr)
			{
				FailGIP("the game state vanished before the second-press phase");
				return false;
			}

			ZM_GroundItemProp* pxProp = GIPResolveProp(eGIP_SUBJECT);
			if (pxProp == nullptr)
			{
				FailGIP("the subject prop's component vanished before the second press");
				return false;
			}

			const ZM_GroundItemInfo& xRow = ZM_GetGroundItemInfo(eGIP_SUBJECT);

			// A second press cannot reach the prop through the picker (it is inert),
			// but a direct call must still be a clean no-op -- the durable half of
			// "a prop yields once, forever".
			pxProp->Interact();
			g_uGIPBagAfterSecond = pxState->m_xBag.GetCount(xRow.m_eItem);

			if (pxProp->GetLastPickupResult()
				!= ZM_GROUND_ITEM_PICKUP_REJECT_ALREADY_COLLECTED)
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"a second pickup of '%s' answered %s rather than ALREADY_COLLECTED",
					ZM_GroundItemName(eGIP_SUBJECT),
					ZM_GroundItemPickupName(pxProp->GetLastPickupResult()));
				FailGIP(g_aszGIPDetail);
				return false;
			}

			if (g_uGIPBagAfterSecond != g_uGIPBagAfter)
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"a second pickup of '%s' moved the bag from %u to %u -- the prop "
					"is yielding more than once",
					ZM_GroundItemName(eGIP_SUBJECT),
					g_uGIPBagAfter, g_uGIPBagAfterSecond);
				FailGIP(g_aszGIPDetail);
				return false;
			}

			// Both presses reached the pickup path, which is what makes the
			// bag-unchanged clause above non-vacuous: a second call that had silently
			// returned early would leave this at 1.
			if (pxProp->GetPickupAttemptCount() != 2u)
			{
				snprintf(g_aszGIPDetail, sizeof(g_aszGIPDetail),
					"the prop recorded %u pickup attempts across two presses -- the "
					"second press did not reach the pickup path, so 'the bag did not "
					"move' proves nothing", pxProp->GetPickupAttemptCount());
				FailGIP(g_aszGIPDetail);
				return false;
			}

			g_bGIPSecondPassed = true;
			g_eGIPPhase        = GIPPhase::Done;
			return true;
		}

		case GIPPhase::Done:
			return false;
		}
		return false;
	}

	bool Verify_GroundItemProp()
	{
		ZM_InteractionRuntime::ResetRuntimeStateForTests();

		if (g_bGIPSkipped)
		{
			return true;
		}

		const bool bPassed = g_bGIPScenePassed
			&& g_bGIPInspectPassed
			&& g_bGIPPickupPassed
			&& g_bGIPInertPassed
			&& g_bGIPSecondPassed;

		if (!bPassed)
		{
			Zenith_Error(LOG_CATEGORY_UNITTEST, "[ZM_GroundItemProp] %s",
				g_szGIPFailure);
			Zenith_Error(LOG_CATEGORY_UNITTEST,
				"[ZM_GroundItemProp] phase flags: scene=%d inspect=%d pickup=%d "
				"inert=%d second=%d | bag %u -> %u -> %u",
				(int)g_bGIPScenePassed, (int)g_bGIPInspectPassed,
				(int)g_bGIPPickupPassed, (int)g_bGIPInertPassed,
				(int)g_bGIPSecondPassed,
				g_uGIPBagBefore, g_uGIPBagAfter, g_uGIPBagAfterSecond);
		}
		return bPassed;
	}
}

static const Zenith_AutomatedTest g_xZMGroundItemPropTest = {
	"ZM_GroundItemProp_Test",
	&Setup_GroundItemProp,
	&Step_GroundItemProp,
	&Verify_GroundItemProp,
	// The warp phase owns a deadline that FAILS with a diagnostic; this cap is only
	// a backstop above it, since every later phase is single-frame.
	/* maxFrames */ 1200,
	false /* m_bRequiresGraphics */,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xZMGroundItemPropTest);

#endif // ZENITH_INPUT_SIMULATOR
