#include "Zenith.h"

#include "CityBuilder/Source/CB_ToolSystem.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_Events.h"
#include "CityBuilder/Source/CB_Config.h"
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

bool CB_ToolSystem::PickGroundCell(const CB_CityGrid& xGrid, uint32_t& uOutX, uint32_t& uOutZ) const
{
	float fHitX = 0.0f, fHitZ = 0.0f;
	if (!PickGroundPoint(fHitX, fHitZ))
	{
		return false;
	}
	return xGrid.WorldToGrid(fHitX, fHitZ, uOutX, uOutZ);
}

void CB_ToolSystem::ApplyToolAt(uint32_t uX, uint32_t uZ, CB_CityGrid& xGrid, CB_RoadNetwork& xRoads,
                                CB_BuildingManager& xBuildings, CB_TerrainHeightfield& xTerrain)
{
	Zenith_EventDispatcher& xDisp = Zenith_EventDispatcher::Get();
	switch (m_eTool)
	{
	case CB_TOOL_ZONE_RES:  xGrid.PaintZone(uX, uZ, m_uBrushRadius, CB_ZONE_RESIDENTIAL, 0); xDisp.Dispatch(CB_OnZonePainted{ uX, uZ, static_cast<uint8_t>(CB_ZONE_RESIDENTIAL) }); break;
	case CB_TOOL_ZONE_COM:  xGrid.PaintZone(uX, uZ, m_uBrushRadius, CB_ZONE_COMMERCIAL, 0);  xDisp.Dispatch(CB_OnZonePainted{ uX, uZ, static_cast<uint8_t>(CB_ZONE_COMMERCIAL) });  break;
	case CB_TOOL_ZONE_IND:  xGrid.PaintZone(uX, uZ, m_uBrushRadius, CB_ZONE_INDUSTRIAL, 0);  xDisp.Dispatch(CB_OnZonePainted{ uX, uZ, static_cast<uint8_t>(CB_ZONE_INDUSTRIAL) });  break;
	case CB_TOOL_ZONE_PARK: xGrid.PaintZone(uX, uZ, m_uBrushRadius, CB_ZONE_PARK, 0);        xDisp.Dispatch(CB_OnZonePainted{ uX, uZ, static_cast<uint8_t>(CB_ZONE_PARK) });        break;
	case CB_TOOL_ROAD:
	{
		if (xRoads.PlaceRoad(uX, uZ, CB_ROAD_SMALL))
		{
			float fWX, fWZ;
			xGrid.GridToWorld(uX, uZ, fWX, fWZ);
			CB_TerrainBrush xBrush;
			xBrush.m_eMode = CB_TERRAIN_BRUSH_FLATTEN;
			xBrush.m_fCentreX = fWX; xBrush.m_fCentreZ = fWZ;
			xBrush.m_fRadius = xGrid.GetCellSize(); xBrush.m_fStrength = 1.0f;
			xBrush.m_fTargetWorldY = xTerrain.GetHeightAt(fWX, fWZ);
			xTerrain.ApplyBrush(xBrush);
			xDisp.Dispatch(CB_OnRoadPlaced{ uX, uZ });
		}
		break;
	}
	case CB_TOOL_POLICE: if (xBuildings.SpawnBuilding(CB_BUILDING_POLICE, uX, uZ)      != CB_BuildingManager::INVALID) { xDisp.Dispatch(CB_OnServicePlaced{ uX, uZ, static_cast<uint8_t>(CB_BUILDING_POLICE) }); }      break;
	case CB_TOOL_POWER:  if (xBuildings.SpawnBuilding(CB_BUILDING_POWER_PLANT, uX, uZ) != CB_BuildingManager::INVALID) { xDisp.Dispatch(CB_OnServicePlaced{ uX, uZ, static_cast<uint8_t>(CB_BUILDING_POWER_PLANT) }); } break;
	case CB_TOOL_WATER:  if (xBuildings.SpawnBuilding(CB_BUILDING_WATER_TOWER, uX, uZ) != CB_BuildingManager::INVALID) { xDisp.Dispatch(CB_OnServicePlaced{ uX, uZ, static_cast<uint8_t>(CB_BUILDING_WATER_TOWER) }); } break;
	case CB_TOOL_BULLDOZE:
	{
		bool bRemoved = xBuildings.RemoveBuildingAtCell(uX, uZ);
		if (!bRemoved) { bRemoved = xRoads.RemoveRoad(uX, uZ); }
		if (!bRemoved) { xGrid.ClearZone(uX, uZ, 0); }
		if (bRemoved)  { xDisp.Dispatch(CB_OnBulldozed{ uX, uZ }); }
		break;
	}
	default: break;
	}
}

void CB_ToolSystem::Update(CB_CityGrid& xGrid, CB_RoadNetwork& xRoads, CB_BuildingManager& xBuildings, CB_TerrainHeightfield& xTerrain)
{
	Zenith_Input& xInput = g_xEngine.Input();

	// Tool selection.
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

	if (m_eTool == CB_TOOL_NONE)
	{
		return;
	}

#if CB_USE_LEGACY_GRID
	// Legacy grid placement: pick the cell under the cursor and apply the tool.
	// In the Cities: Skylines rebuild the road/zone/building tools are free-form
	// (CB_RoadController etc.), so this whole path is compiled out.
	uint32_t uCX = 0, uCZ = 0;
	if (!PickGroundCell(xGrid, uCX, uCZ))
	{
		return;
	}

	const bool bPress = xInput.WasKeyPressedThisFrame(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bHeld  = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bDragTool = (m_eTool == CB_TOOL_ROAD) ||
	                       (m_eTool >= CB_TOOL_ZONE_RES && m_eTool <= CB_TOOL_ZONE_PARK) ||
	                       (m_eTool == CB_TOOL_BULLDOZE);

	if (bPress || (bHeld && bDragTool))
	{
		ApplyToolAt(uCX, uCZ, xGrid, xRoads, xBuildings, xTerrain);
	}
#else
	(void)xGrid; (void)xRoads; (void)xBuildings; (void)xTerrain;
#endif
}
