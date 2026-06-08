#pragma once

#ifdef ZENITH_TOOLS

#include <string>

// File change notification types
enum class FileChangeType
{
	Modified,
	Created,
	Deleted,
	Renamed
};

// Callback signature for file change notifications
// Parameters: opaque caller context (passed through Start), file path
// (relative to watched directory), change type. std::function is forbidden
// engine-wide; capturing state must be routed through pContext.
typedef void (*FileChangeCallback)(void* pContext, const std::string&, FileChangeType);

// Platform-agnostic file watcher interface
// Watches a directory (and optionally subdirectories) for file changes
//
// Usage:
//   Zenith_FileWatcher watcher;
//   watcher.Start("C:/dev/shaders", true,
//       [](void* pContext, const std::string& path, FileChangeType type) {
//           if (type == FileChangeType::Modified) {
//               // Handle shader modification — recover state from pContext
//           }
//       },
//       pMyContext);
//
//   // In main loop:
//   watcher.Update();
//
//   // On shutdown:
//   watcher.Stop();
//
class Zenith_FileWatcher
{
public:
	Zenith_FileWatcher();
	~Zenith_FileWatcher();

	// Start watching a directory
	// strDirectory: Directory to watch
	// bRecursive: Watch subdirectories
	// pfnCallback: Callback for file changes
	// pContext: Opaque pointer passed back as the callback's first argument
	// Returns true if watcher started successfully
	bool Start(const std::string& strDirectory, bool bRecursive, FileChangeCallback pfnCallback, void* pContext = nullptr);

	// Stop watching
	void Stop();

	// Check for and dispatch pending file change notifications
	// Must be called regularly (e.g., each frame) to process changes
	void Update();

	// Check if watcher is currently running
	bool IsRunning() const { return m_bRunning; }

	// Get the watched directory
	const std::string& GetWatchedDirectory() const { return m_strDirectory; }

private:
	std::string m_strDirectory;
	FileChangeCallback m_pfnCallback = nullptr;
	void* m_pCallbackContext = nullptr;
	bool m_bRunning = false;
	bool m_bRecursive = false;

	// Platform-specific implementation data
	void* m_pPlatformData = nullptr;

	// Platform-specific methods
	bool StartPlatform();
	void StopPlatform();
	void UpdatePlatform();
};

#endif // ZENITH_TOOLS
