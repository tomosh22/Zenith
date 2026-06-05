#include "Zenith.h"

#include "CityBuilder/Source/CB_BuildingPlacement.h"
#include "CityBuilder/Source/CB_Serialize.h"
#include "CityBuilder/Source/CB_Districts.h"
#include "CityBuilder/Source/CB_TransitLines.h"
#include "CityBuilder/Source/CB_Conduits.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include <algorithm>

namespace
{
	float Clamp01(float f) { return (f < 0.0f) ? 0.0f : (f > 1.0f ? 1.0f : f); }

	float BuildingHeight(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_RES_LOW:  return 6.0f;
		case CB_BUILDING_RES_MED:  return 12.0f;
		case CB_BUILDING_RES_HIGH: return 26.0f;
		case CB_BUILDING_COM_LOW:  return 6.0f;
		case CB_BUILDING_COM_MED:  return 13.0f;
		case CB_BUILDING_COM_HIGH: return 28.0f;
		case CB_BUILDING_IND_LOW:  return 7.0f;
		case CB_BUILDING_IND_MED:  return 11.0f;
		case CB_BUILDING_IND_HIGH: return 16.0f;
		default:                   return 6.0f;
		}
	}

	Zenith_Maths::Vector3 BuildingColor(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_RES_LOW: case CB_BUILDING_RES_MED: case CB_BUILDING_RES_HIGH: return Zenith_Maths::Vector3(0.30f, 0.72f, 0.34f);  // green
		case CB_BUILDING_COM_LOW: case CB_BUILDING_COM_MED: case CB_BUILDING_COM_HIGH: return Zenith_Maths::Vector3(0.28f, 0.50f, 0.88f);  // blue
		case CB_BUILDING_IND_LOW: case CB_BUILDING_IND_MED: case CB_BUILDING_IND_HIGH: return Zenith_Maths::Vector3(0.85f, 0.70f, 0.24f);  // amber
		default:                   return Zenith_Maths::Vector3(0.7f, 0.7f, 0.7f);
		}
	}

	// A road-facing box: footprint half-extents (halfW along the road, halfD into
	// the lot), oriented so xFwd faces the road. Emits top + 4 sides.
	void EmitBox(const Zenith_Maths::Vector2& xCentre, const Zenith_Maths::Vector2& xFwd,
	             float fHalfW, float fHalfD, float fBaseY, float fHeight, const Zenith_Maths::Vector3& xColor)
	{
		const Zenith_Maths::Vector2 xRight(-xFwd.y, xFwd.x);
		auto Corner = [&](float rs, float fs) -> Zenith_Maths::Vector2
		{
			return Zenith_Maths::Vector2(
				xCentre.x + xRight.x * rs * fHalfW + xFwd.x * fs * fHalfD,
				xCentre.y + xRight.y * rs * fHalfW + xFwd.y * fs * fHalfD);
		};
		const Zenith_Maths::Vector2 c00 = Corner(-1.0f, -1.0f);
		const Zenith_Maths::Vector2 c10 = Corner( 1.0f, -1.0f);
		const Zenith_Maths::Vector2 c11 = Corner( 1.0f,  1.0f);
		const Zenith_Maths::Vector2 c01 = Corner(-1.0f,  1.0f);
		const float by = fBaseY;
		const float ty = fBaseY + fHeight;
		const Zenith_Maths::Vector3 b00(c00.x, by, c00.y), b10(c10.x, by, c10.y), b11(c11.x, by, c11.y), b01(c01.x, by, c01.y);
		const Zenith_Maths::Vector3 t00(c00.x, ty, c00.y), t10(c10.x, ty, c10.y), t11(c11.x, ty, c11.y), t01(c01.x, ty, c01.y);
		Flux_PrimitivesImpl& xP = g_xEngine.Primitives();
		// Top — wound so the face normal (Cross(v1-v0, v2-v0)) points +Y (up). The previous
		// winding pointed it DOWN, so every roof was lit from underneath → ~black buildings.
		xP.AddTriangle(t00, t10, t11, xColor); xP.AddTriangle(t00, t11, t01, xColor);
		// Sides (outward winding).
		xP.AddTriangle(b00, b10, t10, xColor); xP.AddTriangle(b00, t10, t00, xColor);
		xP.AddTriangle(b10, b11, t11, xColor); xP.AddTriangle(b10, t11, t10, xColor);
		xP.AddTriangle(b11, b01, t01, xColor); xP.AddTriangle(b11, t01, t11, xColor);
		xP.AddTriangle(b01, b00, t00, xColor); xP.AddTriangle(b01, t00, t01, xColor);
	}

	uint32_t BuildHash(uint32_t uLot) { return uLot * 2654435761u + 1013904223u; }

	// A varied building: a jittered body + darker roof cap, plus a stepped-back
	// tower crown for high-density. Per-building footprint + colour jitter from the
	// lot hash so a street of one type doesn't read as identical boxes.
	void EmitBuilding(const Zenith_Maths::Vector2& xCentre, const Zenith_Maths::Vector2& xFwd,
	                  float fBaseHalf, float fBaseY, float fHeight, const Zenith_Maths::Vector3& xCol,
	                  uint8_t uLevel, uint32_t uHash)
	{
		const float fJ    = static_cast<float>(uHash & 0xFFu) / 255.0f;
		const float fHalf = fBaseHalf * (0.80f + 0.34f * fJ);
		const float fTint = 0.82f + 0.30f * (static_cast<float>((uHash >> 8) & 0xFFu) / 255.0f);
		const Zenith_Maths::Vector3 xBody = xCol * fTint;
		const Zenith_Maths::Vector3 xRoof = xBody * 0.62f;
		const float fBodyH = fHeight * 0.86f;
		EmitBox(xCentre, xFwd, fHalf, fHalf, fBaseY, fBodyH, xBody);
		EmitBox(xCentre, xFwd, fHalf * 0.94f, fHalf * 0.94f, fBaseY + fBodyH, fHeight * 0.14f, xRoof);
		if (uLevel >= 2)   // high-density: a stepped-back tower crown
		{
			EmitBox(xCentre, xFwd, fHalf * 0.58f, fHalf * 0.58f, fBaseY + fHeight, fHeight * 0.34f, xBody);
			EmitBox(xCentre, xFwd, fHalf * 0.30f, fHalf * 0.30f, fBaseY + fHeight * 1.34f, fHeight * 0.10f, xRoof);
		}
	}

	// Service-building tuning. The def radius is in 4m legacy cells; scale to metres.
	constexpr float COVERAGE_SCALE = 4.0f;

	// Utility-network reach: a building is "connected" only within this distance of
	// a power plant / water tower, so placing sources to cover the city matters.
	constexpr float POWER_REACH = 320.0f;
	constexpr float WATER_REACH = 320.0f;

	// Disasters: a fire fought by a covering fire station is out by FIRE_FIGHT_TICKS;
	// an uncovered fire razes the building at FIRE_DESTROY_TICKS. One growth pass ~= a tick.
	constexpr uint16_t FIRE_FIGHT_TICKS   = 3;
	constexpr uint16_t FIRE_DESTROY_TICKS = 8;

	// CB_SERVICE_POLICE/FIRE/HEALTH/EDUCATION/PARK → 0..4 (the coverage types); else -1.
	int CoverageIndex(CB_EServiceType eService)
	{
		if (eService >= CB_SERVICE_POLICE && eService <= CB_SERVICE_PARK)
		{
			return static_cast<int>(eService) - static_cast<int>(CB_SERVICE_POLICE);
		}
		return -1;
	}

	float ServiceHeight(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_POWER_PLANT: return 22.0f;
		case CB_BUILDING_WATER_TOWER: return 26.0f;
		case CB_BUILDING_HOSPITAL:    return 20.0f;
		case CB_BUILDING_PARK:        return 2.5f;   // flat green space
		case CB_BUILDING_LANDFILL:    return 5.0f;   // low spread mound
		case CB_BUILDING_SEWAGE_PLANT:return 8.0f;   // squat tanks
		case CB_BUILDING_BUS_DEPOT:   return 10.0f;
		case CB_BUILDING_POST_OFFICE: return 13.0f;
		default:                      return 12.0f;
		}
	}

	Zenith_Maths::Vector3 ServiceColor(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_POWER_PLANT: return Zenith_Maths::Vector3(0.85f, 0.20f, 0.16f);  // red
		case CB_BUILDING_WATER_TOWER: return Zenith_Maths::Vector3(0.20f, 0.65f, 0.85f);  // cyan
		case CB_BUILDING_POLICE:      return Zenith_Maths::Vector3(0.16f, 0.22f, 0.70f);  // navy
		case CB_BUILDING_FIRE:        return Zenith_Maths::Vector3(0.90f, 0.45f, 0.12f);  // orange
		case CB_BUILDING_HOSPITAL:    return Zenith_Maths::Vector3(0.95f, 0.95f, 0.97f);  // white
		case CB_BUILDING_SCHOOL:      return Zenith_Maths::Vector3(0.92f, 0.82f, 0.20f);  // yellow
		case CB_BUILDING_PARK:        return Zenith_Maths::Vector3(0.26f, 0.60f, 0.24f);  // park green
		case CB_BUILDING_LANDFILL:    return Zenith_Maths::Vector3(0.45f, 0.38f, 0.20f);  // muddy olive
		case CB_BUILDING_SEWAGE_PLANT:return Zenith_Maths::Vector3(0.40f, 0.45f, 0.35f);  // grey-green
		case CB_BUILDING_BUS_DEPOT:   return Zenith_Maths::Vector3(0.15f, 0.50f, 0.55f);  // teal
		case CB_BUILDING_POST_OFFICE: return Zenith_Maths::Vector3(0.20f, 0.30f, 0.75f);  // postal blue
		default:                      return Zenith_Maths::Vector3(0.60f, 0.60f, 0.60f);
		}
	}
}

void CB_BuildingPlacement::Reset()
{
	m_axBuildings.Clear();
	m_axServices.Clear();
	m_uActiveBuildings = 0;
	m_uActiveServices  = 0;
	m_uResidents = 0;
	m_uComJobs   = 0;
	m_uIndJobs   = 0;
	m_uTick      = 0;
	m_fTreasury   = START_TREASURY;
	m_fPowerProd  = 0.0f; m_fPowerUse = 0.0f;
	m_fWaterProd  = 0.0f; m_fWaterUse = 0.0f;
	m_fHappiness  = 0.5f;
	m_fServedFrac = 0.0f;
	m_fIncome = 0.0f; m_fUpkeep = 0.0f;
	m_fRoadCapacity = 1.0e9f;
	m_fCongestion   = 0.0f;
	m_fPollution    = 0.0f;
	m_fTaxRate      = 1.0f;
	m_fDebt         = 0.0f;
	m_fUtilReach    = 1.0f;
	m_fGarbage      = 0.0f;
	m_fSewage       = 0.0f;
	m_fTransitShare = 0.0f;
	m_fMail         = 0.0f;
	m_fFreightRatio = 1.0f;
	m_uActiveFires    = 0;
	m_uFiresDestroyed = 0;
	for (uint32_t p = 0; p < CB_POLICY_COUNT; ++p) { m_afPolicyFrac[p] = 0.0f; }
}

float CB_BuildingPlacement::GetResDemand() const
{
	const float fJobs = static_cast<float>(m_uComJobs + m_uIndJobs);
	const float fRaw  = (30.0f + fJobs * 1.3f - static_cast<float>(m_uResidents) * 1.0f) / 60.0f;
	return Clamp01(fRaw);
}

float CB_BuildingPlacement::GetComDemand() const
{
	const float fRaw = (static_cast<float>(m_uResidents) * 0.45f - static_cast<float>(m_uComJobs) * 1.0f) / 60.0f;
	return Clamp01(fRaw);
}

float CB_BuildingPlacement::GetIndDemand() const
{
	const float fRaw = (static_cast<float>(m_uResidents) * 0.25f + static_cast<float>(m_uComJobs) * 0.7f
	                    - static_cast<float>(m_uIndJobs) * 1.0f) / 60.0f;
	return Clamp01(fRaw);
}

uint32_t CB_BuildingPlacement::GrowZone(CB_Zoning& xZoning, CB_EZoneType eZone, CB_EBuildingType eLowType, uint32_t uMaxSpawn)
{
	uint32_t uSpawned = 0;
	const uint32_t uLots = xZoning.GetLotSlotCount();
	for (uint32_t i = 0; i < uLots && uSpawned < uMaxSpawn; ++i)
	{
		CB_Lot& xLot = xZoning.GetLotMutable(i);
		if (!xLot.m_bActive || xLot.m_eZone != eZone || xLot.m_uBuildingId != CB_Zoning::INVALID)
		{
			continue;
		}
		CB_Building xB;
		xB.m_uLot       = i;
		xB.m_eType      = eLowType;
		xB.m_uLevel     = 0;
		xB.m_uOccupants = static_cast<uint16_t>(CB_BuildingDefs::Get(eLowType).m_uMaxOccupants);
		xB.m_bActive    = true;
		const uint32_t uId = m_axBuildings.GetSize();
		m_axBuildings.PushBack(xB);
		++m_uActiveBuildings;
		xLot.m_uBuildingId = uId;
		++uSpawned;
	}
	return uSpawned;
}

void CB_BuildingPlacement::PlaceService(CB_EBuildingType eType, float fWorldX, float fWorldZ, float fWorldY)
{
	const CB_BuildingDef& xDef = CB_BuildingDefs::Get(eType);
	m_fTreasury -= xDef.m_fUpkeep * 100.0f;   // build cost derived from upkeep
	CB_ServiceBuilding xS;
	xS.m_xPos    = Zenith_Maths::Vector2(fWorldX, fWorldZ);
	xS.m_fWorldY = fWorldY;
	xS.m_eType   = eType;
	xS.m_bActive = true;
	m_axServices.PushBack(xS);
	++m_uActiveServices;
}

// Recompute the utility balance, service coverage, happiness + economy from the
// current building/service set. Called once per growth pass.
void CB_BuildingPlacement::UpdateSimState(const CB_Zoning& xZoning)
{
	m_fPowerProd = 0.0f; m_fPowerUse = 0.0f;
	m_fWaterProd = 0.0f; m_fWaterUse = 0.0f;
	float fUpkeep = 0.0f;
	float fIncome = 0.0f;
	float fPollution = 0.0f;

	// Services produce power/water (negative use) + cost upkeep; parks clean (negative pollution).
	uint32_t uLandfills = 0u, uSewagePlants = 0u, uBusDepots = 0u, uPostOffices = 0u;
	for (uint32_t i = 0; i < m_axServices.GetSize(); ++i)
	{
		const CB_ServiceBuilding& xS = m_axServices.Get(i);
		if (!xS.m_bActive) continue;
		const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xS.m_eType);
		if (xDef.m_fPowerUse < 0.0f) m_fPowerProd += -xDef.m_fPowerUse; else m_fPowerUse += xDef.m_fPowerUse;
		if (xDef.m_fWaterUse < 0.0f) m_fWaterProd += -xDef.m_fWaterUse; else m_fWaterUse += xDef.m_fWaterUse;
		fUpkeep    += xDef.m_fUpkeep;
		fPollution += xDef.m_fPollution;
		if      (xS.m_eType == CB_BUILDING_LANDFILL)     { ++uLandfills; }
		else if (xS.m_eType == CB_BUILDING_SEWAGE_PLANT) { ++uSewagePlants; }
		else if (xS.m_eType == CB_BUILDING_BUS_DEPOT)    { ++uBusDepots; }
		else if (xS.m_eType == CB_BUILDING_POST_OFFICE)  { ++uPostOffices; }
	}
	// Buildings consume power/water, pay tax + cost upkeep. Commercial tax is held
	// separate so freight (industrial goods supply) can scale it below.
	float fComIncome = 0.0f;
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		const CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive) continue;
		const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xB.m_eType);
		m_fPowerUse += xDef.m_fPowerUse;
		m_fWaterUse += xDef.m_fWaterUse;
		fUpkeep    += xDef.m_fUpkeep;
		if (CB_BuildingDefs::IsCommercial(xB.m_eType)) { fComIncome += xDef.m_fTaxRevenue; }
		else                                           { fIncome    += xDef.m_fTaxRevenue; }
		fPollution += xDef.m_fPollution;
	}

	const bool bPow = (BASELINE_POWER + m_fPowerProd) >= m_fPowerUse;
	const bool bWat = (BASELINE_WATER + m_fWaterProd) >= m_fWaterUse;

	// Energize the utility conduit network (power/water flood out from the sources along
	// connected conduit chains) so a building beyond a source's own radius can still be
	// reached if a conduit chain connects it.
	if (m_pxConduits != nullptr && m_pxConduits->GetCount() > 0u)
	{
		Zenith_Vector<Zenith_Maths::Vector2> xPowerSrcs, xWaterSrcs;
		for (uint32_t s = 0; s < m_axServices.GetSize(); ++s)
		{
			const CB_ServiceBuilding& xS = m_axServices.Get(s);
			if (!xS.m_bActive) continue;
			if (xS.m_eType == CB_BUILDING_POWER_PLANT)      { xPowerSrcs.PushBack(xS.m_xPos); }
			else if (xS.m_eType == CB_BUILDING_WATER_TOWER) { xWaterSrcs.PushBack(xS.m_xPos); }
		}
		m_pxConduits->Energize(xPowerSrcs, xWaterSrcs);
	}

	// Per building: service coverage (avg of the 5 coverage types reaching it) +
	// utility-network connection (within reach of BOTH a power and a water source —
	// so utility PLACEMENT matters, not just total capacity).
	float fCovSum = 0.0f;
	uint32_t uCounted = 0;
	uint32_t uUtilConnected = 0;
	uint32_t auPolicyCount[CB_POLICY_COUNT] = { 0u };   // buildings covered by each policy this pass
	uint32_t uPeopleNearStops = 0u;                     // occupants within reach of a transit stop
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		const CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive || xB.m_uLot >= xZoning.GetLotSlotCount()) continue;
		const Zenith_Maths::Vector2 xPos = xZoning.GetLot(xB.m_uLot).m_xPos;
		m_axBuildings.Get(i).m_xWorldPos = xPos;   // cache for disasters (no zoning needed later)
		++uCounted;
		if (m_pxTransit != nullptr && m_pxTransit->IsNearAnyStop(xPos.x, xPos.y)) { uPeopleNearStops += xB.m_uOccupants; }
		if (m_pxDistricts != nullptr)   // tally which ordinances cover this building
		{
			const uint32_t uMask = m_pxDistricts->GetPolicyMaskAt(xPos.x, xPos.y);
			for (uint32_t p = 0; p < CB_POLICY_COUNT; ++p)
			{
				if ((uMask & (1u << p)) != 0u) { ++auPolicyCount[p]; }
			}
		}
		bool abCov[5] = { false, false, false, false, false };
		bool bPowReach = false, bWatReach = false;
		for (uint32_t s = 0; s < m_axServices.GetSize(); ++s)
		{
			const CB_ServiceBuilding& xS = m_axServices.Get(s);
			if (!xS.m_bActive) continue;
			const float dx = xPos.x - xS.m_xPos.x;
			const float dz = xPos.y - xS.m_xPos.y;
			const float fD2 = dx * dx + dz * dz;
			if (xS.m_eType == CB_BUILDING_POWER_PLANT) { if (fD2 <= POWER_REACH * POWER_REACH) { bPowReach = true; } continue; }
			if (xS.m_eType == CB_BUILDING_WATER_TOWER) { if (fD2 <= WATER_REACH * WATER_REACH) { bWatReach = true; } continue; }
			const int iCov = CoverageIndex(CB_BuildingDefs::Get(xS.m_eType).m_eService);
			if (iCov < 0) continue;
			const float fR = CB_BuildingDefs::Get(xS.m_eType).m_fServiceRadius * COVERAGE_SCALE;
			if (fD2 <= fR * fR) { abCov[iCov] = true; }
		}
		const int iN = (abCov[0] ? 1 : 0) + (abCov[1] ? 1 : 0) + (abCov[2] ? 1 : 0) + (abCov[3] ? 1 : 0) + (abCov[4] ? 1 : 0);
		fCovSum += static_cast<float>(iN) * 0.20f;   // 5 coverage types (police/fire/health/edu/park)
		// A connected conduit chain extends power/water reach beyond the source radius.
		if (m_pxConduits != nullptr)
		{
			if (!bPowReach && m_pxConduits->IsPowered(xPos.x, xPos.y)) { bPowReach = true; }
			if (!bWatReach && m_pxConduits->IsWatered(xPos.x, xPos.y)) { bWatReach = true; }
		}
		if (bPowReach && bWatReach) { ++uUtilConnected; }
	}
	m_fServedFrac = (uCounted > 0) ? (fCovSum / static_cast<float>(uCounted)) : 0.0f;
	m_fUtilReach  = (uCounted > 0) ? (static_cast<float>(uUtilConnected) / static_cast<float>(uCounted)) : 1.0f;

	// Policy coverage: the fraction of buildings each ordinance reaches (city-wide → 1.0;
	// a district policy → that district's building share). Each effect scales by it, and
	// each active program costs a little upkeep per covered building.
	for (uint32_t p = 0; p < CB_POLICY_COUNT; ++p)
	{
		m_afPolicyFrac[p] = (uCounted > 0u) ? (static_cast<float>(auPolicyCount[p]) / static_cast<float>(uCounted)) : 0.0f;
	}
	fUpkeep += static_cast<float>(uCounted) * 0.4f
	         * (m_afPolicyFrac[CB_POLICY_RECYCLING] + m_afPolicyFrac[CB_POLICY_FREE_TRANSIT]
	          + m_afPolicyFrac[CB_POLICY_POLLUTION_CONTROL] + m_afPolicyFrac[CB_POLICY_PARKS_MANDATE]);

	// Public transport: each bus depot carries commuters, taking their cars off the
	// road and relieving congestion (Cities: Skylines transit feedback). The free-transit
	// policy lifts ridership capacity.
	const float fPeople     = static_cast<float>(m_uResidents + m_uComJobs + m_uIndJobs);
	const float fTransitCap = static_cast<float>(uBusDepots) * 450.0f * (1.0f + 0.6f * m_afPolicyFrac[CB_POLICY_FREE_TRANSIT]);
	// With transit LINES, only people within reach of a stop ride (routing matters);
	// without lines, the depot capacity serves everyone (backward-compatible).
	const float fTransitReach = (m_pxTransit != nullptr && m_pxTransit->GetStopCount() > 0u)
		? static_cast<float>(uPeopleNearStops) : fPeople;
	const float fRiders    = (fTransitReach < fPeople) ? fTransitReach : fPeople;
	const float fRidership = (fRiders < fTransitCap) ? fRiders : fTransitCap;
	m_fTransitShare = (fPeople > 1.0f) ? (fRidership / fPeople) : 0.0f;

	// Traffic congestion: (population minus transit riders) vs road capacity (the
	// manager feeds the live road length). More road OR transit relieves it.
	const float fRoadPeople = fPeople * (1.0f - 0.5f * m_fTransitShare);
	const float fUtil   = (m_fRoadCapacity > 1.0f) ? (fRoadPeople / m_fRoadCapacity) : 0.0f;
	m_fCongestion = Clamp01((fUtil - 0.6f) * 2.5f);   // 0 below 60% utilisation, 1 at 100%

	// Pollution: industry emits, parks clean (negative). The per-building average
	// becomes a city-wide pollution level that erodes happiness.
	const float fPollPer = (m_uActiveBuildings > 0u) ? (fPollution / static_cast<float>(m_uActiveBuildings)) : 0.0f;
	m_fPollution = Clamp01(fPollPer / 6.0f) * (1.0f - 0.40f * m_afPolicyFrac[CB_POLICY_POLLUTION_CONTROL]);   // pollution-control ordinance

	// Garbage + sewage: people generate both; landfills collect garbage and sewage
	// plants treat effluent. A free baseline covers a hamlet; past it, an overwhelmed
	// service piles up (0..1) and erodes happiness until more capacity is built.
	const float fGarbageCap = static_cast<float>(uLandfills) * 350.0f + 40.0f;
	const float fGarbageLoad = fPeople * 0.15f * (1.0f - 0.45f * m_afPolicyFrac[CB_POLICY_RECYCLING]);   // recycling ordinance
	m_fGarbage = Clamp01((fGarbageLoad - fGarbageCap) / fGarbageCap);
	const float fSewageCap  = static_cast<float>(uSewagePlants) * 350.0f + 60.0f;
	m_fSewage  = Clamp01((m_fWaterUse - fSewageCap) / fSewageCap);

	// Mail: every building generates post; post offices collect it (like garbage).
	const float fMailCap = static_cast<float>(uPostOffices) * 300.0f + 50.0f;
	m_fMail = Clamp01((fPeople * 0.12f - fMailCap) / fMailCap);

	// Freight: industry supplies goods that commerce sells. Undersupply (too little
	// industry for the commercial base) leaves shelves empty → commercial tax is cut.
	const float fComGoodsDemand = static_cast<float>(m_uComJobs) * 0.6f + 1.0f;
	m_fFreightRatio = Clamp01(static_cast<float>(m_uIndJobs) / fComGoodsDemand);
	fIncome += fComIncome * (0.4f + 0.6f * m_fFreightRatio);

	// Happiness leans on services (land value); utilities baseline; congestion,
	// pollution + over-taxation erode it.
	const float fTaxPenalty = (m_fTaxRate > 1.0f) ? (0.40f * (m_fTaxRate - 1.0f)) : 0.0f;
	m_fHappiness = (0.25f * (bPow ? 1.0f : 0.0f) + 0.15f * (bWat ? 1.0f : 0.0f) + 0.60f * m_fServedFrac)
	             * (1.0f - 0.30f * m_fCongestion)
	             * (1.0f - 0.15f * m_fPollution)
	             * (1.0f - fTaxPenalty)
	             * (0.85f + 0.15f * m_fUtilReach)    // utility-network connection (placement matters)
	             * (1.0f - 0.15f * m_fGarbage)       // uncollected garbage
	             * (1.0f - 0.12f * m_fSewage)        // untreated sewage
	             * (1.0f - 0.10f * m_fMail)          // uncollected mail
	             * (1.0f + 0.06f * m_afPolicyFrac[CB_POLICY_PARKS_MANDATE])    // green-space mandate
	             * (1.0f - ((m_uActiveFires > 7u) ? 0.30f : 0.04f * static_cast<float>(m_uActiveFires)));   // active fires

	// Economy: the tax rate scales income; brownouts cut trade; the treasury accrues
	// the net each pass; any outstanding loan amortizes (principal + interest).
	fIncome *= m_fTaxRate;
	if (!bPow || !bWat) fIncome *= 0.25f;
	m_fIncome = fIncome;
	m_fUpkeep = fUpkeep;
	m_fTreasury += (fIncome - fUpkeep) * ECON_RATE;
	if (m_fDebt > 0.0f)
	{
		float fPay = m_fDebt * 0.03f + 5.0f;
		if (fPay > m_fDebt) { fPay = m_fDebt; }
		m_fDebt     -= fPay;
		m_fTreasury -= fPay;
	}
}

bool CB_BuildingPlacement::HasFireCoverage(float fWorldX, float fWorldZ) const
{
	const float fR  = CB_BuildingDefs::Get(CB_BUILDING_FIRE).m_fServiceRadius * COVERAGE_SCALE;
	const float fR2 = fR * fR;
	for (uint32_t s = 0; s < m_axServices.GetSize(); ++s)
	{
		const CB_ServiceBuilding& xS = m_axServices.Get(s);
		if (!xS.m_bActive || xS.m_eType != CB_BUILDING_FIRE) continue;
		const float dx = fWorldX - xS.m_xPos.x;
		const float dz = fWorldZ - xS.m_xPos.y;
		if (dx * dx + dz * dz <= fR2) { return true; }
	}
	return false;
}

bool CB_BuildingPlacement::TriggerFireAt(float fWorldX, float fWorldZ)
{
	uint32_t uBest = INVALID;
	float    fBestD2 = 1.0e30f;
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		const CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive || xB.m_uFireTicks > 0) continue;
		const float dx = fWorldX - xB.m_xWorldPos.x;
		const float dz = fWorldZ - xB.m_xWorldPos.y;
		const float d2 = dx * dx + dz * dz;
		if (d2 < fBestD2) { fBestD2 = d2; uBest = i; }
	}
	if (uBest == INVALID) { return false; }
	m_axBuildings.Get(uBest).m_uFireTicks = 1;
	return true;
}

void CB_BuildingPlacement::ProcessDisasters(CB_Zoning& xZoning)
{
	// Rare deterministic auto-ignition: a building without fire cover catches alight.
	if (m_bAutoDisasters && (m_uTick % 900u) == 0u && m_axBuildings.GetSize() > 0u)
	{
		const uint32_t uPick = static_cast<uint32_t>(m_uTick / 900u) % m_axBuildings.GetSize();
		CB_Building& xB = m_axBuildings.Get(uPick);
		if (xB.m_bActive && xB.m_uFireTicks == 0 && !HasFireCoverage(xB.m_xWorldPos.x, xB.m_xWorldPos.y))
		{
			xB.m_uFireTicks = 1;
		}
	}

	uint32_t uFires = 0;
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive || xB.m_uFireTicks == 0) continue;
		const bool bCovered = HasFireCoverage(xB.m_xWorldPos.x, xB.m_xWorldPos.y);
		++xB.m_uFireTicks;
		if (bCovered && xB.m_uFireTicks >= FIRE_FIGHT_TICKS)
		{
			xB.m_uFireTicks = 0;   // fire crews extinguish it
		}
		else if (!bCovered && xB.m_uFireTicks >= FIRE_DESTROY_TICKS)
		{
			// Razed: free its lot + deactivate (residents/jobs recount next pass).
			if (xB.m_uLot < xZoning.GetLotSlotCount() && xZoning.GetLot(xB.m_uLot).m_bActive)
			{
				xZoning.GetLotMutable(xB.m_uLot).m_uBuildingId = CB_Zoning::INVALID;
			}
			xB.m_bActive    = false;
			xB.m_uFireTicks = 0;
			++m_uFiresDestroyed;
			if (m_uActiveBuildings > 0) { --m_uActiveBuildings; }
		}
		else
		{
			++uFires;
		}
	}
	m_uActiveFires = uFires;
}

void CB_BuildingPlacement::Tick(CB_Zoning& xZoning)
{
	++m_uTick;

	// --- despawn buildings whose lot was bulldozed or un-zoned ---
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive) continue;
		bool bGone = (xB.m_uLot >= xZoning.GetLotSlotCount());
		if (!bGone)
		{
			const CB_Lot& xLot = xZoning.GetLot(xB.m_uLot);
			if (!xLot.m_bActive || xLot.m_eZone == CB_ZONE_NONE)
			{
				bGone = true;
				if (xLot.m_bActive)
				{
					xZoning.GetLotMutable(xB.m_uLot).m_uBuildingId = CB_Zoning::INVALID;
				}
			}
		}
		if (bGone)
		{
			xB.m_bActive = false;
			if (m_uActiveBuildings > 0) --m_uActiveBuildings;
		}
	}

	// --- growth + level-up pass (rate-limited) ---
	if ((m_uTick % GROW_INTERVAL) == 0)
	{
		UpdateSimState(xZoning);
		ProcessDisasters(xZoning);   // advance fires (uses the positions/coverage just computed)

		// New buildings need power, water + solvency (Cities: Skylines gating).
		if (IsPowered() && IsWatered() && m_fTreasury > 0.0f)
		{
			if (GetResDemand() > 0.1f) GrowZone(xZoning, CB_ZONE_RESIDENTIAL, CB_BUILDING_RES_LOW, SPAWNS_PER_PASS);
			if (GetComDemand() > 0.1f) GrowZone(xZoning, CB_ZONE_COMMERCIAL, CB_BUILDING_COM_LOW, SPAWNS_PER_PASS);
			if (GetIndDemand() > 0.1f) GrowZone(xZoning, CB_ZONE_INDUSTRIAL, CB_BUILDING_IND_LOW, SPAWNS_PER_PASS);
		}

		// Mature buildings level up toward higher density as demand persists.
		const float afDemand[3] = { GetResDemand(), GetComDemand(), GetIndDemand() };
		for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
		{
			CB_Building& xB = m_axBuildings.Get(i);
			if (!xB.m_bActive || xB.m_uLevel >= 2) continue;
			const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xB.m_eType);
			const uint32_t uCat = (xDef.m_eZone == CB_ZONE_RESIDENTIAL) ? 0u
			                    : (xDef.m_eZone == CB_ZONE_COMMERCIAL)  ? 1u : 2u;
			if (afDemand[uCat] < 0.3f) continue;
			if (m_fHappiness < 0.45f) continue;   // higher density needs services + utilities
			xB.m_fGrowth += 0.34f;
			if (xB.m_fGrowth >= 1.0f)
			{
				xB.m_fGrowth = 0.0f;
				const CB_EBuildingType eNext = static_cast<CB_EBuildingType>(xB.m_eType + 1);  // LOW->MED->HIGH within a category
				xB.m_eType  = eNext;
				xB.m_uLevel += 1;
				xB.m_uOccupants = static_cast<uint16_t>(CB_BuildingDefs::Get(eNext).m_uMaxOccupants);
			}
		}
	}

	RecountOccupants();
}

void CB_BuildingPlacement::RecountOccupants()
{
	m_uResidents = 0;
	m_uComJobs   = 0;
	m_uIndJobs   = 0;
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		const CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive) continue;
		const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xB.m_eType);
		switch (xDef.m_eZone)
		{
		case CB_ZONE_RESIDENTIAL: m_uResidents += xB.m_uOccupants; break;
		case CB_ZONE_COMMERCIAL:  m_uComJobs   += xB.m_uOccupants; break;
		case CB_ZONE_INDUSTRIAL:  m_uIndJobs   += xB.m_uOccupants; break;
		default: break;
		}
	}
}

void CB_BuildingPlacement::Render(const CB_Zoning& xZoning) const
{
	for (uint32_t i = 0; i < m_axBuildings.GetSize(); ++i)
	{
		const CB_Building& xB = m_axBuildings.Get(i);
		if (!xB.m_bActive || xB.m_uLot >= xZoning.GetLotSlotCount()) continue;
		const CB_Lot& xLot = xZoning.GetLot(xB.m_uLot);
		if (!xLot.m_bActive) continue;
		const float fH = BuildingHeight(xB.m_eType);
		// Burning buildings glow fiery orange (the disaster read-out).
		const Zenith_Maths::Vector3 xCol = (xB.m_uFireTicks > 0)
			? Zenith_Maths::Vector3(0.95f, 0.35f, 0.08f) : BuildingColor(xB.m_eType);
		// Sink the base 0.6m: m_fWorldY is now the exact fine rendered surface, so a base placed
		// right at it would be coplanar with the terrain mesh (a new base z-fight). Embedding lets
		// the ground occlude the bottom face — the building emerges cleanly with no shimmer.
		EmitBuilding(xLot.m_xPos, xLot.m_xFaceDir, 5.0f, xLot.m_fWorldY - 0.6f, fH, xCol, xB.m_uLevel, BuildHash(xB.m_uLot));
	}

	// Service / utility buildings (free-standing, larger footprint, distinct colours).
	for (uint32_t i = 0; i < m_axServices.GetSize(); ++i)
	{
		const CB_ServiceBuilding& xS = m_axServices.Get(i);
		if (!xS.m_bActive) continue;
		// Embed the base (see the building note): 0.6m > the coarse-vs-fine height gap, so the
		// bottom face always sits below the rendered ground and the terrain occludes it.
		EmitBox(xS.m_xPos, Zenith_Maths::Vector2(0.0f, 1.0f), 9.0f, 9.0f, xS.m_fWorldY - 0.6f,
		        ServiceHeight(xS.m_eType), ServiceColor(xS.m_eType));
	}
}

void CB_BuildingPlacement::WriteToDataStream(Zenith_DataStream& xStream) const
{
	CB_Serialize::WriteVec(xStream, m_axBuildings);
	CB_Serialize::WriteVec(xStream, m_axServices);
	xStream << m_uActiveBuildings;
	xStream << m_uActiveServices;
	xStream << m_fTreasury;
}

void CB_BuildingPlacement::ReadFromDataStream(Zenith_DataStream& xStream)
{
	CB_Serialize::ReadVec(xStream, m_axBuildings);
	CB_Serialize::ReadVec(xStream, m_axServices);
	xStream >> m_uActiveBuildings;
	xStream >> m_uActiveServices;
	xStream >> m_fTreasury;
	RecountOccupants();   // restore residents/jobs; utility/economy state recomputes next Tick
}
