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
#include <chrono>
#include <filesystem>

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
}

#endif // ZENITH_TESTING
