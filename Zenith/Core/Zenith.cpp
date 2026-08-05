#include "Zenith.h"

// ============================================================================
// THE LOG FILE SINK.
//
// WHY IT LIVES IN THIS FILE, WHICH WAS OTHERWISE EMPTY. Zenith_LogImpl is an
// INLINE function in Zenith.h -- i.e. in the PCH every library compiles against,
// including the L0 leaf (ZenithBase) and the leaves above it. Anything it calls
// must therefore resolve at link time for EVERY one of those libs, and the
// Sentinel link proofs deliberately link ONLY zenithbase.lib + <leaf>.lib.
//
// ZenithBase's membership regex (Build/Sharpmake_Zenith.cs) strips every
// top-level Core\Zenith*.cpp EXCEPT Zenith.cpp and Zenith_String.cpp, so a NEW
// Core\Zenith_LogFileSink.cpp would compile into the aggregate engine lib but NOT
// into ZenithBase -- and the Sentinels would fail to link on an undefined
// Zenith_LogFileSinkWrite. Putting the implementation HERE, in a file that is
// already an L0 member, makes the symbol available to every lib with no Sharpmake
// change and no new link edge. Do not move it out without re-reading that regex.
//
// WHAT IT IS FOR. The engine logged only to stdout, so any run not started from a
// redirected shell left NO record -- which is exactly the case that matters, since
// the interesting runs are the ones a human launched by hand and then watched
// something go wrong in. It cost this project two lost repro logs in one session.
//
// ★★ IT MUST NOT ALLOCATE, AND THAT IS NOT A STYLE PREFERENCE -- IT IS THE WHOLE
// DESIGN CONSTRAINT. The FIRST log line of the process is emitted from a STATIC
// INITIALISER (an AssetRegistry serializable-type registration), long before
// Zenith_MemoryManagement is initialised. A first version of this sink used
// std::filesystem, std::string and Zenith_Vector; every one of those routes
// through the tracked global operator new, and the process died with an access
// violation on that very first line, before main. So: fixed char buffers, raw
// Win32, and C stdio only. Nothing here may allocate, and nothing here may call
// anything that allocates. Re-read this paragraph before adding a convenience.
//
// Android is excluded because logcat already IS a persistent, timestamped,
// filterable sink there, and an app's writable directory is not known this early.
// Non-Windows desktop has no sink rather than an untested POSIX path.
// ============================================================================

#ifdef ZENITH_LOG
#ifdef ZENITH_WINDOWS

#include "Zenith_OS_Include.h"                          // Zenith_Mutex_NoProfiling
#include "Core/Multithreading/Zenith_Multithreading.h"  // Zenith_ScopedMutexLock_T (an L0 subdir)
#include "FileAccess/Zenith_FileAccess.h"   // ZENITH_MAX_PATH_LENGTH (an L0 member, so ZenithBase has it)
// GetModuleFileNameA / CreateDirectoryA / FindFirstFileA. <Windows.h> is
// deliberately NOT in the PCH (the W5.2 note in Zenith.h) and including it raw
// trips a warning-as-error inside minwindef.h; Zenith_Win32.h is the wrapper
// carrying the APIENTRY + LEAN_AND_MEAN guards, exactly as Zenith_SaveData.cpp
// does for the same call.
#include "Core/Zenith_Win32.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace
{
	// Keep a handful of runs. The bug being chased is usually reproduced several
	// times in a row, and overwriting a single file would destroy the interesting
	// capture on the very next attempt -- which is the failure mode this sink exists
	// to prevent. Newest are kept; the rest are pruned when the file is opened.
	constexpr u_int uMAX_RETAINED_LOG_FILES = 10u;
	// Bounded scan. More logs than this in the directory simply means several prune
	// passes across successive runs, which converges -- and a fixed bound is what
	// keeps the enumeration allocation-free.
	constexpr u_int uMAX_SCANNED_LOG_FILES = 64u;

	FILE* g_pxLogFile = nullptr;
	bool  g_bOpenAttempted = false;
	char  g_acLogFilePath[ZENITH_MAX_PATH_LENGTH] = { 0 };
	char  g_acLogDirectory[ZENITH_MAX_PATH_LENGTH] = { 0 };

	// The directory holding this EXECUTABLE -- not the working directory, which
	// varies with how the game was launched (`zenith run` from the repo root, VS
	// from the project dir, a double-click from the output dir). Same reasoning as
	// Zenith_SaveData::GetModuleDirectory.
	bool BuildLogDirectory()
	{
		char acModulePath[ZENITH_MAX_PATH_LENGTH] = { 0 };
		const DWORD uLen = ::GetModuleFileNameA(nullptr, acModulePath, ZENITH_MAX_PATH_LENGTH);
		if (uLen == 0 || uLen >= ZENITH_MAX_PATH_LENGTH)
		{
			return false;
		}

		// Trim to the parent directory in place -- no path library, no allocation.
		char* pcLastSlash = nullptr;
		for (char* pc = acModulePath; *pc != '\0'; ++pc)
		{
			if (*pc == '\\' || *pc == '/')
			{
				pcLastSlash = pc;
			}
		}
		if (pcLastSlash == nullptr)
		{
			return false;
		}
		*pcLastSlash = '\0';

		if (snprintf(g_acLogDirectory, sizeof(g_acLogDirectory), "%s\\Logs", acModulePath)
			>= (int)sizeof(g_acLogDirectory))
		{
			return false;
		}

		// Succeeds, or already exists — both are fine; anything else is fatal to the
		// sink but never to the process.
		if (!::CreateDirectoryA(g_acLogDirectory, nullptr)
			&& ::GetLastError() != ERROR_ALREADY_EXISTS)
		{
			return false;
		}
		return true;
	}

	// Best-effort throughout: a logging sink must never take the process down, so
	// every failure is swallowed. There is nowhere to report it TO — this IS the
	// reporting mechanism.
	void PruneOldLogs()
	{
		char acPattern[ZENITH_MAX_PATH_LENGTH] = { 0 };
		if (snprintf(acPattern, sizeof(acPattern), "%s\\zenith_*.log", g_acLogDirectory)
			>= (int)sizeof(acPattern))
		{
			return;
		}

		char     aacNames[uMAX_SCANNED_LOG_FILES][MAX_PATH] = {};
		FILETIME axTimes[uMAX_SCANNED_LOG_FILES] = {};
		u_int    uCount = 0u;

		WIN32_FIND_DATAA xFind = {};
		HANDLE xHandle = ::FindFirstFileA(acPattern, &xFind);
		if (xHandle == INVALID_HANDLE_VALUE)
		{
			return;
		}
		do
		{
			if ((xFind.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				continue;
			}
			strncpy_s(aacNames[uCount], sizeof(aacNames[uCount]), xFind.cFileName, _TRUNCATE);
			axTimes[uCount] = xFind.ftLastWriteTime;
			++uCount;
		} while (uCount < uMAX_SCANNED_LOG_FILES && ::FindNextFileA(xHandle, &xFind));
		::FindClose(xHandle);

		if (uCount <= uMAX_RETAINED_LOG_FILES)
		{
			return;
		}

		// Selection sort, oldest first. ~10 entries, so this beats pulling in a
		// general sort — and it stays allocation-free, which is the hard constraint.
		for (u_int u = 0; u < uCount; ++u)
		{
			u_int uOldest = u;
			for (u_int v = u + 1u; v < uCount; ++v)
			{
				if (::CompareFileTime(&axTimes[v], &axTimes[uOldest]) < 0)
				{
					uOldest = v;
				}
			}
			if (uOldest != u)
			{
				char acTmpName[MAX_PATH];
				strncpy_s(acTmpName, sizeof(acTmpName), aacNames[u], _TRUNCATE);
				strncpy_s(aacNames[u], sizeof(aacNames[u]), aacNames[uOldest], _TRUNCATE);
				strncpy_s(aacNames[uOldest], sizeof(aacNames[uOldest]), acTmpName, _TRUNCATE);
				const FILETIME xTmpTime = axTimes[u];
				axTimes[u] = axTimes[uOldest];
				axTimes[uOldest] = xTmpTime;
			}
		}

		const u_int uToRemove = uCount - uMAX_RETAINED_LOG_FILES;
		for (u_int u = 0; u < uToRemove; ++u)
		{
			char acVictim[ZENITH_MAX_PATH_LENGTH] = { 0 };
			if (snprintf(acVictim, sizeof(acVictim), "%s\\%s", g_acLogDirectory, aacNames[u])
				< (int)sizeof(acVictim))
			{
				::DeleteFileA(acVictim);
			}
		}
	}

	void OpenLogFileOnce()
	{
		if (g_bOpenAttempted)
		{
			return;
		}
		g_bOpenAttempted = true;   // Set FIRST: a failure must not retry on every line.

		if (!BuildLogDirectory())
		{
			return;
		}
		PruneOldLogs();

		// Timestamped rather than rotated-by-rename: the name itself tells a human
		// which run it was, which is what makes "the one where he fell" findable.
		std::time_t xNow = std::time(nullptr);
		std::tm xLocal = {};
		localtime_s(&xLocal, &xNow);
		char acName[64];
		std::strftime(acName, sizeof(acName), "zenith_%Y%m%d_%H%M%S.log", &xLocal);

		if (snprintf(g_acLogFilePath, sizeof(g_acLogFilePath), "%s\\%s", g_acLogDirectory, acName)
			>= (int)sizeof(g_acLogFilePath))
		{
			g_acLogFilePath[0] = '\0';
			return;
		}

		if (fopen_s(&g_pxLogFile, g_acLogFilePath, "w") != 0)
		{
			g_pxLogFile = nullptr;
			g_acLogFilePath[0] = '\0';
			return;
		}

		// Announced on stdout, NOT through Zenith_Log: this runs from inside the log
		// path itself and re-entering it would recurse.
		printf("[Core] Log file: %s\n", g_acLogFilePath);
		fflush(stdout);
	}
}

const char* Zenith_GetLogFilePath()
{
	return g_acLogFilePath;
}

void Zenith_LogFileSinkWrite(const char* szMessage, int eLevel)
{
	// Zenith_Mutex_NoProfiling, not Zenith_Mutex: the profiling variant reaches for
	// engine state, and this runs before the engine exists. Same choice, for the
	// same reason, as Zenith_MemoryTracker::Mutex and the AssetRegistry's
	// serializable-type registry mutex — both of which also run during static init.
	static Zenith_Mutex_NoProfiling s_xMutex;
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xMutex);

	OpenLogFileOnce();
	if (g_pxLogFile == nullptr)
	{
		return;
	}

	static const char* const aszLevel[] = { "INFO", "WARN", "ERROR" };
	const char* szLevel = (eLevel >= 0 && eLevel <= 2) ? aszLevel[eLevel] : "INFO";

	fprintf(g_pxLogFile, "[%-5s] %s\n", szLevel, szMessage);

	// Flush EVERY line, deliberately. The runs worth capturing are the ones that end
	// in a crash or a hang, and a buffered tail is exactly the part that would be
	// lost. Zenith_LogImpl already flushes stdout per line, so this does not change
	// the I/O shape of the engine.
	fflush(g_pxLogFile);
}

#endif   // ZENITH_WINDOWS
#endif   // ZENITH_LOG
