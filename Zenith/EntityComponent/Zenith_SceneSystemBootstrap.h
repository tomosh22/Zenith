#pragma once

// =============================================================================
// Zenith_SceneSystemBootstrap.h
// -----------------------------------------------------------------------------
// Top-level orchestrator free functions for the scene system. Previously
// `Zenith_SceneManager::Initialise/Shutdown/ResetForNextTest`. Moved out of
// the manager class for Phase 5e so the manager class can be deleted.
//
// Each function coordinates multiple subsystems (SceneLifecycle, SceneOperations,
// SceneRegistry, SceneCallbacks, EntityStore) — they cannot belong to any
// single subsystem.
// =============================================================================

// Initialise the entire scene system: spawns the animation task, creates the
// persistent ("DontDestroyOnLoad") scene. Called once from Zenith_Engine
// startup after the entity store is online.
void Zenith_InitialiseSceneSystem();

// Tear down the entire scene system in reverse-startup order: waits for the
// animation task, drains async loads/unloads, deletes scene data, clears
// callback lists, resets entity storage. Called from Zenith_Engine shutdown.
void Zenith_ShutdownSceneSystem();

#ifdef ZENITH_TESTING
// Test-harness reset: clears transient flags, drains the operation queue,
// runs a full Shutdown()+Initialise() cycle, and recreates a default active
// scene named "TestHarnessDefaultScene". Called by the test framework
// between every test.
void Zenith_ResetSceneSystemForNextTest();
#endif
