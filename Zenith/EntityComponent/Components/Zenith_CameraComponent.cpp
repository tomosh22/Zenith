#include "Zenith.h"

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_ComponentMeta.h"
#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"
#include "DataStream/Zenith_DataStream.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

ZENITH_REGISTER_COMPONENT(Zenith_CameraComponent, "Camera")

Zenith_CameraComponent::Zenith_CameraComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_CameraComponent::InitialisePerspective(const PerspectiveParams& xParams)
{
	m_xPosition = xParams.m_xPosition;
	m_fPitch = xParams.m_fPitch;
	m_fYaw = xParams.m_fYaw;
	m_fFOV = xParams.m_fFOV;
	m_fNear = xParams.m_fNear;
	m_fFar = xParams.m_fFar;
	m_fAspect = xParams.m_fAspectRatio;
	m_eType = CAMERA_TYPE_PERSPECTIVE;
}

void Zenith_CameraComponent::BuildViewMatrix(Zenith_Maths::Matrix4& xOut) const
{
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(m_fPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(m_fYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-m_xPosition);
	xOut = xPitchMat * xYawMat * xTransMat;
}

void Zenith_CameraComponent::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOut) const
{
	switch (m_eType)
	{
	case CAMERA_TYPE_PERSPECTIVE:
	{
		// Clamp parameters to safe values to prevent NaN/Infinity from GLM
		constexpr float fMinAspect = 0.0001f;
		constexpr float fMinNear = 0.001f;
		constexpr float fMinFOV = 0.01f;
		constexpr float fNearFarGap = 0.1f;

		float fSafeAspect = glm::max(m_fAspect, fMinAspect);
		float fSafeNear = glm::max(m_fNear, fMinNear);
		float fSafeFar = glm::max(m_fFar, fSafeNear + fNearFarGap);
		float fSafeFOV = glm::max(m_fFOV, fMinFOV);

		xOut = Zenith_Maths::PerspectiveProjection(fSafeFOV, fSafeAspect, fSafeNear, fSafeFar);
		xOut[1][1] *= -1;
		break;
	}
	case CAMERA_TYPE_ORTHOGRAPHIC:
		xOut = Zenith_Maths::OrthographicProjection(m_fLeft, m_fRight, m_fBottom, m_fTop, m_fNear, m_fFar);
		break;
	case CAMERA_TYPE_MAX:
		Zenith_Assert(false, "Camera uninitialised");
		break;
	}
}

Zenith_Maths::Vector3 Zenith_CameraComponent::ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 xScreenSpace)
{
	Zenith_Window* pxWindow = Zenith_Window::GetInstance();
	//#TO_TODO: adjust for viewport not taking up whole window in editor mode
	int32_t iWidth, iHeight;
	pxWindow->GetSize(iWidth, iHeight);

	// Guard against zero screen size (minimized window)
	if (iWidth <= 0 || iHeight <= 0)
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	Zenith_Maths::Vector2 xScreenSize = { static_cast<float>(iWidth), static_cast<float>(iHeight) };

	Zenith_Maths::Matrix4 xViewMat;
	Zenith_Maths::Matrix4 xProjMat;
	BuildViewMatrix(xViewMat);
	BuildProjectionMatrix(xProjMat);
	Zenith_Maths::Matrix4 xInvViewProj = glm::inverse(xViewMat) * glm::inverse(xProjMat);

	Zenith_Maths::Vector4 xClipSpace = {
		(xScreenSpace.x / xScreenSize.x) * 2.0f - 1.0f,
		(xScreenSpace.y / xScreenSize.y) * 2.0f - 1.0f,
		(xScreenSpace.z),
		1.0f
	};

	Zenith_Maths::Vector4 xWorldSpacePreDivide = xInvViewProj * xClipSpace;

	// Guard against perspective division by near-zero w (degenerate matrix)
	constexpr float fMinW = 1e-6f;
	if (fabsf(xWorldSpacePreDivide.w) < fMinW)
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	Zenith_Maths::Vector3 xWorldSpace = {
		xWorldSpacePreDivide.x / xWorldSpacePreDivide.w,
		xWorldSpacePreDivide.y / xWorldSpacePreDivide.w,
		xWorldSpacePreDivide.z / xWorldSpacePreDivide.w
	};

	return xWorldSpace;
}

void Zenith_CameraComponent::GetPosition(Zenith_Maths::Vector3& xOut) const
{
	xOut = m_xPosition;
}

void Zenith_CameraComponent::GetPosition(Zenith_Maths::Vector4& xOut)
{
	xOut.x = m_xPosition.x;
	xOut.y = m_xPosition.y;
	xOut.z = m_xPosition.z;
	xOut.w = 0.;
}

void Zenith_CameraComponent::GetPosition(Zenith_Maths::Vector3& xOut)
{
	xOut.x = m_xPosition.x;
	xOut.y = m_xPosition.y;
	xOut.z = m_xPosition.z;
}

void Zenith_CameraComponent::GetFacingDir(Zenith_Maths::Vector3& xOut) const
{
	xOut.z = static_cast<float>(glm::cos(m_fYaw) * glm::cos(m_fPitch));
	xOut.x = static_cast<float>(-glm::sin(m_fYaw) * glm::cos(m_fPitch));
	xOut.y = static_cast<float>(glm::sin(m_fPitch));
	xOut = glm::normalize(xOut);  // Must assign result back
}

Zenith_Maths::Vector3 Zenith_CameraComponent::GetUpDir()
{
	Zenith_Maths::Matrix4 xYawMatrix = glm::rotate(-m_fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
	Zenith_Maths::Matrix4 xPitchMatrix = glm::rotate(-m_fPitch, Zenith_Maths::Vector3_64(1, 0, 0));
	Zenith_Maths::Matrix4 xRotationMatrix = xYawMatrix * xPitchMatrix;
	Zenith_Maths::Vector4 xUp = xRotationMatrix * Zenith_Maths::Vector4(0, 1, 0, 1);
	return {xUp.x, xUp.y, xUp.z};
}

void Zenith_CameraComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Write camera type
	xStream << static_cast<u_int>(m_eType);

	// Write all camera parameters
	xStream << m_fNear;
	xStream << m_fFar;
	xStream << m_fLeft;
	xStream << m_fRight;
	xStream << m_fTop;
	xStream << m_fBottom;
	xStream << m_fFOV;
	xStream << m_fYaw;
	xStream << m_fPitch;
	xStream << m_fAspect;
	xStream << m_xPosition;
	// Note: m_eType already written at start - do NOT write again (was causing data corruption)

	// m_xParentEntity reference is not serialized - will be restored during deserialization
}

void Zenith_CameraComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	// Read camera type
	u_int uType;
	xStream >> uType;
	m_eType = static_cast<CameraType>(uType);

	// Read all camera parameters
	xStream >> m_fNear;
	xStream >> m_fFar;
	xStream >> m_fLeft;
	xStream >> m_fRight;
	xStream >> m_fTop;
	xStream >> m_fBottom;
	xStream >> m_fFOV;
	xStream >> m_fYaw;
	xStream >> m_fPitch;
	xStream >> m_fAspect;
	xStream >> m_xPosition;
	// Note: m_eType already read at start - do NOT read again (was causing data corruption)

	// m_xParentEntity will be set by the entity deserialization system
}

#ifdef ZENITH_TOOLS
void Zenith_CameraComponent::RenderPropertiesPanel()
{
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Camera type selection
		const char* szCameraTypes[] = { "Perspective", "Orthographic" };
		int iCameraType = static_cast<int>(m_eType);
		if (ImGui::Combo("Camera Type", &iCameraType, szCameraTypes, 2))
		{
			m_eType = static_cast<CameraType>(iCameraType);
		}

		ImGui::Separator();

		// Set as Main Camera button
		Zenith_SceneData* pxSceneData = m_xParentEntity.GetSceneData();
		bool bIsMainCamera = false;
		if (pxSceneData && pxSceneData->GetMainCameraEntity() != INVALID_ENTITY_ID)
		{
			bIsMainCamera = (pxSceneData->GetMainCameraEntity() == m_xParentEntity.GetEntityID());
		}

		if (bIsMainCamera)
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "This is the Main Camera");
		}
		else
		{
			if (ImGui::Button("Set as Main Camera"))
			{
				Zenith_Editor::SetSelectedAsMainCamera();
			}
		}

		ImGui::Separator();

		// Editable camera properties for perspective camera
		if (m_eType == CAMERA_TYPE_PERSPECTIVE)
		{
			// FOV editing
			float fFOV = GetFOV();
			if (ImGui::SliderFloat("FOV", &fFOV, 30.0f, 120.0f, "%.1f"))
			{
				m_fFOV = fFOV;
			}

			// Near plane editing
			float fNear = GetNearPlane();
			if (ImGui::DragFloat("Near Plane", &fNear, 0.01f, 0.001f, 10.0f, "%.3f"))
			{
				m_fNear = fNear;
			}

			// Far plane editing
			float fFar = GetFarPlane();
			if (ImGui::DragFloat("Far Plane", &fFar, 10.0f, 10.0f, 10000.0f, "%.1f"))
			{
				m_fFar = fFar;
			}

			// Aspect ratio editing
			float fAspect = GetAspectRatio();
			if (ImGui::DragFloat("Aspect Ratio", &fAspect, 0.01f, 0.1f, 4.0f, "%.3f"))
			{
				m_fAspect = fAspect;
			}

			ImGui::Separator();

			// Pitch editing
			float fPitch = static_cast<float>(GetPitch());
			if (ImGui::DragFloat("Pitch", &fPitch, 0.01f, -1.5f, 1.5f, "%.3f rad"))
			{
				m_fPitch = fPitch;
			}

			// Yaw editing
			float fYaw = static_cast<float>(GetYaw());
			if (ImGui::DragFloat("Yaw", &fYaw, 0.01f, 0.0f, 6.28318f, "%.3f rad"))
			{
				m_fYaw = fYaw;
			}
		}
		else if (m_eType == CAMERA_TYPE_ORTHOGRAPHIC)
		{
			// Orthographic camera properties
			ImGui::DragFloat("Left", &m_fLeft, 1.0f);
			ImGui::DragFloat("Right", &m_fRight, 1.0f);
			ImGui::DragFloat("Top", &m_fTop, 1.0f);
			ImGui::DragFloat("Bottom", &m_fBottom, 1.0f);
			ImGui::DragFloat("Near", &m_fNear, 0.1f);
			ImGui::DragFloat("Far", &m_fFar, 1.0f);
		}

		ImGui::Separator();

		// Camera position editing
		// Use PushID to avoid ImGui ID collision with TransformComponent's Position field
		ImGui::PushID("CameraPosition");
		Zenith_Maths::Vector3 xPos;
		GetPosition(xPos);
		float afPos[3] = { static_cast<float>(xPos.x), static_cast<float>(xPos.y), static_cast<float>(xPos.z) };
		if (ImGui::DragFloat3("Position", afPos, 1.0f))
		{
			SetPosition({ afPos[0], afPos[1], afPos[2] });
		}
		ImGui::PopID();
	}
}
#endif
