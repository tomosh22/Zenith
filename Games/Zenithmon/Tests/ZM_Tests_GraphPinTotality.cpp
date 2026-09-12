#include "Zenith.h"

// ============================================================================
// ZM_Tests_GraphPinTotality -- the two graph-VALIDATION units for Zenithmon.
//
//   ZenithmonNodesTotality              -- every blackboard-variable-name
//                                          property in ZM's node library is
//                                          covered by a pin descriptor.
//   TrainerChallengeGraph_ValidatesClean -- the one graph ZM authors reports no
//                                          error-severity finding.
//
// WHY BOTH ARE ZENITH_TESTs. Zenithmon's gate runs its boot units
// (run_unit_gate.ps1 -Game Zenithmon), which is the home where a unit actually
// RUNS -- the same reasoning ZM_Tests_TrainerChallengeGraph.cpp:141 acts on.
// They move Zenithmon's pinned baseline by two, and ONLY Zenithmon's: a
// game-exe-only unit cannot move Combat's or RenderTest's row.
//
// WHAT THE TOTALITY WALK PROVES, why the registry is SWAPPED to ZM's registrar
// rather than filtered by category, and why the restore is RAII and replays the
// snapshot IN ORDER (ZM's rows sit at 0..N-1, ahead of every engine row) all
// live once in Zenith/EntityComponent/Zenith_GraphPinTotality.TestHarness.inl.
// Only ZM's registrar is named here.
//
// ★ THE FAILURE THIS CATCHES IS A SILENT ONE. A node class with a m_str*Var*
// property and no pin descriptor is OPAQUE to Zenith_GraphDefinitionValidator:
// it contributes no writer and performs no read that any check can see, so a
// node can drop out of validation entirely while every behavioural unit in
// ZM_Tests_TrainerChallengeGraph.cpp stays green -- they drive the beat, not
// the descriptor table.
// ============================================================================

#ifdef ZENITH_TESTING

#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"
#include "Scripting/Zenith_GraphBuilder.h"
#include "Scripting/Zenith_GraphDefinitionValidator.h"
#include "Scripting/Zenith_BehaviourGraph.h"
#include "Zenithmon/Components/ZM_GraphNodes.h"               // ZM_RegisterGraphNodes
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"         // BuildGraph_ZM_TrainerChallenge

ZENITH_TEST(GraphPinTable, ZenithmonNodesTotality)
{
	// No exemptions: ZM's single var-name property names exactly one blackboard
	// variable (the exempt list exists for a comma-separated LIST property,
	// which ZM has none of).
	Zenith_CheckPinTableTotality(&ZM_RegisterGraphNodes, "ZM_GraphNodes.h", nullptr, 0u);
}

// ---- The graph half: zero error-severity findings, so Build() stays true ------------------
//
// Built IN-PROCESS from BuildGraph_ZM_TrainerChallenge, never from disk:
// `.bgraph` files are gitignored and written only by a TOOLS boot, so a
// disk-reading unit would skip on a fresh checkout and a skip counts as a pass.

ZENITH_TEST(GraphPinTable, ZenithmonTrainerChallengeValidatesClean)
{
	Zenith_GraphDefinition xDefinition;
	Zenith_GraphBuilder xBuilder(xDefinition);
	BuildGraph_ZM_TrainerChallenge(xBuilder);

	// Build() runs the FULL-tier validation pass after the commit loop and keeps
	// the report on the builder, so the builder must stay alive below.
	const bool bBuilt = xBuilder.Build();
	ZENITH_ASSERT_TRUE(bBuilt,
		"BuildGraph_ZM_TrainerChallenge's Build() failed -- the tools boot would "
		"write a broken .bgraph");

	u_int uErrors = 0u;
	for (u_int uFinding = 0; uFinding < xBuilder.GetValidationFindingCount(); ++uFinding)
	{
		const Zenith_GraphValidationFinding& xFinding = xBuilder.GetValidationFindingAt(uFinding);
		if (xFinding.m_eSeverity != GRAPH_VALIDATION_SEVERITY_ERROR)
		{
			continue;	// LIST_NAME / DECLARED_UNUSED are warnings and stay
		}
		++uErrors;
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[ZMGraphs]   %s node=%u:%s pin=%s var=%s rule=%s | %s",
			szZM_GRAPH_TRAINER_CHALLENGE_ASSET, xFinding.m_uNodeID,
			xFinding.m_strTypeName.c_str(),
			xFinding.m_strPin.empty() ? "-" : xFinding.m_strPin.c_str(),
			xFinding.m_strVar.empty() ? "-" : xFinding.m_strVar.c_str(),
			Zenith_GraphDefinitionValidator::GetRuleName(xFinding.m_eRule),
			xFinding.m_strWhat.c_str());
	}

	ZENITH_ASSERT_EQ(uErrors, 0u,
		"%s reports an error-severity finding -- Build() latches on it and "
		"this game's tools boot reds. The lines above name the rule, the "
		"variable and the node.",
		szZM_GRAPH_TRAINER_CHALLENGE_ASSET);
}

#endif // ZENITH_TESTING
