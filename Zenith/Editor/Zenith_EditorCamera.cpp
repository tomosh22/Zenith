#include "Zenith.h"
#pragma warning(disable: 4530) // C++ exception handler used without /EHsc

#ifdef ZENITH_TOOLS

#include "Zenith_Editor.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
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

// Static member definitions
Zenith_Maths::Vector3 Zenith_Editor::s_xEditorCameraPosition = xINITIAL_EDITOR_CAMERA_POSITION;
double Zenith_Editor::s_fEditorCameraPitch = xINITIAL_EDITOR_CAMERA_PITCH;
double Zenith_Editor::s_fEditorCameraYaw = xINITIAL_EDITOR_CAMERA_YAW;
float Zenith_Editor::s_fEditorCameraFOV = xINITIAL_EDITOR_CAMERA_FOV;
float Zenith_Editor::s_fEditorCameraNear = xINITIAL_EDITOR_CAMERA_NEAR;
float Zenith_Editor::s_fEditorCameraFar = xINITIAL_EDITOR_CAMERA_FAR;
Zenith_EntityID Zenith_Editor::s_uGameCameraEntity = INVALID_ENTITY_ID;
float Zenith_Editor::s_fEditorCameraMoveSpeed = 50.0f;
float Zenith_Editor::s_fEditorCameraRotateSpeed = 0.1f;
bool Zenith_Editor::s_bEditorCameraInitialized = false;

//------------------------------------------------------------------------------
// ResetEditorCameraToDefaults
//------------------------------------------------------------------------------
void Zenith_Editor::ResetEditorCameraToDefaults()
{
	s_xEditorCameraPosition = xINITIAL_EDITOR_CAMERA_POSITION;
	s_fEditorCameraPitch = xINITIAL_EDITOR_CAMERA_PITCH;
	s_fEditorCameraYaw = xINITIAL_EDITOR_CAMERA_YAW;
	s_fEditorCameraFOV = xINITIAL_EDITOR_CAMERA_FOV;
	s_fEditorCameraNear = xINITIAL_EDITOR_CAMERA_NEAR;
	s_fEditorCameraFar = xINITIAL_EDITOR_CAMERA_FAR;
	s_bEditorCameraInitialized = false;
}

//------------------------------------------------------------------------------
// InitializeEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::InitializeEditorCamera()
{
	if (s_bEditorCameraInitialized)
		return;

	// Initialize editor camera from scene's main camera if available
	// Otherwise use default values
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
	{
		try
		{
			Zenith_CameraComponent& xSceneCamera = pxSceneData->GetMainCamera();
			xSceneCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xSceneCamera.GetPitch();
			s_fEditorCameraYaw = xSceneCamera.GetYaw();
			s_fEditorCameraFOV = xSceneCamera.GetFOV();
			s_fEditorCameraNear = xSceneCamera.GetNearPlane();
			s_fEditorCameraFar = xSceneCamera.GetFarPlane();
			Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized from scene camera position");
		}
		catch (...)
		{
			Zenith_Log(LOG_CATEGORY_EDITOR, "Scene camera not available, using default position");
		}
	}

	s_bEditorCameraInitialized = true;
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor camera initialized at position (%.1f, %.1f, %.1f)", s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z);
}

//------------------------------------------------------------------------------
// UpdateEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::UpdateEditorCamera(float fDt)
{
	if (!s_bEditorCameraInitialized)
		return;

	// Only update editor camera when in Stopped or Paused mode and viewport is focused
	if (s_eEditorMode == EditorMode::Playing)
		return;

	if (!s_bViewportFocused)
		return;

	// Mouse look (Right click key held for camera rotation)
	if (Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_2))
	{
		Zenith_Maths::Vector2_64 xMouseDelta;
		Zenith_Input::GetMouseDelta(xMouseDelta);

		// Update yaw and pitch (values are stored in radians, matching camera component)
		// Convert rotate speed from degrees to radians for consistency
		const double fRotateSpeedRad = glm::radians(s_fEditorCameraRotateSpeed);
		s_fEditorCameraYaw -= xMouseDelta.x * fRotateSpeedRad;
		s_fEditorCameraPitch -= xMouseDelta.y * fRotateSpeedRad;

		// Clamp pitch to prevent flipping (use radians like PlayerController_Behaviour)
		s_fEditorCameraPitch = std::min(s_fEditorCameraPitch, glm::pi<double>() / 2.0);
		s_fEditorCameraPitch = std::max(s_fEditorCameraPitch, -glm::pi<double>() / 2.0);

		// Wrap yaw around 0 to 2π (like PlayerController_Behaviour)
		if (s_fEditorCameraYaw < 0.0)
		{
			s_fEditorCameraYaw += Zenith_Maths::Pi * 2.0;
		}
		if (s_fEditorCameraYaw > Zenith_Maths::Pi * 2.0)
		{
			s_fEditorCameraYaw -= Zenith_Maths::Pi * 2.0;
		}
		// Yaw is already in radians, no conversion needed
	}

	// Speed modifier (shift = faster)
	float fMoveSpeed = s_fEditorCameraMoveSpeed;
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		fMoveSpeed *= 3.0f;

	// WASD movement (only when right click is held for FPS-style control)
	// Movement uses only yaw (not pitch) to keep movement on horizontal plane
	// This matches PlayerController_Behaviour behavior
	if (Zenith_Input::IsKeyDown(ZENITH_MOUSE_BUTTON_2))
	{
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
		{
			// Forward movement based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1);
			s_xEditorCameraPosition += Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
		{
			// Backward movement based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1);
			s_xEditorCameraPosition -= Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
		{
			// Left strafe based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1);
			s_xEditorCameraPosition += Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
		{
			// Right strafe based on yaw only (stays level)
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-s_fEditorCameraYaw, Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1);
			s_xEditorCameraPosition -= Zenith_Maths::Vector3(xResult) * fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_Q))
		{
			// Vertical down (world space)
			s_xEditorCameraPosition.y -= fMoveSpeed * fDt;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_E))
		{
			// Vertical up (world space)
			s_xEditorCameraPosition.y += fMoveSpeed * fDt;
		}
	}

	// Apply editor camera state to the scene's main camera
	// (In stopped/paused mode, the game camera is being controlled by editor values)
	if (s_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData)
		{
			Zenith_Entity xCameraEntity = pxSceneData->TryGetEntity(s_uGameCameraEntity);
			if (xCameraEntity.IsValid() && xCameraEntity.HasComponent<Zenith_CameraComponent>())
			{
				Zenith_CameraComponent& xCamera = xCameraEntity.GetComponent<Zenith_CameraComponent>();
				xCamera.SetPosition(s_xEditorCameraPosition);
				xCamera.SetPitch(s_fEditorCameraPitch);
				xCamera.SetYaw(s_fEditorCameraYaw);
			}
		}
	}
}

//------------------------------------------------------------------------------
// SwitchToEditorCamera
//------------------------------------------------------------------------------
void Zenith_Editor::SwitchToEditorCamera()
{
	if (!s_bEditorCameraInitialized)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - not initialized");
		return;
	}

	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: Cannot switch to editor camera - no active scene");
		return;
	}

	// Save the game's current main camera entity
	s_uGameCameraEntity = pxSceneData->GetMainCameraEntity();

	// Copy game camera state to editor camera
	if (s_uGameCameraEntity != INVALID_ENTITY_ID)
	{
		Zenith_Entity xEntity = pxSceneData->TryGetEntity(s_uGameCameraEntity);
		if (xEntity.IsValid() && xEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xGameCamera = xEntity.GetComponent<Zenith_CameraComponent>();
			xGameCamera.GetPosition(s_xEditorCameraPosition);
			s_fEditorCameraPitch = xGameCamera.GetPitch();
			s_fEditorCameraYaw = xGameCamera.GetYaw();
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
	if (s_uGameCameraEntity == INVALID_ENTITY_ID)
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
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			pxSceneData->GetMainCamera().BuildViewMatrix(xOutMatrix);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), build view matrix from editor state
	// Use the same approach as Zenith_CameraComponent for consistency
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(s_fEditorCameraPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(s_fEditorCameraYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-s_xEditorCameraPosition);
	xOutMatrix = xPitchMat * xYawMat * xTransMat;
}

//------------------------------------------------------------------------------
// BuildProjectionMatrix
//------------------------------------------------------------------------------
void Zenith_Editor::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOutMatrix)
{
	Zenith_Assert(s_eEditorMode != EditorMode::Playing, "Should be going through scene camera if we are in playing mode");

	float fAspectRatio = s_xViewportSize.x / s_xViewportSize.y;
	xOutMatrix = glm::perspective(glm::radians(s_fEditorCameraFOV), fAspectRatio, s_fEditorCameraNear, s_fEditorCameraFar);
	// Flip Y for Vulkan coordinate system (same as CameraComponent)
	xOutMatrix[1][1] *= -1;
}

//------------------------------------------------------------------------------
// GetCameraPosition
//------------------------------------------------------------------------------
void Zenith_Editor::GetCameraPosition(Zenith_Maths::Vector4& xOutPosition)
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			pxSceneData->GetMainCamera().GetPosition(xOutPosition);
			return;
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor position
	xOutPosition = Zenith_Maths::Vector4(s_xEditorCameraPosition.x, s_xEditorCameraPosition.y, s_xEditorCameraPosition.z, 0.0f);
}

//------------------------------------------------------------------------------
// GetCameraNearPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraNearPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return pxSceneData->GetMainCamera().GetNearPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraNear;
}

//------------------------------------------------------------------------------
// GetCameraFarPlane
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFarPlane()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return pxSceneData->GetMainCamera().GetFarPlane();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraFar;
}

//------------------------------------------------------------------------------
// GetCameraFOV
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraFOV()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return pxSceneData->GetMainCamera().GetFOV();
		}
	}

	// In Stopped/Paused mode (or no scene camera), return editor value
	return s_fEditorCameraFOV;
}

//------------------------------------------------------------------------------
// GetCameraAspectRatio
//------------------------------------------------------------------------------
float Zenith_Editor::GetCameraAspectRatio()
{
	// In Playing mode, use the scene's camera (game controls it)
	if (s_eEditorMode == EditorMode::Playing)
	{
		Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
		Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			return pxSceneData->GetMainCamera().GetAspectRatio();
		}
	}

	// In Stopped/Paused mode (or no scene camera), calculate from viewport
	return s_xViewportSize.x / s_xViewportSize.y;
}

#endif // ZENITH_TOOLS
