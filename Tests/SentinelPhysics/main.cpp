#include "Zenith.h"

// =============================================================================
// SentinelPhysics — leaf-proof executable for the ZenithPhysics extraction.
//
// This exe links ONLY zenithphysics.lib + zenithecs.lib + zenithbase.lib (see
// Build/Sharpmake_SentinelPhysics.cs). ZenithPhysics owns the Jolt backend, so
// every JPH:: symbol resolves from within zenithphysics.lib — no aggregate engine
// lib, no Flux, no AI, no concrete component. If it links and runs, the Physics
// core has no undefined ENGINE externals — i.e. ZenithPhysics really is a
// self-contained leaf over ZenithBase + ZenithECS + Jolt. A new engine-coupling
// leak (g_xEngine / Flux / a concrete component) would surface here as an
// unresolved external at link time (caught by the build gate) or in the dumpbin
// UNDEF scan that backs this proof.
//
// The driver constructs Zenith_Physics directly (the engine normally owns the
// instance), brings up Zenith_SceneSystem so the Update -> deferred-collision
// drain's Zenith_SceneSystem::Get() reference is satisfiable, then exercises the
// core surface: Initialise (Jolt factory/types/system), Update (fixed-timestep +
// deferred-event drain), Raycast (Jolt NarrowPhaseQuery), Shutdown.
// =============================================================================

#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_SceneSystem.h"

static int s_iFailures = 0;

static void Expect(bool bCond, const char* szWhat)
{
	if (!bCond)
	{
		++s_iFailures;
		Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelPhysics FAIL: %s", szWhat);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelPhysics ok:   %s", szWhat);
	}
}

int main()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"SentinelPhysics: Physics leaf-proof starting (zenithphysics.lib + zenithecs.lib + zenithbase.lib + Jolt only)");

	// Zenith_Physics::Update -> ProcessDeferredCollisionEvents reaches
	// Zenith_SceneSystem::Get(); construct the scene system so that reference has a
	// live instance behind it (the deferred queue is empty here, so Get() is not
	// actually dereferenced, but linking still requires the symbol — provided by
	// zenithecs.lib).
	Zenith_SceneSystem* pxScenes = new Zenith_SceneSystem();
	Expect(pxScenes != nullptr, "Zenith_SceneSystem constructed");

	Zenith_Physics xPhysics;
	Expect(!xPhysics.HasActiveSimulation(), "no simulation before Initialise");

	xPhysics.Initialise();
	Expect(xPhysics.HasActiveSimulation(), "Initialise created the Jolt system");
	Expect(xPhysics.GetJoltSystem() != nullptr, "GetJoltSystem returns the live Jolt PhysicsSystem");

	// Step the fixed-timestep accumulator a few times (exercises PhysicsSystem::Update
	// + the deferred-collision-event drain). No bodies/contacts -> empty drain.
	for (int i = 0; i < 4; ++i)
	{
		xPhysics.Update(1.0f / 60.0f);
	}
	Expect(true, "Update() stepped without touching the engine");

	// Raycast into an empty world (exercises the Jolt NarrowPhaseQuery path).
	Zenith_Physics::RaycastResult xHit = xPhysics.Raycast(
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 100.0f);
	Expect(!xHit.m_bHit, "Raycast into empty world misses");

	// Leaf body-creation API. Bodies without a ColliderComponent are the whole
	// point of this surface, so proving it HERE -- in an exe that links no
	// concrete component and no engine lib -- is the leaf proof for it.
	{
		const Zenith_EntityID xOwner = { 7u, 3u };
		const Zenith_PhysicsBodyID xTrunk = xPhysics.CreateStaticCapsuleBody(
			Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
			Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f),
			/*radius*/ 0.5f, /*cylinder half-height*/ 2.0f, xOwner);
		Expect(xTrunk.IsValid(), "CreateStaticCapsuleBody returned a valid body id");

		Zenith_Physics::RaycastResult xCapsuleHit = xPhysics.Raycast(
			Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 100.0f);
		Expect(xCapsuleHit.m_bHit, "Raycast finds the static capsule");
		// UserData -> hit entity is the whole reason the owner id is stamped at
		// creation: without it a contact against an instance body names nothing.
		Expect(xCapsuleHit.m_xHitEntity == xOwner,
			"the capsule's UserData resolves to the owning entity id");

		// Garbage dimensions are refused rather than half-sanitised.
		Expect(!xPhysics.CreateStaticCapsuleBody(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
			Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), -1.0f, 2.0f, xOwner).IsValid(),
			"a negative radius returns an INVALID body id");

		xPhysics.DestroyBody(xTrunk);
		Zenith_Physics::RaycastResult xAfter = xPhysics.Raycast(
			Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f), 100.0f);
		Expect(!xAfter.m_bHit, "DestroyBody removed the capsule from the world");

		// Both no-op shapes every teardown path relies on.
		xPhysics.DestroyBody(xTrunk);
		xPhysics.DestroyBody(Zenith_PhysicsBodyID());
		Expect(true, "double-destroy and destroy-INVALID are no-ops");

		xPhysics.OptimizeBroadPhase();
		Expect(true, "OptimizeBroadPhase runs on a live world");
	}

	xPhysics.Shutdown();
	Expect(!xPhysics.HasActiveSimulation(), "Shutdown freed the Jolt system");
	// The teardown-after-Shutdown shape a component destructor hits: TryGet is
	// null instead of asserting, and the destroy/optimize calls are inert.
	Expect(Zenith_Physics::TryGet() == nullptr, "TryGet returns null after Shutdown");
	xPhysics.DestroyBody(Zenith_PhysicsBodyID(0u));
	xPhysics.OptimizeBroadPhase();
	Expect(true, "DestroyBody / OptimizeBroadPhase are inert with no simulation");

	delete pxScenes;
	pxScenes = nullptr;

	if (s_iFailures == 0)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "SentinelPhysics: PASS (Physics leaf links + runs with no engine externals)");
		return 0;
	}

	Zenith_Error(LOG_CATEGORY_UNITTEST, "SentinelPhysics: FAIL (%d expectation(s) failed)", s_iFailures);
	return 1;
}
