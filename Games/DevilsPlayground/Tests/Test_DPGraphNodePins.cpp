#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

#include "Core/Zenith_AutomatedTest.h"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Core/Zenith_AudioBus.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "Input/Zenith_InputSimulator.h"
#include "Components/DP_GraphNodes.h"
#include "Components/DPItemManager_Component.h"
#include "Components/DPPlayerController_Component.h"
#include "Components/Priest_Component.h"
#include "AI/Navigation/Zenith_NavMesh.h"

#include <cmath>

namespace
{
	int g_iChecks = 0;
	int g_iFailures = 0;
	bool g_bRan = false;
	int g_iChestEvents = 0;
	Zenith_EntityID g_xLastChestVillager = INVALID_ENTITY_ID;
	int g_iItemPickedEvents = 0;
	DP_ItemTag g_eLastPickedTag = DP_ItemTag::None;
	const Zenith_GraphBlackboard* g_pxCommitBlackboard = nullptr;
	const DPNode_ItemCommitPickup* g_pxLiveCommit = nullptr;
	bool g_bCommitEventSawClears = false;
	int g_iEvaporatedEvents = 0;
	Zenith_EntityID g_xEvaporatedItem = INVALID_ENTITY_ID;
	DP_ItemTag g_eEvaporatedTag = DP_ItemTag::None;
	Zenith_Maths::Vector3 g_xEvaporatedPosition(0.0f);

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
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphNodePins] FAILED: %s", szWhat);
		}
	}

	void CheckEqU64(uint64_t ulActual, uint64_t ulExpected, const char* szWhat)
	{
		Check(ulActual == ulExpected, szWhat);
	}

	void CheckEqFloat(float fActual, float fExpected, const char* szWhat)
	{
		Check(fActual == fExpected, szWhat);
	}

	void OnChestOpened(const DP_OnChestOpened& xEvent)
	{
		++g_iChestEvents;
		g_xLastChestVillager = xEvent.m_xVillager;
	}

	void OnItemPickedUp(const DP_OnItemPickedUp& xEvent)
	{
		++g_iItemPickedEvents;
		g_eLastPickedTag = xEvent.m_eTag;
		const Zenith_PropertyValue* pxVillager = g_pxCommitBlackboard ? g_pxCommitBlackboard->TryGetValue("channelVillager") : nullptr;
		const Zenith_PropertyValue* pxRemaining = g_pxCommitBlackboard ? g_pxCommitBlackboard->TryGetValue("channelRemaining") : nullptr;
		const Zenith_PropertyValue* pxOwnerSlot = g_pxLiveCommit ? g_pxLiveCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager) : nullptr;
		const Zenith_PropertyValue* pxRemainingSlot = g_pxLiveCommit ? g_pxLiveCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining) : nullptr;
		g_bCommitEventSawClears = pxVillager && pxRemaining && pxOwnerSlot && pxRemainingSlot
			&& pxVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxVillager->GetPackedEntityID() == 0ull
			&& pxRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxRemaining->GetFloat() == 0.0f
			&& pxOwnerSlot->GetType() == PROPERTY_TYPE_ENTITY_ID && pxOwnerSlot->GetPackedEntityID() == 0ull
			&& pxRemainingSlot->GetType() == PROPERTY_TYPE_FLOAT && pxRemainingSlot->GetFloat() == 0.0f;
	}

	void OnItemEvaporated(const DP_OnItemEvaporated& xEvent)
	{
		++g_iEvaporatedEvents;
		g_xEvaporatedItem = xEvent.m_xItem;
		g_eEvaporatedTag = xEvent.m_eTag;
		g_xEvaporatedPosition = xEvent.m_xPosition;
	}

	Zenith_PropertyValue EntityValue(uint64_t ulPacked)
	{
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(ulPacked);
		return xValue;
	}

	Zenith_PropertyValue FloatValue(float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		return xValue;
	}

	Zenith_PropertyValue BoolValue(bool bValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetBool(bValue);
		return xValue;
	}

	Zenith_PropertyValue IntValue(int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		return xValue;
	}

	bool g_bArmOrderProducerSawOwner = false;
	bool g_bArmOrderProducerSawRemainingBeforePublish = false;
	float g_fArmOrderProducerValue = 7.5f;
	float g_fArmOrderExpectedRemainingBeforePublish = 0.0f;
	int g_iArmOrderProducerPulls = 0;
	bool g_bArmOrderProducerForceValue = false;
	const DPNode_ItemArmChannel* g_pxLiveArm = nullptr;
	uint64_t g_ulCommitProducerValue = 0ull;
	int g_iCommitProducerPulls = 0;
	int g_iFinishTagProducerPulls = 0;
	Zenith_EntityID g_xFinishExpectedVillager = INVALID_ENTITY_ID;
	Zenith_EntityID g_xFinishExpectedPickup = INVALID_ENTITY_ID;
	bool g_bFinishTagProducerSawHeld = false;
	class DPGraphFinishTagProducer final : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(DPGraphFinishTagProducer)
		ZENITH_GRAPH_PINS_BEGIN(DPGraphFinishTagProducer)
		ZENITH_GRAPH_PIN_OUTPUT(Value, "", PROPERTY_TYPE_INT32)
		ZENITH_GRAPH_PINS_END
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			++g_iFinishTagProducerPulls;
			g_bFinishTagProducerSawHeld = DP_Player::GetHeldItemEntity(g_xFinishExpectedVillager).GetPacked() == g_xFinishExpectedPickup.GetPacked();
			SetOutput<int32_t>(xContext, 0u, static_cast<int32_t>(DP_ItemTag::BogWater));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DPTestFinishTagProducer"; }
	};
	class DPGraphCommitVillagerProducer final : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(DPGraphCommitVillagerProducer)
		ZENITH_GRAPH_PINS_BEGIN(DPGraphCommitVillagerProducer)
		ZENITH_GRAPH_PIN_OUTPUT(Value, "", PROPERTY_TYPE_ENTITY_ID)
		ZENITH_GRAPH_PINS_END
	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			++g_iCommitProducerPulls;
			Zenith_PropertyValue xValue; xValue.SetPackedEntityID(g_ulCommitProducerValue);
			SetOutput(xContext, 0u, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DPTestCommitVillagerProducer"; }
	};
	class DPGraphArmOrderProducer final : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(DPGraphArmOrderProducer)
		ZENITH_GRAPH_PINS_BEGIN(DPGraphArmOrderProducer)
		ZENITH_GRAPH_PIN_OUTPUT(Value, "", PROPERTY_TYPE_FLOAT)
		ZENITH_GRAPH_PINS_END
	public:
		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			++g_iArmOrderProducerPulls;
			if (g_bArmOrderProducerForceValue)
			{
				SetOutput<float>(xContext, 0u, g_fArmOrderProducerValue);
				return GRAPH_NODE_STATUS_SUCCESS;
			}
			const Zenith_PropertyValue* pxOwner = g_pxLiveArm ? g_pxLiveArm->GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelVillager) : nullptr;
			const Zenith_PropertyValue* pxRemaining = g_pxLiveArm ? g_pxLiveArm->GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining) : nullptr;
			g_bArmOrderProducerSawOwner = pxOwner && pxOwner->GetType() == PROPERTY_TYPE_ENTITY_ID
				&& pxOwner->GetPackedEntityID() == 0x0000000B0000000Bull;
			g_bArmOrderProducerSawRemainingBeforePublish = pxRemaining && pxRemaining->GetType() == PROPERTY_TYPE_FLOAT
				&& pxRemaining->GetFloat() == g_fArmOrderExpectedRemainingBeforePublish;
			SetOutput<float>(xContext, 0u, g_bArmOrderProducerSawOwner && g_bArmOrderProducerSawRemainingBeforePublish ? g_fArmOrderProducerValue : 1.5f);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DPTestArmOrderProducer"; }
	};

	void EnsureDPGraphArmOrderProducerRegistered()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		if (xRegistry.Find("DPTestArmOrderProducer") == nullptr)
			xRegistry.RegisterNodeType<DPGraphArmOrderProducer>("DPTestArmOrderProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}

	void EnsureDPGraphCommitVillagerProducerRegistered()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		if (xRegistry.Find("DPTestCommitVillagerProducer") == nullptr)
			xRegistry.RegisterNodeType<DPGraphCommitVillagerProducer>("DPTestCommitVillagerProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}
	void EnsureDPGraphFinishTagProducerRegistered()
	{
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		if (xRegistry.Find("DPTestFinishTagProducer") == nullptr)
			xRegistry.RegisterNodeType<DPGraphFinishTagProducer>("DPTestFinishTagProducer", GRAPH_EVENT_NONE, 1, false, "Test", false, true);
	}

	struct DPGraphNodeItemFixture
	{
		Zenith_Scene m_xScene;
		Zenith_SceneData* m_pxScene = nullptr;
		bool m_bReady = false;
		bool m_bOwnsScene = false;

		DPGraphNodeItemFixture()
		{
			const bool bHasItemManager = DPItemManager_Component::Instance() != nullptr;
			const bool bHasController = DPPlayerController_Component::Instance() != nullptr;
			// Never overwrite a live scene singleton.  A partial singleton state is
			// not repairable here without clobbering another test's owner.
			if (bHasItemManager != bHasController) return;
			m_xScene = g_xEngine.Scenes().LoadScene("DPGraphNodePinsFixture", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			m_pxScene = g_xEngine.Scenes().GetSceneData(m_xScene);
			if (m_pxScene == nullptr) return;
			m_bOwnsScene = true;
			if (!bHasItemManager)
			{
				Zenith_Entity xManager = g_xEngine.Scenes().CreateEntity(m_pxScene, "DPGraphNodePinsManager");
				xManager.AddComponent<DPItemManager_Component>().OnAwake();
				xManager.AddComponent<DPPlayerController_Component>().OnAwake();
			}
			m_bReady = DPItemManager_Component::Instance() != nullptr && DPPlayerController_Component::Instance() != nullptr;
		}

		~DPGraphNodeItemFixture()
		{
			if (m_bOwnsScene) g_xEngine.Scenes().UnloadScene(m_xScene);
		}

		Zenith_EntityID CreateItem(const char* szName, DP_ItemTag eTag)
		{
			if (!m_bReady) return INVALID_ENTITY_ID;
			Zenith_Entity xItem = g_xEngine.Scenes().CreateEntity(m_pxScene, szName);
			const Zenith_EntityID xId = xItem.GetEntityID();
			DP_Items::Internal_RegisterItemTag(xId, eTag);
			return xId;
		}
	};

	template<typename TNode>
	void CheckHelperCases()
	{
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_pxBlackboard = &xBlackboard;
		constexpr uint64_t ulDistinct = 0x0000000100000042ull;
		auto Clear = [](TNode& xNode) { xNode.m_strVillagerVar = ""; };
		{
			TNode xNode; Clear(xNode);
			Check(!DPGraph_GetEntityInput(xNode, xContext, TNode::uPIN_Villager).IsValid(), "helper real unbound input returns INVALID_ENTITY_ID");
			Check(xNode.GetFallbackUseCountForTest(TNode::uPIN_Villager) == 0u, "helper real unbound input has no named fallback");
		}
		{
			TNode xNode; Clear(xNode); Zenith_PropertyValue xWrong; xWrong.SetFloat(2.0f);
			xNode.SetInputForTest(TNode::uPIN_Villager, xWrong);
			Check(!DPGraph_GetEntityInput(xNode, xContext, TNode::uPIN_Villager).IsValid(), "helper wrong tag returns INVALID_ENTITY_ID");
			Check(xNode.GetFallbackUseCountForTest(TNode::uPIN_Villager) == 0u, "helper wrong-tag override has no fallback");
		}
		{
			TNode xNode; Clear(xNode); xNode.SetInputForTest(TNode::uPIN_Villager, EntityValue(INVALID_ENTITY_ID.GetPacked()));
			Check(!DPGraph_GetEntityInput(xNode, xContext, TNode::uPIN_Villager).IsValid(), "helper explicit INVALID remains INVALID_ENTITY_ID");
			Check(xNode.GetFallbackUseCountForTest(TNode::uPIN_Villager) == 0u, "helper explicit INVALID has no fallback");
		}
		{
			TNode xNode; Clear(xNode); xNode.SetInputForTest(TNode::uPIN_Villager, EntityValue(0ull));
			CheckEqU64(DPGraph_GetEntityInput(xNode, xContext, TNode::uPIN_Villager).GetPacked(), 0ull, "helper preserves a present packed-zero entity value");
			Check(xNode.GetFallbackUseCountForTest(TNode::uPIN_Villager) == 0u, "helper packed-zero override has no fallback");
		}
		{
			TNode xNode; Clear(xNode); xNode.SetInputForTest(TNode::uPIN_Villager, EntityValue(ulDistinct));
			CheckEqU64(DPGraph_GetEntityInput(xNode, xContext, TNode::uPIN_Villager).GetPacked(), ulDistinct, "helper reads the distinct pin override");
			Check(xNode.GetFallbackUseCountForTest(TNode::uPIN_Villager) == 0u, "helper distinct override has no fallback");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "helper uses valid pin addresses");
		}
	}

	void CheckAllHelperBackedVillagerPins()
	{
		CheckHelperCases<DPNode_WinNotifyCollected>();
		CheckHelperCases<DPNode_ConsumeHeldItem>();
		CheckHelperCases<DPNode_DispatchObjectivePlaced>();
		CheckHelperCases<DPNode_ConsumeKeyForUnlock>();
		CheckHelperCases<DPNode_DispatchDoorOpened>();
		CheckHelperCases<DPNode_DispatchDoorClosed>();
		CheckHelperCases<DPNode_DispatchChestOpened>();
		CheckHelperCases<DPNode_DoorCheckKey>();
		CheckHelperCases<DPNode_DoorPentagramDeferral>();
		CheckHelperCases<DPNode_ForgeCraft>();
		CheckHelperCases<DPNode_TryPossess>();
		CheckHelperCases<DPNode_ItemChildRefusal>();
		CheckHelperCases<DPNode_ItemCommitPickup>();
		CheckHelperCases<DPNode_ItemRingBell>();
	}

	void CheckItemArmChannel()
	{
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_pxBlackboard = &xBlackboard;
		Zenith_PropertyValue xBBVillager = EntityValue(0x0000000200000002ull);
		Zenith_PropertyValue xBBDuration;
		xBBDuration.SetFloat(3.0f);
		xBlackboard.SetValue("possessedVillager", xBBVillager);
		xBlackboard.SetValue("channelDuration", xBBDuration);

		DPNode_ItemArmChannel xNode;
		const uint64_t ulOverrideVillager = 0x0000000300000003ull;
		Zenith_PropertyValue xOverrideDuration;
		xOverrideDuration.SetFloat(9.5f);
		xNode.SetInputForTest(DPNode_ItemArmChannel::uPIN_Villager, EntityValue(ulOverrideVillager));
		xNode.SetInputForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration, xOverrideDuration);
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel executes with entity and duration overrides");

		const Zenith_PropertyValue* pxVillagerOut = xNode.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelVillager);
		const Zenith_PropertyValue* pxRemainingOut = xNode.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining);
		Check(pxVillagerOut != nullptr && pxVillagerOut->GetType() == PROPERTY_TYPE_ENTITY_ID, "ItemArmChannel ChannelVillager output is present and ENTITY_ID-tagged");
		Check(pxRemainingOut != nullptr && pxRemainingOut->GetType() == PROPERTY_TYPE_FLOAT, "ItemArmChannel ChannelRemaining output is present and FLOAT-tagged");
		if (pxVillagerOut && pxVillagerOut->GetType() == PROPERTY_TYPE_ENTITY_ID) CheckEqU64(pxVillagerOut->GetPackedEntityID(), ulOverrideVillager, "ItemArmChannel publishes the overridden Villager");
		if (pxRemainingOut && pxRemainingOut->GetType() == PROPERTY_TYPE_FLOAT) CheckEqFloat(pxRemainingOut->GetFloat(), 9.5f, "ItemArmChannel publishes the overridden ChannelDuration");
		Check(xNode.GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_Villager) == 0u, "ItemArmChannel wired Villager has no fallback");
		Check(xNode.GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration) == 0u, "ItemArmChannel wired ChannelDuration has no fallback");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel uses only valid pin addresses");

		// ItemArmChannel deliberately belongs to the other ENTITY_ID family: its
		// packed accessor returns typed zero for missing or wrong-tag input, while
		// the helper-backed nodes above return INVALID_ENTITY_ID.
		{
			Zenith_GraphBlackboard xEmpty;
			Zenith_GraphContext xEmptyContext;
			xEmptyContext.m_pxBlackboard = &xEmpty;
			DPNode_ItemArmChannel xMissing;
			xMissing.m_strVillagerVar = "";
			Zenith_PropertyValue xDuration;
			xDuration.SetFloat(1.0f);
			xMissing.SetOutput(xEmptyContext, DPNode_ItemArmChannel::uPIN_ChannelVillager, EntityValue(0x0000000900000009ull));
			xMissing.SetOutput(xEmptyContext, DPNode_ItemArmChannel::uPIN_ChannelRemaining, FloatValue(4.0f));
			xMissing.SetInputForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration, xDuration);
			Check(xMissing.Execute(xEmptyContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel accepts missing entity input as packed zero");
			Check(xMissing.GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel missing entity uses valid pin addresses");
			const Zenith_PropertyValue* pxMissing = xMissing.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelVillager);
			const Zenith_PropertyValue* pxMissingRemaining = xMissing.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining);
			Check(pxMissing != nullptr && pxMissing->GetType() == PROPERTY_TYPE_ENTITY_ID, "ItemArmChannel missing entity output is ENTITY_ID-tagged");
			if (pxMissing && pxMissing->GetType() == PROPERTY_TYPE_ENTITY_ID) CheckEqU64(pxMissing->GetPackedEntityID(), 0ull, "ItemArmChannel missing entity becomes packed zero");
			Check(pxMissingRemaining && pxMissingRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxMissingRemaining->GetFloat() == 1.0f, "ItemArmChannel missing entity replaces both preseeded outputs with its fresh values");
			Check(xMissing.GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_Villager) == 0u, "ItemArmChannel missing Villager has no named fallback");
		}
		{
			Zenith_GraphBlackboard xEmpty;
			Zenith_GraphContext xEmptyContext;
			xEmptyContext.m_pxBlackboard = &xEmpty;
			DPNode_ItemArmChannel xWrongTag;
			Zenith_PropertyValue xWrong;
			xWrong.SetFloat(1.0f);
			xWrongTag.SetInputForTest(DPNode_ItemArmChannel::uPIN_Villager, xWrong);
			xWrongTag.SetInputForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration, xWrong);
			Check(xWrongTag.Execute(xEmptyContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel accepts wrong-tag entity input as packed zero");
			Check(xWrongTag.GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel wrong-tag entity uses valid pin addresses");
			const Zenith_PropertyValue* pxWrong = xWrongTag.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelVillager);
			Check(pxWrong != nullptr && pxWrong->GetType() == PROPERTY_TYPE_ENTITY_ID, "ItemArmChannel wrong-tag entity output is ENTITY_ID-tagged");
			if (pxWrong && pxWrong->GetType() == PROPERTY_TYPE_ENTITY_ID) CheckEqU64(pxWrong->GetPackedEntityID(), 0ull, "ItemArmChannel wrong-tag entity becomes packed zero");
			Check(xWrongTag.GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_Villager) == 0u, "ItemArmChannel wrong-tag override does not fall back");
			Check(xWrongTag.GetMismatchWarningCountForTest(DPNode_ItemArmChannel::uPIN_Villager) == 1u, "ItemArmChannel wrong-tag entity input emits its one typed mismatch diagnostic");
		}
		{
			Zenith_GraphBlackboard xUnnamed;
			Zenith_GraphContext xUnnamedContext;
			xUnnamedContext.m_pxBlackboard = &xUnnamed;
			DPNode_ItemArmChannel xUnnamedNode;
			xUnnamedNode.m_strChannelVillagerVar = "";
			xUnnamedNode.m_strChannelRemainingVar = "";
			Zenith_PropertyValue xDuration;
			xDuration.SetFloat(4.0f);
			xUnnamedNode.SetInputForTest(DPNode_ItemArmChannel::uPIN_Villager, EntityValue(0x0000000800000008ull));
			xUnnamedNode.SetInputForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration, xDuration);
			Check(xUnnamedNode.Execute(xUnnamedContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel succeeds with empty output names");
			Check(xUnnamedNode.GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel unnamed outputs use valid pin addresses");
			const Zenith_PropertyValue* pxUnnamedOwner = xUnnamedNode.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelVillager);
			const Zenith_PropertyValue* pxUnnamedRemaining = xUnnamedNode.GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining);
			Check(pxUnnamedOwner && pxUnnamedOwner->GetType() == PROPERTY_TYPE_ENTITY_ID, "ItemArmChannel unnamed owner output still publishes");
			Check(pxUnnamedRemaining && pxUnnamedRemaining->GetType() == PROPERTY_TYPE_FLOAT, "ItemArmChannel unnamed remaining output still publishes");
			if (pxUnnamedOwner && pxUnnamedOwner->GetType() == PROPERTY_TYPE_ENTITY_ID) CheckEqU64(pxUnnamedOwner->GetPackedEntityID(), 0x0000000800000008ull, "ItemArmChannel unnamed owner output carries its pin value");
			if (pxUnnamedRemaining && pxUnnamedRemaining->GetType() == PROPERTY_TYPE_FLOAT) CheckEqFloat(pxUnnamedRemaining->GetFloat(), 4.0f, "ItemArmChannel unnamed remaining output carries its pin value");
			Check(xUnnamed.GetCount() == 0u, "ItemArmChannel unnamed outputs do not create blackboard variables");
		}
	}

	void CheckItemArmChannelWireOrder()
	{
		g_pxLiveArm = nullptr;
		Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
		xRegistry.EnsureInitialized();
		Zenith_GraphDefinition xDefinition;
		const u_int uProducer = xDefinition.AddNode("DPTestArmOrderProducer");
		const u_int uArm = xDefinition.AddNode("DPItemArmChannel");
		DPNode_ItemArmChannel xConfiguredArm;
		xConfiguredArm.m_strVillagerVar = "";
		xConfiguredArm.m_strChannelVillagerVar = "";
		xConfiguredArm.m_strChannelDurationVar = "";
		xConfiguredArm.m_strChannelRemainingVar = "";
		const bool bParams = uArm != 0u && xDefinition.SetNodeParamsFromInstance(uArm, &xConfiguredArm);
		const bool bEdge = uProducer != 0u && uArm != 0u && xDefinition.AddDataEdge(uProducer, "Value", uArm, "ChannelDuration");
		Check(bParams && bEdge, "ItemArmChannel ordering witness configures its real data wire");
		Zenith_BehaviourGraph xGraph;
		const bool bInit = bParams && bEdge && xGraph.InitialiseFromDefinition(xDefinition);
		Check(bInit, "ItemArmChannel ordering witness graph initializes");
		Check(!bInit || xGraph.GetResolutionSkipCountForTest() == 0u, "ItemArmChannel ordering witness resolves every data edge");
		Zenith_GraphNode* pxBase = bInit ? xGraph.FindNode(uArm) : nullptr;
		Zenith_GraphNode* pxProducerBase = bInit ? xGraph.FindNode(uProducer) : nullptr;
		DPNode_ItemArmChannel* pxArm = pxBase && std::strcmp(pxBase->GetTypeName(), "DPItemArmChannel") == 0 ? static_cast<DPNode_ItemArmChannel*>(pxBase) : nullptr;
		Check(pxArm != nullptr, "ItemArmChannel ordering witness resolves Arm node");
		if (pxArm)
		{
			g_bArmOrderProducerSawOwner = false;
			g_bArmOrderProducerSawRemainingBeforePublish = false;
			g_bArmOrderProducerForceValue = false;
			g_fArmOrderProducerValue = 7.5f;
			g_fArmOrderExpectedRemainingBeforePublish = 4.5f;
			g_iArmOrderProducerPulls = 0;
			Zenith_GraphContext xContext;
			xContext.m_pxGraph = &xGraph;
			xContext.m_pxBlackboard = &xGraph.GetBlackboard();
			pxArm->SetInputForTest(DPNode_ItemArmChannel::uPIN_Villager, EntityValue(0x0000000B0000000Bull));
			pxArm->SetOutput(xContext, DPNode_ItemArmChannel::uPIN_ChannelRemaining, FloatValue(4.5f));
			g_pxLiveArm = pxArm;
			Check(pxArm->Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel ordering witness executes");
			g_pxLiveArm = nullptr;
			const Zenith_PropertyValue* pxRemaining = pxArm->GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining);
			Check(g_iArmOrderProducerPulls == 1 && g_bArmOrderProducerSawOwner && g_bArmOrderProducerSawRemainingBeforePublish
				&& pxRemaining && pxRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxRemaining->GetFloat() == 7.5f,
				"ItemArmChannel publishes its live owner slot before one lazy Duration pull, then publishes 7.5");
			Check(pxArm->GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_Villager) == 0u && pxArm->GetFallbackUseCountForTest(DPNode_ItemArmChannel::uPIN_ChannelDuration) == 0u && pxArm->GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel ordering witness has no fallback or BADACCESS");
			Check(pxProducerBase != nullptr && pxProducerBase->GetBadAccessWarningCountForTest() == 0u, "ItemArmChannel ordering producer uses valid output pin addresses");
		}
		xGraph.Shutdown();

		// The legal FLOAT alias must pull its actual Duration producer before the
		// final ChannelRemaining publication overwrites the shared property.
		Zenith_GraphDefinition xAliasDefinition;
		const u_int uAliasProducer = xAliasDefinition.AddNode("DPTestArmOrderProducer");
		const u_int uAliasArm = xAliasDefinition.AddNode("DPItemArmChannel");
		DPNode_ItemArmChannel xAliasArm;
		xAliasArm.m_strVillagerVar = "";
		xAliasArm.m_strChannelVillagerVar = "";
		xAliasArm.m_strChannelDurationVar = "";
		xAliasArm.m_strChannelRemainingVar = "";
		const bool bAliasParams = uAliasArm != 0u && xAliasDefinition.SetNodeParamsFromInstance(uAliasArm, &xAliasArm);
		const bool bAliasEdge = uAliasProducer != 0u && uAliasArm != 0u && xAliasDefinition.AddDataEdge(uAliasProducer, "Value", uAliasArm, "ChannelDuration");
		Zenith_BehaviourGraph xAliasGraph;
		const bool bAliasInit = bAliasParams && bAliasEdge && xAliasGraph.InitialiseFromDefinition(xAliasDefinition);
		Check(bAliasInit && xAliasGraph.GetResolutionSkipCountForTest() == 0u, "ItemArmChannel Duration/Remaining alias graph initializes with its producer wire");
		Zenith_GraphNode* pxAliasBase = bAliasInit ? xAliasGraph.FindNode(uAliasArm) : nullptr;
		DPNode_ItemArmChannel* pxAliasArm = pxAliasBase && std::strcmp(pxAliasBase->GetTypeName(), "DPItemArmChannel") == 0 ? static_cast<DPNode_ItemArmChannel*>(pxAliasBase) : nullptr;
		Check(pxAliasArm != nullptr, "ItemArmChannel Duration/Remaining alias resolves Arm node");
		if (pxAliasArm)
		{
			Zenith_GraphContext xAliasContext;
			xAliasContext.m_pxGraph = &xAliasGraph;
			xAliasContext.m_pxBlackboard = &xAliasGraph.GetBlackboard();
			pxAliasArm->SetInputForTest(DPNode_ItemArmChannel::uPIN_Villager, EntityValue(0x0000000B0000000Bull));
			pxAliasArm->SetOutput(xAliasContext, DPNode_ItemArmChannel::uPIN_ChannelRemaining, FloatValue(3.5f));
			g_bArmOrderProducerSawOwner = false;
			g_bArmOrderProducerSawRemainingBeforePublish = false;
			g_bArmOrderProducerForceValue = false;
			g_fArmOrderProducerValue = 6.5f;
			g_fArmOrderExpectedRemainingBeforePublish = 3.5f;
			g_iArmOrderProducerPulls = 0;
			g_pxLiveArm = pxAliasArm;
			Check(pxAliasArm->Execute(xAliasContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemArmChannel Duration/Remaining alias executes with its lazy producer");
			g_pxLiveArm = nullptr;
			const Zenith_PropertyValue* pxAlias = pxAliasArm->GetOutputForTest(DPNode_ItemArmChannel::uPIN_ChannelRemaining);
			Check(g_iArmOrderProducerPulls == 1 && g_bArmOrderProducerSawOwner && g_bArmOrderProducerSawRemainingBeforePublish
				&& pxAlias && pxAlias->GetType() == PROPERTY_TYPE_FLOAT && pxAlias->GetFloat() == 6.5f,
				"ItemArmChannel pulls the exact 6.5 Duration producer before final slot publication");
		}
		g_pxLiveArm = nullptr;
		xAliasGraph.Shutdown();
	}

	void CheckItemCommitFinishWriterChain()
	{
		EnsureDPGraphCommitVillagerProducerRegistered();
		DPGraphNodeItemFixture xFixture;
		Check(xFixture.m_bReady, "Commit/Finish graph fixture has the required item singletons");
		if (!xFixture.m_bReady) return;
		const Zenith_EntityID xVillager = xFixture.CreateItem("CommitFinishVillager", DP_ItemTag::None);
		const Zenith_EntityID xPickup = xFixture.CreateItem("CommitFinishPickup", DP_ItemTag::BogWater);
		Zenith_Entity xPickupEntity = g_xEngine.Scenes().ResolveEntity(xPickup);
		Check(xVillager.IsValid() && xPickupEntity.IsValid(), "Commit/Finish graph fixture creates villager and pickup entities");
		if (!xVillager.IsValid() || !xPickupEntity.IsValid()) return;

		Zenith_GraphDefinition xDefinition;
		Zenith_GraphBuilder xBuilder(xDefinition);
		xBuilder.Variable("tag", IntValue(static_cast<int32_t>(DP_ItemTag::None)));
		xBuilder.Variable("channelVillager", EntityValue(0ull));
		xBuilder.Variable("channelRemaining", FloatValue(0.0f));
		const u_int uSource = xBuilder.Node("OnCustomEvent");
		xBuilder.ParamString(uSource, "m_strEventName", "Commit");
		const u_int uProducer = xBuilder.Node("DPTestCommitVillagerProducer");
		const u_int uCommit = xBuilder.Node("DPItemCommitPickup");
		xBuilder.ParamString(uCommit, "m_strVillagerVar", "");
		xBuilder.ParamString(uCommit, "m_strChannelVillagerVar", "");
		xBuilder.ParamString(uCommit, "m_strChannelRemainingVar", "");
		xBuilder.ParamString(uCommit, "m_strCommittedVillagerVar", "");
		const u_int uOwner = xBuilder.Node("SetBlackboardEntityID");
		xBuilder.ParamString(uOwner, "m_strVariable", "channelVillager");
		const u_int uRemaining = xBuilder.Node("SetBlackboardFloat");
		xBuilder.ParamString(uRemaining, "m_strVariable", "channelRemaining");
		const u_int uFinish = xBuilder.Node("DPItemFinishPickup");
		xBuilder.ParamString(uFinish, "m_strVillagerVar", "");
		xBuilder.ParamString(uFinish, "m_strTagVar", "");
		const u_int uTag = xBuilder.Node("GetVariable");
		xBuilder.ParamString(uTag, "m_strVariable", "tag");
		xBuilder.DataEdge(uProducer, "Value", uCommit, "Villager");
		xBuilder.DataEdge(uCommit, "ChannelVillager", uOwner, "Value");
		xBuilder.DataEdge(uCommit, "ChannelRemaining", uRemaining, "Value");
		xBuilder.DataEdge(uCommit, "CommittedVillager", uFinish, "Villager");
		xBuilder.DataEdge(uTag, "Value", uFinish, "Tag");
		xBuilder.Edge(uSource, 0u, uCommit);
		xBuilder.Chain(uCommit, uOwner).Chain(uOwner, uRemaining).Chain(uRemaining, uFinish);

		Zenith_BehaviourGraph xGraph;
		const bool bBuilt = xBuilder.Build();
		const bool bInit = bBuilt && xGraph.InitialiseFromDefinition(xDefinition);
		Check(bInit && xGraph.GetResolutionSkipCountForTest() == 0u, "Commit -> writers -> Finish graph initializes with every wire resolved");
		if (!bInit) return;
		Zenith_GraphBlackboard& xBB = xGraph.GetBlackboard();
		xBB.SetValue("tag", IntValue(static_cast<int32_t>(DP_ItemTag::BogWater)));
		xBB.SetValue("channelVillager", EntityValue(0x0000000A0000000Aull));
		xBB.SetValue("channelRemaining", FloatValue(6.5f));
		Zenith_GraphContext xContext;
		xContext.m_pxGraph = &xGraph;
		xContext.m_pxBlackboard = &xBB;
		xContext.m_xSelf = xPickupEntity;
		DPNode_ItemCommitPickup* pxCommit = static_cast<DPNode_ItemCommitPickup*>(xGraph.FindNode(uCommit));
		Check(pxCommit != nullptr, "Commit/Finish graph exposes its Commit instance");
		if (pxCommit == nullptr) { xGraph.Shutdown(); return; }
		Zenith_PropertyValue xSeedOwner = EntityValue(0x0000000D0000000Dull);
		Zenith_PropertyValue xSeedRemaining = FloatValue(9.5f);
		pxCommit->SetOutput(xContext, DPNode_ItemCommitPickup::uPIN_ChannelVillager, xSeedOwner);
		pxCommit->SetOutput(xContext, DPNode_ItemCommitPickup::uPIN_ChannelRemaining, xSeedRemaining);
		g_ulCommitProducerValue = xVillager.GetPacked();
		g_iCommitProducerPulls = 0;
		g_iItemPickedEvents = 0;
		g_eLastPickedTag = DP_ItemTag::None;
		g_pxCommitBlackboard = &xBB;
		g_pxLiveCommit = pxCommit;
		g_bCommitEventSawClears = false;
		const Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnItemPickedUp>(&OnItemPickedUp);
		xGraph.FireCustomEvent("Commit", xContext);
		Check(g_iCommitProducerPulls == 1, "Commit consumes the original Villager producer exactly once");
		Check(g_iItemPickedEvents == 1 && g_eLastPickedTag == DP_ItemTag::BogWater && g_bCommitEventSawClears,
			"Finish event observes both persistent channel clears and the exact Tag");
		Check(DP_Player::GetHeldItemEntity(xVillager).GetPacked() == xPickup.GetPacked(), "Finish assigns the exact pickup to the validated Villager");

		xBB.SetValue("channelVillager", EntityValue(0x0000000C0000000Cull));
		xBB.SetValue("channelRemaining", FloatValue(5.0f));
		pxCommit->SetOutput(xContext, DPNode_ItemCommitPickup::uPIN_ChannelVillager, EntityValue(0x0000000E0000000Eull));
		pxCommit->SetOutput(xContext, DPNode_ItemCommitPickup::uPIN_ChannelRemaining, FloatValue(8.0f));
		g_ulCommitProducerValue = INVALID_ENTITY_ID.GetPacked();
		xGraph.FireCustomEvent("Commit", xContext);
		Check(g_iCommitProducerPulls == 2 && g_iItemPickedEvents == 1, "invalid after valid pulls once but skips writers, Finish, and event");
		const Zenith_PropertyValue* pxPersistedOwner = xBB.TryGetValue("channelVillager");
		const Zenith_PropertyValue* pxPersistedRemaining = xBB.TryGetValue("channelRemaining");
		Check(pxPersistedOwner && pxPersistedOwner->GetType() == PROPERTY_TYPE_ENTITY_ID && pxPersistedOwner->GetPackedEntityID() == 0x0000000C0000000Cull
			&& pxPersistedRemaining && pxPersistedRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxPersistedRemaining->GetFloat() == 5.0f,
			"invalid after valid preserves seeded persistent channel state");
		if (pxCommit != nullptr)
		{
			const Zenith_PropertyValue* pxOwner = pxCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager);
			const Zenith_PropertyValue* pxRemaining = pxCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining);
			const Zenith_PropertyValue* pxLatched = pxCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_CommittedVillager);
			Check(pxOwner && pxOwner->GetType() == PROPERTY_TYPE_ENTITY_ID && pxOwner->GetPackedEntityID() == 0x0000000E0000000Eull
				&& pxRemaining && pxRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxRemaining->GetFloat() == 8.0f
				&& pxLatched && pxLatched->GetType() == PROPERTY_TYPE_ENTITY_ID && pxLatched->GetPackedEntityID() == xVillager.GetPacked(),
				"invalid after valid retains reseeded clear slots and the prior validated-entity latch");
		}
		Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
		g_pxCommitBlackboard = nullptr;
		g_pxLiveCommit = nullptr;
		DP_Player::RemoveHeldItem(xVillager);
		DP_Items::Internal_UnregisterItemTag(xVillager);
		DP_Items::Internal_UnregisterItemTag(xPickup);
		xGraph.Shutdown();
	}

	void CheckReadTuningFloat()
	{
		const char* szKey = "interactables.chest_open_duration_s";
		const float fExpected = DP_Tuning::Get<float>(szKey);
		Check(fExpected != 0.0f, "ReadTuningFloat known chest duration default is nonzero");
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_pxBlackboard = &xBlackboard;
		xBlackboard.SetValue(szKey, FloatValue(fExpected + 1.0f));
		DPNode_ReadTuningFloat xNode;
		xNode.m_strKey = szKey;
		xNode.m_strVar = "";
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "ReadTuningFloat succeeds for the known chest duration key");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u, "ReadTuningFloat success uses valid pin addresses");
		const Zenith_PropertyValue* pxResult = xNode.GetOutputForTest(DPNode_ReadTuningFloat::uPIN_Result);
		Check(pxResult && pxResult->GetType() == PROPERTY_TYPE_FLOAT, "ReadTuningFloat success publishes a FLOAT result");
		if (pxResult && pxResult->GetType() == PROPERTY_TYPE_FLOAT) CheckEqFloat(pxResult->GetFloat(), fExpected, "ReadTuningFloat output matches the live tuning value");
		const Zenith_PropertyValue* pxContrary = xBlackboard.TryGetValue(szKey);
		Check(pxContrary && pxContrary->GetType() == PROPERTY_TYPE_FLOAT && pxContrary->GetFloat() == fExpected + 1.0f,
			"ReadTuningFloat uses its tuning key rather than the contrary same-named blackboard value");

		xNode.m_strKey = "";
		Check(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE, "ReadTuningFloat empty key fails on the same initialized instance");
		Check(xNode.GetBadAccessWarningCountForTest() == 0u, "ReadTuningFloat empty-key failure uses valid pin addresses");
		pxResult = xNode.GetOutputForTest(DPNode_ReadTuningFloat::uPIN_Result);
		Check(pxResult && pxResult->GetType() == PROPERTY_TYPE_FLOAT, "ReadTuningFloat failure retains the initialized FLOAT output slot");
		if (pxResult && pxResult->GetType() == PROPERTY_TYPE_FLOAT) CheckEqFloat(pxResult->GetFloat(), fExpected, "ReadTuningFloat empty-key failure retains the previous result");

		DPNode_ReadTuningFloat xBare;
		Check(xBare.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE, "bare ReadTuningFloat empty key fails before any accessor");
		Check(xBare.GetBadAccessWarningCountForTest() == 0u, "bare ReadTuningFloat failure uses no invalid pin address");
		Check(xBare.GetOutputForTest(DPNode_ReadTuningFloat::uPIN_Result) == nullptr, "bare ReadTuningFloat early failure leaves its output slot unbuilt");

		Zenith_GraphDefinition xDefinition;
		const u_int uNodeID = xDefinition.AddNode("DPReadTuningFloat");
		Zenith_BehaviourGraph xGraph;
		const bool bInitialised = uNodeID != 0u && xGraph.InitialiseFromDefinition(xDefinition);
		Check(bInitialised, "fresh ReadTuningFloat graph definition initializes through the public graph API");
		Zenith_GraphNode* pxBase = bInitialised ? xGraph.FindNode(uNodeID) : nullptr;
		const bool bRightType = pxBase != nullptr && std::strcmp(pxBase->GetTypeName(), "DPReadTuningFloat") == 0;
		Check(bRightType, "fresh graph exposes its initialized DPReadTuningFloat node");
		DPNode_ReadTuningFloat* pxFresh = bRightType ? static_cast<DPNode_ReadTuningFloat*>(pxBase) : nullptr;
		if (pxFresh != nullptr)
		{
			Zenith_GraphContext xFreshContext;
			xFreshContext.m_pxGraph = &xGraph;
			xFreshContext.m_pxBlackboard = &xGraph.GetBlackboard();
			Check(pxFresh->Execute(xFreshContext) == GRAPH_NODE_STATUS_FAILURE, "fresh graph-initialized ReadTuningFloat empty key fails");
			const Zenith_PropertyValue* pxFreshResult = pxFresh->GetOutputForTest(DPNode_ReadTuningFloat::uPIN_Result);
			Check(pxFreshResult && pxFreshResult->GetType() == PROPERTY_TYPE_FLOAT, "fresh graph-initialized failure has a typed FLOAT output slot");
			if (pxFreshResult && pxFreshResult->GetType() == PROPERTY_TYPE_FLOAT) CheckEqFloat(pxFreshResult->GetFloat(), 0.0f, "fresh graph-initialized failure keeps typed FLOAT zero");
			Check(pxFresh->GetBadAccessWarningCountForTest() == 0u, "fresh ReadTuningFloat failure uses valid pin addresses");
		}
		xGraph.Shutdown();
	}

	void CheckReadHeldObjectiveAndCommitPickup()
	{
		DPGraphNodeItemFixture xFixture;
		Check(xFixture.m_bReady, "DP graph item fixture has both manager singletons");
		if (!xFixture.m_bReady) return;

		const Zenith_EntityID xVillager = xFixture.CreateItem("DPGraphNodePinsVillager", DP_ItemTag::None);
		const Zenith_EntityID xObjective = xFixture.CreateItem("DPGraphNodePinsObjective", DP_ItemTag::Objective2);
		const Zenith_EntityID xNoObjective = xFixture.CreateItem("DPGraphNodePinsNoObjective", DP_ItemTag::Key);
		const Zenith_EntityID xPickup = xFixture.CreateItem("DPGraphNodePinsPickup", DP_ItemTag::BogWater);
		Check(xVillager.IsValid() && xObjective.IsValid() && xNoObjective.IsValid() && xPickup.IsValid(), "DP graph item fixture created concrete entities");
		if (!xVillager.IsValid() || !xObjective.IsValid() || !xNoObjective.IsValid() || !xPickup.IsValid())
		{
			DP_Player::RemoveHeldItem(xVillager);
			if (xObjective.IsValid()) DP_Items::Internal_UnregisterItemTag(xObjective);
			if (xNoObjective.IsValid()) DP_Items::Internal_UnregisterItemTag(xNoObjective);
			if (xPickup.IsValid()) DP_Items::Internal_UnregisterItemTag(xPickup);
			if (xVillager.IsValid()) DP_Items::Internal_UnregisterItemTag(xVillager);
			return;
		}
		DP_Player::SetHeldItem(xVillager, xObjective);

		Zenith_GraphBlackboard xHeldBlackboard;
		Zenith_GraphContext xHeldContext;
		xHeldContext.m_pxBlackboard = &xHeldBlackboard;
		DPNode_ReadHeldObjective xHeld;
		xHeld.m_strVillagerVar = "";
		xHeld.m_strTagVar = "";
		xHeld.SetInputForTest(DPNode_ReadHeldObjective::uPIN_Villager, EntityValue(xVillager.GetPacked()));
		Check(xHeld.Execute(xHeldContext) == GRAPH_NODE_STATUS_SUCCESS, "ReadHeldObjective succeeds for a registered held objective via the distinct Villager pin value");
		Check(xHeld.GetBadAccessWarningCountForTest() == 0u, "ReadHeldObjective success uses valid pin addresses");
		const Zenith_PropertyValue* pxHeld = xHeld.GetOutputForTest(DPNode_ReadHeldObjective::uPIN_Tag);
		Check(pxHeld && pxHeld->GetType() == PROPERTY_TYPE_INT32, "ReadHeldObjective publishes a typed Tag output");
		if (pxHeld && pxHeld->GetType() == PROPERTY_TYPE_INT32) Check(pxHeld->GetInt32() == static_cast<int32_t>(DP_ItemTag::Objective2), "ReadHeldObjective output is the nonzero objective tag");
		Check(xHeld.GetFallbackUseCountForTest(DPNode_ReadHeldObjective::uPIN_Villager) == 0u, "ReadHeldObjective distinct override does not fall back");

		DPNode_ReadHeldObjective xUnboundHeld;
		xUnboundHeld.m_strVillagerVar = "";
		xUnboundHeld.m_strTagVar = "";
		Check(xUnboundHeld.Execute(xHeldContext) == GRAPH_NODE_STATUS_FAILURE, "ReadHeldObjective real unbound Villager input fails");
		Check(xUnboundHeld.GetFallbackUseCountForTest(DPNode_ReadHeldObjective::uPIN_Villager) == 0u, "ReadHeldObjective real unbound Villager input has no named fallback");

		EnsureDPGraphCommitVillagerProducerRegistered();
		Zenith_GraphDefinition xWireDefinition;
		const u_int uWireProducer = xWireDefinition.AddNode("DPTestCommitVillagerProducer");
		const u_int uWireHeld = xWireDefinition.AddNode("DPReadHeldObjective");
		DPNode_ReadHeldObjective xWireHeldParams;
		xWireHeldParams.m_strVillagerVar = "";
		xWireHeldParams.m_strTagVar = "";
		const bool bWireParams = uWireHeld != 0u && xWireDefinition.SetNodeParamsFromInstance(uWireHeld, &xWireHeldParams);
		const bool bWireEdge = uWireProducer != 0u && uWireHeld != 0u && xWireDefinition.AddDataEdge(uWireProducer, "Value", uWireHeld, "Villager");
		Zenith_BehaviourGraph xWireGraph;
		const bool bWireInit = bWireParams && bWireEdge && xWireGraph.InitialiseFromDefinition(xWireDefinition);
		Check(bWireInit && xWireGraph.GetResolutionSkipCountForTest() == 0u, "ReadHeldObjective actual Villager wire initializes without resolution skips");
		Zenith_GraphNode* pxWireBase = bWireInit ? xWireGraph.FindNode(uWireHeld) : nullptr;
		DPNode_ReadHeldObjective* pxWireHeld = pxWireBase && std::strcmp(pxWireBase->GetTypeName(), "DPReadHeldObjective") == 0 ? static_cast<DPNode_ReadHeldObjective*>(pxWireBase) : nullptr;
		Check(pxWireHeld != nullptr, "ReadHeldObjective actual Villager wire exposes its node");
		if (pxWireHeld)
		{
			Zenith_GraphContext xWireContext;
			xWireContext.m_pxGraph = &xWireGraph;
			xWireContext.m_pxBlackboard = &xWireGraph.GetBlackboard();
			g_ulCommitProducerValue = xVillager.GetPacked();
			g_iCommitProducerPulls = 0;
			Check(pxWireHeld->Execute(xWireContext) == GRAPH_NODE_STATUS_SUCCESS, "ReadHeldObjective succeeds through its actual Villager wire");
			const Zenith_PropertyValue* pxWireTag = pxWireHeld->GetOutputForTest(DPNode_ReadHeldObjective::uPIN_Tag);
			Check(g_iCommitProducerPulls == 1 && pxWireTag && pxWireTag->GetType() == PROPERTY_TYPE_INT32 && pxWireTag->GetInt32() == static_cast<int32_t>(DP_ItemTag::Objective2), "ReadHeldObjective pulls its exact wired Villager once and publishes the nonzero tag");
			Check(pxWireHeld->GetFallbackUseCountForTest(DPNode_ReadHeldObjective::uPIN_Villager) == 0u, "ReadHeldObjective actual Villager wire has no fallback");
		}
		xWireGraph.Shutdown();

		xHeld.SetInputForTest(DPNode_ReadHeldObjective::uPIN_Villager, EntityValue(INVALID_ENTITY_ID.GetPacked()));
		Check(xHeld.Execute(xHeldContext) == GRAPH_NODE_STATUS_FAILURE, "ReadHeldObjective invalid villager fails after a successful run");
		pxHeld = xHeld.GetOutputForTest(DPNode_ReadHeldObjective::uPIN_Tag);
		Check(pxHeld && pxHeld->GetType() == PROPERTY_TYPE_INT32 && pxHeld->GetInt32() == static_cast<int32_t>(DP_ItemTag::Objective2), "ReadHeldObjective invalid failure retains its output slot");
		Check(xHeld.GetBadAccessWarningCountForTest() == 0u, "ReadHeldObjective failure retains valid pin access");
		DP_Player::SetHeldItem(xVillager, xNoObjective);
		xHeld.SetInputForTest(DPNode_ReadHeldObjective::uPIN_Villager, EntityValue(xVillager.GetPacked()));
		Check(xHeld.Execute(xHeldContext) == GRAPH_NODE_STATUS_FAILURE, "ReadHeldObjective non-objective held item fails after a successful run");
		pxHeld = xHeld.GetOutputForTest(DPNode_ReadHeldObjective::uPIN_Tag);
		Check(pxHeld && pxHeld->GetType() == PROPERTY_TYPE_INT32 && pxHeld->GetInt32() == static_cast<int32_t>(DP_ItemTag::Objective2), "ReadHeldObjective no-objective failure retains its output slot");

		Zenith_GraphDefinition xHeldDefinition;
		const u_int uHeldNode = xHeldDefinition.AddNode("DPReadHeldObjective");
		DPNode_ReadHeldObjective xFreshHeldParams;
		xFreshHeldParams.m_strVillagerVar = "";
		xFreshHeldParams.m_strTagVar = "";
		const bool bFreshParams = uHeldNode != 0u && xHeldDefinition.SetNodeParamsFromInstance(uHeldNode, &xFreshHeldParams);
		Zenith_BehaviourGraph xHeldGraph;
		const bool bHeldInitialized = bFreshParams && xHeldGraph.InitialiseFromDefinition(xHeldDefinition);
		Check(bHeldInitialized, "fresh ReadHeldObjective graph initializes");
		Zenith_GraphNode* pxHeldBase = bHeldInitialized ? xHeldGraph.FindNode(uHeldNode) : nullptr;
		DPNode_ReadHeldObjective* pxFreshHeld = pxHeldBase && std::strcmp(pxHeldBase->GetTypeName(), "DPReadHeldObjective") == 0 ? static_cast<DPNode_ReadHeldObjective*>(pxHeldBase) : nullptr;
		Check(pxFreshHeld != nullptr, "fresh graph exposes ReadHeldObjective");
		if (pxFreshHeld)
		{
			Zenith_GraphContext xFreshContext;
			xFreshContext.m_pxGraph = &xHeldGraph;
			xFreshContext.m_pxBlackboard = &xHeldGraph.GetBlackboard();
			Check(pxFreshHeld->Execute(xFreshContext) == GRAPH_NODE_STATUS_FAILURE, "fresh ReadHeldObjective missing villager fails");
			Check(pxFreshHeld->GetFallbackUseCountForTest(DPNode_ReadHeldObjective::uPIN_Villager) == 0u, "fresh ReadHeldObjective unbound Villager has no named fallback");
			const Zenith_PropertyValue* pxFreshTag = pxFreshHeld->GetOutputForTest(DPNode_ReadHeldObjective::uPIN_Tag);
			Check(pxFreshTag && pxFreshTag->GetType() == PROPERTY_TYPE_INT32 && pxFreshTag->GetInt32() == 0, "fresh ReadHeldObjective failure has typed INT32 zero");
			Check(pxFreshHeld->GetBadAccessWarningCountForTest() == 0u, "fresh ReadHeldObjective failure uses valid pin addresses");
		}
		xHeldGraph.Shutdown();

		Zenith_GraphBlackboard xCommitBlackboard;
		Zenith_GraphContext xCommitContext;
		xCommitContext.m_pxBlackboard = &xCommitBlackboard;
		xCommitContext.m_xSelf = g_xEngine.Scenes().ResolveEntity(xPickup);
		Zenith_PropertyValue xChannelVillager = EntityValue(xVillager.GetPacked());
		Zenith_PropertyValue xChannelRemaining;
		xChannelRemaining.SetFloat(3.0f);
		xCommitBlackboard.SetValue("channelVillager", xChannelVillager);
		xCommitBlackboard.SetValue("channelRemaining", xChannelRemaining);
		g_iItemPickedEvents = 0;
		g_eLastPickedTag = DP_ItemTag::None;
		g_pxCommitBlackboard = &xCommitBlackboard;
		g_pxLiveCommit = nullptr;
		g_bCommitEventSawClears = false;
		const Zenith_EventHandle uPickupHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnItemPickedUp>(&OnItemPickedUp);
		DPNode_ItemCommitPickup xCommit;
		xCommit.m_strVillagerVar = "";
		xCommit.m_strChannelVillagerVar = "";
		xCommit.m_strChannelRemainingVar = "";
		xCommit.m_strCommittedVillagerVar = "";
		g_pxLiveCommit = &xCommit;
		Zenith_PropertyValue xTag;
		xTag.SetInt32(static_cast<int32_t>(DP_ItemTag::BogWater));
		xCommit.SetInputForTest(DPNode_ItemCommitPickup::uPIN_Villager, EntityValue(xVillager.GetPacked()));
		Check(xCommit.Execute(xCommitContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemCommitPickup succeeds for the fixture villager");
		Check(xCommit.GetBadAccessWarningCountForTest() == 0u, "ItemCommitPickup success uses valid pin addresses");
		const Zenith_PropertyValue* pxClearVillager = xCommit.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager);
		const Zenith_PropertyValue* pxClearRemaining = xCommit.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining);
		const Zenith_PropertyValue* pxCommittedVillager = xCommit.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_CommittedVillager);
		Check(pxClearVillager && pxClearVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxClearVillager->GetPackedEntityID() == 0ull, "ItemCommitPickup publishes cleared ChannelVillager");
		Check(pxClearRemaining && pxClearRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxClearRemaining->GetFloat() == 0.0f, "ItemCommitPickup publishes cleared ChannelRemaining");
		Check(pxCommittedVillager && pxCommittedVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxCommittedVillager->GetPackedEntityID() == xVillager.GetPacked(), "ItemCommitPickup latches the exact validated Villager");
		if (pxCommittedVillager != nullptr)
		{
			DPNode_ItemFinishPickup xFinish;
			xFinish.SetInputForTest(DPNode_ItemFinishPickup::uPIN_Villager, *pxCommittedVillager);
			xFinish.SetInputForTest(DPNode_ItemFinishPickup::uPIN_Tag, xTag);
			Check(xFinish.Execute(xCommitContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemFinishPickup succeeds from Commit's latched Villager");
			Check(DP_Player::GetHeldItemEntity(xVillager).GetPacked() == xPickup.GetPacked(), "ItemFinishPickup assigns its self item after channel clears");
			Check(g_iItemPickedEvents == 1 && g_eLastPickedTag == DP_ItemTag::BogWater, "ItemFinishPickup event carries Tag after Commit's slot clears");
		}
		DPNode_ItemFinishPickup xInvalidFinish;
		xInvalidFinish.m_strVillagerVar = "";
		xInvalidFinish.m_strTagVar = "";
		xInvalidFinish.SetInputForTest(DPNode_ItemFinishPickup::uPIN_Villager, EntityValue(INVALID_ENTITY_ID.GetPacked()));
		const int iEventsBeforeInvalidFinish = g_iItemPickedEvents;
		Check(xInvalidFinish.Execute(xCommitContext) == GRAPH_NODE_STATUS_FAILURE && g_iItemPickedEvents == iEventsBeforeInvalidFinish,
			"ItemFinishPickup explicit INVALID Villager fails before tag pull or event");

		EnsureDPGraphFinishTagProducerRegistered();
		Zenith_GraphDefinition xFinishDefinition;
		const u_int uFinishVillager = xFinishDefinition.AddNode("DPTestCommitVillagerProducer");
		const u_int uFinishTag = xFinishDefinition.AddNode("DPTestFinishTagProducer");
		const u_int uFinishNode = xFinishDefinition.AddNode("DPItemFinishPickup");
		DPNode_ItemFinishPickup xFinishParams;
		xFinishParams.m_strVillagerVar = "";
		xFinishParams.m_strTagVar = "";
		const bool bFinishParams = uFinishNode != 0u && xFinishDefinition.SetNodeParamsFromInstance(uFinishNode, &xFinishParams);
		const bool bFinishVillagerEdge = uFinishVillager != 0u && uFinishNode != 0u && xFinishDefinition.AddDataEdge(uFinishVillager, "Value", uFinishNode, "Villager");
		const bool bFinishTagEdge = uFinishTag != 0u && uFinishNode != 0u && xFinishDefinition.AddDataEdge(uFinishTag, "Value", uFinishNode, "Tag");
		Zenith_BehaviourGraph xFinishGraph;
		const bool bFinishInit = bFinishParams && bFinishVillagerEdge && bFinishTagEdge && xFinishGraph.InitialiseFromDefinition(xFinishDefinition);
		Check(bFinishInit && xFinishGraph.GetResolutionSkipCountForTest() == 0u, "Finish guard/order wire graph initializes without skips");
		Zenith_GraphNode* pxFinishBase = bFinishInit ? xFinishGraph.FindNode(uFinishNode) : nullptr;
		DPNode_ItemFinishPickup* pxWiredFinish = pxFinishBase && std::strcmp(pxFinishBase->GetTypeName(), "DPItemFinishPickup") == 0 ? static_cast<DPNode_ItemFinishPickup*>(pxFinishBase) : nullptr;
		if (pxWiredFinish)
		{
			Zenith_GraphContext xFinishContext;
			xFinishContext.m_pxGraph = &xFinishGraph;
			xFinishContext.m_pxBlackboard = &xFinishGraph.GetBlackboard();
			xFinishContext.m_xSelf = g_xEngine.Scenes().ResolveEntity(xPickup);
			g_xFinishExpectedVillager = xVillager;
			g_xFinishExpectedPickup = xPickup;
			DP_Player::RemoveHeldItem(xVillager);
			Check(!DP_Player::GetHeldItemEntity(xVillager).IsValid(), "Finish order witness begins with no held item");
			g_ulCommitProducerValue = INVALID_ENTITY_ID.GetPacked();
			g_iFinishTagProducerPulls = 0;
			const int iEventsBeforeWiredInvalid = g_iItemPickedEvents;
			Check(pxWiredFinish->Execute(xFinishContext) == GRAPH_NODE_STATUS_FAILURE && g_iFinishTagProducerPulls == 0
				&& g_iItemPickedEvents == iEventsBeforeWiredInvalid && !DP_Player::GetHeldItemEntity(xVillager).IsValid(),
				"wired Finish INVALID Villager skips Tag pull, event, and held-item effect");
			g_ulCommitProducerValue = xVillager.GetPacked();
			g_iFinishTagProducerPulls = 0;
			g_bFinishTagProducerSawHeld = false;
			const int iEventsBeforeFinish = g_iItemPickedEvents;
			Check(pxWiredFinish->Execute(xFinishContext) == GRAPH_NODE_STATUS_SUCCESS && g_iFinishTagProducerPulls == 1
				&& g_bFinishTagProducerSawHeld && g_iItemPickedEvents == iEventsBeforeFinish + 1 && g_eLastPickedTag == DP_ItemTag::BogWater,
				"ItemFinishPickup sets held item before its exact wired Tag pull, then dispatches the event");
		}
		xFinishGraph.Shutdown();

		DPNode_ItemCommitPickup xCommitEmpty;
		xCommitEmpty.m_strChannelVillagerVar = "";
		xCommitEmpty.m_strChannelRemainingVar = "";
		xCommitEmpty.m_strCommittedVillagerVar = "";
		xCommitEmpty.SetInputForTest(DPNode_ItemCommitPickup::uPIN_Villager, EntityValue(xVillager.GetPacked()));
		Check(xCommitEmpty.Execute(xCommitContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemCommitPickup succeeds with empty output names");
		Check(xCommitEmpty.GetBadAccessWarningCountForTest() == 0u, "ItemCommitPickup empty outputs use valid pin addresses");
		pxClearVillager = xCommitEmpty.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager);
		pxClearRemaining = xCommitEmpty.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining);
		Check(pxClearVillager && pxClearVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxClearVillager->GetPackedEntityID() == 0ull, "ItemCommitPickup unnamed ChannelVillager output still publishes");
		Check(pxClearRemaining && pxClearRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxClearRemaining->GetFloat() == 0.0f, "ItemCommitPickup unnamed ChannelRemaining output still publishes");

		xCommit.SetInputForTest(DPNode_ItemCommitPickup::uPIN_Villager, EntityValue(INVALID_ENTITY_ID.GetPacked()));
		Zenith_PropertyValue xReseedVillager = EntityValue(0x0000000A0000000Aull);
		Zenith_PropertyValue xReseedRemaining;
		xReseedRemaining.SetFloat(5.0f);
		xCommitBlackboard.SetValue("channelVillager", xReseedVillager);
		xCommitBlackboard.SetValue("channelRemaining", xReseedRemaining);
		Check(xCommit.Execute(xCommitContext) == GRAPH_NODE_STATUS_FAILURE, "ItemCommitPickup invalid villager fails before clears");
		const Zenith_PropertyValue* pxNamedCommitVillager = xCommitBlackboard.TryGetValue("channelVillager");
		const Zenith_PropertyValue* pxNamedCommitRemaining = xCommitBlackboard.TryGetValue("channelRemaining");
		Check(pxNamedCommitVillager && pxNamedCommitVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxNamedCommitVillager->GetPackedEntityID() == 0x0000000A0000000Aull, "ItemCommitPickup invalid failure leaves reseeded named villager value untouched");
		Check(pxNamedCommitRemaining && pxNamedCommitRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxNamedCommitRemaining->GetFloat() == 5.0f, "ItemCommitPickup invalid failure leaves reseeded named remaining value untouched");
		pxClearVillager = xCommit.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager);
		pxClearRemaining = xCommit.GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining);
		Check(pxClearVillager && pxClearVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxClearVillager->GetPackedEntityID() == 0ull, "ItemCommitPickup invalid failure retains prior Villager clear slot");
		Check(pxClearRemaining && pxClearRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxClearRemaining->GetFloat() == 0.0f, "ItemCommitPickup invalid failure retains prior Remaining clear slot");
		Check(xCommit.GetBadAccessWarningCountForTest() == 0u, "ItemCommitPickup failure retains valid pin access");

		Zenith_GraphDefinition xCommitDefinition;
		const u_int uCommitNode = xCommitDefinition.AddNode("DPItemCommitPickup");
		DPNode_ItemCommitPickup xFreshCommitParams;
		xFreshCommitParams.m_strVillagerVar = "";
		xFreshCommitParams.m_strChannelVillagerVar = "";
		xFreshCommitParams.m_strChannelRemainingVar = "";
		xFreshCommitParams.m_strCommittedVillagerVar = "";
		const bool bFreshCommitParams = uCommitNode != 0u && xCommitDefinition.SetNodeParamsFromInstance(uCommitNode, &xFreshCommitParams);
		Zenith_BehaviourGraph xCommitGraph;
		const bool bCommitInitialized = bFreshCommitParams && xCommitGraph.InitialiseFromDefinition(xCommitDefinition);
		Check(bCommitInitialized, "fresh ItemCommitPickup graph initializes");
		Zenith_GraphNode* pxCommitBase = bCommitInitialized ? xCommitGraph.FindNode(uCommitNode) : nullptr;
		DPNode_ItemCommitPickup* pxFreshCommit = pxCommitBase && std::strcmp(pxCommitBase->GetTypeName(), "DPItemCommitPickup") == 0 ? static_cast<DPNode_ItemCommitPickup*>(pxCommitBase) : nullptr;
		Check(pxFreshCommit != nullptr, "fresh graph exposes ItemCommitPickup");
		if (pxFreshCommit)
		{
			Zenith_GraphContext xFreshCommitContext;
			xFreshCommitContext.m_pxGraph = &xCommitGraph;
			xFreshCommitContext.m_pxBlackboard = &xCommitGraph.GetBlackboard();
			Check(pxFreshCommit->Execute(xFreshCommitContext) == GRAPH_NODE_STATUS_FAILURE, "fresh ItemCommitPickup missing villager fails before clear outputs");
			Check(pxFreshCommit->GetFallbackUseCountForTest(DPNode_ItemCommitPickup::uPIN_Villager) == 0u, "fresh ItemCommitPickup missing Villager has no named fallback");
			const Zenith_PropertyValue* pxFreshVillager = pxFreshCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelVillager);
			const Zenith_PropertyValue* pxFreshRemaining = pxFreshCommit->GetOutputForTest(DPNode_ItemCommitPickup::uPIN_ChannelRemaining);
			Check(pxFreshVillager && pxFreshVillager->GetType() == PROPERTY_TYPE_ENTITY_ID && pxFreshVillager->GetPackedEntityID() == 0ull, "fresh ItemCommitPickup failure has typed ENTITY_ID zero output");
			Check(pxFreshRemaining && pxFreshRemaining->GetType() == PROPERTY_TYPE_FLOAT && pxFreshRemaining->GetFloat() == 0.0f, "fresh ItemCommitPickup failure has typed FLOAT zero output");
			Check(pxFreshCommit->GetBadAccessWarningCountForTest() == 0u, "fresh ItemCommitPickup failure uses valid pin addresses");
		}
		xCommitGraph.Shutdown();
		Zenith_EventDispatcher::Get().Unsubscribe(uPickupHandle);
		g_pxCommitBlackboard = nullptr;
		g_pxLiveCommit = nullptr;
		DP_Player::RemoveHeldItem(xVillager);
		DP_Items::Internal_UnregisterItemTag(xObjective);
		DP_Items::Internal_UnregisterItemTag(xNoObjective);
		DP_Items::Internal_UnregisterItemTag(xPickup);
		DP_Items::Internal_UnregisterItemTag(xVillager);
	}

	void SetFootstepInputs(DPNode_VillagerEmitFootstep& xNode, bool bWalkQuiet)
	{
		xNode.m_strWalkQuietVar = "";
		xNode.m_strLoudnessVar = "";
		xNode.m_strRadiusVar = "";
		xNode.m_strQuietMultVar = "";
		xNode.SetInputForTest(DPNode_VillagerEmitFootstep::uPIN_WalkQuiet, BoolValue(bWalkQuiet));
		xNode.SetInputForTest(DPNode_VillagerEmitFootstep::uPIN_Loudness, FloatValue(0.8f));
		xNode.SetInputForTest(DPNode_VillagerEmitFootstep::uPIN_Radius, FloatValue(17.0f));
	}

	void CheckOneFootstepEmission(float fExpectedLoudness, const char* szWhat)
	{
		const Zenith_Vector<Zenith_AudioBus::EmittedSound>& xSounds =
			Zenith_AudioBus::GetEmittedSoundsForTest();
		u_int uFootstepIndex = 0u;
		u_int uFootstepCount = 0u;
		for (u_int u = 0u; u < xSounds.GetSize(); ++u)
		{
			const Zenith_AudioBus::EmittedSound& xSound = xSounds.Get(u);
			if (xSound.m_szName != nullptr && std::strcmp(xSound.m_szName, "DP.Villager.Footstep") == 0)
			{
				uFootstepIndex = u;
				++uFootstepCount;
			}
		}
		Check(uFootstepCount == 1u, szWhat);
		if (uFootstepCount == 1u)
		{
			const Zenith_AudioBus::EmittedSound& xSound = xSounds.Get(uFootstepIndex);
			CheckEqFloat(xSound.m_fLoudness, fExpectedLoudness, "VillagerEmitFootstep records the expected loudness");
			CheckEqFloat(xSound.m_fRadius, 17.0f, "VillagerEmitFootstep records the explicit radius");
		}
	}

	void CheckFootstepAndItemEvaporate()
	{
		DPGraphNodeItemFixture xFixture;
		Check(xFixture.m_bReady, "DP graph Footstep/Evaporate fixture has both manager singletons");
		if (!xFixture.m_bReady) return;

		Zenith_Entity xFootstepEntity =
			g_xEngine.Scenes().CreateEntity(xFixture.m_pxScene, "DPGraphNodePinsFootstep");
		const Zenith_EntityID xEvaporatingItem =
			xFixture.CreateItem("DPGraphNodePinsEvaporatingItem", DP_ItemTag::BogWater);
		Check(xFootstepEntity.IsValid() && xEvaporatingItem.IsValid(), "DP graph Footstep/Evaporate fixture created real entities");
		if (!xFootstepEntity.IsValid() || !xEvaporatingItem.IsValid())
		{
			if (xEvaporatingItem.IsValid()) DP_Items::Internal_UnregisterItemTag(xEvaporatingItem);
			return;
		}

		const Zenith_Maths::Vector3 xEvaporationPosition(13.0f, 2.0f, -7.0f);
		Zenith_Entity xEvaporationEntity = g_xEngine.Scenes().ResolveEntity(xEvaporatingItem);
		Zenith_TransformComponent* pxEvaporationTransform =
			xEvaporationEntity.IsValid() ? xEvaporationEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		Check(pxEvaporationTransform != nullptr, "ItemEvaporate fixture entity has a Transform");
		if (pxEvaporationTransform != nullptr) pxEvaporationTransform->SetPosition(xEvaporationPosition);

		Zenith_GraphContext xFootstepContext;
		xFootstepContext.m_xSelf = xFootstepEntity;
		{
			Zenith_GraphBlackboard xBlackboard;
			xBlackboard.SetValue("quietLoudnessMult", FloatValue(0.75f));
			xFootstepContext.m_pxBlackboard = &xBlackboard;
			DPNode_VillagerEmitFootstep xNode;
			SetFootstepInputs(xNode, true);
			xNode.SetInputForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult, FloatValue(0.25f));
			Zenith_AudioBus::ClearEmittedSoundsForTest();
			Check(xNode.Execute(xFootstepContext) == GRAPH_NODE_STATUS_SUCCESS, "VillagerEmitFootstep executes with all distinct pin overrides");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "VillagerEmitFootstep wired leg uses valid pin addresses");
			CheckOneFootstepEmission(0.2f, "VillagerEmitFootstep finds exactly one quiet footstep emission");
			Check(xNode.GetFallbackUseCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep wired QuietMult has no fallback");
			Check(xNode.GetMismatchWarningCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep wired QuietMult has no mismatch");
		}
		{
			Zenith_GraphBlackboard xBlackboard;
			xFootstepContext.m_pxBlackboard = &xBlackboard;
			DPNode_VillagerEmitFootstep xNode;
			SetFootstepInputs(xNode, true);
			Zenith_AudioBus::ClearEmittedSoundsForTest();
			Check(xNode.Execute(xFootstepContext) == GRAPH_NODE_STATUS_SUCCESS, "VillagerEmitFootstep accepts a missing QuietMult through its constant default");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "VillagerEmitFootstep missing QuietMult uses valid pin addresses");
			CheckOneFootstepEmission(0.8f, "VillagerEmitFootstep finds exactly one missing-QuietMult emission");
			Check(xNode.GetFallbackUseCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep missing QuietMult has no named fallback");
			Check(xNode.GetMismatchWarningCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep missing QuietMult has no mismatch");
		}
		{
			Zenith_GraphBlackboard xBlackboard;
			xBlackboard.SetValue("quietLoudnessMult", IntValue(7));
			xFootstepContext.m_pxBlackboard = &xBlackboard;
			DPNode_VillagerEmitFootstep xNode;
			SetFootstepInputs(xNode, true);
			Zenith_AudioBus::ClearEmittedSoundsForTest();
			Check(xNode.Execute(xFootstepContext) == GRAPH_NODE_STATUS_SUCCESS, "VillagerEmitFootstep uses its constant QuietMult for a wrong-tag blackboard value");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "VillagerEmitFootstep wrong-tag blackboard uses valid pin addresses");
			CheckOneFootstepEmission(0.8f, "VillagerEmitFootstep finds exactly one wrong-blackboard-tag emission");
			Check(xNode.GetFallbackUseCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep wrong-tag blackboard QuietMult has no named fallback");
			Check(xNode.GetMismatchWarningCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep wrong-tag blackboard QuietMult has no mismatch diagnostic");
		}
		{
			Zenith_GraphBlackboard xBlackboard;
			xFootstepContext.m_pxBlackboard = &xBlackboard;
			DPNode_VillagerEmitFootstep xNode;
			SetFootstepInputs(xNode, true);
			xNode.SetInputForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult, IntValue(7));
			Zenith_AudioBus::ClearEmittedSoundsForTest();
			Check(xNode.Execute(xFootstepContext) == GRAPH_NODE_STATUS_SUCCESS, "VillagerEmitFootstep uses its constant QuietMult for a wrong-tag override");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "VillagerEmitFootstep wrong-tag override uses valid pin addresses");
			CheckOneFootstepEmission(0.8f, "VillagerEmitFootstep finds exactly one wrong-override-tag emission");
			Check(xNode.GetFallbackUseCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep wrong-tag override does not fall back");
			Check(xNode.GetMismatchWarningCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 1u, "VillagerEmitFootstep wrong-tag override emits one mismatch diagnostic");
		}
		{
			Zenith_GraphBlackboard xBlackboard;
			xFootstepContext.m_pxBlackboard = &xBlackboard;
			DPNode_VillagerEmitFootstep xNode;
			SetFootstepInputs(xNode, false);
			Zenith_AudioBus::ClearEmittedSoundsForTest();
			Check(xNode.Execute(xFootstepContext) == GRAPH_NODE_STATUS_SUCCESS, "VillagerEmitFootstep executes a non-quiet footstep");
			Check(xNode.GetBadAccessWarningCountForTest() == 0u, "VillagerEmitFootstep non-quiet leg uses valid pin addresses");
			CheckOneFootstepEmission(0.8f, "VillagerEmitFootstep finds exactly one non-quiet emission");
			Check(xNode.GetFallbackUseCountForTest(DPNode_VillagerEmitFootstep::uPIN_QuietMult) == 0u, "VillagerEmitFootstep non-quiet path leaves QuietMult unread");
		}

		Zenith_GraphBlackboard xEvaporationBlackboard;
		xEvaporationBlackboard.SetValue("tag", IntValue(static_cast<int32_t>(DP_ItemTag::Key)));
		Zenith_GraphContext xEvaporationContext;
		xEvaporationContext.m_pxBlackboard = &xEvaporationBlackboard;
		xEvaporationContext.m_xSelf = xEvaporationEntity;
		g_iEvaporatedEvents = 0;
		g_xEvaporatedItem = INVALID_ENTITY_ID;
		g_eEvaporatedTag = DP_ItemTag::None;
		g_xEvaporatedPosition = Zenith_Maths::Vector3(0.0f);
		const Zenith_EventHandle uEvaporationHandle =
			Zenith_EventDispatcher::Get().Subscribe<DP_OnItemEvaporated>(&OnItemEvaporated);
		DPNode_ItemEvaporate xEvaporate;
		xEvaporate.SetInputForTest(DPNode_ItemEvaporate::uPIN_Tag,
			IntValue(static_cast<int32_t>(DP_ItemTag::BogWater)));
		Check(xEvaporate.Execute(xEvaporationContext) == GRAPH_NODE_STATUS_SUCCESS, "ItemEvaporate succeeds for a real self entity");
		Check(xEvaporate.GetBadAccessWarningCountForTest() == 0u, "ItemEvaporate success uses valid pin addresses");
		Check(g_iEvaporatedEvents == 1 && g_xEvaporatedItem.GetPacked() == xEvaporatingItem.GetPacked()
			&& g_eEvaporatedTag == DP_ItemTag::BogWater, "ItemEvaporate dispatches its exact item and overridden BogWater tag");
		Check(g_xEvaporatedPosition.x == xEvaporationPosition.x
			&& g_xEvaporatedPosition.y == xEvaporationPosition.y
			&& g_xEvaporatedPosition.z == xEvaporationPosition.z, "ItemEvaporate dispatches the exact pre-destroy world position");
		Zenith_SceneData* pxEvaporationScene = xEvaporationEntity.GetSceneData();
		Check(pxEvaporationScene != nullptr
			&& IsPendingDestruction(xEvaporatingItem),
			"ItemEvaporate marks its exact self entity for deferred destruction");
		Check(xEvaporate.GetFallbackUseCountForTest(DPNode_ItemEvaporate::uPIN_Tag) == 0u, "ItemEvaporate overridden Tag has no fallback");

		DPNode_ItemEvaporate xInvalidEvaporate;
		Zenith_GraphContext xInvalidContext;
		xInvalidContext.m_pxBlackboard = &xEvaporationBlackboard;
		const int iEventsBeforeInvalid = g_iEvaporatedEvents;
		Check(xInvalidEvaporate.Execute(xInvalidContext) == GRAPH_NODE_STATUS_FAILURE, "ItemEvaporate invalid self fails before reading Tag");
		Check(g_iEvaporatedEvents == iEventsBeforeInvalid, "ItemEvaporate invalid self dispatches no evaporation event");
		Check(xInvalidEvaporate.GetFallbackUseCountForTest(DPNode_ItemEvaporate::uPIN_Tag) == 0u, "ItemEvaporate invalid self leaves Tag unread");
		Check(xInvalidEvaporate.GetBadAccessWarningCountForTest() == 0u, "ItemEvaporate invalid self touches no invalid pin address");
		Zenith_EventDispatcher::Get().Unsubscribe(uEvaporationHandle);
		DP_Items::Internal_UnregisterItemTag(xEvaporatingItem);
	}

	void CheckDispatchChestOpened()
	{
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_pxBlackboard = &xBlackboard;
		const Zenith_EventHandle uHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnChestOpened>(&OnChestOpened);
		const uint64_t ulOverride = 0x0000000400000004ull;

		DPNode_DispatchChestOpened xDistinct;
		xDistinct.SetInputForTest(DPNode_DispatchChestOpened::uPIN_Villager, EntityValue(ulOverride));
		Check(xDistinct.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "DispatchChestOpened executes its distinct Villager override");
		Check(g_iChestEvents == 1 && g_xLastChestVillager.GetPacked() == ulOverride, "DispatchChestOpened event carries the distinct pin override");
		Check(xDistinct.GetFallbackUseCountForTest(DPNode_DispatchChestOpened::uPIN_Villager) == 0u, "DispatchChestOpened distinct override has no fallback");
		Check(xDistinct.GetBadAccessWarningCountForTest() == 0u, "DispatchChestOpened distinct override uses a valid pin address");

		DPNode_DispatchChestOpened xZero;
		xZero.SetInputForTest(DPNode_DispatchChestOpened::uPIN_Villager, EntityValue(0ull));
		Check(xZero.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "DispatchChestOpened executes its packed-zero Villager override");
		Check(g_iChestEvents == 2 && g_xLastChestVillager.GetPacked() == 0ull, "DispatchChestOpened event preserves packed-zero payload");
		Check(xZero.GetFallbackUseCountForTest(DPNode_DispatchChestOpened::uPIN_Villager) == 0u, "DispatchChestOpened packed-zero override has no fallback");
		Check(xZero.GetBadAccessWarningCountForTest() == 0u, "DispatchChestOpened uses a valid pin address");
		Zenith_EventDispatcher::Get().Unsubscribe(uHandle);
	}

	void Setup_DPGraphNodePins()
	{
		// The local producer is appended to a process-lifetime registry.  Destroy
		// every live graph before that append so no graph retains stale registry
		// storage, then register once for all later witness executions.
		Zenith_SceneSystem::ResetWorldForNextTest();
		EnsureDPGraphArmOrderProducerRegistered();
		EnsureDPGraphCommitVillagerProducerRegistered();
		EnsureDPGraphFinishTagProducerRegistered();
		g_iChecks = 0;
		g_iFailures = 0;
		g_bRan = false;
		g_iChestEvents = 0;
		g_xLastChestVillager = INVALID_ENTITY_ID;
		g_iEvaporatedEvents = 0;
		g_xEvaporatedItem = INVALID_ENTITY_ID;
		g_eEvaporatedTag = DP_ItemTag::None;
		g_xEvaporatedPosition = Zenith_Maths::Vector3(0.0f);
	}

	bool Step_DPGraphNodePins(int /*iFrame*/)
	{
		CheckAllHelperBackedVillagerPins();
		CheckItemArmChannel();
		CheckItemArmChannelWireOrder();
		CheckItemCommitFinishWriterChain();
		CheckReadTuningFloat();
		CheckReadHeldObjectiveAndCommitPickup();
		CheckFootstepAndItemEvaporate();
		CheckDispatchChestOpened();
		g_bRan = true;
		return false;
	}

	bool Verify_DPGraphNodePins()
	{
		Check(g_bRan, "DP graph pin runtime checks ran");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphNodePins] %d checks, %d failed", g_iChecks, g_iFailures);
		return g_iChecks > 0 && g_iFailures == 0;
	}
}

static const Zenith_AutomatedTest g_xDPGraphNodePinsTest = {
	"DP_GraphNodePins_Test",
	&Setup_DPGraphNodePins,
	&Step_DPGraphNodePins,
	&Verify_DPGraphNodePins,
	10
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPGraphNodePinsTest);

namespace
{
	int g_iPriestPickChecks = 0;
	int g_iPriestPickFailures = 0;
	int g_iPriestPickWait = 0;
	int g_iPriestPickPhase = 0;
	bool g_bPriestPickRan = false;
	int g_iApprehendStarts = 0;
	Zenith_EntityID g_xApprehendVictim = INVALID_ENTITY_ID;
	void OnApprehendStart(const DP_OnApprehendChannelStart& xEvent)
	{
		++g_iApprehendStarts;
		g_xApprehendVictim = xEvent.m_xVictim;
	}

	void PriestPickCheck(bool bValue, const char* szWhat)
	{
		++g_iPriestPickChecks;
		if (!bValue)
		{
			++g_iPriestPickFailures;
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPriestPins] FAILED: %s", szWhat);
		}
	}

	float PriestPickDistanceXZ(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fX = xA.x - xB.x;
		const float fZ = xA.z - xB.z;
		return std::sqrt(fX * fX + fZ * fZ);
	}

	bool PriestPickGetPos(Zenith_EntityID xId, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxTransform == nullptr) return false;
		pxTransform->GetPosition(xOut);
		return true;
	}

	struct DPGraphCursorCandidate
	{
		Zenith_EntityID m_xId = INVALID_ENTITY_ID;
		double m_fX = 0.0;
		double m_fY = 0.0;
	};

	struct DPGraphCursorCollectContext
	{
		const Zenith_Maths::Matrix4* m_pxVP = nullptr;
		int32_t m_iW = 0;
		int32_t m_iH = 0;
		Zenith_Vector<DPGraphCursorCandidate>* m_paxCandidates = nullptr;
	};

	bool PriestPickProjectBodyCentre(const Zenith_Maths::Matrix4& xVP, int32_t iW, int32_t iH,
		const Zenith_Maths::Vector3& xWorld, double& fOutX, double& fOutY)
	{
		const Zenith_Maths::Vector4 xClip = xVP * Zenith_Maths::Vector4(xWorld.x, xWorld.y + 1.0f, xWorld.z, 1.0f);
		if (xClip.w <= 1e-4f) return false;
		const float fNdcX = xClip.x / xClip.w;
		const float fNdcY = xClip.y / xClip.w;
		fOutX = static_cast<double>((fNdcX + 1.0f) * 0.5f * static_cast<float>(iW));
		fOutY = static_cast<double>((fNdcY + 1.0f) * 0.5f * static_cast<float>(iH));
		return std::isfinite(fOutX) && std::isfinite(fOutY);
	}

	void CollectProjectedVillager(Zenith_EntityID xId, void* pUserData)
	{
		DPGraphCursorCollectContext& xCtx = *static_cast<DPGraphCursorCollectContext*>(pUserData);
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xId);
		Zenith_TransformComponent* pxTransform = xEntity.IsValid()
			? xEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		if (pxTransform == nullptr) return;
		Zenith_Maths::Vector3 xPosition;
		pxTransform->GetPosition(xPosition);
		double fX = 0.0, fY = 0.0;
		if (!PriestPickProjectBodyCentre(*xCtx.m_pxVP, xCtx.m_iW, xCtx.m_iH, xPosition, fX, fY)) return;
		xCtx.m_paxCandidates->PushBack({ xId, fX, fY });
	}

	void CheckPickVillagerUnderCursor()
	{
		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		int32_t iW = 0, iH = 0;
		if (pxWindow != nullptr) pxWindow->GetSize(iW, iH);
		PriestPickCheck(pxCamera != nullptr && pxWindow != nullptr && iW > 0 && iH > 0,
			"PickVillagerUnderCursor fixture has a camera and nonzero window");
		if (pxCamera == nullptr || pxWindow == nullptr || iW <= 0 || iH <= 0) return;

		Zenith_Maths::Matrix4 xView, xProjection;
		pxCamera->BuildViewMatrix(xView);
		pxCamera->BuildProjectionMatrix(xProjection);
		const Zenith_Maths::Matrix4 xVP = xProjection * xView;
		Zenith_Vector<DPGraphCursorCandidate> axCandidates;
		DPGraphCursorCollectContext xCollect = { &xVP, iW, iH, &axCandidates };
		DP_Player::ForEachVillagerInActiveScene(&CollectProjectedVillager, &xCollect);
		const u_int uCandidates = axCandidates.GetSize();
		PriestPickCheck(uCandidates > 0u, "PickVillagerUnderCursor finds finite projected eligible villagers");
		if (uCandidates == 0u) return;

		u_int uTarget = uCandidates;
		for (u_int u = 0u; u < uCandidates && uTarget == uCandidates; ++u)
		{
			bool bUnique = true;
			for (u_int v = 0u; v < uCandidates; ++v)
			{
				if (u == v) continue;
				const double fDX = axCandidates.Get(u).m_fX - axCandidates.Get(v).m_fX;
				const double fDY = axCandidates.Get(u).m_fY - axCandidates.Get(v).m_fY;
				if (fDX * fDX + fDY * fDY <= 1.0) { bUnique = false; break; }
			}
			if (bUnique) uTarget = u;
		}
		PriestPickCheck(uTarget < uCandidates, "PickVillagerUnderCursor has a uniquely projected target");
		if (uTarget == uCandidates) return;

		Zenith_Maths::Vector2_64 xSavedCursor;
		Zenith_InputSimulator::GetMousePositionSimulated(xSavedCursor);
		const DPGraphCursorCandidate& xTarget = axCandidates.Get(uTarget);
		Zenith_InputSimulator::SimulateMousePosition(xTarget.m_fX, xTarget.m_fY);
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_pxBlackboard = &xBlackboard;
		DPNode_PickVillagerUnderCursor xNode;
		xNode.m_strResultVar = "";
		PriestPickCheck(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS,
			"PickVillagerUnderCursor succeeds at a projected body centre");
		const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(DPNode_PickVillagerUnderCursor::uPIN_Result);
		PriestPickCheck(pxSlot && pxSlot->GetType() == PROPERTY_TYPE_ENTITY_ID && pxSlot->GetPackedEntityID() != 0ull
			&& pxSlot->GetPackedEntityID() == xTarget.m_xId.GetPacked(),
			"PickVillagerUnderCursor live Result slot carries the exact nonzero target");

		double fNoHitX = axCandidates.Get(0u).m_fX;
		for (u_int u = 1u; u < uCandidates; ++u) if (axCandidates.Get(u).m_fX > fNoHitX) fNoHitX = axCandidates.Get(u).m_fX;
		fNoHitX += 1000.0;
		bool bNoHitFar = std::isfinite(fNoHitX) && std::isfinite(xTarget.m_fY);
		for (u_int u = 0u; u < uCandidates; ++u)
		{
			const double fDX = fNoHitX - axCandidates.Get(u).m_fX;
			const double fDY = xTarget.m_fY - axCandidates.Get(u).m_fY;
			if (fDX * fDX + fDY * fDY <= 120.0 * 120.0) bNoHitFar = false;
		}
		PriestPickCheck(bNoHitFar, "PickVillagerUnderCursor no-hit point is farther than 120px from every finite eligible projection");
		if (bNoHitFar)
		{
			Zenith_InputSimulator::SimulateMousePosition(fNoHitX, xTarget.m_fY);
			PriestPickCheck(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE,
				"PickVillagerUnderCursor same instance fails at the verified no-hit point");
			pxSlot = xNode.GetOutputForTest(DPNode_PickVillagerUnderCursor::uPIN_Result);
			PriestPickCheck(pxSlot && pxSlot->GetType() == PROPERTY_TYPE_ENTITY_ID && pxSlot->GetPackedEntityID() == xTarget.m_xId.GetPacked(),
				"PickVillagerUnderCursor failure retains its exact prior Result slot");

			Zenith_GraphDefinition xFreshDefinition;
			const u_int uFreshID = xFreshDefinition.AddNode("DPPickVillagerUnderCursor");
			DPNode_PickVillagerUnderCursor xFreshParams;
			xFreshParams.m_strResultVar = "";
			const bool bFreshParams = uFreshID != 0u && xFreshDefinition.SetNodeParamsFromInstance(uFreshID, &xFreshParams);
			Zenith_BehaviourGraph xFreshGraph;
			const bool bFreshInit = bFreshParams && xFreshGraph.InitialiseFromDefinition(xFreshDefinition);
			PriestPickCheck(bFreshInit && xFreshGraph.GetResolutionSkipCountForTest() == 0u,
				"fresh PickVillagerUnderCursor graph initializes without skipped bindings");
			Zenith_GraphNode* pxFreshBase = bFreshInit ? xFreshGraph.FindNode(uFreshID) : nullptr;
			DPNode_PickVillagerUnderCursor* pxFresh = pxFreshBase != nullptr
				&& std::strcmp(pxFreshBase->GetTypeName(), "DPPickVillagerUnderCursor") == 0
				? static_cast<DPNode_PickVillagerUnderCursor*>(pxFreshBase) : nullptr;
			PriestPickCheck(pxFresh != nullptr, "fresh graph exposes PickVillagerUnderCursor");
			if (pxFresh != nullptr)
			{
				Zenith_GraphContext xFreshContext;
				xFreshContext.m_pxGraph = &xFreshGraph;
				xFreshContext.m_pxBlackboard = &xFreshGraph.GetBlackboard();
				PriestPickCheck(pxFresh->Execute(xFreshContext) == GRAPH_NODE_STATUS_FAILURE,
					"fresh PickVillagerUnderCursor fails at the verified no-hit point");
				const Zenith_PropertyValue* pxFreshSlot = pxFresh->GetOutputForTest(DPNode_PickVillagerUnderCursor::uPIN_Result);
				PriestPickCheck(pxFreshSlot && pxFreshSlot->GetType() == PROPERTY_TYPE_ENTITY_ID
					&& pxFreshSlot->GetPackedEntityID() == 0ull,
					"fresh PickVillagerUnderCursor failure has typed ENTITY_ID zero output");
				PriestPickCheck(pxFresh->GetBadAccessWarningCountForTest() == 0u,
					"fresh PickVillagerUnderCursor failure uses valid pin addresses");
			}
			xFreshGraph.Shutdown();
		}
		PriestPickCheck(xNode.GetBadAccessWarningCountForTest() == 0u,
			"PickVillagerUnderCursor output leg uses valid pin addresses");
		Zenith_InputSimulator::SimulateMousePosition(xSavedCursor.x, xSavedCursor.y);
	}

	void Setup_DPGraphPriestPickPins()
	{
		Zenith_SceneSystem::ResetWorldForNextTest();
		EnsureDPGraphArmOrderProducerRegistered();
		EnsureDPGraphCommitVillagerProducerRegistered();
		g_iPriestPickChecks = g_iPriestPickFailures = g_iPriestPickWait = 0;
		g_iPriestPickPhase = 0;
		g_bPriestPickRan = false;
		g_iApprehendStarts = 0;
		g_xApprehendVictim = INVALID_ENTITY_ID;
	}

	bool Step_DPGraphPriestPickPins(int /*iFrame*/)
	{
		if (g_iPriestPickPhase == 0)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPriestPins] fixture load BEGIN");
			g_xEngine.Scenes().LoadSceneByIndex(1, SCENE_LOAD_SINGLE);
			g_iPriestPickPhase = 1;
			return true;
		}

		Zenith_EntityID xPriest = INVALID_ENTITY_ID;
		Zenith_EntityID xWireTarget = INVALID_ENTITY_ID;
		Zenith_EntityID xBBTarget = INVALID_ENTITY_ID;
		DP_Query::ForEachComponentInActiveScene<Priest_Component>(
			[&xPriest](Zenith_EntityID xId, Priest_Component&) { if (!xPriest.IsValid()) xPriest = xId; });
		DP_Query::ForEachComponentInActiveScene<DPVillager_Component>(
			[&xWireTarget, &xBBTarget](Zenith_EntityID xId, DPVillager_Component&)
			{
				if (!xWireTarget.IsValid()) xWireTarget = xId;
				else if (!xBBTarget.IsValid() && xId.GetPacked() != xWireTarget.GetPacked()) xBBTarget = xId;
			});
		const Zenith_NavMesh* pxNavMesh = DP_AI::GetOrBuildLevelNavMesh();
		Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
		Zenith_Window* pxWindow = Zenith_Window::GetInstance();
		int32_t iWindowWidth = 0, iWindowHeight = 0;
		if (pxWindow != nullptr) pxWindow->GetSize(iWindowWidth, iWindowHeight);
		if (!xPriest.IsValid() || !xWireTarget.IsValid() || !xBBTarget.IsValid()
			|| DPPlayerController_Component::Instance() == nullptr || pxNavMesh == nullptr
			|| pxCamera == nullptr || pxWindow == nullptr || iWindowWidth <= 0 || iWindowHeight <= 0)
		{
			if (++g_iPriestPickWait < 120) return true;
			PriestPickCheck(false, "ProcLevel priest fixture timed out waiting for priest, two villagers, controller, navmesh, camera, and window");
			g_bPriestPickRan = true;
			return false;
		}

		Zenith_Maths::Vector3 xPriestPos, xWirePos, xBBPos;
		const bool bPositions = PriestPickGetPos(xPriest, xPriestPos)
			&& PriestPickGetPos(xWireTarget, xWirePos) && PriestPickGetPos(xBBTarget, xBBPos);
		PriestPickCheck(bPositions, "ProcLevel priest fixture resolves all required transforms");
		if (!bPositions) { g_bPriestPickRan = true; return false; }
		// P5's known reachable scent region is 40,1,40. Move the rival explicitly
		// so this proof never depends on arbitrary procgen spawn separation.
		Zenith_Entity xWireEntity = g_xEngine.Scenes().ResolveEntity(xWireTarget);
		Zenith_TransformComponent* pxWireTransform = xWireEntity.IsValid()
			? xWireEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		PriestPickCheck(pxWireTransform != nullptr, "wired scent villager has a Transform");
		if (pxWireTransform == nullptr) { g_bPriestPickRan = true; return false; }
		Zenith_Entity xBBEntity = g_xEngine.Scenes().ResolveEntity(xBBTarget);
		Zenith_TransformComponent* pxBBTransform = xBBEntity.IsValid()
			? xBBEntity.TryGetComponent<Zenith_TransformComponent>() : nullptr;
		PriestPickCheck(pxBBTransform != nullptr, "blackboard scent villager has a Transform");
		if (pxBBTransform == nullptr) { g_bPriestPickRan = true; return false; }
		CheckPickVillagerUnderCursor();
		xWirePos = Zenith_Maths::Vector3(40.0f, 1.0f, 40.0f);
		xBBPos = xWirePos + Zenith_Maths::Vector3(100.0f, 0.0f, 100.0f);
		pxWireTransform->SetPosition(xWirePos);
		pxBBTransform->SetPosition(xBBPos);
		PriestPickCheck(PriestPickDistanceXZ(xWirePos, xBBPos) > 30.0f, "wired and blackboard scent targets are separated beyond the wired patrol radius");
		if (PriestPickDistanceXZ(xWirePos, xBBPos) <= 30.0f)
		{
			g_bPriestPickRan = true;
			return false;
		}

		DPPlayerController_Component* pxController = DPPlayerController_Component::Instance();
		const float fThreshold = DP_Tuning::Get<float>("possession.demon_scent_hound_bark_threshold");
		const float fMax = DP_Tuning::Get<float>("possession.demon_scent_max");
		pxController->BumpDemonScent(xWireTarget, fThreshold + 1.0f, fMax);
		pxController->BumpDemonScent(xBBTarget, fThreshold + 1.0f, fMax);
		PriestPickCheck(DP_Player::GetDemonScent(xWireTarget) >= fThreshold
			&& DP_Player::GetDemonScent(xBBTarget) >= fThreshold, "both scent targets are above the diversion threshold");

		Zenith_Entity xPriestEntity = g_xEngine.Scenes().ResolveEntity(xPriest);
		Zenith_GraphBlackboard xBlackboard;
		Zenith_GraphContext xContext;
		xContext.m_xSelf = xPriestEntity;
		xContext.m_pxBlackboard = &xBlackboard;
		DPNode_PriestPickPatrolTarget xNode;
		xNode.m_strSuspicionRadiusVar = "";
		xNode.m_strHighScentTargetVar = "";
		xNode.m_strPatrolTargetVar = "";
		xNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_SuspicionRadius, FloatValue(15.0f));
		xNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget, EntityValue(xWireTarget.GetPacked()));
		PriestPickCheck(xNode.Execute(xContext) == GRAPH_NODE_STATUS_SUCCESS, "PriestPickPatrolTarget succeeds with wired radius and high-scent target");
		const Zenith_PropertyValue* pxSlot = xNode.GetOutputForTest(DPNode_PriestPickPatrolTarget::uPIN_PatrolTarget);
		PriestPickCheck(pxSlot && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3, "PriestPickPatrolTarget publishes a VECTOR3 output slot");
		if (pxSlot && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3)
		{
			const Zenith_Maths::Vector3 xResult = pxSlot->GetVector3();
			PriestPickCheck(PriestPickDistanceXZ(xResult, xWirePos) <= 15.5f
				&& PriestPickDistanceXZ(xResult, xBBPos) > 15.5f, "PriestPickPatrolTarget uses the wired scent region rather than the blackboard rival");
		}
		const Zenith_Maths::Vector3 xPriestPrior = pxSlot && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3
			? pxSlot->GetVector3() : Zenith_Maths::Vector3(0.0f);
		PriestPickCheck(xNode.GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_SuspicionRadius) == 0u
			&& xNode.GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget) == 0u, "PriestPickPatrolTarget wired inputs take no fallback");
		PriestPickCheck(xNode.GetBadAccessWarningCountForTest() == 0u, "PriestPickPatrolTarget uses valid pin addresses");

		EnsureDPGraphArmOrderProducerRegistered();
		EnsureDPGraphCommitVillagerProducerRegistered();
		Zenith_GraphDefinition xWireDefinition;
		const u_int uRadiusProducer = xWireDefinition.AddNode("DPTestArmOrderProducer");
		const u_int uScentProducer = xWireDefinition.AddNode("DPTestCommitVillagerProducer");
		const u_int uWirePicker = xWireDefinition.AddNode("DPPriestPickPatrolTarget");
		DPNode_PriestPickPatrolTarget xWireParams;
		xWireParams.m_strSuspicionRadiusVar = "";
		xWireParams.m_strHighScentTargetVar = "";
		xWireParams.m_strPatrolTargetVar = "";
		const bool bWireParams = uWirePicker != 0u && xWireDefinition.SetNodeParamsFromInstance(uWirePicker, &xWireParams);
		const bool bRadiusEdge = uRadiusProducer != 0u && uWirePicker != 0u && xWireDefinition.AddDataEdge(uRadiusProducer, "Value", uWirePicker, "SuspicionRadius");
		const bool bScentEdge = uScentProducer != 0u && uWirePicker != 0u && xWireDefinition.AddDataEdge(uScentProducer, "Value", uWirePicker, "HighScentTarget");
		Zenith_BehaviourGraph xWireGraph;
		const bool bWireInit = bWireParams && bRadiusEdge && bScentEdge && xWireGraph.InitialiseFromDefinition(xWireDefinition);
		PriestPickCheck(bWireInit && xWireGraph.GetResolutionSkipCountForTest() == 0u, "PriestPickPatrolTarget two-input wire graph initializes without resolution skips");
		Zenith_GraphNode* pxWireBase = bWireInit ? xWireGraph.FindNode(uWirePicker) : nullptr;
		DPNode_PriestPickPatrolTarget* pxWirePicker = pxWireBase && std::strcmp(pxWireBase->GetTypeName(), "DPPriestPickPatrolTarget") == 0
			? static_cast<DPNode_PriestPickPatrolTarget*>(pxWireBase) : nullptr;
		PriestPickCheck(pxWirePicker != nullptr, "PriestPickPatrolTarget two-input wire graph exposes picker");
		if (pxWirePicker)
		{
			Zenith_GraphContext xWireContext;
			xWireContext.m_xSelf = xPriestEntity;
			xWireContext.m_pxGraph = &xWireGraph;
			xWireContext.m_pxBlackboard = &xWireGraph.GetBlackboard();
			g_bArmOrderProducerForceValue = true;
			g_fArmOrderProducerValue = 15.0f;
			g_iArmOrderProducerPulls = 0;
			g_ulCommitProducerValue = xWireTarget.GetPacked();
			g_iCommitProducerPulls = 0;
			PriestPickCheck(pxWirePicker->Execute(xWireContext) == GRAPH_NODE_STATUS_SUCCESS, "PriestPickPatrolTarget succeeds through exact radius and scent wires");
			g_bArmOrderProducerForceValue = false;
			const Zenith_PropertyValue* pxWireSlot = pxWirePicker->GetOutputForTest(DPNode_PriestPickPatrolTarget::uPIN_PatrolTarget);
			PriestPickCheck(g_iArmOrderProducerPulls == 1 && g_iCommitProducerPulls == 1
				&& pxWireSlot && pxWireSlot->GetType() == PROPERTY_TYPE_VECTOR3
				&& PriestPickDistanceXZ(pxWireSlot->GetVector3(), xWirePos) <= 15.5f
				&& PriestPickDistanceXZ(pxWireSlot->GetVector3(), xBBPos) > 15.5f,
				"PriestPickPatrolTarget two-input wires preserve the distinct villager-centred region");
			PriestPickCheck(pxWirePicker->GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_SuspicionRadius) == 0u
				&& pxWirePicker->GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget) == 0u,
				"PriestPickPatrolTarget two-input wire graph has zero fallback");
		}
		g_bArmOrderProducerForceValue = false;
		xWireGraph.Shutdown();

		xContext.m_xSelf = Zenith_Entity();
		PriestPickCheck(xNode.Execute(xContext) == GRAPH_NODE_STATUS_FAILURE, "PriestPickPatrolTarget invalid self fails after a successful run");
		pxSlot = xNode.GetOutputForTest(DPNode_PriestPickPatrolTarget::uPIN_PatrolTarget);
		PriestPickCheck(pxSlot && pxSlot->GetType() == PROPERTY_TYPE_VECTOR3
			&& pxSlot->GetVector3().x == xPriestPrior.x && pxSlot->GetVector3().y == xPriestPrior.y && pxSlot->GetVector3().z == xPriestPrior.z,
			"PriestPickPatrolTarget invalid-self failure retains the exact prior slot");
		Zenith_GraphDefinition xFreshDefinition;
		const u_int uFreshID = xFreshDefinition.AddNode("DPPriestPickPatrolTarget");
		DPNode_PriestPickPatrolTarget xFreshParams;
		xFreshParams.m_strSuspicionRadiusVar = "";
		xFreshParams.m_strHighScentTargetVar = "";
		xFreshParams.m_strPatrolTargetVar = "";
		const bool bFreshParams = uFreshID != 0u && xFreshDefinition.SetNodeParamsFromInstance(uFreshID, &xFreshParams);
		Zenith_BehaviourGraph xFreshGraph;
		const bool bFreshInit = bFreshParams && xFreshGraph.InitialiseFromDefinition(xFreshDefinition);
		PriestPickCheck(bFreshInit, "fresh PriestPickPatrolTarget graph initializes");
		Zenith_GraphNode* pxFreshBase = bFreshInit ? xFreshGraph.FindNode(uFreshID) : nullptr;
		DPNode_PriestPickPatrolTarget* pxFresh = pxFreshBase && std::strcmp(pxFreshBase->GetTypeName(), "DPPriestPickPatrolTarget") == 0
			? static_cast<DPNode_PriestPickPatrolTarget*>(pxFreshBase) : nullptr;
		PriestPickCheck(pxFresh != nullptr, "fresh graph exposes PriestPickPatrolTarget");
		if (pxFresh)
		{
			Zenith_GraphContext xFreshContext;
			xFreshContext.m_pxGraph = &xFreshGraph;
			xFreshContext.m_pxBlackboard = &xFreshGraph.GetBlackboard();
			PriestPickCheck(pxFresh->Execute(xFreshContext) == GRAPH_NODE_STATUS_FAILURE, "fresh PriestPickPatrolTarget invalid self fails");
			const Zenith_PropertyValue* pxFreshOutput = pxFresh->GetOutputForTest(DPNode_PriestPickPatrolTarget::uPIN_PatrolTarget);
			PriestPickCheck(pxFreshOutput && pxFreshOutput->GetType() == PROPERTY_TYPE_VECTOR3
				&& pxFreshOutput->GetVector3().x == 0.0f && pxFreshOutput->GetVector3().y == 0.0f && pxFreshOutput->GetVector3().z == 0.0f,
				"fresh graph invalid-self failure has typed VECTOR3 zero output");
			PriestPickCheck(pxFresh->GetBadAccessWarningCountForTest() == 0u, "fresh PriestPickPatrolTarget failure uses valid pin addresses");
		}
		xFreshGraph.Shutdown();
		PriestPickCheck(xNode.GetBadAccessWarningCountForTest() == 0u, "PriestPickPatrolTarget output leg uses valid pin addresses");

		auto CheckHighScentZero = [&](int iCase, const char* szWhat)
		{
			Zenith_GraphBlackboard xBB;
			Zenith_GraphContext xCase;
			xCase.m_xSelf = xPriestEntity;
			xCase.m_pxBlackboard = &xBB;
			DPNode_PriestPickPatrolTarget xCaseNode;
			xCaseNode.m_strSuspicionRadiusVar = "";
			xCaseNode.m_strHighScentTargetVar = "";
			xCaseNode.m_strPatrolTargetVar = "";
			xCaseNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_SuspicionRadius, FloatValue(15.0f));
			if (iCase == 0) xCaseNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget, EntityValue(INVALID_ENTITY_ID.GetPacked()));
			if (iCase == 1) xCaseNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget, FloatValue(1.0f));
			if (iCase == 2) xCaseNode.SetInputForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget, EntityValue(0ull));
			PriestPickCheck(xCaseNode.Execute(xCase) == GRAPH_NODE_STATUS_SUCCESS, szWhat);
			const Zenith_PropertyValue* pxCase = xCaseNode.GetOutputForTest(DPNode_PriestPickPatrolTarget::uPIN_PatrolTarget);
			PriestPickCheck(pxCase && pxCase->GetType() == PROPERTY_TYPE_VECTOR3
				&& PriestPickDistanceXZ(pxCase->GetVector3(), xPriestPos) <= 15.5f, "PriestPickPatrolTarget invalid/zero HighScent stays priest-centred");
			PriestPickCheck(xCaseNode.GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_SuspicionRadius) == 0u
				&& xCaseNode.GetFallbackUseCountForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget) == 0u
				&& xCaseNode.GetMismatchWarningCountForTest(DPNode_PriestPickPatrolTarget::uPIN_HighScentTarget) == (iCase == 1 ? 1u : 0u)
				&& xCaseNode.GetBadAccessWarningCountForTest() == 0u, "PriestPickPatrolTarget explicit HighScent cases have no fallback or BADACCESS");
		};
		CheckHighScentZero(0, "PriestPickPatrolTarget explicit INVALID HighScent is an invalid entity input");
		CheckHighScentZero(1, "PriestPickPatrolTarget explicit wrong-tag HighScent is an invalid entity input");
		CheckHighScentZero(2, "PriestPickPatrolTarget packed-zero HighScent is an invalid entity input");

		// Apprehend reads only after OnEnter and its valid-self guard. Keep dt zero
		// so the positive leg can prove Start/RUNNING without completing a capture.
		Zenith_Maths::Vector3 xNearPriest;
		PriestPickCheck(PriestPickGetPos(xPriest, xNearPriest), "PriestApprehend resolves priest position");
		pxWireTransform->SetPosition(xNearPriest);
		Zenith_GraphBlackboard xApprehendBB;
		xApprehendBB.SetValue(DP_AI::BB_KEY_TARGET_WITH_DEVIL, EntityValue(xBBTarget.GetPacked()));
		Zenith_GraphContext xApprehendContext;
		xApprehendContext.m_xSelf = xPriestEntity;
		xApprehendContext.m_pxBlackboard = &xApprehendBB;
		xApprehendContext.m_fDt = 0.0f;
		g_iApprehendStarts = 0;
		g_xApprehendVictim = INVALID_ENTITY_ID;
		const Zenith_EventHandle uStartHandle = Zenith_EventDispatcher::Get().Subscribe<DP_OnApprehendChannelStart>(&OnApprehendStart);
		DPNode_PriestApprehendChannel xApprehend;
		xApprehend.m_strTargetWithDevilVar = "";
		xApprehend.OnEnter(xApprehendContext);
		xApprehend.SetInputForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil, EntityValue(xWireTarget.GetPacked()));
		PriestPickCheck(xApprehend.Execute(xApprehendContext) == GRAPH_NODE_STATUS_RUNNING, "PriestApprehend valid wired target starts and remains RUNNING at dt zero");
		PriestPickCheck(g_iApprehendStarts == 1 && g_xApprehendVictim.GetPacked() == xWireTarget.GetPacked(), "PriestApprehend Start event carries the wired target over blackboard target");
		PriestPickCheck(xApprehend.GetFallbackUseCountForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil) == 0u
			&& xApprehend.GetBadAccessWarningCountForTest() == 0u, "PriestApprehend wired target has no fallback or BADACCESS");

		auto CheckApprehendZero = [&](int iCase, const char* szWhat)
		{
			Zenith_GraphBlackboard xBB;
			Zenith_GraphContext xCase;
			xCase.m_xSelf = xPriestEntity;
			xCase.m_pxBlackboard = &xBB;
			xCase.m_fDt = 0.0f;
			DPNode_PriestApprehendChannel xCaseNode;
			xCaseNode.m_strTargetWithDevilVar = "";
			if (iCase == 0) xCaseNode.SetInputForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil, EntityValue(INVALID_ENTITY_ID.GetPacked()));
			if (iCase == 1) xCaseNode.SetInputForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil, FloatValue(1.0f));
			if (iCase == 2) xCaseNode.SetInputForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil, EntityValue(0ull));
			xCaseNode.OnEnter(xCase);
			const GraphNodeStatus eCaseStatus = xCaseNode.Execute(xCase);
			if (iCase != 2) PriestPickCheck(eCaseStatus == GRAPH_NODE_STATUS_FAILURE, "PriestApprehend missing/wrong target reaches its failure path");
			else PriestPickCheck(eCaseStatus == GRAPH_NODE_STATUS_FAILURE || eCaseStatus == GRAPH_NODE_STATUS_RUNNING || eCaseStatus == GRAPH_NODE_STATUS_SUCCESS, "PriestApprehend packed-zero target reaches the actual execute path");
			PriestPickCheck(xCaseNode.GetInputPackedEntityID(xCase, DPNode_PriestApprehendChannel::uPIN_TargetWithDevil)
				== (iCase == 0 ? INVALID_ENTITY_ID.GetPacked() : 0ull), szWhat);
			PriestPickCheck(xCaseNode.GetFallbackUseCountForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil) == 0u
				&& xCaseNode.GetMismatchWarningCountForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil) == (iCase == 1 ? 1u : 0u)
				&& xCaseNode.GetBadAccessWarningCountForTest() == 0u, "PriestApprehend explicit zero-family has no fallback or BADACCESS");
		};
		CheckApprehendZero(0, "PriestApprehend missing TargetWithDevil reads packed zero");
		CheckApprehendZero(1, "PriestApprehend wrong-tag blackboard TargetWithDevil reads packed zero");
		CheckApprehendZero(2, "PriestApprehend packed-zero TargetWithDevil reads packed zero");
		DPNode_PriestApprehendChannel xEarlyInvalid;
		xEarlyInvalid.m_strTargetWithDevilVar = "";
		Zenith_GraphContext xEarlyContext;
		xEarlyContext.m_pxBlackboard = &xApprehendBB;
		xEarlyInvalid.OnEnter(xEarlyContext);
		PriestPickCheck(xEarlyInvalid.Execute(xEarlyContext) == GRAPH_NODE_STATUS_FAILURE
			&& xEarlyInvalid.GetFallbackUseCountForTest(DPNode_PriestApprehendChannel::uPIN_TargetWithDevil) == 0u
			&& xEarlyInvalid.GetBadAccessWarningCountForTest() == 0u, "PriestApprehend invalid self leaves TargetWithDevil unread");
		Zenith_EventDispatcher::Get().Unsubscribe(uStartHandle);
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPriestPins] fixture load END");
		g_bPriestPickRan = true;
		return false;
	}

	bool Verify_DPGraphPriestPickPins()
	{
		PriestPickCheck(g_bPriestPickRan, "DP graph priest fixture completed");
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[DPGraphPriestPins] %d checks, %d failed", g_iPriestPickChecks, g_iPriestPickFailures);
		return g_iPriestPickChecks > 0 && g_iPriestPickFailures == 0;
	}
}

static const Zenith_AutomatedTest g_xDPGraphPriestPickPinsTest = {
	"DP_GraphPriestPickPins_Test", &Setup_DPGraphPriestPickPins,
	&Step_DPGraphPriestPickPins, &Verify_DPGraphPriestPickPins, 180
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xDPGraphPriestPickPinsTest);

#endif // ZENITH_INPUT_SIMULATOR
