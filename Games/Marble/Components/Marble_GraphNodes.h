#pragma once
/**
 * Marble_GraphNodes - Marble's Behaviour Graph node library (wave 2).
 *
 * Each node wraps EXACTLY the decision body it replaced inside
 * Marble_GameComponent::OnUpdate's PLAYING branch (proven by the Marble_*
 * characterization tests, written against the C++ version first):
 *
 *   MarbleTickTimer       - the countdown decrement + LOST at expiry (dt rides
 *                           the LevelTick event payload).
 *   MarbleApplyCollection - score/collected accumulation + all-collected -> WON
 *                           (the collection RESULT is computed systems-side by
 *                           Marble_CollectibleSystem; only the decisions live
 *                           here).
 *   MarbleCheckFall       - ball fell below the kill plane -> LOST.
 *
 * The component fires "LevelTick" once per PLAYING frame after its systems
 * pass; the chain order (timer -> collection -> fall) preserves the old
 * same-frame decision order.
 *
 * Registered from Project_RegisterGameComponents via Marble_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Marble_GameComponent.h"

// The countdown: 1 second of game time per second, clamped at 0, LOST at
// expiry. Body is the pre-graph timer block verbatim.
class MarbleNode_TickTimer : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(MarbleNode_TickTimer)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Marble_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const float fDt = xContext.m_pxBlackboard->GetFloat(m_strDtVar);
		float fTimeRemaining = pxShim->GetTimeRemaining() - fDt;
		if (fTimeRemaining <= 0.0f)
		{
			fTimeRemaining = 0.0f;
			pxShim->SetGameStateFromGraph(MarbleGameState::LOST);
		}
		pxShim->SetTimeRemaining(fTimeRemaining);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleTickTimer"; }
};

// Collection decisions: score / collected-count accumulation + the
// all-collected -> WON flip. The frame's CollectionResult was computed
// systems-side (Marble_CollectibleSystem::CheckCollectibles) and stashed on
// the component.
class MarbleNode_ApplyCollection : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Marble_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const Marble_CollectibleSystem::CollectionResult& xResult = pxShim->GetLastCollection();
		pxShim->AddScore(xResult.uScoreGained);
		pxShim->AddCollectedCount(xResult.uCollectedCount);
		if (xResult.bAllCollected)
		{
			pxShim->SetGameStateFromGraph(MarbleGameState::WON);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleApplyCollection"; }
};

// Fall decision: the ball dropped below the kill plane (systems query stashed
// on the component) -> LOST.
class MarbleNode_CheckFall : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Marble_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		if (pxShim->HasBallFallen())
		{
			pxShim->SetGameStateFromGraph(MarbleGameState::LOST);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleCheckFall"; }
};

inline void Marble_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<MarbleNode_TickTimer>("MarbleTickTimer", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_ApplyCollection>("MarbleApplyCollection", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_CheckFall>("MarbleCheckFall", GRAPH_EVENT_NONE, 1, false, "Marble");
}
