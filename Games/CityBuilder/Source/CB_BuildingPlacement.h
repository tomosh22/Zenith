#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingDefs.h"
#include "CityBuilder/Source/CB_Policy.h"
#include <cstdint>

class CB_Districts;     // policy provider (city-wide + per-district), queried per building
class CB_TransitLines;  // public-transport stops; gates transit ridership by reach
class CB_Conduits;      // utility conduits; extend power/water reach along connected chains

// ============================================================================
// CB_BuildingPlacement — Cities: Skylines-style zoned growth. Buildings spawn in
// zoned, unoccupied frontage lots (CB_Zoning) when RCI demand calls for them,
// positioned + rotated to face the road. Buildings level up (low→med→high) as
// the neighbourhood matures, and despawn when their lot is bulldozed. Tracks
// residents/jobs and derives the RCI demand the economy + HUD read. Rendered as
// rotated boxes (G4 first pass) so footprints face the road at any angle.
// ============================================================================

struct CB_Building
{
	uint32_t         m_uLot       = 0xFFFFFFFFu;
	CB_EBuildingType m_eType      = CB_BUILDING_NONE;
	uint16_t         m_uOccupants = 0;        // residents (RES) or jobs (COM/IND)
	uint8_t          m_uLevel     = 0;        // 0=low, 1=med, 2=high
	float            m_fGrowth    = 0.0f;     // 0..1 progress toward the next level
	bool             m_bActive    = false;
	uint16_t         m_uFireTicks = 0;        // 0 = not on fire; >0 = sim ticks the building has been burning
	Zenith_Maths::Vector2 m_xWorldPos = Zenith_Maths::Vector2(0.0f, 0.0f);  // cached lot position (for disasters)
};

// Player-placed service / utility building (power plant, water tower, police,
// fire, hospital, school) — free position, not on a zoning lot.
struct CB_ServiceBuilding
{
	Zenith_Maths::Vector2 m_xPos    = Zenith_Maths::Vector2(0.0f, 0.0f);
	float                 m_fWorldY = 0.0f;
	CB_EBuildingType      m_eType   = CB_BUILDING_NONE;
	bool                  m_bActive = false;
};

class CB_BuildingPlacement
{
public:
	static constexpr uint32_t INVALID         = 0xFFFFFFFFu;
	static constexpr uint32_t GROW_INTERVAL   = 20;       // frames between growth passes (~3/s at 60fps)
	static constexpr uint32_t SPAWNS_PER_PASS = 2;        // new buildings per zone per pass
	static constexpr float    START_TREASURY  = 150000.0f; // starting funds (covers initial infrastructure before tax income)
	static constexpr float    BASELINE_POWER  = 40.0f;    // free starting power capacity (before the first plant)
	static constexpr float    BASELINE_WATER  = 40.0f;    // free starting water capacity
	static constexpr float    ECON_RATE       = 0.10f;    // treasury delta scale per growth pass

	void Reset();

	// One simulation step: despawn buildings on removed lots, recompute the
	// utility balance / service coverage / happiness / economy, then grow new
	// buildings + level up existing ones (gated on power, water, funds + land
	// value). Updates resident/job totals. Call each frame; it self-rate-limits.
	void Tick(CB_Zoning& xZoning);

	// Place a player service / utility building (power plant, water tower, police,
	// fire, hospital, school) at a free world position.
	void PlaceService(CB_EBuildingType eType, float fWorldX, float fWorldZ, float fWorldY);

	// Render each building as a road-facing rotated box + service buildings (windowed).
	void Render(const CB_Zoning& xZoning) const;

	uint32_t GetActiveBuildings() const { return m_uActiveBuildings; }
	uint32_t GetActiveServices()  const { return m_uActiveServices; }
	uint32_t GetResidents()       const { return m_uResidents; }
	uint32_t GetComJobs()         const { return m_uComJobs; }
	uint32_t GetIndJobs()         const { return m_uIndJobs; }
	uint32_t GetJobs()            const { return m_uComJobs + m_uIndJobs; }
	uint32_t GetPopulation()      const { return m_uResidents; }

	// RCI demand in [0,1], derived from the resident/job balance.
	float GetResDemand() const;
	float GetComDemand() const;
	float GetIndDemand() const;

	// Economy / utilities / services (for the HUD + growth gating).
	float GetTreasury()       const { return m_fTreasury; }
	float GetHappiness()      const { return m_fHappiness; }
	float GetPowerProduced()  const { return BASELINE_POWER + m_fPowerProd; }
	float GetPowerConsumed()  const { return m_fPowerUse; }
	float GetWaterProduced()  const { return BASELINE_WATER + m_fWaterProd; }
	float GetWaterConsumed()  const { return m_fWaterUse; }
	bool  IsPowered()         const { return GetPowerProduced() >= m_fPowerUse; }
	bool  IsWatered()         const { return GetWaterProduced() >= m_fWaterUse; }
	float GetServedFraction() const { return m_fServedFrac; }
	float GetIncome()         const { return m_fIncome; }
	float GetUpkeep()         const { return m_fUpkeep; }
	float GetCongestion()     const { return m_fCongestion; }   // 0..1 traffic congestion
	float GetPollution()      const { return m_fPollution; }    // 0..1 pollution (industry up, parks down)
	float GetTaxRate()        const { return m_fTaxRate; }      // 0.5..1.5 (higher = more income, less happiness)
	float GetDebt()           const { return m_fDebt; }         // outstanding loan principal+interest
	float GetUtilityReach()   const { return m_fUtilReach; }    // 0..1 fraction of buildings on the power+water network
	float GetGarbage()        const { return m_fGarbage; }      // 0..1 uncollected garbage (needs landfills)
	float GetSewage()         const { return m_fSewage; }       // 0..1 untreated sewage (needs sewage plants)
	float GetTransitShare()   const { return m_fTransitShare; } // 0..1 commuters using public transport
	float GetMail()           const { return m_fMail; }         // 0..1 uncollected mail (needs post offices)
	float GetFreightRatio()   const { return m_fFreightRatio; } // 0..1 industrial goods supply meeting commerce
	// Fraction of buildings covered by a given policy last sim pass (city-wide → 1.0).
	float GetPolicyCoverage(CB_EPolicy ePolicy) const { return m_afPolicyFrac[ePolicy]; }

	// Disasters: buildings can catch fire. Fire-station coverage extinguishes them;
	// an uncovered fire destroys the building. Active fires erode happiness.
	uint32_t GetActiveFires()    const { return m_uActiveFires; }
	uint32_t GetFiresDestroyed() const { return m_uFiresDestroyed; }   // cumulative buildings lost to fire
	void     SetAutoDisasters(bool bOn) { m_bAutoDisasters = bOn; }   // rare deterministic ignition (off for tests)
	bool     TriggerFireAt(float fWorldX, float fWorldZ);             // ignite the nearest building (scripted/test)

	// Budget lever: the tax rate scales tax income and (above 1.0) erodes happiness.
	void  SetTaxRate(float fRate) { m_fTaxRate = (fRate < 0.5f) ? 0.5f : ((fRate > 1.5f) ? 1.5f : fRate); }
	// Borrow a lump sum now (added to the treasury) against 15% interest, amortized
	// from the treasury over time. Lets a young city fund infrastructure up front.
	void  TakeLoan(float fAmount) { if (fAmount > 0.0f) { m_fTreasury += fAmount; m_fDebt += fAmount * 1.15f; } }
	// A no-debt cash grant (population-milestone rewards).
	void  GrantFunds(float fAmount) { m_fTreasury += fAmount; }

	// The manager feeds the live road-network length each frame so the sim derives
	// traffic congestion (population vs road capacity) — it lowers happiness, so more
	// road relieves it (Cities: Skylines feedback). Headless tests leave it at the
	// uncongested default.
	void  SetRoadCapacity(float fTotalRoadLength) { m_fRoadCapacity = fTotalRoadLength * 2.0f; }

	// Provide the policy source (city-wide + per-district ordinances). Optional — null
	// (the default) means no policies are in effect. The manager points this at its
	// CB_Districts; tests build a local one.
	void  SetDistricts(const CB_Districts* pxDistricts) { m_pxDistricts = pxDistricts; }

	// Provide the transit-line network (optional). With lines present, transit ridership
	// is gated by the population their stops reach; null (default) keeps the depot model.
	void  SetTransitLines(const CB_TransitLines* pxTransit) { m_pxTransit = pxTransit; }

	// Provide the utility conduit network (optional). Conduits extend power/water reach
	// from a source along connected chains; null (default) = source-radius reach only.
	void  SetConduits(CB_Conduits* pxConduits) { m_pxConduits = pxConduits; }

	// Serialize buildings + services + treasury (Zenith_DataStream forward-declared
	// via CB_Zoning.h → CB_RoadGraph.h). Residents/jobs are recomputed on load.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	void RecountOccupants();
	uint32_t GrowZone(CB_Zoning& xZoning, CB_EZoneType eZone, CB_EBuildingType eLowType, uint32_t uMaxSpawn);
	void UpdateSimState(const CB_Zoning& xZoning);   // utilities + services + happiness + economy
	void ProcessDisasters(CB_Zoning& xZoning);       // advance fires: fight (covered) or destroy (uncovered)
	bool HasFireCoverage(float fWorldX, float fWorldZ) const;  // a fire station in range of the point

	Zenith_Vector<CB_Building>        m_axBuildings;
	Zenith_Vector<CB_ServiceBuilding> m_axServices;
	uint32_t m_uActiveBuildings = 0;
	uint32_t m_uActiveServices  = 0;
	uint32_t m_uResidents       = 0;
	uint32_t m_uComJobs         = 0;
	uint32_t m_uIndJobs         = 0;
	uint32_t m_uTick            = 0;

	// Sim state (recomputed each growth pass).
	float m_fTreasury   = START_TREASURY;
	float m_fPowerProd  = 0.0f;   // from placed power plants (excludes BASELINE)
	float m_fPowerUse   = 0.0f;
	float m_fWaterProd  = 0.0f;
	float m_fWaterUse   = 0.0f;
	float m_fHappiness  = 0.5f;
	float m_fServedFrac = 0.0f;
	float m_fIncome       = 0.0f;
	float m_fUpkeep       = 0.0f;
	float m_fRoadCapacity = 1.0e9f;   // people the road network can carry (default: uncongested)
	float m_fCongestion   = 0.0f;     // 0..1, lowers happiness
	float m_fPollution    = 0.0f;     // 0..1, lowers happiness
	float m_fTaxRate      = 1.0f;     // 0.5..1.5 budget lever
	float m_fDebt         = 0.0f;     // outstanding loan (amortized each pass)
	float m_fUtilReach    = 1.0f;     // 0..1 utility-network connection
	float m_fGarbage      = 0.0f;     // 0..1 uncollected garbage
	float m_fSewage       = 0.0f;     // 0..1 untreated sewage
	float m_fTransitShare = 0.0f;     // 0..1 commuters on public transport
	float m_fMail         = 0.0f;     // 0..1 uncollected mail
	float m_fFreightRatio = 1.0f;     // 0..1 industrial goods meeting commercial demand

	const CB_Districts*    m_pxDistricts = nullptr;       // policy provider (optional)
	const CB_TransitLines* m_pxTransit   = nullptr;       // transit-line network (optional)
	CB_Conduits*           m_pxConduits  = nullptr;       // utility conduit network (optional; Energize mutates it)
	float m_afPolicyFrac[CB_POLICY_COUNT] = { 0.0f };     // per-policy building coverage (last pass)

	uint32_t m_uActiveFires    = 0;       // buildings currently burning
	uint32_t m_uFiresDestroyed = 0;       // cumulative buildings razed by fire
	bool     m_bAutoDisasters  = false;   // rare deterministic auto-ignition (the game enables it)
};
