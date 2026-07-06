#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"

#include <cstring>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Scene domain.
//
// Scene management wraps Zenith_SceneSystem. LOADS requested mid-update drain
// at the safe point; UNLOADS take effect immediately - unloading a DIFFERENT
// scene mid-dispatch is safe (the scene-update loop re-resolves its snapshot
// per iteration), but a graph must never synchronously unload ITS OWN scene
// (the dispatching SceneData is on the stack) - tear your own scene down via
// a SINGLE-mode load instead, placed at the END of a chain (the dispatching
// entity does not survive the drain).
//
// OnSceneLoaded rides the "__SceneLoaded" broadcast fired by the engine's
// m_pfnSceneLoaded runtime hook at LoadScene completion (file-backed loads;
// the canonical path travels as the string payload).
//------------------------------------------------------------------------------

namespace
{
	// Loads a scene by path. Mode: 0 = single, 1 = additive, 2 = additive
	// without loading (empty procedural scene).
	class Zenith_GraphNode_LoadSceneByAsset : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_LoadSceneByAsset)
	public:
		ZENITH_PROPERTY(std::string, m_strScenePath, "")
		ZENITH_PROPERTY_RANGED(int32_t, m_iLoadMode, 0, 0, 2)

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			if (m_strScenePath.empty())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			g_xEngine.Scenes().LoadScene(m_strScenePath, static_cast<Zenith_SceneLoadMode>(m_iLoadMode));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "LoadSceneByAsset"; }
	};

	// Unloads a loaded scene by path ("" = the active scene).
	class Zenith_GraphNode_UnloadScene : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_UnloadScene)
	public:
		ZENITH_PROPERTY(std::string, m_strScenePath, "")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
			const Zenith_Scene xScene = m_strScenePath.empty()
				? xScenes.GetActiveScene() : xScenes.FindLoadedSceneByPath(m_strScenePath);
			if (!xScene.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xScenes.UnloadScene(xScene);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "UnloadScene"; }
	};

	// Makes a loaded scene (by path) the active scene.
	class Zenith_GraphNode_SetActiveScene : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetActiveScene)
	public:
		ZENITH_PROPERTY(std::string, m_strScenePath, "")

		GraphNodeStatus Execute(Zenith_GraphContext&) override
		{
			Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
			const Zenith_Scene xScene = xScenes.FindLoadedSceneByPath(m_strScenePath);
			if (!xScene.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			return xScenes.SetActiveScene(xScene) ? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
		}
		const char* GetTypeName() const override { return "SetActiveScene"; }
	};

	// Fires when a scene finishes a file-backed load. m_strScenePath filters by
	// canonical-path suffix ("" = any scene); the loaded path lands in
	// m_strStorePathVar for downstream chains.
	class Zenith_GraphNode_OnSceneLoaded : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_OnSceneLoaded)
	public:
		ZENITH_PROPERTY(std::string, m_strScenePath, "")
		ZENITH_PROPERTY(std::string, m_strStorePathVar, "loadedScene")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (!xContext.m_pxEventPayload || xContext.m_pxEventPayload->GetType() != PROPERTY_TYPE_STRING)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const std::string& strLoadedPath = xContext.m_pxEventPayload->GetString();
			if (!m_strScenePath.empty())
			{
				if (strLoadedPath.size() < m_strScenePath.size()
					|| strLoadedPath.compare(strLoadedPath.size() - m_strScenePath.size(), m_strScenePath.size(), m_strScenePath) != 0)
				{
					return GRAPH_NODE_STATUS_FAILURE;	// not the scene this anchor watches
				}
			}
			if (!m_strStorePathVar.empty())
			{
				xContext.m_pxBlackboard->SetValue(m_strStorePathVar, *xContext.m_pxEventPayload);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "OnSceneLoaded"; }
		bool MatchesCustomEvent(const char* szName) const override { return std::strcmp(szName, "__SceneLoaded") == 0; }
	};
}

void Zenith_RegisterEngineGraphNodes_Scene()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	xRegistry.RegisterNodeType<Zenith_GraphNode_LoadSceneByAsset>("LoadSceneByAsset", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_UnloadScene>("UnloadScene", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetActiveScene>("SetActiveScene", GRAPH_EVENT_NONE, 1, false, "Scene");
	xRegistry.RegisterNodeType<Zenith_GraphNode_OnSceneLoaded>("OnSceneLoaded", GRAPH_EVENT_CUSTOM, 1, false, "Scene");
}
