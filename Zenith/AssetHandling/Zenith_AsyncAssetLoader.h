#pragma once

#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Collections/Zenith_Vector.h"
#include <string>
#include <unordered_map>

// =============================================================================
// STATUS: NOT IMPLEMENTED
// -----------------------------------------------------------------------------
// All AsyncLoadAsset<T> specialisations in Zenith_AsyncAssetLoader.cpp stub-
// return nullptr after a one-shot warning log. The request queue, completion
// callbacks, and task dispatch scaffolding below exist, but the per-type GPU
// hand-off (texture staging, mesh buffer creation, etc.) is not done.
//
// Use Zenith_AssetRegistry::Get<T>(path) for ALL asset access. Do not write
// code that depends on async completion. See AssetHandling/CLAUDE.md for the
// implementation status.
// =============================================================================

/**
 * Asset load state enum - tracks the loading progress of an asset
 */
enum class AssetLoadState : uint8_t
{
	UNLOADED,	// Asset has not been requested
	LOADING,	// Asset is currently being loaded (async)
	LOADED,		// Asset is loaded and ready to use
	FAILED		// Asset failed to load
};

/**
 * Callback function types for async loading
 * Using raw function pointers instead of std::function for performance
 */
using AssetLoadCompleteFn = void(*)(void* pxAsset, void* pxUserData);
using AssetLoadFailFn = void(*)(const char* szError, void* pxUserData);

/**
 * Type-erased asset loader function
 * Each asset type specializes this to perform the actual loading
 */
using AssetLoaderFn = void*(*)(const std::string& strPath);

/**
 * Zenith_AsyncAssetLoader - Manages asynchronous asset loading
 *
 * !!! NOT YET IMPLEMENTED !!!
 *
 * The async-loading framework (request queue, completion callbacks, task
 * dispatch) is wired up end-to-end, but the per-type AsyncLoadAsset<T>
 * specialisations all stub-return nullptr after a one-shot warning log.
 * Calling LoadAsync<T>() will currently fail to deliver the asset.
 *
 * For now, use Zenith_AssetRegistry::Get<T>(path) for synchronous loading.
 * See Zenith_AsyncAssetLoader.cpp for the per-type rationale on why each
 * type is harder to async-load than it might first appear.
 *
 * --- Original design (kept for reference once implementation lands) ---
 *
 * This class provides async loading capabilities using the task system.
 * Assets are loaded on worker threads and callbacks are invoked on the main thread.
 *
 * Usage:
 *   // Request async load using prefixed path
 *   Zenith_AsyncAssetLoader::LoadAsync<Zenith_TextureAsset>("game:Textures/diffuse.ztxtr", OnTextureLoaded, userData);
 *
 *   // In main loop (must be called every frame)
 *   Zenith_AsyncAssetLoader::ProcessCompletedLoads();
 *
 *   // Callback is called on main thread when complete
 *   void OnTextureLoaded(void* pxAsset, void* pxUserData)
 *   {
 *       Zenith_TextureAsset* pxTexture = static_cast<Zenith_TextureAsset*>(pxAsset);
 *       // Use texture...
 *   }
 */
class Zenith_AsyncAssetLoader
{
public:
	// Internal types (public for task function access)
	struct LoadRequest
	{
		std::string m_strPath;  // Prefixed path (e.g., "game:Textures/tex.ztxtr")
		AssetLoaderFn m_pfnLoader;
		AssetLoadCompleteFn m_pfnOnComplete;
		AssetLoadFailFn m_pfnOnFail;
		void* m_pxUserData;
	};

	struct CompletedLoad
	{
		std::string m_strPath;
		void* m_pxAsset;
		AssetLoadCompleteFn m_pfnOnComplete;
		AssetLoadFailFn m_pfnOnFail;
		void* m_pxUserData;
		bool m_bSuccess;
		std::string m_strError;
	};

	/**
	 * Request async loading of an asset by path
	 * @param strPath Prefixed path to the asset (e.g., "game:Textures/tex.ztxtr")
	 * @param pfnOnComplete Callback when load completes (called on main thread)
	 * @param pxUserData User data passed to callbacks
	 * @param pfnOnFail Callback on load failure (optional, called on main thread)
	 */
	template<typename T>
	static void LoadAsync(
		const std::string& strPath,
		AssetLoadCompleteFn pfnOnComplete,
		void* pxUserData = nullptr,
		AssetLoadFailFn pfnOnFail = nullptr);

	/**
	 * Process completed loads - must be called every frame from main thread
	 * This dispatches callbacks for completed loads
	 */
	static void ProcessCompletedLoads();

	/**
	 * Get the load state of an asset
	 * @param strPath Prefixed path of the asset
	 * @return Current load state
	 */
	static AssetLoadState GetLoadState(const std::string& strPath);

	/**
	 * Check if any loads are pending
	 */
	static bool HasPendingLoads();

	/**
	 * Cancel all pending loads (e.g., when switching scenes)
	 */
	static void CancelAllPendingLoads();

	/**
	 * Clear all load states (call when switching scenes or at shutdown)
	 */
	static void ClearLoadStates();

	// Thread-safe queues (public for task function access)
	static Zenith_Vector<LoadRequest> s_xPendingLoads;
	static Zenith_Vector<CompletedLoad> s_xCompletedLoads;
	static Zenith_Mutex s_xPendingMutex;
	static Zenith_Mutex s_xCompletedMutex;

	// Track load states by path (public for task function access)
	static std::unordered_map<std::string, AssetLoadState> s_xLoadStates;
	static Zenith_Mutex s_xStateMutex;

private:
	// Submit a load request to the task system
	static void SubmitLoadRequest(const LoadRequest& xRequest);
};

//--------------------------------------------------------------------------
// Template implementation
//--------------------------------------------------------------------------

// Forward declare loader functions for each asset type
template<typename T>
void* AsyncLoadAsset(const std::string& strPath);

template<typename T>
void Zenith_AsyncAssetLoader::LoadAsync(
	const std::string& strPath,
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	if (strPath.empty())
	{
		if (pfnOnFail)
		{
			pfnOnFail("Empty path", pxUserData);
		}
		return;
	}

	// Check if already loaded or loading
	{
		s_xStateMutex.Lock();
		auto xIt = s_xLoadStates.find(strPath);
		if (xIt != s_xLoadStates.end())
		{
			AssetLoadState eState = xIt->second;
			s_xStateMutex.Unlock();

			if (eState == AssetLoadState::LOADED)
			{
				// Already loaded - call callback immediately
				if (pfnOnComplete)
				{
					// Get cached asset and call callback
					// Note: Caller should use AssetHandle::Get() to get the actual pointer
					pfnOnComplete(nullptr, pxUserData);
				}
				return;
			}
			else if (eState == AssetLoadState::LOADING)
			{
				// Already loading - could queue additional callbacks but for simplicity we just return
				return;
			}
		}
		else
		{
			s_xLoadStates[strPath] = AssetLoadState::LOADING;
		}
		s_xStateMutex.Unlock();
	}

	// Create load request
	LoadRequest xRequest;
	xRequest.m_strPath = strPath;
	xRequest.m_pfnLoader = &AsyncLoadAsset<T>;
	xRequest.m_pfnOnComplete = pfnOnComplete;
	xRequest.m_pfnOnFail = pfnOnFail;
	xRequest.m_pxUserData = pxUserData;

	SubmitLoadRequest(xRequest);
}
