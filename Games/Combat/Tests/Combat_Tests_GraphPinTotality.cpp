#include "Zenith.h"

// ============================================================================
// Combat_Tests_GraphPinTotality -- pin-table coverage for Combat's OWN node
// library (Games/Combat/Components/Combat_GraphNodes.h, 26 blackboard-variable
// name properties across 18 node classes).
//
// WHY THIS EXISTS AS A UNIT AND NOT AS A REVIEW HABIT. A node class with a
// m_str*Var* property and no pin descriptor is OPAQUE to
// Zenith_GraphDefinitionValidator: it contributes no writer and performs no
// read as far as any check can tell. So the failure this catches is a whole
// Combat node quietly dropping OUT of validation while every other signal --
// the characterization tests, the boot, the graph report -- stays green. Worse,
// the validator's writer set is graph-wide, so ONE un-annotated writer turns
// every downstream reader into a would-be error and would red A-8's latch.
//
// The walk itself, the registry swap and the RAII restore all live once in
// Zenith/EntityComponent/Zenith_GraphPinTotality.TestHarness.inl -- read that
// header for why the registry is SWAPPED to one registrar rather than filtered
// by category, and why the restore replays the snapshot in its original order
// (a game's rows sit at 0..N-1, so an engine-first rebuild would move them).
//
// This is a ZENITH_TEST rather than an automated test because the engine gate
// runs Combat's boot units through run_unit_gate.ps1 (combat.exe). It is the
// FIRST ZENITH_TEST in Games/Combat, so it moves Combat's pinned baseline by
// exactly the number of units in this file -- and only Combat's row: a
// game-exe-only unit cannot move Zenithmon's or RenderTest's.
// ============================================================================

#ifdef ZENITH_TESTING

#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Zenith_GraphPinTotality.TestHarness.inl"
#include "Combat/Components/Combat_GraphNodes.h"     // Combat_RegisterGraphNodes

ZENITH_TEST(GraphPinTable, CombatNodesTotality)
{
	// No exemptions: every m_str*Var* property in Combat's library names
	// exactly ONE blackboard variable (there is no comma-separated LIST
	// property here, which is the only thing the exempt list exists for).
	Zenith_CheckPinTableTotality(&Combat_RegisterGraphNodes, "Combat_GraphNodes.h", nullptr, 0u);
}

#endif // ZENITH_TESTING
