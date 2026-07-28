#include "Zenith.h"

#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"

#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "Scripting/Zenith_GraphBuilder.h"

void BuildGraph_ZM_TrainerChallenge(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// TWO nodes. The push node's own ZENITH_PROPERTY default for m_strTrainerIdVar
	// IS szZM_GRAPH_VAR_TRAINER_ID, so no ParamString is emitted for it -- the
	// EXACT-DEFAULT RULE (Zenith_EngineGraphBuilder.h:22-27) keeps the authored blob
	// minimal and makes the node's default the single source of that name.
	//
	// NO Wait / Timer / Cooldown / RandomFloat / RandomInt NODE MAY BE ADDED TO THIS
	// CHAIN. It runs synchronously inside the fire site's stack frame, where
	// Zenith_GraphComponent::FireCustomEventWithArgs sets m_fTimeSeconds but NEVER
	// m_fDt (Zenith_GraphComponent.cpp:275-282 vs :188), so every dt-integrating
	// node reads zero. A timed beat is SC8+ and must ride an OnUpdate anchor or take
	// dt as a float payload.
	const u_int uPush = xB.Node(szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE);
	xB.OnCustomEvent(szZM_GRAPH_EVENT_TRAINER_SPOTTED, szZM_GRAPH_VAR_TRAINER_ID)
		.Then(uPush);
}
