#pragma once

// Forward declarations for Jolt Physics types
// Include this header when you only need pointers/references to Jolt types
// without pulling in the full Jolt headers

namespace JPH
{
	class Body;
	class Shape;
	class BodyID;
	class PhysicsSystem;
	class TempAllocatorImpl;
	class JobSystemThreadPool;
	class ContactListener;
}

// Physics enums (needed for ColliderComponent interface)
enum CollisionVolumeType
{
	COLLISION_VOLUME_TYPE_AABB,
	COLLISION_VOLUME_TYPE_OBB,
	COLLISION_VOLUME_TYPE_SPHERE,
	COLLISION_VOLUME_TYPE_CAPSULE,
	COLLISION_VOLUME_TYPE_TERRAIN,
	COLLISION_VOLUME_TYPE_MODEL_MESH
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

// Forward declare the main physics class
class Zenith_Physics;

// Opaque value-type handle to a physics body. Mirrors JPH::BodyID's single
// uint32 representation (invalid == 0xffffffff); game code holds and passes
// these, only the physics layer converts to/from JPH::BodyID.
class Zenith_PhysicsBodyID
{
public:
	static constexpr u_int32 uINVALID_PHYSICS_BODY_ID = 0xffffffffu;

	Zenith_PhysicsBodyID() = default;
	explicit Zenith_PhysicsBodyID(u_int32 uID) : m_uID(uID) {}

	bool IsValid() const { return m_uID != uINVALID_PHYSICS_BODY_ID; }
	bool IsInvalid() const { return m_uID == uINVALID_PHYSICS_BODY_ID; }
	bool operator==(const Zenith_PhysicsBodyID& xOther) const { return m_uID == xOther.m_uID; }
	bool operator!=(const Zenith_PhysicsBodyID& xOther) const { return m_uID != xOther.m_uID; }

	u_int32 m_uID = uINVALID_PHYSICS_BODY_ID;
};
