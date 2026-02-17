// This file creates Jolt Physics objects - disable memory tracking macro to avoid conflicts
// with Jolt's custom operator new
#include "Zenith.h"
#define ZENITH_PLACEMENT_NEW_ZONE
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "Zenith_OS_Include.h"
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>

JPH::TempAllocatorImpl* Zenith_Physics::s_pxTempAllocator = nullptr;
JPH::JobSystemThreadPool* Zenith_Physics::s_pxJobSystem = nullptr;
JPH::PhysicsSystem* Zenith_Physics::s_pxPhysicsSystem = nullptr;
double Zenith_Physics::s_fTimestepAccumulator = 0;
Zenith_Physics::PhysicsContactListener Zenith_Physics::s_xContactListener;
Zenith_Vector<Zenith_Physics::DeferredCollisionEvent> Zenith_Physics::s_xDeferredEvents;
Zenith_Mutex_NoProfiling Zenith_Physics::s_xEventQueueMutex;
uint32_t Zenith_Physics::s_uDroppedEventCount = 0;

static bool g_bInitialised = false;

// Jolt memory tracking
static std::atomic<u_int64> s_ulJoltMemoryAllocated = 0;
static std::atomic<u_int64> s_ulJoltAllocationCount = 0;

// Jolt requires 16-byte alignment on 64-bit platforms for ALL allocations (not just aligned ones)
// Our header must be 16 bytes to preserve alignment from malloc
struct alignas(16) JoltAllocHeader
{
	size_t m_ulSize;
	size_t m_ulPadding;  // Ensure 16-byte alignment
};
static_assert(sizeof(JoltAllocHeader) == 16, "JoltAllocHeader must be 16 bytes for alignment");

static void* JoltAllocate(size_t inSize)
{
	// Allocate extra space for 16-byte aligned header
	size_t ulTotalSize = sizeof(JoltAllocHeader) + inSize;
	void* pRaw = std::malloc(ulTotalSize);
	if (!pRaw)
		return nullptr;

	// Store size in header
	JoltAllocHeader* pHeader = static_cast<JoltAllocHeader*>(pRaw);
	pHeader->m_ulSize = inSize;
	pHeader->m_ulPadding = 0;

	// Track allocation
	s_ulJoltMemoryAllocated += inSize;
	s_ulJoltAllocationCount++;

	// Return pointer past header - guaranteed 16-byte aligned since header is 16 bytes
	void* pResult = pHeader + 1;
	Zenith_Assert((reinterpret_cast<uintptr_t>(pResult) % 16) == 0, "JoltAllocate: 16-byte alignment broken");
	return pResult;
}

static void* JoltReallocate(void* inBlock, size_t, size_t inNewSize)
{
	if (!inBlock)
		return JoltAllocate(inNewSize);

	if (inNewSize == 0)
	{
		// Get header and free
		JoltAllocHeader* pHeader = static_cast<JoltAllocHeader*>(inBlock) - 1;
		size_t ulOldSize = pHeader->m_ulSize;
		s_ulJoltMemoryAllocated -= ulOldSize;
		s_ulJoltAllocationCount--;
		std::free(pHeader);
		return nullptr;
	}

	// Reallocate
	JoltAllocHeader* pOldHeader = static_cast<JoltAllocHeader*>(inBlock) - 1;
	size_t ulOldActual = pOldHeader->m_ulSize;

	size_t ulTotalSize = sizeof(JoltAllocHeader) + inNewSize;
	void* pNewRaw = std::realloc(pOldHeader, ulTotalSize);
	if (!pNewRaw)
		return nullptr;

	// Update tracking (remove old, add new)
	s_ulJoltMemoryAllocated -= ulOldActual;
	s_ulJoltMemoryAllocated += inNewSize;

	// Store new size
	JoltAllocHeader* pNewHeader = static_cast<JoltAllocHeader*>(pNewRaw);
	pNewHeader->m_ulSize = inNewSize;

	return pNewHeader + 1;
}

static void JoltFree(void* inBlock)
{
	if (!inBlock)
		return;

	// Get header (16 bytes before user pointer)
	JoltAllocHeader* pHeader = static_cast<JoltAllocHeader*>(inBlock) - 1;
	size_t ulSize = pHeader->m_ulSize;

	// Track deallocation
	s_ulJoltMemoryAllocated -= ulSize;
	s_ulJoltAllocationCount--;

	std::free(pHeader);
}

// Aligned allocation - store original pointer and size at fixed offset before aligned address
// We allocate extra space and store metadata at the START of the allocation, then return
// an aligned address after it

static void* JoltAlignedAllocate(size_t inSize, size_t inAlignment)
{
	// Ensure alignment is at least sizeof(void*) and is a power of 2
	if (inAlignment < sizeof(void*))
		inAlignment = sizeof(void*);

	// We need space for:
	// - A pointer to store the original malloc address (for freeing)
	// - A size_t to store the allocation size (for tracking)
	// - Padding to achieve requested alignment
	// - The actual user data
	//
	// Layout: [original_ptr][size][padding...][aligned_user_data]
	// We store the original pointer at a known location: aligned_addr - sizeof(void*) - sizeof(size_t)

	// Total size: metadata + alignment padding (worst case) + user data
	const size_t uMetadataSize = sizeof(void*) + sizeof(size_t);
	size_t ulTotalSize = uMetadataSize + inAlignment + inSize;

	void* pRaw = std::malloc(ulTotalSize);
	if (!pRaw)
		return nullptr;

	// Calculate the aligned address for user data
	// Start after the metadata, then align up
	uintptr_t ulRawAddr = reinterpret_cast<uintptr_t>(pRaw);
	uintptr_t ulDataStart = ulRawAddr + uMetadataSize;
	uintptr_t ulAlignedAddr = (ulDataStart + inAlignment - 1) & ~(inAlignment - 1);

	// Store metadata just before the aligned address
	// Use the space immediately before ulAlignedAddr for our metadata
	void** ppOriginal = reinterpret_cast<void**>(ulAlignedAddr - sizeof(void*) - sizeof(size_t));
	size_t* pSize = reinterpret_cast<size_t*>(ulAlignedAddr - sizeof(size_t));

	Zenith_Assert(reinterpret_cast<uintptr_t>(ppOriginal) >= ulRawAddr, "JoltAlignedAllocate: metadata pointer underflow");
	*ppOriginal = pRaw;
	*pSize = inSize;

	// Track allocation
	s_ulJoltMemoryAllocated += inSize;
	s_ulJoltAllocationCount++;

	Zenith_Assert((ulAlignedAddr % inAlignment) == 0, "JoltAlignedAllocate: alignment invariant broken");
	return reinterpret_cast<void*>(ulAlignedAddr);
}

static void JoltAlignedFree(void* inBlock)
{
	if (!inBlock)
		return;

	// Retrieve metadata from known locations before the aligned address
	uintptr_t ulAlignedAddr = reinterpret_cast<uintptr_t>(inBlock);
	void** ppOriginal = reinterpret_cast<void**>(ulAlignedAddr - sizeof(void*) - sizeof(size_t));
	size_t* pSize = reinterpret_cast<size_t*>(ulAlignedAddr - sizeof(size_t));

	void* pOriginal = *ppOriginal;
	size_t ulSize = *pSize;

	// Track deallocation
	s_ulJoltMemoryAllocated -= ulSize;
	s_ulJoltAllocationCount--;

	std::free(pOriginal);
}

u_int64 Zenith_Physics::GetJoltMemoryAllocated()
{
	return s_ulJoltMemoryAllocated.load();
}

u_int64 Zenith_Physics::GetJoltAllocationCount()
{
	return s_ulJoltAllocationCount.load();
}

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
static constexpr u_int uMAX_DEFERRED_COLLISION_EVENTS = 4096;

void QueueCollisionEventInternal(Zenith_EntityID xEntityID1, Zenith_EntityID xEntityID2, CollisionEventType eEventType)
{
	if (!xEntityID1.IsValid() || !xEntityID2.IsValid())
		return;

	Zenith_Physics::DeferredCollisionEvent xEvent;
	xEvent.uEntityID1 = xEntityID1;
	xEvent.uEntityID2 = xEntityID2;
	xEvent.eEventType = eEventType;

	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(Zenith_Physics::s_xEventQueueMutex);
	if (Zenith_Physics::s_xDeferredEvents.GetSize() >= uMAX_DEFERRED_COLLISION_EVENTS)
	{
		Zenith_Physics::s_uDroppedEventCount++;
		Zenith_Assert(false, "Deferred collision event queue overflow (%u events) - events are being dropped", uMAX_DEFERRED_COLLISION_EVENTS);
		return;
	}
	Zenith_Physics::s_xDeferredEvents.PushBack(xEvent);
}

void Zenith_Physics::DispatchCollisionToEntity(Zenith_Entity& xEntity, Zenith_Entity& xOtherEntity, Zenith_EntityID xOtherID, CollisionEventType eEventType)
{
	if (!xEntity.HasComponent<Zenith_ScriptComponent>())
	{
		return;
	}

	Zenith_ScriptComponent& xScript = xEntity.GetComponent<Zenith_ScriptComponent>();
	switch (eEventType)
	{
	case COLLISION_EVENT_TYPE_START:
		xScript.OnCollisionEnter(xOtherEntity);
		break;
	case COLLISION_EVENT_TYPE_STAY:
		xScript.OnCollisionStay(xOtherEntity);
		break;
	case COLLISION_EVENT_TYPE_EXIT:
		xScript.OnCollisionExit(xOtherID);
		break;
	}
}

void Zenith_Physics::ProcessDeferredCollisionEvents()
{
	if (s_uDroppedEventCount > 0)
	{
		Zenith_Warning(LOG_CATEGORY_PHYSICS, "Dropped %u collision events last frame due to queue overflow (max=%u)",
			s_uDroppedEventCount, uMAX_DEFERRED_COLLISION_EVENTS);
		s_uDroppedEventCount = 0;
	}

	// Swap out the events to minimize lock time
	Zenith_Vector<DeferredCollisionEvent> xEventsToProcess;
	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(s_xEventQueueMutex);
		xEventsToProcess = std::move(s_xDeferredEvents);
	}

	// Process all deferred events on the main thread (safe to access scene)
	// Unity parity: dispatch collision events to all loaded scenes, not just the active scene
	for (Zenith_Vector<DeferredCollisionEvent>::Iterator xIt(xEventsToProcess); !xIt.Done(); xIt.Next())
	{
		const DeferredCollisionEvent& xEvent = xIt.GetData();

		// Look up each entity's owning scene from the global entity slot
		// Entities in a collision pair may be in different scenes
		Zenith_SceneData* pxSceneData1 = Zenith_SceneManager::GetSceneDataForEntity(xEvent.uEntityID1);
		Zenith_SceneData* pxSceneData2 = Zenith_SceneManager::GetSceneDataForEntity(xEvent.uEntityID2);

		// Check if entities still exist in their respective scenes (may have been destroyed between queueing and processing)
		if (!pxSceneData1 || !pxSceneData2)
		{
			Zenith_Log(LOG_CATEGORY_PHYSICS, "Dropped collision event: entity no longer exists (idx1=%u, idx2=%u)", xEvent.uEntityID1.m_uIndex, xEvent.uEntityID2.m_uIndex);
			continue;
		}

		Zenith_Entity xEntity1 = pxSceneData1->GetEntity(xEvent.uEntityID1);
		Zenith_Entity xEntity2 = pxSceneData2->GetEntity(xEvent.uEntityID2);

		DispatchCollisionToEntity(xEntity1, xEntity2, xEvent.uEntityID2, xEvent.eEventType);
		DispatchCollisionToEntity(xEntity2, xEntity1, xEvent.uEntityID1, xEvent.eEventType);
	}
}

void Zenith_Physics::Initialise()
{
	if (g_bInitialised)
	{
		return;
	}
	g_bInitialised = true;
	// Set custom allocator functions for Jolt memory tracking
	// Must be done BEFORE any Jolt allocations occur
	JPH::Allocate = JoltAllocate;
	JPH::Reallocate = JoltReallocate;
	JPH::Free = JoltFree;
	JPH::AlignedAllocate = JoltAlignedAllocate;
	JPH::AlignedFree = JoltAlignedFree;

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
	if (!g_bInitialised)
	{
		return;
	}
	g_bInitialised = false;

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

void Zenith_Physics::LockRotation(const JPH::BodyID& xBodyID, bool bLockX, bool bLockY, bool bLockZ)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(s_pxPhysicsSystem != nullptr, "LockRotation: Physics system not initialized");
	if (!s_pxPhysicsSystem) return;

	JPH::BodyLockWrite xLock(s_pxPhysicsSystem->GetBodyLockInterface(), xBodyID);
	if (xLock.Succeeded())
	{
		JPH::Body& xBody = xLock.GetBody();
		if (xBody.IsDynamic())
		{
			JPH::MotionProperties* pxMotion = xBody.GetMotionProperties();

			// Step 1: Zero out angular velocity on locked axes
			JPH::Vec3 xAngVel = pxMotion->GetAngularVelocity();
			if (bLockX) xAngVel.SetX(0.0f);
			if (bLockY) xAngVel.SetY(0.0f);
			if (bLockZ) xAngVel.SetZ(0.0f);
			pxMotion->SetAngularVelocity(xAngVel);

			// Step 2: Set inverse inertia to zero on locked axes to prevent angular acceleration
			JPH::Vec3 xInvInertia = pxMotion->GetInverseInertiaDiagonal();
			if (bLockX) xInvInertia.SetX(0.0f);
			if (bLockY) xInvInertia.SetY(0.0f);
			if (bLockZ) xInvInertia.SetZ(0.0f);
			pxMotion->SetInverseInertia(xInvInertia, pxMotion->GetInertiaRotation());

			// Step 3: Reset rotation to upright (keep only Y rotation)
			// This fixes any tilt that already occurred
			if (bLockX && bLockZ)
			{
				JPH::Quat xRot = xBody.GetRotation();
				// Extract yaw (Y rotation) and create new quaternion with only Y rotation
				JPH::Vec3 xForward = xRot.RotateAxisZ();
				float fYaw = JPH::ATan2(xForward.GetX(), xForward.GetZ());
				JPH::Quat xUprightRot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), fYaw);
				// Use NoLock interface since we already hold the body lock
				s_pxPhysicsSystem->GetBodyInterfaceNoLock().SetRotation(xBodyID, xUprightRot, JPH::EActivation::DontActivate);
			}
		}
	}
}

void Zenith_Physics::EnforceUpright(const JPH::BodyID& xBodyID)
{
	if (xBodyID.IsInvalid()) return;
	if (!s_pxPhysicsSystem) return;

	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();

	// Zero out angular velocity on X and Z axes (keep Y rotation allowed)
	JPH::Vec3 xAngVel = xBodyInterface.GetAngularVelocity(xBodyID);
	xAngVel.SetX(0.0f);
	xAngVel.SetZ(0.0f);
	xBodyInterface.SetAngularVelocity(xBodyID, xAngVel);

	// Reset rotation to upright (preserve only Y rotation/yaw)
	JPH::Quat xRot = xBodyInterface.GetRotation(xBodyID);
	JPH::Vec3 xForward = xRot.RotateAxisZ();
	float fYaw = JPH::ATan2(xForward.GetX(), xForward.GetZ());
	JPH::Quat xUprightRot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), fYaw);
	xBodyInterface.SetRotation(xBodyID, xUprightRot, JPH::EActivation::DontActivate);
}

Zenith_Physics::RaycastResult Zenith_Physics::Raycast(const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDirection, float fMaxDistance)
{
	RaycastResult xResult;
	xResult.m_bHit = false;

	if (!s_pxPhysicsSystem)
	{
		return xResult;
	}

	// Normalize direction
	Zenith_Maths::Vector3 xNormDir = Zenith_Maths::Normalize(xDirection);

	// Create ray
	JPH::RRayCast xRay;
	xRay.mOrigin = JPH::RVec3(xOrigin.x, xOrigin.y, xOrigin.z);
	xRay.mDirection = JPH::Vec3(xNormDir.x * fMaxDistance, xNormDir.y * fMaxDistance, xNormDir.z * fMaxDistance);

	// Cast ray
	JPH::RayCastResult xHit;
	const JPH::NarrowPhaseQuery& xQuery = s_pxPhysicsSystem->GetNarrowPhaseQuery();

	if (xQuery.CastRay(xRay, xHit))
	{
		xResult.m_bHit = true;
		xResult.m_fDistance = xHit.mFraction * fMaxDistance;

		// Calculate hit point
		JPH::RVec3 xHitPoint = xRay.GetPointOnRay(xHit.mFraction);
		xResult.m_xHitPoint = Zenith_Maths::Vector3(
			static_cast<float>(xHitPoint.GetX()),
			static_cast<float>(xHitPoint.GetY()),
			static_cast<float>(xHitPoint.GetZ()));

		// Get entity from body
		JPH::BodyLockRead xLock(s_pxPhysicsSystem->GetBodyLockInterface(), xHit.mBodyID);
		if (xLock.Succeeded())
		{
			const JPH::Body& xBody = xLock.GetBody();
			xResult.m_xHitEntity = Zenith_EntityID::FromPacked(xBody.GetUserData());

			// Get surface normal
			JPH::Vec3 xNormal = xBody.GetWorldSpaceSurfaceNormal(xHit.mSubShapeID2, xHitPoint);
			xResult.m_xHitNormal = Zenith_Maths::Vector3(xNormal.GetX(), xNormal.GetY(), xNormal.GetZ());
		}
	}

	return xResult;
}

JPH::ValidateResult Zenith_Physics::PhysicsContactListener::OnContactValidate(
	const JPH::Body&, const JPH::Body&,
	JPH::RVec3Arg, const JPH::CollideShapeResult&)
{
	return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void Zenith_Physics::PhysicsContactListener::OnContactAdded(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold&, JPH::ContactSettings&)
{
	// Queue event for deferred processing (thread-safe)
	Zenith_EntityID xEntityID1 = Zenith_EntityID::FromPacked(inBody1.GetUserData());
	Zenith_EntityID xEntityID2 = Zenith_EntityID::FromPacked(inBody2.GetUserData());
	QueueCollisionEventInternal(xEntityID1, xEntityID2, COLLISION_EVENT_TYPE_START);
}

void Zenith_Physics::PhysicsContactListener::OnContactPersisted(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold&, JPH::ContactSettings&)
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
