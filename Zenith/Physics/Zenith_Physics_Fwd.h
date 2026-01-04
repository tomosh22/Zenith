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
