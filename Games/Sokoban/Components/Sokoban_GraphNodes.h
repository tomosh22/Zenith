#pragma once
/**
 * Sokoban_GraphNodes - Sokoban's Behaviour Graph node library (W2, the first
 * zero -> full conversion).
 *
 * Every DECISION lives in the boot-authored graphs (Sokoban_LevelFlow on the
 * gameplay GameManager, Sokoban_GameFlow on the menu MenuManager); these
 * nodes are the systems seams the graphs call back into:
 *
 *   SokobanTryMove        - the move gate + systems body: refuses while won
 *                           (blackboard) or mid-step (animation), else runs
 *                           the GridLogic move/push + starts the step
 *                           animation. SUCCESS = the move happened (the
 *                           chain then counts it and refreshes the HUD).
 *   SokobanStageBoardFacts- publishes boxesOnTargets/targetCount/minMoves
 *                           (+ the preformatted "Boxes: X / Y" progress
 *                           string - the one composite the single-var
 *                           SetUIText node cannot format) onto the
 *                           blackboard for the win check and HUD chains.
 *   SokobanRegenerateLevel- fresh puzzle scene + generator + solver +
 *                           visuals (counter/state resets are graph-side).
 *   SokobanUnloadLevel    - level teardown (cancel step animation, clear
 *                           renderer IDs, unload the puzzle scene).
 *   SokobanFocusPlayButton- keeps the single menu button focused.
 *
 * Registered from Project_RegisterGameComponents via Sokoban_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"

#include "Sokoban_GameComponent.h"

#include <cstdio>

namespace
{
	inline Sokoban_GameComponent* Sokoban_ResolveShim(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Sokoban_GameComponent>() : nullptr;
	}
}

// The move gate + systems body. Direction values = SokobanDirection
// (0 up, 1 down, 2 left, 3 right).
class SokobanNode_TryMove : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SokobanNode_TryMove)
public:
	ZENITH_PROPERTY(int32_t, m_iDirection, 0)
	ZENITH_PROPERTY(std::string, m_strWonVar, "won")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Sokoban_GameComponent* pxShim = Sokoban_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		// The old gating verbatim: no input once won, none mid-step.
		if (xContext.m_pxBlackboard->GetBool(m_strWonVar, false) || pxShim->IsAnimating())
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		if (m_iDirection < 0 || m_iDirection > static_cast<int32_t>(SOKOBAN_DIR_RIGHT))
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		return pxShim->TryMoveSystems(static_cast<SokobanDirection>(m_iDirection))
			? GRAPH_NODE_STATUS_SUCCESS : GRAPH_NODE_STATUS_FAILURE;
	}
	const char* GetTypeName() const override { return "SokobanTryMove"; }
};

// Publishes the board's frame facts for the win-check and HUD chains.
class SokobanNode_StageBoardFacts : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SokobanNode_StageBoardFacts)
public:
	ZENITH_PROPERTY(std::string, m_strBoxesOnTargetsVar, "boxesOnTargets")
	ZENITH_PROPERTY(std::string, m_strTargetCountVar, "targetCount")
	ZENITH_PROPERTY(std::string, m_strMinMovesVar, "minMoves")
	ZENITH_PROPERTY(std::string, m_strProgressTextVar, "progressText")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Sokoban_GameComponent* pxShim = Sokoban_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const uint32_t uBoxesOnTargets = pxShim->GetBoxesOnTargets();
		const uint32_t uTargetCount = pxShim->GetTargetCount();

		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(uBoxesOnTargets));
		xContext.m_pxBlackboard->SetValue(m_strBoxesOnTargetsVar, xValue);
		xValue.SetInt32(static_cast<int32_t>(uTargetCount));
		xContext.m_pxBlackboard->SetValue(m_strTargetCountVar, xValue);
		xValue.SetInt32(static_cast<int32_t>(pxShim->GetMinMoves()));
		xContext.m_pxBlackboard->SetValue(m_strMinMovesVar, xValue);

		// The old Sokoban_UIManager "Boxes: X / Y" format, staged as a string
		// (SetUIText formats exactly one value var).
		char acBuffer[64];
		snprintf(acBuffer, sizeof(acBuffer), "Boxes: %u / %u", uBoxesOnTargets, uTargetCount);
		xValue.SetString(acBuffer);
		xContext.m_pxBlackboard->SetValue(m_strProgressTextVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SokobanStageBoardFacts"; }
};

// Fresh puzzle scene + generation systems body (the old StartGame /
// StartNewLevel minus their state writes - the R chain resets the vars).
class SokobanNode_RegenerateLevel : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Sokoban_GameComponent* pxShim = Sokoban_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->RegenerateLevel();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SokobanRegenerateLevel"; }
};

// Level teardown systems body (the menu-scene load is an engine
// LoadSceneByIndex node at the end of the graph chain).
class SokobanNode_UnloadLevel : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Sokoban_GameComponent* pxShim = Sokoban_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->UnloadLevel();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SokobanUnloadLevel"; }
};

// The single menu button holds keyboard focus every frame (Enter activates
// through the UIButton's own focused-Enter path into the engine trampoline).
class SokobanNode_FocusPlayButton : public Zenith_GraphNode
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
	const char* GetTypeName() const override { return "SokobanFocusPlayButton"; }
};

inline void Sokoban_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<SokobanNode_TryMove>("SokobanTryMove", GRAPH_EVENT_NONE, 1, false, "Sokoban");
	xRegistry.RegisterNodeType<SokobanNode_StageBoardFacts>("SokobanStageBoardFacts", GRAPH_EVENT_NONE, 1, false, "Sokoban");
	xRegistry.RegisterNodeType<SokobanNode_RegenerateLevel>("SokobanRegenerateLevel", GRAPH_EVENT_NONE, 1, false, "Sokoban");
	xRegistry.RegisterNodeType<SokobanNode_UnloadLevel>("SokobanUnloadLevel", GRAPH_EVENT_NONE, 1, false, "Sokoban");
	xRegistry.RegisterNodeType<SokobanNode_FocusPlayButton>("SokobanFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "Sokoban");
}
