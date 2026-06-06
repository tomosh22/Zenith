#pragma once

#include "CityBuilder/Source/CB_Zones.h"   // CB_EZoneType
#include <cstdint>

// ============================================================================
// CB_BuildingDefs — the 19 building types and their static tuning. RCI buildings
// (residential/commercial/industrial at 3 densities) auto-grow on demand; the 10
// service/utility buildings (power, water, police, fire, hospital, school, park,
// landfill, sewage plant, bus depot) are placed explicitly. Header-only constexpr table.
// ============================================================================

enum CB_EBuildingType : uint8_t
{
	CB_BUILDING_RES_LOW   = 0,
	CB_BUILDING_RES_MED   = 1,
	CB_BUILDING_RES_HIGH  = 2,
	CB_BUILDING_COM_LOW   = 3,
	CB_BUILDING_COM_MED   = 4,
	CB_BUILDING_COM_HIGH  = 5,
	CB_BUILDING_IND_LOW   = 6,
	CB_BUILDING_IND_MED   = 7,
	CB_BUILDING_IND_HIGH  = 8,
	CB_BUILDING_POWER_PLANT = 9,
	CB_BUILDING_WATER_TOWER = 10,
	CB_BUILDING_POLICE      = 11,
	CB_BUILDING_FIRE        = 12,
	CB_BUILDING_HOSPITAL    = 13,
	CB_BUILDING_SCHOOL      = 14,
	CB_BUILDING_PARK        = 15,
	CB_BUILDING_LANDFILL    = 16,
	CB_BUILDING_SEWAGE_PLANT = 17,
	CB_BUILDING_BUS_DEPOT   = 18,
	CB_BUILDING_POST_OFFICE = 19,
	CB_BUILDING_COUNT       = 20,
	CB_BUILDING_NONE        = 0xFF
};

enum CB_EServiceType : uint8_t
{
	CB_SERVICE_NONE      = 0,
	CB_SERVICE_POWER     = 1,
	CB_SERVICE_WATER     = 2,
	CB_SERVICE_POLICE    = 3,
	CB_SERVICE_FIRE      = 4,
	CB_SERVICE_HEALTH    = 5,
	CB_SERVICE_EDUCATION = 6,
	CB_SERVICE_PARK      = 7,
	CB_SERVICE_GARBAGE   = 8,
	CB_SERVICE_SEWAGE    = 9,
	CB_SERVICE_TRANSIT   = 10,
	CB_SERVICE_MAIL      = 11,
	CB_SERVICE_COUNT     = 12
};

struct CB_BuildingDef
{
	CB_EBuildingType m_eType;
	CB_EZoneType     m_eZone;          // zone this grows in (CB_ZONE_NONE for services)
	uint8_t          m_uDensity;       // 0..2 (RCI); 0 for services
	uint16_t         m_uMaxOccupants;  // residents (RES) or jobs (COM/IND)
	float            m_fPowerUse;      // power consumed; NEGATIVE = produced (power plant)
	float            m_fWaterUse;      // water consumed; NEGATIVE = produced (water tower)
	float            m_fTaxRevenue;    // per sim tick at full occupancy
	float            m_fUpkeep;        // per sim tick
	float            m_fPollution;     // emitted into neighbourhood
	CB_EServiceType  m_eService;       // service provided (CB_SERVICE_NONE for RCI)
	float            m_fServiceRadius;  // coverage radius in cells (police/fire/health/edu)
};

namespace CB_BuildingDefs
{
	// Indexed by CB_EBuildingType (enum order). Internal linkage per TU is fine.
	static constexpr CB_BuildingDef AX_DEFS[CB_BUILDING_COUNT] = {
		// type                   zone               dens occ  pwr    wtr    tax    upkp  poll  service             radius
		{ CB_BUILDING_RES_LOW,    CB_ZONE_RESIDENTIAL, 0,   8,  2.0f,  2.0f,  10.0f, 1.0f, 0.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_RES_MED,    CB_ZONE_RESIDENTIAL, 1,  24,  5.0f,  5.0f,  25.0f, 2.0f, 0.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_RES_HIGH,   CB_ZONE_RESIDENTIAL, 2,  60, 12.0f, 12.0f,  60.0f, 4.0f, 1.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_COM_LOW,    CB_ZONE_COMMERCIAL,  0,   6,  3.0f,  2.0f,  15.0f, 1.0f, 1.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_COM_MED,    CB_ZONE_COMMERCIAL,  1,  18,  7.0f,  4.0f,  35.0f, 2.0f, 2.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_COM_HIGH,   CB_ZONE_COMMERCIAL,  2,  45, 16.0f,  9.0f,  80.0f, 4.0f, 3.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_IND_LOW,    CB_ZONE_INDUSTRIAL,  0,  10,  5.0f,  3.0f,  12.0f, 1.0f, 5.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_IND_MED,    CB_ZONE_INDUSTRIAL,  1,  25, 12.0f,  7.0f,  28.0f, 2.0f,12.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_IND_HIGH,   CB_ZONE_INDUSTRIAL,  2,  55, 25.0f, 15.0f,  65.0f, 4.0f,25.0f, CB_SERVICE_NONE,     0.0f },
		{ CB_BUILDING_POWER_PLANT,CB_ZONE_NONE,        0,   0,-200.0f, 5.0f,   0.0f,50.0f,30.0f, CB_SERVICE_POWER,    0.0f },
		{ CB_BUILDING_WATER_TOWER,CB_ZONE_NONE,        0,   0,  5.0f,-150.0f,  0.0f,30.0f, 0.0f, CB_SERVICE_WATER,    0.0f },
		{ CB_BUILDING_POLICE,     CB_ZONE_NONE,        0,   0,  5.0f,  3.0f,   0.0f,40.0f, 0.0f, CB_SERVICE_POLICE,  40.0f },
		{ CB_BUILDING_FIRE,       CB_ZONE_NONE,        0,   0,  5.0f,  3.0f,   0.0f,40.0f, 0.0f, CB_SERVICE_FIRE,    40.0f },
		{ CB_BUILDING_HOSPITAL,   CB_ZONE_NONE,        0,   0, 12.0f,  8.0f,   0.0f,80.0f, 0.0f, CB_SERVICE_HEALTH,  50.0f },
		{ CB_BUILDING_SCHOOL,     CB_ZONE_NONE,        0,   0,  8.0f,  5.0f,   0.0f,60.0f, 0.0f, CB_SERVICE_EDUCATION,45.0f },
		{ CB_BUILDING_PARK,       CB_ZONE_NONE,        0,   0,  1.0f,  1.0f,   0.0f,12.0f,-3.0f, CB_SERVICE_PARK,     30.0f },
		{ CB_BUILDING_LANDFILL,   CB_ZONE_NONE,        0,   0,  8.0f,  3.0f,   0.0f,40.0f, 8.0f, CB_SERVICE_GARBAGE,   0.0f },
		{ CB_BUILDING_SEWAGE_PLANT,CB_ZONE_NONE,       0,   0, 10.0f,  0.0f,   0.0f,50.0f, 5.0f, CB_SERVICE_SEWAGE,    0.0f },
		{ CB_BUILDING_BUS_DEPOT,  CB_ZONE_NONE,        0,   0,  6.0f,  4.0f,   0.0f,35.0f, 1.0f, CB_SERVICE_TRANSIT,   0.0f },
		{ CB_BUILDING_POST_OFFICE,CB_ZONE_NONE,        0,   0,  5.0f,  3.0f,   0.0f,30.0f, 1.0f, CB_SERVICE_MAIL,      0.0f },
	};

	inline const CB_BuildingDef& Get(CB_EBuildingType eType)
	{
		const uint8_t uIdx = (eType < CB_BUILDING_COUNT) ? static_cast<uint8_t>(eType) : 0;
		return AX_DEFS[uIdx];
	}

	// Building type that grows on a zoned cell at a density, or CB_BUILDING_NONE.
	inline CB_EBuildingType TypeForZone(CB_EZoneType eZone, uint8_t uDensity)
	{
		const uint8_t uD = (uDensity > 2) ? 2 : uDensity;
		switch (eZone)
		{
		case CB_ZONE_RESIDENTIAL: return static_cast<CB_EBuildingType>(CB_BUILDING_RES_LOW + uD);
		case CB_ZONE_COMMERCIAL:  return static_cast<CB_EBuildingType>(CB_BUILDING_COM_LOW + uD);
		case CB_ZONE_INDUSTRIAL:  return static_cast<CB_EBuildingType>(CB_BUILDING_IND_LOW + uD);
		default:                  return CB_BUILDING_NONE;
		}
	}

	inline bool IsResidential(CB_EBuildingType e) { return e <= CB_BUILDING_RES_HIGH; }
	inline bool IsCommercial(CB_EBuildingType e)  { return e >= CB_BUILDING_COM_LOW && e <= CB_BUILDING_COM_HIGH; }
	inline bool IsIndustrial(CB_EBuildingType e)  { return e >= CB_BUILDING_IND_LOW && e <= CB_BUILDING_IND_HIGH; }
	inline bool IsService(CB_EBuildingType e)     { return e >= CB_BUILDING_POWER_PLANT && e < CB_BUILDING_COUNT; }
}
