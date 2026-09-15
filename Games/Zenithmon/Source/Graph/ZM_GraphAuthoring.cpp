#include "Zenith.h"

#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"
#include "Zenithmon/Source/Data/ZM_TrainerData.h"

#include "EntityComponent/Zenith_EngineGraphBuilder.h"
#include "Scripting/Zenith_GraphBuilder.h"

void BuildGraph_ZM_TrainerChallenge(Zenith_GraphBuilder& xBuilder)
{
	Zenith_EngineGraphBuilder xB(xBuilder);

	// Three nodes: the custom event stores its payload, GetVariable reads that
	// declared INT32 slot, and the push node receives a typed TrainerId wire. The
	// pure GetVariable is safe on this synchronous chain.
	//
	// NO Wait / Timer / Cooldown / RandomFloat / RandomInt NODE MAY BE ADDED TO THIS
	// CHAIN. It runs synchronously inside the fire site's stack frame, where
	// Zenith_GraphComponent::FireCustomEventWithArgs sets m_fTimeSeconds but NEVER
	// m_fDt (Zenith_GraphComponent.cpp:275-282 vs :188), so every dt-integrating
	// node reads zero. A timed beat is SC8+ and must ride an OnUpdate anchor or take
	// dt as a float payload.
	Zenith_PropertyValue xNone;
	xNone.SetInt32(static_cast<int32_t>(ZM_TRAINER_NONE));
	xBuilder.Variable(szZM_GRAPH_VAR_TRAINER_ID, xNone);
	const u_int uGetTrainerId = xB.Node("GetVariable");
	xB.ParamString(uGetTrainerId, "m_strVariable", szZM_GRAPH_VAR_TRAINER_ID);
	const u_int uPush = xB.Node(szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE);
	xB.Raw().DataEdge(uGetTrainerId, "Value", uPush, "TrainerId");
	xB.OnCustomEvent(szZM_GRAPH_EVENT_TRAINER_SPOTTED, szZM_GRAPH_VAR_TRAINER_ID)
		.Then(uPush);
}
