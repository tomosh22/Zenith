#pragma once

#include "ZenithECS/Zenith_Entity.h"     // Zenith_EntityID
#include "Maths/Zenith_Maths.h"          // Vector3 / Quat
#include "Physics/Zenith_Physics_Fwd.h"  // Zenith_PhysicsBodyID (value type — AI->Physics is an allowed sibling-leaf edge)

class Zenith_NavMeshAgent;

// =============================================================================
// Zenith_AIWorldHooks -- leaf-safe runtime hooks for the AI core (mirrors
// Zenith_ECSRuntimeHooks).
//
// The AI core (Zenith/AI) is a strict leaf over ZenithBase + ZenithECS +
// ZenithPhysics: it must NOT name a concrete component (Zenith_TransformComponent /
// Zenith_ColliderComponent / Zenith_AIAgentComponent), must NOT name Flux, and must
// NOT reach the g_xEngine singleton. A handful of AI operations nonetheless need
// engine-side data: an entity's world transform, its collider's physics body, the
// Zenith_NavMeshAgent owned by the (engine-side) Zenith_AIAgentComponent, and
// TOOLS-only debug-draw through Flux_Primitives.
//
// The engine installs captureless function pointers (via Zenith_AI_SetWorldHooks)
// that forward to those concrete subsystems; the AI core invokes only the
// convenience accessors below. Every pointer defaults to nullptr; the documented
// null-semantics make an un-installed hook a safe no-op (Get* return false), so the
// AI core behaves identically whether or not the engine has wired the hooks yet --
// which is exactly what SentinelAI proves.
// =============================================================================

struct Zenith_AIWorldHooks
{
	// Transform read/write -- the engine thunk resolves the entity's owning scene +
	// Zenith_TransformComponent. Get* return false (out-param untouched) when the
	// entity has no transform / is a stale handle / no hook is installed.
	bool (*m_pfnGetEntityPosition)(Zenith_EntityID, Zenith_Maths::Vector3&) = nullptr;
	bool (*m_pfnGetEntityRotation)(Zenith_EntityID, Zenith_Maths::Quat&) = nullptr;
	void (*m_pfnSetEntityPosition)(Zenith_EntityID, const Zenith_Maths::Vector3&) = nullptr;
	void (*m_pfnSetEntityRotation)(Zenith_EntityID, const Zenith_Maths::Quat&) = nullptr;

	// Collider body resolution -- the entity's Zenith_ColliderComponent physics body
	// + whether it is a dynamic body. Returns false when there is no valid body.
	bool (*m_pfnGetEntityColliderBody)(Zenith_EntityID, Zenith_PhysicsBodyID&, bool&) = nullptr;

	// The Zenith_NavMeshAgent owned by the entity's (engine-side) Zenith_AIAgentComponent.
	// null when the entity has no agent. Returns a leaf type so callers stay leaf-clean.
	Zenith_NavMeshAgent* (*m_pfnGetNavMeshAgent)(Zenith_EntityID) = nullptr;

	// Run a data-parallel batch (used by batch pathfinding) on the engine task
	// system (Zenith_TaskSystem lives engine-side / reaches g_xEngine, so the AI
	// leaf must not name it). null => the accessor runs the invocations
	// synchronously on the calling thread, so the leaf needs no task system at all
	// (what SentinelAI relies on). Signature matches Zenith_DataParallelTask's func.
	void (*m_pfnRunDataParallel)(void (*)(void*, u_int, u_int), void*, u_int) = nullptr;

	// TOOLS debug-draw -- forward to Flux_Primitives engine-side. No-op when null.
	void (*m_pfnDebugDrawLine)(const Zenith_Maths::Vector3&, const Zenith_Maths::Vector3&, const Zenith_Maths::Vector3&, float) = nullptr;
	void (*m_pfnDebugDrawSphere)(const Zenith_Maths::Vector3&, float, const Zenith_Maths::Vector3&) = nullptr;
	void (*m_pfnDebugDrawCross)(const Zenith_Maths::Vector3&, float, const Zenith_Maths::Vector3&) = nullptr;
	void (*m_pfnDebugDrawTriangle)(const Zenith_Maths::Vector3&, const Zenith_Maths::Vector3&, const Zenith_Maths::Vector3&, const Zenith_Maths::Vector3&) = nullptr;
};

// Composition-root install (Zenith_Engine::Initialise). Copies the struct.
void Zenith_AI_SetWorldHooks(const Zenith_AIWorldHooks& xHooks);

// Null-safe convenience accessors the AI core calls.
bool Zenith_AI_GetEntityPosition(Zenith_EntityID xEntity, Zenith_Maths::Vector3& xOut);
bool Zenith_AI_GetEntityRotation(Zenith_EntityID xEntity, Zenith_Maths::Quat& xOut);
void Zenith_AI_SetEntityPosition(Zenith_EntityID xEntity, const Zenith_Maths::Vector3& xPos);
void Zenith_AI_SetEntityRotation(Zenith_EntityID xEntity, const Zenith_Maths::Quat& xRot);
bool Zenith_AI_GetEntityColliderBody(Zenith_EntityID xEntity, Zenith_PhysicsBodyID& xOutBody, bool& bOutDynamic);
Zenith_NavMeshAgent* Zenith_AI_GetNavMeshAgent(Zenith_EntityID xEntity);
// Runs pfnInvoke(pUserData, i, uCount) for i in [0,uCount) — data-parallel via the
// engine task system when wired, else synchronously on the calling thread.
void Zenith_AI_RunDataParallel(void (*pfnInvoke)(void*, u_int, u_int), void* pUserData, u_int uCount);

#ifdef ZENITH_TOOLS
void Zenith_AI_DebugDrawLine(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xColor, float fThickness);
void Zenith_AI_DebugDrawSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor);
void Zenith_AI_DebugDrawCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor);
void Zenith_AI_DebugDrawTriangle(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xC, const Zenith_Maths::Vector3& xColor);
#endif
