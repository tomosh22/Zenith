#include "Zenith.h"
#include "Physics/Zenith_PhysicsWorldHooks.h"

// Leaf-side storage + fire for the Physics pose-change hook. The engine installs the
// thunk (Zenith_PhysicsWorldHooksInstall.cpp); the Physics core fires it. Mirrors
// Zenith_AIWorldHooks.cpp. Names no concrete component and no engine singleton, so it
// stays inside the ZenithPhysics leaf.

// The installed hooks. Function-local static (the sanctioned cross-TU-singleton shape —
// a module-scope static trips the convention lint), default-constructed so the pointer
// is null and the fire helper is a safe no-op until the engine wires it. This null-safe
// default is what lets SentinelPhysics drive the Physics core with no engine behind it.
static Zenith_PhysicsWorldHooks& Hooks()
{
	static Zenith_PhysicsWorldHooks s_xHooks;
	return s_xHooks;
}

void Zenith_Physics_SetWorldHooks(const Zenith_PhysicsWorldHooks& xHooks)
{
	Hooks() = xHooks;
}

void Zenith_Physics_FireBodyPoseChanged(Zenith_EntityID xEntity)
{
	if (Hooks().m_pfnOnBodyPoseChanged)
	{
		Hooks().m_pfnOnBodyPoseChanged(xEntity);
	}
}
