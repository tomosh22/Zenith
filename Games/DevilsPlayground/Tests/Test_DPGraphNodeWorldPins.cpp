#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

// ============================================================================
// DP_GraphNodeWorldPins_Test -- world-backed pin tests which need real Door
// and Forge components plus ItemManager/PlayerController side tables. The pure
// pin cases remain in Test_DPGraphNodePins.cpp.
// ============================================================================

#include "Core/Zenith_AutomatedTest.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

#include "Components/DP_GraphNodes.h"
#include "Components/DPDoor_Component.h"
#include "Components/DPForge_Component.h"
#include "Components/DPItemManager_Component.h"
#include "Components/DPPlayerController_Component.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "Source/DP_Knots.h"
#include "Source/DP_Win.h"
#include "Source/DPResources.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

#include <cstring>

namespace
{
	int g_iChecks = 0;
	int g_iFailures = 0;
	bool g_bRan = false;
	bool g_bRequestedForgeResourceScene = false;

	bool IsPendingDestruction(Zenith_EntityID xID)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_TransformComponent>()) return false;
		bool bPresent = false;
		xEntity.GetSceneData()->Query<Zenith_TransformComponent>().ForEach([&](Zenith_EntityID xFound, Zenith_TransformComponent&) { bPresent |= xFound == xID; });
		return !bPresent;
	}
	void Check(bool bValue, const char* szWhat)
	{
		++g_iChecks;
		if (!bValue)
		{
			++g_iFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphNodeWorldPins] FAILED: %s", szWhat);
		}
	}

	void CheckEqInt(int32_t iActual, int32_t iExpected, const char* szWhat)
	{
		Check(iActual == iExpected, szWhat);
	}

	Zenith_PropertyValue IntValue(int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		return xValue;
	}

	Zenith_PropertyValue EntityValue(Zenith_EntityID xEntity)
	{
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(xEntity.GetPacked());
		return xValue;
	}

	// A local additive scene gives the door a real entity/component without
	// touching the active gameplay scene.  The pair of global managers is
	// created only when BOTH are absent; a half-live singleton state is unsafe
	// to repair from a test because doing so would replace another owner's
	// singleton.
	struct DoorFixture
	{
		Zenith_Scene m_xScene;
		Zenith_SceneData* m_pxScene = nullptr;
		Zenith_EntityID m_xDoor = INVALID_ENTITY_ID;
		Zenith_EntityID m_xForge = INVALID_ENTITY_ID;
		Zenith_EntityID m_xVillager = INVALID_ENTITY_ID;
		Zenith_EntityID m_xOtherVillager = INVALID_ENTITY_ID;
		Zenith_EntityID m_xKey = INVALID_ENTITY_ID;
		bool m_bReady = false;
		bool m_bOwnsScene = false;

		DoorFixture()
		{
			const bool bHasItems = DPItemManager_Component::Instance() != nullptr;
			const bool bHasPlayer = DPPlayerController_Component::Instance() != nullptr;
			if (bHasItems != bHasPlayer) return;

			m_xScene = g_xEngine.Scenes().LoadScene("DPGraphNodeWorldPinsFixture",
				SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			m_pxScene = g_xEngine.Scenes().GetSceneData(m_xScene);
			if (m_pxScene == nullptr) return;
			m_bOwnsScene = true;

			if (!bHasItems)
			{
				Zenith_Entity xManagers = g_xEngine.Scenes().CreateEntity(m_pxScene,
					"DPGraphNodeWorldPinsManagers");
				xManagers.AddComponent<DPItemManager_Component>().OnAwake();
				xManagers.AddComponent<DPPlayerController_Component>().OnAwake();
			}
			if (DPItemManager_Component::Instance() == nullptr
				|| DPPlayerController_Component::Instance() == nullptr)
			{
				return;
			}

			Zenith_Entity xDoor = g_xEngine.Scenes().CreateEntity(m_pxScene,
				"DPGraphNodeWorldPinsDoor");
			m_xDoor = xDoor.GetEntityID();
			Zenith_ColliderComponent& xDoorCollider = xDoor.AddComponent<Zenith_ColliderComponent>();
			xDoorCollider.AddCollider(COLLISION_VOLUME_TYPE_OBB, RIGIDBODY_TYPE_STATIC);
			xDoor.AddComponent<DPDoor_Component>();
			xDoor.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath("game:Graphs/DP_Door.bgraph");

			Zenith_Entity xVillager = g_xEngine.Scenes().CreateEntity(m_pxScene,
				"DPGraphNodeWorldPinsVillager");
			m_xVillager = xVillager.GetEntityID();
			Zenith_Entity xOtherVillager = g_xEngine.Scenes().CreateEntity(m_pxScene,
				"DPGraphNodeWorldPinsOtherVillager");
			m_xOtherVillager = xOtherVillager.GetEntityID();
			Zenith_Entity xKey = g_xEngine.Scenes().CreateEntity(m_pxScene,
				"DPGraphNodeWorldPinsKey");
			m_xKey = xKey.GetEntityID();
			DP_Items::Internal_RegisterItemTag(m_xKey, DP_ItemTag::Key);
			m_bReady = true;
		}

		~DoorFixture()
		{
			if (m_xVillager.IsValid())
			{
				DP_Player::RemoveHeldItem(m_xVillager);
			}
			if (m_xKey.IsValid())
			{
				DP_Items::Internal_UnregisterItemTag(m_xKey);
			}
			if (m_bOwnsScene)
			{
				// Forge outputs own DPItemBase_Component; scene unload invokes its
				// OnDestroy hook, which unregisters their item tags.
				g_xEngine.Scenes().UnloadScene(m_xScene);
			}
		}

		Zenith_Entity DoorEntity() const
		{
			return m_pxScene != nullptr ? m_pxScene->TryGetEntity(m_xDoor) : Zenith_Entity();
		}

		DPForge_Component* CreateForge()
		{
			if (!m_bReady || m_pxScene == nullptr) return nullptr;
			Zenith_Entity xForge = g_xEngine.Scenes().CreateEntity(m_pxScene,
				"DPGraphNodeWorldPinsForge");
			m_xForge = xForge.GetEntityID();
			return &xForge.AddComponent<DPForge_Component>();
		}

		Zenith_EntityID CreateRegisteredItem(const char* szName, DP_ItemTag eTag)
		{
			if (!m_bReady || m_pxScene == nullptr) return INVALID_ENTITY_ID;
			Zenith_Entity xItem = g_xEngine.Scenes().CreateEntity(m_pxScene, szName);
			const Zenith_EntityID xId = xItem.GetEntityID();
			DP_Items::Internal_RegisterItemTag(xId, eTag);
			return xId;
		}
	};

	void CheckDoorAdvanceAnim()
	{
		DoorFixture xFixture;
		Check(xFixture.m_bReady, "Door world fixture has a real door and both manager singletons");
		if (!xFixture.m_bReady) return;

		Zenith_GraphBlackboard xBlackboard;
		xBlackboard.SetValue("anim", IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed)));
		Zenith_PropertyValue xNearOpen;
		xNearOpen.SetFloat(0.95f);
		xBlackboard.SetValue("openT", xNearOpen);

		Zenith_GraphContext xContext;
		xContext.m_xSelf = xFixture.DoorEntity();
		xContext.m_pxBlackboard = &xBlackboard;
		xContext.m_fDt = DP_Tuning::Get<float>("interactables.door_open_duration_s");
		Check(xContext.m_fDt > 0.0f, "DoorAdvanceAnim uses a positive live duration");

		DPNode_DoorAdvanceAnim xNode;
		xNode.SetInputForTest(DPNode_DoorAdvanceAnim::uPIN_Anim,
			IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening)));
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"DoorAdvanceAnim succeeds for the real door with an Opening pin override");
		const Zenith_PropertyValue* pxOpenT = xBlackboard.TryGetValue("openT");
		Check(pxOpenT && pxOpenT->GetType() == PROPERTY_TYPE_FLOAT && pxOpenT->GetFloat() == 1.0f,
			"DoorAdvanceAnim advances the real door openT to one");
		const Zenith_PropertyValue* pxSettled = xNode.GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
		Check(pxSettled && pxSettled->GetType() == PROPERTY_TYPE_INT32,
			"DoorAdvanceAnim publishes a typed SettledAnim slot");
		if (pxSettled && pxSettled->GetType() == PROPERTY_TYPE_INT32)
		{
			CheckEqInt(pxSettled->GetInt32(), static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
				"DoorAdvanceAnim slot settles Opening to Open");
		}
		Check(xBlackboard.GetInt32("anim", -1) == static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed),
			"DoorAdvanceAnim override wins over the distinct Closed Anim blackboard value");
		Check(xNode.GetMismatchWarningCountForTest(DPNode_DoorAdvanceAnim::uPIN_Anim) == 0u,
			"DoorAdvanceAnim Opening override emits no Anim mismatch");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorAdvanceAnim Opening override has no bad pin access");

		xNode.SetInputForTest(DPNode_DoorAdvanceAnim::uPIN_Anim,
			IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed)));
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"DoorAdvanceAnim non-settling state still succeeds");
		pxSettled = xNode.GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
		Check(pxSettled && pxSettled->GetType() == PROPERTY_TYPE_INT32
			&& pxSettled->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
			"DoorAdvanceAnim non-settling success retains its prior settled output");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorAdvanceAnim non-settling success has no bad pin access");

		xContext.m_xSelf = Zenith_Entity();
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE,
			"DoorAdvanceAnim missing component fails after a settled success");
		pxSettled = xNode.GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
		Check(pxSettled && pxSettled->GetType() == PROPERTY_TYPE_INT32
			&& pxSettled->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
			"DoorAdvanceAnim missing-component failure retains its prior settled output");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorAdvanceAnim missing-component failure has no bad pin access");

		xContext.m_xSelf = xFixture.DoorEntity();
		xBlackboard.SetValue("openT", xNearOpen);
		DPNode_DoorAdvanceAnim xUnnamed;
		xUnnamed.SetInputForTest(DPNode_DoorAdvanceAnim::uPIN_Anim,
			IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening)));
		Check(xUnnamed.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"DoorAdvanceAnim succeeds with an empty SettledAnim output name");
		const Zenith_PropertyValue* pxUnnamed = xUnnamed.GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
		Check(pxUnnamed && pxUnnamed->GetType() == PROPERTY_TYPE_INT32
			&& pxUnnamed->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
			"DoorAdvanceAnim empty output name still publishes the settled slot");
		Check(xBlackboard.TryGetValue("") == nullptr,
			"DoorAdvanceAnim empty output name creates no empty blackboard entry");
		Check(xUnnamed.GetBadAccessWarningCountForTest() == 0u,
			"DoorAdvanceAnim empty-name success has no bad pin access");

		DPNode_DoorAdvanceAnim xBare;
		Zenith_GraphContext xBareContext;
		xBareContext.m_pxBlackboard = &xBlackboard;
		Check(xBare.Execute(xBareContext) == GRAPH_NODE_STATUS_FAILURE,
			"bare DoorAdvanceAnim missing component fails before accessing any pin");
		Check(xBare.GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim) == nullptr,
			"bare DoorAdvanceAnim early failure leaves its output slot unbuilt");
		Check(xBare.GetBadAccessWarningCountForTest() == 0u,
			"bare DoorAdvanceAnim early failure has no bad pin access");

		Zenith_GraphDefinition xDefinition;
		const u_int uNodeID = xDefinition.AddNode("DPDoorAdvanceAnim");
		Zenith_BehaviourGraph xGraph;
		const bool bInitialised = uNodeID != 0u && xGraph.InitialiseFromDefinition(xDefinition);
		Check(bInitialised, "fresh DoorAdvanceAnim graph initializes");
		Zenith_GraphNode* pxBase = bInitialised ? xGraph.FindNode(uNodeID) : nullptr;
		DPNode_DoorAdvanceAnim* pxFresh = pxBase && std::strcmp(pxBase->GetTypeName(), "DPDoorAdvanceAnim") == 0
			? static_cast<DPNode_DoorAdvanceAnim*>(pxBase) : nullptr;
		Check(pxFresh != nullptr, "fresh graph exposes DoorAdvanceAnim");
		if (pxFresh != nullptr)
		{
			Zenith_GraphContext xFreshContext;
			xFreshContext.m_pxGraph = &xGraph;
			xFreshContext.m_pxBlackboard = &xGraph.GetBlackboard();
			Check(pxFresh->Execute(xFreshContext) == GRAPH_NODE_STATUS_FAILURE,
				"fresh graph-initialized DoorAdvanceAnim missing component fails");
			const Zenith_PropertyValue* pxFreshOutput =
				pxFresh->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
			Check(pxFreshOutput && pxFreshOutput->GetType() == PROPERTY_TYPE_INT32
				&& pxFreshOutput->GetInt32() == 0,
				"fresh graph-initialized DoorAdvanceAnim failure retains typed INT32 zero");
			Check(pxFresh->GetBadAccessWarningCountForTest() == 0u,
				"fresh graph-initialized DoorAdvanceAnim failure has no bad pin access");
		}
		xGraph.Shutdown();

		// Exercise the authored DP_Door graph, rather than calling the leaf: its
		// branch after Advance is what prevents an intermediate/stable output slot
		// from being written back into persistent anim or re-running the shim sync.
		Zenith_GraphComponent* pxGraphs = xFixture.DoorEntity().TryGetComponent<Zenith_GraphComponent>();
		Zenith_BehaviourGraph* pxDoorGraph = pxGraphs != nullptr && pxGraphs->GetGraphCount() == 1u
			? pxGraphs->GetGraphAt(0) : nullptr;
		Check(pxDoorGraph != nullptr, "Door fixture binds the authored DP_Door graph");
		if (pxDoorGraph != nullptr)
		{
			Check(pxDoorGraph->GetResolutionSkipCountForTest() == 0u,
				"authored DP_Door graph resolves every data edge");
			Zenith_GraphBlackboard& xDoorBB = pxDoorGraph->GetBlackboard();
			Zenith_GraphContext xGraphContext;
			xGraphContext.m_pxGraph = pxDoorGraph;
			xGraphContext.m_pxBlackboard = &xDoorBB;
			xGraphContext.m_xSelf = xFixture.DoorEntity();
			xGraphContext.m_fDt = DP_Tuning::Get<float>("interactables.door_open_duration_s") * 0.1f;
			DPDoor_Component* pxDoor = xFixture.DoorEntity().TryGetComponent<DPDoor_Component>();
			Zenith_ColliderComponent* pxCollider = xFixture.DoorEntity().TryGetComponent<Zenith_ColliderComponent>();
			Check(pxDoor != nullptr && xGraphContext.m_fDt > 0.0f,
				"authored DP_Door boundary fixture has a real door and positive dt");
			Check(pxCollider != nullptr && pxCollider->HasValidBody(),
				"Door fixture has a lockable STATIC OBB body for solidity inspection");
			if (pxDoor != nullptr && pxCollider != nullptr && pxCollider->HasValidBody() && xGraphContext.m_fDt > 0.0f)
			{
				auto IsSensor = [&]()
				{
					JPH::BodyLockRead xLock(g_xEngine.Physics().GetJoltSystem()->GetBodyLockInterface(),
						JPH::BodyID(pxCollider->GetBodyID().m_uID));
					Check(xLock.Succeeded(), "Door collider body lock succeeds for sensor inspection");
					return xLock.Succeeded() && xLock.GetBody().IsSensor();
				};
				DPNode_DoorAdvanceAnim* pxAdvance = nullptr;
				for (u_int uDoorNodeID = 1; uDoorNodeID <= pxDoorGraph->GetNodeCount(); ++uDoorNodeID)
				{
					Zenith_GraphNode* pxNode = pxDoorGraph->FindNode(uDoorNodeID);
					if (pxNode != nullptr && std::strcmp(pxNode->GetTypeName(), "DPDoorAdvanceAnim") == 0)
					{
						pxAdvance = static_cast<DPNode_DoorAdvanceAnim*>(pxNode);
						break;
					}
				}
				Check(pxAdvance != nullptr,
					"authored Advance node is present");
				Zenith_TransformComponent* pxTransform = xFixture.DoorEntity().TryGetComponent<Zenith_TransformComponent>();
				Check(pxTransform != nullptr, "Door fixture supplies the CreateEntity Transform prerequisite for rotation");
				if (pxAdvance == nullptr || pxTransform == nullptr) return;
				Zenith_Maths::Quat xRotationBefore;
				pxTransform->GetRotation(xRotationBefore);
				xDoorBB.SetValue("anim", IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening)));
				Zenith_PropertyValue xQuarter; xQuarter.SetFloat(0.25f);
				xDoorBB.SetValue("openT", xQuarter);
				pxCollider->SetIsSensor(false); // contrary to Opening's settled state
				Zenith_PropertyValue xOpposite; xOpposite.SetInt32(static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed));
				pxAdvance->SetOutput(xGraphContext, DPNode_DoorAdvanceAnim::uPIN_SettledAnim, xOpposite);
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Opening,
					"intermediate Opening retains persistent anim and skips settle writer");
				Check(!IsSensor(), "intermediate Opening leaves contrary collider sensor state unchanged (no sync)");
				const Zenith_PropertyValue* pxIntermediateSlot = pxAdvance->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
				Check(pxIntermediateSlot && pxIntermediateSlot->GetType() == PROPERTY_TYPE_INT32 && pxIntermediateSlot->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed),
					"intermediate Opening retains its seeded opposite SettledAnim slot");
				Zenith_Maths::Quat xRotationIntermediate;
				pxTransform->GetRotation(xRotationIntermediate);
				Check(xRotationIntermediate.x != xRotationBefore.x || xRotationIntermediate.y != xRotationBefore.y
					|| xRotationIntermediate.z != xRotationBefore.z || xRotationIntermediate.w != xRotationBefore.w,
					"intermediate Opening applies rotation while retaining persistent anim");

				Zenith_PropertyValue xOpenBoundary; xOpenBoundary.SetFloat(0.95f);
				xDoorBB.SetValue("openT", xOpenBoundary);
				xGraphContext.m_fDt = DP_Tuning::Get<float>("interactables.door_open_duration_s");
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Open && IsSensor(),
					"Opening boundary writes Open and makes the collider a sensor in the same graph dispatch");
				Check(xDoorBB.GetFloat("openT", -1.0f) == 1.0f, "Opening boundary clamps openT to exactly one");
				Check(!pxDoor->BlocksPath(), "Opening boundary's persisted Open state has the documented unblocked-path meaning");
				pxCollider->SetIsSensor(false);
				pxAdvance->SetOutput(xGraphContext, DPNode_DoorAdvanceAnim::uPIN_SettledAnim, xOpposite);
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Open,
					"stable Open retains persistent anim without a stale settle write");
				Check(!IsSensor(), "stable Open leaves contrary collider sensor state unchanged (no sync)");
				const Zenith_PropertyValue* pxStableOpenSlot = pxAdvance->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
				Check(pxStableOpenSlot && pxStableOpenSlot->GetType() == PROPERTY_TYPE_INT32 && pxStableOpenSlot->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed),
					"stable Open retains its seeded opposite SettledAnim slot");

				Zenith_PropertyValue xClosing; xClosing.SetFloat(0.75f);
				xDoorBB.SetValue("anim", IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Closing)));
				xDoorBB.SetValue("openT", xClosing);
				pxCollider->SetIsSensor(false); // contrary to Closing's intermediate/stable state
				Zenith_PropertyValue xOpenSeed; xOpenSeed.SetInt32(static_cast<int32_t>(DPDoor_Component::DoorAnim::Open));
				pxAdvance->SetOutput(xGraphContext, DPNode_DoorAdvanceAnim::uPIN_SettledAnim, xOpenSeed);
				xGraphContext.m_fDt = DP_Tuning::Get<float>("interactables.door_open_duration_s") * 0.1f;
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Closing && !IsSensor(),
					"intermediate Closing retains persistent anim and leaves contrary sensor state unchanged");
				Check(pxDoor->BlocksPath(), "intermediate Closing has the documented blocked-path meaning");
				const Zenith_PropertyValue* pxClosingSlot = pxAdvance->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
				Check(pxClosingSlot && pxClosingSlot->GetType() == PROPERTY_TYPE_INT32 && pxClosingSlot->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
					"intermediate Closing retains its seeded SettledAnim slot");

				Zenith_PropertyValue xClosedBoundary; xClosedBoundary.SetFloat(0.05f);
				xDoorBB.SetValue("openT", xClosedBoundary);
				xGraphContext.m_fDt = DP_Tuning::Get<float>("interactables.door_open_duration_s");
				pxCollider->SetIsSensor(true);
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Closed && !IsSensor(),
					"Closing boundary writes Closed and restores collider solidity in the same graph dispatch");
				Check(xDoorBB.GetFloat("openT", -1.0f) == 0.0f, "Closing boundary clamps openT to exactly zero");
				Check(pxDoor->BlocksPath(), "Closing boundary's persisted Closed state has the documented blocked-path meaning");
				pxCollider->SetIsSensor(true);
				Zenith_PropertyValue xOpenSlot; xOpenSlot.SetInt32(static_cast<int32_t>(DPDoor_Component::DoorAnim::Open));
				pxAdvance->SetOutput(xGraphContext, DPNode_DoorAdvanceAnim::uPIN_SettledAnim, xOpenSlot);
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(pxDoor->GetAnim() == DPDoor_Component::DoorAnim::Closed,
					"stable Closed retains persistent anim without a stale settle write");
				Check(IsSensor(), "stable Closed leaves contrary collider sensor state unchanged (no sync)");
				const Zenith_PropertyValue* pxStableClosedSlot = pxAdvance->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
				Check(pxStableClosedSlot && pxStableClosedSlot->GetType() == PROPERTY_TYPE_INT32 && pxStableClosedSlot->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Open),
					"stable Closed retains its seeded opposite SettledAnim slot");

				xDoorBB.SetValue("anim", IntValue(static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening)));
				xDoorBB.SetValue("openT", xOpenBoundary);
				xGraphContext.m_xSelf = Zenith_Entity();
				pxCollider->SetIsSensor(true);
				pxAdvance->SetOutput(xGraphContext, DPNode_DoorAdvanceAnim::uPIN_SettledAnim, xOpposite);
				pxDoorGraph->FireEvent(GRAPH_EVENT_ON_UPDATE, xGraphContext);
				Check(xDoorBB.GetInt32("anim", -1) == static_cast<int32_t>(DPDoor_Component::DoorAnim::Opening),
					"missing-door Advance failure skips the setter and preserves seeded anim");
				const Zenith_PropertyValue* pxFailureSlot = pxAdvance->GetOutputForTest(DPNode_DoorAdvanceAnim::uPIN_SettledAnim);
				Check(pxFailureSlot && pxFailureSlot->GetType() == PROPERTY_TYPE_INT32 && pxFailureSlot->GetInt32() == static_cast<int32_t>(DPDoor_Component::DoorAnim::Closed),
					"missing-door Advance failure preserves the seeded prior output slot");
				Check(IsSensor(), "missing-door Advance failure leaves the seeded collider state unchanged");
			}
		}
	}

	void CheckDoorCheckKey()
	{
		DoorFixture xFixture;
		Check(xFixture.m_bReady, "Door key fixture has a real door and both manager singletons");
		if (!xFixture.m_bReady) return;

		Zenith_GraphBlackboard xBlackboard;
		xBlackboard.SetValue("requiredKey", IntValue(static_cast<int32_t>(DP_ItemTag::Key)));
		xBlackboard.SetValue("payload", EntityValue(xFixture.m_xOtherVillager));
		Zenith_GraphContext xContext;
		xContext.m_xSelf = xFixture.DoorEntity();
		xContext.m_pxBlackboard = &xBlackboard;

		DP_Player::SetHeldItem(xFixture.m_xVillager, xFixture.m_xKey);
		DPNode_DoorCheckKey xNode;
		xNode.SetInputForTest(DPNode_DoorCheckKey::uPIN_Villager, EntityValue(xFixture.m_xVillager));
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"DoorCheckKey accepts the registered held Key from the Villager pin override");
		Check(xBlackboard.GetInt32("requiredKey", -1) == static_cast<int32_t>(DP_ItemTag::None),
			"DoorCheckKey writes RequiredKey None after consuming the matching key");
		Check(!DP_Player::GetHeldItemEntity(xFixture.m_xVillager).IsValid(),
			"DoorCheckKey consumes the held key item");
		Check(xNode.GetMismatchWarningCountForTest(DPNode_DoorCheckKey::uPIN_Villager) == 0u,
			"DoorCheckKey entity override emits no Villager mismatch");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorCheckKey success has no bad pin access");

		xBlackboard.SetValue("requiredKey", IntValue(static_cast<int32_t>(DP_ItemTag::Key)));
		xNode.SetInputForTest(DPNode_DoorCheckKey::uPIN_Villager, EntityValue(INVALID_ENTITY_ID));
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE,
			"DoorCheckKey invalid Villager fails before changing RequiredKey");
		Check(xBlackboard.GetInt32("requiredKey", -1) == static_cast<int32_t>(DP_ItemTag::Key),
			"DoorCheckKey invalid Villager leaves nonzero RequiredKey unchanged");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorCheckKey invalid-Villager failure has no bad pin access");

		xNode.SetInputForTest(DPNode_DoorCheckKey::uPIN_Villager, EntityValue(xFixture.m_xVillager));
		xContext.m_xSelf = Zenith_Entity();
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE,
			"DoorCheckKey missing door component fails");
		Check(xBlackboard.GetInt32("requiredKey", -1) == static_cast<int32_t>(DP_ItemTag::Key),
			"DoorCheckKey missing component leaves nonzero RequiredKey unchanged");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u,
			"DoorCheckKey missing-component failure has no bad pin access");
	}

	void CheckForgeCraft()
	{
		DoorFixture xFixture;
		Check(xFixture.m_bReady, "Forge world fixture has both manager singletons");
		if (!xFixture.m_bReady) return;
		DPForge_Component* pxForge = xFixture.CreateForge();
		Check(pxForge != nullptr, "Forge world fixture creates a real DPForge component");
		if (pxForge == nullptr) return;

		Zenith_GraphContext xContext;
		xContext.m_xSelf = xFixture.m_pxScene->TryGetEntity(xFixture.m_xForge);

		// A fully wired leg proves both recipe pins beat their contradictory
		// names, consumes a real registered Key, and equips a real Iron output.
		{
			const Zenith_EntityID xInput = xFixture.CreateRegisteredItem(
				"DPGraphNodeWorldPinsForgeOverrideInput", DP_ItemTag::Key);
			Check(xInput.IsValid(), "Forge override leg creates a registered Key input item");
			if (xInput.IsValid())
			{
				DP_Player::SetHeldItem(xFixture.m_xVillager, xInput);
				Zenith_GraphBlackboard xBlackboard;
				xBlackboard.SetValue("recipeInput", IntValue(static_cast<int32_t>(DP_ItemTag::Iron)));
				xBlackboard.SetValue("recipeOutput", IntValue(static_cast<int32_t>(DP_ItemTag::Key)));
				xBlackboard.SetValue("craftCount", IntValue(4));
				xContext.m_pxBlackboard = &xBlackboard;
				DPNode_ForgeCraft xNode;
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_Villager, EntityValue(xFixture.m_xVillager));
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_RecipeInput,
					IntValue(static_cast<int32_t>(DP_ItemTag::Key)));
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_RecipeOutput,
					IntValue(static_cast<int32_t>(DP_ItemTag::Iron)));
				Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
					"ForgeCraft succeeds from Key to Iron through both recipe pin overrides");
				const Zenith_EntityID xOutput = DP_Player::GetHeldItemEntity(xFixture.m_xVillager);
				Check(xOutput.IsValid() && xOutput != xInput,
					"ForgeCraft consumes its real input and equips a fresh output entity");
				Check(DP_Player::GetHeldItemTag(xFixture.m_xVillager) == DP_ItemTag::Iron,
					"ForgeCraft applies the overridden Iron output tag");
				Check(IsPendingDestruction(xInput),
					"ForgeCraft marks its registered input for deferred destruction");
				Check(xBlackboard.GetInt32("craftCount", -1) == 5,
					"ForgeCraft increments its CraftCount selector in the actual blackboard");
				Check(xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeInput) == 0u
					&& xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeOutput) == 0u,
					"ForgeCraft fully wired override leg has zero recipe mismatches");
				Check(xNode.GetBadAccessWarningCountForTest() == 0u,
					"ForgeCraft override leg has no bad pin access");
				DP_Items::Internal_UnregisterItemTag(xInput);
				DP_Player::RemoveHeldItem(xFixture.m_xVillager);
			}
		}

		// Missing recipe names use the nonzero ordinary const twins, rather than
		// merely observing their accessor defaults.  The real Iron->Key effect
		// makes the two current-constant uses observable and pins their defaults.
		{
			const Zenith_EntityID xInput = xFixture.CreateRegisteredItem(
				"DPGraphNodeWorldPinsForgeMissingInput", DP_ItemTag::Iron);
			Check(xInput.IsValid(), "Forge missing-default leg creates a registered Iron input item");
			if (xInput.IsValid())
			{
				DP_Player::SetHeldItem(xFixture.m_xVillager, xInput);
				Zenith_GraphBlackboard xBlackboard;
				xBlackboard.SetValue("craftCount", IntValue(9));
				xContext.m_pxBlackboard = &xBlackboard;
				DPNode_ForgeCraft xNode;
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_Villager, EntityValue(xFixture.m_xVillager));
				Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
					"ForgeCraft missing recipe bindings execute the real Iron-to-Key default effect");
				const Zenith_EntityID xOutput = DP_Player::GetHeldItemEntity(xFixture.m_xVillager);
				Check(xOutput.IsValid() && xOutput != xInput,
					"ForgeCraft missing recipe bindings equip a fresh Key output entity");
				Check(DP_Player::GetHeldItemTag(xFixture.m_xVillager) == DP_ItemTag::Key,
					"ForgeCraft missing recipe bindings produce Key");
				Check(IsPendingDestruction(xInput),
					"ForgeCraft missing recipe bindings defer input destruction");
				Check(xBlackboard.GetInt32("craftCount", -1) == 10,
					"ForgeCraft missing recipe bindings increment CraftCount");
				Check(xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeInput) == 0u
					&& xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeOutput) == 0u,
					"ForgeCraft missing recipe bindings have no mismatch diagnostics");
				Check(xNode.GetBadAccessWarningCountForTest() == 0u,
					"ForgeCraft missing recipe leg has no bad pin access");
				DP_Items::Internal_UnregisterItemTag(xInput);
				DP_Player::RemoveHeldItem(xFixture.m_xVillager);
			}
		}

		// Blank recipe selectors deliberately select the ordinary const twins;
		// contradictory typed recipe values still present in the blackboard must
		// not replace the Iron->Key defaults, and cannot emit a type mismatch.
		{
			const Zenith_EntityID xInput = xFixture.CreateRegisteredItem(
				"DPGraphNodeWorldPinsForgeConstInput", DP_ItemTag::Iron);
			Check(xInput.IsValid(), "Forge const-default leg creates a registered Iron input item");
			if (xInput.IsValid())
			{
				DP_Player::SetHeldItem(xFixture.m_xVillager, xInput);
				Zenith_GraphBlackboard xBlackboard;
				xBlackboard.SetValue("recipeInput", IntValue(static_cast<int32_t>(DP_ItemTag::Wood)));
				xBlackboard.SetValue("recipeOutput", IntValue(static_cast<int32_t>(DP_ItemTag::Spike)));
				xBlackboard.SetValue("craftCount", IntValue(14));
				xContext.m_pxBlackboard = &xBlackboard;
				DPNode_ForgeCraft xNode;
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_Villager, EntityValue(xFixture.m_xVillager));
				Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
					"ForgeCraft blank recipe selectors execute the real Iron-to-Key const-default effect");
				const Zenith_EntityID xOutput = DP_Player::GetHeldItemEntity(xFixture.m_xVillager);
				Check(xOutput.IsValid() && xOutput != xInput,
					"ForgeCraft blank selectors equip a fresh Key output entity");
				Check(DP_Player::GetHeldItemTag(xFixture.m_xVillager) == DP_ItemTag::Key,
					"ForgeCraft blank recipe selectors ignore contradictory blackboard tags");
				Check(IsPendingDestruction(xInput),
					"ForgeCraft blank recipe selectors defer input destruction");
				Check(xBlackboard.GetInt32("craftCount", -1) == 15,
					"ForgeCraft blank recipe selectors increment CraftCount");
				Check(xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeInput) == 0u
					&& xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeOutput) == 0u,
					"ForgeCraft const-only recipe leg has silent contradictory typed blackboard values");
				Check(xNode.GetBadAccessWarningCountForTest() == 0u,
					"ForgeCraft blank selector leg has no bad pin access");
				DP_Items::Internal_UnregisterItemTag(xInput);
				DP_Player::RemoveHeldItem(xFixture.m_xVillager);
			}
		}

		// Wrongly typed named recipe values follow the same ordinary const twins
		// as missing values, while retaining their nonempty selectors.
		{
			const Zenith_EntityID xInput = xFixture.CreateRegisteredItem(
				"DPGraphNodeWorldPinsForgeWrongTypeInput", DP_ItemTag::Iron);
			Check(xInput.IsValid(), "Forge wrong-type leg creates a registered Iron input item");
			if (xInput.IsValid())
			{
				DP_Player::SetHeldItem(xFixture.m_xVillager, xInput);
				Zenith_GraphBlackboard xBlackboard;
				Zenith_PropertyValue xWrongInput;
				xWrongInput.SetFloat(1.0f);
				Zenith_PropertyValue xWrongOutput;
				xWrongOutput.SetBool(true);
				xBlackboard.SetValue("recipeInput", xWrongInput);
				xBlackboard.SetValue("recipeOutput", xWrongOutput);
				xBlackboard.SetValue("craftCount", IntValue(20));
				xContext.m_pxBlackboard = &xBlackboard;
				DPNode_ForgeCraft xNode;
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_Villager, EntityValue(xFixture.m_xVillager));
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_RecipeInput, xWrongInput);
				xNode.SetInputForTest(DPNode_ForgeCraft::uPIN_RecipeOutput, xWrongOutput);
				Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
					"ForgeCraft wrongly typed recipe values execute the Iron-to-Key defaults");
				const Zenith_EntityID xOutput = DP_Player::GetHeldItemEntity(xFixture.m_xVillager);
				Check(xOutput.IsValid() && xOutput != xInput,
					"ForgeCraft wrongly typed recipe values equip a fresh Key output entity");
				Check(DP_Player::GetHeldItemTag(xFixture.m_xVillager) == DP_ItemTag::Key,
					"ForgeCraft wrongly typed recipe values produce Key");
				Check(IsPendingDestruction(xInput),
					"ForgeCraft wrongly typed recipe values defer input destruction");
				Check(xBlackboard.GetInt32("craftCount", -1) == 21,
					"ForgeCraft wrongly typed recipe values increment CraftCount");
				Check(xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeInput) == 1u
					&& xNode.GetMismatchWarningCountForTest(DPNode_ForgeCraft::uPIN_RecipeOutput) == 1u,
					"ForgeCraft wrongly typed recipe overrides emit one mismatch per accessed pin");
				Check(xNode.GetBadAccessWarningCountForTest() == 0u,
					"ForgeCraft wrongly typed recipe leg has no bad pin access");
				DP_Items::Internal_UnregisterItemTag(xInput);
				DP_Player::RemoveHeldItem(xFixture.m_xVillager);
			}
		}
	}

	void CheckWorldInputProofs()
	{
		DoorFixture xFixture;
		Check(xFixture.m_bReady, "world input proofs have both manager singletons");
		if (!xFixture.m_bReady) return;

		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_xSelf = xFixture.DoorEntity();
		xContext.m_pxBlackboard = &xBlackboard;

		DP_Win::Reset();
		DP_Knots::ResetForNewRun();
		xBlackboard.SetValue("heldObjective", IntValue(static_cast<int32_t>(DP_ItemTag::Objective2)));
		DPNode_WinCheckAlreadyCollected xCheck;
		xCheck.SetInputForTest(DPNode_WinCheckAlreadyCollected::uPIN_Tag,
			IntValue(static_cast<int32_t>(DP_ItemTag::Objective1)));
		Check(xCheck.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"WinCheckAlreadyCollected accepts Objective1 override over Objective2 blackboard");
		const int iObjectivesForVictory = DP_Tuning::Get<int>("night.reagents_required_for_victory");
		Check(iObjectivesForVictory > 1,
			"world input proof requires one objective to remain below victory");
		if (iObjectivesForVictory <= 1) return;
		DP_Win::NotifyObjectiveCollected(DP_ItemTag::Objective1);
		Check(xCheck.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE,
			"WinCheckAlreadyCollected rejects the collected override tag");
		Check(!DP_Win::HasWon() && !DP_Knots::HasBankedThisRun(),
			"one isolated objective remains below victory and does not bank a run");
		Check(xCheck.GetMismatchWarningCountForTest(DPNode_WinCheckAlreadyCollected::uPIN_Tag) == 0u
			&& xCheck.GetBadAccessWarningCountForTest() == 0u,
			"WinCheckAlreadyCollected override legs have no mismatch or bad access");
		DP_Win::Reset();
		DP_Knots::ResetForNewRun();

		xBlackboard.SetValue("payload", EntityValue(xFixture.m_xOtherVillager));
		xBlackboard.SetValue("heldObjective", IntValue(static_cast<int32_t>(DP_ItemTag::Objective2)));
		DPNode_WinNotifyCollected xNotify;
		xNotify.SetInputForTest(DPNode_WinNotifyCollected::uPIN_Villager, EntityValue(xFixture.m_xVillager));
		xNotify.SetInputForTest(DPNode_WinNotifyCollected::uPIN_Tag,
			IntValue(static_cast<int32_t>(DP_ItemTag::Objective1)));
		Check(xNotify.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"WinNotifyCollected uses Villager and Objective1 pin overrides");
		Check((DP_Win::GetCollectedObjectivesMask() & DP_ObjectiveTagToBit(DP_ItemTag::Objective1)) != 0u
			&& (DP_Win::GetCollectedObjectivesMask() & DP_ObjectiveTagToBit(DP_ItemTag::Objective2)) == 0u,
			"WinNotifyCollected sets only the overridden Objective1 bit");
		Check(!DP_Win::HasWon() && !DP_Knots::HasBankedThisRun(),
			"WinNotifyCollected's one objective remains below victory and does not bank");
		Check(xNotify.GetMismatchWarningCountForTest(DPNode_WinNotifyCollected::uPIN_Villager) == 0u
			&& xNotify.GetMismatchWarningCountForTest(DPNode_WinNotifyCollected::uPIN_Tag) == 0u
			&& xNotify.GetBadAccessWarningCountForTest() == 0u,
			"WinNotifyCollected override leg has no mismatch or bad access");
		DP_Win::Reset();
		DP_Knots::ResetForNewRun();

		bool bPlaced = false;
		Zenith_EntityID xPlacedVillager = INVALID_ENTITY_ID;
		int iPlacedBit = -1;
		const Zenith_EventHandle uPlacedHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnObjectivePlaced>(
			[&](const DP_OnObjectivePlaced& xEvent)
			{
				bPlaced = true;
				xPlacedVillager = xEvent.m_xVillager;
				iPlacedBit = xEvent.m_iObjectiveBitIndex;
			});
		xBlackboard.SetValue("payload", EntityValue(xFixture.m_xOtherVillager));
		xBlackboard.SetValue("heldObjective", IntValue(static_cast<int32_t>(DP_ItemTag::Objective1)));
		DPNode_DispatchObjectivePlaced xDispatch;
		xDispatch.SetInputForTest(DPNode_DispatchObjectivePlaced::uPIN_Villager, EntityValue(xFixture.m_xVillager));
		xDispatch.SetInputForTest(DPNode_DispatchObjectivePlaced::uPIN_Tag,
			IntValue(static_cast<int32_t>(DP_ItemTag::Objective4)));
		Check(xDispatch.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"DispatchObjectivePlaced dispatches from Villager and Objective4 overrides");
		Check(bPlaced && xPlacedVillager == xFixture.m_xVillager && iPlacedBit == 3,
			"DispatchObjectivePlaced event carries overridden Villager and Objective4 bit index");
		Check(xDispatch.GetMismatchWarningCountForTest(DPNode_DispatchObjectivePlaced::uPIN_Villager) == 0u
			&& xDispatch.GetMismatchWarningCountForTest(DPNode_DispatchObjectivePlaced::uPIN_Tag) == 0u
			&& xDispatch.GetBadAccessWarningCountForTest() == 0u,
			"DispatchObjectivePlaced override leg has no mismatch or bad access");
		Zenith_EventDispatcher::Get().Unsubscribe(uPlacedHandle);

		Zenith_PropertyValue xOpenT;
		xOpenT.SetFloat(0.0f);
		Zenith_PropertyValue xFalse;
		xFalse.SetBool(false);
		Zenith_PropertyValue xTrue;
		xTrue.SetBool(true);
		xBlackboard.SetValue("isOpen", xFalse);
		xBlackboard.SetValue("openT", xOpenT);
		xContext.m_fDt = DP_Tuning::Get<float>("interactables.double_door_open_duration_s");
		Check(xContext.m_fDt > 0.0f, "AnimateDoorLeaves uses a positive live duration");
		DPNode_AnimateDoorLeaves xAnimate;
		xAnimate.SetInputForTest(DPNode_AnimateDoorLeaves::uPIN_IsOpen, xTrue);
		Check(xAnimate.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"AnimateDoorLeaves true override advances OpenT over false blackboard");
		const float fAdvanced = xBlackboard.GetFloat("openT");
		Check(fAdvanced > 0.0f, "AnimateDoorLeaves true override advances OpenT");
		xAnimate.SetInputForTest(DPNode_AnimateDoorLeaves::uPIN_IsOpen, xFalse);
		xBlackboard.SetValue("isOpen", xTrue);
		Zenith_PropertyValue xQuarterOpen;
		xQuarterOpen.SetFloat(0.25f);
		xBlackboard.SetValue("openT", xQuarterOpen);
		Check(xAnimate.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"AnimateDoorLeaves false override succeeds over true blackboard");
		Check(xBlackboard.GetFloat("openT") == 0.25f,
			"AnimateDoorLeaves false override retains a nonterminal OpenT");
		Check(xAnimate.GetMismatchWarningCountForTest(DPNode_AnimateDoorLeaves::uPIN_IsOpen) == 0u
			&& xAnimate.GetBadAccessWarningCountForTest() == 0u,
			"AnimateDoorLeaves override legs have no mismatch or bad access");
	}

	void Setup_DPGraphNodeWorldPins()
	{
		g_iChecks = 0;
		g_iFailures = 0;
		g_bRan = false;
		g_bRequestedForgeResourceScene = false;
	}

	bool Step_DPGraphNodeWorldPins(int iFrame)
	{
		if (DevilsPlayground::Resources().m_xItemPrefab.GetDirect() == nullptr)
		{
			if (!g_bRequestedForgeResourceScene)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[DPGraphNodeWorldPins] item prefab unavailable; loading ProcLevel before ForgeCraft coverage");
				g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
				g_bRequestedForgeResourceScene = true;
			}
			if (iFrame < 90) return true;
			Check(false, "ForgeCraft resource fixture loaded ProcLevel but item prefab stayed unavailable");
			g_bRan = true;
			return false;
		}
		CheckDoorAdvanceAnim();
		CheckDoorCheckKey();
		CheckForgeCraft();
		CheckWorldInputProofs();
		g_bRan = true;
		return false;
	}

	bool Verify_DPGraphNodeWorldPins()
	{
		Check(g_bRan, "Door and Forge world pin checks ran");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphNodeWorldPins] %d checks, %d failed",
			g_iChecks, g_iFailures);
		return g_iChecks > 0 && g_iFailures == 0;
	}
}

static const Zenith_AutomatedTest g_xDPGraphNodeWorldPinsTest = {
	"DP_GraphNodeWorldPins_Test",
	&Setup_DPGraphNodeWorldPins,
	&Step_DPGraphNodeWorldPins,
	&Verify_DPGraphNodeWorldPins,
	120
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPGraphNodeWorldPinsTest);

#endif // ZENITH_INPUT_SIMULATOR
