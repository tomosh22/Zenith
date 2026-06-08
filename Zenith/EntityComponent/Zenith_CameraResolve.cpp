#include "Zenith.h"

#include "EntityComponent/Zenith_CameraResolve.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Core/Zenith_RenderGather.h" // Wave 3: EC-side camera render-gather
#include "Core/Zenith_GizmoTransformAccess.h" // Wave 3: EC-side gizmo transform write-accessor
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

// ============================================================================
// ENGINE-side main-camera resolver (ECS leaf-extraction Phase 6).
//
// These bodies are the verbatim relocation of the removed
// Zenith_SceneData::GetMainCamera / ::TryGetMainCamera and
// Zenith_SceneSystem::FindMainCameraAcrossScenes. They name the concrete
// Zenith_CameraComponent (hence the include above) and so MUST live in the
// engine lib, never in the ECS leaf. It physically lives under EntityComponent/
// (NOT the ECS leaf, which is Zenith/ZenithECS/) and compiles into the aggregate
// engine lib, like Zenith_ComponentMeta_Registration.cpp -- so the leaf-ratchet
// (analyze_code_complexity.py, engine-ci architecture.leaf_ratchet, glob
// Zenith/ZenithECS/**) does not scan it.
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

// ---------------------------------------------------------------------------
// Wave 3: main-camera render-gather. Resolves the main camera (EC-side) and
// extracts its renderer-neutral data so Flux_Graphics no longer names
// Zenith_CameraComponent / Zenith_CameraResolve. Forward (EC->renderer) direction.
// ---------------------------------------------------------------------------
static void Zenith_GatherMainCameraImpl(Zenith_CameraRenderData& xOut)
{
	Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
	if (!pxCamera)
	{
		xOut.m_bValid = false;
		return;
	}
	xOut.m_bValid = true;
	pxCamera->BuildViewMatrix(xOut.m_xViewMatrix);
	pxCamera->BuildProjectionMatrix(xOut.m_xProjMatrix);
	pxCamera->GetPosition(xOut.m_xPositionPad);
	xOut.m_fNearPlane   = pxCamera->GetNearPlane();
	xOut.m_fFarPlane    = pxCamera->GetFarPlane();
	xOut.m_fFOV         = pxCamera->GetFOV();
	xOut.m_fAspectRatio = pxCamera->GetAspectRatio();
}

Zenith_CameraGatherFn g_pfnZenithCameraGather = &Zenith_GatherMainCameraImpl;

// ============================================================================
// Wave 3: gizmo transform write-accessor. The editor gizmo (Flux_Gizmos) edits the
// selected entity's transform; this is the EC-side implementation it drives through
// g_xGizmoTransformAccess so the renderer-side gizmo names no EntityComponent type.
// HasTransform preserves the original GetEditableTransform() resolution (the entity's
// OWN scene, so multi-scene editing still works); the get/set ops match the calls the
// gizmo previously made on the resolved Zenith_TransformComponent*.
// ============================================================================
static bool GizmoHasTransform(Zenith_Entity* pxEntity)
{
	// Verbatim relocation of the old Flux_GizmosImpl::GetEditableTransform resolution:
	// the entity's OWN scene (GetSceneData), then EntityHasComponent — so multi-scene
	// editing (persistent / additively-loaded targets) resolves correctly regardless
	// of the active scene.
	if (!pxEntity) return false;
	Zenith_SceneData* pxSceneData = pxEntity->GetSceneData();
	if (!pxSceneData) return false;
	return pxSceneData->EntityHasComponent<Zenith_TransformComponent>(pxEntity->GetEntityID());
}
static void GizmoGetPosition(Zenith_Entity* pxEntity, Zenith_Maths::Vector3& xOut)    { pxEntity->GetComponent<Zenith_TransformComponent>().GetPosition(xOut); }
static void GizmoGetRotation(Zenith_Entity* pxEntity, Zenith_Maths::Quaternion& xOut) { pxEntity->GetComponent<Zenith_TransformComponent>().GetRotation(xOut); }
static void GizmoGetScale   (Zenith_Entity* pxEntity, Zenith_Maths::Vector3& xOut)    { pxEntity->GetComponent<Zenith_TransformComponent>().GetScale(xOut); }
static void GizmoSetPosition(Zenith_Entity* pxEntity, const Zenith_Maths::Vector3& xIn)    { pxEntity->GetComponent<Zenith_TransformComponent>().SetPosition(xIn); }
static void GizmoSetRotation(Zenith_Entity* pxEntity, const Zenith_Maths::Quaternion& xIn) { pxEntity->GetComponent<Zenith_TransformComponent>().SetRotation(xIn); }
static void GizmoSetScale   (Zenith_Entity* pxEntity, const Zenith_Maths::Vector3& xIn)    { pxEntity->GetComponent<Zenith_TransformComponent>().SetScale(xIn); }

Zenith_GizmoTransformAccessor g_xGizmoTransformAccess = {
	&GizmoHasTransform,
	&GizmoGetPosition, &GizmoGetRotation, &GizmoGetScale,
	&GizmoSetPosition, &GizmoSetRotation, &GizmoSetScale,
};
