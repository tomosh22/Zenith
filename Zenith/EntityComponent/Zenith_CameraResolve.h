#pragma once

// ============================================================================
// Zenith_CameraResolve -- ENGINE-side main-camera resolver (ECS leaf-extraction
// Phase 6).
//
// The ECS core (Zenith_SceneData / Zenith_SceneSystem) stores ONLY the main-
// camera EntityID; it deliberately does not name the concrete
// Zenith_CameraComponent (that would drag the component header into the leaf).
// The typed accessors that resolve a scene's main-camera EntityID to a live
// Zenith_CameraComponent used to live on Zenith_SceneData
// (GetMainCamera / TryGetMainCamera) and on Zenith_SceneSystem
// (FindMainCameraAcrossScenes). They moved here, into an engine-side TU
// (Zenith_CameraResolve.cpp) that compiles into the aggregate engine lib -- NOT
// ZenithECS -- so the concrete-component include stays out of the ECS leaf.
//
// This header is itself leaf-clean: it forward-declares both types and pulls in
// no component / engine headers, so any TU may include it freely.
//
// Semantics are IDENTICAL to the removed accessors:
//   Zenith_GetMainCamera(pxSceneData)
//       == old Zenith_SceneData::GetMainCamera()    -- asserts a valid main
//          camera entity exists, then returns the component by reference.
//   Zenith_TryGetMainCamera(pxSceneData)
//       == old Zenith_SceneData::TryGetMainCamera() -- returns nullptr if the
//          scene's camera entity is unset / stale / has no CameraComponent.
//   Zenith_GetMainCameraAcrossScenes()
//       == old Zenith_SceneSystem::FindMainCameraAcrossScenes() -- scans loaded
//          scenes (active first, then slot order) and returns the first scene's
//          main-camera component, or nullptr. Built on the ECS-core leaf finder
//          Zenith_SceneSystem::FindMainCameraEntityAcrossScenes().
// ============================================================================

class Zenith_CameraComponent;
class Zenith_SceneData;

// Resolve a single scene's main-camera EntityID to its Zenith_CameraComponent.
// Asserts the scene has a valid main-camera entity (parity with the old
// Zenith_SceneData::GetMainCamera()).
Zenith_CameraComponent& Zenith_GetMainCamera(Zenith_SceneData* pxSceneData);

// Non-asserting variant: nullptr if the scene's main-camera entity is unset,
// stale, or lacks a Zenith_CameraComponent (parity with the old
// Zenith_SceneData::TryGetMainCamera()).
Zenith_CameraComponent* Zenith_TryGetMainCamera(Zenith_SceneData* pxSceneData);

// Cross-scene resolve: the engine-side replacement for the old
// Zenith_SceneSystem::FindMainCameraAcrossScenes(). Returns nullptr when no
// loaded scene has a resolvable main camera. Must be called from the main
// thread (it walks the live ECS via Zenith_SceneSystem).
Zenith_CameraComponent* Zenith_GetMainCameraAcrossScenes();
