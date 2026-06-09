#pragma once

#include "Physics/Zenith_Physics_Fwd.h"

#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_PHYSICS_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include "Memory/Zenith_MemoryManagement_Disabled.h"
// Only what the class shape itself needs: the by-value PhysicsContactListener
// member derives JPH::ContactListener. Everything else is fwd-declared
// (Zenith_Physics_Fwd.h) — the full Jolt headers live in Zenith_Physics.cpp.
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#ifndef ZENITH_PHYSICS_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_PHYSICS_ZONE_WAS_SET
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "Collections/Zenith_Vector.h"
#include "ZenithECS/Zenith_Entity.h"

class Zenith_CameraComponent;
class Zenith_Input;

// State + behaviour for the Physics subsystem. Held on g_xEngine and
// accessed via g_xEngine.Physics().
class Zenith_Physics
{
public:
	Zenith_Physics() = default;
	~Zenith_Physics() = default;

	Zenith_Physics(const Zenith_Physics&) = delete;
	Zenith_Physics& operator=(const Zenith_Physics&) = delete;

	void Initialise(Zenith_Input& xInput);
	void Update(float fDt);
	void Reset();
	void Shutdown();

	u_int64 GetJoltMemoryAllocated();
	u_int64 GetJoltAllocationCount();

	void SetLinearVelocity(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xVelocity);
	Zenith_Maths::Vector3 GetLinearVelocity(Zenith_PhysicsBodyID xBodyID);
	void SetAngularVelocity(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xVelocity);
	Zenith_Maths::Vector3 GetAngularVelocity(Zenith_PhysicsBodyID xBodyID);
	void AddForce(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xForce);
	void AddImpulse(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xImpulse);
	void SetGravityEnabled(Zenith_PhysicsBodyID xBodyID, bool bEnabled);
	// Toggle a body between solid (collides + pushes) and sensor (detects
	// overlap but doesn't physically collide). The body must already be
	// created (HasValidBody on the component).
	void SetIsSensor(Zenith_PhysicsBodyID xBodyID, bool bSensor);
	void LockRotation(Zenith_PhysicsBodyID xBodyID, bool bLockX, bool bLockY, bool bLockZ);
	void EnforceUpright(Zenith_PhysicsBodyID xBodyID);

	void SetRestitution(Zenith_PhysicsBodyID xBodyID, float fRestitution);
	float GetRestitution(Zenith_PhysicsBodyID xBodyID);
	void SetFriction(Zenith_PhysicsBodyID xBodyID, float fFriction);
	float GetFriction(Zenith_PhysicsBodyID xBodyID);

	// Teleport: position + identity rotation + activate + zero velocity.
	// The wrapper exists so callers never need Jolt's BodyInterface.
	void TeleportBody(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xPosition);

	struct RaycastInfo
	{
		Zenith_Maths::Vector3 m_xOrigin;
		Zenith_Maths::Vector3 m_xDirection;
	};
	RaycastInfo BuildRayFromMouse(Zenith_CameraComponent& xCam);

	struct RaycastResult
	{
		bool m_bHit = false;
		Zenith_Maths::Vector3 m_xHitPoint;
		Zenith_Maths::Vector3 m_xHitNormal;
		float m_fDistance = 0.0f;
		Zenith_EntityID m_xHitEntity = INVALID_ENTITY_ID;
	};
	RaycastResult Raycast(const Zenith_Maths::Vector3& xOrigin, const Zenith_Maths::Vector3& xDirection, float fMaxDistance);
	RaycastResult Raycast(const Zenith_Maths::Vector3& xOrigin, const Zenith_Maths::Vector3& xDirection, float fMaxDistance, Zenith_EntityID xIgnoreEntity);

	static constexpr double s_fDesiredFramerate = 1.0 / 60.0;

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

	void ProcessDeferredCollisionEvents();

	uint32_t GetDroppedCollisionEventCount() const { return m_uDroppedEventCount; }

	void DispatchCollisionToEntity(Zenith_Entity& xEntity, Zenith_Entity& xOtherEntity, Zenith_EntityID xOtherID, CollisionEventType eEventType);

	static constexpr uint32_t s_uMaxBodies = 65536;
	static constexpr uint32_t s_uNumBodyMutexes = 0;
	static constexpr uint32_t s_uMaxBodyPairs = 65536;
	static constexpr uint32_t s_uMaxContactConstraints = 10240;

	// Engine-internal escape hatch (collider/transform body sync, tests).
	// Game code uses the wrapper methods above instead. May be null before
	// Initialise / after Shutdown — callers null-check.
	JPH::PhysicsSystem* GetJoltSystem() { return m_pxPhysicsSystem; }

private:
	// Heap-allocated Jolt singletons — reachable outside the physics layer
	// only through GetJoltSystem().
	JPH::TempAllocatorImpl*   m_pxTempAllocator = nullptr;
	JPH::JobSystemThreadPool* m_pxJobSystem     = nullptr;
	JPH::PhysicsSystem*       m_pxPhysicsSystem = nullptr;

public:
	double m_fTimestepAccumulator = 0.0;

	PhysicsContactListener m_xContactListener;

	Zenith_Vector<DeferredCollisionEvent> m_xDeferredEvents;
	Zenith_Mutex_NoProfiling              m_xEventQueueMutex;
	uint32_t                              m_uDroppedEventCount = 0;

	bool m_bInitialised = false;

	// Injected cross-subsystem dependency (composition root wires this in
	// Initialise). Routes mouse-position reads through Zenith_Input so they
	// respect Zenith_InputSimulator overrides.
	Zenith_Input* m_pxInput = nullptr;

	friend void QueueCollisionEventInternal(Zenith_EntityID, Zenith_EntityID, CollisionEventType);
};
