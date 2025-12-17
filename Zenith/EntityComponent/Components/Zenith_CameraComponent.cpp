#include "Zenith.h"

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"
#include "DataStream/Zenith_DataStream.h"

Zenith_CameraComponent::Zenith_CameraComponent(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_CameraComponent::InitialisePerspective(const Zenith_Maths::Vector3& xPos, const float fPitch, const float fYaw, const float fFOV, const float fNear, const float fFar, const float fAspectRatio)
{
	m_xPosition = xPos;
	m_fPitch = fPitch;
	m_fYaw = fYaw;
	m_fFOV = fFOV;
	m_fNear = fNear;
	m_fFar = fFar;
	m_fAspect = fAspectRatio;
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
		xOut = Zenith_Maths::PerspectiveProjection(m_fFOV, m_fAspect, m_fNear, m_fFar);
		xOut[1][1] *= -1;
		break;
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
	Zenith_Maths::Vector2 xScreenSize = { static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight) };

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
	xOut.z = cos(m_fYaw) * cos(m_fPitch);
	xOut.x = -sin(m_fYaw) * cos(m_fPitch);
	xOut.y = sin(m_fPitch);
	glm::normalize(xOut);
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
	xStream << m_eType;

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
	xStream >> m_eType;

	// m_xParentEntity will be set by the entity deserialization system
}
