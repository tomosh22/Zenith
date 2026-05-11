#pragma once
/**
 * Custom BT nodes used by Priest_Behaviour.
 *
 *  - DP_BTAction_FindPosInSuspicionSphere    (writes BB.PatrolTarget)
 *  - DP_BTCondition_HasInvestigatePos        (reads BB.HasInvestigatePos)
 *  - DP_BTAction_ClearInvestigatePos         (sets BB.HasInvestigatePos=false)
 *  - DP_BTDecorator_IsTargetValid            (gates child on BB.TargetWithDevil != INVALID)
 */

#include "AI/BehaviorTree/Zenith_BTNode.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"

class DP_BTAction_FindPosInSuspicionSphere : public Zenith_BTLeaf
{
public:
	DP_BTAction_FindPosInSuspicionSphere() = default;

	BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		if (m_pxNavMesh == nullptr) return BTNodeStatus::FAILURE;
		if (!xAgent.HasComponent<Zenith_TransformComponent>()) return BTNodeStatus::FAILURE;

		Zenith_Maths::Vector3 xCenter;
		xAgent.GetComponent<Zenith_TransformComponent>().GetPosition(xCenter);

		const float fRadius = xBB.GetFloat(DP_AI::BB_KEY_SUSPICION_RADIUS, 15.0f);
		Zenith_Maths::Vector3 xResult;
		if (!m_pxNavMesh->GetRandomReachablePointInRadius(xCenter, fRadius, xResult))
		{
			return BTNodeStatus::FAILURE;
		}
		xBB.SetVector3(DP_AI::BB_KEY_PATROL_TARGET, xResult);
		return BTNodeStatus::SUCCESS;
	}

	const char* GetTypeName() const override { return "DP_FindPosInSuspicionSphere"; }

	void SetNavMesh(const Zenith_NavMesh* pxNavMesh) { m_pxNavMesh = pxNavMesh; }

private:
	const Zenith_NavMesh* m_pxNavMesh = nullptr;
};

class DP_BTCondition_HasInvestigatePos : public Zenith_BTLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		return xBB.GetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false)
			? BTNodeStatus::SUCCESS
			: BTNodeStatus::FAILURE;
	}
	const char* GetTypeName() const override { return "DP_HasInvestigatePos"; }
};

class DP_BTAction_ClearInvestigatePos : public Zenith_BTLeaf
{
public:
	BTNodeStatus Execute(Zenith_Entity& /*xAgent*/, Zenith_Blackboard& xBB, float /*fDt*/) override
	{
		xBB.SetBool(DP_AI::BB_KEY_HAS_INVESTIGATE_POS, false);
		return BTNodeStatus::SUCCESS;
	}
	const char* GetTypeName() const override { return "DP_ClearInvestigatePos"; }
};

class DP_BTDecorator_IsTargetValid : public Zenith_BTDecorator
{
public:
	BTNodeStatus Execute(Zenith_Entity& xAgent, Zenith_Blackboard& xBB, float fDt) override
	{
		const Zenith_EntityID xTgt = xBB.GetEntityID(DP_AI::BB_KEY_TARGET_WITH_DEVIL);
		if (!xTgt.IsValid()) return BTNodeStatus::FAILURE;
		if (m_pxChild == nullptr) return BTNodeStatus::FAILURE;
		return m_pxChild->Execute(xAgent, xBB, fDt);
	}
	const char* GetTypeName() const override { return "DP_IsTargetValid"; }
};
