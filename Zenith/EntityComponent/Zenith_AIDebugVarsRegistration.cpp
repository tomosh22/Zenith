#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "AI/Zenith_AIDebugVariables.h"

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Engine-side definition of Zenith_AIDebugVariables::Initialise(). The toggle
// BOOLS live in the AI leaf (AI/Zenith_AIDebugVariables.cpp); only the REGISTRATION
// (which reaches g_xEngine.DebugVariables()) lives here so the AI leaf names no
// engine singleton.
//
// The ONE caller is Zenith_Engine::InitialiseEditor(), alongside
// Zenith_GraphicsOptions::RegisterDebugVariables(). It previously had NO caller at
// all, which is why the whole AI/* subtree was missing from the panel; a game must
// not call it a second time (Add* would register duplicate paths).
namespace Zenith_AIDebugVariables
{
	void Initialise()
	{
#ifdef ZENITH_DEBUG_VARIABLES
		// One hoisted reference for the whole table: the per-file engine-singleton
		// ratchet counts every g_xEngine token.
		Zenith_DebugVariables& xDebugVars = g_xEngine.DebugVariables();

		// Master Toggle
		xDebugVars.AddBoolean({ "AI", "Enable All AI Debug" }, s_bEnableAllAIDebug);

		// Pathfinding
		xDebugVars.AddBoolean({ "AI", "Pathfinding", "Agent Paths" }, s_bDrawAgentPaths);
		xDebugVars.AddBoolean({ "AI", "Pathfinding", "Path Waypoints" }, s_bDrawPathWaypoints);

		// Perception
		xDebugVars.AddBoolean({ "AI", "Perception", "Sight Cones" }, s_bDrawSightCones);
		xDebugVars.AddBoolean({ "AI", "Perception", "Hearing Radius" }, s_bDrawHearingRadius);
		xDebugVars.AddBoolean({ "AI", "Perception", "Detection Lines" }, s_bDrawDetectionLines);
		xDebugVars.AddBoolean({ "AI", "Perception", "Memory Positions" }, s_bDrawMemoryPositions);

		// Squad
		xDebugVars.AddBoolean({ "AI", "Squad", "Formation Positions" }, s_bDrawFormationPositions);
		xDebugVars.AddBoolean({ "AI", "Squad", "Squad Links" }, s_bDrawSquadLinks);
		xDebugVars.AddBoolean({ "AI", "Squad", "Role Labels" }, s_bDrawRoleLabels);
		xDebugVars.AddBoolean({ "AI", "Squad", "Shared Targets" }, s_bDrawSharedTargets);

		// Tactical
		xDebugVars.AddBoolean({ "AI", "Tactical", "Cover Points" }, s_bDrawCoverPoints);
		xDebugVars.AddBoolean({ "AI", "Tactical", "Flank Positions" }, s_bDrawFlankPositions);
		xDebugVars.AddBoolean({ "AI", "Tactical", "Point Scores" }, s_bDrawTacticalScores);

		Zenith_Log(LOG_CATEGORY_AI, "AI debug variables registered");
#endif
	}
}
