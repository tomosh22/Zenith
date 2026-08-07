#pragma once

/**
 * Zenith_AIDebugVariables - Debug visualization toggles for AI systems
 *
 * Registers debug variables with Zenith_DebugVariables for controlling
 * AI visualization in the editor. All variables appear under the "AI"
 * category in the Debug Variables panel.
 *
 * EVERY toggle here has a live consumer, and the consumers are driven once per
 * game-logic frame by Zenith_AI::DebugDraw() (Zenith_AI.h). Do NOT add a flag
 * without the draw code that reads it: this whole namespace previously had no
 * caller for Initialise() at all, so none of the toggles even appeared in the
 * panel, and half of them had no reader if they had.
 *
 * NOT here, deliberately:
 *  - NavMesh visualisation. Zenith_NavMeshComponent's editor panel owns that
 *    (six per-component flags -> Zenith_NavMesh::DebugDraw(flags)), which is the
 *    right granularity: two navmeshes can be inspected differently at once.
 *  - Behaviour-tree visualisation. The BT runtime was deleted; decisions live in
 *    Behaviour Graphs (Zenith/Scripting/) and are inspected in the graph editor.
 */
namespace Zenith_AIDebugVariables
{
	// Master toggle - disables all AI debug visualization
	extern bool s_bEnableAllAIDebug;

	// Pathfinding Visualization
	extern bool s_bDrawAgentPaths;
	extern bool s_bDrawPathWaypoints;

	// Perception Visualization
	extern bool s_bDrawSightCones;
	extern bool s_bDrawHearingRadius;
	extern bool s_bDrawDetectionLines;
	extern bool s_bDrawMemoryPositions;

	// Squad Visualization
	extern bool s_bDrawFormationPositions;
	extern bool s_bDrawSquadLinks;
	extern bool s_bDrawRoleLabels;
	extern bool s_bDrawSharedTargets;

	// Tactical Visualization
	extern bool s_bDrawCoverPoints;
	extern bool s_bDrawFlankPositions;
	extern bool s_bDrawTacticalScores;

	/**
	 * Register all AI debug variables with Zenith_DebugVariables.
	 * Called by Zenith_Engine::InitialiseEditor() (the debug-variable
	 * composition root); defined engine-side in
	 * EntityComponent/Zenith_AIDebugVarsRegistration.cpp.
	 */
	void Initialise();
}
