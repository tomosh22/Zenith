#include "Zenith.h"

#include "EntityComponent/Components/Zenith_CameraBehaviour.h"
#include "Input/Zenith_Input.h"
#include "Zenith_OS_Include.h"

Zenith_CameraBehaviour::Zenith_CameraBehaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void Zenith_CameraBehaviour::InitialisePerspective(const Zenith_Maths::Vector3& xPos, const float fPitch, const float fYaw, const float fFOV, const float fNear, const float fFar, const float fAspectRatio)
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

void Zenith_CameraBehaviour::BuildViewMatrix(Zenith_Maths::Matrix4& xOut) const
{
	Zenith_Maths::Matrix4_64 xPitchMat = glm::rotate(-m_fPitch, glm::dvec3(1, 0, 0));
	Zenith_Maths::Matrix4_64 xYawMat = glm::rotate(-m_fYaw, glm::dvec3(0, 1, 0));
	Zenith_Maths::Matrix4_64 xTransMat = glm::translate(-m_xPosition);
	xOut = xPitchMat * xYawMat * xTransMat;
}

void Zenith_CameraBehaviour::BuildProjectionMatrix(Zenith_Maths::Matrix4& xOut) const
{
	switch (m_eType)
	{
	case CAMERA_TYPE_PERSPECTIVE:
		xOut = Zenith_Maths::PerspectiveProjection(m_fFOV, m_fAspect, m_fNear, m_fFar);
		break;
	case CAMERA_TYPE_ORTHOGRAPHIC:
		xOut = Zenith_Maths::OrthographicProjection(m_fLeft, m_fRight, m_fBottom, m_fTop, m_fNear, m_fFar);
		break;
	case CAMERA_TYPE_MAX:
		Zenith_Assert(false, "Camera uninitialised");
		break;
	}
}

Zenith_Maths::Vector3 Zenith_CameraBehaviour::ScreenSpaceToWorldSpace(Zenith_Maths::Vector3 xScreenSpace)
{
	Zenith_Window* pxWindow = Zenith_Window::GetInstance();
	//#TO_TODO: adjust for viewport not taking up whole window in editor mode
	Zenith_Maths::Vector2 xScreenSize = { pxWindow->GetWidth(), pxWindow->GetHeight()};

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

void Zenith_CameraBehaviour::UpdateRotation(const float fDt)
{
	static Zenith_Maths::Vector2_64 s_xPreviousMousePos = { FLT_MAX,FLT_MAX };

	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Input::GetMousePosition(xCurrentMousePos);

	if (s_xPreviousMousePos.x == FLT_MAX)
	{
		s_xPreviousMousePos = xCurrentMousePos;
		return;
	}

	//#TO_TODO: if cursor was not released this frame
	if (true)
	{
		double dDeltaPitch = (xCurrentMousePos.y - s_xPreviousMousePos.y) / 1000.;
		m_fPitch -= dDeltaPitch;
		double dDeltaYaw = (xCurrentMousePos.x - s_xPreviousMousePos.x) / 1000.;
		m_fYaw -= dDeltaYaw;

		m_fPitch = std::min(m_fPitch, glm::pi<double>() / 2);
		m_fPitch = std::max(m_fPitch, -glm::pi<double>() / 2);

		if (m_fYaw < 0)
		{
			m_fYaw += Zenith_Maths::Pi * 2.0;
		}
		if (m_fYaw > Zenith_Maths::Pi * 2.0)
		{
			m_fYaw -= Zenith_Maths::Pi * 2.0;
		}
	}

	s_xPreviousMousePos = xCurrentMousePos;
}

void Zenith_CameraBehaviour::OnUpdate(const float fDt)
{
	UpdateRotation(fDt);

	const double dMoveSpeed = fDt * s_dMoveSpeed;

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(m_fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		m_xPosition += xResult;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(m_fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		m_xPosition -= xResult;
	}

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(m_fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		m_xPosition += xResult;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(m_fYaw, Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		m_xPosition -= xResult;
	}

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		m_xPosition.y -= dMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		m_xPosition.y += dMoveSpeed;
	}
}

void Zenith_CameraBehaviour::GetPosition(Zenith_Maths::Vector3& xOut)
{
	xOut = m_xPosition;
}

void Zenith_CameraBehaviour::GetPosition(Zenith_Maths::Vector4& xOut)
{
	xOut.x = m_xPosition.x;
	xOut.y = m_xPosition.y;
	xOut.z = m_xPosition.z;
	xOut.w = 0.;
}
