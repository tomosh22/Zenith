#include "Zenith.h"
#include "AI/Zenith_AIDebugVariables.h"

// The toggle bools live here in the AI leaf (read by the AI debug-draw code). The
// REGISTRATION (Zenith_AIDebugVariables::Initialise) reaches g_xEngine.DebugVariables(),
// so it lives engine-side in EntityComponent/Zenith_AIDebugVarsRegistration.cpp —
// keeping this leaf TU free of g_xEngine.
namespace Zenith_AIDebugVariables
{
	// Master toggle
	bool s_bEnableAllAIDebug = true;

	// NavMesh Visualization
	bool s_bDrawNavMeshPolygons = false;
	bool s_bDrawNavMeshEdges = false;
	bool s_bDrawNavMeshBoundary = false;
	bool s_bDrawNavMeshNeighbors = false;

	// Pathfinding Visualization
	bool s_bDrawAgentPaths = true;
	bool s_bDrawPathWaypoints = false;

	// Perception Visualization
	bool s_bDrawSightCones = true;
	bool s_bDrawHearingRadius = false;
	bool s_bDrawDetectionLines = true;
	bool s_bDrawMemoryPositions = false;

	// Behavior Tree Visualization
	bool s_bDrawCurrentNode = true;
	bool s_bDrawBlackboardValues = false;

	// Squad Visualization
	bool s_bDrawFormationPositions = true;
	bool s_bDrawSquadLinks = true;
	bool s_bDrawRoleLabels = false;
	bool s_bDrawSharedTargets = true;

	// Tactical Visualization
	bool s_bDrawCoverPoints = true;
	bool s_bDrawFlankPositions = false;
	bool s_bDrawTacticalScores = false;

}
