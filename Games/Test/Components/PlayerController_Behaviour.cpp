#include "Zenith.h"

#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}


void UpdateCameraRotation(const float fDt, Zenith_CameraComponent& xCamera)
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
		xCamera.SetPitch(xCamera.GetPitch() - dDeltaPitch);
		double dDeltaYaw = (xCurrentMousePos.x - s_xPreviousMousePos.x) / 1000.;
		xCamera.SetYaw(xCamera.GetYaw() - dDeltaYaw);

		xCamera.SetPitch(std::min(xCamera.GetPitch(), glm::pi<double>() / 2));
		xCamera.SetPitch(std::max(xCamera.GetPitch(), -glm::pi<double>() / 2));

		if (xCamera.GetYaw() < 0)
		{
			xCamera.SetYaw(xCamera.GetYaw() + Zenith_Maths::Pi * 2.0);
		}
		if (xCamera.GetYaw() > Zenith_Maths::Pi * 2.0)
		{
			xCamera.SetYaw(xCamera.GetYaw() - Zenith_Maths::Pi * 2.0);
		}
	}

	s_xPreviousMousePos = xCurrentMousePos;
}

void PlayerController_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

	const double dMoveSpeed = fDt * s_dMoveSpeed;

	UpdateCameraRotation(fDt, xCamera);

	Zenith_Maths::Vector3 xPos;

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		xTrans.GetPosition(xPos);
		xPos += Zenith_Maths::Vector3(xResult);
		xTrans.SetPosition(xPos);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		xTrans.GetPosition(xPos);
		xPos -= Zenith_Maths::Vector3(xResult);
		xTrans.SetPosition(xPos);
	}

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xTrans.GetPosition(xPos);
		xPos += Zenith_Maths::Vector3(xResult);
		xTrans.SetPosition(xPos);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xTrans.GetPosition(xPos);
		xPos -= Zenith_Maths::Vector3(xResult);
		xTrans.SetPosition(xPos);
	}

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		xTrans.GetPosition(xPos);
		xPos.y -= dMoveSpeed;
		xTrans.SetPosition(xPos);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		xTrans.GetPosition(xPos);
		xPos.y += dMoveSpeed;
		xTrans.SetPosition(xPos);
	}
	xTrans.GetPosition(xPos);
	xCamera.SetPosition(xPos + Zenith_Maths::Vector3(0, 15, 0));
}