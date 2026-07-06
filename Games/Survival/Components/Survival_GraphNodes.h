#pragma once
/**
 * Survival_GraphNodes - Survival's Behaviour Graph node library (wave 2).
 *
 * Each node wraps EXACTLY the body of the C++ decision it replaced (proven by
 * Games/Survival/Tests/Test_SurvivalCharacterization.cpp, written against the
 * C++ versions first). The DECISION logic lives in small `Graph_*` shims on
 * Survival_GameComponent; these nodes resolve the self component and call the
 * shim synchronously, so per-frame ordering and event firing are byte-identical.
 * SYSTEMS (movement, camera, resource-node visuals, HUD render, the background
 * respawn task, scene management) stay in Survival_GameComponent's OnUpdate.
 *
 * Four graphs, all on the SurvivalGame GameManager (GameFlow also on the menu
 * MenuManager):
 *   Survival_GameFlow     - menu Play / Escape->menu / R->reset / menu focus.
 *                           Order-60 input sources (OnUIButtonClicked / OnKeyPressed
 *                           / OnUpdate); the P/R/Escape gates read the per-instance
 *                           game state via SurvivalGetGameState.
 *   Survival_PlayerActions- E->harvest, 1/2->craft. Driven by "SurvivalPlayerTick"
 *                           fired inline from OnUpdate at exactly the point the old
 *                           HandleInteraction / HandleCrafting calls sat (preserving
 *                           order relative to movement + the crafting tick).
 *   Survival_CraftTick     - per-tick craft progression. Driven by "SurvivalCraftTick"
 *                           (dt payload) where UpdateCrafting ran.
 *   Survival_WorldEvents   - the event-handler decisions (inventory add + status on
 *                           harvest; collect + status on craft-complete; the empty
 *                           respawn hook - the plan's headline background-task ->
 *                           deferred-event landing spot). The C++ event subscribers
 *                           forward each event to this graph (multi-field payloads
 *                           via FireCustomEventWithArgs) at the exact fire point.
 *
 * Registered from Project_RegisterGameComponents via Survival_RegisterGraphNodes().
 */

#include "Scripting/Zenith_GraphNode.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Survival_GameComponent.h"

// Self-component resolver (self == the entity hosting the graph: the SurvivalGame
// GameManager for every Survival graph).
namespace Survival_GraphNodeDetail
{
	inline Survival_GameComponent* ResolveGame(Zenith_GraphContext& xContext)
	{
		return xContext.m_xSelf.IsValid()
			? xContext.m_xSelf.TryGetComponent<Survival_GameComponent>() : nullptr;
	}
}

// ----------------------------------------------------------------------------
// Survival_GameFlow.bgraph
// ----------------------------------------------------------------------------

// Reads the (per-instance) game state into the blackboard so the P/R/Escape
// gates can compare it. MenuManager reads MAIN_MENU (0); GameManager reads
// PLAYING (1) - so the same graph on both managers gates the in-game keys off.
class SurvivalNode_GetGameState : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SurvivalNode_GetGameState)
public:
	ZENITH_PROPERTY(std::string, m_strStateVar, "gameState")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		Zenith_PropertyValue xVal;
		xVal.SetInt32(pxGame->Graph_GetGameStateInt());
		xContext.m_pxBlackboard->SetValue(m_strStateVar, xVal);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalGetGameState"; }
};

// Escape during play -> unload the world + reload the menu (the old ReturnToMenu).
class SurvivalNode_ReturnToMenu : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ReturnToMenu();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalReturnToMenu"; }
};

// R during play -> reset player pos, resource nodes, inventory, crafting. The
// old ResetGame self-guards on PLAYING, so a stray R elsewhere is a no-op.
class SurvivalNode_ResetGame : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_ResetGame();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalResetGame"; }
};

// Keeps the menu Play button focused (the old UpdateMenuInput; a no-op in the
// gameplay scene, where there is no MenuPlay button).
class SurvivalNode_FocusPlayButton : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_FocusPlayButton();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalFocusPlayButton"; }
};

// ----------------------------------------------------------------------------
// Survival_PlayerActions.bgraph
// ----------------------------------------------------------------------------

// E -> harvest the nearest in-range resource (the old HandleInteraction body,
// including its WasInteractPressed input gate and tool-bonus logic; Hit() fires
// the harvest/deplete events internally).
class SurvivalNode_Harvest : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_Harvest();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalHarvest"; }
};

// 1/2 -> start crafting an axe / pickaxe (the old HandleCrafting body, including
// its !IsCrafting gate, the axe-before-pickaxe else-if, and the ShowNotEnough
// path). Kept as one decision node so the mutual exclusivity is byte-identical.
class SurvivalNode_HandleCrafting : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_HandleCrafting();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalHandleCrafting"; }
};

// ----------------------------------------------------------------------------
// Survival_CraftTick.bgraph
// ----------------------------------------------------------------------------

// Per-tick craft progression (the old UpdateCrafting = m_xCrafting.Update(dt)).
// dt rides the "SurvivalCraftTick" payload (custom-event context dt is 0); the
// C++ Update accumulates m_fCraftingProgress += dt - not a Timer node.
class SurvivalNode_AdvanceCraft : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SurvivalNode_AdvanceCraft)
public:
	ZENITH_PROPERTY(std::string, m_strDtVar, "payload")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_AdvanceCraft(xContext.m_pxBlackboard->GetFloat(m_strDtVar, 0.0f));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalAdvanceCraft"; }
};

// ----------------------------------------------------------------------------
// Survival_WorldEvents.bgraph
// ----------------------------------------------------------------------------

// Resource harvested -> add the yield to the inventory + show the status message
// (the old OnResourceHarvested body). itemType/amount ride the multi-field
// payload the C++ subscriber forwarder staged.
class SurvivalNode_OnResourceHarvested : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SurvivalNode_OnResourceHarvested)
public:
	ZENITH_PROPERTY(std::string, m_strItemTypeVar, "harvestItemType")
	ZENITH_PROPERTY(std::string, m_strAmountVar, "harvestAmount")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const int32_t iType = xContext.m_pxBlackboard->GetInt32(m_strItemTypeVar, 0);
		const int32_t iAmount = xContext.m_pxBlackboard->GetInt32(m_strAmountVar, 0);
		pxGame->Graph_OnResourceHarvested(static_cast<SurvivalItemType>(iType), static_cast<uint32_t>(iAmount));
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalOnResourceHarvested"; }
};

// Resource respawned -> the empty hook (matching the old OnResourceRespawned).
// The plan's headline landing spot: the background parallel task respawns a node
// off-thread, queues ResourceRespawned, the main thread drains it in
// ProcessDeferredEvents, and the forwarder fires it into this node.
class SurvivalNode_OnResourceRespawned : public Zenith_GraphNode
{
public:
	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		pxGame->Graph_OnResourceRespawned();
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalOnResourceRespawned"; }
};

// Crafting complete -> collect the crafted item + show the craft-complete message
// (the old OnCraftingComplete body, gated on m_bSuccess). itemType/success ride
// the multi-field payload the C++ subscriber forwarder staged.
class SurvivalNode_OnCraftingComplete : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(SurvivalNode_OnCraftingComplete)
public:
	ZENITH_PROPERTY(std::string, m_strItemTypeVar, "craftItemType")
	ZENITH_PROPERTY(std::string, m_strSuccessVar, "craftSuccess")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		Survival_GameComponent* pxGame = Survival_GraphNodeDetail::ResolveGame(xContext);
		if (pxGame == nullptr) return GRAPH_NODE_STATUS_FAILURE;
		const int32_t iType = xContext.m_pxBlackboard->GetInt32(m_strItemTypeVar, 0);
		const bool bSuccess = xContext.m_pxBlackboard->GetBool(m_strSuccessVar, false);
		pxGame->Graph_OnCraftingComplete(static_cast<SurvivalItemType>(iType), bSuccess);
		return GRAPH_NODE_STATUS_SUCCESS;
	}
	const char* GetTypeName() const override { return "SurvivalOnCraftingComplete"; }
};

// ----------------------------------------------------------------------------

inline void Survival_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();
	// Survival_GameFlow.bgraph (menu/escape/reset input decisions).
	xRegistry.RegisterNodeType<SurvivalNode_GetGameState>("SurvivalGetGameState", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_ReturnToMenu>("SurvivalReturnToMenu", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_ResetGame>("SurvivalResetGame", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_FocusPlayButton>("SurvivalFocusPlayButton", GRAPH_EVENT_NONE, 1, false, "Survival");
	// Survival_PlayerActions.bgraph (harvest / craft decisions).
	xRegistry.RegisterNodeType<SurvivalNode_Harvest>("SurvivalHarvest", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_HandleCrafting>("SurvivalHandleCrafting", GRAPH_EVENT_NONE, 1, false, "Survival");
	// Survival_CraftTick.bgraph (per-tick craft progression).
	xRegistry.RegisterNodeType<SurvivalNode_AdvanceCraft>("SurvivalAdvanceCraft", GRAPH_EVENT_NONE, 1, false, "Survival");
	// Survival_WorldEvents.bgraph (event-handler decisions).
	xRegistry.RegisterNodeType<SurvivalNode_OnResourceHarvested>("SurvivalOnResourceHarvested", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_OnResourceRespawned>("SurvivalOnResourceRespawned", GRAPH_EVENT_NONE, 1, false, "Survival");
	xRegistry.RegisterNodeType<SurvivalNode_OnCraftingComplete>("SurvivalOnCraftingComplete", GRAPH_EVENT_NONE, 1, false, "Survival");
}
