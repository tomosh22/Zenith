// This file creates Jolt Physics objects - disable memory tracking macro to avoid conflicts
// with Jolt's custom operator new
#include "Zenith.h"
// Wave-13 PCH slim round 2: <iostream> was demoted out of Zenith.h. This is the
// only TU that uses iostream facilities (std::cout / std::endl in the Jolt trace
// + assert callbacks below) without already pulling the header in, so it carries
// the explicit include here.
#include <iostream>
#define ZENITH_PLACEMENT_NEW_ZONE
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_PhysicsMeshGenerator.h"
#include "Physics/Zenith_PhysicsWorldHooks.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Scene.h"
// Re-enter the placement-new disabled zone for the additional Jolt headers
// not already pulled in by Zenith_Physics.h (which re-enables on exit).
#ifdef ZENITH_PLACEMENT_NEW_ZONE
#define ZENITH_PHYSICS_CPP_ZONE_WAS_SET
#else
#define ZENITH_PLACEMENT_NEW_ZONE
#endif
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#ifndef ZENITH_PHYSICS_CPP_ZONE_WAS_SET
#undef ZENITH_PLACEMENT_NEW_ZONE
#endif
#undef ZENITH_PHYSICS_CPP_ZONE_WAS_SET

// Wrapper<->Jolt conversion: Zenith_PhysicsBodyID mirrors JPH::BodyID's
// single-uint32 representation, so conversion is a bit copy.
static JPH::BodyID ToJolt(Zenith_PhysicsBodyID xID)
{
	return JPH::BodyID(xID.m_uID);
}

// Self back-pointer. Zenith_Physics is a single per-engine instance (owned by
// Zenith_Engine). The leaf's free functions / file-static helpers / contact
// listener reach it through this accessor instead of g_xEngine.Physics(), so the
// Physics layer never names the engine singleton. Set in Initialise, cleared in
// Shutdown. Held as a FUNCTION-LOCAL static (the sanctioned cross-TU-singleton
// shape — a module-scope static trips the convention lint).
static Zenith_Physics*& PhysicsSelf()
{
	static Zenith_Physics* s_pxSelf = nullptr;
	return s_pxSelf;
}

// Per-Engine physics state lives on Zenith_Physics (held by Zenith_Engine as
// m_pxPhysics). What used to be declared as Zenith_Physics:: static storage is now
// an m_x member; the leaf's free functions / file-static helpers / contact listener
// reach the instance via PhysicsSelf() (above), never g_xEngine.
//
// Jolt allocation counters stay file-static below: they're diagnostic
// atomics written from the Jolt allocator on any thread (including
// during process-exit static-destruction of Jolt globals), and treated
// as the same carve-out as logging counters.

// Jolt memory tracking (carve-out: see comment above)
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

	if (!PhysicsSelf()) return;
	Zenith_Physics& xSelf = *PhysicsSelf();
	Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(xSelf.m_xEventQueueMutex);
	if (xSelf.m_xDeferredEvents.GetSize() >= uMAX_DEFERRED_COLLISION_EVENTS)
	{
		xSelf.m_uDroppedEventCount++;
		Zenith_Assert(false, "Deferred collision event queue overflow (%u events) - events are being dropped", uMAX_DEFERRED_COLLISION_EVENTS);
		return;
	}
	xSelf.m_xDeferredEvents.PushBack(xEvent);
}

void Zenith_Physics::DispatchCollisionToEntity(Zenith_Entity& xEntity, Zenith_Entity& xOtherEntity, Zenith_EntityID xOtherID, CollisionEventType eEventType)
{
	// Routed through the component-meta registry so physics names no concrete
	// component: ANY component implementing OnCollisionEnter/Stay/Exit receives
	// the event (concept-detected at registration - GraphComponent and
	// GraphComponent both qualify). Still strictly main-thread (we're inside
	// ProcessDeferredCollisionEvents).
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();
	switch (eEventType)
	{
	case COLLISION_EVENT_TYPE_START:
		xRegistry.DispatchOnCollisionEnter(xEntity, xOtherEntity);
		break;
	case COLLISION_EVENT_TYPE_STAY:
		xRegistry.DispatchOnCollisionStay(xEntity, xOtherEntity);
		break;
	case COLLISION_EVENT_TYPE_EXIT:
		xRegistry.DispatchOnCollisionExit(xEntity, xOtherID);
		break;
	}
}

void Zenith_Physics::ProcessDeferredCollisionEvents()
{
	if (m_uDroppedEventCount > 0)
	{
		Zenith_Warning(LOG_CATEGORY_PHYSICS, "Dropped %u collision events last frame due to queue overflow (max=%u)",
			m_uDroppedEventCount, uMAX_DEFERRED_COLLISION_EVENTS);
		m_uDroppedEventCount = 0;
	}

	// Swap out the events to minimize lock time
	Zenith_Vector<DeferredCollisionEvent> xEventsToProcess;
	{
		Zenith_ScopedMutexLock_T<Zenith_Mutex_NoProfiling> xLock(m_xEventQueueMutex);
		xEventsToProcess = std::move(m_xDeferredEvents);
	}

	// Process all deferred events on the main thread (safe to access scene)
	// Unity parity: dispatch collision events to all loaded scenes, not just the active scene
	for (Zenith_Vector<DeferredCollisionEvent>::Iterator xIt(xEventsToProcess); !xIt.Done(); xIt.Next())
	{
		const DeferredCollisionEvent& xEvent = xIt.GetData();

		// Look up each entity's owning scene from the global entity slot
		// Entities in a collision pair may be in different scenes
		Zenith_SceneData* pxSceneData1 = Zenith_SceneSystem::Get().GetSceneDataForEntity(xEvent.uEntityID1);
		Zenith_SceneData* pxSceneData2 = Zenith_SceneSystem::Get().GetSceneDataForEntity(xEvent.uEntityID2);

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

Zenith_Physics& Zenith_Physics::Get()
{
	Zenith_Assert(PhysicsSelf() != nullptr, "Zenith_Physics::Get() called before Initialise / after Shutdown");
	return *PhysicsSelf();
}

void Zenith_Physics::Initialise()
{
	PhysicsSelf() = this;

	if (m_bInitialised)
	{
		return;
	}
	m_bInitialised = true;
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

	m_pxTempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

	// Ensure we have at least 1 worker thread to avoid deadlock
	// Jolt Physics requires worker threads to process physics jobs
	uint32_t uNumThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
	m_pxJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, uNumThreads);

	m_pxPhysicsSystem = new JPH::PhysicsSystem();
	m_pxPhysicsSystem->Init(s_uMaxBodies, s_uNumBodyMutexes, s_uMaxBodyPairs, s_uMaxContactConstraints,
		s_xBroadPhaseLayerInterface, s_xObjectVsBroadPhaseLayerFilter, s_xObjectLayerPairFilter);

	m_pxPhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

	m_pxPhysicsSystem->SetContactListener(&m_xContactListener);
}

void Zenith_Physics::Update(float fDt)
{
	m_fTimestepAccumulator += fDt;

	while (m_fTimestepAccumulator >= s_fDesiredFramerate)
	{
		m_pxPhysicsSystem->Update(static_cast<float>(s_fDesiredFramerate), 1, m_pxTempAllocator, m_pxJobSystem);
		m_fTimestepAccumulator -= s_fDesiredFramerate;
	}

	// CRITICAL: Process deferred collision events AFTER physics update completes
	// This ensures we're on the main thread and can safely access scene data
	ProcessDeferredCollisionEvents();
}

void Zenith_Physics::Reset()
{
	Shutdown();
	Initialise();
}

void Zenith_Physics::Shutdown()
{
	PhysicsSelf() = nullptr;
	if (!m_bInitialised)
	{
		return;
	}
	m_bInitialised = false;

	if (m_pxPhysicsSystem)
	{
		delete m_pxPhysicsSystem;
		m_pxPhysicsSystem = nullptr;
	}

	if (m_pxJobSystem)
	{
		delete m_pxJobSystem;
		m_pxJobSystem = nullptr;
	}

	if (m_pxTempAllocator)
	{
		delete m_pxTempAllocator;
		m_pxTempAllocator = nullptr;
	}

	if (JPH::Factory::sInstance)
	{
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	JPH::UnregisterTypes();
}

void Zenith_Physics::SetLinearVelocity(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xVelocity)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	// Activate the body before/along with setting velocity. Jolt puts idle
	// dynamic bodies to sleep to save simulation cost; SetLinearVelocity on
	// its own does not wake them, so a body that was at rest would have
	// velocity stored but never integrate. Game code (e.g. character
	// controllers driving capsules via velocity each frame) needs the
	// body to actually move on the next physics step. Match the
	// AddForce / AddImpulse path which activates explicitly.
	//
	// Exact-zero compare here is intentional: callers requesting a stop
	// (controller releases WASD → ZeroHorizontalVelocity) pass
	// exactly Vector3(0,0,0), and any non-stop intent passes a normalized
	// direction × speed which is well above 0. There's no near-zero
	// epsilon use case in current callers; the body's existing sleep
	// timer handles the "approaching rest" decision.
	if (xVelocity.x != 0.0f || xVelocity.y != 0.0f || xVelocity.z != 0.0f)
	{
		xBodyInterface.ActivateBody(ToJolt(xBodyID));
	}
	xBodyInterface.SetLinearVelocity(ToJolt(xBodyID), JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

Zenith_Maths::Vector3 Zenith_Physics::GetLinearVelocity(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid()) return Zenith_Maths::Vector3(0, 0, 0);
	// CRITICAL FIX: Use locked interface for thread safety
	// GetBodyInterfaceNoLock() was unsafe when physics simulation runs on worker threads
	// The setter uses GetBodyInterface() so the getter must match for consistency
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	JPH::Vec3 xVel = xBodyInterface.GetLinearVelocity(ToJolt(xBodyID));
	return Zenith_Maths::Vector3(xVel.GetX(), xVel.GetY(), xVel.GetZ());
}

void Zenith_Physics::SetAngularVelocity(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xVelocity)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetAngularVelocity(ToJolt(xBodyID), JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

Zenith_Maths::Vector3 Zenith_Physics::GetAngularVelocity(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid()) return Zenith_Maths::Vector3(0, 0, 0);
	// CRITICAL FIX: Use locked interface for thread safety (matches setter)
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	JPH::Vec3 xVel = xBodyInterface.GetAngularVelocity(ToJolt(xBodyID));
	return Zenith_Maths::Vector3(xVel.GetX(), xVel.GetY(), xVel.GetZ());
}

void Zenith_Physics::AddForce(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xForce)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(m_pxPhysicsSystem != nullptr, "AddForce: Physics system not initialized");
	if (!m_pxPhysicsSystem) return;  // Defensive check for release builds

	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	// CRITICAL: Activate the body first - sleeping bodies ignore forces
	xBodyInterface.ActivateBody(ToJolt(xBodyID));
	xBodyInterface.AddForce(ToJolt(xBodyID), JPH::Vec3(xForce.x, xForce.y, xForce.z));
}

void Zenith_Physics::AddImpulse(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xImpulse)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(m_pxPhysicsSystem != nullptr, "AddImpulse: Physics system not initialized");
	if (!m_pxPhysicsSystem) return;  // Defensive check for release builds

	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	// Activate the body and apply instant velocity change
	xBodyInterface.ActivateBody(ToJolt(xBodyID));
	xBodyInterface.AddLinearVelocity(ToJolt(xBodyID), JPH::Vec3(xImpulse.x, xImpulse.y, xImpulse.z));
}

void Zenith_Physics::SetGravityEnabled(Zenith_PhysicsBodyID xBodyID, bool bEnabled)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetGravityFactor(ToJolt(xBodyID), bEnabled ? 1.0f : 0.0f);
	if (bEnabled)
	{
		xBodyInterface.ActivateBody(ToJolt(xBodyID));
	}
}

void Zenith_Physics::SetIsSensor(Zenith_PhysicsBodyID xBodyID, bool bSensor)
{
	if (xBodyID.IsInvalid()) return;
	if (m_pxPhysicsSystem == nullptr) return;
	// Jolt's SetIsSensor takes a body lock internally; safe to call from
	// the main thread between physics steps. Activate the body too --
	// sleeping bodies don't pick up the sensor-flag change until they
	// wake (sleep timer eventually wakes them but for door state
	// transitions we want the change to take effect on the next step).
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetIsSensor(ToJolt(xBodyID), bSensor);
	xBodyInterface.ActivateBody(ToJolt(xBodyID));
}

void Zenith_Physics::LockRotation(Zenith_PhysicsBodyID xBodyID, bool bLockX, bool bLockY, bool bLockZ)
{
	if (xBodyID.IsInvalid()) return;
	Zenith_Assert(m_pxPhysicsSystem != nullptr, "LockRotation: Physics system not initialized");
	if (!m_pxPhysicsSystem) return;

	bool bRotationChanged = false;
	{
		JPH::BodyLockWrite xLock(m_pxPhysicsSystem->GetBodyLockInterface(), ToJolt(xBodyID));
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
					m_pxPhysicsSystem->GetBodyInterfaceNoLock().SetRotation(ToJolt(xBodyID), xUprightRot, JPH::EActivation::DontActivate);
					bRotationChanged = true;
				}
			}
		}
	}   // body write-lock released here

	// Out-of-band rotation change (step 3): notify the engine AFTER the lock releases (the
	// hook re-reads the body via the no-lock interface) so the owning entity's cached world
	// transform invalidates this frame. Mirrors TeleportBody.
	if (bRotationChanged)
	{
		JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
		Zenith_Physics_FireBodyPoseChanged(Zenith_EntityID::FromPacked(xBodyInterface.GetUserData(ToJolt(xBodyID))));
	}
}

void Zenith_Physics::EnforceUpright(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid()) return;
	if (!m_pxPhysicsSystem) return;

	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();

	// Zero out angular velocity on X and Z axes (keep Y rotation allowed)
	JPH::Vec3 xAngVel = xBodyInterface.GetAngularVelocity(ToJolt(xBodyID));
	xAngVel.SetX(0.0f);
	xAngVel.SetZ(0.0f);
	xBodyInterface.SetAngularVelocity(ToJolt(xBodyID), xAngVel);

	// Reset rotation to upright (preserve only Y rotation/yaw)
	JPH::Quat xRot = xBodyInterface.GetRotation(ToJolt(xBodyID));
	JPH::Vec3 xForward = xRot.RotateAxisZ();
	float fYaw = JPH::ATan2(xForward.GetX(), xForward.GetZ());
	JPH::Quat xUprightRot = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), fYaw);
	xBodyInterface.SetRotation(ToJolt(xBodyID), xUprightRot, JPH::EActivation::DontActivate);

	// Out-of-band rotation change (bypasses the transform setters): notify the engine so the
	// owning entity's cached world transform invalidates THIS frame, not next (mirrors
	// TeleportBody + the LockRotation guard). EnforceUpright is called every frame for every
	// upright-locked character, so only fire when the rotation ACTUALLY changed (|dot| ~= 1
	// means the body was already upright — no real change, skip the wasted hook dispatch).
	const float fRotDot = xRot.Dot(xUprightRot);
	const bool bRotationChanged = ((fRotDot < 0.0f ? -fRotDot : fRotDot) < 1.0f - 1e-6f);
	if (bRotationChanged)
	{
		Zenith_Physics_FireBodyPoseChanged(Zenith_EntityID::FromPacked(xBodyInterface.GetUserData(ToJolt(xBodyID))));
	}
}

void Zenith_Physics::SetRestitution(Zenith_PhysicsBodyID xBodyID, float fRestitution)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetRestitution(ToJolt(xBodyID), fRestitution);
}

float Zenith_Physics::GetRestitution(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid()) return 0.f;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	return xBodyInterface.GetRestitution(ToJolt(xBodyID));
}

void Zenith_Physics::SetFriction(Zenith_PhysicsBodyID xBodyID, float fFriction)
{
	if (xBodyID.IsInvalid()) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetFriction(ToJolt(xBodyID), fFriction);
}

float Zenith_Physics::GetFriction(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid()) return 0.f;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	return xBodyInterface.GetFriction(ToJolt(xBodyID));
}

void Zenith_Physics::TeleportBody(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xPosition)
{
	if (xBodyID.IsInvalid() || m_pxPhysicsSystem == nullptr) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetPositionAndRotation(ToJolt(xBodyID),
		JPH::RVec3(xPosition.x, xPosition.y, xPosition.z),
		JPH::Quat::sIdentity(),
		JPH::EActivation::Activate);
	xBodyInterface.SetLinearVelocity(ToJolt(xBodyID), JPH::Vec3::sZero());

	// Out-of-band pose change: notify the engine so it can immediately invalidate the
	// owning entity's cached world transform (the per-frame post-physics sweep would
	// otherwise only catch this next frame, lagging the render a frame on a teleport).
	// Resolve the entity from the body user-data, exactly as Raycast does. No-op when
	// no hook is installed (physics-only / headless build).
	Zenith_EntityID xEntity = Zenith_EntityID::FromPacked(xBodyInterface.GetUserData(ToJolt(xBodyID)));
	Zenith_Physics_FireBodyPoseChanged(xEntity);
}

void Zenith_Physics::SetBodyPosition(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Vector3& xPosition)
{
	if (xBodyID.IsInvalid() || m_pxPhysicsSystem == nullptr) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetPosition(ToJolt(xBodyID), JPH::RVec3(xPosition.x, xPosition.y, xPosition.z), JPH::EActivation::Activate);
}

void Zenith_Physics::SetBodyRotation(Zenith_PhysicsBodyID xBodyID, const Zenith_Maths::Quat& xRotation)
{
	if (xBodyID.IsInvalid() || m_pxPhysicsSystem == nullptr) return;
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetRotation(ToJolt(xBodyID), JPH::Quat(xRotation.x, xRotation.y, xRotation.z, xRotation.w), JPH::EActivation::Activate);
}

Zenith_Maths::Vector3 Zenith_Physics::GetBodyPosition(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid() || m_pxPhysicsSystem == nullptr) return Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterfaceNoLock();
	JPH::Vec3 xJoltPos = xBodyInterface.GetPosition(ToJolt(xBodyID));
	return Zenith_Maths::Vector3(xJoltPos.GetX(), xJoltPos.GetY(), xJoltPos.GetZ());
}

Zenith_Maths::Quat Zenith_Physics::GetBodyRotation(Zenith_PhysicsBodyID xBodyID)
{
	if (xBodyID.IsInvalid() || m_pxPhysicsSystem == nullptr) return Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	JPH::BodyInterface& xBodyInterface = m_pxPhysicsSystem->GetBodyInterfaceNoLock();
	JPH::Quat xJoltRot = xBodyInterface.GetRotation(ToJolt(xBodyID));
	// glm::quat ctor is (w, x, y, z).
	return Zenith_Maths::Quat(xJoltRot.GetW(), xJoltRot.GetX(), xJoltRot.GetY(), xJoltRot.GetZ());
}

// Shared implementation. The body filter is supplied by the caller; passing the
// default-constructed JPH::BodyFilter() makes this equivalent to an unfiltered cast.
static Zenith_Physics::RaycastResult RaycastImpl(const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDirection, float fMaxDistance, JPH::PhysicsSystem* pxSystem, const JPH::BodyFilter& xBodyFilter)
{
	Zenith_Physics::RaycastResult xResult;
	xResult.m_bHit = false;

	if (!pxSystem)
	{
		return xResult;
	}

	Zenith_Maths::Vector3 xNormDir = Zenith_Maths::Normalize(xDirection);

	JPH::RRayCast xRay;
	xRay.mOrigin = JPH::RVec3(xOrigin.x, xOrigin.y, xOrigin.z);
	xRay.mDirection = JPH::Vec3(xNormDir.x * fMaxDistance, xNormDir.y * fMaxDistance, xNormDir.z * fMaxDistance);

	JPH::RayCastResult xHit;
	const JPH::NarrowPhaseQuery& xQuery = pxSystem->GetNarrowPhaseQuery();

	if (xQuery.CastRay(xRay, xHit, JPH::BroadPhaseLayerFilter(), JPH::ObjectLayerFilter(), xBodyFilter))
	{
		xResult.m_bHit = true;
		xResult.m_fDistance = xHit.mFraction * fMaxDistance;

		JPH::RVec3 xHitPoint = xRay.GetPointOnRay(xHit.mFraction);
		xResult.m_xHitPoint = Zenith_Maths::Vector3(
			static_cast<float>(xHitPoint.GetX()),
			static_cast<float>(xHitPoint.GetY()),
			static_cast<float>(xHitPoint.GetZ()));

		JPH::BodyLockRead xLock(pxSystem->GetBodyLockInterface(), xHit.mBodyID);
		if (xLock.Succeeded())
		{
			const JPH::Body& xBody = xLock.GetBody();
			xResult.m_xHitEntity = Zenith_EntityID::FromPacked(xBody.GetUserData());

			JPH::Vec3 xNormal = xBody.GetWorldSpaceSurfaceNormal(xHit.mSubShapeID2, xHitPoint);
			xResult.m_xHitNormal = Zenith_Maths::Vector3(xNormal.GetX(), xNormal.GetY(), xNormal.GetZ());
		}
	}

	return xResult;
}

Zenith_Physics::RaycastResult Zenith_Physics::Raycast(const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDirection, float fMaxDistance)
{
	return RaycastImpl(xOrigin, xDirection, fMaxDistance, m_pxPhysicsSystem, JPH::BodyFilter());
}

Zenith_Physics::RaycastResult Zenith_Physics::Raycast(const Zenith_Maths::Vector3& xOrigin,
	const Zenith_Maths::Vector3& xDirection, float fMaxDistance, Zenith_PhysicsBodyID xIgnoreBody)
{
	if (xIgnoreBody.IsInvalid())
	{
		return RaycastImpl(xOrigin, xDirection, fMaxDistance, m_pxPhysicsSystem, JPH::BodyFilter());
	}

	JPH::IgnoreSingleBodyFilter xFilter(ToJolt(xIgnoreBody));
	return RaycastImpl(xOrigin, xDirection, fMaxDistance, m_pxPhysicsSystem, xFilter);
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
	if (!PhysicsSelf() || !PhysicsSelf()->m_pxPhysicsSystem) return;
	const JPH::BodyLockInterface& xLockInterface = PhysicsSelf()->m_pxPhysicsSystem->GetBodyLockInterface();  // listener method: 'this' is the listener, not Zenith_Physics

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

