#include "Zenith.h"
#include "AI/Zenith_AIDebugVariables.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

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

	void Initialise()
	{
#ifdef ZENITH_DEBUG_VARIABLES
		// Master Toggle
		Zenith_DebugVariables::AddBoolean({ "AI", "Enable All AI Debug" }, s_bEnableAllAIDebug);

		// NavMesh
		Zenith_DebugVariables::AddBoolean({ "AI", "NavMesh", "Polygon Surfaces" }, s_bDrawNavMeshPolygons);
		Zenith_DebugVariables::AddBoolean({ "AI", "NavMesh", "Wireframe Edges" }, s_bDrawNavMeshEdges);
		Zenith_DebugVariables::AddBoolean({ "AI", "NavMesh", "Boundary Edges" }, s_bDrawNavMeshBoundary);
		Zenith_DebugVariables::AddBoolean({ "AI", "NavMesh", "Neighbor Links" }, s_bDrawNavMeshNeighbors);

		// Pathfinding
		Zenith_DebugVariables::AddBoolean({ "AI", "Pathfinding", "Agent Paths" }, s_bDrawAgentPaths);
		Zenith_DebugVariables::AddBoolean({ "AI", "Pathfinding", "Path Waypoints" }, s_bDrawPathWaypoints);

		// Perception
		Zenith_DebugVariables::AddBoolean({ "AI", "Perception", "Sight Cones" }, s_bDrawSightCones);
		Zenith_DebugVariables::AddBoolean({ "AI", "Perception", "Hearing Radius" }, s_bDrawHearingRadius);
		Zenith_DebugVariables::AddBoolean({ "AI", "Perception", "Detection Lines" }, s_bDrawDetectionLines);
		Zenith_DebugVariables::AddBoolean({ "AI", "Perception", "Memory Positions" }, s_bDrawMemoryPositions);

		// Behavior Tree
		Zenith_DebugVariables::AddBoolean({ "AI", "Behavior Tree", "Current Node" }, s_bDrawCurrentNode);
		Zenith_DebugVariables::AddBoolean({ "AI", "Behavior Tree", "Blackboard Values" }, s_bDrawBlackboardValues);

		// Squad
		Zenith_DebugVariables::AddBoolean({ "AI", "Squad", "Formation Positions" }, s_bDrawFormationPositions);
		Zenith_DebugVariables::AddBoolean({ "AI", "Squad", "Squad Links" }, s_bDrawSquadLinks);
		Zenith_DebugVariables::AddBoolean({ "AI", "Squad", "Role Labels" }, s_bDrawRoleLabels);
		Zenith_DebugVariables::AddBoolean({ "AI", "Squad", "Shared Targets" }, s_bDrawSharedTargets);

		// Tactical
		Zenith_DebugVariables::AddBoolean({ "AI", "Tactical", "Cover Points" }, s_bDrawCoverPoints);
		Zenith_DebugVariables::AddBoolean({ "AI", "Tactical", "Flank Positions" }, s_bDrawFlankPositions);
		Zenith_DebugVariables::AddBoolean({ "AI", "Tactical", "Point Scores" }, s_bDrawTacticalScores);

		Zenith_Log(LOG_CATEGORY_AI, "AI debug variables registered");
#endif
	}
}
