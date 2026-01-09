#include "Zenith.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Zenith_OS_Include.h"

JPH::TempAllocatorImpl* Zenith_Physics::s_pxTempAllocator = nullptr;
JPH::JobSystemThreadPool* Zenith_Physics::s_pxJobSystem = nullptr;
JPH::PhysicsSystem* Zenith_Physics::s_pxPhysicsSystem = nullptr;
double Zenith_Physics::s_fTimestepAccumulator = 0;
Zenith_Physics::PhysicsContactListener Zenith_Physics::s_xContactListener;
Zenith_Vector<Zenith_Physics::DeferredCollisionEvent> Zenith_Physics::s_xDeferredEvents;
std::mutex Zenith_Physics::s_xEventQueueMutex;

static void TraceImpl(const char* inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
{
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;

	Zenith_DebugBreak();
	return true;
};
#endif

namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr uint32_t NUM_LAYERS(2);
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl()
	{
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint32_t GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		default:														JPH_ASSERT(false); return "INVALID";
		}
	}
#endif

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

static BPLayerInterfaceImpl s_xBroadPhaseLayerInterface;
static ObjectVsBroadPhaseLayerFilterImpl s_xObjectVsBroadPhaseLayerFilter;
static ObjectLayerPairFilterImpl s_xObjectLayerPairFilter;

// Queue collision event for deferred processing on main thread
// CRITICAL: This is called from Jolt worker threads, so it must be thread-safe
static void QueueCollisionEventInternal(Zenith_EntityID xEntityID1, Zenith_EntityID xEntityID2, CollisionEventType eEventType)
{
	// Validate entity IDs
	if (!xEntityID1.IsValid() || !xEntityID2.IsValid())
		return;

	Zenith_Physics::DeferredCollisionEvent xEvent;
	xEvent.uEntityID1 = xEntityID1;
	xEvent.uEntityID2 = xEntityID2;
	xEvent.eEventType = eEventType;

	std::lock_guard<std::mutex> xLock(Zenith_Physics::s_xEventQueueMutex);
	Zenith_Physics::s_xDeferredEvents.PushBack(xEvent);
}

void Zenith_Physics::ProcessDeferredCollisionEvents()
{
	// Swap out the events to minimize lock time
	Zenith_Vector<DeferredCollisionEvent> xEventsToProcess;
	{
		std::lock_guard<std::mutex> xLock(s_xEventQueueMutex);
		xEventsToProcess = std::move(s_xDeferredEvents);
	}

	// Process all deferred events on the main thread (safe to access scene)
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	for (Zenith_Vector<DeferredCollisionEvent>::Iterator xIt(xEventsToProcess); !xIt.Done(); xIt.Next())
	{
		const DeferredCollisionEvent& xEvent = xIt.GetData();
		// Check if entities still exist
		if (!xScene.EntityExists(xEvent.uEntityID1) || !xScene.EntityExists(xEvent.uEntityID2))
			continue;

		Zenith_Entity xEntity1 = xScene.GetEntity(xEvent.uEntityID1);
		Zenith_Entity xEntity2 = xScene.GetEntity(xEvent.uEntityID2);

		// Dispatch to entity 1's script component
		if (xEntity1.HasComponent<Zenith_ScriptComponent>())
		{
			Zenith_ScriptComponent& xScript1 = xEntity1.GetComponent<Zenith_ScriptComponent>();
			switch (xEvent.eEventType)
			{
			case COLLISION_EVENT_TYPE_START:
				xScript1.OnCollisionEnter(xEntity2);
				break;
			case COLLISION_EVENT_TYPE_STAY:
				xScript1.OnCollisionStay(xEntity2);
				break;
			case COLLISION_EVENT_TYPE_EXIT:
				xScript1.OnCollisionExit(xEvent.uEntityID2);
				break;
			}
		}

		// Dispatch to entity 2's script component
		if (xEntity2.HasComponent<Zenith_ScriptComponent>())
		{
			Zenith_ScriptComponent& xScript2 = xEntity2.GetComponent<Zenith_ScriptComponent>();
			switch (xEvent.eEventType)
			{
			case COLLISION_EVENT_TYPE_START:
				xScript2.OnCollisionEnter(xEntity1);
				break;
			case COLLISION_EVENT_TYPE_STAY:
				xScript2.OnCollisionStay(xEntity1);
				break;
			case COLLISION_EVENT_TYPE_EXIT:
				xScript2.OnCollisionExit(xEvent.uEntityID1);
				break;
			}
		}
	}
}

void Zenith_Physics::Initialise()
{
	JPH::RegisterDefaultAllocator();

	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

		JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	s_pxTempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

	// Ensure we have at least 1 worker thread to avoid deadlock
	// Jolt Physics requires worker threads to process physics jobs
	uint32_t uNumThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
	s_pxJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, uNumThreads);

	s_pxPhysicsSystem = new JPH::PhysicsSystem();
	s_pxPhysicsSystem->Init(s_uMaxBodies, s_uNumBodyMutexes, s_uMaxBodyPairs, s_uMaxContactConstraints,
		s_xBroadPhaseLayerInterface, s_xObjectVsBroadPhaseLayerFilter, s_xObjectLayerPairFilter);

	s_pxPhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

	s_pxPhysicsSystem->SetContactListener(&s_xContactListener);
}

void Zenith_Physics::Update(float fDt)
{
	s_fTimestepAccumulator += fDt;

	while (s_fTimestepAccumulator >= s_fDesiredFramerate)
	{
		s_pxPhysicsSystem->Update(static_cast<float>(s_fDesiredFramerate), 1, s_pxTempAllocator, s_pxJobSystem);
		s_fTimestepAccumulator -= s_fDesiredFramerate;
	}

	// CRITICAL: Process deferred collision events AFTER physics update completes
	// This ensures we're on the main thread and can safely access scene data
	ProcessDeferredCollisionEvents();

	Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes();
}

void Zenith_Physics::Reset()
{
	Shutdown();
	Initialise();
}

void Zenith_Physics::Shutdown()
{
	if (s_pxPhysicsSystem)
	{
		delete s_pxPhysicsSystem;
		s_pxPhysicsSystem = nullptr;
	}

	if (s_pxJobSystem)
	{
		delete s_pxJobSystem;
		s_pxJobSystem = nullptr;
	}

	if (s_pxTempAllocator)
	{
		delete s_pxTempAllocator;
		s_pxTempAllocator = nullptr;
	}

	if (JPH::Factory::sInstance)
	{
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	JPH::UnregisterTypes();
}

Zenith_Physics::RaycastInfo Zenith_Physics::BuildRayFromMouse(Zenith_CameraComponent& xCam)
{
	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xMousePos);

	double fX = xMousePos.x;
	double fY = xMousePos.y;


	glm::vec3 xNearPos = { fX, fY, 0.0f };
	glm::vec3 xFarPos = { fX, fY, 1.0f };

	glm::vec3 xOrigin = xCam.ScreenSpaceToWorldSpace(xNearPos);
	glm::vec3 xDest = xCam.ScreenSpaceToWorldSpace(xFarPos);

	Zenith_Maths::Vector3 xRayDirection = Zenith_Maths::Vector3(xDest.x - xOrigin.x, xDest.y - xOrigin.y, xDest.z - xOrigin.z);
	xRayDirection = glm::normalize(xRayDirection);

	RaycastInfo xInfo;
	xInfo.m_xOrigin = xOrigin;
	xInfo.m_xDirection = xRayDirection;
	return xInfo;
}

void Zenith_Physics::SetLinearVelocity(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xVelocity)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetLinearVelocity(xBodyID, JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

Zenith_Maths::Vector3 Zenith_Physics::GetLinearVelocity(const JPH::BodyID& xBodyID)
{
	if (xBodyID.IsInvalid()) return Zenith_Maths::Vector3(0, 0, 0);
	// CRITICAL FIX: Use locked interface for thread safety
	// GetBodyInterfaceNoLock() was unsafe when physics simulation runs on worker threads
	// The setter uses GetBodyInterface() so the getter must match for consistency
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	JPH::Vec3 xVel = xBodyInterface.GetLinearVelocity(xBodyID);
	return Zenith_Maths::Vector3(xVel.GetX(), xVel.GetY(), xVel.GetZ());
}

void Zenith_Physics::SetAngularVelocity(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xVelocity)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetAngularVelocity(xBodyID, JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

Zenith_Maths::Vector3 Zenith_Physics::GetAngularVelocity(const JPH::BodyID& xBodyID)
{
	if (xBodyID.IsInvalid()) return Zenith_Maths::Vector3(0, 0, 0);
	// CRITICAL FIX: Use locked interface for thread safety (matches setter)
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	JPH::Vec3 xVel = xBodyInterface.GetAngularVelocity(xBodyID);
	return Zenith_Maths::Vector3(xVel.GetX(), xVel.GetY(), xVel.GetZ());
}

void Zenith_Physics::AddForce(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xForce)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(s_pxPhysicsSystem != nullptr, "AddForce: Physics system not initialized");
	if (!s_pxPhysicsSystem) return;  // Defensive check for release builds

	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	// CRITICAL: Activate the body first - sleeping bodies ignore forces
	xBodyInterface.ActivateBody(xBodyID);
	xBodyInterface.AddForce(xBodyID, JPH::Vec3(xForce.x, xForce.y, xForce.z));
}

void Zenith_Physics::AddImpulse(const JPH::BodyID& xBodyID, const Zenith_Maths::Vector3& xImpulse)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(s_pxPhysicsSystem != nullptr, "AddImpulse: Physics system not initialized");
	if (!s_pxPhysicsSystem) return;  // Defensive check for release builds

	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	// Activate the body and apply instant velocity change
	xBodyInterface.ActivateBody(xBodyID);
	xBodyInterface.AddLinearVelocity(xBodyID, JPH::Vec3(xImpulse.x, xImpulse.y, xImpulse.z));
}

void Zenith_Physics::SetGravityEnabled(const JPH::BodyID& xBodyID, bool bEnabled)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetGravityFactor(xBodyID, bEnabled ? 1.0f : 0.0f);
}

JPH::ValidateResult Zenith_Physics::PhysicsContactListener::OnContactValidate(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
{
	return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void Zenith_Physics::PhysicsContactListener::OnContactAdded(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
	// Queue event for deferred processing (thread-safe)
	Zenith_EntityID xEntityID1 = Zenith_EntityID::FromPacked(inBody1.GetUserData());
	Zenith_EntityID xEntityID2 = Zenith_EntityID::FromPacked(inBody2.GetUserData());
	QueueCollisionEventInternal(xEntityID1, xEntityID2, COLLISION_EVENT_TYPE_START);
}

void Zenith_Physics::PhysicsContactListener::OnContactPersisted(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
	// Queue event for deferred processing (thread-safe)
	Zenith_EntityID xEntityID1 = Zenith_EntityID::FromPacked(inBody1.GetUserData());
	Zenith_EntityID xEntityID2 = Zenith_EntityID::FromPacked(inBody2.GetUserData());
	QueueCollisionEventInternal(xEntityID1, xEntityID2, COLLISION_EVENT_TYPE_STAY);
}

void Zenith_Physics::PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
	// Queue event for deferred processing (thread-safe)
	JPH::BodyID xBodyID1 = inSubShapePair.GetBody1ID();
	JPH::BodyID xBodyID2 = inSubShapePair.GetBody2ID();

	Zenith_EntityID xEntityID1 = INVALID_ENTITY_ID;
	Zenith_EntityID xEntityID2 = INVALID_ENTITY_ID;

	// CRITICAL: Use TryGetBody instead of BodyLockRead to avoid deadlock
	// We're already inside a physics callback, so the bodies are locked by Jolt
	const JPH::BodyLockInterface& xLockInterface = s_pxPhysicsSystem->GetBodyLockInterface();

	// TryGetBody doesn't acquire locks - safe to use in callbacks
	const JPH::Body* pxBody1 = xLockInterface.TryGetBody(xBodyID1);
	if (pxBody1)
	{
		xEntityID1 = Zenith_EntityID::FromPacked(pxBody1->GetUserData());
	}

	const JPH::Body* pxBody2 = xLockInterface.TryGetBody(xBodyID2);
	if (pxBody2)
	{
		xEntityID2 = Zenith_EntityID::FromPacked(pxBody2->GetUserData());
	}

	QueueCollisionEventInternal(xEntityID1, xEntityID2, COLLISION_EVENT_TYPE_EXIT);
}
