#pragma once

// Include forward declarations and enums
#include "Physics/Zenith_Physics_Fwd.h"

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
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Collections/Zenith_Vector.h"
#include <mutex>

class Zenith_CameraComponent;
using Zenith_EntityID = u_int;

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

	// Deferred collision event for thread-safe processing
	struct DeferredCollisionEvent
	{
		Zenith_EntityID uEntityID1;
		Zenith_EntityID uEntityID2;
		CollisionEventType eEventType;
	};

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
	static void ProcessDeferredCollisionEvents();

private:
	static constexpr uint32_t s_uMaxBodies = 65536;
	static constexpr uint32_t s_uNumBodyMutexes = 0; // 0 = auto-detect
	static constexpr uint32_t s_uMaxBodyPairs = 65536;
	static constexpr uint32_t s_uMaxContactConstraints = 10240;

	// Thread-safe deferred event queue
	static Zenith_Vector<DeferredCollisionEvent> s_xDeferredEvents;
	static std::mutex s_xEventQueueMutex;

	friend void QueueCollisionEventInternal(Zenith_EntityID, Zenith_EntityID, CollisionEventType);
};
