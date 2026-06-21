#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Slang/Flux_ShaderDecl.h"

// Hot reload manager for Slang shaders (ZENITH_TOOLS only).
//
// Watches the shader source root for .slang changes and asks each registered
// feature to rebuild its pipelines. A registered program is keyed by its decl
// IDENTITY (const Flux_ShaderDecl*); the .slang path is matched against the
// decl's module. A change to a shared module under Common/ (Common.Frame,
// Common.PBR, …) marks every registered program as stale because the import
// graph isn't tracked yet — rebuilding is cheap relative to a full FluxCompiler
// run, so the over-reload is acceptable for now.
//
// Features register every decl they own against a single no-arg rebuild
// callback. Multiple programs sharing the same callback are de-duplicated each
// Update pass, so a Common/ touch fires each feature's rebuild exactly once.
//
// AUTOMATIC REGISTRATION (the normal path). Engine subsystems do NOT register
// themselves. Flux::LateInitialise calls AutoRegisterFeatures() after the
// feature registry is populated; that walks the registered features and wires
// every decl in each feature's m_paxShaders (its apxALL) to that feature's
// BuildPipelines callback (Flux_FeatureDesc::m_pfnBuildPipelines). Ownership is
// structural — a new feature gets hot-reload "for free" by listing its decls in
// apxALL and passing them to RegisterFeature. No subsystem-name convention, no
// override table.
//
// Usage:
//   // At init time (already wired in Flux::LateInitialise):
//   Flux_ShaderHotReload::Initialise();
//   // ... after Flux_FeatureRegistry::RegisterDefaultFeatures():
//   Flux_ShaderHotReload::AutoRegisterFeatures();
//
//   // Once per frame at a safe sync point (already wired in Zenith_Vulkan):
//   Flux_ShaderHotReload::Update();
//
// The RegisterProgram / RegisterSubsystem entry points remain public for
// out-of-tree owners that are not engine features (e.g. a game's own pass, or
// the swapchain blit) — call them AFTER AutoRegisterFeatures to override.
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

	// Walk the registered features and register every decl in each feature's
	// m_paxShaders against that feature's BuildPipelines callback. This replaces
	// the per-subsystem RegisterSubsystem boilerplate — subsystems no longer
	// register themselves. Must be called AFTER
	// Flux_FeatureRegistry::RegisterDefaultFeatures (so the rebuild callbacks
	// exist) and after Initialise (so the watcher is live); both hold in
	// Flux::LateInitialise. Idempotent — safe to call again after a re-init.
	static void AutoRegisterFeatures();

	// Drains pending file-change notifications and fires callbacks for any
	// programs whose source changed. Must be called from the main thread at
	// a safe sync point — invokes Flux_PlatformAPI::WaitForGPUIdle before
	// firing callbacks so subsystem rebuilds can free pipeline objects.
	static void Update();

	// Register a single program (by decl identity) against a rebuild callback.
	// Multiple programs may share the same callback; the dispatcher de-duplicates
	// so the callback fires at most once per Update pass even when multiple of its
	// programs are marked stale. The decl must be static-lifetime (it is — the
	// per-feature/per-game decls are inline constexpr).
	static void RegisterProgram(const Flux_ShaderDecl& xDecl,
								 ProgramRebuildCallback pfnRebuild);

	// Convenience: register a batch of decls against the same callback in one
	// call. Typical use: a feature/game registers every decl it owns against its
	// BuildPipelines() helper.
	static void RegisterSubsystem(ProgramRebuildCallback pfnRebuild,
								   const Flux_ShaderDecl* const* apxDecls,
								   u_int uCount);

	// Unregister a program (call before tearing down the pipeline that owns
	// the user data — otherwise the next Update could fire into freed state).
	static void UnregisterProgram(const Flux_ShaderDecl& xDecl);

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
