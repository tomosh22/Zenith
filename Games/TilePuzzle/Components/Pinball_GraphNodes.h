#pragma once
/**
 * Pinball_GraphNodes - the pinball minigame's Behaviour Graph node library
 * (wave 2).
 *
 * PinballHandleBallLost wraps EXACTLY the body of the retired
 * Pinball_GameComponent::HandleBallLost (proven by Pinball_RespawnFlow_Test,
 * written against the C++ version first): limited-ball counter decrement,
 * objective-met -> gate cleared, out-of-balls -> gate failed, otherwise
 * respawn + READY. The component fires "BallLost" from its BALL_LOST state -
 * exactly where the old call sat - and the node executes the systems
 * (gate-cleared/failed handling, ball respawn) back through it.
 *
 * Registered from Project_RegisterGameComponents via
 * Pinball_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Pinball_GameComponent.h"

class PinballNode_HandleBallLost : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Pinball_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Pinball_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		// Decrement ball counter for limited-ball objectives
		if (pxShim->IsGateActive() && pxShim->GetGateMaxBalls() > 0 && pxShim->GetBallsRemaining() > 0)
		{
			pxShim->DecrementBallsRemaining();
		}

		// Check if objective is met
		if (pxShim->IsGateActive() && pxShim->IsGateObjectiveMet())
		{
			pxShim->GateCleared();
			return GRAPH_NODE_STATUS_SUCCESS;
		}

		// Check if out of balls (gate failed)
		if (pxShim->IsGateActive() && pxShim->GetGateMaxBalls() > 0 && pxShim->GetBallsRemaining() == 0)
		{
			pxShim->GateFailed();
			return GRAPH_NODE_STATUS_SUCCESS;
		}

		// Normal respawn
		pxShim->RespawnBallFromGraph();
		pxShim->SetStateFromGraph(PINBALL_STATE_READY);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "PinballHandleBallLost"; }
};

inline void Pinball_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<PinballNode_HandleBallLost>("PinballHandleBallLost", GRAPH_EVENT_NONE, 1, false, "Pinball");
}
