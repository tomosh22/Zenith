#pragma once
/**
 * Marble_GraphNodes - Marble's Behaviour Graph node library (W1 conversion).
 *
 * Every DECISION lives in the boot-authored graphs (Marble_LevelFlow on the
 * gameplay GameManager, Marble_GameFlow on the menu GameManager); these nodes
 * are the systems seams the graphs call back into:
 *
 *   MarbleStageFrameResults - publishes the frame's systems results (the
 *                             collection result computed by
 *                             Marble_CollectibleSystem + the fall query) onto
 *                             the graph blackboard. Pure staging - the
 *                             score/win/loss decisions are engine nodes
 *                             downstream.
 *   MarbleRegenerateLevel   - level (re)creation systems body: unload the old
 *                             level scene if any, create a fresh one, run the
 *                             generator. State resets are graph-side.
 *   MarbleUnloadLevel       - level teardown systems body (clear entity refs,
 *                             unload the level scene).
 *   MarbleSetLevelPaused    - SetScenePaused on the level scene (fired from
 *                             the StateMachine's MarbleEnter_Paused /
 *                             MarbleExit_Paused transition events).
 *   MarbleApplyMenuFocus    - applies the blackboard focus index to the two
 *                             menu buttons (SetFocused visuals; the toggle
 *                             DECISION is engine nodes in Marble_GameFlow).
 *
 * Registered from Project_RegisterGameComponents via Marble_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"

#include "Marble_GameComponent.h"

namespace
{
	inline Marble_GameComponent* Marble_ResolveShim(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Marble_GameComponent>() : nullptr;
	}
}

// Publishes the frame's systems results verbatim: scoreGained/collectedDelta/
// allCollected from the stashed CollectionResult, ballFell from the fall
// query. The perception-bridge pattern - C++ computes, the graph decides.
class MarbleNode_StageFrameResults : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(MarbleNode_StageFrameResults)
public:
	ZENITH_PROPERTY(std::string, m_strScoreGainedVar, "scoreGained")
	ZENITH_PROPERTY(std::string, m_strCollectedDeltaVar, "collectedDelta")
	ZENITH_PROPERTY(std::string, m_strAllCollectedVar, "allCollected")
	ZENITH_PROPERTY(std::string, m_strBallFellVar, "ballFell")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = Marble_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const Marble_CollectibleSystem::CollectionResult& xResult = pxShim->GetLastCollection();
		Zenith_PropertyValue xValue;
		xValue.SetInt32(static_cast<int32_t>(xResult.uScoreGained));
		xContext.m_pxBlackboard->SetValue(m_strScoreGainedVar, xValue);
		xValue.SetInt32(static_cast<int32_t>(xResult.uCollectedCount));
		xContext.m_pxBlackboard->SetValue(m_strCollectedDeltaVar, xValue);
		xValue.SetBool(xResult.bAllCollected);
		xContext.m_pxBlackboard->SetValue(m_strAllCollectedVar, xValue);
		xValue.SetBool(pxShim->HasBallFallen());
		xContext.m_pxBlackboard->SetValue(m_strBallFellVar, xValue);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleStageFrameResults"; }
};

// Level (re)creation systems body (the old StartGame/ResetLevel minus their
// state writes - the R-chain resets the blackboard vars itself).
class MarbleNode_RegenerateLevel : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = Marble_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->RegenerateLevel();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleRegenerateLevel"; }
};

// Level teardown systems body (the old ReturnToMenu minus the menu-scene load
// - LoadSceneByIndex is an engine node at the end of the chain).
class MarbleNode_UnloadLevel : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = Marble_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->UnloadLevel();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleUnloadLevel"; }
};

// Pauses/resumes the LEVEL scene (the GameManager's own scene keeps running
// so the graphs can see the unpause key).
class MarbleNode_SetLevelPaused : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(MarbleNode_SetLevelPaused)
public:
	ZENITH_PROPERTY(bool, m_bPaused, true)

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Marble_GameComponent* pxShim = Marble_ResolveShim(xContext);
		if (pxShim == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxShim->SetLevelPaused(m_bPaused);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleSetLevelPaused"; }
};

// Applies the blackboard focus index to the menu buttons every frame (the
// SetFocused visuals half of the old UpdateMenuInput; the W/S toggle decision
// is engine nodes). Buttons are canvas-owned - re-found every Execute.
class MarbleNode_ApplyMenuFocus : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(MarbleNode_ApplyMenuFocus)
public:
	ZENITH_PROPERTY(std::string, m_strFocusVar, "focusIndex")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		if (!xContext.m_xSelf.IsValid()) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_UIComponent* pxUI = xContext.m_xSelf.TryGetComponent<Zenith_UIComponent>();
		if (pxUI == nullptr) return GRAPH_NODE_STATUS_FAILURE;

		const int32_t iFocus = xContext.m_pxBlackboard->GetInt32(m_strFocusVar, 0);
		Zenith_UI::Zenith_UIButton* pxPlay = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuPlay");
		Zenith_UI::Zenith_UIButton* pxQuit = pxUI->FindElement<Zenith_UI::Zenith_UIButton>("MenuQuit");
		if (pxPlay) pxPlay->SetFocused(iFocus == 0);
		if (pxQuit) pxQuit->SetFocused(iFocus == 1);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "MarbleApplyMenuFocus"; }
};

inline void Marble_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	xRegistry.RegisterNodeType<MarbleNode_StageFrameResults>("MarbleStageFrameResults", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_RegenerateLevel>("MarbleRegenerateLevel", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_UnloadLevel>("MarbleUnloadLevel", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_SetLevelPaused>("MarbleSetLevelPaused", GRAPH_EVENT_NONE, 1, false, "Marble");
	xRegistry.RegisterNodeType<MarbleNode_ApplyMenuFocus>("MarbleApplyMenuFocus", GRAPH_EVENT_NONE, 1, false, "Marble");
}
