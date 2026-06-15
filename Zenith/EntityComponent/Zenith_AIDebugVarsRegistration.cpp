#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "AI/Zenith_AIDebugVariables.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Engine-side definition of Zenith_AIDebugVariables::Initialise(). The toggle
// BOOLS live in the AI leaf (AI/Zenith_AIDebugVariables.cpp); only the REGISTRATION
// (which reaches g_xEngine.DebugVariables()) lives here so the AI leaf names no
// engine singleton. Callers (e.g. games) still call Zenith_AIDebugVariables::Initialise()
// unchanged — it's resolved from this TU at link.
namespace Zenith_AIDebugVariables
{
	void Initialise()
	{
#ifdef ZENITH_DEBUG_VARIABLES
		// Master Toggle
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Enable All AI Debug" }, s_bEnableAllAIDebug);

		// NavMesh
		g_xEngine.DebugVariables().AddBoolean({ "AI", "NavMesh", "Polygon Surfaces" }, s_bDrawNavMeshPolygons);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "NavMesh", "Wireframe Edges" }, s_bDrawNavMeshEdges);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "NavMesh", "Boundary Edges" }, s_bDrawNavMeshBoundary);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "NavMesh", "Neighbor Links" }, s_bDrawNavMeshNeighbors);

		// Pathfinding
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Pathfinding", "Agent Paths" }, s_bDrawAgentPaths);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Pathfinding", "Path Waypoints" }, s_bDrawPathWaypoints);

		// Perception
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Perception", "Sight Cones" }, s_bDrawSightCones);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Perception", "Hearing Radius" }, s_bDrawHearingRadius);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Perception", "Detection Lines" }, s_bDrawDetectionLines);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Perception", "Memory Positions" }, s_bDrawMemoryPositions);

		// Behavior Tree
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Behavior Tree", "Current Node" }, s_bDrawCurrentNode);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Behavior Tree", "Blackboard Values" }, s_bDrawBlackboardValues);

		// Squad
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Squad", "Formation Positions" }, s_bDrawFormationPositions);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Squad", "Squad Links" }, s_bDrawSquadLinks);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Squad", "Role Labels" }, s_bDrawRoleLabels);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Squad", "Shared Targets" }, s_bDrawSharedTargets);

		// Tactical
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Tactical", "Cover Points" }, s_bDrawCoverPoints);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Tactical", "Flank Positions" }, s_bDrawFlankPositions);
		g_xEngine.DebugVariables().AddBoolean({ "AI", "Tactical", "Point Scores" }, s_bDrawTacticalScores);

		Zenith_Log(LOG_CATEGORY_AI, "AI debug variables registered");
#endif
	}
}
