#pragma once

#ifdef ZENITH_TOOLS

#include <string>
#include <functional>

class Zenith_Vulkan_Pipeline;
class Zenith_Vulkan_Shader;
struct Flux_PipelineSpecification;

// Callback signature for pipeline recreation
// Called when a shader's source files have changed and the pipeline needs recreation
// Parameters: pipeline pointer, vertex path, fragment path (or compute path)
// Returns true if recreation succeeded
using PipelineRecreateCallback = std::function<bool(Zenith_Vulkan_Pipeline*, const std::string&, const std::string&)>;

// Hot reload manager for shaders (ZENITH_TOOLS only)
// Watches shader source files and triggers recompilation when files change
//
// Usage:
//   // At init time:
//   Flux_ShaderHotReload::Initialise();
//
//   // Register pipelines for hot reload:
//   Flux_ShaderHotReload::RegisterPipeline(&myPipeline, "path/to/vert.vert", "path/to/frag.frag",
//       [](Zenith_Vulkan_Pipeline* p, const std::string& v, const std::string& f) {
//           // Recreate pipeline with new shaders
//           return true;
//       });
//
//   // In main loop (once per frame):
//   Flux_ShaderHotReload::Update();
//
//   // On shutdown:
//   Flux_ShaderHotReload::Shutdown();
//
class Flux_ShaderHotReload
{
public:
	// Initialize the hot reload system (starts file watcher)
	static void Initialise();

	// Shutdown the hot reload system
	static void Shutdown();

	// Check if hot reload is enabled
	static bool IsEnabled() { return s_bEnabled; }

	// Enable/disable hot reload at runtime
	static void SetEnabled(bool bEnabled);

	// Check for pending reloads and apply them
	// Should be called once per frame, preferably at a safe point (e.g., after GPU idle)
	static void Update();

	// Register a graphics pipeline for hot reload
	// strVertPath, strFragPath: Shader source paths (relative to SHADER_SOURCE_ROOT)
	// pfnRecreate: Callback to recreate the pipeline when shaders change
	static void RegisterPipeline(Zenith_Vulkan_Pipeline* pxPipeline,
								  const std::string& strVertPath,
								  const std::string& strFragPath,
								  PipelineRecreateCallback pfnRecreate);

	// Register a compute pipeline for hot reload
	static void RegisterComputePipeline(Zenith_Vulkan_Pipeline* pxPipeline,
										 const std::string& strComputePath,
										 PipelineRecreateCallback pfnRecreate);

	// Unregister a pipeline (call before destroying the pipeline)
	static void UnregisterPipeline(Zenith_Vulkan_Pipeline* pxPipeline);

	// Force reload all registered pipelines
	static void ReloadAll();

	// Get statistics
	static u_int GetReloadCount() { return s_uReloadCount; }
	static u_int GetFailedReloadCount() { return s_uFailedReloadCount; }

private:
	// Handle file change notification from file watcher
	static void OnFileChanged(const std::string& strPath, int eChangeType);

	// Process pending reloads
	static void ProcessPendingReloads();

	// Find pipelines affected by a changed file (including headers)
	static void MarkPipelinesForReload(const std::string& strChangedFile);

	static bool s_bInitialised;
	static bool s_bEnabled;
	static u_int s_uReloadCount;
	static u_int s_uFailedReloadCount;
};

#endif // ZENITH_TOOLS
