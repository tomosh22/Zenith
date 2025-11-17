#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Gizmo.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_Input.h"
#include <glm/gtc/matrix_transform.hpp>

// Static member initialization
GizmoAxis Zenith_Gizmo::s_eActiveAxis = GizmoAxis::None;
bool Zenith_Gizmo::s_bIsManipulating = false;
Zenith_Maths::Vector3 Zenith_Gizmo::s_xManipulationStartPos = Zenith_Maths::Vector3(0.0f);
Zenith_Maths::Vector2 Zenith_Gizmo::s_xMouseStartPos = Zenith_Maths::Vector2(0.0f);
bool Zenith_Gizmo::s_bSnapEnabled = false;
float Zenith_Gizmo::s_fSnapValue = 1.0f;
float Zenith_Gizmo::s_fGizmoSize = 1.0f;

void Zenith_Gizmo::Initialise()
{
	s_eActiveAxis = GizmoAxis::None;
	s_bIsManipulating = false;
	s_bSnapEnabled = false;
	s_fSnapValue = 1.0f;
	s_fGizmoSize = 1.0f;
}

void Zenith_Gizmo::Shutdown()
{
	// Nothing to clean up yet
}

bool Zenith_Gizmo::Manipulate(
	Zenith_Entity* pxEntity,
	GizmoOperation eOperation,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize)
{
	if (!pxEntity)
		return false;
	
	switch (eOperation)
	{
	case GizmoOperation::Translate:
		return HandleTranslateGizmo(pxEntity, viewMatrix, projMatrix, viewportPos, viewportSize);
	case GizmoOperation::Rotate:
		return HandleRotateGizmo(pxEntity, viewMatrix, projMatrix, viewportPos, viewportSize);
	case GizmoOperation::Scale:
		return HandleScaleGizmo(pxEntity, viewMatrix, projMatrix, viewportPos, viewportSize);
	default:
		return false;
	}
}

bool Zenith_Gizmo::HandleTranslateGizmo(
	Zenith_Entity* pxEntity,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize)
{
	// TODO: Implement translate gizmo interaction
	return false;
}

bool Zenith_Gizmo::HandleRotateGizmo(
	Zenith_Entity* pxEntity,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize)
{
	// TODO: Implement rotate gizmo interaction
	return false;
}

bool Zenith_Gizmo::HandleScaleGizmo(
	Zenith_Entity* pxEntity,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize)
{
	// TODO: Implement scale gizmo interaction
	return false;
}

void Zenith_Gizmo::RenderTranslateGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement translate gizmo rendering
}

void Zenith_Gizmo::RenderRotateGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement rotate gizmo rendering
}

void Zenith_Gizmo::RenderScaleGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement scale gizmo rendering
}

Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
	const Zenith_Maths::Vector2& mousePos,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement screen to world ray conversion
	return Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
}

float Zenith_Gizmo::RayPlaneIntersection(
	const Zenith_Maths::Vector3& rayOrigin,
	const Zenith_Maths::Vector3& rayDir,
	const Zenith_Maths::Vector3& planePoint,
	const Zenith_Maths::Vector3& planeNormal)
{
	float denom = glm::dot(planeNormal, rayDir);
	if (glm::abs(denom) > 1e-6f)
	{
		Zenith_Maths::Vector3 p0l0 = planePoint - rayOrigin;
		float t = glm::dot(p0l0, planeNormal) / denom;
		return t >= 0.0f ? t : -1.0f;
	}
	return -1.0f;
}

#endif // ZENITH_TOOLS
