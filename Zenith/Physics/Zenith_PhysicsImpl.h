#pragma once

#include "Physics/Zenith_Physics.h"
#include "Collections/Zenith_Vector.h"

// Per-Engine physics state. Phase 4 moves the heap-allocated Jolt
// objects + per-frame accumulator + deferred-collision queue +
// contact-listener instance off Zenith_Physics statics onto this Impl,
// held by Zenith_Engine. Three things stay file-static in
// Zenith_Physics.cpp by design:
//
// 1. Jolt allocation counters (s_ulJoltMemoryAllocated /
//    s_ulJoltAllocationCount) -- diagnostic atomics written by the
//    Jolt allocator from any thread, including during process-exit
//    static destruction; treated as the same carve-out as logging
//    counters.
// 2. Layer / filter implementations (BPLayerInterfaceImpl,
//    ObjectVsBroadPhaseLayerFilterImpl, ObjectLayerPairFilterImpl) --
//    these are stateless filter objects covered by the
//    "pure utility / stateless" carve-out.
// 3. PhysicsContactListener -- instance lives on the Impl as
//    m_xContactListener; the type definition stays nested in
//    Zenith_Physics for backwards include compatibility.
//
// Members are public because the static facade in Zenith_Physics.cpp
// accesses them directly. Same rationale as Zenith_ProfilingImpl.
class Zenith_PhysicsImpl
{
public:
	Zenith_PhysicsImpl() = default;
	~Zenith_PhysicsImpl() = default;

	Zenith_PhysicsImpl(const Zenith_PhysicsImpl&) = delete;
	Zenith_PhysicsImpl& operator=(const Zenith_PhysicsImpl&) = delete;

	// Heap-allocated Jolt singletons. Owned by this Impl; created by
	// Zenith_Physics::Initialise, destroyed by Zenith_Physics::Shutdown.
	JPH::TempAllocatorImpl*   m_pxTempAllocator = nullptr;
	JPH::JobSystemThreadPool* m_pxJobSystem     = nullptr;
	JPH::PhysicsSystem*       m_pxPhysicsSystem = nullptr;

	// Fixed-timestep accumulator. Zenith_Physics::Update integrates
	// fDt and steps the physics system in fixed 1/60 ticks.
	double m_fTimestepAccumulator = 0.0;

	// Contact-listener instance (registered with m_pxPhysicsSystem at
	// Initialise). Lives here so it has the same lifetime as the
	// physics system itself.
	Zenith_Physics::PhysicsContactListener m_xContactListener;

	// Deferred-collision queue. Jolt worker threads push events under
	// m_xEventQueueMutex; the main thread drains them in
	// ProcessDeferredCollisionEvents after each physics tick.
	// Zenith_Mutex_NoProfiling because the writes come from Jolt
	// worker threads that aren't registered with the Zenith threading
	// layer.
	Zenith_Vector<Zenith_Physics::DeferredCollisionEvent> m_xDeferredEvents;
	Zenith_Mutex_NoProfiling                              m_xEventQueueMutex;
	uint32_t                                              m_uDroppedEventCount = 0;

	// One-shot initialised flag (replaces file-static g_bInitialised).
	// Zenith_Physics::Initialise / Shutdown are idempotent guards.
	bool m_bInitialised = false;
};
