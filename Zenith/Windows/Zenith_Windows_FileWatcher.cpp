#include "Zenith.h"

#ifdef ZENITH_TOOLS
#ifdef ZENITH_WINDOWS

#include "Core/Zenith_FileWatcher.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include <Windows.h>
#include <queue>

// Windows-specific data for file watching
struct FileWatcherPlatformData
{
	HANDLE hDirectory = INVALID_HANDLE_VALUE;
	HANDLE hCompletionPort = INVALID_HANDLE_VALUE;
	OVERLAPPED xOverlapped = {};
	alignas(DWORD) char acBuffer[32768]; // Buffer for ReadDirectoryChangesW
	std::queue<std::pair<std::string, FileChangeType>> xPendingChanges;
	Zenith_Mutex xMutex;
	bool bPendingRead = false;
};

Zenith_FileWatcher::Zenith_FileWatcher()
{
	m_pPlatformData = new FileWatcherPlatformData();
}

Zenith_FileWatcher::~Zenith_FileWatcher()
{
	Stop();
	delete static_cast<FileWatcherPlatformData*>(m_pPlatformData);
}

bool Zenith_FileWatcher::Start(const std::string& strDirectory, bool bRecursive, FileChangeCallback pfnCallback)
{
	if (m_bRunning)
	{
		Stop();
	}

	m_strDirectory = strDirectory;
	m_bRecursive = bRecursive;
	m_pfnCallback = pfnCallback;

	if (!StartPlatform())
	{
		return false;
	}

	m_bRunning = true;
	Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher started: %s (recursive: %s)",
			   strDirectory.c_str(), bRecursive ? "yes" : "no");
	return true;
}

void Zenith_FileWatcher::Stop()
{
	if (!m_bRunning)
	{
		return;
	}

	StopPlatform();
	m_bRunning = false;
	Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher stopped");
}

void Zenith_FileWatcher::Update()
{
	if (!m_bRunning)
	{
		return;
	}

	UpdatePlatform();
}

bool Zenith_FileWatcher::StartPlatform()
{
	FileWatcherPlatformData* pData = static_cast<FileWatcherPlatformData*>(m_pPlatformData);

	// Open directory handle for overlapped I/O
	pData->hDirectory = CreateFileA(
		m_strDirectory.c_str(),
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		nullptr,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		nullptr);

	if (pData->hDirectory == INVALID_HANDLE_VALUE)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher: Failed to open directory: %s (error %u)",
				   m_strDirectory.c_str(), GetLastError());
		return false;
	}

	// Create I/O completion port for async notifications
	pData->hCompletionPort = CreateIoCompletionPort(pData->hDirectory, nullptr, 0, 1);
	if (pData->hCompletionPort == nullptr)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher: Failed to create completion port (error %u)", GetLastError());
		CloseHandle(pData->hDirectory);
		pData->hDirectory = INVALID_HANDLE_VALUE;
		return false;
	}

	// Start watching
	memset(&pData->xOverlapped, 0, sizeof(pData->xOverlapped));

	DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION;

	BOOL bResult = ReadDirectoryChangesW(
		pData->hDirectory,
		pData->acBuffer,
		sizeof(pData->acBuffer),
		m_bRecursive ? TRUE : FALSE,
		dwNotifyFilter,
		nullptr,
		&pData->xOverlapped,
		nullptr);

	if (!bResult)
	{
		Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher: ReadDirectoryChangesW failed (error %u)", GetLastError());
		CloseHandle(pData->hCompletionPort);
		CloseHandle(pData->hDirectory);
		pData->hCompletionPort = nullptr;
		pData->hDirectory = INVALID_HANDLE_VALUE;
		return false;
	}

	pData->bPendingRead = true;
	return true;
}

void Zenith_FileWatcher::StopPlatform()
{
	FileWatcherPlatformData* pData = static_cast<FileWatcherPlatformData*>(m_pPlatformData);

	if (pData->hCompletionPort != nullptr)
	{
		// Cancel pending I/O
		CancelIo(pData->hDirectory);
		CloseHandle(pData->hCompletionPort);
		pData->hCompletionPort = nullptr;
	}

	if (pData->hDirectory != INVALID_HANDLE_VALUE)
	{
		CloseHandle(pData->hDirectory);
		pData->hDirectory = INVALID_HANDLE_VALUE;
	}

	pData->bPendingRead = false;

	// Clear pending changes
	Zenith_ScopedMutexLock xLock(pData->xMutex);
	while (!pData->xPendingChanges.empty())
	{
		pData->xPendingChanges.pop();
	}
}

void Zenith_FileWatcher::UpdatePlatform()
{
	FileWatcherPlatformData* pData = static_cast<FileWatcherPlatformData*>(m_pPlatformData);

	if (!pData->bPendingRead)
	{
		return;
	}

	// Check for completion (non-blocking)
	DWORD dwBytesTransferred = 0;
	ULONG_PTR ulKey = 0;
	LPOVERLAPPED pOverlapped = nullptr;

	BOOL bResult = GetQueuedCompletionStatus(
		pData->hCompletionPort,
		&dwBytesTransferred,
		&ulKey,
		&pOverlapped,
		0); // Non-blocking (0ms timeout)

	if (!bResult)
	{
		DWORD dwError = GetLastError();
		if (dwError == WAIT_TIMEOUT)
		{
			// No changes yet - this is normal
			return;
		}

		// Error occurred
		Zenith_Log(LOG_CATEGORY_CORE, "FileWatcher: GetQueuedCompletionStatus failed (error %u)", dwError);
		return;
	}

	if (dwBytesTransferred == 0)
	{
		// Buffer overflow - re-issue the read
		pData->bPendingRead = false;
	}
	else
	{
		// Process notifications
		FILE_NOTIFY_INFORMATION* pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(pData->acBuffer);

		while (pNotify != nullptr)
		{
			// Convert wide string filename to narrow string
			int iLen = WideCharToMultiByte(CP_UTF8, 0, pNotify->FileName,
				pNotify->FileNameLength / sizeof(WCHAR), nullptr, 0, nullptr, nullptr);

			std::string strFileName(iLen, '\0');
			WideCharToMultiByte(CP_UTF8, 0, pNotify->FileName,
				pNotify->FileNameLength / sizeof(WCHAR), &strFileName[0], iLen, nullptr, nullptr);

			// Normalize path separators
			for (char& c : strFileName)
			{
				if (c == '\\')
				{
					c = '/';
				}
			}

			// Determine change type
			FileChangeType eType = FileChangeType::Modified;
			switch (pNotify->Action)
			{
			case FILE_ACTION_ADDED:
				eType = FileChangeType::Created;
				break;
			case FILE_ACTION_REMOVED:
				eType = FileChangeType::Deleted;
				break;
			case FILE_ACTION_MODIFIED:
				eType = FileChangeType::Modified;
				break;
			case FILE_ACTION_RENAMED_OLD_NAME:
			case FILE_ACTION_RENAMED_NEW_NAME:
				eType = FileChangeType::Renamed;
				break;
			}

			// Queue the change for callback dispatch
			{
				Zenith_ScopedMutexLock xLock(pData->xMutex);
				pData->xPendingChanges.push({ strFileName, eType });
			}

			// Move to next notification
			if (pNotify->NextEntryOffset == 0)
			{
				break;
			}
			pNotify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
				reinterpret_cast<char*>(pNotify) + pNotify->NextEntryOffset);
		}
	}

	// Re-issue the read for more changes
	memset(&pData->xOverlapped, 0, sizeof(pData->xOverlapped));

	DWORD dwNotifyFilter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION;

	bResult = ReadDirectoryChangesW(
		pData->hDirectory,
		pData->acBuffer,
		sizeof(pData->acBuffer),
		m_bRecursive ? TRUE : FALSE,
		dwNotifyFilter,
		nullptr,
		&pData->xOverlapped,
		nullptr);

	pData->bPendingRead = (bResult != FALSE);

	// Dispatch pending changes via callback
	while (true)
	{
		std::pair<std::string, FileChangeType> xChange;

		{
			Zenith_ScopedMutexLock xLock(pData->xMutex);
			if (pData->xPendingChanges.empty())
			{
				break;
			}
			xChange = pData->xPendingChanges.front();
			pData->xPendingChanges.pop();
		}

		if (m_pfnCallback)
		{
			m_pfnCallback(xChange.first, xChange.second);
		}
	}
}

#endif // ZENITH_WINDOWS
#endif // ZENITH_TOOLS
