#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Gizmo.h"

#include <glm/gtc/matrix_transform.hpp>

Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
	const Zenith_Maths::Vector2& mousePos,
	const Zenith_Maths::Vector2&,
	const Zenith_Maths::Vector2& viewportSize,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// STEP 1: Normalize mouse position to NDC (Normalized Device Coordinates)
	// NDC range is [-1, 1] for both X and Y
	float x = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
	float y = (mousePos.y / viewportSize.y) * 2.0f - 1.0f;

	// FIXED: Don't flip Y - the projection matrix already handles the coordinate system
	// The original flip was causing inverted Y interaction (clicking top hits bottom, dragging up moves down)
	// Screen space: (0,0) = top-left, Y increases downward
	// After normalization: top → y=-1, bottom → y=+1
	// This matches what the projection matrix expects

	// STEP 2: Create clip space coordinates
	// For Vulkan, depth range is [0, 1], use 0 for near plane
	Zenith_Maths::Vector4 rayClip(x, y, 0.0f, 1.0f);

	// STEP 3: Transform to view/eye space
	Zenith_Maths::Matrix4 invProj = glm::inverse(projMatrix);
	Zenith_Maths::Vector4 rayEye = invProj * rayClip;

	// Convert to direction vector (not a point)
	// Perspective divide
	rayEye.x /= rayEye.w;
	rayEye.y /= rayEye.w;
	rayEye.z /= rayEye.w;
	rayEye.w = 0.0f;  // Direction, not a point

	// STEP 4: Transform to world space
	Zenith_Maths::Matrix4 invView = glm::inverse(viewMatrix);
	Zenith_Maths::Vector4 rayWorld4 = invView * rayEye;

	// STEP 5: Extract and normalize direction
	Zenith_Maths::Vector3 rayDir = Zenith_Maths::Vector3(rayWorld4.x, rayWorld4.y, rayWorld4.z);
	rayDir = glm::normalize(rayDir);

	return rayDir;
}

#endif // ZENITH_TOOLS
