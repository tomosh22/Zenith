#include "Zenith.h"

#include "EntityComponent/Zenith_SceneSystemBootstrap.h"
#include "EntityComponent/Internal/Zenith_SceneLifecycleScheduler.h"
#include "EntityComponent/Internal/Zenith_SceneOperationQueue.h"
#include "EntityComponent/Internal/Zenith_SceneRegistry.h"
#include "EntityComponent/Internal/Zenith_SceneCallbackBus.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Core/Zenith_Engine.h"

//=============================================================================
// Zenith_SceneSystemBootstrap — orchestrator free-function implementations.
//
// Each function coordinates multiple subsystems (Lifecycle, Operations,
// Registry, Callbacks, EntityStore). Bodies migrated from
// Zenith_SceneManager::Initialise/Shutdown/ResetForNextTest during Phase 5e.
//=============================================================================

void Zenith_InitialiseSceneSystem()
{
	// Scheduler creates the animation task.
	g_xEngine.SceneLifecycle().Initialise();

	// A6: Create the persistent scene WITHOUT auto-activating. It's a container
	// for DontDestroyOnLoad entities, not a user-visible scene, and must never
	// be "active" in Unity terminology.
	Zenith_Scene xPersistent = g_xEngine.SceneRegistry().CreateEmptyScene("DontDestroyOnLoad", /*bAllowSetActive=*/false);
	g_xEngine.SceneRegistry().m_iPersistentSceneHandle = xPersistent.m_iHandle;

	Zenith_Log(LOG_CATEGORY_SCENE, "Scene system initialised with persistent scene (handle=%d)", g_xEngine.SceneRegistry().m_iPersistentSceneHandle);
}

void Zenith_ShutdownSceneSystem()
{
	// Tear down per-subsystem in reverse-startup order.
	g_xEngine.SceneLifecycle().Shutdown();   // waits for anim task
	g_xEngine.SceneOperations().Shutdown();  // waits for in-flight async loads/unloads
	g_xEngine.SceneRegistry().Shutdown();    // deletes scene data + clears slot tables
	g_xEngine.SceneCallbacks().Shutdown();   // clears callback lists + suppression flags

	// Reset global entity storage (shared across all scenes).
	g_xEngine.EntityStore().Reset();
}

#ifdef ZENITH_TESTING
void Zenith_ResetSceneSystemForNextTest()
{
	// Clear transient flags that might have been left true by a crashed or
	// early-returning test.
	g_xEngine.SceneLifecycle().m_bIsLoadingScene   = false;
	g_xEngine.SceneLifecycle().m_bIsUpdating       = false;
	g_xEngine.SceneLifecycle().m_ulLastDeferredLoadOp = ZENITH_INVALID_OPERATION_ID;
	g_xEngine.SceneOperations().m_uUnloadUnusedAssetsCallCount = 0;

	// Tear down the queue (waits for in-flight workers, deletes jobs and operations).
	g_xEngine.SceneOperations().ResetForNextTest();

	// Full Shutdown() + Initialise() cycle. This is the same operation
	// TestShutdownClearsAllStatics performs and it's the most reliable way
	// to guarantee a pristine baseline — every scene is deleted, every
	// cached handle is invalidated, the animation task is re-created, and
	// the persistent scene is re-spawned.
	Zenith_ShutdownSceneSystem();
	Zenith_InitialiseSceneSystem();

	// Many editor / AI / physics tests assume an active scene exists to
	// create entities in. Provide one up-front so individual tests don't
	// need to repeat the boilerplate.
	Zenith_Scene xActive = g_xEngine.SceneRegistry().CreateEmptyScene("TestHarnessDefaultScene");
	g_xEngine.SceneRegistry().m_iActiveSceneHandle = xActive.m_iHandle;
}
#endif
