#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_GraphReload.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"
#include "AssetHandling/Zenith_BehaviourGraphAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Core/Zenith_FileWatcher.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_Query.h"

#include <cstdio>
#include <filesystem>

namespace
{
	struct GraphReloadState
	{
		Zenith_Vector<std::string> m_axPendingPaths;	// normalized prefixed paths
		Zenith_FileWatcher* m_pxWatcher = nullptr;
		bool m_bWatcherStartAttempted = false;
		u_int m_uReloadCount = 0;
		char m_acLastStatus[256] = {};
	};

	GraphReloadState g_xGraphReload;

	void SetStatus(const char* szFormat, ...)
	{
		va_list xArgs;
		va_start(xArgs, szFormat);
		vsnprintf(g_xGraphReload.m_acLastStatus, sizeof(g_xGraphReload.m_acLastStatus), szFormat, xArgs);
		va_end(xArgs);
		Zenith_Log(LOG_CATEGORY_EDITOR, "GraphReload: %s", g_xGraphReload.m_acLastStatus);
	}

	void OnGraphsDirChanged(void* /*pContext*/, const std::string& strRelativePath, FileChangeType eType)
	{
		if (eType != FileChangeType::Modified && eType != FileChangeType::Created)
		{
			return;
		}
		// Only .bgraph files matter.
		const size_t uDot = strRelativePath.find_last_of('.');
		if (uDot == std::string::npos || strRelativePath.substr(uDot) != ZENITH_BGRAPH_EXT)
		{
			return;
		}
		const std::string strAssetPath = std::string("game:Graphs/") + strRelativePath;
		Zenith_GraphReload::NotifyAssetChanged(strAssetPath.c_str());
	}

	void EnsureWatcherStarted()
	{
		if (g_xGraphReload.m_bWatcherStartAttempted)
		{
			return;
		}
		g_xGraphReload.m_bWatcherStartAttempted = true;

		const std::string strGraphsDir = Zenith_AssetRegistry::ResolvePath("game:Graphs");
		std::error_code xEC;
		if (!std::filesystem::exists(strGraphsDir, xEC))
		{
			return;	// no graphs dir (yet) - external-edit watching stays inert
		}
		g_xGraphReload.m_pxWatcher = new Zenith_FileWatcher();
		if (!g_xGraphReload.m_pxWatcher->Start(strGraphsDir, false, &OnGraphsDirChanged, nullptr))
		{
			delete g_xGraphReload.m_pxWatcher;
			g_xGraphReload.m_pxWatcher = nullptr;
			Zenith_Log(LOG_CATEGORY_EDITOR, "GraphReload: failed to watch '%s'; external-edit reload inert", strGraphsDir.c_str());
		}
	}

	// Refresh the registry-cached asset's definition from disk so external
	// edits are picked up (in-editor saves already updated the cached
	// definition; the refresh is then a harmless reload of identical bytes).
	bool RefreshCachedDefinitionFromDisk(Zenith_BehaviourGraphAsset* pxCached, const std::string& strNormalizedPath)
	{
		const std::string strAbsolutePath = Zenith_AssetRegistry::ResolvePath(strNormalizedPath);
		std::error_code xEC;
		if (!std::filesystem::exists(strAbsolutePath, xEC))
		{
			return false;
		}
		Zenith_Result<Zenith_Asset*> xResult = LoadSerializableAsset(strAbsolutePath);
		if (!xResult.IsOk())
		{
			return false;
		}
		Zenith_BehaviourGraphAsset* pxFresh = static_cast<Zenith_BehaviourGraphAsset*>(xResult.Value());
		if (!pxFresh->LoadedOk())
		{
			delete pxFresh;
			return false;
		}
		// Serialize-copy the freshly-loaded definition into the cached asset.
		Zenith_DataStream xCopy;
		pxFresh->GetDefinition().WriteToDataStream(xCopy);
		xCopy.SetCursor(0);
		pxCached->GetDefinition().ReadFromDataStream(xCopy);
		delete pxFresh;
		return true;
	}

	void ReloadAsset(const std::string& strNormalizedPath)
	{
		Zenith_BehaviourGraphAsset* pxAsset = Zenith_AssetRegistry::Get<Zenith_BehaviourGraphAsset>(strNormalizedPath);
		if (!pxAsset)
		{
			SetStatus("reload FAILED: '%s' could not be loaded", strNormalizedPath.c_str());
			return;
		}
		if (!RefreshCachedDefinitionFromDisk(pxAsset, strNormalizedPath))
		{
			// Keep the old definition + old live instances (atomic failure).
			SetStatus("reload FAILED: '%s' invalid on disk; live graphs untouched", strNormalizedPath.c_str());
			return;
		}

		// Re-instantiate every live slot referencing this asset, across all
		// loaded scenes. Blackboard state migrates name+type-matched.
		u_int uReloadedSlots = 0;
		g_xEngine.Scenes().QueryAllScenes<Zenith_GraphComponent>()
			.ForEach([&uReloadedSlots, &strNormalizedPath](Zenith_EntityID, Zenith_GraphComponent& xComponent)
		{
			uReloadedSlots += xComponent.ReloadSlotsForAsset(strNormalizedPath.c_str());
		});

		++g_xGraphReload.m_uReloadCount;
		SetStatus("reloaded '%s' (%u live instance%s)", strNormalizedPath.c_str(), uReloadedSlots, uReloadedSlots == 1 ? "" : "s");
	}
}

void Zenith_GraphReload::NotifyAssetChanged(const char* szAssetPath)
{
	if (!szAssetPath || szAssetPath[0] == '\0')
	{
		return;
	}
	const std::string strNormalized = Zenith_AssetRegistry::NormalizeAssetPath(szAssetPath);
	for (u_int u = 0; u < g_xGraphReload.m_axPendingPaths.GetSize(); ++u)
	{
		if (g_xGraphReload.m_axPendingPaths.Get(u) == strNormalized)
		{
			return;	// already queued
		}
	}
	g_xGraphReload.m_axPendingPaths.PushBack(strNormalized);
}

void Zenith_GraphReload::Update()
{
	EnsureWatcherStarted();
	if (g_xGraphReload.m_pxWatcher)
	{
		g_xGraphReload.m_pxWatcher->Update();	// may queue via OnGraphsDirChanged
	}

	if (g_xGraphReload.m_axPendingPaths.GetSize() == 0)
	{
		return;
	}
	Zenith_Assert(!Zenith_GraphComponent::IsDispatchInProgress(),
		"GraphReload::Update must run at a safe point, never during graph dispatch");

	Zenith_Vector<std::string> axPaths = std::move(g_xGraphReload.m_axPendingPaths);
	g_xGraphReload.m_axPendingPaths.Clear();
	for (u_int u = 0; u < axPaths.GetSize(); ++u)
	{
		ReloadAsset(axPaths.Get(u));
	}
}

u_int Zenith_GraphReload::GetReloadCount()
{
	return g_xGraphReload.m_uReloadCount;
}

const char* Zenith_GraphReload::GetLastStatusLine()
{
	return g_xGraphReload.m_acLastStatus;
}

#endif // ZENITH_TOOLS
