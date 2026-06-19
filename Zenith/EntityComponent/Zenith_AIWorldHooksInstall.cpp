#include "Zenith.h"
#include "AI/Zenith_AIWorldHooks.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_AIAgentComponent.h"
#include "AI/Navigation/Zenith_NavMeshAgent.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#endif

// Engine-side wiring of Zenith_AIWorldHooks. These captureless thunks forward the
// AI leaf's needs (entity transform read/write, collider body, NavMeshAgent
// resolution, TOOLS debug-draw) to the concrete components / Flux — so the AI leaf
// names none of them. Installed once by Zenith_Engine::Initialise via
// Zenith_AI_InstallWorldHooks(). Mirrors Zenith_RegisterEngineComponents (the same
// inversion-of-control that keeps the ECS leaf clean).

namespace
{
	// Resolve an entity handle from its EntityID across all loaded scenes (Unity
	// GameObject.scene parity). Returns an invalid handle if the id is stale / gone.
	Zenith_Entity ResolveEntity(Zenith_EntityID xEntityID)
	{
		Zenith_SceneData* pxSceneData = Zenith_SceneSystem::Get().GetSceneDataForEntity(xEntityID);
		if (!pxSceneData)
		{
			return Zenith_Entity();
		}
		return pxSceneData->TryGetEntity(xEntityID);
	}

	bool GetEntityPosition(Zenith_EntityID xEntityID, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_TransformComponent>())
		{
			return false;
		}
		xEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xOut);
		return true;
	}

	bool GetEntityRotation(Zenith_EntityID xEntityID, Zenith_Maths::Quat& xOut)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_TransformComponent>())
		{
			return false;
		}
		xEntity.GetComponent<Zenith_TransformComponent>().GetRotation(xOut);
		return true;
	}

	void SetEntityPosition(Zenith_EntityID xEntityID, const Zenith_Maths::Vector3& xPos)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (xEntity.IsValid() && xEntity.HasComponent<Zenith_TransformComponent>())
		{
			xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
		}
	}

	void SetEntityRotation(Zenith_EntityID xEntityID, const Zenith_Maths::Quat& xRot)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (xEntity.IsValid() && xEntity.HasComponent<Zenith_TransformComponent>())
		{
			xEntity.GetComponent<Zenith_TransformComponent>().SetRotation(xRot);
		}
	}

	bool GetEntityColliderBody(Zenith_EntityID xEntityID, Zenith_PhysicsBodyID& xOutBody, bool& bOutDynamic)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_ColliderComponent>())
		{
			return false;
		}
		Zenith_ColliderComponent& xCollider = xEntity.GetComponent<Zenith_ColliderComponent>();
		if (!xCollider.HasValidBody())
		{
			return false;
		}
		xOutBody = xCollider.GetBodyID();
		bOutDynamic = (xCollider.GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC);
		return true;
	}

	Zenith_NavMeshAgent* GetNavMeshAgent(Zenith_EntityID xEntityID)
	{
		Zenith_Entity xEntity = ResolveEntity(xEntityID);
		if (!xEntity.IsValid() || !xEntity.HasComponent<Zenith_AIAgentComponent>())
		{
			return nullptr;
		}
		return xEntity.GetComponent<Zenith_AIAgentComponent>().GetNavMeshAgent();
	}

	void RunDataParallel(void (*pfnInvoke)(void*, u_int, u_int), void* pUserData, u_int uCount)
	{
		// Run the batch on the engine task system (calling thread joins). Used by
		// batch pathfinding. The profile index tags the work as AI pathfinding.
		Zenith_DataParallelTask xTask(ZENITH_PROFILE_ZONE("AI Pathfinding"), pfnInvoke, pUserData, uCount, /*bCallingThreadJoins=*/true);
		g_xEngine.Tasks().SubmitDataParallelTask(&xTask);
		xTask.WaitUntilComplete();
	}

#ifdef ZENITH_TOOLS
	void DebugDrawLine(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xColor, float fThickness)
	{
		g_xEngine.Primitives().AddLine(xA, xB, xColor, fThickness);
	}
	void DebugDrawSphere(const Zenith_Maths::Vector3& xCenter, float fRadius, const Zenith_Maths::Vector3& xColor)
	{
		g_xEngine.Primitives().AddSphere(xCenter, fRadius, xColor);
	}
	void DebugDrawCross(const Zenith_Maths::Vector3& xCenter, float fSize, const Zenith_Maths::Vector3& xColor)
	{
		g_xEngine.Primitives().AddCross(xCenter, fSize, xColor);
	}
	void DebugDrawTriangle(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB, const Zenith_Maths::Vector3& xC, const Zenith_Maths::Vector3& xColor)
	{
		g_xEngine.Primitives().AddTriangle(xA, xB, xC, xColor);
	}
#endif
}

// Called from Zenith_Engine::Initialise (forward-declared there, like
// Zenith_AI_RegisterComponents). Builds the hook table and installs it on the leaf.
void Zenith_AI_InstallWorldHooks()
{
	Zenith_AIWorldHooks xHooks;
	xHooks.m_pfnGetEntityPosition    = &GetEntityPosition;
	xHooks.m_pfnGetEntityRotation    = &GetEntityRotation;
	xHooks.m_pfnSetEntityPosition    = &SetEntityPosition;
	xHooks.m_pfnSetEntityRotation    = &SetEntityRotation;
	xHooks.m_pfnGetEntityColliderBody = &GetEntityColliderBody;
	xHooks.m_pfnGetNavMeshAgent      = &GetNavMeshAgent;
	xHooks.m_pfnRunDataParallel      = &RunDataParallel;
#ifdef ZENITH_TOOLS
	xHooks.m_pfnDebugDrawLine     = &DebugDrawLine;
	xHooks.m_pfnDebugDrawSphere   = &DebugDrawSphere;
	xHooks.m_pfnDebugDrawCross    = &DebugDrawCross;
	xHooks.m_pfnDebugDrawTriangle = &DebugDrawTriangle;
#endif
	Zenith_AI_SetWorldHooks(xHooks);
}
