#pragma once

// =============================================================================
// Zenith_ProjectHooks.h — the single declaration of the game-wiring contract
// =============================================================================
//
// Every game defines these free functions in its entry .cpp (e.g.
// Games/Sokoban/Sokoban.cpp); the engine, editor, tools and platform mains
// call them. This header is the ONE place they are declared — it replaces the
// `extern` forward-declarations that used to be scattered across Zenith_Engine,
// Zenith_Main, the editor, the terrain editor, the Android main and the asset
// tools. `Zenith.h` (the precompiled header every .cpp includes first) pulls
// this in, so both sides — the game definitions and every engine call site —
// see the same signatures and a mismatch is a compile error, not a link error.
//
// === Project contract ===
//
// The engine drives the hooks in this order (see Zenith_Engine::Initialise /
// InitialiseProject / Shutdown):
//
//   1. Project_SetGraphicsOptions          — early, before Flux init
//   2. Project_RegisterGameComponents      — register components / graph nodes / events
//   3a. [ZENITH_TOOLS] Project_InitializeResources
//   3b. [ZENITH_TOOLS] Project_RegisterEditorAutomationSteps  (then automation drains)
//   3c. [runtime]      Project_LoadInitialScene                (loads the pre-authored .zscen)
//   ... main loop ...
//   4. Project_Shutdown  — runs AFTER physics shutdown and BEFORE
//                          Zenith_AssetRegistry::Shutdown.
//
// LIFECYCLE RULE (load-bearing): Project_Shutdown MUST Clear() every
// Zenith_AssetHandle the game holds in process/static storage. Static handle
// destructors run after the asset registry is already gone, so an un-Clear'd
// handle Release()s into freed registry memory (use-after-free). Clearing the
// handles here drops their refs while the registry is still alive.
//
// Resource-registration rule: create game assets (meshes/materials/prefabs) in
// Project_RegisterGameComponents (runtime) or Project_InitializeResources
// (tools), NOT lazily — the registry and component meta must be live first.
// =============================================================================

struct Zenith_GraphicsOptions;

// --- Unconditional (every configuration) ---

// Human-readable game name (used for asset dirs, window title, ImGui ini, logs).
extern const char* Project_GetName();

// Absolute path to the game's Assets directory (resolves the "game:" prefix).
extern const char* Project_GetGameAssetsDirectory();

// Populate the graphics configuration before the renderer initialises.
extern void Project_SetGraphicsOptions(Zenith_GraphicsOptions& xOptions);

// Register the game's ECS components, Behaviour Graph nodes and event handlers.
extern void Project_RegisterGameComponents();

// Load the initial (pre-authored) scene — the runtime, non-tools boot path.
extern void Project_LoadInitialScene();

// Drop all held asset-handle refs (see LIFECYCLE RULE above) and free game
// resources. Runs before Zenith_AssetRegistry::Shutdown.
extern void Project_Shutdown();

// --- Tools-only (editor/authoring builds) ---

#ifdef ZENITH_TOOLS
// Create the game's procedural resources (meshes, materials, prefabs, configs).
extern void Project_InitializeResources();

// Enqueue boot-time editor-automation steps (scene / graph / UI authoring),
// drained before the initial scene is loaded.
extern void Project_RegisterEditorAutomationSteps();
#endif
