#include "Zenith.h"
#include "Zenith_AsyncAssetLoader.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "TaskSystem/Zenith_TaskSystem.h"

// Static member definitions
Zenith_Vector<Zenith_AsyncAssetLoader::LoadRequest> Zenith_AsyncAssetLoader::s_xPendingLoads;
Zenith_Vector<Zenith_AsyncAssetLoader::CompletedLoad> Zenith_AsyncAssetLoader::s_xCompletedLoads;
Zenith_Mutex Zenith_AsyncAssetLoader::s_xPendingMutex;
Zenith_Mutex Zenith_AsyncAssetLoader::s_xCompletedMutex;
std::unordered_map<std::string, AssetLoadState> Zenith_AsyncAssetLoader::s_xLoadStates;
Zenith_Mutex Zenith_AsyncAssetLoader::s_xStateMutex;

//------------------------------------------------------------------------------
// Internal task for processing load requests
//------------------------------------------------------------------------------

// Task wrapper that holds a single load request
struct AsyncLoadTaskData
{
	Zenith_AsyncAssetLoader::LoadRequest m_xRequest;
	Zenith_Task* m_pxTask;  // Pointer to task for cleanup
};

static void AsyncLoadTaskFunction(void* pData)
{
	AsyncLoadTaskData* pxTaskData = static_cast<AsyncLoadTaskData*>(pData);
	const auto& xRequest = pxTaskData->m_xRequest;

	Zenith_AsyncAssetLoader::CompletedLoad xCompleted;
	xCompleted.m_strPath = xRequest.m_strPath;
	xCompleted.m_pfnOnComplete = xRequest.m_pfnOnComplete;
	xCompleted.m_pfnOnFail = xRequest.m_pfnOnFail;
	xCompleted.m_pxUserData = xRequest.m_pxUserData;

	// Resolve prefixed path to absolute path for loading
	std::string strAbsolutePath = Zenith_AssetRegistry::ResolvePath(xRequest.m_strPath);

	// Call the type-specific loader with the resolved absolute path
	void* pxAsset = xRequest.m_pfnLoader(strAbsolutePath);
	if (pxAsset != nullptr)
	{
		xCompleted.m_bSuccess = true;
		xCompleted.m_pxAsset = pxAsset;
	}
	else
	{
		xCompleted.m_bSuccess = false;
		xCompleted.m_strError = "Failed to load asset from path: " + xRequest.m_strPath;
		xCompleted.m_pxAsset = nullptr;
	}

	// Queue completed load for main thread processing
	{
		Zenith_ScopedMutexLock xLock(Zenith_AsyncAssetLoader::s_xCompletedMutex);
		Zenith_AsyncAssetLoader::s_xCompletedLoads.PushBack(xCompleted);
	}

	// Update load state
	{
		Zenith_ScopedMutexLock xLock(Zenith_AsyncAssetLoader::s_xStateMutex);
		Zenith_AsyncAssetLoader::s_xLoadStates[xRequest.m_strPath] =
			xCompleted.m_bSuccess ? AssetLoadState::LOADED : AssetLoadState::FAILED;
	}

	// Clean up task and task data
	Zenith_Task* pxTask = pxTaskData->m_pxTask;
	delete pxTaskData;
	delete pxTask;
}

//------------------------------------------------------------------------------
// Public API Implementation
//------------------------------------------------------------------------------

void Zenith_AsyncAssetLoader::ProcessCompletedLoads()
{
	// Move completed loads to local vector to minimize lock time
	Zenith_Vector<CompletedLoad> xLocalCompleted;

	{
		Zenith_ScopedMutexLock xLock(s_xCompletedMutex);
		for (u_int i = 0; i < s_xCompletedLoads.GetSize(); ++i)
		{
			xLocalCompleted.PushBack(s_xCompletedLoads.Get(i));
		}
		s_xCompletedLoads.Clear();
	}

	// Dispatch callbacks on main thread
	for (u_int i = 0; i < xLocalCompleted.GetSize(); ++i)
	{
		const CompletedLoad& xCompleted = xLocalCompleted.Get(i);

		if (xCompleted.m_bSuccess)
		{
			if (xCompleted.m_pfnOnComplete)
			{
				xCompleted.m_pfnOnComplete(xCompleted.m_pxAsset, xCompleted.m_pxUserData);
			}
		}
		else
		{
			if (xCompleted.m_pfnOnFail)
			{
				xCompleted.m_pfnOnFail(xCompleted.m_strError.c_str(), xCompleted.m_pxUserData);
			}
			Zenith_Log(LOG_CATEGORY_ASSET, "Async load failed: %s", xCompleted.m_strError.c_str());
		}
	}
}

AssetLoadState Zenith_AsyncAssetLoader::GetLoadState(const std::string& strPath)
{
	Zenith_ScopedMutexLock xLock(s_xStateMutex);
	auto xIt = s_xLoadStates.find(strPath);
	return (xIt != s_xLoadStates.end()) ? xIt->second : AssetLoadState::UNLOADED;
}

bool Zenith_AsyncAssetLoader::HasPendingLoads()
{
	Zenith_ScopedMutexLock xLock(s_xPendingMutex);
	return s_xPendingLoads.GetSize() > 0;
}

void Zenith_AsyncAssetLoader::CancelAllPendingLoads()
{
	Zenith_ScopedMutexLock xLock(s_xPendingMutex);
	s_xPendingLoads.Clear();

	// Note: Tasks already submitted cannot be cancelled
	// They will complete but their callbacks will be ignored
}

void Zenith_AsyncAssetLoader::ClearLoadStates()
{
	Zenith_ScopedMutexLock xLock(s_xStateMutex);
	s_xLoadStates.clear();
}

void Zenith_AsyncAssetLoader::SubmitLoadRequest(const LoadRequest& xRequest)
{
	// Create task data (will be deleted after task completes)
	AsyncLoadTaskData* pxTaskData = new AsyncLoadTaskData();
	pxTaskData->m_xRequest = xRequest;

	// Create and submit task
	// Note: We create tasks on heap because they outlive this function
	Zenith_Task* pxTask = new Zenith_Task(
		ZENITH_PROFILE_INDEX__ASSET_LOAD,
		AsyncLoadTaskFunction,
		pxTaskData
	);

	// Store task pointer in data so it can be deleted when task completes
	pxTaskData->m_pxTask = pxTask;

	g_xEngine.Tasks().SubmitTask(pxTask);
}

//------------------------------------------------------------------------------
// Asset type loader specializations
//
// Async loading is currently not implemented for any asset type. The original
// design called for worker-thread loading with a main-thread completion
// callback (see header docs and Zenith_AsyncAssetLoader::LoadAsync), but the
// per-type loaders below all stub-return nullptr after logging once.
//
// Reasons it isn't implemented yet (kept here so newcomers know it isn't an
// oversight to "just fill in"):
//   - Textures: GPU upload paths aren't thread-safe; would need staging buffers
//     and a deferred GPU-side completion step.
//   - Materials: reference textures, so async-loading them recursively triggers
//     the texture problem.
//   - Meshes / models: CPU-side parse is async-safe, but the GPU buffer
//     creation isn't.
//   - Prefabs: deserialise into the scene graph, which is main-thread-only.
//
// Callers should use the synchronous Zenith_AssetRegistry::Get<T>(path) path
// until this is implemented. The async path is wired up end-to-end so a future
// implementation only needs to fill in these per-type loaders.
//------------------------------------------------------------------------------

// Forward declarations of asset types
class Zenith_TextureAsset;
class Zenith_MaterialAsset;
class Zenith_MeshAsset;
class Zenith_ModelAsset;
class Zenith_Prefab;
class Zenith_FontAsset;

namespace
{
	// Single point that logs "not implemented" for every per-type stub. Logged
	// once per process per type so a frame-rate-loop call site doesn't spam.
	void* LogAsyncNotImplemented(const char* szTypeName)
	{
		static std::unordered_map<std::string, bool> s_xLogged;
		static Zenith_Mutex s_xMutex;
		Zenith_ScopedMutexLock xLock(s_xMutex);
		if (s_xLogged.find(szTypeName) == s_xLogged.end())
		{
			s_xLogged[szTypeName] = true;
			Zenith_Warning(LOG_CATEGORY_ASSET,
				"AsyncLoadAsset<%s>: not implemented. Use Zenith_AssetRegistry::Get<%s>(path) for synchronous load. (Logged once per type.)",
				szTypeName, szTypeName);
		}
		return nullptr;
	}
}

template<> void* AsyncLoadAsset<Zenith_TextureAsset>(const std::string&)  { return LogAsyncNotImplemented("Zenith_TextureAsset"); }
template<> void* AsyncLoadAsset<Zenith_MaterialAsset>(const std::string&) { return LogAsyncNotImplemented("Zenith_MaterialAsset"); }
template<> void* AsyncLoadAsset<Zenith_MeshAsset>(const std::string&)     { return LogAsyncNotImplemented("Zenith_MeshAsset"); }
template<> void* AsyncLoadAsset<Zenith_ModelAsset>(const std::string&)    { return LogAsyncNotImplemented("Zenith_ModelAsset"); }
template<> void* AsyncLoadAsset<Zenith_Prefab>(const std::string&)
{
	return LogAsyncNotImplemented("Zenith_Prefab");
}
template<> void* AsyncLoadAsset<Zenith_FontAsset>(const std::string&) { return LogAsyncNotImplemented("Zenith_FontAsset"); }
