#pragma once

// Include forward declarations and enums
#include "Physics/Zenith_Physics_Fwd.h"

// Save zone state and set up placement new protection for Jolt includes
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_PHYSICS_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
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
// Restore zone state - only undefine if we defined it ourselves
#ifndef ZENITH_PHYSICS_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_PHYSICS_ZONE_WAS_SET
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Collections/Zenith_Vector.h"
#include "EntityComponent/Zenith_Entity.h"

class Zenith_CameraComponent;

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

	// Jolt memory tracking (tracked separately from normal allocations)
	static u_int64 GetJoltMemoryAllocated();
	static u_int64 GetJoltAllocationCount();

	// Physics body manipulation - use BodyID for thread-safe access
	static void SetLinearVelocity(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xVelocity);
	static Zenith_Maths::Vector3 GetLinearVelocity(const JPH::BodyID& xBodyID);
	static void SetAngularVelocity(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xVelocity);
	static Zenith_Maths::Vector3 GetAngularVelocity(const JPH::BodyID& xBodyID);
	static void AddForce(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xForce);
	static void AddImpulse(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xImpulse);
	static void SetGravityEnabled(const JPH::BodyID& xBodyID, bool bEnabled);
	static void LockRotation(const JPH::BodyID& xBodyID, bool bLockX, bool bLockY, bool bLockZ);
	static void EnforceUpright(const JPH::BodyID& xBodyID); // Call every frame to keep body upright

	struct RaycastInfo
	{
		Zenith_Maths::Vector3 m_xOrigin;
		Zenith_Maths::Vector3 m_xDirection;
	};
	static RaycastInfo BuildRayFromMouse(Zenith_CameraComponent& xCam);

	struct RaycastResult
	{
		bool m_bHit = false;
		Zenith_Maths::Vector3 m_xHitPoint;
		Zenith_Maths::Vector3 m_xHitNormal;
		float m_fDistance = 0.0f;
		Zenith_EntityID m_xHitEntity = INVALID_ENTITY_ID;
	};
	// Cast a ray and return the first hit. Returns true if hit, false if no hit.
	static RaycastResult Raycast(const Zenith_Maths::Vector3& xOrigin, const Zenith_Maths::Vector3& xDirection, float fMaxDistance);

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

	// Diagnostics: number of collision events dropped due to queue overflow since last frame
	static uint32_t GetDroppedCollisionEventCount() { return s_uDroppedEventCount; }
	static uint32_t s_uDroppedEventCount;

private:
	static constexpr uint32_t s_uMaxBodies = 65536;
	static constexpr uint32_t s_uNumBodyMutexes = 0; // 0 = auto-detect
	static constexpr uint32_t s_uMaxBodyPairs = 65536;
	static constexpr uint32_t s_uMaxContactConstraints = 10240;

	// Thread-safe deferred event queue
	static Zenith_Vector<DeferredCollisionEvent> s_xDeferredEvents;
	static Zenith_Mutex_NoProfiling s_xEventQueueMutex; // No profiling - accessed from Jolt worker threads which aren't registered with Zenith threading

	// Dispatch a collision event to an entity's script component
	static void DispatchCollisionToEntity(Zenith_Entity& xEntity, Zenith_Entity& xOtherEntity, Zenith_EntityID xOtherID, CollisionEventType eEventType);

	friend void QueueCollisionEventInternal(Zenith_EntityID, Zenith_EntityID, CollisionEventType);
};
