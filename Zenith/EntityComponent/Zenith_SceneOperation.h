#pragma once

#include <atomic>
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"

// Forward declaration
struct Zenith_Scene;

/**
 * Zenith_SceneOperation - Tracks asynchronous scene loading operations
 *
 * Similar to Unity's AsyncOperation but with Zenith naming conventions.
 * Provides progress tracking, activation control, and completion callbacks.
 *
 * UNITY API MAPPING:
 * - Unity: AsyncOperation.allowSceneActivation
 *   Zenith: SetActivationAllowed() / IsActivationAllowed()
 *
 * - Unity: AsyncOperation.progress
 *   Zenith: GetProgress()
 *
 * - Unity: AsyncOperation.isDone
 *   Zenith: IsComplete()
 *
 * PROGRESS MILESTONES (differs from Unity's 0→0.9→1.0 pattern):
 *   0.1  - FILE_READ_STARTED
 *   0.7  - FILE_READ_COMPLETE
 *   0.75 - SCENE_CREATED
 *   0.8  - DESERIALIZE_START
 *   0.85 - DESERIALIZE_COMPLETE
 *   0.9  - ACTIVATION_PAUSED (when activation not allowed)
 *   1.0  - COMPLETE
 *
 * Usage:
 *   Zenith_SceneOperation* pxOp = Zenith_SceneManager::LoadSceneAsync("Level.zscen");
 *   pxOp->SetActivationAllowed(false);  // Pause at ~90%
 *
 *   // In update loop:
 *   if (pxOp->GetProgress() >= 0.9f && bPlayerReady)
 *   {
 *       pxOp->SetActivationAllowed(true);  // Resume loading
 *   }
 */
class Zenith_SceneOperation
{
public:
	// Completion callback type
	using CompletionCallback = void(*)(Zenith_Scene);

	//==========================================================================
	// Progress Tracking
	//==========================================================================

	/**
	 * Get loading progress (0.0 to 1.0)
	 * Progress pauses at ~0.9 if activation is not allowed.
	 * Uses acquire ordering to ensure consistency with IsComplete().
	 */
	float GetProgress() const { return m_fProgress.load(std::memory_order_acquire); }

	/**
	 * Check if operation has completed
	 */
	bool IsComplete() const { return m_bIsComplete.load(std::memory_order_acquire); }

	/**
	 * Check if operation failed (file not found, circular load, etc.)
	 * Only valid after IsComplete() returns true.
	 * @note Must be called from the main thread only.
	 */
	bool HasFailed() const
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "HasFailed must be called from main thread");
		return m_bHasFailed;
	}

	//==========================================================================
	// Activation Control
	//==========================================================================

	/**
	 * Check if scene activation is allowed
	 * When false, loading pauses at ~90% to allow "Press to Continue" UI.
	 * @note Must be called from the main thread only.
	 */
	bool IsActivationAllowed() const
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "IsActivationAllowed must be called from main thread");
		return m_bActivationAllowed;
	}

	/**
	 * Set whether scene activation is allowed
	 * Set to false before starting load to pause at ~90%.
	 * Set to true to resume and complete loading.
	 * @note Must be called from the main thread only.
	 */
	void SetActivationAllowed(bool bAllow)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetActivationAllowed must be called from main thread");
		m_bActivationAllowed = bAllow;
	}

	//==========================================================================
	// Completion Callback
	//==========================================================================

	/**
	 * Set callback to be invoked when operation completes
	 * Callback receives the loaded scene handle.
	 * @note Must be called from the main thread only.
	 */
	void SetOnComplete(CompletionCallback pfnCallback)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetOnComplete must be called from main thread");
		m_pfnOnComplete = pfnCallback;
	}

	//==========================================================================
	// Priority Control (Unity-style)
	//==========================================================================

	/**
	 * Get loading priority (0 = low, higher = more priority)
	 * Affects order of processing when multiple async loads are pending.
	 * @note Must be called from the main thread only.
	 */
	int GetPriority() const
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "GetPriority must be called from main thread");
		return m_iPriority;
	}

	/**
	 * Set loading priority
	 * @param iPriority Priority value (0 = low, higher = more priority)
	 * @note Must be called from the main thread only.
	 */
	void SetPriority(int iPriority);

	//==========================================================================
	// Cancellation
	//==========================================================================

	/**
	 * Request cancellation of this async operation.
	 * The operation will be cleaned up on the next SceneManager::Update() call.
	 *
	 * @note Cancellation is not immediate - if the file load has already completed,
	 *       the operation may still finish. Check HasFailed() after IsComplete()
	 *       to determine if the operation was cancelled.
	 * @note Must be called from the main thread only.
	 */
	void RequestCancel()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "RequestCancel must be called from main thread");
		m_bCancellationRequested = true;
	}

	/**
	 * Check if cancellation has been requested for this operation.
	 * @note Must be called from the main thread only.
	 */
	bool IsCancellationRequested() const
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "IsCancellationRequested must be called from main thread");
		return m_bCancellationRequested;
	}

	//==========================================================================
	// Result Access
	//==========================================================================

	/**
	 * Get the resulting scene handle (only valid after IsComplete() returns true).
	 *
	 * Returns:
	 *   - LoadSceneAsync success: handle to the loaded scene, with captured generation.
	 *   - LoadSceneAsync failure/cancel: Zenith_Scene::INVALID_SCENE.
	 *   - UnloadSceneAsync (any outcome): Zenith_Scene::INVALID_SCENE — the scene
	 *     the caller asked about is gone by the time the op completes. Code paths that
	 *     want to refer to the unloaded scene should capture its handle before calling
	 *     UnloadSceneAsync.
	 */
	Zenith_Scene GetResultScene() const;

private:
	// Cancellation request flag - not atomic (only modified from main thread)
	bool m_bCancellationRequested = false;

	// Progress (0.0 to 1.0) - atomic for thread-safe reads during async load.
	// N8: C++ standard does not guarantee std::atomic<float> is lock-free on all
	// platforms. We rely on lock-free behaviour for cheap worker-thread writes;
	// the static_assert below ensures a port to a platform without it would fail
	// fast at compile time rather than introducing silent lock acquisition.
	std::atomic<float> m_fProgress = 0.0f;
	static_assert(std::atomic<float>::is_always_lock_free,
		"Zenith_SceneOperation::m_fProgress must be lock-free on the target platform; "
		"otherwise worker-thread progress updates acquire a lock that defeats the "
		"async-loading concurrency model.");

	// Completion flag - atomic for thread-safe polling
	std::atomic<bool> m_bIsComplete = false;

	// Activation control - not atomic (only modified from main thread)
	bool m_bActivationAllowed = true;

	// Error flag - set when operation fails (file not found, circular load, etc.)
	bool m_bHasFailed = false;

	// Completion callback
	CompletionCallback m_pfnOnComplete = nullptr;

	// Priority for ordering multiple async operations (0 = low, higher = more priority)
	int m_iPriority = 0;

	// Result scene handle (set by SceneManager when load completes)
	int m_iResultSceneHandle = -1;

	// Result scene generation captured at the point the result was recorded.
	// Stored alongside the handle so GetResultScene/FireCompletionCallback can
	// return a Zenith_Scene that stays faithful to the moment the op completed
	// — otherwise a scene unloaded between completion and callback would cause
	// the callback to receive a freshly-recycled handle pointing at a different
	// scene (or an invalid one if the slot is free).
	uint32_t m_uResultSceneGeneration = 0;

	// Frame counter for delayed cleanup (allows result access after completion)
	uint32_t m_uFramesSinceComplete = 0;

	// Operation ID for safe access via GetOperation()
	uint64_t m_ulOperationID = 0;

	// Scene manager needs access to internal state
	friend class Zenith_SceneManager;

	// A3: queue runs the phase machines that mutate operation state directly.
	friend class Zenith_SceneOperationQueue;

	// Unit tests exercise the generation-capture invariant directly (F8 regression).
	friend class Zenith_SceneTests;

	//==========================================================================
	// Internal Methods (SceneManager use only)
	//==========================================================================

	void SetProgress(float fProgress)
	{
		// D3: progress must be a normalised [0, 1] fraction. Upstream call sites
		// feed the FILE_READ / DESERIALIZE milestones defined in the header comment;
		// any out-of-range value means a caller bug, not user input.
		Zenith_Assert(fProgress >= 0.0f && fProgress <= 1.0f,
			"Zenith_SceneOperation::SetProgress: value %f out of [0, 1]", fProgress);
		m_fProgress.store(fProgress, std::memory_order_release);
	}
	void SetComplete(bool bComplete)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetComplete must be called from main thread");
		m_bIsComplete.store(bComplete, std::memory_order_release);
	}
	void SetResultSceneHandle(int iHandle)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetResultSceneHandle must be called from main thread");
		m_iResultSceneHandle = iHandle;
		m_uResultSceneGeneration = 0;  // invalid-result path; callers that want a live generation must use SetResultScene
	}

	// Preferred setter for success paths: captures both the handle and the scene's
	// generation at the moment of set, so a later completion callback sees the
	// scene that existed at op-completion time, not whatever currently sits in
	// the slot.
	void SetResultScene(int iHandle, uint32_t uGeneration)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetResultScene must be called from main thread");
		m_iResultSceneHandle = iHandle;
		m_uResultSceneGeneration = uGeneration;
	}
	void SetFailed(bool bFailed)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(), "SetFailed must be called from main thread");
		m_bHasFailed = bFailed;
	}

	// Fire completion callback if set
	void FireCompletionCallback();
};
