#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Flux/Slang/Flux_ShaderHotReload.h"
#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Core/Zenith_FileWatcher.h"
#include "Core/Multithreading/Zenith_Multithreading.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "Vulkan/Zenith_Vulkan.h"
#include <unordered_map> // #TODO: Replace with engine hash map
#include <unordered_set> // #TODO: Replace with engine hash set when available
#include <vector> // #TODO: Replace with Zenith_Vector (requires removing range-for and std::remove_if usage)
#include <algorithm>

// Registered pipeline info
struct RegisteredPipeline
{
	Zenith_Vulkan_Pipeline* m_pxPipeline = nullptr;
	std::string m_strVertPath;
	std::string m_strFragPath;     // Empty for compute
	std::string m_strComputePath;  // Empty for graphics
	PipelineRecreateCallback m_pfnRecreate;
	bool m_bNeedsReload = false;
	bool m_bIsCompute = false;
};

// Static data
static Zenith_FileWatcher s_xFileWatcher;
static std::vector<RegisteredPipeline> s_axRegisteredPipelines;
static std::unordered_set<std::string> s_xPendingReloadFiles;
static Zenith_Mutex s_xMutex;
static double s_fLastReloadTime = 0.0;
static constexpr double fRELOAD_DEBOUNCE_TIME = 0.5; // Seconds to wait before reloading (debounce rapid saves)

bool Flux_ShaderHotReload::s_bInitialised = false;
bool Flux_ShaderHotReload::s_bEnabled = true;
u_int Flux_ShaderHotReload::s_uReloadCount = 0;
u_int Flux_ShaderHotReload::s_uFailedReloadCount = 0;

// Normalize path for comparison
static std::string NormalizePath(const std::string& strPath)
{
	std::string strNormalized = strPath;
	for (char& c : strNormalized)
	{
		if (c == '\\')
		{
			c = '/';
		}
	}
	// Convert to lowercase for case-insensitive comparison on Windows
	std::transform(strNormalized.begin(), strNormalized.end(), strNormalized.begin(), ::tolower);
	return strNormalized;
}

// Check if a shader path matches a changed file (handles relative vs absolute paths)
static bool PathMatches(const std::string& strShaderPath, const std::string& strChangedFile)
{
	std::string strNormalizedShader = NormalizePath(strShaderPath);
	std::string strNormalizedChanged = NormalizePath(strChangedFile);

	// Direct match
	if (strNormalizedShader == strNormalizedChanged)
	{
		return true;
	}

	// Check if changed file ends with shader path (relative match)
	if (strNormalizedChanged.length() >= strNormalizedShader.length())
	{
		size_t ulOffset = strNormalizedChanged.length() - strNormalizedShader.length();
		if (strNormalizedChanged.substr(ulOffset) == strNormalizedShader)
		{
			return true;
		}
	}

	// Check if shader path ends with changed file
	if (strNormalizedShader.length() >= strNormalizedChanged.length())
	{
		size_t ulOffset = strNormalizedShader.length() - strNormalizedChanged.length();
		if (strNormalizedShader.substr(ulOffset) == strNormalizedChanged)
		{
			return true;
		}
	}

	return false;
}

void Flux_ShaderHotReload::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	// Start watching the shader source directory
	std::string strShaderRoot(SHADER_SOURCE_ROOT);

	bool bStarted = s_xFileWatcher.Start(strShaderRoot, true,
		[](const std::string& strPath, FileChangeType eType)
		{
			// Forward to our handler, converting enum to int
			OnFileChanged(strPath, static_cast<int>(eType));
		});

	if (!bStarted)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Failed to start file watcher for %s", strShaderRoot.c_str());
		return;
	}

	s_bInitialised = true;
	s_bEnabled = true;
	s_uReloadCount = 0;
	s_uFailedReloadCount = 0;

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload initialized - watching: %s", strShaderRoot.c_str());
}

void Flux_ShaderHotReload::Shutdown()
{
	if (!s_bInitialised)
	{
		return;
	}

	s_xFileWatcher.Stop();

	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		s_axRegisteredPipelines.clear();
		s_xPendingReloadFiles.clear();
	}

	s_bInitialised = false;

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload shutdown - Reloads: %u, Failed: %u",
			   s_uReloadCount, s_uFailedReloadCount);
}

void Flux_ShaderHotReload::SetEnabled(bool bEnabled)
{
	s_bEnabled = bEnabled;
	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload %s", bEnabled ? "enabled" : "disabled");
}

void Flux_ShaderHotReload::Update()
{
	if (!s_bInitialised || !s_bEnabled)
	{
		return;
	}

	// Update file watcher to process OS notifications
	s_xFileWatcher.Update();

	// Process pending reloads with debouncing
	ProcessPendingReloads();
}

void Flux_ShaderHotReload::OnFileChanged(const std::string& strPath, int eChangeType)
{
	// Only care about modifications
	FileChangeType eType = static_cast<FileChangeType>(eChangeType);
	if (eType != FileChangeType::Modified)
	{
		return;
	}

	// Check if this is a shader file (by extension)
	std::string strExt;
	size_t ulDot = strPath.rfind('.');
	if (ulDot != std::string::npos)
	{
		strExt = strPath.substr(ulDot);
	}

	// Shader extensions we care about
	static const char* aszShaderExtensions[] = {
		".vert", ".frag", ".comp", ".tesc", ".tese", ".geom",
		".fxh", ".slang", ".hlsl", ".glsl"
	};

	bool bIsShader = false;
	for (const char* szExt : aszShaderExtensions)
	{
		if (strExt == szExt)
		{
			bIsShader = true;
			break;
		}
	}

	if (!bIsShader)
	{
		return;
	}

	// Add to pending reload set
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		s_xPendingReloadFiles.insert(strPath);
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: File changed: %s", strPath.c_str());
}

void Flux_ShaderHotReload::MarkPipelinesForReload(const std::string& strChangedFile)
{
	Zenith_ScopedMutexLock xLock(s_xMutex);

	for (RegisteredPipeline& xPipeline : s_axRegisteredPipelines)
	{
		bool bAffected = false;

		if (xPipeline.m_bIsCompute)
		{
			bAffected = PathMatches(xPipeline.m_strComputePath, strChangedFile);
		}
		else
		{
			bAffected = PathMatches(xPipeline.m_strVertPath, strChangedFile) ||
						PathMatches(xPipeline.m_strFragPath, strChangedFile);
		}

		// Also check if it's a header file that might be included
		// Headers can affect any shader, so mark all for reload if a .fxh changes
		std::string strExt;
		size_t ulDot = strChangedFile.rfind('.');
		if (ulDot != std::string::npos)
		{
			strExt = strChangedFile.substr(ulDot);
		}

		if (strExt == ".fxh" || strExt == ".hlsl" || strExt == ".slang")
		{
			// For header files, we need to check dependencies
			// For now, mark all pipelines as potentially affected
			// The cache system will handle avoiding unnecessary recompilations
			bAffected = true;
		}

		if (bAffected)
		{
			xPipeline.m_bNeedsReload = true;
		}
	}
}

void Flux_ShaderHotReload::ProcessPendingReloads()
{
	// Check if we have pending files
	std::unordered_set<std::string> xFilesToProcess;
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		if (s_xPendingReloadFiles.empty())
		{
			return;
		}
		xFilesToProcess = std::move(s_xPendingReloadFiles);
		s_xPendingReloadFiles.clear();
	}

	// Mark affected pipelines
	for (const std::string& strFile : xFilesToProcess)
	{
		MarkPipelinesForReload(strFile);
	}

	// Collect pipelines that need reload
	std::vector<RegisteredPipeline*> axPipelinesToReload;
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		for (RegisteredPipeline& xPipeline : s_axRegisteredPipelines)
		{
			if (xPipeline.m_bNeedsReload)
			{
				axPipelinesToReload.push_back(&xPipeline);
			}
		}
	}

	if (axPipelinesToReload.empty())
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Reloading %u pipeline(s)...",
			   static_cast<u_int>(axPipelinesToReload.size()));

	// Wait for GPU to be idle before recreating pipelines
	Zenith_Vulkan::GetDevice().waitIdle();

	// Reload each pipeline
	for (RegisteredPipeline* pxPipeline : axPipelinesToReload)
	{
		bool bSuccess = false;

		if (pxPipeline->m_pfnRecreate)
		{
			if (pxPipeline->m_bIsCompute)
			{
				bSuccess = pxPipeline->m_pfnRecreate(pxPipeline->m_pxPipeline,
					pxPipeline->m_strComputePath, "");
			}
			else
			{
				bSuccess = pxPipeline->m_pfnRecreate(pxPipeline->m_pxPipeline,
					pxPipeline->m_strVertPath, pxPipeline->m_strFragPath);
			}
		}

		if (bSuccess)
		{
			s_uReloadCount++;
			Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Reloaded pipeline successfully");
		}
		else
		{
			s_uFailedReloadCount++;
			Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Failed to reload pipeline");
		}

		pxPipeline->m_bNeedsReload = false;
	}
}

void Flux_ShaderHotReload::RegisterPipeline(Zenith_Vulkan_Pipeline* pxPipeline,
											 const std::string& strVertPath,
											 const std::string& strFragPath,
											 PipelineRecreateCallback pfnRecreate)
{
	if (!s_bInitialised)
	{
		return;
	}

	Zenith_ScopedMutexLock xLock(s_xMutex);

	// Check if already registered
	for (const RegisteredPipeline& xExisting : s_axRegisteredPipelines)
	{
		if (xExisting.m_pxPipeline == pxPipeline)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Pipeline already registered");
			return;
		}
	}

	RegisteredPipeline xPipeline;
	xPipeline.m_pxPipeline = pxPipeline;
	xPipeline.m_strVertPath = strVertPath;
	xPipeline.m_strFragPath = strFragPath;
	xPipeline.m_pfnRecreate = pfnRecreate;
	xPipeline.m_bIsCompute = false;
	xPipeline.m_bNeedsReload = false;

	s_axRegisteredPipelines.push_back(xPipeline);

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Registered pipeline (%s + %s)",
			   strVertPath.c_str(), strFragPath.c_str());
}

void Flux_ShaderHotReload::RegisterComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline,
													const std::string& strComputePath,
													PipelineRecreateCallback pfnRecreate)
{
	if (!s_bInitialised)
	{
		return;
	}

	Zenith_ScopedMutexLock xLock(s_xMutex);

	// Check if already registered
	for (const RegisteredPipeline& xExisting : s_axRegisteredPipelines)
	{
		if (xExisting.m_pxPipeline == pxPipeline)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Pipeline already registered");
			return;
		}
	}

	RegisteredPipeline xPipeline;
	xPipeline.m_pxPipeline = pxPipeline;
	xPipeline.m_strComputePath = strComputePath;
	xPipeline.m_pfnRecreate = pfnRecreate;
	xPipeline.m_bIsCompute = true;
	xPipeline.m_bNeedsReload = false;

	s_axRegisteredPipelines.push_back(xPipeline);

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Registered compute pipeline (%s)",
			   strComputePath.c_str());
}

void Flux_ShaderHotReload::UnregisterPipeline(Zenith_Vulkan_Pipeline* pxPipeline)
{
	if (!s_bInitialised)
	{
		return;
	}

	Zenith_ScopedMutexLock xLock(s_xMutex);

	auto it = std::remove_if(s_axRegisteredPipelines.begin(), s_axRegisteredPipelines.end(),
		[pxPipeline](const RegisteredPipeline& xPipeline) {
			return xPipeline.m_pxPipeline == pxPipeline;
		});

	if (it != s_axRegisteredPipelines.end())
	{
		s_axRegisteredPipelines.erase(it, s_axRegisteredPipelines.end());
		Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Unregistered pipeline");
	}
}

void Flux_ShaderHotReload::ReloadAll()
{
	if (!s_bInitialised)
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "ShaderHotReload: Force reloading all pipelines...");

	// Mark all pipelines for reload
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		for (RegisteredPipeline& xPipeline : s_axRegisteredPipelines)
		{
			xPipeline.m_bNeedsReload = true;
		}
	}

	// Add a dummy file to trigger processing
	{
		Zenith_ScopedMutexLock xLock(s_xMutex);
		s_xPendingReloadFiles.insert("__force_reload__");
	}
}

#endif // ZENITH_TOOLS
