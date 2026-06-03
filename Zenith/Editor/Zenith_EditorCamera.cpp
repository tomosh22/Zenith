#include "Zenith.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "Zenith_Editor.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "Input/Zenith_Input.h"

//==============================================================================
// Editor Camera System
//
// Implements the standalone editor camera that is separate from the game's
// entity/scene system. This allows free camera movement in the editor
// while the game is stopped or paused.
//==============================================================================

// Default camera values
static constexpr Zenith_Maths::Vector3 xINITIAL_EDITOR_CAMERA_POSITION = { 0, 100, 0 };
static constexpr float xINITIAL_EDITOR_CAMERA_PITCH = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_YAW = 0.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FOV = 45.f;
static constexpr float xINITIAL_EDITOR_CAMERA_NEAR = 1.f;
static constexpr float xINITIAL_EDITOR_CAMERA_FAR = 2000.f;

// Phase 5.5c: camera state lives on Zenith_Editor held by Zenith_Engine
// with the same defaults the constants above describe.

//------------------------------------------------------------------------------
// ResetEditorCameraToDefaults
//------------------------------------------------------------------------------
void Zenith_Editor::ResetEditorCameraToDefaults()
{
	g_xEngine.Editor().m_xEditorCameraPosition = xINITIAL_EDITOR_CAMERA_POSITION;
	g_xEngine.Editor().m_fEditorCameraPitch = xINITIAL_EDITOR_CAMERA_PITCH;
	g_xEngine.Editor().m_fEditorCameraYaw = xINITIAL_EDITOR_CAMERA_YAW;
	g_xEngine.Editor().m_fEditorCameraFOV = xINITIAL_EDITOR_CAMERA_FOV;
	g_xEngine.Editor().m_fEditorCameraNear = xINITIAL_EDITOR_CAMERA_NEAR;
	g_xEngine.Editor().m_fEditorCameraFar = xINITIAL_EDITOR_CAMERA_FAR;
	g_xEngine.Editor().m_bEditorCameraInitialized = false;
}

//------------------------------------------------------------------------------
// InitializeEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::InitializeEditorCamera()
{
	if (g_xEngine.Editor().m_bEditorCameraInitialized)
		return;

	// Initialize editor camera from scene's main camera if available
	// Otherwise use default values
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
	{
		try
		{
			Zenith_CameraComponent& xSceneCamera = Zenith_GetMainCamera(pxSceneData);
			xSceneCamera.GetPosition(g_xEngine.Editor().m_xEditorCameraPosition);
			g_xEngine.Editor().m_fEditorCameraPitch = xSceneCamera.GetPitch();
			g_xEngine.Editor().m_fEditorCameraYaw = xSceneCamera.GetYaw();
			g_xEngine.Editor().m_fEditorCameraFOV = xSceneCamera.GetFOV();
			g_xEngine.Editor().m_fEditorCameraNear = xSceneCamera.GetNearPlane();
			g_xEngine.Editor().m_fEditorCameraFar = xSceneCamera.GetFarPlane();
			Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized from scene camera position");
		}
		catch (...)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene camera not available, using default position");
		}
	}

	g_xEngine.Editor().m_bEditorCameraInitialized = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized at position (%.1f, %.1f, %.1f)", g_xEngine.Editor().m_xEditorCameraPosition.x, g_xEngine.Editor().m_xEditorCameraPosition.y, g_xEngine.Editor().m_xEditorCameraPosition.z);
}

//------------------------------------------------------------------------------
// UpdateEditorCamera helpers
//------------------------------------------------------------------------------
namespace
{
	// Rotate a local-space direction by the editor camera's yaw only (keeps
	// WASD movement on the horizontal plane), then scale by speed * dt. Used
	// for W/S (forward/back) and A/D (strafe).
	Zenith_Maths::Vector3 YawDirectionScaled(double fYaw, Zenith_Maths::Vector4 xLocal, float fSpeedDt)
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * xLocal;
		return Zenith_Maths::Vector3(xResult) * fSpeedDt;
	}
}

void Zenith_Editor::UpdateEditorCameraLook()
{
	if (!g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_2))
		return;

	Zenith_Maths::Vector2_64 xMouseDelta;
	g_xEngine.Input().GetMouseDelta(xMouseDelta);

	// Yaw/pitch stored in radians (same convention as Zenith_CameraComponent).
	const double fRotateSpeedRad = glm::radians(g_xEngine.Editor().m_fEditorCameraRotateSpeed);
	g_xEngine.Editor().m_fEditorCameraYaw   -= xMouseDelta.x * fRotateSpeedRad;
	g_xEngine.Editor().m_fEditorCameraPitch -= xMouseDelta.y * fRotateSpeedRad;

	// Clamp pitch to ±π/2 to prevent gimbal flip.
	g_xEngine.Editor().m_fEditorCameraPitch = std::clamp(g_xEngine.Editor().m_fEditorCameraPitch, -glm::pi<double>() / 2.0, glm::pi<double>() / 2.0);

	// Wrap yaw into [0, 2π).
	constexpr double f2Pi = Zenith_Maths::Pi * 2.0;
	if (g_xEngine.Editor().m_fEditorCameraYaw < 0.0)  g_xEngine.Editor().m_fEditorCameraYaw += f2Pi;
	if (g_xEngine.Editor().m_fEditorCameraYaw > f2Pi) g_xEngine.Editor().m_fEditorCameraYaw -= f2Pi;
}

void Zenith_Editor::UpdateEditorCameraMovement(float fDt)
{
	if (!g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_2))
		return;

	float fMoveSpeed = g_xEngine.Editor().m_fEditorCameraMoveSpeed;
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		fMoveSpeed *= 3.0f;

	const float fStep = fMoveSpeed * fDt;
	const double fYaw = g_xEngine.Editor().m_fEditorCameraYaw;

	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_W))
		g_xEngine.Editor().m_xEditorCameraPosition += YawDirectionScaled(fYaw, {0, 0,  1, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_S))
		g_xEngine.Editor().m_xEditorCameraPosition -= YawDirectionScaled(fYaw, {0, 0,  1, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_A))
		g_xEngine.Editor().m_xEditorCameraPosition += YawDirectionScaled(fYaw, {-1, 0, 0, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_D))
		g_xEngine.Editor().m_xEditorCameraPosition -= YawDirectionScaled(fYaw, {-1, 0, 0, 1}, fStep);

	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_Q))
		g_xEngine.Editor().m_xEditorCameraPosition.y -= fStep;
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_E))
		g_xEngine.Editor().m_xEditorCameraPosition.y += fStep;
}

void Zenith_Editor::ApplyEditorCameraToScene()
{
	if (g_xEngine.Editor().m_uGameCameraEntity == INVALID_ENTITY_ID)
		return;

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (!pxSceneData)
		return;

	Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(g_xEngine.Editor().m_uGameCameraEntity);
	if (!xCameraEntity.IsValid() || !xCameraEntity.HasComponent<Zenith_CameraComponent>())
		return;

	Zenith_CameraComponent& xCamera = xCameraEntity.GetComponent<Zenith_CameraComponent>();
	xCamera.SetPosition(g_xEngine.Editor().m_xEditorCameraPosition);
	xCamera.SetPitch   (g_xEngine.Editor().m_fEditorCameraPitch);
	xCamera.SetYaw     (g_xEngine.Editor().m_fEditorCameraYaw);
}

//------------------------------------------------------------------------------
// UpdateEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::UpdateEditorCamera(float fDt)
{
	if (!g_xEngine.Editor().m_bEditorCameraInitialized)  return;
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)  return;  // Stopped/Paused only.
	if (!g_xEngine.Editor().m_bViewportHovered && !g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_2))  return;

	UpdateEditorCameraLook();
	UpdateEditorCameraMovement(fDt);
	ApplyEditorCameraToScene();
}

//------------------------------------------------------------------------------
// SwitchToEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::SwitchToEditorCamera()
{
	if (!g_xEngine.Editor().m_bEditorCameraInitialized)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - not initialized");
		return;
	}

	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - no active scene");
		return;
	}

	// Save the game's current main camera entity
	g_xEngine.Editor().m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

	// Copy game camera state to editor camera
	if (g_xEngine.Editor().m_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		Zenith_Entity xEntity = pxSceneData->TryGetEntity(g_xEngine.Editor().m_uGameCameraEntity);
		if (xEntity.IsValid() && xEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xGameCamera = xEntity.GetComponent<Zenith_CameraComponent>();
			xGameCamera.GetPosition(g_xEngine.Editor().m_xEditorCameraPosition);
			g_xEngine.Editor().m_fEditorCameraPitch = xGameCamera.GetPitch();
			g_xEngine.Editor().m_fEditorCameraYaw = xGameCamera.GetYaw();
		}
		else
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
	if (g_xEngine.Editor().m_uGameCameraEntity == INVALID_ENTITY_ID)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to game camera - no game camera saved");
		return;
	}

	// Game camera is already the main camera in the scene
	// We just stop applying editor camera overrides
	Zenith_Log(LOG_CATEGORY_EDITOR, "Switched to game camera");
}

//------------------------------------------------------------------------------
// BuildViewMatrix
//------------------------------------------------------------------------------
void Zenith_Editor::BuildViewMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			Zenith_GetMainCamera(pxSceneData).BuildViewMatrix(xOutMatrix);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), build view matrix from editor state
	// Use the same approach as Zenith_CameraComponent for consistency
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(g_xEngine.Editor().m_fEditorCameraPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(g_xEngine.Editor().m_fEditorCameraYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-g_xEngine.Editor().m_xEditorCameraPosition);
	xOutMatrix = xPitchMat * xYawMat * xTransMat;
}

//------------------------------------------------------------------------------
// BuildProjectionMatrix
//------------------------------------------------------------------------------
void Zenith_Editor::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	Zenith_Assert(g_xEngine.Editor().m_eEditorMode != EditorMode::Playing, "Should be going through scene camera if we are in playing mode");

	float fAspectRatio = g_xEngine.Editor().m_xViewportSize.x / g_xEngine.Editor().m_xViewportSize.y;
	xOutMatrix = glm::perspective(glm::radians(g_xEngine.Editor().m_fEditorCameraFOV), fAspectRatio, g_xEngine.Editor().m_fEditorCameraNear, g_xEngine.Editor().m_fEditorCameraFar);
	// Flip Y for Vulkan coordinate system (same as CameraComponent)
	xOutMatrix[1][1] *= -1;
}

//------------------------------------------------------------------------------
// GetCameraPosition
//------------------------------------------------------------------------------
void Zenith_Editor::GetCameraPosition(Zenith_Maths::Vector4& xOutPosition)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			Zenith_GetMainCamera(pxSceneData).GetPosition(xOutPosition);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor position
	xOutPosition = Zenith_Maths::Vector4(g_xEngine.Editor().m_xEditorCameraPosition.x, g_xEngine.Editor().m_xEditorCameraPosition.y, g_xEngine.Editor().m_xEditorCameraPosition.z, 0.0f);
}

//------------------------------------------------------------------------------
// GetCameraNearPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraNearPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetNearPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_fEditorCameraNear;
}

//------------------------------------------------------------------------------
// GetCameraFarPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFarPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetFarPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_fEditorCameraFar;
}

//------------------------------------------------------------------------------
// GetCameraFOV
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFOV()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetFOV();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_fEditorCameraFOV;
}

//------------------------------------------------------------------------------
// GetCameraAspectRatio
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraAspectRatio()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetAspectRatio();
		}
	}

	// In Stopped/Paused mode (or no scene camera), calculate from viewport
	return g_xEngine.Editor().m_xViewportSize.x / g_xEngine.Editor().m_xViewportSize.y;
}

#endif // ZENITH_TOOLS
