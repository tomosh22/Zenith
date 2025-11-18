#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Gizmo.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Input/Zenith_Input.h"
#include <glm/gtc/matrix_transform.hpp>

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

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
	if (!pxEntity || !pxEntity->HasComponent<Zenith_TransformComponent>())
		return false;

	Zenith_TransformComponent& xTransform = pxEntity->GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xEntityPos;
	xTransform.GetPosition(xEntityPos);

	// Get current mouse position in viewport space
	Zenith_Maths::Vector2_64 xMousePos64;
	Zenith_Input::GetMousePosition(xMousePos64);
	Zenith_Maths::Vector2 xMousePos = {
		static_cast<float>(xMousePos64.x - viewportPos.x),
		static_cast<float>(xMousePos64.y - viewportPos.y)
	};

	// STATE MACHINE: Idle -> Manipulating -> Idle
	if (!s_bIsManipulating)
	{
		// Check if mouse button was pressed to start manipulation
		if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		{
			// For simplicity, we'll use a basic hit test
			// In a full implementation, we'd test against rendered gizmo geometry
			// Here we'll start manipulation if mouse is near gizmo center
			s_bIsManipulating = true;
			s_eActiveAxis = GizmoAxis::X;  // Default to X axis for now
			s_xManipulationStartPos = xEntityPos;
			s_xMouseStartPos = xMousePos;
		}
	}
	else
	{
		// Currently manipulating - update entity position
		// Get camera position for ray origin
		Zenith_Maths::Matrix4 xInvView = glm::inverse(viewMatrix);
		Zenith_Maths::Vector3 xCameraPos = Zenith_Maths::Vector3(xInvView[3]);

		// Convert mouse to world ray
		Zenith_Maths::Vector3 xRayDir = ScreenToWorldRay(
			xMousePos,
			{ 0, 0 },
			viewportSize,
			viewMatrix,
			projMatrix
		);

		// Create constraint plane based on active axis
		Zenith_Maths::Vector3 xPlaneNormal;
		switch (s_eActiveAxis)
		{
		case GizmoAxis::X:
			xPlaneNormal = { 0.0f, 1.0f, 0.0f };  // XZ plane
			break;
		case GizmoAxis::Y:
			xPlaneNormal = { 0.0f, 0.0f, 1.0f };  // XY plane
			break;
		case GizmoAxis::Z:
			xPlaneNormal = { 1.0f, 0.0f, 0.0f };  // YZ plane
			break;
		default:
			xPlaneNormal = { 0.0f, 1.0f, 0.0f };
			break;
		}

		// Intersect ray with plane
		float fT = RayPlaneIntersection(xCameraPos, xRayDir, s_xManipulationStartPos, xPlaneNormal);

		if (fT >= 0.0f)
		{
			// Calculate intersection point
			Zenith_Maths::Vector3 xIntersection = xCameraPos + xRayDir * fT;
			Zenith_Maths::Vector3 xNewPos = s_xManipulationStartPos;

			// Apply movement only along active axis
			switch (s_eActiveAxis)
			{
			case GizmoAxis::X:
				xNewPos.x = xIntersection.x;
				break;
			case GizmoAxis::Y:
				xNewPos.y = xIntersection.y;
				break;
			case GizmoAxis::Z:
				xNewPos.z = xIntersection.z;
				break;
			}

			// Apply snapping if enabled
			if (s_bSnapEnabled)
			{
				xNewPos = glm::round(xNewPos / s_fSnapValue) * s_fSnapValue;
			}

			// Update entity transform
			xTransform.SetPosition(xNewPos);
		}

		// Check for mouse release to end manipulation
		if (!Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT))
		{
			s_bIsManipulating = false;
			s_eActiveAxis = GizmoAxis::None;
		}
	}

	// Render the gizmo
	RenderTranslateGizmo(xEntityPos, viewMatrix, projMatrix);

	return s_bIsManipulating;
}

bool Zenith_Gizmo::HandleRotateGizmo(
	Zenith_Entity* pxEntity,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize)
{
	// TODO: Implement rotate gizmo interaction
	// Similar state machine to translate gizmo but with rotation math
	//
	// VISUAL REPRESENTATION:
	// - Three colored rings/circles around the object
	// - X axis = red ring in YZ plane
	// - Y axis = green ring in XZ plane  
	// - Z axis = blue ring in XY plane
	//
	// INTERACTION:
	// 1. Mouse hover detection
	//    - Check distance from mouse to each ring arc
	//    - Highlight hovered ring
	//
	// 2. Rotation calculation when dragging:
	//    a. Project mouse movement onto rotation plane
	//    b. Calculate angle from starting position
	//    c. Convert angle to quaternion rotation
	//    d. Apply to entity's rotation component
	//
	// MATHEMATICAL APPROACH:
	// 1. When click starts, store:
	//    - Initial entity rotation (quaternion)
	//    - Initial mouse position
	//    - Rotation plane (based on active axis)
	//
	// 2. During drag:
	//    - Project current mouse to rotation plane
	//    - Calculate vector from gizmo center to projected mouse pos
	//    - Calculate angle between start vector and current vector
	//    - Create rotation quaternion from axis and angle
	//    - Multiply with initial rotation
	//    - Apply to entity
	//
	// 3. Snapping:
	//    - If enabled, snap angle to nearest s_fSnapValue degrees
	//    - Common values: 15�, 45�, 90�
	//
	// QUATERNION MATH REMINDER:
	// - Rotation around axis: quat = (axis * sin(angle/2), cos(angle/2))
	// - Combine rotations: q_final = q_new * q_initial
	// - Always normalize after operations
	
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
	// Similar to translate but modifies scale instead of position
	//
	// VISUAL REPRESENTATION:
	// - Three colored lines with small cubes/boxes at the ends
	// - X axis = red line with red cube
	// - Y axis = green line with green cube
	// - Z axis = blue line with blue cube
	// - Optional: White cube at center for uniform scaling
	//
	// SCALING BEHAVIOR:
	// 1. Dragging on an axis scales along that axis only
	//    - Moving mouse away from center = increase scale
	//    - Moving mouse toward center = decrease scale
	//
	// 2. Uniform scaling (if implemented):
	//    - Click on center cube
	//    - Scale all three axes equally
	//
	// CALCULATION:
	// 1. When drag starts:
	//    - Store initial scale vector
	//    - Store initial mouse position
	//    - Store mouse distance from gizmo center
	//
	// 2. During drag:
	//    - Calculate current mouse distance from gizmo center
	//    - Scale factor = current_distance / initial_distance
	//    - newScale = initialScale * scaleFactor (for active axis only)
	//
	// 3. Constraints:
	//    - Minimum scale (prevent negative/zero scale)
	//    - Suggested minimum: 0.01 or 0.001
	//    - Maximum scale (optional, prevent extreme values)
	//
	// 4. Snapping:
	//    - Snap to nearest s_fSnapValue increment
	//    - Common values: 0.1, 0.25, 0.5
	//
	// ALTERNATIVE APPROACH (Mouse delta based):
	// - Calculate mouse movement delta (current - start)
	// - Convert delta to scale multiplier
	// - More intuitive for some users
	
	return false;
}

void Zenith_Gizmo::RenderTranslateGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// Helper lambda to convert world position to screen space
	auto WorldToScreen = [&](const Zenith_Maths::Vector3& worldPos) -> Zenith_Maths::Vector2 {
		Zenith_Maths::Vector4 clipPos = projMatrix * viewMatrix * Zenith_Maths::Vector4(worldPos, 1.0f);

		// Perspective divide
		if (clipPos.w != 0.0f)
		{
			clipPos.x /= clipPos.w;
			clipPos.y /= clipPos.w;
			clipPos.z /= clipPos.w;
		}

		// Convert from NDC [-1,1] to screen space
		// Note: ImGui coordinates start from top-left
		Zenith_Maths::Vector2 screenPos;
		screenPos.x = (clipPos.x + 1.0f) * 0.5f;
		screenPos.y = (1.0f - clipPos.y) * 0.5f;  // Flip Y

		return screenPos;
	};

	// Calculate gizmo size based on distance to camera for constant screen size
	Zenith_Maths::Matrix4 invView = glm::inverse(viewMatrix);
	Zenith_Maths::Vector3 cameraPos = Zenith_Maths::Vector3(invView[3]);
	float distanceToCamera = glm::length(position - cameraPos);
	float gizmoWorldSize = distanceToCamera * 0.15f;  // Adjust this factor for desired size

	// Get ImGui draw list for rendering
	ImDrawList* pDrawList = ImGui::GetForegroundDrawList();

	// Define axis colors
	ImU32 xAxisColor = (s_eActiveAxis == GizmoAxis::X) ?
		IM_COL32(255, 128, 128, 255) :  // Bright red when active
		IM_COL32(255, 0, 0, 255);        // Red
	ImU32 yAxisColor = (s_eActiveAxis == GizmoAxis::Y) ?
		IM_COL32(128, 255, 128, 255) :  // Bright green when active
		IM_COL32(0, 255, 0, 255);        // Green
	ImU32 zAxisColor = (s_eActiveAxis == GizmoAxis::Z) ?
		IM_COL32(128, 128, 255, 255) :  // Bright blue when active
		IM_COL32(0, 0, 255, 255);        // Blue

	// Render X axis (Red)
	{
		Zenith_Maths::Vector3 axisEnd = position + Zenith_Maths::Vector3(gizmoWorldSize, 0, 0);
		Zenith_Maths::Vector2 screenStart = WorldToScreen(position);
		Zenith_Maths::Vector2 screenEnd = WorldToScreen(axisEnd);

		// Convert to ImVec2
		ImVec2 start(screenStart.x, screenStart.y);
		ImVec2 end(screenEnd.x, screenEnd.y);

		pDrawList->AddLine(start, end, xAxisColor, 3.0f);

		// Simple arrow head (small circle at end)
		pDrawList->AddCircleFilled(end, 5.0f, xAxisColor);
	}

	// Render Y axis (Green)
	{
		Zenith_Maths::Vector3 axisEnd = position + Zenith_Maths::Vector3(0, gizmoWorldSize, 0);
		Zenith_Maths::Vector2 screenStart = WorldToScreen(position);
		Zenith_Maths::Vector2 screenEnd = WorldToScreen(axisEnd);

		ImVec2 start(screenStart.x, screenStart.y);
		ImVec2 end(screenEnd.x, screenEnd.y);

		pDrawList->AddLine(start, end, yAxisColor, 3.0f);
		pDrawList->AddCircleFilled(end, 5.0f, yAxisColor);
	}

	// Render Z axis (Blue)
	{
		Zenith_Maths::Vector3 axisEnd = position + Zenith_Maths::Vector3(0, 0, gizmoWorldSize);
		Zenith_Maths::Vector2 screenStart = WorldToScreen(position);
		Zenith_Maths::Vector2 screenEnd = WorldToScreen(axisEnd);

		ImVec2 start(screenStart.x, screenStart.y);
		ImVec2 end(screenEnd.x, screenEnd.y);

		pDrawList->AddLine(start, end, zAxisColor, 3.0f);
		pDrawList->AddCircleFilled(end, 5.0f, zAxisColor);
	}

	// Render center point
	Zenith_Maths::Vector2 screenCenter = WorldToScreen(position);
	ImVec2 center(screenCenter.x, screenCenter.y);
	pDrawList->AddCircleFilled(center, 4.0f, IM_COL32(255, 255, 255, 255));
}

void Zenith_Gizmo::RenderRotateGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement rotate gizmo rendering
	// Draw three colored circles/rings representing rotation axes
	//
	// GIZMO COMPONENTS:
	// 1. Three circles (rings):
	//    - X axis: Red circle in YZ plane
	//    - Y axis: Green circle in XZ plane
	//    - Z axis: Blue circle in XY plane
	//
	// 2. Optional: View-aligned circle for screen-space rotation
	//
	// RENDERING APPROACH:
	// 1. Generate circle vertices:
	//    - Create points around circumference
	//    - Number of segments: 32-64 for smooth appearance
	//    - radius = constant screen size (like translate gizmo)
	//
	// 2. For each axis:
	//    - Generate circle in appropriate plane
	//    - Project points to screen space
	//    - Connect points with lines
	//
	// 3. Visibility handling:
	//    - Back-facing parts of circles should be dimmer or dashed
	//    - Front-facing parts at full brightness
	//    - Can check point normal dot camera direction
	//
	// PSEUDO-CODE for X-axis circle (YZ plane):
	// for (int i = 0; i < numSegments; i++)
	// {
	//     float angle1 = (i / numSegments) * 2 * PI;
	//     float angle2 = ((i + 1) / numSegments) * 2 * PI;
	//     
	//     Vector3 p1 = position + Vector3(0, cos(angle1), sin(angle1)) * radius;
	//     Vector3 p2 = position + Vector3(0, cos(angle2), sin(angle2)) * radius;
	//     
	//     Vector2 screen1 = WorldToScreen(p1);
	//     Vector2 screen2 = WorldToScreen(p2);
	//     
	//     Color c = (active == X) ? BRIGHT_RED : RED;
	//     drawList->AddLine(screen1, screen2, c, 2.0f);
	// }
}

void Zenith_Gizmo::RenderScaleGizmo(
	const Zenith_Maths::Vector3& position,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// TODO: Implement scale gizmo rendering
	// Draw three colored lines with boxes/cubes at the ends
	//
	// GIZMO COMPONENTS:
	// 1. Three lines:
	//    - X axis: Red line
	//    - Y axis: Green line
	//    - Z axis: Blue line
	//
	// 2. End caps (boxes/cubes):
	//    - Small cube at the end of each line
	//    - Same color as the line
	//    - Larger when hovered/active
	//
	// 3. Optional: Center cube for uniform scaling
	//    - White or gray
	//    - Scales all axes together
	//
	// RENDERING:
	// Similar to translate gizmo but with boxes instead of arrows
	//
	// PSEUDO-CODE for X axis:
	// Vector3 lineEnd = position + Vector3(gizmoSize, 0, 0);
	// Vector2 screenStart = WorldToScreen(position);
	// Vector2 screenEnd = WorldToScreen(lineEnd);
	// 
	// // Draw line
	// drawList->AddLine(screenStart, screenEnd, RED, 2.0f);
	// 
	// // Draw cube at end
	// // Can draw as filled square in screen space or actual 3D cube
	// float cubeSize = 8.0f; // Screen space pixels
	// drawList->AddRectFilled(
	//     screenEnd - Vector2(cubeSize/2),
	//     screenEnd + Vector2(cubeSize/2),
	//     RED
	// );
}

Zenith_Maths::Vector3 Zenith_Gizmo::ScreenToWorldRay(
	const Zenith_Maths::Vector2& mousePos,
	const Zenith_Maths::Vector2& viewportPos,
	const Zenith_Maths::Vector2& viewportSize,
	const Zenith_Maths::Matrix4& viewMatrix,
	const Zenith_Maths::Matrix4& projMatrix)
{
	// STEP 1: Normalize mouse position to NDC (Normalized Device Coordinates)
	// NDC range is [-1, 1] for both X and Y
	float x = (mousePos.x / viewportSize.x) * 2.0f - 1.0f;
	float y = (mousePos.y / viewportSize.y) * 2.0f - 1.0f;

	// Invert Y for Vulkan coordinate system (top-left origin)
	y = -y;

	// STEP 2: Create clip space coordinates (near plane)
	Zenith_Maths::Vector4 rayClip(x, y, -1.0f, 1.0f);

	// STEP 3: Transform to view/eye space
	Zenith_Maths::Matrix4 invProj = glm::inverse(projMatrix);
	Zenith_Maths::Vector4 rayEye = invProj * rayClip;

	// Convert to direction vector (not a point)
	rayEye = Zenith_Maths::Vector4(rayEye.x, rayEye.y, -1.0f, 0.0f);

	// STEP 4: Transform to world space
	Zenith_Maths::Matrix4 invView = glm::inverse(viewMatrix);
	Zenith_Maths::Vector4 rayWorld4 = invView * rayEye;

	// STEP 5: Extract and normalize direction
	Zenith_Maths::Vector3 rayDir = Zenith_Maths::Vector3(rayWorld4.x, rayWorld4.y, rayWorld4.z);
	rayDir = glm::normalize(rayDir);

	return rayDir;
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
