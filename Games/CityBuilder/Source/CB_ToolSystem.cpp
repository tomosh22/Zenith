#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/CB_Bindings.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_Events.h"
#include "ZenithECS/Zenith_EventSystem.h"
#include <cmath>

const char* CB_ToolSystem::ToolName(CB_ETool eTool)
{
	switch (eTool)
	{
	case CB_TOOL_ZONE_RES:  return "Residential";
	case CB_TOOL_ZONE_COM:  return "Commercial";
	case CB_TOOL_ZONE_IND:  return "Industrial";
	case CB_TOOL_ZONE_PARK: return "Park";
	case CB_TOOL_ROAD:      return "Road";
	case CB_TOOL_POLICE:    return "Police";
	case CB_TOOL_POWER:     return "Power Plant";
	case CB_TOOL_WATER:     return "Water Tower";
	case CB_TOOL_BULLDOZE:  return "Bulldoze";
	case CB_TOOL_TERRAFORM: return "Terraform (LMB raise / RMB lower)";
	case CB_TOOL_DISTRICT:  return "District (paint; F1-F4 policies)";
	case CB_TOOL_TRANSIT:   return "Transit (LMB add stop / RMB new line)";
	case CB_TOOL_CONDUIT:   return "Utility Conduit (LMB lay)";
	default:                return "None";
	}
}

bool CB_ToolSystem::PickGroundPoint(float& fOutX, float& fOutZ) const
{
	Zenith_CameraComponent* pxCam = Zenith_GetMainCameraAcrossScenes();
	if (!pxCam)
	{
		return false;
	}
	// The cursor POSITION is not an action (no binding row can carry a position),
	// so it comes through CB_Bindings' claim-guarded device read. A false answer
	// means a UI widget owns the pointer — the pick refuses rather than building
	// the ground point under a HUD button the player is dragging on.
	Zenith_Maths::Vector2_64 xMouse;
	if (!CB_Bindings::ReadCursorPosition(xMouse))
	{
		return false;
	}

	const Zenith_Maths::Vector3 xNear = pxCam->ScreenSpaceToWorldSpace(
		Zenith_Maths::Vector3(static_cast<float>(xMouse.x), static_cast<float>(xMouse.y), 0.0f));
	const Zenith_Maths::Vector3 xFar = pxCam->ScreenSpaceToWorldSpace(
		Zenith_Maths::Vector3(static_cast<float>(xMouse.x), static_cast<float>(xMouse.y), 1.0f));

	const Zenith_Maths::Vector3 xDir = xFar - xNear;

	// Terrain-aware pick: march the cursor ray and find where it crosses the rendered terrain
	// SURFACE. The old flat-plane (y=0) intersection is correct only when the ground is flat;
	// with hills it lands tens of metres past the cursor toward the horizon (the ray reaches
	// y=0 well beyond where it actually hits the hillside), so the road/zone tools drift.
	if (m_pxTerrainField != nullptr && m_pxTerrainField->IsInitialized())
	{
		const int   iSTEPS = 1024;
		float fPrevDiff = xNear.y - m_pxTerrainField->GetRenderSurfaceY(xNear.x, xNear.z);
		float fPrevT    = 0.0f;
		for (int i = 1; i <= iSTEPS; ++i)
		{
			const float fS = static_cast<float>(i) / static_cast<float>(iSTEPS);
			const Zenith_Maths::Vector3 xP = xNear + xDir * fS;
			const float fDiff = xP.y - m_pxTerrainField->GetRenderSurfaceY(xP.x, xP.z);
			if (fPrevDiff > 0.0f && fDiff <= 0.0f)   // descended through the surface
			{
				float fa = fPrevT, fb = fS;          // bisect for the exact crossing
				for (int j = 0; j < 20; ++j)
				{
					const float fm = (fa + fb) * 0.5f;
					const Zenith_Maths::Vector3 xm = xNear + xDir * fm;
					if (xm.y - m_pxTerrainField->GetRenderSurfaceY(xm.x, xm.z) > 0.0f) { fa = fm; }
					else                                                                { fb = fm; }
				}
				const Zenith_Maths::Vector3 xHit = xNear + xDir * ((fa + fb) * 0.5f);
				fOutX = xHit.x;
				fOutZ = xHit.z;
				return true;
			}
			fPrevDiff = fDiff;
			fPrevT    = fS;
		}
		// No crossing (cursor points above the horizon / off the terrain) → fall back to y=0.
	}

	if (std::fabs(xDir.y) < 1e-6f)
	{
		return false;
	}
	const float fT = (0.0f - xNear.y) / xDir.y;  // intersect ground plane y=0
	if (fT < 0.0f)
	{
		return false;
	}
	fOutX = xNear.x + xDir.x * fT;
	fOutZ = xNear.z + xDir.z * fT;
	return true;
}

// The tool each selection ACTION picks, in lockstep with
// CB_Bindings::auTOOL_SELECT_ACTIONS. The action (i.e. which device row fires)
// belongs to CB_Bindings; the tool IDENTITY belongs here, which is why the two
// tables are parallel rather than one table in one place.
static const CB_ETool g_aeToolForSelectAction[CB_Bindings::uTOOL_SELECT_COUNT] = {
	CB_TOOL_ZONE_RES,   // Tool1
	CB_TOOL_ZONE_COM,   // Tool2
	CB_TOOL_ZONE_IND,   // Tool3
	CB_TOOL_ZONE_PARK,  // Tool4
	CB_TOOL_ROAD,       // Tool5
	CB_TOOL_POLICE,     // Tool6 (re-pressing it cycles the service sub-type — CB_RoadController)
	CB_TOOL_POWER,      // Tool7
	CB_TOOL_WATER,      // Tool8
	CB_TOOL_BULLDOZE,   // Tool9
	CB_TOOL_NONE,       // Tool0
	CB_TOOL_TERRAFORM,  // ToolT
	CB_TOOL_DISTRICT,   // ToolB
	CB_TOOL_TRANSIT,    // ToolL
	CB_TOOL_CONDUIT,    // ToolK
};
static_assert(sizeof(g_aeToolForSelectAction) / sizeof(g_aeToolForSelectAction[0])
	== CB_Bindings::uTOOL_SELECT_COUNT,
	"the tool table and the binding table's selection rows must stay the same length");

void CB_ToolSystem::Update()
{
	// Tool selection. The free-form tools themselves are applied by CB_RoadController (road / zone /
	// service / bulldoze), which reads GetTool() + uses PickGroundPoint for the world cursor.
	//
	// The rows are walked in order and the LAST press of the frame wins, exactly as the retired
	// straight-line WasKeyPressedThisFrame chain did (two keys down on one frame is not a state the
	// table arbitrates).
	const CB_ETool eOldTool = m_eTool;
	for (u_int32 u = 0; u < CB_Bindings::uTOOL_SELECT_COUNT; ++u)
	{
		if (CB_Bindings::WasToolSelectPressed(u)) { m_eTool = g_aeToolForSelectAction[u]; }
	}

	if (m_eTool != eOldTool)
	{
		Zenith_EventDispatcher::Get().Dispatch(CB_OnToolSelected{ static_cast<uint8_t>(m_eTool) });
	}
}
