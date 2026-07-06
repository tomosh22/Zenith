//------------------------------------------------------------------------------
// Zenith_GraphComponent integration tests (Phase 1 of the Behaviour Graphs
// program): asset round-trip, lifecycle dispatch through the component-meta
// registry, collision dispatch generalization, serialization, and the
// 1000-entity benchmark. Included at the bottom of Zenith_GraphComponent.cpp.
//------------------------------------------------------------------------------

#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "UnitTests/Zenith_TempScene.h"
#include "UnitTests/Zenith_UnitTests.h"
#include "Input/Zenith_KeyCodes.h"
#ifdef ZENITH_INPUT_SIMULATOR
#include "Input/Zenith_InputSimulator.h"
#endif

// P2 node-library coverage fixtures (physics / animation / UI / AI / entity).
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_TweenComponent.h"
#include "EntityComponent/Components/Zenith_ParticleEmitterComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "UI/Zenith_UIText.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIButton.h"
#include "Physics/Zenith_Physics.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "Prefab/Zenith_Prefab.h"
#include "Flux/MeshAnimation/Flux_AnimationStateMachine.h"
#include "Flux/MeshAnimation/Flux_BonePose.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Core/Zenith_Tween.h"

#include <chrono>
#include <filesystem>
#include <cstring>

namespace
{
	// Builds + saves a minimal "OnUpdate -> RotateEntity(90 deg/s)" graph asset
	// and returns its asset path. Idempotent per leaf name.
	std::string SaveRotateGraphAsset(const char* szLeafName, float fDegreesPerSecond)
	{
		const std::string strAssetPath = std::string("game:Graphs/") + szLeafName;

		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);

		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uRotate = xDef.AddNode("RotateEntity");
		{
			// Configure the rotate rate through the param-blob path (the same
			// route the editor uses).
			const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find("RotateEntity");
			Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
			Zenith_PropertyValue xValue;
			xValue.SetFloat(fDegreesPerSecond);
			Zenith_PropertySystem::SetPropertyValue(pxTemp, *pxInfo->m_pfnGetPropertyTable()->FindProperty("m_fDegreesPerSecond"), xValue);
			xDef.SetNodeParamsFromInstance(uRotate, pxTemp);
			delete pxTemp;
		}
		xDef.AddEdge(uSource, 0, uRotate, 0);

		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
		return strAssetPath;
	}

	float GetEntityForwardX(Zenith_Entity& xEntity)
	{
		Zenith_Maths::Quat xRotation;
		xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
		const Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		return xForward.x;
	}

	// Multi-param node configuration through the same param-blob path the
	// editor uses: Set() properties on a temp instance, commit on destruction.
	struct NodeParamWriter
	{
		NodeParamWriter(Zenith_GraphDefinition& xDef, u_int uNodeID, const char* szTypeName)
			: m_xDef(xDef)
			, m_uNodeID(uNodeID)
			, m_pxInfo(Zenith_GraphNodeRegistry::Get().Find(szTypeName))
			, m_pxTemp(m_pxInfo ? m_pxInfo->m_pfnCreate() : nullptr)
		{
		}
		~NodeParamWriter()
		{
			if (m_pxTemp)
			{
				m_xDef.SetNodeParamsFromInstance(m_uNodeID, m_pxTemp);
				delete m_pxTemp;
			}
		}
		void Set(const char* szProperty, const Zenith_PropertyValue& xValue)
		{
			if (m_pxTemp)
			{
				Zenith_PropertySystem::SetPropertyValue(m_pxTemp, *m_pxInfo->m_pfnGetPropertyTable()->FindProperty(szProperty), xValue);
			}
		}
		void SetString(const char* szProperty, const char* szValue)
		{
			Zenith_PropertyValue xValue;
			xValue.SetString(szValue);
			Set(szProperty, xValue);
		}
		void SetInt(const char* szProperty, int32_t iValue)
		{
			Zenith_PropertyValue xValue;
			xValue.SetInt32(iValue);
			Set(szProperty, xValue);
		}

		Zenith_GraphDefinition& m_xDef;
		u_int m_uNodeID;
		const Zenith_GraphNodeTypeInfo* m_pxInfo;
		Zenith_GraphNode* m_pxTemp;
	};
}

ZENITH_TEST(GraphComponent, AssetRoundTripAndUpdateDispatch)
{
	const std::string strAssetPath = SaveRotateGraphAsset("UnitTest_Rotate90.bgraph", 90.0f);

	Zenith_TempScene xTempScene("TestGraphComponentScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphRotator");

	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	// Dispatch THROUGH the component-meta registry (the real engine path - the
	// GraphComponent's OnUpdate hook is concept-detected like any component's).
	const Zenith_ComponentMeta* pxMeta = Zenith_ComponentMetaRegistry::Get().GetMetaByName("Graph");
	ZENITH_ASSERT_NOT_NULL(pxMeta);
	ZENITH_ASSERT_NOT_NULL(reinterpret_cast<const void*>(pxMeta->m_pfnOnUpdate));

	// One simulated second at 90 deg/s: forward (0,0,1) -> (1,0,0).
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(GetEntityForwardX(xEntity), 1.0f, 0.01f);
}

ZENITH_TEST(GraphComponent, SerializationRoundTripWithOverridesAndUnresolved)
{
	const std::string strAssetPath = SaveRotateGraphAsset("UnitTest_RotateSer.bgraph", 45.0f);

	Zenith_TempScene xTempScene("TestGraphSerScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphSerSource");
	Zenith_GraphComponent& xSourceComponent = xSource.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xSourceComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;

	// Per-entity blackboard override that must survive the round trip.
	Zenith_PropertyValue xOverride;
	xOverride.SetFloat(123.5f);
	pxGraph->GetBlackboard().SetValue("tweaked", xOverride);

	// Second slot pointing at a MISSING asset - must round-trip unresolved.
	xSourceComponent.AddGraphByAssetPath("game:Graphs/DoesNotExist_UnitTest.bgraph");
	ZENITH_ASSERT_EQ(xSourceComponent.GetGraphCount(), 2u);
	ZENITH_ASSERT_NULL(xSourceComponent.GetGraphAt(1));

	Zenith_DataStream xStream;
	xSourceComponent.WriteToDataStream(xStream);
	const u_int uSentinel = 0xFEEDBEEFu;
	xStream << uSentinel;

	Zenith_Entity xDest = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphSerDest");
	Zenith_GraphComponent& xDestComponent = xDest.AddComponent<Zenith_GraphComponent>();
	xStream.SetCursor(0);
	xDestComponent.ReadFromDataStream(xStream);
	u_int uReadSentinel = 0;
	xStream >> uReadSentinel;
	ZENITH_ASSERT_EQ(uReadSentinel, 0xFEEDBEEFu);

	ZENITH_ASSERT_EQ(xDestComponent.GetGraphCount(), 2u);
	if (xDestComponent.GetGraphCount() != 2u) return;
	ZENITH_ASSERT_NOT_NULL(xDestComponent.GetGraphAt(0));
	if (!xDestComponent.GetGraphAt(0)) return;
	ZENITH_ASSERT_EQ_FLOAT(xDestComponent.GetGraphAt(0)->GetBlackboard().GetFloat("tweaked"), 123.5f, 0.0001f);
	ZENITH_ASSERT_NULL(xDestComponent.GetGraphAt(1));
	ZENITH_ASSERT_STREQ(xDestComponent.GetGraphAssetPathAt(1), "game:Graphs/DoesNotExist_UnitTest.bgraph");
}

ZENITH_TEST(GraphComponent, CollisionDispatchViaMetaRegistry)
{
	// The graph component exposes collision hooks through the registry -
	// physics dispatches by meta, never by naming a concrete component.
	const Zenith_ComponentMeta* pxGraphMeta = Zenith_ComponentMetaRegistry::Get().GetMetaByName("Graph");
	ZENITH_ASSERT_NOT_NULL(pxGraphMeta);
	ZENITH_ASSERT_NOT_NULL(reinterpret_cast<const void*>(pxGraphMeta->m_pfnOnCollisionEnter));
	ZENITH_ASSERT_NOT_NULL(reinterpret_cast<const void*>(pxGraphMeta->m_pfnOnCollisionStay));
	ZENITH_ASSERT_NOT_NULL(reinterpret_cast<const void*>(pxGraphMeta->m_pfnOnCollisionExit));

	// Functional: an OnCollisionEnter graph chain runs and captures the other
	// entity when the registry dispatches.
	Zenith_TempScene xTempScene("TestGraphCollisionScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphCollider");
	Zenith_Entity xOther = g_xEngine.Scenes().CreateEntity(pxSceneData, "GraphOther");

	const std::string strAssetPath = std::string("game:Graphs/UnitTest_Collision.bgraph");
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnCollisionEnter");
		const u_int uFlag = xDef.AddNode("SetBlackboardBool");
		{
			const Zenith_GraphNodeTypeInfo* pxInfo = Zenith_GraphNodeRegistry::Get().Find("SetBlackboardBool");
			Zenith_GraphNode* pxTemp = pxInfo->m_pfnCreate();
			Zenith_PropertyValue xName;
			xName.SetString("hit");
			Zenith_PropertySystem::SetPropertyValue(pxTemp, *pxInfo->m_pfnGetPropertyTable()->FindProperty("m_strVariable"), xName);
			xDef.SetNodeParamsFromInstance(uFlag, pxTemp);
			delete pxTemp;
		}
		xDef.AddEdge(uSource, 0, uFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;

	Zenith_ComponentMetaRegistry::Get().DispatchOnCollisionEnter(xEntity, xOther);

	ZENITH_ASSERT_TRUE(pxGraph->GetBlackboard().GetBool("hit"));
	const Zenith_PropertyValue* pxStored = pxGraph->GetBlackboard().TryGetValue("other");
	ZENITH_ASSERT_NOT_NULL(pxStored);
	if (!pxStored) return;
	ZENITH_ASSERT_EQ(pxStored->GetPackedEntityID(), xOther.GetEntityID().GetPacked());
}

ZENITH_TEST(GraphComponent, BlackboardNodeFamilyExecution)
{
	// The P1a blackboard node family (SetBlackboardInt / SetBlackboardVector3 /
	// SetBlackboardString / CompareBlackboardInt / StoreSelfEntityID) executing
	// through the engine registrar in one OnUpdate chain.
	const std::string strAssetPath = "game:Graphs/UnitTest_BlackboardNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSetInt = xDef.AddNode("SetBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uSetInt, "SetBlackboardInt");
			xParams.SetString("m_strVariable", "i");
			xParams.SetInt("m_iValue", 5);
		}
		const u_int uCompareConst = xDef.AddNode("CompareBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uCompareConst, "CompareBlackboardInt");
			xParams.SetString("m_strVar", "i");
			xParams.SetInt("m_iCompareTo", 5);
			xParams.SetInt("m_iOp", 4);	// equal
			xParams.SetString("m_strResultVar", "eq");
		}
		const u_int uCompareVar = xDef.AddNode("CompareBlackboardInt");
		{
			// var-vs-var: i != i must be false.
			NodeParamWriter xParams(xDef, uCompareVar, "CompareBlackboardInt");
			xParams.SetString("m_strVar", "i");
			xParams.SetString("m_strCompareVar", "i");
			xParams.SetInt("m_iOp", 5);	// notEqual
			xParams.SetString("m_strResultVar", "neq");
		}
		const u_int uSetVec = xDef.AddNode("SetBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uSetVec, "SetBlackboardVector3");
			xParams.SetString("m_strVariable", "v");
			Zenith_PropertyValue xVec;
			xVec.SetVector3(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
			xParams.Set("m_xValue", xVec);
		}
		const u_int uSetStr = xDef.AddNode("SetBlackboardString");
		{
			NodeParamWriter xParams(xDef, uSetStr, "SetBlackboardString");
			xParams.SetString("m_strVariable", "s");
			xParams.SetString("m_strValue", "hello");
		}
		const u_int uStoreSelf = xDef.AddNode("StoreSelfEntityID");
		{
			NodeParamWriter xParams(xDef, uStoreSelf, "StoreSelfEntityID");
			xParams.SetString("m_strVariable", "me");
		}

		xDef.AddEdge(uSource, 0, uSetInt, 0);
		xDef.AddEdge(uSetInt, 0, uCompareConst, 0);
		xDef.AddEdge(uCompareConst, 0, uCompareVar, 0);
		xDef.AddEdge(uCompareVar, 0, uSetVec, 0);
		xDef.AddEdge(uSetVec, 0, uSetStr, 0);
		xDef.AddEdge(uSetStr, 0, uStoreSelf, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphBlackboardNodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "BlackboardNodes");
	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);

	const Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("i"), 5);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("eq"));
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("neq", true));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("v").y, 2.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("s").c_str(), "hello");
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("me"), xEntity.GetEntityID().GetPacked());
}

namespace
{
	// Saves a graph asset: OnCustomEvent(szListenName) -> AddBlackboardFloat
	// (counter szCounter) [-> optional FireCustomEvent(szFireName) at var
	// "peer"]. The building block for the cross-entity event tests.
	std::string SaveEventRelayGraphAsset(const char* szLeafName, const char* szListenName, const char* szCounter, const char* szFireName)
	{
		const std::string strAssetPath = std::string("game:Graphs/") + szLeafName;
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);

		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", szListenName);
		}
		const u_int uCounter = xDef.AddNode("AddBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uCounter, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", szCounter);
		}
		xDef.AddEdge(uSource, 0, uCounter, 0);
		if (szFireName != nullptr)
		{
			const u_int uFire = xDef.AddNode("FireCustomEvent");
			{
				NodeParamWriter xParams(xDef, uFire, "FireCustomEvent");
				xParams.SetString("m_strEventName", szFireName);
				xParams.SetString("m_strTargetVar", "peer");
			}
			xDef.AddEdge(uCounter, 0, uFire, 0);
		}
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
		return strAssetPath;
	}

	void StorePeer(Zenith_BehaviourGraph* pxGraph, const Zenith_Entity& xPeer)
	{
		Zenith_PropertyValue xID;
		xID.SetPackedEntityID(xPeer.GetEntityID().GetPacked());
		pxGraph->GetBlackboard().SetValue("peer", xID);
	}
}

ZENITH_TEST(GraphComponent, CrossEntityEventsTargetingBroadcastAndArgs)
{
	Zenith_TempScene xTempScene("TestGraphCrossEventScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xA = g_xEngine.Scenes().CreateEntity(pxSceneData, "EventA");
	Zenith_Entity xB = g_xEngine.Scenes().CreateEntity(pxSceneData, "EventB");

	// A relays "Ping" -> fires "Pong" at its peer; B just counts "Pong".
	const std::string strRelayPath = SaveEventRelayGraphAsset("UnitTest_EventRelay.bgraph", "Ping", "pings", "Pong");
	const std::string strCountPath = SaveEventRelayGraphAsset("UnitTest_EventCount.bgraph", "Pong", "pongs", nullptr);

	Zenith_BehaviourGraph* pxGraphA = xA.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strRelayPath.c_str());
	Zenith_BehaviourGraph* pxGraphB = xB.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strCountPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraphA);
	ZENITH_ASSERT_NOT_NULL(pxGraphB);
	if (!pxGraphA || !pxGraphB) return;
	StorePeer(pxGraphA, xB);

	// Targeted delivery: A's relay chain runs, B's counter increments, A has
	// no "pongs" var of its own.
	xA.GetComponent<Zenith_GraphComponent>().FireCustomEvent("Ping");
	ZENITH_ASSERT_EQ_FLOAT(pxGraphA->GetBlackboard().GetFloat("pings"), 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(pxGraphB->GetBlackboard().GetFloat("pongs"), 1.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(pxGraphA->GetBlackboard().HasValue("pongs"));

	// Broadcast: reaches every GraphComponent (B counts another Pong; A's
	// graph doesn't listen for Pong so only its absence is asserted).
	Zenith_GraphComponent::BroadcastCustomEvent("Pong");
	ZENITH_ASSERT_EQ_FLOAT(pxGraphB->GetBlackboard().GetFloat("pongs"), 2.0f, 0.0001f);

	// Named args: every arg lands on the receiver's blackboard verbatim, and
	// arg 0 doubles as the legacy payload (default "payload" stash var).
	Zenith_GraphEventArg axArgs[3];
	axArgs[0].m_strName = "damage";
	axArgs[0].m_xValue.SetFloat(25.0f);
	axArgs[1].m_strName = "attacker";
	axArgs[1].m_xValue.SetPackedEntityID(xA.GetEntityID().GetPacked());
	axArgs[2].m_strName = "hitPos";
	axArgs[2].m_xValue.SetVector3(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xB.GetComponent<Zenith_GraphComponent>().FireCustomEventWithArgs("Pong", axArgs, 3);

	const Zenith_GraphBlackboard& xBlackboardB = pxGraphB->GetBlackboard();
	ZENITH_ASSERT_EQ_FLOAT(xBlackboardB.GetFloat("pongs"), 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboardB.GetFloat("damage"), 25.0f, 0.0001f);
	ZENITH_ASSERT_EQ(xBlackboardB.GetPackedEntityID("attacker"), xA.GetEntityID().GetPacked());
	ZENITH_ASSERT_EQ_FLOAT(xBlackboardB.GetVector3("hitPos").z, 3.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboardB.GetFloat("payload"), 25.0f, 0.0001f);	// legacy stash = arg 0
}

ZENITH_TEST(GraphComponent, EventPingPongTerminatesAtDepthCap)
{
	Zenith_TempScene xTempScene("TestGraphPingPongScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xA = g_xEngine.Scenes().CreateEntity(pxSceneData, "PingPongA");
	Zenith_Entity xB = g_xEngine.Scenes().CreateEntity(pxSceneData, "PingPongB");

	// A: Ping -> fire Pong at peer. B: Pong -> fire Ping at peer. Mutual
	// recursion that only the dispatch-depth cap terminates.
	const std::string strAPath = SaveEventRelayGraphAsset("UnitTest_PingPongA.bgraph", "Ping", "pings", "Pong");
	const std::string strBPath = SaveEventRelayGraphAsset("UnitTest_PingPongB.bgraph", "Pong", "pongs", "Ping");

	Zenith_BehaviourGraph* pxGraphA = xA.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAPath.c_str());
	Zenith_BehaviourGraph* pxGraphB = xB.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strBPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraphA);
	ZENITH_ASSERT_NOT_NULL(pxGraphB);
	if (!pxGraphA || !pxGraphB) return;
	StorePeer(pxGraphA, xB);
	StorePeer(pxGraphB, xA);

	// Must terminate (cap logs + drops) with the engine intact.
	xA.GetComponent<Zenith_GraphComponent>().FireCustomEvent("Ping");

	const float fPings = pxGraphA->GetBlackboard().GetFloat("pings");
	ZENITH_ASSERT_TRUE(fPings >= 1.0f && fPings <= 16.0f, "ping-pong ran %f rounds", fPings);

	// The dispatcher is healthy afterwards: a plain event still delivers.
	xA.GetComponent<Zenith_GraphComponent>().FireCustomEvent("Ping");
	ZENITH_ASSERT_TRUE(pxGraphA->GetBlackboard().GetFloat("pings") > fPings);
}

ZENITH_TEST(GraphComponent, EntityTargetingActsOnOtherEntity)
{
	// P1b: a chain on entity A stores B's ID, then acts on B through the
	// m_strTargetVar convention (translate) and queries it (position/distance).
	const std::string strAssetPath = "game:Graphs/UnitTest_EntityTargeting.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// The "other" var is written by the test before dispatch (the shim
		// pre-stage pattern); the chain moves the target up 2 units/s and reads
		// back its position + the self->target distance.
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uTranslate = xDef.AddNode("TranslateEntity");
		{
			NodeParamWriter xParams(xDef, uTranslate, "TranslateEntity");
			Zenith_PropertyValue xVec;
			xVec.SetVector3(Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
			xParams.Set("m_xUnitsPerSecond", xVec);
			xParams.SetString("m_strTargetVar", "other");
		}
		const u_int uReadPos = xDef.AddNode("ReadEntityPosition");
		{
			NodeParamWriter xParams(xDef, uReadPos, "ReadEntityPosition");
			xParams.SetString("m_strTargetVar", "other");
			xParams.SetString("m_strResultVar", "otherPos");
		}
		const u_int uDistance = xDef.AddNode("ComputeDistance");
		{
			NodeParamWriter xParams(xDef, uDistance, "ComputeDistance");
			xParams.SetString("m_strToVar", "other");
			xParams.SetString("m_strResultVar", "dist");
		}
		const u_int uValid = xDef.AddNode("QueryEntityValid");
		{
			NodeParamWriter xParams(xDef, uValid, "QueryEntityValid");
			xParams.SetString("m_strEntityVar", "other");
			xParams.SetString("m_strResultVar", "otherAlive");
		}
		xDef.AddEdge(uSource, 0, uTranslate, 0);
		xDef.AddEdge(uTranslate, 0, uReadPos, 0);
		xDef.AddEdge(uReadPos, 0, uDistance, 0);
		xDef.AddEdge(uDistance, 0, uValid, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphEntityTargetScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xHost = g_xEngine.Scenes().CreateEntity(pxSceneData, "TargetingHost");
	Zenith_Entity xOther = g_xEngine.Scenes().CreateEntity(pxSceneData, "TargetingOther");
	xOther.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));

	Zenith_GraphComponent& xComponent = xHost.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	Zenith_PropertyValue xOtherID;
	xOtherID.SetPackedEntityID(xOther.GetEntityID().GetPacked());
	pxGraph->GetBlackboard().SetValue("other", xOtherID);

	// One simulated second: B rises 2 units; A stays put at the origin.
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xHost, 1.0f);

	Zenith_Maths::Vector3 xOtherPosition;
	xOther.GetComponent<Zenith_TransformComponent>().GetPosition(xOtherPosition);
	ZENITH_ASSERT_EQ_FLOAT(xOtherPosition.y, 2.0f, 0.001f);

	const Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("otherPos").y, 2.0f, 0.001f);
	// |(3, 2, 4)| after the move.
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("dist"), glm::length(Zenith_Maths::Vector3(3.0f, 2.0f, 4.0f)), 0.001f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("otherAlive"));

	// Destroy B: targeting must fail closed - the chain aborts at the
	// translate node and the validity query never runs again (stale values
	// stay; assert via a fresh graph state instead: hasTarget flips false).
	xOther.DestroyImmediate();
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xHost, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("otherPos").y, 2.0f, 0.001f);	// unchanged - chain aborted early
}

#ifdef ZENITH_INPUT_SIMULATOR
ZENITH_TEST(GraphComponent, InputNodeFamilyExecution)
{
	// P1h input nodes driven through the real sim-aware input path. No frame
	// stepping (StepFrame re-enters the main loop - forbidden here): state is
	// staged per "frame" via SetKeyHeld/SimulateKeyDown/ResetAllInputState.
	const std::string strAssetPath = "game:Graphs/UnitTest_InputNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// Gated counter chains - one per event-source kind.
		struct SourceCounter { const char* szSourceType; const char* szCounter; int32_t iKey; int32_t iMode; };
		const SourceCounter axSources[] = {
			{ "OnKeyHeld",     "heldCount",    ZENITH_KEY_W,            -1 },
			{ "OnKeyPressed",  "pressCount",   ZENITH_KEY_SPACE,        -1 },
			{ "OnKeyReleased", "releaseCount", ZENITH_KEY_SPACE,        -1 },
			{ "OnMouseButton", "lmbPress",     ZENITH_MOUSE_BUTTON_LEFT, 0 },
		};
		for (const SourceCounter& xSource : axSources)
		{
			const u_int uSource = xDef.AddNode(xSource.szSourceType);
			{
				NodeParamWriter xParams(xDef, uSource, xSource.szSourceType);
				xParams.SetInt(std::strcmp(xSource.szSourceType, "OnMouseButton") == 0 ? "m_iButton" : "m_iKeyCode", xSource.iKey);
				if (xSource.iMode >= 0)
				{
					xParams.SetInt("m_iMode", xSource.iMode);
				}
			}
			const u_int uCounter = xDef.AddNode("AddBlackboardFloat");
			{
				NodeParamWriter xParams(xDef, uCounter, "AddBlackboardFloat");
				xParams.SetString("m_strVariable", xSource.szCounter);
			}
			xDef.AddEdge(uSource, 0, uCounter, 0);
		}

		// Query chain off a plain OnUpdate anchor.
		const u_int uUpdate = xDef.AddNode("OnUpdate");
		const u_int uShift = xDef.AddNode("ReadKeyState");
		{
			NodeParamWriter xParams(xDef, uShift, "ReadKeyState");
			xParams.SetInt("m_iKeyCode", ZENITH_KEY_LEFT_SHIFT);
			xParams.SetInt("m_iMode", 0);
			xParams.SetString("m_strResultVar", "shift");
		}
		const u_int uMove = xDef.AddNode("ReadMovementAxis");	// WASD defaults
		const u_int uAxis = xDef.AddNode("ReadInputAxis");		// A/D defaults
		const u_int uLmb = xDef.AddNode("ReadMouseButtonHeld");
		{
			NodeParamWriter xParams(xDef, uLmb, "ReadMouseButtonHeld");
			xParams.SetString("m_strResultVar", "lmb");
		}
		const u_int uMousePos = xDef.AddNode("ReadMousePosition");
		xDef.AddEdge(uUpdate, 0, uShift, 0);
		xDef.AddEdge(uShift, 0, uMove, 0);
		xDef.AddEdge(uMove, 0, uAxis, 0);
		xDef.AddEdge(uAxis, 0, uLmb, 0);
		xDef.AddEdge(uLmb, 0, uMousePos, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphInputNodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "InputNodes");
	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	Zenith_InputSimulator::Enable();
	Zenith_InputSimulator::ResetAllInputState();

	// "Frame 1": W + A + Shift held, SPACE pressed, LMB down, cursor at (100, 200).
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_W, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_A, true);
	Zenith_InputSimulator::SetKeyHeld(ZENITH_KEY_LEFT_SHIFT, true);
	Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_SPACE);
	Zenith_InputSimulator::SimulateMouseButtonDown(ZENITH_MOUSE_BUTTON_LEFT);
	Zenith_InputSimulator::SimulateMousePosition(100.0, 200.0);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);

	const Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("heldCount"), 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("pressCount"), 1.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("releaseCount", 0.0f), 0.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("lmbPress"), 1.0f, 0.0001f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("shift"));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("moveDir").x, -0.70710678f, 0.001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("moveDir").z, 0.70710678f, 0.001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("axis"), -1.0f, 0.0001f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("lmb"));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector2("mousePos").x, 100.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector2("mousePos").y, 200.0f, 0.0001f);

	// "Frame 2": everything released (reset also clears the stale
	// pressed-this-frame latch, which only a frame step would consume).
	Zenith_InputSimulator::ResetAllInputState();
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);

	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("heldCount"), 1.0f, 0.0001f);		// W no longer held
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("pressCount"), 1.0f, 0.0001f);		// no new press
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("releaseCount"), 1.0f, 0.0001f);	// SPACE down -> up edge
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("lmbPress"), 1.0f, 0.0001f);		// no new press edge
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("shift", true));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("moveDir", Zenith_Maths::Vector3(9.0f)).x, 0.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("axis", 9.0f), 0.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("lmb", true));

	Zenith_InputSimulator::ResetAllInputState();
	Zenith_InputSimulator::Disable();
}
#endif // ZENITH_INPUT_SIMULATOR

namespace
{
	// Saves a parent graph asset: OnUpdate -> CallGraph(szChildPath).
	std::string SaveCallGraphParentAsset(const char* szLeafName, const std::string& strChildPath)
	{
		const std::string strAssetPath = std::string("game:Graphs/") + szLeafName;
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uCall = xDef.AddNode("CallGraph");
		{
			NodeParamWriter xParams(xDef, uCall, "CallGraph");
			xParams.SetString("m_strGraphAssetPath", strChildPath.c_str());
		}
		xDef.AddEdge(uSource, 0, uCall, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
		return strAssetPath;
	}
}

ZENITH_TEST(GraphComponent, CallGraphSharedBlackboardDefaultsAndRunning)
{
	// Child: declares "childTuning" = 5; entry chain Wait(0.03) -> counter++.
	const std::string strChildPath = "game:Graphs/UnitTest_CallChild.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		Zenith_PropertyValue xTuning;
		xTuning.SetFloat(5.0f);
		xDef.DeclareVariable("childTuning", xTuning);
		const u_int uEntry = xDef.AddNode("OnGraphCall");
		const u_int uWait = xDef.AddNode("Wait");
		{
			NodeParamWriter xParams(xDef, uWait, "Wait");
			Zenith_PropertyValue xSeconds;
			xSeconds.SetFloat(0.03f);
			xParams.Set("m_fSeconds", xSeconds);
		}
		const u_int uCounter = xDef.AddNode("AddBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uCounter, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", "sharedCounter");
		}
		xDef.AddEdge(uEntry, 0, uWait, 0);
		xDef.AddEdge(uWait, 0, uCounter, 0);
		Zenith_AssetRegistry::Save(&xAsset, strChildPath);
	}
	const std::string strParentPath = SaveCallGraphParentAsset("UnitTest_CallParent.bgraph", strChildPath);

	Zenith_TempScene xTempScene("TestGraphCallScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "CallHost");
	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxParent = xComponent.AddGraphByAssetPath(strParentPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxParent);
	if (!pxParent) return;

	// Fire 1: child resolves + its declared default seeds the SHARED (parent)
	// blackboard; the child's Wait suspends (0.016 < 0.03) -> counter untouched.
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	const Zenith_GraphBlackboard& xBlackboard = pxParent->GetBlackboard();
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("childTuning"), 5.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(xBlackboard.HasValue("sharedCounter"));

	// Fire 2: the suspended child chain resumes and completes -> counter = 1,
	// written to the PARENT blackboard (shared scope).
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("sharedCounter"), 1.0f, 0.0001f);

	// Next call cycle: two more fires -> counter = 2.
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("sharedCounter"), 2.0f, 0.0001f);
}

ZENITH_TEST(GraphComponent, CallGraphRecursionCapAndMissingAsset)
{
	Zenith_TempScene xTempScene("TestGraphCallEdgeScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	// Missing child asset: the call fails its chain, nothing crashes.
	{
		const std::string strParentPath = SaveCallGraphParentAsset("UnitTest_CallMissing.bgraph", "game:Graphs/DoesNotExist_CallGraph.bgraph");
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "CallMissingHost");
		Zenith_BehaviourGraph* pxParent = xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strParentPath.c_str());
		ZENITH_ASSERT_NOT_NULL(pxParent);
		if (!pxParent) return;
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);	// must not crash
	}

	// Self-recursive child: the depth cap (8) cuts the cycle with an error +
	// FAILURE cascade; the dispatcher survives and stays usable.
	{
		const std::string strRecursivePath = "game:Graphs/UnitTest_CallRecursive.bgraph";
		{
			std::error_code xEC;
			std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
			Zenith_BehaviourGraphAsset xAsset;
			Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
			const u_int uEntry = xDef.AddNode("OnGraphCall");
			const u_int uCall = xDef.AddNode("CallGraph");
			{
				NodeParamWriter xParams(xDef, uCall, "CallGraph");
				xParams.SetString("m_strGraphAssetPath", strRecursivePath.c_str());
			}
			xDef.AddEdge(uEntry, 0, uCall, 0);
			Zenith_AssetRegistry::Save(&xAsset, strRecursivePath);
		}
		const std::string strParentPath = SaveCallGraphParentAsset("UnitTest_CallRecursiveParent.bgraph", strRecursivePath);
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "CallRecursiveHost");
		Zenith_BehaviourGraph* pxParent = xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strParentPath.c_str());
		ZENITH_ASSERT_NOT_NULL(pxParent);
		if (!pxParent) return;
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);	// terminates at the cap
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);	// dispatcher still healthy
	}
}

ZENITH_TEST(GraphComponent, OnSceneLoadedFilterAndStash)
{
	// The OnSceneLoaded anchor rides the "__SceneLoaded" broadcast the engine's
	// m_pfnSceneLoaded hook fires at LoadScene completion. Drive the same seam
	// directly: filter matches by canonical-path suffix, stashes the path.
	const std::string strAssetPath = "game:Graphs/UnitTest_OnSceneLoaded.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnSceneLoaded");
		{
			NodeParamWriter xParams(xDef, uSource, "OnSceneLoaded");
			xParams.SetString("m_strScenePath", "frontend.zscen");
		}
		const u_int uCounter = xDef.AddNode("AddBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uCounter, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", "loads");
		}
		xDef.AddEdge(uSource, 0, uCounter, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphSceneLoadedScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SceneLoadedHost");
	Zenith_BehaviourGraph* pxGraph = xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;

	// Wrong scene: the filter closes the gate.
	Zenith_PropertyValue xPayload;
	xPayload.SetString("c:/dev/zenith/games/foo/assets/scenes/other.zscen");
	Zenith_GraphComponent::BroadcastCustomEvent("__SceneLoaded", &xPayload);
	ZENITH_ASSERT_FALSE(pxGraph->GetBlackboard().HasValue("loads"));

	// Matching suffix: chain runs + path stashed.
	xPayload.SetString("c:/dev/zenith/games/foo/assets/scenes/frontend.zscen");
	Zenith_GraphComponent::BroadcastCustomEvent("__SceneLoaded", &xPayload);
	ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("loads"), 1.0f, 0.0001f);
	ZENITH_ASSERT_STREQ(pxGraph->GetBlackboard().GetString("loadedScene").c_str(), "c:/dev/zenith/games/foo/assets/scenes/frontend.zscen");
}

ZENITH_TEST(GraphComponent, UnloadSceneMidUpdateIsSafe)
{
	// Regression (W2 Sokoban Esc-path UAF): a chain that synchronously unloads
	// a DIFFERENT scene mid-Zenith_SceneSystem::Update used to leave the freed
	// SceneData in the update loop's pointer snapshot - the loop then updated
	// freed memory (0xdd/0xfeeefeee Reserve assert). The snapshot is
	// handle+generation now; drive the REAL per-frame path with a host graph
	// whose OnUpdate unloads the later-created, ACTIVE victim scene.
	const std::string strAssetPath = "game:Graphs/UnitTest_UnloadMidUpdate.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnUpdate");
		// Once-gate: only the FIRST tick unloads. Without it the second tick's
		// UnloadScene("") would tear down whatever became active - possibly
		// the host's own scene mid-dispatch (the unsupported self-unload).
		const u_int uOnce = xDef.AddNode("Once");
		const u_int uUnload = xDef.AddNode("UnloadScene");	// "" = the active scene
		xDef.AddEdge(uSource, 0, uOnce, 0);
		xDef.AddEdge(uOnce, 0, uUnload, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	const Zenith_Scene xPreviousActive = g_xEngine.Scenes().GetActiveScene();

	Zenith_Scene xHost = g_xEngine.Scenes().LoadScene("UnloadMidUpdateHost", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxHostData = g_xEngine.Scenes().GetSceneData(xHost);
	ZENITH_ASSERT_NOT_NULL(pxHostData);
	Zenith_Entity xHostEntity = g_xEngine.Scenes().CreateEntity(pxHostData, "UnloadMidUpdateHost");
	Zenith_BehaviourGraph* pxGraph = xHostEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;

	// Victim: created AFTER the host (so it usually sits later in the update
	// snapshot), populated, and made ACTIVE (the UnloadScene node's target).
	Zenith_Scene xVictim = g_xEngine.Scenes().LoadScene("UnloadMidUpdateVictim", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxVictimData = g_xEngine.Scenes().GetSceneData(xVictim);
	ZENITH_ASSERT_NOT_NULL(pxVictimData);
	g_xEngine.Scenes().CreateEntity(pxVictimData, "UnloadMidUpdateVictimEntity");
	g_xEngine.Scenes().SetActiveScene(xVictim);

	// The frame under test: the host graph's chain unloads the victim while
	// the victim is still pending in this frame's update snapshot.
	g_xEngine.Scenes().Update(1.0f / 60.0f);
	ZENITH_ASSERT_FALSE(xVictim.IsValid());
	ZENITH_ASSERT_TRUE(xHost.IsValid());

	// A second clean tick proves no stale snapshot state survived the frame.
	g_xEngine.Scenes().Update(1.0f / 60.0f);
	ZENITH_ASSERT_TRUE(xHost.IsValid());

	g_xEngine.Scenes().UnloadScene(xHost);
	if (xPreviousActive.IsValid())
	{
		g_xEngine.Scenes().SetActiveScene(xPreviousActive);
	}
}

ZENITH_TEST(GraphComponent, GraphSurvivesDontDestroyOnLoadRelocation)
{
	// The pause-menu pattern (DP W3, risk R6): an entity promotes itself to the
	// persistent scene via DontDestroyOnLoad - which move-constructs EVERY
	// component, including Zenith_GraphComponent, into the persistent scene's
	// pools (Zenith_ComponentMetaRegistry::TransferAllComponents) - and the
	// source gameplay scene then unloads. Pin that the graph's blackboard, its
	// per-frame dispatch, AND an in-flight RUNNING chain cursor all survive the
	// relocation + source-scene unload.
	const std::string strAssetPath = "game:Graphs/UnitTest_PersistentMove.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		// Chain A (own source; chains run in node order): count every tick.
		const u_int uTickSource = xDef.AddNode("OnUpdate");
		const u_int uCount = xDef.AddNode("AddBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uCount, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", "ticks");
		}
		xDef.AddEdge(uTickSource, 0, uCount, 0);
		// Chain B: the first tick arms a 1s Wait whose chain cursor is the
		// in-flight RUNNING state that must survive the cross-scene move.
		const u_int uWaitSource = xDef.AddNode("OnUpdate");
		const u_int uOnce = xDef.AddNode("Once");
		const u_int uWait = xDef.AddNode("Wait");
		const u_int uDone = xDef.AddNode("SetBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uDone, "SetBlackboardFloat");
			xParams.SetString("m_strVariable", "done");
			Zenith_PropertyValue xValue;
			xValue.SetFloat(1.0f);
			xParams.Set("m_fValue", xValue);
		}
		xDef.AddEdge(uWaitSource, 0, uOnce, 0);
		xDef.AddEdge(uOnce, 0, uWait, 0);
		xDef.AddEdge(uWait, 0, uDone, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	const Zenith_Scene xPreviousActive = g_xEngine.Scenes().GetActiveScene();

	// Stand-in for the gameplay scene the pause-menu entity boots in.
	Zenith_Scene xGameplay = g_xEngine.Scenes().LoadScene("PersistentMoveGameplay", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxGameplayData = g_xEngine.Scenes().GetSceneData(xGameplay);
	ZENITH_ASSERT_NOT_NULL(pxGameplayData);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxGameplayData, "PersistentMoveHost");
	Zenith_BehaviourGraph* pxGraph = xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;

	// One tick in the source scene: counter = 1, the Wait chain arms (RUNNING).
	g_xEngine.Scenes().Update(0.25f);
	ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("ticks"), 1.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(pxGraph->GetBlackboard().HasValue("done"));

	// Promote (root-only DontDestroyOnLoad -> MoveEntityToScene(persistent)),
	// then unload the source scene - exactly what SCENE_LOAD_SINGLE does to
	// non-persistent scenes underneath the pause menu.
	xEntity.DontDestroyOnLoad();
	g_xEngine.Scenes().UnloadScene(xGameplay);
	ZENITH_ASSERT_FALSE(xGameplay.IsValid());
	ZENITH_ASSERT_TRUE(xEntity.IsValid());

	// The moved component must still own the SAME heap graph instance.
	Zenith_BehaviourGraph* pxMovedGraph = xEntity.GetComponent<Zenith_GraphComponent>().GetGraphAt(0);
	ZENITH_ASSERT_EQ(pxMovedGraph, pxGraph);

	// Dispatch continues from the persistent scene; the armed Wait is still
	// RUNNING (cursor survived the move-construct).
	g_xEngine.Scenes().Update(0.25f);
	ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("ticks"), 2.0f, 0.0001f);
	ZENITH_ASSERT_FALSE(pxGraph->GetBlackboard().HasValue("done"));

	// Overshoot the remaining wait: the suspended chain completes post-move.
	g_xEngine.Scenes().Update(1.5f);
	ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("done"), 1.0f, 0.0001f);

	// The persistent scene outlives every test - remove what we promoted.
	xEntity.DestroyImmediate();
	if (xPreviousActive.IsValid())
	{
		g_xEngine.Scenes().SetActiveScene(xPreviousActive);
	}
}

ZENITH_TEST(GraphComponent, SelectorAbortPreemptedFlagSemantics)
{
	// W3 priest (risk R3): branches of a reactive Selector that drive a
	// SHARED effector must be able to opt out of preemption aborts -
	// AbortChain runs AFTER the higher-priority pin executed, so an
	// OnAbort side effect (NavMoveTo::Stop on the shared nav agent) would
	// clobber the new pin's work and ping-pong the selector. With
	// m_bAbortPreempted=false the preempted branch keeps its cursor and
	// per-node state and resumes STALE when the selector falls back to it
	// (the BT memory-composite parity). Pin both modes:
	//   pin0 = Gate("goHigh") -> SetBlackboardFloat("hi")
	//   pin1 = Wait(1.0)      -> SetBlackboardFloat("done")
	// Drive: 0.6s (pin1 waits 0.6/1.0) -> goHigh (pin0 preempts) ->
	// !goHigh, 0.6s. No-abort: Wait resumes 0.6+0.6 >= 1.0 -> done set.
	// Default abort: Wait was reset -> 0.6/1.0 -> done still unset.
	auto BuildSelectorAsset = [](const char* szPath, bool bAbortPreempted)
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSelector = xDef.AddNode("Selector");
		{
			NodeParamWriter xParams(xDef, uSelector, "Selector");
			Zenith_PropertyValue xValue;
			xValue.SetBool(bAbortPreempted);
			xParams.Set("m_bAbortPreempted", xValue);
		}
		const u_int uGate = xDef.AddNode("Gate");
		{
			NodeParamWriter xParams(xDef, uGate, "Gate");
			xParams.SetString("m_strOpenVar", "goHigh");
		}
		const u_int uHi = xDef.AddNode("SetBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uHi, "SetBlackboardFloat");
			xParams.SetString("m_strVariable", "hi");
			Zenith_PropertyValue xValue;
			xValue.SetFloat(1.0f);
			xParams.Set("m_fValue", xValue);
		}
		const u_int uWait = xDef.AddNode("Wait");
		const u_int uDone = xDef.AddNode("SetBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uDone, "SetBlackboardFloat");
			xParams.SetString("m_strVariable", "done");
			Zenith_PropertyValue xValue;
			xValue.SetFloat(1.0f);
			xParams.Set("m_fValue", xValue);
		}
		xDef.AddEdge(uSource, 0, uSelector, 0);
		xDef.AddEdge(uSelector, 0, uGate, 0);
		xDef.AddEdge(uGate, 0, uHi, 0);
		xDef.AddEdge(uSelector, 1, uWait, 0);
		xDef.AddEdge(uWait, 0, uDone, 0);
		Zenith_AssetRegistry::Save(&xAsset, szPath);
	};

	auto DriveSequence = [](Zenith_Entity& xEntity, Zenith_BehaviourGraph* pxGraph)
	{
		Zenith_PropertyValue xFlag;
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.6f);	// pin1 waits 0.6/1.0
		xFlag.SetBool(true);
		pxGraph->GetBlackboard().SetValue("goHigh", xFlag);
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.6f);	// pin0 preempts
		xFlag.SetBool(false);
		pxGraph->GetBlackboard().SetValue("goHigh", xFlag);
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.6f);	// pin1 resumes
	};

	Zenith_TempScene xTempScene("TestSelectorAbortFlagScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	// No-abort mode: the preempted Wait keeps its 0.6s and completes.
	BuildSelectorAsset("game:Graphs/UnitTest_SelectorNoAbort.bgraph", false);
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SelectorNoAbortHost");
		Zenith_BehaviourGraph* pxGraph = xEntity.AddComponent<Zenith_GraphComponent>()
			.AddGraphByAssetPath("game:Graphs/UnitTest_SelectorNoAbort.bgraph");
		ZENITH_ASSERT_NOT_NULL(pxGraph);
		if (!pxGraph) return;
		DriveSequence(xEntity, pxGraph);
		ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("hi"), 1.0f, 0.0001f);
		ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("done"), 1.0f, 0.0001f);
	}

	// Default mode: preemption aborts pin1 - the Wait restarts, so the
	// same drive leaves "done" unset.
	BuildSelectorAsset("game:Graphs/UnitTest_SelectorAbort.bgraph", true);
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SelectorAbortHost");
		Zenith_BehaviourGraph* pxGraph = xEntity.AddComponent<Zenith_GraphComponent>()
			.AddGraphByAssetPath("game:Graphs/UnitTest_SelectorAbort.bgraph");
		ZENITH_ASSERT_NOT_NULL(pxGraph);
		if (!pxGraph) return;
		DriveSequence(xEntity, pxGraph);
		ZENITH_ASSERT_EQ_FLOAT(pxGraph->GetBlackboard().GetFloat("hi"), 1.0f, 0.0001f);
		ZENITH_ASSERT_FALSE(pxGraph->GetBlackboard().HasValue("done"));
	}
}

ZENITH_TEST(GraphComponent, OverrideBlobV2ListsAndV1Compat)
{
	const std::string strAssetPath = SaveRotateGraphAsset("UnitTest_RotateV2.bgraph", 45.0f);

	Zenith_TempScene xTempScene("TestGraphV2Scene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	// v2 round-trip: a populated blackboard LIST survives component
	// serialization (the program's only on-disk format change).
	{
		Zenith_Entity xSource = g_xEngine.Scenes().CreateEntity(pxSceneData, "V2Source");
		Zenith_GraphComponent& xComponent = xSource.AddComponent<Zenith_GraphComponent>();
		Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
		ZENITH_ASSERT_NOT_NULL(pxGraph);
		if (!pxGraph) return;

		Zenith_Vector<Zenith_PropertyValue>& axList = pxGraph->GetBlackboard().GetOrCreateList("inventory");
		Zenith_PropertyValue xItem;
		xItem.SetInt32(7);
		axList.PushBack(xItem);
		xItem.SetInt32(9);
		axList.PushBack(xItem);

		Zenith_DataStream xStream;
		xComponent.WriteToDataStream(xStream);
		xStream.SetCursor(0);

		Zenith_Entity xDest = g_xEngine.Scenes().CreateEntity(pxSceneData, "V2Dest");
		Zenith_GraphComponent& xDestComponent = xDest.AddComponent<Zenith_GraphComponent>();
		xDestComponent.ReadFromDataStream(xStream);
		ZENITH_ASSERT_EQ(xDestComponent.GetGraphCount(), 1u);
		ZENITH_ASSERT_NOT_NULL(xDestComponent.GetGraphAt(0));
		if (!xDestComponent.GetGraphAt(0)) return;
		const Zenith_Vector<Zenith_PropertyValue>* pxList = xDestComponent.GetGraphAt(0)->GetBlackboard().TryGetList("inventory");
		ZENITH_ASSERT_NOT_NULL(pxList);
		if (!pxList) return;
		ZENITH_ASSERT_EQ(pxList->GetSize(), 2u);
		ZENITH_ASSERT_EQ(pxList->Get(1).GetInt32(), 9);
	}

	// v1 fixture: a hand-built version-1 stream (values-only override bytes,
	// the pre-list format) still loads - values apply, no list section is
	// expected or read. Pins back-compat for every existing saved scene.
	{
		Zenith_DataStream xV1Stream;
		const u_int uVersion = 1;
		xV1Stream << uVersion;
		const uint64_t ulBlobSizeCursor = xV1Stream.GetCursor();
		u_int uPlaceholder = 0;
		xV1Stream << uPlaceholder;
		const uint64_t ulBlobStart = xV1Stream.GetCursor();

		const u_int uSlotCount = 1;
		xV1Stream << uSlotCount;
		xV1Stream << std::string(strAssetPath);

		const uint64_t ulOverrideSizeCursor = xV1Stream.GetCursor();
		xV1Stream << uPlaceholder;
		const uint64_t ulOverrideStart = xV1Stream.GetCursor();
		// v1 blackboard bytes = values section ONLY.
		const u_int uVarCount = 1;
		xV1Stream << uVarCount;
		xV1Stream << std::string("tweaked");
		Zenith_PropertyValue xValue;
		xValue.SetFloat(55.5f);
		xV1Stream << xValue;
		const uint64_t ulOverrideEnd = xV1Stream.GetCursor();
		xV1Stream.SetCursor(ulOverrideSizeCursor);
		xV1Stream << static_cast<u_int>(ulOverrideEnd - ulOverrideStart);
		xV1Stream.SetCursor(ulOverrideEnd);

		const uint64_t ulBlobEnd = xV1Stream.GetCursor();
		xV1Stream.SetCursor(ulBlobSizeCursor);
		xV1Stream << static_cast<u_int>(ulBlobEnd - ulBlobStart);
		xV1Stream.SetCursor(0);

		Zenith_Entity xV1Dest = g_xEngine.Scenes().CreateEntity(pxSceneData, "V1Dest");
		Zenith_GraphComponent& xV1Component = xV1Dest.AddComponent<Zenith_GraphComponent>();
		xV1Component.ReadFromDataStream(xV1Stream);
		ZENITH_ASSERT_EQ(xV1Component.GetGraphCount(), 1u);
		ZENITH_ASSERT_NOT_NULL(xV1Component.GetGraphAt(0));
		if (!xV1Component.GetGraphAt(0)) return;
		ZENITH_ASSERT_EQ_FLOAT(xV1Component.GetGraphAt(0)->GetBlackboard().GetFloat("tweaked"), 55.5f, 0.0001f);
		ZENITH_ASSERT_EQ(xV1Component.GetGraphAt(0)->GetBlackboard().GetListCount(), 0u);
	}
}

ZENITH_TEST(GraphComponent, ThousandEntityUpdateBenchmark)
{
	const std::string strAssetPath = SaveRotateGraphAsset("UnitTest_RotateBench.bgraph", 90.0f);

	Zenith_TempScene xTempScene("TestGraphBenchScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	constexpr u_int uENTITY_COUNT = 1000;
	Zenith_Vector<Zenith_Entity> axEntities;
	for (u_int u = 0; u < uENTITY_COUNT; ++u)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "Bench");
		xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
		axEntities.PushBack(xEntity);
	}

	constexpr u_int uFRAMES = 60;
	const auto xStart = std::chrono::high_resolution_clock::now();
	for (u_int uFrame = 0; uFrame < uFRAMES; ++uFrame)
	{
		for (u_int u = 0; u < uENTITY_COUNT; ++u)
		{
			Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(axEntities.Get(u), 0.016f);
		}
	}
	const auto xEnd = std::chrono::high_resolution_clock::now();
	const double fTotalMs = std::chrono::duration<double, std::milli>(xEnd - xStart).count();
	const double fPerFrameMs = fTotalMs / static_cast<double>(uFRAMES);

	Zenith_Log(LOG_CATEGORY_CORE,
		"[BehaviourGraph benchmark] %u graph entities, %u frames: %.3f ms/frame (%.1f ms total)",
		uENTITY_COUNT, uFRAMES, fPerFrameMs, fTotalMs);

	// Pathology guard (Debug build budget). The plan's shipping target is far
	// lower; this catches accidental O(n^2) or per-dispatch allocation storms.
	ZENITH_ASSERT_LT(fPerFrameMs, 20.0, "1000-entity graph update took %.3f ms/frame", fPerFrameMs);

	// IDLE-graph phase (the NeedsUpdateDispatch merge gate): the same entity
	// count whose graphs anchor only on a custom event must dispatch at a
	// fraction of the active cost - idle graphs are skipped before any
	// snapshot allocation.
	const std::string strIdlePath = "game:Graphs/UnitTest_IdleBench.bgraph";
	{
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uSource = xDef.AddNode("OnCustomEvent");
		const u_int uFlag = xDef.AddNode("SetBlackboardBool");
		xDef.AddEdge(uSource, 0, uFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strIdlePath);
	}
	Zenith_Vector<Zenith_Entity> axIdleEntities;
	for (u_int u = 0; u < uENTITY_COUNT; ++u)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "IdleBench");
		xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strIdlePath.c_str());
		axIdleEntities.PushBack(xEntity);
	}
	const auto xIdleStart = std::chrono::high_resolution_clock::now();
	for (u_int uFrame = 0; uFrame < uFRAMES; ++uFrame)
	{
		for (u_int u = 0; u < uENTITY_COUNT; ++u)
		{
			Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(axIdleEntities.Get(u), 0.016f);
		}
	}
	const auto xIdleEnd = std::chrono::high_resolution_clock::now();
	const double fIdleTotalMs = std::chrono::duration<double, std::milli>(xIdleEnd - xIdleStart).count();
	const double fIdlePerFrameMs = fIdleTotalMs / static_cast<double>(uFRAMES);

	Zenith_Log(LOG_CATEGORY_CORE,
		"[BehaviourGraph benchmark] %u IDLE graph entities, %u frames: %.3f ms/frame (%.1f ms total)",
		uENTITY_COUNT, uFRAMES, fIdlePerFrameMs, fIdleTotalMs);
	ZENITH_ASSERT_LT(fIdlePerFrameMs, 5.0, "1000 idle graphs took %.3f ms/frame", fIdlePerFrameMs);
}

//==============================================================================
// P2 node-library coverage: math/random, retrofitted params, physics,
// animation/tween/particles, UI (+ button trampoline), AI nav/perception,
// entity remainder (+ camera seam), and the registry-wide round-trip.
//==============================================================================

// Main-camera assignment is friend-gated on Zenith_SceneData; free
// ZENITH_TEST functions route through this Zenith_UnitTests forwarder.
void Zenith_UnitTests::SetMainCameraForTest(Zenith_SceneData* pxSceneData, Zenith_EntityID xEntity)
{
	pxSceneData->SetMainCameraEntity(xEntity);
}

namespace
{
	void SetFloatParam(NodeParamWriter& xParams, const char* szProperty, float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		xParams.Set(szProperty, xValue);
	}
	void SetBoolParam(NodeParamWriter& xParams, const char* szProperty, bool bValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetBool(bValue);
		xParams.Set(szProperty, xValue);
	}
	void SetVec3Param(NodeParamWriter& xParams, const char* szProperty, const Zenith_Maths::Vector3& xVec)
	{
		Zenith_PropertyValue xValue;
		xValue.SetVector3(xVec);
		xParams.Set(szProperty, xValue);
	}
	void SetVec4Param(NodeParamWriter& xParams, const char* szProperty, const Zenith_Maths::Vector4& xVec)
	{
		Zenith_PropertyValue xValue;
		xValue.SetVector4(xVec);
		xParams.Set(szProperty, xValue);
	}

	void StageFloat(Zenith_GraphBlackboard& xBlackboard, const char* szName, float fValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetFloat(fValue);
		xBlackboard.SetValue(szName, xValue);
	}
	void StageInt(Zenith_GraphBlackboard& xBlackboard, const char* szName, int32_t iValue)
	{
		Zenith_PropertyValue xValue;
		xValue.SetInt32(iValue);
		xBlackboard.SetValue(szName, xValue);
	}
	void StageVec3(Zenith_GraphBlackboard& xBlackboard, const char* szName, const Zenith_Maths::Vector3& xVec)
	{
		Zenith_PropertyValue xValue;
		xValue.SetVector3(xVec);
		xBlackboard.SetValue(szName, xValue);
	}
	void StageEntity(Zenith_GraphBlackboard& xBlackboard, const char* szName, const Zenith_Entity& xEntity)
	{
		Zenith_PropertyValue xValue;
		xValue.SetPackedEntityID(xEntity.GetEntityID().GetPacked());
		xBlackboard.SetValue(szName, xValue);
	}
}

ZENITH_TEST(GraphComponent, MathNodeFamilyExecution)
{
	const std::string strAssetPath = "game:Graphs/UnitTest_MathNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		const u_int uSource = xDef.AddNode("OnUpdate");
		const u_int uSetA = xDef.AddNode("SetBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uSetA, "SetBlackboardFloat");
			xParams.SetString("m_strVariable", "a");
			SetFloatParam(xParams, "m_fValue", 10.0f);
		}
		const u_int uSub = xDef.AddNode("MathBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uSub, "MathBlackboardFloat");
			xParams.SetString("m_strVar", "a");
			xParams.SetInt("m_iOp", 0);	// sub, in place
			SetFloatParam(xParams, "m_fOperand", 4.0f);
		}
		const u_int uMul = xDef.AddNode("MathBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uMul, "MathBlackboardFloat");
			xParams.SetString("m_strVar", "a");
			xParams.SetInt("m_iOp", 1);	// mul by var m -> prod
			xParams.SetString("m_strOperandVar", "m");
			xParams.SetString("m_strResultVar", "prod");
		}
		const u_int uSetV = xDef.AddNode("SetBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uSetV, "SetBlackboardVector3");
			xParams.SetString("m_strVariable", "v");
			SetVec3Param(xParams, "m_xValue", Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));
		}
		const u_int uLen = xDef.AddNode("MathBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uLen, "MathBlackboardVector3");
			xParams.SetString("m_strVar", "v");
			xParams.SetInt("m_iOp", 4);	// length -> len
			xParams.SetString("m_strResultVar", "len");
		}
		const u_int uScale = xDef.AddNode("MathBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uScale, "MathBlackboardVector3");
			xParams.SetString("m_strVar", "v");
			xParams.SetInt("m_iOp", 2);	// scale x2 in place
			SetFloatParam(xParams, "m_fScalar", 2.0f);
		}
		const u_int uLerpT = xDef.AddNode("LerpBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uLerpT, "LerpBlackboardFloat");
			xParams.SetString("m_strVar", "l");
			SetFloatParam(xParams, "m_fTarget", 10.0f);
			xParams.SetInt("m_iMode", 0);
			SetFloatParam(xParams, "m_fT", 0.5f);
		}
		const u_int uLerpR = xDef.AddNode("LerpBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uLerpR, "LerpBlackboardFloat");
			xParams.SetString("m_strVar", "r");
			SetFloatParam(xParams, "m_fTarget", 10.0f);
			xParams.SetInt("m_iMode", 1);	// rate mode: 4 units/s
			SetFloatParam(xParams, "m_fRate", 4.0f);
		}
		const u_int uLerpV = xDef.AddNode("LerpBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uLerpV, "LerpBlackboardVector3");
			xParams.SetString("m_strVar", "lv");
			SetVec3Param(xParams, "m_xTarget", Zenith_Maths::Vector3(0.0f, 8.0f, 0.0f));
			xParams.SetInt("m_iMode", 0);
			SetFloatParam(xParams, "m_fT", 0.5f);
		}
		const u_int uClamp = xDef.AddNode("ClampBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uClamp, "ClampBlackboardFloat");
			xParams.SetString("m_strVar", "c");
			SetFloatParam(xParams, "m_fMin", 0.0f);
			SetFloatParam(xParams, "m_fMax", 5.0f);
		}
		const u_int uAddI = xDef.AddNode("AddBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uAddI, "AddBlackboardInt");
			xParams.SetString("m_strVariable", "i");
			xParams.SetInt("m_iDelta", 3);
		}
		const u_int uAddI2 = xDef.AddNode("AddBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uAddI2, "AddBlackboardInt");
			xParams.SetString("m_strVariable", "i");
			xParams.SetString("m_strDeltaVar", "di");
		}
		const u_int uAddV = xDef.AddNode("AddBlackboardVector3");
		{
			NodeParamWriter xParams(xDef, uAddV, "AddBlackboardVector3");
			xParams.SetString("m_strVariable", "av");
			SetVec3Param(xParams, "m_xDelta", Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));
			SetBoolParam(xParams, "m_bScaleByDt", true);
		}
		const u_int uSelf1 = xDef.AddNode("StoreSelfEntityID");
		{
			NodeParamWriter xParams(xDef, uSelf1, "StoreSelfEntityID");
			xParams.SetString("m_strVariable", "s1");
		}
		const u_int uSelf2 = xDef.AddNode("StoreSelfEntityID");
		{
			NodeParamWriter xParams(xDef, uSelf2, "StoreSelfEntityID");
			xParams.SetString("m_strVariable", "s2");
		}
		const u_int uCmpE = xDef.AddNode("CompareBlackboardEntity");
		{
			NodeParamWriter xParams(xDef, uCmpE, "CompareBlackboardEntity");
			xParams.SetString("m_strVarA", "s1");
			xParams.SetString("m_strVarB", "s2");
			xParams.SetInt("m_iOp", 0);	// equal
			xParams.SetString("m_strResultVar", "same");
		}
		const u_int uCmpNE = xDef.AddNode("CompareBlackboardEntity");
		{
			// s1 (self) vs eOther (a different entity, pre-staged): notEqual.
			NodeParamWriter xParams(xDef, uCmpNE, "CompareBlackboardEntity");
			xParams.SetString("m_strVarA", "s1");
			xParams.SetString("m_strVarB", "eOther");
			xParams.SetInt("m_iOp", 1);	// notEqual
			xParams.SetString("m_strResultVar", "differ");
		}
		const u_int uRandF = xDef.AddNode("RandomFloat");
		{
			NodeParamWriter xParams(xDef, uRandF, "RandomFloat");
			SetFloatParam(xParams, "m_fMin", 5.0f);
			SetFloatParam(xParams, "m_fMax", 6.0f);
			xParams.SetInt("m_iSeed", 77);
			xParams.SetString("m_strResultVar", "rf");
		}
		const u_int uRandI = xDef.AddNode("RandomInt");
		{
			NodeParamWriter xParams(xDef, uRandI, "RandomInt");
			xParams.SetInt("m_iMin", 2);
			xParams.SetInt("m_iMax", 4);
			xParams.SetInt("m_iSeed", 77);
			xParams.SetString("m_strResultVar", "ri");
		}

		u_int uPrev = uSource;
		const u_int auChain[] = { uSetA, uSub, uMul, uSetV, uLen, uScale, uLerpT, uLerpR, uLerpV,
			uClamp, uAddI, uAddI2, uAddV, uSelf1, uSelf2, uCmpE, uCmpNE, uRandF, uRandI };
		for (u_int u = 0; u < sizeof(auChain) / sizeof(auChain[0]); ++u)
		{
			xDef.AddEdge(uPrev, 0, auChain[u], 0);
			uPrev = auChain[u];
		}

		// Fail-loudly contract: division by zero gates the chain (custom
		// anchor so the abort doesn't take the main chain down with it).
		const u_int uBadSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uBadSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "MathBad");
		}
		const u_int uDivZero = xDef.AddNode("MathBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uDivZero, "MathBlackboardFloat");
			xParams.SetString("m_strVar", "a");
			xParams.SetInt("m_iOp", 2);	// div
			SetFloatParam(xParams, "m_fOperand", 0.0f);
		}
		const u_int uBadFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uBadFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "divRan");
		}
		xDef.AddEdge(uBadSource, 0, uDivZero, 0);
		xDef.AddEdge(uDivZero, 0, uBadFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphMathNodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MathNodes");
	Zenith_BehaviourGraph* pxGraph = xEntity.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	Zenith_Entity xOther = g_xEngine.Scenes().CreateEntity(pxSceneData, "MathNodesOther");

	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();
	StageFloat(xBlackboard, "m", 2.0f);
	StageFloat(xBlackboard, "l", 0.0f);
	StageFloat(xBlackboard, "r", 0.0f);
	StageFloat(xBlackboard, "c", 7.0f);
	StageInt(xBlackboard, "di", 2);
	StageVec3(xBlackboard, "av", Zenith_Maths::Vector3(0.0f));
	StageVec3(xBlackboard, "lv", Zenith_Maths::Vector3(0.0f));
	StageEntity(xBlackboard, "eOther", xOther);

	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.5f);

	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("a"), 6.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("prod"), 12.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("len"), 5.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("v").z, 8.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("l"), 5.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("r"), 2.0f, 0.0001f);	// 4 units/s * 0.5s
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("c"), 5.0f, 0.0001f);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("lv").y, 4.0f, 0.0001f);	// t-mode 0.5 toward (0,8,0)
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("i"), 5);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("av").y, 5.0f, 0.0001f);	// dt-scaled
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("same"));
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("differ"));

	// div-by-zero FAILURE gates the chain: 'a' untouched, flag never set.
	xEntity.GetComponent<Zenith_GraphComponent>().FireCustomEvent("MathBad");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("divRan", false));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("a"), 6.0f, 0.0001f);
	const float fRandom = xBlackboard.GetFloat("rf");
	ZENITH_ASSERT_TRUE(fRandom >= 5.0f && fRandom <= 6.0f, "RandomFloat out of range: %f", fRandom);
	const int32_t iRandom = xBlackboard.GetInt32("ri");
	ZENITH_ASSERT_TRUE(iRandom >= 2 && iRandom <= 4, "RandomInt out of range: %d", iRandom);

	// Determinism: a second instance with the same seed draws the same first
	// values (per-instance xorshift stream).
	Zenith_Entity xTwin = g_xEngine.Scenes().CreateEntity(pxSceneData, "MathNodesTwin");
	Zenith_BehaviourGraph* pxTwin = xTwin.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxTwin);
	if (!pxTwin) return;
	StageFloat(pxTwin->GetBlackboard(), "m", 2.0f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xTwin, 0.5f);
	ZENITH_ASSERT_EQ_FLOAT(pxTwin->GetBlackboard().GetFloat("rf"), fRandom, 0.0f);
	ZENITH_ASSERT_EQ(pxTwin->GetBlackboard().GetInt32("ri"), iRandom);
}

ZENITH_TEST(GraphComponent, RetrofittedNodeParamsExecution)
{
	const std::string strAssetPath = "game:Graphs/UnitTest_RetrofitNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// Chain A (OnUpdate): var-vs-var compare + dt-scaled deltaVar add.
		const u_int uUpdate = xDef.AddNode("OnUpdate");
		const u_int uCmp = xDef.AddNode("CompareBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uCmp, "CompareBlackboardFloat");
			xParams.SetString("m_strVar", "x");
			xParams.SetString("m_strCompareVar", "y");
			xParams.SetInt("m_iOp", 4);	// equal
			xParams.SetString("m_strResultVar", "feq");
		}
		const u_int uAdd = xDef.AddNode("AddBlackboardFloat");
		{
			NodeParamWriter xParams(xDef, uAdd, "AddBlackboardFloat");
			xParams.SetString("m_strVariable", "t");
			xParams.SetString("m_strDeltaVar", "spd");
			SetBoolParam(xParams, "m_bScaleByDt", true);
		}
		xDef.AddEdge(uUpdate, 0, uCmp, 0);
		xDef.AddEdge(uCmp, 0, uAdd, 0);

		// Chain B (custom "WaitGo"): Wait with secondsVar, then a flag.
		const u_int uWaitSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uWaitSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "WaitGo");
		}
		const u_int uWait = xDef.AddNode("Wait");
		{
			NodeParamWriter xParams(xDef, uWait, "Wait");
			xParams.SetString("m_strSecondsVar", "wsec");
		}
		const u_int uWaitFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uWaitFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "waitDone");
		}
		xDef.AddEdge(uWaitSource, 0, uWait, 0);
		xDef.AddEdge(uWait, 0, uWaitFlag, 0);

		// Chain C (custom "LoopGo"): Loop with countVar.
		const u_int uLoopSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uLoopSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "LoopGo");
		}
		const u_int uLoop = xDef.AddNode("Loop");
		{
			NodeParamWriter xParams(xDef, uLoop, "Loop");
			xParams.SetString("m_strCountVar", "n");
		}
		const u_int uBody = xDef.AddNode("AddBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uBody, "AddBlackboardInt");
			xParams.SetString("m_strVariable", "li");
			xParams.SetInt("m_iDelta", 1);
		}
		const u_int uLoopDone = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uLoopDone, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "loopDone");
		}
		xDef.AddEdge(uLoopSource, 0, uLoop, 0);
		xDef.AddEdge(uLoop, 0, uBody, 0);
		xDef.AddEdge(uLoop, 1, uLoopDone, 0);

		// Chain D (custom "LoopWaitGo"): Loop whose DONE chain suspends - the
		// body must not re-run while the done Wait resumes.
		const u_int uLoopWaitSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uLoopWaitSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "LoopWaitGo");
		}
		const u_int uLoopWait = xDef.AddNode("Loop");
		{
			NodeParamWriter xParams(xDef, uLoopWait, "Loop");
			xParams.SetInt("m_iCount", 2);
		}
		const u_int uLoopWaitBody = xDef.AddNode("AddBlackboardInt");
		{
			NodeParamWriter xParams(xDef, uLoopWaitBody, "AddBlackboardInt");
			xParams.SetString("m_strVariable", "lw");
			xParams.SetInt("m_iDelta", 1);
		}
		const u_int uDoneWait = xDef.AddNode("Wait");
		{
			NodeParamWriter xParams(xDef, uDoneWait, "Wait");
			SetFloatParam(xParams, "m_fSeconds", 0.05f);
		}
		const u_int uLoopWaitFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uLoopWaitFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "lwDone");
		}
		xDef.AddEdge(uLoopWaitSource, 0, uLoopWait, 0);
		xDef.AddEdge(uLoopWait, 0, uLoopWaitBody, 0);
		xDef.AddEdge(uLoopWait, 1, uDoneWait, 0);
		xDef.AddEdge(uDoneWait, 0, uLoopWaitFlag, 0);

		// Chain E (custom "TouchGo"): the deferred input queries - touch
		// state, mouse delta/wheel (simulator-neutral defaults headless).
		const u_int uTouchSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uTouchSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "TouchGo");
		}
		const u_int uTouch = xDef.AddNode("ReadTouchState");
		{
			NodeParamWriter xParams(xDef, uTouch, "ReadTouchState");
			xParams.SetString("m_strTapVar", "touchTap");
		}
		const u_int uDelta = xDef.AddNode("ReadMouseDelta");
		{
			NodeParamWriter xParams(xDef, uDelta, "ReadMouseDelta");
			xParams.SetString("m_strResultVar", "mDelta");
		}
		const u_int uWheel = xDef.AddNode("ReadMouseWheel");
		{
			NodeParamWriter xParams(xDef, uWheel, "ReadMouseWheel");
			xParams.SetString("m_strResultVar", "mWheel");
		}
		xDef.AddEdge(uTouchSource, 0, uTouch, 0);
		xDef.AddEdge(uTouch, 0, uDelta, 0);
		xDef.AddEdge(uDelta, 0, uWheel, 0);

		// Chain F (custom "PickRayGo"): no main camera in this scene - the
		// pick-ray node must gate the chain.
		const u_int uRaySource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uRaySource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "PickRayGo");
		}
		const u_int uRay = xDef.AddNode("ReadMousePickRay");
		const u_int uRayFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uRayFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "rayOk");
		}
		xDef.AddEdge(uRaySource, 0, uRay, 0);
		xDef.AddEdge(uRay, 0, uRayFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphRetrofitScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "RetrofitNodes");
	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);

	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();
	StageFloat(xBlackboard, "x", 3.0f);
	StageFloat(xBlackboard, "y", 3.0f);
	StageFloat(xBlackboard, "spd", 10.0f);
	StageFloat(xBlackboard, "wsec", 0.05f);
	StageInt(xBlackboard, "n", 3);

	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.25f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("feq"));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("t"), 2.5f, 0.0001f);	// 10/s * 0.25s

	// Wait via secondsVar: suspends, then completes once dispatched dt sums
	// past the var (one-shot anchors are re-driven by the update dispatch).
	xComponent.FireCustomEvent("WaitGo");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("waitDone", false));
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.03f);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("waitDone", false));
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.03f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("waitDone", false));

	xComponent.FireCustomEvent("LoopGo");
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("li"), 3);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("loopDone"));

	// Loop with a SUSPENDED done chain: the body ran exactly count times up
	// front and must NOT re-run while the done Wait resumes across ticks.
	xComponent.FireCustomEvent("LoopWaitGo");
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("lw"), 2);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("lwDone", false));
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.03f);
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("lw"), 2);	// body did not re-run mid-wait
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.03f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("lwDone", false));
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("lw"), 2);

	// Touch/mouse queries write their outputs (headless defaults).
	xComponent.FireCustomEvent("TouchGo");
	ZENITH_ASSERT_TRUE(xBlackboard.HasValue("touchHeld"));
	ZENITH_ASSERT_TRUE(xBlackboard.HasValue("touchPos"));
	ZENITH_ASSERT_TRUE(xBlackboard.HasValue("touchTap"));
	ZENITH_ASSERT_TRUE(xBlackboard.HasValue("mDelta"));
	ZENITH_ASSERT_TRUE(xBlackboard.HasValue("mWheel"));

	// Pick ray with no main camera anywhere gates its chain.
	if (Zenith_GetMainCameraAcrossScenes() == nullptr)
	{
		xComponent.FireCustomEvent("PickRayGo");
		ZENITH_ASSERT_FALSE(xBlackboard.GetBool("rayOk", false));
	}
}

ZENITH_TEST(GraphComponent, PhysicsNodeFamilyExecution)
{
	const std::string strAssetPath = "game:Graphs/UnitTest_PhysicsNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// "Impulse": velocity-delta impulse then read back.
		const u_int uImpulseSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uImpulseSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Impulse");
		}
		const u_int uImpulse = xDef.AddNode("ApplyImpulse");
		{
			NodeParamWriter xParams(xDef, uImpulse, "ApplyImpulse");
			SetVec3Param(xParams, "m_xImpulse", Zenith_Maths::Vector3(0.0f, 5.0f, 0.0f));
		}
		const u_int uReadVel = xDef.AddNode("ReadVelocity");
		{
			NodeParamWriter xParams(xDef, uReadVel, "ReadVelocity");
			xParams.SetString("m_strResultVar", "vel");
		}
		xDef.AddEdge(uImpulseSource, 0, uImpulse, 0);
		xDef.AddEdge(uImpulse, 0, uReadVel, 0);

		// "SetVel": per-axis preserve (Y kept).
		const u_int uSetVelSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uSetVelSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "SetVel");
		}
		const u_int uSetVel = xDef.AddNode("SetVelocity");
		{
			NodeParamWriter xParams(xDef, uSetVel, "SetVelocity");
			SetVec3Param(xParams, "m_xVelocity", Zenith_Maths::Vector3(3.0f, 99.0f, 4.0f));
			SetBoolParam(xParams, "m_bSetY", false);
		}
		const u_int uReadVel2 = xDef.AddNode("ReadVelocity");
		{
			NodeParamWriter xParams(xDef, uReadVel2, "ReadVelocity");
			xParams.SetString("m_strResultVar", "vel2");
		}
		xDef.AddEdge(uSetVelSource, 0, uSetVel, 0);
		xDef.AddEdge(uSetVel, 0, uReadVel2, 0);

		// "Angular" + body-state toggles.
		const u_int uAngularSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uAngularSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Angular");
		}
		const u_int uAngular = xDef.AddNode("SetAngularVelocity");
		{
			NodeParamWriter xParams(xDef, uAngular, "SetAngularVelocity");
			SetVec3Param(xParams, "m_xAngularVelocity", Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f));
		}
		const u_int uGravity = xDef.AddNode("SetGravityEnabled");
		{
			NodeParamWriter xParams(xDef, uGravity, "SetGravityEnabled");
			SetBoolParam(xParams, "m_bEnabled", false);
		}
		const u_int uForce = xDef.AddNode("ApplyForce");
		{
			NodeParamWriter xParams(xDef, uForce, "ApplyForce");
			SetVec3Param(xParams, "m_xForce", Zenith_Maths::Vector3(0.0f, 100.0f, 0.0f));
		}
		const u_int uLock = xDef.AddNode("LockRotation");
		const u_int uSensor = xDef.AddNode("SetSensor");
		{
			NodeParamWriter xParams(xDef, uSensor, "SetSensor");
			SetBoolParam(xParams, "m_bSensor", true);
		}
		const u_int uToggleFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uToggleFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "toggled");
		}
		xDef.AddEdge(uAngularSource, 0, uAngular, 0);
		xDef.AddEdge(uAngular, 0, uGravity, 0);
		xDef.AddEdge(uGravity, 0, uForce, 0);
		xDef.AddEdge(uForce, 0, uLock, 0);
		xDef.AddEdge(uLock, 0, uSensor, 0);
		xDef.AddEdge(uSensor, 0, uToggleFlag, 0);

		// "Cast": downward raycast (ignore self) hits the floor.
		const u_int uCastSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uCastSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Cast");
		}
		const u_int uCast = xDef.AddNode("Raycast");
		{
			NodeParamWriter xParams(xDef, uCast, "Raycast");
			SetVec3Param(xParams, "m_xDirection", Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f));
			SetFloatParam(xParams, "m_fMaxDistance", 100.0f);
			xParams.SetString("m_strHitDistanceVar", "hitDist");
		}
		const u_int uCastFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uCastFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "castHit");
		}
		xDef.AddEdge(uCastSource, 0, uCast, 0);
		xDef.AddEdge(uCast, 0, uCastFlag, 0);

		// "CastMiss": upward ray, FAILURE gates the chain.
		const u_int uMissSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uMissSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "CastMiss");
		}
		const u_int uMiss = xDef.AddNode("Raycast");
		{
			NodeParamWriter xParams(xDef, uMiss, "Raycast");
			SetVec3Param(xParams, "m_xDirection", Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
		}
		const u_int uMissFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uMissFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "missFlag");
		}
		xDef.AddEdge(uMissSource, 0, uMiss, 0);
		xDef.AddEdge(uMiss, 0, uMissFlag, 0);

		// "Teleport": zero velocities + TeleportBody, transform synced.
		const u_int uTeleportSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uTeleportSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Teleport");
		}
		const u_int uTeleport = xDef.AddNode("SetEntityPosition");
		{
			NodeParamWriter xParams(xDef, uTeleport, "SetEntityPosition");
			xParams.SetString("m_strPositionVar", "tpPos");
			SetBoolParam(xParams, "m_bTeleport", true);
		}
		xDef.AddEdge(uTeleportSource, 0, uTeleport, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphPhysicsNodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	Zenith_Entity xFloor = g_xEngine.Scenes().CreateEntity(pxSceneData, "PhysFloor");
	xFloor.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC);

	Zenith_Entity xBody = g_xEngine.Scenes().CreateEntity(pxSceneData, "PhysBody");
	xBody.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f));
	Zenith_ColliderComponent& xCollider = xBody.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	ZENITH_ASSERT_TRUE(xCollider.HasValidBody());

	Zenith_GraphComponent& xComponent = xBody.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);
	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();

	xComponent.FireCustomEvent("Impulse");
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("vel").y, 5.0f, 0.01f);

	xComponent.FireCustomEvent("SetVel");
	const Zenith_Maths::Vector3 xVelocity = xBlackboard.GetVector3("vel2");
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.x, 3.0f, 0.01f);
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.y, 5.0f, 0.01f);	// preserved, not 99
	ZENITH_ASSERT_EQ_FLOAT(xVelocity.z, 4.0f, 0.01f);

	xComponent.FireCustomEvent("Angular");
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("toggled"));
	ZENITH_ASSERT_EQ_FLOAT(g_xEngine.Physics().GetAngularVelocity(xCollider.GetBodyID()).y, 2.0f, 0.01f);

	xComponent.FireCustomEvent("Cast");
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("castHit"));
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("hitEntity"), xFloor.GetEntityID().GetPacked());
	ZENITH_ASSERT_TRUE(xBlackboard.GetFloat("hitDist") > 8.0f);

	xComponent.FireCustomEvent("CastMiss");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("missFlag", false));	// chain gated at the miss

	StageVec3(xBlackboard, "tpPos", Zenith_Maths::Vector3(5.0f, 20.0f, 5.0f));
	xComponent.FireCustomEvent("Teleport");
	Zenith_Maths::Vector3 xPosition;
	xBody.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);
	ZENITH_ASSERT_EQ_FLOAT(xPosition.x, 5.0f, 0.01f);
	ZENITH_ASSERT_EQ_FLOAT(xPosition.y, 20.0f, 0.01f);
	const Zenith_Maths::Vector3 xAfter = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xAfter), 0.0f, 0.001f);

	// SetGravityEnabled(false) made observable: fresh upward impulse, half a
	// second of real stepping - with gravity on, vy would have dropped ~4.9.
	xComponent.FireCustomEvent("Impulse");
	for (u_int u = 0; u < 30; ++u)
	{
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}
	ZENITH_ASSERT_TRUE(g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID()).y > 4.0f,
		"gravity still acting despite SetGravityEnabled(false)");
}

ZENITH_TEST(GraphComponent, AnimatorTweenParticleNodesExecution)
{
	const std::string strAssetPath = "game:Graphs/UnitTest_AnimNodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// "AnimGo": the four parameter writers + a crossfade.
		const u_int uAnimSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uAnimSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "AnimGo");
		}
		const u_int uFloat = xDef.AddNode("SetAnimatorFloat");
		{
			NodeParamWriter xParams(xDef, uFloat, "SetAnimatorFloat");
			xParams.SetString("m_strParameter", "Speed");
			SetFloatParam(xParams, "m_fValue", 2.5f);
		}
		const u_int uInt = xDef.AddNode("SetAnimatorInt");
		{
			NodeParamWriter xParams(xDef, uInt, "SetAnimatorInt");
			xParams.SetString("m_strParameter", "Phase");
			xParams.SetInt("m_iValue", 3);
		}
		const u_int uBool = xDef.AddNode("SetAnimatorBool");
		{
			NodeParamWriter xParams(xDef, uBool, "SetAnimatorBool");
			xParams.SetString("m_strParameter", "Grounded");
			SetBoolParam(xParams, "m_bValue", true);
		}
		const u_int uTrigger = xDef.AddNode("SetAnimatorTrigger");
		{
			NodeParamWriter xParams(xDef, uTrigger, "SetAnimatorTrigger");
			xParams.SetString("m_strParameter", "Attack");
		}
		const u_int uFade = xDef.AddNode("CrossFadeAnimation");
		{
			NodeParamWriter xParams(xDef, uFade, "CrossFadeAnimation");
			xParams.SetString("m_strState", "Run");
			SetFloatParam(xParams, "m_fDuration", 0.1f);
		}
		xDef.AddEdge(uAnimSource, 0, uFloat, 0);
		xDef.AddEdge(uFloat, 0, uInt, 0);
		xDef.AddEdge(uInt, 0, uBool, 0);
		xDef.AddEdge(uBool, 0, uTrigger, 0);
		xDef.AddEdge(uTrigger, 0, uFade, 0);

		// "ReadAnim": state info -> blackboard.
		const u_int uReadSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uReadSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "ReadAnim");
		}
		const u_int uRead = xDef.AddNode("ReadAnimatorState");
		{
			NodeParamWriter xParams(xDef, uRead, "ReadAnimatorState");
			xParams.SetString("m_strTransitioningVar", "animTrans");
		}
		xDef.AddEdge(uReadSource, 0, uRead, 0);

		// "TweenGo": scale tween (adds the component on demand).
		const u_int uTweenSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uTweenSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "TweenGo");
		}
		const u_int uTween = xDef.AddNode("TweenScale");
		{
			NodeParamWriter xParams(xDef, uTween, "TweenScale");
			SetVec3Param(xParams, "m_xTo", Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));
			SetFloatParam(xParams, "m_fDuration", 1.0f);
			xParams.SetInt("m_iEasing", EASING_LINEAR);
		}
		xDef.AddEdge(uTweenSource, 0, uTween, 0);

		// "RotGo": rotation tween, zero duration = completes on first tick.
		const u_int uRotSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uRotSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "RotGo");
		}
		const u_int uRot = xDef.AddNode("TweenRotation");
		{
			NodeParamWriter xParams(xDef, uRot, "TweenRotation");
			SetVec3Param(xParams, "m_xToEulerDegrees", Zenith_Maths::Vector3(0.0f, 90.0f, 0.0f));
			SetFloatParam(xParams, "m_fDuration", 0.0f);
		}
		xDef.AddEdge(uRotSource, 0, uRot, 0);

		// "WaitTween" -> flag; "StopGo" -> cancel; "MoveGo" -> position tween.
		const u_int uWaitSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uWaitSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "WaitTween");
		}
		const u_int uWaitTween = xDef.AddNode("WaitForTween");
		const u_int uWaitFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uWaitFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "twDone");
		}
		xDef.AddEdge(uWaitSource, 0, uWaitTween, 0);
		xDef.AddEdge(uWaitTween, 0, uWaitFlag, 0);

		const u_int uMoveSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uMoveSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "MoveGo");
		}
		const u_int uMove = xDef.AddNode("TweenPosition");
		{
			NodeParamWriter xParams(xDef, uMove, "TweenPosition");
			SetVec3Param(xParams, "m_xTo", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
			SetFloatParam(xParams, "m_fDuration", 0.2f);
		}
		xDef.AddEdge(uMoveSource, 0, uMove, 0);

		const u_int uStopSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uStopSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "StopGo");
		}
		const u_int uStop = xDef.AddNode("StopTweens");
		xDef.AddEdge(uStopSource, 0, uStop, 0);

		// Particle trio.
		const u_int uEmitSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uEmitSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "EmitGo");
		}
		const u_int uEmit = xDef.AddNode("EmitParticles");
		{
			NodeParamWriter xParams(xDef, uEmit, "EmitParticles");
			xParams.SetInt("m_iCount", 5);
		}
		const u_int uEmitToggle = xDef.AddNode("SetParticleEmitting");
		{
			NodeParamWriter xParams(xDef, uEmitToggle, "SetParticleEmitting");
			SetBoolParam(xParams, "m_bEmitting", false);
		}
		const u_int uEmitPos = xDef.AddNode("SetParticleEmitPosition");
		{
			NodeParamWriter xParams(xDef, uEmitPos, "SetParticleEmitPosition");
			xParams.SetString("m_strPositionVar", "epos");
			SetBoolParam(xParams, "m_bSetDirection", true);
			SetVec3Param(xParams, "m_xDirection", Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
		}
		const u_int uEmitFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uEmitFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "emitChainDone");
		}
		xDef.AddEdge(uEmitSource, 0, uEmit, 0);
		xDef.AddEdge(uEmit, 0, uEmitToggle, 0);
		xDef.AddEdge(uEmitToggle, 0, uEmitPos, 0);
		xDef.AddEdge(uEmitPos, 0, uEmitFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphAnimNodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AnimNodes");

	Zenith_AnimatorComponent& xAnimator = xEntity.AddComponent<Zenith_AnimatorComponent>();
	Flux_AnimationStateMachine* pxSM = xAnimator.CreateStateMachine();
	ZENITH_ASSERT_NOT_NULL(pxSM);
	if (!pxSM) return;
	pxSM->AddState("Idle");
	pxSM->AddState("Run");
	pxSM->SetDefaultState("Idle");
	pxSM->GetParameters().AddFloat("Speed");
	pxSM->GetParameters().AddInt("Phase");
	pxSM->GetParameters().AddBool("Grounded");
	pxSM->GetParameters().AddTrigger("Attack");

	Zenith_ParticleEmitterComponent& xEmitter = xEntity.AddComponent<Zenith_ParticleEmitterComponent>();
	static Flux_ParticleEmitterConfig ls_xGraphTestEmitterConfig;
	ls_xGraphTestEmitterConfig.m_uMaxParticles = 64;
	xEmitter.SetConfig(&ls_xGraphTestEmitterConfig);

	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);
	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();

	// Enter the default state, then drive the animator from the graph.
	Flux_SkeletonPose xPose;
	xPose.Initialize(2);
	Zenith_SkeletonAsset xSkeleton;
	pxSM->Update(0.0f, xPose, xSkeleton);

	xComponent.FireCustomEvent("AnimGo");
	ZENITH_ASSERT_EQ_FLOAT(pxSM->GetParameters().GetFloat("Speed"), 2.5f, 0.0001f);
	ZENITH_ASSERT_EQ(pxSM->GetParameters().GetInt("Phase"), 3);
	ZENITH_ASSERT_TRUE(pxSM->GetParameters().GetBool("Grounded"));
	ZENITH_ASSERT_TRUE(pxSM->GetParameters().PeekTrigger("Attack"));

	// Let the synthetic crossfade transition finish, then read state.
	pxSM->Update(0.5f, xPose, xSkeleton);
	pxSM->Update(0.5f, xPose, xSkeleton);
	xComponent.FireCustomEvent("ReadAnim");
	ZENITH_ASSERT_STREQ(xBlackboard.GetString("animState").c_str(), "Run");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("animTrans", true));

	// Tween: node adds the component on demand; ticked by hand (headless).
	xComponent.FireCustomEvent("TweenGo");
	Zenith_TweenComponent* pxTween = xEntity.TryGetComponent<Zenith_TweenComponent>();
	ZENITH_ASSERT_NOT_NULL(pxTween);
	if (!pxTween) return;
	ZENITH_ASSERT_TRUE(pxTween->HasActiveTweens());
	pxTween->OnUpdate(0.5f);
	pxTween->OnUpdate(0.5f);
	Zenith_Maths::Vector3 xScale;
	xEntity.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_EQ_FLOAT(xScale.x, 2.0f, 0.001f);
	ZENITH_ASSERT_FALSE(pxTween->HasActiveTweens());

	// Rotation tween (euler degrees): zero duration completes on first tick.
	xComponent.FireCustomEvent("RotGo");
	pxTween->OnUpdate(0.016f);
	Zenith_Maths::Quat xRotation;
	xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
	const Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xForward.x, 1.0f, 0.01f);

	// WaitForTween suspends while a tween runs, completes after - and the
	// position tween actually landed on its target.
	xComponent.FireCustomEvent("MoveGo");
	xComponent.FireCustomEvent("WaitTween");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("twDone", false));
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);	// still tweening (also ticks the tween)
	pxTween->OnUpdate(0.5f);	// finish it
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("twDone", false));
	Zenith_Maths::Vector3 xTweenedPos;
	xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xTweenedPos);
	ZENITH_ASSERT_EQ_FLOAT(xTweenedPos.x, 1.0f, 0.001f);

	// StopTweens cancels without snapping to the tween target.
	pxTween->TweenScale(Zenith_Maths::Vector3(10.0f), 5.0f);
	ZENITH_ASSERT_TRUE(pxTween->HasActiveTweens());
	xComponent.FireCustomEvent("StopGo");
	ZENITH_ASSERT_FALSE(pxTween->HasActiveTweens());
	xEntity.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	ZENITH_ASSERT_TRUE(xScale.x < 3.0f, "StopTweens snapped to the target scale (%f)", xScale.x);

	// Particles: burst + flag + world-space override (the override getters
	// are private - the chain-done flag pins the override path executed).
	StageVec3(xBlackboard, "epos", Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xComponent.FireCustomEvent("EmitGo");
	ZENITH_ASSERT_EQ(xEmitter.GetAliveCount(), 5u);
	ZENITH_ASSERT_FALSE(xEmitter.IsEmitting());
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("emitChainDone", false));
}

ZENITH_TEST(GraphComponent, UINodeFamilyExecution)
{
	const std::string strAssetPath = "game:Graphs/UnitTest_UINodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		const u_int uSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "UIGo");
		}
		const u_int uText = xDef.AddNode("SetUIText");
		{
			NodeParamWriter xParams(xDef, uText, "SetUIText");
			xParams.SetString("m_strElement", "Title");
			xParams.SetString("m_strText", "Score: {}");
			xParams.SetString("m_strValueVar", "score");
		}
		const u_int uColor = xDef.AddNode("SetUIColor");
		{
			NodeParamWriter xParams(xDef, uColor, "SetUIColor");
			xParams.SetString("m_strElement", "Title");
			SetVec4Param(xParams, "m_xColor", Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		}
		const u_int uVisible = xDef.AddNode("SetUIVisible");
		{
			NodeParamWriter xParams(xDef, uVisible, "SetUIVisible");
			xParams.SetString("m_strElement", "Bar");
			SetBoolParam(xParams, "m_bVisible", false);
		}
		const u_int uFill = xDef.AddNode("SetUIFillAmount");
		{
			NodeParamWriter xParams(xDef, uFill, "SetUIFillAmount");
			xParams.SetString("m_strElement", "Bar");
			SetFloatParam(xParams, "m_fAmount", 0.25f);
		}
		xDef.AddEdge(uSource, 0, uText, 0);
		xDef.AddEdge(uText, 0, uColor, 0);
		xDef.AddEdge(uColor, 0, uVisible, 0);
		xDef.AddEdge(uVisible, 0, uFill, 0);

		// Fill on a TEXT element must gate the chain (typed-element guard).
		const u_int uBadSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uBadSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "UIBad");
		}
		const u_int uBadFill = xDef.AddNode("SetUIFillAmount");
		{
			NodeParamWriter xParams(xDef, uBadFill, "SetUIFillAmount");
			xParams.SetString("m_strElement", "Title");
		}
		const u_int uBadFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uBadFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "badFill");
		}
		xDef.AddEdge(uBadSource, 0, uBadFill, 0);
		xDef.AddEdge(uBadFill, 0, uBadFlag, 0);

		// Button trampoline: ON_UPDATE-anchored self-wiring source.
		const u_int uClickSource = xDef.AddNode("OnUIButtonClicked");
		{
			NodeParamWriter xParams(xDef, uClickSource, "OnUIButtonClicked");
			xParams.SetString("m_strButton", "Play");
		}
		const u_int uClickFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uClickFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "clicked");
		}
		xDef.AddEdge(uClickSource, 0, uClickFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	Zenith_TempScene xTempScene("TestGraphUINodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "UINodes");

	Zenith_UIComponent& xUI = xEntity.AddComponent<Zenith_UIComponent>();
	Zenith_UI::Zenith_UIText* pxTitle = xUI.CreateText("Title", "placeholder");
	Zenith_UI::Zenith_UIRect* pxBar = xUI.CreateRect("Bar");
	Zenith_UI::Zenith_UIButton* pxPlay = xUI.CreateButton("Play", "Play");
	ZENITH_ASSERT_NOT_NULL(pxTitle);
	ZENITH_ASSERT_NOT_NULL(pxBar);
	ZENITH_ASSERT_NOT_NULL(pxPlay);
	if (!pxTitle || !pxBar || !pxPlay) return;

	Zenith_GraphComponent& xComponent = xEntity.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);
	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();

	StageInt(xBlackboard, "score", 42);
	xComponent.FireCustomEvent("UIGo");
	ZENITH_ASSERT_STREQ(pxTitle->GetText().c_str(), "Score: 42");
	ZENITH_ASSERT_EQ_FLOAT(pxTitle->GetColor().x, 1.0f, 0.001f);
	ZENITH_ASSERT_EQ_FLOAT(pxTitle->GetColor().y, 0.0f, 0.001f);
	ZENITH_ASSERT_FALSE(pxBar->IsVisible());
	ZENITH_ASSERT_EQ_FLOAT(pxBar->GetFillAmount(), 0.25f, 0.001f);

	xComponent.FireCustomEvent("UIBad");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("badFill", false));	// wrong element type gated

	// Trampoline: first update wires the button; Activate() records a click;
	// the next update fires the chain exactly once.
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("clicked", false));
	pxPlay->Activate();
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("clicked", false));

	Zenith_PropertyValue xReset;
	xReset.SetBool(false);
	xBlackboard.SetValue("clicked", xReset);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xEntity, 0.016f);
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("clicked", false));	// no new click, no re-fire
}

ZENITH_TEST(GraphComponent, AINavPerceptionNodesExecution)
{
	// Hand-authored 10x10 navmesh quad (the AI suite fixture).
	Zenith_NavMesh xNavMesh;
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(10.0f, 0.0f, 10.0f));
	xNavMesh.AddVertex(Zenith_Maths::Vector3(0.0f, 0.0f, 10.0f));
	Zenith_Vector<uint32_t> axIndices;
	axIndices.PushBack(0);
	axIndices.PushBack(1);
	axIndices.PushBack(2);
	axIndices.PushBack(3);
	xNavMesh.AddPolygon(axIndices);
	xNavMesh.ComputeSpatialData();
	xNavMesh.BuildSpatialGrid();

	const std::string strAssetPath = "game:Graphs/UnitTest_AINodes.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		// "Go" -> NavMoveTo -> arrived flag (one-shot anchor, re-driven by
		// the update dispatch until it completes).
		const u_int uGoSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uGoSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Go");
		}
		const u_int uMove = xDef.AddNode("NavMoveTo");
		{
			NodeParamWriter xParams(xDef, uMove, "NavMoveTo");
			xParams.SetString("m_strDestinationVar", "dest");
			SetFloatParam(xParams, "m_fAcceptanceRadius", 1.0f);
		}
		const u_int uArrived = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uArrived, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "arrived");
		}
		xDef.AddEdge(uGoSource, 0, uMove, 0);
		xDef.AddEdge(uMove, 0, uArrived, 0);

		// "Speed" / "ReadNav" / "Stop" / "Wander" utility chains.
		const u_int uSpeedSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uSpeedSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Speed");
		}
		const u_int uSpeed = xDef.AddNode("SetNavSpeed");
		{
			NodeParamWriter xParams(xDef, uSpeed, "SetNavSpeed");
			SetFloatParam(xParams, "m_fSpeed", 10.0f);
		}
		xDef.AddEdge(uSpeedSource, 0, uSpeed, 0);

		const u_int uReadSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uReadSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "ReadNav");
		}
		const u_int uRead = xDef.AddNode("ReadNavState");
		{
			NodeParamWriter xParams(xDef, uRead, "ReadNavState");
			xParams.SetString("m_strRemainingVar", "navLeft");
		}
		xDef.AddEdge(uReadSource, 0, uRead, 0);

		const u_int uStopSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uStopSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Stop");
		}
		const u_int uStop = xDef.AddNode("StopNav");
		xDef.AddEdge(uStopSource, 0, uStop, 0);

		const u_int uDestSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uDestSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Dest");
		}
		const u_int uDest = xDef.AddNode("SetNavDestination");
		{
			NodeParamWriter xParams(xDef, uDest, "SetNavDestination");
			xParams.SetString("m_strDestinationVar", "dest");
		}
		xDef.AddEdge(uDestSource, 0, uDest, 0);

		const u_int uWanderSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uWanderSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Wander");
		}
		const u_int uWander = xDef.AddNode("FindRandomReachablePoint");
		{
			NodeParamWriter xParams(xDef, uWander, "FindRandomReachablePoint");
			SetFloatParam(xParams, "m_fRadius", 8.0f);
		}
		xDef.AddEdge(uWanderSource, 0, uWander, 0);

		// Perception chains.
		const u_int uRegSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uRegSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Reg");
		}
		const u_int uReg = xDef.AddNode("RegisterPerceptionTarget");
		{
			NodeParamWriter xParams(xDef, uReg, "RegisterPerceptionTarget");
			xParams.SetString("m_strTargetVar", "tgt");
		}
		xDef.AddEdge(uRegSource, 0, uReg, 0);

		const u_int uQuerySource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uQuerySource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Query");
		}
		const u_int uList = xDef.AddNode("QueryPerceivedTargets");
		const u_int uPrimary = xDef.AddNode("QueryPrimaryPerceivedTarget");
		{
			NodeParamWriter xParams(xDef, uPrimary, "QueryPrimaryPerceivedTarget");
			xParams.SetString("m_strResultVar", "ptgt");
		}
		const u_int uAware = xDef.AddNode("QueryAwarenessOf");
		{
			NodeParamWriter xParams(xDef, uAware, "QueryAwarenessOf");
			xParams.SetString("m_strOfVar", "tgt");
		}
		xDef.AddEdge(uQuerySource, 0, uList, 0);
		xDef.AddEdge(uList, 0, uPrimary, 0);
		xDef.AddEdge(uPrimary, 0, uAware, 0);

		const u_int uNoiseSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uNoiseSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Noise");
		}
		const u_int uNoise = xDef.AddNode("EmitSoundStimulus");
		{
			NodeParamWriter xParams(xDef, uNoise, "EmitSoundStimulus");
			xParams.SetString("m_strTargetVar", "tgt");	// source = the target entity
			SetFloatParam(xParams, "m_fLoudness", 0.5f);
			SetFloatParam(xParams, "m_fRadius", 10.0f);
		}
		xDef.AddEdge(uNoiseSource, 0, uNoise, 0);

		const u_int uHeardSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uHeardSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Heard");
		}
		const u_int uHeard = xDef.AddNode("QueryLastHeardSound");
		{
			NodeParamWriter xParams(xDef, uHeard, "QueryLastHeardSound");
			xParams.SetString("m_strSourceVar", "heardSrc");
		}
		const u_int uHeardFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uHeardFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "heardOk");
		}
		xDef.AddEdge(uHeardSource, 0, uHeard, 0);
		xDef.AddEdge(uHeard, 0, uHeardFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	// StateMachine-hosted NavMoveTo for the OnAbort -> Stop path.
	const std::string strAbortPath = "game:Graphs/UnitTest_AINavAbort.bgraph";
	{
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();
		const u_int uUpdate = xDef.AddNode("OnUpdate");
		const u_int uMachine = xDef.AddNode("StateMachine");
		{
			NodeParamWriter xParams(xDef, uMachine, "StateMachine");
			xParams.SetString("m_strStateVar", "st");
			xParams.SetInt("m_iStateCount", 2);
		}
		const u_int uMove = xDef.AddNode("NavMoveTo");
		{
			NodeParamWriter xParams(xDef, uMove, "NavMoveTo");
			xParams.SetString("m_strDestinationVar", "dest");
		}
		const u_int uIdleFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uIdleFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "inIdle");
		}
		xDef.AddEdge(uUpdate, 0, uMachine, 0);
		xDef.AddEdge(uMachine, 0, uMove, 0);
		xDef.AddEdge(uMachine, 1, uIdleFlag, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAbortPath);
	}

	Zenith_TempScene xTempScene("TestGraphAINodesScene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();

	Zenith_Entity xAgent = g_xEngine.Scenes().CreateEntity(pxSceneData, "AINodesAgent");
	xAgent.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	Zenith_NavMeshAgent xNavAgent;
	xNavAgent.SetNavMesh(&xNavMesh);
	xAgent.AddComponent<Zenith_AIAgentComponent>().SetNavMeshAgent(&xNavAgent);

	Zenith_GraphComponent& xComponent = xAgent.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);
	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();

	// NavMoveTo: RUNNING until real arrival (dispatch runs Graph then the
	// AIAgent component, which ticks the nav agent). Default speed (5) with
	// small dt so each 0.1-unit step can land inside the 0.2-unit stopping
	// window - big steps orbit the final waypoint forever.
	StageVec3(xBlackboard, "dest", Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));
	xComponent.FireCustomEvent("Go");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("arrived", false));
	for (u_int u = 0; u < 600 && !xBlackboard.GetBool("arrived", false); ++u)
	{
		Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xAgent, 0.02f);
	}
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("arrived", false), "NavMoveTo never arrived");

	// Success path Stop()s the agent (BT MoveToEntity semantics), so the
	// post-arrival decode is 0 = idle/none, remaining 0.
	xComponent.FireCustomEvent("ReadNav");
	ZENITH_ASSERT_EQ(xBlackboard.GetInt32("navState"), 0);
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("navLeft"), 0.0f, 0.001f);

	// Fire-and-forget destination issue queues a path request.
	StageVec3(xBlackboard, "dest", Zenith_Maths::Vector3(2.0f, 0.0f, 2.0f));
	xComponent.FireCustomEvent("Dest");
	ZENITH_ASSERT_TRUE(xNavAgent.NeedsPath() || xNavAgent.HasPath());

	xComponent.FireCustomEvent("Speed");
	ZENITH_ASSERT_EQ_FLOAT(xNavAgent.GetMoveSpeed(), 10.0f, 0.001f);

	xComponent.FireCustomEvent("Stop");
	ZENITH_ASSERT_FALSE(xNavAgent.HasPath());

	Zenith_Maths::Vector3 xAgentPos;
	xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xAgentPos);
	xComponent.FireCustomEvent("Wander");
	const Zenith_Maths::Vector3 xWander = xBlackboard.GetVector3("wanderPoint", Zenith_Maths::Vector3(-100.0f));
	ZENITH_ASSERT_TRUE(xWander.x >= -0.01f && xWander.x <= 10.01f && xWander.z >= -0.01f && xWander.z <= 10.01f,
		"wander point off-mesh: (%f, %f, %f)", xWander.x, xWander.y, xWander.z);
	const float fWanderDX = xWander.x - xAgentPos.x;
	const float fWanderDZ = xWander.z - xAgentPos.z;
	ZENITH_ASSERT_TRUE(fWanderDX * fWanderDX + fWanderDZ * fWanderDZ <= 8.0f * 8.0f + 0.01f,
		"wander point outside the 8-unit XZ radius");

	// StateMachine transition aborts the RUNNING NavMoveTo (OnAbort -> Stop).
	Zenith_Entity xAgent2 = g_xEngine.Scenes().CreateEntity(pxSceneData, "AINodesAgent2");
	xAgent2.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(1.0f, 0.0f, 1.0f));
	Zenith_NavMeshAgent xNavAgent2;
	xNavAgent2.SetNavMesh(&xNavMesh);
	xAgent2.AddComponent<Zenith_AIAgentComponent>().SetNavMeshAgent(&xNavAgent2);
	Zenith_BehaviourGraph* pxAbortGraph = xAgent2.AddComponent<Zenith_GraphComponent>().AddGraphByAssetPath(strAbortPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxAbortGraph);
	if (!pxAbortGraph) return;
	StageVec3(pxAbortGraph->GetBlackboard(), "dest", Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f));
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xAgent2, 0.1f);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xAgent2, 0.1f);
	ZENITH_ASSERT_TRUE(xNavAgent2.HasPath());
	StageInt(pxAbortGraph->GetBlackboard(), "st", 1);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xAgent2, 0.1f);
	ZENITH_ASSERT_FALSE(xNavAgent2.HasPath());	// NavMoveTo::OnAbort stopped the agent
	ZENITH_ASSERT_TRUE(pxAbortGraph->GetBlackboard().GetBool("inIdle", false));

	// Perception: agent at origin facing +Z, target 5 ahead; LOS off.
	Zenith_PerceptionSystem::Initialise();
	Zenith_Entity xSeer = g_xEngine.Scenes().CreateEntity(pxSceneData, "AISeer");
	Zenith_Entity xPrey = g_xEngine.Scenes().CreateEntity(pxSceneData, "AIPrey");
	xSeer.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xPrey.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(0.0f, 0.0f, 5.0f));
	Zenith_PerceptionSystem::RegisterAgent(xSeer.GetEntityID());
	Zenith_SightConfig xSight;
	xSight.m_fMaxRange = 20.0f;
	xSight.m_fFOVAngle = 90.0f;
	xSight.m_bRequireLineOfSight = false;
	Zenith_PerceptionSystem::SetSightConfig(xSeer.GetEntityID(), xSight);
	Zenith_HearingConfig xHearing;	// defaults: 20m range, 0.1 threshold
	Zenith_PerceptionSystem::SetHearingConfig(xSeer.GetEntityID(), xHearing);

	Zenith_GraphComponent& xSeerComponent = xSeer.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxSeerGraph = xSeerComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxSeerGraph);
	if (!pxSeerGraph) { Zenith_PerceptionSystem::Shutdown(); return; }
	Zenith_GraphBlackboard& xSeerBlackboard = pxSeerGraph->GetBlackboard();
	StageEntity(xSeerBlackboard, "tgt", xPrey);

	xSeerComponent.FireCustomEvent("Reg");	// RegisterPerceptionTarget(tgt)
	Zenith_PerceptionSystem::Update(0.1f);
	xSeerComponent.FireCustomEvent("Query");
	ZENITH_ASSERT_TRUE(xSeerBlackboard.GetInt32("perceivedCount") >= 1);
	ZENITH_ASSERT_EQ(xSeerBlackboard.GetPackedEntityID("ptgt"), xPrey.GetEntityID().GetPacked());
	ZENITH_ASSERT_TRUE(xSeerBlackboard.GetFloat("awareness") > 0.0f);

	xSeerComponent.FireCustomEvent("Noise");	// source = prey, position = seer
	Zenith_PerceptionSystem::Update(0.1f);
	xSeerComponent.FireCustomEvent("Heard");
	ZENITH_ASSERT_TRUE(xSeerBlackboard.GetBool("heardOk", false));
	ZENITH_ASSERT_EQ(xSeerBlackboard.GetPackedEntityID("heardSrc"), xPrey.GetEntityID().GetPacked());

	Zenith_PerceptionSystem::Shutdown();
}

ZENITH_TEST(GraphComponent, EntityNodeFamilyRemainderExecution)
{
	// In-memory prefab fixture (the Test_ShootCharacterization recipe; the
	// engine lib has no GAME_ASSETS_DIR define - resolve through the registry).
	Zenith_TempScene xTempScene("TestGraphEntity2Scene");
	Zenith_SceneData* pxSceneData = xTempScene.Data();
	const std::string strPrefabPath = std::string("game:Prefabs/UnitTestGraphSpawn") + ZENITH_PREFAB_EXT;
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Prefabs"), xEC);
		Zenith_Entity xTemplate = g_xEngine.Scenes().CreateEntity(pxSceneData, "SpawnTemplate");
		Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::Create<Zenith_Prefab>();
		pxPrefab->CreateFromEntity(xTemplate, "GraphSpawnFixture");
		pxPrefab->SaveToFile(Zenith_AssetRegistry::ResolvePath(strPrefabPath));
		xTemplate.Destroy();
	}

	const std::string strAssetPath = "game:Graphs/UnitTest_EntityNodes2.bgraph";
	{
		std::error_code xEC;
		std::filesystem::create_directories(Zenith_AssetRegistry::ResolvePath("game:Graphs"), xEC);
		Zenith_BehaviourGraphAsset xAsset;
		Zenith_GraphDefinition& xDef = xAsset.GetDefinition();

		const u_int uSpawnSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uSpawnSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Spawn");
		}
		const u_int uSpawn = xDef.AddNode("SpawnPrefab");
		{
			NodeParamWriter xParams(xDef, uSpawn, "SpawnPrefab");
			xParams.SetString("m_strPrefabPath", strPrefabPath.c_str());
			xParams.SetString("m_strEntityName", "SpawnedByGraph");
			xParams.SetString("m_strPositionVar", "spawnAt");
		}
		xDef.AddEdge(uSpawnSource, 0, uSpawn, 0);

		const u_int uFindSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uFindSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Find");
		}
		const u_int uFindName = xDef.AddNode("FindEntityByName");
		{
			NodeParamWriter xParams(xDef, uFindName, "FindEntityByName");
			xParams.SetString("m_strName", "SpawnedByGraph");
		}
		const u_int uNearest = xDef.AddNode("FindNearestEntity");
		{
			NodeParamWriter xParams(xDef, uNearest, "FindNearestEntity");
			SetFloatParam(xParams, "m_fRadius", 50.0f);
			xParams.SetString("m_strDistanceVar", "nearDist");
		}
		xDef.AddEdge(uFindSource, 0, uFindName, 0);
		xDef.AddEdge(uFindName, 0, uNearest, 0);

		const u_int uMissSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uMissSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "FindMiss");
		}
		const u_int uFindMiss = xDef.AddNode("FindEntityByName");
		{
			NodeParamWriter xParams(xDef, uFindMiss, "FindEntityByName");
			xParams.SetString("m_strName", "NoSuchEntityAnywhere");
		}
		const u_int uMissFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uMissFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "foundMissing");
		}
		xDef.AddEdge(uMissSource, 0, uFindMiss, 0);
		xDef.AddEdge(uFindMiss, 0, uMissFlag, 0);

		const u_int uAttachSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uAttachSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Attach");
		}
		const u_int uAttach = xDef.AddNode("AttachToBone");
		{
			NodeParamWriter xParams(xDef, uAttach, "AttachToBone");
			xParams.SetString("m_strSkeletonVar", "skel");
			xParams.SetString("m_strBone", "RightHand");
		}
		xDef.AddEdge(uAttachSource, 0, uAttach, 0);

		const u_int uDetachSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uDetachSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Detach");
		}
		const u_int uDetach = xDef.AddNode("DetachFromBone");
		xDef.AddEdge(uDetachSource, 0, uDetach, 0);

		const u_int uRotSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uRotSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Face");
		}
		const u_int uRotate = xDef.AddNode("RotateTowardDirection");
		{
			NodeParamWriter xParams(xDef, uRotate, "RotateTowardDirection");
			xParams.SetString("m_strDirectionVar", "faceDir");
			SetFloatParam(xParams, "m_fDegreesPerSecond", 0.0f);	// snap
		}
		const u_int uReadRot = xDef.AddNode("ReadEntityRotation");
		{
			NodeParamWriter xParams(xDef, uReadRot, "ReadEntityRotation");
			xParams.SetString("m_strEulerVar", "euler");
		}
		xDef.AddEdge(uRotSource, 0, uRotate, 0);
		xDef.AddEdge(uRotate, 0, uReadRot, 0);

		// Rate-limited turn needs real dt: OnUpdate-anchored, gated so it
		// only runs when the test opens the gate.
		const u_int uTurnSource = xDef.AddNode("OnUpdate");
		const u_int uTurnGate = xDef.AddNode("Gate");
		{
			NodeParamWriter xParams(xDef, uTurnGate, "Gate");
			xParams.SetString("m_strOpenVar", "turnGate");
		}
		const u_int uTurn = xDef.AddNode("RotateTowardDirection");
		{
			NodeParamWriter xParams(xDef, uTurn, "RotateTowardDirection");
			xParams.SetString("m_strDirectionVar", "faceDir");
			SetFloatParam(xParams, "m_fDegreesPerSecond", 90.0f);	// rate-limited
		}
		xDef.AddEdge(uTurnSource, 0, uTurnGate, 0);
		xDef.AddEdge(uTurnGate, 0, uTurn, 0);

		// P1-node coverage riders: direction, scale, radius query.
		const u_int uDirSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uDirSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Aux");
		}
		const u_int uDir = xDef.AddNode("ComputeDirection");
		{
			NodeParamWriter xParams(xDef, uDir, "ComputeDirection");
			xParams.SetString("m_strToVar", "auxTarget");
			xParams.SetString("m_strResultVar", "auxDir");
		}
		const u_int uScale = xDef.AddNode("SetEntityScale");
		{
			NodeParamWriter xParams(xDef, uScale, "SetEntityScale");
			SetVec3Param(xParams, "m_xScale", Zenith_Maths::Vector3(2.0f, 2.0f, 2.0f));
		}
		const u_int uRadius = xDef.AddNode("FindEntitiesInRadius");
		{
			NodeParamWriter xParams(xDef, uRadius, "FindEntitiesInRadius");
			SetFloatParam(xParams, "m_fRadius", 50.0f);
			xParams.SetString("m_strListVar", "inRange");
			xParams.SetString("m_strCountVar", "inRangeCount");
		}
		xDef.AddEdge(uDirSource, 0, uDir, 0);
		xDef.AddEdge(uDir, 0, uScale, 0);
		xDef.AddEdge(uScale, 0, uRadius, 0);

		// Pick ray success path (fired after the test seam sets a camera).
		const u_int uRaySource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uRaySource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Ray");
		}
		const u_int uRay = xDef.AddNode("ReadMousePickRay");
		const u_int uRayFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uRayFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "rayOk");
		}
		xDef.AddEdge(uRaySource, 0, uRay, 0);
		xDef.AddEdge(uRay, 0, uRayFlag, 0);

		const u_int uCamSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uCamSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Cam");
		}
		const u_int uBasis = xDef.AddNode("ReadCameraBasis");
		const u_int uCamFlag = xDef.AddNode("SetBlackboardBool");
		{
			NodeParamWriter xParams(xDef, uCamFlag, "SetBlackboardBool");
			xParams.SetString("m_strVariable", "camOk");
		}
		xDef.AddEdge(uCamSource, 0, uBasis, 0);
		xDef.AddEdge(uBasis, 0, uCamFlag, 0);

		const u_int uPitchSource = xDef.AddNode("OnCustomEvent");
		{
			NodeParamWriter xParams(xDef, uPitchSource, "OnCustomEvent");
			xParams.SetString("m_strEventName", "Pitch");
		}
		const u_int uPitch = xDef.AddNode("SetCameraPitchYaw");
		{
			NodeParamWriter xParams(xDef, uPitch, "SetCameraPitchYaw");
			SetFloatParam(xParams, "m_fPitchDegrees", -120.0f);	// clamps to -89
			SetFloatParam(xParams, "m_fYawDegrees", 90.0f);
		}
		xDef.AddEdge(uPitchSource, 0, uPitch, 0);
		Zenith_AssetRegistry::Save(&xAsset, strAssetPath);
	}

	// The cluster sits far from the origin so FindNearestEntity (radius 50,
	// all scenes) cannot pick up origin-dwelling fixtures from other tests
	// or the persistent scene.
	Zenith_Entity xHost = g_xEngine.Scenes().CreateEntity(pxSceneData, "EntityNodes2Host");
	xHost.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f));
	Zenith_Entity xNear = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2Near");
	xNear.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(102.0f, 0.0f, 0.0f));
	Zenith_Entity xSkeleton = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2Skeleton");
	xSkeleton.GetComponent<Zenith_TransformComponent>().SetPosition(Zenith_Maths::Vector3(150.0f, 0.0f, 0.0f));

	Zenith_GraphComponent& xComponent = xHost.AddComponent<Zenith_GraphComponent>();
	Zenith_BehaviourGraph* pxGraph = xComponent.AddGraphByAssetPath(strAssetPath.c_str());
	ZENITH_ASSERT_NOT_NULL(pxGraph);
	if (!pxGraph) return;
	ZENITH_ASSERT_EQ(pxGraph->GetUnresolvedCount(), 0u);
	Zenith_GraphBlackboard& xBlackboard = pxGraph->GetBlackboard();

	// SpawnPrefab at a staged position (dist ~7.07 from the host - farther
	// than Near, inside the FindNearest radius).
	StageVec3(xBlackboard, "spawnAt", Zenith_Maths::Vector3(103.0f, 4.0f, 5.0f));
	xComponent.FireCustomEvent("Spawn");
	const u_int64 ulSpawned = xBlackboard.GetPackedEntityID("spawned");
	ZENITH_ASSERT_TRUE(ulSpawned != 0);
	Zenith_Entity xSpawned = g_xEngine.Scenes().ResolveEntity(Zenith_EntityID::FromPacked(ulSpawned));
	ZENITH_ASSERT_TRUE(xSpawned.IsValid());
	if (xSpawned.IsValid())
	{
		Zenith_Maths::Vector3 xSpawnPos;
		xSpawned.GetComponent<Zenith_TransformComponent>().GetPosition(xSpawnPos);
		ZENITH_ASSERT_EQ_FLOAT(xSpawnPos.y, 4.0f, 0.001f);
	}

	// FindEntityByName + FindNearestEntity (host at origin; Near at 2,0,0 -
	// but the spawned entity sits at (3,4,5), dist ~7.07, so Near wins).
	xComponent.FireCustomEvent("Find");
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("found"), ulSpawned);
	ZENITH_ASSERT_EQ(xBlackboard.GetPackedEntityID("nearest"), xNear.GetEntityID().GetPacked());
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetFloat("nearDist"), 2.0f, 0.01f);

	xComponent.FireCustomEvent("FindMiss");
	ZENITH_ASSERT_FALSE(xBlackboard.GetBool("foundMissing", false));

	// Attach/detach state (bone follow itself needs a real skeleton; the
	// state machine is what the node owns).
	StageEntity(xBlackboard, "skel", xSkeleton);
	xComponent.FireCustomEvent("Attach");
	Zenith_AttachmentComponent* pxAttachment = xHost.TryGetComponent<Zenith_AttachmentComponent>();
	ZENITH_ASSERT_NOT_NULL(pxAttachment);
	if (pxAttachment)
	{
		ZENITH_ASSERT_TRUE(pxAttachment->IsAttached());
		xComponent.FireCustomEvent("Detach");
		ZENITH_ASSERT_FALSE(pxAttachment->IsAttached());
	}

	// RotateTowardDirection snap + euler read, then the rate-limited path
	// under a real update dt: 90 deg/s * 0.5s = 45 degrees from +X toward -Z.
	StageVec3(xBlackboard, "faceDir", Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xComponent.FireCustomEvent("Face");
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("forward").x, 1.0f, 0.01f);

	StageVec3(xBlackboard, "faceDir", Zenith_Maths::Vector3(0.0f, 0.0f, -1.0f));
	Zenith_PropertyValue xGateOpen;
	xGateOpen.SetBool(true);
	xBlackboard.SetValue("turnGate", xGateOpen);
	Zenith_ComponentMetaRegistry::Get().DispatchOnUpdate(xHost, 0.5f);
	xGateOpen.SetBool(false);
	xBlackboard.SetValue("turnGate", xGateOpen);
	Zenith_Maths::Quat xRotation;
	xHost.GetComponent<Zenith_TransformComponent>().GetRotation(xRotation);
	Zenith_Maths::Vector3 xForward = xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_EQ_FLOAT(xForward.x, 0.7071f, 0.02f);	// halfway to -Z
	ZENITH_ASSERT_EQ_FLOAT(xForward.z, -0.7071f, 0.02f);

	// P1-coverage riders: direction to Near, scale, radius query.
	StageEntity(xBlackboard, "auxTarget", xNear);
	xComponent.FireCustomEvent("Aux");
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("auxDir").x, 1.0f, 0.01f);	// host -> Near = +X
	Zenith_Maths::Vector3 xHostScale;
	xHost.GetComponent<Zenith_TransformComponent>().GetScale(xHostScale);
	ZENITH_ASSERT_EQ_FLOAT(xHostScale.x, 2.0f, 0.001f);
	ZENITH_ASSERT_TRUE(xBlackboard.GetInt32("inRangeCount") >= 2);	// Near + spawned at least

	// Camera nodes: FAILURE without a main camera, success once the test
	// seam assigns one.
	if (Zenith_GetMainCameraAcrossScenes() == nullptr)
	{
		xComponent.FireCustomEvent("Cam");
		ZENITH_ASSERT_FALSE(xBlackboard.GetBool("camOk", false));
	}
	Zenith_Entity xCamera = g_xEngine.Scenes().CreateEntity(pxSceneData, "Entity2Camera");
	Zenith_CameraComponent& xCameraComponent = xCamera.AddComponent<Zenith_CameraComponent>();
	Zenith_UnitTests::SetMainCameraForTest(pxSceneData, xCamera.GetEntityID());
	ZENITH_ASSERT_NOT_NULL(Zenith_GetMainCameraAcrossScenes());

	xCameraComponent.SetPitch(0.0);
	xCameraComponent.SetYaw(0.0);
	xComponent.FireCustomEvent("Cam");
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("camOk", false));
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("camForward").z, 1.0f, 0.01f);	// yaw 0 faces +Z
	ZENITH_ASSERT_EQ_FLOAT(xBlackboard.GetVector3("camRight").x, 1.0f, 0.01f);

	xComponent.FireCustomEvent("Pitch");
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCameraComponent.GetPitch()), glm::radians(-89.0f), 0.001f);	// clamped
	ZENITH_ASSERT_EQ_FLOAT(static_cast<float>(xCameraComponent.GetYaw()), glm::radians(90.0f), 0.001f);

	// Pick ray success path: a normalized direction lands on the blackboard.
	xComponent.FireCustomEvent("Ray");
	ZENITH_ASSERT_TRUE(xBlackboard.GetBool("rayOk", false));
	ZENITH_ASSERT_EQ_FLOAT(glm::length(xBlackboard.GetVector3("rayDir")), 1.0f, 0.01f);
}

ZENITH_TEST(GraphComponent, RegistryWideNodeRoundTrip)
{
	// Every registered node type: instantiate -> params-from-default-instance
	// (parameterless nodes skipped: SetNodeParamsFromInstance rejects them) ->
	// definition serialize -> reload -> everything resolves.
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	const u_int uTypeCount = xRegistry.GetTypeCount();
	ZENITH_ASSERT_TRUE(uTypeCount >= 100u, "engine registry unexpectedly small: %u", uTypeCount);

	Zenith_GraphDefinition xDef;
	for (u_int u = 0; u < uTypeCount; ++u)
	{
		const Zenith_GraphNodeTypeInfo& xInfo = xRegistry.GetTypeAt(u);
		const u_int uNode = xDef.AddNode(xInfo.m_strTypeName.c_str());
		if (xInfo.m_pfnGetPropertyTable != nullptr && xInfo.m_pfnGetPropertyTable()->GetPropertyCount() > 0)
		{
			Zenith_GraphNode* pxTemp = xInfo.m_pfnCreate();
			xDef.SetNodeParamsFromInstance(uNode, pxTemp);
			delete pxTemp;
		}
	}

	Zenith_DataStream xStream;
	xDef.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	Zenith_GraphDefinition xReloaded;
	ZENITH_ASSERT_TRUE(xReloaded.ReadFromDataStream(xStream));
	ZENITH_ASSERT_EQ(xReloaded.GetNodeCount(), uTypeCount);

	Zenith_BehaviourGraph xGraph;
	xGraph.InitialiseFromDefinition(xReloaded);
	ZENITH_ASSERT_EQ(xGraph.GetUnresolvedCount(), 0u);
}

#endif // ZENITH_TESTING
