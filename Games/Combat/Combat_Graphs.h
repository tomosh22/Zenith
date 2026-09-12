#pragma once

// ============================================================================
// Combat_Graphs -- the declarations of Combat's five top-level Behaviour Graph
// builders, so a test TU can build the PRODUCTION definition in-process rather
// than reading a .bgraph off disk (the RenderTest_Tennis.h:97-103 shape).
//
// ★ EVERY DECLARATION HERE IS #ifdef ZENITH_TOOLS, because every DEFINITION is:
// the five functions live inside Combat.cpp's tools block (Combat.cpp:524),
// alongside Project_InitializeResources and the editor-automation authoring.
// Declaring them unconditionally would let a `_False` config compile a caller
// and then fail to LINK -- a break the Null_*_True gate cannot see.
//
// Only the FIVE that AddStep_GraphBuild names are here. The
// BuildCombatGameFlow_* sub-builders stay `static` in Combat.cpp: they take a
// Zenith_EngineGraphBuilder and author a fragment, not a graph, so there is no
// definition for a test to validate on its own.
// ============================================================================

#ifdef ZENITH_TOOLS

class Zenith_GraphBuilder;

// game:Graphs/Combat_PlayerAttack.bgraph - hitbox lifecycle + hit registration
// + combo push, three guards under three "AttackTick" sources.
void BuildGraph_CombatPlayerAttack(Zenith_GraphBuilder& xBuilder);

// game:Graphs/Combat_RoundFlow.bgraph - combo-timer tick then the decomposed
// win/lose decision, VICTORY chain before GAME_OVER chain.
void BuildGraph_CombatRoundFlow(Zenith_GraphBuilder& xBuilder);

// game:Graphs/Combat_PlayerState.bgraph - StateMachine(playerState) dispatch.
void BuildGraph_CombatPlayerState(Zenith_GraphBuilder& xBuilder);

// game:Graphs/Combat_EnemyBrain.bgraph - StateMachine(enemyState) dispatch.
void BuildGraph_CombatEnemyBrain(Zenith_GraphBuilder& xBuilder);

// game:Graphs/Combat_GameFlow.bgraph - menu/pause/restart/return-to-menu input.
void BuildGraph_CombatGameFlow(Zenith_GraphBuilder& xBuilder);

#endif // ZENITH_TOOLS
