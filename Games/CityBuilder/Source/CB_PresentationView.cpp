#include "Zenith.h"

#include "CityBuilder/Source/CB_PresentationView.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Maths/Zenith_Maths.h"

namespace
{
	// Visual height (world units) per building type.
	float BuildingHeight(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_RES_LOW:   return 4.0f;
		case CB_BUILDING_RES_MED:   return 9.0f;
		case CB_BUILDING_RES_HIGH:  return 18.0f;
		case CB_BUILDING_COM_LOW:   return 5.0f;
		case CB_BUILDING_COM_MED:   return 11.0f;
		case CB_BUILDING_COM_HIGH:  return 22.0f;
		case CB_BUILDING_IND_LOW:   return 6.0f;
		case CB_BUILDING_IND_MED:   return 10.0f;
		case CB_BUILDING_IND_HIGH:  return 15.0f;
		case CB_BUILDING_POWER_PLANT: return 9.0f;
		case CB_BUILDING_WATER_TOWER: return 12.0f;
		case CB_BUILDING_POLICE:    return 6.0f;
		case CB_BUILDING_FIRE:      return 6.0f;
		case CB_BUILDING_HOSPITAL:  return 13.0f;
		case CB_BUILDING_SCHOOL:    return 8.0f;
		default:                    return 4.0f;
		}
	}

	Zenith_Maths::Vector3 BuildingColor(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_RES_LOW:  case CB_BUILDING_RES_MED:  case CB_BUILDING_RES_HIGH:  return Zenith_Maths::Vector3(0.25f, 0.72f, 0.32f);  // green
		case CB_BUILDING_COM_LOW:  case CB_BUILDING_COM_MED:  case CB_BUILDING_COM_HIGH:  return Zenith_Maths::Vector3(0.25f, 0.45f, 0.85f);  // blue
		case CB_BUILDING_IND_LOW:  case CB_BUILDING_IND_MED:  case CB_BUILDING_IND_HIGH:  return Zenith_Maths::Vector3(0.80f, 0.66f, 0.22f);  // amber
		case CB_BUILDING_POWER_PLANT: return Zenith_Maths::Vector3(0.85f, 0.25f, 0.20f);  // red
		case CB_BUILDING_WATER_TOWER: return Zenith_Maths::Vector3(0.25f, 0.70f, 0.85f);  // cyan
		case CB_BUILDING_POLICE:      return Zenith_Maths::Vector3(0.20f, 0.25f, 0.65f);  // navy
		case CB_BUILDING_FIRE:        return Zenith_Maths::Vector3(0.90f, 0.40f, 0.12f);  // orange
		case CB_BUILDING_HOSPITAL:    return Zenith_Maths::Vector3(0.92f, 0.92f, 0.95f);  // white
		case CB_BUILDING_SCHOOL:      return Zenith_Maths::Vector3(0.62f, 0.35f, 0.72f);  // purple
		default:                      return Zenith_Maths::Vector3(0.7f, 0.7f, 0.7f);
		}
	}
}

void CB_PresentationView::Render(const CB_CityGrid& xGrid, const CB_RoadNetwork& xRoads,
                                 const CB_BuildingManager& xBuildings, const CB_TerrainHeightfield& xTerrain)
{
	if (!xGrid.IsInitialized())
	{
		return;
	}
	const float fCell = xGrid.GetCellSize();
	const float fHalf = fCell * 0.42f;

	// The ground is a real Zenith_TerrainComponent (flat at y=0), created in the
	// City scene — no debug-primitive ground box here. Roads/buildings sit on it
	// via xTerrain.GetHeightAt (the CB_TerrainHeightfield, flat at 0, matches).

	// Roads — flat grey boxes.
	const Zenith_Vector<uint32_t>& xRoadCells = xRoads.GetRoadCells();
	for (uint32_t u = 0; u < xRoadCells.GetSize(); ++u)
	{
		const uint32_t uIdx = xRoadCells.Get(u);
		const uint32_t uX = uIdx % xGrid.GetWidth();
		const uint32_t uZ = uIdx / xGrid.GetWidth();
		float fWX, fWZ;
		xGrid.GridToWorld(uX, uZ, fWX, fWZ);
		const float fBase = xTerrain.GetHeightAt(fWX, fWZ);
		g_xEngine.Primitives().AddCube(
			Zenith_Maths::Vector3(fWX, fBase + 0.1f, fWZ),
			Zenith_Maths::Vector3(fCell * 0.5f, 0.1f, fCell * 0.5f),
			Zenith_Maths::Vector3(0.28f, 0.28f, 0.30f));
	}

	// Buildings — colored boxes, height + colour by type. Capped so a fully
	// built city can't overflow the debug-primitive buffers.
	static constexpr uint32_t uMAX_DRAWN = 40000;
	uint32_t uDrawn = 0;
	const uint32_t uRecords = xBuildings.GetRecordCount();
	for (uint32_t u = 0; u < uRecords && uDrawn < uMAX_DRAWN; ++u)
	{
		const CB_BuildingRecord& xRec = xBuildings.GetRecord(u);
		if (!xRec.m_bActive)
		{
			continue;
		}
		++uDrawn;
		float fWX, fWZ;
		xGrid.GridToWorld(xRec.m_uGridX, xRec.m_uGridZ, fWX, fWZ);
		const float fBase = xTerrain.GetHeightAt(fWX, fWZ);
		const float fH = BuildingHeight(xRec.m_eType);
		Zenith_Maths::Vector3 xColor = BuildingColor(xRec.m_eType);
		if (!xRec.m_bPowered)
		{
			xColor *= 0.45f;  // dim unpowered buildings
		}
		g_xEngine.Primitives().AddCube(
			Zenith_Maths::Vector3(fWX, fBase + fH * 0.5f, fWZ),
			Zenith_Maths::Vector3(fHalf, fH * 0.5f, fHalf),
			xColor);
	}
}
