#pragma once

#include "Maths/Zenith_Maths.h"

class Zenith_Entity;

// ---------------------------------------------------------------------------
// Gizmo transform-access boundary (Wave 3: Flux<-EC reverse-edge decoupling).
//
// The editor translate/rotate/scale gizmo (Flux_Gizmos, a renderer-side tool)
// reads and WRITES the selected entity's transform. Rather than Flux including
// EntityComponent/Components/Zenith_TransformComponent.h (a Flux->EC reverse
// edge), the EC side publishes this neutral read/write accessor and the gizmo
// drives it through a function-pointer table. This is the WRITE counterpart of
// the Wave-3 render-data gathers in Zenith_RenderGather.h: the renderer names no
// component type; the EC-side implementation (tools-only) is the only place that
// resolves Zenith_Entity -> Zenith_TransformComponent.
//
// Each accessor takes the target Zenith_Entity* (forward-declared — the gizmo
// already holds one) and resolves the transform through the entity's OWN scene,
// preserving the multi-scene-editing behavior of the original GetEditableTransform().
// m_pfnHasTransform must be checked before the get/set accessors are called.
// ---------------------------------------------------------------------------

struct Zenith_GizmoTransformAccessor
{
	bool (*m_pfnHasTransform)(Zenith_Entity* pxEntity)                                          = nullptr;
	void (*m_pfnGetPosition) (Zenith_Entity* pxEntity, Zenith_Maths::Vector3& xOut)             = nullptr;
	void (*m_pfnGetRotation) (Zenith_Entity* pxEntity, Zenith_Maths::Quaternion& xOut)          = nullptr;
	void (*m_pfnGetScale)    (Zenith_Entity* pxEntity, Zenith_Maths::Vector3& xOut)             = nullptr;
	void (*m_pfnSetPosition) (Zenith_Entity* pxEntity, const Zenith_Maths::Vector3& xIn)        = nullptr;
	void (*m_pfnSetRotation) (Zenith_Entity* pxEntity, const Zenith_Maths::Quaternion& xIn)     = nullptr;
	void (*m_pfnSetScale)    (Zenith_Entity* pxEntity, const Zenith_Maths::Vector3& xIn)        = nullptr;
};

// Wired by the EC side (tools-only). Null in non-tools builds, where the gizmo
// (itself #ifdef ZENITH_TOOLS) does not exist.
extern Zenith_GizmoTransformAccessor g_xGizmoTransformAccess;
