#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"

// ============================================================================
// CB_Traffic — visual road traffic: a pool of vehicles that drive along the
// road-graph splines (terrain-following), turning at junctions. Vehicles wander
// the live network (respawning when their road is bulldozed), giving the city
// the moving life of Cities: Skylines. Also exposes A* over the road graph
// (FindPath) — node-to-node shortest path by segment length — for goal-directed
// routing + headless testing. Pure logic + debug-primitive rendering.
// ============================================================================

struct CB_Vehicle
{
	uint32_t              m_uSeg      = 0xFFFFFFFFu;  // current segment
	uint32_t              m_uFromNode = 0xFFFFFFFFu;  // node entered from (sets travel direction)
	float                 m_fT        = 0.0f;         // [0,1] progress from m_uFromNode
	float                 m_fSpeed    = 16.0f;        // m/s
	Zenith_Maths::Vector3 m_xPos      = Zenith_Maths::Vector3(0.0f);  // world (updated each tick)
	Zenith_Maths::Vector2 m_xFwd      = Zenith_Maths::Vector2(0.0f, 1.0f);
	Zenith_Maths::Vector3 m_xColor    = Zenith_Maths::Vector3(0.85f, 0.85f, 0.88f);
	bool                  m_bActive   = false;
};

class CB_Traffic
{
public:
	static constexpr uint32_t TARGET_VEHICLES = 48;

	void Reset();

	// Top up the vehicle pool from the live network + advance every vehicle along
	// its segment, turning to a random connected segment at each junction.
	void Update(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, float fDt);

	// Draw each vehicle as a small oriented box (windowed).
	void Render() const;

	uint32_t GetActiveVehicleCount() const { return m_uActive; }
	uint32_t GetVehicleSlotCount()   const { return m_axVehicles.GetSize(); }
	const CB_Vehicle& GetVehicle(uint32_t i) const { return m_axVehicles.Get(i); }

	// A* over the road graph: shortest segment path from node uStart to uGoal by
	// summed segment length. Fills outSegs (start→goal order); returns false if
	// unreachable or the nodes are invalid.
	static bool FindPath(const CB_RoadGraph& xGraph, uint32_t uStart, uint32_t uGoal, Zenith_Vector<uint32_t>& outSegs);

private:
	uint32_t NextRand();
	bool     SpawnVehicle(const CB_RoadGraph& xGraph, CB_Vehicle& xV);
	void     AdvanceVehicle(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField, CB_Vehicle& xV, float fDt);

	Zenith_Vector<CB_Vehicle> m_axVehicles;
	uint32_t m_uActive = 0;
	uint32_t m_uRng    = 0x1234567u;
};
