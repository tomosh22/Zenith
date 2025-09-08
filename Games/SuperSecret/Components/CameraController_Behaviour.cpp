#include "Zenith.h"

#include "SuperSecret/Components/CameraController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_Input.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Entity.h"

#define MOVE_SPEED 10

static Zenith_Entity s_axBulletEntities[128];
static u_int s_uCurrentBulletIndex = 0;

CameraController_Behaviour::CameraController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{

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

void CameraController_Behaviour::OnUpdate(const float fDt)
{
	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

	const float fMoveSpeed = MOVE_SPEED * fDt;

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_O))
	{
		UpdateCameraRotation(xCamera);
	}

	Zenith_Maths::Vector3 xPosDelta(0, 0, 0);

	double yaw = xCamera.GetYaw();
	Zenith_Maths::Vector3 xForward = {
		std::sin(yaw),
		0.0,
		-std::cos(yaw)
	};
	Zenith_Maths::Vector3 xRight = {
		std::cos(yaw),
		0.0,
		std::sin(yaw)
	};

	Zenith_Maths::Matrix4_64 xRotation = glm::rotate(xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		xPosDelta -= xForward * fMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		xPosDelta += xForward * fMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		xPosDelta -= xRight * fMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		xPosDelta += xRight * fMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		xPosDelta.y -= fMoveSpeed;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		xPosDelta.y += fMoveSpeed;
	}

	Zenith_Maths::Vector3 xPos;
	xCamera.GetPosition(xPos);
	xCamera.SetPosition(xPos + xPosDelta);
}