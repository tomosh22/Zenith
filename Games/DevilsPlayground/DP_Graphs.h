#pragma once

// ============================================================================
// DP_Graphs.h -- declarations of DevilsPlayground's 12 top-level Behaviour
// Graph builders.
//
// Each authors ONE .bgraph through a Zenith_EngineGraphBuilder over the plain
// Zenith_GraphBuilder it is handed, and each is the SINGLE definition of that
// graph: AuthorBehaviourGraphs() in DevilsPlayground.cpp hands every one of
// them to AddStep_GraphBuild on a tools boot, and Tests/Test_GraphsValidateClean
// .cpp builds every one of them IN PROCESS to prove the validator reports zero
// would-be errors -- the mechanical precondition for latching the validator.
//
// ★ TOOLS-GATED, AND IT HAS TO BE. The definitions live inside
// DevilsPlayground.cpp's `#ifdef ZENITH_TOOLS` block (:431..:2046), because
// boot authoring is a tools-only path. Declaring them unconditionally would let
// a _False configuration reference symbols that were never compiled -- a link
// error the Null_*_True gate cannot see. The test that consumes them carries
// the same guard.
//
// The per-concern SUB-builders (BuildDPItem_* / BuildDPVillager_*) stay
// `static` in the .cpp: they are stages of one graph, not graphs, and nothing
// outside that file may author half a graph.
// ============================================================================

#ifdef ZENITH_TOOLS

class Zenith_GraphBuilder;

void BuildGraph_DPVillager(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPItem(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPForge(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPPlayerControl(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPPauseMenu(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPPriest(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPMainMenu(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPPentagram(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPChest(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPNoiseMachine(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPDoubleDoor(Zenith_GraphBuilder& xBuilder);
void BuildGraph_DPDoor(Zenith_GraphBuilder& xBuilder);

#endif // ZENITH_TOOLS
