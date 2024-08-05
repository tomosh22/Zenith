#pragma once
#include "reactphysics3d/reactphysics3d.h"
class Zenith_CameraComponent;

enum CollisionVolumeType
{
	COLLISION_VOLUME_TYPE_AABB,
	COLLISION_VOLUME_TYPE_OBB,
	COLLISION_VOLUME_TYPE_SPHERE,
	COLLISION_VOLUME_TYPE_CAPSULE,
	COLLISION_VOLUME_TYPE_TERRAIN
};

enum CollisionEventType
{
	COLLISION_EVENT_TYPE_START,
	COLLISION_EVENT_TYPE_EXIT,
	COLLISION_EVENT_TYPE_STAY
};

enum RigidBodyType
{
	RIGIDBODY_TYPE_DYNAMIC,
	RIGIDBODY_TYPE_STATIC
};

class Zenith_Physics
{
public:
	static reactphysics3d::PhysicsCommon s_xPhysicsCommon;
	static reactphysics3d::PhysicsWorld* s_pxPhysicsWorld;


	static void Initialise();
	static void Update(float fDt);
	static void Reset();

	reactphysics3d::Ray BuildRayFromMouse(Zenith_CameraComponent& xCam);

	static double s_fTimestepAccumulator;
	//#TO_TODO: make this a define
	static constexpr double s_fDesiredFramerate = 1. / 60.;

	class PhysicsEventListener : public reactphysics3d::EventListener
	{
	public:
		PhysicsEventListener();
		void onContact(const CollisionCallback::CallbackData& xCallbackData) override;
	};

	static PhysicsEventListener s_xEventListener;

};
