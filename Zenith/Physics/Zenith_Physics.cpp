#include "Zenith.h"
#include "Physics/Zenith_Physics.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Zenith_OS_Include.h"

reactphysics3d::PhysicsCommon Zenith_Physics::s_xPhysicsCommon;
reactphysics3d::PhysicsWorld* Zenith_Physics::s_pxPhysicsWorld = nullptr;
double Zenith_Physics::s_fTimestepAccumulator = 0;
Zenith_Physics::PhysicsEventListener Zenith_Physics::s_xEventListener;

void Zenith_Physics::Initialise()
{
	s_pxPhysicsWorld = s_xPhysicsCommon.createPhysicsWorld();
	s_pxPhysicsWorld->setGravity({ 0,-9.81,0 });
	s_pxPhysicsWorld->setEventListener(&s_xEventListener);
}

void Zenith_Physics::Update(float fDt) {
	s_fTimestepAccumulator += fDt;
	while (s_fTimestepAccumulator > s_fDesiredFramerate) {
		s_pxPhysicsWorld->update(s_fDesiredFramerate);
		s_fTimestepAccumulator -= s_fDesiredFramerate;
	}
}

void Zenith_Physics::Reset() {
	s_xPhysicsCommon.destroyPhysicsWorld(s_pxPhysicsWorld);
	Initialise();
}

reactphysics3d::Ray Zenith_Physics::BuildRayFromMouse(Zenith_CameraComponent& xCam)
{
	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xMousePos);

	double fX = xMousePos.x;
	double fY = xMousePos.y;

#if 0//def ZENITH_TOOLS
	//accounting for extra padding from imgui border
	fX -= 10;
	fY -= 45;

	//#TO_TODO: what happens on window resize?
	fX /= (float)VCE_GAME_WIDTH / float(VCE_GAME_WIDTH + VCE_EDITOR_ADDITIONAL_WIDTH);
	fY /= (float)VCE_GAME_HEIGHT / float(VCE_GAME_HEIGHT + VCE_EDITOR_ADDITIONAL_HEIGHT);
#endif

	glm::vec3 xNearPos = { fX, fY, 0.0f };
	glm::vec3 xFarPos = { fX, fY, 1.0f };

	glm::vec3 xOrigin = xCam.ScreenSpaceToWorldSpace(xNearPos);
	glm::vec3 xDest = xCam.ScreenSpaceToWorldSpace(xFarPos);

	reactphysics3d::Vector3 xRayOrigin = { xOrigin.x, xOrigin.y, xOrigin.z };
	reactphysics3d::Vector3 xRayDest = { xDest.x, xDest.y, xDest.z };

	reactphysics3d::Ray xRet(xRayOrigin, xRayDest);

	return xRet;
}

Zenith_Physics::PhysicsEventListener::PhysicsEventListener() {}

void Zenith_Physics::PhysicsEventListener::onContact(const CollisionCallback::CallbackData& xCallbackData) {
#if 0
	Application* pxApp = Application::GetInstance();
	for (uint32_t i = 0; i < xCallbackData.getNbContactPairs(); i++) {
		CollisionCallback::ContactPair xContactPair = xCallbackData.getContactPair(i);

		Entity xEntity1 = pxApp->m_pxCurrentScene->GetEntityByGuid(reinterpret_cast<GuidType>(xContactPair.getBody1()->getUserData()));
		Entity xEntity2 = pxApp->m_pxCurrentScene->GetEntityByGuid(reinterpret_cast<GuidType>(xContactPair.getBody2()->getUserData()));
		switch (xContactPair.getEventType()) {
		case reactphysics3d::CollisionCallback::ContactPair::EventType::ContactStart:
			if (xEntity1.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity1.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity2, CollisionEventType::Start);
			}
			if (xEntity2.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity2.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity1, CollisionEventType::Start);
			}
			break;
		case reactphysics3d::CollisionCallback::ContactPair::EventType::ContactExit:
			if (xEntity1.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity1.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity2, CollisionEventType::Exit);
			}
			if (xEntity2.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity2.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity1, CollisionEventType::Exit);
			}
			break;
		case reactphysics3d::CollisionCallback::ContactPair::EventType::ContactStay:
			if (xEntity1.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity1.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity2, CollisionEventType::Stay);
			}
			if (xEntity2.HasComponent<ScriptComponent>()) {
				ScriptComponent& xScript = xEntity2.GetComponent<ScriptComponent>();
				xScript.OnCollision(xEntity1, CollisionEventType::Stay);
			}
			break;
		}
	}
#endif
}