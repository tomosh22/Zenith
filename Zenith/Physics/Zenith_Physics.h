#pragma once
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

class Zenith_CameraComponent;

enum CollisionVolumeType
{
	COLLISION_VOLUME_TYPE_AABB,
	COLLISION_VOLUME_TYPE_OBB,
	COLLISION_VOLUME_TYPE_SPHERE,
	COLLISION_VOLUME_TYPE_CAPSULE,
	COLLISION_VOLUME_TYPE_TERRAIN,
	COLLISION_VOLUME_TYPE_MODEL_MESH  // Uses physics mesh from Zenith_ModelComponent
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
	static JPH::TempAllocatorImpl* s_pxTempAllocator;
	static JPH::JobSystemThreadPool* s_pxJobSystem;
	static JPH::PhysicsSystem* s_pxPhysicsSystem;

	static void Initialise();
	static void Update(float fDt);
	static void Reset();
	static void Shutdown();

	static void SetLinearVelocity(JPH::Body* pxBody, const Zenith_Maths::Vector3& xVelocity);
	static Zenith_Maths::Vector3 GetLinearVelocity(JPH::Body* pxBody);
	static void SetAngularVelocity(JPH::Body* pxBody, const Zenith_Maths::Vector3& xVelocity);
	static Zenith_Maths::Vector3 GetAngularVelocity(JPH::Body* pxBody);
	static void AddForce(JPH::Body* pxBody, const Zenith_Maths::Vector3& xForce);
	static void SetGravityEnabled(JPH::Body* pxBody, bool bEnabled);

	struct RaycastInfo
	{
		Zenith_Maths::Vector3 m_xOrigin;
		Zenith_Maths::Vector3 m_xDirection;
	};
	static RaycastInfo BuildRayFromMouse(Zenith_CameraComponent& xCam);

	static double s_fTimestepAccumulator;
	static constexpr double s_fDesiredFramerate = 1.0 / 60.0;

	class PhysicsContactListener : public JPH::ContactListener
	{
	public:
		PhysicsContactListener() = default;

		virtual JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2,
			JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override;

		virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

		virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2,
			const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;

		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;
	};

	static PhysicsContactListener s_xContactListener;

private:
	static constexpr uint32_t s_uMaxBodies = 65536;
	static constexpr uint32_t s_uNumBodyMutexes = 0; // 0 = auto-detect
	static constexpr uint32_t s_uMaxBodyPairs = 65536;
	static constexpr uint32_t s_uMaxContactConstraints = 10240;
};
