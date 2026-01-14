#pragma once

/**
 * Zenith_AIDebugVariables - Debug visualization toggles for AI systems
 *
 * Registers debug variables with Zenith_DebugVariables for controlling
 * AI visualization in the editor. All variables appear under the "AI"
 * category in the Debug Variables panel.
 *
 * Usage:
 *   Call Zenith_AIDebugVariables::Initialise() during AI system startup
 *   Check s_bDrawXxx variables before rendering debug visualization
 */
namespace Zenith_AIDebugVariables
{
	// Master toggle - disables all AI debug visualization
	extern bool s_bEnableAllAIDebug;

	// NavMesh Visualization
	extern bool s_bDrawNavMeshPolygons;
	extern bool s_bDrawNavMeshEdges;
	extern bool s_bDrawNavMeshBoundary;
	extern bool s_bDrawNavMeshNeighbors;

	// Pathfinding Visualization
	extern bool s_bDrawAgentPaths;
	extern bool s_bDrawPathWaypoints;

	// Perception Visualization
	extern bool s_bDrawSightCones;
	extern bool s_bDrawHearingRadius;
	extern bool s_bDrawDetectionLines;
	extern bool s_bDrawMemoryPositions;

	// Behavior Tree Visualization
	extern bool s_bDrawCurrentNode;
	extern bool s_bDrawBlackboardValues;

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
	 * Initialize debug variables
	 * Registers all AI debug variables with Zenith_DebugVariables
	 * Call during AI system initialization
	 */
	void Initialise();
}
