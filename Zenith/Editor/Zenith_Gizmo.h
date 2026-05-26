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
	static void Initialise();
	static void Shutdown();
	
	// Render and interact with gizmo
	static bool Manipulate(
		Zenith_Entity* pxEntity,
		GizmoOperation eOperation,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
	// Settings
	static void SetSnapEnabled(bool enabled);
	static bool IsSnapEnabled();

	static void SetSnapValue(float value);
	static float GetSnapValue();

	// State queries
	static bool IsManipulating();

	static bool HandleTranslateGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
	static bool HandleRotateGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
	static bool HandleScaleGizmo(
		Zenith_Entity* pxEntity,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize
	);
	
	static void RenderTranslateGizmo(
		const Zenith_Maths::Vector3& position,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix
	);

	// Mouse ray casting
	static Zenith_Maths::Vector3 ScreenToWorldRay(
		const Zenith_Maths::Vector2& mousePos,
		const Zenith_Maths::Vector2& viewportPos,
		const Zenith_Maths::Vector2& viewportSize,
		const Zenith_Maths::Matrix4& viewMatrix,
		const Zenith_Maths::Matrix4& projMatrix
	);
	
	static float RayPlaneIntersection(
		const Zenith_Maths::Vector3& rayOrigin,
		const Zenith_Maths::Vector3& rayDir,
		const Zenith_Maths::Vector3& planePoint,
		const Zenith_Maths::Vector3& planeNormal
	);
	
	// Phase 5.5d: 7 statics moved to Zenith_GizmoImpl (held by Zenith_Engine).
};

#endif // ZENITH_TOOLS
