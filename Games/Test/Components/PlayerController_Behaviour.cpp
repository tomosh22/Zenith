#include "Zenith.h"

#include "Test/Components/PlayerController_Behaviour.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Input/Zenith_Input.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Zenith_Entity.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"

DEBUGVAR float dbg_fCamDistance = 25.f;

static Zenith_Entity s_axBulletEntities[128];
static u_int s_uCurrentBulletIndex = 0;

// Cached assets for bullet spawning (loaded once on first use)
static Flux_MeshGeometry* s_pxBulletMesh = nullptr;
static Flux_Material* s_pxBulletMaterial = nullptr;

PlayerController_Behaviour::PlayerController_Behaviour(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
	Zenith_Assert(m_xParentEntity.HasComponent<Zenith_ColliderComponent>(), "");

#ifdef ZENITH_DEBUG_VARIABLES
	static bool ls_bInit = false;
	if (!ls_bInit)
	{
		ls_bInit = true;
		Zenith_DebugVariables::AddFloat({ "PlayerController", "Camera Distance" }, dbg_fCamDistance, 0, 50);
	}
#endif
}

static void UpdateCameraRotation(Zenith_CameraComponent& xCamera)
{
	static Zenith_Maths::Vector2_64 s_xPreviousMousePos = { FLT_MAX,FLT_MAX };
	Zenith_Maths::Vector2_64 xCurrentMousePos;
	Zenith_Input::GetMousePosition(xCurrentMousePos);
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_Q))
	{
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
	}
	s_xPreviousMousePos = xCurrentMousePos;
}

void PlayerController_Behaviour::Shoot()
{
	// Load bullet assets once on first use
	if (!s_pxBulletMesh)
	{
		s_pxBulletMesh = Zenith_AssetHandler::AddMeshFromFile(ASSETS_ROOT"Meshes/sphereSmooth_Mesh0_Mat0.zmsh");
		s_pxBulletMaterial = Zenith_AssetHandler::AddMaterial();
	}

	Zenith_Entity& xBulletEntity = s_axBulletEntities[s_uCurrentBulletIndex];
	xBulletEntity.Initialise(&Zenith_Scene::GetCurrentScene(),"Bullet" + std::to_string(s_uCurrentBulletIndex));
	s_uCurrentBulletIndex++;

	Zenith_ModelComponent& xModel = xBulletEntity.AddComponent<Zenith_ModelComponent>();
	xModel.AddMeshEntry(*s_pxBulletMesh, *s_pxBulletMaterial);

	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

	Zenith_Maths::Vector3 xFacingDir;
	xCamera.GetFacingDir(xFacingDir);

	Zenith_TransformComponent& xTrans = xBulletEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Vector3 xPlayerPos;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPlayerPos);
	xFacingDir *= 10; //#TO_TODO: why isn't the * operator overload working?
	xTrans.SetPosition(xPlayerPos + Zenith_Maths::Vector3(0,7,0) + xFacingDir);
	xFacingDir /= 10;
	xTrans.SetScale({ 1,1,1 });

	//#TO I'm being lazy and using this as initial velocity
	xFacingDir *= 50;

	Zenith_ColliderComponent& xCollider = xBulletEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);
	Zenith_Physics::SetLinearVelocity(xCollider.GetRigidBody(), xFacingDir);
}

void PlayerController_Behaviour::OnUpdate(const float fDt)
{
	Zenith_TransformComponent& xTrans = m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_CameraComponent& xCamera = m_xParentEntity.GetComponent<Zenith_CameraComponent>();

	UpdateCameraRotation(xCamera);

	//#TO i don't think i need to multiply by fDt? physics update should handle frame rate inconsistencies right?
	const double dMoveSpeed = s_dMoveSpeed;

	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_C))
	{
		m_bFlyCamEnabled = !m_bFlyCamEnabled;
	}

	if (m_bFlyCamEnabled)
	{
		Zenith_Maths::Vector3 xFinalVelocity(0,0,0);

		if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
			xFinalVelocity += Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
			xFinalVelocity -= Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
			xFinalVelocity += Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
		{
			Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
			Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
			xFinalVelocity -= Zenith_Maths::Vector3(xResult);
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
		{
			Zenith_Maths::Vector3_64 xUp = xCamera.GetUpDir();

			xFinalVelocity -= xUp * dMoveSpeed;
		}
		if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
		{
			Zenith_Maths::Vector3_64 xUp = xCamera.GetUpDir();

			xFinalVelocity += xUp * dMoveSpeed;
		}

		Zenith_Maths::Vector3 xPos;
		xCamera.GetPosition(xPos);
		xFinalVelocity *= Zenith_Maths::Vector3(Zenith_Core::GetDt());
		xPos += xFinalVelocity;
		xCamera.SetPosition(xPos);

		return;
	}

	

	Zenith_Maths::Vector3 xFinalVelocity(0, Zenith_Physics::GetLinearVelocity(xTrans.m_pxRigidBody).y, 0);

	if (Zenith_Input::IsKeyDown(ZENITH_KEY_W))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_S))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(0, 0, 1, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_A))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity += Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_D))
	{
		Zenith_Maths::Matrix4_64 xRotation = glm::rotate(-xCamera.GetYaw(), Zenith_Maths::Vector3_64(0, 1, 0));
		Zenith_Maths::Vector4_64 xResult = xRotation * Zenith_Maths::Vector4(-1, 0, 0, 1) * dMoveSpeed;
		xFinalVelocity -= Zenith_Maths::Vector3(xResult);
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_LEFT_SHIFT))
	{
		xFinalVelocity.y = -dMoveSpeed / 10;
	}
	if (Zenith_Input::IsKeyDown(ZENITH_KEY_SPACE))
	{
		xFinalVelocity.y = dMoveSpeed / 10;
	}

	Zenith_Physics::SetLinearVelocity(xTrans.m_pxRigidBody, xFinalVelocity);
	xTrans.SetRotation(Zenith_Maths::EulerRotationToMatrix4(-xCamera.GetYaw() * Zenith_Maths::RadToDeg, { 0,1,0 }));

	Zenith_Maths::Vector3 xOffsetXZ = Zenith_Maths::Vector3(sinf(xCamera.GetYaw()), 0, -cosf(xCamera.GetYaw())) * dbg_fCamDistance;
	Zenith_Maths::Vector3 xPos;
	xTrans.GetPosition(xPos);
	xCamera.SetPosition(xPos + Zenith_Maths::Vector3(0, 20, 0) + xOffsetXZ);

	if (Zenith_Input::WasKeyPressedThisFrame(ZENITH_KEY_E))
	{
		Shoot();
	}
}