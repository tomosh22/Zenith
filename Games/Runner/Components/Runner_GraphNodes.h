#pragma once
/**
 * Runner_GraphNodes - Runner's Behaviour Graph node library (wave 2).
 *
 * Each node wraps EXACTLY the decision body it replaced inside
 * Runner_GameComponent::OnUpdate's PLAYING branch (proven by the
 * Runner_GameOverFlow characterization test, written against the C++ version
 * first):
 *
 *   RunnerApplyScoring  - score accumulation from the frame's collectible
 *                         pickups (the pickup detection + particle bursts stay
 *                         systems-side on the component).
 *   RunnerCheckGameOver - obstacle hit -> character OnObstacleHit + GAME_OVER
 *                         + high-score sync; character DEAD -> GAME_OVER
 *                         (verbatim, including the pre-graph quirk that the
 *                         DEAD path does NOT sync the high score).
 *
 * The component fires "RunTick" once per PLAYING frame at the point the old
 * decision block ran, preserving same-frame ordering against the particle /
 * camera / UI updates that follow it.
 *
 * Registered from Project_RegisterGameComponents via Runner_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Runner_GameComponent.h"
#include "Runner_CharacterController.h"

// Score accumulation - the pre-graph `if collected > 0: score += points`
// verbatim; the CollectionResult was computed systems-side this frame.
class RunnerNode_ApplyScoring : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Runner_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const Runner_CollectibleSpawner::CollectionResult& xResult = pxShim->GetLastCollection();
		if (xResult.m_uCollectedCount > 0)
		{
			pxShim->AddScore(xResult.m_uPointsGained);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerApplyScoring"; }
};

// Game-over decisions - obstacle hit and character-dead paths verbatim.
class RunnerNode_CheckGameOver : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Runner_GameComponent>() : nullptr;
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		if (pxShim->WasObstacleHit())
		{
			Runner_CharacterController::OnObstacleHit();
			pxShim->SetGameStateFromGraph(RunnerGameState::GAME_OVER);
			if (pxShim->GetScore() > pxShim->GetHighScore())
			{
				pxShim->SetHighScore(pxShim->GetScore());
			}
		}

		// Character dead (set by OnObstacleHit above, or by any future death
		// source). NOTE: no high-score sync on this path - pre-graph quirk
		// preserved verbatim.
		if (Runner_CharacterController::GetState() == RunnerCharacterState::DEAD)
		{
			pxShim->SetGameStateFromGraph(RunnerGameState::GAME_OVER);
		}
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerCheckGameOver"; }
};

inline void Runner_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<RunnerNode_ApplyScoring>("RunnerApplyScoring", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_CheckGameOver>("RunnerCheckGameOver", GRAPH_EVENT_NONE, 1, false, "Runner");
}
