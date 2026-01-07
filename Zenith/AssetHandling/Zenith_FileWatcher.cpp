#include "Zenith.h"
#include "AssetHandling/Zenith_FileWatcher.h"
#include "AssetHandling/Zenith_AssetDatabase.h"
#include "AssetHandling/Zenith_AssetMeta.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

// Static member initialization
bool Zenith_FileWatcher::s_bInitialized = false;
bool Zenith_FileWatcher::s_bPaused = false;
std::string Zenith_FileWatcher::s_strWatchPath;
Zenith_Vector<FileChangeEvent> Zenith_FileWatcher::s_xPendingEvents;
Zenith_Mutex Zenith_FileWatcher::s_xEventMutex;
std::unordered_map<uint32_t, Zenith_FileWatcher::ChangeCallback> Zenith_FileWatcher::s_xCallbacks;
uint32_t Zenith_FileWatcher::s_uNextCallbackHandle = 1;
Zenith_Mutex Zenith_FileWatcher::s_xCallbackMutex;
std::unordered_map<std::string, uint64_t> Zenith_FileWatcher::s_xFileModTimes;

#ifdef _WIN32
void* Zenith_FileWatcher::s_hDirectory = INVALID_HANDLE_VALUE;
void* Zenith_FileWatcher::s_hCompletionPort = nullptr;
void* Zenith_FileWatcher::s_hWatchThread = nullptr;
bool Zenith_FileWatcher::s_bWatchThreadRunning = false;
#endif


//=============================================================================
// Lifecycle
//=============================================================================

void Zenith_FileWatcher::Initialize(const std::string& strWatchPath)
{
	if (s_bInitialized)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher already initialized");
		return;
	}

	s_strWatchPath = strWatchPath;
	s_bPaused = false;
	s_xPendingEvents.Clear();
	s_xFileModTimes.clear();

	// Build initial file modification time cache
	ForceRescan();

	// Start platform-specific watching
	Platform_StartWatching();

	s_bInitialized = true;
	Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher initialized for path: %s", strWatchPath.c_str());
}

void Zenith_FileWatcher::Shutdown()
{
	if (!s_bInitialized)
	{
		return;
	}

	Platform_StopWatching();

	{
		Zenith_ScopedMutexLock xLock(s_xEventMutex);
		s_xPendingEvents.Clear();
	}

	{
		Zenith_ScopedMutexLock xLock(s_xCallbackMutex);
		s_xCallbacks.clear();
	}

	s_xFileModTimes.clear();
	s_strWatchPath.clear();
	s_bInitialized = false;

	Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher shutdown complete");
}

//=============================================================================
// Update
//=============================================================================

void Zenith_FileWatcher::Update()
{
	if (!s_bInitialized || s_bPaused)
	{
		return;
	}

	// Check for changes (platform-specific or polling)
	Platform_CheckForChanges();

	// Process any pending events
	ProcessEvents();
}

//=============================================================================
// Callbacks
//=============================================================================

uint32_t Zenith_FileWatcher::RegisterCallback(ChangeCallback pfnCallback)
{
	Zenith_ScopedMutexLock xLock(s_xCallbackMutex);
	uint32_t uHandle = s_uNextCallbackHandle++;
	s_xCallbacks[uHandle] = pfnCallback;
	return uHandle;
}

void Zenith_FileWatcher::UnregisterCallback(uint32_t uHandle)
{
	Zenith_ScopedMutexLock xLock(s_xCallbackMutex);
	s_xCallbacks.erase(uHandle);
}

//=============================================================================
// Utility
//=============================================================================

void Zenith_FileWatcher::ForceRescan()
{
	s_xFileModTimes.clear();

	if (s_strWatchPath.empty() || !std::filesystem::exists(s_strWatchPath))
	{
		return;
	}

	// Recursively scan directory and build mod time cache
	for (const auto& xEntry : std::filesystem::recursive_directory_iterator(s_strWatchPath))
	{
		if (xEntry.is_regular_file())
		{
			std::string strPath = xEntry.path().string();
			if (!IsIgnoredFile(strPath))
			{
				s_xFileModTimes[strPath] = GetFileModificationTime(strPath);
			}
		}
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher scanned %zu files", s_xFileModTimes.size());
}

void Zenith_FileWatcher::SetPaused(bool bPaused)
{
	s_bPaused = bPaused;
	Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher %s", bPaused ? "paused" : "resumed");
}

//=============================================================================
// Internal Helpers
//=============================================================================

void Zenith_FileWatcher::EnqueueEvent(const FileChangeEvent& xEvent)
{
	Zenith_ScopedMutexLock xLock(s_xEventMutex);

	// Debounce: Check if we already have a similar event pending
	for (u_int u = 0; u < s_xPendingEvents.GetSize(); ++u)
	{
		FileChangeEvent& xPending = s_xPendingEvents.Get(u);
		if (xPending.m_strPath == xEvent.m_strPath && xPending.m_eType == xEvent.m_eType)
		{
			// Update timestamp instead of adding duplicate
			xPending.m_ulTimestamp = xEvent.m_ulTimestamp;
			return;
		}
	}

	s_xPendingEvents.PushBack(xEvent);
}

void Zenith_FileWatcher::ProcessEvents()
{
	Zenith_Vector<FileChangeEvent> xEventsToProcess;

	{
		Zenith_ScopedMutexLock xLock(s_xEventMutex);
		xEventsToProcess = std::move(s_xPendingEvents);
	}

	for (Zenith_Vector<FileChangeEvent>::Iterator xIt(xEventsToProcess); !xIt.Done(); xIt.Next())
	{
		NotifyCallbacks(xIt.GetData());
	}
}

void Zenith_FileWatcher::NotifyCallbacks(const FileChangeEvent& xEvent)
{
	Zenith_Vector<ChangeCallback> xCallbacksCopy;

	{
		Zenith_ScopedMutexLock xLock(s_xCallbackMutex);
		for (const auto& xPair : s_xCallbacks)
		{
			xCallbacksCopy.PushBack(xPair.second);
		}
	}

	for (Zenith_Vector<ChangeCallback>::Iterator xIt(xCallbacksCopy); !xIt.Done(); xIt.Next())
	{
		xIt.GetData()(xEvent);
	}
}

bool Zenith_FileWatcher::IsIgnoredFile(const std::string& strPath)
{
	// Ignore temp files, backups, and system files
	std::filesystem::path xPath(strPath);
	std::string strFilename = xPath.filename().string();

	// Ignore hidden files
	if (!strFilename.empty() && strFilename[0] == '.')
	{
		return true;
	}

	// Ignore backup files
	if (strFilename.find('~') != std::string::npos)
	{
		return true;
	}

	// Ignore temp files
	std::string strExt = xPath.extension().string();
	if (strExt == ".tmp" || strExt == ".temp" || strExt == ".swp")
	{
		return true;
	}

	return false;
}

uint64_t Zenith_FileWatcher::GetFileModificationTime(const std::string& strPath)
{
	try
	{
		auto xTime = std::filesystem::last_write_time(strPath);
		return static_cast<uint64_t>(xTime.time_since_epoch().count());
	}
	catch (...)
	{
		return 0;
	}
}

//=============================================================================
// Platform-Specific Implementation - Windows
//=============================================================================

#ifdef _WIN32

void Zenith_FileWatcher::WatchThreadFunc(const void* /*pUserData*/)
{
	constexpr DWORD BUFFER_SIZE = 32768;
	char acBuffer[BUFFER_SIZE];
	OVERLAPPED xOverlapped = {};
	xOverlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (xOverlapped.hEvent == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher failed to create event");
		return;
	}

	HANDLE hDirectory = static_cast<HANDLE>(s_hDirectory);

	while (s_bWatchThreadRunning)
	{
		DWORD dwBytesReturned = 0;

		BOOL bResult = ReadDirectoryChangesW(
			hDirectory,
			acBuffer,
			BUFFER_SIZE,
			TRUE,  // Watch subdirectories
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			&dwBytesReturned,
			&xOverlapped,
			nullptr
		);

		if (!bResult)
		{
			DWORD dwError = GetLastError();
			if (dwError != ERROR_IO_PENDING)
			{
				Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher ReadDirectoryChangesW failed: %u", dwError);
				break;
			}
		}

		// Wait for changes or shutdown
		DWORD dwWaitResult = WaitForSingleObject(xOverlapped.hEvent, 100);

		if (dwWaitResult == WAIT_OBJECT_0)
		{
			if (GetOverlappedResult(hDirectory, &xOverlapped, &dwBytesReturned, FALSE))
			{
				if (dwBytesReturned > 0)
				{
					FILE_NOTIFY_INFORMATION* pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(acBuffer);

					while (pInfo)
					{
						// Convert wide string to narrow string
						int iLen = WideCharToMultiByte(CP_UTF8, 0, pInfo->FileName,
							pInfo->FileNameLength / sizeof(WCHAR), nullptr, 0, nullptr, nullptr);

						std::string strFilename(iLen, '\0');
						WideCharToMultiByte(CP_UTF8, 0, pInfo->FileName,
							pInfo->FileNameLength / sizeof(WCHAR), &strFilename[0], iLen, nullptr, nullptr);

						std::string strFullPath = s_strWatchPath + "/" + strFilename;

						// Replace backslashes with forward slashes
						std::replace(strFullPath.begin(), strFullPath.end(), '\\', '/');

						if (!IsIgnoredFile(strFullPath))
						{
							FileChangeEvent xEvent;
							xEvent.m_strPath = strFullPath;
							xEvent.m_ulTimestamp = static_cast<uint64_t>(GetTickCount64());

							switch (pInfo->Action)
							{
							case FILE_ACTION_ADDED:
								xEvent.m_eType = FileChangeType::ADDED;
								break;
							case FILE_ACTION_REMOVED:
								xEvent.m_eType = FileChangeType::DELETED;
								break;
							case FILE_ACTION_MODIFIED:
								xEvent.m_eType = FileChangeType::MODIFIED;
								break;
							case FILE_ACTION_RENAMED_OLD_NAME:
								xEvent.m_eType = FileChangeType::RENAMED;
								xEvent.m_strOldPath = strFullPath;
								break;
							case FILE_ACTION_RENAMED_NEW_NAME:
								xEvent.m_eType = FileChangeType::RENAMED;
								break;
							default:
								goto next_entry;
							}

							EnqueueEvent(xEvent);
						}

					next_entry:
						if (pInfo->NextEntryOffset == 0)
						{
							break;
						}
						pInfo = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
							reinterpret_cast<char*>(pInfo) + pInfo->NextEntryOffset);
					}
				}
			}

			ResetEvent(xOverlapped.hEvent);
		}
	}

	CloseHandle(xOverlapped.hEvent);
}

void Zenith_FileWatcher::Platform_StartWatching()
{
	// Convert path to wide string
	std::wstring wstrPath(s_strWatchPath.begin(), s_strWatchPath.end());

	s_hDirectory = CreateFileW(
		wstrPath.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		nullptr
	);

	if (s_hDirectory == INVALID_HANDLE_VALUE)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "FileWatcher failed to open directory for watching: %u", GetLastError());
		return;
	}

	// Start watch thread
	s_bWatchThreadRunning = true;
	Zenith_Multithreading::CreateThread("FileWatcher", WatchThreadFunc, nullptr);

	Zenith_Log(LOG_CATEGORY_ASSET, "Windows file watcher started");
}

void Zenith_FileWatcher::Platform_StopWatching()
{
	s_bWatchThreadRunning = false;

	if (s_hDirectory != INVALID_HANDLE_VALUE)
	{
		CancelIo(static_cast<HANDLE>(s_hDirectory));
		CloseHandle(static_cast<HANDLE>(s_hDirectory));
		s_hDirectory = INVALID_HANDLE_VALUE;
	}

	// Give thread time to exit
	Sleep(200);

	Zenith_Log(LOG_CATEGORY_ASSET, "Windows file watcher stopped");
}

void Zenith_FileWatcher::Platform_CheckForChanges()
{
	// On Windows, the watch thread handles change detection
	// This function is just for processing the event queue
}

#else

//=============================================================================
// Platform-Specific Implementation - Polling Fallback
//=============================================================================

void Zenith_FileWatcher::Platform_StartWatching()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Using polling-based file watcher");
}

void Zenith_FileWatcher::Platform_StopWatching()
{
	// Nothing to clean up for polling
}

void Zenith_FileWatcher::Platform_CheckForChanges()
{
	// Polling: Check modification times periodically
	// This is less efficient but works on all platforms

	if (s_strWatchPath.empty() || !std::filesystem::exists(s_strWatchPath))
	{
		return;
	}

	std::unordered_map<std::string, uint64_t> xCurrentFiles;

	// Scan for current files
	try
	{
		for (const auto& xEntry : std::filesystem::recursive_directory_iterator(s_strWatchPath))
		{
			if (xEntry.is_regular_file())
			{
				std::string strPath = xEntry.path().string();
				if (!IsIgnoredFile(strPath))
				{
					xCurrentFiles[strPath] = GetFileModificationTime(strPath);
				}
			}
		}
	}
	catch (...)
	{
		// Ignore filesystem errors during scan
		return;
	}

	// Check for new and modified files
	for (const auto& xPair : xCurrentFiles)
	{
		const std::string& strPath = xPair.first;
		uint64_t ulCurrentTime = xPair.second;

		auto xIt = s_xFileModTimes.find(strPath);
		if (xIt == s_xFileModTimes.end())
		{
			// New file
			FileChangeEvent xEvent;
			xEvent.m_eType = FileChangeType::ADDED;
			xEvent.m_strPath = strPath;
			xEvent.m_ulTimestamp = ulCurrentTime;
			EnqueueEvent(xEvent);
		}
		else if (xIt->second != ulCurrentTime)
		{
			// Modified file
			FileChangeEvent xEvent;
			xEvent.m_eType = FileChangeType::MODIFIED;
			xEvent.m_strPath = strPath;
			xEvent.m_ulTimestamp = ulCurrentTime;
			EnqueueEvent(xEvent);
		}
	}

	// Check for deleted files
	for (const auto& xPair : s_xFileModTimes)
	{
		if (xCurrentFiles.find(xPair.first) == xCurrentFiles.end())
		{
			FileChangeEvent xEvent;
			xEvent.m_eType = FileChangeType::DELETED;
			xEvent.m_strPath = xPair.first;
			xEvent.m_ulTimestamp = 0;
			EnqueueEvent(xEvent);
		}
	}

	// Update cache
	s_xFileModTimes = std::move(xCurrentFiles);
}

#endif
