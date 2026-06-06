#include "Zenith.h"

#include "CityBuilder/Source/CB_ToolSystem.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
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
	Zenith_Maths::Vector2_64 xMouse;
	g_xEngine.Input().GetMousePosition(xMouse);

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

void CB_ToolSystem::Update()
{
	Zenith_Input& xInput = g_xEngine.Input();

	// Tool selection. The free-form tools themselves are applied by CB_RoadController (road / zone /
	// service / bulldoze), which reads GetTool() + uses PickGroundPoint for the world cursor.
	const CB_ETool eOldTool = m_eTool;
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_1)) { m_eTool = CB_TOOL_ZONE_RES; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_2)) { m_eTool = CB_TOOL_ZONE_COM; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_3)) { m_eTool = CB_TOOL_ZONE_IND; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_4)) { m_eTool = CB_TOOL_ZONE_PARK; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_5)) { m_eTool = CB_TOOL_ROAD; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_6)) { m_eTool = CB_TOOL_POLICE; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_7)) { m_eTool = CB_TOOL_POWER; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_8)) { m_eTool = CB_TOOL_WATER; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_9)) { m_eTool = CB_TOOL_BULLDOZE; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_0)) { m_eTool = CB_TOOL_NONE; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_T)) { m_eTool = CB_TOOL_TERRAFORM; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_B)) { m_eTool = CB_TOOL_DISTRICT; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_L)) { m_eTool = CB_TOOL_TRANSIT; }
	if (xInput.WasKeyPressedThisFrame(ZENITH_KEY_K)) { m_eTool = CB_TOOL_CONDUIT; }

	if (m_eTool != eOldTool)
	{
		Zenith_EventDispatcher::Get().Dispatch(CB_OnToolSelected{ static_cast<uint8_t>(m_eTool) });
	}
}
