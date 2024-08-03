#include "Zenith.h"

#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "Input/Zenith_Input.h"

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
	Zenith_Assert(m_xParentEntity.HasComponent<Zenith_ColliderComponent>(), "");
}


void UpdateCameraRotation(Zenith_CameraComponent& xCamera)
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

	//#TO i don't think i need to multiply by fDt? physics update should handle frame rate inconsistencies right?
	const double dMoveSpeed = s_dMoveSpeed;

	UpdateCameraRotation(xCamera);

	Zenith_Maths::Vector3 xFinalVelocity(0,0,0);

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, -1, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		xFinalVelocity.y -= dMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		xFinalVelocity.y += dMoveSpeed;
	}

	xTrans.m_pxRigidBody->applyWorldForceAtCenterOfMass({ xFinalVelocity.x, xFinalVelocity.y, xFinalVelocity.z });

	Zenith_Maths::Vector3 xPos;
	xTrans.GetPosition(xPos);
	xCamera.SetPosition(xPos + Zenith_Maths::Vector3(0, 15, 0));
}