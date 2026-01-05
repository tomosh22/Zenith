#pragma once
/**
 * Exploration_AsyncLoader.h - Asset streaming manager
 *
 * Demonstrates:
 * - Background texture loading
 * - Load state tracking
 * - Progress reporting for UI
 * - Priority-based loading queue
 *
 * Engine APIs used:
 * - Zenith_AsyncAssetLoader
 */

#include "AssetHandling/Zenith_AsyncAssetLoader.h"
#include "Core/Zenith_GUID.h"
// Note: Zenith_Log macro is available from precompiled header via Exploration.cpp

#include <string>
#include <vector>
#include <queue>

namespace Exploration_AsyncLoader
{
	// ========================================================================
	// Load Request Structure
	// ========================================================================
	struct LoadRequest
	{
		Zenith_AssetGUID m_xGUID;
		std::string m_strAssetPath;
		int32_t m_iPriority = 0;  // Higher = more important
		bool m_bCompleted = false;
		bool m_bFailed = false;
		void* m_pxLoadedAsset = nullptr;
	};

	// ========================================================================
	// Loading Statistics
	// ========================================================================
	struct LoadingStats
	{
		uint32_t m_uPendingLoads = 0;
		uint32_t m_uCompletedLoads = 0;
		uint32_t m_uFailedLoads = 0;
		uint32_t m_uTotalRequests = 0;
		float m_fProgressPercent = 100.0f;
	};

	// ========================================================================
	// Internal State
	// ========================================================================
	static std::vector<LoadRequest> s_xPendingRequests;
	static std::vector<LoadRequest> s_xCompletedRequests;
	static LoadingStats s_xStats;
	static bool s_bIsLoading = false;

	/**
	 * Callback when async load completes
	 */
	inline void OnLoadComplete(void* pxAsset, void* pxUserData)
	{
		size_t uIndex = reinterpret_cast<size_t>(pxUserData);
		if (uIndex < s_xPendingRequests.size())
		{
			s_xPendingRequests[uIndex].m_bCompleted = true;
			s_xPendingRequests[uIndex].m_pxLoadedAsset = pxAsset;
			s_xStats.m_uCompletedLoads++;
			s_xStats.m_uPendingLoads--;

			Zenith_Log(LOG_CATEGORY_ASSET, "Async load completed for asset index %zu", uIndex);
		}
	}

	/**
	 * Callback when async load fails
	 */
	inline void OnLoadFailed(const char* szError, void* pxUserData)
	{
		size_t uIndex = reinterpret_cast<size_t>(pxUserData);
		if (uIndex < s_xPendingRequests.size())
		{
			s_xPendingRequests[uIndex].m_bCompleted = true;
			s_xPendingRequests[uIndex].m_bFailed = true;
			s_xStats.m_uFailedLoads++;
			s_xStats.m_uPendingLoads--;

			Zenith_Log(LOG_CATEGORY_ASSET, "Async load failed for asset index %zu: %s", uIndex, szError);
		}
	}

	/**
	 * Queue an asset for async loading
	 * @param xGUID Asset GUID to load
	 * @param strPath Asset path (for display/debugging)
	 * @param iPriority Load priority (higher = sooner)
	 */
	inline void QueueAsset(const Zenith_AssetGUID& xGUID, const std::string& strPath, int32_t iPriority = 0)
	{
		LoadRequest xRequest;
		xRequest.m_xGUID = xGUID;
		xRequest.m_strAssetPath = strPath;
		xRequest.m_iPriority = iPriority;
		xRequest.m_bCompleted = false;
		xRequest.m_bFailed = false;
		xRequest.m_pxLoadedAsset = nullptr;

		s_xPendingRequests.push_back(xRequest);
		s_xStats.m_uTotalRequests++;
		s_xStats.m_uPendingLoads++;
		s_bIsLoading = true;

		Zenith_Log(LOG_CATEGORY_ASSET, "Queued asset for loading: %s", strPath.c_str());
	}

	/**
	 * Start loading queued assets
	 * Call this after queueing to begin async loads
	 */
	template<typename AssetType>
	inline void StartLoadingQueued()
	{
		for (size_t i = 0; i < s_xPendingRequests.size(); ++i)
		{
			LoadRequest& xRequest = s_xPendingRequests[i];
			if (!xRequest.m_bCompleted && xRequest.m_xGUID.IsValid())
			{
				Zenith_AsyncAssetLoader::LoadAsync<AssetType>(
					xRequest.m_xGUID,
					&OnLoadComplete,
					reinterpret_cast<void*>(i),
					&OnLoadFailed);
			}
		}
	}

	/**
	 * Update loading state - call each frame
	 */
	inline void Update()
	{
		// Process completed loads
		Zenith_AsyncAssetLoader::ProcessCompletedLoads();

		// Update progress
		if (s_xStats.m_uTotalRequests > 0)
		{
			uint32_t uCompleted = s_xStats.m_uCompletedLoads + s_xStats.m_uFailedLoads;
			s_xStats.m_fProgressPercent = (static_cast<float>(uCompleted) / static_cast<float>(s_xStats.m_uTotalRequests)) * 100.0f;
		}
		else
		{
			s_xStats.m_fProgressPercent = 100.0f;
		}

		// Check if all loading is done
		if (s_xStats.m_uPendingLoads == 0 && s_bIsLoading)
		{
			s_bIsLoading = false;
			Zenith_Log(LOG_CATEGORY_ASSET, "All async loads completed. Success: %u, Failed: %u",
				s_xStats.m_uCompletedLoads, s_xStats.m_uFailedLoads);
		}
	}

	/**
	 * Check if any loads are still pending
	 */
	inline bool HasPendingLoads()
	{
		return s_xStats.m_uPendingLoads > 0;
	}

	/**
	 * Get loading progress (0-100)
	 */
	inline float GetProgress()
	{
		return s_xStats.m_fProgressPercent;
	}

	/**
	 * Get loading statistics
	 */
	inline const LoadingStats& GetStats()
	{
		return s_xStats;
	}

	/**
	 * Get number of pending loads
	 */
	inline uint32_t GetPendingCount()
	{
		return s_xStats.m_uPendingLoads;
	}

	/**
	 * Cancel all pending loads
	 */
	inline void CancelAll()
	{
		Zenith_AsyncAssetLoader::CancelAllPendingLoads();
		s_xStats.m_uPendingLoads = 0;
		s_bIsLoading = false;

		Zenith_Log(LOG_CATEGORY_ASSET, "Cancelled all pending async loads");
	}

	/**
	 * Reset loader state (e.g., when switching scenes)
	 */
	inline void Reset()
	{
		CancelAll();
		s_xPendingRequests.clear();
		s_xCompletedRequests.clear();
		s_xStats = LoadingStats();
		s_bIsLoading = false;
	}

	/**
	 * Check if currently loading
	 */
	inline bool IsLoading()
	{
		return s_bIsLoading;
	}

	/**
	 * Get loaded asset from a request (returns nullptr if not loaded or failed)
	 */
	inline void* GetLoadedAsset(size_t uRequestIndex)
	{
		if (uRequestIndex < s_xPendingRequests.size())
		{
			if (s_xPendingRequests[uRequestIndex].m_bCompleted &&
			    !s_xPendingRequests[uRequestIndex].m_bFailed)
			{
				return s_xPendingRequests[uRequestIndex].m_pxLoadedAsset;
			}
		}
		return nullptr;
	}

	/**
	 * Get status string for display
	 */
	inline const char* GetStatusString()
	{
		if (s_bIsLoading)
		{
			static char s_szBuffer[64];
			snprintf(s_szBuffer, sizeof(s_szBuffer), "Loading... %.0f%% (%u pending)",
				s_xStats.m_fProgressPercent, s_xStats.m_uPendingLoads);
			return s_szBuffer;
		}
		else if (s_xStats.m_uFailedLoads > 0)
		{
			static char s_szBuffer[64];
			snprintf(s_szBuffer, sizeof(s_szBuffer), "Loaded (%u failed)", s_xStats.m_uFailedLoads);
			return s_szBuffer;
		}
		else
		{
			return "Ready";
		}
	}

} // namespace Exploration_AsyncLoader
