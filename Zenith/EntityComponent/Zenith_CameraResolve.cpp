#include "Zenith.h"

#include "EntityComponent/Zenith_CameraResolve.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

// ============================================================================
// ENGINE-side main-camera resolver (ECS leaf-extraction Phase 6).
//
// These bodies are the verbatim relocation of the removed
// Zenith_SceneData::GetMainCamera / ::TryGetMainCamera and
// Zenith_SceneSystem::FindMainCameraAcrossScenes. They name the concrete
// Zenith_CameraComponent (hence the include above) and so MUST live in the
// engine lib, never in the ECS leaf. This TU's basename is listed in the
// ECS-leaf ratchet's ExcludeNames (Tools/layering_gate.ps1) for exactly that
// reason -- it physically lives under EntityComponent/ but compiles into the
// aggregate engine lib, like Zenith_ComponentMeta_Registration.cpp.
// ============================================================================

Zenith_CameraComponent& Zenith_GetMainCamera(Zenith_SceneData* pxSceneData)
{
	// Parity with the old Zenith_SceneData::GetMainCamera(): assert a valid,
	// existing main-camera entity, then resolve it through the same
	// GetComponentFromEntity<Zenith_CameraComponent> path.
	const Zenith_EntityID xCameraEntity = pxSceneData->GetMainCameraEntity();
	Zenith_Assert(xCameraEntity.IsValid() && pxSceneData->EntityExists(xCameraEntity), "Zenith_GetMainCamera: No valid main camera set");
	return pxSceneData->GetEntity(xCameraEntity).GetComponent<Zenith_CameraComponent>();
}

Zenith_CameraComponent* Zenith_TryGetMainCamera(Zenith_SceneData* pxSceneData)
{
	// Parity with the old Zenith_SceneData::TryGetMainCamera(): nullptr unless
	// the scene's main-camera entity is valid, still exists, AND carries a
	// Zenith_CameraComponent.
	const Zenith_EntityID xCameraEntity = pxSceneData->GetMainCameraEntity();
	if (!xCameraEntity.IsValid() || !pxSceneData->EntityExists(xCameraEntity))
	{
		return nullptr;
	}
	if (!pxSceneData->EntityHasComponent<Zenith_CameraComponent>(xCameraEntity))
	{
		return nullptr;
	}
	return &pxSceneData->GetEntity(xCameraEntity).GetComponent<Zenith_CameraComponent>();
}

Zenith_CameraComponent* Zenith_GetMainCameraAcrossScenes()
{
	// Parity with the old Zenith_SceneSystem::FindMainCameraAcrossScenes(): the
	// ECS-core leaf finder applies the same scene-iteration order (active scene
	// first, then loaded scenes in slot order) and the same per-scene
	// valid-and-exists gate; here we resolve the returned EntityID to its
	// Zenith_CameraComponent (the HasComponent gate), returning nullptr if the
	// camera entity carries no CameraComponent or no scene had a camera.
	const Zenith_EntityID xCameraEntity = g_xEngine.Scenes().FindMainCameraEntityAcrossScenes();
	if (!xCameraEntity.IsValid())
	{
		return nullptr;
	}

	// Resolve through the entity's OWNING scene (component pools are per-scene).
	// GetSceneDataForEntity validates slot occupancy + generation before mapping
	// the slot's scene handle back to its Zenith_SceneData.
	Zenith_SceneData* pxOwningScene = g_xEngine.Scenes().GetSceneDataForEntity(xCameraEntity);
	if (pxOwningScene == nullptr)
	{
		return nullptr;
	}
	return Zenith_TryGetMainCamera(pxOwningScene);
}
