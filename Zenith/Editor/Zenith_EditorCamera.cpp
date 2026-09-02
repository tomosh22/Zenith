#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_SelectionSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "Input/Zenith_Input.h"
#include "Windows/Zenith_Windows_Window.h"

//==============================================================================
// Editor Camera System
//
// The standalone camera the editor looks through while the game is stopped or
// paused. It is separate from the scene's entities and navigates the way Unity
// and Unreal users expect:
//
//   RMB (hold)      mouse look + WASD / QE fly; cursor captured; wheel = speed
//   Alt + LMB       orbit the pivot (the selection, or the last focus point)
//   MMB drag        pan across the view plane
//   Wheel           dolly towards the pivot
//   F               fly smoothly to frame the selection
//   Shift           3x speed while flying
//==============================================================================

// Default camera values
static constexpr Zenith_Maths::Vector3 xINITIAL_EDITOR_CAMERA_POSITION = { 0, 100, 0 };
static constexpr float xINITIAL_EDITOR_CAMERA_PITCH = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_YAW = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FOV = 45.f;
static constexpr float xINITIAL_EDITOR_CAMERA_NEAR = 1.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FAR = 2000.f;

namespace
{
	constexpr float fFOCUS_DURATION_SECONDS = 0.28f;
	constexpr float fMIN_MOVE_SPEED = 0.5f;
	constexpr float fMAX_MOVE_SPEED = 5000.0f;
	constexpr float fMIN_PIVOT_DISTANCE = 0.25f;
	constexpr double fPITCH_LIMIT = glm::pi<double>() / 2.0 - 0.001;

	// A single frame must never be able to spin the camera. Real pointer travel
	// tops out at a few hundred pixels per frame, so this only ever catches a
	// NON-MOVEMENT delta: a cursor warp, a cursor-mode switch, or a hitch that
	// batched many packets into one frame. The movement path clamps dt for
	// exactly the same reason.
	constexpr double fMAX_LOOK_PIXELS_PER_FRAME = 1000.0;

	// Direction helpers. The view matrix is Rx(pitch) * Ry(yaw) * T(-pos) with
	// +Z forward in view space, so a world direction is Ry(-yaw) * Rx(-pitch) * v.
	Zenith_Maths::Vector3 RotateByCamera(double fYaw, double fPitch, const Zenith_Maths::Vector3& xLocal)
	{
		const Zenith_Maths::Matrix4_64 xYaw = glm::rotate(-fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		const Zenith_Maths::Matrix4_64 xPitch = glm::rotate(-fPitch, Zenith_Maths::Vector3_64(1, 0, 0));
		const Zenith_Maths::Vector4_64 xResult = xYaw * xPitch * Zenith_Maths::Vector4_64(xLocal.x, xLocal.y, xLocal.z, 0.0);
		return Zenith_Maths::Vector3(xResult);
	}

	void WrapYaw(double& fYaw)
	{
		constexpr double f2Pi = Zenith_Maths::Pi * 2.0;
		if (fYaw < 0.0)  fYaw += f2Pi;
		if (fYaw > f2Pi) fYaw -= f2Pi;
	}

	float SmoothStep(float fT)
	{
		fT = std::clamp(fT, 0.0f, 1.0f);
		return fT * fT * (3.0f - 2.0f * fT);
	}
}

//------------------------------------------------------------------------------
// ResetEditorCameraToDefaults
//------------------------------------------------------------------------------
void Zenith_Editor::ResetEditorCameraToDefaults()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	xCamera.m_xPosition = xINITIAL_EDITOR_CAMERA_POSITION;
	xCamera.m_fPitch = xINITIAL_EDITOR_CAMERA_PITCH;
	xCamera.m_fYaw = xINITIAL_EDITOR_CAMERA_YAW;
	xCamera.m_fFOV = xINITIAL_EDITOR_CAMERA_FOV;
	xCamera.m_fNear = xINITIAL_EDITOR_CAMERA_NEAR;
	xCamera.m_fFar = xINITIAL_EDITOR_CAMERA_FAR;
	xCamera.m_bInitialized = false;
	xCamera.m_bFocusAnimating = false;
	xCamera.m_xPivot = Zenith_Maths::Vector3(0.0f);
	xCamera.m_fPivotDistance = 20.0f;
	EndCameraGestures();
}

//------------------------------------------------------------------------------
// InitializeEditorCamera
//------------------------------------------------------------------------------
namespace
{
	// Copy the game camera's position/orientation onto the editor camera
	// state. Returns false if the entity is missing or has no camera.
	bool CopyGameCameraToEditorCamera(Zenith_SceneData* pxSceneData, Zenith_EntityID uCameraEntity, Zenith_EditorCameraState& xCamera)
	{
		Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(uCameraEntity);
		if (!xCameraEntity.IsValid())
			return false;
		Zenith_CameraComponent* pxGameCamera = xCameraEntity.TryGetComponent<Zenith_CameraComponent>();
		if (pxGameCamera == nullptr)
			return false;

		Zenith_CameraComponent& xGameCamera = *pxGameCamera;
		xGameCamera.GetPosition(xCamera.m_xPosition);
		xCamera.m_fPitch = xGameCamera.GetPitch();
		xCamera.m_fYaw = xGameCamera.GetYaw();
		return true;
	}
}

void Zenith_Editor::InitializeEditorCamera()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (xCamera.m_bInitialized)
		return;

	// Initialise from the scene's main camera if available, otherwise keep the
	// defaults. One-shot per camera-state reset: m_bInitialized is set
	// regardless, so a scene with no main camera doesn't retry every frame.
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
	{
		if (CopyGameCameraToEditorCamera(pxSceneData, pxSceneData->GetMainCameraEntity(), xCamera))
		{
			// Save reference to game camera for later
			xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

			Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera synced from game camera at (%.1f, %.1f, %.1f)",
				xCamera.m_xPosition.x, xCamera.m_xPosition.y, xCamera.m_xPosition.z);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Could not sync editor camera from game camera");
		}
	}

	// The pivot starts a comfortable distance ahead of the camera.
	xCamera.m_xPivot = xCamera.m_xPosition + GetEditorCameraForward() * xCamera.m_fPivotDistance;
	xCamera.m_bInitialized = true;
}

//------------------------------------------------------------------------------
// Direction queries
//------------------------------------------------------------------------------
Zenith_Maths::Vector3 Zenith_Editor::GetEditorCameraForward() const
{
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	return RotateByCamera(xCamera.m_fYaw, xCamera.m_fPitch, Zenith_Maths::Vector3(0, 0, 1));
}

Zenith_Maths::Vector3 Zenith_Editor::GetEditorCameraRight() const
{
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	return RotateByCamera(xCamera.m_fYaw, 0.0, Zenith_Maths::Vector3(1, 0, 0));
}

Zenith_Maths::Vector3 Zenith_Editor::GetEditorCameraUp() const
{
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	return RotateByCamera(xCamera.m_fYaw, xCamera.m_fPitch, Zenith_Maths::Vector3(0, 1, 0));
}

Zenith_Maths::Vector3 Zenith_Editor::GetCameraPlacementPoint() const
{
	// New entities land where the user is looking: at the pivot if it is in
	// front of the camera, else a fixed distance down the view direction.
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	const Zenith_Maths::Vector3 xForward = GetEditorCameraForward();
	const Zenith_Maths::Vector3 xToPivot = xCamera.m_xPivot - xCamera.m_xPosition;
	if (glm::dot(xToPivot, xForward) > 0.5f && glm::length(xToPivot) < 500.0f)
	{
		return xCamera.m_xPivot;
	}
	return xCamera.m_xPosition + xForward * 10.0f;
}

bool Zenith_Editor::IsCameraNavigating() const
{
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	return xCamera.m_bLooking || xCamera.m_bOrbiting || xCamera.m_bPanning;
}

//------------------------------------------------------------------------------
// Focus
//------------------------------------------------------------------------------
void Zenith_Editor::FocusCameraOn(const Zenith_Maths::Vector3& xCentre, float fRadius)
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	fRadius = glm::max(fRadius, 0.5f);
	// Distance at which a sphere of fRadius fills ~80% of the vertical FOV.
	const float fHalfFov = glm::radians(xCamera.m_fFOV) * 0.5f;
	const float fDistance = (fRadius / sinf(fHalfFov)) * 1.15f;

	xCamera.m_xPivot = xCentre;
	xCamera.m_fPivotDistance = fDistance;
	xCamera.m_xFocusStart = xCamera.m_xPosition;
	xCamera.m_xFocusEnd = xCentre - GetEditorCameraForward() * fDistance;
	xCamera.m_fFocusT = 0.0f;
	xCamera.m_bFocusAnimating = true;
}

void Zenith_Editor::FocusCameraOnSelection()
{
	if (!HasSelection())
	{
		return;
	}
	Zenith_Maths::Vector3 xMin(std::numeric_limits<float>::max());
	Zenith_Maths::Vector3 xMax(std::numeric_limits<float>::lowest());
	bool bAny = false;
	for (const Zenith_EntityID& xID : GetSelectedEntityIDs())
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
		if (!xEntity.IsValid())
		{
			continue;
		}
		const BoundingBox xBox = g_xEngine.Selection().GetEntityBoundingBox(&xEntity);
		if (xBox.m_xMin.x > xBox.m_xMax.x)
		{
			continue;
		}
		xMin = glm::min(xMin, xBox.m_xMin);
		xMax = glm::max(xMax, xBox.m_xMax);
		bAny = true;
	}
	if (!bAny)
	{
		return;
	}
	const Zenith_Maths::Vector3 xCentre = (xMin + xMax) * 0.5f;
	const float fRadius = glm::length(xMax - xMin) * 0.5f;
	FocusCameraOn(xCentre, fRadius);
}

void Zenith_Editor::RefreshCameraPivotFromSelection()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (IsCameraNavigating() || !HasSelection())
	{
		return;
	}
	Zenith_Entity* pxEntity = GetSelectedEntity();
	if (pxEntity == nullptr)
	{
		return;
	}
	const BoundingBox xBox = g_xEngine.Selection().GetEntityBoundingBox(pxEntity);
	if (xBox.m_xMin.x > xBox.m_xMax.x)
	{
		return;
	}
	xCamera.m_xPivot = (xBox.m_xMin + xBox.m_xMax) * 0.5f;
	xCamera.m_fPivotDistance = glm::max(glm::length(xCamera.m_xPivot - xCamera.m_xPosition), fMIN_PIVOT_DISTANCE);
}

//------------------------------------------------------------------------------
// Gestures
//------------------------------------------------------------------------------
void Zenith_Editor::EndCameraGestures()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	xCamera.m_bLooking = false;
	xCamera.m_bOrbiting = false;
	xCamera.m_bPanning = false;
}

// Yaw/pitch from this frame's mouse delta (radians, same convention as
// Zenith_CameraComponent); shared by the look and orbit gestures.
//
// The delta is the OS-processed pointer movement in SCREEN PIXELS — the same
// thing every other mouse-driven part of the editor sees. The editor
// deliberately does NOT capture the cursor for this: capturing turns on raw
// device motion, which changes the unit to unbounded, unaccelerated device
// counts. That reads as a camera that spins far too fast, drifts in pitch on a
// horizontal sweep (no desktop edge to bound it, no pointer curve to damp it)
// and eventually stares at the sky. Bounded pointer movement is what makes this
// legible; the price is that a sweep stops at the edge of the screen.
void Zenith_Editor::ApplyMouseLookDelta()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	Zenith_Maths::Vector2_64 xMouseDelta;
	g_xEngine.Input().GetMouseDelta(xMouseDelta);

	const double fDeltaX = std::clamp(xMouseDelta.x, -fMAX_LOOK_PIXELS_PER_FRAME, fMAX_LOOK_PIXELS_PER_FRAME);
	const double fDeltaY = std::clamp(xMouseDelta.y, -fMAX_LOOK_PIXELS_PER_FRAME, fMAX_LOOK_PIXELS_PER_FRAME);

	const double fRadiansPerPixel = glm::radians(m_xEditorState.m_xPrefs.m_fLookSensitivity);
	xCamera.m_fYaw   -= fDeltaX * fRadiansPerPixel;
	xCamera.m_fPitch -= fDeltaY * fRadiansPerPixel;
	xCamera.m_fPitch = std::clamp(xCamera.m_fPitch, -fPITCH_LIMIT, fPITCH_LIMIT);
	WrapYaw(xCamera.m_fYaw);
}

void Zenith_Editor::UpdateEditorCameraLook()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	ApplyMouseLookDelta();

	// Wheel while looking adjusts the fly speed (multiplicative, like Unreal).
	const float fWheel = g_xEngine.Input().GetMouseWheelDelta();
	if (fWheel != 0.0f)
	{
		xCamera.m_fMoveSpeed = std::clamp(xCamera.m_fMoveSpeed * powf(1.2f, fWheel), fMIN_MOVE_SPEED, fMAX_MOVE_SPEED);
		m_xEditorState.m_xPrefs.m_fCameraMoveSpeed = xCamera.m_fMoveSpeed;
	}
}

void Zenith_Editor::UpdateEditorCameraMovement(float fDt)
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	Zenith_Input& xInput = g_xEngine.Input();

	float fMoveSpeed = xCamera.m_fMoveSpeed;
	if (xInput.IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		fMoveSpeed *= 3.0f;

	const float fStep = fMoveSpeed * fDt;
	Zenith_Maths::Vector3 xMove(0.0f);
	if (xInput.IsKeyDown(ZENITH_KEY_W)) xMove += GetEditorCameraForward();
	if (xInput.IsKeyDown(ZENITH_KEY_S)) xMove -= GetEditorCameraForward();
	if (xInput.IsKeyDown(ZENITH_KEY_D)) xMove += GetEditorCameraRight();
	if (xInput.IsKeyDown(ZENITH_KEY_A)) xMove -= GetEditorCameraRight();
	if (xInput.IsKeyDown(ZENITH_KEY_E)) xMove.y += 1.0f;
	if (xInput.IsKeyDown(ZENITH_KEY_Q)) xMove.y -= 1.0f;

	if (glm::length(xMove) > 0.0f)
	{
		const Zenith_Maths::Vector3 xDelta = glm::normalize(xMove) * fStep;
		xCamera.m_xPosition += xDelta;
		// Flying carries the pivot along so a later orbit turns around what is
		// in front of the camera, not a point left far behind.
		xCamera.m_xPivot += xDelta;
	}
}

void Zenith_Editor::UpdateEditorCameraOrbit()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	ApplyMouseLookDelta();
	xCamera.m_xPosition = xCamera.m_xPivot - GetEditorCameraForward() * xCamera.m_fPivotDistance;
}

void Zenith_Editor::UpdateEditorCameraPan()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	Zenith_Maths::Vector2_64 xMouseDelta;
	g_xEngine.Input().GetMouseDelta(xMouseDelta);

	// World units per screen pixel at the pivot's depth, so the point under the
	// cursor stays under the cursor.
	const float fViewportHeight = glm::max(m_xEditorState.m_xViewport.m_xSize.y, 1.0f);
	const float fUnitsPerPixel = 2.0f * xCamera.m_fPivotDistance * tanf(glm::radians(xCamera.m_fFOV) * 0.5f) / fViewportHeight;
	const Zenith_Maths::Vector3 xDelta =
		GetEditorCameraRight() * static_cast<float>(-xMouseDelta.x * fUnitsPerPixel) +
		GetEditorCameraUp() * static_cast<float>(xMouseDelta.y * fUnitsPerPixel);
	xCamera.m_xPosition += xDelta;
	xCamera.m_xPivot += xDelta;
}

void Zenith_Editor::UpdateEditorCameraDolly(float fWheel)
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	// Proportional to the pivot distance: fine near the subject, fast far away.
	const float fStep = glm::max(xCamera.m_fPivotDistance * 0.18f, 0.05f) * fWheel;
	const float fNewDistance = glm::max(xCamera.m_fPivotDistance - fStep, fMIN_PIVOT_DISTANCE);
	const float fApplied = xCamera.m_fPivotDistance - fNewDistance;
	xCamera.m_xPosition += GetEditorCameraForward() * fApplied;
	xCamera.m_fPivotDistance = fNewDistance;
}

void Zenith_Editor::AdvanceCameraFocus(float fDt)
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (!xCamera.m_bFocusAnimating)
	{
		return;
	}
	xCamera.m_fFocusT += fDt / fFOCUS_DURATION_SECONDS;
	const float fT = SmoothStep(xCamera.m_fFocusT);
	xCamera.m_xPosition = xCamera.m_xFocusStart + (xCamera.m_xFocusEnd - xCamera.m_xFocusStart) * fT;
	if (xCamera.m_fFocusT >= 1.0f)
	{
		xCamera.m_xPosition = xCamera.m_xFocusEnd;
		xCamera.m_bFocusAnimating = false;
	}
}

// Decides which gesture (if any) owns this frame, starting and ending them on
// the button edges. A gesture that started over the viewport keeps ownership
// until its button is released, even if the cursor leaves the panel.
void Zenith_Editor::UpdateEditorCameraGestures()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	Zenith_Input& xInput = g_xEngine.Input();
	const bool bHovered = m_xEditorState.m_xViewport.m_bHovered;
	const bool bRMB = xInput.IsKeyDown(ZENITH_MOUSE_BUTTON_2);
	const bool bMMB = xInput.IsKeyDown(ZENITH_MOUSE_BUTTON_3);
	const bool bLMB = xInput.IsKeyDown(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bAlt = xInput.IsKeyDown(ZENITH_KEY_LEFT_ALT) || xInput.IsKeyDown(ZENITH_KEY_RIGHT_ALT);
	xInput.GetMouseDelta(xCamera.m_xLastMouseDelta);

	// End gestures on release.
	if (xCamera.m_bLooking && !bRMB)
	{
		EndCameraGestures();
		m_xEditorState.m_xPrefs.Save();   // the wheel may have changed move speed
	}
	if (xCamera.m_bOrbiting && !(bLMB && bAlt)) xCamera.m_bOrbiting = false;
	if (xCamera.m_bPanning && !bMMB)            xCamera.m_bPanning = false;

	// Start gestures on press over the viewport (one at a time).
	if (!IsCameraNavigating() && bHovered)
	{
		if (bRMB && xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_2))
		{
			xCamera.m_bLooking = true;
			xCamera.m_bFocusAnimating = false;
		}
		else if (bAlt && bLMB && xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT))
		{
			xCamera.m_bOrbiting = true;
			xCamera.m_bFocusAnimating = false;
			xCamera.m_fPivotDistance = glm::max(glm::length(xCamera.m_xPivot - xCamera.m_xPosition), fMIN_PIVOT_DISTANCE);
		}
		else if (bMMB && xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_3))
		{
			xCamera.m_bPanning = true;
			xCamera.m_bFocusAnimating = false;
		}
	}

}

//------------------------------------------------------------------------------
// UpdateEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::UpdateEditorCamera(float fDt)
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (!xCamera.m_bInitialized)  return;
	if (m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		EndCameraGestures();
		return;  // Stopped/Paused only.
	}

	// A hitch (asset load, alt-tab) must not fling the camera.
	fDt = std::clamp(fDt, 0.0f, 0.1f);

	AdvanceCameraFocus(fDt);
	UpdateEditorCameraGestures();

	if (xCamera.m_bLooking)
	{
		UpdateEditorCameraLook();
		UpdateEditorCameraMovement(fDt);
	}
	else if (xCamera.m_bOrbiting)
	{
		UpdateEditorCameraOrbit();
	}
	else if (xCamera.m_bPanning)
	{
		UpdateEditorCameraPan();
	}
	else if (m_xEditorState.m_xViewport.m_bHovered)
	{
		const float fWheel = g_xEngine.Input().GetMouseWheelDelta();
		if (fWheel != 0.0f)
		{
			xCamera.m_bFocusAnimating = false;
			UpdateEditorCameraDolly(fWheel);
		}
	}

	ApplyEditorCameraToScene();
}

void Zenith_Editor::ApplyEditorCameraToScene()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
		return;

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (!pxSceneData)
		return;

	Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(xCamera.m_uGameCameraEntity);
	if (!xCameraEntity.IsValid())
		return;
	Zenith_CameraComponent* pxCamera = xCameraEntity.TryGetComponent<Zenith_CameraComponent>();
	if (pxCamera == nullptr)
		return;

	pxCamera->SetPosition(xCamera.m_xPosition);
	pxCamera->SetPitch   (xCamera.m_fPitch);
	pxCamera->SetYaw     (xCamera.m_fYaw);
}

//------------------------------------------------------------------------------
// SwitchToEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::SwitchToEditorCamera()
{
	Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	if (!xCamera.m_bInitialized)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - not initialized");
		return;
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - no active scene");
		return;
	}

	// Save the game's current main camera entity
	xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

	// Copy game camera state to editor camera
	if (xCamera.m_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		if (!CopyGameCameraToEditorCamera(pxSceneData, xCamera.m_uGameCameraEntity, xCamera))
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Could not copy game camera state to editor camera");
		}
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "Switched to editor camera");
}

//------------------------------------------------------------------------------
// SwitchToGameCamera
//------------------------------------------------------------------------------
void Zenith_Editor::SwitchToGameCamera()
{
	if (m_xEditorState.m_xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to game camera - no game camera saved");
		return;
	}

	// Game camera is already the main camera in the scene
	// We just stop applying editor camera overrides
	Zenith_Log(LOG_CATEGORY_EDITOR, "Switched to game camera");
}

//------------------------------------------------------------------------------
// Matrices and camera queries. In Playing mode every one of these defers to the
// scene's main camera (the game controls it); otherwise the editor state is
// the camera.
//------------------------------------------------------------------------------
namespace
{
	Zenith_SceneData* PlayingSceneWithCamera(EditorMode eMode)
	{
		if (eMode != EditorMode::Playing)
		{
			return nullptr;
		}
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		return (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID) ? pxSceneData : nullptr;
	}
}

void Zenith_Editor::BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		Zenith_GetMainCamera(pxSceneData).BuildViewMatrix(xOutMatrix);
		return;
	}

	// Same approach as Zenith_CameraComponent for consistency
	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(xCamera.m_fPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(xCamera.m_fYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-xCamera.m_xPosition);
	xOutMatrix = xPitchMat * xYawMat * xTransMat;
}

void Zenith_Editor::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	Zenith_Assert(m_xEditorState.m_eEditorMode != EditorMode::Playing, "Should be going through scene camera if we are in playing mode");

	const Zenith_EditorCameraState& xCamera = m_xEditorState.m_xCamera;
	float fAspectRatio = m_xEditorState.m_xViewport.m_xSize.x / m_xEditorState.m_xViewport.m_xSize.y;
	xOutMatrix = glm::perspective(glm::radians(xCamera.m_fFOV), fAspectRatio, xCamera.m_fNear, xCamera.m_fFar);
	// Flip Y for Vulkan coordinate system (same as CameraComponent)
	xOutMatrix[1][1] *= -1;
}

void Zenith_Editor::GetCameraPosition(Zenith_Maths::Vector4& xOutPosition)
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		Zenith_GetMainCamera(pxSceneData).GetPosition(xOutPosition);
		return;
	}
	const Zenith_Maths::Vector3& xPos = m_xEditorState.m_xCamera.m_xPosition;
	xOutPosition = Zenith_Maths::Vector4(xPos.x, xPos.y, xPos.z, 0.0f);
}

float Zenith_Editor::GetCameraNearPlane()
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		return Zenith_GetMainCamera(pxSceneData).GetNearPlane();
	}
	return m_xEditorState.m_xCamera.m_fNear;
}

float Zenith_Editor::GetCameraFarPlane()
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		return Zenith_GetMainCamera(pxSceneData).GetFarPlane();
	}
	return m_xEditorState.m_xCamera.m_fFar;
}

float Zenith_Editor::GetCameraFOV()
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		return Zenith_GetMainCamera(pxSceneData).GetFOV();
	}
	return m_xEditorState.m_xCamera.m_fFOV;
}

float Zenith_Editor::GetCameraAspectRatio()
{
	if (Zenith_SceneData* pxSceneData = PlayingSceneWithCamera(m_xEditorState.m_eEditorMode))
	{
		return Zenith_GetMainCamera(pxSceneData).GetAspectRatio();
	}
	return m_xEditorState.m_xViewport.m_xSize.x / m_xEditorState.m_xViewport.m_xSize.y;
}

#endif // ZENITH_TOOLS
