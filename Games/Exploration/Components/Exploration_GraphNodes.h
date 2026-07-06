#pragma once
/**
 * Exploration_GraphNodes - Exploration's Behaviour Graph node library (wave 2).
 *
 * Each node wraps EXACTLY the body of the C++ decision it replaced (proven by
 * Games/Exploration/Tests/Test_ExplorationCharacterization.cpp, written against the
 * C++ versions first). The DECISION logic lives in small `Graph_*` shims on
 * Exploration_GameComponent; these nodes resolve the self component and call the
 * shim synchronously, so per-frame ordering is byte-identical. SYSTEMS (FPS
 * movement simulation, terrain sampling, sun/fog math + ApplyToEngine/Flux, HUD
 * text, FPS counter) stay in Exploration_GameComponent's OnUpdate.
 *
 * Three graphs:
 *   Exploration_GameFlow   - menu Play / Escape->menu / menu-focus DECISIONS.
 *                            Order-60 input sources (OnUIButtonClicked / OnKeyPressed
 *                            / OnUpdate); the Escape gate reads the per-instance game
 *                            state via ExplorationGetGameState. On BOTH managers.
 *   Exploration_PlayerActions - Tab debug-HUD toggle (E2).
 *   Exploration_Atmosphere - day/night time-advance + weather FSM (E3).
 *
 * Registered from Project_RegisterGameComponents via Exploration_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Exploration_GameComponent.h"

// Self-component resolver (self == the entity hosting the graph: the ExplorationGame
// MenuManager or GameManager).
namespace Exploration_GraphNodeDetail
{
	inline Exploration_GameComponent* ResolveGame(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Exploration_GameComponent>() : nullptr;
	}
}

// ----------------------------------------------------------------------------
// Exploration_GameFlow.bgraph
// ----------------------------------------------------------------------------

// Reads the (per-instance) game state into the blackboard so the Escape gate can
// compare it. MenuManager reads MAIN_MENU (0); GameManager reads PLAYING (1) - so
// the same graph on both managers gates the in-game Escape off on the menu.
class ExplorationNode_GetGameState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(ExplorationNode_GetGameState)
public:
	ZENITH_PROPERTY(std::string, m_strStateVar, "gameState")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xVal;
		xVal.SetInt32(pxGame->Graph_GetGameStateInt());
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationGetGameState"; }
};

// Escape during play -> clean up the world + reload the menu (the old ReturnToMenu).
class ExplorationNode_ReturnToMenu : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ReturnToMenu();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationReturnToMenu"; }
};

// Keeps the menu Play button focused (the old UpdateMenuInput; a no-op in the
// gameplay scene, where there is no MenuPlay button).
class ExplorationNode_FocusPlayButton : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_FocusPlayButton();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationFocusPlayButton"; }
};

// ----------------------------------------------------------------------------
// Exploration_PlayerActions.bgraph
// ----------------------------------------------------------------------------

// Tab -> toggle the debug HUD (the old OnUpdate `if (WasKeyPressed(TAB))
// ToggleDebugHUD()`; flips the standalone Exploration_UIManager namespace bool that
// UpdateUI reads @100 the same frame). Only on the GameManager (always PLAYING), so
// the old only-toggle-when-PLAYING guard is preserved structurally.
class ExplorationNode_ToggleDebugHUD : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ToggleDebugHUD();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationToggleDebugHUD"; }
};

// ----------------------------------------------------------------------------
// Exploration_Atmosphere.bgraph
// ----------------------------------------------------------------------------

// Advance the day/night time-of-day (step A of the old AtmosphereController::Update;
// gated on the cycle being enabled, wraps at 1.0). dt rides the "ExplorationAtmosphereTick"
// payload (custom-event context dt is 0); AdvanceTimeOfDay does timeOfDay += dt/duration.
class ExplorationNode_AdvanceTime : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(ExplorationNode_AdvanceTime)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_AdvanceTime(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationAdvanceTime"; }
};

// Tick the weather state machine (step B = UpdateWeather verbatim: accumulate timer,
// advance any in-flight transition + interpolate the fog-density target, and on the
// change interval pick a new random weather via the mt19937 kept in the C++ shim so
// the sequence is byte-identical). dt rides the same payload.
class ExplorationNode_TickWeather : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(ExplorationNode_TickWeather)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Exploration_GameComponent* pxGame = Exploration_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_TickWeather(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "ExplorationTickWeather"; }
};

// ----------------------------------------------------------------------------

inline void Exploration_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	// Exploration_GameFlow.bgraph (menu / escape / focus input decisions).
	xRegistry.RegisterNodeType<ExplorationNode_GetGameState>("ExplorationGetGameState", GRAPH_EVENT_NONE, 1, false, "Exploration");
	xRegistry.RegisterNodeType<ExplorationNode_ReturnToMenu>("ExplorationReturnToMenu", GRAPH_EVENT_NONE, 1, false, "Exploration");
	xRegistry.RegisterNodeType<ExplorationNode_FocusPlayButton>("ExplorationFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "Exploration");
	// Exploration_PlayerActions.bgraph (Tab debug-HUD toggle).
	xRegistry.RegisterNodeType<ExplorationNode_ToggleDebugHUD>("ExplorationToggleDebugHUD", GRAPH_EVENT_NONE, 1, false, "Exploration");
	// Exploration_Atmosphere.bgraph (day/night time-advance + weather FSM).
	xRegistry.RegisterNodeType<ExplorationNode_AdvanceTime>("ExplorationAdvanceTime", GRAPH_EVENT_NONE, 1, false, "Exploration");
	xRegistry.RegisterNodeType<ExplorationNode_TickWeather>("ExplorationTickWeather", GRAPH_EVENT_NONE, 1, false, "Exploration");
}
