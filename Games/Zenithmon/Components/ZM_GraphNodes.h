#pragma once

#include "Core/Zenith_PropertySystem.h"                  // Zenith_PropertyValue / PROPERTY_TYPE_INT32
#include "Scripting/Zenith_GraphNode.h"                  // Zenith_GraphNode / Zenith_GraphContext / status enum
#include "Scripting/Zenith_GraphNodeRegistry.h"          // RegisterNodeType
#include "Scripting/Zenith_GraphBlackboard.h"            // TryGetValue -- the payload read
#include "Zenithmon/Components/ZM_UI_MenuStack.h"        // TryPushDialogue -- the shipped bark surface
#include "Zenithmon/Source/Data/ZM_TrainerData.h"        // the roster + ZM_SelectTrainerChallengeLines
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"    // the shared name constants

#include <string>

// ============================================================================
// ZM_GraphNodes (S7 item 3 SC7) -- Zenithmon's game-side Behaviour Graph nodes.
//
// A game node is a thin adapter over an ALREADY-SHIPPED game surface; it owns no
// policy. ZM_GraphNode_PushTrainerChallenge selects the row's lines with the same
// pure selector the C++ fire site uses and hands them to the same
// ZM_UI_MenuStack::TryPushDialogue every NPC conversation already goes through.
//
// EVERY PATH IS TOTAL AND ASSERT-FREE. These nodes execute inside BOOT UNITS
// (ZM_Tests_TrainerChallengeGraph.cpp fires the production event with no scene, no
// player and no ZM_MenuRoot on purpose), and Zenith_Assert breaks the process in
// EVERY configuration -- one assert here does not fail one test, it ends the whole
// boot unit run and takes the gate down with it. In particular
// Zenith_PropertyValue::GetInt32 ASSERTS on a type mismatch
// (Zenith_PropertySystem.h:85), so the type tag is checked BEFORE it is called.
// ============================================================================

// Ownerless process-global observation, so a unit can prove the node was REACHED
// rather than merely that nothing crashed. Deliberately NOT built on
// Zenith_BehaviourGraph::GetRecentlyExecuted: that trace is documented as cleared
// per ON_UPDATE dispatch and its behaviour under a CUSTOM-event dispatch is
// unverified (see the open question in Risks).
//
// Ownerless => convention C3: it MUST be cleared from the between-tests hook in
// Zenithmon.cpp.
struct ZM_GraphNodeTestCounters
{
	static inline u_int         s_uChallengePushAttempts   = 0u;
	static inline u_int         s_uChallengePushSucceeded  = 0u;
	static inline ZM_TRAINER_ID s_eLastChallengeTrainer    = ZM_TRAINER_NONE;

	static void ResetRuntimeStateForTests()
	{
		s_uChallengePushAttempts  = 0u;
		s_uChallengePushSucceeded = 0u;
		s_eLastChallengeTrainer   = ZM_TRAINER_NONE;
	}
};

namespace ZM_GraphNodeDetail
{
	// Reads the trainer id the fire site stashed on the blackboard. TOTAL: a missing
	// variable, a wrong type tag, a negative value and an unregistered id all yield
	// ZM_TRAINER_NONE, and none of them asserts.
	inline ZM_TRAINER_ID ResolveChallengeTrainer(Zenith_GraphContext& xContext,
		const std::string& strVar)
	{
		if (xContext.m_pxBlackboard == nullptr)
		{
			return ZM_TRAINER_NONE;
		}
		const Zenith_PropertyValue* pxValue = xContext.m_pxBlackboard->TryGetValue(strVar);
		// The TYPE CHECK IS MANDATORY, not defensive tidiness: GetInt32 asserts on a
		// mismatch, and a graph author can point m_strTrainerIdVar at any variable.
		if (pxValue == nullptr || pxValue->GetType() != PROPERTY_TYPE_INT32)
		{
			return ZM_TRAINER_NONE;
		}
		const int32_t iId = pxValue->GetInt32();
		if (iId < 0)
		{
			return ZM_TRAINER_NONE;
		}
		const ZM_TRAINER_ID eId = (ZM_TRAINER_ID)(u_int)iId;
		// ZM_IsRegisteredTrainer collapses the sentinel and every garbage value into
		// one comparison, and unlike ZM_GetTrainerData it logs nothing.
		return ZM_IsRegisteredTrainer(eId) ? eId : ZM_TRAINER_NONE;
	}
}

// Pushes the named trainer's challenge lines through the shipped dialogue surface.
// SUCCESS on a push; FAILURE (which aborts the chain, harmlessly, since it is the
// chain's last node) on an unresolvable id, a silent row, or a refused push.
class ZM_GraphNode_PushTrainerChallenge : public Zenith_GraphNode
{
public:
	ZENITH_PROPERTIES_BEGIN(ZM_GraphNode_PushTrainerChallenge)
public:
	// Default MUST equal szZM_GRAPH_VAR_TRAINER_ID -- the builder relies on the
	// EXACT-DEFAULT RULE and emits no ParamString for it.
	ZENITH_PROPERTY(std::string, m_strTrainerIdVar, "zmTrainerId")

	GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
	{
		// Bumped FIRST and unconditionally: this is the anti-vacuity handle every
		// contract unit asserts on, and it must move even on the failure arms.
		++ZM_GraphNodeTestCounters::s_uChallengePushAttempts;

		const ZM_TRAINER_ID eTrainer =
			ZM_GraphNodeDetail::ResolveChallengeTrainer(xContext, m_strTrainerIdVar);
		ZM_GraphNodeTestCounters::s_eLastChallengeTrainer = eTrainer;
		if (!ZM_IsRegisteredTrainer(eTrainer))
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}

		const char* const* paszLines = nullptr;
		u_int uCount = 0u;
		ZM_SelectTrainerChallengeLines(ZM_GetTrainerData(eTrainer), paszLines, uCount);
		if (paszLines == nullptr || uCount == 0u)
		{
			// A silent row. The C++ fire site never fires for one, so this is the
			// defensive arm only -- but it must be TOTAL, because a unit drives it.
			return GRAPH_NODE_STATUS_FAILURE;
		}

		// The SAME seam every NPC conversation uses. It resolves the ZM_MenuRoot
		// singleton itself and returns FALSE (never asserts) when there is none --
		// which is exactly the boot-unit context.
		if (!ZM_UI_MenuStack::TryPushDialogue(paszLines, uCount))
		{
			return GRAPH_NODE_STATUS_FAILURE;
		}
		++ZM_GraphNodeTestCounters::s_uChallengePushSucceeded;
		return GRAPH_NODE_STATUS_SUCCESS;
	}

	const char* GetTypeName() const override { return szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE; }
};

// Called from Project_RegisterGameComponents in EVERY configuration -- node
// registration is config-independent; only .bgraph AUTHORING is tools-only.
// Getting that wrong is invisible in every build a developer runs locally, because
// ZENITH_TOOLS is on in all four of them.
inline void ZM_RegisterGraphNodes()
{
	Zenith_GraphNodeRegistry::Get().RegisterNodeType<ZM_GraphNode_PushTrainerChallenge>(
		szZM_GRAPH_NODE_PUSH_TRAINER_CHALLENGE, GRAPH_EVENT_NONE, 1, false, "Zenithmon");
}
