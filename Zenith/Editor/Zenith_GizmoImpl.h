#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Gizmo.h"
#include "Maths/Zenith_Maths.h"

// Phase 5.5d: per-Engine gizmo manipulation state.
class Zenith_GizmoImpl
{
public:
	Zenith_GizmoImpl() = default;
	~Zenith_GizmoImpl() = default;

	Zenith_GizmoImpl(const Zenith_GizmoImpl&) = delete;
	Zenith_GizmoImpl& operator=(const Zenith_GizmoImpl&) = delete;

	GizmoAxis             m_eActiveAxis          = GizmoAxis::None;
	bool                  m_bIsManipulating      = false;
	Zenith_Maths::Vector3 m_xManipulationStartPos = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector2 m_xMouseStartPos        = Zenith_Maths::Vector2(0.0f);
	bool                  m_bSnapEnabled         = false;
	float                 m_fSnapValue           = 1.0f;
	float                 m_fGizmoSize           = 1.0f;
};

#endif // ZENITH_TOOLS
