#include "Zenith.h"
#include "AI/Zenith_AIWorldHooks.h"

// The installed hooks. Function-local static (the sanctioned cross-TU-singleton
// shape — a module-scope static trips the convention lint). Default-constructed:
// every pointer nullptr, so all accessors are safe no-ops until the engine wires
// them (Zenith_AI_SetWorldHooks). This null-safe default is what lets SentinelAI
// drive the AI core with no engine behind it.
static Zenith_AIWorldHooks& Hooks()
{
	static Zenith_AIWorldHooks s_xHooks;
	return s_xHooks;
}

void Zenith_AI_SetWorldHooks(const Zenith_AIWorldHooks& xHooks)
{
	Hooks() = xHooks;
}

bool Zenith_AI_GetEntityPosition(Zenith_EntityID xEntity, Zenith_Maths::Vector3& xOut)
{
	return Hooks().m_pfnGetEntityPosition ? Hooks().m_pfnGetEntityPosition(xEntity, xOut) : false;
}

bool Zenith_AI_GetEntityRotation(Zenith_EntityID xEntity, Zenith_Maths::Quat& xOut)
{
	return Hooks().m_pfnGetEntityRotation ? Hooks().m_pfnGetEntityRotation(xEntity, xOut) : false;
}

void Zenith_AI_SetEntityPosition(Zenith_EntityID xEntity, const Zenith_Maths::Vector3& xPos)
{
	if (Hooks().m_pfnSetEntityPosition) { Hooks().m_pfnSetEntityPosition(xEntity, xPos); }
}

void Zenith_AI_SetEntityRotation(Zenith_EntityID xEntity, const Zenith_Maths::Quat& xRot)
{
	if (Hooks().m_pfnSetEntityRotation) { Hooks().m_pfnSetEntityRotation(xEntity, xRot); }
}

bool Zenith_AI_GetEntityColliderBody(Zenith_EntityID xEntity, Zenith_PhysicsBodyID& xOutBody, bool& bOutDynamic)
{
	return Hooks().m_pfnGetEntityColliderBody ? Hooks().m_pfnGetEntityColliderBody(xEntity, xOutBody, bOutDynamic) : false;
}

Zenith_NavMeshAgent* Zenith_AI_GetNavMeshAgent(Zenith_EntityID xEntity)
{
	return Hooks().m_pfnGetNavMeshAgent ? Hooks().m_pfnGetNavMeshAgent(xEntity) : nullptr;
}

void Zenith_AI_RunDataParallel(void (*pfnInvoke)(void*, u_int, u_int), void* pUserData, u_int uCount)
{
	if (Hooks().m_pfnRunDataParallel)
	{
		Hooks().m_pfnRunDataParallel(pfnInvoke, pUserData, uCount);
	}
	else if (pfnInvoke)
	{
		// No engine task system wired (e.g. SentinelAI): run synchronously. Behaviour
		// matches a Zenith_DataParallelTask whose invocations all run on one thread.
		for (u_int i = 0; i < uCount; ++i) { pfnInvoke(pUserData, i, uCount); }
	}
}

#ifdef ZENITH_TOOLS
void Zenith_AI_DebugDrawLine(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xColor, float fThickness)
{
	if (Hooks().m_pfnDebugDrawLine) { Hooks().m_pfnDebugDrawLine(xA, xB, xColor, fThickness); }
}

void Zenith_AI_DebugDrawSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor)
{
	if (Hooks().m_pfnDebugDrawSphere) { Hooks().m_pfnDebugDrawSphere(xCenter, fRadius, xColor); }
}

void Zenith_AI_DebugDrawCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor)
{
	if (Hooks().m_pfnDebugDrawCross) { Hooks().m_pfnDebugDrawCross(xCenter, fSize, xColor); }
}

void Zenith_AI_DebugDrawTriangle(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xC, const Zenith_Maths::Vector3& xColor)
{
	if (Hooks().m_pfnDebugDrawTriangle) { Hooks().m_pfnDebugDrawTriangle(xA, xB, xC, xColor); }
}
#endif
