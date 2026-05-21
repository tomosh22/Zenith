#include "Zenith.h"

#include "Core/Zenith_Engine.h"

#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ScriptAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Core/Zenith_CommandLine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Core/FrameContext.h"
#include "Core/Multithreading/Zenith_MultithreadingImpl.h"
#include "Profiling/Zenith_ProfilingImpl.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_EntityStore.h"
#include "EntityComponent/Internal/Zenith_SceneRegistryImpl.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "Flux/Flux_Graphics.h"
#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorAutomation.h"
#endif
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsImpl.h"
#include "UnitTests/Zenith_UnitTests.h"

#ifdef ZENITH_WINDOWS
#include <cstring>
#endif

#ifdef ZENITH_TOOLS
extern void ExportAllMeshes();
extern void ExportAllTextures();
extern void ExportHeightmap();
extern void ExportDefaultFontAtlas();
extern void GenerateTestAssets();
#endif

extern void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions);
extern const char* Project_GetGameAssetsDir();
extern void Project_RegisterScriptBehaviours();
extern void Project_Shutdown();

#ifdef ZENITH_TOOLS
extern void Project_InitializeResources();
extern void Project_RegisterEditorAutomationSteps();
#endif
extern void Project_LoadInitialScene();

// The single engine instance. constinit guarantees zero static-init
// cost — every member of Zenith_Engine has a constant default
// initialiser (currently none, but the rule must hold as members are
// added in later phases).
constinit Zenith_Engine g_xEngine;

namespace
{
	bool HasCommandLineFlag(const char* szFlag)
	{
#ifdef ZENITH_WINDOWS
		for (int i = 1; i < __argc; i++)
		{
			if (std::strcmp(__argv[i], szFlag) == 0)
				return true;
		}
#else
		(void)szFlag;
#endif
		return false;
	}
}

FrameContext& Zenith_Engine::Frame()
{
	Zenith_Assert(m_pxFrame != nullptr,
		"Zenith_Engine::Frame() called before Initialise() or after Shutdown(). "
		"Frame timing is unavailable outside the engine lifetime.");
	return *m_pxFrame;
}

Zenith_MultithreadingImpl& Zenith_Engine::Threading()
{
	// NOTE: no assert here. Threading() is read on the IsMainThread
	// hot path (every Zenith_Assert that gates on main-thread fires
	// it), and the engine guarantees m_pxThreading exists before any
	// thread can call RegisterThread / IsMainThread (see Initialise).
	return *m_pxThreading;
}

Zenith_TaskSystemImpl& Zenith_Engine::Tasks()
{
	// No assert: SubmitTask is a hot path, and Zenith_Engine::Initialise
	// allocates m_pxTasks before Zenith_TaskSystem::Inititalise() (the
	// forwarder that brings worker threads online).
	return *m_pxTasks;
}

Zenith_ProfilingImpl& Zenith_Engine::Profiling()
{
	// No assert: BeginProfile / EndProfile sit on the hottest path in
	// the engine (every ZENITH_PROFILING_FUNCTION_WRAPPER call fires
	// it). Zenith_Engine::Initialise allocates m_pxProfiling before
	// Zenith_Profiling::Initialise() so the impl is always available
	// once any thread starts profiling.
	return *m_pxProfiling;
}

Zenith_AssetRegistry& Zenith_Engine::Assets()
{
	Zenith_Assert(m_pxAssets != nullptr,
		"Zenith_Engine::Assets() called before Initialise() or after Shutdown(). "
		"AssetRegistry is unavailable outside the engine lifetime.");
	return *m_pxAssets;
}

Zenith_PhysicsImpl& Zenith_Engine::Physics()
{
	Zenith_Assert(m_pxPhysics != nullptr,
		"Zenith_Engine::Physics() called before Initialise() or after Shutdown(). "
		"Physics is unavailable outside the engine lifetime.");
	return *m_pxPhysics;
}

Zenith_EntityStore& Zenith_Engine::EntityStore()
{
	// No assert: EntityStore is read on the hot path for every entity
	// access (lifecycle queries, component lookups). Zenith_Engine::Initialise
	// allocates m_pxEntityStore very early -- before SceneManager / Physics
	// init -- so the store is always available once any subsystem touches
	// entity slots.
	return *m_pxEntityStore;
}

Zenith_SceneRegistryImpl& Zenith_Engine::SceneRegistry()
{
	// No assert: SceneRegistry is read every scene-handle resolution and
	// from inside the per-frame Update pipeline. Allocated alongside the
	// EntityStore VERY EARLY in Initialise so it's live before
	// Zenith_SceneManager::Initialise creates the persistent scene.
	return *m_pxSceneRegistry;
}

void Zenith_Engine::Initialise()
{
	// Phase 2: per-frame timing state lives here now. Construct
	// FIRST so any subsystem that reads dt during init sees a sane
	// zero. Last-frame time is bumped at the end of Initialise()
	// below, matching the historical Zenith_Init behaviour.
	Zenith_Assert(m_pxFrame == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxFrame = new FrameContext();

	// Phase 5a: global entity storage. Allocate VERY EARLY -- any
	// subsystem that touches entity slots reads g_xEngine.EntityStore()
	// and would dereference nullptr if we wait. Empty store is fine
	// until SceneManager / scene-load starts creating entities.
	Zenith_Assert(m_pxEntityStore == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxEntityStore = new Zenith_EntityStore();

	// Phase 5b: scene-registry state (slot table, generations, active /
	// persistent handles, build-index map, name cache). Allocated alongside
	// EntityStore -- both must exist before Zenith_SceneManager::Initialise
	// creates the persistent scene.
	Zenith_Assert(m_pxSceneRegistry == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxSceneRegistry = new Zenith_SceneRegistryImpl();

	// Phase 3a: multithreading registry (thread-ID allocator +
	// main-thread ID) lives on the engine now. Allocate BEFORE
	// Zenith_Multithreading::RegisterThread(true) below, which reads
	// from g_xEngine.Threading() to issue the main thread's ID.
	Zenith_Assert(m_pxThreading == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxThreading = new Zenith_MultithreadingImpl();

	// Populate graphics options from the game project FIRST
	// Must happen before any Flux initialisation reads from Zenith_GraphicsOptions::Get()
	Project_SetGraphicsOptions(Zenith_GraphicsOptions::Get());

	// CRITICAL: Memory tracking must be initialized FIRST to capture all allocations
	Zenith_MemoryManagement::Initialise();

	// Phase 3b: per-Engine Profiling state. Allocate BEFORE
	// Zenith_Multithreading::RegisterThread(true) below --
	// RegisterThread transitively calls Zenith_Profiling::RegisterThread
	// (which reads g_xEngine.Profiling().m_xEvents). The Profiling
	// impl also has to exist before Zenith_Profiling::Initialise()
	// further down for the same reason.
	Zenith_Assert(m_pxProfiling == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxProfiling = new Zenith_ProfilingImpl();

	Zenith_Multithreading::RegisterThread(true);
	Zenith_Profiling::Initialise();

	// Phase 3b: per-Engine TaskSystem state. Allocate BEFORE
	// Zenith_TaskSystem::Inititalise() below, which spawns worker
	// threads whose ThreadFunc reads from g_xEngine.Tasks().
	Zenith_Assert(m_pxTasks == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxTasks = new Zenith_TaskSystemImpl();

	Zenith_TaskSystem::Inititalise();

	// Set asset directories before registry initialization
	// Game assets dir comes from the game project (each game defines GAME_ASSETS_DIR)
	Zenith_AssetRegistry::SetGameAssetsDir(Project_GetGameAssetsDir());
#ifdef ENGINE_ASSETS_DIR
	Zenith_AssetRegistry::SetEngineAssetsDir(ENGINE_ASSETS_DIR);
#else
	Zenith_AssetRegistry::SetEngineAssetsDir("./Zenith/Assets/");
#endif

	// Phase 4: Engine owns the AssetRegistry instance. Allocate and
	// install the view-pointer BEFORE Zenith_AssetRegistry::Initialize()
	// (which now only registers loaders and asserts s_pxInstance is set).
	// The ~50 existing call sites of the static facade (Get<T>, Create<T>,
	// Save, etc.) keep working through s_pxInstance until Phase 9 sweeps
	// them away.
	Zenith_Assert(m_pxAssets == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxAssets = new Zenith_AssetRegistry();
	Zenith_AssetRegistry::s_pxInstance = m_pxAssets;
	Zenith_AssetRegistry::Initialize();

#ifdef ZENITH_TOOLS
	if (HasCommandLineFlag("--skip-tool-exports"))
	{
		Zenith_Log(LOG_CATEGORY_CORE, "Tool asset exports skipped by --skip-tool-exports");
	}
	else
	{
		ExportAllMeshes();
		ExportAllTextures();
		//ExportHeightmap();
		ExportDefaultFontAtlas();  // Generate font atlas from TTF
		GenerateTestAssets();      // Generate procedural test assets (StickFigure, Tree)
	}
#endif

	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::EarlyInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::EarlyInitialise();
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Physics::Initialise...");
	// Phase 4: per-Engine Physics state lives on Zenith_PhysicsImpl.
	// Allocate BEFORE Zenith_Physics::Initialise() below -- the static
	// facade now reads/writes g_xEngine.Physics().m_pxXxx for every
	// piece of state it used to keep as Zenith_Physics::s_*.
	Zenith_Assert(m_pxPhysics == nullptr, "Zenith_Engine::Initialise called twice without Shutdown");
	m_pxPhysics = new Zenith_PhysicsImpl();
	Zenith_Physics::Initialise();
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: SceneManager::Initialise...");
	Zenith_SceneManager::Initialise();

	//#TO_TODO: move somewhere sensible
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
		Zenith_AssetRegistry::InitializeGPUDependentAssets();  // Must be after Flux::EarlyInitialise()

		// Load cubemap texture (pinned)
		if (Zenith_TextureAsset* pxCubemap = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
		{
			pxCubemap->LoadCubemapFromFiles(
				ENGINE_ASSETS_DIR"Textures/Cubemap/px" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nx" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/py" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/ny" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/pz" ZENITH_TEXTURE_EXT,
				ENGINE_ASSETS_DIR"Textures/Cubemap/nz" ZENITH_TEXTURE_EXT
			);
			Flux_Graphics::s_xCubemapTexture.Set(pxCubemap);
		}

		// Load water normal texture (pinned)
		if (Zenith_TextureAsset* pxWaterNormal = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR"Textures/Water/normal" ZENITH_TEXTURE_EXT))
		{
			Flux_Graphics::s_xWaterNormalTexture.Set(pxWaterNormal);
		}

		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Log(LOG_CATEGORY_CORE, "Zenith_Init: Flux::LateInitialise...");
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::LateInitialise();
	}

#if defined ZENITH_TOOLS && defined ZENITH_DEBUG_VARIABLES
	Zenith_GraphicsOptions::RegisterDebugVariables();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Zenith_Editor::Initialise();
		Zenith_DebugVariables::AddButton({ "Export", "Meshes", "Export All Meshes" }, ExportAllMeshes);
		Zenith_DebugVariables::AddButton({ "Export", "Textures", "Export All Textures" }, ExportAllTextures);
		Zenith_DebugVariables::AddButton({ "Export", "Terrain", "Export Heightmap" }, ExportHeightmap);
		Zenith_DebugVariables::AddButton({ "Export", "Font", "Export Font Atlas" }, ExportDefaultFontAtlas);
	}
#endif

	Project_RegisterScriptBehaviours();

#ifdef ZENITH_TOOLS
	// Sync registered script behaviours to disk: write missing game:Scripts/<TypeName>.zscript
	// files for each behaviour that auto-registered via the ZENITH_BEHAVIOUR_TYPE_NAME macro,
	// and rename orphan .zscript files (whose behaviour is no longer registered) to .stale.
	// Must run AFTER static-init / Project_RegisterScriptBehaviours and BEFORE the test runner
	// (so tests can resolve .zscript assets via Zenith_AssetRegistry::Get) and before any scene load.
	Zenith_ScriptAsset::SyncRegisteredTypesToDisk();
#endif

	// Run unit tests BEFORE loading the game scene
	// This ensures tests don't corrupt game entities - scene loads fresh after tests
#ifdef ZENITH_TESTING
	if (HasCommandLineFlag("--skip-unit-tests"))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "Unit tests skipped by --skip-unit-tests");
	}
	else
	{
		Zenith_TestRunner::Instance().RunAllTests();
	}
#endif

#ifdef ZENITH_TOOLS
	// Initialize game-specific resources (geometry, materials, prefabs, particle configs)
	// Must be inside BeginFrame/EndFrame for GPU resource allocation
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Project_InitializeResources();
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}

	// Register automation steps and begin execution (one step per frame in main loop)
	Project_RegisterEditorAutomationSteps();
	Zenith_EditorAutomation::Begin();
#else
	// Non-tools: load pre-generated scene files
	// Run a tools build first to generate .zscen files
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::BeginFrame();
	}
	Zenith_SceneManager::SetInitialSceneLoadCallback(&Project_LoadInitialScene);
	{
		Zenith_SceneManager::LifecycleDeferralGuard xLoadingGuard(Zenith_SceneLifecycleScheduler::s_bIsLoadingScene);
		Project_LoadInitialScene();
	}
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_MemoryManager::EndFrame(false);
	}
	Zenith_Assert(Zenith_SceneManager::GetActiveScene().IsValid(),
		"No scene loaded. Run a ZENITH_TOOLS build first to generate .zscen files.");
#endif

	m_pxFrame->SetLastFrameTime(std::chrono::high_resolution_clock::now());
}

void Zenith_Engine::Shutdown()
{
	//--------------------------------------------------------------------------
	// Shutdown sequence - reverse order of initialization
	// Critical: Must wait for GPU before destroying resources it's using
	//--------------------------------------------------------------------------
	Zenith_Log(LOG_CATEGORY_CORE, "Beginning shutdown sequence...");

	// 1. Wait for GPU to finish all pending work
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux_PlatformAPI::WaitForGPUIdle();
	}

#ifdef ZENITH_TOOLS
	// 2. Shutdown editor (processes pending deletions, cleans up editor state)
	Zenith_Editor::Shutdown();
#endif

	// 3. Shutdown SceneManager (unloads all scenes, releases resources)
	// Must happen before physics (colliders need to remove bodies) and before
	// memory manager (model/mesh components hold VRAM handles)
	Zenith_SceneManager::Shutdown();

	// 4. Shutdown physics system. The static facade drains state out of
	// g_xEngine.Physics().m_pxXxx; engine then reclaims the Impl below
	// (Phase 4 makes Zenith_Engine the sole owner).
	Zenith_Physics::Shutdown();
	delete m_pxPhysics;
	m_pxPhysics = nullptr;

	// 5. Project shutdown - clean up game-specific resources
	Project_Shutdown();

	// 6. Release Flux's asset-system references BEFORE the registry shuts down.
	// Flux statics hold TextureHandle / MaterialHandle defaults that must drop their
	// refs while the registry still owns its assets — Flux::Shutdown() runs too late.
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::ReleaseAssetReferences();
	}

	// 7. Shutdown asset registry (unloads all assets). Engine then
	// reclaims the instance — Phase 4 makes Zenith_Engine the sole
	// owner; the static facade now drains state only.
	Zenith_AssetRegistry::Shutdown();
	delete m_pxAssets;
	m_pxAssets = nullptr;
	Zenith_AssetRegistry::s_pxInstance = nullptr;

	// 8. Shutdown Flux (all subsystems + graphics + memory manager)
	if (!Zenith_CommandLine::IsHeadless())
	{
		Flux::Shutdown();
	}

	// 9. Shutdown task system (terminates worker threads)
	Zenith_TaskSystem::Shutdown();

	// 10. Free the per-Engine TaskSystem state. Must come AFTER
	// Zenith_TaskSystem::Shutdown above -- the forwarder there reads
	// from g_xEngine.Tasks() to drive Shutdown.
	delete m_pxTasks;
	m_pxTasks = nullptr;

	// 11. Free the per-Engine Profiling state. Comes AFTER TaskSystem
	// shutdown (step 9) -- worker threads may have profile scopes
	// pending until they exit, and AFTER tearing down m_pxTasks so no
	// stale ThreadFunc can call Profiling.
	delete m_pxProfiling;
	m_pxProfiling = nullptr;

	// 12. Tear down per-frame timing state. Done late so any
	// subsystem shutdown that needs to log dt or read accumulated
	// time still can.
	delete m_pxFrame;
	m_pxFrame = nullptr;

	// 13. Tear down the multithreading registry. Done after the task
	// system shutdown (step 9) so worker threads aren't still calling
	// IsMainThread while we're freeing the registry.
	delete m_pxThreading;
	m_pxThreading = nullptr;

	// 14. Free the per-Engine entity store. Done VERY LATE -- after
	// SceneManager::Shutdown (step 3), Physics::Shutdown (step 4),
	// Project_Shutdown (step 5), Flux::Shutdown (step 8) which all
	// may touch entity slots during teardown.
	delete m_pxEntityStore;
	m_pxEntityStore = nullptr;

	// 15. Free the scene-registry state. SceneManager::Shutdown above
	// already drained the slot table; just reclaim the holder.
	delete m_pxSceneRegistry;
	m_pxSceneRegistry = nullptr;

	Zenith_Log(LOG_CATEGORY_CORE, "Shutdown complete");
}
