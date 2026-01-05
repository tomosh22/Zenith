#pragma once

#include "Core/Zenith_GUID.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Collections/Zenith_Vector.h"
#include <queue>

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
 * This class provides async loading capabilities using the task system.
 * Assets are loaded on worker threads and callbacks are invoked on the main thread.
 *
 * Usage:
 *   // Request async load
 *   Zenith_AsyncAssetLoader::LoadAsync<Flux_Texture>(guid, OnTextureLoaded, userData);
 *
 *   // In main loop (must be called every frame)
 *   Zenith_AsyncAssetLoader::ProcessCompletedLoads();
 *
 *   // Callback is called on main thread when complete
 *   void OnTextureLoaded(void* pxAsset, void* pxUserData)
 *   {
 *       Flux_Texture* pxTexture = static_cast<Flux_Texture*>(pxAsset);
 *       // Use texture...
 *   }
 */
class Zenith_AsyncAssetLoader
{
public:
	// Internal types (public for task function access)
	struct LoadRequest
	{
		Zenith_AssetGUID m_xGUID;
		AssetLoaderFn m_pfnLoader;
		AssetLoadCompleteFn m_pfnOnComplete;
		AssetLoadFailFn m_pfnOnFail;
		void* m_pxUserData;
	};

	struct CompletedLoad
	{
		Zenith_AssetGUID m_xGUID;
		void* m_pxAsset;
		AssetLoadCompleteFn m_pfnOnComplete;
		AssetLoadFailFn m_pfnOnFail;
		void* m_pxUserData;
		bool m_bSuccess;
		std::string m_strError;
	};

	/**
	 * Request async loading of an asset by GUID
	 * @param xGUID GUID of the asset to load
	 * @param pfnOnComplete Callback when load completes (called on main thread)
	 * @param pxUserData User data passed to callbacks
	 * @param pfnOnFail Callback on load failure (optional, called on main thread)
	 */
	template<typename T>
	static void LoadAsync(
		const Zenith_AssetGUID& xGUID,
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
	 * @param xGUID GUID of the asset
	 * @return Current load state
	 */
	static AssetLoadState GetLoadState(const Zenith_AssetGUID& xGUID);

	/**
	 * Check if any loads are pending
	 */
	static bool HasPendingLoads();

	/**
	 * Cancel all pending loads (e.g., when switching scenes)
	 */
	static void CancelAllPendingLoads();

	// Thread-safe queues (public for task function access)
	static Zenith_Vector<LoadRequest> s_xPendingLoads;
	static Zenith_Vector<CompletedLoad> s_xCompletedLoads;
	static Zenith_Mutex s_xPendingMutex;
	static Zenith_Mutex s_xCompletedMutex;

	// Track load states (public for task function access)
	static std::unordered_map<Zenith_AssetGUID, AssetLoadState> s_xLoadStates;
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
	const Zenith_AssetGUID& xGUID,
	AssetLoadCompleteFn pfnOnComplete,
	void* pxUserData,
	AssetLoadFailFn pfnOnFail)
{
	if (!xGUID.IsValid())
	{
		if (pfnOnFail)
		{
			pfnOnFail("Invalid GUID", pxUserData);
		}
		return;
	}

	// Check if already loaded or loading
	{
		s_xStateMutex.Lock();
		auto xIt = s_xLoadStates.find(xGUID);
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
					// Note: Caller should use AssetRef::Get() to get the actual pointer
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
			s_xLoadStates[xGUID] = AssetLoadState::LOADING;
		}
		s_xStateMutex.Unlock();
	}

	// Create load request
	LoadRequest xRequest;
	xRequest.m_xGUID = xGUID;
	xRequest.m_pfnLoader = &AsyncLoadAsset<T>;
	xRequest.m_pfnOnComplete = pfnOnComplete;
	xRequest.m_pfnOnFail = pfnOnFail;
	xRequest.m_pxUserData = pxUserData;

	SubmitLoadRequest(xRequest);
}
