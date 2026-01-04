#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include "Collections/Zenith_Vector.h"
#include "Core/Zenith_GUID.h"
#include "Core/Multithreading/Zenith_Multithreading.h"

/**
 * Zenith_FileWatcher - Hot-reload file monitoring system
 *
 * Monitors the project asset directory for file changes and notifies
 * the AssetDatabase when assets are modified, added, or deleted.
 *
 * Platform-specific:
 * - Windows: Uses ReadDirectoryChangesW for efficient change notification
 * - Other platforms: Polling-based fallback (checks modification times)
 *
 * Usage:
 *   // Initialize once at startup
 *   Zenith_FileWatcher::Initialize("path/to/project/Assets");
 *
 *   // Each frame, process any pending file changes
 *   Zenith_FileWatcher::Update();
 *
 *   // Shutdown when done
 *   Zenith_FileWatcher::Shutdown();
 */

enum class FileChangeType : uint32_t
{
	ADDED,
	MODIFIED,
	DELETED,
	RENAMED
};

struct FileChangeEvent
{
	FileChangeType m_eType;
	std::string m_strPath;
	std::string m_strOldPath;  // Only used for RENAMED
	uint64_t m_ulTimestamp;    // When the event was detected
};

class Zenith_FileWatcher
{
public:
	using ChangeCallback = std::function<void(const FileChangeEvent&)>;

	//--------------------------------------------------------------------------
	// Lifecycle
	//--------------------------------------------------------------------------

	/**
	 * Initialize the file watcher
	 * @param strWatchPath Root directory to watch (typically project Assets folder)
	 */
	static void Initialize(const std::string& strWatchPath);

	/**
	 * Shutdown the file watcher and release resources
	 */
	static void Shutdown();

	/**
	 * Check if the file watcher is initialized
	 */
	static bool IsInitialized() { return s_bInitialized; }

	//--------------------------------------------------------------------------
	// Update
	//--------------------------------------------------------------------------

	/**
	 * Process pending file change events
	 * Call this once per frame from the main thread
	 */
	static void Update();

	//--------------------------------------------------------------------------
	// Callbacks
	//--------------------------------------------------------------------------

	/**
	 * Register a callback for file change events
	 * @param pfnCallback Function to call when files change
	 * @return Handle for unregistering the callback
	 */
	static uint32_t RegisterCallback(ChangeCallback pfnCallback);

	/**
	 * Unregister a previously registered callback
	 * @param uHandle Handle returned by RegisterCallback
	 */
	static void UnregisterCallback(uint32_t uHandle);

	//--------------------------------------------------------------------------
	// Utility
	//--------------------------------------------------------------------------

	/**
	 * Force a rescan of all files in the watch directory
	 * Useful after bulk operations or to sync state
	 */
	static void ForceRescan();

	/**
	 * Pause/resume file watching
	 * Useful during save operations to avoid self-triggering
	 */
	static void SetPaused(bool bPaused);
	static bool IsPaused() { return s_bPaused; }

	/**
	 * Get the watch directory path
	 */
	static const std::string& GetWatchPath() { return s_strWatchPath; }

private:
	//--------------------------------------------------------------------------
	// Platform-specific implementation
	//--------------------------------------------------------------------------

	static void Platform_StartWatching();
	static void Platform_StopWatching();
	static void Platform_CheckForChanges();

	//--------------------------------------------------------------------------
	// Internal helpers
	//--------------------------------------------------------------------------

	static void EnqueueEvent(const FileChangeEvent& xEvent);
	static void ProcessEvents();
	static void NotifyCallbacks(const FileChangeEvent& xEvent);
	static bool IsIgnoredFile(const std::string& strPath);
	static uint64_t GetFileModificationTime(const std::string& strPath);

	//--------------------------------------------------------------------------
	// State
	//--------------------------------------------------------------------------

	static bool s_bInitialized;
	static bool s_bPaused;
	static std::string s_strWatchPath;

	// Event queue (thread-safe)
	static Zenith_Vector<FileChangeEvent> s_xPendingEvents;
	static Zenith_Mutex s_xEventMutex;

	// Callbacks
	static std::unordered_map<uint32_t, ChangeCallback> s_xCallbacks;
	static uint32_t s_uNextCallbackHandle;
	static Zenith_Mutex s_xCallbackMutex;

	// File modification times for polling fallback
	static std::unordered_map<std::string, uint64_t> s_xFileModTimes;

#ifdef _WIN32
	// Windows-specific handles
	static void* s_hDirectory;
	static void* s_hCompletionPort;
	static void* s_hWatchThread;
	static bool s_bWatchThreadRunning;

	// Windows watch thread function
	static void WatchThreadFunc(const void* pUserData);
#endif
};
