#pragma once

class Zenith_GraphBuilder;

// ============================================================================
// ZM_GraphAuthoring (S7 item 3 SC7) -- Zenithmon's Behaviour Graph authoring
// surface: the BuildGraph_ZM_* functions plus the name strings they and the
// runtime must agree on.
//
// EVERY NAME IS SPELLED EXACTLY ONCE, HERE. Node type names, event names and
// blackboard variable names are NAME-STRING references with NO compile-time
// checking anywhere in the engine: a typo yields a node that loads, warns once and
// silently fails its chain. Sharing one constant between the builder, the node's
// property default, the fire site and the contract unit is the structural defence, and
// it is a STRONGER one than it looks: because every site reads THIS constant, renaming
// it renames the registration, the GetTypeName() override and the builder lookup
// together, so the two sides cannot diverge and there is no mismatch to catch. A
// mutation battery confirmed exactly that -- re-spelling this constant reds NOTHING,
// and correctly so.
//
// The residual hazard is therefore NOT a typo in this constant. It is a hard-coded
// string LITERAL at one site instead of this constant, which is what makes the sides
// diverge. That is caught behaviourally by
// TrainerChallengeGraph_BuildsAndResolvesEveryNodeType's Build()==true /
// HasErrors()==false pair: Zenith_GraphBuilder::Node
// (Zenith/Scripting/Zenith_GraphBuilder.cpp) looks the type up in the registry and, on
// a miss, logs "unknown node type", latches m_bErrors and returns id 0.
//
// That same mechanism is why the unit's GetUnresolvedCount()==0 clause can never fire
// for an IN-PROCESS build -- the node never reaches the definition, so the count stays
// 0 and Build() returns false instead. GetUnresolvedCount() earns its keep only against
// a stale or hand-edited LOADED .bgraph, where an unknown type survives deserialization
// as an unresolved node.
//
// * THIS FILE IS DELIBERATELY NOT #ifdef ZENITH_TOOLS, and neither is its .cpp.
// Zenith_GraphBuilder (Zenith/Scripting) and Zenith_EngineGraphBuilder
// (Zenith/EntityComponent) are CORE, not editor. Compiling unconditionally is what
// lets the boot units build the PRODUCTION definition in-process, in every config,
// with no .bgraph on disk -- the same reason RenderTest_Tennis.h:92-97 gives for
// BuildGraph_RenderTestTennisBrain. Only the AddStep_GraphBuild CALL SITE in
// Zenithmon.cpp is tools-gated.
// ============================================================================

// The one graph, shared by every trainer. Written by the tools boot, loaded at
// runtime by ZM_Interactable::EnsureTrainerChallengeGraph.
inline constexpr const char* szZM_GRAPH_TRAINER_CHALLENGE_ASSET =
	"game:Graphs/ZM_TrainerChallenge.bgraph";
// Fired synchronously from ZM_Interactable::TickTrainerSight on the FSM's
// RUN_CHALLENGE action.
inline constexpr const char* szZM_GRAPH_EVENT_TRAINER_SPOTTED = "ZM_TrainerSpotted";
// The OnCustomEvent source's m_strStorePayloadVar AND the push node's
// m_strTrainerIdVar. Both must name the same variable; one constant guarantees it.
inline constexpr const char* szZM_GRAPH_VAR_TRAINER_ID = "zmTrainerId";
// The game node's registered type name.
inline constexpr const char* szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE = "ZMPushTrainerChallenge";

// Authors the trainer challenge graph into xBuilder. THE single definition of that
// graph: the tools boot writes szZM_GRAPH_TRAINER_CHALLENGE_ASSET from it, and the
// contract units build it in-process so they can gate the shape with no .bgraph on
// disk and no prior tools boot.
void BuildGraph_ZM_TrainerChallenge(Zenith_GraphBuilder& xBuilder);
