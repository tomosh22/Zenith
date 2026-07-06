#pragma once
/**
 * Runner_GraphNodes - Runner's Behaviour Graph node library (W1 conversion).
 *
 * Every DECISION lives in the boot-authored graphs; these nodes are the
 * systems seams the graphs call back into.
 *
 * Run-flow seams (resolve Runner_GameComponent on the GameManager):
 *   RunnerStageFrameResults - publishes the frame's systems results
 *                             (pointsGained/collectedCount/obstacleHit/
 *                             characterDead) onto the graph blackboard.
 *   RunnerKillCharacter     - Runner_CharacterController::OnObstacleHit (the
 *                             obstacle-death systems body).
 *   RunnerRegenerateRun     - run (re)creation: unload old run scene, create
 *                             fresh, initialise all systems + the character.
 *   RunnerUnloadRun         - run teardown (the escape-to-menu systems body).
 *   RunnerSetRunPaused      - SetScenePaused on the run scene (fired from the
 *                             StateMachine's RunnerEnter_Paused /
 *                             RunnerExit_Paused transition events).
 *   RunnerFocusPlayButton   - keeps the single menu button focused (the
 *                             SetFocused visuals half of the old
 *                             UpdateMenuInput).
 *
 * Character seams (resolve Runner_CharacterShim on the character entity -
 * the static-scope -> shim-wrapper pilot):
 *   RunnerTrySwitchLane     - lane-switch gate (m_iDirection); FAILURE when
 *                             clamped at the outer lane.
 *   RunnerTryJump           - jump gate (grounded + not sliding).
 *   RunnerTrySlide          - slide gate; on success seeds the blackboard
 *                             slide timer from the config duration (the
 *                             DecrementTimer pilot's arming half).
 *   RunnerEndSlide          - slide-expiry systems body (no-op unless
 *                             sliding; the countdown decides).
 *   RunnerStageCharacterFacts - publishes charState/speed for the animation
 *                             decision chain.
 *   RunnerSetAnimState      - applies the graph-decided animation state.
 *
 * Registered from Project_RegisterGameComponents via Runner_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"

#include "Runner_GameComponent.h"
#include "Runner_CharacterController.h"
#include "Runner_CharacterShim.h"

namespace
{
	inline Runner_GameComponent* Runner_ResolveGame(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Runner_GameComponent>() : nullptr;
	}

	inline Runner_CharacterShim* Runner_ResolveCharacter(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Runner_CharacterShim>() : nullptr;
	}
}

// ============================================================================
// Run-flow seams (GameManager graph)
// ============================================================================

// Publishes the frame's systems results verbatim. pointsGained is a FLOAT var
// so the score/high-score arithmetic downstream is pure engine float nodes
// (AddBlackboardFloat / MathBlackboardFloat max).
class RunnerNode_StageFrameResults : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_StageFrameResults)
public:
	ZENITH_PROPERTY(std::string, m_strPointsGainedVar, "pointsGained")
	ZENITH_PROPERTY(std::string, m_strCollectedCountVar, "collectedCount")
	ZENITH_PROPERTY(std::string, m_strObstacleHitVar, "obstacleHit")
	ZENITH_PROPERTY(std::string, m_strCharacterDeadVar, "characterDead")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = Runner_ResolveGame(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const Runner_CollectibleSpawner::CollectionResult& xResult = pxShim->GetLastCollection();
		Zenith_PropertyValue xValue;
		xValue.SetFloat(static_cast<float>(xResult.m_uPointsGained));
		xContext.m_pxBlackboard->SetValue(m_strPointsGainedVar, xValue);
		xValue.SetInt32(static_cast<int32_t>(xResult.m_uCollectedCount));
		xContext.m_pxBlackboard->SetValue(m_strCollectedCountVar, xValue);
		xValue.SetBool(pxShim->WasObstacleHit());
		xContext.m_pxBlackboard->SetValue(m_strObstacleHitVar, xValue);
		xValue.SetBool(Runner_CharacterController::GetState() == RunnerCharacterState::DEAD);
		xContext.m_pxBlackboard->SetValue(m_strCharacterDeadVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerStageFrameResults"; }
};

// The obstacle-death systems body (state write on the character module).
class RunnerNode_KillCharacter : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext&) override
	{
		Runner_CharacterController::OnObstacleHit();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerKillCharacter"; }
};

// Run (re)creation systems body (the old StartGame/ResetGame minus their
// state writes - the R chain resets the blackboard vars itself).
class RunnerNode_RegenerateRun : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = Runner_ResolveGame(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->RegenerateRun();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerRegenerateRun"; }
};

// Run teardown systems body (the menu-scene load is an engine
// LoadSceneByIndex node at the end of the graph chain).
class RunnerNode_UnloadRun : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = Runner_ResolveGame(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->UnloadRun();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerUnloadRun"; }
};

// Pauses/resumes the RUN scene (the GameManager's own scene keeps running so
// the graphs still see the resume key).
class RunnerNode_SetRunPaused : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_SetRunPaused)
public:
	ZENITH_PROPERTY(bool, m_bPaused, true)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_GameComponent* pxShim = Runner_ResolveGame(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->SetRunPaused(m_bPaused);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerSetRunPaused"; }
};

// The single menu button holds keyboard focus every frame (Enter activates
// through the UIButton's own focused-Enter path into the engine trampoline).
class RunnerNode_FocusPlayButton : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_UIComponent* pxUI = xContext.m_xSelf.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_UI::Zenith_UIButton* pxPlay = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		if (pxPlay == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxPlay->SetFocused(true);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerFocusPlayButton"; }
};

// ============================================================================
// Character seams (character-entity graph, via Runner_CharacterShim)
// ============================================================================

// Lane-switch gate: FAILURE when refused (outer-lane clamp / dead).
class RunnerNode_TrySwitchLane : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_TrySwitchLane)
public:
	ZENITH_PROPERTY(int32_t, m_iDirection, 1)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		return pxCharacter->TrySwitchLane(m_iDirection)
			? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "RunnerTrySwitchLane"; }
};

// Jump gate: FAILURE when refused (airborne / sliding / dead).
class RunnerNode_TryJump : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		return pxCharacter->TryJump()
			? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "RunnerTryJump"; }
};

// Slide gate: on success arms the blackboard countdown with the config
// duration (the graph's timer chain decrements it and fires EndSlide).
class RunnerNode_TrySlide : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_TrySlide)
public:
	ZENITH_PROPERTY(std::string, m_strTimerVar, "slideTimer")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		if (!pxCharacter->TrySlide())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		Zenith_PropertyValue xDuration;
		xDuration.SetFloat(pxCharacter->GetSlideDuration());
		xContext.m_pxBlackboard->SetValue(m_strTimerVar, xDuration);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerTrySlide"; }
};

// Slide-expiry systems body (internally gated on SLIDING - safe to fire
// every expired tick).
class RunnerNode_EndSlide : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxCharacter->EndSlide();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerEndSlide"; }
};

// Publishes the character's frame facts for the animation decision chain.
class RunnerNode_StageCharacterFacts : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_StageCharacterFacts)
public:
	ZENITH_PROPERTY(std::string, m_strStateVar, "charState")
	ZENITH_PROPERTY(std::string, m_strSpeedVar, "speed")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xValue;
		xValue.SetInt32(pxCharacter->GetCharacterState());
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xValue);
		xValue.SetFloat(pxCharacter->GetCurrentSpeed());
		xContext.m_pxBlackboard->SetValue(m_strSpeedVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerStageCharacterFacts"; }
};

// Applies the graph-decided animation state (idempotent transition).
class RunnerNode_SetAnimState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(RunnerNode_SetAnimState)
public:
	ZENITH_PROPERTY(int32_t, m_iAnimState, 0)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Runner_CharacterShim* pxCharacter = Runner_ResolveCharacter(xContext);
		if (pxCharacter == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxCharacter->SetAnimState(m_iAnimState);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "RunnerSetAnimState"; }
};

inline void Runner_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<RunnerNode_StageFrameResults>("RunnerStageFrameResults", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_KillCharacter>("RunnerKillCharacter", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_RegenerateRun>("RunnerRegenerateRun", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_UnloadRun>("RunnerUnloadRun", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_SetRunPaused>("RunnerSetRunPaused", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_FocusPlayButton>("RunnerFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_TrySwitchLane>("RunnerTrySwitchLane", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_TryJump>("RunnerTryJump", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_TrySlide>("RunnerTrySlide", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_EndSlide>("RunnerEndSlide", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_StageCharacterFacts>("RunnerStageCharacterFacts", GRAPH_EVENT_NONE, 1, false, "Runner");
	xRegistry.RegisterNodeType<RunnerNode_SetAnimState>("RunnerSetAnimState", GRAPH_EVENT_NONE, 1, false, "Runner");
}
