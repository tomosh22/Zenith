#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Flux/Slang/Flux_ShaderHotReload.h"
#include "Flux/Slang/Flux_ShaderRegistry.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Core/Zenith_FileWatcher.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Zenith_PlatformGraphics_Include.h"
#include <algorithm>
#include <string>
#include <unordered_set> // #TODO: Replace with engine hash set

struct RegisteredProgram
{
	FluxShaderProgram                            m_eProgram   = FluxShaderProgram::COUNT;
	Flux_ShaderHotReload::ProgramRebuildCallback m_pfnRebuild = nullptr;
	bool                                         m_bNeedsReload = false;
};

static Zenith_FileWatcher                 s_xFileWatcher;
static Zenith_Vector<RegisteredProgram>   s_axRegistered;
static std::unordered_set<std::string>    s_xPendingChanges;
static Zenith_Mutex                       s_xMutex;

bool   Flux_ShaderHotReload::s_bInitialised      = false;
bool   Flux_ShaderHotReload::s_bEnabled          = true;
u_int  Flux_ShaderHotReload::s_uReloadCount      = 0;
u_int  Flux_ShaderHotReload::s_uFailedReloadCount = 0;

static std::string NormalizePath(const std::string& strPath)
{
	std::string strNorm = strPath;
	for (char& c : strNorm)
	{
		if (c == '\\') c = '/';
	}
	std::transform(strNorm.begin(), strNorm.end(), strNorm.begin(), ::tolower);
	return strNorm;
}

// True if szPath ends with `<m_szModuleName>.slang` (case-insensitive). The
// registry stores module names without an extension and with `/` separators
// already, so suffix-matching against a normalised path is exact.
static bool ProgramMatchesPath(const Flux_ShaderRegistryEntry& xEntry, const std::string& strNormPath)
{
	if (!xEntry.m_szModuleName) return false;
	std::string strModule = xEntry.m_szModuleName;
	std::string strSuffix = NormalizePath(strModule) + ".slang";
	if (strNormPath.size() < strSuffix.size()) return false;
	return strNormPath.compare(strNormPath.size() - strSuffix.size(), strSuffix.size(), strSuffix) == 0;
}

// True if the changed file lives under the Common/ shared-module folder. A
// change there can affect any program transitively, so we flag everything.
// File-watcher hands us relative paths (no leading slash), so check both the
// "starts with common/" and "contains /common/" cases.
static bool IsSharedModulePath(const std::string& strNormPath)
{
	if (strNormPath.rfind("common/", 0) == 0) return true;       // starts with
	if (strNormPath.find("/common/") != std::string::npos) return true;
	return false;
}

void Flux_ShaderHotReload::Initialise()
{
	if (s_bInitialised) return;

	std::string strRoot(SHADER_SOURCE_ROOT);
	const bool bStarted = s_xFileWatcher.Start(strRoot, true,
		[](const std::string& strPath, FileChangeType eType)
		{
			OnFileChanged(strPath.c_str(), static_cast<int>(eType));
		});

	if (!bStarted)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Failed to start file watcher for %s", strRoot.c_str());
		return;
	}

	s_bInitialised        = true;
	s_bEnabled            = true;
	s_uReloadCount        = 0;
	s_uFailedReloadCount  = 0;
	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload initialised — watching %s", strRoot.c_str());
}

void Flux_ShaderHotReload::Shutdown()
{
	if (!s_bInitialised) return;

	s_xFileWatcher.Stop();
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		s_axRegistered.Clear();
		s_xPendingChanges.clear();
	}

	s_bInitialised = false;
	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload shutdown — reloads:%u failed:%u",
			   s_uReloadCount, s_uFailedReloadCount);
}

void Flux_ShaderHotReload::SetEnabled(bool bEnabled)
{
	s_bEnabled = bEnabled;
	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload %s", bEnabled ? "enabled" : "disabled");
}

void Flux_ShaderHotReload::Update()
{
	if (!s_bInitialised || !s_bEnabled) return;
	s_xFileWatcher.Update();
	ProcessPendingReloads();
}

void Flux_ShaderHotReload::OnFileChanged(const char* szPath, int eChangeType)
{
	if (static_cast<FileChangeType>(eChangeType) != FileChangeType::Modified) return;
	if (!szPath || !szPath[0]) return;

	std::string strPath = szPath;
	const size_t ulDot = strPath.rfind('.');
	if (ulDot == std::string::npos) return;
	const std::string strExt = strPath.substr(ulDot);
	if (strExt != ".slang") return;

	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		s_xPendingChanges.insert(strPath);
	}
	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: detected change %s", szPath);
}

void Flux_ShaderHotReload::MarkProgramsForReload(const char* szChangedFile)
{
	const std::string strNorm = NormalizePath(szChangedFile);
	const bool bShared = IsSharedModulePath(strNorm);

	Zenith_ScopedMutexLock xLock(s_xMutex);
	for (u_int u = 0; u < s_axRegistered.GetSize(); u++)
	{
		RegisteredProgram& xReg = s_axRegistered.Get(u);
		if (xReg.m_eProgram >= FluxShaderProgram::COUNT) continue;

		if (bShared)
		{
			xReg.m_bNeedsReload = true;
			continue;
		}

		const Flux_ShaderRegistryEntry& xEntry = Flux_ShaderRegistry::GetProgram(xReg.m_eProgram);
		if (ProgramMatchesPath(xEntry, strNorm))
		{
			xReg.m_bNeedsReload = true;
		}
	}
}

void Flux_ShaderHotReload::ProcessPendingReloads()
{
	std::unordered_set<std::string> xChanges;
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		if (s_xPendingChanges.empty()) return;
		xChanges = std::move(s_xPendingChanges);
		s_xPendingChanges.clear();
	}

	for (const std::string& strFile : xChanges)
	{
		MarkProgramsForReload(strFile.c_str());
	}

	// Snapshot unique callbacks outside the lock so they can do anything
	// they like (including registering more programs) without recursive-
	// mutex concerns. Subsystems register every program they own against
	// the same callback pointer — dedup keeps a `Common.Frame` change from
	// firing each subsystem's rebuild N times.
	Zenith_Vector<Flux_ShaderHotReload::ProgramRebuildCallback> axCallbacks;
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		for (u_int u = 0; u < s_axRegistered.GetSize(); u++)
		{
			RegisteredProgram& xReg = s_axRegistered.Get(u);
			if (!xReg.m_bNeedsReload) continue;
			xReg.m_bNeedsReload = false;
			if (!xReg.m_pfnRebuild) continue;

			// O(N^2) dedup is fine — N is registered programs, ~50 today
			// and callbacks are coarse-grained.
			bool bAlreadyQueued = false;
			for (u_int v = 0; v < axCallbacks.GetSize(); v++)
			{
				if (axCallbacks.Get(v) == xReg.m_pfnRebuild)
				{
					bAlreadyQueued = true;
					break;
				}
			}
			if (!bAlreadyQueued) axCallbacks.PushBack(xReg.m_pfnRebuild);
		}
	}

	if (axCallbacks.GetSize() == 0) return;

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: firing %u rebuild callback(s)…",
			   axCallbacks.GetSize());

	// Pipelines tear-down through QueueVRAMDeletion etc, but the actual
	// vk::Pipeline destruction must wait for the GPU to release in-flight
	// references. Subsystems' rebuild callbacks expect the GPU to be idle.
	g_xEngine.Vulkan().WaitForGPUIdle();

	for (u_int u = 0; u < axCallbacks.GetSize(); u++)
	{
		axCallbacks.Get(u)();
		s_uReloadCount++;
	}
}

void Flux_ShaderHotReload::RegisterProgram(FluxShaderProgram eProgram,
											ProgramRebuildCallback pfnRebuild)
{
	// Subsystems may register before Flux_ShaderHotReload::Initialise runs
	// (Flux::LateInitialise calls Initialise() AFTER subsystem inits in the
	// current ordering). The registration list is a static-storage container
	// that's safe to append to before Initialise; once the watcher starts,
	// any pre-registered callbacks fire normally on file changes.
	if (!pfnRebuild) return;
	if (eProgram >= FluxShaderProgram::COUNT) return;

	Zenith_ScopedMutexLock xLock(s_xMutex);
	for (u_int u = 0; u < s_axRegistered.GetSize(); u++)
	{
		RegisteredProgram& xReg = s_axRegistered.Get(u);
		if (xReg.m_eProgram == eProgram)
		{
			xReg.m_pfnRebuild = pfnRebuild;
			return;
		}
	}

	RegisteredProgram xNew;
	xNew.m_eProgram   = eProgram;
	xNew.m_pfnRebuild = pfnRebuild;
	s_axRegistered.PushBack(xNew);
}

void Flux_ShaderHotReload::RegisterSubsystem(ProgramRebuildCallback pfnRebuild,
											  const FluxShaderProgram* axPrograms,
											  u_int uCount)
{
	if (!pfnRebuild || !axPrograms) return;
	for (u_int u = 0; u < uCount; u++)
	{
		RegisterProgram(axPrograms[u], pfnRebuild);
	}
}

void Flux_ShaderHotReload::UnregisterProgram(FluxShaderProgram eProgram)
{
	// Mirrors RegisterProgram — operates on the static registration list,
	// which is safe before Initialise / after Shutdown.
	Zenith_ScopedMutexLock xLock(s_xMutex);
	for (u_int u = s_axRegistered.GetSize(); u-- > 0; )
	{
		if (s_axRegistered.Get(u).m_eProgram == eProgram)
		{
			s_axRegistered.Remove(u);
		}
	}
}

void Flux_ShaderHotReload::ReloadAll()
{
	if (!s_bInitialised) return;
	Zenith_ScopedMutexLock xLock(s_xMutex);
	for (u_int u = 0; u < s_axRegistered.GetSize(); u++)
	{
		s_axRegistered.Get(u).m_bNeedsReload = true;
	}
	// Force ProcessPendingReloads to run by stamping a sentinel — without it
	// the early-out (`if pending empty return`) skips the rebuild scan.
	s_xPendingChanges.insert("__force_reload__");
}

#endif // ZENITH_TOOLS
