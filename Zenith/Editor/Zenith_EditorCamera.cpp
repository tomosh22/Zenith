#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

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
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition = xINITIAL_EDITOR_CAMERA_POSITION;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch = xINITIAL_EDITOR_CAMERA_PITCH;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw = xINITIAL_EDITOR_CAMERA_YAW;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFOV = xINITIAL_EDITOR_CAMERA_FOV;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fNear = xINITIAL_EDITOR_CAMERA_NEAR;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFar = xINITIAL_EDITOR_CAMERA_FAR;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_bInitialized = false;
}

//------------------------------------------------------------------------------
// InitializeEditorCamera
//------------------------------------------------------------------------------
namespace
{
	// Copy the game camera's position/orientation onto the editor camera
	// state. Returns false if the entity is missing or has no camera.
	bool CopyGameCameraToEditorCamera(Zenith_SceneData* pxSceneData, Zenith_EntityID uCameraEntity)
	{
		Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(uCameraEntity);
		if (!xCameraEntity.IsValid())
			return false;
		Zenith_CameraComponent* pxGameCamera = xCameraEntity.TryGetComponent<Zenith_CameraComponent>();
		if (pxGameCamera == nullptr)
			return false;

		Zenith_CameraComponent& xGameCamera = *pxGameCamera;
		xGameCamera.GetPosition(g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition);
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch = xGameCamera.GetPitch();
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw = xGameCamera.GetYaw();
		return true;
	}
}

void Zenith_Editor::InitializeEditorCamera()
{
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_bInitialized)
		return;

	// Initialise from the scene's main camera if available, otherwise keep the
	// defaults. One-shot per camera-state reset: m_bInitialized is set
	// regardless, so a scene with no main camera doesn't retry every frame.
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
	{
		if (CopyGameCameraToEditorCamera(pxSceneData, pxSceneData->GetMainCameraEntity()))
		{
			// Save reference to game camera for later
			g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

			Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera synced from game camera at (%.1f, %.1f, %.1f)",
				g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.x, g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.y, g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.z);
		}
		else
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Could not sync editor camera from game camera");
		}
	}

	g_xEngine.Editor().m_xEditorState.m_xCamera.m_bInitialized = true;
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
	const double fRotateSpeedRad = glm::radians(g_xEngine.Editor().m_xEditorState.m_xCamera.m_fRotateSpeed);
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw   -= xMouseDelta.x * fRotateSpeedRad;
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch -= xMouseDelta.y * fRotateSpeedRad;

	// Clamp pitch to ±π/2 to prevent gimbal flip.
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch = std::clamp(g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch, -glm::pi<double>() / 2.0, glm::pi<double>() / 2.0);

	// Wrap yaw into [0, 2π).
	constexpr double f2Pi = Zenith_Maths::Pi * 2.0;
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw < 0.0)  g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw += f2Pi;
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw > f2Pi) g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw -= f2Pi;
}

void Zenith_Editor::UpdateEditorCameraMovement(float fDt)
{
	if (!g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_2))
		return;

	float fMoveSpeed = g_xEngine.Editor().m_xEditorState.m_xCamera.m_fMoveSpeed;
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		fMoveSpeed *= 3.0f;

	const float fStep = fMoveSpeed * fDt;
	const double fYaw = g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw;

	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_W))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition += YawDirectionScaled(fYaw, {0, 0,  1, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_S))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition -= YawDirectionScaled(fYaw, {0, 0,  1, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_A))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition += YawDirectionScaled(fYaw, {-1, 0, 0, 1}, fStep);
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_D))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition -= YawDirectionScaled(fYaw, {-1, 0, 0, 1}, fStep);

	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_Q))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.y -= fStep;
	if (g_xEngine.Input().IsKeyDown(ZENITH_KEY_E))
		g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.y += fStep;
}

void Zenith_Editor::ApplyEditorCameraToScene()
{
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
		return;

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (!pxSceneData)
		return;

	Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity);
	if (!xCameraEntity.IsValid())
		return;
	Zenith_CameraComponent* pxCamera = xCameraEntity.TryGetComponent<Zenith_CameraComponent>();
	if (pxCamera == nullptr)
		return;

	Zenith_CameraComponent& xCamera = *pxCamera;
	xCamera.SetPosition(g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition);
	xCamera.SetPitch   (g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch);
	xCamera.SetYaw     (g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw);
}

//------------------------------------------------------------------------------
// UpdateEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::UpdateEditorCamera(float fDt)
{
	if (!g_xEngine.Editor().m_xEditorState.m_xCamera.m_bInitialized)  return;
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)  return;  // Stopped/Paused only.
	if (!g_xEngine.Editor().m_xEditorState.m_xViewport.m_bHovered && !g_xEngine.Input().IsKeyDown(ZENITH_MOUSE_BUTTON_2))  return;

	UpdateEditorCameraLook();
	UpdateEditorCameraMovement(fDt);
	ApplyEditorCameraToScene();
}

//------------------------------------------------------------------------------
// SwitchToEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::SwitchToEditorCamera()
{
	if (!g_xEngine.Editor().m_xEditorState.m_xCamera.m_bInitialized)
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
	g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

	// Copy game camera state to editor camera
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		if (!CopyGameCameraToEditorCamera(pxSceneData, g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity))
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
	if (g_xEngine.Editor().m_xEditorState.m_xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
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
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			Zenith_GetMainCamera(pxSceneData).BuildViewMatrix(xOutMatrix);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), build view matrix from editor state
	// Use the same approach as Zenith_CameraComponent for consistency
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(g_xEngine.Editor().m_xEditorState.m_xCamera.m_fPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(g_xEngine.Editor().m_xEditorState.m_xCamera.m_fYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition);
	xOutMatrix = xPitchMat * xYawMat * xTransMat;
}

//------------------------------------------------------------------------------
// BuildProjectionMatrix
//------------------------------------------------------------------------------
void Zenith_Editor::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	Zenith_Assert(g_xEngine.Editor().m_xEditorState.m_eEditorMode != EditorMode::Playing, "Should be going through scene camera if we are in playing mode");

	float fAspectRatio = g_xEngine.Editor().m_xEditorState.m_xViewport.m_xSize.x / g_xEngine.Editor().m_xEditorState.m_xViewport.m_xSize.y;
	xOutMatrix = glm::perspective(glm::radians(g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFOV), fAspectRatio, g_xEngine.Editor().m_xEditorState.m_xCamera.m_fNear, g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFar);
	// Flip Y for Vulkan coordinate system (same as CameraComponent)
	xOutMatrix[1][1] *= -1;
}

//------------------------------------------------------------------------------
// GetCameraPosition
//------------------------------------------------------------------------------
void Zenith_Editor::GetCameraPosition(Zenith_Maths::Vector4& xOutPosition)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			Zenith_GetMainCamera(pxSceneData).GetPosition(xOutPosition);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor position
	xOutPosition = Zenith_Maths::Vector4(g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.x, g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.y, g_xEngine.Editor().m_xEditorState.m_xCamera.m_xPosition.z, 0.0f);
}

//------------------------------------------------------------------------------
// GetCameraNearPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraNearPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetNearPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_xEditorState.m_xCamera.m_fNear;
}

//------------------------------------------------------------------------------
// GetCameraFarPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFarPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetFarPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFar;
}

//------------------------------------------------------------------------------
// GetCameraFOV
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFOV()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetFOV();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return g_xEngine.Editor().m_xEditorState.m_xCamera.m_fFOV;
}

//------------------------------------------------------------------------------
// GetCameraAspectRatio
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraAspectRatio()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (g_xEngine.Editor().m_xEditorState.m_eEditorMode == EditorMode::Playing)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return Zenith_GetMainCamera(pxSceneData).GetAspectRatio();
		}
	}

	// In Stopped/Paused mode (or no scene camera), calculate from viewport
	return g_xEngine.Editor().m_xEditorState.m_xViewport.m_xSize.x / g_xEngine.Editor().m_xEditorState.m_xViewport.m_xSize.y;
}

#endif // ZENITH_TOOLS
