#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"

class Zenith_Entity;

enum class GizmoAxis
{
	None = 0,
	X = 1,
	Y = 2,
	Z = 4,
	XY = X | Y,
	XZ = X | Z,
	YZ = Y | Z,
	XYZ = X | Y | Z
};

enum class GizmoOperation
{
	Translate,
	Rotate,
	Scale
};

class Zenith_Gizmo
{
public:
void Initialise();
void Shutdown();
	
	// Render and interact with gizmo
bool Manipulate(
		Zenith_Entity* pxEntity,
		GizmoOperation eOperation,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
	// Settings
void SetSnapEnabled(bool enabled);
bool IsSnapEnabled();

void SetSnapValue(float value);
float GetSnapValue();

	// State queries
bool IsManipulating();

bool HandleTranslateGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
bool HandleRotateGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
bool HandleScaleGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
void RenderTranslateGizmo(
		const Zenith_Maths::Vector3& position,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix
	);

	// Mouse ray casting
Zenith_Maths::Vector3 ScreenToWorldRay(
		const Zenith_Maths::Vector2& mousePos,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix
	);
	
float RayPlaneIntersection(
		const Zenith_Maths::Vector3& rayOrigin,
		const Zenith_Maths::Vector3& rayDir,
		const Zenith_Maths::Vector3& planePoint,
		const Zenith_Maths::Vector3& planeNormal
	);
	
	// ===== Data members (was Zenith_Gizmo) =====
	GizmoAxis             m_eActiveAxis          = GizmoAxis::None;
	bool                  m_bIsManipulating      = false;
	Zenith_Maths::Vector3 m_xManipulationStartPos = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector2 m_xMouseStartPos        = Zenith_Maths::Vector2(0.0f);
	bool                  m_bSnapEnabled         = false;
	float                 m_fSnapValue           = 1.0f;
	float                 m_fGizmoSize           = 1.0f;
};

#endif // ZENITH_TOOLS
