#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Gizmos/Flux_Gizmos.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

class Zenith_Entity;

// Phase 7e: per-Engine state for Flux_Gizmos subsystem.
// GizmoGeometry is a private nested struct on Flux_Gizmos; this Impl is
// granted friend access in the facade so the geometry-list members can
// be typed as the nested type.
class Flux_GizmosImpl
{
public:
	Flux_GizmosImpl() = default;
	~Flux_GizmosImpl() = default;

	Flux_GizmosImpl(const Flux_GizmosImpl&) = delete;
	Flux_GizmosImpl& operator=(const Flux_GizmosImpl&) = delete;

	Zenith_Entity*           m_pxTargetEntity      = nullptr;
	GizmoMode                m_eMode               = GizmoMode::Translate;
	GizmoComponent           m_eHoveredComponent   = GizmoComponent::None;
	GizmoComponent           m_eActiveComponent    = GizmoComponent::None;
	bool                     m_bIsInteracting      = false;

	Zenith_Maths::Vector3    m_xInteractionStartPos   = Zenith_Maths::Vector3(0, 0, 0);
	Zenith_Maths::Vector3    m_xInitialEntityPosition = Zenith_Maths::Vector3(0, 0, 0);
	Zenith_Maths::Quaternion m_xInitialEntityRotation = Zenith_Maths::Quaternion(1, 0, 0, 0);
	Zenith_Maths::Vector3    m_xInitialEntityScale    = Zenith_Maths::Vector3(1, 1, 1);
	float                    m_fGizmoScale            = 1.0f;

	Flux_Pipeline            m_xPipeline;
	Flux_Shader              m_xShader;
	Flux_CommandList         m_xCommandList{"Gizmos"};

	Zenith_Vector<Flux_Gizmos::GizmoGeometry> m_xTranslateGeometry;
	Zenith_Vector<Flux_Gizmos::GizmoGeometry> m_xRotateGeometry;
	Zenith_Vector<Flux_Gizmos::GizmoGeometry> m_xScaleGeometry;
};

#endif // ZENITH_TOOLS
