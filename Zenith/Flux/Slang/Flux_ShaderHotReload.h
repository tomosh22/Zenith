#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Shaders/Generated/FluxShaderProgram.h"

// Hot reload manager for Slang shaders (ZENITH_TOOLS only).
//
// Watches the shader source root for .slang changes and asks each registered
// subsystem to rebuild its pipelines. Module → program mapping is resolved
// via Flux_ShaderRegistry; a change to a shared module under Common/
// (Common.Frame, Common.PBR, …) marks every registered program as stale
// because the import graph isn't tracked yet — rebuilding is cheap relative
// to a full FluxCompiler run, so the over-reload is acceptable for now.
//
// Subsystems register all their FluxShaderProgram IDs against a single
// no-arg rebuild callback. Multiple programs sharing the same callback are
// de-duplicated each Update pass, so a Common/ touch fires each subsystem's
// callback exactly once even though it marks every program.
//
// Usage:
//   // At init time (already wired in Flux::LateInitialise):
//   Flux_ShaderHotReload::Initialise();
//
//   // Inside a subsystem's Initialise(), after the first build:
//   static const FluxShaderProgram s_axMyPrograms[] = {
//       FluxShaderProgram::Foo,
//       FluxShaderProgram::Bar,
//   };
//   Flux_ShaderHotReload::RegisterSubsystem(&Flux_Foo::BuildPipelines,
//       s_axMyPrograms, sizeof(s_axMyPrograms) / sizeof(s_axMyPrograms[0]));
//
//   // Once per frame at a safe sync point (already wired in Zenith_Vulkan):
//   Flux_ShaderHotReload::Update();
class Flux_ShaderHotReload
{
public:
	// Function-pointer callback. Engine convention forbids std::function.
	// Subsystems hold their pipeline state in file-static globals so a
	// no-arg callback is sufficient.
	using ProgramRebuildCallback = void (*)();

	static void Initialise();
	static void Shutdown();
	static bool IsEnabled() { return s_bEnabled; }
	static void SetEnabled(bool bEnabled);

	// Drains pending file-change notifications and fires callbacks for any
	// programs whose source changed. Must be called from the main thread at
	// a safe sync point — invokes Flux_PlatformAPI::WaitForGPUIdle before
	// firing callbacks so subsystem rebuilds can free pipeline objects.
	static void Update();

	// Register a single program against a rebuild callback. Multiple
	// programs may share the same callback; the dispatcher de-duplicates so
	// the callback fires at most once per Update pass even when multiple of
	// its programs are marked stale.
	static void RegisterProgram(FluxShaderProgram eProgram,
								 ProgramRebuildCallback pfnRebuild);

	// Convenience: register a batch of programs against the same callback
	// in one call. Typical use: a subsystem registers every FluxShaderProgram
	// it owns against its BuildPipelines() helper.
	static void RegisterSubsystem(ProgramRebuildCallback pfnRebuild,
								   const FluxShaderProgram* axPrograms,
								   u_int uCount);

	// Unregister a program (call before tearing down the pipeline that owns
	// the user data — otherwise the next Update could fire into freed state).
	static void UnregisterProgram(FluxShaderProgram eProgram);

	// Force rebuild of every registered program. Useful when the user pokes
	// a debug button or a global include changes outside the watch root.
	static void ReloadAll();

	static u_int GetReloadCount() { return s_uReloadCount; }
	static u_int GetFailedReloadCount() { return s_uFailedReloadCount; }

private:
	static void OnFileChanged(const char* szPath, int eChangeType);
	static void ProcessPendingReloads();
	static void MarkProgramsForReload(const char* szChangedFile);

	static bool s_bInitialised;
	static bool s_bEnabled;
	static u_int s_uReloadCount;
	static u_int s_uFailedReloadCount;
};

#endif // ZENITH_TOOLS
